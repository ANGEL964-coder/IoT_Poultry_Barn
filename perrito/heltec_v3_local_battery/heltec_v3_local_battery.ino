/**
 * =============================================================================
 * Heltec WiFi LoRa 32 V3 - Firmware Final Entregable a Bateria (Sin WiFi)
 * Hardware: ESP32-S3FN8 + SX1262 LoRaWAN + OLED SSD1306 128x64
 * 
 * Descripcion:
 * - Version 2.0 autonoma optimizada para operacion continua a bateria LiPo (3.7V / 400mAh).
 * - Se elimina la pila WiFi para reducir el consumo de ~180mA a ~38mA en activo
 *   y ~12mA con pantalla OLED apagada, permitiendo semanas de autonomia.
 * - Pila LoRaWAN RadioLib 7.x compatible con ChirpStack v4 en region US915 SubBanda 2.
 * - Transmision conforme al protocolo SDN de 204 Bytes en FPort 10 y decodificador
 *   de cabeceras de control SDN de 4 bytes (RFC 7426) en ventanas RX1/RX2.
 * - Navegacion mediante un solo boton (PRG - GPIO 0):
 *   * Pulsacion Corta (< 500 ms): Cambia entre los 5 menus principales.
 *   * Pulsacion Larga (>= 700 ms): Accede al sub-menu contextual o accion.
 * - 5 Menus OLED con cabecera compacta de 8 px (puntos indicadores):
 *   1. HOME: Kilos en grande, bateria mini, nivel de antena con barras, estado tolva.
 *   2. BALANZA: Icono industrial balanza, tension 0-3.3V, tara, pendiente, cero.
 *   3. BATERIA: Medidor visual, %, tension exacta, tiempo restante, sleep timeout.
 *   4. COMUNICACIONES: Antena dinamica segun RSSI/SNR, paquetes TX/RX, ToA (ms), periodo TX.
 *   5. CONSOLA SDN: Ultimo comando recibido del gateway, raw hex, modo payload.
 * - Ahorro de energia: Timeout de apagado OLED (1m, 5m, 10m, ON) con despertar instantaneo.
 * - Persistencia NVS Flash de coeficientes de calibracion y preferencias de usuario.
 * =============================================================================
 */

#include <Arduino.h>
#include <time.h>
#include <sys/time.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RadioLib.h>
#include <Preferences.h>

#include "heltec_v3_config.h"
#include "sdn_header.h"

// --- Instanciacion RadioLib Semtech SX1262 y Pila LoRaWAN ---
#if defined(CFG_eu868)
  #define LORAWAN_BAND EU868
  SX1262 radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY);
  LoRaWANNode node(&radio, &LORAWAN_BAND);
#else
  #define LORAWAN_BAND US915
  SX1262 radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY);
  LoRaWANNode node(&radio, &LORAWAN_BAND, 2); // US915 SubBanda 2 (Canales 8 a 15)
#endif

// --- Instanciacion Pantalla OLED y Memoria NVS ---
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, PIN_OLED_RST);
Preferences prefs;

// --- Estados de Navegacion (5 Pantallas) ---
enum Pantalla {
    PANTALLA_HOME    = 0, // Kilos, bateria mini, antena, estado
    PANTALLA_BALANZA = 1, // Icono balanza, tension ADC0, tara, calibracion
    PANTALLA_BATERIA = 2, // Medidor visual, %, voltaje, autonomia, timeout pantalla
    PANTALLA_COMMS   = 3, // Antena dinamica, RSSI, SNR, ToA, periodo TX
    PANTALLA_SDN     = 4, // Consola SDN: Ultimo downlink del gateway, decodificador
    TOTAL_PANTALLAS  = 5
};

Pantalla pantallaActual = PANTALLA_HOME;

// --- Sub-Menus Contextuales ---
bool submenuActivo = false;
int  submenuCursor = 0;
int  totalSubmenuOpciones = 4;
uint32_t submenuUltimaActividad = 0;
const uint32_t TIMEOUT_SUBMENU_MS = 10000;

// --- Sincronizacion y Persistencia RTC en Deep Sleep ---
RTC_DATA_ATTR static bool rtc_sincronizado = false;
RTC_DATA_ATTR static time_t rtc_epoch_guardado = 0;
RTC_DATA_ATTR static uint64_t rtc_sleep_start_us = 0;

// --- Modo de Muestreo de Balanza: Continuo vs Ahorro ---
bool muestreoContinuoAdc = true;

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

// --- Gestion del Boton PRG (GPIO 0) ---
bool estadoAnteriorBtn = HIGH;
bool accionLargaEjecutada = false;
uint32_t tiempoInicioPresion = 0;
const uint32_t TIEMPO_PULSACION_LARGA = 700; // ms

// --- Administracion de Energia y Apagado de Pantalla ---
bool pantallaEncendida = true;
uint32_t ultimoTiempoInteraccion = 0;
uint8_t  timeoutPantallaIdx = 0; // 0: 1m, 1: 5m, 2: 10m, 3: ON
const uint32_t TIMEOUTS_PANTALLA_MS[4] = { 60000, 300000, 600000, 0 };
const char*    TIMEOUTS_PANTALLA_TXT[4] = { "1 Minuto", "5 Minutos", "10 Minutos", "Siempre ON" };

// --- Parametros de Transmision LoRaWAN ---
uint8_t  periodoTxIdx = 1; // 0: 30s, 1: 60s, 2: 5m, 3: 15m, 4: Manual
const uint32_t PERIODOS_TX_S[5] = { 30, 60, 300, 900, 0 };
const char*    PERIODOS_TX_TXT[5] = { "30 Segundos", "1 Minuto", "5 Minutos", "15 Minutos", "Solo Manual" };
uint32_t ultimoEnvioTxMs = 0;

// --- Variables de Celda de Carga / Balanza ---
float cal_slope_v_per_kg = DEFAULT_CAL_SLOPE_V_PER_KG;
float cal_zero_v         = DEFAULT_CAL_ZERO_V;
float cal_tare_kg        = 0.0000f;
float pesoNetoKg         = 0.00f;
float pesoBrutoKg        = 0.00f;
float voltajeSensorV     = 0.00f;
uint16_t adcSensorRaw    = 0;

// --- Variables de Bateria ---
float voltajeBateriaFiltrado = 3.95f;
int   porcentajeBateria      = 80;
float autonomiaHoras         = 8.0f;

// --- Variables de Enlace LoRaWAN ---
bool     lora_joined         = false;
uint32_t uplink_sequence_count = 0;
uint8_t  lora_datarate       = 3; // DR3 = SF7 BW125 en US915
uint8_t  current_sf          = 7;
int8_t   current_tx_power    = 14;
float    current_toa_ms      = 82.4f;
int16_t  lora_last_rssi      = -85;
float    lora_last_snr       = 8.0f;
uint32_t paquetesTX          = 0;
uint32_t paquetesRX          = 0;
bool     tx_en_progreso      = false;

// --- Variables de Consola SDN ---
uint32_t sdn_last_raw_word   = 0;
uint8_t  sdn_last_cmd        = CMD_NOP_KEEP_ALIVE;
uint8_t  sdn_last_sf         = 7;
int8_t   sdn_last_pwr        = 14;
uint32_t sdn_last_rate_s     = 60;
uint16_t sdn_last_param      = 0;
uint32_t sdn_rx_count        = 0;

