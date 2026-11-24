# E2E Test Specification Handoff Report: R1 (Deep Sleep) & R2 (OLED Menu Redesign)

- **Target Architecture**: Heltec WiFi LoRa 32 V3 (ESP32-S3FN8 + SX1262 + SSD1306 128x64 OLED)
- **Scope**: Requirement 1 (Debounced Deep Sleep Entry) and Requirement 2 (OLED Menu Redesign)
- **Target Files**:
  - `heltec_v3_local_battery/heltec_v3_local_battery.ino`
  - `heltec_v3_local_battery/heltec_v3_config.h`
  - `heltec_v3_local_diag/heltec_v3_local_diag.ino`
- **Protected File (Strictly Immutable)**: `main/` (v1.0 legacy)
- **Author**: `teamwork_spec_miner` (spec_miner_e2e_1)
- **State Directory**: `c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\spec_miner_e2e_1\`

---

## 1. Observation

### 1.1 Deep Sleep Implementation and Button Timing Defect
Direct inspection of `heltec_v3_local_battery/heltec_v3_local_battery.ino` and `heltec_v3_local_diag/heltec_v3_local_diag.ino`:

1. Button Definition and Thresholds:
   - `heltec_v3_config.h:28`: `#define PIN_PRG 0 // Boton PRG integrado (LOW al presionar)`
   - `heltec_v3_local_battery.ino:77`: `const uint32_t TIEMPO_PULSACION_LARGA = 700; // ms`
   - `heltec_v3_local_diag.ino:76`: `const uint32_t TIEMPO_PULSACION_LARGA = 700; // ms`
   - Active low with internal pull-up: `pinMode(PIN_PRG, INPUT_PULLUP);` (battery line 729, diag line 707).

2. Menu Sleep Execution Path:
   - `heltec_v3_local_battery.ino:617-622`:
     ```cpp
     } else if (submenuCursor == 2) {
         display.ssd1306_command(SSD1306_DISPLAYOFF);
         digitalWrite(PIN_VEXT_CTL, HIGH);
         esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0);
         esp_deep_sleep_start();
     }
     ```
   - `heltec_v3_local_diag.ino:568-573`:
     ```cpp
     } else if (submenuCursor == 2) {
         // Suspender Nodo (Deep Sleep)
         display.ssd1306_command(SSD1306_DISPLAYOFF);
         digitalWrite(PIN_VEXT, HIGH); // Apagar perifericos
         esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0); // Despertar con boton PRG
         esp_deep_sleep_start();
     }
     ```
   - Verbatim Timing Flaw: In both sketches, `gestionarBotonPRG()` evaluates `millis() - tiempoInicioPresion >= TIEMPO_PULSACION_LARGA` (700 ms). At exactly 700 ms, `ejecutarAccionSubmenu()` is invoked synchronously while the user is actively holding down the tactile button (GPIO 0 is at 0V / LOW).
   - In both sketches, `esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0)` arms the wakeup condition on level 0 (LOW).
   - `esp_deep_sleep_start()` is called immediately on the next statement without polling for button release or debounce stabilization. The RTC power domain detects level 0 on GPIO 0 within microseconds and aborts sleep, causing an immediate reboot.

3. Peripheral Rails and Radio Sleep Omission:
   - Semtech SX1262 LoRa transceiver is wired to the always-on 3.3V rail (`VDD_3V3`) and not switched by `Vext`.
   - In baseline sketches, `radio.sleep()` is NEVER called prior to `esp_deep_sleep_start()`.
   - Inspection of RadioLib header `C:\Users\mate1\Documents\Arduino\libraries\RadioLib\src\modules\SX126x\SX126x.h:219`:
     ```cpp
     int16_t sleep(bool retainConfig);
     ```
     Parameter `retainConfig = true` maintains modem configuration (warm start, ~0.6 uA); `retainConfig = false` initiates cold sleep (~0.16 uA). In baseline, the SX1262 remains in Standby RC mode consuming ~1.2 mA to 1.6 mA continuously.
   - Vext Rail (`PIN_VEXT_CTL` / `PIN_VEXT` on GPIO 36) controls the P-MOSFET gate (LOW = ON, HIGH = OFF). While `digitalWrite` sets GPIO 36 HIGH, neither sketch calls `gpio_hold_en((gpio_num_t)PIN_VEXT_CTL)` or `gpio_deep_sleep_hold_en()`. On ESP32-S3, digital pads lose latch state during deep sleep unless pad hold is enabled.
   - Battery divider MOSFET gate (`PIN_ADC_CTRL` on GPIO 37) is not locked via `gpio_hold_en`.

### 1.2 OLED UI Layout, Footers, and String Width Flaws
Direct inspection of display routines in `heltec_v3_local_battery.ino` and `heltec_v3_local_diag.ino`:

1. Static Footer Legends Consuming Screen Real Estate:
   - Screen 0 (HOME): lines 454-456 in battery, lines 386-388 in diag:
     ```cpp
     display.drawLine(0, 55, 127, 55, SSD1306_WHITE);
     display.setCursor(0, 57);
     display.print("[Mantener PRG: Menu]");
     ```
   - Screen 1 (BALANZA): lines 476-478 in battery, lines 412-414 in diag:
     `display.drawLine(0, 54, 127, 54, SSD1306_WHITE); display.print("[Mantener: Calibrar]");`
   - Screen 2 (BATERIA): lines 502-504 in battery, lines 441-443 in diag:
     `display.drawLine(0, 54, 127, 54, SSD1306_WHITE); display.printf("OLED Sleep: %s", TIMEOUTS_PANTALLA_TXT[timeoutPantallaIdx]);`
   - Screen 3 (COMMS): lines 524-526 in battery, lines 465-467 in diag:
     `display.drawLine(0, 54, 127, 54, SSD1306_WHITE); display.print("[Mantener: Config TX]");`
   - Screen 4 (SDN): lines 545-547 in battery, lines 487-489 in diag:
     `display.drawLine(0, 54, 127, 54, SSD1306_WHITE); display.print("[Mantener: Accion SDN]");` / `[Mantener: Test SDN]`
   - Impact: Lines y=54 to 63 (10 vertical pixels, 15.6% of screen) are wasted on static hints.

2. Critical String Width Violations (SSD1306 128 px, font size 1 = 6 px per character, max 21 chars = 126 px):
   - HOME line 452: `display.printf("Bruto:%.2fkg V:%.3fV", pesoBrutoKg, voltajeSensorV);`
     With values `18.45` and `0.922`: string is `"Bruto:18.45kg V:0.922V"` -> 22 characters = 132 pixels. Exceeds screen by 4 pixels (the final 'V' is clipped off-screen).
   - BATERIA line 504: `display.printf("OLED Sleep: %s", TIMEOUTS_PANTALLA_TXT[timeoutPantallaIdx]);`
     With index 2: `"OLED Sleep: 10 Minutos"` -> 22 characters = 132 pixels (clipped).
     With index 3: `"OLED Sleep: Siempre ON"` -> 22 characters = 132 pixels (clipped).
   - COMMS line 520: `display.printf("ToA: %.1fms SF%u @ %ddBm", current_toa_ms, current_sf, current_tx_power);`
     With typical values: `"ToA: 82.4ms SF7 @ 14dBm"` -> 23 characters = 138 pixels (clipped by 10 pixels).
   - SDN line 537: `display.printf("CMD : 0x%02X (%s)", sdn_last_cmd, getSdnCommandName(sdn_last_cmd));`
     With `CMD_SET_TEMP_THRESHOLD`: `"CMD : 0x02 (SET_TEMP_THRESHOLD)"` -> 31 characters = 186 pixels (clipped by 58 pixels).
     With `CMD_RECONFIG_RADIO`: `"CMD : 0x01 (RECONFIG_RADIO)"` -> 27 characters = 162 pixels (clipped by 34 pixels).
   - Splash Screen:
     - `heltec_v3_local_diag.ino:727`: `display.setCursor(10, 35); display.println("Modo Diagnostico Loc");` (20 chars = 120 px; 10 + 120 = 130 px > 128 px; 2 px clipped).
     - `heltec_v3_local_battery.ino:749`: `display.setCursor(12, 46); display.println("Iniciando LoRaWAN...");` (20 chars = 120 px; 12 + 120 = 132 px > 128 px; 4 px clipped).

3. Submenu Modal Architecture Constraint:
   - Lines 564 in battery, 509 in diag: `for (int i = 0; i < totalOpciones && i < 4; i++)`
   - Lines 709 in battery, 686 in diag: `submenuCursor = (submenuCursor + 1) % 4;`
   - Both rendering loop and button modulo are hardcoded to 4 items max. R4 requires 5 ADC interval options (`Solo en TX`, `Cada 10s`, `Cada 30s`, `Cada 60s`, `Cada 300s`). Rendering and navigation freeze or truncate item 5 without refactoring.

---

## 2. Logic Chain

1. Root Cause of Immediate Wakeup:
   - Observation 1.1 demonstrates that the user enters sleep via a long press of PRG (GPIO 0).
   - At the 700 ms mark, `gestionarBotonPRG()` executes `ejecutarAccionSubmenu()`.
   - The user's finger is still depressing the physical switch. Therefore, GPIO 0 is grounded (0V).
   - `esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0)` arms the RTC power domain to wake up whenever GPIO 0 is LOW (0).
   - Because GPIO 0 is ALREADY LOW when `esp_deep_sleep_start()` is called, the RTC wakeup detector triggers instantaneously (< 1 us).
   - Therefore, a debouncing loop that waits until GPIO 0 reads HIGH continuously for at least 80 ms is strictly necessary before calling `esp_sleep_enable_ext0_wakeup()` and `esp_deep_sleep_start()`.

2. Radio and Power Pin Retention Chain:
   - Observation 1.1 proves the SX1262 radio is wired to `VDD_3V3`. Setting Vext HIGH cuts OLED power but leaves the SX1262 powered in Standby mode (~1.5 mA).
   - Calling `radio.sleep(false)` cuts radio current to 0.16 uA (cold) or 0.6 uA (warm retention).
   - Because GPIO 36 and GPIO 37 are digital IOs on ESP32-S3, deep sleep floating state causes gate voltage drift on the P-MOSFET and N-MOSFET. Calling `gpio_hold_en()` and `gpio_deep_sleep_hold_en()` locks the output latches.

