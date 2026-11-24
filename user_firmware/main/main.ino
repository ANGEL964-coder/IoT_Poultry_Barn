/**
 * =============================================================================
 * SDN-LoRa Firmware Node - HELTEC WiFi LoRa 32 V3 (ESP32-S3 + SX1262)
 * Proyecto: Balanza de Alimento para Galpones Avicolas
 * 
 * Funcionalidades y Control de Red:
 * - Acondicionador de celda de carga en ADC0 (GPIO 1 = ADC1_CH0, compatible con WiFi)
 * - Interfaz Web local (Punto de Acceso) con esquematico SVG CAD en tiempo real
 * - Calibracion en caliente: Cero y Span automatico + Pendiente V/kg manual
 * - Visualizacion tecnica en pantalla OLED integrada SSD1306 y Web (sin emojis)
 * - Enlace LoRaWAN avanzado: Datarate dinamico (DR3/SF7), ADR, Confirmed/Unconfirmed
 * - Cancelacion de transmision manual y por timeout con restablecimiento de boton
 * - Medicion y visualizacion precisa del Tiempo de Dato en el Aire (ToA) y ventana total
 * - Protocolo SDN estricto de 204 Bytes y deserializador de downlinks en RX1/RX2
 * =============================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <RadioLib.h>

#include "heltec_v3_config.h"
#include "sdn_header.h"
#include "scale_driver.h"
#include "web_interface.h"

// Seleccion de Region LoRaWAN
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

// Controlador de Balanza, ADC y Pantalla OLED
ScaleDriver scale;

// Servidor Web HTTP en puerto 80
WebServer server(WEB_SERVER_PORT);

// Parametros y Estados de SDN en tiempo de ejecucion
static uint32_t current_tx_interval_s = DEFAULT_TX_INTERVAL_S;
static uint8_t  current_sf            = 7;      // Default SF7 (DR3 en US915)
static int8_t   current_tx_power      = 14;     // Default 14 dBm
static uint8_t  current_payload_mode  = PAYLOAD_MODE_FULL_TELEMETRY;
static bool     deep_sleep_enabled    = false;
static float    temp_alert_threshold  = 30.0f;
static uint32_t uplink_sequence_count = 0;
static uint32_t last_tx_time_ms       = 0;

// Parametros de Conexion y Control LoRaWAN
static uint8_t  lora_datarate         = 3;      // DR3 = SF7 BW125 (Por defecto)
static bool     lora_adr_enabled      = false;  // ADR desactivado inicialmente
static bool     lora_is_confirmed     = false;  // Unconfirmed por defecto
static uint8_t  lora_fport            = LORAWAN_APP_PORT;
static bool     lora_joined           = false;
static uint32_t lora_dev_addr         = 0;
static float    lora_last_rssi        = 0.0f;
static float    lora_last_snr         = 0.0f;

// Metricas de Tiempo de Transmision y Time-on-Air (ToA)
static float    current_toa_ms        = 0.0f;
static uint32_t last_tx_duration_ms   = 0;
static bool     tx_in_progress        = false;
static uint32_t tx_start_timestamp_ms = 0;
static bool     manual_tx_requested   = false;
static bool     rejoin_requested      = false;
static bool     last_tx_success       = false;
static int16_t  last_tx_code          = 0;
static String   last_sdn_cmd_name     = "NINGUNO";

// Buffers de Datos LoRaWAN
static uint8_t telemetry_buffer[TELEMETRY_PAYLOAD_SIZE];
static uint8_t rx_buffer[256];
static size_t  rx_len = 0;

// Declaraciones de funciones
void powerOnHeltecPeripherals();
void setupWebServer();
bool performOtaaJoin(int max_retries = 10);
void sendTelemetryUplink();
void cancelActiveTransmission(const char* reason);
void processSdnDownlink(const SdnHeader* header, size_t len);
void executeSdnCommand(const SdnHeader* header);

/**
 * -----------------------------------------------------------------------------
 * Heltec V3 Gestion de Energia (Control Vext)
 * -----------------------------------------------------------------------------
 */
void powerOnHeltecPeripherals() {
    pinMode(PIN_VEXT_CTL, OUTPUT);
    digitalWrite(PIN_VEXT_CTL, LOW); // LOW = Encender perifericos (SX1262 y OLED)
    delay(100);

    pinMode(PIN_LED_USER, OUTPUT);
    digitalWrite(PIN_LED_USER, LOW);
}

