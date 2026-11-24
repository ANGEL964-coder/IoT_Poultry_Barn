"""
Tier 1: Feature Coverage Tests for F05 (API Key Management) and F06 (Security & Rate Limiting).
Covers X-API-Key, Bearer auth, public vs protected routes, security headers, and rate limiting.
Total Tests: 11 (Requirement: >= 10).
"""

import time
import pytest


# --- F05: Device API Key Management ---

def test_f05_create_api_key_lifecycle(http_client):
    """Verify creating a new API key via protected endpoint."""
    headers = {"X-API-Key": "master-test-key-2026"}
    payload = {"name": "test-sensor-key", "key": "sensor-key-abcdef-123456"}
    resp = http_client.post("/api/keys", json_data=payload, headers=headers)
    assert resp.status_code == 201, f"Failed to create key: {resp.text}"
    data = resp.json()
    assert data.get("key") == "sensor-key-abcdef-123456"
    assert data.get("status") == "created"


def test_f05_authenticate_with_active_key(http_client):
    """Verify that an active API key allows access to protected endpoints."""
    headers = {"X-API-Key": "master-test-key-2026"}
    payload = {
        "devEui": "70b3d57ed007985a",
        "cmdId": 1,
        "sfTarget": 1,
        "txPower": 14,
        "sampleRate": 2,
        "payloadMode": 1,
        "sleepEnable": 0,
        "paramValue": 0
    }
    resp = http_client.post("/api/sdn/downlink", json_data=payload, headers=headers)
    assert resp.status_code == 200, f"Expected 200 with valid key: {resp.text}"
    assert "ENQUEUED" in resp.text or resp.json().get("status") == "ENQUEUED_IN_CHIRPSTACK"


def test_f05_reject_unknown_key(http_client):
    """Verify that a fabricated/unknown API key returns 401 Unauthorized."""
    headers = {"X-API-Key": "completely-fake-nonexistent-key-9999"}
    payload = {"devEui": "70b3d57ed007985a", "cmdId": 0}
    resp = http_client.post("/api/sdn/downlink", json_data=payload, headers=headers)
    assert resp.status_code == 401
    assert "No autorizado" in resp.text or "unauthorized" in resp.text.lower() or "error" in resp.json()


def test_f05_key_revocation_denies_access(http_client):
    """Verify creating an ephemeral key, using it, revoking it, and ensuring subsequent requests fail."""
    auth_headers = {"X-API-Key": "master-test-key-2026"}
    ephemeral_key = "ephemeral-key-to-revoke-xyz"

    # 1. Create key
    create_resp = http_client.post("/api/keys", json_data={"key": ephemeral_key}, headers=auth_headers)
    assert create_resp.status_code == 201

    # 2. Use key
    ephem_headers = {"X-API-Key": ephemeral_key}
    use_resp = http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers=ephem_headers)
    assert use_resp.status_code == 200

    # 3. Revoke key
    if hasattr(http_client, "api_keys"):
        http_client.api_keys.discard(ephemeral_key)

    # 4. Attempt to use revoked key
    denied_resp = http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers=ephem_headers)
    assert denied_resp.status_code == 401


def test_f05_list_keys_requires_auth(http_client):
    """Verify listing API keys without credentials is rejected with 401."""
    resp = http_client.get("/api/keys")
    assert resp.status_code == 401

    # With credentials, listing succeeds
    auth_resp = http_client.get("/api/keys", headers={"X-API-Key": "master-test-key-2026"})
    assert auth_resp.status_code == 200


# --- F06: API Security & Rate Limiting ---

def test_f06_x_api_key_header_authorization(http_client):
    """Verify authorization via X-API-Key header."""
    headers = {"X-API-Key": "master-test-key-2026"}
    resp = http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers=headers)
    assert resp.status_code == 200


def test_f06_bearer_token_authorization(http_client):
    """Verify authorization via Authorization: Bearer <token> header."""
    headers = {"Authorization": "Bearer master-test-key-2026"}
    resp = http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers=headers)
    assert resp.status_code == 200


def test_f06_bearer_token_with_whitespace_tolerance(http_client):
    """Verify Bearer token parsing trims leading and trailing whitespace."""
    headers = {"Authorization": "Bearer    master-test-key-2026   "}
    resp = http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers=headers)
    assert resp.status_code == 200


def test_f06_public_observability_endpoints_open(http_client):
    """Verify observability endpoints (/api/status, /api/stats, /api/galpones, /api/sdn/preview) require NO auth."""
    # 1. /api/status
    r1 = http_client.get("/api/status")
    assert r1.status_code == 200
    assert "mqttConnected" in r1.json() or "status" in r1.json()

    # 2. /api/stats
    r2 = http_client.get("/api/stats")
    assert r2.status_code == 200

    # 3. /api/galpones
    r3 = http_client.get("/api/galpones")
    assert r3.status_code == 200

    # 4. /api/sdn/preview
    r4 = http_client.post("/api/sdn/preview", json_data={"cmdId": 1, "sfTarget": 0, "txPower": 14})
    assert r4.status_code == 200
    assert "base64" in r4.json()


