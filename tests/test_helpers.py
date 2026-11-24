"""
Canonical Test Helpers and Protocol Codecs for E2E Testing Track.
Covers Little-Endian telemetry packing (24B, 48B, 204B), 32-bit LE RFC 7426 SDN bitfields,
ChirpStack v4 JSON envelopes, IEEE 754 float handlers, and Claude Opus IPC framing.
"""

import base64
import glob
import json
import math
import os
import struct
from typing import Any, Dict, List, Optional, Tuple, Union

# Protocol Struct Formats (All Little-Endian)
FMT_TELEMETRY_HEADER = "<IIffff"              # 24 bytes
FMT_POULTRY_SCALE_DATA = "<fffHffH"           # 24 bytes
FMT_TELEMETRY_CRITICAL = "<IIfffffffHffH"     # 48 bytes
FMT_TELEMETRY_FULL = "<IIfffffffHffH156s"     # 204 bytes
FMT_SDN_HEADER = "<I"                         # 4 bytes (32-bit word)

# Lookup Tables
CMD_NAMES = [
    "NOP_KEEP_ALIVE",      # 0x0
    "RECONFIG_RADIO",      # 0x1
    "SET_TEMP_THRESHOLD",  # 0x2
    "FORCE_SLEEP",         # 0x3
    "SET_P2P_RELAY",       # 0x4
    "SET_ALARM_HYST",      # 0x5
    "REQ_DIAGNOSTICS",     # 0x6
    "SET_TX_POWER",        # 0x7
    "SET_SAMPLE_RATE",     # 0x8
    "CALIBRATE_ADC",       # 0x9
    "SET_FAILSAFE",        # 0xA
    "CLEAR_STATS",         # 0xB
    "PING_GATEWAY",        # 0xC
    "SYNC_TIME",           # 0xD
    "CUSTOM_RULE",         # 0xE
    "SYSTEM_RESET",        # 0xF
]

SF_NAMES = ["SF7", "SF8", "SF9", "SF10", "SF11", "SF12", "RESERVED_6", "RESERVED_7"]
SAMPLE_RATE_INTERVALS = [10, 60, 300, 900, 1800, 3600, 14400, 86400]
PAYLOAD_MODE_NAMES = ["FULL_TELEMETRY", "CRITICAL", "DIAGNOSTIC", "RESERVED"]


# --- Telemetry Codecs ---

def pack_telemetry_header(
    seq: int,
    uptime_ms: int,
    battery_mv: float,
    temperature: float,
    humidity: float,
    pressure: float
) -> bytes:
    """Pack 24-byte Mode 2 Diagnostic Telemetry Header in Little-Endian."""
    return struct.pack(
        FMT_TELEMETRY_HEADER,
        seq & 0xFFFFFFFF,
        uptime_ms & 0xFFFFFFFF,
        float(battery_mv),
        float(temperature),
        float(humidity),
        float(pressure)
    )


def unpack_telemetry_header(raw_bytes: bytes) -> Dict[str, Any]:
    """Unpack 24-byte TelemetryHeader."""
    if len(raw_bytes) < 24:
        raise ValueError(f"Payload insufficient for TelemetryHeader: got {len(raw_bytes)} bytes, expected at least 24")
    seq, uptime_ms, battery_mv, temperature, humidity, pressure = struct.unpack(
        FMT_TELEMETRY_HEADER, raw_bytes[:24]
    )
    return {
        "seq": seq,
        "uptime_ms": uptime_ms,
        "battery_mv": battery_mv,
        "temperature": temperature,
        "humidity": humidity,
        "pressure": pressure,
    }


def pack_scale_data(
    weight_kg: float,
    gross_weight_kg: float,
    voltage_v: float,
    adc_raw: int,
    cal_slope: float,
    cal_zero: float,
    time_on_air_ms: int
) -> bytes:
    """Pack 24-byte PoultryScaleData extension in Little-Endian."""
    return struct.pack(
        FMT_POULTRY_SCALE_DATA,
        float(weight_kg),
        float(gross_weight_kg),
        float(voltage_v),
        adc_raw & 0xFFFF,
        float(cal_slope),
        float(cal_zero),
        time_on_air_ms & 0xFFFF
    )


