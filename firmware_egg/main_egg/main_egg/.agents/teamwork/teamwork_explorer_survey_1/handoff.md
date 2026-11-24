# Investigation Handoff Report: Heltec V3 Battery Firmware

Target Sketch: heltec_v3_local_battery/heltec_v3_local_battery.ino
Explorer Role: Firmware Explorer Battery
State Directory: c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_survey_1\

## 1. Observation

### 1.1 Deep Sleep Implementation and PRG Button Handling
In `heltec_v3_local_battery/heltec_v3_local_battery.ino`:
- Pin definition (line 28 in `heltec_v3_config.h`):
  `#define PIN_PRG 0 // Boton PRG integrado (LOW al presionar)`
- Long press threshold definition (line 77):
  `const uint32_t TIEMPO_PULSACION_LARGA = 700; // ms`
- Sleep entry from SDN downlink command `CMD_FORCE_SLEEP` (lines 260-271):
  ```cpp
  case CMD_FORCE_SLEEP: {
      uint16_t mins = h->param_value;
      if (mins > 0) {
          display.ssd1306_command(SSD1306_DISPLAYOFF);
          digitalWrite(PIN_VEXT_CTL, HIGH);
          uint64_t sleep_us = (uint64_t)mins * 60ULL * 1000000ULL;
          esp_sleep_enable_timer_wakeup(sleep_us);
          esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0);
          esp_deep_sleep_start();
      }
      break;
  }
  ```
- Sleep entry from OLED Menu action (lines 617-622):
  ```cpp
  } else if (submenuCursor == 2) {
      display.ssd1306_command(SSD1306_DISPLAYOFF);
      digitalWrite(PIN_VEXT_CTL, HIGH);
      esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0);
      esp_deep_sleep_start();
  }
  ```
- Button state machine in `gestionarBotonPRG()` (lines 683-700):
  ```cpp
  if (estadoActual == LOW && !accionLargaEjecutada) {
      if (millis() - tiempoInicioPresion >= TIEMPO_PULSACION_LARGA) {
          ultimoTiempoInteraccion = millis();
          accionLargaEjecutada = true;

          digitalWrite(PIN_LED_USER, HIGH);
          delay(50);
          digitalWrite(PIN_LED_USER, LOW);

          if (!submenuActivo) {
              submenuActivo = true;
              submenuCursor = 0;
              submenuUltimaActividad = millis();
          } else {
              ejecutarAccionSubmenu();
          }
      }
  }
  ```
- Menu navigation path:
  1. From HOME screen (`pantallaActual == PANTALLA_HOME`), user performs a long press (>= 700 ms) to open `MENU ENERGIA / TX` (line 586).
  2. Submenu items:
     - Index 0: "Transmitir Ahora"
     - Index 1: "Apagar Pantalla"
     - Index 2: "Suspender (Sleep)"
     - Index 3: "Volver"
  3. User presses PRG briefly twice to select Index 2 (`submenuCursor == 2`).
  4. User performs a long press (>= 700 ms) to execute `ejecutarAccionSubmenu()`.
  5. At exactly 700 ms from depression, `ejecutarAccionSubmenu()` is executed while the user is still physically depressing the PRG button (GPIO 0 is LOW).
  6. `esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0)` is invoked with level parameter `0` (active LOW).
  7. `esp_deep_sleep_start()` is called immediately on the subsequent line without checking if GPIO 0 has been released or waiting for contact bounce stabilization.

### 1.2 Peripheral Power Management State During Sleep
In `heltec_v3_local_battery/heltec_v3_local_battery.ino`:
- LoRa SX1262 Transceiver:
  - `radio.sleep()` is never called anywhere in the sketch prior to `esp_deep_sleep_start()`.
  - In `ejecutarAccionSubmenu()` (lines 617-622) and in `CMD_FORCE_SLEEP` (lines 260-271), only `display.ssd1306_command(SSD1306_DISPLAYOFF)` and `digitalWrite(PIN_VEXT_CTL, HIGH)` are invoked.
  - The SX1262 transceiver remains in Standby mode during sleep.
