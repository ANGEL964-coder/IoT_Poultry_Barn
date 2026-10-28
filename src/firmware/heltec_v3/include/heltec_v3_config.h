#ifndef HELTEC_V3_CONFIG_H
#define HELTEC_V3_CONFIG_H

#include <stdint.h>

/**
 * -----------------------------------------------------------------------------
 * Hardware Pin Mapping: Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262)
 * -----------------------------------------------------------------------------
 * The Heltec V3 uses an ESP32-S3FN8 and a Semtech SX1262 LoRa transceiver.
 * Crucial: GPIO 36 (Vext) MUST be set to LOW to power the SX1262 and OLED.
 */
#define PIN_LORA_NSS    8   // SX1262 Chip Select
#define PIN_LORA_SCK    9   // SPI Clock
#define PIN_LORA_MOSI   10  // SPI MOSI
#define PIN_LORA_MISO   11  // SPI MISO
#define PIN_LORA_RST    12  // SX1262 Reset
#define PIN_LORA_BUSY   13  // SX1262 Busy indicator (Mandatory for SX126x)
#define PIN_LORA_DIO1   14  // SX1262 DIO1 (Packet Done / Timeout)
#define PIN_VEXT_CTL    36  // Power control: LOW = ON, HIGH = OFF

#define PIN_OLED_SDA    17  // Built-in 0.96" OLED I2C SDA
#define PIN_OLED_SCL    18  // Built-in 0.96" OLED I2C SCL
#define PIN_OLED_RST    21  // Built-in OLED Reset

#define PIN_LED_USER    35  // On-board user LED

/**
 * -----------------------------------------------------------------------------
 * LoRaWAN OTAA Credentials (16 Hex characters for EUI, 32 for Keys)
 * In RadioLib, uint64_t or byte arrays can be used.
 * -----------------------------------------------------------------------------
 */
// DevEUI (8 bytes): Registra este mismo valor en ChirpStack v4
static const uint8_t LORAWAN_DEV_EUI[8] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
};

// JoinEUI / AppEUI (8 bytes)
static const uint8_t LORAWAN_JOIN_EUI[8] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// AppKey (16 bytes): Llave simétrica AES-128
static const uint8_t LORAWAN_APP_KEY[16] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
};

// NwkKey (Igual a AppKey para LoRaWAN 1.0.3/1.0.4)
static const uint8_t LORAWAN_NWK_KEY[16] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
};

/**
 * -----------------------------------------------------------------------------
 * Telemetría y SDN
 * -----------------------------------------------------------------------------
 */
#define TELEMETRY_PAYLOAD_SIZE 204  // 204 bytes exactos según SDN.md
#define LORAWAN_APP_PORT       10   // Puerto para telemetría y reglas SDN
#define DEFAULT_TX_INTERVAL_S  60   // Intervalo inicial de transmisión (segundos)

#endif // HELTEC_V3_CONFIG_H
