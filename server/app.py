"""
RespiroSync Dashboard Server
=============================
A lightweight Flask web server that exposes a REST API and serves the
dashboard UI for the RespiroSync phase–memory operator.

Endpoints
---------
  GET  /                  → dashboard UI (server/static/index.html)
  GET  /ping              → keepalive probe (no auth required)
  POST /api/auth/token    → obtain a signed JWT access token
  GET  /api/status        → system status (uptime, version)
  GET  /api/logs          → recent structured log entries (?n=50)
  GET  /api/config        → current operator configuration
  POST /api/config        → update operator configuration
  POST /api/run           → run the phase–memory operator on synthetic data

Authentication
--------------
All /api/* endpoints (except /api/auth/token) require a Bearer JWT in the
Authorization header::

    Authorization: Bearer <token>

Obtain a token via POST /api/auth/token with JSON body::

    {"username": "...", "password": "..."}

Required environment variables
-------------------------------
JWT_SECRET_KEY  – secret used to sign tokens (required for auth to be active)
API_USERNAME    – valid username (default: "admin")
API_PASSWORD    – valid password (required; no default for security)
JWT_EXPIRES_IN  – token lifetime in seconds (default: 3600)

If JWT_SECRET_KEY is not set the server starts without authentication so that
existing deployments are not broken by the upgrade.

All log output goes to stdout so that Render.com captures it in the
service dashboard without any extra configuration.

Scientific basis: PAPER.md §3–4 (phase–memory operator pipeline).
"""

import os
import sys
import logging
import threading
import time
from collections import deque
from datetime import datetime, timezone, timedelta
from functools import wraps
from pathlib import Path

import jwt
from flask import Flask, jsonify, request, send_from_directory

# ── Import the validation pipeline (validation/ lives at the repo root) ────────
sys.path.insert(0, str(Path(__file__).parent.parent))

from validation.pipeline import (  # noqa: E402
    run_pipeline,
    DEFAULT_MEMORY_SAMPLES,
    DEFAULT_ALPHA,
    DEFAULT_BASELINE_SAMP,
    DEFAULT_FS,
)
from validation.physionet_loader import generate_synthetic_resp  # noqa: E402

# ── Structured logging ──────────────────────────────────────────────────────────
# All handlers write to stdout so Render captures the stream automatically.

LOG_BUFFER_SIZE = 500  # keep the last N log entries in memory for /api/logs

_log_buffer: deque = deque(maxlen=LOG_BUFFER_SIZE)


class _BufferHandler(logging.Handler):
    """Append formatted log records to the in-memory ring buffer."""

    def emit(self, record: logging.LogRecord) -> None:
        import datetime
        ts = datetime.datetime.fromtimestamp(record.created).strftime("%Y-%m-%dT%H:%M:%S")
        _log_buffer.append({
            "ts": ts,
            "level": record.levelname,
            "msg": record.getMessage(),
        })


_fmt = logging.Formatter("%(asctime)s [%(levelname)-8s] %(name)s — %(message)s",
                          datefmt="%Y-%m-%dT%H:%M:%S")

_stream_handler = logging.StreamHandler()  # → stdout → Render log tail
_stream_handler.setFormatter(_fmt)

_buffer_handler = _BufferHandler()
_buffer_handler.setFormatter(_fmt)

logging.basicConfig(level=logging.INFO, handlers=[_stream_handler])

logger = logging.getLogger("respirosync")
logger.addHandler(_buffer_handler)
logger.setLevel(logging.INFO)

# ── Flask application ───────────────────────────────────────────────────────────

app = Flask(__name__, static_folder="static")

_start_time = time.time()

# ── JWT configuration ───────────────────────────────────────────────────────────
# Authentication is only enforced when JWT_SECRET_KEY is set in the environment.
# This preserves backward compatibility for deployments that have not yet
# configured auth credentials.

_JWT_SECRET    = os.environ.get("JWT_SECRET_KEY", "")
_API_USERNAME  = os.environ.get("API_USERNAME", "admin")
_API_PASSWORD  = os.environ.get("API_PASSWORD", "")
_JWT_EXPIRES   = int(os.environ.get("JWT_EXPIRES_IN", 3600))  # seconds

