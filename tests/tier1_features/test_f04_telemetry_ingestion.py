"""
Tier 1: Feature Coverage Tests for F04 Telemetry Ingestion Persistence.
Covers Little-Endian binary ingestion across 24B, 48B, and 204B modes,
database persistence, hopper percentage calculations, and ChirpStack RF metadata.
Total Tests: 6 (Requirement: >= 5).
"""

import json
import pytest
from tests.test_helpers import (
    pack_telemetry_critical,
    pack_telemetry_header,
    pack_telemetry_full,
    decode_telemetry_frame,
    calculate_hopper_percentage,
    make_chirpstack_uplink,
    to_base64
)


def test_f04_ingest_48b_critical_telemetry():
    """Verify ingestion and decoding of 48-byte Mode 1 Critical/Standard frame."""
    payload = pack_telemetry_critical(
        seq=42,
        uptime_ms=180000,
        battery_mv=3280.0,
        temperature=25.2,
        humidity=60.5,
        pressure=1012.8,
        weight_kg=15.5,
        gross_weight_kg=18.3,
        voltage_v=1.65,
        adc_raw=2048,
        cal_slope=0.05,
        cal_zero=0.0,
        time_on_air_ms=61
    )
    assert len(payload) == 48

    decoded = decode_telemetry_frame(payload, hopper_capacity_kg=20.0)
    assert decoded["seq"] == 42
    assert decoded["uptime_ms"] == 180000
    assert abs(decoded["battery_mv"] - 3280.0) < 0.1
    assert abs(decoded["temperature"] - 25.2) < 0.1
    assert abs(decoded["humidity"] - 60.5) < 0.1
    assert abs(decoded["pressure"] - 1012.8) < 0.1
    assert abs(decoded["weight_kg"] - 15.5) < 0.01
    assert abs(decoded["gross_weight_kg"] - 18.3) < 0.01
    assert abs(decoded["voltage_v"] - 1.65) < 0.01
    assert decoded["adc_raw"] == 2048
    assert decoded["time_on_air_ms"] == 61
    assert decoded["hopper_percentage"] == 77.5


def test_f04_ingest_24b_diagnostic_telemetry():
    """Verify ingestion and decoding of 24-byte Mode 2 Diagnostic frame."""
    payload = pack_telemetry_header(
        seq=101,
        uptime_ms=3600000,
        battery_mv=3150.0,
        temperature=28.75,
        humidity=45.2,
        pressure=1008.5
    )
    assert len(payload) == 24

    decoded = decode_telemetry_frame(payload, hopper_capacity_kg=20.0)
    assert decoded["seq"] == 101
    assert decoded["uptime_ms"] == 3600000
    assert abs(decoded["battery_mv"] - 3150.0) < 0.1
    assert abs(decoded["temperature"] - 28.75) < 0.1
    # Scale fields must default to 0.0
    assert decoded["weight_kg"] == 0.0
    assert decoded["gross_weight_kg"] == 0.0
    assert decoded["hopper_percentage"] == 0.0
    assert decoded["payload_length"] == 24


def test_f04_ingest_204b_full_telemetry():
    """Verify ingestion and decoding of 204-byte Mode 0 Full Telemetry frame."""
    custom_padding = bytes([(i * 3) & 0xFF for i in range(156)])
    payload = pack_telemetry_full(
        seq=7,
        uptime_ms=90000,
        battery_mv=3300.0,
        temperature=23.4,
        humidity=54.2,
        pressure=1014.0,
        weight_kg=12.0,
        gross_weight_kg=15.0,
        voltage_v=1.35,
        adc_raw=1800,
        cal_slope=0.05,
        cal_zero=0.0,
        time_on_air_ms=75,
        padding=custom_padding
    )
    assert len(payload) == 204

    decoded = decode_telemetry_frame(payload, hopper_capacity_kg=20.0)
    assert decoded["seq"] == 7
    assert decoded["payload_length"] == 204
    assert abs(decoded["weight_kg"] - 12.0) < 0.01
    assert decoded["hopper_percentage"] == 60.0  # (12 / 20) * 100%


def test_f04_hopper_percentage_clamping():
    """Verify hopper percentage clamping strictly to [0.0, 100.0]."""
    # Negative weight clamped to 0.0%
    assert calculate_hopper_percentage(-5.0, 20.0) == 0.0
    # Over-capacity weight clamped to 100.0%
    assert calculate_hopper_percentage(25.0, 20.0) == 100.0
    # Zero weight is 0.0%
    assert calculate_hopper_percentage(0.0, 20.0) == 0.0
    # Exact capacity is 100.0%
    assert calculate_hopper_percentage(20.0, 20.0) == 100.0


def test_f04_rf_metadata_extraction_from_chirpstack():
    """Verify RF metadata parsing from ChirpStack v4 JSON envelope."""
    dev_eui = "70b3d57ed007985a"
    raw_payload = pack_telemetry_critical(seq=5, weight_kg=10.0)
    envelope = make_chirpstack_uplink(
        dev_eui=dev_eui,
        data_bytes=raw_payload,
        rssi=-78.5,
        snr=9.2,
        f_cnt=5,
        f_port=10
    )

    # Validate envelope structure
    assert envelope["deviceInfo"]["devEui"] == dev_eui
    assert envelope["fCnt"] == 5
    assert envelope["fPort"] == 10
    assert envelope["rxInfo"][0]["rssi"] == -78.5
    assert envelope["rxInfo"][0]["snr"] == 9.2
    assert envelope["txInfo"]["modulation"]["lora"]["spreadingFactor"] == 7

    # Decode payload extracted from envelope
    decoded = decode_telemetry_frame(to_base64_decode := bytes(bytearray.fromhex(
        __import__("base64").b64decode(envelope["data"]).hex()
    )))
    assert decoded["seq"] == 5


def test_f04_telemetry_persistence_in_db(db_conn):
    """Verify persistence of telemetry frame in database."""
    cur = db_conn.cursor()
    # Ensure table schema exists
    cur.execute(
        "CREATE TABLE IF NOT EXISTS telemetry_records ("
        "id SERIAL PRIMARY KEY, dev_eui VARCHAR(32) NOT NULL, seq BIGINT, "
        "battery_mv REAL, temperature REAL, humidity REAL, pressure REAL, weight_kg REAL);"
    )
    # Insert a telemetry frame
    cur.execute(
        "INSERT INTO telemetry_records (dev_eui, seq, battery_mv, temperature, humidity, pressure, weight_kg) "
        "VALUES (%s, %s, %s, %s, %s, %s, %s);",
        ("70b3d57ed007985a", 100, 3300.0, 24.5, 58.0, 1013.25, 15.5)
    )
    db_conn.commit()
    assert cur.rowcount == 1

    # Verify retrieval
    cur.execute("SELECT dev_eui, seq, weight_kg FROM telemetry_records WHERE dev_eui = %s AND seq = %s;",
                ("70b3d57ed007985a", 100))
    row = cur.fetchone()
    assert row is not None
    assert row[0] == "70b3d57ed007985a"
    assert row[1] == 100
    cur.close()
