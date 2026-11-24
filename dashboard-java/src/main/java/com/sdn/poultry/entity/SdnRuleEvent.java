package com.sdn.poultry.entity;

import jakarta.persistence.*;
import java.io.Serializable;
import java.time.Instant;

@Entity
@Table(name = "sdn_rule_events", indexes = {
    @Index(name = "idx_sdn_rule_events_dev_time", columnList = "dev_eui, sent_at DESC"),
    @Index(name = "idx_sdn_rule_events_sent_at", columnList = "sent_at")
})
public class SdnRuleEvent implements Serializable {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    @Column(name = "id")
    private Long id;

    @Column(name = "dev_eui", length = 32, nullable = false)
    private String devEui;

    @Column(name = "app_id", length = 64)
    private String appId;

    @Column(name = "cmd_id")
    private Integer cmdId;

    @Column(name = "cmd_name", length = 64)
    private String cmdName;

    @Column(name = "sf_target")
    private Integer sfTarget;

    @Column(name = "sf_name", length = 32)
    private String sfName;

    @Column(name = "tx_power")
    private Integer txPower;

    @Column(name = "sample_rate")
    private Integer sampleRate;

    @Column(name = "payload_mode")
    private Integer payloadMode;

    @Column(name = "sleep_enable")
    private Boolean sleepEnable;

    @Column(name = "param_value")
    private Integer paramValue;

    @Column(name = "raw_word_hex", length = 32)
    private String rawWordHex;

    @Column(name = "base64_payload", length = 64)
    private String base64Payload;

    @Column(name = "topic", length = 255)
    private String topic;

    @Column(name = "status", length = 64)
    private String status;

    @Column(name = "sent_at", nullable = false)
    private Instant sentAt;

    public SdnRuleEvent() {
    }

    @PrePersist
    public void prePersist() {
        if (this.sentAt == null) {
            this.sentAt = Instant.now();
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

    public String getAppId() {
        return appId;
    }

    public void setAppId(String appId) {
        this.appId = appId;
    }

    public Integer getCmdId() {
        return cmdId;
    }

    public void setCmdId(Integer cmdId) {
        this.cmdId = cmdId;
    }

    public String getCmdName() {
        return cmdName;
    }

    public void setCmdName(String cmdName) {
        this.cmdName = cmdName;
    }

    public Integer getSfTarget() {
        return sfTarget;
    }

    public void setSfTarget(Integer sfTarget) {
        this.sfTarget = sfTarget;
    }

    public String getSfName() {
        return sfName;
    }

    public void setSfName(String sfName) {
        this.sfName = sfName;
    }

    public Integer getTxPower() {
        return txPower;
    }

    public void setTxPower(Integer txPower) {
        this.txPower = txPower;
    }

    public Integer getSampleRate() {
        return sampleRate;
    }

    public void setSampleRate(Integer sampleRate) {
        this.sampleRate = sampleRate;
    }

    public Integer getPayloadMode() {
        return payloadMode;
    }

    public void setPayloadMode(Integer payloadMode) {
        this.payloadMode = payloadMode;
    }

    public Boolean getSleepEnable() {
        return sleepEnable;
    }

    public void setSleepEnable(Boolean sleepEnable) {
        this.sleepEnable = sleepEnable;
    }

    public Integer getParamValue() {
        return paramValue;
    }

    public void setParamValue(Integer paramValue) {
        this.paramValue = paramValue;
    }

    public String getRawWordHex() {
        return rawWordHex;
    }

    public void setRawWordHex(String rawWordHex) {
        this.rawWordHex = rawWordHex;
    }

    public String getBase64Payload() {
        return base64Payload;
    }

    public void setBase64Payload(String base64Payload) {
        this.base64Payload = base64Payload;
    }

    public String getTopic() {
        return topic;
    }

    public void setTopic(String topic) {
        this.topic = topic;
    }

    public String getStatus() {
        return status;
    }

    public void setStatus(String status) {
        this.status = status;
    }

    public Instant getSentAt() {
        return sentAt;
    }

    public void setSentAt(Instant sentAt) {
        this.sentAt = sentAt;
    }
}
