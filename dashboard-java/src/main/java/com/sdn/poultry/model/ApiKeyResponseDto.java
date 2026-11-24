package com.sdn.poultry.model;

import java.time.Instant;

public class ApiKeyResponseDto {
    private Long id;
    private String keyPrefix;
    private String description;
    private Boolean active;
    private Instant createdAt;
    private Instant expiresAt;
    private Instant lastUsedAt;
    private Boolean valid;

    public ApiKeyResponseDto() {
    }

    public ApiKeyResponseDto(Long id, String keyPrefix, String description, Boolean active,
                             Instant createdAt, Instant expiresAt, Instant lastUsedAt, Boolean valid) {
        this.id = id;
        this.keyPrefix = keyPrefix;
        this.description = description;
        this.active = active;
        this.createdAt = createdAt;
        this.expiresAt = expiresAt;
        this.lastUsedAt = lastUsedAt;
        this.valid = valid;
    }

    public Long getId() {
        return id;
    }

    public void setId(Long id) {
        this.id = id;
    }

    public String getKeyPrefix() {
        return keyPrefix;
    }

    public void setKeyPrefix(String keyPrefix) {
        this.keyPrefix = keyPrefix;
    }

    public String getDescription() {
        return description;
    }

    public void setDescription(String description) {
        this.description = description;
    }

    public Boolean getActive() {
        return active;
    }

    public void setActive(Boolean active) {
        this.active = active;
    }

    public Instant getCreatedAt() {
        return createdAt;
    }

    public void setCreatedAt(Instant createdAt) {
        this.createdAt = createdAt;
    }

    public Instant getExpiresAt() {
        return expiresAt;
    }

    public void setExpiresAt(Instant expiresAt) {
        this.expiresAt = expiresAt;
    }

    public Instant getLastUsedAt() {
        return lastUsedAt;
    }

    public void setLastUsedAt(Instant lastUsedAt) {
        this.lastUsedAt = lastUsedAt;
    }

    public Boolean getValid() {
        return valid;
    }

    public void setValid(Boolean valid) {
        this.valid = valid;
    }
}
