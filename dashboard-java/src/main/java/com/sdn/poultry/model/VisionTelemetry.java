package com.sdn.poultry.model;

import com.fasterxml.jackson.annotation.JsonInclude;
import java.io.Serializable;
import java.time.Instant;

@JsonInclude(JsonInclude.Include.NON_NULL)
public class VisionTelemetry implements Serializable {

    private String devEui;
    private int eggCount;
    private int targetCount;
    private double efficiency;
    private long inferenceTimeMs;
    private String cameraStatus;
    private double confidence;
    private Instant timestamp;

    public VisionTelemetry() {
        this.timestamp = Instant.now();
    }

    public VisionTelemetry(String devEui, int eggCount, int targetCount, double efficiency,
                           long inferenceTimeMs, String cameraStatus, double confidence) {
        this.devEui = devEui;
        this.eggCount = eggCount;
        this.targetCount = targetCount;
        this.efficiency = efficiency;
        this.inferenceTimeMs = inferenceTimeMs;
        this.cameraStatus = cameraStatus;
        this.confidence = confidence;
        this.timestamp = Instant.now();
    }

    public String getDevEui() {
        return devEui;
    }

    public void setDevEui(String devEui) {
        this.devEui = devEui;
    }

    public int getEggCount() {
        return eggCount;
    }

    public void setEggCount(int eggCount) {
        this.eggCount = eggCount;
    }

    public int getTargetCount() {
        return targetCount;
    }

    public void setTargetCount(int targetCount) {
        this.targetCount = targetCount;
    }

    public double getEfficiency() {
        return efficiency;
    }

    public void setEfficiency(double efficiency) {
        this.efficiency = efficiency;
    }

    public long getInferenceTimeMs() {
        return inferenceTimeMs;
    }

    public void setInferenceTimeMs(long inferenceTimeMs) {
        this.inferenceTimeMs = inferenceTimeMs;
    }

    public String getCameraStatus() {
        return cameraStatus;
    }

    public void setCameraStatus(String cameraStatus) {
        this.cameraStatus = cameraStatus;
    }

    public double getConfidence() {
        return confidence;
    }

    public void setConfidence(double confidence) {
        this.confidence = confidence;
    }

    public Instant getTimestamp() {
        return timestamp;
    }

    public void setTimestamp(Instant timestamp) {
        this.timestamp = timestamp;
    }
}
