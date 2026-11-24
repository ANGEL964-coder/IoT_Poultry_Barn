# Handoff Report: UI Diagnostics and OLED Menu System Exploration

## 1. Observation

### 1.1 OLED SSD1306 Display Hardware Setup and Bus Configuration
Direct inspection of `heltec_v3_local_diag\heltec_v3_local_diag.ino` and `heltec_v3_local_battery\heltec_v3_local_battery.ino` reveals:

1. Display Hardware Parameters:
   - Controller: Solomon Systech SSD1306 Monochrome OLED
   - Resolution: 128 x 64 pixels (`SCREEN_WIDTH 128`, `SCREEN_HEIGHT 64`)
   - Interface: I2C (Wire library)
   - I2C Address: `0x3C` (`OLED_ADDR 0x3C`)
   - Pin Mapping:
     - I2C SDA: GPIO 17 (`OLED_SDA 17` in diag line 38; `PIN_OLED_SDA 17` in battery config line 22)
     - I2C SCL: GPIO 18 (`OLED_SCL 18` in diag line 39; `PIN_OLED_SCL 18` in battery config line 23)
     - Hardware Reset: GPIO 21 (`OLED_RST 21` in diag line 40; `PIN_OLED_RST 21` in battery config line 24)
     - Peripheral Power Gate (Vext): GPIO 36 (`PIN_VEXT 36` in diag line 32; `PIN_VEXT_CTL 36` in battery config line 20)
       - Logic level: Active LOW (LOW powers VDD for OLED and SX1262; HIGH disconnects power).
     - User Button (PRG): GPIO 0 (`PIN_PRG 0`, active LOW with internal pull-up `INPUT_PULLUP`).
     - Battery Measurement ADC: GPIO 1 (`PIN_VBAT_ADC 1`, ADC1_CH0, connected to 4.9x divider).
     - Battery Measurement Gate: GPIO 37 (`PIN_ADC_CTRL 37`, active HIGH N-MOSFET).
     - Load Cell Conditioning Input: GPIO 2 (`PIN_WEIGHT_ADC 2`, ADC1_CH1, 0-3.3V analog input).

2. Display Initialization Sequence:
   - `heltec_v3_local_diag.ino` lines 711-721:
     ```cpp
     pinMode(PIN_VEXT, OUTPUT);
     digitalWrite(PIN_VEXT, LOW); // LOW = Encender Vext
     delay(60);

     Wire.begin(OLED_SDA, OLED_SCL);
     if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
         while (true);
     }
     ```
   - `heltec_v3_local_battery.ino` lines 733-741:
     ```cpp
     pinMode(PIN_VEXT_CTL, OUTPUT);
     digitalWrite(PIN_VEXT_CTL, LOW); // LOW = Alimentar SX1262 y OLED
     delay(80);

     Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
     if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
         while (true);
     }
     ```

### 1.2 Menu Architecture and Navigation State Machine
Both sketches implement a 2-tier menu system:
1. Top-Level Screens: 5 fixed screens defined by `enum Pantalla` (`PANTALLA_HOME` = 0, `PANTALLA_BALANZA` = 1, `PANTALLA_BATERIA` = 2, `PANTALLA_COMMS` = 3, `PANTALLA_SDN` = 4, `TOTAL_PANTALLAS` = 5).
2. Contextual Submenus: Triggered from within each top-level screen by holding PRG for >= 700 ms (`TIEMPO_PULSACION_LARGA = 700`).
   - When inside a submenu, `submenuActivo = true`.
   - Inactivity timeout: 10,000 ms (`TIMEOUT_SUBMENU_MS = 10000`). If no button activity occurs for 10 seconds, `submenuActivo` reverts to false.
3. Single Button (`PIN_PRG` on GPIO 0) Dispatch:
   - Short Press (duration >= 40 ms and < 700 ms upon release):
     - If `!submenuActivo`: `pantallaActual = (Pantalla)((pantallaActual + 1) % TOTAL_PANTALLAS);`
     - If `submenuActivo`: `submenuCursor = (submenuCursor + 1) % 4;` (Hardcoded modulo 4 in `heltec_v3_local_diag.ino:686` and `heltec_v3_local_battery.ino:709`).
   - Long Press (holding LOW for >= 700 ms):
     - If `!submenuActivo`: opens submenu (`submenuActivo = true; submenuCursor = 0;`).
     - If `submenuActivo`: invokes `ejecutarAccionSubmenu()`.