3. OLED Real Estate & Formatting Optimization:
   - Observation 1.2 demonstrates that static footers consume y=54 to 63 (10 pixels).
   - Eliminating footers expands usable vertical body space from 44 pixels (y=10..53) to 54 pixels (y=10..63).
   - This allows expanding cramped 2-column data into clean single-line rows or abbreviated formats that strictly adhere to the 21-character limit (`width <= 126 px`), eliminating all text truncation bugs.

4. Header Bar and Dynamic Modal Geometry:
   - Header bar consumes y=0 to 8, with separator line at y=9.
   - Screen body occupies y=10 to 63.
   - In the modal dialog (`renderSubmenu`), replacing the 10px item pitch (`y = 17 + i * 10`) with a 9px pitch (`y = 15 + i * 9`) allows 5 items to fit within `51 + 8 = 59 px <= 60 px` dialog height.
   - Updating modulo navigation to `(submenuCursor + 1) % totalOpciones` allows full dynamic navigation across 4-item and 5-item menus.

---

## 3. Caveats

1. Hardware Tactile Switch Degradation:
   - Tactile pushbuttons on development boards can accumulate contact oxidation. While typical contact bounce is 10 ms to 30 ms, degraded buttons can exhibit bounce up to 60 ms. The 80 ms continuous HIGH threshold provides a 2x safety factor over standard bounce, but won't exit if a button is physically jammed closed. A stuck-button watchdog/timeout should be specified.
2. ESP32-S3 Ext0 Wakeup Pin Constraints:
   - On ESP32-S3, `esp_sleep_enable_ext0_wakeup` supports any RTC GPIO (GPIO 0 to 21). GPIO 0 is mapped to RTC_GPIO0. Ext0 supports level-triggered wakeup only (0 = low, 1 = high), not edge-triggered. Thus, debouncing prior to sleep is the only architectural way to prevent immediate re-wake.
3. LoRaWAN Network Latency:
   - DeviceTimeReq response latency depends on gateway connectivity. Test specifications must account for both synchronized and unsynchronized states.
4. Protected Main Tree:
   - All tests and assertions apply to `heltec_v3_local_battery/` and `heltec_v3_local_diag/`. The `main/` directory remains strictly read-only and unedited.

---

## 4. Conclusion

1. **R1 (Debounced Deep Sleep Entry)** is fully decomposed into 4 critical hardware and software sub-features:
   - Stable physical release with continuous HIGH >= 80 ms
   - SX1262 LoRa sleep command with register retention
   - Peripheral rail cutoff (Vext HIGH, ADC_CTRL LOW, USER_LED LOW) with pad hold
   - Post-debounce ext0 wakeup re-arming on level 0 and sleep invocation
2. **R2 (OLED Menu Redesign)** is fully decomposed into 4 critical display and interaction sub-features:
   - Elimination of static footer legends and y=54/55 separator lines
   - Zero-clipping string width compliance (<= 21 characters / 128 px)
   - Technical header bar with RTC clock (`HH:MM:SS`), screen title, sync dot, and page dots
   - Dynamic 5-item modal dialog geometry with dynamic modulo cursor navigation
3. A four-tier test suite is established, spanning 48 feature coverage tests, 40 boundary & corner tests, 12 pairwise combinations, and 5 operational real-world scenarios.

---

## Features Discovered

| # | Category | Feature | Description | Inputs | Outputs | Error Behavior | Discovered Via |
|---|----------|---------|-------------|--------|---------|----------------|----------------|
| 1 | Power Domains | Vext Power Rail (GPIO 36) | Active-LOW P-MOSFET gate supplying 3.3V to OLED and external sensor conditioning | GPIO 36: LOW = ON, HIGH = OFF | 3.3V on Vext pin when active; 0V when cut | Leaving LOW in sleep leaks power to OLED; floating gate leaks through partial conduction | `heltec_v3_config.h`:20, schematic |
| 2 | Power Domains | Battery ADC Divider Gate (GPIO 37) | Transistor-isolated 390k/100k (4.9:1) divider on LiPo VBAT | GPIO 37: HIGH = connect to GND, LOW = disconnect | Scaled voltage on GPIO 1 (ADC1_CH0) | Leaving HIGH drains 8.57 uA continuously through divider resistor string | `heltec_v3_config.h`:40, `heltec_v3_local_battery.ino`:137 |
| 3 | Power Domains | Load Cell Conditioner ADC (GPIO 2) | ADC1_CH1 analog input (0-3.3V) for external strain gauge conditioner | Analog voltage on GPIO 2 | 12-bit ADC raw value and millivolt conversion | Unpowered conditioner or floating pin reads < 8 mV | `heltec_v3_config.h`:41, `heltec_v3_local_battery.ino`:185 |
| 4 | Power Management | SX1262 LoRa Sleep Retention | Software-commanded sleep with retention of modem registers | RadioLib `radio.sleep(false)` or `radio.sleep(true)` | SX1262 current drops from ~1.5 mA to 0.16-0.6 uA | Omitting sleep leaves radio in Standby RC (~1.5 mA) drawing 95% of sleep budget | RadioLib `SX126x.h`:219 |
| 5 | Power Management | OLED SSD1306 Display Off | SSD1306 DC-DC charge pump shutdown command prior to power cut | `display.ssd1306_command(SSD1306_DISPLAYOFF)` | OLED internal charge pump safely discharged | Cutting Vext without DISPLAYOFF command risks ghosting or ESD back-feed | SSD1306 datasheet |
| 6 | Power Management | GPIO Pad Hold Latches | RTC pad hold locking GPIO 36 and GPIO 37 states across deep sleep | `gpio_hold_en((gpio_num_t)PIN_VEXT_CTL); gpio_deep_sleep_hold_en();` | Driven pin levels frozen during power gating | Without hold, digital pads float and P-MOSFET gate drifts LOW, turning Vext on | ESP32-S3 TRM §6.5 |
| 7 | Deep Sleep | Debounced Button Release Loop | Software loop requiring continuous HIGH for >= 80 ms on GPIO 0 | Polling `digitalRead(PIN_PRG)` in a tight loop with reset on LOW | Return from loop only when button is physically and stably released | Loop hangs if button switch is mechanically shorted to ground (requires safety watchdog) | `heltec_v3_local_battery.ino`:683 |
| 8 | Deep Sleep | Ext0 Wakeup Source on GPIO 0 | RTC IO wakeup triggered on level 0 (LOW) from PRG pushbutton | `esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0);` | Wakeup event on button press, cause `ESP_SLEEP_WAKEUP_EXT0` | If armed while button is still LOW, system wakes immediately in < 1 ms | `heltec_v3_local_battery.ino`:620 |
| 9 | Deep Sleep | RTC Timer Wakeup Source | 64-bit microsecond counter wakeup source for scheduled uplinks | `esp_sleep_enable_timer_wakeup(sleep_us);` | Wakeup event on timer expiration, cause `ESP_SLEEP_WAKEUP_TIMER` | Drift of internal RC oscillator if not calibrated against slow clock | `heltec_v3_local_battery.ino`:265 |
| 10 | OLED UI | Footer Legend Removal | Reclamation of y=54..63 by eliminating static instructional strings | Render functions without `[Mantener PRG: Menu]` or dividing lines | 10 pixels added to usable data canvas | Static footers caused cramped upper lines and text clipping | `heltec_v3_local_battery.ino`:454-456 |
| 11 | OLED UI | String Width Bound (<= 21 chars) | Font size 1 string formatting restriction to prevent 128 px clipping | Truncation, abbreviation, or line-splitting for strings > 21 chars | Clean rendering within display borders (0..127 px) | Unchecked strings overflow right border by up to 58 pixels | `heltec_v3_local_diag.ino`:383, 443, 460 |
| 12 | OLED UI | Technical Header with RTC | Unified 10 px bar displaying screen title, RTC clock, sync dot, and page dots | `time()`, `localtime_r()`, `pantallaActual`, `rtc_sincronizado` | Formatted header at y=0..8 with rule at y=9 | Overlapping text if screen title or time strings exceed allotted pixel widths | `PROJECT.md` Contract C4 |
| 13 | OLED UI | Dynamic 5-Item Modal Renderer | Submenu dialog supporting up to 5 items with 9 px pitch and dynamic modulo | `opciones[]`, `totalOpciones`, `submenuCursor` | Modal box at (2,2,124,60) with cursor indicator `>` | Items > 4 clipped if loop is hardcoded to `i < 4`; cursor wraps at 4 instead of 5 | `heltec_v3_local_battery.ino`:564 |
| 14 | Clock & Timing | Internal RTC POSIX Driver | POSIX timekeeping maintained across sleep modes by internal RTC | `settimeofday(&tv, NULL)`, `time(&now)`, `localtime_r()` | Standard Unix time and formatted `HH:MM:SS` string | Drift over 24h if internal 150kHz oscillator is not re-synchronized | `<time.h>`, `<sys/time.h>` |
| 15 | Network Sync | LoRaWAN DeviceTimeReq MAC | Querying ChirpStack v4 network server for UTC timestamp via MAC command 0x0D | `node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME)` | Unix epoch in `DeviceTimeAns` | Gateway without GPS/NTP returns error or omits answer | RadioLib `LoRaWAN.h`:173, 760, 780 |
| 16 | ADC Control | Configurable ADC Interval | NVS-persisted sampling modes: Solo en TX, 10s, 30s, 60s, 300s | `modoMuestreoAdcIdx` (0..4), NVS key `"adc_idx"` | Decoupled ADC sampling reducing average power consumption | Continuous 300ms sampling wastes energy through divider and CPU active time | `PROJECT.md` Contract C3 |

---

## Edge Cases

