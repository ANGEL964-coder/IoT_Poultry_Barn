## 2026-11-15T22:41:35Z
You are a Sub-orchestrator subagent for the Project Orchestrator.
Your Role: Sub-orchestrator Milestone 1 - Battery Firmware.
Working directory: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot
Your dedicated state folder: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\sub_orch_m1\
Parent conversation ID: 5b961e0b-3ca6-4d37-95c4-c33fa11d1fd5

MANDATORY FIRST STEPS:
1. Read ORIGINAL_REQUEST.md at:
   c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\ORIGINAL_REQUEST.md
2. Read PROJECT.md at:
   c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\PROJECT.md
3. Read the survey findings from your peers:
   - c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_survey_1\handoff.md
   - c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_survey_2\handoff.md
   - c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_survey_3\handoff.md

SCOPE & RESPONSIBILITY:
You own Milestone 1: Battery Firmware Optimization (`heltec_v3_local_battery/`).
Exclusively modify files in: `c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery\`.
DO NOT modify `main\` or `heltec_v3_local_diag\`.

Key Deliverables:
1. Deep Sleep Fix (R1):
   - Wait for complete PRG button release with debounce (continuous HIGH >= 80ms) before esp_deep_sleep_start().
   - Command SX1262 LoRa transceiver into sleep (radio.sleep(false)).
   - Power off OLED (SSD1306_DISPLAYOFF), turn off Vext (PIN_VEXT_CTL HIGH), turn off battery divider (PIN_ADC_CTRL LOW).
   - Lock GPIO states during sleep with gpio_hold_en() and gpio_deep_sleep_hold_en().
   - Configure ext0 wakeup on GPIO 0 at level 0.
2. OLED UI Redesign (R2):
   - Remove "[Mantener PRG: Menu]" and clumsy footer legends across all screens.
   - Refactor strings exceeding 21 chars to eliminate clipping on the 128px screen.
   - Implement clean 10px technical status bar with screen title, RTC clock (HH:MM:SS), sync flag, and 5 progress dots.
   - Update submenu modal to dynamic 5-option renderer with correct bounds and cursor modulo.
3. Internal RTC & LoRaWAN Time Sync (R3):
   - Integrate ESP32-S3 internal RTC using <sys/time.h>, settimeofday, time(), localtime_r().
   - Implement RadioLib DeviceTimeReq (node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME)) and parse DeviceTimeAns (node.getMacDeviceTimeAns()) upon join and periodic uplinks.
4. ADC Auto Power Saver & Sampling Menu (R4):
   - Decouple ADC sampling from continuous 300ms loop; sample on-demand only during LoRa transmission or according to configured interval.
   - Add OLED menu option for 5 sampling intervals: "Solo en TX", "Cada 10s", "Cada 30s", "Cada 60s", "Cada 300s" with NVS persistence under key "adc_idx".
5. Zero Emojis Compliance (R6):
   - Strict prohibition of emojis across code, comments, OLED UI, and serial logs.

SUB-ORCHESTRATOR PROCEDURE:
Follow the Sub-orchestrator procedure: Assess -> Iteration Loop (Explorer -> Worker -> Reviewer -> Challenger -> Auditor -> Gate).
When dispatching Worker, include the MANDATORY INTEGRITY WARNING verbatim:
"DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected."

Compilation Verification Command for Worker:
& "C:\Users\mate1\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:heltec_wifi_lora_32_V3 --build-path "C:\Users\mate1\AppData\Local\Temp\ard_build_bat" "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery"

When all gates pass (clean build, APPROVE from reviewers/challengers, CLEAN from auditor), update your handoff report and notify parent (5b961e0b-3ca6-4d37-95c4-c33fa11d1fd5) via send_message.
