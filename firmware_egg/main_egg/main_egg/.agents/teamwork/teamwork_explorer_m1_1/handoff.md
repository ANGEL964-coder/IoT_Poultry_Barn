# Handoff Report: Deep Sleep & Power Rails Architecture (Features F1 - F4)

- **Target Sketch**: `heltec_v3_local_battery/heltec_v3_local_battery.ino` & `heltec_v3_config.h`
- **Module**: Milestone 1 (Battery Firmware Optimization)
- **Explorer Role**: Deep Sleep Explorer (Features F1, F2, F3, F4)
- **State Folder**: `c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_m1_1\`

---

## 1. Observation

### 1.1 Direct Baseline Code Inspection: Deep Sleep Invocations
In `heltec_v3_local_battery\heltec_v3_local_battery.ino`, deep sleep is currently invoked in exactly two locations:

1. **SDN Downlink Command `CMD_FORCE_SLEEP` (lines 260-271)**:
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

2. **OLED Menu Action (lines 617-622)**:
   ```cpp
   } else if (submenuCursor == 2) {
       display.ssd1306_command(SSD1306_DISPLAYOFF);
       digitalWrite(PIN_VEXT_CTL, HIGH);
       esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0);
       esp_deep_sleep_start();
   }
   ```

### 1.2 Direct Observation: Button State Machine & Immediate Wakeup Defect
In `heltec_v3_local_battery\heltec_v3_local_battery.ino`, PRG button handling in `gestionarBotonPRG()` (lines 683-700):
```cpp
if (estadoActual == LOW && !accionLargaEjecutada) {
    if (millis() - tiempoInicioPresion >= TIEMPO_PULSACION_LARGA) { // TIEMPO_PULSACION_LARGA = 700 ms
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
**Mechanism of the Failure**:
1. When navigating to option 2 in the menu ("Suspender (Sleep)"), the user holds the PRG button for >= 700 ms to confirm execution.
2. At exactly `millis() - tiempoInicioPresion == 700`, `ejecutarAccionSubmenu()` is invoked synchronously inside `loop()`.
3. At that moment, the user's finger is still pressing the button down; `PIN_PRG` (GPIO 0) is at electrical LOW level (0V).
4. `esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0)` is configured to trigger wakeup on level `0` (LOW).
5. `esp_deep_sleep_start()` is called immediately on the subsequent line without any release wait loop.
6. The ESP32-S3 RTC power controller evaluates the wakeup source instantly, sees GPIO 0 already at level 0, and wakes the chip back up immediately (within microseconds), resetting the ESP32 into `setup()`.

### 1.3 Direct Observation: LoRa Transceiver SX1262 State
In `heltec_v3_local_battery\heltec_v3_local_battery.ino`:
- `radio.sleep()` is never called in the entire sketch.
- Neither `CMD_FORCE_SLEEP` nor `ejecutarAccionSubmenu()` puts the SX1262 into sleep.
- In `heltec_v3_config.h`:
  `PIN_VEXT_CTL` controls the switched `Vext` rail (powering the OLED and external sensors).
  However, the Semtech SX1262 is connected to the primary permanent 3.3V rail (`VDD_3V3`).
  Cutting `Vext` does **not** power off the SX1262.
- The SX1262 remains in Standby RC mode during ESP32 deep sleep, continuously consuming ~1.2 mA to 1.6 mA from the battery.

### 1.4 Direct Observation: GPIO Holds on Vext & ADC MOSFET
In `heltec_v3_local_battery\heltec_v3_local_battery.ino`:
- Lines 264 and 619 execute `digitalWrite(PIN_VEXT_CTL, HIGH)`.
- However, `gpio_hold_en()` and `gpio_deep_sleep_hold_en()` are never invoked.
- On ESP32-S3, GPIO 36 (`PIN_VEXT_CTL`) and GPIO 37 (`PIN_ADC_CTRL`) are digital IO pads (not RTC IOs; RTC IOs on ESP32-S3 are GPIO 0-21). In deep sleep, digital IO pads float unless `gpio_hold_en()` and `gpio_deep_sleep_hold_en()` are enabled.
- If GPIO 36 floats LOW during deep sleep, the P-MOSFET gate turns ON, leaking current to the OLED and sensors.
- Furthermore, on wakeup, any pin held by `gpio_deep_sleep_hold_en()` will remain locked until `gpio_deep_sleep_hold_dis()` and `gpio_hold_dis()` are called in `setup()`.

### 1.5 Direct Observation: Parasitic Leakage Through I2C Lines
- The OLED SSD1306 is powered from `Vext`. When `Vext` is cut (GPIO 36 HIGH), the SSD1306 VDD is 0V.
- If I2C SDA (GPIO 17) and SCL (GPIO 18) remain driven HIGH or held with pullups, current flows from the ESP32-S3 through SSD1306 internal ESD clamping diodes into the unpowered VDD line, generating a parasitic current leakage of 250 uA to 400 uA.
- Calling `Wire.end()` and configuring SDA/SCL as high-impedance inputs (`INPUT`) isolates the bus.

---

## 2. Logic Chain

### 2.1 Logic: Debounced Button Release (Feature F1)
1. **Observation 1.2** proves that the root cause of the immediate wakeup is that GPIO 0 is LOW when `esp_deep_sleep_start()` executes.
2. Mechanical tactile buttons experience contact bounce upon physical release, with bounce transients typically lasting between 5 ms and 30 ms.
3. Simply polling until `digitalRead(PIN_PRG) == HIGH` once is insufficient: a bounce spike could prematurely satisfy the condition while contacts are still settling.
4. Therefore, the firmware must enforce a continuous HIGH duration of at least 80 ms:
   - Poll `digitalRead(PIN_PRG)` in a tight loop with a 5 ms sampling cadence.
   - If `PIN_PRG == LOW`, reset the stable timer (`t_high_start = 0`).
   - If `PIN_PRG == HIGH`, track the elapsed duration since first HIGH.
   - Only when `millis() - t_high_start >= 80` do we conclude the button is genuinely released and mechanically stable.
5. This guarantees that when `esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0)` is armed, GPIO 0 is stably HIGH and will only trigger on the next physical button press.

### 2.2 Logic: Radio SX1262 Deep Sleep (Feature F2)
1. **Observation 1.3** proves that the SX1262 is connected to the unswitched 3.3V rail and remains in active Standby (~1,400 uA) unless commanded to sleep by software.
2. In RadioLib 7.x, the SX1262 driver provides:
   `int16_t SX126x::sleep(bool retainConfig);`
   - Setting `retainConfig = false` issues `RADIOLIB_SX126X_CMD_SET_SLEEP` with `RADIOLIB_SX126X_SLEEP_START_COLD | RADIOLIB_SX126X_SLEEP_RTC_OFF`, reducing current consumption to 0.16 uA (160 nA).
3. Executing `radio.sleep(false)` before deep sleep eliminates ~1,400 uA of parasitic battery drain.

### 2.3 Logic: Peripheral Rails and GPIO Holds (Feature F3)
1. `PIN_VEXT_CTL` (GPIO 36) controls the P-MOSFET for Vext. In active mode, LOW = ON. To turn OFF, GPIO 36 must be driven HIGH.
2. `PIN_ADC_CTRL` (GPIO 37) controls the N-MOSFET for the battery voltage divider. To disconnect the divider and prevent current drain through the 490 kOhm resistors, GPIO 37 must be driven LOW.
3. `PIN_LED_USER` (GPIO 35) must be driven LOW to ensure the LED is completely OFF.
4. Calling `gpio_hold_en((gpio_num_t)PIN_VEXT_CTL)` and `gpio_hold_en((gpio_num_t)PIN_ADC_CTRL)` followed by `gpio_deep_sleep_hold_en()` locks the output states in hardware across the entire deep sleep cycle.
5. Upon wakeup in `setup()`, the ESP32-S3 starts from reset. Calling `gpio_deep_sleep_hold_dis()`, `gpio_hold_dis((gpio_num_t)PIN_VEXT_CTL)`, and `gpio_hold_dis((gpio_num_t)PIN_ADC_CTRL)` unlocks the pads so `setup()` can re-energize Vext cleanly.

### 2.4 Logic: Ext0 Wakeup and Timer Wakeup Configuration (Feature F4)
1. GPIO 0 is mapped to RTC_GPIO0 on ESP32-S3, which is supported by `esp_sleep_enable_ext0_wakeup()`.
2. Arming `esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0)` ensures that any future press of the PRG button pulls GPIO 0 LOW and awakens the node.
3. If `sleep_segundos > 0` (such as from `CMD_FORCE_SLEEP` or scheduled sleep), `esp_sleep_enable_timer_wakeup((uint64_t)sleep_segundos * 1000000ULL)` is also armed, allowing either timeout or button press to awaken the node.

### 2.5 Logic: Interface Contract C1 Unification
1. The common function `entrarEnDeepSleepSeguro(uint32_t sleep_segundos = 0)` encapsulates all seven steps into a single atomic routine.
2. Replacing disparate in-line sleep calls in `CMD_FORCE_SLEEP` and `ejecutarAccionSubmenu()` with calls to `entrarEnDeepSleepSeguro()` ensures single-point-of-maintenance and zero divergence in power management.

---

## 3. Caveats

1. **Bootloader Invocation on Long Press Wakeup**:
   On ESP32-S3, holding GPIO 0 LOW during power-on reset puts the chip into ROM serial download mode. With our fix, waking up from deep sleep requires pressing PRG (pulling GPIO 0 LOW). If the user holds the button down for multiple seconds continuously, the ESP32-S3 might reset into bootloader mode if an external watchdog or brownout occurs. To prevent this, users should perform a standard click (~200-500 ms) to wake.
2. **I2C Reinitialization on Wakeup**:
   Because `Wire.end()` is invoked in `entrarEnDeepSleepSeguro()`, upon waking from deep sleep the chip undergoes a full reboot where `setup()` calls `Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL)`. This reinitializes the I2C bus cleanly.
3. **Immutability of `main/`**:
   No changes are proposed or allowed in `main/`. All modifications apply strictly to `heltec_v3_local_battery/`.

---

## 4. Conclusion & Concrete Code Modifications

### 4.1 Implementation of Contract C1: `entrarEnDeepSleepSeguro()`
The exact function to be added to `heltec_v3_local_battery\heltec_v3_local_battery.ino`:

```cpp
// =============================================================================
// CONTRATO C1: ENTRADA SEGURA EN DEEP SLEEP Y CONTROL DE RIELES DE ENERGIA
// =============================================================================
void entrarEnDeepSleepSeguro(uint32_t sleep_segundos) {
    Serial.println(F("[SLEEP] Preparando suspension profunda..."));
    Serial.flush();

    // 1. Poner radio LoRa SX1262 en modo sleep de ultra-bajo consumo (0.16 uA)
    radio.sleep(false);

    // 2. Apagar pantalla OLED SSD1306 y apagar bomba de carga
    display.ssd1306_command(SSD1306_DISPLAYOFF);

    // 3. Desactivar rieles de alimentacion y actuadores
    pinMode(PIN_VEXT_CTL, OUTPUT);
    digitalWrite(PIN_VEXT_CTL, HIGH); // HIGH = Cortar alimentacion Vext (OLED, sensores)

    pinMode(PIN_ADC_CTRL, OUTPUT);
    digitalWrite(PIN_ADC_CTRL, LOW);  // LOW = Desconectar divisor resistivo de bateria

    pinMode(PIN_LED_USER, OUTPUT);
    digitalWrite(PIN_LED_USER, LOW);  // LOW = Apagar LED de usuario

    // 4. Aislar pines I2C para prevenir corriente parasita hacia SSD1306 apagado
    Wire.end();
    pinMode(PIN_OLED_SDA, INPUT);
    pinMode(PIN_OLED_SCL, INPUT);

    // 5. Esperar liberacion estable del boton PRG (GPIO 0 HIGH continuo >= 80ms)
    pinMode(PIN_PRG, INPUT_PULLUP);
    uint32_t t_inicio_high = 0;
    while (true) {
        if (digitalRead(PIN_PRG) == LOW) {
            t_inicio_high = 0; // Rebote o pulsacion detectada, reiniciar contador
            delay(5);
        } else {
            if (t_inicio_high == 0) {
                t_inicio_high = millis();
            } else if (millis() - t_inicio_high >= 80) {
                break; // Estable en HIGH durante >= 80 ms continuos
            }
            delay(5);
        }
    }

    // 6. Retener estados de GPIO durante suspension profunda
    gpio_hold_en((gpio_num_t)PIN_VEXT_CTL);
    gpio_hold_en((gpio_num_t)PIN_ADC_CTRL);
    gpio_deep_sleep_hold_en();

    // 7. Configurar fuentes de despertar (ext0 en GPIO 0 nivel LOW + timer opcional)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0);
    if (sleep_segundos > 0) {
        esp_sleep_enable_timer_wakeup((uint64_t)sleep_segundos * 1000000ULL);
    }

    // 8. Iniciar suspension profunda
    esp_deep_sleep_start();
}
```

### 4.2 Required Header Inclusions in `heltec_v3_local_battery.ino`
Add to top includes (around line 34):
```cpp
#include <driver/gpio.h>
#include <esp_sleep.h>
```

### 4.3 Required Changes in `setup()` in `heltec_v3_local_battery.ino`
At the beginning of `setup()` (line 726):
```cpp
void setup() {
    Serial.begin(115200);

    // Liberar retenciones de GPIO heredadas de ciclos previos de deep sleep
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis((gpio_num_t)PIN_VEXT_CTL);
    gpio_hold_dis((gpio_num_t)PIN_ADC_CTRL);

    pinMode(PIN_PRG, INPUT_PULLUP);
    pinMode(PIN_LED_USER, OUTPUT);
    digitalWrite(PIN_LED_USER, LOW);
    ...
```

### 4.4 Unification of Sleep Invocations in `heltec_v3_local_battery.ino`

1. **In `ejecutarAccionSdn()` (CMD_FORCE_SLEEP, lines 260-271)**:
   *Before*:
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
   *After*:
   ```cpp
   case CMD_FORCE_SLEEP: {
       uint16_t mins = h->param_value;
       if (mins > 0) {
           entrarEnDeepSleepSeguro((uint32_t)mins * 60UL);
       }
       break;
   }
   ```

2. **In `ejecutarAccionSubmenu()` (Option 2 "Suspender (Sleep)", lines 617-622)**:
   *Before*:
   ```cpp
   } else if (submenuCursor == 2) {
       display.ssd1306_command(SSD1306_DISPLAYOFF);
       digitalWrite(PIN_VEXT_CTL, HIGH);
       esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PRG, 0);
       esp_deep_sleep_start();
   }
   ```
   *After*:
   ```cpp
   } else if (submenuCursor == 2) {
       entrarEnDeepSleepSeguro(0);
   }
   ```

### 4.5 Configuration Additions in `heltec_v3_config.h`
Add constant definition for release debounce timing:
```cpp
#define PRG_DEBOUNCE_RELEASE_MS      80   // Tiempo minimo continuo en HIGH antes de deep sleep
```

---

## 5. Verification Method

### 5.1 Verification Commands & Empirical Build Test
1. **Empirical Compilation Proof of Contract C1**:
   A standalone compilation test integrating `entrarEnDeepSleepSeguro`, RadioLib `SX1262::sleep(false)`, `Adafruit_SSD1306::ssd1306_command(SSD1306_DISPLAYOFF)`, `Wire.end()`, and ESP-IDF pad hold functions (`gpio_hold_en`, `gpio_deep_sleep_hold_en`, `gpio_deep_sleep_hold_dis`, `gpio_hold_dis`, `esp_sleep_enable_ext0_wakeup`, `esp_sleep_enable_timer_wakeup`, `esp_deep_sleep_start`) was executed against Arduino core `esp32:esp32:heltec_wifi_lora_32_V3` (v3.3.6).
   - **Result**: Exit code 0 (Success).
   - **Flash Usage**: 405,179 bytes (12% of 3,342,336 bytes maximum).
   - **RAM Usage**: 23,144 bytes (7% of 327,680 bytes maximum).

2. **Full Sketch Compilation Test with `arduino-cli`**:
   Run the compilation verification:
   ```powershell
   $cli = "C:\Users\mate1\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
   & $cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V3 --build-path "C:\Users\mate1\AppData\Local\Temp\ard_build_bat" "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery"
   ```
   *Pass Criteria*: Exit code 0, 0 compilation errors or symbol warnings.

2. **Debounce Release Logic Invalidation Test**:
   - Condition to invalidate: If `esp_deep_sleep_start()` is called while `digitalRead(PIN_PRG) == LOW`, the implementation fails.
   - Verification: Verify that `entrarEnDeepSleepSeguro` contains the loop `while(true)` enforcing `millis() - t_inicio_high >= 80` with reset on `LOW`.

3. **SX1262 LoRa Sleep Invalidation Test**:
   - Condition to invalidate: If `radio.sleep(false)` is omitted before entering deep sleep, the transceiver current will remain ~1.4 mA.
   - Verification: Confirm `radio.sleep(false)` is present prior to `esp_deep_sleep_start()`.

4. **Pad Hold Verification**:
   - Condition to invalidate: If `gpio_deep_sleep_hold_dis()` is omitted from `setup()`, GPIO 36 remains held HIGH on wakeup, preventing the OLED and sensors from re-powering.
   - Verification: Confirm `gpio_deep_sleep_hold_dis()` and `gpio_hold_dis()` are present at the entry of `setup()`.
