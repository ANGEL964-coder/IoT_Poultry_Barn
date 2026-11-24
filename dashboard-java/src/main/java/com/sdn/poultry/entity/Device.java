package com.sdn.poultry.entity;

import jakarta.persistence.*;
import java.io.Serializable;
import java.time.Instant;

@Entity
@Table(name = "devices")
public class Device implements Serializable {

    @Id
    @Column(name = "dev_eui", length = 32, nullable = false)
    private String devEui;

    @Column(name = "name", length = 100)
    private String name;

    @Column(name = "application_id", length = 64)
    private String applicationId;

    @Column(name = "status", length = 32)
    private String status = "ONLINE";

    @Column(name = "hopper_capacity_kg")
    private Float hopperCapacityKg = 20.0f;

    @Column(name = "cal_slope")
    private Float calSlope = 0.0500f;

    @Column(name = "cal_zero")
    private Float calZero = 0.5000f;

    @Column(name = "last_seen")
    private Instant lastSeen;

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt;

    public Device() {
    }

    public Device(String devEui, String name, String applicationId, String status) {
        this.devEui = devEui != null ? devEui.toLowerCase().trim() : null;
        this.name = name;
        this.applicationId = applicationId;
        this.status = status;
        this.hopperCapacityKg = 20.0f;
        this.calSlope = 0.0500f;
        this.calZero = 0.5000f;
        this.lastSeen = Instant.now();
        this.createdAt = Instant.now();
    }

    @PrePersist
    public void prePersist() {
        if (this.createdAt == null) {
            this.createdAt = Instant.now();
        }
        if (this.lastSeen == null) {
            this.lastSeen = Instant.now();
        }
        if (this.devEui != null) {
            this.devEui = this.devEui.toLowerCase().trim();
        }
    }

    public String getDevEui() {
        return devEui;
    }

    public void setDevEui(String devEui) {
        this.devEui = devEui != null ? devEui.toLowerCase().trim() : null;
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public String getApplicationId() {
        return applicationId;
    }

    public void setApplicationId(String applicationId) {
        this.applicationId = applicationId;
    }

    public String getStatus() {
        return status;
    }

    public void setStatus(String status) {
        this.status = status;
    }

    public Float getHopperCapacityKg() {
        return hopperCapacityKg;
    }

    public void setHopperCapacityKg(Float hopperCapacityKg) {
        this.hopperCapacityKg = hopperCapacityKg;
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

    public Instant getLastSeen() {
        return lastSeen;
    }

    public void setLastSeen(Instant lastSeen) {
        this.lastSeen = lastSeen;
    }

    public Instant getCreatedAt() {
        return createdAt;
    }

    public void setCreatedAt(Instant createdAt) {
        this.createdAt = createdAt;
    }
}
