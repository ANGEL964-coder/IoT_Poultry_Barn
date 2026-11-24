/**
 * =============================================================================
 * SDN-LoRa Firmware Node - NODO CONTADOR DE HUEVOS AVÍCOLA
 * HELTEC WiFi LoRa 32 V3 (ESP32-S3 + SX1262) + Receptor MaixCAM YOLOv5 (UART)
 *
 * En este sistema, la CÁMARA MaixCAM Pro dicta el tiempo de transmisión:
 * cada 1 minuto (60 segundos) detecta los huevos con su modelo YOLOv5 y envía
 * inmediatamente el conteo por UART (115200 bps).
 *
 * El Heltec V3:
 *   1. Recibe la trama de conteo con verificación CRC-16-CCITT.
 *   2. Responde inmediatamente con ACK (0x06) a la MaixCAM.
 *   3. Muestra en la pantalla OLED la ilustración de unos huevos y la cantidad.
 *   4. Dispara de inmediato la transmisión LoRaWAN hacia ChirpStack / Servidor.
 *
 * Conexión física con el conector lateral de la MaixCAM:
 *   - MaixCAM Pin A16 (UART0_TX) -> Heltec V3 GPIO 4 (RX)
 *   - MaixCAM Pin A17 (UART0_RX) <- Heltec V3 GPIO 5 (TX) [OBLIGATORIO para enviar ACK 0x06]
 *   - MaixCAM GND                -> Heltec V3 GND
 *
 * Trama LoRaWAN SDN estándar (204 / 48 / 24 bytes):
 *   Bytes  0-23 : EggTelemetryHeader (seq, uptime, bateria, egg_count, eggs_per_min, cam_time, uart_status)
 *   Bytes 24-47 : EggCounterData (total_eggs, cam_hour, cam_minute, time_on_air, eggs_per_min, last_rx_ms, etc.)
 *   Bytes 48-203: Relleno determinístico SDN (modo FULL_TELEMETRY)
 * =============================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <U8g2lib.h>

#include "heltec_v3_config.h"
#include "sdn_header.h"

// -----------------------------------------------------------------------------
// Puerto Serie para MaixCAM (UART1 en ESP32-S3)
// -----------------------------------------------------------------------------
HardwareSerial SerialEgg(1);

// -----------------------------------------------------------------------------
// MODO STANDALONE / SIMULACION (SIN CAMARA UART FISICA)
// -----------------------------------------------------------------------------
// Permite que el nodo transmita periodicamente por LoRaWAN sin necesidad de la
// camara MaixCAM conectada. Puedes ajustar el conteo inicial y si se incrementa
// o se mantiene fijo.
// -----------------------------------------------------------------------------
#define MOCK_STANDALONE_MODE       true   // true = Transmitir valor fijado en codigo
#define MOCK_DEFAULT_EGG_COUNT     14     // Conteo de huevos determinado en codigo
#define MOCK_INCREMENT_PER_TX      1      // Incremento de huevos por cada envio (0 = valor fijo)
#define MOCK_SIMULATED_HOUR        10     // Hora simulada RTC (0-23)
#define MOCK_SIMULATED_MINUTE      30     // Minuto simulado RTC (0-59)

// Variables de estado del conteo y camara MaixCAM
#if MOCK_STANDALONE_MODE
static uint32_t total_egg_count        = MOCK_DEFAULT_EGG_COUNT;
static uint32_t last_tx_egg_count      = 0;
static uint8_t  cam_hour               = MOCK_SIMULATED_HOUR;
static uint8_t  cam_minute             = MOCK_SIMULATED_MINUTE;
static float    eggs_per_minute        = 0.93f;  // ~14 huevos / 15 min
static uint32_t last_uart_rx_ms        = 0;
static bool     uart_sensor_connected  = true;   // Activo para reflejar datos validos
static bool     pending_lora_tx        = false;
#else
static uint32_t total_egg_count        = 0;      // Conteo actual de huevos recibido
static uint32_t last_tx_egg_count      = 0;      // Conteo en la última TX LoRa
static uint8_t  cam_hour               = 0;      // Hora reportada por RTC MaixCAM
static uint8_t  cam_minute             = 0;      // Minuto reportado por RTC MaixCAM
static float    eggs_per_minute        = 0.0f;   // Tasa calculada
static uint32_t last_uart_rx_ms        = 0;      // Timestamp de última trama UART recibida
static bool     uart_sensor_connected  = false;  // Flag de enlace MaixCAM activo
static bool     pending_lora_tx        = false;  // Dispara uplink al recibir nuevo conteo
#endif

// -----------------------------------------------------------------------------
// Máquina de estados para recepción binaria MaixCAM (CRC-16 + ARQ)
// Trama: [0xAA, 0x55, 0x01, Hora(1B), Minuto(1B), Conteo(2B_BE), CRC_H, CRC_L]
// -----------------------------------------------------------------------------
enum MaixRxState {
    STATE_WAIT_SYNC_1,
    STATE_WAIT_SYNC_2,
    STATE_READ_TIPO,
    STATE_READ_PAYLOAD,
    STATE_READ_CRC
};

static MaixRxState rx_state             = STATE_WAIT_SYNC_1;
static uint8_t     rx_tipo              = 0;
static uint16_t    rx_payload_len       = 4; // [Hora, Minuto, Conteo_H, Conteo_L]
static uint16_t    rx_bytes_read        = 0;
static uint8_t     rx_payload_buf[16];
static uint8_t     rx_crc_bytes[2];
static uint8_t     rx_crc_bytes_read    = 0;
static uint32_t    rx_last_byte_ms      = 0;

// -----------------------------------------------------------------------------
// Trama de telemetría del Nodo de Huevos (Cabecera primaria de 24 bytes)
// -----------------------------------------------------------------------------
struct __attribute__((packed)) EggTelemetryHeader {
    uint32_t seq;
    uint32_t uptime_ms;
    float    battery_mv;
    uint32_t egg_count;        // Bytes 12-15: Conteo de huevos reportado por YOLOv5
    float    eggs_per_minute;  // Bytes 16-19: Tasa estimada (huevos/minuto)
    uint16_t cam_time;         // Bytes 20-21: (Hora << 8) | Minuto del RTC
    uint16_t uart_status;      // Bytes 22-23: Estado del enlace (1 = OK, 0 = Inactivo)
};
static_assert(sizeof(EggTelemetryHeader) == 24, "EggTelemetryHeader debe medir 24 bytes");

#define CRITICAL_PAYLOAD_SIZE 48     // Cabecera (24) + EggCounterData (24)

// -----------------------------------------------------------------------------
// Selección de Región LoRaWAN
// -----------------------------------------------------------------------------
#if defined(CFG_eu868)
  #define LORAWAN_BAND    EU868
#else
  #define LORAWAN_BAND    US915
#endif

// Transceptor Semtech SX1262 y Pila LoRaWAN
SX1262 radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY);
#if defined(CFG_eu868)
  LoRaWANNode node(&radio, &LORAWAN_BAND);
#else
  LoRaWANNode node(&radio, &LORAWAN_BAND, 2);   // US915, subbanda 2 (canales 8-15)
#endif

// -----------------------------------------------------------------------------
// Parámetros y estados SDN en tiempo de ejecución
// -----------------------------------------------------------------------------
static uint32_t current_tx_interval_s = DEFAULT_TX_INTERVAL_S;
static uint8_t  current_sf            = 7;      // SF7 (DR3 en US915)
static int8_t   current_tx_power      = 14;     // 14 dBm
static uint8_t  current_payload_mode  = PAYLOAD_MODE_FULL_TELEMETRY;
static bool     deep_sleep_enabled    = false;
static uint32_t uplink_sequence_count = 0;
static uint32_t last_tx_time_ms       = 0;

// Parámetros de conexión LoRaWAN
static uint8_t  lora_datarate         = 3;      // DR3 = SF7 BW125
static bool     lora_adr_enabled      = false;
static bool     lora_is_confirmed     = false;
static uint8_t  lora_fport            = LORAWAN_PORT_HUEVOS;  // Puerto 12 para el nodo de huevos
static bool     lora_joined           = false;
static uint32_t lora_dev_addr         = 0;
static float    lora_last_rssi        = 0.0f;
static float    lora_last_snr         = 0.0f;

// Métricas de transmisión
static float    current_toa_ms        = 0.0f;
static uint32_t last_tx_duration_ms   = 0;
static bool     tx_in_progress        = false;
static bool     last_tx_success       = false;
static int16_t  last_tx_code          = 0;

// Buffers
static uint8_t telemetry_buffer[TELEMETRY_PAYLOAD_SIZE];
static uint8_t rx_buffer[256];
static size_t  rx_len = 0;

// -----------------------------------------------------------------------------
// Pantalla OLED SSD1306 128x64 (Heltec V3: SDA=17, SCL=18, RST=21)
// -----------------------------------------------------------------------------
#ifndef CLIMA_OLED_SDA
  #define CLIMA_OLED_SDA 17
#endif
#ifndef CLIMA_OLED_SCL
  #define CLIMA_OLED_SCL 18
#endif
#ifndef CLIMA_OLED_RST
  #define CLIMA_OLED_RST 21
#endif

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, CLIMA_OLED_RST, CLIMA_OLED_SCL, CLIMA_OLED_SDA);

/**
 * -----------------------------------------------------------------------------
 * Imagen gráfica de huevos en nido (32x32 píxeles en formato XBM)
 * Ilustración nítida para pantallas OLED SSD1306
 * -----------------------------------------------------------------------------
 */