| # | Feature | Input | Observed Behavior |
|---|---------|-------|-------------------|
| 1 | PRG Debounce | Button held down continuously for 5,000 ms after selecting Sleep | System enters debounce loop; `tInicioEstable` resets continuously; ESP32 remains in active loop until button is released; only sleeps after 80 ms of continuous HIGH. |
| 2 | PRG Debounce | Mechanical bounce generates 1 ms LOW pulse at millisecond 79 of release | Debounce window resets to 0; counter restarts; guarantees true contact stabilization before sleep arming. |
| 3 | PRG Debounce | Button stuck permanently LOW (hardware fault / short circuit) | Firmware remains trapped in release loop unless safety watchdog/timeout (e.g. 10s) triggers forced abort or error. |
| 4 | SX1262 Sleep | Radio commanded to sleep while a LoRa transmission is in progress | Transceiver aborts TX abruptly or enters undefined state; firmware must verify `radio.getChannelScanMark()` / TX idle before calling `radio.sleep()`. |
| 5 | SX1262 Sleep | `radio.sleep()` invoked when SPI bus is uninitialized or in reset | SPI transaction fails; NSS pin remains low; SX1262 does not enter sleep; current draw remains high. |
| 6 | Vext Pin Hold | GPIO 36 driven HIGH while I2C SDA/SCL lines remain pulled HIGH | Parasitic current flows through SSD1306 internal ESD clamping diodes into unpowered VDD line, leaking 250 uA to 400 uA. |
| 7 | GPIO Hold | `gpio_hold_dis()` not called in `setup()` after waking from deep sleep | Subsequent `digitalWrite()` or `pinMode()` calls fail to change GPIO 36/37 states because output latches remain locked. |
| 8 | Ext0 Wakeup | Rapid double-click on PRG during sleep entry transition | First release starts 80 ms timer; second tap interrupts timer; release loop resets; sleep deferred until second release stabilizes. |
| 9 | Ext0 + Timer | Both ext0 (button) and timer wakeup expire at the exact same millisecond | ESP32-S3 wakes up; `esp_sleep_get_wakeup_cause()` returns either `ESP_SLEEP_WAKEUP_EXT0` or `ESP_SLEEP_WAKEUP_TIMER` depending on hardware priority arbitration. |
| 10 | OLED Formatting | 22-character string rendered with `display.print()` at `x = 0` | Rightmost character (pixels 127..131) clipped by hardware framebuffer boundaries; visually appears as cut-off character. |
| 11 | OLED Formatting | Negative temperature or large floating-point value in `printf("%0.2f")` (e.g. `-105.45kg`) | String width expands from 7 chars to 9 chars; pushes right-aligned text past pixel 127 if spacing is not bounded. |
| 12 | OLED Header | System time reaches 23:59:59 and rolls over to 00:00:00 | Header string transitions smoothly from `"23:59:59"` to `"00:00:00"`; character length remains exactly 8 chars (48 px). |
| 13 | OLED Modal | Modal rendered with `totalOpciones = 5` and `submenuCursor = 4` (last item) | Inverted highlight bar rendered at `y = 15 + (4 * 9) - 1 = 50`; bottom edge at `50 + 9 = 59 px <= 60 px`; perfectly contained within dialog border. |
| 14 | OLED Modal | Modal with `totalOpciones = 1` navigated by pressing PRG | Cursor modulo `(0 + 1) % 1 = 0`; cursor stays on item 0 without out-of-bounds error. |
| 15 | LoRaWAN Time Sync | Gateway returns `DeviceTimeAns` with 0 Unix epoch (invalid timestamp) | Firmware checks `epoch > 1700000000` (valid post-2023 timestamp); rejects 0 timestamp and keeps `rtc_sincronizado = false`. |

---

## Tier 1: Feature Coverage Specifications (>= 5 Tests Per Feature)

### Group R1: Debounced Deep Sleep Entry Subsystem

#### Feature 1: Debounced Physical Release of PRG Button (GPIO 0)
- **TC-R1-F1-01: Standard Button Release Sleep Transition**
  - *Objective*: Verify device enters deep sleep when PRG button is released normally after 700 ms hold.
  - *Preconditions*: Device in HOME screen, submenu active at index 2 ("Suspender (Sleep)").
  - *Test Vector*: Button held LOW for 750 ms, then released to HIGH for 85 ms continuously.
  - *Expected Behavior*: Release loop detects HIGH for >= 80 ms, completes safely, enables ext0, and invokes `esp_deep_sleep_start()`. Device halts execution and current drops to < 25 uA.
  - *Assertion*: `digitalRead(PIN_PRG) == HIGH` verified for >= 80 ms before `esp_deep_sleep_start()` is reached.

- **TC-R1-F1-02: Prolonged Button Hold Suppression**
  - *Objective*: Verify device does NOT enter deep sleep while user continues to hold button down.
  - *Preconditions*: Submenu action 2 triggered at t = 700 ms.
  - *Test Vector*: Button maintained at LOW for 3,500 ms following action trigger.
  - *Expected Behavior*: System polls inside release loop; does NOT invoke `esp_deep_sleep_start()` during the 3,500 ms hold; OLED remains dark; CPU remains awake in polling loop.
  - *Assertion*: Elapsed time between action trigger and `esp_deep_sleep_start()` >= 3,580 ms.

- **TC-R1-F1-03: Immediate Release at Threshold Boundary**
  - *Objective*: Verify clean sleep entry when button is released within 5 ms of the 700 ms threshold.
  - *Preconditions*: HOME submenu active.
  - *Test Vector*: Button held LOW for exactly 705 ms, then released to HIGH.
  - *Expected Behavior*: System executes release debounce loop for 80 ms, arms ext0 at t = 785 ms, enters deep sleep.
  - *Assertion*: Wakeup cause on subsequent press is `ESP_SLEEP_WAKEUP_EXT0`.

- **TC-R1-F1-04: Mechanical Contact Bounce Filter**
  - *Objective*: Verify debounce loop restarts timer if contact bounce occurs during release window.
  - *Preconditions*: Submenu sleep action triggered.
  - *Test Vector*: GPIO 0 goes HIGH for 30 ms, drops to LOW for 5 ms (bounce), then goes HIGH and stays HIGH.
  - *Expected Behavior*: First 30 ms window is discarded; timer restarts upon detecting LOW; enters sleep only after 80 ms of continuous HIGH following the bounce.
  - *Assertion*: Total wait time in release loop >= (30 + 5 + 80) = 115 ms.

- **TC-R1-F1-05: Late Re-Depression Rejection**
  - *Objective*: Verify debounce loop resets if user releases and quickly taps again at t = 70 ms.
  - *Preconditions*: Sleep action triggered.
  - *Test Vector*: Button HIGH for 70 ms, LOW for 200 ms, then HIGH continuously for 80 ms.
  - *Expected Behavior*: System does not sleep during the 200 ms re-depression; only sleeps after the final 80 ms stable HIGH.
  - *Assertion*: `esp_deep_sleep_start()` occurs strictly after the second release window.

- **TC-R1-F1-06: Programmatic / Downlink Sleep Entry with Button Already HIGH**
  - *Objective*: Verify debounced sleep helper operates cleanly when triggered by SDN downlink command `CMD_FORCE_SLEEP` while button is untouched.
  - *Preconditions*: Device operating in LoRaWAN network; PRG button at resting state (HIGH).
  - *Test Vector*: Downlink frame arrives with `CMD_FORCE_SLEEP` (param = 5 minutes).
  - *Expected Behavior*: `digitalRead(PIN_PRG)` is immediately HIGH; release loop runs for exactly 80 ms; arms timer wakeup for 300s and ext0 on GPIO 0; halts.
  - *Assertion*: Execution delay in release loop is between 80 ms and 90 ms.

---

#### Feature 2: SX1262 LoRa Radio Ultra-Low-Power Sleep (`radio.sleep(false)`)
- **TC-R1-F2-01: SX1262 Sleep Command Execution**
  - *Objective*: Verify `radio.sleep()` is invoked before deep sleep entry.
  - *Preconditions*: Radio initialized and in Standby or RX mode.
  - *Test Vector*: `entrarEnDeepSleepSeguro()` invoked.
  - *Expected Behavior*: `radio.sleep(false)` is transmitted over SPI bus; SX1262 acknowledges and transitions into sleep mode.
  - *Assertion*: SPI bus transaction for OpCode `0x84` (`SetSleep`) with parameter `0x00` (cold sleep / warm retention) executed prior to `esp_deep_sleep_start()`.

- **TC-R1-F2-02: Radio Quiescent Current Draw Assertion**
  - *Objective*: Verify transceiver current drops to < 1.0 uA upon sleep command.
  - *Preconditions*: Multimeter or current analyzer measuring 3.3V rail to SX1262.
  - *Test Vector*: Execute `entrarEnDeepSleepSeguro()`.
  - *Expected Behavior*: SX1262 current drops from ~1.5 mA (Standby RC) to <= 0.6 uA (sleep mode).
  - *Assertion*: Current on SX1262 supply line <= 1.0 uA in deep sleep state.

- **TC-R1-F2-03: Radio Wakeup and Configuration Re-establishment**
  - *Objective*: Verify SX1262 wakes up and successfully transmits/receives after node wakes from deep sleep.
  - *Preconditions*: Device in deep sleep with SX1262 sleeping.
  - *Test Vector*: PRG button pressed; ESP32-S3 reboots; `setup()` initializes radio via `radio.begin()`.
  - *Expected Behavior*: Radio wakes from sleep via NSS low toggle / reset pulse; SPI communication succeeds; LoRaWAN session restored.
  - *Assertion*: `radio.begin()` returns `RADIOLIB_ERR_NONE` (0).

- **TC-R1-F2-04: Radio Sleep Call When Radio Is Idle vs Listening**
  - *Objective*: Verify `radio.sleep()` behaves cleanly whether radio is in RX continuous mode or standby.
  - *Preconditions*: Radio listening for LoRaWAN downlink (RX1/RX2 window).
  - *Test Vector*: Sleep triggered while RX is active.
  - *Expected Behavior*: Radio aborts RX reception cleanly, shuts down RF frontend, and enters sleep.
  - *Assertion*: Return code of `radio.sleep()` is `RADIOLIB_ERR_NONE`.

- **TC-R1-F2-05: Redundant Sleep Call Idempotency**
  - *Objective*: Verify that calling `radio.sleep()` when the radio is already in sleep state produces no bus hang or error.
  - *Preconditions*: Radio in sleep mode.
  - *Test Vector*: Invoke `radio.sleep()` a second time.
  - *Expected Behavior*: RadioLib handles state gracefully; no bus lockup; returns `RADIOLIB_ERR_NONE`.
  - *Assertion*: System does not hang; watchdog timer does not trigger.

---