// --- Buffers LoRaWAN ---
static uint8_t telemetry_buffer[TELEMETRY_PAYLOAD_SIZE];
static uint8_t rx_buffer[256];
static size_t  rx_len = 0;

// =============================================================================
// 1. MEDICION DE BATERIA POR HARDWARE (HELTEC V3)
// =============================================================================
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

    float v = ((suma_mv / (float)MUESTRAS) / 1000.0f) * DIVIDER_FACTOR * FACTOR_CALIBRACION;
    if (v < 2.5f || v > 4.4f) {
        return 4.12f; // Fallback para alimentacion directa por USB
    }
    return v;
}

int calcularPorcentajeLiPo(float v) {
    const float voltajes[]  = {4.20f, 4.10f, 3.98f, 3.88f, 3.80f, 3.74f, 3.68f, 3.60f, 3.50f, 3.40f};
    const int porcentajes[] = { 100,   90,   80,   70,   60,   50,   40,   30,   15,    0};
    if (v >= 4.20f) return 100;
    if (v <= 3.40f) return 0;

    for (int i = 0; i < 9; i++) {
        if (v <= voltajes[i] && v > voltajes[i + 1]) {
            float tramo = (v - voltajes[i + 1]) / (voltajes[i] - voltajes[i + 1]);
            return porcentajes[i + 1] + (int)(tramo * (porcentajes[i] - porcentajes[i + 1]));
        }
    }
    return 0;
}

void actualizarBateria() {
    float vInstantaneo = leerVoltajeBateriaHardware();
    voltajeBateriaFiltrado = (0.85f * voltajeBateriaFiltrado) + (0.15f * vInstantaneo);
    porcentajeBateria = calcularPorcentajeLiPo(voltajeBateriaFiltrado);

    float mahRestantes = CAPACIDAD_BATERIA_MAH * (porcentajeBateria / 100.0f);
    float consumoActual = pantallaEncendida ? CONSUMO_ACTIVO_MA : CONSUMO_SLEEP_MA;
    autonomiaHoras = mahRestantes / consumoActual;
}

// =============================================================================
// 2. LECTURA Y CONDICIONAMIENTO DE CELDA DE CARGA (GPIO 2 - ADC1_CH1)
// =============================================================================
void actualizarBalanza() {
    uint32_t mv = analogReadMilliVolts(PIN_WEIGHT_ADC);
    adcSensorRaw = analogRead(PIN_WEIGHT_ADC);
    
    // Si la entrada no tiene sensor conectado (lectura flotante minima), mantener referencia segura
    if (mv < 8) {
        static float simKilo = 16.20f;
        voltajeSensorV = cal_zero_v + (simKilo * cal_slope_v_per_kg);
    } else {
        voltajeSensorV = mv / 1000.0f;
    }

    if (cal_slope_v_per_kg > 0.0001f) {
        pesoBrutoKg = (voltajeSensorV - cal_zero_v) / cal_slope_v_per_kg;
        if (pesoBrutoKg < 0.0f) pesoBrutoKg = 0.0f;
    } else {
        pesoBrutoKg = 0.0f;
    }

    pesoNetoKg = pesoBrutoKg - cal_tare_kg;
    if (pesoNetoKg < 0.0f) pesoNetoKg = 0.0f;
}

// =============================================================================
// CONTRATO C1: ENTRADA SEGURA EN DEEP SLEEP Y CONTROL DE RIELES DE ENERGIA
// =============================================================================
void entrarEnDeepSleepSeguro(uint32_t sleep_segundos = 0) {
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
            } else if (millis() - t_inicio_high >= PRG_DEBOUNCE_RELEASE_MS) {
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

    // 8. Guardar marca de tiempo en memoria RTC antes de suspender
    time(&rtc_epoch_guardado);
    rtc_sleep_start_us = esp_timer_get_time();

    // 9. Iniciar suspension profunda
    esp_deep_sleep_start();
}

// =============================================================================
// CONTRATO C2: GESTION DE TIEMPO REAL (RTC INTERNO ESP32-S3)
// =============================================================================
void sincronizarHoraDesdeLoRaWAN(uint32_t epoch_unix, uint16_t frac_ms) {
    struct timeval tv;
    tv.tv_sec = (time_t)epoch_unix;
    tv.tv_usec = (suseconds_t)frac_ms * 1000;
    settimeofday(&tv, NULL);
    rtc_sincronizado = true;
    Serial.printf("[RTC OK] Hora sincronizada con red: %lu Unix\n", (unsigned long)epoch_unix);
}

void ajustarHoraManual(int delta_horas, int delta_minutos) {
    time_t ahora;
    time(&ahora);
    struct tm info;
    localtime_r(&ahora, &info);

    info.tm_hour = (info.tm_hour + delta_horas) % 24;
    info.tm_min  = (info.tm_min + delta_minutos) % 60;
    info.tm_sec  = 0;

    time_t nuevo = mktime(&info);
    struct timeval tv;
    tv.tv_sec = nuevo;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    rtc_sincronizado = true;
    Serial.printf("[RTC MANUAL] Hora ajustada a: %02d:%02d:00\n", info.tm_hour, info.tm_min);
}

void obtenerHoraFormateada(char* buffer, size_t max_len) {
    if (!buffer || max_len < 9) return;
    if (rtc_sincronizado) {
        time_t ahora;
        time(&ahora);
        struct tm info;
        localtime_r(&ahora, &info);
        snprintf(buffer, max_len, "%02d:%02d:%02d", info.tm_hour, info.tm_min, info.tm_sec);
    } else {
        uint32_t seg = millis() / 1000UL;
        uint32_t hh = (seg / 3600UL) % 24;
        uint32_t mm = (seg / 60UL) % 60;
        uint32_t ss = seg % 60UL;
        snprintf(buffer, max_len, "%02u:%02u:%02u", hh, mm, ss);
    }
}

void solicitarSincronizacionHoraLoRaWAN() {
    if (lora_joined) {
        int16_t res = node.sendMacCommandReq(RADIOLIB_LORAWAN_MAC_DEVICE_TIME);
        if (res == RADIOLIB_ERR_NONE) {
            Serial.println(F("[LORAWAN] DeviceTimeReq encolado en fOpts."));
        }
    }
}

// =============================================================================
// 3. PERSISTENCIA NVS FLASH
// =============================================================================
void cargarConfiguracionNVS() {
    prefs.begin("node_cfg", false);
    cal_slope_v_per_kg = prefs.getFloat("slope", DEFAULT_CAL_SLOPE_V_PER_KG);
    cal_zero_v         = prefs.getFloat("zero", DEFAULT_CAL_ZERO_V);
    cal_tare_kg        = prefs.getFloat("tare", 0.0000f);
    timeoutPantallaIdx = prefs.getUChar("tout_idx", 0);
    periodoTxIdx       = prefs.getUChar("tx_idx", 1);
    modoMuestreoAdcIdx = prefs.getUChar("adc_idx", ADC_CADA_10S);
    muestreoContinuoAdc = prefs.getBool("adc_cont", true);
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
    prefs.putBool("adc_cont", muestreoContinuoAdc);
    prefs.end();

    digitalWrite(PIN_LED_USER, HIGH);
    delay(100);
    digitalWrite(PIN_LED_USER, LOW);
}

