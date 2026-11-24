package com.sdn.poultry.service;

import com.sdn.poultry.entity.*;
import com.sdn.poultry.model.PoultryTelemetry;
import com.sdn.poultry.model.SdnRuleRequest;
import com.sdn.poultry.protocol.SdnHeaderPacker;
import com.sdn.poultry.repository.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.data.domain.PageRequest;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;
import java.util.List;
import java.util.Optional;

@Service
public class PersistenceService {

    private static final Logger log = LoggerFactory.getLogger(PersistenceService.class);

    private final DeviceRepository deviceRepository;
    private final TelemetryRecordRepository telemetryRecordRepository;
    private final SdnRuleEventRepository sdnRuleEventRepository;
    private final AuditLogRepository auditLogRepository;
    private final ApiKeyRepository apiKeyRepository;

    public PersistenceService(DeviceRepository deviceRepository,
                              TelemetryRecordRepository telemetryRecordRepository,
                              SdnRuleEventRepository sdnRuleEventRepository,
                              AuditLogRepository auditLogRepository,
                              ApiKeyRepository apiKeyRepository) {
        this.deviceRepository = deviceRepository;
        this.telemetryRecordRepository = telemetryRecordRepository;
        this.sdnRuleEventRepository = sdnRuleEventRepository;
        this.auditLogRepository = auditLogRepository;
        this.apiKeyRepository = apiKeyRepository;
    }

    /**
     * Saves incoming decoded telemetry into telemetry_records and upserts device entity in devices table.
     */
    @Transactional
    public TelemetryRecord saveTelemetryAndUpsertDevice(PoultryTelemetry telemetry, String rawPayloadBase64, String applicationId) {
        if (telemetry == null || telemetry.getDevEui() == null || telemetry.getDevEui().isBlank()) {
            log.warn("Cannot persist telemetry: missing telemetry or devEui");
            return null;
        }

        String devEui = telemetry.getDevEui().toLowerCase().trim();

        // 1. Upsert Device in devices table
        try {
            Device device = deviceRepository.findById(devEui).orElseGet(() -> {
                Device d = new Device();
                d.setDevEui(devEui);
                d.setCreatedAt(Instant.now());
                d.setStatus("ONLINE");
                return d;
            });

            if (telemetry.getDeviceName() != null && !telemetry.getDeviceName().isBlank()) {
                device.setName(telemetry.getDeviceName());
            } else if (device.getName() == null) {
                device.setName(devEui);
            }

            if (applicationId != null && !applicationId.isBlank()) {
                device.setApplicationId(applicationId);
            }

            device.setStatus("ONLINE");
            device.setLastSeen(Instant.now());

            if (telemetry.getCalSlope() != null && telemetry.getCalSlope() > 0.0f) {
                device.setCalSlope(telemetry.getCalSlope());
            }
            if (telemetry.getCalZero() != null) {
                device.setCalZero(telemetry.getCalZero());
            }

            deviceRepository.save(device);
        } catch (Exception ex) {
            log.error("Error upserting device {}: {}", devEui, ex.getMessage(), ex);
        }

        // 2. Insert TelemetryRecord in telemetry_records table
        TelemetryRecord record = new TelemetryRecord();
        record.setDevEui(devEui);
        record.setSeq(telemetry.getSeq());
        record.setUptimeMs(telemetry.getUptimeMs());
        record.setBatteryMv(telemetry.getBatteryMv());
        record.setTemperature(telemetry.getTemperature());
        record.setHumidity(telemetry.getHumidity());
        record.setPressure(telemetry.getPressure());

        int payloadLen = telemetry.getPayloadLength();
        if (telemetry.isVisionAvailable()) {
            // Egg counter device
            record.setTotalEggs(telemetry.getTotalEggs());
            record.setImageSizeBytes(telemetry.getImageSizeBytes());
            record.setCamHour(telemetry.getCamHour());
            record.setCamMinute(telemetry.getCamMinute());
            record.setEggsPerMinute(telemetry.getEggsPerMinute());
            record.setUartStatus(telemetry.getUartStatus());
            record.setEggTimeOnAirMs(telemetry.getEggTimeOnAirMs());
        } else if (telemetry.isScaleAvailable()) {
            // Scale device
            record.setWeightKg(telemetry.getWeightKg());
            record.setGrossWeightKg(telemetry.getGrossWeightKg());
            record.setVoltageV(telemetry.getVoltageV());
            record.setAdcRaw(telemetry.getAdcRaw());
            record.setCalSlope(telemetry.getCalSlope());
            record.setCalZero(telemetry.getCalZero());
            record.setTimeOnAirMs(telemetry.getTimeOnAirMs());
            record.setHopperPercentage(telemetry.getHopperPercentage());
        }

        if (payloadLen >= 204) {
            record.setPayloadMode(0); // FULL_TELEMETRY
        } else if (payloadLen >= 48) {
            record.setPayloadMode(1); // CRITICAL
        } else {
            record.setPayloadMode(2); // DIAGNOSTIC
        }

        record.setRssi(telemetry.getRssi());
        record.setSnr(telemetry.getSnr());
        record.setRawPayloadBase64(rawPayloadBase64);
        record.setFCnt(telemetry.getFCnt());
        record.setFPort(telemetry.getFPort());
        record.setGatewayId(telemetry.getGatewayId());
        record.setTimestamp(Instant.now());

        return telemetryRecordRepository.save(record);
    }