static const unsigned char epd_bitmap_eggs_32x32[] U8X8_PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00,
    0x00, 0x3E, 0x70, 0x00, 0x00, 0x7F, 0xF8, 0x00,
    0x80, 0xFF, 0xFD, 0x01, 0x80, 0xFD, 0xFF, 0x03,
    0xC0, 0xFD, 0xEF, 0x03, 0xC0, 0xFF, 0xEF, 0x07,
    0xC0, 0xFF, 0xFF, 0x07, 0xE0, 0xBF, 0xFF, 0x07,
    0xE0, 0x9F, 0xFF, 0x0F, 0xE0, 0xFF, 0xFF, 0x0F,
    0xE0, 0xFF, 0xFF, 0x0F, 0xE0, 0xFF, 0xFF, 0x0F,
    0xE0, 0xFF, 0xFF, 0x0F, 0xE0, 0xFF, 0xFF, 0x0F,
    0xC0, 0xFF, 0xFF, 0x0F, 0xC0, 0xFF, 0xFF, 0x07,
    0x80, 0xFF, 0xFF, 0x07, 0x00, 0xFE, 0xFF, 0x03,
    0x00, 0xFC, 0xFF, 0x00, 0x00, 0xFC, 0x7F, 0x00,
    0x00, 0xF8, 0x3F, 0x00, 0xB0, 0xED, 0xDF, 0x06,
    0xA0, 0xAA, 0xAB, 0x0A, 0xC0, 0xB6, 0x6D, 0x03,
    0x00, 0x55, 0x55, 0x01, 0x00, 0x00, 0x00, 0x00
};

