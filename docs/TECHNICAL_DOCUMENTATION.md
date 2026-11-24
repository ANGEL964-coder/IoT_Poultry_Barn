# Technical Architecture & System Specification: SDN LoRaWAN Poultry Dashboard
**Document Version:** 1.0.0 (Production Release)
**Engine:** Spring Boot 3.3.5 / Java 21 LTS
**Orchestration / Delegation:** Claude Opus IPC (`/run/user/1000/cc-socks/2894.sock`)
**Generated At:** 2026-11-17T20:20:38.391095840Z
**Generation Source Mode:** UNKNOWN

---

## 1. Executive Summary & End-to-End System Topology

The **SDN LoRaWAN Poultry Monitoring and Control Dashboard** provides a mission-critical telemetry ingestion pipeline, real-time visualization interface, and Software-Defined Networking (SDN) control plane for industrial poultry barn management.

### Data Plane & Control Plane Topology
```
┌─────────────────────────────────┐
│  Heltec WiFi LoRa 32 V3 Nodes   │
│  - HX711 Load Cell (Scale)      │
│  - BME280 Ambient Sensor        │
└────────────────┬────────────────┘
                 │ LoRaWAN US915 RF (902-928 MHz)
                 ▼
┌─────────────────────────────────┐
│  Laird Sentrius RG1 Gateway     │
│  - Semtech SX1301/SX1308 Base   │
└────────────────┬────────────────┘
                 │ UDP Packet Forwarder
                 ▼
┌─────────────────────────────────┐
│  ChirpStack v4 Network Server   │
│  - MAC De-duplication & Routing │
└────────────────┬────────────────┘
                 │ MQTT (JSON frames with Base64 payloads)
                 ▼
┌─────────────────────────────────┐
│  Mosquitto MQTT Broker (1883)   │
│  - Topic: application/+/device/+/event/up
└────────────────┬────────────────┘
                 │ TCP Stream
                 ▼
┌─────────────────────────────────────────────────────────────┐
│  Spring Boot 3.3.5 Backend (Port 8085)                      │
│  - TelemetryDecoder: IEEE 754 Little-Endian Binary Parser   │
│  - SdnHeaderPacker: 32-Bit RFC 7426 Downlink Formatter      │
│  - Security Pipeline: RateLimiter (120/min) + API Key (SHA-256)
│  - Claude Opus IPC Client (Unix Domain Socket)             │
└───────┬──────────────────────────────┬──────────────────────┘
        │ STOMP over WebSockets        │ JDBC / JPA
        ▼                              ▼
┌────────────────────────┐   ┌────────────────────────────────┐
│  Browser Web Client    │   │  PostgreSQL Container (5432)   │
│  - Real-time Gauges    │   │  - devices, api_keys,          │
│  - Hopper Scale Chart  │   │    telemetry_records,          │
│  - SDN Control Console │   │    sdn_rule_events, audit_logs │
└────────────────────────┘   └────────────────────────────────┘
```

### Live Runtime Telemetry Statistics
- **Active Registered Devices:** 19
- **Total Ingested Telemetry Frames:** 845
- **Injected SDN Downlink Events:** 0
- **Configured Security API Keys:** 1

---

## 2. Little-Endian Binary Telemetry Wire Protocol

Uplink packets are transmitted by the Heltec V3 microcontrollers in Little-Endian byte order. Payloads are encapsulated in Base64 strings by ChirpStack and decoded in `TelemetryDecoder.java`.

### 2.1 Fixed Telemetry Header (24 Bytes)
All telemetry modes begin with this common 24-byte header:

| Byte Offset | Field Name | Data Type | Encoding | Unit / Description |
|---|---|---|---|---|
| `00 - 03` | `seq` | `uint32` | Little-Endian | Monotonic transmission sequence number |
| `04 - 07` | `uptime_ms` | `uint32` | Little-Endian | Controller uptime in milliseconds |
| `08 - 11` | `battery_mv` | `float32` | IEEE 754 Little-Endian | Battery voltage in millivolts |
| `12 - 15` | `temperature` | `float32` | IEEE 754 Little-Endian | Barn temperature in °C (-40.0 to +85.0) |
| `16 - 19` | `humidity` | `float32` | IEEE 754 Little-Endian | Relative humidity in % RH (0.0 to 100.0) |
| `20 - 23` | `pressure` | `float32` | IEEE 754 Little-Endian | Atmospheric pressure in hPa (300.0 to 1100.0) |

