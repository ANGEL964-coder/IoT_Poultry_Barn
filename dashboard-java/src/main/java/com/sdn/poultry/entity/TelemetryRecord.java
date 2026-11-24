package com.sdn.poultry.entity;

import jakarta.persistence.*;
import java.io.Serializable;
import java.time.Instant;

@Entity
@Table(name = "telemetry_records", indexes = {
    @Index(name = "idx_telemetry_dev_time", columnList = "dev_eui, timestamp DESC"),
    @Index(name = "idx_telemetry_timestamp", columnList = "timestamp")
})
public class TelemetryRecord implements Serializable {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    @Column(name = "id")
    private Long id;

    @Column(name = "dev_eui", length = 32, nullable = false)
    private String devEui;

    // TelemetryHeader (24B)
    @Column(name = "seq")
    private Long seq;

    @Column(name = "uptime_ms")
    private Long uptimeMs;

    @Column(name = "battery_mv")
    private Float batteryMv;

    @Column(name = "temperature")
    private Float temperature;

    @Column(name = "humidity")
    private Float humidity;

    @Column(name = "pressure")
    private Float pressure;

    // PoultryScaleData (24B - nullable for 24B diagnostic frames)
    @Column(name = "weight_kg")
    private Float weightKg;

    @Column(name = "gross_weight_kg")
    private Float grossWeightKg;

    @Column(name = "voltage_v")
    private Float voltageV;

    @Column(name = "adc_raw")
    private Integer adcRaw;

    @Column(name = "cal_slope")
    private Float calSlope;

    @Column(name = "cal_zero")
    private Float calZero;

    @Column(name = "time_on_air_ms")
    private Integer timeOnAirMs;

    @Column(name = "hopper_percentage")
    private Float hopperPercentage;

    // PoultryEggCounterData (24B)
    @Column(name = "total_eggs")
    private Long totalEggs;

    @Column(name = "image_size_bytes")
    private Long imageSizeBytes;

    @Column(name = "cam_hour")
    private Integer camHour;

    @Column(name = "cam_minute")
    private Integer camMinute;

    @Column(name = "eggs_per_minute")
    private Float eggsPerMinute;

    @Column(name = "uart_status")
    private Integer uartStatus;

    @Column(name = "egg_time_on_air_ms")
    private Integer eggTimeOnAirMs;

    // RF & Metadata
    @Column(name = "rssi")
    private Integer rssi;

    @Column(name = "snr")
    private Float snr;

    @Column(name = "payload_mode")
    private Integer payloadMode;

    @Column(name = "raw_payload_base64", columnDefinition = "TEXT")
    private String rawPayloadBase64;

    @Column(name = "f_cnt")
    private Long fCnt;

    @Column(name = "f_port")
    private Integer fPort;

    @Column(name = "gateway_id", length = 32)
    private String gatewayId;

    @Column(name = "timestamp", nullable = false)
    private Instant timestamp;

    public TelemetryRecord() {
    }

    @PrePersist
    public void prePersist() {
        if (this.timestamp == null) {
            this.timestamp = Instant.now();
        }
        if (this.devEui != null) {
            this.devEui = this.devEui.toLowerCase().trim();
        }
    }

    public Long getId() {
        return id;
    }

    public void setId(Long id) {
        this.id = id;
    }

    public String getDevEui() {
        return devEui;
    }

    public void setDevEui(String devEui) {
        this.devEui = devEui != null ? devEui.toLowerCase().trim() : null;
    }

    public Long getSeq() {
        return seq;
    }

    public void setSeq(Long seq) {
        this.seq = seq;
    }

    public Long getUptimeMs() {
        return uptimeMs;
    }

    public void setUptimeMs(Long uptimeMs) {
        this.uptimeMs = uptimeMs;
    }

    public Float getBatteryMv() {
        return batteryMv;
    }

    public void setBatteryMv(Float batteryMv) {
        this.batteryMv = batteryMv;
    }

    public Float getTemperature() {
        return temperature;
    }

    public void setTemperature(Float temperature) {
        this.temperature = temperature;
    }

    public Float getHumidity() {
        return humidity;
    }

    public void setHumidity(Float humidity) {
        this.humidity = humidity;
    }

    public Float getPressure() {
        return pressure;
    }

