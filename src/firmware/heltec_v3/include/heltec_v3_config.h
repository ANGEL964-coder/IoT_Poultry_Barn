#ifndef HELTEC_V3_CONFIG_H
#define HELTEC_V3_CONFIG_H

#include <stdint.h>

#define LORAWAN_SUBBAND 2

#define PIN_LORA_NSS    8
#define PIN_LORA_SCK    9
#define PIN_LORA_MOSI   10
#define PIN_LORA_MISO   11
#define PIN_LORA_RST    12
#define PIN_LORA_BUSY   13
#define PIN_LORA_DIO1   14
#define PIN_VEXT_CTL    36
#define PIN_LED_USER    35

static const uint64_t LORAWAN_DEV_EUI = 0x0102030405060708ULL;
static const uint64_t LORAWAN_JOIN_EUI = 0x0000000000000000ULL;

static const uint8_t LORAWAN_APP_KEY[16] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
};

static const uint8_t LORAWAN_NWK_KEY[16] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
};

#define TELEMETRY_PAYLOAD_SIZE 204
#define LORAWAN_APP_PORT       10
#define DEFAULT_TX_INTERVAL_S  60

#endif // HELTEC_V3_CONFIG_H
