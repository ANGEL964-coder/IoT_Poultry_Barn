package com.sdn.poultry.security;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.sdn.poultry.entity.ApiKey;
import com.sdn.poultry.repository.ApiKeyRepository;
import com.sdn.poultry.service.PersistenceService;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.mockito.Mockito;
import org.springframework.mock.web.MockFilterChain;
import org.springframework.mock.web.MockHttpServletRequest;
import org.springframework.mock.web.MockHttpServletResponse;

import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.Optional;

import static org.assertj.core.api.Assertions.assertThat;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.*;

class ApiKeyAuthenticationFilterTest {

    private ApiKeyRepository apiKeyRepository;
    private PersistenceService persistenceService;
    private ApiKeyAuthenticationFilter filter;
    private final ObjectMapper objectMapper = new ObjectMapper();

    @BeforeEach
    void setUp() {
        apiKeyRepository = mock(ApiKeyRepository.class);
        persistenceService = mock(PersistenceService.class);
        filter = new ApiKeyAuthenticationFilter(apiKeyRepository, persistenceService, objectMapper);
    }

    @Test
    @DisplayName("Should block protected route when API key is missing")
    void testMissingApiKeyReturns401() throws Exception {
        MockHttpServletRequest request = new MockHttpServletRequest("POST", "/api/sdn/downlink");
        MockHttpServletResponse response = new MockHttpServletResponse();
        MockFilterChain chain = new MockFilterChain();

        filter.doFilter(request, response, chain);

        assertThat(response.getStatus()).isEqualTo(401);
        assertThat(response.getContentType()).contains("application/json");
        assertThat(response.getContentAsString()).contains("API key missing or invalid");

        verify(persistenceService).logAudit(eq("AUTH_FAILURE"), anyString(), anyString(), anyString());
    }

    @Test
    @DisplayName("Should block protected route when API key is invalid/unknown")
    void testInvalidApiKeyReturns401() throws Exception {
        String invalidKey = "sdn_live_unknownkey1234567890abcdef";
        String hash = ApiKeyAuthenticationFilter.sha256Hex(invalidKey);

        when(apiKeyRepository.findByKeyHashAndActiveTrue(hash)).thenReturn(Optional.empty());

        MockHttpServletRequest request = new MockHttpServletRequest("POST", "/api/sdn/downlink");
        request.addHeader("X-API-Key", invalidKey);
        MockHttpServletResponse response = new MockHttpServletResponse();
        MockFilterChain chain = new MockFilterChain();

        filter.doFilter(request, response, chain);

        assertThat(response.getStatus()).isEqualTo(401);
        verify(persistenceService).logAudit(eq("AUTH_FAILURE"), anyString(), anyString(), anyString());
    }

    @Test
    @DisplayName("Should block protected route when API key is expired")
    void testExpiredApiKeyReturns401() throws Exception {
        String rawKey = "sdn_live_expiredkey1234567890abcdef";
        String hash = ApiKeyAuthenticationFilter.sha256Hex(rawKey);

        ApiKey expiredKey = new ApiKey(hash, "sdn_live_expiredk", "Expired Test Key");
        expiredKey.setActive(true);
        expiredKey.setExpiresAt(Instant.now().minus(1, ChronoUnit.DAYS));

        when(apiKeyRepository.findByKeyHashAndActiveTrue(hash)).thenReturn(Optional.of(expiredKey));

        MockHttpServletRequest request = new MockHttpServletRequest("POST", "/api/sdn/downlink");
        request.addHeader("X-API-Key", rawKey);
        MockHttpServletResponse response = new MockHttpServletResponse();
        MockFilterChain chain = new MockFilterChain();

        filter.doFilter(request, response, chain);

        assertThat(response.getStatus()).isEqualTo(401);
        verify(persistenceService).logAudit(eq("AUTH_FAILURE"), anyString(), anyString(), anyString());
    }

    @Test
    @DisplayName("Should allow protected route when valid API key is passed in X-API-Key header")
    void testValidApiKeyViaXApiKeyHeader() throws Exception {
        String rawKey = "sdn_live_validkey1234567890abcdef1234567890abcdef1234567890abcdef";
        String hash = ApiKeyAuthenticationFilter.sha256Hex(rawKey);

        ApiKey validKey = new ApiKey(hash, "sdn_live_validkey", "Valid Test Key");
        validKey.setActive(true);
        validKey.setExpiresAt(Instant.now().plus(30, ChronoUnit.DAYS));

        when(apiKeyRepository.findByKeyHashAndActiveTrue(hash)).thenReturn(Optional.of(validKey));

        MockHttpServletRequest request = new MockHttpServletRequest("POST", "/api/sdn/downlink");
        request.addHeader("X-API-Key", rawKey);
        MockHttpServletResponse response = new MockHttpServletResponse();
        MockFilterChain chain = new MockFilterChain();

        filter.doFilter(request, response, chain);

        assertThat(response.getStatus()).isEqualTo(200);
        assertThat(request.getAttribute("authenticatedApiKey")).isEqualTo(validKey);
        verify(persistenceService, never()).logAudit(eq("AUTH_FAILURE"), anyString(), anyString(), anyString());
    }

    @Test
    @DisplayName("Should allow protected route when valid API key is passed in Authorization: Bearer header")
    void testValidApiKeyViaBearerHeader() throws Exception {
        String rawKey = "sdn_live_bearerkey1234567890abcdef1234567890abcdef1234567890abcde";
        String hash = ApiKeyAuthenticationFilter.sha256Hex(rawKey);

        ApiKey validKey = new ApiKey(hash, "sdn_live_bearerk", "Bearer Test Key");
        validKey.setActive(true);

        when(apiKeyRepository.findByKeyHashAndActiveTrue(hash)).thenReturn(Optional.of(validKey));

        MockHttpServletRequest request = new MockHttpServletRequest("POST", "/api/keys/generate");
        request.addHeader("Authorization", "Bearer " + rawKey);
        MockHttpServletResponse response = new MockHttpServletResponse();
        MockFilterChain chain = new MockFilterChain();

        filter.doFilter(request, response, chain);

        assertThat(response.getStatus()).isEqualTo(200);
        assertThat(request.getAttribute("authenticatedApiKey")).isEqualTo(validKey);
    }

    @Test
    @DisplayName("Should bypass authentication on public observability and preview routes")
    void testPublicRoutesBypassAuthentication() throws Exception {
        String[] publicRoutes = {
                "/api/status",
                "/api/stats",
                "/api/galpones",
                "/api/galpones/1234567890abcdef",
                "/api/sdn/downlinks",
                "/api/docs/status"
        };

        for (String path : publicRoutes) {
            MockHttpServletRequest req = new MockHttpServletRequest("GET", path);
            MockHttpServletResponse resp = new MockHttpServletResponse();
            MockFilterChain chain = new MockFilterChain();

            filter.doFilter(req, resp, chain);
            assertThat(resp.getStatus()).as("Path " + path + " should be public").isEqualTo(200);
        }

        // Test POST /api/sdn/preview
        MockHttpServletRequest previewReq = new MockHttpServletRequest("POST", "/api/sdn/preview");
        MockHttpServletResponse previewResp = new MockHttpServletResponse();
        MockFilterChain previewChain = new MockFilterChain();
        filter.doFilter(previewReq, previewResp, previewChain);
        assertThat(previewResp.getStatus()).isEqualTo(200);
    }
}
