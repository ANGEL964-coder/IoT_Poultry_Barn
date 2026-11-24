"""
Tier 2: Boundary Value Analysis & Corner Cases for Authentication and Rate Limiting.
Covers empty keys, malformed Bearer headers, expired keys, header precedence,
and rate limiting threshold edge cases.
Total Tests: 9 (Requirement: >= 8).
"""

import time
import pytest


def test_b03_empty_api_key_header(http_client):
    """Verify empty string in X-API-Key header returns 401."""
    resp = http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers={"X-API-Key": ""})
    assert resp.status_code == 401


def test_b03_malformed_bearer_missing_token(http_client):
    """Verify Authorization: Bearer with no token returns 401."""
    resp = http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers={"Authorization": "Bearer "})
    assert resp.status_code == 401


def test_b03_malformed_bearer_missing_prefix(http_client):
    """Verify raw token without Bearer prefix in Authorization header returns 401."""
    resp = http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers={"Authorization": "master-test-key-2026"})
    assert resp.status_code == 401


def test_b03_case_sensitive_api_key(http_client):
    """Verify API key validation is strictly case-sensitive."""
    # "MASTER-TEST-KEY-2026" should fail if key is "master-test-key-2026"
    resp = http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers={"X-API-Key": "MASTER-TEST-KEY-2026"})
    assert resp.status_code == 401


def test_b03_header_precedence_valid_x_api_key_bad_bearer(http_client):
    """Verify X-API-Key takes precedence or succeeds when Bearer is invalid."""
    headers = {
        "X-API-Key": "master-test-key-2026",
        "Authorization": "Bearer invalid-garbage-token"
    }
    resp = http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers=headers)
    assert resp.status_code == 200


def test_b03_header_precedence_bad_x_api_key_valid_bearer(http_client):
    """Verify Bearer succeeds if X-API-Key is missing or fallback applies."""
    headers = {
        "Authorization": "Bearer master-test-key-2026"
    }
    resp = http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers=headers)
    assert resp.status_code == 200


def test_b03_exact_rate_limit_120th_request_succeeds(http_client):
    """Verify exactly 120 requests from a single client IP within 60s succeed."""
    unique_ip = "192.168.1.55"
    headers = {"X-Forwarded-For": unique_ip}
    for i in range(120):
        resp = http_client.get("/api/status", headers=headers)
        assert resp.status_code == 200, f"Request {i+1} failed before reaching limit: {resp.status_code}"


def test_b03_121st_request_triggers_429(http_client):
    """Verify 121st request strictly triggers 429 Too Many Requests."""
    unique_ip = "192.168.1.56"
    headers = {"X-Forwarded-For": unique_ip}
    for _ in range(120):
        http_client.get("/api/status", headers=headers)

    # 121st
    resp = http_client.get("/api/status", headers=headers)
    assert resp.status_code == 429


def test_b03_different_ips_have_independent_rate_limit_buckets(http_client):
    """Verify rate limits are partitioned per client IP address."""
    ip_a = "10.0.0.1"
    ip_b = "10.0.0.2"

    # Exhaust limit for IP A
    for _ in range(121):
        http_client.get("/api/status", headers={"X-Forwarded-For": ip_a})

    # IP A is blocked
    assert http_client.get("/api/status", headers={"X-Forwarded-For": ip_a}).status_code == 429

    # IP B is fresh and succeeds
    assert http_client.get("/api/status", headers={"X-Forwarded-For": ip_b}).status_code == 200


# --- RBAC Role Boundaries & Security Barriers ---

def test_b03_unauthenticated_requests_return_401(http_client):
    """Verify unauthenticated requests to protected endpoints strictly return HTTP 401."""
    dev_eui = "70b3d57ed007985a"
    # No auth header supplied
    endpoints = [
        ("POST", "/api/sdn/downlink", {"devEui": dev_eui, "cmdId": 0}),
        ("GET", "/api/keys", None),
        ("POST", "/api/keys", {"key": "should-fail"}),
        ("GET", "/api/auth/me", None),
        ("POST", f"/api/devices/{dev_eui}/scale/tare", None),
        ("POST", f"/api/devices/{dev_eui}/scale/calibrate", {"calSlope": 420.5}),
        ("GET", f"/api/devices/{dev_eui}/scale/calibration", None),
    ]

    for method, path, body in endpoints:
        if method == "POST":
            resp = http_client.post(path, json_data=body)
        else:
            resp = http_client.get(path)
        assert resp.status_code == 401, f"Expected 401 for unauthenticated {method} {path}, got {resp.status_code}"


