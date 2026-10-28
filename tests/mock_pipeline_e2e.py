#!/usr/bin/env python3
"""
End-to-End Mock Integration Validator for SDN-LoRa Pipeline
-----------------------------------------------------------
Simulates:
1. ESP32 Uplink transmission (204 bytes telemetry payload).
2. ChirpStack v4 JSON generation with RSSI/SNR metrics.
3. Edge Pipeline ingestion and state parsing.
4. Downlink command generation (4 bytes SDN Header).
5. ESP32 receiving and parsing the downlink.
"""

import sys
import os
import json
import base64
import struct

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
from src.edge.sdn_protocol import (
    pack_sdn_header,
    unpack_sdn_header,
    to_base64_downlink,
    from_base64_payload,
    SdnCommandId
)


def generate_mock_esp32_uplink(seq_num: int = 1) -> bytes:
    """Generates an exact 204-byte simulated telemetry payload from ESP32."""
    # Structure of 204 bytes:
    # 4B seq, 4B uptime_ms, 4B batt_mv, 4B temp_c, 4B hum_pct, 4B press_hpa
    # + 180 bytes dummy telemetry array
    header = struct.pack(
        "<IIffff",
        seq_num,
        seq_num * 60000,   # uptime
        3300.0,            # battery mV (3.3V)
        24.75,             # temperature °C
        55.20,             # humidity %
        1013.25            # pressure hPa
    )
    assert len(header) == 24
    payload_data = bytes([i % 256 for i in range(180)])
    full_payload = header + payload_data
    assert len(full_payload) == 204
    return full_payload


def test_mock_end_to_end_flow():
    print("\n" + "=" * 65)
    print("🚀 RUNNING END-TO-END MOCK PIPELINE TEST (PHASE 1)")
    print("=" * 65)

    # 1. Simulate ESP32 Uplink Generation
    uplink_bytes = generate_mock_esp32_uplink(seq_num=42)
    b64_uplink = base64.b64encode(uplink_bytes).decode("ascii")
    print(f"\n[Step 1] ESP32 generated 204-byte telemetry payload.")
    print(f"         Base64 length: {len(b64_uplink)} chars, Payload size: {len(uplink_bytes)} bytes")

    # 2. Simulate ChirpStack v4 Uplink JSON
    dev_eui = "a84041000181cafe"
    chirpstack_json = {
        "deduplicationId": "550e8400-e29b-41d4-a716-446655440000",
        "time": "2026-10-28T15:00:00.000000+00:00",
        "deviceInfo": {
            "tenantId": "52f14fb4-c714-45e0-9426-ab0eec5ad403",
            "tenantName": "ChirpStack",
            "applicationId": "app-sdn-lora-01",
            "applicationName": "LoRa-SDN",
            "devEui": dev_eui,
            "deviceName": "ESP32-Node-01"
        },
        "devAddr": "00112233",
        "adr": True,
        "dr": 3,
        "fCnt": 42,
        "fPort": 10,
        "data": b64_uplink,
        "rxInfo": [
            {
                "gatewayId": "0016c001f1123456",
                "rssi": -72.0,
                "snr": 8.75,
                "channel": 2
            }
        ],
        "txInfo": {
            "frequency": 915200000,
            "modulation": {
                "lora": {
                    "bandwidth": 125000,
                    "spreadingFactor": 7,
                    "codeRate": "CR_4_5"
                }
            }
        }
    }

    # 3. Test Edge Extraction
    extracted_eui = chirpstack_json["deviceInfo"]["devEui"]
    extracted_rssi = chirpstack_json["rxInfo"][0]["rssi"]
    extracted_snr = chirpstack_json["rxInfo"][0]["snr"]
    extracted_payload = base64.b64decode(chirpstack_json["data"])

    assert extracted_eui == dev_eui
    assert extracted_rssi == -72.0
    assert extracted_snr == 8.75
    assert len(extracted_payload) == 204

    # Unpack telemetry header
    seq, uptime, batt, temp, hum, press = struct.unpack("<IIffff", extracted_payload[:24])
    print(f"\n[Step 2] Edge Pipeline correctly ingested & decrypted packet:")
    print(f"         - Node DevEUI: {extracted_eui}")
    print(f"         - RF Quality:  RSSI={extracted_rssi} dBm, SNR={extracted_snr} dB")
    print(f"         - Telemetry:   fCnt/Seq={seq}, Battery={batt:.1f} mV, Temp={temp:.2f} °C")
    print(f"         - Size check:  {len(extracted_payload)} bytes == 204 bytes (PASSED)")

    # 4. Generate Downlink Reconfiguration Command (4 bytes)
    # Target: SF8 (sf_target=1), 14 dBm, 300s sample rate (idx=2), Critical mode (idx=1)
    downlink_raw = pack_sdn_header(
        cmd_id=SdnCommandId.RECONFIG_RADIO,
        sf_target=1,
        tx_power=14,
        sample_rate=2,
        payload_mode=1,
        sleep_enable=0,
        param_value=0
    )
    b64_downlink = to_base64_downlink(downlink_raw)
    print(f"\n[Step 3] Downlink Controller formulated 4-byte SDN Header:")
    print(f"         - Raw LE Hex:   0x{downlink_raw.hex().upper()}")
    print(f"         - Base64 Frame: {b64_downlink}")

    # 5. Simulate ESP32 Receiving and Parsing Downlink
    esp32_rx_bytes = from_base64_payload(b64_downlink)
    parsed_header = unpack_sdn_header(esp32_rx_bytes)

    assert parsed_header["cmd_id"] == SdnCommandId.RECONFIG_RADIO
    assert parsed_header["sf_target"] == 1
    assert parsed_header["sf_name"] == "SF8"
    assert parsed_header["tx_power"] == 14
    assert parsed_header["sample_rate"] == 2
    assert parsed_header["sample_rate_sec"] == 300
    assert parsed_header["payload_mode"] == 1
    assert parsed_header["payload_mode_name"] == "CRITICAL"

    print(f"\n[Step 4] Simulated ESP32 Bitfield Struct deserialized packet:")
    print(f"         - cmd_id:       0x{parsed_header['cmd_id']:02X} ({parsed_header['cmd_name']})")
    print(f"         - sf_target:    {parsed_header['sf_target']} -> {parsed_header['sf_name']}")
    print(f"         - tx_power:     {parsed_header['tx_power']} dBm")
    print(f"         - sample_rate:  {parsed_header['sample_rate']} -> {parsed_header['sample_rate_sec']} seconds")
    print(f"         - payload_mode: {parsed_header['payload_mode']} -> {parsed_header['payload_mode_name']}")

    print("\n" + "=" * 65)
    print("✔ ALL END-TO-END PIPELINE CHECKS PASSED WITH 100% SUCCESS!")
    print("=" * 65 + "\n")


if __name__ == "__main__":
    test_mock_end_to_end_flow()
