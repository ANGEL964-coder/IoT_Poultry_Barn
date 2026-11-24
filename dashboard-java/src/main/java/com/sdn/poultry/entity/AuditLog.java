package com.sdn.poultry.entity;

import jakarta.persistence.*;
import java.io.Serializable;
import java.time.Instant;

@Entity
@Table(name = "audit_logs", indexes = {
    @Index(name = "idx_audit_logs_action_time", columnList = "action, timestamp DESC"),
    @Index(name = "idx_audit_logs_time", columnList = "timestamp DESC")
})
public class AuditLog implements Serializable {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    @Column(name = "id")
    private Long id;

    @Column(name = "action", length = 64, nullable = false)
    private String action;

    @Column(name = "description", length = 255)
    private String description;

    @Column(name = "details", columnDefinition = "TEXT")
    private String details;

    @Column(name = "ip", length = 45)
    private String ip = "127.0.0.1";

    @Column(name = "timestamp", nullable = false)
    private Instant timestamp;

    public AuditLog() {
    }

    public AuditLog(String action, String description, String details, String ip) {
        this.action = action;
        this.description = description;
        this.details = details;
        this.ip = ip != null ? ip : "127.0.0.1";
        this.timestamp = Instant.now();
    }

    @PrePersist
    public void prePersist() {
        if (this.timestamp == null) {
            this.timestamp = Instant.now();
        }
        if (this.ip == null) {
            this.ip = "127.0.0.1";
        }
    }

    public Long getId() {
        return id;
    }

    public void setId(Long id) {
        this.id = id;
    }

    public String getAction() {
        return action;
    }

    public void setAction(String action) {
        this.action = action;
    }

    public String getDescription() {
        return description;
    }

    public void setDescription(String description) {
        this.description = description;
    }

    public String getDetails() {
        return details;
    }

    public void setDetails(String details) {
        this.details = details;
    }

    public String getIp() {
        return ip;
    }

    public void setIp(String ip) {
        this.ip = ip;
    }

    public Instant getTimestamp() {
        return timestamp;
    }

    public void setTimestamp(Instant timestamp) {
        this.timestamp = timestamp;
    }
}
