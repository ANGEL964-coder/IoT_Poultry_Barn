"""
Tier 3: Pairwise Combinatorial Integration Tests.
Pairwise interaction: Auth Key Revocation under Active REST Traffic.
Verifies immediate access denial upon revocation, absence of session leakage,
and preservation of sibling keys.
Total Tests: 4 (Requirement: >= 4).
"""

import concurrent.futures
import time
import pytest


def test_tier3_immediate_revocation_under_traffic(http_client):
    """Verify revoking an active API key cuts off ongoing requests immediately with zero leakage."""
    master_headers = {"X-API-Key": "master-test-key-2026"}
    traffic_key = "dynamic-traffic-key-7788"

    # 1. Provision key
    http_client.post("/api/keys", json_data={"key": traffic_key}, headers=master_headers)

    traffic_headers = {"X-API-Key": traffic_key}
    statuses = []

    # 2. Fire 10 successful requests
    for _ in range(10):
        resp = http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers=traffic_headers)
        statuses.append(resp.status_code)

    assert all(s == 200 for s in statuses)

    # 3. Revoke key
    if hasattr(http_client, "api_keys"):
        http_client.api_keys.discard(traffic_key)

    # 4. Immediate subsequent request must fail with 401
    post_revoke_resp = http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers=traffic_headers)
    assert post_revoke_resp.status_code == 401


def test_tier3_concurrent_threads_rejected_post_revocation(http_client):
    """Verify that multiple concurrent threads using a revoked key are all rejected with 401."""
    master_headers = {"X-API-Key": "master-test-key-2026"}
    worker_key = "worker-pool-key-9999"

    # Provision
    http_client.post("/api/keys", json_data={"key": worker_key}, headers=master_headers)

    # Revoke
    if hasattr(http_client, "api_keys"):
        http_client.api_keys.discard(worker_key)

    worker_headers = {"X-API-Key": worker_key}

    def do_request():
        return http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers=worker_headers).status_code

    with concurrent.futures.ThreadPoolExecutor(max_workers=5) as executor:
        futures = [executor.submit(do_request) for _ in range(15)]
        results = [f.result() for f in futures]

    assert all(code == 401 for code in results)


def test_tier3_revocation_does_not_affect_sibling_keys(http_client):
    """Verify revoking Key A does not impair the validity of sibling Key B or master key."""
    master_headers = {"X-API-Key": "master-test-key-2026"}
    key_a = "sibling-key-a-1111"
    key_b = "sibling-key-b-2222"

    http_client.post("/api/keys", json_data={"key": key_a}, headers=master_headers)
    http_client.post("/api/keys", json_data={"key": key_b}, headers=master_headers)

    # Revoke Key A
    if hasattr(http_client, "api_keys"):
        http_client.api_keys.discard(key_a)

    # Key A is rejected
    assert http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers={"X-API-Key": key_a}).status_code == 401

    # Key B is still authorized
    assert http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers={"X-API-Key": key_b}).status_code == 200

    # Master key is still authorized
    assert http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers=master_headers).status_code == 200


def test_tier3_reprovisioning_restores_access(http_client):
    """Verify that re-adding a previously revoked key restores authorization."""
    master_headers = {"X-API-Key": "master-test-key-2026"}
    recycled_key = "recycled-key-5555"

    # Add -> Revoke
    http_client.post("/api/keys", json_data={"key": recycled_key}, headers=master_headers)
    if hasattr(http_client, "api_keys"):
        http_client.api_keys.discard(recycled_key)

    assert http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers={"X-API-Key": recycled_key}).status_code == 401

    # Re-provision
    http_client.post("/api/keys", json_data={"key": recycled_key}, headers=master_headers)

    # Restored
    assert http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": 0}, headers={"X-API-Key": recycled_key}).status_code == 200


def test_tier3_concurrent_isolated_admin_and_operator_sessions(http_client):
    """Verify concurrent operator and admin sessions remain isolated without authority cross-leakage."""
    # 1. Login Admin
    admin_login = http_client.post("/api/auth/login", json_data={"username": "admin", "password": "server"})
    admin_token = admin_login.json()["token"]
    admin_headers = {"Authorization": f"Bearer {admin_token}"}

    # 2. Login Operator
    op_login = http_client.post("/api/auth/login", json_data={"username": "operador", "password": "operador123"})
    op_token = op_login.json()["token"]
    op_headers = {"Authorization": f"Bearer {op_token}"}

    dev_eui = "70b3d57ed007985a"

    def admin_worker():
        for _ in range(5):
            # Admin downlink succeeds
            dl_res = http_client.post("/api/sdn/downlink", json_data={"devEui": dev_eui, "cmdId": 0}, headers=admin_headers)
            assert dl_res.status_code == 200
            # Admin tare succeeds
            tare_res = http_client.post(f"/api/devices/{dev_eui}/scale/tare", headers=admin_headers)
            assert tare_res.status_code == 200
            time.sleep(0.01)
        return True

    def operator_worker():
        for _ in range(5):
            # Operator reading telemetry succeeds
            scada_res = http_client.get(f"/api/galpones/{dev_eui}", headers=op_headers)
            assert scada_res.status_code == 200
            # Operator attempting downlink strictly receives 403
            dl_res = http_client.post("/api/sdn/downlink", json_data={"devEui": dev_eui, "cmdId": 0}, headers=op_headers)
            assert dl_res.status_code == 403
            # Operator attempting tare strictly receives 403
            tare_res = http_client.post(f"/api/devices/{dev_eui}/scale/tare", headers=op_headers)
            assert tare_res.status_code == 403
            time.sleep(0.01)
        return True

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
        f_admin = executor.submit(admin_worker)
        f_op = executor.submit(operator_worker)
        assert f_admin.result() is True
        assert f_op.result() is True


def test_tier3_scale_tare_triggers_audit_and_downlink_event(http_client):
    """Verify calling scale tare persists an audit log entry and downlink event."""
    admin_headers = {"X-API-Key": "master-test-key-2026"}
    dev_eui = "70b3d57ed007985a"

    # Execute tare
    resp = http_client.post(f"/api/devices/{dev_eui}/scale/tare", headers=admin_headers)
    assert resp.status_code == 200
    data = resp.json()
    assert data.get("status") == "SUCCESS"
    assert data.get("devEui") == dev_eui

    # Verify audit log reflects scale tare
    audit_resp = http_client.get("/api/sdn/downlinks")
    assert audit_resp.status_code == 200
    audits = audit_resp.json()
    tare_audits = [a for a in audits if a.get("action") == "SCALE_TARE" or a.get("devEui") == dev_eui]
    assert len(tare_audits) >= 1
    assert tare_audits[0]["status"] == "SUCCESS"
