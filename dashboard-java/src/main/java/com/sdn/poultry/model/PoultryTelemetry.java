package com.sdn.poultry.model;

import com.fasterxml.jackson.annotation.JsonInclude;
import java.io.Serializable;

@JsonInclude(JsonInclude.Include.NON_NULL)
public class PoultryTelemetry implements Serializable {
    private String devEui;
    private String deviceName;
    private String gatewayId;
    private double rssi;
    private double snr;
    private long frequency;
    private int spreadingFactor;
    private long fCnt;
    private int fPort;

    // TelemetryHeader (24B)
    private long seq;
    private long uptimeMs;
    private float batteryMv;
    private Float temperature;
    private Float humidity;
    private Float pressure;

    // PoultryScaleData (24B)
    private Float weightKg;
    private Float grossWeightKg;
    private Float voltageV;
    private Integer adcRaw;
    private Float calSlope;
    private Float calZero;
    private Integer timeOnAirMs;

    // PoultryEggCounterData (24B)
    private Long totalEggs;
    private Long imageSizeBytes;
    private Long totalDetections;
    private Integer camHour;
    private Integer camMinute;
    private Float eggsPerMinute;
    private Integer uartStatus;
    private Integer eggTimeOnAirMs;

    // Derived fields
    private Float hopperPercentage;
    private int payloadLength;
    private String payloadHex;
    private String timestamp;

    public boolean isVisionAvailable() { return totalEggs != null; }
    public boolean isScaleAvailable() { return weightKg != null; }
    public boolean isClimateAvailable() { return temperature != null || humidity != null || pressure != null; }

    public PoultryTelemetry() {}

    // Getters and Setters
    public String getDevEui() { return devEui; }
    public void setDevEui(String devEui) { this.devEui = devEui; }

    public String getDeviceName() { return deviceName; }
    public void setDeviceName(String deviceName) { this.deviceName = deviceName; }

    public String getGatewayId() { return gatewayId; }
    public void setGatewayId(String gatewayId) { this.gatewayId = gatewayId; }

    public double getRssi() { return rssi; }
    public void setRssi(double rssi) { this.rssi = rssi; }

    public double getSnr() { return snr; }
    public void setSnr(double snr) { this.snr = snr; }

    public long getFrequency() { return frequency; }
    public void setFrequency(long frequency) { this.frequency = frequency; }

    public int getSpreadingFactor() { return spreadingFactor; }
    public void setSpreadingFactor(int spreadingFactor) { this.spreadingFactor = spreadingFactor; }

    public long getFCnt() { return fCnt; }
    public void setFCnt(long fCnt) { this.fCnt = fCnt; }

    public int getFPort() { return fPort; }
    public void setFPort(int fPort) { this.fPort = fPort; }

    public long getSeq() { return seq; }
    public void setSeq(long seq) { this.seq = seq; }

    public long getUptimeMs() { return uptimeMs; }
    public void setUptimeMs(long uptimeMs) { this.uptimeMs = uptimeMs; }

    public float getBatteryMv() { return batteryMv; }
    public void setBatteryMv(float batteryMv) { this.batteryMv = batteryMv; }

    public Float getTemperature() { return temperature; }
    public void setTemperature(Float temperature) { this.temperature = temperature; }

    public Float getHumidity() { return humidity; }
    public void setHumidity(Float humidity) { this.humidity = humidity; }

    public Float getPressure() { return pressure; }
    public void setPressure(Float pressure) { this.pressure = pressure; }

    public Float getWeightKg() { return weightKg; }
    public void setWeightKg(Float weightKg) { this.weightKg = weightKg; }

    public Float getGrossWeightKg() { return grossWeightKg; }
    public void setGrossWeightKg(Float grossWeightKg) { this.grossWeightKg = grossWeightKg; }

    public Float getVoltageV() { return voltageV; }
    public void setVoltageV(Float voltageV) { this.voltageV = voltageV; }

    public Integer getAdcRaw() { return adcRaw; }
    public void setAdcRaw(Integer adcRaw) { this.adcRaw = adcRaw; }

    public Float getCalSlope() { return calSlope; }
    public void setCalSlope(Float calSlope) { this.calSlope = calSlope; }

    public Float getCalZero() { return calZero; }
    public void setCalZero(Float calZero) { this.calZero = calZero; }

    public Integer getTimeOnAirMs() { return timeOnAirMs; }
    public void setTimeOnAirMs(Integer timeOnAirMs) { this.timeOnAirMs = timeOnAirMs; }

    public Float getHopperPercentage() { return hopperPercentage; }
    public void setHopperPercentage(Float hopperPercentage) { this.hopperPercentage = hopperPercentage; }

    public int getPayloadLength() { return payloadLength; }
    public void setPayloadLength(int payloadLength) { this.payloadLength = payloadLength; }

    public String getPayloadHex() { return payloadHex; }
    public void setPayloadHex(String payloadHex) { this.payloadHex = payloadHex; }

    public String getTimestamp() { return timestamp; }
    public void setTimestamp(String timestamp) { this.timestamp = timestamp; }

    // Egg Counter Getters and Setters
    public Long getTotalEggs() { return totalEggs; }
    public void setTotalEggs(Long totalEggs) { this.totalEggs = totalEggs; }

    public Long getTotalDetections() { return totalDetections; }
    public void setTotalDetections(Long totalDetections) { this.totalDetections = totalDetections; }

    public Long getImageSizeBytes() { return imageSizeBytes; }
    public void setImageSizeBytes(Long imageSizeBytes) { this.imageSizeBytes = imageSizeBytes; }

    public Integer getCamHour() { return camHour; }
    public void setCamHour(Integer camHour) { this.camHour = camHour; }

    public Integer getCamMinute() { return camMinute; }
    public void setCamMinute(Integer camMinute) { this.camMinute = camMinute; }

    public Float getEggsPerMinute() { return eggsPerMinute; }
    public void setEggsPerMinute(Float eggsPerMinute) { this.eggsPerMinute = eggsPerMinute; }

    public Integer getUartStatus() { return uartStatus; }
    public void setUartStatus(Integer uartStatus) { this.uartStatus = uartStatus; }

    public Integer getEggTimeOnAirMs() { return eggTimeOnAirMs; }
    public void setEggTimeOnAirMs(Integer eggTimeOnAirMs) { this.eggTimeOnAirMs = eggTimeOnAirMs; }
}
