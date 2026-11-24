# BRIEFING — 2026-11-15T22:33:00Z

## Mission
Analyze UI diagnostics and OLED menu implementation in heltec_v3_local_diag and heltec_v3_local_battery, identify design shortcomings, scan for emojis, and propose clean UI, RTC display, and ADC interval configuration.

## 🔒 My Identity
- Archetype: explorer
- Roles: UI Diagnostics Explorer
- Working directory: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot
- State folder: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_survey_2\
- Original parent: 5b961e0b-3ca6-4d37-95c4-c33fa11d1fd5
- Milestone: Milestone 0 - Survey / UI Diagnostics Exploration

## 🔒 Key Constraints
- Read-only investigation — do NOT implement or modify source files
- Prohibit emojis strictly in all reports, text, and documentation
- Maintain main\ directory strictly untouched

## Current Parent
- Conversation ID: 5b961e0b-3ca6-4d37-95c4-c33fa11d1fd5
- Updated: 2026-11-15T22:33:00Z

## Investigation State
- **Explored paths**:
  - heltec_v3_local_diag\heltec_v3_local_diag.ino
  - heltec_v3_local_battery\heltec_v3_local_battery.ino
  - heltec_v3_local_battery\heltec_v3_config.h
  - heltec_v3_local_diag\sdn_header.h
  - main\main.ino (v1.0 baseline inspection for emojis)
  - RadioLib TS003 / LoRaWAN headers for DeviceTimeReq
- **Key findings**:
  - OLED SSD1306 128x64 driven via Wire (SDA=17, SCL=18, RST=21, Addr=0x3C). Vext power switch on GPIO 36 (active LOW).
  - Menu navigation uses GPIO 0 (PRG button): short press (<500ms) cycles screens/options, long press (>=700ms) opens submenu/executes.
  - Clumsy footer "[Mantener PRG: Menu]" consumes 10px on every screen.
  - Multiple text overflow bugs on 128px screen (e.g. "Bruto:18.45kg V:0.922V" = 132px, "OLED Sleep: 10 Minutos" = 132px, "ToA: 82.4ms SF7 @ 14dBm" = 138px).
  - Hardcoded modulo 4 in submenu cycling prevents lists of 5 items.
  - Neither sketch has RTC clock integration or network sync indicator.
  - ADC sampled unconditionally every 300ms, energizing GPIO 37 MOSFET constantly.
  - Zero emojis found in v2 sketches, but v1 had 4 emojis, and degree symbol in diag:265 causes garbled OLED display.
- **Unexplored areas**: None. Complete investigation finished.

## Key Decisions Made
- Layout refinement: Eliminate footer text completely, move RTC HH:MM:SS clock and sync indicator to a 10px compact top header.
- ADC Interval: Add 5-option selector (Solo en TX, 10s, 30s, 60s, 300s) with dynamic modal cursor modulo and NVS persistence under "adc_idx".

## Artifact Index
- DISPATCH.md — incoming dispatch instructions
- BRIEFING.md — persistent agent working memory
- progress.md — liveness heartbeat and milestone tracking
- handoff.md — comprehensive technical report