_AUTH_ENABLED = bool(_JWT_SECRET)

if _AUTH_ENABLED:
    logger.info("JWT authentication enabled (token lifetime: %ds)", _JWT_EXPIRES)
else:
    logger.warning(
        "JWT_SECRET_KEY is not set — API is running WITHOUT authentication. "
        "Set JWT_SECRET_KEY, API_USERNAME and API_PASSWORD to enable auth."
    )


def _make_token() -> str:
    """Return a signed JWT valid for _JWT_EXPIRES seconds."""
    now = datetime.now(tz=timezone.utc)
    payload = {
        "iat": now,
        "exp": now + timedelta(seconds=_JWT_EXPIRES),
    }
    return jwt.encode(payload, _JWT_SECRET, algorithm="HS256")


def require_jwt(f):
    """Decorator that enforces Bearer-token JWT auth when auth is enabled."""
    @wraps(f)
    def decorated(*args, **kwargs):
        if not _AUTH_ENABLED:
            return f(*args, **kwargs)

        auth_header = request.headers.get("Authorization", "")
        if not auth_header.startswith("Bearer "):
            return jsonify({"error": "Missing or invalid Authorization header"}), 401

        token = auth_header[len("Bearer "):]
        try:
            jwt.decode(token, _JWT_SECRET, algorithms=["HS256"])
        except jwt.ExpiredSignatureError:
            return jsonify({"error": "Token has expired"}), 401
        except jwt.InvalidTokenError:
            return jsonify({"error": "Invalid token"}), 401

        return f(*args, **kwargs)
    return decorated


# ── Operator configuration (mutable at runtime via /api/config) ────────────────
_config: dict = {
    "memory_samples":   DEFAULT_MEMORY_SAMPLES,  # M  — rolling window (Eq. 4)
    "alpha":            DEFAULT_ALPHA,            # α  — sensitivity     (Eq. 6)
    "baseline_samples": DEFAULT_BASELINE_SAMP,   # calibration window
    "fs":               DEFAULT_FS,              # sample rate (Hz)
}

# ── Processing state ────────────────────────────────────────────────────────────
_run_lock = threading.Lock()
_last_metrics: dict = {}


# ── Routes ──────────────────────────────────────────────────────────────────────

@app.route("/")
def index() -> object:
    """Serve the dashboard UI."""
    return send_from_directory(app.static_folder, "index.html")


@app.route("/ping")
def ping() -> object:
    """Lightweight keepalive probe — no authentication required.

    Designed to be called by uptime monitors (e.g. UptimeRobot, Freshping)
    so that the Render.com free-tier service does not spin down between real
    requests.  The response body is intentionally minimal to minimise latency.
    """
    return jsonify({"ok": True})


@app.route("/api/auth/token", methods=["POST"])
def api_auth_token() -> object:
    """Issue a signed JWT access token.

    Request body (JSON)
    -------------------
    username : str
    password : str

    Response (200)
    --------------
    {"token": "<jwt>", "expires_in": <seconds>}
    """
    if not _AUTH_ENABLED:
        return jsonify({"error": "Authentication is not configured on this server"}), 501

    body = request.get_json(force=True, silent=True) or {}
    username = body.get("username", "")
    password = body.get("password", "")

    if username != _API_USERNAME or password != _API_PASSWORD:
        logger.warning("Failed login attempt for user %r", username)
        return jsonify({"error": "Invalid credentials"}), 401

    token = _make_token()
    logger.info("JWT issued for user %r (expires in %ds)", username, _JWT_EXPIRES)
    return jsonify({"token": token, "expires_in": _JWT_EXPIRES})


@app.route("/api/status")
@require_jwt
def api_status() -> object:
    """Return current system status."""
    return jsonify({
        "status":    "running",
        "version":   "1.0.0",
        "uptime_s":  round(time.time() - _start_time, 1),
        "pipeline":  "phase-memory operator (PAPER.md §3–4)",
    })


@app.route("/api/logs")
@require_jwt
def api_logs() -> object:
    """Return recent log entries.

    Query params
    ------------
    n : int  Maximum number of entries to return (default 50, max 500).
    """
    try:
        n = min(int(request.args.get("n", 50)), LOG_BUFFER_SIZE)
    except ValueError:
        n = 50
    return jsonify(list(_log_buffer)[-n:])