#### Feature 3: Peripheral Rail & Pin Holds (Vext GPIO 36, ADC_CTRL GPIO 37, USER_LED GPIO 35)
- **TC-R1-F3-01: OLED Display Power-Down Command Precedence**
  - *Objective*: Verify `SSD1306_DISPLAYOFF` command is sent to OLED before cutting Vext rail.
  - *Preconditions*: OLED display active and rendering content.
  - *Test Vector*: Invoke `entrarEnDeepSleepSeguro()`.
  - *Expected Behavior*: I2C transaction `0x00, 0xAE` (DISPLAYOFF) sent to address 0x3C; followed by `digitalWrite(PIN_VEXT_CTL, HIGH)` to open P-MOSFET gate.
  - *Assertion*: I2C command precedes GPIO 36 rising edge by >= 10 us.

- **TC-R1-F3-02: Vext Rail Cutoff & Pad Hold Activation**
  - *Objective*: Verify GPIO 36 is driven HIGH and pad hold is enabled.
  - *Preconditions*: Node transitioning to deep sleep.
  - *Test Vector*: Execute `digitalWrite(PIN_VEXT_CTL, HIGH); gpio_hold_en((gpio_num_t)PIN_VEXT_CTL); gpio_deep_sleep_hold_en();`.
  - *Expected Behavior*: GPIO 36 measures 3.3V (HIGH); P-MOSFET is completely open; Vext voltage drops to 0.0V; state is retained across sleep.
  - *Assertion*: Voltage on Vext pin <= 50 mV in deep sleep; `gpio_is_deep_sleep_hold_enabled()` is true.

- **TC-R1-F3-03: Battery ADC Divider Disconnection & Pad Hold**
  - *Objective*: Verify GPIO 37 is driven LOW and pad hold is enabled.
  - *Preconditions*: Node transitioning to deep sleep.
  - *Test Vector*: Execute `digitalWrite(PIN_ADC_CTRL, LOW); gpio_hold_en((gpio_num_t)PIN_ADC_CTRL);`.
  - *Expected Behavior*: GPIO 37 measures 0.0V (LOW); N-MOSFET gate is grounded; current through 490k divider is 0.0 uA.
  - *Assertion*: Current through battery divider <= 0.05 uA; GPIO 37 pad hold active.

- **TC-R1-F3-04: User LED Extinction**
  - *Objective*: Verify user status LED (GPIO 35) is extinguished prior to deep sleep.
  - *Preconditions*: LED lit (e.g. from button acknowledgement pulse).
  - *Test Vector*: `digitalWrite(PIN_LED_USER, LOW);` executed.
  - *Expected Behavior*: GPIO 35 driven LOW; LED turns off completely.
  - *Assertion*: Voltage on GPIO 35 == 0.0V; LED current == 0 mA.

- **TC-R1-F3-05: Wakeup Pin Hold Release in Setup**
  - *Objective*: Verify `gpio_hold_dis()` and `gpio_deep_sleep_hold_dis()` are called upon boot so pins can be manipulated.
  - *Preconditions*: ESP32-S3 rebooting from deep sleep with pad holds active.
  - *Test Vector*: Execute `setup()` initialization sequence.
  - *Expected Behavior*: `gpio_deep_sleep_hold_dis()` releases output latches; `pinMode(PIN_VEXT_CTL, OUTPUT)` and `digitalWrite(PIN_VEXT_CTL, LOW)` successfully energize Vext rail.
  - *Assertion*: Vext rail measures 3.3V after `setup()`; OLED initializes successfully on I2C bus.

---

#### Feature 4: Ext0 Wakeup Re-arming & Deep Sleep Execution
- **TC-R1-F4-01: Ext0 Source Configuration on Level 0**
  - *Objective*: Verify `esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0)` arms wakeup on active LOW.
  - *Preconditions*: PRG button release verified (GPIO 0 is HIGH).
  - *Test Vector*: Arm ext0 wakeup and call `esp_deep_sleep_start()`.
  - *Expected Behavior*: RTC controller arms GPIO 0; system enters deep sleep; stays asleep while GPIO 0 remains HIGH.
  - *Assertion*: ESP32-S3 power current drops to deep sleep level (< 25 uA); does not reset.

- **TC-R1-F4-02: Strict Ordering Constraint: Ext0 Armed Post-Debounce**
  - *Objective*: Verify ext0 wakeup source is NOT enabled prior to debouncing.
  - *Preconditions*: Code inspection / static trace of `entrarEnDeepSleepSeguro()`.
  - *Test Vector*: Trace program counter or instrument debug markers.
  - *Expected Behavior*: Polling loop for button release executes entirely before `esp_sleep_enable_ext0_wakeup()` is called.
  - *Assertion*: Timestamp of `esp_sleep_enable_ext0_wakeup` > timestamp of release loop completion.

- **TC-R1-F4-03: Ext0 Trigger on Intentional User Press**
  - *Objective*: Verify device wakes up cleanly when user presses PRG button during deep sleep.
  - *Preconditions*: Device in deep sleep for >= 5 seconds.
  - *Test Vector*: PRG button pressed down (GPIO 0 driven to LOW).
  - *Expected Behavior*: RTC controller triggers reset; ESP32-S3 boots; ROM bootloader executes; `setup()` runs.
  - *Assertion*: `esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0`.

- **TC-R1-F4-04: Timer Wakeup Co-existence with Ext0**
  - *Objective*: Verify timer wakeup works concurrently with ext0 wakeup when `sleep_segundos > 0`.
  - *Preconditions*: Sleep requested for 10 seconds (`sleep_segundos = 10`).
  - *Test Vector*: Device enters sleep; PRG button is NOT pressed.
  - *Expected Behavior*: Device sleeps for 10 seconds; RTC timer triggers wakeup at t = 10s; ESP32-S3 reboots.
  - *Assertion*: `esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER`; elapsed time between sleep and boot is 10,000 ms +/- 2%.

- **TC-R1-F4-05: Ext0 Preempting Timer Wakeup**
  - *Objective*: Verify pressing PRG button before timer expiration wakes the device early with ext0 cause.
  - *Preconditions*: Device configured for 300s timer sleep.
  - *Test Vector*: User presses PRG button at t = 5 seconds after sleep entry.
  - *Expected Behavior*: Device wakes up immediately at t = 5s without waiting for 300s timer.
  - *Assertion*: `esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0`.

---

### Group R2: OLED Menu Redesign Subsystem

#### Feature 5: Clumsy Footer Legend Removal
- **TC-R2-F5-01: Screen 0 (HOME) Footer Absence Assertion**
  - *Objective*: Verify Screen 0 rendering buffer contains zero pixels from `[Mantener PRG: Menu]` and no dividing line at y = 55.
  - *Preconditions*: OLED initialized, `renderPantallaHome()` executed.
  - *Test Vector*: Capture display framebuffer (1024 bytes).
  - *Expected Behavior*: Pixel scan between y = 54 and y = 63 shows no horizontal line at y=55 and no glyph pattern for `[Mantener PRG: Menu]`.
  - *Assertion*: String search for `"[Mantener"` in Screen 0 source returns 0 matches; framebuffer line y=55 is black (all 0x00) unless legitimate body data occupies it.

- **TC-R2-F5-02: Screen 1 (BALANZA) Footer Absence Assertion**
  - *Objective*: Verify Screen 1 rendering buffer contains zero pixels from `[Mantener: Calibrar]`.
  - *Preconditions*: `renderPantallaBalanza()` executed.
  - *Test Vector*: Inspect source and framebuffer.
  - *Expected Behavior*: Line y=54 dividing rule removed; string `[Mantener: Calibrar]` completely absent.
  - *Assertion*: Source scan for `"[Mantener: Calibrar]"` returns 0 matches.

- **TC-R2-F5-03: Screen 2 (BATERIA) Static Footer Replacement**
  - *Objective*: Verify Screen 2 static footer `OLED Sleep: ...` is eliminated from bottom line and integrated cleanly into body.
  - *Preconditions*: `renderPantallaBateria()` executed.
  - *Test Vector*: Inspect framebuffer lines y=54..63.
  - *Expected Behavior*: Bottom dividing rule removed; energy parameters displayed in body (y=53) as `Sleep: 5m | ADC: 10s` (<= 21 chars).
  - *Assertion*: No footer text starting at y=56.

- **TC-R2-F5-04: Screen 3 (COMMS) Footer Absence Assertion**
  - *Objective*: Verify Screen 3 rendering buffer contains zero occurrences of `[Mantener: Config TX]`.
  - *Preconditions*: `renderPantallaComms()` executed.
  - *Test Vector*: Source and framebuffer analysis.
  - *Expected Behavior*: Dividing line at y=54 removed; `[Mantener: Config TX]` removed; body area expanded.
  - *Assertion*: String search for `"Config TX"` footer returns 0 matches.

- **TC-R2-F5-05: Screen 4 (SDN) Footer Absence Assertion**
  - *Objective*: Verify Screen 4 contains zero occurrences of `[Mantener: Accion SDN]` / `[Mantener: Test SDN]`.
  - *Preconditions*: `renderPantallaSDN()` executed.
  - *Test Vector*: Source and framebuffer analysis.
  - *Expected Behavior*: Dividing line at y=54 removed; footer legend removed.
  - *Assertion*: String search for `"[Mantener:"` across entire codebase returns 0 matches.

- **TC-R2-F5-06: Global Vertical Space Reclamation Assertion**
  - *Objective*: Verify body rendering coordinates on all screens utilize y = 10 to y = 63 without overlap or clipping.
  - *Preconditions*: All 5 top-level screens rendered sequentially.
  - *Test Vector*: Measure vertical extents of text/graphics for each screen.
  - *Expected Behavior*: Content spans up to y = 61 or 62; bottom border remains clean; 10 extra vertical pixels successfully utilized for telemetry.
  - *Assertion*: Max Y coordinate of drawn content <= 63; min Y coordinate of body >= 10.

---

#### Feature 6: OLED Text Width & String Truncation Compliance (<= 21 Chars / 128 px)
- **TC-R2-F6-01: Screen 0 Primary Weight String Width Boundary**
  - *Objective*: Verify weight display line never exceeds 21 characters across all float values.
  - *Preconditions*: `pesoBrutoKg = 99.99`, `voltajeSensorV = 3.300`.
  - *Test Vector*: Formatted string `snprintf(buf, sizeof(buf), "B: %.2fkg V:%.2fV", pesoBrutoKg, voltajeSensorV);`.
  - *Expected Behavior*: Generates `"B: 99.99kg V:3.30V"` (18 characters = 108 pixels).
  - *Assertion*: `strlen(buf) <= 21` (18 <= 21); rightmost pixel at `0 + (18 * 6) = 108 <= 127`.

