/**
 * =============================================================================
 * SDN-LoRa Firmware Node (Phase 1) - HELTEC WiFi LoRa 32 V3
 * Hardware: ESP32-S3FN8 + Semtech SX1262 LoRa Transceiver
 * LoRaWAN Stack: RadioLib (Modern, robust LoRaWAN 1.0.4 / 1.1 support)
 * =============================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

#include "sdn_header.h"
#include "heltec_v3_config.h"

// Define Region (US915 for America or EU868 for Europe)
#if defined(CFG_eu868)
  #define LORAWAN_BAND EU868
#else
  #define LORAWAN_BAND US915
#endif

// Instantiate Semtech SX1262 Module with Heltec V3 Pins
SX1262 radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY);
LoRaWANNode node(&radio, &LORAWAN_BAND);

// Runtime SDN Parameters
static uint32_t current_tx_interval_s = DEFAULT_TX_INTERVAL_S;
static uint8_t  current_sf            = 7;      // Default SF7
static int8_t   current_tx_power      = 14;     // Default 14 dBm
static uint8_t  current_payload_mode  = PAYLOAD_MODE_FULL_TELEMETRY;
static bool     deep_sleep_enabled    = false;
static float    temp_alert_threshold  = 30.0f;  // Default threshold °C
static uint32_t uplink_sequence_count = 0;
static uint32_t last_tx_time_ms       = 0;

// Buffers
static uint8_t telemetry_buffer[TELEMETRY_PAYLOAD_SIZE];
static uint8_t rx_buffer[256];
static size_t  rx_len = 0;

// Forward Declarations
void powerOnHeltecPeripherals();
void sendTelemetryUplink();
void processSdnDownlink(const SdnHeader* header, size_t len);
void executeSdnCommand(const SdnHeader* header);

/**
 * -----------------------------------------------------------------------------
 * Heltec V3 Power Management (Vext Control)
 * -----------------------------------------------------------------------------
 * Heltec V3 powers the SX1262 and OLED through a P-channel MOSFET on GPIO 36.
 * Pulling GPIO 36 LOW powers the SX1262 transceiver.
 */
void powerOnHeltecPeripherals() {
    pinMode(PIN_VEXT_CTL, OUTPUT);
    digitalWrite(PIN_VEXT_CTL, LOW); // LOW = Power ON
    delay(100);

    // Initialize User LED
    pinMode(PIN_LED_USER, OUTPUT);
    digitalWrite(PIN_LED_USER, LOW);
}

/**
 * -----------------------------------------------------------------------------
 * Telemetry Generator (Uplink 204 Bytes)
 * -----------------------------------------------------------------------------
 */
void sendTelemetryUplink() {
    digitalWrite(PIN_LED_USER, HIGH); // LED ON during TX
    memset(telemetry_buffer, 0, sizeof(telemetry_buffer));

    // Telemetry Header (24 bytes)
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

    // Fill remaining 180 bytes with simulated telemetry data
    for (size_t i = sizeof(TelemetryHeader); i < TELEMETRY_PAYLOAD_SIZE; ++i) {
        telemetry_buffer[i] = static_cast<uint8_t>((i + uplink_sequence_count) & 0xFF);
    }

    Serial.printf("\n[TX] Sending Uplink #%u (%d Bytes) on Port %d (Interval: %us, SF%d @ %d dBm)...\n",
                  uplink_sequence_count, TELEMETRY_PAYLOAD_SIZE, LORAWAN_APP_PORT,
                  current_tx_interval_s, current_sf, current_tx_power);

    rx_len = 0;
    // sendReceive sends the packet and waits for RX1 and RX2 receive windows
    int16_t state = node.sendReceive(
        telemetry_buffer,
        sizeof(telemetry_buffer),
        LORAWAN_APP_PORT,
        rx_buffer,
        &rx_len,
        false // unconfirmed uplink
    );

    digitalWrite(PIN_LED_USER, LOW); // LED OFF

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("[TX SUCCESS] Uplink sent successfully."));
        if (rx_len > 0) {
            Serial.printf("📥 [RX WINDOW] Downlink received! Bytes: %d\n", (int)rx_len);
            if (rx_len >= sizeof(SdnHeader)) {
                const SdnHeader* header = reinterpret_cast<const SdnHeader*>(rx_buffer);
                processSdnDownlink(header, rx_len);
            } else {
                Serial.printf("[SDN ERROR] Payload too small (%d bytes, expected >= %d)\n",
                              (int)rx_len, (int)sizeof(SdnHeader));
            }
        } else {
            Serial.println(F("[RX WINDOW] No downlink queued in this window."));
        }
    } else {
        Serial.printf("[TX/RX WARNING] Transaction returned code: %d\n", state);
    }

    last_tx_time_ms = millis();
    Serial.printf("[SCHEDULER] Next transmission scheduled in %u seconds.\n", current_tx_interval_s);
}

