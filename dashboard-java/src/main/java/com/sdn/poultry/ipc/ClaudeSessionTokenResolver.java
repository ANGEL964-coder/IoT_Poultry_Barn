package com.sdn.poultry.ipc;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.HexFormat;
import java.util.List;

@Component
public class ClaudeSessionTokenResolver {

    private static final Logger log = LoggerFactory.getLogger(ClaudeSessionTokenResolver.class);
    private final ObjectMapper objectMapper;

    public ClaudeSessionTokenResolver(ObjectMapper objectMapper) {
        this.objectMapper = objectMapper;
    }

    public String resolveToken(Path sessionsDir, Path socketPath, String pid) throws IOException {
        if (sessionsDir == null || !Files.isDirectory(sessionsDir)) {
            throw new NoSuchFileException("Claude sessions directory does not exist or is not a directory: " + sessionsDir);
        }

        // 1. Exact match attempt: <pid>.<sha256(canonicalSocketPath)>.key
        if (pid != null && !pid.isBlank() && socketPath != null) {
            String canonicalSocket = socketPath.toAbsolutePath().normalize().toString();
            String hash = sha256Hex(canonicalSocket);
            Path exactFile = sessionsDir.resolve(pid + "." + hash + ".key");
            if (Files.isRegularFile(exactFile)) {
                log.debug("Found exact matching Claude session key file: {}", exactFile);
                return extractPeerToken(exactFile);
            }
        }

        // 2. Glob pattern match: <pid>.*.key (newest first)
        String pattern = (pid != null && !pid.isBlank()) ? (pid + ".*.key") : "*.key";
        List<Path> matchingKeys = new ArrayList<>();
        try (DirectoryStream<Path> stream = Files.newDirectoryStream(sessionsDir, pattern)) {
            for (Path p : stream) {
                if (Files.isRegularFile(p)) {
                    matchingKeys.add(p);
                }
            }
        }

        // 3. Fallback to any *.key if pid-specific pattern matched nothing
        if (matchingKeys.isEmpty() && !"*.key".equals(pattern)) {
            try (DirectoryStream<Path> stream = Files.newDirectoryStream(sessionsDir, "*.key")) {
                for (Path p : stream) {
                    if (Files.isRegularFile(p)) {
                        matchingKeys.add(p);
                    }
                }
            }
        }

        if (matchingKeys.isEmpty()) {
            throw new NoSuchFileException("No Claude session key files found matching " + pattern + " in " + sessionsDir);
        }

        // Sort descending by last modified time
        matchingKeys.sort((a, b) -> {
            try {
                return Files.getLastModifiedTime(b).compareTo(Files.getLastModifiedTime(a));
            } catch (IOException e) {
                return 0;
            }
        });

        Path candidate = matchingKeys.get(0);
        log.debug("Resolved Claude session key file: {}", candidate);
        return extractPeerToken(candidate);
    }

    public String extractPeerToken(Path keyFile) throws IOException {
        String content = Files.readString(keyFile, StandardCharsets.UTF_8);
        JsonNode root = objectMapper.readTree(content);
        JsonNode tokenNode = root.path("peerToken");
        if (tokenNode.isMissingNode() || tokenNode.asText().isBlank()) {
            throw new IllegalStateException("Field 'peerToken' not found or blank in key file: " + keyFile);
        }
        return tokenNode.asText();
    }

    private String sha256Hex(String input) {
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            byte[] digest = md.digest(input.getBytes(StandardCharsets.UTF_8));
            return HexFormat.of().formatHex(digest);
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException("SHA-256 algorithm unavailable", e);
        }
    }
}
