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
    data = rv = client.get("/ping").get_json()
    assert data == {"pong": True}


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