// Actualiza pantalla OLED: Imagen de huevos + Cantidad en grande + Estado LoRa
void updateOled() {
    char buf[32];
    int w;

    u8g2.clearBuffer();

    // 1. Barra superior: Estado MaixCAM y hora
    u8g2.setFont(u8g2_font_5x7_tr);
#if MOCK_STANDALONE_MODE
    u8g2.drawStr(0, 7, "MODO: SIMULADO (1m)");
    snprintf(buf, sizeof(buf), "%02d:%02d", cam_hour, cam_minute);
    u8g2.drawStr(128 - u8g2.getStrWidth(buf), 7, buf);
#else
    if (uart_sensor_connected) {
        u8g2.drawStr(0, 7, "MAIXCAM: ONLINE (1m)");
        snprintf(buf, sizeof(buf), "%02d:%02d", cam_hour, cam_minute);
        u8g2.drawStr(128 - u8g2.getStrWidth(buf), 7, buf);
    } else {
        u8g2.drawStr(0, 7, "MAIXCAM: ESPERANDO...");
    }
#endif
    u8g2.drawHLine(0, 9, 128);

    // 2. Imagen gráfica de los huevos (32x32 píxeles) a la izquierda
    u8g2.drawXBMP(3, 14, 32, 32, epd_bitmap_eggs_32x32);

    // 3. Cantidad de huevos en tamaño grande a la derecha
    u8g2.setFont(u8g2_font_logisoso24_tr);
    snprintf(buf, sizeof(buf), "%u", (unsigned)total_egg_count);
    u8g2.drawStr(42, 38, buf);

    // Etiqueta "HUEVOS"
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(42, 49, "HUEVOS");

    // 4. Barra inferior: Estado LoRaWAN y secuencia
    u8g2.drawHLine(0, 52, 128);
    u8g2.setFont(u8g2_font_5x7_tr);
    const char* st = tx_in_progress ? "TX LORAWAN..."
                                    : (lora_joined ? "LORA: CONECTADO" : "UNIENDO...");
    u8g2.drawStr(0, 62, st);
    snprintf(buf, sizeof(buf), "#%u", (unsigned)uplink_sequence_count);
    u8g2.drawStr(128 - u8g2.getStrWidth(buf), 62, buf);

    u8g2.sendBuffer();
}

// Declaraciones de funciones
void powerOnHeltecPeripherals();
bool performOtaaJoin(int max_retries = 10);
void sendTelemetryUplink();
void processSdnDownlink(const SdnHeader* header, size_t len);
void executeSdnCommand(const SdnHeader* header);
void processUartData();
uint16_t calcular_crc16(const uint8_t* data, size_t len);

/**
 * -----------------------------------------------------------------------------
 * Heltec V3 Gestión de Energía (Control Vext)
 * -----------------------------------------------------------------------------
 */
void powerOnHeltecPeripherals() {
    pinMode(PIN_VEXT_CTL, OUTPUT);
    digitalWrite(PIN_VEXT_CTL, LOW); // LOW = Encender periféricos (SX1262 y OLED)
    delay(100);

    pinMode(PIN_LED_USER, OUTPUT);
    digitalWrite(PIN_LED_USER, LOW);
}