@app.route("/api/config", methods=["GET"])
@require_jwt
def api_get_config() -> object:
    """Return the current operator configuration."""
    return jsonify(dict(_config))


@app.route("/api/config", methods=["POST"])
@require_jwt
def api_set_config() -> object:
    """Update operator configuration parameters.

    Accepted JSON keys
    ------------------
    memory_samples   : int   (Eq. 4 window M)
    alpha            : float (Eq. 6 sensitivity α ∈ [2, 3])
    baseline_samples : int
    fs               : float (sample rate Hz)
    """
    data = request.get_json(force=True, silent=True) or {}
    updated = {}
    for key, cast in (
        ("memory_samples",   int),
        ("alpha",            float),
        ("baseline_samples", int),
        ("fs",               float),
    ):
        if key in data:
            try:
                _config[key] = cast(data[key])
                updated[key] = _config[key]
            except (ValueError, TypeError) as exc:
                logger.warning("Config update rejected for %s: %s", key, exc)
    if updated:
        logger.info("Configuration updated: %s", updated)
    return jsonify({"status": "ok", "config": dict(_config)})


@app.route("/api/run", methods=["POST"])
@require_jwt
def api_run() -> object:
    """Run the phase–memory operator on a synthetic respiratory signal.

    Returns summary metrics derived from the operator output (PAPER.md §4).
    Use POST body JSON ``{"duration_s": 90}`` to control signal length.
    """
    if not _run_lock.acquire(blocking=False):
        logger.warning("Run requested while another run is in progress — rejected")
        return jsonify({"error": "A run is already in progress"}), 429

    try:
        body = request.get_json(force=True, silent=True) or {}
        duration_s = float(body.get("duration_s", 90))

        logger.info(
            "Operator run started — duration=%.0fs  M=%d  α=%.2f  fs=%.0f Hz",
            duration_s, _config["memory_samples"], _config["alpha"], _config["fs"],
        )

        # Generate synthetic respiratory signal (PAPER.md §5.1 regimes)
        data = generate_synthetic_resp(duration_s=duration_s, fs=int(_config["fs"]))
        signal = data["signal"]
        fs = data["fs"]

        # Run the full phase–memory pipeline (PAPER.md §7.1)
        result = run_pipeline(
            signal,
            fs=float(fs),
            M=int(_config["memory_samples"]),
            alpha=float(_config["alpha"]),
            baseline_samples=int(_config["baseline_samples"]),
        )

        metrics = {
            "sigma_omega":       round(float(result["sigma_omega"]),  6),
            "threshold":         round(float(result["threshold"]),    6),
            "instability_count": int(result["instability"].sum()),
            "instability_rate":  round(float(result["instability"].mean()), 4),
            "delta_phi_max":     round(float(result["delta_phi"].max()),    4),
            "delta_phi_mean":    round(float(result["delta_phi"].mean()),   4),
            "n_samples":         int(len(signal)),
            "duration_s":        round(float(len(signal) / fs), 1),
        }

        _last_metrics.clear()
        _last_metrics.update(metrics)

        logger.info(
            "Operator run complete — σ_ω=%.4f rad/s  threshold=%.4f rad/s  "
            "instability_rate=%.2f%%  n_alarms=%d",
            metrics["sigma_omega"],
            metrics["threshold"],
            metrics["instability_rate"] * 100,
            metrics["instability_count"],
        )

        return jsonify({"status": "ok", "metrics": metrics})

    except Exception as exc:  # pylint: disable=broad-except
        logger.error("Operator run failed: %s", exc, exc_info=True)
        return jsonify({"error": str(exc)}), 500

    finally:
        _run_lock.release()


# ── Entry point ─────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    port = int(os.environ.get("PORT", 5000))
    logger.info("RespiroSync dashboard starting on port %d", port)
    logger.info("Pipeline: preprocessing → analytic signal → θ(t) → ω(t) → ω̄(t) → ΔΦ(t)")
    app.run(host="0.0.0.0", port=port, debug=False)