- **TC-R2-F6-02: Screen 2 Battery Timeout String Formatting**
  - *Objective*: Verify battery timeout line does not exceed 21 characters for any timeout index.
  - *Preconditions*: `timeoutPantallaIdx` tested for all values (0: 1m, 1: 5m, 2: 10m, 3: ON).
  - *Test Vector*: Formatted string `snprintf(buf, sizeof(buf), "Sleep: %s | ADC:%s", tout_txt, adc_txt);`.
  - *Expected Behavior*: Worst case: `"Sleep: 10m | ADC:300s"` -> 21 characters exactly = 126 pixels.
  - *Assertion*: `strlen(buf) <= 21`; rightmost pixel at `126 <= 127`; zero clipping.

- **TC-R2-F6-03: Screen 3 Comms ToA and RF String Formatting**
  - *Objective*: Verify time-on-air and LoRa parameters do not overflow screen border.
  - *Preconditions*: `current_toa_ms = 999.9`, `current_sf = 12`, `current_tx_power = 22`.
  - *Test Vector*: Formatted string `snprintf(buf, sizeof(buf), "ToA:%lums SF%u@%ddBm", (uint32_t)toa, sf, pwr);`.
  - *Expected Behavior*: Generates `"ToA:999ms SF12@22dBm"` -> 20 characters = 120 pixels.
  - *Assertion*: `strlen(buf) <= 21`; rightmost pixel at `120 <= 127`.

- **TC-R2-F6-04: Screen 4 SDN Command Name Truncation**
  - *Objective*: Verify SDN command names are safely abbreviated to prevent line overflow.
  - *Preconditions*: `sdn_last_cmd` set to `CMD_SET_TEMP_THRESHOLD` (0x02) and `CMD_RECONFIG_RADIO` (0x01).
  - *Test Vector*: Display formatted string `snprintf(buf, sizeof(buf), "CMD: 0x%02X (%s)", cmd, getShortCmdName(cmd));`.
  - *Expected Behavior*: `getShortCmdName(0x02)` returns `"SET_TH"`; string is `"CMD: 0x02 (SET_TH)"` (18 chars = 108 px).
  - *Assertion*: `strlen(buf) <= 21`; overflow eliminated.

- **TC-R2-F6-05: Boot Splash Text Coordinate Bounds**
  - *Objective*: Verify splash screen text starts at coordinates that do not clip the right margin.
  - *Preconditions*: Boot splash rendered in `setup()`.
  - *Test Vector*: Diag sketch splash `"Modo Diag Local"` (15 chars) at x = 18; Battery splash `"Iniciando LoRaWAN"` (17 chars) at x = 12.
  - *Expected Behavior*: Diag right edge: `18 + (15 * 6) = 108 <= 127`. Battery right edge: `12 + (17 * 6) = 114 <= 127`.
  - *Assertion*: Maximum X coordinate <= 127; 0 pixels clipped.

- **TC-R2-F6-06: Global Regex Zero-Clipping Assertion**
  - *Objective*: Perform static analysis on all `display.print()` and `display.printf()` calls in both sketches.
  - *Test Vector*: Scan every string literal and format template for line length and start coordinate.
  - *Expected Behavior*: For every font size 1 print at starting column `x_cur`, `(strlen(literal) * 6) + x_cur <= 128`.
  - *Assertion*: 100% compliance across all 5 screens and modals.

---

#### Feature 7: Technical Header Bar with RTC Clock & Page Dots
- **TC-R2-F7-01: Header Bar Geometry and Separator Rule**
  - *Objective*: Verify header bar occupies y = 0 to 8 with horizontal dividing rule at y = 9.
  - *Preconditions*: `dibujarCabeceraCompacta(pantalla)` invoked.
  - *Test Vector*: Check draw calls in `dibujarCabeceraCompacta`.
  - *Expected Behavior*: Horizontal line drawn via `display.drawLine(0, 9, 127, 9, SSD1306_WHITE);`; no header text drawn at y >= 9.
  - *Assertion*: Line drawn at y = 9; header height == 10 pixels (y = 0..9).

- **TC-R2-F7-02: Screen Title Abbreviation Rendering**
  - *Objective*: Verify 4-character screen title renders cleanly at x = 0..24.
  - *Preconditions*: Test screens 0 through 4 (`HOME`, `BALA`, `BATT`, `COMM`, `SDN`).
  - *Test Vector*: `display.setCursor(0, 0); display.print(etiquetas[pantalla]);`.
  - *Expected Behavior*: Title text width = 4 * 6 = 24 pixels (x = 0 to 23); leaves 18 pixels margin before RTC clock.
  - *Assertion*: Screen title does not collide with RTC clock (x_end <= 24, rtc_x_start >= 36).

- **TC-R2-F7-03: RTC Clock Format and Placement**
  - *Objective*: Verify RTC clock renders formatted string `HH:MM:SS` (8 characters = 48 px) centered at x = 40..87.
  - *Preconditions*: Internal RTC initialized with valid epoch.
  - *Test Vector*: `obtenerHoraRTC(buf, sizeof(buf)); display.setCursor(40, 0); display.print(buf);`.
  - *Expected Behavior*: Correct 24-hour time displayed; string length exactly 8 chars; width = 48 pixels.
  - *Assertion*: Characters match regex `^[0-2][0-9]:[0-5][0-9]:[0-5][0-9]$`.

- **TC-R2-F7-04: RTC Unsynchronized / Fallback Display State**
  - *Objective*: Verify header displays clean fallback when RTC has not yet synchronized with LoRaWAN network.
  - *Preconditions*: `rtc_sincronizado = false`.
  - *Test Vector*: Render header prior to network join.
  - *Expected Behavior*: Clock displays `--:--:--` or `00:00:00` (8 chars, 48 px); sync dot at x = 90 is unlit/cleared.
  - *Assertion*: Fallback string length <= 8 characters; no garbage or negative numbers.

- **TC-R2-F7-05: Network Time Synchronization Dot Indicator**
  - *Objective*: Verify synchronization status dot indicates ChirpStack DeviceTime status.
  - *Preconditions*: Test both `rtc_sincronizado = false` and `rtc_sincronizado = true`.
  - *Test Vector*: Render header in both states.
  - *Expected Behavior*: When false, pixel at (x=90, y=4) is unlit (`0`); when true, pixel is lit (`1` or small 2x2 dot).
  - *Assertion*: Sync dot state strictly mirrors `rtc_sincronizado` boolean.

- **TC-R2-F7-06: 5-Page Circular Progress Indicators**
  - *Objective*: Verify 5 circular progress dots represent active screen index without overlapping border.
  - *Preconditions*: Cycle through `pantallaActual` = 0, 1, 2, 3, 4.
  - *Test Vector*: Dots rendered at center coordinates `cx = 98, 104, 110, 116, 122; cy = 4; r = 2`.
  - *Expected Behavior*: Active screen dot is drawn with `fillCircle` (solid); remaining 4 dots drawn with `drawCircle` (outline); rightmost dot edge at `122 + 2 = 124 <= 127`.
  - *Assertion*: Exactly one dot is filled per screen; all dots fit within x = 96..126; cy = 4 fits within y = 0..8.

---

#### Feature 8: Dynamic Submenu Modal Architecture & Bounds
- **TC-R2-F8-01: 5-Item Modal Vertical Geometry and Clearance**
  - *Objective*: Verify 5-item submenu renders within the 64-pixel display height without clipping.
  - *Preconditions*: Modal invoked with `totalOpciones = 5`.
  - *Test Vector*: Calculate vertical positions for 9-pixel item pitch:
    - Dialog border: `display.drawRect(2, 2, 124, 60, SSD1306_WHITE);` (y = 2 to 61).
    - Inverted title: `display.fillRect(3, 3, 122, 10, SSD1306_WHITE);` (y = 3 to 12).
    - Item 0: y = 15 (y_bottom = 22)
    - Item 1: y = 24 (y_bottom = 31)
    - Item 2: y = 33 (y_bottom = 40)
    - Item 3: y = 42 (y_bottom = 49)
    - Item 4: y = 51 (y_bottom = 58)
  - *Expected Behavior*: Item 4 bottom edge at y = 58; clearance inside dialog box is `61 - 58 = 3` pixels. Zero clipping.
  - *Assertion*: Bottom pixel of item 4 text (y=58) < dialog bottom boundary (y=61) < screen height (64).

- **TC-R2-F8-02: Selection Highlight Box and Cursor Indicator**
  - *Objective*: Verify selected option is highlighted with inverted white box and `>` cursor.
  - *Preconditions*: Modal active, `submenuCursor = 3`.
  - *Test Vector*: `renderSubmenu("INTERVALO ADC", ops, 5, 3);`.
  - *Expected Behavior*: Inverted box drawn at `(5, 41, 118, 9)`; cursor `>` printed at (7, 42); text printed in black at (15, 42); unselected items in white.
  - *Assertion*: Highlight box does not overlap adjacent item text (y=33 or y=51).

- **TC-R2-F8-03: Dynamic Modulo Cursor Wrapping for 5 Items**
  - *Objective*: Verify short presses cycle through all 5 items and wrap from 4 back to 0.
  - *Preconditions*: Submenu open with 5 options (`totalOpciones = 5`), `submenuCursor = 0`.
  - *Test Vector*: 5 consecutive short presses of PRG button.
  - *Expected Behavior*: Cursor sequence: 0 -> 1 -> 2 -> 3 -> 4 -> 0.
  - *Assertion*: `submenuCursor` visits each index 0..4; wraps to 0 on 5th press; never exceeds 4.

- **TC-R2-F8-04: Backward Compatibility Modulo Wrapping for 4 Items**
  - *Objective*: Verify standard 4-item submenus (e.g. HOME menu) wrap correctly from 3 to 0.
  - *Preconditions*: HOME submenu open (`totalOpciones = 4`), `submenuCursor = 0`.
  - *Test Vector*: 4 consecutive short presses of PRG button.
  - *Expected Behavior*: Cursor sequence: 0 -> 1 -> 2 -> 3 -> 0.
  - *Assertion*: `submenuCursor` wraps to 0 on 4th press; never reaches 4.

