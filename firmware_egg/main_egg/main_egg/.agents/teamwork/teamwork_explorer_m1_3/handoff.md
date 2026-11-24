# Handoff Report: RTC ADC Explorer for Milestone 1 (Battery Firmware)

**Target Sketch**: `heltec_v3_local_battery/heltec_v3_local_battery.ino`  
**Explorer Role**: RTC ADC Explorer for Milestone 1  
**Features Covered**: F9 (Internal RTC Engine), F10 (LoRaWAN DeviceTimeReq Sync), F11 (On-Demand ADC Power Saver), F12 (Configurable ADC Sampling Menu & NVS), F14 (Zero Emojis Compliance)  
**State Directory**: `c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\main\.agents\teamwork\teamwork_explorer_m1_3\`  

---

## 1. Observation

### 1.1 Existing System Time Handling & Lack of RTC
In `heltec_v3_local_battery/heltec_v3_local_battery.ino`:
- Standard C/POSIX time headers (`<time.h>` and `<sys/time.h>`) are not included.
- The functions `settimeofday()`, `gettimeofday()`, `time()`, and `localtime_r()` are completely absent.
- The sketch relies exclusively on `millis()` for time elapsed calculations (e.g. line 294: `telem->uptime_ms = millis();`).
- Deep sleep resets `millis()` to 0 upon every reboot, losing all sense of absolute wall-clock time.
- There is currently no time display formatted as `"HH:MM:SS"` or `"--:--:--"` in the user interface.

### 1.2 RadioLib LoRaWAN DeviceTime Capabilities
In the installed RadioLib 7.x library (`C:\Users\mate1\Documents\Arduino\libraries\RadioLib\src\protocols\LoRaWAN\`):
- In `LoRaWAN.h` line 173:
  `#define RADIOLIB_LORAWAN_MAC_DEVICE_TIME (0x0D)`
- In `LoRaWAN.h` line 760:
  `int16_t sendMacCommandReq(uint8_t cid);`
  Queues a MAC command request into `fOptsUp` (maximum 15 bytes) to be transmitted on the next uplink frame.
- In `LoRaWAN.h` line 780:
  `int16_t getMacDeviceTimeAns(uint32_t* timestamp, uint16_t* milliseconds, bool returnUnix = true);`
  Parses `DeviceTimeAns` from the downlink frame (`fOptsDown`), computes the 32-bit Unix epoch seconds and 16-bit fractional milliseconds, and compensates for the internal transmission delay (`mod->hal->millis() - this->tUplinkEnd`).
- In `heltec_v3_local_battery/heltec_v3_local_battery.ino`:
  - `node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME)` is never invoked.
  - `node.getMacDeviceTimeAns(...)` is never called in `enviarTelemetriaLoRaWAN()` or anywhere else.
  - No `rtc_sincronizado` flag exists.

### 1.3 Unconditional 300 ms ADC Sampling Loop
In `heltec_v3_local_battery/heltec_v3_local_battery.ino`:
- Lines 796-802:
  ```cpp
  // 3. Muestreo de sensores cada 300 ms
  static uint32_t ultimoMuestreo = 0;
  if (millis() - ultimoMuestreo >= 300) {
      ultimoMuestreo = millis();
      actualizarBalanza();
      actualizarBateria();
  }
  ```
- Lines 136-154 (`leerVoltajeBateriaHardware()` called by `actualizarBateria()`):
  ```cpp
  float leerVoltajeBateriaHardware() {
      pinMode(PIN_ADC_CTRL, OUTPUT);
      digitalWrite(PIN_ADC_CTRL, HIGH); // Activar MOSFET de medicion
      delay(4);

      uint32_t suma_mv = 0;
      const int MUESTRAS = 16;
      for (int i = 0; i < MUESTRAS; i++) {
          suma_mv += analogReadMilliVolts(PIN_VBAT_ADC);
          delayMicroseconds(50);
      }
      digitalWrite(PIN_ADC_CTRL, LOW); // Desactivar MOSFET para ahorrar energia
      ...
  }
  ```
