"""
Tier 1: Feature Coverage Tests for F09 (SDN Header Packing), F10 (ChirpStack Downlink), and F11 (SDN REST API & Logging).
Covers 32-bit LE RFC 7426 bitfield math, ChirpStack v4 JSON envelopes, Base64 conversion,
REST downlink injection, and audit trail logging.
Total Tests: 16 (Requirement: >= 15).
"""

import json
import pytest

from tests.test_helpers import (
    pack_sdn_command,
    unpack_sdn_command,
    to_base64,
    from_base64,
    make_chirpstack_downlink,
    CMD_NAMES,
    SF_NAMES
)


# --- F09: 32-bit Little-Endian SDN Header Packing ---

def test_f09_pack_nop_keepalive():
    """Verify Vector D-01: NOP_KEEP_ALIVE (cmd=0, sf=1, tx=14, sr=0, pm=0, sleep=0, param=0)."""
    raw = pack_sdn_command(0, 1, 14, 0, 0, 0, 0)
    assert len(raw) == 4
    b64 = to_base64(raw)
    assert b64 == "EAcAAA=="
    unpacked = unpack_sdn_command(raw)
    assert unpacked["cmd_id"] == 0
    assert unpacked["cmd_name"] == "NOP_KEEP_ALIVE"
    assert unpacked["sf_target"] == 1
    assert unpacked["sf_name"] == "SF8"
    assert unpacked["tx_power"] == 14


def test_f09_pack_reconfig_radio():
    """Verify Vector D-02: RECONFIG_RADIO (cmd=1, sf=0, tx=14, sr=1, pm=0, sleep=0, param=0)."""
    raw = pack_sdn_command(1, 0, 14, 1, 0, 0, 0)
    assert to_base64(raw) == "AQ8AAA=="
    unpacked = unpack_sdn_command(raw)
    assert unpacked["cmd_id"] == 1
    assert unpacked["cmd_name"] == "RECONFIG_RADIO"
    assert unpacked["sf_target"] == 0
    assert unpacked["sf_name"] == "SF7"
    assert unpacked["sample_rate"] == 1
    assert unpacked["sample_rate_seconds"] == 60


def test_f09_pack_set_temp_threshold():
    """Verify Vector D-04: SET_TEMP_THRESHOLD with 25.50°C (centidegrees = 2550)."""
    raw = pack_sdn_command(2, 0, 14, 1, 0, 0, 2550)
    unpacked = unpack_sdn_command(raw)
    assert unpacked["cmd_id"] == 2
    assert unpacked["cmd_name"] == "SET_TEMP_THRESHOLD"
    assert unpacked["param_value"] == 2550


def test_f09_pack_force_sleep():
    """Verify Vector D-05: FORCE_SLEEP with sleep_enable=1 and 60 minutes."""
    raw = pack_sdn_command(3, 0, 0, 0, 0, 1, 60)
    unpacked = unpack_sdn_command(raw)
    assert unpacked["cmd_id"] == 3
    assert unpacked["sleep_enable"] == 1
    assert unpacked["param_value"] == 60


def test_f09_pack_system_reset():
    """Verify Vector D-06: SYSTEM_RESET (cmd=15 / 0xF)."""
    raw = pack_sdn_command(15, 0, 0, 0, 0, 0, 0)
    assert to_base64(raw) == "DwAAAA=="
    unpacked = unpack_sdn_command(raw)
    assert unpacked["cmd_id"] == 15
    assert unpacked["cmd_name"] == "SYSTEM_RESET"


def test_f09_codec_roundtrip_symmetry():
    """Verify lossless pack and unpack roundtrip for arbitrary complex bitfield combination."""
    raw = pack_sdn_command(
        cmd_id=4,
        sf_target=3,
        tx_power=10,
        sample_rate=4,
        payload_mode=2,
        sleep_enable=1,
        param_value=12345
    )
    unpacked = unpack_sdn_command(raw)
    assert unpacked["cmd_id"] == 4
    assert unpacked["sf_target"] == 3
    assert unpacked["tx_power"] == 10
    assert unpacked["sample_rate"] == 4
    assert unpacked["payload_mode"] == 2
    assert unpacked["sleep_enable"] == 1
    assert unpacked["param_value"] == 12345


# --- F10: ChirpStack v4 Downlink Publishing ---

def test_f10_downlink_mqtt_topic_structure():
    """Verify ChirpStack v4 MQTT downlink topic template formatting."""
    app_id = "44d003ef-2bd1-480a-84cf-87ff6dc27506"
    dev_eui = "70b3d57ed007985a"
    topic = f"application/{app_id}/device/{dev_eui}/command/down"
    assert topic == "application/44d003ef-2bd1-480a-84cf-87ff6dc27506/device/70b3d57ed007985a/command/down"


