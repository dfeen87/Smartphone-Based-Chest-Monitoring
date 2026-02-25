"""
Tests for JWT authentication and /ping keepalive endpoint in server/app.py.
"""

import sys
import os
import pytest

# Ensure the repo root is on the path so server/app.py can import validation/
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

# Use a fixed secret so tests are deterministic
os.environ.setdefault("JWT_SECRET", "test-secret")
os.environ.setdefault("API_KEY", "test-api-key")

from server.app import app, _make_token, _API_KEY  # noqa: E402


@pytest.fixture
def client():
    app.config["TESTING"] = True
    with app.test_client() as c:
        yield c


@pytest.fixture
def auth_headers():
    token = _make_token()
    return {"Authorization": f"Bearer {token}"}


# ── /ping ────────────────────────────────────────────────────────────────────────

def test_ping_returns_200(client):
    rv = client.get("/ping")
    assert rv.status_code == 200


def test_ping_returns_pong(client):
    data = client.get("/ping").get_json()
    assert data["pong"] is True
    assert "default_key_active" in data


def test_ping_default_key_active_with_custom_key(client):
    """When API_KEY env var is explicitly set, default_key_active must be False."""
    # The test setUp sets API_KEY=test-api-key, so default_key_active is False.
    data = client.get("/ping").get_json()
    assert data["default_key_active"] is False


def test_ping_needs_no_auth(client):
    """Ping must succeed without any Authorization header."""
    rv = client.get("/ping")
    assert rv.status_code == 200


# ── /api/auth/token ──────────────────────────────────────────────────────────────

def test_auth_token_valid_key(client):
    rv = client.post("/api/auth/token", json={"key": _API_KEY})
    assert rv.status_code == 200
    data = rv.get_json()
    assert "token" in data
    assert isinstance(data["token"], str) and len(data["token"]) > 0


def test_auth_token_invalid_key(client):
    rv = client.post("/api/auth/token", json={"key": "wrong-key"})
    assert rv.status_code == 401
    assert "error" in rv.get_json()


def test_auth_token_missing_key(client):
    rv = client.post("/api/auth/token", json={})
    assert rv.status_code == 401


# ── JWT protection of /api/* endpoints ──────────────────────────────────────────

def test_api_status_requires_jwt(client):
    rv = client.get("/api/status")
    assert rv.status_code == 401


def test_api_status_with_valid_jwt(client, auth_headers):
    rv = client.get("/api/status", headers=auth_headers)
    assert rv.status_code == 200
    data = rv.get_json()
    assert data["status"] == "running"


def test_api_logs_requires_jwt(client):
    rv = client.get("/api/logs")
    assert rv.status_code == 401


def test_api_logs_with_valid_jwt(client, auth_headers):
    rv = client.get("/api/logs", headers=auth_headers)
    assert rv.status_code == 200
    assert isinstance(rv.get_json(), list)


def test_api_config_get_requires_jwt(client):
    rv = client.get("/api/config")
    assert rv.status_code == 401


def test_api_config_get_with_valid_jwt(client, auth_headers):
    rv = client.get("/api/config", headers=auth_headers)
    assert rv.status_code == 200
    data = rv.get_json()
    assert "memory_samples" in data


def test_api_config_post_requires_jwt(client):
    rv = client.post("/api/config", json={})
    assert rv.status_code == 401


def test_api_config_post_with_valid_jwt(client, auth_headers):
    rv = client.post("/api/config", json={"alpha": 2.5}, headers=auth_headers)
    assert rv.status_code == 200
    assert rv.get_json()["status"] == "ok"


def test_api_run_requires_jwt(client):
    rv = client.post("/api/run", json={})
    assert rv.status_code == 401


def test_invalid_bearer_token(client):
    rv = client.get("/api/status", headers={"Authorization": "Bearer not-a-real-token"})
    assert rv.status_code == 401
    assert "error" in rv.get_json()


def test_missing_bearer_prefix(client):
    token = _make_token()
    rv = client.get("/api/status", headers={"Authorization": token})
    assert rv.status_code == 401


# ── /api/validate ────────────────────────────────────────────────────────────────

def test_api_validate_requires_jwt(client):
    rv = client.post("/api/validate", json={})
    assert rv.status_code == 401


def test_api_validate_with_valid_jwt(client, auth_headers):
    rv = client.post(
        "/api/validate",
        json={"n_records": 2, "synthetic": True},
        headers=auth_headers,
    )
    assert rv.status_code == 200
    data = rv.get_json()
    assert data["status"] == "ok"
    assert data["n_records"] == 2
    assert "stats" in data
    assert "methods_statement" in data
    stats = data["stats"]
    for key in ("drift_latency", "pause_latency", "false_alarms", "rms_latency", "fft_latency"):
        assert key in stats
        assert "mean" in stats[key]
        assert "std" in stats[key]