- Vext Rail (GPIO 36):
  - In `heltec_v3_config.h` (line 20): `#define PIN_VEXT_CTL 36 // Power control: LOW = ON, HIGH = OFF`
  - In `heltec_v3_local_battery.ino`: `digitalWrite(PIN_VEXT_CTL, HIGH)` turns off the P-MOSFET that supplies Vext to the OLED display.
  - However, `gpio_hold_en((gpio_num_t)PIN_VEXT_CTL)` and `gpio_deep_sleep_hold_en()` are never called. On ESP32-S3, GPIO 36 is not an RTC GPIO (RTC GPIOs are GPIO 0-21); digital IO pads lose their driven state in deep sleep unless hold is enabled.
- Battery Measurement MOSFET (GPIO 37 / PIN_ADC_CTRL) and Divider (GPIO 1 / PIN_VBAT_ADC):
  - In `leerVoltajeBateriaHardware()` (lines 136-154), GPIO 37 is driven HIGH for 4 ms plus 16 ADC conversions, then driven LOW.
  - In `loop()` (lines 797-802), `actualizarBateria()` is called every 300 ms unconditionally:
    ```cpp
    static uint32_t ultimoMuestreo = 0;
    if (millis() - ultimoMuestreo >= 300) {
        ultimoMuestreo = millis();
        actualizarBalanza();
        actualizarBateria();
    }
    ```
  - This switches the MOSFET on GPIO 37 on and off every 300 ms, draining power needlessly during idle.
- Weight Sensor Conditioning (GPIO 2 / PIN_WEIGHT_ADC):
  - `actualizarBalanza()` (lines 184-205) is also invoked every 300 ms unconditionally in `loop()`.
  - There is currently no configurable ADC sampling interval or automatic power saving mode that links ADC readings to LoRaWAN transmission intervals.

### 1.3 System Time and ESP32-S3 Internal RTC
In `heltec_v3_local_battery/heltec_v3_local_battery.ino`:
- Standard time headers (`<time.h>`, `<sys/time.h>`) are not included.
- Functions `settimeofday()`, `gettimeofday()`, `time()`, and `localtime_r()` are not utilized.
- Telemetry timestamps rely solely on `millis()` (line 294: `telem->uptime_ms = millis();`).
- Deep sleep resets `millis()` to 0 on each reboot.
- RadioLib 7.x LoRaWAN stack inspection:
  - RadioLib provides native support for MAC command `DeviceTimeReq` (`RADIOLIB_LORAWAN_MAC_DEVICE_TIME = 0x0D`):
    - `LoRaWANNode::sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME)` queues the request in `fOpts`.
    - `LoRaWANNode::getMacDeviceTimeAns(uint32_t* timestamp, uint16_t* fraction, bool returnUnix = true)` parses `DeviceTimeAns` from the downlink frame, computes Unix epoch seconds and millisecond fraction, and accounts for transmission latency offset (`ms += mod->hal->millis() - this->tUplinkEnd`).
  - Currently, `heltec_v3_local_battery.ino` does not queue `sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME)` and does not process `getMacDeviceTimeAns`.

### 1.4 OLED User Interface Design and Text Legend
In `heltec_v3_local_battery/heltec_v3_local_battery.ino`:
- `renderPantallaHome()` (lines 434-457):
  ```cpp
  display.drawLine(0, 55, 127, 55, SSD1306_WHITE);
  display.setCursor(0, 57);
  display.print("[Mantener PRG: Menu]");
  ```
  This hardcoded string occupies the bottom 9 pixels (lines 55 to 63) of the 128x64 display.
- Submenu options in `dibujarSubmenuActivo()` (lines 581-604) lack any entry for configuring ADC sampling intervals.

### 1.5 Emoji Audit
- File `heltec_v3_local_battery/heltec_v3_local_battery.ino`:
  - Byte scan for non-ASCII characters: 0 occurrences found (100% 7-bit ASCII clean).
  - Regex scan for Unicode symbols/emojis (`[\uD83C-\uDBFF\uDC00-\uDFFF]|\p{So}|\p{Cs}`): 0 matches.
- File `heltec_v3_config.h`: 0 non-ASCII, 0 emojis.
- File `sdn_header.h`: 0 non-ASCII, 0 emojis.
- File `heltec_v3_local_diag/heltec_v3_local_diag.ino`: Found 1 degree symbol at line 249 (`// 32.50 deg C`).

### 1.6 Compilation Toolchain Verification
Execution of tool command:
`& "C:\Users\mate1\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:heltec_wifi_lora_32_V3 "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery"`
Result:
- Return code: 0
- Program storage usage: 454,671 bytes (13% of 3,342,336 bytes maximum)
- Dynamic memory usage: 25,632 bytes (7% of 327,680 bytes maximum)

