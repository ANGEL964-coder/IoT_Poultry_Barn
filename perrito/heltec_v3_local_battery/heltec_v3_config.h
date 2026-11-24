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
#define OLED_ADDR       0x3C

#define PIN_LED_USER    35  // On-board user LED
#define PIN_PRG         0   // Boton PRG integrado (LOW al presionar)

/**
 * -----------------------------------------------------------------------------
 * Bateria y Sensor de Peso
 * -----------------------------------------------------------------------------
 * En el Heltec V3:
 * - GPIO 1 (PIN_VBAT_ADC): conectado al divisor resistivo interno de bateria (ratio 4.9).
 * - GPIO 37 (PIN_ADC_CTRL): compuerta del MOSFET que energiza el divisor (HIGH = ON).
 * - GPIO 2 (PIN_WEIGHT_ADC): pin de cabecera expuesto ADC1_CH1 para acondicionador 0-3.3V.
 */
#define PIN_VBAT_ADC    1   // ADC1_CH0 (Divisor resistivo interno bateria)
#define PIN_ADC_CTRL    37  // Control MOSFET divisor de bateria
#define PIN_WEIGHT_ADC  2   // ADC1_CH1 (Entrada de senal de celda de carga 0 a 3.3V)

#define PRG_DEBOUNCE_RELEASE_MS      80   // Tiempo minimo continuo en HIGH antes de deep sleep

/**
 * -----------------------------------------------------------------------------
 * Parametros por Defecto de Calibracion de la Balanza Avicola
 * -----------------------------------------------------------------------------
 */
#define DEFAULT_CAL_SLOPE_V_PER_KG   0.0500f  // 50 mV por kg (Ej: 3.3V = 66 kg)
#define DEFAULT_CAL_ZERO_V           0.0000f  // Voltaje con tolva vacia (0V)
#define DEFAULT_HOPPER_CAPACITY_KG   50.00f   // Capacidad nominal de la tolva (kg)

/**
 * -----------------------------------------------------------------------------
 * Parametros de Bateria LiPo (400 mAh)
 * -----------------------------------------------------------------------------
 */
#define CAPACIDAD_BATERIA_MAH        400.0f
#define CONSUMO_ACTIVO_MA            38.0f    // Consumo con OLED encendido
#define CONSUMO_SLEEP_MA             12.0f    // Consumo con OLED apagado
#define DIVIDER_FACTOR               4.90f
#define FACTOR_CALIBRACION           1.00f

/**
 * -----------------------------------------------------------------------------
 * LoRaWAN OTAA Credentials (16 Hex characters for EUI, 32 for Keys)
 * In RadioLib, uint64_t or byte arrays can be used.
 * -----------------------------------------------------------------------------
 */
// DevEUI (8 bytes): Registra este mismo valor en ChirpStack v4
static const uint8_t LORAWAN_DEV_EUI[8] = {
    0x70, 0xb3, 0xd5, 0x7e, 0xd0, 0x07, 0x98, 0x5a
};

// JoinEUI / AppEUI (8 bytes)
static const uint8_t LORAWAN_JOIN_EUI[8] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// AppKey (16 bytes): Llave simetrica AES-128
static const uint8_t LORAWAN_APP_KEY[16] = {
    0x12, 0x0d, 0xc8, 0x46, 0xf9, 0xb2, 0x8f, 0x6f,
    0x45, 0x4c, 0x4c, 0x30, 0x2c, 0x04, 0xce, 0x48
};

// NwkKey (Igual a AppKey para LoRaWAN 1.0.3/1.0.4)
static const uint8_t LORAWAN_NWK_KEY[16] = {
    0x12, 0x0d, 0xc8, 0x46, 0xf9, 0xb2, 0x8f, 0x6f,
    0x45, 0x4c, 0x4c, 0x30, 0x2c, 0x04, 0xce, 0x48
};

/**
 * -----------------------------------------------------------------------------
 * Telemetria y SDN
 * -----------------------------------------------------------------------------
 */
#define TELEMETRY_PAYLOAD_SIZE 204  // 204 bytes exactos segun SDN
#define LORAWAN_APP_PORT       10   // Puerto para telemetria y reglas SDN
#define DEFAULT_TX_INTERVAL_S  60   // Intervalo de transmision (segundos)

#endif // HELTEC_V3_CONFIG_H