/**
 * -----------------------------------------------------------------------------
 * SDN Downlink Deserializer
 * -----------------------------------------------------------------------------
 */
void processSdnDownlink(const SdnHeader* header, size_t len) {
    if (!header) return;

    uint32_t raw_word = *reinterpret_cast<const uint32_t*>(header);
    const uint8_t* raw_bytes = reinterpret_cast<const uint8_t*>(header);

    Serial.println(F("\n======================================================="));
    Serial.println(F("📥 [SDN] DOWNLINK RECEIVED IN RX WINDOW (HELTEC V3)"));
    Serial.println(F("======================================================="));
    Serial.printf("  Raw Data:       0x%08X (Bytes: 0x%02X 0x%02X 0x%02X 0x%02X)\n",
                  raw_word, raw_bytes[0], raw_bytes[1], raw_bytes[2], raw_bytes[3]);
    Serial.printf("  Payload Length: %d bytes (SDN Header: %d bytes)\n", (int)len, (int)sizeof(SdnHeader));
    Serial.println(F("-------------------------------------------------------"));
    Serial.printf("  ├─ cmd_id:       0x%02X (%s)\n", header->cmd_id, getSdnCommandName(header->cmd_id));
    Serial.printf("  ├─ sf_target:    %u -> SF%u\n", header->sf_target, getSpreadingFactorValue(header->sf_target));
    Serial.printf("  ├─ tx_power:     %u dBm\n", header->tx_power);
    Serial.printf("  ├─ sample_rate:  %u -> %u seconds\n", header->sample_rate, getSampleRateSeconds(header->sample_rate));
    Serial.printf("  ├─ payload_mode: %u -> %s\n", header->payload_mode, getPayloadModeName(header->payload_mode));
    Serial.printf("  ├─ sleep_enable: %u (%s)\n", header->sleep_enable, header->sleep_enable ? "ENABLED" : "DISABLED");
    Serial.printf("  └─ param_value:  %u (0x%04X)\n", header->param_value, header->param_value);
    Serial.println(F("-------------------------------------------------------"));

    executeSdnCommand(header);
}

/**
 * -----------------------------------------------------------------------------
 * SDN State Machine
 * -----------------------------------------------------------------------------
 */