    public TelemetryRecord saveTelemetryAndUpsertDevice(PoultryTelemetry telemetry) {
        return saveTelemetryAndUpsertDevice(telemetry, null, null);
    }

    /**
     * Saves an SDN rule downlink event and logs an audit record.
     */
    @Transactional
    public SdnRuleEvent saveDownlinkEvent(SdnRuleRequest req, String rawWordHex, String base64Payload, String topic, String status) {
        if (req == null || req.getDevEui() == null || req.getDevEui().isBlank()) {
            log.warn("Cannot persist downlink event: missing request or devEui");
            return null;
        }

        String devEui = req.getDevEui().toLowerCase().trim();

        SdnRuleEvent event = new SdnRuleEvent();
        event.setDevEui(devEui);
        event.setAppId(req.getAppId());
        event.setCmdId(req.getCmdId());
        event.setCmdName((req.getCmdId() < SdnHeaderPacker.CMD_NAMES.length) ? SdnHeaderPacker.CMD_NAMES[req.getCmdId()] : "CUSTOM");
        event.setSfTarget(req.getSfTarget());
        event.setSfName((req.getSfTarget() < SdnHeaderPacker.SF_NAMES.length) ? SdnHeaderPacker.SF_NAMES[req.getSfTarget()] : "CUSTOM");
        event.setTxPower(req.getTxPower());
        event.setSampleRate(req.getSampleRate());
        event.setPayloadMode(req.getPayloadMode());
        event.setSleepEnable(req.getSleepEnable() != 0);
        event.setParamValue(req.getParamValue());
        event.setRawWordHex(rawWordHex);
        event.setBase64Payload(base64Payload);
        event.setTopic(topic);
        event.setStatus(status != null ? status : "ENQUEUED_IN_CHIRPSTACK");
        event.setSentAt(Instant.now());

        SdnRuleEvent savedEvent = sdnRuleEventRepository.save(event);

        // Record audit entry
        String auditDesc = String.format("Injected SDN cmd %d (%s) to %s",
                req.getCmdId(), event.getCmdName(), devEui);
        String auditDetails = String.format("SF: %s, TX: %d dBm, Rate: %d, Sleep: %s, Param: %d, Hex: %s, B64: %s, Topic: %s",
                event.getSfName(), req.getTxPower(), req.getSampleRate(), event.getSleepEnable(), req.getParamValue(),
                rawWordHex, base64Payload, topic);
        logAudit("SDN_DOWNLINK", auditDesc, auditDetails, "127.0.0.1");

        return savedEvent;
    }

    /**
     * Logs an operational audit entry in audit_logs table.
     */
    @Transactional
    public AuditLog logAudit(String action, String description, String details, String ip) {
        AuditLog audit = new AuditLog(action, description, details, ip != null ? ip : "127.0.0.1");
        return auditLogRepository.save(audit);
    }

    public AuditLog logAudit(String action, String description) {
        return logAudit(action, description, null, "127.0.0.1");
    }

    // Read queries
    public List<Device> getAllDevices() {
        return deviceRepository.findAll();
    }

    public Optional<Device> getDevice(String devEui) {
        return deviceRepository.findById(devEui.toLowerCase().trim());
    }

    public List<TelemetryRecord> getRecentTelemetry(String devEui, int limit) {
        return telemetryRecordRepository.findByDevEuiOrderByTimestampDesc(
                devEui.toLowerCase().trim(),
                PageRequest.of(0, Math.max(1, limit))
        );
    }

    public Optional<TelemetryRecord> getLatestTelemetry(String devEui) {
        return telemetryRecordRepository.findTopByDevEuiOrderByTimestampDesc(devEui.toLowerCase().trim());
    }

    public List<SdnRuleEvent> getRecentDownlinks(String devEui) {
        if (devEui != null && !devEui.isBlank()) {
            return sdnRuleEventRepository.findByDevEuiOrderBySentAtDesc(devEui.toLowerCase().trim());
        }
        return sdnRuleEventRepository.findTop50ByOrderBySentAtDesc();
    }

    public List<AuditLog> getRecentAuditLogs() {
        return auditLogRepository.findTop50ByOrderByTimestampDesc();
    }

    public DeviceRepository getDeviceRepository() {
        return deviceRepository;
    }

    public TelemetryRecordRepository getTelemetryRecordRepository() {
        return telemetryRecordRepository;
    }

    public SdnRuleEventRepository getSdnRuleEventRepository() {
        return sdnRuleEventRepository;
    }

    public AuditLogRepository getAuditLogRepository() {
        return auditLogRepository;
    }

    public ApiKeyRepository getApiKeyRepository() {
        return apiKeyRepository;
    }
}