/**
 * -----------------------------------------------------------------------------
 * Cálculo de CRC-16-CCITT (Polinomio 0x1021, Inicial 0xFFFF)
 * Idéntico a la función calcular_crc16() en Python de la MaixCAM.
 * -----------------------------------------------------------------------------
 */
uint16_t calcular_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF;
            } else {
                crc = (crc << 1) & 0xFFFF;
            }
        }
    }
    return crc;
}

/**
 * -----------------------------------------------------------------------------
 * Máquina de Estados UART: Recepción de conteo con verificación CRC y ACK (0x06)
 * Trama esperada: [0xAA, 0x55, 0x01, Hora, Minuto, Conteo_H, Conteo_L, CRC_H, CRC_L]
 * -----------------------------------------------------------------------------
 */
void processUartData() {
    uint32_t now = millis();

    // Timeout entre bytes para reiniciar estado si se corta la trama
    if (rx_state != STATE_WAIT_SYNC_1 && (now - rx_last_byte_ms > 300)) {
        rx_state = STATE_WAIT_SYNC_1;
    }

    while (SerialEgg.available() > 0) {
        uint8_t b = (uint8_t)SerialEgg.read();
        rx_last_byte_ms = millis();

        switch (rx_state) {
            case STATE_WAIT_SYNC_1:
                if (b == MAIX_FRAME_SYNC_1) { // 0xAA
                    rx_state = STATE_WAIT_SYNC_2;
                }
                break;

            case STATE_WAIT_SYNC_2:
                if (b == MAIX_FRAME_SYNC_2) { // 0x55
                    rx_state = STATE_READ_TIPO;
                } else if (b != MAIX_FRAME_SYNC_1) {
                    rx_state = STATE_WAIT_SYNC_1;
                }
                break;

            case STATE_READ_TIPO:
                rx_tipo = b;
                rx_bytes_read = 0;
                if (rx_tipo == MAIX_TIPO_CONTEO) { // 0x01
                    rx_payload_len = 4; // [Hora(1B), Minuto(1B), Conteo(2B)]
                    rx_state = STATE_READ_PAYLOAD;
                } else {
                    rx_state = STATE_WAIT_SYNC_1;
                }
                break;

            case STATE_READ_PAYLOAD:
                if (rx_bytes_read < sizeof(rx_payload_buf)) {
                    rx_payload_buf[rx_bytes_read++] = b;
                }
                if (rx_bytes_read >= rx_payload_len) {
                    rx_crc_bytes_read = 0;
                    rx_state = STATE_READ_CRC;
                }
                break;

            case STATE_READ_CRC:
                rx_crc_bytes[rx_crc_bytes_read++] = b;
                if (rx_crc_bytes_read >= 2) {
                    uint16_t received_crc = ((uint16_t)rx_crc_bytes[0] << 8) | rx_crc_bytes[1];

                    // Calcular CRC sobre cuerpo = [Tipo] + Payload
                    uint16_t calc_crc = 0xFFFF;
                    calc_crc ^= ((uint16_t)rx_tipo << 8);
                    for (int bit = 0; bit < 8; bit++) {
                        if (calc_crc & 0x8000) calc_crc = ((calc_crc << 1) ^ 0x1021) & 0xFFFF;
                        else                   calc_crc = (calc_crc << 1) & 0xFFFF;
                    }
                    for (size_t i = 0; i < rx_payload_len; i++) {
                        calc_crc ^= ((uint16_t)rx_payload_buf[i] << 8);
                        for (int bit = 0; bit < 8; bit++) {
                            if (calc_crc & 0x8000) calc_crc = ((calc_crc << 1) ^ 0x1021) & 0xFFFF;
                            else                   calc_crc = (calc_crc << 1) & 0xFFFF;
                        }
                    }

                    if (calc_crc == received_crc) {
                        // Enviar ACK inmediato (0x06) a la MaixCAM
                        SerialEgg.write((uint8_t)MAIX_ACK_BYTE);
                        SerialEgg.flush();

                        // Extraer valores
                        cam_hour        = rx_payload_buf[0];
                        cam_minute      = rx_payload_buf[1];
                        total_egg_count = ((uint16_t)rx_payload_buf[2] << 8) | rx_payload_buf[3];

                        last_uart_rx_ms       = millis();
                        uart_sensor_connected = true;
                        pending_lora_tx       = true; // ¡Disparar transmisión LoRaWAN de inmediato!

                        Serial.printf("\n[MAIXCAM OK] %02d:%02d -> Conteo recibido: %u huevos (ACK 0x06 enviado)\n",
                                      cam_hour, cam_minute, total_egg_count);

                        updateOled();
                    } else {
                        // Enviar NACK (0x15) por fallo de CRC
                        SerialEgg.write((uint8_t)MAIX_NACK_BYTE);
                        SerialEgg.flush();
                        Serial.printf("[MAIXCAM ERROR] Fallo CRC (Calc: 0x%04X, Rec: 0x%04X)\n",
                                      calc_crc, received_crc);
                    }

                    rx_state = STATE_WAIT_SYNC_1;
                }
                break;
        }
    }

    // Timeout de inactividad de la cámara (> 2 minutos)
    if (!MOCK_STANDALONE_MODE && last_uart_rx_ms > 0 && (millis() - last_uart_rx_ms > EGG_UART_TIMEOUT_MS)) {
        if (uart_sensor_connected) {
            uart_sensor_connected = false;
            Serial.println(F("[UART AVISO] Timeout UART: MaixCAM sin reportar en >2 minutos."));
            updateOled();
        }
    }
}

