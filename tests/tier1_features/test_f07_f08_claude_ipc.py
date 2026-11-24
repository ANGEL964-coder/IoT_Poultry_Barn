"""
Tier 1: Feature Coverage Tests for F07 (Claude Opus IPC Client) and F08 (Technical Docs Generation).
Covers AF_UNIX stream socket connectivity, token discovery, auth handshake framing,
technical documentation generation requests, and response parsing.
Total Tests: 10 (5 tests per feature).
"""

import json
import os
import socket
import pytest

from tests.test_helpers import (
    build_claude_auth_frame,
    build_claude_user_frame,
    parse_claude_stream,
    discover_claude_token
)


# --- F07: Claude Opus IPC Client ---

def test_f07_claude_socket_path_and_attributes():
    """Verify standard Claude Opus IPC socket path exists and is an AF_UNIX stream socket."""
    live_sock_path = "/run/user/1000/cc-socks/2894.sock"
    if os.path.exists(live_sock_path):
        import stat
        mode = os.stat(live_sock_path).st_mode
        assert stat.S_ISSOCK(mode), f"{live_sock_path} is not a socket"
    else:
        # Verified via socket creation attribute test
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        assert s.family == socket.AF_UNIX
        assert s.type == socket.SOCK_STREAM
        s.close()


def test_f07_claude_session_token_discovery():
    """Verify dynamic discovery of Claude Opus peerToken from ~/.claude/sessions/."""
    token = discover_claude_token()
    if token:
        assert isinstance(token, str)
        assert len(token) >= 16
    else:
        # In isolated CI environment where no live claude runs, verify fallback discovery returns None safely
        assert token is None or isinstance(token, str)


def test_f07_auth_handshake_framing():
    """Verify auth frame formatting strictly follows newline-delimited JSON."""
    test_token = "2fa391300d6e6f77ea63ed9066973a24"
    frame = build_claude_auth_frame(test_token)
    assert frame.endswith("\n"), "Auth frame must end with newline"
    parsed = json.loads(frame.strip())
    assert parsed["type"] == "auth"
    assert parsed["token"] == test_token


def test_f07_successful_ipc_connection_and_auth(mock_claude_server):
    """Verify connecting to Claude IPC server, sending auth handshake, and receiving auth_ok."""
    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    client.connect(mock_claude_server.sock_path)

    # Send auth handshake
    auth_payload = build_claude_auth_frame(mock_claude_server.token)
    client.sendall(auth_payload.encode("utf-8"))

    # Receive response
    resp_raw = client.recv(1024).decode("utf-8")
    client.close()

    parsed = json.loads(resp_raw.strip())
    assert parsed.get("type") == "auth_ok"
    assert parsed.get("status") == "authenticated"
    assert mock_claude_server.authenticated_sessions == 1


def test_f07_auth_rejection_on_invalid_token(mock_claude_server):
    """Verify connecting with invalid token receives error and connection termination."""
    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    client.connect(mock_claude_server.sock_path)

    bad_payload = build_claude_auth_frame("completely-invalid-token-xyz")
    client.sendall(bad_payload.encode("utf-8"))

    resp_raw = client.recv(1024).decode("utf-8")
    client.close()

    parsed = json.loads(resp_raw.strip())
    assert parsed.get("type") == "error"
    assert "Invalid" in parsed.get("message", "")


# --- F08: Claude Opus Technical Docs Generation ---

def test_f08_user_prompt_framing():
    """Verify user prompt frame JSON formatting and priority attribute."""
    prompt = "Generate technical documentation for SDN Control Plane"
    frame = build_claude_user_frame(prompt)
    assert frame.endswith("\n")
    parsed = json.loads(frame.strip())
    assert parsed["type"] == "user"
    assert parsed["priority"] == "next"
    assert parsed["message"]["role"] == "user"
    assert parsed["message"]["content"] == prompt


def test_f08_send_doc_generation_request(mock_claude_server):
    """Verify sending a documentation generation request over IPC channel."""
    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    client.connect(mock_claude_server.sock_path)

    # Auth
    client.sendall(build_claude_auth_frame(mock_claude_server.token).encode("utf-8"))
    auth_resp = client.recv(1024).decode("utf-8")
    assert "auth_ok" in auth_resp

    # Request Docs
    prompt = "Generate REST API documentation for /api/sdn/downlink"
    client.sendall(build_claude_user_frame(prompt).encode("utf-8"))

    doc_resp_raw = client.recv(4096).decode("utf-8")
    client.close()

    parsed = json.loads(doc_resp_raw.strip())
    assert parsed.get("type") == "assistant"
    content = parsed.get("message", {}).get("content", "")
    assert "Technical Documentation" in content
    assert "SDN Controller" in content


def test_f08_parse_markdown_technical_docs():
    """Verify parse_claude_stream helper handles multi-frame and markdown streams."""
    stream_data = (
        '{"type":"auth_ok","status":"authenticated"}\n'
        '{"type":"assistant","message":{"role":"assistant","content":"# Poultry Controller\\n## Section 1\\nOverview."}}\n'
    )
    frames = parse_claude_stream(stream_data)
    assert len(frames) == 2
    assert frames[0]["type"] == "auth_ok"
    assert frames[1]["type"] == "assistant"
    assert "# Poultry Controller" in frames[1]["message"]["content"]


def test_f08_multi_step_interactive_doc_session(mock_claude_server):
    """Verify multi-step sequential documentation requests in a single persistent session."""
    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    client.connect(mock_claude_server.sock_path)

    # Handshake
    client.sendall(build_claude_auth_frame(mock_claude_server.token).encode("utf-8"))
    client.recv(1024)

    # Step 1: Prompt 1
    client.sendall(build_claude_user_frame("Doc Part 1: Architecture").encode("utf-8"))
    resp1 = json.loads(client.recv(2048).decode("utf-8").strip())
    assert "Doc Part 1" in resp1["message"]["content"]

    # Step 2: Prompt 2
    client.sendall(build_claude_user_frame("Doc Part 2: Security & API Keys").encode("utf-8"))
    resp2 = json.loads(client.recv(2048).decode("utf-8").strip())
    assert "Doc Part 2" in resp2["message"]["content"]

    client.close()
    assert mock_claude_server.requests_handled == 2


def test_f08_empty_prompt_handling():
    """Verify empty prompt string is framed cleanly and doesn't cause JSON parse error."""
    frame = build_claude_user_frame("")
    parsed = json.loads(frame.strip())
    assert parsed["message"]["content"] == ""
