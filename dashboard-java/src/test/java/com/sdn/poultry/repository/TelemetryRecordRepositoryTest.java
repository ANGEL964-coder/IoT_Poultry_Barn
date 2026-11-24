package com.sdn.poultry.repository;

import com.sdn.poultry.entity.TelemetryRecord;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.orm.jpa.DataJpaTest;
import org.springframework.data.domain.PageRequest;
import org.springframework.test.context.TestPropertySource;

import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.List;
import java.util.Optional;

import static org.assertj.core.api.Assertions.assertThat;

@DataJpaTest
@TestPropertySource(locations = "classpath:application-test.properties")
public class TelemetryRecordRepositoryTest {

    @Autowired
    private TelemetryRecordRepository telemetryRecordRepository;

    @Test
    @DisplayName("Should persist and retrieve complete telemetry record with scale data")
    public void testSaveAndFindFullTelemetry() {
        TelemetryRecord record = new TelemetryRecord();
        record.setDevEui("70b3d57ed007985a");
        record.setSeq(101L);
        record.setUptimeMs(500000L);
        record.setBatteryMv(3295.0f);
        record.setTemperature(26.4f);
        record.setHumidity(62.1f);
        record.setPressure(1012.8f);
        record.setWeightKg(14.85f);
        record.setGrossWeightKg(16.20f);
        record.setVoltageV(1.242f);
        record.setAdcRaw(1540);
        record.setCalSlope(0.0500f);
        record.setCalZero(0.5000f);
        record.setTimeOnAirMs(62);
        record.setHopperPercentage(74.25f);
        record.setRssi(-82);
        record.setSnr(9.5f);
        record.setPayloadMode(1);
        record.setRawPayloadBase64("dGVzdFBheWxvYWQ=");
        record.setFCnt(205L);
        record.setFPort(10);
        record.setGatewayId("0016c001f1234567");
        record.setTimestamp(Instant.now());

        TelemetryRecord saved = telemetryRecordRepository.save(record);
        assertThat(saved.getId()).isNotNull();

        Optional<TelemetryRecord> latest = telemetryRecordRepository.findTopByDevEuiOrderByTimestampDesc("70b3d57ed007985a");
        assertThat(latest).isPresent();
        assertThat(latest.get().getSeq()).isEqualTo(101L);
        assertThat(latest.get().getTemperature()).isEqualTo(26.4f);
        assertThat(latest.get().getWeightKg()).isEqualTo(14.85f);
        assertThat(latest.get().getHopperPercentage()).isEqualTo(74.25f);
        assertThat(latest.get().getRssi()).isEqualTo(-82);
    }

    @Test
    @DisplayName("Should support diagnostic frames (Mode 2) with nullable scale columns")
    public void testDiagnosticModeNullableScaleColumns() {
        TelemetryRecord diagRecord = new TelemetryRecord();
        diagRecord.setDevEui("70b3d57ed007985a");
        diagRecord.setSeq(200L);
        diagRecord.setUptimeMs(600000L);
        diagRecord.setBatteryMv(3310.0f);
        diagRecord.setTemperature(24.1f);
        diagRecord.setHumidity(55.0f);
        diagRecord.setPressure(1015.0f);
        // Null scale fields for 24B frame
        diagRecord.setWeightKg(null);
        diagRecord.setGrossWeightKg(null);
        diagRecord.setVoltageV(null);
        diagRecord.setAdcRaw(null);
        diagRecord.setCalSlope(null);
        diagRecord.setCalZero(null);
        diagRecord.setTimeOnAirMs(null);
        diagRecord.setHopperPercentage(null);
        diagRecord.setPayloadMode(2);
        diagRecord.setTimestamp(Instant.now());

        TelemetryRecord saved = telemetryRecordRepository.save(diagRecord);
        assertThat(saved.getId()).isNotNull();
        assertThat(saved.getWeightKg()).isNull();
        assertThat(saved.getAdcRaw()).isNull();
        assertThat(saved.getPayloadMode()).isEqualTo(2);
    }

    @Test
    @DisplayName("Should paginate telemetry records ordered by timestamp descending")
    public void testPagination() {
        String devEui = "70b3d57ed0079801";
        Instant now = Instant.now();

        for (int i = 1; i <= 10; i++) {
            TelemetryRecord r = new TelemetryRecord();
            r.setDevEui(devEui);
            r.setSeq((long) i);
            r.setUptimeMs(i * 1000L);
            r.setBatteryMv(3300.0f);
            r.setTemperature(25.0f + i * 0.1f);
            r.setHumidity(60.0f);
            r.setPressure(1013.0f);
            r.setTimestamp(now.minus(10 - i, ChronoUnit.MINUTES));
            telemetryRecordRepository.save(r);
        }

        List<TelemetryRecord> page = telemetryRecordRepository.findByDevEuiOrderByTimestampDesc(
                devEui, PageRequest.of(0, 3)
        );

        assertThat(page).hasSize(3);
        // First item should be the newest (seq = 10)
        assertThat(page.get(0).getSeq()).isEqualTo(10L);
        assertThat(page.get(1).getSeq()).isEqualTo(9L);
        assertThat(page.get(2).getSeq()).isEqualTo(8L);
    }
}