### 2.2 Poultry Scale Data Block (24 Bytes)
Present in Mode 0 (Full) and Mode 1 (Critical):

| Byte Offset | Field Name | Data Type | Encoding | Unit / Description |
|---|---|---|---|---|
| `24 - 27` | `weight_kg` | `float32` | IEEE 754 Little-Endian | Net feed hopper weight in kilograms |
| `28 - 31` | `gross_weight_kg` | `float32` | IEEE 754 Little-Endian | Gross weight on load cell (kg) |
| `32 - 35` | `voltage_v` | `float32` | IEEE 754 Little-Endian | Load cell excitation / signal voltage (0 - 3.3V) |
| `36 - 37` | `adc_raw` | `uint16` | Little-Endian | Raw ESP32 ADC quantization (0 - 4095) |
| `38 - 41` | `cal_slope` | `float32` | IEEE 754 Little-Endian | Active scale calibration slope factor |
| `42 - 45` | `cal_zero` | `float32` | IEEE 754 Little-Endian | Active zero-tare reference offset |
| `46 - 47` | `time_on_air_ms` | `uint16` | Little-Endian | LoRa RF transmission time-on-air in ms |

### 2.3 Payload Operating Modes
- **Mode 0 (Full, 204 Bytes):** Header (24B) + Scale (24B) + 152 bytes of extended environmental history and sub-sensors.
- **Mode 1 (Critical, 48 Bytes):** Header (24B) + Scale (24B). High-frequency operational telemetry.
- **Mode 2 (Diagnostic, 24 Bytes):** Header (24B) only. Low-power beacon mode.

---

## 3. SDN Control Plane Protocol (RFC 7426)

The Software-Defined Networking plane allows operators to dynamically reconfigure wireless parameters and operational modes over the air. Commands are packed into a single 32-bit Little-Endian unsigned integer in `SdnHeaderPacker.java`.

### 3.1 32-Bit Control Header Bitfield Mapping
```
 31                          17 16 15 14 13   11 10    7 6    4 3    0
+------------------------------+--+-----+-------+-------+------+------+
|         param_value          |S | PM  |  SR   |  TXP  |  SF  | CMD  |
|          (15 bits)           |1b| 2b  | 3b    |  4b   |  3b  |  4b  |
+------------------------------+--+-----+-------+-------+------+------+
```

- **`cmd_id` [Bits 0-3, 4 bits]:**
  `0` = NOP / Heartbeat Ping
  `1` = RECONFIG_RADIO (apply SF, TX power, sampling rate)
  `2` = SET_TEMP_THRESHOLD (param_value contains target temperature)
  `3` = FORCE_SLEEP (enter deep sleep immediately)
  `4` = SET_P2P_RELAY (enable mesh peer-to-peer relaying)
  `15` = SYSTEM_RESET (reboot microcontroller)
- **`sf_target` [Bits 4-6, 3 bits]:** `0`=SF7, `1`=SF8, `2`=SF9, `3`=SF10, `4`=SF11, `5`=SF12.
- **`tx_power` [Bits 7-10, 4 bits]:** Transmit output power in dBm (0 to 15 dBm).
- **`sample_rate` [Bits 11-13, 3 bits]:** `0`=10s, `1`=60s, `2`=300s, `3`=900s, `4`=1800s, `5`=3600s, `6`=14400s, `7`=86400s.
- **`payload_mode` [Bits 14-15, 2 bits]:** `0`=Mode 0 (204B Full), `1`=Mode 1 (48B Critical), `2`=Mode 2 (24B Diagnostic).
- **`sleep_enable` [Bit 16, 1 bit]:** `1` = Deep sleep between sampling intervals, `0` = Continuous RF RX window.
- **`param_value` [Bits 17-31, 15 bits]:** Auxiliary parameter (temperature threshold, sleep duration multiplier, or calibration target).

### 3.2 Downlink Transmission Format
Packed 32-bit integers are converted to 4 bytes in Little-Endian format, Base64-encoded, and published to Mosquitto MQTT topic:
`application/{applicationId}/device/{devEui}/command/down`
```json
{
  "confirmed": false,
  "fPort": 10,
  "data": "AQD/AA=="
}
```

---

## 4. Security Architecture & API Key Management

The backend enforces strict defensive layering across all incoming HTTP traffic using Servlet filters ordered deterministically:

