"""
Tier 4: Real-World Scenario 3 - Full Operator Lifecycle: Onboarding, IPC Docs, and Control.
Emulates end-to-end operator workflow: device onboarding, API key provisioning,
Claude Opus IPC documentation orchestration, privileged SDN injection, and key revocation.
"""

import json
import socket
import pytest

from tests.test_helpers import (
    pack_telemetry_critical,
    decode_telemetry_frame,
    build_claude_auth_frame,
    build_claude_user_frame
)

NEW_DEV_EUI = "70b3d57ed0079999"


def test_scenario3_full_lifecycle_operator_flow(http_client, mock_claude_server):
    """
    Step 1: Admin onboards new device (POST /api/devices).
    Step 2: Admin generates new device API key (POST /api/keys).
    Step 3: Connect to Claude Opus IPC socket, authenticate, and request onboarding documentation.
    Step 4: New device streams telemetry; verify SCADA reflection.
    Step 5: Operator executes privileged SDN downlink using new key.
    Step 6: Admin revokes operator key and confirms immediate 401 denial.
    """
    admin_headers = {"X-API-Key": "master-test-key-2026"}

    # Step 1: Onboard device
    dev_payload = {
        "devEui": NEW_DEV_EUI,
        "name": "heltec-v3-galpon-expansion",
        "hopperCapacityKg": 25.0
    }
    dev_resp = http_client.post("/api/devices", json_data=dev_payload, headers=admin_headers)
    assert dev_resp.status_code == 201

    # Step 2: Provision operator API key
    new_operator_key = "operator-key-expansion-farm-99"
    key_resp = http_client.post("/api/keys", json_data={"key": new_operator_key}, headers=admin_headers)
    assert key_resp.status_code == 201

    # Step 3: Orchestrate technical docs via Claude Opus IPC
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(mock_claude_server.sock_path)
    # Auth
    s.sendall(build_claude_auth_frame(mock_claude_server.token).encode("utf-8"))
    auth_resp = s.recv(1024).decode("utf-8")
    assert "auth_ok" in auth_resp

    # Generate documentation
    prompt = f"Generate onboarding documentation for Galpón device {NEW_DEV_EUI}"
    s.sendall(build_claude_user_frame(prompt).encode("utf-8"))
    doc_resp = json.loads(s.recv(4096).decode("utf-8").strip())
    s.close()
    assert doc_resp.get("type") == "assistant"
    assert "Technical Documentation" in doc_resp["message"]["content"]

    # Step 4: Stream telemetry from new device
    telem_raw = pack_telemetry_critical(seq=1, weight_kg=20.0, temperature=23.5)
    decoded = decode_telemetry_frame(telem_raw, hopper_capacity_kg=25.0)
    assert decoded["hopper_percentage"] == 80.0  # 20.0 / 25.0 * 100%

    if hasattr(http_client, "telemetries"):
        http_client.telemetries[NEW_DEV_EUI] = [decoded]

    # Verify SCADA query
    scada_resp = http_client.get(f"/api/galpones/{NEW_DEV_EUI}")
    assert scada_resp.status_code == 200
    assert scada_resp.json()["hopper_percentage"] == 80.0

    # Step 5: Execute SDN command using new operator key
    op_headers = {"X-API-Key": new_operator_key}
    sdn_req = {
        "devEui": NEW_DEV_EUI,
        "cmdId": 1,
        "sfTarget": 0,
        "txPower": 14,
        "sampleRate": 1,
        "payloadMode": 0,
        "sleepEnable": 0,
        "paramValue": 0
    }
    sdn_resp = http_client.post("/api/sdn/downlink", json_data=sdn_req, headers=op_headers)
    assert sdn_resp.status_code == 200

    # Step 6: Revoke key and verify access denied
    if hasattr(http_client, "api_keys"):
        http_client.api_keys.discard(new_operator_key)

    denied_resp = http_client.post("/api/sdn/downlink", json_data=sdn_req, headers=op_headers)
    assert denied_resp.status_code == 401


