/**
 * =============================================================================
 * Proyecto Arduino IDE: HELTEC WiFi LoRa 32 V3 (ESP32-S3 + Semtech SX1262)
 * Arquitectura SDN sobre LoRaWAN (Fase 1: Uplink 204B + Downlink SDN 4B)
 * Biblioteca requerida en Arduino IDE: "RadioLib" por Jan Gromes
 * =============================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <Preferences.h>

#include "sdn_header.h"
#include "heltec_v3_config.h"

// 1. Instanciar transceptor SX1262 con los pines físicos del Heltec V3
SX1262 radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY);

// 2. Instanciar el nodo LoRaWAN para la banda US915 y Subbanda 2 (canales 8 al 15)
// La subbanda se pasa directamente en el constructor de RadioLib
LoRaWANNode node(&radio, &US915, LORAWAN_SUBBAND);

// Variables de estado dinámicas del controlador SDN
static uint32_t current_tx_interval_s = DEFAULT_TX_INTERVAL_S;
static uint8_t  current_sf            = 7;      // Por defecto SF7 (DR 3 en US915)
static int8_t   current_tx_power      = 14;     // Por defecto 14 dBm
static uint8_t  current_payload_mode  = PAYLOAD_MODE_FULL_TELEMETRY;
static bool     deep_sleep_enabled    = false;
static float    temp_alert_threshold  = 30.0f;
static uint32_t uplink_sequence_count = 0;
static uint32_t last_tx_time_ms       = 0;

// Búferes de memoria
static uint8_t telemetry_buffer[TELEMETRY_PAYLOAD_SIZE];
static uint8_t rx_buffer[64];
static size_t  rx_len = 0;

// --- Persistencia DevNonce (evita "Invalid DevNonce" tras resets) ---
Preferences lorawanPrefs;
static uint16_t savedDevNonce = 0;

void saveDevNonce(uint16_t nonce) {
    lorawanPrefs.begin("lorawan", false);
    lorawanPrefs.putUShort("devnonce", nonce);
    lorawanPrefs.end();
    savedDevNonce = nonce;
}

uint16_t loadDevNonce() {
    lorawanPrefs.begin("lorawan", true);
    uint16_t nonce = lorawanPrefs.getUShort("devnonce", 0);
    lorawanPrefs.end();
    savedDevNonce = nonce;
    return nonce;
}

// Declaraciones de funciones
void powerOnHeltecPeripherals();
void sendTelemetryUplink();
void processSdnDownlink(const SdnHeader* header, size_t len);
void executeSdnCommand(const SdnHeader* header);

/**
 * -----------------------------------------------------------------------------
 * Encender periféricos del Heltec V3 (Vext en GPIO 36)
 * -----------------------------------------------------------------------------
 */
void powerOnHeltecPeripherals() {
    pinMode(PIN_VEXT_CTL, OUTPUT);
    digitalWrite(PIN_VEXT_CTL, LOW); // LOW = Activar alimentación para SX1262 y OLED
    delay(100);

    pinMode(PIN_LED_USER, OUTPUT);
    digitalWrite(PIN_LED_USER, LOW);
}

/**
 * -----------------------------------------------------------------------------
 * Enviar paquete de telemetría de 204 bytes (Uplink)
 * -----------------------------------------------------------------------------
 */