### 1.3 Identification of Clumsy ("Bruto") Design and Overflow Flaws
Direct inspection identified the following UI issues:

1. Wasteful and Repetitive Footer Legends:
   On every single screen, a dividing horizontal line at y=54 or y=55 is drawn, followed by instructional text taking up the bottom 10 vertical pixels:
   - Screen 0 (HOME):
     - `heltec_v3_local_diag.ino:386-388` / `heltec_v3_local_battery.ino:454-456`:
       ```cpp
       display.drawLine(0, 55, 127, 55, SSD1306_WHITE);
       display.setCursor(0, 57);
       display.print("[Mantener PRG: Menu]");
       ```
   - Screen 1 (BALANZA):
     - `heltec_v3_local_diag.ino:412-414` / `heltec_v3_local_battery.ino:476-478`:
       ```cpp
       display.drawLine(0, 54, 127, 54, SSD1306_WHITE);
       display.setCursor(0, 56);
       display.print("[Mantener: Calibrar]");
       ```
   - Screen 2 (BATERIA):
     - `heltec_v3_local_diag.ino:441-443` / `heltec_v3_local_battery.ino:502-504`:
       ```cpp
       display.drawLine(0, 54, 127, 54, SSD1306_WHITE);
       display.setCursor(0, 56);
       display.printf("OLED Sleep: %s", TIMEOUTS_PANTALLA_TXT[timeoutPantallaIdx]);
       ```
   - Screen 3 (COMMS):
     - `heltec_v3_local_diag.ino:465-467` / `heltec_v3_local_battery.ino:524-526`:
       ```cpp
       display.drawLine(0, 54, 127, 54, SSD1306_WHITE);
       display.setCursor(0, 56);
       display.print("[Mantener: Config TX]");
       ```
   - Screen 4 (SDN):
     - `heltec_v3_local_diag.ino:487-489`: `display.print("[Mantener: Test SDN]");`
     - `heltec_v3_local_battery.ino:545-547`: `display.print("[Mantener: Accion SDN]");`

   Impact: These footers consume 10 out of 64 vertical pixels (15.6% of entire screen area), forcing upper content into cramped lines and causing severe text clipping.

2. Critical Text Overflow Bugs on 128-Pixel Display (Font size 1 = 6x8 pixels per glyph; max width = 21 characters):
   - Text Overflow Bug 1 - Screen 0 (HOME):
     `display.printf("Bruto:%.2fkg V:%.3fV", pesoBrutoKg, voltajeSensorV);`
     (`heltec_v3_local_diag.ino:383` / `heltec_v3_local_battery.ino:452`)
     When `pesoBrutoKg = 18.45` and `voltajeSensorV = 0.922`: string is `"Bruto:18.45kg V:0.922V"` (22 characters = 132 pixels). The rightmost 4 pixels (the letter 'V') are clipped off the right screen border.
   - Text Overflow Bug 2 - Screen 2 (BATERIA):
     `display.printf("OLED Sleep: %s", TIMEOUTS_PANTALLA_TXT[timeoutPantallaIdx]);`
     (`heltec_v3_local_diag.ino:443` / `heltec_v3_local_battery.ino:504`)
     When `timeoutPantallaIdx == 2` ("10 Minutos"): string is `"OLED Sleep: 10 Minutos"` (22 characters = 132 pixels -> clipped).
     When `timeoutPantallaIdx == 3` ("Siempre ON"): string is `"OLED Sleep: Siempre ON"` (22 characters = 132 pixels -> clipped).
   - Text Overflow Bug 3 - Screen 3 (COMMS):
     `display.printf("ToA: %.1fms SF%u @ %ddBm", tiempoEnElAireMs, current_sf, current_tx_power);`
     (`heltec_v3_local_diag.ino:460` / `heltec_v3_local_battery.ino:520`)
     Example output: `"ToA: 82.4ms SF7 @ 14dBm"` (23 characters = 138 pixels). 10 pixels clipped off screen edge.
   - Text Overflow Bug 4 - Screen 4 (SDN):
     `display.printf("CMD : 0x%02X (%s)", sdn_last_cmd, getSdnCommandName(sdn_last_cmd));`
     (`heltec_v3_local_diag.ino:479` / `heltec_v3_local_battery.ino:537`)
     `getSdnCommandName(CMD_SET_TEMP_THRESHOLD)` returns `"SET_TEMP_THRESHOLD"` (18 characters). Total string: `"CMD : 0x02 (SET_TEMP_THRESHOLD)"` (31 characters = 186 pixels). Overflows by 58 pixels.
     `getSdnCommandName(CMD_RECONFIG_RADIO)` returns `"RECONFIG_RADIO"`. Total string: `"CMD : 0x01 (RECONFIG_RADIO)"` (27 characters = 162 pixels). Overflows by 34 pixels.
   - Text Overflow Bug 5 - Boot Splash Screen:
     `heltec_v3_local_diag.ino:727-728`:
     `display.setCursor(10, 35); display.println("Modo Diagnostico Loc");`
     String length is 20 characters (120 px). Starting at x=10 yields right edge at x=130 px (2 pixels clipped).
     `heltec_v3_local_battery.ino:749-750`:
     `display.setCursor(12, 46); display.println("Iniciando LoRaWAN...");`
     String length is 20 characters (120 px). Starting at x=12 yields right edge at x=132 px (4 pixels clipped).