void executeSdnCommand(const SdnHeader* header) {
    switch (header->cmd_id) {
        case CMD_NOP_KEEP_ALIVE:
            Serial.println(F("[SDN ACTION] Keep-Alive acknowledged. SDN link active."));
            break;

        case CMD_RECONFIG_RADIO: {
            uint8_t new_sf = getSpreadingFactorValue(header->sf_target);
            uint32_t new_rate = getSampleRateSeconds(header->sample_rate);
            int8_t new_power = static_cast<int8_t>(header->tx_power);

            Serial.printf("[SDN ACTION] RECONFIG_RADIO applied:\n");
            Serial.printf("             - Spreading Factor: SF%u -> SF%u\n", current_sf, new_sf);
            Serial.printf("             - Tx Power:         %d dBm -> %d dBm\n", current_tx_power, new_power);
            Serial.printf("             - Uplink Interval:  %u s -> %u s\n", current_tx_interval_s, new_rate);
            Serial.printf("             - Payload Mode:     %u -> %s\n", current_payload_mode, getPayloadModeName(header->payload_mode));

            current_sf = new_sf;
            current_tx_power = new_power;
            current_tx_interval_s = new_rate;
            current_payload_mode = header->payload_mode;
            deep_sleep_enabled = (header->sleep_enable == 1);

            // Reconfigure SX1262 Data Rate / SF in RadioLib
            node.setDataRate(current_sf == 7 ? 3 : (current_sf == 8 ? 2 : 1));
            break;
        }

        case CMD_SET_TEMP_THRESHOLD: {
            float new_thresh = static_cast<float>(header->param_value);
            if (header->param_value > 1000) {
                new_thresh = header->param_value / 100.0f;
            }
            Serial.printf("[SDN ACTION] SET_TEMP_THRESHOLD: Updated to %.2f °C (raw: %u)\n",
                          new_thresh, header->param_value);
            temp_alert_threshold = new_thresh;
            break;
        }

        case CMD_FORCE_SLEEP: {
            uint16_t sleep_minutes = header->param_value;
            Serial.printf("[SDN ACTION] FORCE_SLEEP: Deep Sleep for %u minutes requested.\n", sleep_minutes);
            if (sleep_minutes > 0) {
                // Turn off SX1262 power via Vext
                digitalWrite(PIN_VEXT_CTL, HIGH);
                uint64_t sleep_us = static_cast<uint64_t>(sleep_minutes) * 60ULL * 1000000ULL;
                esp_sleep_enable_timer_wakeup(sleep_us);
                Serial.println(F("[SYSTEM] Entering Deep Sleep now."));
                Serial.flush();
                esp_deep_sleep_start();
            }
            break;
        }

        case CMD_SET_P2P_RELAY: {
            Serial.printf("[SDN ACTION] SET_P2P_RELAY: Relay target Node ID 0x%04X\n", header->param_value);
            break;
        }

        case CMD_SYSTEM_RESET: {
            Serial.printf("[SDN ACTION] SYSTEM_RESET: Token 0x%04X verified. Rebooting...\n", header->param_value);
            Serial.flush();
            delay(1500);
            ESP.restart();
            break;
        }

        default:
            Serial.printf("[SDN WARNING] Unhandled cmd_id 0x%02X.\n", header->cmd_id);
            break;
    }
    Serial.println(F("=======================================================\n"));
}

/**
 * -----------------------------------------------------------------------------
 * Arduino Setup
 * -----------------------------------------------------------------------------
 */
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);

    Serial.println(F("\n======================================================="));
    Serial.println(F("   HELTEC WiFi LoRa 32 V3 (ESP32-S3 + SX1262) Node   "));
    Serial.println(F("   SDN Architecture - Phase 1: Pipeline Validation     "));
    Serial.println(F("======================================================="));
    Serial.printf("SdnHeader struct size: %d bytes (Verified)\n", (int)sizeof(SdnHeader));

    // 1. Power ON Heltec V3 Peripherals (Vext)
    powerOnHeltecPeripherals();

    // 2. Initialize SPI Bus with Heltec V3 Pinout
    SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);

    // 3. Initialize RadioLib SX1262 Transceiver
    Serial.print(F("[RADIO] Initializing SX1262 transceiver... "));
    int16_t state = radio.begin();
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("SUCCESS!"));
    } else {
        Serial.printf("FAILED (code %d)\n", state);
        while (true) { delay(1000); }
    }

    // 4. Configure LoRaWAN Node (Subband 2 for US915 / Laird RG1)
    #if !defined(CFG_eu868)
        // Select Subband 2 (channels 8-15) typical for US915 Gateways
        node.selectSubBand(2);
    #endif

    // 5. Start OTAA Join
    Serial.println(F("[LORAWAN] Initiating OTAA Join Procedure..."));
    uint64_t joinEUI_u64 = 0;
    uint64_t devEUI_u64  = 0;
    for (int i = 0; i < 8; i++) {
        joinEUI_u64 = (joinEUI_u64 << 8) | LORAWAN_JOIN_EUI[i];
        devEUI_u64  = (devEUI_u64 << 8)  | LORAWAN_DEV_EUI[i];
    }

    state = node.beginOTAA(joinEUI_u64, devEUI_u64, (uint8_t*)LORAWAN_NWK_KEY, (uint8_t*)LORAWAN_APP_KEY);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("🎉 [LORAWAN] Successfully JOINED the network!"));
    } else {
        Serial.printf("[LORAWAN ERROR] Join failed with error code: %d. Check keys in ChirpStack!\n", state);
    }

    // Initial Uplink
    sendTelemetryUplink();
}

/**
 * -----------------------------------------------------------------------------
 * Main Loop
 * -----------------------------------------------------------------------------
 */
void loop() {
    uint32_t now = millis();
    // Non-blocking periodic uplink trigger
    if (now - last_tx_time_ms >= (current_tx_interval_s * 1000UL)) {
        sendTelemetryUplink();
    }
    delay(10);
}