---

## 2. Logic Chain

### 2.1 Cause of Immediate Wakeup on Deep Sleep
1. Observation 1.1 shows that the user selects option 2 ("Suspender (Sleep)") via a long press of PRG (GPIO 0) exceeding 700 ms (`TIEMPO_PULSACION_LARGA`).
2. Observation 1.1 shows that at 700 ms, `gestionarBotonPRG()` immediately calls `ejecutarAccionSubmenu()`. At this millisecond mark, the user's physical finger is actively pressing the tactile switch down; GPIO 0 is at electrical LOW (0V).
3. Observation 1.1 shows that `ejecutarAccionSubmenu()` immediately executes `esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0)`. The second argument `0` configures the ESP32-S3 RTC controller to trigger a wakeup whenever GPIO 0 is LOW.
4. Observation 1.1 shows that `esp_deep_sleep_start()` is called immediately afterwards without delay, release check, or debounce.
5. Because GPIO 0 is already LOW when deep sleep initiates, the RTC power controller senses the trigger condition (level LOW) as already satisfied.
6. Consequently, the chip exits deep sleep within microseconds, resets, executes ROM bootloader, and runs `setup()` again. If the user continues holding the button during reset, the chip may re-enter download mode or register another long press upon reboot.

### 2.2 Debounce and Release Logic Necessary to Resolve the Wakeup Issue
1. To ensure that `esp_deep_sleep_start()` only wakes on a subsequent intentional press, the firmware must ensure that:
   a. The physical PRG button is released (GPIO 0 reads HIGH).
   b. The contact release has stabilized past any mechanical bounce (typically lasting 10-40 ms on miniature SMD tactile buttons).
2. The logic must poll `digitalRead(PIN_PRG)` until it remains continuously HIGH for a debounce period (e.g., 80 ms to 100 ms). If a LOW reading is detected during this window, the debounce timer resets.
3. Once the 80 ms stable HIGH window passes, `esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0)` can be registered.
4. Only then is `esp_deep_sleep_start()` executed. As a result, deep sleep will persist until the user initiates a new, distinct downward press of the PRG button.

### 2.3 Peripheral Power Optimization Logic
1. Observation 1.2 shows that `radio.sleep()` is not called. In Semtech SX1262, Standby_RC mode consumes ~1.2 mA to 1.6 mA, whereas sleep mode (`radio.sleep(false)`) consumes 0.16 uA (160 nA). Failing to sleep the SX1262 increases deep sleep current by approximately 100x to 200x.
2. Observation 1.2 shows that Vext control pin GPIO 36 is driven HIGH before sleep, but pad hold is not enabled. Calling `gpio_hold_en((gpio_num_t)PIN_VEXT_CTL)` and `gpio_deep_sleep_hold_en()` prevents the gate voltage from floating during deep sleep, ensuring zero power reaches the OLED panel.
3. Observation 1.2 shows that the battery measurement MOSFET (GPIO 37) and weight sensor (GPIO 2) are sampled every 300 ms. In battery-operated deployment, updating ADC channels every 300 ms wastes energy through the resistive divider and CPU processing. Shifting ADC reads to occur exclusively at LoRaWAN transmission time or at user-selected intervals (e.g., 5s, 30s, or TX only) drastically lowers average active consumption.

### 2.4 Internal RTC and LoRaWAN Time Sync Logic
1. Observation 1.3 shows that system time is not maintained in internal RTC.
2. The ESP32-S3 contains an internal RTC timer driven by the slow clock that maintains real time across deep sleep cycles. ESP-IDF standard POSIX time functions (`settimeofday`, `gettimeofday`, `time`, `localtime_r`) automatically integrate with the RTC timer.
3. Observation 1.3 shows that RadioLib 7.x possesses built-in functions `sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME)` and `getMacDeviceTimeAns(&timestamp, &fraction, true)`.
4. Integrating these two functions enables the Heltec V3 node to synchronize its internal RTC to GPS/UTC network time provided by ChirpStack v4 without requiring any external RTC hardware (e.g. DS3231).

---

## 3. Caveats

