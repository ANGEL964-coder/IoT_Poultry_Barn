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
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.core.Ordered;
import org.springframework.core.annotation.Order;
import org.springframework.http.MediaType;
import org.springframework.stereotype.Component;
import org.springframework.util.AntPathMatcher;
import org.springframework.web.filter.OncePerRequestFilter;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;

@Component
@Order(Ordered.HIGHEST_PRECEDENCE + 15)
public class RbacAuthorizationFilter extends OncePerRequestFilter {

    private static final Logger log = LoggerFactory.getLogger(RbacAuthorizationFilter.class);

    private final UserSessionService userSessionService;
    private final ApiKeyRepository apiKeyRepository;
    private final PersistenceService persistenceService;
    private final ObjectMapper objectMapper;
    private final AntPathMatcher pathMatcher = new AntPathMatcher();

    // Whitelisted public endpoints (accessible without authentication)
    private static final List<String> PUBLIC_PATTERNS = List.of(
            "/",
            "/index.html",
            "/favicon.ico",
            "/css/**",
            "/js/**",
            "/assets/**",
            "/images/**",
            "/error",
            "/api/auth/login",
            "/api/status",
            "/api/status/**",
            "/api/docs/status",
            "/api/sdn/preview",
            "/ws",
            "/ws/**",
            "/ws-plain",
            "/ws-plain/**"
    );

    // Admin-only endpoints requiring ROLE_ADMIN
    private static final List<String> ADMIN_ANY_METHOD_PATTERNS = List.of(
            "/api/keys",
            "/api/keys/**"
    );

    private static final List<String> ADMIN_POST_PATTERNS = List.of(
            "/api/sdn/downlink",
            "/api/docs/generate",
            "/api/devices",
            "/api/devices/",
            "/api/devices/**/scale/tare",
            "/api/devices/**/scale/calibrate",
            "/api/devices/**/scale/tare/clear"
    );

    @Autowired
    public RbacAuthorizationFilter(
            UserSessionService userSessionService,
            ApiKeyRepository apiKeyRepository,
            @Autowired(required = false) PersistenceService persistenceService,
            ObjectMapper objectMapper
    ) {
        this.userSessionService = userSessionService;
        this.apiKeyRepository = apiKeyRepository;
        this.persistenceService = persistenceService;
        this.objectMapper = objectMapper;
    }

    @Override
    protected void doFilterInternal(
            HttpServletRequest request,
            HttpServletResponse response,
            FilterChain filterChain
    ) throws ServletException, IOException {

        String rawPath = request.getRequestURI();
        String path = (rawPath != null && rawPath.length() > 1 && rawPath.endsWith("/"))
                ? rawPath.substring(0, rawPath.length() - 1)
                : rawPath;
        String method = request.getMethod();

        // 1. CORS Pre-flight OPTIONS requests always pass through
        if ("OPTIONS".equalsIgnoreCase(method)) {
            filterChain.doFilter(request, response);
            return;
        }

        // 2. Resolve credentials (Bearer token, X-Auth-Token, or X-API-Key)
        String credential = extractCredential(request);
        UserSessionService.UserSession principal = resolvePrincipal(credential, request);

        // 3. Attach principal to request attribute if resolved
        if (principal != null) {
            request.setAttribute("authenticatedUser", principal);
            request.setAttribute("authenticatedRole", principal.getRole());
            request.setAttribute("authenticatedUsername", principal.getUsername());
        }

        // 4. Check if public route
        if (isPublicRoute(path)) {
            filterChain.doFilter(request, response);
            return;
        }

        String clientIp = ClientIpResolver.resolveClientIp(request);

        // 5. Protected route without authentication -> 401 Unauthorized
        if (principal == null) {
            handleUnauthorized(response, clientIp, path, "Autenticación requerida");
            return;
        }

        // 6. Check RBAC: Admin-only paths require ROLE_ADMIN
        if (isAdminOnlyRoute(method, path)) {
            if (!"ROLE_ADMIN".equalsIgnoreCase(principal.getRole())) {
                handleForbidden(response, clientIp, path, "Acceso denegado: Se requiere rol ROLE_ADMIN");
                return;
            }
        }

        // 7. Authorized -> proceed along the filter chain
        filterChain.doFilter(request, response);
    }

    /**
     * Extracts token or key from headers in priority order:
     * 1. Authorization: Bearer <token>
     * 2. X-Auth-Token: <token>
     * 3. X-API-Key: <key>
     */
    public static String extractCredential(HttpServletRequest request) {
        if (request == null) {
            return null;
        }

        String authHeader = request.getHeader("Authorization");
        if (authHeader != null && authHeader.regionMatches(true, 0, "Bearer ", 0, 7)) {
            String token = authHeader.substring(7).trim();
            if (!token.isBlank()) {
                return token;
            }
        }

        String xAuthToken = request.getHeader("X-Auth-Token");
        if (xAuthToken != null && !xAuthToken.isBlank()) {
            return xAuthToken.trim();
        }

        String xApiKey = request.getHeader("X-API-Key");
        if (xApiKey != null && !xApiKey.isBlank()) {
            return xApiKey.trim();
        }

        return null;
    }

