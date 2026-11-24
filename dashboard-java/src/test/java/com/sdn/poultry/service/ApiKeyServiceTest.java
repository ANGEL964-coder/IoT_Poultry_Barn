package com.sdn.poultry.service;

import com.sdn.poultry.entity.ApiKey;
import com.sdn.poultry.model.ApiKeyCreateRequest;
import com.sdn.poultry.model.ApiKeyGenerateResponse;
import com.sdn.poultry.model.ApiKeyResponseDto;
import com.sdn.poultry.repository.ApiKeyRepository;
import com.sdn.poultry.security.ApiKeyAuthenticationFilter;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.mockito.ArgumentCaptor;

import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.List;
import java.util.Optional;

import static org.assertj.core.api.Assertions.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.*;

class ApiKeyServiceTest {

    private ApiKeyRepository apiKeyRepository;
    private PersistenceService persistenceService;
    private ApiKeyService apiKeyService;

    @BeforeEach
    void setUp() {
        apiKeyRepository = mock(ApiKeyRepository.class);
        persistenceService = mock(PersistenceService.class);
        apiKeyService = new ApiKeyService(apiKeyRepository, persistenceService);
    }

    @Test
    @DisplayName("Should generate cryptographically secure key with sdn_live_ prefix and SHA-256 hash")
    void testGenerateApiKey() {
        when(apiKeyRepository.save(any(ApiKey.class))).thenAnswer(invocation -> {
            ApiKey k = invocation.getArgument(0);
            k.setId(42L);
            return k;
        });

        ApiKeyCreateRequest req = new ApiKeyCreateRequest("Sensor Galpón 1", 30, null);
        ApiKeyGenerateResponse resp = apiKeyService.generateApiKey(req, "192.168.1.10");

        assertThat(resp.getId()).isEqualTo(42L);
        assertThat(resp.getRawKey()).startsWith("sdn_live_");
        assertThat(resp.getRawKey().length()).isEqualTo(72); // 8 + 64
        assertThat(resp.getKeyPrefix()).isEqualTo(resp.getRawKey().substring(0, 16));
        assertThat(resp.getDescription()).isEqualTo("Sensor Galpón 1");
        assertThat(resp.getActive()).isTrue();
        assertThat(resp.getExpiresAt()).isAfter(Instant.now());

        ArgumentCaptor<ApiKey> captor = ArgumentCaptor.forClass(ApiKey.class);
        verify(apiKeyRepository).save(captor.capture());
        ApiKey saved = captor.getValue();

        String expectedHash = ApiKeyAuthenticationFilter.sha256Hex(resp.getRawKey());
        assertThat(saved.getKeyHash()).isEqualTo(expectedHash);
        assertThat(saved.getKeyHash()).isNotEqualTo(resp.getRawKey()); // Plain key never stored directly

        verify(persistenceService).logAudit(eq("API_KEY_CREATED"), anyString(), anyString(), eq("192.168.1.10"));
    }

    @Test
    @DisplayName("Should list all keys as response DTOs without revealing secret keyHash")
    void testListAllKeys() {
        ApiKey k1 = new ApiKey("hash1", "sdn_live_11111111", "Key 1");
        k1.setId(1L);
        ApiKey k2 = new ApiKey("hash2", "sdn_live_22222222", "Key 2");
        k2.setId(2L);

        when(apiKeyRepository.findAllByOrderByCreatedAtDesc()).thenReturn(List.of(k1, k2));

        List<ApiKeyResponseDto> list = apiKeyService.listAllKeys();
        assertThat(list).hasSize(2);
        assertThat(list.get(0).getId()).isEqualTo(1L);
        assertThat(list.get(0).getKeyPrefix()).isEqualTo("sdn_live_11111111");
        assertThat(list.get(1).getId()).isEqualTo(2L);
    }

    @Test
    @DisplayName("Should revoke existing key and log API_KEY_REVOKED audit event")
    void testRevokeKey() {
        ApiKey key = new ApiKey("test_hash", "sdn_live_testpref", "Key to Revoke");
        key.setId(5L);
        key.setActive(true);

        when(apiKeyRepository.findById(5L)).thenReturn(Optional.of(key));

        boolean revoked = apiKeyService.revokeKey(5L, "10.0.0.5");
        assertThat(revoked).isTrue();
        assertThat(key.getActive()).isFalse();

        verify(apiKeyRepository).save(key);
        verify(persistenceService).logAudit(eq("API_KEY_REVOKED"), anyString(), anyString(), eq("10.0.0.5"));
    }

    @Test
    @DisplayName("Should delete key permanently and log API_KEY_DELETED audit event")
    void testDeleteKey() {
        ApiKey key = new ApiKey("test_hash", "sdn_live_testpref", "Key to Delete");
        key.setId(7L);

        when(apiKeyRepository.findById(7L)).thenReturn(Optional.of(key));

        boolean deleted = apiKeyService.deleteKey(7L, "10.0.0.7");
        assertThat(deleted).isTrue();

        verify(apiKeyRepository).delete(key);
        verify(persistenceService).logAudit(eq("API_KEY_DELETED"), anyString(), anyString(), eq("10.0.0.7"));
    }

    @Test
    @DisplayName("Should bootstrap admin key on application startup when repository is empty")
    void testBootstrapInitialKey() {
        when(apiKeyRepository.findByKeyHashAndActiveTrue(anyString())).thenReturn(Optional.empty());
        when(apiKeyRepository.save(any(ApiKey.class))).thenAnswer(inv -> {
            ApiKey k = inv.getArgument(0);
            k.setId(1L);
            return k;
        });

        apiKeyService.bootstrapInitialKeyIfEmpty();

        verify(apiKeyRepository, atLeastOnce()).save(any(ApiKey.class));
        verify(persistenceService, atLeastOnce()).logAudit(eq("SECURITY_BOOTSTRAP"), anyString(), anyString(), eq("127.0.0.1"));
    }

    @Test
    @DisplayName("Should not bootstrap admin key if repository already contains keys")
    void testSkipBootstrapIfNotEmpty() {
        when(apiKeyRepository.findByKeyHashAndActiveTrue(anyString())).thenReturn(Optional.of(new ApiKey("hash", "pfx", "desc")));

        apiKeyService.bootstrapInitialKeyIfEmpty();

        verify(apiKeyRepository, never()).save(any(ApiKey.class));
        verify(persistenceService, never()).logAudit(eq("SECURITY_BOOTSTRAP"), anyString(), anyString(), anyString());
    }
}
