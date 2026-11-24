#ifndef SDN_HEADER_H
#define SDN_HEADER_H

#include <stdint.h>
#include <Arduino.h>

/**
 * -----------------------------------------------------------------------------
 * SDN LoRaWAN Control Header Specification (RFC 7426 Adaptation)
 * Total Width: 32 bits (4 bytes) - Packed Little-Endian
 * -----------------------------------------------------------------------------
 */

#pragma pack(push, 1)
struct __attribute__((packed)) SdnHeader {
    uint32_t cmd_id       : 4;  // Bits 0 - 3   (0..15)
    uint32_t sf_target    : 3;  // Bits 4 - 6   (0..7 -> SF7..SF12)
    uint32_t tx_power     : 4;  // Bits 7 - 10  (0..15 dBm)
    uint32_t sample_rate  : 3;  // Bits 11 - 13 (0..7 LUT index)
    uint32_t payload_mode : 2;  // Bits 14 - 15 (0..3)
    uint32_t sleep_enable : 1;  // Bit 16       (0..1)
    uint32_t param_value  : 15; // Bits 17 - 31 (0..32,767)
};

// Cabecera estándar de telemetría SDN (24 bytes)
struct __attribute__((packed)) TelemetryHeader {
    uint32_t seq;
    uint32_t uptime_ms;
    float    battery_mv;
    float    temperature;
    float    humidity;
    float    pressure;
};

// Datos específicos de la Balanza de Alimento Avícola (24 bytes)
struct __attribute__((packed)) PoultryScaleData {
    float    weight_kg;        // Peso neto de comida en kg
    float    gross_weight_kg;  // Peso bruto en kg
    float    voltage_v;        // Voltaje del circuito acondicionador (0 - 3.3V)
    uint16_t adc_raw;          // Lectura cruda ADC (0 - 4095)
    float    cal_slope;        // Pendiente de calibración (V/kg)
    float    cal_zero;         // Voltaje de Cero (V)
    uint16_t time_on_air_ms;   // Tiempo de transmisión en el aire (ToA) en ms
};
#pragma pack(pop)

static_assert(sizeof(SdnHeader) == 4, "SdnHeader struct size must be strictly 4 bytes (32 bits)");
static_assert(sizeof(TelemetryHeader) == 24, "TelemetryHeader struct size must be strictly 24 bytes");
static_assert(sizeof(PoultryScaleData) == 24, "PoultryScaleData struct size must be strictly 24 bytes");

// Command IDs
enum SdnCommandId : uint8_t {
    CMD_NOP_KEEP_ALIVE     = 0x00,
    CMD_RECONFIG_RADIO     = 0x01,
    CMD_SET_TEMP_THRESHOLD = 0x02,
    CMD_FORCE_SLEEP        = 0x03,
    CMD_SET_P2P_RELAY      = 0x04,
    CMD_SYSTEM_RESET       = 0x0F
};

// Payload Modes
enum SdnPayloadMode : uint8_t {
    PAYLOAD_MODE_FULL_TELEMETRY = 0,
    PAYLOAD_MODE_CRITICAL       = 1,
    PAYLOAD_MODE_DIAGNOSTIC     = 2,
    PAYLOAD_MODE_RESERVED       = 3
};

inline const char* getSdnCommandName(uint8_t cmd) {
    switch (cmd) {
        case CMD_NOP_KEEP_ALIVE:     return "NOP / KEEP_ALIVE";
        case CMD_RECONFIG_RADIO:     return "RECONFIG_RADIO";
        case CMD_SET_TEMP_THRESHOLD: return "SET_TEMP_THRESHOLD";
        case CMD_FORCE_SLEEP:        return "FORCE_SLEEP";
        case CMD_SET_P2P_RELAY:      return "SET_P2P_RELAY";
        case CMD_SYSTEM_RESET:       return "SYSTEM_RESET";
        default:                     return "UNKNOWN_CMD";
    }
}

inline uint8_t getSpreadingFactorValue(uint8_t sf_index) {
    if (sf_index <= 5) return 7 + sf_index;
    return 7;
}

inline uint32_t getSampleRateSeconds(uint8_t rate_index) {
    static const uint32_t lut[8] = {10, 60, 300, 900, 1800, 3600, 14400, 86400};
    return lut[rate_index & 0x07];
}

inline const char* getPayloadModeName(uint8_t mode) {
    switch (mode) {
        case PAYLOAD_MODE_FULL_TELEMETRY: return "FULL_TELEMETRY (204 Bytes)";
        case PAYLOAD_MODE_CRITICAL:       return "CRITICAL_ALARM";
        case PAYLOAD_MODE_DIAGNOSTIC:     return "DIAGNOSTIC_HEALTH";
        default:                          return "RESERVED";
    }
}

#endif // SDN_HEADER_H