/**
 * -----------------------------------------------------------------------------
 * Procedimiento de Union OTAA con ChirpStack
 * -----------------------------------------------------------------------------
 */
bool performOtaaJoin(int max_retries) {
    Serial.println(F("[LORAWAN] Iniciando negociacion OTAA con ChirpStack..."));
    
    for (int attempt = 1; attempt <= max_retries; attempt++) {
        Serial.printf("[LORAWAN] Intento Join #%d de %d (SubBanda 2 US915)...\n", attempt, max_retries);
        scale.updateOled(current_toa_ms, "UNINEDO...", false);
        server.handleClient();

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
            if (attempt < max_retries) {
                uint32_t t_wait = millis();
                while (millis() - t_wait < 3000) {
                    server.handleClient();
                    scale.updateReadings();
                    delay(10);
                }
            }
        }
    }

    lora_joined = false;
    Serial.println(F("[LORAWAN AVISO] Limite de intentos Join alcanzado. Reintente desde la Web."));
    return false;
}

/**
 * -----------------------------------------------------------------------------
 * Cancelacion de Transmision y Restablecimiento de Estados
 * -----------------------------------------------------------------------------
 */
void cancelActiveTransmission(const char* reason) {
    manual_tx_requested = false;
    tx_in_progress = false;
    digitalWrite(PIN_LED_USER, LOW);
    Serial.printf("[TX CANCEL] %s\n", reason ? reason : "Transmision cancelada.");
}

/**
 * -----------------------------------------------------------------------------
 * Configuracion de Rutas de la Interfaz Web y API REST
 * -----------------------------------------------------------------------------
 */