def test_scenario6_rbac_operational_lifecycle_flow(http_client):
    """
    Tier 4 Real-World Scenario: End-to-End Multi-Role Operational SCADA Lifecycle.
    Phase 1: Unauthenticated user is challenged with 401.
    Phase 2: Operator authenticates and navigates digital twin telemetry.
    Phase 3: Operator attempts hardware tare and SDN downlink -> receives strictly 403 Forbidden.
    Phase 4: Admin authenticates and executes tare, calibration, and downlink reconfiguration -> receives 200.
    Phase 5: Audit log verification confirms all actions recorded.
    """
    dev_eui = "70b3d57ed007985a"

    # --- Phase 1: Unauthenticated Challenge ---
    assert http_client.get("/api/auth/me").status_code == 401
    assert http_client.post(f"/api/devices/{dev_eui}/scale/tare").status_code == 401
    assert http_client.post("/api/sdn/downlink", json_data={"devEui": dev_eui, "cmdId": 0}).status_code == 401

    # --- Phase 2: Operator Authentication ---
    op_login = http_client.post("/api/auth/login", json_data={"username": "operador", "password": "operador123"})
    assert op_login.status_code == 200
    op_token = op_login.json()["token"]
    op_headers = {"Authorization": f"Bearer {op_token}"}

    # Verify operator profile
    me_resp = http_client.get("/api/auth/me", headers=op_headers)
    assert me_resp.status_code == 200
    assert me_resp.json()["role"] == "ROLE_OPERADOR"

    # --- Phase 3: Digital Twin Telemetry Navigation ---
    vision_resp = http_client.get(f"/api/modules/vision/{dev_eui}", headers=op_headers)
    assert vision_resp.status_code == 200
    assert "eggCount" in vision_resp.json()

    scale_resp = http_client.get(f"/api/modules/scale/{dev_eui}", headers=op_headers)
    assert scale_resp.status_code == 200
    assert "netWeightKg" in scale_resp.json()

    climate_resp = http_client.get(f"/api/modules/climate/{dev_eui}", headers=op_headers)
    assert climate_resp.status_code == 200
    assert "temperature" in climate_resp.json()

    # --- Phase 4: Blocked Privilege Escalation ---
    # Operator cannot tare scale
    op_tare = http_client.post(f"/api/devices/{dev_eui}/scale/tare", headers=op_headers)
    assert op_tare.status_code == 403
    assert "Forbidden" in op_tare.text or "denegado" in op_tare.text.lower()

    # Operator cannot calibrate scale
    op_cal = http_client.post(
        f"/api/devices/{dev_eui}/scale/calibrate",
        json_data={"calSlope": 420.5, "knownWeightKg": 10.0},
        headers=op_headers
    )
    assert op_cal.status_code == 403

    # Operator cannot trigger SDN downlinks
    op_dl = http_client.post(
        "/api/sdn/downlink",
        json_data={"devEui": dev_eui, "cmdId": 1},
        headers=op_headers
    )
    assert op_dl.status_code == 403

    # --- Phase 5: Admin Escalation & Execution ---
    admin_login = http_client.post("/api/auth/login", json_data={"username": "admin", "password": "server"})
    assert admin_login.status_code == 200
    admin_token = admin_login.json()["token"]
    admin_headers = {"Authorization": f"Bearer {admin_token}"}

    # Admin tare scale -> 200
    admin_tare = http_client.post(f"/api/devices/{dev_eui}/scale/tare", headers=admin_headers)
    assert admin_tare.status_code == 200
    assert admin_tare.json().get("status") == "SUCCESS"

    # Admin calibrate scale -> 200
    admin_cal = http_client.post(
        f"/api/devices/{dev_eui}/scale/calibrate",
        json_data={"calSlope": 425.0, "calZero": 0.125, "knownWeightKg": 10.0},
        headers=admin_headers
    )
    assert admin_cal.status_code == 200

    # Admin inject SDN downlink -> 200
    admin_dl = http_client.post(
        "/api/sdn/downlink",
        json_data={
            "devEui": dev_eui,
            "cmdId": 1,
            "sfTarget": 1,
            "txPower": 14,
            "sampleRate": 1,
            "payloadMode": 0,
            "sleepEnable": 0,
            "paramValue": 0
        },
        headers=admin_headers
    )
    assert admin_dl.status_code == 200
    assert admin_dl.json().get("status") == "ENQUEUED_IN_CHIRPSTACK"

    # --- Phase 6: Audit Log Verification ---
    audit_resp = http_client.get("/api/sdn/downlinks")
    assert audit_resp.status_code == 200
    audits = audit_resp.json()
    assert len(audits) >= 2
    # Verify tare and downlink recorded
    has_tare = any(a.get("action") == "SCALE_TARE" for a in audits)
    has_dl = any(a.get("status") == "ENQUEUED_IN_CHIRPSTACK" for a in audits)
    assert has_tare and has_dl