- **TC-R2-F8-05: Submenu Inactivity Auto-Dismissal**
  - *Objective*: Verify submenu closes automatically after 10 seconds of button inactivity.
  - *Preconditions*: Submenu open (`submenuActivo = true`), `submenuUltimaActividad = millis()`.
  - *Test Vector*: Advance time by 10,001 ms without button press.
  - *Expected Behavior*: `gestionarBotonPRG()` evaluates `millis() - submenuUltimaActividad > TIMEOUT_SUBMENU_MS`; sets `submenuActivo = false`; `submenuCursor = 0`; screen reverts to underlying dashboard.
  - *Assertion*: `submenuActivo == false` at t = 10,050 ms.

- **TC-R2-F8-06: Option String Width Clamping in Modal**
  - *Objective*: Verify option strings in modal fit within the 118-pixel inner box width.
  - *Preconditions*: 5-item ADC menu strings: `"Solo en TX"`, `"Cada 10s"`, `"Cada 30s"`, `"Cada 60s"`, `"Cada 300s"`.
  - *Test Vector*: Measure character lengths.
  - *Expected Behavior*: Longest string is `"Solo en TX"` (10 chars = 60 px). Starts at x = 15; right edge at `15 + 60 = 75 <= 122`.
  - *Assertion*: Max item text width <= 107 pixels (122 - 15).

---

## Tier 2: Boundary & Corner Cases (>= 5 Tests Per Feature)

### Group R1: Debounced Deep Sleep Entry Subsystem

#### Feature 1: Debounced Physical Release (GPIO 0)
- **TB-R1-F1-01: 79 ms Stable HIGH Followed by 1 ms LOW Glitch**
  - *Condition*: Button released; reads HIGH for 79 ms; microsecond contact bounce pulls pin LOW for 1 ms; then reads HIGH again.
  - *Requirement*: Debounce counter MUST reset to 0 upon the 1 ms glitch.
  - *Pass Criteria*: Firmware does NOT enter sleep at t = 80 ms; waits for a full new 80 ms continuous HIGH window before sleeping.

- **TB-R1-F1-02: Exactly 80 ms Stable HIGH Boundary**
  - *Condition*: Button released; reads HIGH for exactly 80.0 ms without glitch.
  - *Requirement*: Minimal boundary threshold pass.
  - *Pass Criteria*: Firmware exits release loop immediately upon reaching 80 ms and executes deep sleep entry.

- **TB-R1-F1-03: Rapid Mechanical Chattering on Release (50 Hz Bounce)**
  - *Condition*: Button switch contacts chatter between HIGH and LOW every 5 ms for a duration of 45 ms before settling HIGH.
  - *Requirement*: Filter must ignore all intermediate HIGH segments (< 80 ms).
  - *Pass Criteria*: Sleep entry occurs exactly 80 ms after the final bounce edge (total time from initial release = 45 ms chatter + 80 ms stable = 125 ms).

- **TB-R1-F1-04: Permanent LOW Stuck-Button Protection / Timeout**
  - *Condition*: User physically wedges button down or switch contact solders shut permanently.
  - *Requirement*: System must not hang indefinitely in an un-interruptible tight loop that blocks watchdog timers.
  - *Pass Criteria*: Either loop feeds task watchdog / executes `yield()`/`delay()`, or an operational timeout (e.g. 15s) aborts sleep attempt and returns to main loop with error log.

- **TB-R1-F1-05: Hair-Trigger Immediate Depression at Millisecond 81**
  - *Condition*: User releases button for 81 ms, then presses down firmly again just as `esp_sleep_enable_ext0_wakeup()` is executing.
  - *Requirement*: RTC controller catches level 0; ESP32-S3 either wakes immediately or reboots.
  - *Pass Criteria*: System does not enter an unrecoverable lockup state; executes clean reboot.

---

#### Feature 2: SX1262 LoRa Radio Sleep Retention
- **TB-R1-F2-01: Sleep Commanded While Transmitting Uplink (Radio BUSY)**
  - *Condition*: Sleep action triggered while SX1262 DIO1 is active or RF power amplifier is transmitting.
  - *Requirement*: Firmware must wait for TX completion (`STANDBY_RC` state) before commanding sleep.
  - *Pass Criteria*: Transmission completes without RF packet corruption; `radio.sleep()` called only when BUSY pin is LOW.

- **TB-R1-F2-02: SPI Clock Frequency Boundary During Sleep Command**
  - *Condition*: SPI bus operated at maximum 16 MHz vs minimum 1 MHz.
  - *Requirement*: SX1262 `SetSleep` command opcode (0x84) must be latched reliably.
  - *Pass Criteria*: `radio.sleep()` succeeds across all supported SPI clock frequencies; sleep current confirmed < 1 uA.

- **TB-R1-F2-03: Retention RAM Integrity Verification (Warm Start)**
  - *Condition*: Radio put to sleep with `retainConfig = true` vs `retainConfig = false`.
  - *Requirement*: When `retainConfig = true`, modem registers (frequency, SF, BW, CR) must remain preserved in retention RAM.
  - *Pass Criteria*: Upon waking with `radio.standby()`, register values match pre-sleep configuration without full re-initialization.

- **TB-R1-F2-04: Radio Sleep Call with Supply Rail at Minimum Operating Voltage (2.0V)**
  - *Condition*: Battery discharged to 3.0V (LDO output 2.7V - 3.0V).
  - *Requirement*: SX1262 brownout circuitry must not trigger erroneous state.
  - *Pass Criteria*: `radio.sleep()` command completes cleanly without freezing on BUSY line.

- **TB-R1-F2-05: Rapid Sleep-Wake-Sleep Cycling (100 ms Interval)**
  - *Condition*: Node wakes, immediately triggers sleep, wakes, and sleeps in rapid succession.
  - *Requirement*: Transceiver must tolerate rapid DC-DC converter startup and shutdown cycles.
  - *Pass Criteria*: No SPI communication timeout; no bus deadlock; SX1262 current settles to < 1 uA on every sleep cycle.

---

#### Feature 3: Peripheral Rail & Pin Holds (Vext & ADC_CTRL)
- **TB-R1-F3-01: Vext P-MOSFET Gate Voltage Drift in Deep Sleep**
  - *Condition*: ESP32-S3 deep sleep maintained for 24 hours.
  - *Requirement*: GPIO 36 pad hold must maintain gate at 3.3V (HIGH) against junction leakage.
  - *Pass Criteria*: Vext voltage remains < 50 mV after 24 hours; OLED current remains 0.0 uA.

- **TB-R1-F3-02: Battery ADC Divider MOSFET Gate Drift**
  - *Condition*: GPIO 37 pad hold maintained at LOW across long sleep.
  - *Requirement*: MOSFET must not experience subthreshold conduction.
  - *Pass Criteria*: Voltage on GPIO 1 (ADC input) remains 0.0V; current through divider < 50 nA.

- **TB-R1-F3-03: Pad Hold Disable Order on Boot**
  - *Condition*: Device boots from deep sleep; `setup()` runs.
  - *Requirement*: Pad hold MUST be disabled before re-configuring `pinMode()` and driving pins LOW.
  - *Pass Criteria*: Calling `gpio_deep_sleep_hold_dis()` allows GPIO 36 to be driven LOW cleanly; Vext powers on to 3.3V without glitch.

- **TB-R1-F3-04: I2C Bus State when Vext is Disconnected**
  - *Condition*: Vext cut to OLED; I2C SDA (GPIO 17) and SCL (GPIO 18) pins left as standard outputs vs set to floating/input.
  - *Requirement*: Avoid back-powering OLED through internal ESD protection diodes.
  - *Pass Criteria*: Parasitic current through I2C pull-ups into unpowered OLED <= 1 uA.

- **TB-R1-F3-05: LED Output State Under Power Glitch**
  - *Condition*: USER_LED (GPIO 35) driven LOW before sleep; transient 3.3V rail ripple occurs.
  - *Requirement*: LED must remain completely dark during deep sleep.
  - *Pass Criteria*: Zero light output; voltage across LED == 0.0V.

---

#### Feature 4: Ext0 Wakeup Re-arming & Deep Sleep
- **TB-R1-F4-01: Ext0 Wakeup Trigger Pulse Width Threshold**
  - *Condition*: Button pressed for a very short duration during deep sleep (1 us, 10 us, 100 us, 1 ms).
  - *Requirement*: ESP32-S3 RTC controller requires pulse width longer than RTC slow clock period (~7 us at 150 kHz).
  - *Pass Criteria*: Pulses > 15 us reliably wake the device from deep sleep.

- **TB-R1-F4-02: Zero-Duration Timer Wakeup (`sleep_segundos = 0`)**
  - *Condition*: Function called with `sleep_segundos = 0`.
  - *Requirement*: Timer wakeup must NOT be enabled; system must rely exclusively on ext0 button wakeup.
  - *Pass Criteria*: Device remains asleep indefinitely until PRG button is physically pressed.

- **TB-R1-F4-03: Maximum 32-Bit Timer Wakeup Duration (`0xFFFFFFFF` seconds)**
  - *Condition*: Function called with maximum 32-bit integer seconds (~136 years).
  - *Requirement*: Multiplication `(uint64_t)sleep_segundos * 1000000ULL` must not overflow 64-bit integer.
  - *Pass Criteria*: 64-bit microsecond value evaluates correctly without truncation or negative value.

- **TB-R1-F4-04: Simultaneous Wakeup Trigger Arbitration**
  - *Condition*: PRG button pressed at the exact microsecond the RTC timer expires.
  - *Requirement*: RTC controller hardware arbitration resolves the event cleanly.
  - *Pass Criteria*: Device boots without exception; `esp_sleep_get_wakeup_cause()` returns a valid cause (`EXT0` or `TIMER`).

- **TB-R1-F4-05: Sub-Zero Temperature Button Contact Behavior (-10 deg C)**
  - *Condition*: Tactile switch operated in freezing field conditions (increased contact resistance).
  - *Requirement*: Internal pull-up (~45 kOhm) must still pull GPIO 0 below 0.25 * VDD (0.82V) when pressed, and above 0.75 * VDD (2.47V) when released.
  - *Pass Criteria*: Logic LOW and HIGH levels detected reliably; debounce loop completes in cold environment.

---

### Group R2: OLED Menu Redesign Subsystem