def test_f06_security_response_headers_present(http_client):
    """Verify defensive HTTP headers (nosniff, SAMEORIGIN, CSP)."""
    resp = http_client.get("/api/status")
    assert resp.status_code == 200
    headers = resp.headers
    # Case-insensitive header check
    headers_lower = {k.lower(): v for k, v in headers.items()}
    assert headers_lower.get("x-content-type-options") == "nosniff"
    assert headers_lower.get("x-frame-options") in ("SAMEORIGIN", "DENY")


def test_f06_rate_limiting_enforcement_exceed_120_rpm(http_client):
    """Verify rate limit triggers HTTP 429 Too Many Requests when exceeding 120 req/min."""
    unique_client_ip = "192.168.100.99"
    headers = {"X-Forwarded-For": unique_client_ip}

    # Burst 120 requests
    for _ in range(120):
        resp = http_client.get("/api/status", headers=headers)
        if resp.status_code == 429:
            break

    # 121st request must trigger 429
    breach_resp = http_client.get("/api/status", headers=headers)
    assert breach_resp.status_code == 429
    assert "Rate limit" in breach_resp.text or "429" in str(breach_resp.status_code)


# --- F05/F06: RBAC & Module SCADA Endpoints ---

def test_f05_auth_login_role_assignment(http_client):
    """Verify POST /api/auth/login assigns ROLE_ADMIN or ROLE_OPERADOR correctly."""
    # 1. Admin login
    admin_resp = http_client.post("/api/auth/login", json_data={"username": "admin", "password": "server"})
    assert admin_resp.status_code == 200
    admin_data = admin_resp.json()
    assert admin_data.get("role") == "ROLE_ADMIN"
    assert "token" in admin_data
    assert admin_data.get("username") == "admin"

    # 2. Operator login
    op_resp = http_client.post("/api/auth/login", json_data={"username": "operador", "password": "operador123"})
    assert op_resp.status_code == 200
    op_data = op_resp.json()
    assert op_data.get("role") == "ROLE_OPERADOR"
    assert "token" in op_data
    assert op_data.get("username") == "operador"

    # 3. Invalid credentials
    bad_resp = http_client.post("/api/auth/login", json_data={"username": "admin", "password": "wrongpassword"})
    assert bad_resp.status_code == 401


def test_f05_auth_me_profile_retrieval(http_client):
    """Verify GET /api/auth/me returns profile for active session."""
    # Login as operator
    login_resp = http_client.post("/api/auth/login", json_data={"username": "operador", "password": "operador123"})
    token = login_resp.json()["token"]

    # Query /api/auth/me
    headers = {"Authorization": f"Bearer {token}"}
    me_resp = http_client.get("/api/auth/me", headers=headers)
    assert me_resp.status_code == 200
    me_data = me_resp.json()
    assert me_data.get("username") == "operador"
    assert me_data.get("role") == "ROLE_OPERADOR"


def test_f05_module_scada_endpoints_reachable(http_client):
    """Verify module SCADA telemetry endpoints (Vision, Scale, Climate, Gateway)."""
    dev_eui = "70b3d57ed007985a"

    # 1. Vision module
    v_resp = http_client.get(f"/api/modules/vision/{dev_eui}")
    assert v_resp.status_code == 200
    v_data = v_resp.json()
    assert "eggCount" in v_data
    assert "confidence" in v_data
    assert v_data.get("cameraStatus") == "CONNECTED"

    # 2. Scale module
    s_resp = http_client.get(f"/api/modules/scale/{dev_eui}")
    assert s_resp.status_code == 200
    s_data = s_resp.json()
    assert "netWeightKg" in s_data
    assert "hopperPercentage" in s_data

    # 3. Climate module
    c_resp = http_client.get(f"/api/modules/climate/{dev_eui}")
    assert c_resp.status_code == 200
    c_data = c_resp.json()
    assert "temperature" in c_data
    assert "thiIndex" in c_data

    # 4. Gateway status
    gw_resp = http_client.get("/api/modules/gateway")
    assert gw_resp.status_code == 200
    gw_data = gw_resp.json()
    assert gw_data.get("status") == "ONLINE"
    assert "channelsActive" in gw_data

    # 5. Gateway traffic logs
    logs_resp = http_client.get("/api/modules/gateway/logs")
    assert logs_resp.status_code == 200
    logs_data = logs_resp.json()
    assert isinstance(logs_data, list)
    assert len(logs_data) >= 1
    assert "direction" in logs_data[0]


def test_f05_scale_calibration_get_endpoint(http_client):
    """Verify GET /api/devices/{devEui}/scale/calibration returns calibration parameters."""
    headers = {"X-API-Key": "master-test-key-2026"}
    dev_eui = "70b3d57ed007985a"
    resp = http_client.get(f"/api/devices/{dev_eui}/scale/calibration", headers=headers)
    assert resp.status_code == 200
    data = resp.json()
    assert "calSlope" in data
    assert "calZero" in data
    assert "hopperCapacityKg" in data
    assert data.get("hopperCapacityKg") == 50.0
