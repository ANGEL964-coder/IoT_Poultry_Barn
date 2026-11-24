package com.sdn.poultry.controller;

import com.sdn.poultry.security.RbacAuthorizationFilter;
import com.sdn.poultry.security.UserSessionService;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Optional;

@RestController
@RequestMapping("/api/auth")
@CrossOrigin(origins = "*")
public class AuthController {

    private final UserSessionService sessionService;

    public AuthController(UserSessionService sessionService) {
        this.sessionService = sessionService;
    }

    @PostMapping("/login")
    public ResponseEntity<?> login(@RequestBody(required = false) Map<String, String> credentials) {
        if (credentials == null) {
            return ResponseEntity.status(HttpStatus.UNAUTHORIZED).body(Map.of(
                    "error", "Unauthorized",
                    "message", "Credenciales inválidas"
            ));
        }

        String username = credentials.getOrDefault("username", "").trim();
        String password = credentials.getOrDefault("password", "").trim();

        // 1. Admin login (credentials: admin / server or admin / admin123 or admin / admin)
        if ("admin".equalsIgnoreCase(username) && ("server".equals(password) || "admin123".equals(password) || "admin".equals(password))) {
            UserSessionService.UserSession session = sessionService.createSession(
                    "admin",
                    "ROLE_ADMIN",
                    "Julio C. (Administrador SDN)",
                    "JC",
                    UserSessionService.MASTER_KEY
            );

            Map<String, Object> resp = new LinkedHashMap<>();
            resp.put("status", "SUCCESS");
            resp.put("token", session.getToken());
            resp.put("username", session.getUsername());
            resp.put("role", session.getRole());
            resp.put("name", session.getDisplayName());
            resp.put("initials", session.getInitials());
            resp.put("apiKey", session.getApiKey());
            return ResponseEntity.ok(resp);
        }

        // 2. Operator login (credentials: operador / operador123 or operador / server or operador / operador)
        if ("operador".equalsIgnoreCase(username) && ("operador123".equals(password) || "server".equals(password) || "operador".equals(password))) {
            UserSessionService.UserSession session = sessionService.createSession(
                    "operador",
                    "ROLE_OPERADOR",
                    "Carlos Mendoza (Operador de Galpón)",
                    "CM",
                    UserSessionService.OPERATOR_KEY
            );

            Map<String, Object> resp = new LinkedHashMap<>();
            resp.put("status", "SUCCESS");
            resp.put("token", session.getToken());
            resp.put("username", session.getUsername());
            resp.put("role", session.getRole());
            resp.put("name", session.getDisplayName());
            resp.put("initials", session.getInitials());
            resp.put("apiKey", session.getApiKey());
            return ResponseEntity.ok(resp);
        }

        // Invalid credentials
        return ResponseEntity.status(HttpStatus.UNAUTHORIZED).body(Map.of(
                "error", "Unauthorized",
                "message", "Credenciales inválidas"
        ));
    }

    @GetMapping("/me")
    public ResponseEntity<?> getCurrentUser(HttpServletRequest request) {
        String credential = RbacAuthorizationFilter.extractCredential(request);
        if (credential == null || credential.isBlank()) {
            return ResponseEntity.status(HttpStatus.UNAUTHORIZED).body(Map.of(
                    "error", "Unauthorized",
                    "message", "Sesión inválida o expirada"
            ));
        }

        Optional<UserSessionService.UserSession> sessionOpt = sessionService.getSession(credential);
        if (sessionOpt.isEmpty()) {
            return ResponseEntity.status(HttpStatus.UNAUTHORIZED).body(Map.of(
                    "error", "Unauthorized",
                    "message", "Sesión inválida o expirada"
            ));
        }

        UserSessionService.UserSession session = sessionOpt.get();
        Map<String, Object> resp = new LinkedHashMap<>();
        resp.put("username", session.getUsername());
        resp.put("role", session.getRole());
        resp.put("name", session.getDisplayName());
        resp.put("initials", session.getInitials());
        resp.put("apiKey", session.getApiKey());
        return ResponseEntity.ok(resp);
    }

    @PostMapping("/logout")
    public ResponseEntity<?> logout(HttpServletRequest request) {
        String credential = RbacAuthorizationFilter.extractCredential(request);
        if (credential != null && !credential.isBlank()) {
            sessionService.invalidateSession(credential);
        }
        return ResponseEntity.ok(Map.of(
                "status", "SUCCESS",
                "message", "Sesión cerrada correctamente"
        ));
    }
}
