package com.sdn.poultry.repository;

import com.sdn.poultry.entity.ApiKey;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.List;
import java.util.Optional;

@Repository
public interface ApiKeyRepository extends JpaRepository<ApiKey, Long> {
    Optional<ApiKey> findByKeyHash(String keyHash);
    Optional<ApiKey> findByKeyHashAndActiveTrue(String keyHash);
    Optional<ApiKey> findByKeyPrefix(String keyPrefix);
    List<ApiKey> findByActiveTrue();

    List<ApiKey> findAllByOrderByCreatedAtDesc();
    boolean existsByKeyHash(String keyHash);
}