void sendTelemetryUplink() {
    digitalWrite(PIN_LED_USER, HIGH); // Encender LED durante la transmisión
    memset(telemetry_buffer, 0, sizeof(telemetry_buffer));

    // Cabecera estructurada de telemetría (24 bytes)
    struct __attribute__((packed)) TelemetryHeader {
        uint32_t seq;
        uint32_t uptime_ms;
        float    battery_mv;
        float    temperature;
        float    humidity;
        float    pressure;
    };

    TelemetryHeader* telem = reinterpret_cast<TelemetryHeader*>(telemetry_buffer);
    telem->seq         = ++uplink_sequence_count;
    telem->uptime_ms   = millis();
    telem->battery_mv  = 3300.0f - (uplink_sequence_count * 0.1f);
    telem->temperature = 24.5f + ((uplink_sequence_count % 10) * 0.25f);
    telem->humidity    = 58.0f;
    telem->pressure    = 1013.25f;

    // Llenar los 180 bytes restantes con patrón de prueba
    for (size_t i = sizeof(TelemetryHeader); i < TELEMETRY_PAYLOAD_SIZE; ++i) {
        telemetry_buffer[i] = static_cast<uint8_t>((i + uplink_sequence_count) & 0xFF);
    }

    Serial.printf("\n[TX] Enviando Uplink #%u (%d Bytes) en Puerto %d (Periodo: %us, SF%d @ %d dBm)...\n",
                  uplink_sequence_count, TELEMETRY_PAYLOAD_SIZE, LORAWAN_APP_PORT,
                  current_tx_interval_s, current_sf, current_tx_power);

    rx_len = 0;
    // Datarate dinamico segun current_sf (DR3 para SF7, DR2 para SF8, etc.)
    uint8_t dr = 3;
    if (current_sf == 8) dr = 2;
    else if (current_sf == 9) dr = 1;
    else if (current_sf >= 10) dr = 0;
    node.setDatarate(dr);

    // Transmitir y escuchar en las ventanas de recepción RX1 y RX2
    int16_t state = node.sendReceive(
        telemetry_buffer,
        sizeof(telemetry_buffer),
        LORAWAN_APP_PORT,
        rx_buffer,
        &rx_len,
        false // Uplink no confirmado
    );

    digitalWrite(PIN_LED_USER, LOW); // Apagar LED

    if (state > 0) {
        // state > 0 indica que se recibió un paquete de bajada (1 = RX1, 2 = RX2)
        Serial.printf("📥 [RX WINDOW] ¡Downlink recibido en ventana Rx%d! Tamaño: %d bytes\n", state, (int)rx_len);
        if (rx_len >= sizeof(SdnHeader)) {
            const SdnHeader* header = reinterpret_cast<const SdnHeader*>(rx_buffer);
            processSdnDownlink(header, rx_len);
        } else {
            Serial.printf("[SDN ERROR] Payload recibido demasiado corto (%d bytes, esperado >= %d)\n",
                          (int)rx_len, (int)sizeof(SdnHeader));
        }
    } else if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("✔ [TX SUCCESS] Uplink enviado con éxito. Ventanas RX cerradas (sin downlink en cola)."));
    } else {
        Serial.printf("[TX/RX WARNING] sendReceive retornó código: %d\n", state);
    }

    last_tx_time_ms = millis();
    Serial.printf("[SCHEDULER] Siguiente transmisión programada en %u segundos.\n", current_tx_interval_s);
}

/**
 * -----------------------------------------------------------------------------
 * Deserializador de la Cabecera SDN (Downlink de 4 bytes)
 * -----------------------------------------------------------------------------
 */
void processSdnDownlink(const SdnHeader* header, size_t len) {
    if (!header) return;

    uint32_t raw_word = *reinterpret_cast<const uint32_t*>(header);
    const uint8_t* raw_bytes = reinterpret_cast<const uint8_t*>(header);

    Serial.println(F("\n======================================================="));
    Serial.println(F("📥 [SDN] REGLA DE CONTROL DESCENDENTE RECIBIDA"));
    Serial.println(F("======================================================="));
    Serial.printf("  Palabra Cruda 32-bit: 0x%08X (Bytes: 0x%02X 0x%02X 0x%02X 0x%02X)\n",
                  raw_word, raw_bytes[0], raw_bytes[1], raw_bytes[2], raw_bytes[3]);
    Serial.printf("  Longitud:             %d bytes (Cabecera SDN: %d bytes)\n", (int)len, (int)sizeof(SdnHeader));
    Serial.println(F("-------------------------------------------------------"));
    Serial.printf("  ├─ cmd_id:       0x%02X (%s)\n", header->cmd_id, getSdnCommandName(header->cmd_id));
    Serial.printf("  ├─ sf_target:    %u -> SF%u\n", header->sf_target, getSpreadingFactorValue(header->sf_target));
    Serial.printf("  ├─ tx_power:     %u dBm\n", header->tx_power);
    Serial.printf("  ├─ sample_rate:  %u -> %u segundos\n", header->sample_rate, getSampleRateSeconds(header->sample_rate));
    Serial.printf("  ├─ payload_mode: %u -> %s\n", header->payload_mode, getPayloadModeName(header->payload_mode));
    Serial.printf("  ├─ sleep_enable: %u (%s)\n", header->sleep_enable, header->sleep_enable ? "HABILITADO" : "DESHABILITADO");
    Serial.printf("  └─ param_value:  %u (0x%04X)\n", header->param_value, header->param_value);
    Serial.println(F("-------------------------------------------------------"));

    executeSdnCommand(header);
}