- Lines 184-205 (`actualizarBalanza()`):
  Reads analog voltage on GPIO 2 (`PIN_WEIGHT_ADC`) using `analogReadMilliVolts(PIN_WEIGHT_ADC)`.
- Physical Impact:
  - The loop wakes up the CPU and activates the battery divider transistor on GPIO 37 every 300 ms (3.33 times per second = 200 times per minute).
  - Current flows through the 490 kOhm divider (drawing ~8.57 uA during sampling) and keeps the ESP32-S3 execution cores active for > 5 ms every 300 ms, wasting battery energy when the device is idle.

### 1.4 Menu Architecture & Modal Cursor Limitation
In `heltec_v3_local_battery/heltec_v3_local_battery.ino`:
- Submenu bounds and cursor modulo are currently hardcoded to 4 items:
  - Line 564: `for (int i = 0; i < totalOpciones && i < 4; i++)`
  - Line 709: `submenuCursor = (submenuCursor + 1) % 4;`
- In `dibujarSubmenuActivo()` (lines 581-604), there is no submenu for configuring the ADC sampling interval.
- In `cargarConfiguracionNVS()` (lines 210-221) and `guardarConfiguracionNVS()` (lines 223-235), Preferences namespace `"node_cfg"` stores `"slope"`, `"zero"`, `"tare"`, `"tout_idx"`, and `"tx_idx"`, but does not store an ADC sampling mode key.

### 1.5 Emoji Audit
Inspection of `heltec_v3_local_battery/heltec_v3_local_battery.ino` and `heltec_v3_config.h`:
- Zero Unicode emojis present.
- 100% 7-bit ASCII compliant.

---

## 2. Logic Chain

### 2.1 Internal RTC Engine (F9)
1. The ESP32-S3 system architecture features a dedicated internal RTC slow-clock counter.
2. ESP-IDF provides POSIX `<time.h>` and `<sys/time.h>` functions (`settimeofday`, `gettimeofday`, `time`, `localtime_r`).
3. Calling `settimeofday(&tv, NULL)` synchronizes the POSIX epoch offset with the RTC timer. Once set, the hardware RTC autonomously maintains real time across CPU active cycles and light/deep sleep modes without needing external I2C RTC chips (e.g. DS3231).
4. `obtenerHoraFormateada(char* buffer, size_t max_len)` reads `time(NULL)`, breaks it down using thread-safe `localtime_r()`, and formats `"HH:MM:SS"` if `rtc_sincronizado == true`, or returns `"--:--:--"` if unsynchronized.

### 2.2 LoRaWAN DeviceTimeReq Synchronization (F10)
1. LoRaWAN 1.0.3/1.0.4 specification defines MAC command `DeviceTimeReq` (`0x0D`).
2. When the node activates via OTAA (`node.activateOTAA()`), or during periodic transmissions if `!rtc_sincronizado`, invoking `node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME)` places the request into the uplink `fOpts` header.
3. Upon receiving an uplink frame with `DeviceTimeReq`, ChirpStack v4 responds in the subsequent downlink frame (RX1/RX2) with `DeviceTimeAns` (5 bytes: 4 bytes seconds since GPS epoch + 1 byte fractional second).
4. RadioLib's `node.getMacDeviceTimeAns(&epoch, &fracMs, true)` extracts the payload, converts GPS epoch to Unix epoch (offset by 315964800s - 18s leap seconds), and adds the transmission delay offset (`millis() - tUplinkEnd`).
5. When `getMacDeviceTimeAns` returns `RADIOLIB_ERR_NONE`, `sincronizarHoraDesdeLoRaWAN(epoch, fracMs)` is called, setting the ESP32-S3 RTC via `settimeofday()`, setting `rtc_sincronizado = true`, and printing `[RTC OK]`.