def unpack_scale_data(raw_bytes: bytes) -> Dict[str, Any]:
    """Unpack 24-byte PoultryScaleData."""
    if len(raw_bytes) < 24:
        raise ValueError(f"Payload insufficient for PoultryScaleData: got {len(raw_bytes)} bytes, expected 24")
    weight_kg, gross_weight_kg, voltage_v, adc_raw, cal_slope, cal_zero, time_on_air_ms = struct.unpack(
        FMT_POULTRY_SCALE_DATA, raw_bytes[:24]
    )
    return {
        "weight_kg": weight_kg,
        "gross_weight_kg": gross_weight_kg,
        "voltage_v": voltage_v,
        "adc_raw": adc_raw,
        "cal_slope": cal_slope,
        "cal_zero": cal_zero,
        "time_on_air_ms": time_on_air_ms,
    }


def pack_telemetry_critical(
    seq: int = 1,
    uptime_ms: int = 60000,
    battery_mv: float = 3300.0,
    temperature: float = 24.5,
    humidity: float = 58.0,
    pressure: float = 1013.25,
    weight_kg: float = 15.5,
    gross_weight_kg: float = 18.2,
    voltage_v: float = 1.65,
    adc_raw: int = 2048,
    cal_slope: float = 0.05,
    cal_zero: float = 0.0,
    time_on_air_ms: int = 61
) -> bytes:
    """Pack 48-byte Mode 1 Critical / Standard Telemetry Frame."""
    header = pack_telemetry_header(seq, uptime_ms, battery_mv, temperature, humidity, pressure)
    scale = pack_scale_data(weight_kg, gross_weight_kg, voltage_v, adc_raw, cal_slope, cal_zero, time_on_air_ms)
    return header + scale


def pack_telemetry_full(
    seq: int = 1,
    uptime_ms: int = 60000,
    battery_mv: float = 3300.0,
    temperature: float = 24.5,
    humidity: float = 58.0,
    pressure: float = 1013.25,
    weight_kg: float = 15.5,
    gross_weight_kg: float = 18.2,
    voltage_v: float = 1.65,
    adc_raw: int = 2048,
    cal_slope: float = 0.05,
    cal_zero: float = 0.0,
    time_on_air_ms: int = 61,
    padding: Optional[bytes] = None
) -> bytes:
    """Pack 204-byte Mode 0 Full Telemetry Frame (48 bytes + 156 bytes padding)."""
    crit = pack_telemetry_critical(
        seq, uptime_ms, battery_mv, temperature, humidity, pressure,
        weight_kg, gross_weight_kg, voltage_v, adc_raw, cal_slope, cal_zero, time_on_air_ms
    )
    if padding is None:
        padding = bytes([(i + seq) & 0xFF for i in range(156)])
    elif len(padding) != 156:
        padding = (padding + bytes(156))[:156]
    return crit + padding


def calculate_hopper_percentage(weight_kg: float, capacity_kg: float = 20.0) -> float:
    """Calculate feed hopper percentage clamped strictly to [0.0, 100.0]."""
    if math.isnan(weight_kg):
        return 0.0
    if capacity_kg <= 0.0:
        return 0.0
    pct = (weight_kg / capacity_kg) * 100.0
    if pct < 0.0:
        return 0.0
    if pct > 100.0:
        return 100.0
    return round(pct, 2)


def decode_telemetry_frame(payload_bytes: bytes, hopper_capacity_kg: float = 20.0) -> Dict[str, Any]:
    """
    Mirror Java TelemetryDecoder.decode(byte[], float).
    Returns a unified dict representation of the decoded telemetry frame.
    """
    if len(payload_bytes) < 24:
        raise ValueError("Payload insuficiente (< 24 bytes)")

    result = unpack_telemetry_header(payload_bytes[:24])
    result["payload_length"] = len(payload_bytes)

    if len(payload_bytes) >= 48:
        scale = unpack_scale_data(payload_bytes[24:48])
        result.update(scale)
        result["hopper_percentage"] = calculate_hopper_percentage(scale["weight_kg"], hopper_capacity_kg)
    else:
        result.update({
            "weight_kg": 0.0,
            "gross_weight_kg": 0.0,
            "voltage_v": 0.0,
            "adc_raw": 0,
            "cal_slope": 0.0,
            "cal_zero": 0.0,
            "time_on_air_ms": 0,
            "hopper_percentage": 0.0,
        })

    return result


