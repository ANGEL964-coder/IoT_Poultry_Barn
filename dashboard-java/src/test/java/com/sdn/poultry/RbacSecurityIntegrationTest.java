package com.sdn.poultry;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.sdn.poultry.security.RateLimiterService;
import com.sdn.poultry.security.UserSessionService;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.AutoConfigureMockMvc;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.http.MediaType;
import org.springframework.test.context.TestPropertySource;
import org.springframework.test.web.servlet.MockMvc;
import org.springframework.test.web.servlet.MvcResult;

import java.util.Map;

import static org.assertj.core.api.Assertions.assertThat;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.*;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.*;

@SpringBootTest
@AutoConfigureMockMvc
@TestPropertySource(locations = "classpath:application-test.properties")
class RbacSecurityIntegrationTest {

    @Autowired
    private MockMvc mockMvc;

    @Autowired
    private UserSessionService userSessionService;

    @Autowired
    private RateLimiterService rateLimiterService;

    @Autowired
    private ObjectMapper objectMapper;

    private String adminToken;
    private String operatorToken;

    @BeforeEach
    void setUp() throws Exception {
        rateLimiterService.resetAll();

        // 1. Authenticate as Admin
        MvcResult adminLoginResult = mockMvc.perform(post("/api/auth/login")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(objectMapper.writeValueAsString(Map.of("username", "admin", "password", "server"))))
                .andExpect(status().isOk())
                .andReturn();

        JsonNode adminJson = objectMapper.readTree(adminLoginResult.getResponse().getContentAsString());
        adminToken = adminJson.get("token").asText();

        // 2. Authenticate as Operator
        MvcResult operatorLoginResult = mockMvc.perform(post("/api/auth/login")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(objectMapper.writeValueAsString(Map.of("username", "operador", "password", "operador123"))))
                .andExpect(status().isOk())
                .andReturn();

        JsonNode operatorJson = objectMapper.readTree(operatorLoginResult.getResponse().getContentAsString());
        operatorToken = operatorJson.get("token").asText();
    }

    @Test
    @DisplayName("R1: Unauthenticated requests to protected endpoints must return HTTP 401")
    void testUnauthenticatedAccessReturns401() throws Exception {
        // Protected GET routes
        mockMvc.perform(get("/api/auth/me")).andExpect(status().isUnauthorized());
        mockMvc.perform(get("/api/stats")).andExpect(status().isUnauthorized());
        mockMvc.perform(get("/api/galpones")).andExpect(status().isUnauthorized());
        mockMvc.perform(get("/api/modules/vision/70b3d57ed007985a")).andExpect(status().isUnauthorized());
        mockMvc.perform(get("/api/modules/scale/70b3d57ed007985a")).andExpect(status().isUnauthorized());
        mockMvc.perform(get("/api/modules/climate/70b3d57ed007985a")).andExpect(status().isUnauthorized());
        mockMvc.perform(get("/api/modules/gateway")).andExpect(status().isUnauthorized());
        mockMvc.perform(get("/api/devices")).andExpect(status().isUnauthorized());

        // Admin-only routes
        mockMvc.perform(get("/api/keys")).andExpect(status().isUnauthorized());
        mockMvc.perform(post("/api/sdn/downlink")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content("{\"devEui\":\"70b3d57ed007985a\",\"cmdId\":1}"))
                .andExpect(status().isUnauthorized());

        mockMvc.perform(post("/api/devices/70b3d57ed007985a/scale/tare"))
                .andExpect(status().isUnauthorized());

        mockMvc.perform(post("/api/devices/70b3d57ed007985a/scale/calibrate")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content("{\"calSlope\":420.5,\"calZero\":0.125}"))
                .andExpect(status().isUnauthorized());
    }