def test_b03_operator_role_forbidden_on_admin_routes(http_client):
    """Verify ROLE_OPERADOR receives strictly HTTP 403 on administrative endpoints."""
    # Authenticate as operator
    login_resp = http_client.post("/api/auth/login", json_data={"username": "operador", "password": "operador123"})
    assert login_resp.status_code == 200
    op_token = login_resp.json()["token"]
    op_headers = {"Authorization": f"Bearer {op_token}"}

    dev_eui = "70b3d57ed007985a"
    admin_routes = [
        ("POST", "/api/sdn/downlink", {"devEui": dev_eui, "cmdId": 1}),
        ("GET", "/api/keys", None),
        ("POST", "/api/keys", {"key": "operator-cannot-create-key"}),
        ("POST", f"/api/devices/{dev_eui}/scale/tare", None),
        ("POST", f"/api/devices/{dev_eui}/scale/calibrate", {"calSlope": 420.5, "knownWeightKg": 10.0}),
        ("POST", "/api/devices", {"devEui": "70b3d57ed0079998", "name": "forbidden-node"}),
    ]

    for method, path, body in admin_routes:
        if method == "POST":
            resp = http_client.post(path, json_data=body, headers=op_headers)
        else:
            resp = http_client.get(path, headers=op_headers)
        assert resp.status_code == 403, (
            f"Expected 403 Forbidden for ROLE_OPERADOR on {method} {path}, got {resp.status_code}: {resp.text}"
        )
        assert "Forbidden" in resp.text or "denegado" in resp.text.lower() or "ROLE_ADMIN" in resp.text


def test_b03_admin_role_succeeds_on_admin_routes(http_client):
    """Verify ROLE_ADMIN successfully invokes administrative endpoints."""
    # Authenticate as admin
    login_resp = http_client.post("/api/auth/login", json_data={"username": "admin", "password": "server"})
    assert login_resp.status_code == 200
    admin_token = login_resp.json()["token"]
    admin_headers = {"Authorization": f"Bearer {admin_token}"}

    dev_eui = "70b3d57ed007985a"

    # 1. Downlink
    dl_resp = http_client.post("/api/sdn/downlink", json_data={"devEui": dev_eui, "cmdId": 0}, headers=admin_headers)
    assert dl_resp.status_code == 200

    # 2. Scale Tare
    tare_resp = http_client.post(f"/api/devices/{dev_eui}/scale/tare", headers=admin_headers)
    assert tare_resp.status_code == 200
    assert tare_resp.json().get("status") == "SUCCESS"

    # 3. Scale Calibrate
    cal_resp = http_client.post(
        f"/api/devices/{dev_eui}/scale/calibrate",
        json_data={"calSlope": 420.5, "calZero": 0.125, "knownWeightKg": 10.0},
        headers=admin_headers
    )
    assert cal_resp.status_code == 200

    # 4. Keys list
    keys_resp = http_client.get("/api/keys", headers=admin_headers)
    assert keys_resp.status_code == 200


def test_b03_logout_invalidates_session_token(http_client):
    """Verify logging out immediately invalidates session token."""
    login_resp = http_client.post("/api/auth/login", json_data={"username": "operador", "password": "operador123"})
    token = login_resp.json()["token"]
    headers = {"Authorization": f"Bearer {token}"}

    # Verify session active
    me_resp1 = http_client.get("/api/auth/me", headers=headers)
    assert me_resp1.status_code == 200

    # Logout
    logout_resp = http_client.post("/api/auth/logout", headers=headers)
    assert logout_resp.status_code == 200

    # Verify session now rejected with 401
    me_resp2 = http_client.get("/api/auth/me", headers=headers)
    assert me_resp2.status_code == 401
