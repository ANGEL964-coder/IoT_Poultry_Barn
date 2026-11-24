## 2026-11-15T22:51:39Z
You are a Worker Agent (teamwork_worker) implementing Milestone 1: Battery Firmware Optimization.
Working directory: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot
Your dedicated state folder: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_worker_m1\

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

MANDATORY FIRST STEPS:
1. Read ORIGINAL_REQUEST.md at:
   c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\ORIGINAL_REQUEST.md
2. Read PROJECT.md at:
   c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\PROJECT.md
3. Read SCOPE.md at:
   c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\sub_orch_m1\SCOPE.md
4. Read the 3 Explorer handoff reports:
   - Deep Sleep & Power Rails: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_m1_1\handoff.md
   - OLED UI & Submenu Modal: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_m1_2\handoff.md
   - RTC & ADC Sampling: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_m1_3\handoff.md

WRITE OWNERSHIP & SCOPE BOUNDARIES:
You exclusively own:
`c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery\`
Modify:
- `c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery\heltec_v3_local_battery.ino`
- `c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery\heltec_v3_config.h` (if needed for constants)
DO NOT touch `main\` or `heltec_v3_local_diag\`.

DELIVERABLES & IMPLEMENTATION REQUIREMENTS:
1. Deep Sleep Fix (R1, Features F1-F4, Contract C1):
   - Implement `entrarEnDeepSleepSeguro(uint32_t sleep_segundos = 0)` in `heltec_v3_local_battery.ino`.
   - SX1262 LoRa sleep via `radio.sleep(false)`.
   - OLED shutdown `display.ssd1306_command(SSD1306_DISPLAYOFF)`.
   - Cut Vext (`digitalWrite(PIN_VEXT_CTL, HIGH)`), disconnect battery divider (`digitalWrite(PIN_ADC_CTRL, LOW)`), turn off LED (`digitalWrite(PIN_LED_USER, LOW)`).
   - Isolate I2C bus (`Wire.end()`, float SDA/SCL pins) to prevent parasitic ESD leakage into unpowered OLED.
   - Wait for physical PRG button release with debounce: continuous HIGH for >= 80ms before enabling ext0 wakeup.
   - Enable GPIO pad retention during deep sleep with `gpio_hold_en()` and `gpio_deep_sleep_hold_en()`.
   - In `setup()`, release pad holds with `gpio_deep_sleep_hold_dis()` and `gpio_hold_dis()` so peripherals can power up on wake.
   - Unify all deep sleep calls (`CMD_FORCE_SLEEP` and OLED submenu option 2) to call `entrarEnDeepSleepSeguro()`.
2. OLED UI Redesign (R2, Features F5-F8, Contract C4):
   - Completely remove `[Mantener PRG: Menu]` and clumsy footer legends across all 5 screens (`renderPantallaHome`, `renderPantallaBalanza`, `renderPantallaBateria`, `renderPantallaComms`, `renderPantallaSdn`), freeing $y = 54..63$.
   - Redesign `dibujarCabeceraCompacta`: 10px height ($y=0..9$, separator line at $y=9$), 4-char screen title (`HOME`, `BALA`, `BATT`, `COMM`, `SDN`), internal RTC clock `HH:MM:SS` (or `--:--:--` if unsynced), LoRa sync indicator (`*` or `-`), and 5 progress indicator dots.
   - Fix all string clipping bugs: Ensure every string on every line is strictly <= 21 characters for size 1 (accounting for x offset so $x + 6 \times len \le 128$). Fix splash screen, home screen, balanza, bateria, comms, and SDN screens per Explorer 2 blueprint.
   - Dynamic Submenu Modal: Overhaul `renderSubmenu()` to support up to 5 items with vertical spacing ($y_{\text{step}}=9\text{ px}$ for 5 items) inside the 124x60 box.
   - Fix submenu cursor wrap in `gestionarBotonPRG`: dynamic modulo `(submenuCursor + 1) % totalItems`.
3. Internal RTC & LoRaWAN Time Sync (R3, Features F9-F10, Contract C2):
   - Include `<time.h>` and `<sys/time.h>`.
   - Implement `sincronizarHoraDesdeLoRaWAN(uint32_t epoch_unix, uint16_t frac_ms)` setting POSIX RTC via `settimeofday()`.
   - Implement `obtenerHoraFormateada(char* buffer, size_t max_len)` returning `HH:MM:SS` or `--:--:--`.
   - Implement `solicitarSincronizacionHoraLoRaWAN()` queuing `node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME)` upon OTAA join and periodic uplink if `!rtc_sincronizado`.
   - In `enviarTelemetriaLoRaWAN()`, parse downlink `DeviceTimeAns` with `node.getMacDeviceTimeAns(&netEpoch, &fracMs, true)` and call `sincronizarHoraDesdeLoRaWAN()`.
4. ADC Auto Power Saver & Configurable Interval (R4, Features F11-F12, Contract C3):
   - Implement `enum ModoMuestreoAdc : uint8_t { ADC_SOLO_TX = 0, ADC_CADA_10S = 1, ADC_CADA_30S = 2, ADC_CADA_60S = 3, ADC_CADA_300S = 4 };`
   - Arrays `INTERVALOS_ADC_MS[5]` and `INTERVALOS_ADC_TXT[5]`.
   - In `loop()`, decouple ADC sampling from 300ms loop; execute only if `intervaloAdcMs > 0` and elapsed time >= interval.
   - In `enviarTelemetriaLoRaWAN()`, if `modoMuestreoAdcIdx == ADC_SOLO_TX`, perform on-demand sampling of balanza and battery prior to payload assembly.
   - Submenu in Screen 2 (BATERIA) provides "INTERVALO ADC" with the 5 options.
   - NVS persistence under namespace `"node_cfg"`, key `"adc_idx"`, default `ADC_CADA_10S` (1) or `ADC_SOLO_TX` (0).
5. Zero Emojis Compliance (R6, Feature F14):
   - Zero emojis in all code, comments, strings, UI buffers, and logs. Strict 7-bit ASCII.

VERIFICATION COMMAND (MUST RUN AND PASS):
& "C:\Users\mate1\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:heltec_wifi_lora_32_V3 --build-path "C:\Users\mate1\AppData\Local\Temp\ard_build_bat" "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery"

Produce your handoff report at:
`c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_worker_m1\handoff.md`
Report compilation results, files modified, changes made, and verification output. When done, notify me via message.
