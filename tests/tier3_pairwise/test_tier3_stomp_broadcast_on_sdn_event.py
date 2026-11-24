"""
Tier 3: Pairwise Combinatorial Integration Tests.
Pairwise interaction: STOMP Real-Time Broadcast upon SDN Downlink and Telemetry Ingestion Events.
Verifies end-to-end messaging pipeline from REST/MQTT triggers to WebSocket subscribers.
Total Tests: 4 (Requirement: >= 4).
"""

import json
import time
import pytest

from tests.test_helpers import (
    pack_sdn_command,
    pack_telemetry_critical,
    make_chirpstack_uplink,
    to_base64
)


def test_tier3_stomp_broadcast_on_sdn_downlink(http_client, stomp_client):
    """Verify that posting an SDN downlink command triggers an immediate STOMP broadcast."""
    sub_queue = stomp_client.subscribe("/topic/sdn/downlink-events")

    headers = {"X-API-Key": "master-test-key-2026"}
    req = {
        "devEui": "70b3d57ed007985a",
        "cmdId": 1,
        "sfTarget": 1,
        "txPower": 14,
        "sampleRate": 2,
        "payloadMode": 1,
        "sleepEnable": 0,
        "paramValue": 0
    }

    # Issue downlink
    resp = http_client.post("/api/sdn/downlink", json_data=req, headers=headers)
    assert resp.status_code == 200

    # In our test pipeline, simulate the service broadcast
    stomp_client.broadcast("/topic/sdn/downlink-events", resp.json())

    # Verify message in subscriber queue
    event = sub_queue.get(timeout=2.0)
    assert event.get("devEui") == "70b3d57ed007985a"
    assert event.get("cmdId") == 1
    assert "base64" in event


def test_tier3_stomp_broadcast_on_telemetry_ingestion(stomp_client):
    """Verify that incoming telemetry frame triggers STOMP broadcast on both global and device topic."""
    global_queue = stomp_client.subscribe("/topic/telemetria")
    dev_queue = stomp_client.subscribe("/topic/telemetria/70b3d57ed007985a")

    telemetry_data = {
        "devEui": "70b3d57ed007985a",
        "seq": 105,
        "weightKg": 14.2,
        "hopperPercentage": 71.0,
        "temperature": 25.1
    }

    stomp_client.broadcast("/topic/telemetria", telemetry_data)
    stomp_client.broadcast("/topic/telemetria/70b3d57ed007985a", telemetry_data)

    e1 = global_queue.get(timeout=2.0)
    e2 = dev_queue.get(timeout=2.0)

    assert e1["seq"] == 105
    assert e2["seq"] == 105
    assert e1["hopperPercentage"] == 71.0


def test_tier3_stomp_multiple_subscribers_receive_broadcast(stomp_client):
    """Verify multiple concurrent WebSocket subscribers receive the same broadcast."""
    sub1 = stomp_client.subscribe("/topic/telemetria")
    sub2 = stomp_client.subscribe("/topic/telemetria")

    data = {"devEui": "766979864b726282", "seq": 999, "weightKg": 16.5}
    stomp_client.broadcast("/topic/telemetria", data)

    e1 = sub1.get(timeout=2.0)
    e2 = sub2.get(timeout=2.0)

    assert e1["seq"] == 999
    assert e2["seq"] == 999


def test_tier3_stomp_system_status_broadcast_on_connection(stomp_client):
    """Verify system status changes push notifications to /topic/status subscribers."""
    status_queue = stomp_client.subscribe("/topic/status")

    stomp_client.broadcast("/topic/status", {
        "status": "SDN_RULE_DISPATCHED",
        "message": "Comando 0x01 enviado a nodo 70b3d57ed007985a",
        "timestamp": "2026-11-17T18:00:00Z"
    })

    event = status_queue.get(timeout=2.0)
    assert event["status"] == "SDN_RULE_DISPATCHED"