void setupWebServer() {
    // 1. Pagina Principal (Dashboard Tecnico con Balanza SVG)
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", PAGE_INDEX);
    });

    // 2. API de Estado en Tiempo Real (JSON)
    server.on("/api/status", HTTP_GET, []() {
        char devAddrHex[12];
        snprintf(devAddrHex, sizeof(devAddrHex), "0x%08X", (unsigned int)lora_dev_addr);

        String json = "{";
        json += "\"weight_kg\":" + String(scale.live_net_kg, 2) + ",";
        json += "\"gross_kg\":" + String(scale.live_gross_kg, 2) + ",";
        json += "\"voltage_v\":" + String(scale.live_voltage_v, 4) + ",";
        json += "\"raw_adc\":" + String(scale.live_adc_raw) + ",";
        json += "\"hopper_pct\":" + String(scale.live_hopper_pct, 1) + ",";
        json += "\"zero_v\":" + String(scale.cal_zero_v, 4) + ",";
        json += "\"slope_v_kg\":" + String(scale.cal_slope_v_per_kg, 5) + ",";
        json += "\"tare_kg\":" + String(scale.cal_tare_kg, 2) + ",";
        json += "\"toa_ms\":" + String(current_toa_ms, 1) + ",";
        json += "\"tx_duration_ms\":" + String(last_tx_duration_ms) + ",";
        json += "\"tx_in_progress\":" + String(tx_in_progress ? "true" : "false") + ",";
        json += "\"last_tx_success\":" + String(last_tx_success ? "true" : "false") + ",";
        json += "\"last_tx_code\":" + String(last_tx_code) + ",";
        json += "\"uplink_seq\":" + String(uplink_sequence_count) + ",";
        json += "\"datarate\":" + String(lora_datarate) + ",";
        json += "\"sf\":" + String(current_sf) + ",";
        json += "\"tx_power\":" + String(current_tx_power) + ",";
        json += "\"adr\":" + String(lora_adr_enabled ? "true" : "false") + ",";
        json += "\"confirmed\":" + String(lora_is_confirmed ? "true" : "false") + ",";
        json += "\"fport\":" + String(lora_fport) + ",";
        json += "\"rssi\":" + String(lora_last_rssi, 1) + ",";
        json += "\"snr\":" + String(lora_last_snr, 1) + ",";
        json += "\"dev_addr\":\"" + String(devAddrHex) + "\",";
        json += "\"lora_joined\":" + String(lora_joined ? "true" : "false") + ",";
        json += "\"last_sdn_cmd\":\"" + last_sdn_cmd_name + "\"";
        json += "}";
        server.send(200, "application/json", json);
    });

    // 3. Disparo Manual de Transmision LoRaWAN
    server.on("/api/send_lora", HTTP_POST, []() {
        if (tx_in_progress) {
            server.send(409, "application/json", "{\"success\":false,\"message\":\"Transmision en curso\"}");
            return;
        }
        manual_tx_requested = true;
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Transmision solicitada\"}");
    });

    // 4. Cancelacion de Transmision y Restablecimiento de Boton
    server.on("/api/lora/cancel", HTTP_POST, []() {
        cancelActiveTransmission("Cancelado por peticion web del usuario");
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Transmision cancelada y boton restablecido\"}");
    });

    // 5. Reconexion / Rejoin OTAA bajo demanda
    server.on("/api/lora/rejoin", HTTP_POST, []() {
        rejoin_requested = true;
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Secuencia Rejoin iniciada\"}");
    });

    // 6. Configuracion de Parametros LoRaWAN (Datarate, ADR, Confirmed)
    server.on("/api/lora/config", HTTP_POST, []() {
        if (server.hasArg("dr")) {
            uint8_t new_dr = server.arg("dr").toInt();
            if (new_dr <= 4) {
                lora_datarate = new_dr;
                node.setDatarate(lora_datarate);
                current_sf = (new_dr == 3) ? 7 : ((new_dr == 2) ? 8 : ((new_dr == 1) ? 9 : 10));
                Serial.printf("[LORAWAN CONFIG] Datarate actualizado a DR%u (SF%u)\n", lora_datarate, current_sf);
            }
        }
        if (server.hasArg("adr")) {
            lora_adr_enabled = (server.arg("adr").toInt() == 1);
            node.setADR(lora_adr_enabled);
            Serial.printf("[LORAWAN CONFIG] ADR actualizado: %s\n", lora_adr_enabled ? "ACTIVO" : "INACTIVO");
        }
        if (server.hasArg("confirmed")) {
            lora_is_confirmed = (server.arg("confirmed").toInt() == 1);
            Serial.printf("[LORAWAN CONFIG] Tipo Uplink: %s\n", lora_is_confirmed ? "CONFIRMADO (ACK)" : "NO CONFIRMADO");
        }
        if (server.hasArg("fport")) {
            uint8_t p = server.arg("fport").toInt();
            if (p >= 1 && p <= 223) lora_fport = p;
        }
        server.send(200, "application/json", "{\"success\":true}");
    });

    // 7. Calibracion: Cero Automatico (Zero)
    server.on("/api/cal_zero", HTTP_POST, []() {
        scale.calibrateZero();
        server.send(200, "application/json", "{\"success\":true,\"zero_v\":" + String(scale.cal_zero_v, 4) + "}");
    });

    // 8. Calibracion: Span Automatico (con peso patron conocido)
    server.on("/api/cal_span", HTTP_POST, []() {
        if (!server.hasArg("weight")) {
            server.send(400, "application/json", "{\"success\":false,\"message\":\"Falta argumento weight\"}");
            return;
        }
        float spanWeight = server.arg("weight").toFloat();
        bool ok = scale.calibrateSpan(spanWeight);
        if (ok) {
            server.send(200, "application/json", "{\"success\":true,\"slope_v_kg\":" + String(scale.cal_slope_v_per_kg, 5) + "}");
        } else {
            server.send(400, "application/json", "{\"success\":false,\"message\":\"Delta de voltaje insuficiente para calibrar span\"}");
        }
    });

    // 9. Calibracion Manual: Pendiente (V/kg) o Cero (V)
    server.on("/api/cal_manual", HTTP_POST, []() {
        if (server.hasArg("slope")) {
            float slope = server.arg("slope").toFloat();
            scale.setManualSlope(slope);
        }
        if (server.hasArg("zero")) {
            float zero = server.arg("zero").toFloat();
            scale.setManualZero(zero);
        }
        server.send(200, "application/json", "{\"success\":true,\"slope_v_kg\":" + String(scale.cal_slope_v_per_kg, 5) + ",\"zero_v\":" + String(scale.cal_zero_v, 4) + "}");
    });

    // 10. Tara
    server.on("/api/tare", HTTP_POST, []() {
        scale.setTare();
        server.send(200, "application/json", "{\"success\":true,\"tare_kg\":" + String(scale.cal_tare_kg, 2) + "}");
    });

    // 11. Limpiar Tara
    server.on("/api/tare_clear", HTTP_POST, []() {
        scale.clearTare();
        server.send(200, "application/json", "{\"success\":true,\"tare_kg\":0.0}");
    });

    // 12. Guardar Calibracion en Memoria Flash (NVS)
    server.on("/api/cal_save", HTTP_POST, []() {
        scale.saveCalibration();
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Guardado en NVS\"}");
    });

    // 13. Reset a Valores de Fabrica
    server.on("/api/cal_reset", HTTP_POST, []() {
        scale.resetDefaults();
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Valores reiniciados\"}");
    });

    server.begin();
    Serial.println(F("[HTTP] Servidor Web de la Balanza Avicola iniciado en puerto 80"));
}

