package com.sdn.poultry.service;

import com.sdn.poultry.entity.*;
import com.sdn.poultry.model.PoultryTelemetry;
import com.sdn.poultry.model.SdnRuleRequest;
import com.sdn.poultry.repository.*;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.orm.jpa.DataJpaTest;
import org.springframework.test.context.TestPropertySource;

import java.util.List;
import java.util.Optional;

import static org.assertj.core.api.Assertions.assertThat;

@DataJpaTest
@TestPropertySource(locations = "classpath:application-test.properties")
public class PersistenceServiceTest {

    @Autowired
    private DeviceRepository deviceRepository;

    @Autowired
    private TelemetryRecordRepository telemetryRecordRepository;

    @Autowired
    private SdnRuleEventRepository sdnRuleEventRepository;

    @Autowired
    private AuditLogRepository auditLogRepository;

    @Autowired
    private ApiKeyRepository apiKeyRepository;

    private PersistenceService persistenceService;

    @BeforeEach
    public void setUp() {
        persistenceService = new PersistenceService(
                deviceRepository,
                telemetryRecordRepository,
                sdnRuleEventRepository,
                auditLogRepository,
                apiKeyRepository
        );
    }

    @Test
    @DisplayName("Should upsert device and save telemetry record for Mode 1 (Critical 48B)")
    public void testSaveTelemetryAndUpsertDeviceMode1() {
        PoultryTelemetry telem = new PoultryTelemetry();
        telem.setDevEui("70B3D57ED007985A");
        telem.setDeviceName("heltec-galpon-01");
        telem.setSeq(10L);
        telem.setUptimeMs(120000L);
        telem.setBatteryMv(3312.0f);
        telem.setTemperature(25.5f);
        telem.setHumidity(64.2f);
        telem.setPressure(1013.1f);
        telem.setWeightKg(18.4f);
        telem.setGrossWeightKg(20.0f);
        telem.setVoltageV(1.42f);
        telem.setAdcRaw(1760);
        telem.setCalSlope(0.0500f);
        telem.setCalZero(0.5000f);
        telem.setTimeOnAirMs(62);
        telem.setHopperPercentage(92.0f);
        telem.setPayloadLength(48);
        telem.setRssi(-78.0);
        telem.setSnr(8.5);
        telem.setFCnt(55L);
        telem.setFPort(10);
        telem.setGatewayId("0016c001f1234567");

        String rawB64 = "ABCD12345678";
        String appId = "44d003ef-2bd1-480a-84cf-87ff6dc27506";

        TelemetryRecord saved = persistenceService.saveTelemetryAndUpsertDevice(telem, rawB64, appId);

        assertThat(saved).isNotNull();
        assertThat(saved.getId()).isNotNull();
        assertThat(saved.getDevEui()).isEqualTo("70b3d57ed007985a");
        assertThat(saved.getWeightKg()).isEqualTo(18.4f);
        assertThat(saved.getPayloadMode()).isEqualTo(1); // Mode 1: CRITICAL
        assertThat(saved.getRssi()).isEqualTo(-78);
        assertThat(saved.getRawPayloadBase64()).isEqualTo(rawB64);

        // Verify Device was created in devices table
        Optional<Device> devOpt = persistenceService.getDevice("70b3d57ed007985a");
        assertThat(devOpt).isPresent();
        Device device = devOpt.get();
        assertThat(device.getDevEui()).isEqualTo("70b3d57ed007985a");
        assertThat(device.getName()).isEqualTo("heltec-galpon-01");
        assertThat(device.getApplicationId()).isEqualTo(appId);
        assertThat(device.getStatus()).isEqualTo("ONLINE");
        assertThat(device.getCalSlope()).isEqualTo(0.0500f);
        assertThat(device.getCalZero()).isEqualTo(0.5000f);

        // Second telemetry message from same device updates existing record rather than duplicating
        telem.setSeq(11L);
        telem.setTemperature(26.0f);
        persistenceService.saveTelemetryAndUpsertDevice(telem, rawB64, appId);

        List<Device> allDevices = persistenceService.getAllDevices();
        assertThat(allDevices).hasSize(1);
        assertThat(persistenceService.getRecentTelemetry("70b3d57ed007985a", 10)).hasSize(2);
    }