# --- SDN Control Header Codecs (RFC 7426) ---

def pack_sdn_command(
    cmd_id: int = 0,
    sf_target: int = 0,
    tx_power: int = 0,
    sample_rate: int = 0,
    payload_mode: int = 0,
    sleep_enable: int = 0,
    param_value: int = 0,
    strict_bounds: bool = True
) -> bytes:
    """
    Pack 32-bit Little-Endian RFC 7426 SDN Control Header.
    Bitfield Layout:
      [3:0]   cmd_id       (4 bits, 0-15)
      [6:4]   sf_target    (3 bits, 0-7)
      [10:7]  tx_power     (4 bits, 0-15)
      [13:11] sample_rate  (3 bits, 0-7)
      [15:14] payload_mode (2 bits, 0-3)
      [16]    sleep_enable (1 bit, 0-1)
      [31:17] param_value  (15 bits, 0-32767)
    """
    if strict_bounds:
        if not (0 <= cmd_id <= 15):
            raise ValueError(f"cmd_id out of range (0..15): {cmd_id}")
        if not (0 <= sf_target <= 7):
            raise ValueError(f"sf_target out of range (0..7): {sf_target}")
        if not (0 <= tx_power <= 15):
            raise ValueError(f"tx_power out of range (0..15): {tx_power}")
        if not (0 <= sample_rate <= 7):
            raise ValueError(f"sample_rate out of range (0..7): {sample_rate}")
        if not (0 <= payload_mode <= 3):
            raise ValueError(f"payload_mode out of range (0..3): {payload_mode}")
        if not (0 <= sleep_enable <= 1):
            raise ValueError(f"sleep_enable out of range (0..1): {sleep_enable}")
        if not (0 <= param_value <= 32767):
            raise ValueError(f"param_value out of range (0..32767): {param_value}")

    word32 = (
        (cmd_id & 0x0F)
        | ((sf_target & 0x07) << 4)
        | ((tx_power & 0x0F) << 7)
        | ((sample_rate & 0x07) << 11)
        | ((payload_mode & 0x03) << 14)
        | ((sleep_enable & 0x01) << 16)
        | ((param_value & 0x7FFF) << 17)
    )
    return struct.pack(FMT_SDN_HEADER, word32)


def unpack_sdn_command(raw_bytes: bytes, strict_length: bool = False) -> Dict[str, Any]:
    """Unpack 32-bit Little-Endian SDN Control Header."""
    if len(raw_bytes) < 4:
        raise ValueError(f"SDN Header requires at least 4 bytes: got {len(raw_bytes)}")
    if strict_length and len(raw_bytes) != 4:
        raise ValueError(f"SDN Header requires exactly 4 bytes: got {len(raw_bytes)}")

    word32 = struct.unpack(FMT_SDN_HEADER, raw_bytes[:4])[0]
    cmd_id = word32 & 0x0F
    sf_target = (word32 >> 4) & 0x07
    tx_power = (word32 >> 7) & 0x0F
    sample_rate = (word32 >> 11) & 0x07
    payload_mode = (word32 >> 14) & 0x03
    sleep_enable = (word32 >> 16) & 0x01
    param_value = (word32 >> 17) & 0x7FFF

    return {
        "word32": word32,
        "word32_hex": f"0x{word32:08X}",
        "cmd_id": cmd_id,
        "cmd_name": CMD_NAMES[cmd_id] if cmd_id < len(CMD_NAMES) else f"CUSTOM_{cmd_id}",
        "sf_target": sf_target,
        "sf_name": SF_NAMES[sf_target] if sf_target < len(SF_NAMES) else f"SF_{sf_target}",
        "tx_power": tx_power,
        "sample_rate": sample_rate,
        "sample_rate_seconds": SAMPLE_RATE_INTERVALS[sample_rate] if sample_rate < len(SAMPLE_RATE_INTERVALS) else 0,
        "payload_mode": payload_mode,
        "payload_mode_name": PAYLOAD_MODE_NAMES[payload_mode] if payload_mode < len(PAYLOAD_MODE_NAMES) else "UNKNOWN",
        "sleep_enable": sleep_enable,
        "param_value": param_value,
    }