/**
 * -----------------------------------------------------------------------------
 * Transmision de Telemetria por LoRaWAN (204 Bytes SDN Compliant)
 * -----------------------------------------------------------------------------
 */
void sendTelemetryUplink() {
    tx_in_progress = true;
    tx_start_timestamp_ms = millis();
    digitalWrite(PIN_LED_USER, HIGH);

    // Si el nodo no se ha unido aun, reintentar la union OTAA
    if (!node.isActivated()) {
        Serial.println(F("[LORAWAN] Sesion inactiva. Intentando Join OTAA con ChirpStack..."));
        if (!performOtaaJoin(5)) {
            Serial.println(F("[LORAWAN ERROR] No se pudo unir a ChirpStack. Abortando transmision."));
            last_tx_success = false;
            last_tx_code = -1116;
            digitalWrite(PIN_LED_USER, LOW);
            tx_in_progress = false;
            return;
        }
    }

    // Calcular Tiempo de Dato en el Aire (ToA) fisico del transceptor SX1262
    RadioLibTime_t toa_us = radio.getTimeOnAir(TELEMETRY_PAYLOAD_SIZE);
    current_toa_ms = (float)toa_us / 1000.0f;

    // Limpiar buffer de telemetria (204 bytes exactos requeridos por SDN)
    memset(telemetry_buffer, 0, sizeof(telemetry_buffer));

    // 1. Cabecera Estandar SDN Telemetry (Bytes 0 - 23)
    TelemetryHeader* telem = reinterpret_cast<TelemetryHeader*>(telemetry_buffer);
    telem->seq         = ++uplink_sequence_count;
    telem->uptime_ms   = millis();
    telem->battery_mv  = 3300.0f - (uplink_sequence_count * 0.1f);
    telem->temperature = 24.5f + ((uplink_sequence_count % 10) * 0.25f);
    telem->humidity    = 58.0f;
    telem->pressure    = 1013.25f;

    // 2. Datos Especificos de la Balanza Avicola (Bytes 24 - 47)
    PoultryScaleData* scaleData = reinterpret_cast<PoultryScaleData*>(telemetry_buffer + sizeof(TelemetryHeader));
    scaleData->weight_kg       = scale.live_net_kg;
    scaleData->gross_weight_kg = scale.live_gross_kg;
    scaleData->voltage_v       = scale.live_voltage_v;
    scaleData->adc_raw         = scale.live_adc_raw;
    scaleData->cal_slope       = scale.cal_slope_v_per_kg;
    scaleData->cal_zero        = scale.cal_zero_v;
    scaleData->time_on_air_ms  = (uint16_t)current_toa_ms;

    // 3. Relleno deterministico hasta 204 bytes SDN si es FULL_TELEMETRY
    for (size_t i = sizeof(TelemetryHeader) + sizeof(PoultryScaleData); i < TELEMETRY_PAYLOAD_SIZE; ++i) {
        telemetry_buffer[i] = static_cast<uint8_t>((i + uplink_sequence_count) & 0xFF);
    }

    // Determinar tamano real del payload segun el modo SDN
    size_t actual_payload_size = TELEMETRY_PAYLOAD_SIZE; // 204 bytes por defecto (FULL)
    if (current_payload_mode == PAYLOAD_MODE_CRITICAL) {
        actual_payload_size = sizeof(TelemetryHeader) + sizeof(PoultryScaleData); // 48 bytes
    } else if (current_payload_mode == PAYLOAD_MODE_DIAGNOSTIC) {
        actual_payload_size = sizeof(TelemetryHeader); // 24 bytes
    }

    // Salvaguarda US915: Si DR <= 2 (SF8 o superior), el paquete NO puede superar 125B en canal de 125 kHz
    if (lora_datarate <= 2 && actual_payload_size > 125) {
        actual_payload_size = sizeof(TelemetryHeader) + sizeof(PoultryScaleData); // 48 bytes
        Serial.printf("[SDN AUTO] Reduciendo payload a %d bytes para permitir transmision en DR%u (SF%u)\n",
                      (int)actual_payload_size, lora_datarate, current_sf);
    }

    Serial.println(F("\n-------------------------------------------------------"));
    Serial.printf("[DIAGNOSTICO TX LORAWAN] Uplink #%u en Puerto %d\n", uplink_sequence_count, lora_fport);
    Serial.printf("  ├─ Tamano Payload:    %d Bytes (Modo %u: %s)\n",
                  (int)actual_payload_size, current_payload_mode, getPayloadModeName(current_payload_mode));
    Serial.printf("  ├─ Modulacion:        DR%u (SF%u) @ %d dBm\n", lora_datarate, current_sf, current_tx_power);
    Serial.printf("  ├─ Peso Comida:       %.2f kg (Bruto: %.2f kg)\n", scale.live_net_kg, scale.live_gross_kg);
    Serial.printf("  ├─ Acondicionador:    %.4f V (ADC0: %u)\n", scale.live_voltage_v, scale.live_adc_raw);
    Serial.printf("  └─ ToA Estimado:      %.1f ms\n", current_toa_ms);
    Serial.println(F("-------------------------------------------------------"));

    rx_len = 0;
    uint32_t start_tx_ms = millis();

    // Asegurar ADR desactivado y forzar Datarate antes de emitir
    node.setADR(false);
    int16_t set_dr_res = node.setDatarate(lora_datarate);
    if (set_dr_res != RADIOLIB_ERR_NONE) {
        Serial.printf("[ADVERTENCIA RADIO] node.setDatarate(%u) retorno codigo: %d\n", lora_datarate, set_dr_res);
    }

    // sendReceive transmite el paquete y escucha en RX1 y RX2
    int16_t state = node.sendReceive(
        telemetry_buffer,
        actual_payload_size,
        lora_fport,
        rx_buffer,
        &rx_len,
        lora_is_confirmed
    );

    last_tx_duration_ms = millis() - start_tx_ms;
    last_tx_code = state;
    digitalWrite(PIN_LED_USER, LOW);

    // Actualizar metricas de enlace
    lora_dev_addr = node.getDevAddr();
    lora_last_rssi = radio.getRSSI();
    lora_last_snr = radio.getSNR();

    if (state > 0) {
        // En RadioLib, state > 0 indica que se recibio un paquete de bajada (1 = RX1, 2 = RX2)
        last_tx_success = true;
        Serial.printf("📥 [TX/RX DIAGNOSTICO] Transmision EXITOSA en DR%u (SF%u). Downlink recibido en RX%d! (Bytes: %d)\n",
                      lora_datarate, current_sf, state, (int)rx_len);
        Serial.printf("   - Duracion Ventana (TX + RX): %u ms\n", last_tx_duration_ms);

        if (rx_len >= sizeof(SdnHeader)) {
            const SdnHeader* header = reinterpret_cast<const SdnHeader*>(rx_buffer);
            processSdnDownlink(header, rx_len);
        } else {
            Serial.printf("[SDN ERROR] Payload recibido menor a cabecera SDN (%d bytes)\n", (int)rx_len);
        }
    } else if (state == RADIOLIB_ERR_NONE) {
        // RADIOLIB_ERR_NONE (0) indica que la transmision fue exitosa y las ventanas RX cerraron sin paquetes
        last_tx_success = true;
        Serial.printf("✔ [TX DIAGNOSTICO] Transmision EXITOSA en DR%u (SF%u). Ventanas RX cerradas (sin downlink pendiente).\n",
                      lora_datarate, current_sf);
        Serial.printf("   - Duracion Ventana (TX + RX): %u ms | RSSI: %.1f dBm | SNR: %.1f dB\n",
                      last_tx_duration_ms, lora_last_rssi, lora_last_snr);
    } else {
        last_tx_success = false;
        Serial.printf("❌ [TX DIAGNOSTICO ERROR] Transaccion fallo con codigo RadioLib: %d\n", state);
        if (state == -4) {
            Serial.println(F("   -> CAUSA: RADIOLIB_ERR_PACKET_TOO_LONG. El tamano del paquete supera el limite maximo de este SF en US915."));
        } else if (state == -1116) {
            Serial.println(F("   -> CAUSA: Sesion LoRaWAN no unida o desconectada."));
        }
    }

    last_tx_time_ms = millis();
    tx_in_progress = false;
}

