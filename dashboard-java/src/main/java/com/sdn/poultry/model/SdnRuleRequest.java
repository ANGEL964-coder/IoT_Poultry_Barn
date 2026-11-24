package com.sdn.poultry.model;

import java.io.Serializable;

public class SdnRuleRequest implements Serializable {
    private String devEui;
    private String appId;
    private int cmdId = 1;         // default: RECONFIG_RADIO (1)
    private int sfTarget = 0;      // default: SF7 (0)
    private int txPower = 14;      // default: 14 dBm
    private int sampleRate = 0;    // default: 10s (0) o 60s (1)
    private int payloadMode = 0;   // default: FULL_TELEMETRY (0)
    private int sleepEnable = 0;   // default: 0 (No sleep)
    private int paramValue = 0;    // default: 0
    private boolean confirmed = false;
    private int fPort = 10;

    public SdnRuleRequest() {}

    // Getters and Setters
    public String getDevEui() { return devEui; }
    public void setDevEui(String devEui) { this.devEui = devEui; }

    public String getAppId() { return appId; }
    public void setAppId(String appId) { this.appId = appId; }

    public int getCmdId() { return cmdId; }
    public void setCmdId(int cmdId) { this.cmdId = cmdId; }

    public int getSfTarget() { return sfTarget; }
    public void setSfTarget(int sfTarget) { this.sfTarget = sfTarget; }

    public int getTxPower() { return txPower; }
    public void setTxPower(int txPower) { this.txPower = txPower; }

    public int getSampleRate() { return sampleRate; }
    public void setSampleRate(int sampleRate) { this.sampleRate = sampleRate; }

    public int getPayloadMode() { return payloadMode; }
    public void setPayloadMode(int payloadMode) { this.payloadMode = payloadMode; }

    public int getSleepEnable() { return sleepEnable; }
    public void setSleepEnable(int sleepEnable) { this.sleepEnable = sleepEnable; }

    public int getParamValue() { return paramValue; }
    public void setParamValue(int paramValue) { this.paramValue = paramValue; }

    public boolean isConfirmed() { return confirmed; }
    public void setConfirmed(boolean confirmed) { this.confirmed = confirmed; }

    public int getFPort() { return fPort; }
    public void setFPort(int fPort) { this.fPort = fPort; }
}