3. Submenu Modal Architecture Limitation:
   In `renderSubmenu(const char* titulo, const char* opciones[], int totalOpciones, int seleccionado)`:
   - Header box is 11 px (`display.fillRect(3, 3, 122, 11, SSD1306_WHITE)`).
   - Loop bounds are hardcoded: `for (int i = 0; i < totalOpciones && i < 4; i++)` (`heltec_v3_local_diag.ino:509` and `heltec_v3_local_battery.ino:564`).
   - Modulo navigation is hardcoded: `submenuCursor = (submenuCursor + 1) % 4;` in `gestionarBotonPRG` (`heltec_v3_local_diag.ino:686` and `heltec_v3_local_battery.ino:709`).
   - Consequence: It is impossible to render or navigate a list of 5 items (such as the 5 required ADC sampling intervals) without refactoring the modal geometry and cursor modulo.

### 1.4 Scan for Emojis in Workspace Code and Documentation
1. Inspection of current sketches:
   - `heltec_v3_local_diag\heltec_v3_local_diag.ino`: ZERO Unicode emojis detected.
     - Note: Non-ASCII symbol at line 265: `sdn_last_param = 3250; // 32.50 °C` contains the degree symbol `°` (0xB0), which renders as corrupt garbage on SSD1306 standard CP437 ASCII font.
   - `heltec_v3_local_battery\heltec_v3_local_battery.ino`: ZERO Unicode emojis detected.
   - `heltec_v3_local_battery\heltec_v3_config.h`: ZERO Unicode emojis detected (one accented word at line 41: `señal`).
   - `sdn_header.h` (in both directories): ZERO Unicode emojis detected (Spanish accent characters in comments: `estándar`, `específicos`, `Avícola`, `calibración`, `transmisión`).
2. Inspection of legacy version 1.0 (`main\main.ino`):
   Emojis were present in Serial log statements in v1.0:
   - Line 429: [EMOJI_INBOX_TRAY] (U+1F4E5)
   - Line 442: [EMOJI_CHECK_MARK] (U+2714)
   - Line 448: [EMOJI_CROSS_MARK] (U+274C)
   - Line 506: [EMOJI_INBOX_TRAY] (U+1F4E5)
   - Line 537: [EMOJI_HIGH_VOLTAGE] (U+26A1)
   - Line 545: [EMOJI_CHECK_MARK] (U+2714)
   - Line 547: [EMOJI_CROSS_MARK] (U+274C)
   These must remain prohibited in all new code, display routines, and documentation.

### 1.5 Current ADC Sampling Implementation and Energy Waste
In both `heltec_v3_local_diag.ino:756-761` and `heltec_v3_local_battery.ino:797-802`:
```cpp
// 3. Muestreo de sensores cada 300 ms
static uint32_t ultimoMuestreo = 0;
if (millis() - ultimoMuestreo >= 300) {
    ultimoMuestreo = millis();
    actualizarBalanza();
    actualizarBateria();
}
```
In `actualizarBateria()` (`heltec_v3_local_diag.ino:126-145` and `heltec_v3_local_battery.ino:136-154`):
- Line 128 / 138: `digitalWrite(PIN_ADC_CTRL, HIGH);`
- Line 129 / 139: `delay(4);` (blocks CPU for 4 ms)
- Line 133-136 / 142-146: 16 iterations of `analogReadMilliVolts(PIN_VBAT_ADC)` with `delayMicroseconds(50)`
- Line 137 / 147: `digitalWrite(PIN_ADC_CTRL, LOW);`
Running this loop every 300 ms continuously turns on the battery divider MOSFET, causing periodic current spikes (~800 uA through divider) and burning CPU active time 3.33 times per second, even when the device is idle.