#### Feature 5: Clumsy Footer Legend Removal
- **TB-R2-F5-01: Screen Body Line at Exact Boundary y = 54**
  - *Condition*: Render text or telemetry line with baseline at y = 54 (where old dividing line was).
  - *Requirement*: Text must render cleanly without collision or clipping.
  - *Pass Criteria*: Full 8-pixel glyph height (y=47..54 or y=54..61) displays crisply on OLED matrix.

- **TB-R2-F5-02: Screen Body Line at Exact Bottom Boundary y = 63**
  - *Condition*: Render single-pixel horizontal indicator or line at y = 63.
  - *Requirement*: Drawn exactly on the bottommost row of the physical display.
  - *Pass Criteria*: Pixels at y=63 illuminate; no write occurs to nonexistent row y=64 (no framebuffer overrun).

- **TB-R2-F5-03: Rapid Screen Switching (50 ms per screen)**
  - *Condition*: User cycles through all 5 screens in 250 ms.
  - *Requirement*: Framebuffer must clear previous screen data completely in y=54..63.
  - *Pass Criteria*: Zero ghost pixels from previous screen; each screen renders only its own telemetry.

- **TB-R2-F5-04: Display Re-Initialization After Vext Power Cycle**
  - *Condition*: Screen turned off via menu (`SSD1306_DISPLAYOFF`), then re-awakened via button press.
  - *Requirement*: Display re-initializes without recreating old footer lines.
  - *Pass Criteria*: Waking screen displays new clean layout without any footer legends.

- **TB-R2-F5-05: Framebuffer Boundary Enforcement (1024-Byte Memory Check)**
  - *Condition*: Continuous drawing across all screens and modals.
  - *Requirement*: Framebuffer memory bounds (`display.getBuffer()`, 128 * 64 / 8 = 1024 bytes) must never be exceeded.
  - *Pass Criteria*: Zero heap corruption; zero stack smashing; valid memory bounds confirmed.

---

#### Feature 6: OLED Text Width & Truncation Compliance
- **TB-R2-F6-01: Exactly 21 Characters at Column x = 0**
  - *Condition*: String of exactly 21 characters: `"123456789012345678901"` rendered at (0, 20).
  - *Requirement*: Pixel width = 21 * 6 = 126 pixels (x = 0 to 125).
  - *Pass Criteria*: Rightmost character glyph occupies pixels 120..124; column 125 is spacing; columns 126 and 127 remain blank margin. Zero pixels clipped.

- **TB-R2-F6-02: 22 Characters at Column x = 0 (Violation Boundary Test)**
  - *Condition*: String of 22 characters: `"1234567890123456789012"` rendered at (0, 20).
  - *Requirement*: Pixel width = 22 * 6 = 132 pixels. Exceeds 128 px display.
  - *Pass Criteria*: Test harness detects and flags this as a string formatting violation; firmware code MUST NOT generate 22-char strings.

- **TB-R2-F6-03: Exactly 20 Characters at Column x = 4**
  - *Condition*: String of 20 characters rendered at x = 4.
  - *Requirement*: Right edge = `4 + (20 * 6) = 124 <= 127`.
  - *Pass Criteria*: Text fits with 3 pixels right margin; zero clipping.

- **TB-R2-F6-04: 21 Characters at Column x = 4 (Clipping Boundary Test)**
  - *Condition*: String of 21 characters rendered at x = 4.
  - *Requirement*: Right edge = `4 + (21 * 6) = 130 > 127`.
  - *Pass Criteria*: Last character is clipped off by 3 pixels; test harness detects and forbids starting 21-char strings at x > 0.

- **TB-R2-F6-05: Floating Point Extreme Value Expansion in snprintf**
  - *Condition*: Sensor voltage reads negative (`-0.0050V`) or large (`12.3456V`); battery percentage reads `100%`.
  - *Requirement*: Format strings must use explicit field-width specifiers or safe bounding (e.g. `snprintf(buf, sizeof(buf), "%.2f", val)`).
  - *Pass Criteria*: Formatted output never exceeds 21 characters regardless of input floating point magnitude.

---

#### Feature 7: Technical Header Bar with RTC
- **TB-R2-F7-01: Midnight Rollover Transition**
  - *Condition*: Internal RTC rolls over from 23:59:59 to 00:00:00 during display refresh.
  - *Requirement*: Time string must transition cleanly between valid representations.
  - *Pass Criteria*: Header displays `"00:00:00"`; no character misalignment or partial redraw artifacts.

- **TB-R2-F7-02: Network Time Sudden Jump (POSIX settimeofday)**
  - *Condition*: LoRaWAN `DeviceTimeAns` updates system time from unsynced 00:05:22 to UTC 14:32:00.
  - *Requirement*: Next header redraw immediately reflects the new time without crashing or flickering.
  - *Pass Criteria*: Header displays `"14:32:00"`; sync dot turns ON at x = 90.

- **TB-R2-F7-03: Header Separator Line Continuity**
  - *Condition*: Pixel line at y = 9 drawn from x = 0 to x = 127.
  - *Requirement*: Continuous white line across all 128 pixels without gaps.
  - *Pass Criteria*: Every pixel in row y=9 is illuminated (`1`); separates header (y=0..8) from body (y=10..63).

- **TB-R2-F7-04: Active Page Indicator at Extremes (Page 0 and Page 4)**
  - *Condition*: Page 0 active (`HOME`) vs Page 4 active (`SDN`).
  - *Requirement*: Dot 0 (cx=98) filled for Page 0; Dot 4 (cx=122) filled for Page 4.
  - *Pass Criteria*: Dot coordinates remain within x = 96..124; no dot clips the right screen edge (127).

- **TB-R2-F7-05: High Frequency Loop Refresh (Header Flicker Prevention)**
  - *Condition*: Display redraw called 10 times per second in main loop.
  - *Requirement*: Header redraw must only update time when seconds value changes.
  - *Pass Criteria*: Zero visual flicker on OLED; stable display rendering.

---

#### Feature 8: Dynamic Submenu Modal Architecture
- **TB-R2-F8-01: 5-Item Selection Boundary at Index 4 (Last Item)**
  - *Condition*: Submenu open with 5 items; `submenuCursor = 4` (last item).
  - *Requirement*: Inverted white highlight box rendered for item 4 (`y = 51`); text `"Cada 300s"`.
  - *Pass Criteria*: Highlight box bottom at y = 59; does not touch or exceed dialog bottom border at y = 61.

- **TB-R2-F8-02: Inactivity Timeout Boundary at Exactly 10,000 ms**
  - *Condition*: Submenu open; button untouched for 9,990 ms vs 10,010 ms.
  - *Requirement*: At 9,990 ms, submenu remains active; at 10,010 ms, submenu closes.
  - *Pass Criteria*: Submenu dismissed precisely on or after the 10,000 ms mark.

- **TB-R2-F8-03: Single Item Submenu Boundary (`totalOpciones = 1`)**
  - *Condition*: Modal called with `totalOpciones = 1`.
  - *Requirement*: `submenuCursor = (submenuCursor + 1) % 1` always yields 0.
  - *Pass Criteria*: Cursor remains on item 0; no divide-by-zero exception; clean rendering.

- **TB-R2-F8-04: Modulo with `totalOpciones = 0` Defense**
  - *Condition*: Erroneous call with `totalOpciones = 0`.
  - *Requirement*: Division by zero in modulo operation must be prevented by guard clause (`if (totalOpciones <= 0) return;`).
  - *Pass Criteria*: CPU does not trigger integer divide-by-zero panic or Guru Meditation Error.

- **TB-R2-F8-05: Submenu Title String Length Boundary**
  - *Condition*: Title string of 18 characters: `"CONFIGURACION ADC"`.
  - *Requirement*: Title printed in inverted white box (width = 122 px, x = 3 to 124).
  - *Pass Criteria*: 18 chars * 6 px = 108 px; starting at x = 6 yields right edge at `6 + 108 = 114 <= 124`. Fits cleanly within title bar.

---

## Tier 3: Pairwise Combinations (Orthogonal Array Test Matrix)

The system behavior is verified across all pairwise interactions among 5 key operational factors:
- **Factor A: Sleep Mode Entry Vector** {Menu PRG Long-Press, SDN Downlink CMD_FORCE_SLEEP, Automatic Battery Exhaustion}
- **Factor B: Radio State** {Unjoined / Idle, Joining OTAA, Joined Connected, TX In Progress}
- **Factor C: OLED UI Active Screen** {HOME, BALANZA, BATERIA, COMMS, Submenu Active}
- **Factor D: RTC Synchronization State** {Unsynchronized (`rtc_sincronizado = false`), Synced via DeviceTimeReq (`rtc_sincronizado = true`)}
- **Factor E: PRG Button Physical Release Profile** {Clean Release (0 ms bounce), Typical Bounce (25 ms bounce), Extended Hold (2000 ms hold)}

### Pairwise Test Matrix Table

| Test ID | Sleep Entry (A) | Radio State (B) | Screen (C) | RTC State (D) | PRG Release Profile (E) | Expected System Assertion |
|---|---|---|---|---|---|---|
| **PW-01** | Menu PRG Long-Press | Joined Connected | Submenu Active | Synced | Typical Bounce (25 ms) | Debounce loop discards 25ms bounce; waits 80ms stable HIGH; radio.sleep() called; Vext pad hold set; clean ext0 sleep entry. |
| **PW-02** | Menu PRG Long-Press | Unjoined / Idle | HOME | Unsynchronized | Extended Hold (2000 ms) | System waits 2000ms until physical release; 80ms debounce; RTC fallback `--:--:--` in header; enters sleep cleanly. |
| **PW-03** | Menu PRG Long-Press | TX In Progress | BALANZA | Synced | Clean Release (0 ms) | System waits for LoRa TX completion; puts radio to sleep; executes 80ms release; arms ext0 and enters sleep. |
| **PW-04** | SDN Downlink Sleep | Joined Connected | COMMS | Synced | Clean Release (Untouched) | Button already HIGH; release loop runs for 80ms immediately; radio sleeping; arms timer and ext0; halts. |
| **PW-05** | SDN Downlink Sleep | Joining OTAA | BATERIA | Unsynchronized | Typical Bounce (Noise) | Software command overrides UI; radio put to sleep; Vext cut; wakes upon timer expiration. |
| **PW-06** | SDN Downlink Sleep | Unjoined / Idle | Submenu Active | Synced | Extended Hold (Held) | Even if operator is touching button, system requires release before sleep; preserves NVS config. |
| **PW-07** | Auto Low-Battery | Joined Connected | HOME | Synced | Clean Release (Untouched) | Low battery threshold triggers auto-sleep; OLED DISPLAYOFF; Vext cut; ADC_CTRL LOW; radio.sleep(); ext0 armed. |
| **PW-08** | Auto Low-Battery | Unjoined / Idle | BATERIA | Unsynchronized | Extended Hold (Held) | Low battery protection overrides menu; enforces debounced release; enters deep sleep to protect LiPo from undervoltage. |
| **PW-09** | Menu PRG Long-Press | Joining OTAA | Submenu Active (ADC) | Unsynchronized | Clean Release (0 ms) | 5-item ADC menu closes; NVS persists choice; 80ms debounce; radio aborted into sleep; enters deep sleep. |
| **PW-10** | SDN Downlink Sleep | TX In Progress | HOME | Synced | Extended Hold (Held) | Transmit completes; release loop holds until user releases PRG; wakes on scheduled timer. |
| **PW-11** | Auto Low-Battery | TX In Progress | BALANZA | Synced | Typical Bounce (25 ms) | Final TX telemetry sent; battery divider isolated; debounce verified; enters deep sleep. |
| **PW-12** | Menu PRG Long-Press | Joined Connected | COMMS | Synced | Extended Hold (2000 ms) | User holds button through menu action; CPU polls release safely; sleeps after 80ms stable HIGH; wakes on new press. |