/**
 * -----------------------------------------------------------------------------
 * Procedimiento de Unión OTAA con ChirpStack
 * -----------------------------------------------------------------------------
 */
bool performOtaaJoin(int max_retries) {
    Serial.println(F("[LORAWAN] Iniciando negociacion OTAA con ChirpStack..."));

    int attempt = 1;
    while (!lora_joined && (max_retries <= 0 || attempt <= max_retries)) {
        Serial.printf("[LORAWAN] Intento Join #%d (SubBanda 2 US915)...\n", attempt++);
        processUartData();
        updateOled();

        int16_t state = node.activateOTAA();
        if (state == RADIOLIB_LORAWAN_NEW_SESSION || state >= RADIOLIB_ERR_NONE) {
            Serial.println(F("[LORAWAN OK] UNION EXITOSA (JOINED) CON CHIRPSTACK"));
            node.setADR(lora_adr_enabled);
            node.setDatarate(lora_datarate);
            lora_joined = true;
            lora_dev_addr = node.getDevAddr();
            Serial.printf("[LORAWAN OK] DevAddr asignado: 0x%08X\n", (unsigned int)lora_dev_addr);
            return true;
        } else {
            Serial.printf("[LORAWAN ERROR] Join fallo con codigo: %d. Reintentando en 3s...\n", state);
            uint32_t wait_start = millis();
            while (millis() - wait_start < 3000) {
                processUartData();
                delay(10);
            }
        }
    }

    return lora_joined;
}

/**
 * -----------------------------------------------------------------------------
 * Transmisión de Telemetría por LoRaWAN (Tramas SDN de 204 / 48 / 24 bytes)
 * -----------------------------------------------------------------------------
 */
