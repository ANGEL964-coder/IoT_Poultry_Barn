package com.sdn.poultry;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.sdn.poultry.entity.ApiKey;
import com.sdn.poultry.model.ApiKeyCreateRequest;
import com.sdn.poultry.model.ApiKeyGenerateResponse;
import com.sdn.poultry.repository.ApiKeyRepository;
import com.sdn.poultry.security.ApiKeyAuthenticationFilter;
import com.sdn.poultry.security.RateLimiterService;
import com.sdn.poultry.service.ApiKeyService;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.AutoConfigureMockMvc;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.http.MediaType;
import org.springframework.test.context.TestPropertySource;
import org.springframework.test.web.servlet.MockMvc;

import java.io.File;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.time.Instant;
import java.time.temporal.ChronoUnit;

import static org.assertj.core.api.Assertions.assertThat;
import static org.hamcrest.Matchers.startsWith;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.*;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.*;

@SpringBootTest
@AutoConfigureMockMvc
@TestPropertySource(locations = "classpath:application-test.properties")
class SecurityAndIntegrationTest {

    @Autowired
    private MockMvc mockMvc;

    @Autowired
    private ApiKeyService apiKeyService;

    @Autowired
    private ApiKeyRepository apiKeyRepository;

    @Autowired
    private RateLimiterService rateLimiterService;

    @Autowired
    private ObjectMapper objectMapper;

    private String validRawKey;
    private Long validKeyId;

    @BeforeEach
    void setUp() {
        rateLimiterService.resetAll();

        // Create a known valid API key for integration tests
        ApiKeyCreateRequest req = new ApiKeyCreateRequest("Integration Test Key", 30, null);
        ApiKeyGenerateResponse resp = apiKeyService.generateApiKey(req, "127.0.0.1");
        validRawKey = resp.getRawKey();
        validKeyId = resp.getId();
    }

    @Test
    @DisplayName("SecurityHeaders: Injected on all HTTP responses")
    void testSecurityHeadersPresent() throws Exception {
        mockMvc.perform(get("/api/status"))
                .andExpect(status().isOk())
                .andExpect(header().string("X-Content-Type-Options", "nosniff"))
                .andExpect(header().string("X-Frame-Options", "SAMEORIGIN"))
                .andExpect(header().string("X-XSS-Protection", "1; mode=block"))
                .andExpect(header().string("Referrer-Policy", "strict-origin-when-cross-origin"))
                .andExpect(header().string("Content-Security-Policy", "default-src 'self' 'unsafe-inline' 'unsafe-eval' data: https: http:; font-src 'self' data: https:; img-src 'self' data: https: blob:; connect-src 'self' ws: wss: http: https:;"));
    }

