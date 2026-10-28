"""
SDN Protocol Specification & Codec for LoRaWAN (Phase 1)
According to RFC 7426 and SDN-LoRa specification.

Header Structure (32 bits / 4 bytes):
+--------------------+----------------+------------------+---------------------+
| Bits               | Field          | Width            | Valid Range         |
+--------------------+----------------+------------------+---------------------+
| 0 - 3              | cmd_id         | 4 bits           | 0 - 15 (0x0 - 0xF)  |
| 4 - 6              | sf_target      | 3 bits           | 0 - 7               |
| 7 - 10             | tx_power       | 4 bits           | 0 - 15 (dBm)        |
| 11 - 13            | sample_rate    | 3 bits           | 0 - 7 (LUT index)   |
| 14 - 15            | payload_mode   | 2 bits           | 0 - 3               |
| 16                 | sleep_enable   | 1 bit            | 0 - 1 (bool)        |
| 17 - 31            | param_value    | 15 bits          | 0 - 32,767          |
+--------------------+----------------+------------------+---------------------+
Total: 32 bits (4 bytes)
"""

import struct
import base64
from enum import IntEnum
from typing import Dict, Any, Tuple


class SdnCommandId(IntEnum):
    NOP_KEEP_ALIVE      = 0x00
    RECONFIG_RADIO      = 0x01
    SET_TEMP_THRESHOLD  = 0x02
    FORCE_SLEEP         = 0x03
    SET_P2P_RELAY       = 0x04
    # 0x05 - 0x0E reserved for future rules
    SYSTEM_RESET        = 0x0F


class SdnPayloadMode(IntEnum):
    FULL_TELEMETRY = 0  # Sends all sensor readings (~204 bytes)
    CRITICAL       = 1  # Sends only the variable triggering an alarm
    DIAGNOSTIC     = 2  # Only MCU health / battery
    RESERVED       = 3


# Spreading Factor lookup table: Index 0..5 -> SF7..SF12
SPREADING_FACTORS = {
    0: "SF7",
    1: "SF8",
    2: "SF9",
    3: "SF10",
    4: "SF11",
    5: "SF12",
    6: "RESERVED",
    7: "RESERVED"
}

# Sample Rate lookup table: Index 0..7 -> interval in seconds
SAMPLE_RATES = {
    0: 10,       # 10s (Test mode / critical)
    1: 60,       # 60s
    2: 300,      # 5 min
    3: 900,      # 15 min
    4: 1800,     # 30 min
    5: 3600,     # 1 hour (Low consumption)
    6: 14400,    # 4 hours
    7: 86400     # 24 hours (Ultra low consumption)
}

COMMAND_NAMES = {
    SdnCommandId.NOP_KEEP_ALIVE: "NOP / KEEP_ALIVE",
    SdnCommandId.RECONFIG_RADIO: "RECONFIG_RADIO",
    SdnCommandId.SET_TEMP_THRESHOLD: "SET_TEMP_THRESHOLD",
    SdnCommandId.FORCE_SLEEP: "FORCE_SLEEP",
    SdnCommandId.SET_P2P_RELAY: "SET_P2P_RELAY",
    SdnCommandId.SYSTEM_RESET: "SYSTEM_RESET"
}


def validate_sdn_fields(
    cmd_id: int,
    sf_target: int,
    tx_power: int,
    sample_rate: int,
    payload_mode: int,
    sleep_enable: int,
    param_value: int
) -> None:
    """Validates that each field is within its bit-field boundaries."""
    if not (0 <= cmd_id <= 0x0F):
        raise ValueError(f"cmd_id out of range (0..15): got {cmd_id}")
    if not (0 <= sf_target <= 0x07):
        raise ValueError(f"sf_target out of range (0..7): got {sf_target}")
    if not (0 <= tx_power <= 0x0F):
        raise ValueError(f"tx_power out of range (0..15): got {tx_power}")
    if not (0 <= sample_rate <= 0x07):
        raise ValueError(f"sample_rate out of range (0..7): got {sample_rate}")
    if not (0 <= payload_mode <= 0x03):
        raise ValueError(f"payload_mode out of range (0..3): got {payload_mode}")
    if sleep_enable not in (0, 1):
        raise ValueError(f"sleep_enable out of range (0..1): got {sleep_enable}")
    if not (0 <= param_value <= 0x7FFF):
        raise ValueError(f"param_value out of range (0..32767): got {param_value}")


def pack_sdn_header(
    cmd_id: int,
    sf_target: int,
    tx_power: int,
    sample_rate: int,
    payload_mode: int,
    sleep_enable: int,
    param_value: int
) -> bytes:
    """
    Packs SDN fields into a 32-bit unsigned integer (little-endian byte order).
    Little-endian matches the Xtensa/ESP32 native memory layout for bitfields.
    Returns 4 bytes.
    """
    validate_sdn_fields(cmd_id, sf_target, tx_power, sample_rate, payload_mode, sleep_enable, param_value)

    word32 = (
        (cmd_id & 0x0F) |
        ((sf_target & 0x07) << 4) |
        ((tx_power & 0x0F) << 7) |
        ((sample_rate & 0x07) << 11) |
        ((payload_mode & 0x03) << 14) |
        ((sleep_enable & 0x01) << 16) |
        ((param_value & 0x7FFF) << 17)
    )
    return struct.pack("<I", word32)


def unpack_sdn_header(raw_bytes: bytes) -> Dict[str, Any]:
    """
    Unpacks a 4-byte buffer into a dictionary representing the SdnHeader bitfields.
    """
    if len(raw_bytes) != 4:
        raise ValueError(f"SdnHeader requires exactly 4 bytes, received {len(raw_bytes)}")

    word32 = struct.unpack("<I", raw_bytes)[0]

    cmd_id = word32 & 0x0F
    sf_target = (word32 >> 4) & 0x07
    tx_power = (word32 >> 7) & 0x0F
    sample_rate = (word32 >> 11) & 0x07
    payload_mode = (word32 >> 14) & 0x03
    sleep_enable = (word32 >> 16) & 0x01
    param_value = (word32 >> 17) & 0x7FFF

    cmd_desc = COMMAND_NAMES.get(cmd_id, f"CUSTOM_RULE_0x{cmd_id:02X}")
    sf_desc = SPREADING_FACTORS.get(sf_target, "UNKNOWN")
    rate_desc = f"{SAMPLE_RATES.get(sample_rate, 0)}s"

    try:
        mode_desc = SdnPayloadMode(payload_mode).name
    except ValueError:
        mode_desc = f"RESERVED({payload_mode})"

    return {
        "raw_uint32": word32,
        "hex": raw_bytes.hex().upper(),
        "cmd_id": cmd_id,
        "cmd_name": cmd_desc,
        "sf_target": sf_target,
        "sf_name": sf_desc,
        "tx_power": tx_power,
        "sample_rate": sample_rate,
        "sample_rate_sec": SAMPLE_RATES.get(sample_rate, 0),
        "payload_mode": payload_mode,
        "payload_mode_name": mode_desc,
        "sleep_enable": bool(sleep_enable),
        "param_value": param_value
    }


def to_base64_downlink(raw_bytes: bytes) -> str:
    """Encodes bytes to base64 ASCII string as expected by ChirpStack."""
    return base64.b64encode(raw_bytes).decode("ascii")


def from_base64_payload(b64_str: str) -> bytes:
    """Decodes ChirpStack base64 payload to raw bytes."""
    return base64.b64decode(b64_str)
