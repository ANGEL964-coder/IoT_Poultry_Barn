package com.sdn.poultry.controller;

import com.sdn.poultry.entity.ApiKey;
import com.sdn.poultry.model.ApiKeyCreateRequest;
import com.sdn.poultry.model.ApiKeyGenerateResponse;
import com.sdn.poultry.model.ApiKeyResponseDto;
import com.sdn.poultry.repository.ApiKeyRepository;
import com.sdn.poultry.security.ApiKeyAuthenticationFilter;
import com.sdn.poultry.security.ClientIpResolver;
import com.sdn.poultry.service.ApiKeyService;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.List;
import java.util.Map;

@RestController
@RequestMapping("/api/keys")
@CrossOrigin(origins = "*")
public class ApiKeyController {

    private final ApiKeyService apiKeyService;
    private final ApiKeyRepository apiKeyRepository;

    public ApiKeyController(ApiKeyService apiKeyService, ApiKeyRepository apiKeyRepository) {
        this.apiKeyService = apiKeyService;
        this.apiKeyRepository = apiKeyRepository;
    }

    @PostMapping
    public ResponseEntity<?> createKey(
            @RequestBody(required = false) Map<String, Object> body,
            HttpServletRequest servletRequest
    ) {
        String clientIp = ClientIpResolver.resolveClientIp(servletRequest);
        String name = body != null && body.containsKey("name") ? String.valueOf(body.get("name")) : "Device API Key";
        String explicitKey = body != null && body.containsKey("key") ? String.valueOf(body.get("key")) : null;

        if (explicitKey != null && !explicitKey.isBlank()) {
            String hash = ApiKeyAuthenticationFilter.sha256Hex(explicitKey);
            String prefix = explicitKey.substring(0, Math.min(16, explicitKey.length()));
            ApiKey apiKey = new ApiKey(hash, prefix, name);
            apiKey.setActive(true);
            apiKey.setCreatedAt(Instant.now());
            apiKey.setExpiresAt(Instant.now().plus(365, ChronoUnit.DAYS));
            ApiKey saved = apiKeyRepository.save(apiKey);
            return ResponseEntity.status(HttpStatus.CREATED)
                    .body(Map.of("key", explicitKey, "status", "created", "id", saved.getId()));
        }

        ApiKeyCreateRequest request = new ApiKeyCreateRequest();
        request.setDescription(name);
        ApiKeyGenerateResponse response = apiKeyService.generateApiKey(request, clientIp);
        return ResponseEntity.status(HttpStatus.CREATED).body(response);
    }

    @PostMapping("/generate")
    public ResponseEntity<ApiKeyGenerateResponse> generateKey(
            @RequestBody(required = false) ApiKeyCreateRequest request,
            HttpServletRequest servletRequest
    ) {
        String clientIp = ClientIpResolver.resolveClientIp(servletRequest);
        ApiKeyGenerateResponse response = apiKeyService.generateApiKey(request, clientIp);
        return ResponseEntity.status(HttpStatus.CREATED).body(response);
    }

    @GetMapping
    public ResponseEntity<List<ApiKeyResponseDto>> listKeys() {
        List<ApiKeyResponseDto> keys = apiKeyService.listAllKeys();
        return ResponseEntity.ok(keys);
    }

    @DeleteMapping("/{id}")
    public ResponseEntity<?> revokeKey(
            @PathVariable Long id,
            @RequestParam(name = "permanent", defaultValue = "false") boolean permanent,
            HttpServletRequest servletRequest
    ) {
        String clientIp = ClientIpResolver.resolveClientIp(servletRequest);
        if (permanent) {
            boolean deleted = apiKeyService.deleteKey(id, clientIp);
            if (!deleted) {
                return ResponseEntity.status(HttpStatus.NOT_FOUND)
                        .body(Map.of("error", "Not Found", "message", "API key not found with id " + id));
            }
            return ResponseEntity.ok(Map.of("id", id, "status", "DELETED", "message", "API key permanently deleted"));
        } else {
            boolean revoked = apiKeyService.revokeKey(id, clientIp);
            if (!revoked) {
                return ResponseEntity.status(HttpStatus.NOT_FOUND)
                        .body(Map.of("error", "Not Found", "message", "API key not found with id " + id));
            }
            return ResponseEntity.ok(Map.of("id", id, "status", "REVOKED", "message", "API key revoked"));
        }
    }
}