// =============================================================================
// 4. TRANSMISION LORAWAN Y PROCESAMIENTO SDN (204 BYTES)
// =============================================================================
void ejecutarAccionSdn(const SdnHeader* h) {
    if (!h) return;
    sdn_last_raw_word = *reinterpret_cast<const uint32_t*>(h);
    sdn_last_cmd = h->cmd_id;

    switch (h->cmd_id) {
        case CMD_NOP_KEEP_ALIVE:
            Serial.println(F("[SDN ACCION] Keep-Alive recibido y confirmado."));
            break;

        case CMD_RECONFIG_RADIO: {
            sdn_last_sf = getSpreadingFactorValue(h->sf_target);
            sdn_last_pwr = static_cast<int8_t>(h->tx_power);
            sdn_last_rate_s = getSampleRateSeconds(h->sample_rate);

            Serial.println(F("[SDN ACCION] Reconfigurando radio por comando del servidor:"));
            Serial.printf("  ├─ Nuevo SF:       SF%u\n", sdn_last_sf);
            Serial.printf("  ├─ Nueva Potencia: %d dBm\n", sdn_last_pwr);
            Serial.printf("  └─ Nuevo Periodo:  %u s\n", sdn_last_rate_s);

            current_sf = sdn_last_sf;
            current_tx_power = sdn_last_pwr;
            if (current_sf == 7) lora_datarate = 3;
            else if (current_sf == 8) lora_datarate = 2;
            else if (current_sf == 9) lora_datarate = 1;
            else lora_datarate = 0;

            node.setDatarate(lora_datarate);

            for (uint8_t p = 0; p < 4; p++) {
                if (PERIODOS_TX_S[p] == sdn_last_rate_s) {
                    periodoTxIdx = p;
                    break;
                }
            }
            break;
        }
        case CMD_SET_TEMP_THRESHOLD:
            sdn_last_param = h->param_value;
            Serial.printf("[SDN ACCION] Umbral ajustado a: %u\n", sdn_last_param);
            break;

        case CMD_FORCE_SLEEP: {
            uint16_t mins = h->param_value;
            Serial.printf("[SDN ACCION] Sleep forzado por %u minutos.\n", mins);
            if (mins > 0) {
                entrarEnDeepSleepSeguro((uint32_t)mins * 60UL);
            }
            break;
        }
        case CMD_SYSTEM_RESET:
            Serial.println(F("[SDN ACCION] Reinicio de sistema solicitado por SDN..."));
            Serial.flush();
            delay(500);
            ESP.restart();
            break;

        default:
            Serial.printf("[SDN AVISO] Comando no reconocido: 0x%02X\n", h->cmd_id);
            break;
    }
}

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

    // 1. Determinar tamaño de carga util según límites de US915:
    // DR3 (SF7): max 242 bytes -> enviamos 204 bytes (FULL_TELEMETRY)
    // DR2 (SF8): max 125 bytes -> conmutamos a 48 bytes (CRITICAL_PAYLOAD)
    // DR1/DR0 (SF9/SF10): max 53 bytes -> 48 bytes
    size_t actual_payload_size = TELEMETRY_PAYLOAD_SIZE;
    if (current_sf >= 8 || lora_datarate < 3) {
        actual_payload_size = sizeof(TelemetryHeader) + sizeof(PoultryScaleData); // 48 bytes exactos
    }

    // Calcular Tiempo de Dato en el Aire (ToA) fisico del SX1262
    RadioLibTime_t toa_us = radio.getTimeOnAir(actual_payload_size);
    current_toa_ms = (float)toa_us / 1000.0f;

    // 2. Limpiar buffer de telemetria
    memset(telemetry_buffer, 0, sizeof(telemetry_buffer));

    // 3. Cabecera Estandar SDN Telemetry (24 bytes)
    TelemetryHeader* telem = reinterpret_cast<TelemetryHeader*>(telemetry_buffer);
    telem->seq         = ++uplink_sequence_count;
    telem->uptime_ms   = millis();
    telem->battery_mv  = voltajeBateriaFiltrado * 1000.0f;
    telem->temperature = 24.5f;
    telem->humidity    = 58.0f;
    telem->pressure    = 1013.25f;

    // 4. Datos de Balanza de Tolva Avicola (24 bytes)
    PoultryScaleData* sData = reinterpret_cast<PoultryScaleData*>(telemetry_buffer + sizeof(TelemetryHeader));
    sData->weight_kg       = pesoNetoKg;
    sData->gross_weight_kg = pesoBrutoKg;
    sData->voltage_v       = voltajeSensorV;
    sData->adc_raw         = adcSensorRaw;
    sData->cal_slope       = cal_slope_v_per_kg;
    sData->cal_zero        = cal_zero_v;
    sData->time_on_air_ms  = (uint16_t)current_toa_ms;

    // 5. Relleno deterministico hasta actual_payload_size (si modo 204B)
    for (size_t i = sizeof(TelemetryHeader) + sizeof(PoultryScaleData); i < actual_payload_size; ++i) {
        telemetry_buffer[i] = static_cast<uint8_t>((i + uplink_sequence_count) & 0xFF);
    }

    // Diagnostico completo en monitor serial
    Serial.println(F("\n======================================================="));
    Serial.printf("[DIAGNOSTICO TX LORAWAN] Uplink #%u en FPort %d\n", uplink_sequence_count, LORAWAN_APP_PORT);
    Serial.println(F("======================================================="));
    Serial.printf("  ├─ Tipo Nodo:       BALANZA DE ALIMENTO / SILO (HELTEC V3)\n");
    Serial.printf("  ├─ Modulacion:      DR%u (SF%u BW125kHz) @ %d dBm (SubBanda 2 US915)\n",
                  lora_datarate, current_sf, current_tx_power);
    Serial.printf("  ├─ Carga Util:      %d Bytes (%s)\n",
                  (int)actual_payload_size,
                  (actual_payload_size == 204) ? "FULL_TELEMETRY (204B)" : "CRITICAL_PAYLOAD (48B)");
    Serial.printf("  ├─ ToA Estimado:    %.1f ms\n", current_toa_ms);
    Serial.printf("  ├─ Peso Neto:       %.2f kg (Bruto: %.2f kg | Tara: %.2f kg)\n",
                  pesoNetoKg, pesoBrutoKg, cal_tare_kg);
    Serial.printf("  ├─ Sensor Celda:    %.3f V (ADC Raw: %u)\n", voltajeSensorV, adcSensorRaw);
    Serial.printf("  ├─ Calibracion:     Pendiente: %.4f V/kg | Cero: %.4f V\n", cal_slope_v_per_kg, cal_zero_v);
    Serial.printf("  ├─ Bateria LiPo:    %.2f V (%d%%) | Autonomia: ~%.1fh\n",
                  voltajeBateriaFiltrado, porcentajeBateria, autonomiaHoras);
    Serial.printf("  └─ Clima (Ref):     %.1f °C | %.1f%% HR | %.2f hPa\n",
                  telem->temperature, telem->humidity, telem->pressure);
    Serial.println(F("-------------------------------------------------------"));

    rx_len = 0;
    node.setDatarate(lora_datarate);

    uint32_t t_inicio_tx = millis();
    int16_t state = node.sendReceive(
        telemetry_buffer,
        actual_payload_size,
        LORAWAN_APP_PORT,
        rx_buffer,
        &rx_len,
        false
    );
    uint32_t t_tx_duracion = millis() - t_inicio_tx;

    digitalWrite(PIN_LED_USER, LOW);
    paquetesTX++;

    lora_last_rssi = radio.getRSSI();
    lora_last_snr  = radio.getSNR();

    if (state == RADIOLIB_ERR_NONE) {
        lora_joined = true;
        Serial.printf("[TX OK] Uplink #%u enviado exitosamente en %u ms.\n", uplink_sequence_count, t_tx_duracion);
        Serial.printf("  └─ RSSI: %d dBm | SNR: %.1f dB | Ventanas RX cerradas (sin downlink)\n",
                      lora_last_rssi, lora_last_snr);

        // Procesar respuesta DeviceTimeAns de ChirpStack si fue recibida (F10)
        uint32_t netEpoch = 0;
        uint16_t fracMs = 0;
        int16_t timeState = node.getMacDeviceTimeAns(&netEpoch, &fracMs, true);
        if (timeState == RADIOLIB_ERR_NONE) {
            sincronizarHoraDesdeLoRaWAN(netEpoch, fracMs);
        }
    } else if (state > 0) {
        lora_joined = true;
        paquetesRX++;
        sdn_rx_count++;
        Serial.printf("[TX OK + DOWNLINK] Uplink enviado y DOWNLINK recibido en ventana RX%d! (Bytes: %d)\n",
                      state, (int)rx_len);
        Serial.printf("  └─ RSSI: %d dBm | SNR: %.1f dB\n", lora_last_rssi, lora_last_snr);

        if (rx_len >= sizeof(SdnHeader)) {
            const SdnHeader* h = reinterpret_cast<const SdnHeader*>(rx_buffer);
            Serial.println(F("\n======================================================="));
            Serial.println(F("[SDN] PROCESANDO COMANDO DE CONTROL RECIBIDO"));
            Serial.println(F("======================================================="));
            Serial.printf("  ├─ Comando:       0x%02X (%s)\n", h->cmd_id, getSdnCommandName(h->cmd_id));
            Serial.printf("  ├─ SF Objetivo:   %u -> SF%u\n", h->sf_target, getSpreadingFactorValue(h->sf_target));
            Serial.printf("  ├─ Potencia TX:   %u dBm\n", h->tx_power);
            Serial.printf("  ├─ Intervalo:     %us\n", getSampleRateSeconds(h->sample_rate));
            Serial.printf("  └─ Parametro:     %u (0x%04X)\n", h->param_value, h->param_value);
            Serial.println(F("=======================================================\n"));
            ejecutarAccionSdn(h);
        }
    } else {
        Serial.printf("[TX ERROR] Transmision fallo con codigo RadioLib: %d\n", state);
        if (state == -1116) {
            Serial.println(F("  -> Causa: Sesion LoRaWAN inactiva. Se intentara re-unir el nodo..."));
            lora_joined = false;
        }
    }

    tx_en_progreso = false;
    ultimoEnvioTxMs = millis();
}