    @Test
    @DisplayName("Should handle Mode 2 (Diagnostic 24B) with null scale data")
    public void testSaveTelemetryMode2Diagnostic() {
        PoultryTelemetry diag = new PoultryTelemetry();
        diag.setDevEui("70b3d57ed0079802");
        diag.setDeviceName("node-diag");
        diag.setSeq(1L);
        diag.setUptimeMs(1000L);
        diag.setBatteryMv(3300.0f);
        diag.setTemperature(22.0f);
        diag.setHumidity(50.0f);
        diag.setPressure(1010.0f);
        diag.setPayloadLength(24);

        TelemetryRecord saved = persistenceService.saveTelemetryAndUpsertDevice(diag, "24B_PAYLOAD", "app-1");

        assertThat(saved).isNotNull();
        assertThat(saved.getPayloadMode()).isEqualTo(2); // Mode 2: DIAGNOSTIC
        assertThat(saved.getWeightKg()).isNull();
        assertThat(saved.getGrossWeightKg()).isNull();
        assertThat(saved.getVoltageV()).isNull();
        assertThat(saved.getAdcRaw()).isNull();
    }

    @Test
    @DisplayName("Should save SDN downlink rule event and log audit entry")
    public void testSaveDownlinkEventAndAudit() {
        SdnRuleRequest req = new SdnRuleRequest();
        req.setDevEui("70B3D57ED007985A");
        req.setAppId("44d003ef-2bd1-480a-84cf-87ff6dc27506");
        req.setCmdId(1); // RECONFIG_RADIO
        req.setSfTarget(3); // SF10
        req.setTxPower(14);
        req.setSampleRate(1); // 60s
        req.setPayloadMode(1); // CRITICAL
        req.setSleepEnable(0);
        req.setParamValue(0);

        String hex = "0x00000731";
        String b64 = "MQcAAA==";
        String topic = "application/44d003ef-2bd1-480a-84cf-87ff6dc27506/device/70b3d57ed007985a/command/down";

        SdnRuleEvent event = persistenceService.saveDownlinkEvent(req, hex, b64, topic, "ENQUEUED_IN_CHIRPSTACK");

        assertThat(event).isNotNull();
        assertThat(event.getId()).isNotNull();
        assertThat(event.getDevEui()).isEqualTo("70b3d57ed007985a");
        assertThat(event.getCmdName()).isEqualTo("RECONFIG_RADIO");
        assertThat(event.getSfName()).isEqualTo("SF10 (DR0)");
        assertThat(event.getRawWordHex()).isEqualTo(hex);
        assertThat(event.getBase64Payload()).isEqualTo(b64);
        assertThat(event.getStatus()).isEqualTo("ENQUEUED_IN_CHIRPSTACK");

        // Verify audit log entry was created
        List<AuditLog> auditLogs = persistenceService.getRecentAuditLogs();
        assertThat(auditLogs).isNotEmpty();
        AuditLog latestAudit = auditLogs.get(0);
        assertThat(latestAudit.getAction()).isEqualTo("SDN_DOWNLINK");
        assertThat(latestAudit.getDescription()).contains("70b3d57ed007985a");
        assertThat(latestAudit.getDetails()).contains(hex);
    }

    @Test
    @DisplayName("Should save custom operational audit logs")
    public void testLogAudit() {
        AuditLog log = persistenceService.logAudit("MAINTENANCE", "Database check completed", "status=OK", "10.0.0.1");

        assertThat(log).isNotNull();
        assertThat(log.getId()).isNotNull();
        assertThat(log.getAction()).isEqualTo("MAINTENANCE");
        assertThat(log.getDescription()).isEqualTo("Database check completed");
        assertThat(log.getIp()).isEqualTo("10.0.0.1");
    }
}