/**
 * -----------------------------------------------------------------------------
 * Máquina de Estados SDN: switch(header->cmd_id)
 * -----------------------------------------------------------------------------
 */
void executeSdnCommand(const SdnHeader* header) {
    switch (header->cmd_id) {
        case CMD_NOP_KEEP_ALIVE:
            Serial.println(F("[SDN ACCION] NOP / Keep-Alive reconocido. Enlace SDN activo."));
            break;

        case CMD_RECONFIG_RADIO: {
            uint8_t new_sf = getSpreadingFactorValue(header->sf_target);
            uint32_t new_rate = getSampleRateSeconds(header->sample_rate);
            int8_t new_power = static_cast<int8_t>(header->tx_power);

            Serial.printf("[SDN ACCION] RECONFIG_RADIO aplicado:\n");
            Serial.printf("             - Spreading Factor: SF%u -> SF%u\n", current_sf, new_sf);
            Serial.printf("             - Potencia Tx:      %d dBm -> %d dBm\n", current_tx_power, new_power);
            Serial.printf("             - Intervalo:        %u s -> %u s\n", current_tx_interval_s, new_rate);
            Serial.printf("             - Modo Carga Útil:  %u -> %s\n", current_payload_mode, getPayloadModeName(header->payload_mode));

            current_sf = new_sf;
            current_tx_power = new_power;
            current_tx_interval_s = new_rate;
            current_payload_mode = header->payload_mode;
            deep_sleep_enabled = (header->sleep_enable == 1);

            // En US915: DR3=SF7, DR2=SF8, DR1=SF9, DR0=SF10
            uint8_t dr = 3;
            if (current_sf == 8) dr = 2;
            else if (current_sf == 9) dr = 1;
            else if (current_sf == 10) dr = 0;
            node.setDatarate(dr);
            break;
        }

        case CMD_SET_TEMP_THRESHOLD: {
            float new_thresh = static_cast<float>(header->param_value);
            if (header->param_value > 1000) {
                new_thresh = header->param_value / 100.0f;
            }
            Serial.printf("[SDN ACCION] SET_TEMP_THRESHOLD: Nuevo umbral de alerta: %.2f °C\n", new_thresh);
            temp_alert_threshold = new_thresh;
            break;
        }

        case CMD_FORCE_SLEEP: {
            uint16_t sleep_minutes = header->param_value;
            Serial.printf("[SDN ACCION] FORCE_SLEEP: Deep Sleep solicitado por %u minutos.\n", sleep_minutes);
            if (sleep_minutes > 0) {
                digitalWrite(PIN_VEXT_CTL, HIGH); // Apagar alimentación de radio
                uint64_t sleep_us = static_cast<uint64_t>(sleep_minutes) * 60ULL * 1000000ULL;
                esp_sleep_enable_timer_wakeup(sleep_us);
                Serial.println(F("[SISTEMA] Entrando en Deep Sleep ahora..."));
                Serial.flush();
                esp_deep_sleep_start();
            }
            break;
        }

        case CMD_SET_P2P_RELAY: {
            Serial.printf("[SDN ACCION] SET_P2P_RELAY: Conmutando a modo retransmisión para nodo 0x%04X\n", header->param_value);
            break;
        }

        case CMD_SYSTEM_RESET: {
            Serial.printf("[SDN ACCION] SYSTEM_RESET: Token 0x%04X validado. Reiniciando microcontrolador...\n", header->param_value);
            Serial.flush();
            delay(1500);
            ESP.restart();
            break;
        }

        default:
            Serial.printf("[SDN ADVERTENCIA] Código cmd_id 0x%02X no manejado.\n", header->cmd_id);
            break;
    }
    Serial.println(F("=======================================================\n"));
}

/**
 * -----------------------------------------------------------------------------
 * Setup de Arduino
 * -----------------------------------------------------------------------------
 */