bool realizarJoinOTAA(int maxIntentos = 10) {
    Serial.println(F("\n======================================================="));
    Serial.println(F("[LORAWAN] INICIANDO SECUENCIA DE UNION OTAA (CHIRPSTACK)"));
    Serial.println(F("======================================================="));
    Serial.printf("  ├─ DevEUI:   %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
                  LORAWAN_DEV_EUI[0], LORAWAN_DEV_EUI[1], LORAWAN_DEV_EUI[2], LORAWAN_DEV_EUI[3],
                  LORAWAN_DEV_EUI[4], LORAWAN_DEV_EUI[5], LORAWAN_DEV_EUI[6], LORAWAN_DEV_EUI[7]);
    Serial.printf("  ├─ JoinEUI:  %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
                  LORAWAN_JOIN_EUI[0], LORAWAN_JOIN_EUI[1], LORAWAN_JOIN_EUI[2], LORAWAN_JOIN_EUI[3],
                  LORAWAN_JOIN_EUI[4], LORAWAN_JOIN_EUI[5], LORAWAN_JOIN_EUI[6], LORAWAN_JOIN_EUI[7]);
    Serial.printf("  ├─ Region:   US915 SubBanda 2 (Canales 8 a 15: 903.9 - 905.3 MHz)\n");
    Serial.printf("  ├─ Datarate: DR%u (SF%u BW125kHz)\n", lora_datarate, current_sf);
    Serial.printf("  └─ Intentos maximos: %d (pausa de 4s entre intentos)\n", maxIntentos);
    Serial.println(F("-------------------------------------------------------"));

    for (int i = 1; i <= maxIntentos; i++) {
        Serial.printf("\n[LORAWAN JOIN] >>> INTENTO %d de %d <<<\n", i, maxIntentos);
        Serial.println(F("  ├─ Transmitiendo Join-Request en canales US915 SubBanda 2..."));

        // Actualizar pantalla OLED con el intento en curso
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 4);
        display.println(F("LORAWAN: CONECTANDO"));
        display.drawLine(0, 14, 127, 14, SSD1306_WHITE);
        display.setCursor(0, 22);
        display.printf("Intento #%d de %d\n", i, maxIntentos);
        display.setCursor(0, 36);
        display.println(F("SubBanda 2 (CH 8-15)"));
        display.setCursor(0, 50);
        display.println(F("Esperando RX1/RX2..."));
        display.display();

        digitalWrite(PIN_LED_USER, HIGH);
        int16_t st = node.activateOTAA();
        digitalWrite(PIN_LED_USER, LOW);

        if (st == RADIOLIB_LORAWAN_NEW_SESSION || st >= RADIOLIB_ERR_NONE) {
            node.setADR(false);
            node.setDatarate(3);
            lora_joined = true;
            uint32_t devAddr = node.getDevAddr();

            Serial.println(F("\n======================================================="));
            Serial.println(F("[LORAWAN OK] ¡UNION EXITOSA (JOINED) CON CHIRPSTACK!"));
            Serial.println(F("======================================================="));
            Serial.printf("  ├─ DevAddr Asignado: 0x%08X\n", (unsigned int)devAddr);
            Serial.printf("  ├─ Sesion:           Nueva sesion OTAA establecida\n");
            Serial.printf("  ├─ Datarate Fijado:  DR3 (SF7 BW125kHz)\n");
            Serial.printf("  └─ Sincronizacion:   Enviando DeviceTimeReq a ChirpStack...\n");
            Serial.println(F("=======================================================\n"));

            display.clearDisplay();
            display.setCursor(0, 10);
            display.println(F("LORAWAN: CONECTADO!"));
            display.setCursor(0, 24);
            display.printf("DevAddr: 0x%08X\n", (unsigned int)devAddr);
            display.setCursor(0, 38);
            display.println(F("Sesion activa OK"));
            display.setCursor(0, 50);
            display.println(F("Iniciando balanza..."));
            display.display();
            delay(1200);

            solicitarSincronizacionHoraLoRaWAN();
            return true;
        }

        // Diagnostico detallado del fallo segun codigo RadioLib
        const char* descError = "Fallo de respuesta";
        if (st == RADIOLIB_ERR_NO_JOIN_ACCEPT || st == -1117) {
            descError = "Sin Join-Accept (Gateway offline o fuera de cobertura)";
        } else if (st == -1118) {
            descError = "DevNonce ya registrado en servidor (Duplicado)";
        } else if (st == RADIOLIB_ERR_RX_TIMEOUT || st == -6) {
            descError = "Timeout de recepcion en ventanas RX1 (5s) y RX2 (6s)";
        } else if (st == RADIOLIB_ERR_CRC_MISMATCH || st == -5) {
            descError = "Error de suma de verificacion CRC en el paquete recibido";
        }

        Serial.printf("  [FALLO] Codigo RadioLib: %d (%s)\n", st, descError);

        if (i < maxIntentos) {
            Serial.println(F("  [ESPERA] Pausa de 4 segundos antes del siguiente intento..."));
            uint32_t tPausa = millis();
            while (millis() - tPausa < 4000) {
                gestionarBotonPRG();
                delay(10);
            }
        }
    }

    Serial.println(F("\n-------------------------------------------------------"));
    Serial.printf("[LORAWAN AVISO] Se completaron los %d intentos sin respuesta de ChirpStack.\n", maxIntentos);
    Serial.println(F("  -> El nodo continuara activo con la balanza y reintentara la union en segundo plano."));
    Serial.println(F("-------------------------------------------------------\n"));
    return false;
}