/**
 * -----------------------------------------------------------------------------
 * Procesamiento y Deserializacion de Downlink SDN
 * -----------------------------------------------------------------------------
 */
void processSdnDownlink(const SdnHeader* header, size_t len) {
    if (!header) return;

    uint32_t raw_word = *reinterpret_cast<const uint32_t*>(header);
    const uint8_t* raw_bytes = reinterpret_cast<const uint8_t*>(header);

    last_sdn_cmd_name = String(getSdnCommandName(header->cmd_id));

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
 * Maquina de Estados SDN (Ejecucion de Reglas de Control)
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
            Serial.println(F("📥 [SDN DIAGNOSTICO] EJECUTANDO CMD_RECONFIG_RADIO"));
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

            // Mapeo dinamico de DataRate para US915 (125 kHz):
            // DR3 = SF7 (max 242B), DR2 = SF8 (max 125B), DR1 = SF9 (max 53B)
            if (current_sf == 7) {
                lora_datarate = 3;
            } else if (current_sf == 8) {
                lora_datarate = 2; // SF8 en 125 kHz
            } else if (current_sf == 9) {
                lora_datarate = 1; // SF9 en 125 kHz
            } else if (current_sf >= 10) {
                lora_datarate = 0; // SF10 en 125 kHz
            }

            // Regla de adaptacion SDN en US915:
            // Si se solicita SF8 o superior, 204B excede el limite regional de DR2 (125B).
            // Conmutar automaticamente a modo CRITICAL (48B) para permitir transmision a SF8:
            if (lora_datarate < 3 && current_payload_mode == PAYLOAD_MODE_FULL_TELEMETRY) {
                Serial.println(F("⚡ [SDN ADAPTACION] Payload 204B excede limite US915 DR2/SF8 (125B)."));
                Serial.println(F("   -> Conmutando automaticamente a modo CRITICAL (48B) para emitir en SF8."));
                current_payload_mode = PAYLOAD_MODE_CRITICAL;
            }

            node.setADR(false);
            int16_t dr_res = node.setDatarate(lora_datarate);
            if (dr_res == RADIOLIB_ERR_NONE) {
                Serial.printf("✔ [DIAGNOSTICO OK] RadioLib configuro DR%u (SF%u) exitosamente.\n", lora_datarate, current_sf);
            } else {
                Serial.printf("❌ [DIAGNOSTICO ERROR] RadioLib fallo al asignar DR%u con codigo: %d\n", lora_datarate, dr_res);
            }
            Serial.println(F("=======================================================\n"));
            break;
        }

        case CMD_SET_TEMP_THRESHOLD: {
            float new_thresh = static_cast<float>(header->param_value);
            if (header->param_value > 1000) {
                new_thresh = header->param_value / 100.0f;
            }
            Serial.printf("[SDN ACTION] SET_TEMP_THRESHOLD: Actualizado a %.2f C\n", new_thresh);
            temp_alert_threshold = new_thresh;
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
    Serial.println(F("   BALANZA DE ALIMENTO AVICOLA - HELTEC LORA32 V3      "));
    Serial.println(F("   ESP32-S3 + SX1262 LoRaWAN + Servidor Web WiFi Local "));
    Serial.println(F("======================================================="));

    // 1. Encender perifericos (Alimentacion SX1262 y pantalla OLED por Vext)
    powerOnHeltecPeripherals();

    // 2. Inicializar controlador de balanza, ADC0 (GPIO 1) y pantalla OLED
    scale.begin();

    // 3. Iniciar Punto de Acceso WiFi para la Interfaz Web local de inmediato
    Serial.printf("[WIFI] Iniciando Punto de Acceso: %s ...\n", WIFI_AP_SSID);
    WiFi.mode(WIFI_AP);
    bool ap_ok = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
    if (ap_ok) {
        IPAddress apIP = WiFi.softAPIP();
        Serial.printf("[WIFI OK] Red lista. SSID: '%s' | URL: http://%s\n",
                      WIFI_AP_SSID, apIP.toString().c_str());
    } else {
        Serial.println(F("[WIFI ERROR] No se pudo iniciar el Punto de Acceso"));
    }

    // 4. Iniciar Servidor Web HTTP
    setupWebServer();

    // 5. Inicializar Bus SPI para SX1262
    SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);

    // 6. Inicializar modulo SX1262
    Serial.print(F("[RADIO] Inicializando transceptor SX1262... "));
    int16_t state = radio.begin();
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("EXITO"));
    } else {
        Serial.printf("FALLO (codigo %d). Verifique conexiones hardware.\n", state);
    }

    // Calcular Time-on-Air inicial teorico para 204 bytes
    RadioLibTime_t init_toa_us = radio.getTimeOnAir(TELEMETRY_PAYLOAD_SIZE);
    current_toa_ms = (float)init_toa_us / 1000.0f;

    // 7. Configurar Credenciales LoRaWAN OTAA (Se llama una sola vez para no borrar los DevNonces)
    uint64_t joinEUI_u64 = 0;
    uint64_t devEUI_u64  = 0;
    for (int i = 0; i < 8; i++) {
        joinEUI_u64 = (joinEUI_u64 << 8) | LORAWAN_JOIN_EUI[i];
        devEUI_u64  = (devEUI_u64 << 8)  | LORAWAN_DEV_EUI[i];
    }

    state = node.beginOTAA(joinEUI_u64, devEUI_u64, NULL, (uint8_t*)LORAWAN_APP_KEY);
    Serial.println(F("[LORAWAN] Credenciales OTAA cargadas en RadioLib."));

    // 8. Bucle de Union con ChirpStack (US915 SubBanda 2, canales 8 a 15)
    // Se ejecuta ciclando canales hasta que ChirpStack entregue Join-Accept,
    // manteniendo activo el servidor web para permitir conexiones WiFi inmediatas.
    Serial.println(F("[LORAWAN] Iniciando ciclo de Join con ChirpStack..."));
    
    while (true) {
        server.handleClient();
        scale.updateReadings();
        scale.updateOled(current_toa_ms, "UNINEDO...", false);

        Serial.printf("[LORAWAN] Enviando Join-Request a ChirpStack (SubBanda 2 canales 8-15)...\n");
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
            Serial.printf("[LORAWAN ERROR] Join fallo con codigo: %d. Reintentando en 3s...\n", state);
            uint32_t t_wait = millis();
            while (millis() - t_wait < 3000) {
                server.handleClient();
                scale.updateReadings();
                delay(10);
            }
        }
    }

    Serial.println(F("\n[SISTEMA LISTO] Servidor Web y Balanza operando.\n"));

    // Enviar primer paquete de telemetria para registrar presencia y descargar reglas pendientes
    sendTelemetryUplink();
}

