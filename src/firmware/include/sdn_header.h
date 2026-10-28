#ifndef SDN_HEADER_H
#define SDN_HEADER_H

#include <stdint.h>
#include <Arduino.h>

/**
 * -----------------------------------------------------------------------------
 * SDN LoRaWAN Control Header Specification (RFC 7426 Adaptation)
 * Total Width: 32 bits (4 bytes) - Packed Little-Endian
 * -----------------------------------------------------------------------------
 * Bit Allocation:
 *   [0..3]   cmd_id       (4 bits) : Command instruction code (0x0..0xF)
 *   [4..6]   sf_target    (3 bits) : Target Spreading Factor (0..7 -> SF7..SF12)
 *   [7..10]  tx_power     (4 bits) : Radio Tx Power (0..15 dBm)
 *   [11..13] sample_rate  (3 bits) : Uplink interval LUT index (0..7)
 *   [14..15] payload_mode (2 bits) : Payload format (0=Full 204B, 1=Crit, 2=Diag)
 *   [16]     sleep_enable (1 bit)  : Deep Sleep state (0=Disabled, 1=Active)
 *   [17..31] param_value  (15 bits): Multipurpose dynamic argument (0..32,767)
 * -----------------------------------------------------------------------------
 */

#pragma pack(push, 1)
struct __attribute__((packed)) SdnHeader {
    uint32_t cmd_id       : 4;  // Bits 0 - 3
    uint32_t sf_target    : 3;  // Bits 4 - 6
    uint32_t tx_power     : 4;  // Bits 7 - 10
    uint32_t sample_rate  : 3;  // Bits 11 - 13
    uint32_t payload_mode : 2;  // Bits 14 - 15
    uint32_t sleep_enable : 1;  // Bit 16
    uint32_t param_value  : 15; // Bits 17 - 31
};
#pragma pack(pop)

// Ensure at compile time that the struct is exactly 4 bytes
static_assert(sizeof(SdnHeader) == 4, "SdnHeader struct size must be strictly 4 bytes (32 bits)");

// Command IDs
enum SdnCommandId : uint8_t {
    CMD_NOP_KEEP_ALIVE     = 0x00,
    CMD_RECONFIG_RADIO     = 0x01,
    CMD_SET_TEMP_THRESHOLD = 0x02,
    CMD_FORCE_SLEEP        = 0x03,
    CMD_SET_P2P_RELAY      = 0x04,
    // 0x05 - 0x0E Reserved for future SDN rules
    CMD_SYSTEM_RESET       = 0x0F
};

// Payload Modes
enum SdnPayloadMode : uint8_t {
    PAYLOAD_MODE_FULL_TELEMETRY = 0, // 204 bytes full telemetry packet
    PAYLOAD_MODE_CRITICAL       = 1, // Only alarm triggering metrics
    PAYLOAD_MODE_DIAGNOSTIC     = 2, // MCU health & battery diagnostics
    PAYLOAD_MODE_RESERVED       = 3
};

// Helper lookup functions for string representation
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
    // sf_index 0..5 -> SF7..SF12
    if (sf_index <= 5) return 7 + sf_index;
    return 7; // Default fallback to SF7
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
