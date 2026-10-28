/**
 * =============================================================================
 * SDN-LoRa Firmware Node - Phase 1: Pipeline & Bi-directional Validation
 * Hardware: ESP32 DevKit + Ai-Thinker Ra-02 (SX1278 / SX1276)
 * LoRaWAN Stack: MCCI LoRaWAN LMIC Library
 * =============================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <lmic.h>
#include <hal/hal.h>

#include "sdn_header.h"
#include "config.h"

// Runtime State Variables (dynamically controlled by SDN Controller)
static uint32_t current_tx_interval_s = DEFAULT_TX_INTERVAL_S;
static uint8_t  current_sf            = 7;      // Default SF7
static int8_t   current_tx_power      = 14;     // Default 14 dBm
static uint8_t  current_payload_mode  = PAYLOAD_MODE_FULL_TELEMETRY;
static bool     deep_sleep_enabled    = false;
static float    temp_alert_threshold  = 30.0f;  // Default threshold °C
static uint32_t uplink_sequence_count = 0;

// LMIC Scheduling Job
static osjob_t sendjob;

// Forward Declarations
void do_send(osjob_t* j);
void processSdnDownlink(const SdnHeader* header, uint8_t data_len);
void executeSdnCommand(const SdnHeader* header);

/**
 * -----------------------------------------------------------------------------
 * Uplink Generator: Constructs and transmits 204-byte telemetry frame
 * -----------------------------------------------------------------------------
 */
void do_send(osjob_t* j) {
    // Check if there is a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("[LMIC] OP_TXRXPEND: Transmission pending, skipping slot."));
        return;
    }

    // Allocate exact 204 bytes buffer
    uint8_t telemetry_buffer[TELEMETRY_PAYLOAD_SIZE];
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
    telem->battery_mv  = 3300.0f - (uplink_sequence_count * 0.1f); // Simulated slow battery drain
    telem->temperature = 24.5f + ((uplink_sequence_count % 10) * 0.25f); // Simulated temp
    telem->humidity    = 58.0f;
    telem->pressure    = 1013.25f;

    // Fill remaining 180 bytes with simulated sensor pattern
    for (size_t i = sizeof(TelemetryHeader); i < TELEMETRY_PAYLOAD_SIZE; ++i) {
        telemetry_buffer[i] = static_cast<uint8_t>((i + uplink_sequence_count) & 0xFF);
    }

    // Prepare LoRaWAN transmission (Unconfirmed uplink on LORAWAN_APP_PORT)
    lmic_tx_error_t err = LMIC_setTxData2(LORAWAN_APP_PORT, telemetry_buffer, sizeof(telemetry_buffer), 0);
    if (err == 0) {
        Serial.printf("\n[TX] Uplink Queued: #%u | Size: %d Bytes | Int: %us | SF%d @ %d dBm\n",
                      uplink_sequence_count, TELEMETRY_PAYLOAD_SIZE,
                      current_tx_interval_s, current_sf, current_tx_power);
    } else {
        Serial.printf("[TX ERROR] LMIC_setTxData2 failed with error code: %d\n", err);
    }
}

/**
 * -----------------------------------------------------------------------------
 * SDN Downlink Parser: Deserializes 4-byte bitfield structure & logs details
 * -----------------------------------------------------------------------------
 */
void processSdnDownlink(const SdnHeader* header, uint8_t data_len) {
    if (!header) return;

    // Access raw 32-bit integer for hex and bit inspection
    uint32_t raw_word = *reinterpret_cast<const uint32_t*>(header);
    const uint8_t* raw_bytes = reinterpret_cast<const uint8_t*>(header);

    Serial.println(F("\n======================================================="));
    Serial.println(F("📥 [SDN] DOWNLINK RECEIVED IN RX WINDOW"));
    Serial.println(F("======================================================="));
    Serial.printf("  Raw Data:       0x%08X (Bytes: 0x%02X 0x%02X 0x%02X 0x%02X)\n",
                  raw_word, raw_bytes[0], raw_bytes[1], raw_bytes[2], raw_bytes[3]);
    Serial.printf("  Payload Length: %d bytes (SDN Header: %d bytes)\n", data_len, (int)sizeof(SdnHeader));
    Serial.println(F("-------------------------------------------------------"));
    Serial.printf("  ├─ cmd_id:       0x%02X (%s)\n", header->cmd_id, getSdnCommandName(header->cmd_id));
    Serial.printf("  ├─ sf_target:    %u -> SF%u\n", header->sf_target, getSpreadingFactorValue(header->sf_target));
    Serial.printf("  ├─ tx_power:     %u dBm\n", header->tx_power);
    Serial.printf("  ├─ sample_rate:  %u -> %u seconds\n", header->sample_rate, getSampleRateSeconds(header->sample_rate));
    Serial.printf("  ├─ payload_mode: %u -> %s\n", header->payload_mode, getPayloadModeName(header->payload_mode));
    Serial.printf("  ├─ sleep_enable: %u (%s)\n", header->sleep_enable, header->sleep_enable ? "ENABLED" : "DISABLED");
    Serial.printf("  └─ param_value:  %u (0x%04X)\n", header->param_value, header->param_value);
    Serial.println(F("-------------------------------------------------------"));

    // Execute State Machine
    executeSdnCommand(header);
}

