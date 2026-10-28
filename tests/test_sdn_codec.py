"""
Unit Tests for SDN Header Codec (Phase 1)
"""

import pytest
import struct
from src.edge.sdn_protocol import (
    pack_sdn_header,
    unpack_sdn_header,
    to_base64_downlink,
    from_base64_payload,
    SdnCommandId
)


def test_sdn_header_size():
    """Verify that the packed header is strictly 4 bytes (32 bits)."""
    raw = pack_sdn_header(0, 0, 0, 0, 0, 0, 0)
    assert len(raw) == 4


def test_reconfig_radio_packing():
    """Test standard radio reconfiguration command."""
    # cmd_id=1, sf_target=3 (SF10), tx_power=14 (14 dBm), sample_rate=2 (300s),
    # payload_mode=1 (Critical), sleep_enable=1, param_value=2500
    raw = pack_sdn_header(
        cmd_id=SdnCommandId.RECONFIG_RADIO,
        sf_target=3,
        tx_power=14,
        sample_rate=2,
        payload_mode=1,
        sleep_enable=1,
        param_value=2500
    )

    unpacked = unpack_sdn_header(raw)
    assert unpacked["cmd_id"] == 1
    assert unpacked["cmd_name"] == "RECONFIG_RADIO"
    assert unpacked["sf_target"] == 3
    assert unpacked["sf_name"] == "SF10"
    assert unpacked["tx_power"] == 14
    assert unpacked["sample_rate"] == 2
    assert unpacked["sample_rate_sec"] == 300
    assert unpacked["payload_mode"] == 1
    assert unpacked["payload_mode_name"] == "CRITICAL"
    assert unpacked["sleep_enable"] is True
    assert unpacked["param_value"] == 2500


def test_boundary_max_values():
    """Test maximum possible values for all bitfields without overflow."""
    raw = pack_sdn_header(
        cmd_id=0x0F,       # 4 bits: 15
        sf_target=0x07,    # 3 bits: 7
        tx_power=0x0F,     # 4 bits: 15
        sample_rate=0x07,  # 3 bits: 7
        payload_mode=0x03, # 2 bits: 3
        sleep_enable=1,    # 1 bit:  1
        param_value=0x7FFF # 15 bits: 32767
    )

    assert len(raw) == 4
    # All 32 bits should be 1
    word32 = struct.unpack("<I", raw)[0]
    assert word32 == 0xFFFFFFFF

    unpacked = unpack_sdn_header(raw)
    assert unpacked["cmd_id"] == 15
    assert unpacked["sf_target"] == 7
    assert unpacked["tx_power"] == 15
    assert unpacked["sample_rate"] == 7
    assert unpacked["payload_mode"] == 3
    assert unpacked["sleep_enable"] is True
    assert unpacked["param_value"] == 32767


def test_base64_roundtrip():
    """Verify Base64 encoding/decoding cycle."""
    raw = pack_sdn_header(0x02, 0, 14, 1, 0, 0, 2550) # 25.50 °C threshold
    b64 = to_base64_downlink(raw)
    decoded_raw = from_base64_payload(b64)
    assert decoded_raw == raw

    unpacked = unpack_sdn_header(decoded_raw)
    assert unpacked["cmd_id"] == 2
    assert unpacked["param_value"] == 2550


def test_validation_out_of_bounds():
    """Verify exceptions on out-of-range inputs."""
    with pytest.raises(ValueError):
        pack_sdn_header(cmd_id=16, sf_target=0, tx_power=0, sample_rate=0, payload_mode=0, sleep_enable=0, param_value=0)

    with pytest.raises(ValueError):
        pack_sdn_header(cmd_id=0, sf_target=8, tx_power=0, sample_rate=0, payload_mode=0, sleep_enable=0, param_value=0)

    with pytest.raises(ValueError):
        pack_sdn_header(cmd_id=0, sf_target=0, tx_power=16, sample_rate=0, payload_mode=0, sleep_enable=0, param_value=0)

    with pytest.raises(ValueError):
        pack_sdn_header(cmd_id=0, sf_target=0, tx_power=0, sample_rate=8, payload_mode=0, sleep_enable=0, param_value=0)

    with pytest.raises(ValueError):
        pack_sdn_header(cmd_id=0, sf_target=0, tx_power=0, sample_rate=0, payload_mode=4, sleep_enable=0, param_value=0)

    with pytest.raises(ValueError):
        pack_sdn_header(cmd_id=0, sf_target=0, tx_power=0, sample_rate=0, payload_mode=0, sleep_enable=2, param_value=0)

    with pytest.raises(ValueError):
        pack_sdn_header(cmd_id=0, sf_target=0, tx_power=0, sample_rate=0, payload_mode=0, sleep_enable=0, param_value=32768)