---

## 2. Logic Chain

1. Hardware Bus Constraints:
   From Observation 1.1, the OLED SSD1306 is powered through GPIO 36 (Vext). When Vext is cut (HIGH), I2C communication to address 0x3C fails. Therefore, any screen wake-up or refresh requires Vext to be asserted LOW before executing `Wire` transactions or `display.begin()`.
2. UI Usability and Aesthetic Optimization:
   From Observation 1.3.1, 10 pixels on every screen are currently squandered on static legends (`[Mantener PRG: Menu]`, etc.). Removing these footers frees 10 vertical pixels, providing 54 vertical pixels of usable data space instead of 44.
3. Resolution of Text Overflow Bugs:
   From Observation 1.3.2, multiple strings exceed 21 characters (128 pixels). Re-allocating the vertical space allows splitting crowded two-column lines (such as `Bruto:%.2fkg V:%.3fV`) into clean, dedicated rows or abbreviations (e.g. `B: 18.45kg  V: 0.92V` = 19 chars = 114 px).
4. Enabling ADC Interval Configuration in UI:
   From Observation 1.3.3 and R4, 5 sampling intervals are needed:
   `{ "Solo en TX", "Cada 10s", "Cada 30s", "Cada 60s", "Cada 300s" }`.
   Because the modal currently caps `i < 4` and modulo `% 4`, the modal rendering function must be upgraded to dynamically handle `totalOpciones` (using a 9-pixel vertical step: `y = 16 + (i * 9)`, where 5 items fit in 45 vertical pixels within the 60px dialog box).
5. State Persistence Architecture:
   Preferences namespace `"node_cfg"` (battery sketch) and `"scale_cfg"` (diag sketch) currently store `tout_idx` and `tx_idx`. An entry `"adc_idx"` (type `uint8_t`, values 0..4) fits directly into NVS Flash, defaulting to 1 (10s) or 0 (Solo en TX).
6. Power Reduction via ADC Throttling:
   From Observation 1.5, decoupling ADC execution from the 300ms timer to user-selected intervals (`0s` [On Transmit], `10s`, `30s`, `60s`, `300s`) directly eliminates unnecessary MOSFET switching and CPU active cycles.
7. Real-Time Clock Integration:
   The ESP32-S3 internal RTC operates via POSIX `<time.h>` / `sys/time.h` functions (`settimeofday` and `localtime_r`). In RadioLib LoRaWAN (`LoRaWAN.h:760, 780`), calling `node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME)` followed by `node.getMacDeviceTimeAns(&timestamp, &fraction, true)` delivers network epoch time without extra hardware. A compact 8-character string `HH:MM:SS` occupies 48 pixels, fitting comfortably in the header bar.

---

## 3. Caveats

1. Hardware RTC Crystal: The Heltec WiFi LoRa 32 V3 relies on the internal 150 kHz RC oscillator or internal RTC slow clock of the ESP32-S3 (unless an external 32.768 kHz crystal is populated). Internal RC drift is typically 1-2% without periodic network synchronization; for field applications, periodic LoRaWAN `DeviceTimeReq` (e.g. every 24 hours or upon join) maintains accurate time.
2. ADC Sampling in "Solo en TX" Mode while Viewing Balanza: If the user selects "Solo en TX" (index 0), sensor readings are updated only when an uplink packet is transmitted. If the user navigates to Screen 1 (BALANZA), the display will show the last sampled weight until a transmission occurs, unless the UI specifically triggers a single on-demand read upon entering Screen 1.
3. Memory Footprint: Adafruit_SSD1306 maintains a 1024-byte framebuffer in SRAM (`128 * 64 / 8`). Dynamic allocations must be avoided to prevent heap fragmentation.
4. Read-Only Investigation Boundary: No source code modifications were committed during this analysis. All implementation proposals are provided as structured specifications.

---

## 4. Conclusion

### 4.1 Proposed Refined OLED UI Layout (128x64 Architecture)