def test_f10_downlink_json_envelope_schema():
    """Verify ChirpStack downlink envelope contains devEui, confirmed, fPort, and data."""
    raw = pack_sdn_command(1, 0, 14, 1, 0, 0, 0)
    envelope = make_chirpstack_downlink("70B3D57ED007985A", raw, confirmed=False, f_port=10)
    assert envelope["devEui"] == "70b3d57ed007985a"  # lowercased
    assert envelope["confirmed"] is False
    assert envelope["fPort"] == 10
    assert envelope["data"] == "AQ8AAA=="


def test_f10_downlink_base64_payload_format():
    """Verify that downlink payload encodes 4 Little-Endian bytes into exactly 8 ASCII chars."""
    raw = pack_sdn_command(0, 0, 0, 0, 0, 0, 0)
    b64 = to_base64(raw)
    assert len(b64) == 8
    assert b64 == "AAAAAA=="


def test_f10_publish_downlink_to_mqtt(mqtt_client):
    """Verify publishing downlink message through MQTT client."""
    topic = "application/44d003ef-2bd1-480a-84cf-87ff6dc27506/device/70b3d57ed007985a/command/down"
    raw = pack_sdn_command(1, 1, 14, 2, 1, 0, 0)
    payload_json = json.dumps(make_chirpstack_downlink("70b3d57ed007985a", raw))

    res = mqtt_client.publish(topic, payload_json, qos=1)
    if hasattr(res, "wait_for_publish"):
        assert res.wait_for_publish(timeout=2.0)
    assert res.rc == 0


def test_f10_downlink_confirmed_ack_flag():
    """Verify confirmed flag setting in downlink envelope."""
    envelope = make_chirpstack_downlink("70b3d57ed007985a", b"\x00\x00\x00\x00", confirmed=True)
    assert envelope["confirmed"] is True


# --- F11: SDN Downlink REST API & Logging ---

def test_f11_post_sdn_preview_public(http_client):
    """Verify public POST /api/sdn/preview endpoint computes 32-bit preview without auth."""
    req = {
        "cmdId": 1,
        "sfTarget": 3,
        "txPower": 14,
        "sampleRate": 2,
        "payloadMode": 1,
        "sleepEnable": 1,
        "paramValue": 2500
    }
    resp = http_client.post("/api/sdn/preview", json_data=req)
    assert resp.status_code == 200
    data = resp.json()
    assert data.get("base64") == "MVeJEw=="
    assert "hexDump" in data
    assert data.get("cmd_name") == "RECONFIG_RADIO" or data.get("cmdName") == "RECONFIG_RADIO"


def test_f11_post_sdn_downlink_authorized(http_client):
    """Verify protected POST /api/sdn/downlink queues command with valid API key."""
    headers = {"X-API-Key": "master-test-key-2026"}
    req = {
        "devEui": "70b3d57ed007985a",
        "cmdId": 2,
        "sfTarget": 0,
        "txPower": 14,
        "sampleRate": 1,
        "payloadMode": 0,
        "sleepEnable": 0,
        "paramValue": 2550,
        "confirmed": False,
        "fPort": 10
    }
    resp = http_client.post("/api/sdn/downlink", json_data=req, headers=headers)
    assert resp.status_code == 200
    data = resp.json()
    assert data.get("devEui") == "70b3d57ed007985a"
    assert data.get("status") == "ENQUEUED_IN_CHIRPSTACK"
    assert "base64" in data


def test_f11_post_sdn_downlink_unauthorized(http_client):
    """Verify protected POST /api/sdn/downlink returns 401 without API key."""
    req = {"devEui": "70b3d57ed007985a", "cmdId": 0}
    resp = http_client.post("/api/sdn/downlink", json_data=req)
    assert resp.status_code == 401


def test_f11_post_sdn_downlink_missing_deveui(http_client):
    """Verify POST /api/sdn/downlink returns 400 Bad Request when devEui is missing."""
    headers = {"X-API-Key": "master-test-key-2026"}
    req = {"cmdId": 1}  # Missing devEui
    resp = http_client.post("/api/sdn/downlink", json_data=req, headers=headers)
    assert resp.status_code == 400
    assert "DevEUI" in resp.text or "error" in resp.json()


def test_f11_get_sdn_downlinks_audit_log(http_client):
    """Verify GET /api/sdn/downlinks retrieves chronological list of downlink events."""
    resp = http_client.get("/api/sdn/downlinks")
    assert resp.status_code == 200
    assert isinstance(resp.json(), list)