    @Test
    @DisplayName("Public Routes: Observability endpoints accessible without API key")
    void testPublicEndpointsAccessibleWithoutKey() throws Exception {
        mockMvc.perform(get("/api/status"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.serverTime").exists());

        // Authenticated listing of keys
        mockMvc.perform(get("/api/keys").header("X-API-Key", validRawKey))
                .andExpect(status().isOk());

        mockMvc.perform(get("/api/docs/status"))
                .andExpect(status().isOk());

        // POST preview is public
        String previewBody = """
                {
                  "cmdId": 1,
                  "sfTarget": 0,
                  "txPower": 14,
                  "sampleRate": 1,
                  "payloadMode": 0,
                  "sleepEnable": 0,
                  "paramValue": 250
                }
                """;
        mockMvc.perform(post("/api/sdn/preview")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(previewBody))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.hexDump").exists())
                .andExpect(jsonPath("$.base64").exists());
    }

    @Test
    @DisplayName("Protected Routes: Missing API key returns 401 Unauthorized")
    void testMissingApiKeyReturns401() throws Exception {
        mockMvc.perform(post("/api/keys/generate")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content("{}"))
                .andExpect(status().isUnauthorized())
                .andExpect(jsonPath("$.error").value("Unauthorized"))
                .andExpect(jsonPath("$.message").exists());

        mockMvc.perform(post("/api/docs/generate"))
                .andExpect(status().isUnauthorized())
                .andExpect(jsonPath("$.error").value("Unauthorized"));

        mockMvc.perform(post("/api/sdn/downlink")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content("{\"devEui\":\"70b3d57ed007985a\"}"))
                .andExpect(status().isUnauthorized());
    }

    @Test
    @DisplayName("Protected Routes: Invalid or expired API key returns 401 Unauthorized")
    void testInvalidOrExpiredApiKeyReturns401() throws Exception {
        // 1. Invalid key
        mockMvc.perform(post("/api/docs/generate")
                        .header("X-API-Key", "sdn_live_totallyinvalidkey00000000000000000000000000000000000000000000"))
                .andExpect(status().isUnauthorized());

        // 2. Expired key
        String expiredRawKey = "sdn_live_expired11111111111111111111111111111111111111111111111111111111";
        String hash = ApiKeyAuthenticationFilter.sha256Hex(expiredRawKey);
        ApiKey expiredEntity = new ApiKey(hash, "sdn_live_expired1", "Expired Key");
        expiredEntity.setActive(true);
        expiredEntity.setExpiresAt(Instant.now().minus(2, ChronoUnit.HOURS));
        apiKeyRepository.save(expiredEntity);

        mockMvc.perform(post("/api/docs/generate")
                        .header("X-API-Key", expiredRawKey))
                .andExpect(status().isUnauthorized());

        // 3. Revoked key
        String revokedRawKey = "sdn_live_revoked22222222222222222222222222222222222222222222222222222222";
        String hashRevoked = ApiKeyAuthenticationFilter.sha256Hex(revokedRawKey);
        ApiKey revokedEntity = new ApiKey(hashRevoked, "sdn_live_revoked2", "Revoked Key");
        revokedEntity.setActive(false);
        apiKeyRepository.save(revokedEntity);

        mockMvc.perform(post("/api/docs/generate")
                        .header("X-API-Key", revokedRawKey))
                .andExpect(status().isUnauthorized());
    }

    @Test
    @DisplayName("Protected Routes: Valid X-API-Key header permits access")
    void testValidXApiKeyHeaderPermitsAccess() throws Exception {
        String body = """
                {
                  "description": "Secondary Integration Key",
                  "validDays": 15
                }
                """;

        mockMvc.perform(post("/api/keys/generate")
                        .header("X-API-Key", validRawKey)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(body))
                .andExpect(status().isCreated())
                .andExpect(jsonPath("$.id").exists())
                .andExpect(jsonPath("$.rawKey").value(startsWith("sdn_live_")))
                .andExpect(jsonPath("$.apiKey").value(startsWith("sdn_live_")))
                .andExpect(jsonPath("$.keyPrefix").exists())
                .andExpect(jsonPath("$.description").value("Secondary Integration Key"));
    }

    @Test
    @DisplayName("Protected Routes: Valid Authorization: Bearer header permits access")
    void testValidAuthorizationBearerHeaderPermitsAccess() throws Exception {
        mockMvc.perform(delete("/api/keys/" + validKeyId)
                        .header("Authorization", "Bearer " + validRawKey))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.status").value("REVOKED"));
    }

    @Test
    @DisplayName("Rate Limiting: Exceeding 120 requests/minute returns HTTP 429 Too Many Requests")
    void testRateLimiterReturns429WhenExceeded() throws Exception {
        String testIp = "192.168.99.1";

        for (int i = 0; i < 120; i++) {
            mockMvc.perform(get("/api/status")
                            .header("X-Forwarded-For", testIp))
                    .andExpect(status().isOk());
        }

        // 121st request
        mockMvc.perform(get("/api/status")
                        .header("X-Forwarded-For", testIp))
                .andExpect(status().is(429))
                .andExpect(header().string("Retry-After", "60"))
                .andExpect(jsonPath("$.error").value("Too Many Requests"))
                .andExpect(jsonPath("$.message").value("Rate limit exceeded. Maximum 120 requests per minute."));
    }

    @Test
    @DisplayName("Documentation Generation: Generates TECHNICAL_DOCUMENTATION.md on disk")
    void testDocumentationGenerationEndpoint() throws Exception {
        mockMvc.perform(post("/api/docs/generate?forceOffline=true")
                        .header("X-API-Key", validRawKey))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.status").value("SUCCESS"))
                .andExpect(jsonPath("$.filePath").exists())
                .andExpect(jsonPath("$.bytesWritten").isNumber());

        // Check status endpoint
        mockMvc.perform(get("/api/docs/status"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.exists").value(true))
                .andExpect(jsonPath("$.status").value("AVAILABLE"))
                .andExpect(jsonPath("$.fileSizeBytes").isNumber());

        // Verify file is readable and non-empty
        Path docPath = Paths.get("target/test-docs/TECHNICAL_DOCUMENTATION.md");
        if (!Files.exists(docPath) && Files.exists(Paths.get("dashboard-java/target/test-docs/TECHNICAL_DOCUMENTATION.md"))) {
            docPath = Paths.get("dashboard-java/target/test-docs/TECHNICAL_DOCUMENTATION.md");
        }
        assertThat(Files.exists(docPath)).isTrue();
        String content = Files.readString(docPath);
        assertThat(content).contains("SDN LoRaWAN Poultry Dashboard");
        assertThat(content).contains("Binary Telemetry Wire Protocol");
        assertThat(content).contains("SDN Control Plane Protocol (RFC 7426)");
        assertThat(content).contains("Security Architecture & API Key Management");
    }
}
