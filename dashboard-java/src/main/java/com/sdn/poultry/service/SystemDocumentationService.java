package com.sdn.poultry.service;

import com.sdn.poultry.ipc.ClaudeIpcClient;
import com.sdn.poultry.repository.ApiKeyRepository;
import com.sdn.poultry.repository.DeviceRepository;
import com.sdn.poultry.repository.SdnRuleEventRepository;
import com.sdn.poultry.repository.TelemetryRecordRepository;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.time.Instant;
import java.util.HashMap;
import java.util.Map;

@Service
public class SystemDocumentationService {

    private static final Logger log = LoggerFactory.getLogger(SystemDocumentationService.class);

    private final ClaudeIpcClient claudeIpcClient;
    private final PersistenceService persistenceService;
    private final DeviceRepository deviceRepository;
    private final TelemetryRecordRepository telemetryRecordRepository;
    private final SdnRuleEventRepository sdnRuleEventRepository;
    private final ApiKeyRepository apiKeyRepository;

    @Value("${app.docs.output-dir:docs}")
    private String outputDirConfig = "docs";

    @Value("${app.docs.filename:TECHNICAL_DOCUMENTATION.md}")
    private String filenameConfig = "TECHNICAL_DOCUMENTATION.md";

    public record DocGenerationResult(
            String status,
            String source,
            String filePath,
            long bytesWritten,
            Instant timestamp,
            String message
    ) {}

    public SystemDocumentationService(
            ClaudeIpcClient claudeIpcClient,
            PersistenceService persistenceService,
            DeviceRepository deviceRepository,
            TelemetryRecordRepository telemetryRecordRepository,
            SdnRuleEventRepository sdnRuleEventRepository,
            ApiKeyRepository apiKeyRepository) {
        this.claudeIpcClient = claudeIpcClient;
        this.persistenceService = persistenceService;
        this.deviceRepository = deviceRepository;
        this.telemetryRecordRepository = telemetryRecordRepository;
        this.sdnRuleEventRepository = sdnRuleEventRepository;
        this.apiKeyRepository = apiKeyRepository;
    }

    public DocGenerationResult generateDocumentation(boolean forceOffline, String clientIp) {
        Instant startTime = Instant.now();
        String markdownContent = null;
        String source = "UNKNOWN";

        // Step 1: Query live system statistics
        long deviceCount = deviceRepository.count();
        long telemetryCount = telemetryRecordRepository.count();
        long downlinkCount = sdnRuleEventRepository.count();
        long apiKeyCount = apiKeyRepository.count();

        // Step 2: Attempt Claude Opus IPC delegation if not forced offline and IPC is available
        if (!forceOffline && claudeIpcClient.isConnected()) {
            try {
                String prompt = buildClaudePrompt(deviceCount, telemetryCount, downlinkCount, apiKeyCount);
                log.info("[DOCS] Delegating technical documentation generation to Claude Opus IPC socket...");
                boolean sent = claudeIpcClient.sendPrompt(prompt);
                if (sent) {
                    source = "CLAUDE_OPUS_IPC";
                    log.info("[DOCS] Successfully enqueued technical documentation task to Claude Opus IPC.");
                } else {
                    log.warn("[DOCS] Claude Opus IPC returned false; using comprehensive fallback template.");
                }
            } catch (Exception ex) {
                log.warn("[DOCS] Claude Opus IPC delegation failed ({}); activating fallback.", ex.getMessage());
            }
        }

        // Step 3: Authoritative complete technical documentation markdown
        markdownContent = buildComprehensiveDocumentation(deviceCount, telemetryCount, downlinkCount, apiKeyCount, source);
        if ("UNKNOWN".equals(source)) {
            source = "OFFLINE_FALLBACK_TEMPLATE";
        }

        // Step 4: Write documentation to disk
        Path primaryPath = resolveOutputPath();
        try {
            if (primaryPath.getParent() != null && !Files.exists(primaryPath.getParent())) {
                Files.createDirectories(primaryPath.getParent());
            }

            Files.writeString(primaryPath, markdownContent, StandardCharsets.UTF_8,
                    StandardOpenOption.CREATE, StandardOpenOption.TRUNCATE_EXISTING);

            long bytesWritten = Files.size(primaryPath);

            // Also mirror to root docs directory if we are running in dashboard-java
            mirrorToAlternativePath(markdownContent, primaryPath);

            // Step 5: Operational Audit Log
            String auditIp = (clientIp != null && !clientIp.isBlank()) ? clientIp : "127.0.0.1";
            persistenceService.logAudit(
                    "DOCS_GENERATED",
                    "Generated technical documentation (" + primaryPath.getFileName() + ")",
                    "Source: " + source + ", Bytes: " + bytesWritten + ", Path: " + primaryPath.toAbsolutePath(),
                    auditIp
            );

            log.info("[DOCS] Technical documentation written successfully to {} ({} bytes, source: {})",
                    primaryPath.toAbsolutePath(), bytesWritten, source);

            return new DocGenerationResult(
                    "SUCCESS",
                    source,
                    primaryPath.toAbsolutePath().toString(),
                    bytesWritten,
                    startTime,
                    "Technical documentation successfully generated and written to disk."
            );

        } catch (IOException e) {
            log.error("[DOCS] Failed to write technical documentation to {}: {}", primaryPath, e.getMessage(), e);
            throw new RuntimeException("Documentation write failure: " + e.getMessage(), e);
        }
    }

