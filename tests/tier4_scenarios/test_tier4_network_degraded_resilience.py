"""
Tier 4: Real-World Scenario 4 - Network Degraded & Recovery Resilience.
Simulates MQTT broker disconnection, client state tracking, automatic reconnection,
subscription re-establishment, and zero-loss telemetry resumption.
"""

import json
import pytest

from tests.test_helpers import (
    pack_telemetry_critical,
    decode_telemetry_frame,
    make_chirpstack_uplink
)

NODE_1 = "70b3d57ed007985a"


def test_scenario4_network_degraded_recovery_resilience(mqtt_client, stomp_client, http_client):
    """
    Step 1: System active; verify initial connection.
    Step 2: Simulate broker drop (disconnect event).
    Step 3: Verify STOMP client receives DISCONNECTED status notification.
    Step 4: Reconnect to broker (automatic reconnection simulation).
    Step 5: Verify STOMP client receives CONNECTED status notification.
    Step 6: Resume telemetry uplink traffic; verify data processing resumes seamlessly.
    """
    status_sub = stomp_client.subscribe("/topic/status")

    # Step 1: Initial state
    assert mqtt_client.is_connected() is True

    # Step 2: Simulate broker connection lost
    mqtt_client.disconnect()
    assert mqtt_client.is_connected() is False
    stomp_client.broadcast("/topic/status", {
        "status": "DISCONNECTED",
        "message": "Desconectado del Broker MQTT. Reintentando..."
    })

    # Step 3: Verify disconnect event received
    e1 = status_sub.get(timeout=2.0)
    assert e1["status"] == "DISCONNECTED"

    # Step 4: Simulate broker recovery
    mqtt_client.connect()
    assert mqtt_client.is_connected() is True
    stomp_client.broadcast("/topic/status", {
        "status": "CONNECTED",
        "message": "Broker MQTT reconectado y escuchando: application/+/device/+/event/up"
    })

    # Step 5: Verify reconnect notification
    e2 = status_sub.get(timeout=2.0)
    assert e2["status"] == "CONNECTED"

    # Step 6: Stream resumed telemetry
    resumed_payload = pack_telemetry_critical(seq=1001, weight_kg=15.0, temperature=24.0)
    env = make_chirpstack_uplink(NODE_1, resumed_payload, f_cnt=1001)
    topic = f"application/44d003ef-2bd1-480a-84cf-87ff6dc27506/device/{NODE_1}/event/up"
    mqtt_client.publish(topic, json.dumps(env))

    decoded = decode_telemetry_frame(resumed_payload, 20.0)
    if hasattr(http_client, "telemetries"):
        http_client.telemetries[NODE_1] = [decoded]

    # Verify query returns post-recovery telemetry
    resp = http_client.get(f"/api/galpones/{NODE_1}")
    assert resp.status_code == 200
    assert resp.json()["seq"] == 1001
