"""
Tier 1: Feature Coverage Tests for F17 (STOMP WebSocket Live Updates).
Covers STOMP subscriptions to /topic/telemetria, /topic/telemetria/{devEui},
/topic/sdn/downlink-events, /topic/status, and WebSocketConfig.java verification.
Total Tests: 6 (Requirement: >= 5).
"""

import os
import pytest

WS_CONFIG_PATH = "/home/server/CONTROLADOR_SDN/dashboard-java/src/main/java/com/sdn/poultry/config/WebSocketConfig.java"


def test_f17_websocket_config_java_declaration():
    """Verify WebSocketConfig.java configures STOMP broker and endpoints."""
    assert os.path.exists(WS_CONFIG_PATH), f"WebSocketConfig.java missing at {WS_CONFIG_PATH}"
    with open(WS_CONFIG_PATH, "r", encoding="utf-8") as f:
        src = f.read()
    assert "@EnableWebSocketMessageBroker" in src
    assert "/ws" in src or "/ws-plain" in src
    assert "/topic" in src


def test_f17_stomp_connection_lifecycle(stomp_client):
    """Verify STOMP client establishes connection and terminates cleanly."""
    assert stomp_client.connected is True
    stomp_client.disconnect()
    assert stomp_client.connected is False


def test_f17_stomp_subscribe_telemetria_broadcast(stomp_client):
    """Verify subscription to /topic/telemetria receives global telemetry broadcasts."""
    q = stomp_client.subscribe("/topic/telemetria")
    assert q.empty()

    telemetry_payload = {
        "devEui": "70b3d57ed007985a",
        "seq": 10,
        "weightKg": 15.5,
        "hopperPercentage": 77.5,
        "temperature": 24.5
    }
    stomp_client.broadcast("/topic/telemetria", telemetry_payload)

    event = q.get(timeout=2.0)
    assert event["devEui"] == "70b3d57ed007985a"
    assert event["seq"] == 10
    assert event["hopperPercentage"] == 77.5


def test_f17_stomp_subscribe_device_specific_topic(stomp_client):
    """Verify subscription to /topic/telemetria/{devEui} receives device-specific frames."""
    dev_eui = "766979864b726282"
    topic = f"/topic/telemetria/{dev_eui}"
    q = stomp_client.subscribe(topic)

    payload = {"devEui": dev_eui, "seq": 55, "weightKg": 18.2}
    stomp_client.broadcast(topic, payload)

    event = q.get(timeout=2.0)
    assert event["devEui"] == dev_eui
    assert event["seq"] == 55


def test_f17_stomp_subscribe_sdn_downlink_events(stomp_client):
    """Verify subscription to /topic/sdn/downlink-events receives downlink execution notifications."""
    q = stomp_client.subscribe("/topic/sdn/downlink-events")

    downlink_event = {
        "timestamp": "2026-11-17T18:00:00Z",
        "devEui": "70b3d57ed007985a",
        "cmdId": 1,
        "cmdName": "RECONFIG_RADIO",
        "base64": "AQ8AAA==",
        "status": "ENQUEUED_IN_CHIRPSTACK"
    }
    stomp_client.broadcast("/topic/sdn/downlink-events", downlink_event)

    event = q.get(timeout=2.0)
    assert event["cmdName"] == "RECONFIG_RADIO"
    assert event["base64"] == "AQ8AAA=="


def test_f17_stomp_subscribe_system_status(stomp_client):
    """Verify subscription to /topic/status receives system health updates."""
    q = stomp_client.subscribe("/topic/status")

    status_event = {
        "status": "CONNECTED",
        "message": "Broker MQTT conectado y escuchando: application/+/device/+/event/up",
        "timestamp": "2026-11-17T18:00:00Z"
    }
    stomp_client.broadcast("/topic/status", status_event)

    event = q.get(timeout=2.0)
    assert event["status"] == "CONNECTED"
    assert "MQTT" in event["message"]