1. Hardware RTC Crystal: The Heltec WiFi LoRa 32 V3 board relies on the ESP32-S3 internal 136/150 kHz RC slow clock or internal calibrated RC oscillator unless an external 32.768 kHz crystal is populated on GPIO 15/16. Over multi-hour deep sleep periods, internal RC drift can range between 1% and 2%. Periodic LoRaWAN DeviceTimeReq re-synchronization (e.g. once every 12 to 24 hours) mitigates long-term drift.
2. Network Support for DeviceTimeReq: ChirpStack v4 supports LoRaWAN MAC command `DeviceTimeReq` provided the gateway reports GPS time or the network server has system time synchronization. If a gateway lacks GPS/NTP, `DeviceTimeAns` may be delayed or omitted by the server. The firmware must handle unsynchronized states gracefully with a fallback status indicator.
3. Main Directory Untouched: All findings and recommendations are strictly isolated to `heltec_v3_local_battery/` (and matching changes for `heltec_v3_local_diag/`), leaving `main/` version 1.0 completely untouched.

---

## 4. Conclusion

The root causes of the reported issues in `heltec_v3_local_battery.ino` and their solutions are:

1. **Immediate Deep Sleep Wakeup**: Caused by executing `esp_deep_sleep_start()` while GPIO 0 is still held LOW by the user following the 700 ms long press. Resolved by introducing a debounced button-release loop requiring 80 ms of continuous electrical HIGH on GPIO 0 before sleep initiation.
2. **Missing Radio & Peripheral Sleep**: The SX1262 is left in active Standby (~1.5 mA) and GPIO 36 is not locked with RTC pad hold. Resolved by calling `radio.sleep(false)`, `digitalWrite(PIN_VEXT_CTL, HIGH)`, `gpio_hold_en()`, and `gpio_deep_sleep_hold_en()`.
3. **Internal RTC and LoRaWAN Time Sync**: The sketch lacks `<sys/time.h>` and timekeeping. Resolved by queuing RadioLib `sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME)` on join/uplink, extracting the Unix timestamp with `getMacDeviceTimeAns()`, and passing it to `settimeofday()`.
4. **OLED Interface Cleanup**: The label `[Mantener PRG: Menu]` on line 456 must be replaced by technical RTC/ADC indicators. The menu must include an option for configuring the ADC sampling interval (TX-only, 5s, 30s, 300ms) with NVS persistence.
5. **Emoji Verification**: `heltec_v3_local_battery.ino` is completely free of emojis and non-ASCII characters.

---

## 5. Verification Method

### 5.1 Independent Compilation Verification
Run the official Arduino CLI compile command:
```powershell
& "C:\Users\mate1\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:heltec_wifi_lora_32_V3 "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery"
```
Verification passes if exit code is 0 and no syntax errors or undeclared symbols are reported.

### 5.2 Deep Sleep & Wakeup Behavior Verification
1. Flash firmware to Heltec V3 node.
2. Navigate to HOME submenu item "Suspender (Sleep)".
3. Press and hold PRG for > 1.5 seconds.
4. Verify via serial monitor (115200 baud) and multimeter:
   - The device does not reboot while the button is physically held down.
   - Upon releasing the button, current drops to ultralow deep sleep level (< 20 uA).
   - Pressing PRG once initiates a clean reboot with wakeup cause `ESP_SLEEP_WAKEUP_EXT0`.

### 5.3 LoRaWAN Time Synchronization Verification
1. Connect node to a ChirpStack v4 gateway.
2. Observe serial log: upon OTAA join, `sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME)` is transmitted.
3. In ChirpStack device event log, verify downlink frame contains MAC command `0x0D` (`DeviceTimeAns`).
4. On node, verify `getMacDeviceTimeAns` returns `RADIOLIB_ERR_NONE` and `time(NULL)` yields valid current UTC epoch time.