/**
 * -----------------------------------------------------------------------------
 * Bucle Principal (Loop No Bloqueante)
 * -----------------------------------------------------------------------------
 */
void loop() {
    // 1. Atender peticiones HTTP entrantes de clientes WiFi
    server.handleClient();

    // 2. Actualizar lecturas estabilizadas del ADC0 (GPIO 1) con tasa de muestreo adecuada (cada 50 ms)
    static uint32_t last_scale_update = 0;
    if (millis() - last_scale_update >= 50) {
        last_scale_update = millis();
        scale.updateReadings();
    }

    // 3. Temporizador de Transmision Periodica SDN o Disparo Manual desde la Web
    uint32_t now = millis();
    bool timer_triggered = (lora_joined && (now - last_tx_time_ms >= (current_tx_interval_s * 1000UL)));

    if ((timer_triggered || manual_tx_requested) && !tx_in_progress) {
        manual_tx_requested = false;
        sendTelemetryUplink();
    }

    // 4. Atender solicitud de Rejoin OTAA desde la Web
    if (rejoin_requested && !tx_in_progress) {
        rejoin_requested = false;
        performOtaaJoin(5);
    }

    // 5. Watchdog de seguridad para cancelar transmisiones atascadas (> 15 segundos)
    if (tx_in_progress && (millis() - tx_start_timestamp_ms > 15000)) {
        cancelActiveTransmission("Watchdog activado: Tiempo maximo de transmision superado (15s)");
    }

    // 6. Actualizar periodicamente la pantalla OLED del Heltec V3
    static uint32_t last_display_update = 0;
    if (millis() - last_display_update >= 200) {
        last_display_update = millis();
        scale.updateOled(current_toa_ms, lora_joined ? "CONECTADO" : "SIN RED", tx_in_progress);
    }

    delay(2);
}
