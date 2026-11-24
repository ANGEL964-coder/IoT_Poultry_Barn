package com.sdn.poultry.protocol;

import com.sdn.poultry.model.PoultryTelemetry;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.time.Instant;
import java.time.format.DateTimeFormatter;

public class TelemetryDecoder {

    /**
     * Decodes a telemetry payload from a scale device, climate sensor, or egg counter device.
     *
     * Payload structure:
     * - Egg counter 24-byte payload (EggNodeTelemetry): seq, uptime_ms, battery_mv, egg_count, cam_hour, cam_minute, uart_status, total_detections
     * - TelemetryHeader (24 bytes): seq, uptime_ms, battery_mv, temperature, humidity, pressure
     * - PoultryScaleData (24 bytes) OR EggCounterData (24 bytes)
     * - Padding up to 204 bytes (FULL_TELEMETRY mode)
     *
     * @param payloadBytes The raw payload bytes
     * @param hopperCapacityKg Capacity for hopper percentage calculation (scale devices)
     * @param devEui Device EUI to determine payload type (scale vs egg counter)
     * @param fPort LoRaWAN fPort used for this uplink
     * @return Decoded PoultryTelemetry
     */
    public static PoultryTelemetry decode(byte[] payloadBytes, float hopperCapacityKg, String devEui, int fPort) {
        if (payloadBytes == null || payloadBytes.length < 24) {
            throw new IllegalArgumentException("Payload insuficiente (< 24 bytes)");
        }

        ByteBuffer buf = ByteBuffer.wrap(payloadBytes);
        buf.order(ByteOrder.LITTLE_ENDIAN);

        PoultryTelemetry telem = new PoultryTelemetry();
        telem.setPayloadLength(payloadBytes.length);
        telem.setTimestamp(DateTimeFormatter.ISO_INSTANT.format(Instant.now()));

        // Hex preview
        StringBuilder hexSb = new StringBuilder();
        int previewLen = Math.min(payloadBytes.length, 32);
        for (int i = 0; i < previewLen; i++) {
            hexSb.append(String.format("%02X ", payloadBytes[i]));
        }
        if (payloadBytes.length > 32) {
            hexSb.append("...");
        }
        telem.setPayloadHex(hexSb.toString().trim());

        boolean isEggCounter = isEggCounterDevice(devEui, fPort);

        // Case 1: Exclusive 24-byte Egg Node payload (EggNodeTelemetry from main_egg.ino)
        // Bytes 0-3:   uint32_t seq
        // Bytes 4-7:   uint32_t uptime_ms
        // Bytes 8-11:  float    battery_mv
        // Bytes 12-15: uint32_t egg_count
        // Byte 16:     uint8_t  cam_hour
        // Byte 17:     uint8_t  cam_minute
        // Bytes 18-19: uint16_t uart_status
        // Bytes 20-23: uint32_t total_detections
        if (isEggCounter && payloadBytes.length == 24) {
            telem.setSeq(Integer.toUnsignedLong(buf.getInt()));
            telem.setUptimeMs(Integer.toUnsignedLong(buf.getInt()));
            telem.setBatteryMv(buf.getFloat());
            long eggs = Integer.toUnsignedLong(buf.getInt());
            telem.setTotalEggs(eggs);
            telem.setCamHour(buf.get() & 0xFF);
            telem.setCamMinute(buf.get() & 0xFF);
            telem.setUartStatus(buf.getShort() & 0xFFFF);
            long detections = Integer.toUnsignedLong(buf.getInt());
            telem.setTotalDetections(detections);
            return telem;
        }

        // Case 2: Standard TelemetryHeader (24 Bytes) for Climate / Scale / 48B Egg counter
        telem.setSeq(Integer.toUnsignedLong(buf.getInt()));
        telem.setUptimeMs(Integer.toUnsignedLong(buf.getInt()));
        telem.setBatteryMv(buf.getFloat());
        if (!isEggCounter) {
            float temp = buf.getFloat();
            float hum = buf.getFloat();
            float press = buf.getFloat();
            if (Math.abs(temp) > 0.001f) {
                telem.setTemperature(temp);
            }
            if (hum > 0.001f) {
                telem.setHumidity(hum);
            }
            if (press > 0.001f) {
                telem.setPressure(press);
            }
        } else {
            buf.position(buf.position() + 12);
        }

        if (payloadBytes.length >= 48) {
            if (isEggCounter) {
                parseEggCounterData(buf, telem);
            } else if (!hasDiagnosticPadding(payloadBytes, telem.getSeq())) {
                parseScaleData(buf, telem, hopperCapacityKg);
            }
        }

        return telem;
    }

    /**
     * Legacy decode method for backward compatibility - defaults to scale data
     */
    public static PoultryTelemetry decode(byte[] payloadBytes, float hopperCapacityKg) {
        return decode(payloadBytes, hopperCapacityKg, "", 0);
    }

    /**
     * Determines if the device is an egg counter based on DevEUI or fPort.
     */
    public static boolean isEggCounterDevice(String devEui, int fPort) {
        if (devEui != null && !devEui.isEmpty()) {
            String lowerDevEui = devEui.toLowerCase();
            if (lowerDevEui.contains("70b3d57ed007985b") ||
                lowerDevEui.contains("44ae5e17e0e18ad8") ||
                lowerDevEui.contains("lora_huevos") ||
                lowerDevEui.contains("vision") ||
                lowerDevEui.contains("egg") ||
                lowerDevEui.contains("huevo") ||
                lowerDevEui.contains("cam")) {
                return true;
            }
        }
        if (fPort == 11 || fPort == 12) {
            return true;
        }
        return false;
    }

    // The original 204-byte diagnostic firmware fills everything after the header
    // with (offset + sequence) instead of a PoultryScaleData record.
    private static boolean hasDiagnosticPadding(byte[] payload, long seq) {
        if (payload.length != 204) return false;
        for (int i = 24; i < 48; i++) {
            if (payload[i] != (byte) (i + seq)) return false;
        }
        return true;
    }

    private static void parseScaleData(ByteBuffer buf, PoultryTelemetry telem, float hopperCapacityKg) {
        telem.setWeightKg(buf.getFloat());
        telem.setGrossWeightKg(buf.getFloat());
        telem.setVoltageV(buf.getFloat());
        telem.setAdcRaw(buf.getShort() & 0xFFFF);
        telem.setCalSlope(buf.getFloat());
        telem.setCalZero(buf.getFloat());
        telem.setTimeOnAirMs(buf.getShort() & 0xFFFF);

        // Nivel de Tolva porcentual (0 - 100%)
        float cap = (hopperCapacityKg > 0.01f) ? hopperCapacityKg : 20.0f;
        float pct = (telem.getWeightKg() / cap) * 100.0f;
        if (pct < 0.0f) pct = 0.0f;
        if (pct > 100.0f) pct = 100.0f;
        telem.setHopperPercentage(pct);
    }

    private static void parseEggCounterData(ByteBuffer buf, PoultryTelemetry telem) {
        telem.setTotalEggs(Integer.toUnsignedLong(buf.getInt()));
        telem.setImageSizeBytes(Integer.toUnsignedLong(buf.getInt()));
        telem.setCamHour(buf.get() & 0xFF);
        telem.setCamMinute(buf.get() & 0xFF);
        telem.setEggsPerMinute(buf.getFloat());
        telem.setUartStatus(buf.get() & 0xFF);
        telem.setEggTimeOnAirMs(buf.getShort() & 0xFFFF);
    }
}
