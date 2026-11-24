package com.sdn.poultry.model;

import java.time.Instant;

public class ApiKeyCreateRequest {
    private String description;
    private Integer validDays;
    private Instant expiresAt;

    public ApiKeyCreateRequest() {
    }

    public ApiKeyCreateRequest(String description, Integer validDays, Instant expiresAt) {
        this.description = description;
        this.validDays = validDays;
        this.expiresAt = expiresAt;
    }

    public String getDescription() {
        return description;
    }

    public void setDescription(String description) {
        this.description = description;
    }

    public Integer getValidDays() {
        return validDays;
    }

    public void setValidDays(Integer validDays) {
        this.validDays = validDays;
    }

    public Instant getExpiresAt() {
        return expiresAt;
    }

    public void setExpiresAt(Instant expiresAt) {
        this.expiresAt = expiresAt;
    }
}
