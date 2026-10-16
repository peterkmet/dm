# SPDX-License-Identifier: GPL-3.0-or-later
"""Socket helpers for the turn-based control socket, without the mcp package (fast to import)."""

import json
import os
import socket

DEFAULT_SOCKET = "~/.dm/control.sock"
TIMEOUT = 120.0


def socket_path() -> str:
    return os.path.expanduser(os.environ.get("DM_CONTROL", DEFAULT_SOCKET))


def request(payload: dict) -> dict:
    """Sends one request line to the game and returns the response object."""
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
        s.settimeout(TIMEOUT)
        s.connect(socket_path())
        s.sendall(json.dumps(payload).encode() + b"\n")
        data = b""
        while not data.endswith(b"\n"):
            chunk = s.recv(65536)
            if not chunk:
                break
            data += chunk
    return json.loads(data)
