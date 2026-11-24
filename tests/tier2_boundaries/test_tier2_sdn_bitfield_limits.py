"""
Tier 2: Boundary Value Analysis & Limit Tests for 32-bit LE SDN Bitfield Codec.
Covers 0x00000000 min, 0xFFFFFFFF max, out-of-bounds validations, buffer lengths,
and Spreading Factor / TX power limits.
Total Tests: 11 (Requirement: >= 10).
"""

import pytest

from tests.test_helpers import (
    pack_sdn_command,
    unpack_sdn_command,
    to_base64,
    from_base64
)


def test_b02_min_boundary_all_zeros():
    """Verify Vector D-07: Minimum boundary where all bitfields are 0 (Word32 = 0x00000000)."""
    raw = pack_sdn_command(0, 0, 0, 0, 0, 0, 0)
    assert raw == b"\x00\x00\x00\x00"
    assert to_base64(raw) == "AAAAAA=="
    unpacked = unpack_sdn_command(raw)
    assert unpacked["word32"] == 0
    assert unpacked["word32_hex"] == "0x00000000"
    assert unpacked["cmd_id"] == 0
    assert unpacked["sf_target"] == 0
    assert unpacked["tx_power"] == 0
    assert unpacked["sample_rate"] == 0
    assert unpacked["payload_mode"] == 0
    assert unpacked["sleep_enable"] == 0
    assert unpacked["param_value"] == 0


def test_b02_max_boundary_all_ones():
    """Verify Vector D-08: Maximum boundary where all bitfields are at max (Word32 = 0xFFFFFFFF)."""
    raw = pack_sdn_command(15, 7, 15, 7, 3, 1, 32767)
    assert raw == b"\xFF\xFF\xFF\xFF"
    assert to_base64(raw) == "/////w=="
    unpacked = unpack_sdn_command(raw)
    assert unpacked["word32"] == 0xFFFFFFFF
    assert unpacked["word32_hex"] == "0xFFFFFFFF"
    assert unpacked["cmd_id"] == 15
    assert unpacked["sf_target"] == 7
    assert unpacked["tx_power"] == 15
    assert unpacked["sample_rate"] == 7
    assert unpacked["payload_mode"] == 3
    assert unpacked["sleep_enable"] == 1
    assert unpacked["param_value"] == 32767


def test_b02_out_of_bounds_cmd_id():
    """Verify cmd_id = 16 (> 4 bits) raises ValueError."""
    with pytest.raises(ValueError, match="cmd_id out of range"):
        pack_sdn_command(cmd_id=16)


def test_b02_out_of_bounds_sf_target():
    """Verify sf_target = 8 (> 3 bits) raises ValueError."""
    with pytest.raises(ValueError, match="sf_target out of range"):
        pack_sdn_command(sf_target=8)


def test_b02_out_of_bounds_tx_power():
    """Verify tx_power = 16 (> 4 bits) raises ValueError."""
    with pytest.raises(ValueError, match="tx_power out of range"):
        pack_sdn_command(tx_power=16)


def test_b02_out_of_bounds_sample_rate():
    """Verify sample_rate = 8 (> 3 bits) raises ValueError."""
    with pytest.raises(ValueError, match="sample_rate out of range"):
        pack_sdn_command(sample_rate=8)


def test_b02_out_of_bounds_payload_mode():
    """Verify payload_mode = 4 (> 2 bits) raises ValueError."""
    with pytest.raises(ValueError, match="payload_mode out of range"):
        pack_sdn_command(payload_mode=4)


def test_b02_out_of_bounds_sleep_enable():
    """Verify sleep_enable = 2 (> 1 bit) raises ValueError."""
    with pytest.raises(ValueError, match="sleep_enable out of range"):
        pack_sdn_command(sleep_enable=2)


def test_b02_out_of_bounds_param_value():
    """Verify param_value = 32768 (> 15 bits) raises ValueError."""
    with pytest.raises(ValueError, match="param_value out of range"):
        pack_sdn_command(param_value=32768)


def test_b02_negative_values_rejected():
    """Verify negative values raise ValueError."""
    with pytest.raises(ValueError):
        pack_sdn_command(cmd_id=-1)
    with pytest.raises(ValueError):
        pack_sdn_command(param_value=-50)


def test_b02_sdn_unpacker_buffer_length_boundaries():
    """Verify unpacker boundary behavior on short buffers (< 4 bytes) and trailing bytes."""
    with pytest.raises(ValueError, match="at least 4 bytes"):
        unpack_sdn_command(b"\x00\x00\x00")  # 3 bytes

    # Buffer with extra trailing bytes should unpack first 4 bytes
    unpacked = unpack_sdn_command(b"\x00\x00\x00\x00\xDE\xAD\xBE\xEF")
    assert unpacked["word32"] == 0


# --- Hardware Scale Calibration Boundary & Negative Tests ---

def test_b01_scale_calibration_negative_slope(http_client):
    """Verify calibration with negative slope is rejected with HTTP 400 Bad Request."""
    admin_headers = {"X-API-Key": "master-test-key-2026"}
    dev_eui = "70b3d57ed007985a"
    bad_payload = {
        "calSlope": -15.5,
        "calZero": 0.125,
        "knownWeightKg": 10.0
    }
    resp = http_client.post(f"/api/devices/{dev_eui}/scale/calibrate", json_data=bad_payload, headers=admin_headers)
    assert resp.status_code == 400
    assert "invalida" in resp.text.lower() or "error" in resp.text.lower() or "slope" in resp.text.lower()


def test_b01_scale_calibration_out_of_bounds_span(http_client):
    """Verify calibration with span weight exceeding hopper capacity (> 50 kg) is rejected with 400."""
    admin_headers = {"X-API-Key": "master-test-key-2026"}
    dev_eui = "70b3d57ed007985a"
    over_capacity_payload = {
        "calSlope": 420.5,
        "calZero": 0.125,
        "knownWeightKg": 125.0  # Capacity is 50.0 kg
    }
    resp = http_client.post(f"/api/devices/{dev_eui}/scale/calibrate", json_data=over_capacity_payload, headers=admin_headers)
    assert resp.status_code == 400
    assert "capacidad" in resp.text.lower() or "excede" in resp.text.lower() or "error" in resp.text.lower()


def test_b01_scale_calibration_negative_span(http_client):
    """Verify calibration with negative span weight is rejected with 400."""
    admin_headers = {"X-API-Key": "master-test-key-2026"}
    dev_eui = "70b3d57ed007985a"
    negative_span_payload = {
        "calSlope": 420.5,
        "calZero": 0.125,
        "knownWeightKg": -5.0
    }
    resp = http_client.post(f"/api/devices/{dev_eui}/scale/calibrate", json_data=negative_span_payload, headers=admin_headers)
    assert resp.status_code == 400