    @Test
    @DisplayName("R1: Login authentication succeeds for admin and operator, rejects bad credentials")
    void testLoginAuthenticationSuccessAndFailure() throws Exception {
        // Successful Admin login
        mockMvc.perform(post("/api/auth/login")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(objectMapper.writeValueAsString(Map.of("username", "admin", "password", "server"))))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.status").value("SUCCESS"))
                .andExpect(jsonPath("$.role").value("ROLE_ADMIN"))
                .andExpect(jsonPath("$.username").value("admin"))
                .andExpect(jsonPath("$.token").isString())
                .andExpect(jsonPath("$.apiKey").value("master-test-key-2026"));

        // Successful Operator login
        mockMvc.perform(post("/api/auth/login")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(objectMapper.writeValueAsString(Map.of("username", "operador", "password", "operador123"))))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.status").value("SUCCESS"))
                .andExpect(jsonPath("$.role").value("ROLE_OPERADOR"))
                .andExpect(jsonPath("$.username").value("operador"))
                .andExpect(jsonPath("$.token").isString());

        // Invalid credentials return 401
        mockMvc.perform(post("/api/auth/login")
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(objectMapper.writeValueAsString(Map.of("username", "admin", "password", "wrong-password"))))
                .andExpect(status().isUnauthorized())
                .andExpect(jsonPath("$.error").value("Unauthorized"));
    }

    @Test
    @DisplayName("R1: Operator cannot execute administrative actions (downlink, keys, tare, calibrate, docs) -> returns 403")
    void testOperatorCannotExecuteAdminActions() throws Exception {
        String authHeader = "Bearer " + operatorToken;

        // 1. Operator blocked from POST /api/sdn/downlink -> 403 Forbidden
        mockMvc.perform(post("/api/sdn/downlink")
                        .header("Authorization", authHeader)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content("{\"devEui\":\"70b3d57ed007985a\",\"cmdId\":1}"))
                .andExpect(status().isForbidden())
                .andExpect(jsonPath("$.error").value("Forbidden"))
                .andExpect(jsonPath("$.message").value("Acceso denegado: Se requiere rol ROLE_ADMIN"));

        // 2. Operator blocked from /api/keys -> 403 Forbidden
        mockMvc.perform(get("/api/keys").header("Authorization", authHeader))
                .andExpect(status().isForbidden());

        // 3. Operator blocked from tare -> 403 Forbidden
        mockMvc.perform(post("/api/devices/70b3d57ed007985a/scale/tare")
                        .header("Authorization", authHeader))
                .andExpect(status().isForbidden());

        // 4. Operator blocked from calibrate -> 403 Forbidden
        mockMvc.perform(post("/api/devices/70b3d57ed007985a/scale/calibrate")
                        .header("Authorization", authHeader)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content("{\"calSlope\":420.5,\"calZero\":0.125}"))
                .andExpect(status().isForbidden());

        // 5. Operator blocked from docs generation -> 403 Forbidden
        mockMvc.perform(post("/api/docs/generate")
                        .header("Authorization", authHeader))
                .andExpect(status().isForbidden());

        // 6. Operator CAN access read-only protected endpoints
        mockMvc.perform(get("/api/auth/me").header("Authorization", authHeader))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.role").value("ROLE_OPERADOR"));

        mockMvc.perform(get("/api/devices/70b3d57ed007985a/scale/calibration").header("Authorization", authHeader))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.calSlope").exists());

        mockMvc.perform(get("/api/modules/vision/70b3d57ed007985a").header("Authorization", authHeader))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.available").isBoolean());
    }