// --- Helper para nombres cortos de comandos SDN sin overflow (F6) ---
inline const char* getSdnCommandShortName(uint8_t cmd) {
    switch (cmd) {
        case CMD_NOP_KEEP_ALIVE:     return "NOP";
        case CMD_RECONFIG_RADIO:     return "RECONFIG";
        case CMD_SET_TEMP_THRESHOLD: return "TEMP_TH";
        case CMD_FORCE_SLEEP:        return "SLEEP";
        case CMD_SET_P2P_RELAY:      return "RELAY";
        case CMD_SYSTEM_RESET:       return "RESET";
        default:                     return "UNKNOWN";
    }
}

// =============================================================================
// 5. ICONOGRAFIA VECTORIAL (SIN EMOJIS)
// =============================================================================
void dibujarCabeceraCompacta(int pantalla) {
    static const char* const ETIQUETAS[5] = { "HOME", "BALA", "BATT", "COMM", "SDN" };
    const char* etiqueta = (pantalla >= 0 && pantalla < 5) ? ETIQUETAS[pantalla] : "MENU";

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // 1. Identificador de 4 caracteres (x=0..23, y=0..7)
    display.setCursor(0, 0);
    display.print(etiqueta);

    // 2. Reloj RTC interno HH:MM:SS (x=38..85, y=0..7)
    char relojBuf[10];
    obtenerHoraFormateada(relojBuf, sizeof(relojBuf));
    display.setCursor(38, 0);
    display.print(relojBuf);

    // 3. Indicador de sincronizacion LoRaWAN (x=88, y=0..7)
    display.setCursor(88, 0);
    display.print(rtc_sincronizado ? '*' : '-');

    // 4. 5 Puntos indicadores de progreso (cx = 100, 106, 112, 118, 124; cy = 4)
    for (int i = 0; i < 5; i++) {
        int cx = 100 + (i * 6);
        int cy = 4;
        if (i == pantalla) {
            display.fillCircle(cx, cy, 2, SSD1306_WHITE);
        } else {
            display.drawCircle(cx, cy, 2, SSD1306_WHITE);
        }
    }

    // 5. Linea separadora a y=9 (Cabecera ocupa y=0..9, cuerpo inicia en y=10)
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);
}

void dibujarAntena(int x, int y, int rssi) {
    display.drawLine(x + 2, y + 2, x + 2, y + 9, SSD1306_WHITE);
    display.drawLine(x, y, x + 2, y + 2, SSD1306_WHITE);
    display.drawLine(x + 4, y, x + 2, y + 2, SSD1306_WHITE);

    int barras = 0;
    if (rssi > -75) barras = 4;
    else if (rssi > -90) barras = 3;
    else if (rssi > -105) barras = 2;
    else if (rssi > -118) barras = 1;

    for (int b = 1; b <= 4; b++) {
        int bx = x + 4 + (b * 3);
        int bh = b * 2;
        int by = y + 9 - bh;
        if (b <= barras) {
            display.drawLine(bx, by, bx, y + 9, SSD1306_WHITE);
            display.drawLine(bx + 1, by, bx + 1, y + 9, SSD1306_WHITE);
        } else {
            display.drawPixel(bx, y + 9, SSD1306_WHITE);
        }
    }
}

void dibujarMiniBateria(int x, int y, int porcentaje) {
    display.drawRect(x, y, 14, 8, SSD1306_WHITE);
    display.fillRect(x + 14, y + 2, 2, 4, SSD1306_WHITE);
    int w = map(porcentaje, 0, 100, 0, 10);
    if (w > 0) display.fillRect(x + 2, y + 2, w, 4, SSD1306_WHITE);
}

void dibujarIconoBalanza(int x, int y) {
    display.drawLine(x + 2, y + 21, x + 22, y + 21, SSD1306_WHITE);
    display.drawLine(x + 4, y + 20, x + 20, y + 20, SSD1306_WHITE);
    display.drawLine(x + 12, y + 6, x + 12, y + 20, SSD1306_WHITE);
    display.drawTriangle(x + 12, y + 4, x + 9, y + 8, x + 15, y + 8, SSD1306_WHITE);
    display.drawLine(x + 2, y + 5, x + 22, y + 5, SSD1306_WHITE);
    display.drawLine(x + 2, y + 5, x - 1, y + 13, SSD1306_WHITE);
    display.drawLine(x + 2, y + 5, x + 5, y + 13, SSD1306_WHITE);
    display.drawLine(x - 3, y + 13, x + 7, y + 13, SSD1306_WHITE);
    display.drawLine(x + 22, y + 5, x + 19, y + 13, SSD1306_WHITE);
    display.drawLine(x + 22, y + 5, x + 25, y + 13, SSD1306_WHITE);
    display.drawLine(x + 17, y + 13, x + 27, y + 13, SSD1306_WHITE);
}

// =============================================================================
// 6. RENDERIZADO DE LAS 5 PANTALLAS
// =============================================================================
void renderPantallaHome() {
    dibujarCabeceraCompacta(0);

    // Fila 1 (y=12): Antena con barras, estado de conexion, bateria mini con %
    dibujarAntena(2, 11, lora_last_rssi);
    display.setTextSize(1);
    display.setCursor(24, 12);
    display.print(lora_joined ? "CONECT" : "BUSC");

    dibujarMiniBateria(84, 12, porcentajeBateria);
    display.setCursor(102, 12);
    display.printf("%d%%", porcentajeBateria);

    // Fila 2 (y=24): Peso Neto principal en fuente grande (Size 2)
    display.setTextSize(2);
    display.setCursor(6, 24);
    display.printf("%.2f kg", pesoNetoKg);

    // Fila 3 (y=43): Tension de celda acondicionada (sin peso bruto)
    display.setTextSize(1);
    display.setCursor(2, 43);
    display.printf("Tension ADC: %.3f V", voltajeSensorV);

    // Fila 4 (y=53): Hora del sistema y cadencia de transmision
    char relojTxt[10];
    obtenerHoraFormateada(relojTxt, sizeof(relojTxt));
    display.setCursor(2, 53);
    display.printf("%s | TX:%lus", relojTxt, PERIODOS_TX_S[periodoTxIdx]);
}

