"""
Tier 2: Boundary Value Analysis & Corner Cases for Binary Telemetry Payloads.
Covers truncated buffers (<24B), odd lengths (35B, 100B), oversized frames (>204B),
IEEE 754 NaN/Infinity values, and hopper clamping boundaries.
Total Tests: 12 (Requirement: >= 10).
"""

import math
import struct
import pytest

from tests.test_helpers import (
    pack_telemetry_header,
    pack_telemetry_critical,
    pack_telemetry_full,
    decode_telemetry_frame,
    calculate_hopper_percentage,
    FMT_TELEMETRY_HEADER,
    FMT_TELEMETRY_CRITICAL
)


def test_b01_empty_payload_0_bytes():
    """Verify 0-byte payload raises ValueError."""
    with pytest.raises(ValueError, match="Payload insuficiente"):
        decode_telemetry_frame(b"")


def test_b01_truncated_header_10_bytes():
    """Verify 10-byte truncated payload raises ValueError."""
    with pytest.raises(ValueError, match="Payload insuficiente"):
        decode_telemetry_frame(b"\x00" * 10)


def test_b01_truncated_header_23_bytes():
    """Verify 23-byte payload (1 byte short of 24) raises ValueError."""
    with pytest.raises(ValueError, match="Payload insuficiente"):
        decode_telemetry_frame(b"\x00" * 23)


def test_b01_exact_24_bytes_boundary():
    """Verify exactly 24 bytes decodes TelemetryHeader cleanly with default scale values."""
    payload = pack_telemetry_header(1, 1000, 3300.0, 25.0, 50.0, 1013.0)
    assert len(payload) == 24
    decoded = decode_telemetry_frame(payload)
    assert decoded["seq"] == 1
    assert decoded["weight_kg"] == 0.0
    assert decoded["hopper_percentage"] == 0.0
    assert decoded["payload_length"] == 24


def test_b01_odd_length_between_24_and_48_bytes():
    """Verify 35-byte payload decodes header and gracefully defaults scale data without exception."""
    header = pack_telemetry_header(2, 2000, 3200.0, 24.0, 52.0, 1012.0)
    extra_11_bytes = b"\xFF" * 11
    payload = header + extra_11_bytes
    assert len(payload) == 35

    decoded = decode_telemetry_frame(payload)
    assert decoded["seq"] == 2
    assert decoded["weight_kg"] == 0.0
    assert decoded["hopper_percentage"] == 0.0
    assert decoded["payload_length"] == 35


def test_b01_exact_48_bytes_boundary():
    """Verify exactly 48 bytes decodes all header and scale metrics."""
    payload = pack_telemetry_critical(seq=10, weight_kg=10.0)
    assert len(payload) == 48
    decoded = decode_telemetry_frame(payload, hopper_capacity_kg=20.0)
    assert decoded["seq"] == 10
    assert decoded["weight_kg"] == 10.0
    assert decoded["hopper_percentage"] == 50.0
    assert decoded["payload_length"] == 48


def test_b01_odd_length_between_48_and_204_bytes():
    """Verify 100-byte payload decodes first 48 bytes cleanly and reports payload_length=100."""
    crit = pack_telemetry_critical(seq=15, weight_kg=16.0)
    payload = crit + (b"\xAA" * 52)
    assert len(payload) == 100

    decoded = decode_telemetry_frame(payload, hopper_capacity_kg=20.0)
    assert decoded["seq"] == 15
    assert decoded["weight_kg"] == 16.0
    assert decoded["hopper_percentage"] == 80.0
    assert decoded["payload_length"] == 100


def test_b01_exact_204_bytes_boundary():
    """Verify exactly 204 bytes full telemetry frame."""
    payload = pack_telemetry_full(seq=20, weight_kg=14.0)
    assert len(payload) == 204
    decoded = decode_telemetry_frame(payload, hopper_capacity_kg=20.0)
    assert decoded["seq"] == 20
    assert decoded["weight_kg"] == 14.0
    assert decoded["payload_length"] == 204


def test_b01_oversized_payload_300_bytes():
    """Verify oversized 300-byte payload decodes first 48 bytes without buffer overflow."""
    crit = pack_telemetry_critical(seq=99, weight_kg=18.0)
    payload = crit + (b"\x00" * 252)
    assert len(payload) == 300
    decoded = decode_telemetry_frame(payload, hopper_capacity_kg=20.0)
    assert decoded["seq"] == 99
    assert decoded["weight_kg"] == 18.0
    assert decoded["payload_length"] == 300


def test_b01_ieee754_nan_float_handling():
    """Verify IEEE 754 NaN float values deserialize safely and hopper clamping handles NaN as 0.0%."""
    nan_header = struct.pack(
        FMT_TELEMETRY_HEADER,
        1, 1000, float("nan"), float("nan"), 50.0, 1013.0
    )
    decoded = decode_telemetry_frame(nan_header)
    assert math.isnan(decoded["battery_mv"])
    assert math.isnan(decoded["temperature"])
    assert decoded["humidity"] == 50.0
    assert calculate_hopper_percentage(float("nan"), 20.0) == 0.0


def test_b01_ieee754_infinity_float_handling():
    """Verify IEEE 754 +Inf and -Inf float values deserialize safely."""
    inf_header = struct.pack(
        FMT_TELEMETRY_HEADER,
        2, 2000, float("inf"), float("-inf"), 55.0, 1012.0
    )
    decoded = decode_telemetry_frame(inf_header)
    assert math.isinf(decoded["battery_mv"]) and decoded["battery_mv"] > 0
    assert math.isinf(decoded["temperature"]) and decoded["temperature"] < 0


def test_b01_hopper_clamping_exact_boundaries():
    """Verify exact numerical boundaries for hopper capacity clamping."""
    cap = 20.0
    # Exact 0 kg
    assert calculate_hopper_percentage(0.0, cap) == 0.0
    # Negative epsilon
    assert calculate_hopper_percentage(-0.001, cap) == 0.0
    # Half capacity
    assert calculate_hopper_percentage(10.0, cap) == 50.0
    # Exact 20 kg
    assert calculate_hopper_percentage(20.0, cap) == 100.0
    # Over capacity epsilon
    assert calculate_hopper_percentage(20.001, cap) == 100.0
    # Zero capacity division by zero safety
    assert calculate_hopper_percentage(10.0, 0.0) == 0.0