    public void setPressure(Float pressure) {
        this.pressure = pressure;
    }

    public Float getWeightKg() {
        return weightKg;
    }

    public void setWeightKg(Float weightKg) {
        this.weightKg = weightKg;
    }

    public Float getGrossWeightKg() {
        return grossWeightKg;
    }

    public void setGrossWeightKg(Float grossWeightKg) {
        this.grossWeightKg = grossWeightKg;
    }

    public Float getVoltageV() {
        return voltageV;
    }

    public void setVoltageV(Float voltageV) {
        this.voltageV = voltageV;
    }

    public Integer getAdcRaw() {
        return adcRaw;
    }

    public void setAdcRaw(Integer adcRaw) {
        this.adcRaw = adcRaw;
    }

    public Float getCalSlope() {
        return calSlope;
    }

    public void setCalSlope(Float calSlope) {
        this.calSlope = calSlope;
    }

    public Float getCalZero() {
        return calZero;
    }

    public void setCalZero(Float calZero) {
        this.calZero = calZero;
    }

    public Integer getTimeOnAirMs() {
        return timeOnAirMs;
    }

    public void setTimeOnAirMs(Integer timeOnAirMs) {
        this.timeOnAirMs = timeOnAirMs;
    }

    public Float getHopperPercentage() {
        return hopperPercentage;
    }

    public void setHopperPercentage(Float hopperPercentage) {
        this.hopperPercentage = hopperPercentage;
    }

    // Egg Counter Getters and Setters
    public Long getTotalEggs() {
        return totalEggs;
    }

    public void setTotalEggs(Long totalEggs) {
        this.totalEggs = totalEggs;
    }

    public Long getImageSizeBytes() {
        return imageSizeBytes;
    }

    public void setImageSizeBytes(Long imageSizeBytes) {
        this.imageSizeBytes = imageSizeBytes;
    }

    public Integer getCamHour() {
        return camHour;
    }

    public void setCamHour(Integer camHour) {
        this.camHour = camHour;
    }

    public Integer getCamMinute() {
        return camMinute;
    }

    public void setCamMinute(Integer camMinute) {
        this.camMinute = camMinute;
    }

    public Float getEggsPerMinute() {
        return eggsPerMinute;
    }

    public void setEggsPerMinute(Float eggsPerMinute) {
        this.eggsPerMinute = eggsPerMinute;
    }

    public Integer getUartStatus() {
        return uartStatus;
    }

    public void setUartStatus(Integer uartStatus) {
        this.uartStatus = uartStatus;
    }

    public Integer getEggTimeOnAirMs() {
        return eggTimeOnAirMs;
    }

    public void setEggTimeOnAirMs(Integer eggTimeOnAirMs) {
        this.eggTimeOnAirMs = eggTimeOnAirMs;
    }

    public Integer getRssi() {
        return rssi;
    }

    public void setRssi(Integer rssi) {
        this.rssi = rssi;
    }

    public void setRssi(Double rssi) {
        this.rssi = rssi != null ? rssi.intValue() : null;
    }

    public Float getSnr() {
        return snr;
    }

    public void setSnr(Float snr) {
        this.snr = snr;
    }

    public void setSnr(Double snr) {
        this.snr = snr != null ? snr.floatValue() : null;
    }

    public Integer getPayloadMode() {
        return payloadMode;
    }

    public void setPayloadMode(Integer payloadMode) {
        this.payloadMode = payloadMode;
    }

    public String getRawPayloadBase64() {
        return rawPayloadBase64;
    }

    public void setRawPayloadBase64(String rawPayloadBase64) {
        this.rawPayloadBase64 = rawPayloadBase64;
    }

    public Long getFCnt() {
        return fCnt;
    }

    public void setFCnt(Long fCnt) {
        this.fCnt = fCnt;
    }

    public Integer getFPort() {
        return fPort;
    }

    public void setFPort(Integer fPort) {
        this.fPort = fPort;
    }

    public String getGatewayId() {
        return gatewayId;
    }

    public void setGatewayId(String gatewayId) {
        this.gatewayId = gatewayId;
    }

    public Instant getTimestamp() {
        return timestamp;
    }

    public void setTimestamp(Instant timestamp) {
        this.timestamp = timestamp;
    }
}
