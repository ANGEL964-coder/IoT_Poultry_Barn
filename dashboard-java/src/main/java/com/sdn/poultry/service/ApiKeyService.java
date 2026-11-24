package com.sdn.poultry.service;

import com.sdn.poultry.entity.ApiKey;
import com.sdn.poultry.model.ApiKeyCreateRequest;
import com.sdn.poultry.model.ApiKeyGenerateResponse;
import com.sdn.poultry.model.ApiKeyResponseDto;
import com.sdn.poultry.repository.ApiKeyRepository;
import com.sdn.poultry.security.ApiKeyAuthenticationFilter;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.context.event.ApplicationReadyEvent;
import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.security.SecureRandom;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.HexFormat;
import java.util.List;
import java.util.Optional;

@Service
public class ApiKeyService {

    private static final Logger log = LoggerFactory.getLogger(ApiKeyService.class);
    private static final String KEY_PREFIX = "sdn_live_";

    private final ApiKeyRepository apiKeyRepository;
    private final PersistenceService persistenceService;
    private final SecureRandom secureRandom = new SecureRandom();

    @Value("${app.security.bootstrap-key:sdn_master_admin_key_2026}")
    private String bootstrapKey;

    public ApiKeyService(ApiKeyRepository apiKeyRepository, PersistenceService persistenceService) {
        this.apiKeyRepository = apiKeyRepository;
        this.persistenceService = persistenceService;
    }

    @EventListener(ApplicationReadyEvent.class)
    @Transactional
    public void bootstrapInitialKeyIfEmpty() {
        bootstrapSingleKey("master-test-key-2026", "Master Test Key 2026");
        bootstrapSingleKey("sdn_master_admin_key_2026", "Default Bootstrap Administrative Key");
    }

    private void bootstrapSingleKey(String rawKey, String desc) {
        String hash = ApiKeyAuthenticationFilter.sha256Hex(rawKey);
        if (apiKeyRepository.findByKeyHashAndActiveTrue(hash).isEmpty()) {
            String prefix = rawKey.substring(0, Math.min(16, rawKey.length()));
            ApiKey adminKey = new ApiKey(hash, prefix, desc);
            adminKey.setActive(true);
            adminKey.setCreatedAt(Instant.now());
            adminKey.setExpiresAt(Instant.now().plus(365, ChronoUnit.DAYS));
            ApiKey saved = apiKeyRepository.save(adminKey);
            if (persistenceService != null) {
                persistenceService.logAudit(
                        "SECURITY_BOOTSTRAP",
                        "Bootstrapped administrative API key",
                        "ID: " + saved.getId() + ", Prefix: " + prefix,
                        "127.0.0.1"
                );
            }
            log.info("[SECURITY] Administrative API key bootstrapped (prefix: {})", prefix);
        }
    }

    @Transactional
    public ApiKeyGenerateResponse generateApiKey(ApiKeyCreateRequest request, String clientIp) {
        String description = (request != null && request.getDescription() != null && !request.getDescription().isBlank())
                ? request.getDescription()
                : "Standard Device API Key";

        byte[] entropy = generateRandomBytes(32);
        String hex = HexFormat.of().formatHex(entropy);
        String rawKey = KEY_PREFIX + hex;
        String prefix = rawKey.substring(0, 16);
        String keyHash = ApiKeyAuthenticationFilter.sha256Hex(rawKey);

        Instant expiresAt = null;
        if (request != null) {
            if (request.getValidDays() != null && request.getValidDays() > 0) {
                expiresAt = Instant.now().plus(request.getValidDays(), ChronoUnit.DAYS);
            } else if (request.getExpiresAt() != null) {
                expiresAt = request.getExpiresAt();
            }
        }

        ApiKey entity = new ApiKey(keyHash, prefix, description);
        entity.setActive(true);
        entity.setCreatedAt(Instant.now());
        entity.setExpiresAt(expiresAt);

        ApiKey saved = apiKeyRepository.save(entity);

        String auditIp = (clientIp != null && !clientIp.isBlank()) ? clientIp : "127.0.0.1";
        persistenceService.logAudit(
                "API_KEY_CREATED",
                "Generated new API Key ID " + saved.getId() + " (" + prefix + ")",
                "Description: " + description + ", ExpiresAt: " + expiresAt,
                auditIp
        );

        return new ApiKeyGenerateResponse(
                saved.getId(),
                rawKey,
                saved.getKeyPrefix(),
                saved.getDescription(),
                saved.getActive(),
                saved.getCreatedAt(),
                saved.getExpiresAt(),
                "Store this API key securely. It will not be shown again."
        );
    }

    public ApiKeyGenerateResponse generateApiKey(ApiKeyCreateRequest request) {
        return generateApiKey(request, "127.0.0.1");
    }

    @Transactional(readOnly = true)
    public List<ApiKeyResponseDto> listAllKeys() {
        return apiKeyRepository.findAllByOrderByCreatedAtDesc().stream()
                .map(this::toResponseDto)
                .toList();
    }

    @Transactional
    public boolean revokeKey(Long id, String clientIp) {
        Optional<ApiKey> keyOpt = apiKeyRepository.findById(id);
        if (keyOpt.isPresent()) {
            ApiKey key = keyOpt.get();
            key.setActive(false);
            apiKeyRepository.save(key);

            String auditIp = (clientIp != null && !clientIp.isBlank()) ? clientIp : "127.0.0.1";
            persistenceService.logAudit(
                    "API_KEY_REVOKED",
                    "Revoked API Key ID " + id + " (" + key.getKeyPrefix() + ")",
                    "Description: " + key.getDescription(),
                    auditIp
            );
            return true;
        }
        return false;
    }

    public boolean revokeKey(Long id) {
        return revokeKey(id, "127.0.0.1");
    }

    @Transactional
    public boolean deleteKey(Long id, String clientIp) {
        Optional<ApiKey> keyOpt = apiKeyRepository.findById(id);
        if (keyOpt.isPresent()) {
            ApiKey key = keyOpt.get();
            apiKeyRepository.delete(key);

            String auditIp = (clientIp != null && !clientIp.isBlank()) ? clientIp : "127.0.0.1";
            persistenceService.logAudit(
                    "API_KEY_DELETED",
                    "Deleted API Key ID " + id + " (" + key.getKeyPrefix() + ")",
                    "Description: " + key.getDescription(),
                    auditIp
            );
            return true;
        }
        return false;
    }

    public boolean deleteKey(Long id) {
        return deleteKey(id, "127.0.0.1");
    }

    @Transactional(readOnly = true)
    public Optional<ApiKey> findById(Long id) {
        return apiKeyRepository.findById(id);
    }

    @Transactional(readOnly = true)
    public Optional<ApiKey> findByHash(String hash) {
        return apiKeyRepository.findByKeyHash(hash);
    }

    private byte[] generateRandomBytes(int length) {
        byte[] bytes = new byte[length];
        secureRandom.nextBytes(bytes);
        return bytes;
    }

    private ApiKeyResponseDto toResponseDto(ApiKey entity) {
        return new ApiKeyResponseDto(
                entity.getId(),
                entity.getKeyPrefix(),
                entity.getDescription(),
                entity.getActive(),
                entity.getCreatedAt(),
                entity.getExpiresAt(),
                entity.getLastUsedAt(),
                entity.isValid()
        );
    }
}
