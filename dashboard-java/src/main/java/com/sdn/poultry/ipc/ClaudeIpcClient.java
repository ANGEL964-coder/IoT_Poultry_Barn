package com.sdn.poultry.ipc;

import com.fasterxml.jackson.databind.ObjectMapper;
import jakarta.annotation.PostConstruct;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import java.io.IOException;
import java.net.SocketTimeoutException;
import java.net.StandardProtocolFamily;
import java.net.UnixDomainSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.SocketChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Map;

@Service
public class ClaudeIpcClient {

    private static final Logger log = LoggerFactory.getLogger(ClaudeIpcClient.class);

    private final ClaudeSessionTokenResolver tokenResolver;
    private final ObjectMapper objectMapper;

    @Value("${app.claude.ipc.enabled:true}")
    private boolean enabled = true;

    @Value("${app.claude.ipc.socket-path:/run/user/1000/cc-socks/2894.sock}")
    private String socketPathStr = "/run/user/1000/cc-socks/2894.sock";

    @Value("${app.claude.ipc.sessions-dir:/home/server/.claude/sessions}")
    private String sessionsDirStr = "/home/server/.claude/sessions";

    @Value("${app.claude.ipc.pid:2894}")
    private String pid = "2894";

    @Value("${app.claude.ipc.connect-timeout-ms:2000}")
    private int connectTimeoutMs = 2000;

    private volatile boolean connected = false;
    private volatile String lastError = null;

    public ClaudeIpcClient(ClaudeSessionTokenResolver tokenResolver, ObjectMapper objectMapper) {
        this.tokenResolver = tokenResolver;
        this.objectMapper = objectMapper;
    }

    @PostConstruct
    public void init() {
        if (!enabled) {
            log.info("[CLAUDE-IPC] Claude Opus IPC Client is disabled by configuration.");
            connected = false;
            return;
        }
        try {
            checkAvailability();
        } catch (Exception e) {
            connected = false;
            lastError = e.getMessage();
            log.warn("[CLAUDE-IPC] Initial socket check failed: {} (Offline resilient mode active)", lastError);
        }
    }

    public synchronized boolean checkAvailability() {
        if (!enabled) {
            connected = false;
            return false;
        }

        Path socketPath = Paths.get(socketPathStr);
        Path sessionsDir = Paths.get(sessionsDirStr);

        if (!Files.exists(socketPath)) {
            connected = false;
            lastError = "Socket file not present at " + socketPathStr;
            log.warn("[CLAUDE-IPC] Claude Opus IPC socket unavailable: {} (Offline resilient mode)", lastError);
            return false;
        }

        try {
            String token = tokenResolver.resolveToken(sessionsDir, socketPath, pid);
            if (token != null && !token.isBlank()) {
                connected = true;
                lastError = null;
                log.info("[CLAUDE-IPC] Claude Opus IPC socket and session token verified: socket={}, pid={}", socketPathStr, pid);
                return true;
            } else {
                connected = false;
                lastError = "Peer token is blank";
                return false;
            }
        } catch (Exception e) {
            connected = false;
            lastError = e.getMessage();
            log.warn("[CLAUDE-IPC] Session token resolution failed: {} (Offline resilient mode)", lastError);
            return false;
        }
    }

    public boolean isConnected() {
        return connected && Files.exists(Paths.get(socketPathStr));
    }

    public boolean isAvailable() {
        return isConnected();
    }

    public boolean sendPrompt(String prompt) {
        ClaudeIpcResult result = sendPromptWithResult(prompt);
        return result.success();
    }

    public ClaudeIpcResult sendPromptWithResult(String prompt) {
        if (prompt == null || prompt.isBlank()) {
            return ClaudeIpcResult.failure("Prompt must not be empty or blank");
        }

        Path socketPath = Paths.get(socketPathStr);
        if (!Files.exists(socketPath)) {
            connected = false;
            lastError = "Socket file does not exist: " + socketPathStr;
            return ClaudeIpcResult.offline(lastError);
        }

        Path sessionsDir = Paths.get(sessionsDirStr);
        String token;
        try {
            token = tokenResolver.resolveToken(sessionsDir, socketPath, pid);
        } catch (Exception e) {
            connected = false;
            lastError = "Token resolution failed: " + e.getMessage();
            return ClaudeIpcResult.failure(lastError);
        }

        UnixDomainSocketAddress address = UnixDomainSocketAddress.of(socketPath);

        try (Selector selector = Selector.open();
             SocketChannel channel = SocketChannel.open(StandardProtocolFamily.UNIX)) {

            channel.configureBlocking(false);
            boolean connectedImmediately = channel.connect(address);
            if (!connectedImmediately) {
                channel.register(selector, SelectionKey.OP_CONNECT);
                if (selector.select(connectTimeoutMs) > 0 && channel.finishConnect()) {
                    log.debug("[CLAUDE-IPC] Connected to Claude IPC socket via Selector");
                } else {
                    connected = false;
                    lastError = "Connection timed out after " + connectTimeoutMs + " ms";
                    return ClaudeIpcResult.failure(lastError);
                }
            }

            channel.configureBlocking(true);

            // Frame 1: Auth handshake (must complete within 3000 ms)
            Map<String, Object> authFrame = Map.of("type", "auth", "token", token);
            String authJson = objectMapper.writeValueAsString(authFrame) + "\n";
            writeAll(channel, authJson);

            // Frame 2: User prompt frame
            Map<String, Object> messageContent = Map.of("role", "user", "content", prompt);
            Map<String, Object> userFrame = Map.of(
                    "type", "user",
                    "message", messageContent,
                    "priority", "next"
            );
            String userJson = objectMapper.writeValueAsString(userFrame) + "\n";
            writeAll(channel, userJson);

            this.connected = true;
            this.lastError = null;
            log.info("[CLAUDE-IPC] Successfully enqueued prompt to Claude Opus queue ({} chars)", prompt.length());
            return ClaudeIpcResult.success("Prompt successfully transmitted to Claude Opus queue");

        } catch (SocketTimeoutException te) {
            this.connected = false;
            this.lastError = "Socket timeout: " + te.getMessage();
            log.warn("[CLAUDE-IPC] Timeout communicating with socket: {}", te.getMessage());
            return ClaudeIpcResult.failure(lastError);
        } catch (IOException ioe) {
            this.connected = false;
            this.lastError = "I/O error: " + ioe.getMessage();
            log.warn("[CLAUDE-IPC] Error writing to socket: {}", ioe.getMessage());
            return ClaudeIpcResult.failure(lastError);
        } catch (Exception e) {
            this.connected = false;
            this.lastError = "Unexpected error: " + e.getMessage();
            log.error("[CLAUDE-IPC] Unexpected error communicating with Claude socket", e);
            return ClaudeIpcResult.failure(lastError);
        }
    }

    private void writeAll(SocketChannel channel, String text) throws IOException {
        ByteBuffer buf = ByteBuffer.wrap(text.getBytes(StandardCharsets.UTF_8));
        while (buf.hasRemaining()) {
            channel.write(buf);
        }
    }

    public String getLastError() {
        return lastError;
    }

    public String getSocketPathStr() {
        return socketPathStr;
    }

    public void setSocketPathStr(String socketPathStr) {
        this.socketPathStr = socketPathStr;
    }

    public void setSessionsDirStr(String sessionsDirStr) {
        this.sessionsDirStr = sessionsDirStr;
    }

    public void setPid(String pid) {
        this.pid = pid;
    }
}
