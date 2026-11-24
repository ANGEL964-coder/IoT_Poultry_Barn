package com.sdn.poultry.repository;

import com.sdn.poultry.entity.*;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.context.TestPropertySource;

import java.time.Instant;
import java.util.Optional;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest
@TestPropertySource(properties = {
        "spring.datasource.url=jdbc:postgresql://localhost:5432/chirpstack",
        "spring.datasource.username=chirpstack",
        "spring.datasource.password=chirpstack",
        "spring.datasource.driver-class-name=org.postgresql.Driver",
        "spring.jpa.hibernate.ddl-auto=update",
        "spring.jpa.properties.hibernate.dialect=org.hibernate.dialect.PostgreSQLDialect",
        "app.mqtt.broker-url=tcp://127.0.0.1:1883"
})
public class PostgresLiveIntegrationTest {

    @Autowired
    private DeviceRepository deviceRepository;

    @Autowired
    private ApiKeyRepository apiKeyRepository;

    @Autowired
    private TelemetryRecordRepository telemetryRecordRepository;

    @Autowired
    private SdnRuleEventRepository sdnRuleEventRepository;

    @Autowired
    private AuditLogRepository auditLogRepository;

    @Test
    @DisplayName("Verify live PostgreSQL connection and coexistence of plural tables with ChirpStack tables")
    public void testLivePostgresCoexistence() {
        // 1. Device
        String testDevEui = "70b3d57ed0079899";
        Device dev = new Device(testDevEui, "Test Poultry Node", "44d003ef-2bd1-480a-84cf-87ff6dc27506", "ONLINE");
        deviceRepository.save(dev);

        Optional<Device> loadedDev = deviceRepository.findById(testDevEui);
        assertThat(loadedDev).isPresent();
        assertThat(loadedDev.get().getName()).isEqualTo("Test Poultry Node");

        // 2. ApiKey
        String hash = "live_test_hash_" + System.currentTimeMillis();
        apiKeyRepository.findByKeyPrefix("sdn_live_test").ifPresent(apiKeyRepository::delete);
        ApiKey apiKey = new ApiKey(hash, "sdn_live_test", "Live integration test key");
        apiKeyRepository.save(apiKey);

        Optional<ApiKey> loadedKey = apiKeyRepository.findByKeyHash(hash);
        assertThat(loadedKey).isPresent();

        // 3. TelemetryRecord
        TelemetryRecord rec = new TelemetryRecord();
        rec.setDevEui(testDevEui);
        rec.setSeq(1L);
        rec.setUptimeMs(10000L);
        rec.setBatteryMv(3300.0f);
        rec.setTemperature(24.5f);
        rec.setHumidity(58.0f);
        rec.setPressure(1012.0f);
        rec.setWeightKg(12.5f);
        rec.setTimestamp(Instant.now());
        telemetryRecordRepository.save(rec);

        Optional<TelemetryRecord> loadedRec = telemetryRecordRepository.findTopByDevEuiOrderByTimestampDesc(testDevEui);
        assertThat(loadedRec).isPresent();
        assertThat(loadedRec.get().getWeightKg()).isEqualTo(12.5f);

        // 4. SdnRuleEvent
        SdnRuleEvent sdnEvent = new SdnRuleEvent();
        sdnEvent.setDevEui(testDevEui);
        sdnEvent.setCmdId(1);
        sdnEvent.setSfTarget(0);
        sdnEvent.setTxPower(14);
        sdnEvent.setSampleRate(0);
        sdnEvent.setPayloadMode(1);
        sdnEvent.setSleepEnable(false);
        sdnEvent.setParamValue(0);
        sdnEvent.setRawWordHex("0x00000701");
        sdnEvent.setBase64Payload("AQcAAA==");
        sdnEvent.setStatus("ENQUEUED_IN_CHIRPSTACK");
        sdnEvent.setSentAt(Instant.now());
        sdnRuleEventRepository.save(sdnEvent);

        assertThat(sdnRuleEventRepository.findByDevEuiOrderBySentAtDesc(testDevEui)).isNotEmpty();

        // 5. AuditLog
        AuditLog audit = new AuditLog("TEST_ACTION", "Testing live DB", "details=OK", "127.0.0.1");
        auditLogRepository.save(audit);
        assertThat(auditLogRepository.findByActionOrderByTimestampDesc("TEST_ACTION")).isNotEmpty();

        // Clean up test records
        deviceRepository.deleteById(testDevEui);
        apiKeyRepository.delete(apiKey);
        telemetryRecordRepository.delete(rec);
        sdnRuleEventRepository.delete(sdnEvent);
        auditLogRepository.delete(audit);
    }
}