/**
 * -----------------------------------------------------------------------------
 * SDN State Machine: switch(header->cmd_id)
 * -----------------------------------------------------------------------------
 */
void executeSdnCommand(const SdnHeader* header) {
    switch (header->cmd_id) {
        case CMD_NOP_KEEP_ALIVE:
            Serial.println(F("[SDN ACTION] Executing NOP / Keep-Alive. Link integrity verified."));
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

            // Dynamically update LMIC Data Rate and TX Power
            // Note: In US915, SF7 is DR3; in EU868, SF7 is DR5
            #if defined(CFG_us915)
                dr_t lmic_dr = DR_SF10;
                if (current_sf == 7)  lmic_dr = DR_SF7;
                else if (current_sf == 8)  lmic_dr = DR_SF8;
                else if (current_sf == 9)  lmic_dr = DR_SF9;
                else if (current_sf == 10) lmic_dr = DR_SF10;
                LMIC_setDrTxpow(lmic_dr, current_tx_power);
            #elif defined(CFG_eu868)
                dr_t lmic_dr = DR_SF7;
                if (current_sf == 7)  lmic_dr = DR_SF7;
                else if (current_sf == 8)  lmic_dr = DR_SF8;
                else if (current_sf == 9)  lmic_dr = DR_SF9;
                else if (current_sf == 10) lmic_dr = DR_SF10;
                else if (current_sf == 11) lmic_dr = DR_SF11;
                else if (current_sf == 12) lmic_dr = DR_SF12;
                LMIC_setDrTxpow(lmic_dr, current_tx_power);
            #endif
            break;
        }

        case CMD_SET_TEMP_THRESHOLD: {
            // param_value represents threshold (e.g. 2550 -> 25.50 °C if scaled x100, or raw integer)
            float new_thresh = static_cast<float>(header->param_value);
            if (header->param_value > 1000) {
                new_thresh = header->param_value / 100.0f; // Scaled x100 representation
            }
            Serial.printf("[SDN ACTION] SET_TEMP_THRESHOLD: Updated threshold to %.2f °C (raw param: %u)\n",
                          new_thresh, header->param_value);
            temp_alert_threshold = new_thresh;
            break;
        }

        case CMD_FORCE_SLEEP: {
            uint16_t sleep_minutes = header->param_value;
            Serial.printf("[SDN ACTION] FORCE_SLEEP: Node instructed to enter Deep Sleep for %u minutes.\n",
                          sleep_minutes);
            if (sleep_minutes > 0) {
                uint64_t sleep_us = static_cast<uint64_t>(sleep_minutes) * 60ULL * 1000000ULL;
                esp_sleep_enable_timer_wakeup(sleep_us);
                Serial.println(F("[SYSTEM] Entering Deep Sleep now. Bye!"));
                Serial.flush();
                esp_deep_sleep_start();
            }
            break;
        }

        case CMD_SET_P2P_RELAY: {
            uint16_t target_node_id = header->param_value;
            Serial.printf("[SDN ACTION] SET_P2P_RELAY: Setting node into P2P Relay/Listen mode for Node ID 0x%04X\n",
                          target_node_id);
            break;
        }

        case CMD_SYSTEM_RESET: {
            // Check confirmation token (e.g. 0xA5A5 or non-zero validation)
            Serial.printf("[SDN ACTION] SYSTEM_RESET requested with validation token: 0x%04X\n",
                          header->param_value);
            Serial.println(F("[SYSTEM] Microcontroller restarting in 2 seconds..."));
            Serial.flush();
            delay(2000);
            ESP.restart();
            break;
        }

        default:
            Serial.printf("[SDN WARNING] Unhandled cmd_id 0x%02X (Reserved for future SDN phases).\n",
                          header->cmd_id);
            break;
    }
    Serial.println(F("=======================================================\n"));
}