### 2.3 On-Demand ADC Power Saver (F11)
1. Continuous 300 ms sampling is completely unnecessary for battery-operated nodes transmitting at intervals of 1 to 15 minutes.
2. When sampling mode is set to `ADC_SOLO_TX` (index 0, interval = 0 ms), the periodic sampling code in `loop()` is bypassed entirely (`if (intervaloAdcMs > 0)`).
3. Instead, `actualizarBalanza()` and `actualizarBateria()` are invoked on-demand inside `enviarTelemetriaLoRaWAN()` immediately prior to packet assembly.
4. This keeps the battery measurement MOSFET (GPIO 37) and ADC converter quiescent during idle periods, eliminating unnecessary current draw and CPU duty cycles.

### 2.4 Configurable ADC Sampling Menu & NVS Persistence (F12)
1. Contract C3 defines:
   ```cpp
   enum ModoMuestreoAdc : uint8_t {
       ADC_SOLO_TX  = 0,
       ADC_CADA_10S = 1,
       ADC_CADA_30S = 2,
       ADC_CADA_60S = 3,
       ADC_CADA_300S = 4
   };
   const uint32_t INTERVALOS_ADC_MS[5] = { 0, 10000, 30000, 60000, 300000 };
   const char*    INTERVALOS_ADC_TXT[5] = { "Solo en TX", "Cada 10s", "Cada 30s", "Cada 60s", "Cada 300s" };
   ```
2. In `PANTALLA_BATERIA` (Screen 2), which represents energy management, the contextual submenu is upgraded to `"INTERVALO ADC"` presenting the 5 options.
3. To support 5 options without cursor overflow, `gestionarBotonPRG()` dynamically calculates `uint8_t totalItems = (pantallaActual == PANTALLA_BATERIA) ? 5 : 4;` and applies modulo `(submenuCursor + 1) % totalItems`.
4. In `ejecutarAccionSubmenu()`, selecting an option stores `modoMuestreoAdcIdx = submenuCursor;` and invokes `guardarConfiguracionNVS()`, storing key `"adc_idx"` in NVS namespace `"node_cfg"`.
5. On boot, `cargarConfiguracionNVS()` restores `modoMuestreoAdcIdx = prefs.getUChar("adc_idx", ADC_CADA_10S);` (validating bounds <= 4).

---

## 3. Caveats

1. **Internal RC Oscillator Drift**: The Heltec WiFi LoRa 32 V3 relies on the ESP32-S3 internal slow RC oscillator (136/150 kHz) unless an external crystal is installed. Internal RC drift can accumulate 1-2% error over several hours of sleep. Requesting `DeviceTimeReq` once every 24 hours or upon join ensures high accuracy.
2. **Gateway GPS/NTP Availability**: If a ChirpStack v4 gateway does not have GPS synchronization or NTP enabled, it may not respond with `DeviceTimeAns`. The firmware handles this gracefully: `rtc_sincronizado` remains `false`, `obtenerHoraFormateada()` continues to display `"--:--:--"`, and telemetry packets transmit normally without blocking.
3. **Display Reading in "Solo en TX" Mode**: In `ADC_SOLO_TX` mode, weight and battery readings displayed on the OLED reflect the values captured during the most recent transmission. If the user desires real-time on-screen tracking, they can select "Cada 10s" or "Cada 30s" in the submenu.
4. **Immutability of `main/`**: The legacy version in `main/` is strictly preserved without any edits.

---

## 4. Conclusion

Features F9, F10, F11, and F12 provide a complete, low-power, self-synchronizing time and measurement architecture for the Heltec V3 battery firmware.

### 4.1 Interface Contracts Realization

