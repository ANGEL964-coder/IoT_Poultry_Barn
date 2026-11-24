package com.sdn.poultry.protocol;

import com.sdn.poultry.model.PoultryTelemetry;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

public class TelemetryDecoderTest {

    @Test
    @DisplayName("Should decode 48-byte Little-Endian critical payload")
    public void testDecode48BytesCriticalPayload() {
        ByteBuffer buf = ByteBuffer.allocate(48).order(ByteOrder.LITTLE_ENDIAN);
        buf.putInt(100);          // seq
        buf.putInt(240000);       // uptime_ms
        buf.putFloat(3315.0f);    // battery_mv
        buf.putFloat(25.8f);      // temperature
        buf.putFloat(60.5f);      // humidity
        buf.putFloat(1013.25f);   // pressure
        buf.putFloat(15.5f);      // weight_kg
        buf.putFloat(17.2f);      // gross_weight_kg
        buf.putFloat(1.275f);     // voltage_v
        buf.putShort((short) 1580); // adc_raw
        buf.putFloat(0.0500f);    // cal_slope
        buf.putFloat(0.5000f);    // cal_zero
        buf.putShort((short) 62); // time_on_air_ms

        PoultryTelemetry telem = TelemetryDecoder.decode(buf.array(), 20.0f);

        assertThat(telem.getSeq()).isEqualTo(100L);
        assertThat(telem.getUptimeMs()).isEqualTo(240000L);
        assertThat(telem.getBatteryMv()).isEqualTo(3315.0f);
        assertThat(telem.getTemperature()).isEqualTo(25.8f);
        assertThat(telem.getHumidity()).isEqualTo(60.5f);
        assertThat(telem.getPressure()).isEqualTo(1013.25f);
        assertThat(telem.getWeightKg()).isEqualTo(15.5f);
        assertThat(telem.getGrossWeightKg()).isEqualTo(17.2f);
        assertThat(telem.getVoltageV()).isEqualTo(1.275f);
        assertThat(telem.getAdcRaw()).isEqualTo(1580);
        assertThat(telem.getTimeOnAirMs()).isEqualTo(62);
        assertThat(telem.getHopperPercentage()).isEqualTo((15.5f / 20.0f) * 100.0f);
    }

    @Test
    @DisplayName("Should decode 24-byte Little-Endian diagnostic payload without scale data")
    public void testDecode24BytesDiagnosticPayload() {
        ByteBuffer buf = ByteBuffer.allocate(24).order(ByteOrder.LITTLE_ENDIAN);
        buf.putInt(1);
        buf.putInt(5000);
        buf.putFloat(3250.0f);
        buf.putFloat(21.5f);
        buf.putFloat(48.0f);
        buf.putFloat(1015.0f);

        PoultryTelemetry telem = TelemetryDecoder.decode(buf.array(), 20.0f);

        assertThat(telem.getSeq()).isEqualTo(1L);
        assertThat(telem.getTemperature()).isEqualTo(21.5f);
        assertThat(telem.getPayloadLength()).isEqualTo(24);
        assertThat(telem.isClimateAvailable()).isTrue();
        assertThat(telem.isScaleAvailable()).isFalse();
        assertThat(telem.getWeightKg()).isNull();
    }

    @Test
    @DisplayName("Should decode 24-byte Egg Node payload (EggNodeTelemetry)")
    public void testDecode24BytesEggPayload() {
        ByteBuffer buf = ByteBuffer.allocate(24).order(ByteOrder.LITTLE_ENDIAN);
        buf.putInt(5);            // seq
        buf.putInt(300000);       // uptime_ms
        buf.putFloat(3299.5f);    // battery_mv
        buf.putInt(14);           // egg_count
        buf.put((byte) 10);       // cam_hour
        buf.put((byte) 30);       // cam_minute
        buf.putShort((short) 1);  // uart_status
        buf.putInt(5);            // total_detections

        PoultryTelemetry telem = TelemetryDecoder.decode(buf.array(), 20.0f, "44ae5e17e0e18ad8", 12);

        assertThat(telem.getSeq()).isEqualTo(5L);
        assertThat(telem.getUptimeMs()).isEqualTo(300000L);
        assertThat(telem.getBatteryMv()).isEqualTo(3299.5f);
        assertThat(telem.getTotalEggs()).isEqualTo(14L);
        assertThat(telem.getCamHour()).isEqualTo(10);
        assertThat(telem.getCamMinute()).isEqualTo(30);
        assertThat(telem.getUartStatus()).isEqualTo(1);
        assertThat(telem.getTotalDetections()).isEqualTo(5L);
        assertThat(telem.getImageSizeBytes()).isNull();
        assertThat(telem.getEggsPerMinute()).isNull();
        assertThat(telem.isVisionAvailable()).isTrue();
        assertThat(telem.getTemperature()).isNull();
    }

    @Test
    void distinguishesEmptyHopperFromDiagnosticPadding() {
        byte[] scale = new byte[48];
        PoultryTelemetry empty = TelemetryDecoder.decode(scale, 20);
        assertThat(empty.isScaleAvailable()).isTrue();
        assertThat(empty.getWeightKg()).isZero();
        assertThat(empty.getHopperPercentage()).isZero();
        assertThat(empty.isVisionAvailable()).isFalse();

        byte[] diagnostic = new byte[204];
        for (int offset = 24; offset < diagnostic.length; offset++) diagnostic[offset] = (byte) offset;
        PoultryTelemetry climate = TelemetryDecoder.decode(diagnostic, 20);
        assertThat(climate.isClimateAvailable()).isTrue();
        assertThat(climate.isScaleAvailable()).isFalse();
        assertThat(climate.getWeightKg()).isNull();
    }

    @Test
    @DisplayName("Should throw IllegalArgumentException if payload is smaller than 24 bytes")
    public void testRejectShortPayload() {
        byte[] shortBytes = new byte[23];
        assertThatThrownBy(() -> TelemetryDecoder.decode(shortBytes, 20.0f))
                .isInstanceOf(IllegalArgumentException.class)
                .hasMessageContaining("Payload insuficiente");
    }
}