def test_api_validate_n_records_too_large(client, auth_headers):
    rv = client.post(
        "/api/validate",
        json={"n_records": 54, "synthetic": True},
        headers=auth_headers,
    )
    assert rv.status_code == 400
    assert "error" in rv.get_json()


def test_api_validate_methods_statement(client, auth_headers):
    rv = client.post(
        "/api/validate",
        json={"n_records": 2, "synthetic": True},
        headers=auth_headers,
    )
    assert rv.status_code == 200
    ms = rv.get_json()["methods_statement"]
    assert "N = 2" in ms
    assert "Section 5" in ms


# ── /api/results/metrics.csv ─────────────────────────────────────────────────────

def test_api_results_metrics_csv_requires_jwt(client):
    rv = client.get("/api/results/metrics.csv")
    assert rv.status_code == 401


def test_api_results_metrics_csv_after_validate(client, auth_headers):
    # Run validation first to create the CSV
    client.post(
        "/api/validate",
        json={"n_records": 2, "synthetic": True},
        headers=auth_headers,
    )
    rv = client.get("/api/results/metrics.csv", headers=auth_headers)
    assert rv.status_code == 200
    assert "text/csv" in rv.content_type


# ── /api/results/summary.csv ─────────────────────────────────────────────────────

def test_api_results_summary_csv_requires_jwt(client):
    rv = client.get("/api/results/summary.csv")
    assert rv.status_code == 401


def test_api_results_summary_csv_after_validate(client, auth_headers):
    client.post(
        "/api/validate",
        json={"n_records": 2, "synthetic": True},
        headers=auth_headers,
    )
    rv = client.get("/api/results/summary.csv", headers=auth_headers)
    assert rv.status_code == 200
    assert "text/csv" in rv.content_type


# ── /api/results/pdf ─────────────────────────────────────────────────────────────

def test_api_results_pdf_requires_jwt(client):
    rv = client.get("/api/results/pdf")
    assert rv.status_code == 401


def test_api_results_pdf_after_validate(client, auth_headers):
    client.post(
        "/api/validate",
        json={"n_records": 2, "synthetic": True},
        headers=auth_headers,
    )
    rv = client.get("/api/results/pdf", headers=auth_headers)
    assert rv.status_code == 200
    assert rv.content_type == "application/pdf"


# ── /api/results/docx ────────────────────────────────────────────────────────────

def test_api_results_docx_requires_jwt(client):
    rv = client.get("/api/results/docx")
    assert rv.status_code == 401


def test_api_results_docx_after_validate(client, auth_headers):
    client.post(
        "/api/validate",
        json={"n_records": 2, "synthetic": True},
        headers=auth_headers,
    )
    rv = client.get("/api/results/docx", headers=auth_headers)
    assert rv.status_code == 200
    assert "wordprocessingml" in rv.content_type


# ── /api/send-results ────────────────────────────────────────────────────────────

def test_api_send_results_requires_jwt(client):
    rv = client.post("/api/send-results", json={"email": "test@example.com"})
    assert rv.status_code == 401


def test_api_send_results_missing_email(client, auth_headers):
    rv = client.post("/api/send-results", json={}, headers=auth_headers)
    assert rv.status_code == 400
    assert "error" in rv.get_json()


def test_api_send_results_invalid_email(client, auth_headers):
    rv = client.post("/api/send-results", json={"email": "notvalid"}, headers=auth_headers)
    assert rv.status_code == 400


# ── /api/metrics ─────────────────────────────────────────────────────────────────

@pytest.fixture
def clear_metrics():
    """Ensure _last_metrics is empty for the duration of the test, then restore."""
    from server import app as app_module
    saved = dict(app_module._last_metrics)
    app_module._last_metrics.clear()
    yield
    app_module._last_metrics.clear()
    app_module._last_metrics.update(saved)


def test_api_metrics_requires_jwt(client):
    rv = client.get("/api/metrics")
    assert rv.status_code == 401


def test_api_metrics_empty_before_run(client, auth_headers, clear_metrics):
    """Before any run /api/metrics must return an empty object, not an error."""
    rv = client.get("/api/metrics", headers=auth_headers)
    assert rv.status_code == 200
    assert rv.get_json() == {}


def test_api_metrics_populated_after_run(client, auth_headers):
    """After a successful /api/run the metrics endpoint returns the same data."""
    run_rv = client.post("/api/run", json={"duration_s": 10}, headers=auth_headers)
    assert run_rv.status_code == 200
    run_metrics = run_rv.get_json()["metrics"]

    metrics_rv = client.get("/api/metrics", headers=auth_headers)
    assert metrics_rv.status_code == 200
    data = metrics_rv.get_json()
    # All keys from the run response must be present and equal
    for key in run_metrics:
        assert data[key] == run_metrics[key]
