#ifndef HELTEC_V3_CONFIG_H
#define HELTEC_V3_CONFIG_H

#include <stdint.h>

/**
 * =============================================================================
 * Configuración para HELTEC WiFi LoRa 32 V3 (ESP32-S3 + Semtech SX1262)
 * =============================================================================
 */

// Subbanda para US915 (El Gateway Laird RG1 usa Subbanda 2: canales 8 al 15)
#define LORAWAN_SUBBAND 2

/**
 * -----------------------------------------------------------------------------
 * Mapeo de Pines Físicos Internos del Heltec V3
 * -----------------------------------------------------------------------------
 */
#define PIN_LORA_NSS    8   // Chip Select SX1262
#define PIN_LORA_SCK    9   // SPI Clock
#define PIN_LORA_MOSI   10  // SPI MOSI
#define PIN_LORA_MISO   11  // SPI MISO
#define PIN_LORA_RST    12  // Reset SX1262
#define PIN_LORA_BUSY   13  // Indicador Busy (OBLIGATORIO en SX126x)
#define PIN_LORA_DIO1   14  // Interrupción DIO1 (Packet Rx/Tx)
#define PIN_VEXT_CTL    36  // Control de energía Vext (LOW = Encendido)
#define PIN_LED_USER    35  // LED blanco en la placa

/**
 * -----------------------------------------------------------------------------
 * Credenciales LoRaWAN OTAA (Deben coincidir con ChirpStack v4)
 * -----------------------------------------------------------------------------
 */
// 1. Device EUI (DevEUI): 16 dígitos hex (8 bytes)
// Ejemplo: 0x0102030405060708ULL
static const uint64_t LORAWAN_DEV_EUI = 0x0102030405060708ULL;

// 2. Join EUI (AppEUI): 16 dígitos hex (8 bytes). En ChirpStack suele ser 0
static const uint64_t LORAWAN_JOIN_EUI = 0x0000000000000000ULL;

// 3. Application Key (AppKey): 32 dígitos hex separados en 16 bytes
static const uint8_t LORAWAN_APP_KEY[16] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
};

// 4. Network Key (NwkKey): En LoRaWAN 1.0.3 es igual a AppKey
static const uint8_t LORAWAN_NWK_KEY[16] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
};

/**
 * -----------------------------------------------------------------------------
 * Parámetros de Telemetría SDN
 * -----------------------------------------------------------------------------
 */
#define TELEMETRY_PAYLOAD_SIZE 204  // 204 bytes exactos según especificación SDN.md
#define LORAWAN_APP_PORT       10   // Puerto LoRaWAN (fPort)
#define DEFAULT_TX_INTERVAL_S  60   // Intervalo de reporte inicial en segundos

#endif // HELTEC_V3_CONFIG_H
