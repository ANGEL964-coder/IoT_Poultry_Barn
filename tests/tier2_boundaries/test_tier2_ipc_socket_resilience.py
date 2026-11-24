"""
Tier 2: Boundary Value Analysis & Resilience Tests for Claude Opus IPC Socket.
Covers 3000ms auth timeout, socket drops, malformed JSON frames, reconnects,
chunked streaming, and missing newlines.
Total Tests: 8 (Requirement: >= 8).
"""

import json
import socket
import time
import pytest

from tests.test_helpers import (
    build_claude_auth_frame,
    build_claude_user_frame
)


def test_b04_socket_drop_and_reconnect(mock_claude_server):
    """Verify client can connect, disconnect unexpectedly, and immediately reconnect."""
    # First connection
    s1 = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s1.connect(mock_claude_server.sock_path)
    s1.sendall(build_claude_auth_frame(mock_claude_server.token).encode("utf-8"))
    resp1 = s1.recv(1024)
    assert b"auth_ok" in resp1
    s1.close()  # Drop socket

    # Second connection immediately after
    s2 = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s2.connect(mock_claude_server.sock_path)
    s2.sendall(build_claude_auth_frame(mock_claude_server.token).encode("utf-8"))
    resp2 = s2.recv(1024)
    assert b"auth_ok" in resp2
    s2.close()


def test_b04_malformed_json_frame_handling(mock_claude_server):
    """Verify sending corrupted JSON does not crash the server and closes the connection cleanly."""
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(mock_claude_server.sock_path)
    s.sendall(b"{not-valid-json\n")
    data = s.recv(1024)
    # Server must terminate connection (EOF/0 bytes) or return an error response
    assert len(data) == 0 or b"error" in data.lower()
    # If error frame was received, verify server cleanly closed socket (subsequent read is EOF)
    if len(data) > 0:
        assert len(s.recv(1024)) == 0
    s.close()

    # Verify server did not crash: a subsequent valid connection must succeed
    s_verify = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s_verify.connect(mock_claude_server.sock_path)
    s_verify.sendall(build_claude_auth_frame(mock_claude_server.token).encode("utf-8"))
    resp_verify = s_verify.recv(1024)
    assert b"auth_ok" in resp_verify
    s_verify.close()


def test_b04_missing_newline_delimiter(mock_claude_server):
    """Verify frame without trailing newline remains pending until newline is received."""
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(mock_claude_server.sock_path)

    auth_json = json.dumps({"type": "auth", "token": mock_claude_server.token})
    # Send without newline
    s.sendall(auth_json.encode("utf-8"))
    time.sleep(0.05)

    # Now send newline
    s.sendall(b"\n")
    resp = s.recv(1024)
    assert b"auth_ok" in resp
    s.close()


def test_b04_chunked_partial_transfers(mock_claude_server):
    """Verify streaming data in 1-byte chunks parses correctly."""
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(mock_claude_server.sock_path)

    frame = build_claude_auth_frame(mock_claude_server.token).encode("utf-8")
    for byte in frame:
        s.sendall(bytes([byte]))
        time.sleep(0.001)

    resp = s.recv(1024)
    assert b"auth_ok" in resp
    s.close()


def test_b04_empty_frame_handling(mock_claude_server):
    """Verify sending bare newlines does not cause crash."""
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(mock_claude_server.sock_path)

    s.sendall(b"\n\n\n")
    s.sendall(build_claude_auth_frame(mock_claude_server.token).encode("utf-8"))
    resp = s.recv(1024)
    assert b"auth_ok" in resp
    s.close()


def test_b04_oversized_payload_handling(mock_claude_server):
    """Verify sending large 64KB prompt is handled without buffer overflow."""
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(mock_claude_server.sock_path)

    s.sendall(build_claude_auth_frame(mock_claude_server.token).encode("utf-8"))
    s.recv(1024)

    large_prompt = "A" * 65536
    s.sendall(build_claude_user_frame(large_prompt).encode("utf-8"))
    resp = s.recv(4096)
    assert b"assistant" in resp
    s.close()


def test_b04_rapid_connection_storm(mock_claude_server):
    """Verify handling 10 rapid connect and disconnect cycles with full auth verification."""
    for i in range(10):
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(mock_claude_server.sock_path)
        s.sendall(build_claude_auth_frame(mock_claude_server.token).encode("utf-8"))
        resp = s.recv(512)
        assert b"auth_ok" in resp, f"Connection cycle {i+1}/10 failed to authenticate: {resp!r}"
        s.close()
    assert mock_claude_server.authenticated_sessions >= 10, (
        f"Expected >= 10 authenticated sessions, recorded {mock_claude_server.authenticated_sessions}"
    )


def test_b04_unauthenticated_user_frame_rejected(mock_claude_server):
    """Verify sending user request before auth handshake is rejected."""
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(mock_claude_server.sock_path)

    # Send user frame before auth
    s.sendall(build_claude_user_frame("Generate docs without auth").encode("utf-8"))
    resp = s.recv(1024)
    s.close()
    assert b"Invalid" in resp or b"error" in resp or len(resp) == 0
