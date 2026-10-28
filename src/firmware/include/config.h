#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <lmic.h>
#include <hal/hal.h>

/**
 * -----------------------------------------------------------------------------
 * LoRaWAN Network Credentials (OTAA / ABP)
 * -----------------------------------------------------------------------------
 * NOTE FOR OTAA:
 * - APPEUI / JOINEUI: 8 bytes, MUST be in LITTLE-ENDIAN format (LSB first)
 * - DEVEUI: 8 bytes, MUST be in LITTLE-ENDIAN format (LSB first)
 * - APPKEY: 16 bytes, MUST be in BIG-ENDIAN format (MSB first)
 * Configure these values to match your ChirpStack device registration.
 */

// AppEUI / JoinEUI (8 bytes, Little-Endian)
// Example: 00 00 00 00 00 00 00 00
static const uint8_t PROGMEM APPEUI[8] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// DevEUI (8 bytes, Little-Endian)
// Match with your ChirpStack registered DevEUI (e.g. 0102030405060708)
static const uint8_t PROGMEM DEVEUI[8] = {
    0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01
};

// AppKey (16 bytes, Big-Endian)
static const uint8_t PROGMEM APPKEY[16] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
};

// Provide callbacks required by LMIC
inline void os_getArtEui(u1_t* buf) { memcpy_P(buf, APPEUI, 8); }
inline void os_getDevEui(u1_t* buf) { memcpy_P(buf, DEVEUI, 8); }
inline void os_getDevKey(u1_t* buf) { memcpy_P(buf, APPKEY, 16); }

/**
 * -----------------------------------------------------------------------------
 * Hardware Pin Mapping: ESP32 DevKit + Ai-Thinker Ra-02 (SX1278 / SX1276)
 * -----------------------------------------------------------------------------
 * Standard ESP32 SPI Pinout:
 *   SCK  -> GPIO 18
 *   MISO -> GPIO 19
 *   MOSI -> GPIO 23
 *
 * Ra-02 Control Pins:
 *   NSS (CS) -> GPIO 5   (or GPIO 15 / 18 on custom boards)
 *   RST      -> GPIO 14  (or GPIO 26 / 2)
 *   DIO0     -> GPIO 26  (Packet Tx/Rx interrupt - MANDATORY)
 *   DIO1     -> GPIO 33  (RX Timeout interrupt - MANDATORY for LoRaWAN RX1/RX2)
 *   DIO2     -> LMIC_UNUSED_PIN (Optional for FSK, unused in LoRa)
 */
const lmic_pinmap lmic_pins = {
    .nss = 5,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 14,
    .dio = {26, 33, LMIC_UNUSED_PIN},
};

/**
 * -----------------------------------------------------------------------------
 * Telemetry and Transmission Parameters
 * -----------------------------------------------------------------------------
 */
#define TELEMETRY_PAYLOAD_SIZE 204  // Uplink payload size in bytes (as specified in SDN.md)
#define LORAWAN_APP_PORT       10   // LoRaWAN Port for SDN Telemetry and Downlinks
#define DEFAULT_TX_INTERVAL_S  60   // Initial transmission period in seconds

#endif // CONFIG_H