#### Contract C2: Internal RTC & LoRaWAN Time Sync
```cpp
// Header inclusions
#include <time.h>
#include <sys/time.h>

// Global state
bool rtc_sincronizado = false;

// Sincronizar RTC interno con epoch y fraccion ms recibidos de ChirpStack
void sincronizarHoraDesdeLoRaWAN(uint32_t epoch_unix, uint16_t frac_ms) {
    struct timeval tv;
    tv.tv_sec = (time_t)epoch_unix;
    tv.tv_usec = (suseconds_t)frac_ms * 1000;
    settimeofday(&tv, NULL);
    rtc_sincronizado = true;
    Serial.printf("[RTC OK] Hora sincronizada con red: %lu Unix\n", (unsigned long)epoch_unix);
}

// Obtener cadena formateada "HH:MM:SS" o "--:--:--"
void obtenerHoraFormateada(char* buffer, size_t max_len) {
    if (!buffer || max_len < 9) return;
    if (!rtc_sincronizado) {
        snprintf(buffer, max_len, "--:--:--");
        return;
    }
    time_t ahora;
    time(&ahora);
    struct tm info;
    localtime_r(&ahora, &info);
    snprintf(buffer, max_len, "%02d:%02d:%02d", info.tm_hour, info.tm_min, info.tm_sec);
}

// Encolar solicitud DeviceTimeReq en LoRaWAN fOpts
void solicitarSincronizacionHoraLoRaWAN() {
    if (lora_joined) {
        int16_t res = node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME);
        if (res == RADIOLIB_ERR_NONE) {
            Serial.println(F("[LORAWAN] DeviceTimeReq encolado en fOpts."));
        }
    }
}
```

#### Contract C3: Configurable ADC Sampling Mode
```cpp
enum ModoMuestreoAdc : uint8_t {
    ADC_SOLO_TX  = 0,
    ADC_CADA_10S = 1,
    ADC_CADA_30S = 2,
    ADC_CADA_60S = 3,
    ADC_CADA_300S = 4
};

uint8_t modoMuestreoAdcIdx = ADC_CADA_10S; // Default: 1 (10s)
const uint32_t INTERVALOS_ADC_MS[5] = { 0, 10000, 30000, 60000, 300000 };
const char*    INTERVALOS_ADC_TXT[5] = { "Solo en TX", "Cada 10s", "Cada 30s", "Cada 60s", "Cada 300s" };
```

---

### 4.2 Exact Code Modifications Blueprint for `heltec_v3_local_battery.ino`

#### 1. Header Additions (Top of file)
**Target**: Lines 27-34  
```cpp
#include <Arduino.h>
#include <time.h>
#include <sys/time.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RadioLib.h>
#include <Preferences.h>
```

#### 2. Contract Variables Definition
**Target**: Around line 85 (after power management / navigation variables)
```cpp
// --- Sincronizacion RTC Interno (F9, F10) ---
bool rtc_sincronizado = false;

// --- Modo de Muestreo de ADC Configurable (F11, F12) ---
enum ModoMuestreoAdc : uint8_t {
    ADC_SOLO_TX  = 0,
    ADC_CADA_10S = 1,
    ADC_CADA_30S = 2,
    ADC_CADA_60S = 3,
    ADC_CADA_300S = 4
};

uint8_t modoMuestreoAdcIdx = ADC_CADA_10S;
const uint32_t INTERVALOS_ADC_MS[5] = { 0, 10000, 30000, 60000, 300000 };
const char*    INTERVALOS_ADC_TXT[5] = { "Solo en TX", "Cada 10s", "Cada 30s", "Cada 60s", "Cada 300s" };
```

#### 3. RTC Helper Functions
**Target**: Place immediately before Section 3 (Persistencia NVS Flash)
```cpp
// =============================================================================
// GESTION DE TIEMPO REAL (RTC INTERNO ESP32-S3)
// =============================================================================
void sincronizarHoraDesdeLoRaWAN(uint32_t epoch_unix, uint16_t frac_ms) {
    struct timeval tv;
    tv.tv_sec = (time_t)epoch_unix;
    tv.tv_usec = (suseconds_t)frac_ms * 1000;
    settimeofday(&tv, NULL);
    rtc_sincronizado = true;
    Serial.printf("[RTC OK] Hora sincronizada con red: %lu Unix\n", (unsigned long)epoch_unix);
}

void obtenerHoraFormateada(char* buffer, size_t max_len) {
    if (!buffer || max_len < 9) return;
    if (!rtc_sincronizado) {
        snprintf(buffer, max_len, "--:--:--");
        return;
    }
    time_t ahora;
    time(&ahora);
    struct tm info;
    localtime_r(&ahora, &info);
    snprintf(buffer, max_len, "%02d:%02d:%02d", info.tm_hour, info.tm_min, info.tm_sec);
}

void solicitarSincronizacionHoraLoRaWAN() {
    if (lora_joined) {
        int16_t res = node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME);
        if (res == RADIOLIB_ERR_NONE) {
            Serial.println(F("[LORAWAN] DeviceTimeReq encolado en fOpts."));
        }
    }
}
```