void sendTelemetryUplink() {
    tx_in_progress = true;
    digitalWrite(PIN_LED_USER, HIGH);

    // Si el nodo no tiene sesión, reintentar la unión OTAA
    if (!node.isActivated()) {
        Serial.println(F("[LORAWAN] Sesion inactiva. Intentando Join OTAA con ChirpStack..."));
        if (!performOtaaJoin(5)) {
            Serial.println(F("[LORAWAN ERROR] No se pudo unir a ChirpStack. Abortando transmision."));
            last_tx_success = false;
            last_tx_code = -1116;
            digitalWrite(PIN_LED_USER, LOW);
            tx_in_progress = false;
            last_tx_time_ms = millis();
            return;
        }
    }

#if MOCK_STANDALONE_MODE
    // Simular avance de reloj RTC MaixCAM
    cam_minute = (cam_minute + 1) % 60;
    if (cam_minute == 0) cam_hour = (cam_hour + 1) % 24;
#endif

    last_tx_egg_count = total_egg_count;

    // 1. Armar trama exclusiva de huevos (24 bytes, sin campos de clima ni ceros)
    EggNodeTelemetry telem;
    telem.seq              = ++uplink_sequence_count;
    telem.uptime_ms        = millis();
    telem.battery_mv       = 3300.0f - (uplink_sequence_count * 0.1f);
    telem.egg_count        = total_egg_count;
    telem.cam_hour         = cam_hour;
    telem.cam_minute       = cam_minute;
    telem.uart_status      = uart_sensor_connected ? 1 : 0;
    telem.total_detections = uplink_sequence_count;

#if MOCK_STANDALONE_MODE
    #if MOCK_INCREMENT_PER_TX > 0
    total_egg_count += MOCK_INCREMENT_PER_TX;
    #endif
#endif

    size_t actual_payload_size = sizeof(EggNodeTelemetry); // 24 bytes exactos

    // Tiempo en el aire (ToA)
    RadioLibTime_t toa_us = radio.getTimeOnAir(actual_payload_size);
    current_toa_ms = (float)toa_us / 1000.0f;

    Serial.println(F("\n-------------------------------------------------------"));
    Serial.printf("[DIAGNOSTICO TX LORAWAN] Uplink #%u en Puerto %d\n", uplink_sequence_count, lora_fport);
    Serial.printf("  ├─ Tipo Nodo:         CONTADOR DE HUEVOS (TRAMA EXCLUSIVA)\n");
    Serial.printf("  ├─ Conteo Total:      %u huevos\n", total_egg_count);
    Serial.printf("  ├─ Hora Captura:      %02d:%02d (RTC BM8563)\n", cam_hour, cam_minute);
    Serial.printf("  ├─ Tamano Trama:      %d Bytes (Sin variables de clima ni ceros)\n", (int)actual_payload_size);
    Serial.printf("  ├─ Modulacion:        DR%u (SF%u) @ %d dBm\n", lora_datarate, current_sf, current_tx_power);
    Serial.printf("  └─ ToA Estimado:      %.1f ms\n", current_toa_ms);
    Serial.println(F("-------------------------------------------------------"));

    updateOled();

    rx_len = 0;
    uint32_t start_tx_ms = millis();

    node.setADR(false);
    node.setDatarate(lora_datarate);

    // sendReceive transmite los 24 bytes y escucha en RX1 y RX2
    int16_t state = node.sendReceive(
        reinterpret_cast<uint8_t*>(&telem),
        actual_payload_size,
        lora_fport,
        rx_buffer,
        &rx_len,
        lora_is_confirmed
    );

    last_tx_duration_ms = millis() - start_tx_ms;
    last_tx_code = state;
    digitalWrite(PIN_LED_USER, LOW);

    lora_dev_addr = node.getDevAddr();
    lora_last_rssi = radio.getRSSI();
    lora_last_snr = radio.getSNR();

    if (state > 0) {
        last_tx_success = true;
        Serial.printf("[TX/RX DIAGNOSTICO] Transmision EXITOSA en DR%u (SF%u). Downlink recibido en RX%d! (Bytes: %d)\n",
                      lora_datarate, current_sf, state, (int)rx_len);
        if (rx_len >= sizeof(SdnHeader)) {
            const SdnHeader* header = reinterpret_cast<const SdnHeader*>(rx_buffer);
            processSdnDownlink(header, rx_len);
        }
    } else if (state == RADIOLIB_ERR_NONE) {
        last_tx_success = true;
        Serial.printf("[TX DIAGNOSTICO] Transmision EXITOSA en DR%u (SF%u). Ventanas RX cerradas.\n",
                      lora_datarate, current_sf);
        Serial.printf("   - Duracion Ventana: %u ms | RSSI: %.1f dBm | SNR: %.1f dB\n",
                      last_tx_duration_ms, lora_last_rssi, lora_last_snr);
    } else {
        last_tx_success = false;
        Serial.printf("[TX DIAGNOSTICO ERROR] Transaccion fallo con codigo RadioLib: %d\n", state);
    }

    last_tx_time_ms = millis();
    tx_in_progress = false;
    updateOled();
}

/**
 * -----------------------------------------------------------------------------
 * Procesamiento y Deserialización de Downlink SDN
 * -----------------------------------------------------------------------------
 */
void processSdnDownlink(const SdnHeader* header, size_t len) {
    if (!header) return;

    uint32_t raw_word = *reinterpret_cast<const uint32_t*>(header);
    const uint8_t* raw_bytes = reinterpret_cast<const uint8_t*>(header);

    Serial.println(F("\n======================================================="));
    Serial.println(F("[SDN] DOWNLINK RECIBIDO DESDE CHIRPSTACK"));
    Serial.println(F("======================================================="));
    Serial.printf("  Raw Data:       0x%08X (Bytes: 0x%02X 0x%02X 0x%02X 0x%02X)\n",
                  raw_word, raw_bytes[0], raw_bytes[1], raw_bytes[2], raw_bytes[3]);
    Serial.printf("  Comando:        0x%02X (%s)\n", header->cmd_id, getSdnCommandName(header->cmd_id));
    Serial.printf("  SF Objetivo:    %u -> SF%u\n", header->sf_target, getSpreadingFactorValue(header->sf_target));
    Serial.printf("  Potencia TX:    %u dBm\n", header->tx_power);
    Serial.printf("  Intervalo TX:   %u -> %u segundos\n", header->sample_rate, getSampleRateSeconds(header->sample_rate));
    Serial.printf("  Modo Payload:   %u -> %s\n", header->payload_mode, getPayloadModeName(header->payload_mode));
    Serial.printf("  Param Value:    %u (0x%04X)\n", header->param_value, header->param_value);
    Serial.println(F("-------------------------------------------------------"));

    executeSdnCommand(header);
}

/**
 * -----------------------------------------------------------------------------
 * Máquina de Estados SDN (Ejecución de Reglas de Control)
 * -----------------------------------------------------------------------------
 */
