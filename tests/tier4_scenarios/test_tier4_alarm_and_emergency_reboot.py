"""
Tier 4: Real-World Scenario 2 - Closed-Loop SDN Adaptation & Alarm Flow.
Simulates a high-temperature alarm in Galpón 2, followed by closed-loop SDN reconfiguration
(sample rate increase and SF clamp), and an emergency force-sleep / reset cycle.
"""

import json
import pytest

from tests.test_helpers import (
    pack_telemetry_critical,
    pack_sdn_command,
    unpack_sdn_command,
    decode_telemetry_frame,
    make_chirpstack_uplink,
    make_chirpstack_downlink,
    to_base64
)

TARGET_NODE = "766979864b726282"  # Galpón 2


def test_scenario2_alarm_detection_and_sdn_reconfiguration(http_client, stomp_client, mqtt_client):
    """
    Step 1: Node 2 sends critical high-temperature frame (38.5°C).
    Step 2: Detect alarm condition (temp > 32.0°C).
    Step 3: Operator issues SDN reconfig command (cmd=1, SF8/DR2, fast 10s sample rate).
    Step 4: Verify 32-bit LE packing and ChirpStack downlink emission.
    Step 5: Issue emergency deep-sleep command (cmd=3, sleep_enable=1, 30 minutes).
    Step 6: Verify audit trail in /api/sdn/downlinks.
    """
    # Step 1: Ingest high-temp frame
    high_temp_raw = pack_telemetry_critical(
        seq=500,
        battery_mv=3050.0,
        temperature=38.5,  # Alarm condition
        humidity=78.0,
        weight_kg=12.0
    )
    decoded = decode_telemetry_frame(high_temp_raw, 20.0)
    assert decoded["temperature"] == 38.5

    # Step 2: Alarm evaluation
    is_alarm = decoded["temperature"] > 32.0 or decoded["battery_mv"] < 3100.0
    assert is_alarm is True

    # Step 3: Dispatch SDN reconfig command
    headers = {"X-API-Key": "master-test-key-2026"}
    reconfig_req = {
        "devEui": TARGET_NODE,
        "cmdId": 1,           # RECONFIG_RADIO
        "sfTarget": 1,        # SF8
        "txPower": 14,        # 14 dBm
        "sampleRate": 0,      # 10s fast sample rate
        "payloadMode": 1,     # Critical 48B mode
        "sleepEnable": 0,
        "paramValue": 0
    }
    reconfig_resp = http_client.post("/api/sdn/downlink", json_data=reconfig_req, headers=headers)
    assert reconfig_resp.status_code == 200
    reconfig_audit = reconfig_resp.json()
    assert reconfig_audit["base64"] == "EUcAAA=="
    assert reconfig_audit["status"] == "ENQUEUED_IN_CHIRPSTACK"
    assert reconfig_audit["devEui"] == TARGET_NODE

    # Step 4: Verify 32-bit unpack symmetry
    packed_bytes = pack_sdn_command(1, 1, 14, 0, 1, 0, 0)
    unpacked = unpack_sdn_command(packed_bytes)
    assert unpacked["cmd_name"] == "RECONFIG_RADIO"
    assert unpacked["sf_name"] == "SF8"
    assert unpacked["sample_rate_seconds"] == 10

    # Step 5: Issue emergency force sleep command
    sleep_req = {
        "devEui": TARGET_NODE,
        "cmdId": 3,           # FORCE_SLEEP
        "sfTarget": 0,
        "txPower": 0,
        "sampleRate": 0,
        "payloadMode": 0,
        "sleepEnable": 1,     # Enable sleep
        "paramValue": 30      # 30 minutes
    }
    sleep_resp = http_client.post("/api/sdn/downlink", json_data=sleep_req, headers=headers)
    assert sleep_resp.status_code == 200

    # Step 6: Verify audit trail
    history_resp = http_client.get("/api/sdn/downlinks")
    assert history_resp.status_code == 200
    events = history_resp.json()
    assert len(events) >= 2
    assert any(e.get("devEui") == TARGET_NODE for e in events)