void setup() {
    Serial.begin(115200);
    // Pausa para que el puerto serie USB CDC del ESP32-S3 se conecte a la PC
    delay(2500);

    Serial.println(F("\n======================================================="));
    Serial.println(F("   HELTEC WiFi LoRa 32 V3 (ESP32-S3 + SX1262) Node   "));
    Serial.println(F("   Arquitectura SDN - Fase 1: Pipeline y Downlink       "));
    Serial.println(F("======================================================="));
    Serial.printf("Tamaño struct SdnHeader: %d bytes (Verificado)\n", (int)sizeof(SdnHeader));

    // 1. Energizar periféricos del Heltec V3 (Vext en GPIO 36)
    powerOnHeltecPeripherals();

    // 2. Iniciar bus SPI
    SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);

    // 3. Iniciar transceptor de radio
    Serial.print(F("[RADIO] Inicializando transceptor SX1262... "));
    int16_t state = radio.begin();
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("¡EXITOSO!"));
    } else {
        Serial.printf("FALLO (código %d)\n", state);
        while (true) { delay(1000); }
    }

    // 4. Configurar llaves OTAA (Para LoRaWAN 1.0.3 en RadioLib, el 3er parámetro debe ser NULL)
    Serial.println(F("[LORAWAN] Configurando llaves OTAA..."));
    node.beginOTAA(LORAWAN_JOIN_EUI, LORAWAN_DEV_EUI, NULL, (uint8_t*)LORAWAN_APP_KEY);

    // Desactivar ADR automático para que las decisiones del controlador SDN manden
    node.setADR(false);

    // Establecer DR3 (SF7 en US915, necesario para paquete de 204 bytes)
    node.setDatarate(3);

    // --- Cargar DevNonce persistido para evitar "Invalid DevNonce" ---
    uint16_t initialDevNonce = loadDevNonce();
    if (initialDevNonce > 0) {
        node.setDevNonce(initialDevNonce);
        Serial.printf("[LORAWAN] DevNonce restaurado: %u\n", initialDevNonce);
    } else {
        Serial.println(F("[LORAWAN] DevNonce inicial: 0 (primer join)"));
    }

    // 5. Iniciar proceso de unión OTAA (con reintentos y backoff exponencial)
    Serial.println(F("[LORAWAN] Enviando Join-Request a ChirpStack..."));
    const uint8_t MAX_JOIN_RETRIES = 10;
    uint8_t joinRetry = 0;
    uint16_t baseDelaySec = 10;

    while (joinRetry < MAX_JOIN_RETRIES) {
        joinRetry++;
        Serial.printf("[LORAWAN] Intento Join %d/%d...\n", joinRetry, MAX_JOIN_RETRIES);

        state = node.activateOTAA();

        // Guardar DevNonce después de cada intento (RadioLib lo incrementa internamente)
        uint16_t currentDevNonce = node.getDevNonce();
        if (currentDevNonce > savedDevNonce) {
            saveDevNonce(currentDevNonce);
            Serial.printf("[LORAWAN] DevNonce actualizado y guardado: %u\n", currentDevNonce);
        }

        if (state == RADIOLIB_LORAWAN_NEW_SESSION || state >= RADIOLIB_ERR_NONE) {
            Serial.println(F("🎉 [LORAWAN] ¡UNIÓN EXITOSA (JOINED) CON CHIRPSTACK!"));
            // IMPORTANTE: activateOTAA() resetea el Datarate a DR0 (max 11 bytes en US915).
            // Forzar DR3 (SF7 / 125 kHz, max 242 bytes) para permitir el paquete de 204 bytes:
            node.setADR(false);
            node.setDatarate(3);
            break;
        } else {
            Serial.printf("[LORAWAN ERROR] Join falló con código: %d", state);
            if (state == RADIOLIB_ERR_INVALID_NONCE) {
                Serial.print(F(" (DevNonce inválido/duplicado)"));
            }
            Serial.println();

            if (joinRetry < MAX_JOIN_RETRIES) {
                // Backoff exponencial: 10s, 20s, 40s, 80s... (máx 5 min)
                uint16_t delaySec = baseDelaySec * (1 << (joinRetry - 1));
                if (delaySec > 300) delaySec = 300;
                Serial.printf("[LORAWAN] Reintentando en %u segundos...\n", delaySec);
                delay(delaySec * 1000);
            }
        }
    }

    if (joinRetry >= MAX_JOIN_RETRIES) {
        Serial.println(F("[LORAWAN FATAL] Máximos reintentos agotados. Reiniciando en 60s..."));
        delay(60000);
        ESP.restart();
    }

    // 6. Primer envío de telemetría de 204 bytes
    sendTelemetryUplink();
}

/**
 * -----------------------------------------------------------------------------
 * Bucle Principal (Loop)
 * -----------------------------------------------------------------------------
 */
void loop() {
    uint32_t now = millis();
    // Temporizador no bloqueante
    if (now - last_tx_time_ms >= (current_tx_interval_s * 1000UL)) {
        sendTelemetryUplink();
    }
    delay(10);
}
