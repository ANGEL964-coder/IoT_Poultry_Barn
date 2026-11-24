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
 * Interfaz UART - Sensor de Visión MaixCAM Pro (YOLOv5 Contador de Huevos)
 * -----------------------------------------------------------------------------
 * Conexión física con el conector lateral de la MaixCAM:
 *   - MaixCAM Pin A16 (UART0_TX) -> Heltec V3 GPIO 4 (RX)
 *   - MaixCAM Pin A17 (UART0_RX) <- Heltec V3 GPIO 5 (TX) [OBLIGATORIO para enviar ACK 0x06]
 *   - MaixCAM GND                -> Heltec V3 GND
 *   - Baudrate: 115200 bps 8N1
 */
#define PIN_EGG_UART_RX        4       // Pin RX en Heltec V3 (GPIO 4 <- Conectar a Pin A16 TX de MaixCAM)
#define PIN_EGG_UART_TX        5       // Pin TX en Heltec V3 (GPIO 5 -> Conectar a Pin A17 RX de MaixCAM para ACK)
#define EGG_UART_BAUDRATE      115200  // 115200 bps
#define EGG_UART_TIMEOUT_MS    120000  // Timeout de 2 minutos (la cámara envía cada 60s)

// Protocolo binario de conteo de huevos MaixCAM (CRC-16-CCITT + ARQ)
#define MAIX_FRAME_SYNC_1      0xAA
#define MAIX_FRAME_SYNC_2      0x55
#define MAIX_TIPO_CONTEO       0x01    // Trama única de conteo: [Hora, Minuto, Conteo_uint16]
#define MAIX_ACK_BYTE          0x06
#define MAIX_NACK_BYTE         0x15

/**
 * -----------------------------------------------------------------------------
 * Sensor de Peso - Circuito Acondicionador ADC
 * -----------------------------------------------------------------------------
 * En el ESP32-S3, el ADC2 está compartido con el hardware WiFi y causa conflictos.
 * Por ello, utilizamos ADC1 Canal 0 (GPIO 1), que opera de forma totalmente
 * independiente y compatible con WiFi activo (0 a 3.3V).
 */
#define PIN_WEIGHT_ADC  1   // GPIO 1 -> ADC1_CH0 (Inmune a interferencia WiFi)

/**
 * -----------------------------------------------------------------------------
 * Parámetros por Defecto de Calibración de la Tolva/Balanza Avícola
 * -----------------------------------------------------------------------------
 */
#define DEFAULT_CAL_SLOPE_V_PER_KG   0.0500f  // 50 mV por kg (Ej: 3.3V = 66 kg)
#define DEFAULT_CAL_ZERO_V           0.0000f  // Voltaje con tolva vacía (0V)
#define DEFAULT_HOPPER_CAPACITY_KG   50.00f   // Capacidad nominal de la tolva (kg)

/**
 * -----------------------------------------------------------------------------
 * Configuración Punto de Acceso WiFi (Web Interface)
 * -----------------------------------------------------------------------------
 */
#define WIFI_AP_SSID    "Balanza-Avicola-Heltec"
#define WIFI_AP_PASS    "galpon1234"
#define WEB_SERVER_PORT 80

/**
 * -----------------------------------------------------------------------------
 * LoRaWAN OTAA Credentials (16 Hex characters for EUI, 32 for Keys)
 * In RadioLib, uint64_t or byte arrays can be used.
 * -----------------------------------------------------------------------------
 */
// DevEUI (8 bytes): Registra este mismo valor en ChirpStack v4
static const uint8_t LORAWAN_DEV_EUI[8] = {
    0x44, 0xae, 0x5e, 0x17, 0xe0, 0xe1, 0x8a, 0xd8
};

// JoinEUI / AppEUI (8 bytes)
static const uint8_t LORAWAN_JOIN_EUI[8] = {
    0xc2, 0x99, 0x40, 0xf0, 0x49, 0x98, 0xec, 0xf1
};

// AppKey (16 bytes): Llave simétrica AES-128
static const uint8_t LORAWAN_APP_KEY[16] = {
    0xef, 0x03, 0x11, 0x14, 0x9e, 0xa0, 0x88, 0xa7,
    0x8f, 0xba, 0x2f, 0xa5, 0x54, 0x58, 0x55, 0x94
};

// NwkKey (Igual a AppKey para LoRaWAN 1.0.3/1.0.4)
static const uint8_t LORAWAN_NWK_KEY[16] = {
    0xef, 0x03, 0x11, 0x14, 0x9e, 0xa0, 0x88, 0xa7,
    0x8f, 0xba, 0x2f, 0xa5, 0x54, 0x58, 0x55, 0x94
};


/**
 * -----------------------------------------------------------------------------
 * Puertos de Aplicacion LoRaWAN y Telemetria
 * -----------------------------------------------------------------------------
 */
#define LORAWAN_PORT_CLIMA      10   // Puerto fPort para el nodo de Clima (Temperatura y Humedad)
#define LORAWAN_PORT_HUEVOS     12   // Puerto fPort para el nodo Contador de Huevos
#define TELEMETRY_PAYLOAD_SIZE  24   // Trama de 24 bytes para ambos nodos
#define DEFAULT_TX_INTERVAL_S   60   // Intervalo de transmision (segundos)

#endif // HELTEC_V3_CONFIG_H
