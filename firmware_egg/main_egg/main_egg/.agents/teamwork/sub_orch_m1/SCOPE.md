# Scope: Milestone 1 - Battery Firmware Optimization

## Architecture
Module: `heltec_v3_local_battery/`
Target board: Heltec WiFi LoRa 32 V3 (ESP32-S3FN8 + SX1262 + SSD1306 OLED)

## Feature Inventory (Milestone 1 Scope)
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| F1 | Debounced Deep Sleep Entry | Wait for PRG button (GPIO 0) stable release (continuous HIGH >= 80ms) before esp_deep_sleep_start() | M1 | R1, Survey E1/E3 |
| F2 | SX1262 Radio Deep Sleep | Put SX1262 into ultra-low-power sleep (0.6 uA) via radio.sleep(false) before deep sleep | M1 | R1, Survey E1/E3 |
| F3 | Peripheral Rail & Pin Hold | Disable Vext (GPIO 36 HIGH), disable ADC divider (GPIO 37 LOW), lock states via gpio_hold_en() and gpio_deep_sleep_hold_en() | M1 | R1, Survey E1/E3 |
| F4 | Ext0 Wakeup Re-arming | Configure ext0 wakeup source on GPIO 0 level 0 after verified physical release | M1 | R1, Survey E1/E3 |
| F5 | OLED Clumsy Footer Removal | Eliminate "[Mantener PRG: Menu]" and static footer texts, freeing 10px vertical space | M1 | R2, Survey E2 |
| F6 | OLED Text Overflow Fixes | Refactor strings to <= 21 chars (128px font 1) to eliminate all clipping bugs | M1 | R2, Survey E2 |
| F7 | Technical Header Bar with RTC | Display screen title, HH:MM:SS RTC clock, sync dot, and page progress indicators | M1 | R2, R3, Survey E2 |
| F8 | Dynamic Submenu Modal | Refactor submenu renderer to handle 5 items dynamically with bounds and cursor modulo | M1 | R2, R4, Survey E2 |
| F9 | ESP32-S3 Internal RTC Engine | Integrate POSIX time functions (sys/time.h, settimeofday, time, localtime_r) | M1 | R3, Survey E1/E2 |
| F10 | LoRaWAN DeviceTimeReq Sync | Request network time via RADIOLIB_LORAWAN_MAC_DEVICE_TIME and parse DeviceTimeAns | M1 | R3, Survey E1 |
| F11 | On-Demand ADC Power Saver | Sample battery and weight ADC channels only on-demand during TX or configured interval | M1 | R4, Survey E1/E3 |
| F12 | Configurable ADC Sampling Menu | OLED submenu option for ADC intervals (Solo en TX, 10s, 30s, 60s, 300s) with NVS storage | M1 | R4, Survey E2 |
| F14 | Zero Emojis Compliance | Ensure 100% absence of emojis in all code, comments, OLED UI, and documentation | M1 | R6, Survey E1/E2/E3 |
| F16 | Dual Firmware Compilation | Guarantee sketch compiles cleanly with arduino-cli for esp32:esp32:heltec_wifi_lora_32_V3 | M1 | R6, Survey E1/E2/E3 |

## Interface Contracts
- C1: Safe Deep Sleep Function Contract (`entrarEnDeepSleepSeguro(uint32_t sleep_segundos = 0)`)
- C2: Internal RTC Timekeeping & LoRaWAN Sync Contract (`sincronizarHoraDesdeLoRaWAN`, `obtenerHoraFormateada`, `solicitarSincronizacionHoraLoRaWAN`)
- C3: Configurable ADC Interval Contract (`ModoMuestreoAdc`, `INTERVALOS_ADC_MS`, `INTERVALOS_ADC_TXT`, NVS key "adc_idx" namespace "node_cfg")
- C4: OLED Header & UI Geometry Contract (10px header y=0..9, body y=10..63, <=21 chars/line)

## Write Boundaries
Worker owns exclusively:
- `c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery\`
DO NOT modify `main\` or `heltec_v3_local_diag\`.