def to_base64(raw_bytes: bytes) -> str:
    """Encode bytes to ASCII Base64."""
    return base64.b64encode(raw_bytes).decode("ascii")


def from_base64(b64_str: str) -> bytes:
    """Decode ASCII Base64 to bytes."""
    return base64.b64decode(b64_str)


# --- ChirpStack v4 JSON Envelope Builders ---

def make_chirpstack_uplink(
    dev_eui: str,
    data_bytes: bytes,
    rssi: float = -72.0,
    snr: float = 8.75,
    f_cnt: int = 1,
    f_port: int = 10,
    app_id: str = "44d003ef-2bd1-480a-84cf-87ff6dc27506",
    device_name: Optional[str] = None
) -> Dict[str, Any]:
    """Generate a standard ChirpStack v4 JSON uplink envelope."""
    dev_eui_clean = dev_eui.lower().strip()
    if device_name is None:
        device_name = f"heltec-v3-{dev_eui_clean[-4:]}"

    return {
        "deduplicationId": f"dedup-{dev_eui_clean}-{f_cnt}",
        "time": "2026-11-17T18:00:00.000000Z",
        "deviceInfo": {
            "applicationId": app_id,
            "applicationName": "lora-poultry",
            "devEui": dev_eui_clean,
            "deviceName": device_name,
        },
        "devAddr": "01655b38",
        "fCnt": f_cnt,
        "fPort": f_port,
        "data": to_base64(data_bytes),
        "rxInfo": [
            {
                "gatewayId": "c0ee40ffff29424b",
                "rssi": rssi,
                "snr": snr,
            }
        ],
        "txInfo": {
            "frequency": 902300000,
            "modulation": {
                "lora": {
                    "bandwidth": 125000,
                    "spreadingFactor": 7,
                    "codeRate": "CR_4_5"
                }
            }
        }
    }


def make_chirpstack_downlink(
    dev_eui: str,
    data_bytes_or_b64: Union[bytes, str],
    confirmed: bool = False,
    f_port: int = 10
) -> Dict[str, Any]:
    """Generate a standard ChirpStack v4 JSON downlink envelope."""
    if isinstance(data_bytes_or_b64, bytes):
        b64_payload = to_base64(data_bytes_or_b64)
    else:
        b64_payload = str(data_bytes_or_b64)

    return {
        "devEui": dev_eui.lower().strip(),
        "confirmed": confirmed,
        "fPort": f_port,
        "data": b64_payload
    }


# --- Claude Opus IPC Framing Helpers ---

def build_claude_auth_frame(token: str) -> str:
    """Build newline-delimited JSON auth handshake for Claude Opus IPC."""
    return json.dumps({"type": "auth", "token": token}) + "\n"


def build_claude_user_frame(prompt: str, role: str = "user", priority: str = "next") -> str:
    """Build newline-delimited JSON user prompt request for Claude Opus IPC."""
    return json.dumps({
        "type": "user",
        "message": {
            "role": role,
            "content": prompt
        },
        "priority": priority
    }) + "\n"


def parse_claude_stream(buffer_str: str) -> List[Dict[str, Any]]:
    """Parse newline-delimited JSON frames from a stream buffer."""
    results = []
    for line in buffer_str.split("\n"):
        line = line.strip()
        if line:
            try:
                results.append(json.loads(line))
            except json.JSONDecodeError:
                pass
    return results


def discover_claude_token() -> Optional[str]:
    """Discover peerToken from ~/.claude/sessions/*.key dynamically."""
    pattern = os.path.expanduser("~/.claude/sessions/*.key")
    matches = glob.glob(pattern)
    for match in sorted(matches, reverse=True):
        try:
            with open(match, "r", encoding="utf-8") as f:
                data = json.load(f)
                token = data.get("peerToken")
                if token:
                    return token
        except Exception:
            continue
    return None