---

## Tier 4: Real-World Scenarios

### Scenario 1: Field Operator Manual Shutdown in Cold Agricultural Environment
- **Context**: Operator in poultry farm (-5 deg C) wants to put sensor node to sleep for seasonal maintenance.
- **Trigger**: Operator navigates to Screen 0 (HOME), holds PRG button for 700 ms to open menu, presses twice briefly to select "Suspender (Sleep)", and holds PRG for 700 ms to confirm.
- **Physical Dynamics**: Because of thick gloves and cold switch grease, the button is held for 1,800 ms after selection, and contact release exhibits 40 ms of intermittent chattering.
- **Firmware Execution Path**:
  1. `ejecutarAccionSubmenu()` executes `entrarEnDeepSleepSeguro()`.
  2. SX1262 LoRa radio commanded to `radio.sleep(false)`.
  3. OLED sends `SSD1306_DISPLAYOFF`; Vext pin driven HIGH; ADC_CTRL driven LOW; LED driven LOW.
  4. Release loop polls GPIO 0: during the 1,800 ms hold, `digitalRead(PIN_PRG) == LOW`, resetting `tInicioEstable`.
  5. Operator releases gloves: contacts chatter for 40 ms (pulses of HIGH/LOW); each LOW pulse resets the timer.
  6. Contacts settle into permanent HIGH; timer increments continuously from 0 to 80 ms without reset.
  7. At t = 80 ms, pad holds are enabled via `gpio_hold_en()` and `gpio_deep_sleep_hold_en()`.
  8. Ext0 wakeup is registered on GPIO 0 at level 0.
  9. `esp_deep_sleep_start()` is executed.
- **Pass Criteria**: Node does not reboot immediately; current drops to 24 uA; node remains asleep all winter; wakes cleanly in spring on first intentional button press.

### Scenario 2: Remote Farm SDN Downlink Sleep Command During Operator Interaction
- **Context**: Operator is adjusting scale calibration in Screen 1 (BALANZA) submenu when ChirpStack network sends an emergency `CMD_FORCE_SLEEP` SDN downlink command due to gateway maintenance.
- **Firmware Execution Path**:
  1. Radio receives downlink during LoRaWAN RX window; unpacks SDN header; identifies `CMD_FORCE_SLEEP`.
  2. System terminates active submenu modal cleanly; saves modified calibration to NVS flash.
  3. Invokes `entrarEnDeepSleepSeguro(sleep_segundos)`.
  4. SX1262 put to sleep; OLED turned off; Vext and ADC pins locked with pad hold.
  5. Button release loop verifies PRG is released (HIGH for >= 80 ms).
  6. Configures ext0 for button wake and timer wakeup for scheduled downlink duration.
  7. Enters deep sleep.
- **Pass Criteria**: No NVS corruption; no display freeze; node enters sleep and automatically wakes at timer expiration.

### Scenario 3: Accidental Double-Tap During Deep Sleep Initiation
- **Context**: User selects "Suspender (Sleep)", releases button for 40 ms, and reflexively presses it again (thinking the command did not register).
- **Physical Dynamics**: Button is HIGH for 40 ms, then LOW for 300 ms, then released HIGH permanently.
- **Firmware Execution Path**:
  1. Release loop starts counting; reaches 40 ms.
  2. Reflexive tap pulls GPIO 0 LOW; loop detects LOW and resets `tInicioEstable = millis()`.
  3. Loop waits out the 300 ms second press without sleeping.
  4. When finally released, a new 80 ms stable HIGH window completes.
  5. Sleep initiates only after the second release is fully debounced.
- **Pass Criteria**: System does not lock up; does not wake prematurely; enters persistent deep sleep.

### Scenario 4: Critical LiPo Battery Depletion with OLED Display Active
- **Context**: Battery voltage drops to 3.30V (critical LiPo cutoff). OLED and sensors are active.
- **Firmware Execution Path**:
  1. ADC power management samples battery voltage; detects critical threshold (< 3.30V).
  2. Emergency auto-sleep sequence triggered.
  3. OLED displays warning message (abbreviated <= 21 chars: `"BATERIA BAJA: SLEEP"`), then shuts off via `SSD1306_DISPLAYOFF`.
  4. Vext cut via GPIO 36 HIGH; ADC divider disconnected via GPIO 37 LOW; radio commanded to sleep.
  5. Debounced release verified; pad holds engaged.
  6. Ext0 armed; deep sleep initiated.
- **Pass Criteria**: Total board current drops from 38 mA to < 25 uA, preventing battery over-discharge and cell destruction.

### Scenario 5: Rapid Menu Navigation Under Heavy LoRaWAN Traffic
- **Context**: High-speed operator cycling through screens 0 -> 1 -> 2 -> 3 -> 4 -> 0 while periodic telemetry packet is transmitted and RTC DeviceTimeAns is received.
- **Firmware Execution Path**:
  1. Short button presses (50 ms) cycle `pantallaActual` modulo 5.
  2. Telemetry packet transmits; header updates RTC time and lights sync dot.
  3. Every screen redraw respects the 10 px technical header and frees y=54..63.
  4. All formatted strings remain strictly <= 21 chars (126 px); no text overflows right edge.
  5. Zero emojis emitted on display or serial log.
- **Pass Criteria**: Framebuffer displays cleanly without tearing or text clipping; LoRaWAN time synchronizes without dropping frames.

---

## 5. Verification Method

### 5.1 Static Verification Commands (PowerShell)
To verify compilation, absence of emojis, string bounds, and immutability of `main/`:

```powershell
$cli = "C:\Users\mate1\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$fqbn = "esp32:esp32:heltec_wifi_lora_32_V3"

# 1. Verify Clean Compilation of Both Development Sketches
& $cli compile --fqbn $fqbn --build-path "C:\Users\mate1\AppData\Local\Temp\ard_build_diag" "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_diag"
if ($LASTEXITCODE -ne 0) { throw "Diagnostic sketch compilation failed!" }

& $cli compile --fqbn $fqbn --build-path "C:\Users\mate1\AppData\Local\Temp\ard_build_bat" "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery"
if ($LASTEXITCODE -ne 0) { throw "Battery sketch compilation failed!" }

# 2. Strict Zero Emojis Audit Across Workspace
$emojiMatches = Select-String -Path "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery\*.*", "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_diag\*.*" -Pattern "[\uD83C-\uDBFF\uDC00-\uDFFF]"
if ($emojiMatches.Count -gt 0) { throw "Emoji rule violation detected!" }

# 3. Footer Legend Elimination Audit
$footerMatches = Select-String -Path "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery\*.ino", "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_diag\*.ino" -Pattern "\[Mantener.*\]"
if ($footerMatches.Count -gt 0) { throw "Footer legend still present!" }

# 4. Strict Immutability Verification for main/ Directory
$mainStatus = git status --porcelain "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main"
if ($mainStatus) { throw "Protected directory main/ was modified!" }
```

### 5.2 Dynamic Hardware Verification Procedure
1. **Sleep Debounce Verification**:
   - Connect Heltec V3 to digital storage oscilloscope or logic analyzer: Channel 1 on GPIO 0 (PRG), Channel 2 on GPIO 36 (Vext), Channel 3 on 3.3V rail current shunt.
   - Long-press PRG to enter sleep; maintain finger hold for 3 seconds.
   - Verify on oscilloscope: Channel 2 does not assert pad hold and Channel 3 current does not drop to < 25 uA until Channel 1 has remained continuously HIGH for >= 80 ms after finger release.
   - Measure sleep current: Must read <= 25 uA with ext0 wakeup enabled and SX1262 asleep.
2. **OLED Redesign Verification**:
   - Inspect physical OLED SSD1306 during operation across all 5 screens.
   - Confirm header shows 4-char title, `HH:MM:SS` RTC time, sync indicator, and 5 page dots.
   - Confirm footer area (y=54..63) contains zero instructional legends.
   - Confirm all text lines fit comfortably within the 128-pixel horizontal screen width without letter truncation.
   - Open 5-item ADC sampling submenu: verify smooth navigation from item 0 to 4 and clean modulo wrap to 0.

### 5.3 Invalidation Conditions
- If `esp_deep_sleep_start()` is executed before `digitalRead(PIN_PRG) == HIGH` has stabilized for >= 80 ms.
- If `radio.sleep()` is omitted, leaving SX1262 in Standby mode (~1.5 mA leakage).
- If GPIO 36 and GPIO 37 pad holds (`gpio_hold_en` + `gpio_deep_sleep_hold_en`) are omitted.
- If `[Mantener PRG: Menu]` or any static footer legend is rendered on screens 0 to 4.
- If any string in font size 1 exceeds 21 characters across the 128-pixel width.
- If the submenu modal cursor freezes or modulo fails when presenting 5 items.
- If any file in `main/` is modified.
- If any emoji is introduced anywhere in code, UI, or documentation.
