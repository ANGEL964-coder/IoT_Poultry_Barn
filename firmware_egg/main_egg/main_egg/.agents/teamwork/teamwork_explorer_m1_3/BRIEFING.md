# BRIEFING — 2026-11-15T22:49:15Z

## Mission
Analyze and formulate exact code modifications for Features F9, F10, F11, F12 (Internal RTC, LoRaWAN Time Sync, ADC On-Demand Power Saver, Configurable ADC Sampling Menu) for Heltec V3 Battery Firmware.

## 🔒 My Identity
- Archetype: teamwork_explorer
- Roles: RTC ADC Explorer for Milestone 1 (Battery Firmware)
- Working directory: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot
- State folder: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_m1_3\
- Original parent: a01bfaff-e987-4a23-b666-0f3388b3fc3c
- Milestone: Milestone 1 (Battery Firmware)

## 🔒 Key Constraints
- Read-only investigation — do NOT modify firmware source files directly
- Propose exact diffs / replacement snippets in handoff report
- Strict zero emojis policy compliance (F14)
- Layout compliance: write only to own state folder

## Current Parent
- Conversation ID: a01bfaff-e987-4a23-b666-0f3388b3fc3c
- Updated: 2026-11-15T22:43:04Z

## Investigation State
- **Explored paths**:
  - `heltec_v3_local_battery/heltec_v3_local_battery.ino`
  - `heltec_v3_local_battery/heltec_v3_config.h`
  - RadioLib `LoRaWAN.h` and `LoRaWAN.cpp`
  - Teamwork survey handoffs 1, 2, 3
- **Key findings**:
  - F9: ESP32-S3 internal RTC POSIX functions (`<time.h>`, `<sys/time.h>`, `settimeofday`, `time`, `localtime_r`) provide microsecond-accurate timekeeping and formatting (`"HH:MM:SS"` or `"--:--:--"`).
  - F10: RadioLib 7.x supports `sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME)` and `getMacDeviceTimeAns(&epoch, &fracMs, true)`. Sync is queued upon OTAA join and periodic uplink if un-synced.
  - F11: Decoupling ADC sampling (`actualizarBalanza` and `actualizarBateria`) from 300ms loop to on-demand during TX or configurable intervals eliminates continuous 3.33 Hz CPU wakeups and MOSFET divider switching.
  - F12: `ModoMuestreoAdc` enum (0=Solo en TX, 1=10s, 2=30s, 3=60s, 4=300s) persisted under NVS `"node_cfg"` key `"adc_idx"`, integrated into Screen 2 (BATERIA) submenu with dynamic 5-item modulo navigation.
  - F14: 100% 7-bit ASCII clean, strictly zero emojis across all code and documentation.
- **Unexplored areas**: None.

## Key Decisions Made
- Menu integration: Screen 2 (BATERIA) submenu upgraded from static timeout to "INTERVALO ADC" with 5 options.
- Dynamic submenu modulo: Set totalItems to 5 for BATERIA submenu so option 4 (Cada 300s) is fully accessible.
- Time sync fallback: Return `"--:--:--"` when `rtc_sincronizado` is false.
- Zero emojis: Verified 100% clean across all code and reports.

## Artifact Index
- `DISPATCH.md` — Incoming parent instructions
- `progress.md` — Liveness heartbeat and milestone tracker
- `handoff.md` — Final 5-component handoff report