#### A. Unified Compact Header Bar (Height: 10 px, y = 0 to 9)
Eliminate the text label redundancy and integrate the RTC clock:
- Layout (x = 0 to 127, y = 0 to 8; horizontal rule at y = 9):
  - Left (x = 0 to 30): Active screen name (4 chars: `HOME`, `BALA`, `BATT`, `COMM`, `SDN`)
  - Center (x = 42 to 89): RTC time in format `HH:MM:SS` (font size 1, 48 px width)
  - Sync Indicator (x = 91): Single pixel dot or symbol `*` if synced with LoRaWAN, or `-` if unsynced
  - Right (x = 98 to 126): 5 circular progress dots (radius 2 px, center coordinates cx = 98, 104, 110, 116, 122; cy = 4)
  - Separator: `display.drawLine(0, 9, 127, 9, SSD1306_WHITE);`

#### B. Redesigned Screen Bodies (Freeing y = 10 to 63 without Clumsy Footers)

1. Screen 0: HOME (Clean Dashboard)
   - y = 12: Status line: Left `[ANT]` + RSSI/link status (`OK` / `JOIN`); Right: Mini battery icon + percentage (`85%`)
   - y = 24 to 39: Primary weight display in large font (Size 2): `24.50 kg`
   - y = 43: Secondary data line: `B: 24.50kg  V: 1.225V` (19 chars = 114 px -> fits comfortably)
   - y = 53: Energy state summary: `TX: 60s | ADC: TX-only` (or `ADC: 10s`)

2. Screen 1: BALANZA (Scale Diagnostics)
   - Left side (x = 2 to 26, y = 14 to 36): Clean industrial scale vector glyph
   - Right side (x = 32, y = 12, 22, 32, 42):
     - `Neto :  24.50 kg`
     - `Bruto:  24.50 kg`
     - `V_adc: 1.2250 V`
     - `Pend : 0.0500 V/k`
   - Bottom (y = 53): `Tara: 0.00kg | ADC:OK`

3. Screen 2: BATERIA (Power and Battery Management)
   - Left side: Battery gauge icon (36x16 px) at (x = 2, y = 13)
   - Right side: Big percentage (Size 2) at (x = 44, y = 13): `85%`
   - y = 33: `V: 3.98V (LiPo 400mAh)` (21 chars = 126 px -> fits)
   - y = 43: `Autonomia: ~18h 30m`
   - y = 53: `Sleep: 5m | ADC: 10s` (Displays current timeout and ADC interval)

4. Screen 3: COMMS (LoRaWAN Radio Diagnostics)
   - y = 12: `RSSI: -85dBm  SNR: 8.5` (21 chars = 126 px -> fits)
   - y = 22: `Uplink #142  | DR3/SF7`
   - y = 32: `TX Pwr: 14dBm | ToA:82ms`
   - y = 42: `Periodo TX: 60 Segundos`
   - y = 53: `Modo: SDN / US915 SubB2`

5. Screen 4: SDN (Software-Defined Network Console)
   - y = 12: `RX SDN #3 (Gateway OK)`
   - y = 22: `CMD: 0x01 (RADIO_CFG)` (Clean, truncated command label)
   - y = 32: `CFG: SF7@14dBm Rate:60s`
   - y = 42: `RAW: 0x01A4B001`
   - y = 53: `FPort: 10 (204 Bytes)`

### 4.2 Submenu Modal Overhaul
Update `renderSubmenu()` to support 5 items without overflow:
- Dialog bounding box: `(2, 2, 124, 60)`
- Title bar inverted header: `(3, 3, 122, 10)`
- Item vertical spacing: 9 pixels per item (`y = 15 + (i * 9)`)
  - Item 0: y = 15
  - Item 1: y = 24
  - Item 2: y = 33
  - Item 3: y = 42
  - Item 4: y = 51
- Total height consumed: `51 + 8 = 59 px <= 60 px` (fits within display height 64).
- Fix navigation cursor modulo:
  Replace hardcoded `(submenuCursor + 1) % 4` with `(submenuCursor + 1) % totalSubmenuItems`.

### 4.3 ADC Sampling Interval Configuration Design

1. Options Table:
   | Index | Display String | Internal Interval (s) | Power Profile Description |
   |:-----:|:--------------:|:---------------------:|:--------------------------|
   | 0     | Solo en TX     | 0                     | Maximum energy saving; ADC sampled only upon packet transmission. |
   | 1     | Cada 10s       | 10                    | High responsive diagnostics. |
   | 2     | Cada 30s       | 30                    | Balanced field mode. |
   | 3     | Cada 60s       | 60                    | Low power field mode. |
   | 4     | Cada 300s (5m) | 300                   | Long term autonomous battery deployment. |