#### 4. NVS Storage Updates (`cargarConfiguracionNVS` & `guardarConfiguracionNVS`)
**Target**: Lines 210-235  
```cpp
void cargarConfiguracionNVS() {
    prefs.begin("node_cfg", false);
    cal_slope_v_per_kg = prefs.getFloat("slope", DEFAULT_CAL_SLOPE_V_PER_KG);
    cal_zero_v         = prefs.getFloat("zero", DEFAULT_CAL_ZERO_V);
    cal_tare_kg        = prefs.getFloat("tare", 0.0000f);
    timeoutPantallaIdx = prefs.getUChar("tout_idx", 0);
    periodoTxIdx       = prefs.getUChar("tx_idx", 1);
    modoMuestreoAdcIdx = prefs.getUChar("adc_idx", ADC_CADA_10S);
    prefs.end();

    if (timeoutPantallaIdx > 3) timeoutPantallaIdx = 0;
    if (periodoTxIdx > 4) periodoTxIdx = 1;
    if (modoMuestreoAdcIdx > 4) modoMuestreoAdcIdx = ADC_CADA_10S;
}

void guardarConfiguracionNVS() {
    prefs.begin("node_cfg", false);
    prefs.putFloat("slope", cal_slope_v_per_kg);
    prefs.putFloat("zero", cal_zero_v);
    prefs.putFloat("tare", cal_tare_kg);
    prefs.putUChar("tout_idx", timeoutPantallaIdx);
    prefs.putUChar("tx_idx", periodoTxIdx);
    prefs.putUChar("adc_idx", modoMuestreoAdcIdx);
    prefs.end();

    digitalWrite(PIN_LED_USER, HIGH);
    delay(100);
    digitalWrite(PIN_LED_USER, LOW);
}
```

