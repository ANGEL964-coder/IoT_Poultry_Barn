package com.sdn.poultry.model;

import java.time.Instant;

public class ApiKeyGenerateResponse {
    private Long id;
    private String rawKey;
    private String keyPrefix;
    private String description;
    private Boolean active;
    private Instant createdAt;
    private Instant expiresAt;
    private String message;

    public ApiKeyGenerateResponse() {
    }

    public ApiKeyGenerateResponse(Long id, String rawKey, String keyPrefix, String description,
                                  Boolean active, Instant createdAt, Instant expiresAt, String message) {
        this.id = id;
        this.rawKey = rawKey;
        this.keyPrefix = keyPrefix;
        this.description = description;
        this.active = active;
        this.createdAt = createdAt;
        this.expiresAt = expiresAt;
        this.message = message;
    }

    public Long getId() {
        return id;
    }

    public void setId(Long id) {
        this.id = id;
    }

    public String getRawKey() {
        return rawKey;
    }

    public String getApiKey() {
        return rawKey;
    }

    public void setRawKey(String rawKey) {
        this.rawKey = rawKey;
    }

    public void setApiKey(String apiKey) {
        this.rawKey = apiKey;
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

    public String getMessage() {
        return message;
    }

    public void setMessage(String message) {
        this.message = message;
    }
}
