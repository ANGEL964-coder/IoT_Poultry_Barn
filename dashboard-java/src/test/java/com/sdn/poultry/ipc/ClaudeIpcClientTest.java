package com.sdn.poultry.ipc;

import com.fasterxml.jackson.databind.ObjectMapper;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.StandardProtocolFamily;
import java.net.UnixDomainSocketAddress;
import java.nio.channels.Channels;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.concurrent.*;

import static org.assertj.core.api.Assertions.assertThat;

class ClaudeIpcClientTest {

    @TempDir
    Path tempDir;

    private final ObjectMapper objectMapper = new ObjectMapper();
    private ClaudeSessionTokenResolver tokenResolver;
    private ClaudeIpcClient client;

    private ServerSocketChannel mockServerChannel;
    private Path mockSocketPath;
    private Path sessionsDir;
    private ExecutorService serverExecutor;

    @BeforeEach
    void setUp() throws Exception {
        tokenResolver = new ClaudeSessionTokenResolver(objectMapper);
        client = new ClaudeIpcClient(tokenResolver, objectMapper);

        sessionsDir = tempDir.resolve("sessions");
        Files.createDirectories(sessionsDir);

        mockSocketPath = tempDir.resolve("mock-claude.sock");
        serverExecutor = Executors.newSingleThreadExecutor();
    }

    @AfterEach
    void tearDown() throws Exception {
        if (serverExecutor != null) {
            serverExecutor.shutdownNow();
        }
        if (mockServerChannel != null && mockServerChannel.isOpen()) {
            mockServerChannel.close();
        }
        if (Files.exists(mockSocketPath)) {
            Files.deleteIfExists(mockSocketPath);
        }
    }

    @Test
    @DisplayName("Should successfully connect, authenticate, and enqueue prompt over native Java 21 Unix domain socket")
    void testSuccessfulHandshakeAndPromptTransmission() throws Exception {
        // 1. Start mock Java 21 Unix domain socket server
        UnixDomainSocketAddress serverAddress = UnixDomainSocketAddress.of(mockSocketPath);
        mockServerChannel = ServerSocketChannel.open(StandardProtocolFamily.UNIX);
        mockServerChannel.bind(serverAddress);

        // 2. Provision mock session token key file
        String peerToken = "secret_peer_token_abc123";
        Path keyFile = sessionsDir.resolve("2894.testsession.key");
        Files.writeString(keyFile, "{\"peerToken\":\"" + peerToken + "\"}", StandardCharsets.UTF_8);

        // 3. Configure client
        client.setSocketPathStr(mockSocketPath.toString());
        client.setSessionsDirStr(sessionsDir.toString());
        client.setPid("2894");

        // 4. Run mock server in background thread accepting connection and asserting wire protocol
        CompletableFuture<String[]> serverReceived = new CompletableFuture<>();
        serverExecutor.submit(() -> {
            try {
                SocketChannel incoming = mockServerChannel.accept();
                BufferedReader reader = new BufferedReader(Channels.newReader(incoming, StandardCharsets.UTF_8));
                String line1 = reader.readLine(); // Line 1: Auth frame
                String line2 = reader.readLine(); // Line 2: User prompt frame
                serverReceived.complete(new String[]{line1, line2});
                incoming.close();
            } catch (Exception e) {
                serverReceived.completeExceptionally(e);
            }
        });

        // 5. Client sends prompt
        String testPrompt = "Generate architecture documentation for SDN poultry dashboard";
        boolean sent = client.sendPrompt(testPrompt);

        assertThat(sent).isTrue();
        assertThat(client.isConnected()).isTrue();

        String[] frames = serverReceived.get(3, TimeUnit.SECONDS);
        assertThat(frames[0]).contains("\"type\":\"auth\"");
        assertThat(frames[0]).contains(peerToken);

        assertThat(frames[1]).contains("\"type\":\"user\"");
        assertThat(frames[1]).contains(testPrompt);
        assertThat(frames[1]).contains("\"priority\":\"next\"");
    }

    @Test
    @DisplayName("Should handle missing or offline socket gracefully without throwing exceptions or crashing")
    void testResilientOfflineFallback() {
        Path nonexistentSocket = tempDir.resolve("missing.sock");
        client.setSocketPathStr(nonexistentSocket.toString());
        client.setSessionsDirStr(sessionsDir.toString());

        boolean available = client.checkAvailability();
        assertThat(available).isFalse();
        assertThat(client.isConnected()).isFalse();

        ClaudeIpcResult result = client.sendPromptWithResult("Any prompt");
        assertThat(result.success()).isFalse();
        assertThat(result.offline()).isTrue();
        assertThat(result.message()).contains("does not exist");
    }

    @Test
    @DisplayName("ClaudeSessionTokenResolver should pick newest session key matching glob")
    void testTokenResolverPicksNewestKey() throws Exception {
        Path olderKey = sessionsDir.resolve("2894.old.key");
        Files.writeString(olderKey, "{\"peerToken\":\"old_token\"}");

        Thread.sleep(20); // ensure timestamp difference

        Path newerKey = sessionsDir.resolve("2894.new.key");
        Files.writeString(newerKey, "{\"peerToken\":\"newest_token\"}");

        String resolved = tokenResolver.resolveToken(sessionsDir, mockSocketPath, "2894");
        assertThat(resolved).isEqualTo("newest_token");
    }
}