#### 5. LoRaWAN Telemetry Uplink & Downlink Handling (`enviarTelemetriaLoRaWAN`)
**Target**: Lines 280-346  
```cpp
void enviarTelemetriaLoRaWAN() {
    tx_en_progreso = true;
    digitalWrite(PIN_LED_USER, HIGH);

    // Muestreo bajo demanda para telemetria fresca (F11)
    if (modoMuestreoAdcIdx == ADC_SOLO_TX) {
        actualizarBalanza();
        actualizarBateria();
    }

    // Solicitar sincronizacion de hora si aun no esta sincronizado (F10)
    if (!rtc_sincronizado) {
        solicitarSincronizacionHoraLoRaWAN();
    }

    // Calcular Tiempo de Dato en el Aire (ToA) fisico del SX1262
    RadioLibTime_t toa_us = radio.getTimeOnAir(TELEMETRY_PAYLOAD_SIZE);
    current_toa_ms = (float)toa_us / 1000.0f;

    // 1. Limpiar buffer de telemetria
    memset(telemetry_buffer, 0, sizeof(telemetry_buffer));

    // 2. Cabecera Estandar SDN Telemetry (24 bytes)
    TelemetryHeader* telem = reinterpret_cast<TelemetryHeader*>(telemetry_buffer);
    telem->seq         = ++uplink_sequence_count;
    telem->uptime_ms   = millis();
    telem->battery_mv  = voltajeBateriaFiltrado * 1000.0f;
    telem->temperature = 24.5f;
    telem->humidity    = 58.0f;
    telem->pressure    = 1013.25f;

    // 3. Datos de Balanza de Tolva Avicola (24 bytes)
    PoultryScaleData* sData = reinterpret_cast<PoultryScaleData*>(telemetry_buffer + sizeof(TelemetryHeader));
    sData->weight_kg       = pesoNetoKg;
    sData->gross_weight_kg = pesoBrutoKg;
    sData->voltage_v       = voltajeSensorV;
    sData->adc_raw         = adcSensorRaw;
    sData->cal_slope       = cal_slope_v_per_kg;
    sData->cal_zero        = cal_zero_v;
    sData->time_on_air_ms  = (uint16_t)current_toa_ms;

    // 4. Relleno deterministico hasta 204 bytes
    for (size_t i = sizeof(TelemetryHeader) + sizeof(PoultryScaleData); i < TELEMETRY_PAYLOAD_SIZE; ++i) {
        telemetry_buffer[i] = static_cast<uint8_t>((i + uplink_sequence_count) & 0xFF);
    }

    rx_len = 0;
    node.setDatarate(lora_datarate);

    // sendReceive transmite el uplink y escucha en RX1 y RX2
    int16_t state = node.sendReceive(
        telemetry_buffer,
        sizeof(telemetry_buffer),
        LORAWAN_APP_PORT,
        rx_buffer,
        &rx_len,
        false
    );

    digitalWrite(PIN_LED_USER, LOW);
    paquetesTX++;

    lora_last_rssi = radio.getRSSI();
    lora_last_snr  = radio.getSNR();

    if (state == RADIOLIB_ERR_NONE) {
        lora_joined = true;

        // Procesar respuesta DeviceTimeAns de ChirpStack si fue recibida (F10)
        uint32_t netEpoch = 0;
        uint16_t fracMs = 0;
        int16_t timeState = node.getMacDeviceTimeAns(&netEpoch, &fracMs, true);
        if (timeState == RADIOLIB_ERR_NONE) {
            sincronizarHoraDesdeLoRaWAN(netEpoch, fracMs);
        }

        if (rx_len >= sizeof(SdnHeader)) {
            paquetesRX++;
            sdn_rx_count++;
            const SdnHeader* h = reinterpret_cast<const SdnHeader*>(rx_buffer);
            ejecutarAccionSdn(h);
        }
    }

    tx_en_progreso = false;
    ultimoEnvioTxMs = millis();
}
```

#### 6. Queue DeviceTimeReq on OTAA Join (`realizarJoinOTAA`)
**Target**: Lines 348-360  
```cpp
bool realizarJoinOTAA(int maxIntentos = 3) {
    for (int i = 1; i <= maxIntentos; i++) {
        int16_t st = node.activateOTAA();
        if (st == RADIOLIB_LORAWAN_NEW_SESSION || st >= RADIOLIB_ERR_NONE) {
            node.setADR(false);
            node.setDatarate(3);
            lora_joined = true;
            solicitarSincronizacionHoraLoRaWAN(); // Encolar solicitud de hora en primer uplink
            return true;
        }
        delay(1000);
    }
    return false;
}
```

#### 7. Submenu Rendering & Action (`dibujarSubmenuActivo` & `ejecutarAccionSubmenu`)
**Target**: Lines 581-663  
In `dibujarSubmenuActivo()`:
```cpp
    else if (pantallaActual == PANTALLA_BATERIA) {
        renderSubmenu("INTERVALO ADC", INTERVALOS_ADC_TXT, 5, submenuCursor);
    }
```
In `ejecutarAccionSubmenu()`:
```cpp
    else if (pantallaActual == PANTALLA_BATERIA) {
        if (submenuCursor >= 0 && submenuCursor < 5) {
            modoMuestreoAdcIdx = (uint8_t)submenuCursor;
            guardarConfiguracionNVS();
        }
    }
```