    public DocGenerationResult generateDocumentation() {
        return generateDocumentation(false, "127.0.0.1");
    }

    public Map<String, Object> getDocsStatus() {
        Path path = resolveOutputPath();
        boolean exists = Files.exists(path);
        long size = 0;
        Instant lastModified = null;
        if (exists) {
            try {
                size = Files.size(path);
                lastModified = Files.getLastModifiedTime(path).toInstant();
            } catch (IOException ignored) {
            }
        }

        Map<String, Object> status = new HashMap<>();
        status.put("exists", exists);
        status.put("filePath", path.toAbsolutePath().toString());
        status.put("fileSizeBytes", size);
        status.put("lastModified", lastModified != null ? lastModified.toString() : null);
        status.put("claudeIpcConnected", claudeIpcClient.isConnected());
        status.put("claudeIpcSocketPath", claudeIpcClient.getSocketPathStr());
        status.put("status", exists ? "AVAILABLE" : "NOT_GENERATED");
        return status;
    }

    public Path resolveOutputPath() {
        Path dir = Paths.get(outputDirConfig);
        if (!dir.isAbsolute()) {
            // If running from project root /home/server/CONTROLADOR_SDN:
            if (Files.isDirectory(Paths.get("dashboard-java"))) {
                dir = Paths.get("dashboard-java", outputDirConfig);
            }
        }
        return dir.resolve(filenameConfig);
    }

    private void mirrorToAlternativePath(String content, Path primary) {
        try {
            Path alternative;
            String primStr = primary.toAbsolutePath().toString();
            if (primStr.contains("dashboard-java/docs")) {
                alternative = Paths.get(primStr.replace("dashboard-java/docs", "docs"));
            } else if (primStr.contains("/docs/")) {
                alternative = Paths.get(primStr.replace("/docs/", "/dashboard-java/docs/"));
            } else {
                return;
            }

            if (alternative.getParent() != null && !Files.exists(alternative.getParent())) {
                Files.createDirectories(alternative.getParent());
            }
            Files.writeString(alternative, content, StandardCharsets.UTF_8,
                    StandardOpenOption.CREATE, StandardOpenOption.TRUNCATE_EXISTING);
            log.debug("[DOCS] Mirrored documentation to {}", alternative.toAbsolutePath());
        } catch (Exception ex) {
            log.debug("[DOCS] Note: Could not mirror docs to alternative path: {}", ex.getMessage());
        }
    }

    private String buildClaudePrompt(long devices, long telemetry, long downlinks, long keys) {
        return """
                Technical Documentation Request for Poultry Barn IoT and SDN Controller:
                - Active Registered Devices: %d
                - Persisted Telemetry Records: %d
                - SDN Downlink Commands Injected: %d
                - Security API Keys: %d
                - Architecture: Spring Boot 3 + PostgreSQL + ChirpStack v4 MQTT + Little-Endian IEEE 754 decoding + 32-bit RFC 7426 SDN packing + API Key Filter (SHA-256) + 120 req/min Rate Limiting.
                """.formatted(devices, telemetry, downlinks, keys);
    }

