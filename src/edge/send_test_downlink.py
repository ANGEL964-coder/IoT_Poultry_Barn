#!/usr/bin/env python3
"""
SDN-LoRa Test Downlink Injector CLI (Phase 1)
--------------------------------------------
Packs 4-byte SDN Header bit-fields, encodes to Base64, and publishes
to ChirpStack v4 Downlink MQTT topic:
  application/<APP_ID>/device/<DEV_EUI>/command/down
"""

import os
import sys
import json
import time
import argparse
from typing import Optional

import paho.mqtt.client as mqtt

# Ensure module path imports sdn_protocol properly
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
from edge.sdn_protocol import (
    pack_sdn_header,
    unpack_sdn_header,
    to_base64_downlink,
    SdnCommandId,
    COMMAND_NAMES,
    SPREADING_FACTORS,
    SAMPLE_RATES
)


def parse_int_auto_base(val: str) -> int:
    """Parses integer supporting hex (0x...) and decimal."""
    val = val.strip()
    if val.lower().startswith("0x"):
        return int(val, 16)
    return int(val)


def build_chirpstack_v4_downlink_json(
    dev_eui: str,
    base64_payload: str,
    f_port: int = 10,
    confirmed: bool = False
) -> str:
    """
    Constructs the JSON schema expected by ChirpStack v4 for queued downlinks.
    """
    payload_obj = {
        "devEui": dev_eui,
        "confirmed": confirmed,
        "fPort": f_port,
        "data": base64_payload
    }
    return json.dumps(payload_obj, indent=2)