void executeSdnCommand(const SdnHeader* header) {
    switch (header->cmd_id) {
        case CMD_NOP_KEEP_ALIVE:
            Serial.println(F("[SDN ACTION] Keep-Alive confirmado."));
            break;

        case CMD_RECONFIG_RADIO: {
            uint8_t new_sf = getSpreadingFactorValue(header->sf_target);
            uint32_t new_rate = getSampleRateSeconds(header->sample_rate);
            int8_t new_power = static_cast<int8_t>(header->tx_power);

            Serial.println(F("\n======================================================="));
            Serial.println(F("[SDN DIAGNOSTICO] EJECUTANDO CMD_RECONFIG_RADIO"));
            Serial.println(F("======================================================="));
            Serial.printf("  ├─ SF Solicitado:     %u (header->sf_target: %u -> SF%u)\n",
                          new_sf, header->sf_target, new_sf);
            Serial.printf("  ├─ Potencia TX:       %d dBm\n", new_power);
            Serial.printf("  ├─ Intervalo TX:      %u s -> %u s\n", current_tx_interval_s, new_rate);
            Serial.printf("  └─ Modo de Payload:   %u (%s)\n", header->payload_mode, getPayloadModeName(header->payload_mode));
            Serial.println(F("-------------------------------------------------------"));

            current_sf = new_sf;
            current_tx_power = new_power;
            current_tx_interval_s = new_rate;
            current_payload_mode = header->payload_mode;
            deep_sleep_enabled = (header->sleep_enable == 1);

            if (current_sf == 7) {
                lora_datarate = 3;
            } else if (current_sf == 8) {
                lora_datarate = 2;
            } else if (current_sf == 9) {
                lora_datarate = 1;
            } else if (current_sf >= 10) {
                lora_datarate = 0;
            }

            if (lora_datarate < 3 && current_payload_mode == PAYLOAD_MODE_FULL_TELEMETRY) {
                Serial.println(F("[SDN ADAPTACION] Payload 204B excede limite US915 DR2/SF8 (125B)."));
                Serial.println(F("   -> Conmutando a modo CRITICAL (48B) para emitir en SF8."));
                current_payload_mode = PAYLOAD_MODE_CRITICAL;
            }

            node.setADR(false);
            int16_t dr_res = node.setDatarate(lora_datarate);
            if (dr_res == RADIOLIB_ERR_NONE) {
                Serial.printf("[DIAGNOSTICO OK] RadioLib configuro DR%u (SF%u) exitosamente.\n", lora_datarate, current_sf);
            } else {
                Serial.printf("[DIAGNOSTICO ERROR] RadioLib fallo al asignar DR%u con codigo: %d\n", lora_datarate, dr_res);
            }
            Serial.println(F("=======================================================\n"));
            break;
        }

        case CMD_SET_TEMP_THRESHOLD: {
            uint16_t egg_thresh = header->param_value;
            Serial.printf("[SDN ACTION] SET_THRESHOLD: Umbral de huevos ajustado a %u\n", egg_thresh);
            break;
        }

        case CMD_FORCE_SLEEP: {
            uint16_t sleep_minutes = header->param_value;
            Serial.printf("[SDN ACTION] FORCE_SLEEP: Deep Sleep por %u minutos\n", sleep_minutes);
            if (sleep_minutes > 0) {
                digitalWrite(PIN_VEXT_CTL, HIGH);
                uint64_t sleep_us = static_cast<uint64_t>(sleep_minutes) * 60ULL * 1000000ULL;
                esp_sleep_enable_timer_wakeup(sleep_us);
                Serial.flush();
                esp_deep_sleep_start();
            }
            break;
        }

        case CMD_SYSTEM_RESET: {
            Serial.println(F("[SDN ACTION] SYSTEM_RESET: Reiniciando nodo ESP32-S3..."));
            Serial.flush();
            delay(1000);
            ESP.restart();
            break;
        }

        default:
            Serial.printf("[SDN WARNING] Comando no implementado: 0x%02X\n", header->cmd_id);
            break;
    }
}

/**
 * -----------------------------------------------------------------------------
 * Setup Principal
 * -----------------------------------------------------------------------------
 */
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);

    Serial.println(F("\n======================================================="));
    Serial.println(F("   NODO CONTADOR DE HUEVOS (MAIXCAM YOLOv5) - HELTEC V3"));
    Serial.println(F("   ESP32-S3 + SX1262 LoRaWAN + UART1 (115200) + SDN"));
    Serial.println(F("   Cadencia dictada por la camara: cada 1 minuto"));
    Serial.println(F("======================================================="));

    // 1. Encender periféricos (Alimentación SX1262 y OLED por Vext)
    powerOnHeltecPeripherals();

    // 2. Inicializar UART1 a 115200 baudios para MaixCAM
    SerialEgg.begin(EGG_UART_BAUDRATE, SERIAL_8N1, PIN_EGG_UART_RX, PIN_EGG_UART_TX);
    Serial.printf("[UART] MaixCAM configurada en RX=GPIO %d, TX=GPIO %d a %d bps (ACK activo)\n",
                  PIN_EGG_UART_RX, PIN_EGG_UART_TX, EGG_UART_BAUDRATE);
