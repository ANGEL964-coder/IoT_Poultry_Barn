package com.sdn.poultry.protocol;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Base64;
import java.util.HashMap;
import java.util.Map;

/**
 * Empaquetador y Desempaquetador de la Cabecera SDN (32 bits / 4 bytes)
 * Cumple estrictamente con la especificación RFC 7426 Little-Endian
 */
public class SdnHeaderPacker {

    public static final String[] CMD_NAMES = {
            "NOP / KEEP_ALIVE",      // 0
            "RECONFIG_RADIO",        // 1
            "SET_TEMP_THRESHOLD",    // 2
            "FORCE_SLEEP",           // 3
            "SET_P2P_RELAY",         // 4
            "RESERVED_5", "RESERVED_6", "RESERVED_7",
            "RESERVED_8", "RESERVED_9", "RESERVED_10", "RESERVED_11",
            "RESERVED_12", "RESERVED_13", "RESERVED_14",
            "SYSTEM_RESET"           // 15 (0x0F)
    };

    public static final String[] SF_NAMES = {
            "SF7 (DR3)", "SF8 (DR2)", "SF9 (DR1)", "SF10 (DR0)", "SF11", "SF12", "RESERVED", "RESERVED"
    };

    public static final int[] SAMPLE_RATE_SECONDS = {
            10, 60, 300, 900, 1800, 3600, 14400, 86400
    };

    public static byte[] pack(int cmdId, int sfTarget, int txPower, int sampleRate, int payloadMode, int sleepEnable, int paramValue) {
        long word32 = 0L;
        word32 |= (cmdId & 0x0FL);
        word32 |= ((sfTarget & 0x07L) << 4);
        word32 |= ((txPower & 0x0FL) << 7);
        word32 |= ((sampleRate & 0x07L) << 11);
        word32 |= ((payloadMode & 0x03L) << 14);
        word32 |= ((sleepEnable & 0x01L) << 16);
        word32 |= ((paramValue & 0x7FFFL) << 17);

        ByteBuffer buf = ByteBuffer.allocate(4);
        buf.order(ByteOrder.LITTLE_ENDIAN);
        buf.putInt((int) word32);
        return buf.array();
    }

    public static String toBase64(byte[] packedBytes) {
        return Base64.getEncoder().encodeToString(packedBytes);
    }

    public static String toHex(byte[] bytes) {
        StringBuilder sb = new StringBuilder();
        for (byte b : bytes) {
            sb.append(String.format("%02X ", b));
        }
        return sb.toString().trim();
    }

    public static Map<String, Object> unpack(byte[] bytes) {
        if (bytes == null || bytes.length < 4) {
            throw new IllegalArgumentException("Se requieren al menos 4 bytes");
        }
        ByteBuffer buf = ByteBuffer.wrap(bytes);
        buf.order(ByteOrder.LITTLE_ENDIAN);
        long word32 = Integer.toUnsignedLong(buf.getInt());

        int cmdId = (int) (word32 & 0x0F);
        int sfTarget = (int) ((word32 >> 4) & 0x07);
        int txPower = (int) ((word32 >> 7) & 0x0F);
        int sampleRate = (int) ((word32 >> 11) & 0x07);
        int payloadMode = (int) ((word32 >> 14) & 0x03);
        int sleepEnable = (int) ((word32 >> 16) & 0x01);
        int paramValue = (int) ((word32 >> 17) & 0x7FFF);

        Map<String, Object> map = new HashMap<>();
        map.put("rawWordHex", String.format("0x%08X", word32));
        map.put("cmdId", cmdId);
        map.put("cmdName", cmdId < CMD_NAMES.length ? CMD_NAMES[cmdId] : "UNKNOWN");
        map.put("sfTarget", sfTarget);
        map.put("sfName", sfTarget < SF_NAMES.length ? SF_NAMES[sfTarget] : "UNKNOWN");
        map.put("txPower", txPower);
        map.put("sampleRateIndex", sampleRate);
        map.put("sampleRateSeconds", sampleRate < SAMPLE_RATE_SECONDS.length ? SAMPLE_RATE_SECONDS[sampleRate] : 60);
        map.put("payloadMode", payloadMode);
        map.put("sleepEnable", sleepEnable);
        map.put("paramValue", paramValue);
        return map;
    }
}
