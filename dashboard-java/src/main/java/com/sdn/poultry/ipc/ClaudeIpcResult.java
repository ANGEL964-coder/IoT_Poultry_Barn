package com.sdn.poultry.ipc;

public record ClaudeIpcResult(boolean success, String message, boolean offline) {

    public static ClaudeIpcResult success(String message) {
        return new ClaudeIpcResult(true, message, false);
    }

    public static ClaudeIpcResult failure(String message) {
        return new ClaudeIpcResult(false, message, false);
    }

    public static ClaudeIpcResult offline(String message) {
        return new ClaudeIpcResult(false, message, true);
    }
}
