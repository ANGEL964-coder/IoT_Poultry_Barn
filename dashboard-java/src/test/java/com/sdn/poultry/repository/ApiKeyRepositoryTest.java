package com.sdn.poultry.repository;

import com.sdn.poultry.entity.ApiKey;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.orm.jpa.DataJpaTest;
import org.springframework.dao.DataIntegrityViolationException;
import org.springframework.test.context.TestPropertySource;

import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.List;
import java.util.Optional;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

@DataJpaTest
@TestPropertySource(locations = "classpath:application-test.properties")
public class ApiKeyRepositoryTest {

    @Autowired
    private ApiKeyRepository apiKeyRepository;

    @Test
    @DisplayName("Should persist and find active API key by hash")
    public void testSaveAndFindActiveApiKey() {
        String hash = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
        ApiKey key = new ApiKey(hash, "sdn_live_test", "Operador de prueba");
        key.setExpiresAt(Instant.now().plus(30, ChronoUnit.DAYS));

        ApiKey saved = apiKeyRepository.save(key);
        assertThat(saved.getId()).isNotNull();

        Optional<ApiKey> found = apiKeyRepository.findByKeyHash(hash);
        assertThat(found).isPresent();
        assertThat(found.get().getKeyPrefix()).isEqualTo("sdn_live_test");
        assertThat(found.get().isActive()).isTrue();
        assertThat(found.get().isValid()).isTrue();

        Optional<ApiKey> activeFound = apiKeyRepository.findByKeyHashAndActiveTrue(hash);
        assertThat(activeFound).isPresent();
    }

    @Test
    @DisplayName("Should filter inactive or revoked keys")
    public void testInactiveApiKey() {
        String hash = "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
        ApiKey key = new ApiKey(hash, "sdn_revoked", "Clave revocada");
        key.setActive(false);
        apiKeyRepository.save(key);

        List<ApiKey> activeKeys = apiKeyRepository.findByActiveTrue();
        assertThat(activeKeys).noneMatch(k -> k.getKeyHash().equals(hash));

        Optional<ApiKey> activeFound = apiKeyRepository.findByKeyHashAndActiveTrue(hash);
        assertThat(activeFound).isEmpty();

        Optional<ApiKey> anyFound = apiKeyRepository.findByKeyHash(hash);
        assertThat(anyFound).isPresent();
        assertThat(anyFound.get().isValid()).isFalse();
    }

    @Test
    @DisplayName("Should enforce unique constraint on keyHash")
    public void testUniqueKeyHashConstraint() {
        String hash = "duplicate_hash_000000000000000000000000000000000000000000000000000";
        ApiKey key1 = new ApiKey(hash, "sdn_dup_1", "Key 1");
        apiKeyRepository.saveAndFlush(key1);

        ApiKey key2 = new ApiKey(hash, "sdn_dup_2", "Key 2");
        assertThatThrownBy(() -> apiKeyRepository.saveAndFlush(key2))
                .isInstanceOf(DataIntegrityViolationException.class);
    }

    @Test
    @DisplayName("Should detect expired keys via isValid helper")
    public void testExpiredKeyValidation() {
        ApiKey expiredKey = new ApiKey("expired_hash", "sdn_exp", "Expired key");
        expiredKey.setExpiresAt(Instant.now().minus(1, ChronoUnit.HOURS));
        assertThat(expiredKey.isValid()).isFalse();

        ApiKey validKey = new ApiKey("valid_hash", "sdn_valid", "Valid key");
        validKey.setExpiresAt(Instant.now().plus(1, ChronoUnit.HOURS));
        assertThat(validKey.isValid()).isTrue();
    }
}