### 5.4 Invalidation Conditions
- If `esp_deep_sleep_start()` is called before `digitalRead(PIN_PRG) == HIGH` has been stable for >= 50 ms.
- If `radio.sleep()` is omitted before `esp_deep_sleep_start()`, causing ~1.5 mA leakage.
- If changes modify any file in `main\`.

---

## 6. Concrete Implementation Blueprints for Implementer

### 6.1 Debounced Sleep Helper
```cpp
void entrarEnDeepSleepSeguro(uint32_t sleep_segundos = 0) {
    // 1. Poner radio SX1262 en modo ultra-bajo consumo
    radio.sleep(false); // cold sleep sin retencion para minima corriente (0.16 uA)

    // 2. Apagar pantalla OLED SSD1306
    display.ssd1306_command(SSD1306_DISPLAYOFF);

    // 3. Desactivar riel de alimentacion Vext (GPIO 36 HIGH = OFF)
    pinMode(PIN_VEXT_CTL, OUTPUT);
    digitalWrite(PIN_VEXT_CTL, HIGH);

    // 4. Desactivar MOSFET de bateria (GPIO 37 LOW = OFF) y LED
    pinMode(PIN_ADC_CTRL, OUTPUT);
    digitalWrite(PIN_ADC_CTRL, LOW);
    pinMode(PIN_LED_USER, OUTPUT);
    digitalWrite(PIN_LED_USER, LOW);

    // 5. Esperar liberacion fisica del boton PRG (GPIO 0) con filtro de rebote de 80 ms
    uint32_t tInicioEstable = millis();
    while (millis() - tInicioEstable < 80) {
        if (digitalRead(PIN_PRG) == LOW) {
            tInicioEstable = millis(); // Reiniciar ventana si el boton sigue presionado o hay rebote
        }
        delay(5);
    }

    // 6. Activar retencion de estado en pines durante deep sleep
    gpio_hold_en((gpio_num_t)PIN_VEXT_CTL);
    gpio_hold_en((gpio_num_t)PIN_ADC_CTRL);
    gpio_deep_sleep_hold_en();

    // 7. Configurar fuentes de despertar
    if (sleep_segundos > 0) {
        esp_sleep_enable_timer_wakeup((uint64_t)sleep_segundos * 1000000ULL);
    }
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0); // Wakeup en nivel LOW

    // 8. Iniciar deep sleep
    esp_deep_sleep_start();
}
```

### 6.2 Internal RTC and LoRaWAN DeviceTimeReq Integration
```cpp
#include <time.h>
#include <sys/time.h>

bool rtc_sincronizado = false;

void solicitarSincronizacionHoraLoRaWAN() {
    if (lora_joined) {
        node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME);
    }
}

void procesarRespuestaHoraLoRaWAN() {
    uint32_t epoch_unix = 0;
    uint16_t fraccion_ms = 0;
    int16_t res = node.getMacDeviceTimeAns(&epoch_unix, &fraccion_ms, true);
    if (res == RADIOLIB_ERR_NONE) {
        struct timeval tv;
        tv.tv_sec = epoch_unix;
        tv.tv_usec = (suseconds_t)fraccion_ms * 1000;
        settimeofday(&tv, NULL);
        rtc_sincronizado = true;
        Serial.printf("[RTC OK] Hora sincronizada con ChirpStack: %lu Unix\n", (unsigned long)epoch_unix);
    }
}

void obtenerHoraFormateada(char* buffer, size_t max_len) {
    if (!rtc_sincronizado) {
        snprintf(buffer, max_len, "NO SYNC");
        return;
    }
    time_t ahora;
    time(&ahora);
    struct tm info;
    localtime_r(&ahora, &info);
    strftime(buffer, max_len, "%H:%M:%S", &info);
}
```

### 6.3 Configurable ADC Sampling Interval
```cpp
// Opciones: 0 = Solo en TX, 1 = Cada 5s, 2 = Cada 30s, 3 = Continuo (300ms)
uint8_t  modoMuestreoAdcIdx = 0;
const uint32_t INTERVALOS_ADC_MS[4] = { 0, 5000, 30000, 300 };
const char*    INTERVALOS_ADC_TXT[4] = { "Solo en TX", "Cada 5s", "Cada 30s", "Continuo 300ms" };
```
In `loop()`:
```cpp
uint32_t intervaloAdc = INTERVALOS_ADC_MS[modoMuestreoAdcIdx];
if (intervaloAdc > 0) {
    static uint32_t ultimoMuestreoAdc = 0;
    if (millis() - ultimoMuestreoAdc >= intervaloAdc) {
        ultimoMuestreoAdc = millis();
        actualizarBalanza();
        actualizarBateria();
    }
}
```
And inside `enviarTelemetriaLoRaWAN()`:
```cpp
if (modoMuestreoAdcIdx == 0) {
    actualizarBalanza();
    actualizarBateria();
}
```
