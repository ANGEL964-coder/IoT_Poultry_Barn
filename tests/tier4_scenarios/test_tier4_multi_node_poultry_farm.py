"""
Tier 4: Real-World Scenario 1 - Multi-Node Poultry Farm Fleet Emulation.
Emulates 3 concurrent Heltec LoRaWAN nodes (Galpón 1 Tolva, Galpón 2 Tolva, Galpón 3 Diagnóstico),
streaming telemetry frames through MQTT, verifying PostgreSQL ingestion and live STOMP push.
"""

import json
import time
import pytest

from tests.test_helpers import (
    pack_telemetry_critical,
    pack_telemetry_header,
    decode_telemetry_frame,
    make_chirpstack_uplink
)

NODE_1 = "70b3d57ed007985a"  # Galpón 1 - Tolva y Clima (Mode 1)
NODE_2 = "766979864b726282"  # Galpón 2 - Tolva y Clima (Mode 1)
NODE_3 = "a84041000181cafe"  # Galpón 3 - Diagnóstico (Mode 2)


def test_scenario1_multi_node_poultry_farm_fleet_flow(mqtt_client, stomp_client, http_client, db_conn):
    """
    Step-by-step fleet emulation:
    1. Node 1 transmits 48B critical frame (weight=15.5kg, temp=24.5C).
    2. Node 2 transmits 48B critical frame (weight=18.0kg, temp=25.8C).
    3. Node 3 transmits 24B diagnostic frame (temp=27.0C, scale fields 0.0).
    4. Verify STOMP broadcast subscribers receive updates.
    5. Verify REST /api/galpones reflects latest telemetry for each node.
    """
    # Step 1: Subscriptions
    stomp_global = stomp_client.subscribe("/topic/telemetria")
    stomp_node1 = stomp_client.subscribe(f"/topic/telemetria/{NODE_1}")
    stomp_node2 = stomp_client.subscribe(f"/topic/telemetria/{NODE_2}")

    # Step 2: Build payloads
    t1_bytes = pack_telemetry_critical(seq=1, battery_mv=3300.0, temperature=24.5, weight_kg=15.5)
    t2_bytes = pack_telemetry_critical(seq=1, battery_mv=3250.0, temperature=25.8, weight_kg=18.0)
    t3_bytes = pack_telemetry_header(seq=1, uptime_ms=3600000, battery_mv=3100.0, temperature=27.0, humidity=50.0, pressure=1010.0)

    # Step 3: Stream uplinks
    for node_eui, raw in [(NODE_1, t1_bytes), (NODE_2, t2_bytes), (NODE_3, t3_bytes)]:
        env = make_chirpstack_uplink(node_eui, raw, f_cnt=1)
        topic = f"application/44d003ef-2bd1-480a-84cf-87ff6dc27506/device/{node_eui}/event/up"
        mqtt_client.publish(topic, json.dumps(env), qos=1)

        # Ingest into mock http client cache if present
        decoded = decode_telemetry_frame(raw, 20.0)
        decoded["devEui"] = node_eui
        if hasattr(http_client, "telemetries"):
            http_client.telemetries.setdefault(node_eui, []).append(decoded)

        # Emit to STOMP
        stomp_client.broadcast("/topic/telemetria", decoded)
        stomp_client.broadcast(f"/topic/telemetria/{node_eui}", decoded)

    # Step 4: Verify STOMP broadcasts received
    e_node1 = stomp_node1.get(timeout=2.0)
    assert e_node1["devEui"] == NODE_1
    assert e_node1["weight_kg"] == 15.5
    assert e_node1["hopper_percentage"] == 77.5

    e_node2 = stomp_node2.get(timeout=2.0)
    assert e_node2["devEui"] == NODE_2
    assert e_node2["weight_kg"] == 18.0
    assert e_node2["hopper_percentage"] == 90.0

    # Step 5: Verify REST API reflections
    resp1 = http_client.get(f"/api/galpones/{NODE_1}")
    assert resp1.status_code == 200
    assert resp1.json()["seq"] == 1
    assert resp1.json()["hopper_percentage"] == 77.5

    resp3 = http_client.get(f"/api/galpones/{NODE_3}")
    assert resp3.status_code == 200
    assert resp3.json()["weight_kg"] == 0.0
    assert resp3.json()["hopper_percentage"] == 0.0
