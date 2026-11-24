package com.sdn.poultry.security;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.sdn.poultry.entity.ApiKey;
import com.sdn.poultry.repository.ApiKeyRepository;
import com.sdn.poultry.service.PersistenceService;
import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.core.Ordered;
import org.springframework.core.annotation.Order;
import org.springframework.http.MediaType;
import org.springframework.stereotype.Component;
import org.springframework.util.AntPathMatcher;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.time.Instant;
import java.util.HexFormat;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;

@Component
@Order(Ordered.HIGHEST_PRECEDENCE + 20)
public class ApiKeyAuthenticationFilter extends OncePerRequestFilter {

    private static final Logger log = LoggerFactory.getLogger(ApiKeyAuthenticationFilter.class);

    private final ApiKeyRepository apiKeyRepository;
    private final PersistenceService persistenceService;
    private final ObjectMapper objectMapper;
    private final AntPathMatcher pathMatcher = new AntPathMatcher();

    // Whitelisted public endpoint patterns (all methods)
    private static final List<String> PUBLIC_ALL_METHOD_PATTERNS = List.of(
            "/ws",
            "/ws/**",
            "/ws-plain",
            "/ws-plain/**",
            "/api/auth",
            "/api/auth/**"
    );

    // Whitelisted public GET endpoint patterns
    private static final List<String> PUBLIC_GET_PATTERNS = List.of(
            "/",
            "/index.html",
            "/favicon.ico",
            "/css/**",
            "/js/**",
            "/assets/**",
            "/images/**",
            "/error",
            "/api/status",
            "/api/status/**",
            "/api/stats",
            "/api/stats/**",
            "/api/galpones",
            "/api/galpones/**",
            "/api/telemetry/**",
            "/api/sdn/**",
            "/api/docs",
            "/api/docs/**"
    );

    // Protected patterns (require valid API Key)
    private static final List<String> PROTECTED_PATTERNS = List.of(
            "/api/sdn/downlink",
            "/api/devices/**",
            "/api/devices",
            "/api/keys/**",
            "/api/docs/generate"
    );

    public ApiKeyAuthenticationFilter(ApiKeyRepository apiKeyRepository,
                                      PersistenceService persistenceService,
                                      ObjectMapper objectMapper) {
        this.apiKeyRepository = apiKeyRepository;
        this.persistenceService = persistenceService;
        this.objectMapper = objectMapper;
    }

    @Override
    protected void doFilterInternal(HttpServletRequest request,
                                    HttpServletResponse response,
                                    FilterChain filterChain) throws ServletException, IOException {
        // 0. If already authenticated by upstream RBAC filter, pass through
        if (request.getAttribute("authenticatedUser") != null || request.getAttribute("authenticatedApiKey") != null) {
            filterChain.doFilter(request, response);
            return;
        }

        String path = request.getRequestURI();
        String method = request.getMethod();

        // 1. Check if public route
        if (isPublicRoute(method, path)) {
            filterChain.doFilter(request, response);
            return;
        }

        // 2. Extract API key
        String rawKey = extractApiKey(request);
        String clientIp = ClientIpResolver.resolveClientIp(request);

        if (rawKey == null || rawKey.isBlank()) {
            handleUnauthorized(response, clientIp, path, "API key missing or invalid");
            return;
        }

        // 3. Compute SHA-256 hex
        String keyHash = sha256Hex(rawKey);

        // 4. Query repository for active key
        Optional<ApiKey> keyOpt = apiKeyRepository.findByKeyHashAndActiveTrue(keyHash);
        if (keyOpt.isEmpty()) {
            handleUnauthorized(response, clientIp, path, "API key missing or invalid");
            return;
        }

        ApiKey apiKey = keyOpt.get();

        // 5. Verify validity (active and unexpired)
        if (!apiKey.isValid()) {
            handleUnauthorized(response, clientIp, path, "API key missing or invalid");
            return;
        }

        // 6. Update lastUsedAt asynchronously
        CompletableFuture.runAsync(() -> {
            try {
                apiKey.setLastUsedAt(Instant.now());
                apiKeyRepository.save(apiKey);
            } catch (Exception ex) {
                log.debug("Failed to update lastUsedAt for key id {}: {}", apiKey.getId(), ex.getMessage());
            }
        });

        // 7. Success: attach to request attribute and continue
        request.setAttribute("authenticatedApiKey", apiKey);
        filterChain.doFilter(request, response);
    }

    public boolean isPublicRoute(String method, String path) {
        if ("OPTIONS".equalsIgnoreCase(method)) {
            return true;
        }

        // All-method whitelists (WebSocket / SockJS)
        for (String pattern : PUBLIC_ALL_METHOD_PATTERNS) {
            if (pathMatcher.match(pattern, path)) {
                return true;
            }
        }

        // POST /api/sdn/preview is public
        if ("POST".equalsIgnoreCase(method) && pathMatcher.match("/api/sdn/preview", path)) {
            return true;
        }

        // GET and HEAD public endpoints
        if ("GET".equalsIgnoreCase(method) || "HEAD".equalsIgnoreCase(method)) {
            for (String pattern : PUBLIC_GET_PATTERNS) {
                if (pathMatcher.match(pattern, path)) {
                    return true;
                }
            }
        }

        // Explicitly check protected patterns for mutating methods
        for (String pattern : PROTECTED_PATTERNS) {
            if (pathMatcher.match(pattern, path)) {
                return false;
            }
        }

        // Default to public if non-mutating, else require auth
        return "GET".equalsIgnoreCase(method) || "HEAD".equalsIgnoreCase(method);
    }

    public static String extractApiKey(HttpServletRequest request) {
        if (request == null) {
            return null;
        }

        String xApiKey = request.getHeader("X-API-Key");
        if (xApiKey != null && !xApiKey.isBlank()) {
            return xApiKey.trim();
        }

        String authHeader = request.getHeader("Authorization");
        if (authHeader != null && authHeader.regionMatches(true, 0, "Bearer ", 0, 7)) {
            String token = authHeader.substring(7).trim();
            if (!token.isBlank()) {
                return token;
            }
        }

        return null;
    }

    public static String sha256Hex(String input) {
        if (input == null) {
            return null;
        }
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            byte[] digest = md.digest(input.getBytes(StandardCharsets.UTF_8));
            return HexFormat.of().formatHex(digest);
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException("SHA-256 algorithm not available", e);
        }
    }

    private void handleUnauthorized(HttpServletResponse response,
                                    String clientIp,
                                    String path,
                                    String message) throws IOException {
        try {
            persistenceService.logAudit("AUTH_FAILURE", "Unauthorized access attempt to " + path, "Client IP: " + clientIp, clientIp);
        } catch (Exception e) {
            log.warn("Failed to log audit for auth failure: {}", e.getMessage());
        }

        response.setStatus(HttpServletResponse.SC_UNAUTHORIZED); // 401
        response.setContentType(MediaType.APPLICATION_JSON_VALUE);
        response.setCharacterEncoding(StandardCharsets.UTF_8.name());

        Map<String, String> body = Map.of(
                "error", "Unauthorized",
                "message", message
        );
        response.getWriter().write(objectMapper.writeValueAsString(body));
    }
}
