## 2026-11-15T22:42:51Z
You are a Read-Only Exploration Agent (teamwork_explorer).
Your Role: Deep Sleep Explorer for Milestone 1 (Battery Firmware).
Working directory: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot
Your dedicated state folder: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_m1_1\

MANDATORY FIRST STEP:
Read ORIGINAL_REQUEST.md at:
c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\ORIGINAL_REQUEST.md
Also read PROJECT.md at:
c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\PROJECT.md
and SCOPE.md at:
c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\sub_orch_m1\SCOPE.md

TARGET CODE:
Inspect c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery\heltec_v3_local_battery.ino
and c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery\heltec_v3_config.h

OBJECTIVE:
Analyze and formulate the exact code modifications for Features F1, F2, F3, F4 (Safe Deep Sleep & Power Rails):
1. Debounced release of PRG button (GPIO 0): Ensure continuous HIGH for >= 80ms before esp_deep_sleep_start().
2. SX1262 LoRa sleep: radio.sleep(false) with configuration retention.
3. OLED shutdown: display.ssd1306_command(SSD1306_DISPLAYOFF).
4. Power rails & pins: Set PIN_VEXT_CTL (GPIO 36) HIGH, PIN_ADC_CTRL (GPIO 37) LOW, PIN_LED_USER LOW.
5. GPIO holds: gpio_hold_en() on VEXT_CTL and ADC_CTRL, and gpio_deep_sleep_hold_en().
6. Wakeup configuration: esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0) and esp_sleep_enable_timer_wakeup() if duration specified.
7. Unify all sleep calls (OLED submenu, CMD_FORCE_SLEEP, etc.) into entering C1 contract function: entrarEnDeepSleepSeguro(uint32_t sleep_segundos = 0).

Produce a comprehensive handoff report at:
c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_m1_1\handoff.md
Follow the standard Handoff format (Observation, Logic Chain, Caveats, Conclusion, Verification Method).
When complete, notify me with your report path.

## 2026-11-15T22:49:41Z
**Context**: Milestone 1 Deep Sleep Explorer (Features F1-F4)
**Content**: Please report your progress on the deep sleep investigation and handoff report.
**Action**: Finish writing handoff.md in your state folder and notify me when complete.