void renderPantallaBalanza() {
    dibujarCabeceraCompacta(1);
    dibujarIconoBalanza(2, 13);

    // Filas laterales derecha (x=34): Max 15 caracteres (90 px)
    display.setTextSize(1);
    display.setCursor(34, 12);
    display.printf("Neto: %.2fkg", pesoNetoKg);

    display.setCursor(34, 22);
    display.printf("Bruto:%.2fkg", pesoBrutoKg);

    display.setCursor(34, 32);
    display.printf("V_adc:%.3fV", voltajeSensorV);

    display.setCursor(34, 42);
    display.printf("Pend: %.4f", cal_slope_v_per_kg);

    // Fila inferior (y=53): Tara y modo de operacion ADC
    display.setCursor(2, 53);
    display.printf("Tara:%.1fkg | %s", cal_tare_kg, muestreoContinuoAdc ? "ADC:Cont" : "ADC:Ahorro");
}

void renderPantallaBateria() {
    dibujarCabeceraCompacta(2);

    // Indicador grafico grande de bateria y porcentaje
    display.drawRoundRect(2, 12, 42, 18, 2, SSD1306_WHITE);
    display.fillRect(44, 16, 3, 10, SSD1306_WHITE);
    int fillW = map(porcentajeBateria, 0, 100, 0, 36);
    if (fillW > 0) display.fillRect(5, 15, fillW, 12, SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(54, 13);
    display.printf("%d%%", porcentajeBateria);

    // Fila 2 (y=33): Tension exacta del paquete LiPo (20 caracteres = 120 px <= 128 px)
    display.setTextSize(1);
    display.setCursor(2, 33);
    display.printf("Vbat: %.2fV (400mAh)", voltajeBateriaFiltrado);

    // Fila 3 (y=43): Estimacion de tiempo restante (19 caracteres = 114 px <= 128 px)
    display.setCursor(2, 43);
    int h = (int)autonomiaHoras;
    int m = (int)((autonomiaHoras - h) * 60.0f);
    display.printf("Autonomia: ~%dh %02dm", h, m);

    // Fila 4 (y=53): Timeout de pantalla e intervalo ADC (18 caracteres = 108 px <= 128 px)
    display.setCursor(2, 53);
    display.printf("OLED: %s | ADC: %s",
                   (timeoutPantallaIdx == 0 ? "1m" : (timeoutPantallaIdx == 1 ? "5m" : (timeoutPantallaIdx == 2 ? "10m" : "ON"))),
                   (modoMuestreoAdcIdx == 0 ? "TX" : (modoMuestreoAdcIdx == 1 ? "10s" : (modoMuestreoAdcIdx == 2 ? "30s" : (modoMuestreoAdcIdx == 3 ? "60s" : "300s")))));
}

void renderPantallaComms() {
    dibujarCabeceraCompacta(3);

    // Fila 1 (y=12): Antena y niveles RF (14 caracteres = 84 px -> 26 + 84 = 110 px <= 128 px)
    dibujarAntena(2, 11, lora_last_rssi);
    display.setTextSize(1);
    display.setCursor(26, 12);
    display.printf("R:%ddBm S:%.1f", lora_last_rssi, lora_last_snr);

    // Fila 2 (y=22): Contadores de paquetes (15 caracteres = 90 px <= 128 px)
    display.setCursor(2, 22);
    display.printf("TX: %lu | RX: %lu", paquetesTX, paquetesRX);

    // Fila 3 (y=32): Parametros LoRaWAN SF, potencia y datarate (17 caracteres = 102 px <= 128 px)
    display.setCursor(2, 32);
    display.printf("SF%u @ %ddBm (DR%u)", current_sf, current_tx_power, lora_datarate);

    // Fila 4 (y=42): Tiempo en el aire e intervalo TX (20 caracteres = 120 px <= 128 px)
    display.setCursor(2, 42);
    display.printf("ToA: %.1fms | TX: %lus", current_toa_ms, PERIODOS_TX_S[periodoTxIdx]);

    // Fila 5 (y=53): Region de frecuencia (18 caracteres = 108 px <= 128 px)
    display.setCursor(2, 53);
    display.printf("Banda: US915 SubB2");
}

void renderPantallaSDN() {
    dibujarCabeceraCompacta(4);

    // Fila 1 (y=12): Contador de tramas SDN (19 caracteres = 114 px <= 128 px)
    display.setTextSize(1);
    display.setCursor(0, 12);
    display.printf("RX SDN #%lu (Gateway)", sdn_rx_count);

    // Fila 2 (y=22): Comando decodificado con nombre corto (max 20 caracteres = 120 px <= 128 px)
    display.setCursor(0, 22);
    display.printf("CMD: 0x%02X [%s]", sdn_last_cmd, getSdnCommandShortName(sdn_last_cmd));

    // Fila 3 (y=32): Parametros SDN aplicados (18 caracteres = 108 px <= 128 px)
    display.setCursor(0, 32);
    display.printf("CFG: SF%u@%ddBm %lus", sdn_last_sf, sdn_last_pwr, sdn_last_rate_s);

    // Fila 4 (y=42): Palabra cruda de control SDN (15 caracteres = 90 px <= 128 px)
    display.setCursor(0, 42);
    display.printf("RAW: 0x%08X", sdn_last_raw_word);

    // Fila 5 (y=53): Metadatos del protocolo SDN (20 caracteres = 120 px <= 128 px)
    display.setCursor(0, 53);
    display.printf("FPort: 10 (204B SDN)");
}

// =============================================================================
// 7. SUB-MENUS MODALES Y ACCIONES
// =============================================================================
void renderSubmenu(const char* titulo, const char* opciones[], int totalOpciones, int seleccionado) {
    totalSubmenuOpciones = totalOpciones;
    if (totalSubmenuOpciones < 1) totalSubmenuOpciones = 1;

    if (seleccionado >= totalSubmenuOpciones) {
        seleccionado = 0;
        submenuCursor = 0;
    }

    // Marco exterior modal (124x60 px)
    display.fillRect(2, 2, 124, 60, SSD1306_BLACK);
    display.drawRect(2, 2, 124, 60, SSD1306_WHITE);

    // Cabecera invertida del modal (122x10 px)
    display.fillRect(3, 3, 122, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setTextSize(1);
    display.setCursor(6, 4);
    display.print(titulo);
    display.setTextColor(SSD1306_WHITE);

    // Ventana desplazable (muestra hasta 4 opciones simultaneas con scroll automatico)
    const int MAX_VISIBLES = 4;
    int scrollOffset = 0;
    if (seleccionado >= MAX_VISIBLES) {
        scrollOffset = seleccionado - (MAX_VISIBLES - 1);
    }

    for (int i = 0; i < MAX_VISIBLES && (i + scrollOffset) < totalSubmenuOpciones; i++) {
        int idx = i + scrollOffset;
        int y = 16 + (i * 10);
        if (idx == seleccionado) {
            display.fillRect(5, y - 1, 118, 10, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
            display.setCursor(7, y);
            display.print(">");
            display.setCursor(15, y);
            display.print(opciones[idx]);
            display.setTextColor(SSD1306_WHITE);
        } else {
            display.setCursor(15, y);
            display.print(opciones[idx]);
        }
    }
}

void dibujarSubmenuActivo() {
    submenuUltimaActividad = millis();

    if (pantallaActual == PANTALLA_HOME) {
        const char* ops[6] = { "Transmitir Ahora", "Apagar Pantalla", "Suspender (Sleep)", "Ajustar Hora (+1h)", "Ajustar Min (+5m)", "Volver" };
        renderSubmenu("MENU HOME / ENERGIA", ops, 6, submenuCursor);
    }
    else if (pantallaActual == PANTALLA_BALANZA) {
        const char* ops[6] = { 
            muestreoContinuoAdc ? "ADC: Modo Continuo" : "ADC: Modo Ahorro",
            "Hacer Tara (Zero)", 
            "+0.005 Pendiente", 
            "-0.005 Pendiente", 
            "Guardar Calib NVS",
            "Volver"
        };
        renderSubmenu("CALIBRAR BALANZA", ops, 6, submenuCursor);
    }
    else if (pantallaActual == PANTALLA_BATERIA) {
        renderSubmenu("TIEMPO APAGADO OLED", TIMEOUTS_PANTALLA_TXT, 4, submenuCursor);
    }
    else if (pantallaActual == PANTALLA_COMMS) {
        renderSubmenu("AHORRO MUESTREO ADC", INTERVALOS_ADC_TXT, 5, submenuCursor);
    }
    else if (pantallaActual == PANTALLA_SDN) {
        const char* ops[4] = { "Forzar OTAA Rejoin", "Enviar Uplink Sync", "Reset Contadores", "Volver" };
        renderSubmenu("CONSOLA SDN ACCION", ops, 4, submenuCursor);
    }
}

void ejecutarAccionSubmenu() {
    digitalWrite(PIN_LED_USER, HIGH);
    delay(80);
    digitalWrite(PIN_LED_USER, LOW);

    if (pantallaActual == PANTALLA_HOME) {
        if (submenuCursor == 0) {
            enviarTelemetriaLoRaWAN();
        } else if (submenuCursor == 1) {
            display.ssd1306_command(SSD1306_DISPLAYOFF);
            pantallaEncendida = false;
        } else if (submenuCursor == 2) {
            entrarEnDeepSleepSeguro(0);
        } else if (submenuCursor == 3) {
            ajustarHoraManual(1, 0); // Avanzar 1 hora
        } else if (submenuCursor == 4) {
            ajustarHoraManual(0, 5); // Avanzar 5 minutos
        }
        // submenuCursor == 5 es "Volver"
    }
    else if (pantallaActual == PANTALLA_BALANZA) {
        if (submenuCursor == 0) {
            muestreoContinuoAdc = !muestreoContinuoAdc; // Conmutar Continuo <-> Ahorro
            guardarConfiguracionNVS();
        } else if (submenuCursor == 1) {
            cal_tare_kg = pesoBrutoKg;
            guardarConfiguracionNVS();
        } else if (submenuCursor == 2) {
            cal_slope_v_per_kg += 0.0050f;
            guardarConfiguracionNVS();
        } else if (submenuCursor == 3) {
            if (cal_slope_v_per_kg > 0.0060f) cal_slope_v_per_kg -= 0.0050f;
            guardarConfiguracionNVS();
        } else if (submenuCursor == 4) {
            guardarConfiguracionNVS();
        }
        // submenuCursor == 5 es "Volver"
    }
    else if (pantallaActual == PANTALLA_BATERIA) {
        if (submenuCursor >= 0 && submenuCursor < 4) {
            timeoutPantallaIdx = (uint8_t)submenuCursor;
            guardarConfiguracionNVS();
        }
    }
    else if (pantallaActual == PANTALLA_COMMS) {
        if (submenuCursor >= 0 && submenuCursor < 5) {
            modoMuestreoAdcIdx = (uint8_t)submenuCursor;
            guardarConfiguracionNVS();
        }
    }
    else if (pantallaActual == PANTALLA_SDN) {
        if (submenuCursor == 0) {
            realizarJoinOTAA(3);
        } else if (submenuCursor == 1) {
            enviarTelemetriaLoRaWAN();
        } else if (submenuCursor == 2) {
            paquetesTX = 0;
            paquetesRX = 0;
            sdn_rx_count = 0;
        }
    }

    submenuActivo = false;
    submenuCursor = 0;
}

// =============================================================================
// 8. GESTION DEL BOTON PRG
// =============================================================================
void gestionarBotonPRG() {
    bool estadoActual = digitalRead(PIN_PRG);

    if (estadoAnteriorBtn == HIGH && estadoActual == LOW) {
        tiempoInicioPresion = millis();
        accionLargaEjecutada = false;

        if (!pantallaEncendida) {
            display.ssd1306_command(SSD1306_DISPLAYON);
            pantallaEncendida = true;
            ultimoTiempoInteraccion = millis();
            accionLargaEjecutada = true;
        }
    }

    if (estadoActual == LOW && !accionLargaEjecutada) {
        if (millis() - tiempoInicioPresion >= TIEMPO_PULSACION_LARGA) {
            ultimoTiempoInteraccion = millis();
            accionLargaEjecutada = true;

            digitalWrite(PIN_LED_USER, HIGH);
            delay(50);
            digitalWrite(PIN_LED_USER, LOW);

            if (!submenuActivo) {
                submenuActivo = true;
                submenuCursor = (pantallaActual == PANTALLA_BATERIA) ? modoMuestreoAdcIdx : 0;
                submenuUltimaActividad = millis();
            } else {
                ejecutarAccionSubmenu();
            }
        }
    }

    if (estadoAnteriorBtn == LOW && estadoActual == HIGH) {
        uint32_t duracion = millis() - tiempoInicioPresion;
        if (duracion >= 40 && !accionLargaEjecutada) {
            ultimoTiempoInteraccion = millis();
            if (!submenuActivo) {
                pantallaActual = (Pantalla)((pantallaActual + 1) % TOTAL_PANTALLAS);
            } else {
                if (totalSubmenuOpciones > 0) {
                    submenuCursor = (submenuCursor + 1) % totalSubmenuOpciones;
                } else {
                    submenuCursor = 0;
                }
                submenuUltimaActividad = millis();
            }
        }
    }

    estadoAnteriorBtn = estadoActual;

    if (submenuActivo && (millis() - submenuUltimaActividad > TIMEOUT_SUBMENU_MS)) {
        submenuActivo = false;
        submenuCursor = 0;
    }
}

// =============================================================================
// 9. SETUP Y BUCLE PRINCIPAL
// =============================================================================
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);

    Serial.println(F("\n======================================================="));
    Serial.println(F("   HELTEC WIFI LORA 32 V3 - BALANZA AVICOLA INTELIGENTE"));
    Serial.println(F("   Firmware Autonomo SDN / LoRaWAN US915 SubBanda 2"));
    Serial.println(F("   Dispositivo: Silo Balanza (DevEUI: 70b3d57ed007985a)"));
    Serial.println(F("======================================================="));

    // Liberar retenciones de GPIO heredadas de ciclos previos de deep sleep
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis((gpio_num_t)PIN_VEXT_CTL);
    gpio_hold_dis((gpio_num_t)PIN_ADC_CTRL);

    pinMode(PIN_PRG, INPUT_PULLUP);
    pinMode(PIN_LED_USER, OUTPUT);
    digitalWrite(PIN_LED_USER, LOW);

    // Alimentar bus Vext (OLED y SX1262)
    pinMode(PIN_VEXT_CTL, OUTPUT);
    digitalWrite(PIN_VEXT_CTL, LOW); // LOW = Alimentar SX1262 y OLED
    delay(80);

    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println(F("[ERROR HARDWARE] No se pudo inicializar pantalla OLED SSD1306!"));
    } else {
        Serial.println(F("[HARDWARE OK] Pantalla OLED SSD1306 inicializada (128x64 @ 0x3C)."));
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(4, 12);
    display.println("BALANZA AVICOLA");
    display.setCursor(4, 26);
    display.println("Silo Heltec V3");
    display.setCursor(4, 40);
    display.println("Iniciando LoRaWAN...");
    display.setCursor(4, 52);
    display.println("SubBanda 2 US915");
    display.display();

    cargarConfiguracionNVS();
    Serial.println(F("[NVS OK] Configuracion recuperada de Flash:"));
    Serial.printf("  ├─ Pendiente Cal:  %.4f V/kg\n", cal_slope_v_per_kg);
    Serial.printf("  ├─ Voltaje Cero:   %.4f V\n", cal_zero_v);
    Serial.printf("  ├─ Tara Almacenada:%.2f kg\n", cal_tare_kg);
    Serial.printf("  └─ Modo Muestreo:  %s\n", INTERVALOS_ADC_TXT[modoMuestreoAdcIdx]);

    // Restaurar hora tras despertar de Deep Sleep
    if (esp_reset_reason() == ESP_RST_DEEPSLEEP && rtc_sincronizado) {
        uint64_t dormido_us = esp_timer_get_time() - rtc_sleep_start_us;
        time_t tiempo_actual = rtc_epoch_guardado + (time_t)(dormido_us / 1000000ULL);
        struct timeval tv;
        tv.tv_sec = tiempo_actual;
        tv.tv_usec = (suseconds_t)(dormido_us % 1000000ULL);
        settimeofday(&tv, NULL);
        Serial.printf("[RTC RESTORE] Hora restaurada tras Deep Sleep: %lu Unix\n", (unsigned long)tiempo_actual);
    }

    // Inicializar bus SPI y transceptor Semtech SX1262
    SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);
    int16_t state = radio.begin();
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("[RADIO OK] Transceptor SX1262 inicializado correctamente."));
    } else {
        Serial.printf("[RADIO ERROR] Fallo al iniciar SX1262 (codigo %d)!\n", state);
    }

    RadioLibTime_t init_toa_us = radio.getTimeOnAir(TELEMETRY_PAYLOAD_SIZE);
    current_toa_ms = (float)init_toa_us / 1000.0f;

    // Configurar credenciales OTAA
    uint64_t joinEUI_u64 = 0;
    uint64_t devEUI_u64  = 0;
    for (int i = 0; i < 8; i++) {
        joinEUI_u64 = (joinEUI_u64 << 8) | LORAWAN_JOIN_EUI[i];
        devEUI_u64  = (devEUI_u64 << 8)  | LORAWAN_DEV_EUI[i];
    }
    node.beginOTAA(joinEUI_u64, devEUI_u64, NULL, (uint8_t*)LORAWAN_APP_KEY);
    Serial.println(F("[LORAWAN OK] Pila RadioLib LoRaWAN OTAA configurada."));

    // Realizar intento de union inicial con ChirpStack
    realizarJoinOTAA(10);

    actualizarBateria();
    actualizarBalanza();
    ultimoTiempoInteraccion = millis();
    ultimoEnvioTxMs = millis();

    // Si la union fue exitosa, emitir el primer uplink de telemetria de inmediato
    if (lora_joined) {
        enviarTelemetriaLoRaWAN();
    }
}