2. NVS Persistence:
   - Namespace: `"node_cfg"` (battery sketch) / `"scale_cfg"` (diag sketch)
   - Key: `"adc_idx"` (`uint8_t`)
   - Flash write: Executed in `guardarConfiguracionNVS()` only upon user confirmation.

3. Integration Point in Menu System:
   - Screen 2 (BATERIA) submenu is upgraded to:
     Option 0: "Timeout Pantalla" -> cycles OLED timeout (1m, 5m, 10m, ON)
     Option 1: "Muestreo ADC" -> opens 5-option modal or cycles 5 intervals
     Option 2: "Suspender (Sleep)" -> immediate deep sleep with safe button release
     Option 3: "Volver"
   - Alternatively, a dedicated submenu "INTERVALO ADC" with the 5 options can be invoked directly from Screen 1 (BALANZA) or Screen 2 (BATERIA).

4. Non-Blocking Loop Implementation:
   ```cpp
   // ADC Sampling Interval Control
   uint32_t intervaloAdcMs = ADC_INTERVALOS_S[adcIntervalIdx] * 1000UL;
   if (intervaloAdcMs > 0) {
       static uint32_t ultimoMuestreoAdc = 0;
       if (millis() - ultimoMuestreoAdc >= intervaloAdcMs) {
           ultimoMuestreoAdc = millis();
           actualizarBalanza();
           actualizarBateria();
       }
   }
   ```
   When `intervaloAdcMs == 0` ("Solo en TX"), `actualizarBalanza()` and `actualizarBateria()` are invoked exclusively inside `enviarTelemetriaLoRaWAN()` immediately before payload packaging.

### 4.4 RTC Clock Implementation Specification

1. Internal RTC Configuration:
   - Standard headers: `#include <time.h>` and `#include <sys/time.h>`.
   - ESP32-S3 internal RTC retains time during active operation and light sleep.
2. Network Synchronization (RadioLib LoRaWAN):
   - In `heltec_v3_local_battery.ino`, request network time:
     ```cpp
     node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME);
     ```
   - In `enviarTelemetriaLoRaWAN()`, after `node.sendReceive()` returns:
     ```cpp
     uint32_t netEpoch = 0;
     uint16_t fracMs = 0;
     if (node.getMacDeviceTimeAns(&netEpoch, &fracMs, true) == RADIOLIB_ERR_NONE) {
         struct timeval tv = { .tv_sec = (time_t)netEpoch, .tv_usec = (suseconds_t)(fracMs * 1000) };
         settimeofday(&tv, NULL);
         rtc_sincronizado = true;
     }
     ```
3. Formatted Display Helper:
   ```cpp
   void obtenerHoraRTC(char* buf, size_t len) {
       time_t now;
       struct tm ti;
       time(&now);
       localtime_r(&now, &ti);
       snprintf(buf, len, "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
   }
   ```

---

## 5. Verification Method

### 5.1 Verification Commands and Environment
To independently verify the syntax and compilation of both sketches for the Heltec WiFi LoRa 32 V3 hardware target:

```powershell
$cli = "C:\Users\mate1\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$fqbn = "esp32:esp32:heltec_wifi_lora_32_V3"

# Verify Compilation of Diagnostic Sketch
& $cli compile --fqbn $fqbn "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_diag\heltec_v3_local_diag.ino"

# Verify Compilation of Battery LoRaWAN Sketch
& $cli compile --fqbn $fqbn "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery\heltec_v3_local_battery.ino"
```

### 5.2 Files to Inspect
1. `c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_diag\heltec_v3_local_diag.ino`
   - Lines 360-490: verify removal of footer legends and string width truncation fixes.
   - Lines 495-550: verify submenu modal geometry and variable option bounds.
   - Lines 635-700: verify PRG button state machine.
2. `c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery\heltec_v3_local_battery.ino`
   - Lines 430-550: verify OLED screen rendering and elimination of footer text.
   - Lines 318-345: verify DeviceTimeReq sync logic.
3. Target `main\` directory:
   - Ensure `c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\` remains strictly bit-for-bit identical to baseline v1.0.

### 5.3 Invalidation Conditions
- Any occurrence of Unicode emojis in code, comments, OLED UI, or markdown documents invalidates compliance.
- Any text string in font size 1 that exceeds 21 characters across the 128-pixel width invalidates UI compliance.
- Submenu cursor freezing or modulo errors when presenting 5 options invalidates menu compliance.