    @Test
    @DisplayName("R1 & R3: Admin can execute administrative, downlink, and calibration functions -> returns 200")
    void testAdminCanExecuteAllActions() throws Exception {
        String authHeader = "Bearer " + adminToken;

        // 1. Admin reads profile
        mockMvc.perform(get("/api/auth/me").header("Authorization", authHeader))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.role").value("ROLE_ADMIN"))
                .andExpect(jsonPath("$.username").value("admin"));

        // 2. Admin reads keys
        mockMvc.perform(get("/api/keys").header("Authorization", authHeader))
                .andExpect(status().isOk());

        // 3. Admin executes scale tare
        mockMvc.perform(post("/api/devices/70b3d57ed007985a/scale/tare")
                        .header("Authorization", authHeader))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.status").value("SUCCESS"))
                .andExpect(jsonPath("$.tareKg").value(15.50));

        // 4. Admin executes scale calibration
        mockMvc.perform(post("/api/devices/70b3d57ed007985a/scale/calibrate")
                        .header("Authorization", authHeader)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content("{\"knownWeightKg\":10.0,\"calSlope\":420.5,\"calZero\":0.125}"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.status").value("SUCCESS"))
                .andExpect(jsonPath("$.calSlope").value(420.5))
                .andExpect(jsonPath("$.calZero").value(0.125));

        // 5. Admin clears tare
        mockMvc.perform(post("/api/devices/70b3d57ed007985a/scale/tare/clear")
                        .header("Authorization", authHeader))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.status").value("SUCCESS"));

        // 6. Admin accesses Module SCADA endpoints
        mockMvc.perform(get("/api/modules/vision/70b3d57ed007985a").header("Authorization", authHeader))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.available").value(false))
                .andExpect(jsonPath("$.cameraStatus").isEmpty());

        mockMvc.perform(get("/api/modules/scale/70b3d57ed007985a").header("Authorization", authHeader))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.available").isBoolean());

        mockMvc.perform(get("/api/modules/climate/70b3d57ed007985a").header("Authorization", authHeader))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.available").isBoolean());

        mockMvc.perform(get("/api/modules/gateway").header("Authorization", authHeader))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.model").value("Laird Sentrius RG1"));
    }

    @Test
    @DisplayName("R1: Master test key functions as ROLE_ADMIN with backward compatibility")
    void testMasterTestKeyFunctionsAsAdmin() throws Exception {
        // Authenticate via X-API-Key with master key
        mockMvc.perform(get("/api/auth/me").header("X-API-Key", UserSessionService.MASTER_KEY))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.role").value("ROLE_ADMIN"));

        // Master key can execute tare
        mockMvc.perform(post("/api/devices/70b3d57ed007985a/scale/tare")
                        .header("X-API-Key", UserSessionService.MASTER_KEY))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.status").value("SUCCESS"));

        // Master key can access /api/keys
        mockMvc.perform(get("/api/keys").header("X-API-Key", UserSessionService.MASTER_KEY))
                .andExpect(status().isOk());
    }

    @Test
    @DisplayName("R1: Logout invalidates active session token")
    void testLogoutInvalidatesSessionToken() throws Exception {
        // Verify token is initially valid
        mockMvc.perform(get("/api/auth/me").header("Authorization", "Bearer " + adminToken))
                .andExpect(status().isOk());

        // Call logout
        mockMvc.perform(post("/api/auth/logout").header("Authorization", "Bearer " + adminToken))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.status").value("SUCCESS"));

        // Subsequent requests with invalidated token must fail with 401
        mockMvc.perform(get("/api/auth/me").header("Authorization", "Bearer " + adminToken))
                .andExpect(status().isUnauthorized());
    }

    @Test
    @DisplayName("Regression Defect 1: Operator blocked from POST /api/devices/ with trailing slash -> returns 403 Forbidden")
    void testOperatorBlockedFromRegisterDeviceWithTrailingSlashReturns403() throws Exception {
        String authHeader = "Bearer " + operatorToken;

        // POST /api/devices/ (with trailing slash) must return 403 Forbidden for operator
        mockMvc.perform(post("/api/devices/")
                        .header("Authorization", authHeader)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content("{\"devEui\":\"70b3d57ed0079999\",\"name\":\"Trailing Slash Sensor\"}"))
                .andExpect(status().isForbidden())
                .andExpect(jsonPath("$.error").value("Forbidden"))
                .andExpect(jsonPath("$.message").value("Acceso denegado: Se requiere rol ROLE_ADMIN"));
    }

    @Test
    @DisplayName("Regression Defect 2: POST /api/devices/{devEui}/scale/calibrate with null/non-numeric falls back to defaults without HTTP 500")
    void testCalibrateWithNullOrNonNumericValuesFallsBackToDefaults() throws Exception {
        String authHeader = "Bearer " + adminToken;

        // Test with {"calSlope": null}
        mockMvc.perform(post("/api/devices/70b3d57ed007985a/scale/calibrate")
                        .header("Authorization", authHeader)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content("{\"calSlope\":null}"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.status").value("SUCCESS"))
                .andExpect(jsonPath("$.devEui").value("70b3d57ed007985a"))
                .andExpect(jsonPath("$.calSlope").value(420.5))
                .andExpect(jsonPath("$.calZero").value(0.125));

        // Test with non-numeric and mixed nulls: {"calSlope": "NaN", "calZero": null, "knownWeightKg": null}
        mockMvc.perform(post("/api/devices/70b3d57ed007985a/scale/calibrate")
                        .header("Authorization", authHeader)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content("{\"calSlope\":\"NaN\",\"calZero\":null,\"knownWeightKg\":null}"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.status").value("SUCCESS"))
                .andExpect(jsonPath("$.calSlope").value(420.5))
                .andExpect(jsonPath("$.calZero").value(0.125));
    }
}