    private UserSessionService.UserSession resolvePrincipal(String credential, HttpServletRequest request) {
        if (credential == null || credential.isBlank()) {
            return null;
        }

        // 1. Try session manager
        Optional<UserSessionService.UserSession> sessionOpt = userSessionService.getSession(credential);
        if (sessionOpt.isPresent()) {
            return sessionOpt.get();
        }

        // 2. Try database API keys
        if (apiKeyRepository != null) {
            String keyHash = ApiKeyAuthenticationFilter.sha256Hex(credential);
            Optional<ApiKey> apiKeyOpt = apiKeyRepository.findByKeyHashAndActiveTrue(keyHash);
            if (apiKeyOpt.isPresent()) {
                ApiKey apiKey = apiKeyOpt.get();
                if (apiKey.isValid()) {
                    request.setAttribute("authenticatedApiKey", apiKey);

                    // Asynchronously update lastUsedAt
                    CompletableFuture.runAsync(() -> {
                        try {
                            apiKey.setLastUsedAt(Instant.now());
                            apiKeyRepository.save(apiKey);
                        } catch (Exception e) {
                            log.debug("Failed updating lastUsedAt for key {}: {}", apiKey.getId(), e.getMessage());
                        }
                    });

                    String desc = apiKey.getDescription() != null ? apiKey.getDescription().toLowerCase() : "";
                    String role = (desc.contains("operador") || desc.contains("operator")) ? "ROLE_OPERADOR" : "ROLE_ADMIN";
                    String username = apiKey.getDescription() != null ? apiKey.getDescription() : "api-client";

                    return new UserSessionService.UserSession(
                            credential,
                            username,
                            role,
                            username,
                            "AK",
                            credential,
                            apiKey.getCreatedAt() != null ? apiKey.getCreatedAt() : Instant.now()
                    );
                }
            }
        }

        return null;
    }

    private String normalizePath(String rawPath) {
        if (rawPath != null && rawPath.length() > 1 && rawPath.endsWith("/")) {
            return rawPath.substring(0, rawPath.length() - 1);
        }
        return rawPath;
    }

    public boolean isPublicRoute(String path) {
        String normalized = normalizePath(path);
        for (String pattern : PUBLIC_PATTERNS) {
            if (pathMatcher.match(pattern, normalized)) {
                return true;
            }
        }
        return false;
    }

    public boolean isAdminOnlyRoute(String method, String path) {
        String normalized = normalizePath(path);
        // Any method on /api/keys/**
        for (String pattern : ADMIN_ANY_METHOD_PATTERNS) {
            if (pathMatcher.match(pattern, normalized)) {
                return true;
            }
        }

        // POST requests on administrative routes
        if ("POST".equalsIgnoreCase(method)) {
            for (String pattern : ADMIN_POST_PATTERNS) {
                if (pathMatcher.match(pattern, normalized)) {
                    return true;
                }
            }
        }

        // PUT and DELETE on devices
        if ("PUT".equalsIgnoreCase(method) || "DELETE".equalsIgnoreCase(method)) {
            if (pathMatcher.match("/api/devices", normalized) || pathMatcher.match("/api/devices/**", normalized)) {
                return true;
            }
        }

        return false;
    }

    private void handleUnauthorized(
            HttpServletResponse response,
            String clientIp,
            String path,
            String message
    ) throws IOException {
        if (persistenceService != null) {
            try {
                persistenceService.logAudit("AUTH_FAILURE", "Unauthorized access attempt to " + path, "Client IP: " + clientIp, clientIp);
            } catch (Exception e) {
                log.warn("Failed to log audit for auth failure: {}", e.getMessage());
            }
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

    private void handleForbidden(
            HttpServletResponse response,
            String clientIp,
            String path,
            String message
    ) throws IOException {
        if (persistenceService != null) {
            try {
                persistenceService.logAudit("ACCESS_DENIED", "Forbidden access attempt to " + path, "Client IP: " + clientIp, clientIp);
            } catch (Exception e) {
                log.warn("Failed to log audit for forbidden access: {}", e.getMessage());
            }
        }

        response.setStatus(HttpServletResponse.SC_FORBIDDEN); // 403
        response.setContentType(MediaType.APPLICATION_JSON_VALUE);
        response.setCharacterEncoding(StandardCharsets.UTF_8.name());

        Map<String, String> body = Map.of(
                "error", "Forbidden",
                "message", message
        );
        response.getWriter().write(objectMapper.writeValueAsString(body));
    }
}