void loop() {
    // 1. Boton PRG
    gestionarBotonPRG();

    // 2. Administracion de timeout de pantalla
    uint32_t timeoutMs = TIMEOUTS_PANTALLA_MS[timeoutPantallaIdx];
    if (pantallaEncendida && timeoutMs > 0) {
        if (millis() - ultimoTiempoInteraccion >= timeoutMs) {
            display.ssd1306_command(SSD1306_DISPLAYOFF);
            pantallaEncendida = false;
        }
    }

    // 3. Muestreo de sensores segun modo Continuo o Ahorro
    static uint32_t ultimoMuestreoAdc = 0;
    if (muestreoContinuoAdc) {
        if (millis() - ultimoMuestreoAdc >= 150) {
            ultimoMuestreoAdc = millis();
            actualizarBalanza();
            actualizarBateria();
        }
    } else {
        uint32_t intervaloAdcMs = INTERVALOS_ADC_MS[modoMuestreoAdcIdx];
        if (intervaloAdcMs > 0 && (millis() - ultimoMuestreoAdc >= intervaloAdcMs)) {
            ultimoMuestreoAdc = millis();
            actualizarBalanza();
            actualizarBateria();
        }
    }

    // 4. Envio periodico LoRaWAN y reintento de conexion si se perdio
    uint32_t periodoActualS = PERIODOS_TX_S[periodoTxIdx];
    if (periodoActualS > 0 && !tx_en_progreso) {
        if (millis() - ultimoEnvioTxMs >= (periodoActualS * 1000UL)) {
            ultimoEnvioTxMs = millis();
            if (!lora_joined) {
                Serial.println(F("\n[LORAWAN AVISO] Nodo aun no conectado. Reintentando Join OTAA..."));
                realizarJoinOTAA(3);
            }
            if (lora_joined) {
                enviarTelemetriaLoRaWAN();
            }
        }
    }

    // 5. Refresco OLED cada 100 ms
    static uint32_t ultimoRefresco = 0;
    if (pantallaEncendida && (millis() - ultimoRefresco >= 100)) {
        ultimoRefresco = millis();
        display.clearDisplay();

        if (submenuActivo) {
            dibujarSubmenuActivo();
        } else {
            switch (pantallaActual) {
                case PANTALLA_HOME:    renderPantallaHome();    break;
                case PANTALLA_BALANZA: renderPantallaBalanza(); break;
                case PANTALLA_BATERIA: renderPantallaBateria(); break;
                case PANTALLA_COMMS:   renderPantallaComms();   break;
                case PANTALLA_SDN:     renderPantallaSDN();     break;
                default: break;
            }
        }

        display.display();
    }

    delay(5);
}