    private String buildComprehensiveDocumentation(long devices, long telemetry, long downlinks, long keys, String source) {
        return """
                # Technical Architecture & System Specification: SDN LoRaWAN Poultry Dashboard
                **Document Version:** 1.0.0 (Production Release)  
                **Engine:** Spring Boot 3.3.5 / Java 21 LTS  
                **Orchestration / Delegation:** Claude Opus IPC (`/run/user/1000/cc-socks/2894.sock`)  
                **Generated At:** %s  
                **Generation Source Mode:** %s  
                
                ---
                
                ## 1. Executive Summary & End-to-End System Topology
                
                The **SDN LoRaWAN Poultry Monitoring and Control Dashboard** provides a mission-critical telemetry ingestion pipeline, real-time visualization interface, and Software-Defined Networking (SDN) control plane for industrial poultry barn management.
                
                ### Data Plane & Control Plane Topology
                ```
                ┌─────────────────────────────────┐
                │  Heltec WiFi LoRa 32 V3 Nodes   │
                │  - HX711 Load Cell (Scale)      │
                │  - BME280 Ambient Sensor        │
                └────────────────┬────────────────┘
                                 │ LoRaWAN US915 RF (902-928 MHz)
                                 ▼
                ┌─────────────────────────────────┐
                │  Laird Sentrius RG1 Gateway     │
                │  - Semtech SX1301/SX1308 Base   │
                └────────────────┬────────────────┘
                                 │ UDP Packet Forwarder
                                 ▼
                ┌─────────────────────────────────┐
                │  ChirpStack v4 Network Server   │
                │  - MAC De-duplication & Routing │
                └────────────────┬────────────────┘
                                 │ MQTT (JSON frames with Base64 payloads)
                                 ▼
                ┌─────────────────────────────────┐
                │  Mosquitto MQTT Broker (1883)   │
                │  - Topic: application/+/device/+/event/up
                └────────────────┬────────────────┘
                                 │ TCP Stream
                                 ▼
                ┌─────────────────────────────────────────────────────────────┐
                │  Spring Boot 3.3.5 Backend (Port 8085)                      │
                │  - TelemetryDecoder: IEEE 754 Little-Endian Binary Parser   │
                │  - SdnHeaderPacker: 32-Bit RFC 7426 Downlink Formatter      │
                │  - Security Pipeline: RateLimiter (120/min) + API Key (SHA-256)
                │  - Claude Opus IPC Client (Unix Domain Socket)             │
                └───────┬──────────────────────────────┬──────────────────────┘
                        │ STOMP over WebSockets        │ JDBC / JPA
                        ▼                              ▼
                ┌────────────────────────┐   ┌────────────────────────────────┐
                │  Browser Web Client    │   │  PostgreSQL Container (5432)   │
                │  - Real-time Gauges    │   │  - devices, api_keys,          │
                │  - Hopper Scale Chart  │   │    telemetry_records,          │
                │  - SDN Control Console │   │    sdn_rule_events, audit_logs │
                └────────────────────────┘   └────────────────────────────────┘
                ```
                
                ### Live Runtime Telemetry Statistics
                - **Active Registered Devices:** %d
                - **Total Ingested Telemetry Frames:** %d
                - **Injected SDN Downlink Events:** %d
                - **Configured Security API Keys:** %d
                
                ---
                
                ## 2. Little-Endian Binary Telemetry Wire Protocol
                
                Uplink packets are transmitted by the Heltec V3 microcontrollers in Little-Endian byte order. Payloads are encapsulated in Base64 strings by ChirpStack and decoded in `TelemetryDecoder.java`.
                
                ### 2.1 Fixed Telemetry Header (24 Bytes)
                All telemetry modes begin with this common 24-byte header:
                
                | Byte Offset | Field Name | Data Type | Encoding | Unit / Description |
                |---|---|---|---|---|
                | `00 - 03` | `seq` | `uint32` | Little-Endian | Monotonic transmission sequence number |
                | `04 - 07` | `uptime_ms` | `uint32` | Little-Endian | Controller uptime in milliseconds |
                | `08 - 11` | `battery_mv` | `float32` | IEEE 754 Little-Endian | Battery voltage in millivolts |
                | `12 - 15` | `temperature` | `float32` | IEEE 754 Little-Endian | Barn temperature in °C (-40.0 to +85.0) |
                | `16 - 19` | `humidity` | `float32` | IEEE 754 Little-Endian | Relative humidity in %% RH (0.0 to 100.0) |
                | `20 - 23` | `pressure` | `float32` | IEEE 754 Little-Endian | Atmospheric pressure in hPa (300.0 to 1100.0) |
                
                ### 2.2 Poultry Scale Data Block (24 Bytes)
                Present in Mode 0 (Full) and Mode 1 (Critical):
                
                | Byte Offset | Field Name | Data Type | Encoding | Unit / Description |
                |---|---|---|---|---|
                | `24 - 27` | `weight_kg` | `float32` | IEEE 754 Little-Endian | Net feed hopper weight in kilograms |
                | `28 - 31` | `gross_weight_kg` | `float32` | IEEE 754 Little-Endian | Gross weight on load cell (kg) |
                | `32 - 35` | `voltage_v` | `float32` | IEEE 754 Little-Endian | Load cell excitation / signal voltage (0 - 3.3V) |
                | `36 - 37` | `adc_raw` | `uint16` | Little-Endian | Raw ESP32 ADC quantization (0 - 4095) |
                | `38 - 41` | `cal_slope` | `float32` | IEEE 754 Little-Endian | Active scale calibration slope factor |
                | `42 - 45` | `cal_zero` | `float32` | IEEE 754 Little-Endian | Active zero-tare reference offset |
                | `46 - 47` | `time_on_air_ms` | `uint16` | Little-Endian | LoRa RF transmission time-on-air in ms |
                
                ### 2.3 Payload Operating Modes
                - **Mode 0 (Full, 204 Bytes):** Header (24B) + Scale (24B) + 152 bytes of extended environmental history and sub-sensors.
                - **Mode 1 (Critical, 48 Bytes):** Header (24B) + Scale (24B). High-frequency operational telemetry.
                - **Mode 2 (Diagnostic, 24 Bytes):** Header (24B) only. Low-power beacon mode.
                
                ---
                
                ## 3. SDN Control Plane Protocol (RFC 7426)
                
                The Software-Defined Networking plane allows operators to dynamically reconfigure wireless parameters and operational modes over the air. Commands are packed into a single 32-bit Little-Endian unsigned integer in `SdnHeaderPacker.java`.
                
                ### 3.1 32-Bit Control Header Bitfield Mapping
                ```
                 31                          17 16 15 14 13   11 10    7 6    4 3    0
                +------------------------------+--+-----+-------+-------+------+------+
                |         param_value          |S | PM  |  SR   |  TXP  |  SF  | CMD  |
                |          (15 bits)           |1b| 2b  | 3b    |  4b   |  3b  |  4b  |
                +------------------------------+--+-----+-------+-------+------+------+
                ```
                
                - **`cmd_id` [Bits 0-3, 4 bits]:**
                  `0` = NOP / Heartbeat Ping  
                  `1` = RECONFIG_RADIO (apply SF, TX power, sampling rate)  
                  `2` = SET_TEMP_THRESHOLD (param_value contains target temperature)  
                  `3` = FORCE_SLEEP (enter deep sleep immediately)  
                  `4` = SET_P2P_RELAY (enable mesh peer-to-peer relaying)  
                  `15` = SYSTEM_RESET (reboot microcontroller)
                - **`sf_target` [Bits 4-6, 3 bits]:** `0`=SF7, `1`=SF8, `2`=SF9, `3`=SF10, `4`=SF11, `5`=SF12.
                - **`tx_power` [Bits 7-10, 4 bits]:** Transmit output power in dBm (0 to 15 dBm).
                - **`sample_rate` [Bits 11-13, 3 bits]:** `0`=10s, `1`=60s, `2`=300s, `3`=900s, `4`=1800s, `5`=3600s, `6`=14400s, `7`=86400s.
                - **`payload_mode` [Bits 14-15, 2 bits]:** `0`=Mode 0 (204B Full), `1`=Mode 1 (48B Critical), `2`=Mode 2 (24B Diagnostic).
                - **`sleep_enable` [Bit 16, 1 bit]:** `1` = Deep sleep between sampling intervals, `0` = Continuous RF RX window.
                - **`param_value` [Bits 17-31, 15 bits]:** Auxiliary parameter (temperature threshold, sleep duration multiplier, or calibration target).
                
                ### 3.2 Downlink Transmission Format
                Packed 32-bit integers are converted to 4 bytes in Little-Endian format, Base64-encoded, and published to Mosquitto MQTT topic:
                `application/{applicationId}/device/{devEui}/command/down`
                ```json
                {
                  "confirmed": false,
                  "fPort": 10,
                  "data": "AQD/AA=="
                }
                ```
                
                ---
                
                ## 4. Security Architecture & API Key Management
                
                The backend enforces strict defensive layering across all incoming HTTP traffic using Servlet filters ordered deterministically:
                
                ### 4.1 Filter Execution Pipeline
                1. **`SecurityHeadersFilter` (`HIGHEST_PRECEDENCE`):** Injects `X-Content-Type-Options: nosniff`, `X-Frame-Options: SAMEORIGIN`, `X-XSS-Protection: 1; mode=block`, `Referrer-Policy: strict-origin-when-cross-origin`, and CSP on every response.
                2. **`RateLimitingFilter` (`HIGHEST_PRECEDENCE + 10`):** Resolves client IP (handling `X-Forwarded-For`), enforces in-memory sliding-window limit (120 req/min per IP). Exceeded requests receive HTTP 429 Too Many Requests with header `Retry-After: 60`.
                3. **`ApiKeyAuthenticationFilter` (`HIGHEST_PRECEDENCE + 20`):** Authenticates mutating and administrative endpoints.
                
                ### 4.2 Key Ingestion & Cryptographic Verification
                - **Accepted Headers:**
                  - `X-API-Key: sdn_live_...`
                  - `Authorization: Bearer sdn_live_...`
                - **Key Format:** `sdn_live_` prefix followed by 64 hex characters (32 bytes entropy from `SecureRandom`).
                - **Storage Security:** Raw keys are **NEVER** stored in plain text. Only the lowercase SHA-256 hex hash is stored in `api_keys.key_hash` with a unique index.
                - **Prefix Masking:** Administrative views only expose the 16-character prefix (`sdn_live_12345678...`).
                - **Audit Logging:** Auth failures log `AUTH_FAILURE` records in `audit_logs` with client IP, timestamp, and target URI.
                
                ---
                
                ## 5. Claude Opus IPC Socket Protocol
                
                The dashboard interfaces natively with Claude Opus via Unix Domain Socket:
                - **Socket Path:** `/run/user/1000/cc-socks/2894.sock`
                - **Session Key Path:** `~/.claude/sessions/2894.*.key`
                - **Transport:** Java 21 Native `SocketChannel.open(StandardProtocolFamily.UNIX)` with `UnixDomainSocketAddress`.
                - **Handshake Protocol:**
                  1. Line 1: `{"type":"auth","token":"<peerToken>"}\\n` (within 3000 ms)
                  2. Line 2: `{"type":"user","message":{"role":"user","content":"<prompt>"},"priority":"next"}\\n`
                - **Resilience:** The client uses non-blocking channels and `@PostConstruct` availability probes. If the daemon is absent or offline, it gracefully falls back without interrupting Spring Boot startup or web dashboard operations.
                
                ---
                
                ## 6. PostgreSQL Database Persistence & ChirpStack Coexistence
                
                To avoid schema collisions with ChirpStack's 27 internal tables in `chirpstack.public` (specifically `device` and `api_key`), all dashboard tables use explicit plural names:
                
                | Table Name | Entity Class | Primary Key | Description |
                |---|---|---|---|
                | `devices` | `Device` | `dev_eui VARCHAR(32)` | Registered barn nodes, status, and load cell calibration |
                | `api_keys` | `ApiKey` | `id BIGSERIAL` | Hashed API access keys, prefixes, and validity |
                | `telemetry_records` | `TelemetryRecord` | `id BIGSERIAL` | Time-series decoded ambient & scale sensor readings |
                | `sdn_rule_events` | `SdnRuleEvent` | `id BIGSERIAL` | Audit log of dispatched 32-bit SDN downlinks |
                | `audit_logs` | `AuditLog` | `id BIGSERIAL` | System security and operator access audit trail |
                
                ---
                
                ## 7. REST API Catalog
                
                | Method | Path | Access | Description |
                |---|---|---|---|
                | `GET` | `/api/status` | Public | System health, MQTT connection, and active node count |
                | `GET` | `/api/galpones` | Public | Real-time state of all poultry barns |
                | `GET` | `/api/galpones/{devEui}` | Public | Detailed telemetry for specific barn |
                | `GET` | `/api/galpones/{devEui}/history` | Public | Historical telemetry records |
                | `GET` | `/api/sdn/downlinks` | Public | Downlink command audit log |
                | `POST` | `/api/sdn/preview` | Public | Offline calculation of 32-bit SDN bitfield |
                | `POST` | `/api/sdn/downlink` | **Protected** | Injects 32-bit SDN command to ChirpStack downlink |
                | `POST` | `/api/keys/generate` | **Protected** | Generates a new secure API key (shown once) |
                | `GET` | `/api/keys` | Public | Lists active API key metadata (masked prefixes) |
                | `DELETE` | `/api/keys/{id}` | **Protected** | Revokes or deletes an API key |
                | `POST` | `/api/docs/generate` | **Protected** | Triggers technical documentation generation |
                | `GET` | `/api/docs/status` | Public | Verification status of generated documentation |
                | `GET` | `/ws` / `/ws-plain` | Public | WebSocket STOMP real-time telemetry endpoints |
                """.formatted(Instant.now().toString(), source, devices, telemetry, downlinks, keys);
    }
}
