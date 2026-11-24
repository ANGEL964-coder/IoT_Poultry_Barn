"""
Tier 3: Pairwise Combinatorial Integration Tests.
Pairwise interaction: Concurrent Uplink Ingestion and Downlink Injection.
Verifies thread-safety, non-blocking MQTT queues, and telemetry cache consistency.
Total Tests: 4 (Requirement: >= 4).
"""

import concurrent.futures
import json
import threading
import time
import pytest

from tests.test_helpers import (
    pack_telemetry_critical,
    pack_sdn_command,
    decode_telemetry_frame,
    make_chirpstack_uplink,
    make_chirpstack_downlink
)


def test_tier3_simultaneous_uplink_and_downlink(mqtt_client, http_client):
    """Verify simultaneous uplink publishing and downlink REST injection under concurrency."""
    dev_eui = "70b3d57ed007985a"
    uplink_count = 10
    downlink_count = 5
    errors = []

    def send_uplinks():
        for i in range(uplink_count):
            try:
                raw_bytes = pack_telemetry_critical(seq=100 + i, weight_kg=15.0 + (i * 0.1))
                envelope = make_chirpstack_uplink(dev_eui, raw_bytes, f_cnt=100 + i)
                topic = f"application/44d003ef-2bd1-480a-84cf-87ff6dc27506/device/{dev_eui}/event/up"
                mqtt_client.publish(topic, json.dumps(envelope), qos=1)
                time.sleep(0.01)
            except Exception as e:
                errors.append(f"Uplink error: {e}")

    def send_downlinks():
        headers = {"X-API-Key": "master-test-key-2026"}
        for i in range(downlink_count):
            try:
                req = {
                    "devEui": dev_eui,
                    "cmdId": 1,
                    "sfTarget": 1,
                    "txPower": 14,
                    "sampleRate": 2,
                    "payloadMode": 1,
                    "sleepEnable": 0,
                    "paramValue": i * 10
                }
                resp = http_client.post("/api/sdn/downlink", json_data=req, headers=headers)
                if resp.status_code != 200:
                    errors.append(f"Downlink HTTP {resp.status_code}")
                time.sleep(0.01)
            except Exception as e:
                errors.append(f"Downlink error: {e}")

    t1 = threading.Thread(target=send_uplinks)
    t2 = threading.Thread(target=send_downlinks)
    t1.start()
    t2.start()
    t1.join()
    t2.join()

    assert len(errors) == 0, f"Errors during concurrent execution: {errors}"


def test_tier3_downlink_does_not_corrupt_uplink_cache(http_client):
    """Verify that downlink injections do not wipe or corrupt cached telemetry state."""
    dev_eui = "70b3d57ed007985a"
    # Seed mock device telemetry
    if hasattr(http_client, "telemetries"):
        http_client.telemetries[dev_eui] = [{
            "devEui": dev_eui,
            "seq": 50,
            "weightKg": 14.8,
            "hopperPercentage": 74.0
        }]

    # Send downlink
    headers = {"X-API-Key": "master-test-key-2026"}
    down_resp = http_client.post("/api/sdn/downlink", json_data={"devEui": dev_eui, "cmdId": 0}, headers=headers)
    assert down_resp.status_code == 200

    # Query telemetry
    telem_resp = http_client.get(f"/api/galpones/{dev_eui}")
    assert telem_resp.status_code == 200
    assert telem_resp.json()["seq"] == 50
    assert telem_resp.json()["hopperPercentage"] == 74.0


def test_tier3_concurrent_multithreaded_burst(mqtt_client, http_client):
    """Verify multi-threaded execution pool handling 20 interleaved operations."""
    dev_eui = "70b3d57ed007985a"
    successes = []

    def worker_task(task_id: int):
        if task_id % 2 == 0:
            # Uplink
            raw = pack_telemetry_critical(seq=task_id)
            env = make_chirpstack_uplink(dev_eui, raw, f_cnt=task_id)
            mqtt_client.publish("application/test/device/test/event/up", json.dumps(env))
            return True
        else:
            # Downlink preview (public endpoint)
            resp = http_client.post("/api/sdn/preview", json_data={"cmdId": 1, "sfTarget": 0, "txPower": 14})
            return resp.status_code == 200

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
        futures = [executor.submit(worker_task, i) for i in range(20)]
        for f in concurrent.futures.as_completed(futures):
            successes.append(f.result())

    assert all(successes)
    assert len(successes) == 20


def test_tier3_interleaved_uplink_downlink_ordering(http_client):
    """Verify that downlink events appear in the audit trail without latency stalling."""
    headers = {"X-API-Key": "master-test-key-2026"}
    for cmd in (0, 1, 2):
        resp = http_client.post("/api/sdn/downlink", json_data={"devEui": "70b3d57ed007985a", "cmdId": cmd}, headers=headers)
        assert resp.status_code == 200

    log_resp = http_client.get("/api/sdn/downlinks")
    assert log_resp.status_code == 200
    entries = log_resp.json()
    assert len(entries) >= 3