#### 8. Submenu Navigation Modulo (`gestionarBotonPRG`)
**Target**: Lines 692-715  
When opening submenu:
```cpp
            if (!submenuActivo) {
                submenuActivo = true;
                submenuCursor = (pantallaActual == PANTALLA_BATERIA) ? modoMuestreoAdcIdx : 0;
                submenuUltimaActividad = millis();
            } else {
                ejecutarAccionSubmenu();
            }
```
When rotating through options (short press):
```cpp
            if (!submenuActivo) {
                pantallaActual = (Pantalla)((pantallaActual + 1) % TOTAL_PANTALLAS);
            } else {
                uint8_t totalItems = (pantallaActual == PANTALLA_BATERIA) ? 5 : 4;
                submenuCursor = (submenuCursor + 1) % totalItems;
                submenuUltimaActividad = millis();
            }
```

#### 9. Decoupled ADC Loop (`loop`)
**Target**: Lines 796-802  
```cpp
    // 3. Muestreo de sensores segun modo programado (F11, F12)
    uint32_t intervaloAdcMs = INTERVALOS_ADC_MS[modoMuestreoAdcIdx];
    if (intervaloAdcMs > 0) {
        static uint32_t ultimoMuestreoAdc = 0;
        if (millis() - ultimoMuestreoAdc >= intervaloAdcMs) {
            ultimoMuestreoAdc = millis();
            actualizarBalanza();
            actualizarBateria();
        }
    }
```

---

## 5. Verification Method

### 5.1 Verification Commands and Compilation
To independently verify that the code compiles without errors or warnings for the Heltec WiFi LoRa 32 V3 target:

```powershell
$cli = "C:\Users\mate1\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$buildPath = "C:\Users\mate1\AppData\Local\Temp\ard_build_bat"

& $cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V3 --build-path $buildPath "c:\Users\mate1\Documents\chirpstack_iot\chirpstack_iot\heltec_v3_local_battery"
```
Verification passes if the process exits with code 0.

### 5.2 Functional Inspection Points
1. **Internal RTC Time Formatting**:
   - Inspect `obtenerHoraFormateada()`: verify that when `rtc_sincronizado == false`, it copies `"--:--:--"`.
   - When `rtc_sincronizado == true`, verify output matches `strftime(..., "%02d:%02d:%02d", ...)`.
2. **LoRaWAN Time Sync**:
   - Verify `solicitarSincronizacionHoraLoRaWAN()` calls `node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME)`.
   - In `enviarTelemetriaLoRaWAN()`, verify `node.getMacDeviceTimeAns(&netEpoch, &fracMs, true)` is called and passes values to `sincronizarHoraDesdeLoRaWAN()`.
3. **On-Demand ADC Power Saver**:
   - Verify that when `modoMuestreoAdcIdx == ADC_SOLO_TX` (0), `INTERVALOS_ADC_MS[0] == 0`, so the timer condition in `loop()` never triggers.
   - Verify `enviarTelemetriaLoRaWAN()` executes `actualizarBalanza()` and `actualizarBateria()` immediately prior to packet packing.
4. **Configurable ADC Menu & NVS**:
   - Verify `INTERVALOS_ADC_TXT` has 5 items matching Contract C3.
   - Verify `gestionarBotonPRG()` uses `totalItems = 5` for `PANTALLA_BATERIA`.
   - Verify key `"adc_idx"` is saved in `guardarConfiguracionNVS()` and loaded in `cargarConfiguracionNVS()`.
5. **Zero Emojis Compliance**:
   - Verify 0 emoji characters in code, comments, OLED UI strings, and documentation.

### 5.3 Invalidation Conditions
- Any occurrence of Unicode emojis in code, comments, or UI invalidates compliance (F14).
- If `submenuCursor` modulo remains `% 4` when 5 items are displayed in the ADC menu, preventing selection of option 4 ("Cada 300s").
- If `settimeofday` is called with invalid parameters, or `getMacDeviceTimeAns` is called without error checking.
- If any file in `main/` is modified.