### 4.1 Filter Execution Pipeline
1. **`SecurityHeadersFilter` (`HIGHEST_PRECEDENCE`):** Injects `X-Content-Type-Options: nosniff`, `X-Frame-Options: SAMEORIGIN`, `X-XSS-Protection: 1; mode=block`, `Referrer-Policy: strict-origin-when-cross-origin`, and CSP on every response.
2. **`RateLimitingFilter` (`HIGHEST_PRECEDENCE + 10`):** Resolves client IP (handling `X-Forwarded-For`), enforces in-memory sliding-window limit (120 req/min per IP). Exceeded requests receive HTTP 429 Too Many Requests with header `Retry-After: 60`.
3. **`ApiKeyAuthenticationFilter` (`HIGHEST_PRECEDENCE + 20`):** Authenticates mutating and administrative endpoints.

### 4.2 Key Ingestion & Cryptographic Verification
- **Accepted Headers:**
  - `X-API-Key: sdn_live_...`
  - `Authorization: Bearer sdn_live_...`
- **Key Format:** `sdn_live_` prefix followed by 64 hex characters (32 bytes entropy from `SecureRandom`).
- **Storage Security:** Raw keys are **NEVER** stored in plain text. Only the lowercase SHA-256 hex hash is stored in `api_keys.key_hash` with a unique index.
- **Prefix Masking:** Administrative views only expose the 16-character prefix (`sdn_live_12345678...`).
- **Audit Logging:** Auth failures log `AUTH_FAILURE` records in `audit_logs` with client IP, timestamp, and target URI.

---

## 5. Claude Opus IPC Socket Protocol

The dashboard interfaces natively with Claude Opus via Unix Domain Socket:
- **Socket Path:** `/run/user/1000/cc-socks/2894.sock`
- **Session Key Path:** `~/.claude/sessions/2894.*.key`
- **Transport:** Java 21 Native `SocketChannel.open(StandardProtocolFamily.UNIX)` with `UnixDomainSocketAddress`.
- **Handshake Protocol:**
  1. Line 1: `{"type":"auth","token":"<peerToken>"}\n` (within 3000 ms)
  2. Line 2: `{"type":"user","message":{"role":"user","content":"<prompt>"},"priority":"next"}\n`
- **Resilience:** The client uses non-blocking channels and `@PostConstruct` availability probes. If the daemon is absent or offline, it gracefully falls back without interrupting Spring Boot startup or web dashboard operations.

---

## 6. PostgreSQL Database Persistence & ChirpStack Coexistence

To avoid schema collisions with ChirpStack's 27 internal tables in `chirpstack.public` (specifically `device` and `api_key`), all dashboard tables use explicit plural names:

| Table Name | Entity Class | Primary Key | Description |
|---|---|---|---|
| `devices` | `Device` | `dev_eui VARCHAR(32)` | Registered barn nodes, status, and load cell calibration |
| `api_keys` | `ApiKey` | `id BIGSERIAL` | Hashed API access keys, prefixes, and validity |
| `telemetry_records` | `TelemetryRecord` | `id BIGSERIAL` | Time-series decoded ambient & scale sensor readings |
| `sdn_rule_events` | `SdnRuleEvent` | `id BIGSERIAL` | Audit log of dispatched 32-bit SDN downlinks |
| `audit_logs` | `AuditLog` | `id BIGSERIAL` | System security and operator access audit trail |

---

## 7. REST API Catalog

| Method | Path | Access | Description |
|---|---|---|---|
| `GET` | `/api/status` | Public | System health, MQTT connection, and active node count |
| `GET` | `/api/galpones` | Public | Real-time state of all poultry barns |
| `GET` | `/api/galpones/{devEui}` | Public | Detailed telemetry for specific barn |
| `GET` | `/api/galpones/{devEui}/history` | Public | Historical telemetry records |
| `GET` | `/api/sdn/downlinks` | Public | Downlink command audit log |
| `POST` | `/api/sdn/preview` | Public | Offline calculation of 32-bit SDN bitfield |
| `POST` | `/api/sdn/downlink` | **Protected** | Injects 32-bit SDN command to ChirpStack downlink |
| `POST` | `/api/keys/generate` | **Protected** | Generates a new secure API key (shown once) |
| `GET` | `/api/keys` | Public | Lists active API key metadata (masked prefixes) |
| `DELETE` | `/api/keys/{id}` | **Protected** | Revokes or deletes an API key |
| `POST` | `/api/docs/generate` | **Protected** | Triggers technical documentation generation |
| `GET` | `/api/docs/status` | Public | Verification status of generated documentation |
| `GET` | `/ws` / `/ws-plain` | Public | WebSocket STOMP real-time telemetry endpoints |