#if MOCK_STANDALONE_MODE
    Serial.println(F("[MODO STANDALONE] SIMULACION ACTIVA (Sin camara UART obligatoria)"));
    Serial.printf("  -> Conteo inicial: %u huevos | Incremento por TX: +%d | RTC: %02d:%02d\n",
                  total_egg_count, MOCK_INCREMENT_PER_TX, cam_hour, cam_minute);
#endif

    // 3. Pantalla OLED
    u8g2.begin();
    updateOled();

    // 4. Bus SPI para SX1262
    SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);

    // 5. Inicializar transceptor LoRa SX1262
    Serial.print(F("[RADIO] Inicializando transceptor SX1262... "));
    int16_t state = radio.begin();
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("EXITO"));
    } else {
        Serial.printf("FALLO (codigo %d). Verifique conexiones hardware.\n", state);
    }

    // 6. Credenciales LoRaWAN OTAA
    uint64_t joinEUI_u64 = 0;
    uint64_t devEUI_u64  = 0;
    for (int i = 0; i < 8; i++) {
        joinEUI_u64 = (joinEUI_u64 << 8) | LORAWAN_JOIN_EUI[i];
        devEUI_u64  = (devEUI_u64 << 8)  | LORAWAN_DEV_EUI[i];
    }

    state = node.beginOTAA(joinEUI_u64, devEUI_u64, NULL, (uint8_t*)LORAWAN_APP_KEY);
    Serial.println(F("[LORAWAN] Credenciales OTAA cargadas en RadioLib."));

    // 7. Bucle continuo de Join con ChirpStack (reintenta indefinidamente hasta conectar)
    Serial.println(F("[LORAWAN] Iniciando ciclo de Join con ChirpStack (reintentando hasta conectar)..."));
    int attempt = 1;
    while (!lora_joined) {
        processUartData();
        updateOled();
        Serial.printf("[LORAWAN] Intento Join #%d (SubBanda 2 US915 canales 8-15)...\n", attempt++);
        state = node.activateOTAA();

        if (state == RADIOLIB_LORAWAN_NEW_SESSION || state >= RADIOLIB_ERR_NONE) {
            Serial.println(F("[LORAWAN OK] UNION EXITOSA (JOINED) CON CHIRPSTACK"));
            node.setADR(false);
            node.setDatarate(3);
            lora_joined = true;
            lora_dev_addr = node.getDevAddr();
            Serial.printf("[LORAWAN OK] DevAddr: 0x%08X | Datarate fijado en DR3 (SF7)\n", (unsigned int)lora_dev_addr);
            break;
        } else {
            Serial.printf("[LORAWAN ERROR] Join fallo con codigo: %d (Reintentando en 3s...)\n", state);
            uint32_t wait_start = millis();
            while (millis() - wait_start < 3000) {
                processUartData();
                delay(10);
            }
        }
    }

#if MOCK_STANDALONE_MODE
    Serial.println(F("\n[SISTEMA LISTO] Conectado a ChirpStack. Modo simulado: transmitiendo telemetria de huevos...\n"));
#else
    Serial.println(F("\n[SISTEMA LISTO] Conectado a ChirpStack. Esperando reporte de MaixCAM...\n"));
#endif
    sendTelemetryUplink();
}

/**
 * -----------------------------------------------------------------------------
 * Bucle Principal (no bloqueante)
 * -----------------------------------------------------------------------------
 */
void loop() {
    uint32_t now = millis();

    // 1. Lectura continua de la trama de conteo enviada por la MaixCAM
    processUartData();

    // 2. Refresco de pantalla OLED cada 500 ms
    static uint32_t last_oled_refresh_ms = 0;
    if (!tx_in_progress && (now - last_oled_refresh_ms >= 500)) {
        last_oled_refresh_ms = now;
        updateOled();
    }

    // 3. Verificación de transmisión LoRaWAN:
    //    a) Disparada INMEDIATAMENTE por la cámara cada minuto al recibir nuevo conteo (pending_lora_tx)
    //    b) O temporizador de respaldo periódico si no hay tramas UART
    bool timer_triggered = (lora_joined && (now - last_tx_time_ms >= (current_tx_interval_s * 1000UL)));

    if ((pending_lora_tx || timer_triggered) && !tx_in_progress) {
        pending_lora_tx = false;
        sendTelemetryUplink();
    }

    delay(2);
}