def main():
    parser = argparse.ArgumentParser(
        description="Inject 4-byte SDN Header downlink commands via ChirpStack v4 MQTT.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Reconfigure node to SF7, 14 dBm, 60s sample rate:
  python3 send_test_downlink.py --app-id app-1 --dev-eui 0102030405060708 --cmd-id 1 --sf-target 0 --tx-power 14 --sample-rate 1

  # Send Keep-Alive test:
  python3 send_test_downlink.py --app-id app-1 --dev-eui 0102030405060708 --cmd-id 0

  # Set temperature threshold to 25.50 °C (param_value = 2550):
  python3 send_test_downlink.py --app-id app-1 --dev-eui 0102030405060708 --cmd-id 2 --param-value 2550
        """
    )

    # ChirpStack routing identifiers
    parser.add_argument("--app-id", default=os.getenv("CHIRPSTACK_APP_ID", "application-01"),
                        help="ChirpStack Application ID (UUID or slug)")
    parser.add_argument("--dev-eui", default=os.getenv("DEV_EUI", "0102030405060708"),
                        help="Node Device EUI (16 hex chars)")
    parser.add_argument("--f-port", type=int, default=10, help="LoRaWAN Port (default: 10)")
    parser.add_argument("--confirmed", action="store_true", help="Request LoRaWAN Confirmed Downlink (ACK)")

    # SDN Header Fields (4 bytes / 32 bits)
    parser.add_argument("--cmd-id", type=parse_int_auto_base, default=1,
                        help="Command ID (0..15). 0=NOP, 1=RECONFIG_RADIO, 2=TEMP_THRESH, 3=FORCE_SLEEP, 15=RESET")
    parser.add_argument("--sf-target", type=int, default=0,
                        help="Spreading Factor Index (0..7). 0=SF7, 1=SF8, 2=SF9, 3=SF10, 4=SF11, 5=SF12")
    parser.add_argument("--tx-power", type=int, default=14,
                        help="Tx Power in dBm (0..15). Typical LoRa: 2..14 dBm")
    parser.add_argument("--sample-rate", type=int, default=1,
                        help="Sample Rate LUT index (0..7). 0=10s, 1=60s, 2=300s, 3=900s, 4=1800s, 5=3600s")
    parser.add_argument("--payload-mode", type=int, default=0,
                        help="Payload Mode (0..3). 0=Full Telemetry (204B), 1=Critical, 2=Diagnostic")
    parser.add_argument("--sleep-enable", type=int, choices=[0, 1], default=0,
                        help="Deep Sleep enable flag (0=Disabled, 1=Enabled)")
    parser.add_argument("--param-value", type=parse_int_auto_base, default=0,
                        help="Multipurpose 15-bit parameter (0..32767)")

    # MQTT Options
    parser.add_argument("--mqtt-host", default=os.getenv("MQTT_HOST", "localhost"), help="MQTT broker host")
    parser.add_argument("--mqtt-port", type=int, default=int(os.getenv("MQTT_PORT", 1883)), help="MQTT broker port")
    parser.add_argument("--dry-run", action="store_true", help="Pack and print payload without sending over MQTT")

    args = parser.parse_args()

    # 1. Clean DevEUI
    dev_eui = args.dev_eui.replace("-", "").replace(":", "").strip().lower()

    # 2. Pack 32-bit SDN Header into Little-Endian bytes
    try:
        raw_bytes = pack_sdn_header(
            cmd_id=args.cmd_id,
            sf_target=args.sf_target,
            tx_power=args.tx_power,
            sample_rate=args.sample_rate,
            payload_mode=args.payload_mode,
            sleep_enable=args.sleep_enable,
            param_value=args.param_value
        )
    except ValueError as e:
        print(f"\033[91m[ERROR] Validation failed: {e}\033[0m", file=sys.stderr)
        sys.exit(1)

    # 3. Base64 Encode
    b64_payload = to_base64_downlink(raw_bytes)

    # 4. Unpack to verify bit-exact consistency
    unpacked = unpack_sdn_header(raw_bytes)

    # 5. Build ChirpStack Downlink Topic and JSON Payload
    topic = f"application/{args.app_id}/device/{dev_eui}/command/down"
    downlink_json = build_chirpstack_v4_downlink_json(
        dev_eui=dev_eui,
        base64_payload=b64_payload,
        f_port=args.f_port,
        confirmed=args.confirmed
    )

    word32 = unpacked["raw_uint32"]
    bits_str = f"{word32:032b}"
    # Formatted bit blocks: param(15) | sleep(1) | mode(2) | rate(3) | tx(4) | sf(3) | cmd(4)
    pretty_bits = f"{bits_str[0:15]} {bits_str[15]} {bits_str[16:18]} {bits_str[18:21]} {bits_str[21:25]} {bits_str[25:28]} {bits_str[28:32]}"

    print("\n" + "=" * 70)
    print("🛰️  \033[1;36mSDN DOWNLINK HEADER PACKER (32 BITS / 4 BYTES)\033[0m")
    print("=" * 70)
    print(f"  Field breakdown:")
    print(f"    ├─ cmd_id:       0x{args.cmd_id:02X} ({unpacked['cmd_name']})")
    print(f"    ├─ sf_target:    {args.sf_target} ({unpacked['sf_name']})")
    print(f"    ├─ tx_power:     {args.tx_power} dBm")
    print(f"    ├─ sample_rate:  {args.sample_rate} ({unpacked['sample_rate_sec']} seconds)")
    print(f"    ├─ payload_mode: {args.payload_mode} ({unpacked['payload_mode_name']})")
    print(f"    ├─ sleep_enable: {args.sleep_enable} ({'Active' if args.sleep_enable else 'Disabled'})")
    print(f"    └─ param_value:  {args.param_value} (0x{args.param_value:04X})")
    print("-" * 70)
    print(f"  Binary Layout: [param:15][slp:1][mod:2][rate:3][tx:4][sf:3][cmd:4]")
    print(f"  Bits (MSB..LSB): {pretty_bits}")
    print(f"  Hex (LE Bytes):  0x{unpacked['hex']} ({list(raw_bytes)})")
    print(f"  Base64 Payload:  \033[1;32m{b64_payload}\033[0m")
    print(f"  Target Topic:    \033[1;34m{topic}\033[0m")
    print(f"  LoRaWAN Port:    fPort={args.f_port} (Confirmed: {args.confirmed})")
    print("=" * 70)

    if args.dry_run:
        print("\033[93m[DRY RUN] Packet generated successfully. Not published to MQTT.\033[0m")
        return

    # 6. Publish to Mosquitto
    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id="sdn_downlink_injector"
    )

    try:
        client.connect(args.mqtt_host, args.mqtt_port, keepalive=10)
        client.loop_start()

        msg_info = client.publish(topic, downlink_json, qos=1)
        msg_info.wait_for_publish(timeout=5)

        if msg_info.rc == mqtt.MQTT_ERR_SUCCESS:
            print("\n\033[1;92m✔ DOWNLINK ENQUEUED SUCCESSFULLY IN CHIRPSTACK!\033[0m")
            print("ChirpStack will deliver this frame to the Laird RG1 Gateway")
            print("for transmission in the ESP32 node's next RX1/RX2 receive window.")
        else:
            print(f"\033[91m✖ MQTT Publish Error code: {msg_info.rc}\033[0m", file=sys.stderr)

        time.sleep(0.2)
        client.loop_stop()
        client.disconnect()

    except Exception as e:
        print(f"\033[91m✖ Connection failed: {e}\033[0m", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