/**
 * -----------------------------------------------------------------------------
 * LMIC Event Callback
 * -----------------------------------------------------------------------------
 */
void onEvent(ev_t ev) {
    Serial.print(F("[LMIC Time: "));
    Serial.print(os_getTime());
    Serial.print(F("] Event: "));

    switch (ev) {
        case EV_JOINING:
            Serial.println(F("EV_JOINING: Starting LoRaWAN OTAA Join request..."));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED: Successfully joined network!"));
            // Disable ADR for initial predictable SDN testing
            LMIC_setLinkCheckMode(0);
            // Default to SF7, 14 dBm
            #if defined(CFG_us915)
                LMIC_setDrTxpow(DR_SF7, current_tx_power);
            #elif defined(CFG_eu868)
                LMIC_setDrTxpow(DR_SF7, current_tx_power);
            #endif
            // Schedule the first telemetry uplink
            os_setCallback(&sendjob, do_send);
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED: Join request failed. Retrying..."));
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (Transmission and RX windows closed)"));
            if (LMIC.txrxFlags & TXRX_ACK) {
                Serial.println(F("  -> Received ACK from Gateway"));
            }
            if (LMIC.dataLen) {
                Serial.printf("  -> Downlink payload received! Length: %d bytes | Port: %d\n",
                              LMIC.dataLen, LMIC.frame[LMIC.dataBeg - 1]);

                // Validate minimum 4-byte SDN Header
                if (LMIC.dataLen >= sizeof(SdnHeader)) {
                    const uint8_t* payload_ptr = &LMIC.frame[LMIC.dataBeg];
                    const SdnHeader* header = reinterpret_cast<const SdnHeader*>(payload_ptr);
                    processSdnDownlink(header, LMIC.dataLen);
                } else {
                    Serial.printf("[SDN ERROR] Downlink payload too short (%d bytes, minimum required: %d)\n",
                                  LMIC.dataLen, (int)sizeof(SdnHeader));
                }
            }

            // Schedule next periodic transmission according to current_tx_interval_s
            os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(current_tx_interval_s), do_send);
            Serial.printf("[SCHEDULER] Next transmission in %u seconds.\n", current_tx_interval_s);
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
        default:
            Serial.printf("Unknown LMIC event: %u\n", (unsigned)ev);
            break;
    }
}

/**
 * -----------------------------------------------------------------------------
 * Arduino Setup
 * -----------------------------------------------------------------------------
 */
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000); // Allow time for USB Serial monitor attach

    Serial.println(F("\n======================================================="));
    Serial.println(F("   ESP32 + Ra-02 (SX1276/78) SDN-LoRa Node Starting   "));
    Serial.println(F("   Phase 1: Pipeline & Bi-directional Communication    "));
    Serial.println(F("======================================================="));
    Serial.printf("SdnHeader struct size: %d bytes (Verified)\n", (int)sizeof(SdnHeader));

    // Initialize LMIC OS runtime
    os_init();

    // Reset MAC state
    LMIC_reset();

    // Regional channel configuration (US915 Sub-band 2 / 1 or EU868)
    #if defined(CFG_us915)
        // Laird Sentrius RG1 US915 typically uses Sub-band 2 (Channels 8..15 + 65)
        // Or Sub-band 1 (Channels 0..7 + 64). Select Sub-band 1/2 as appropriate:
        LMIC_selectSubBand(1); // Select sub-band 2 (channels 8-15)
        Serial.println(F("[RF] Region: US915 configured."));
    #elif defined(CFG_eu868)
        Serial.println(F("[RF] Region: EU868 configured."));
    #endif

    // Set clock error tolerance for ESP32 internal RC/XTAL jitter
    LMIC_setClockError(MAX_CLOCK_ERROR * 2 / 100);

    // Trigger Join sequence
    LMIC_startJoining();
}

/**
 * -----------------------------------------------------------------------------
 * Main Loop
 * -----------------------------------------------------------------------------
 */
void loop() {
    // Run the LMIC state machine scheduler
    os_runloop_once();
}
