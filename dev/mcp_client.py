"""Minimal direct TCP client for the UnrealMCP C++ bridge on 127.0.0.1:55557.

Bypasses the Python MCP server and stdio plumbing — useful when the Claude
Code session can't pick up .mcp.json (worktree scope, etc.) but the editor's
TCP bridge is still up.

Wire protocol: connect, send {"type": "<command>", "params": {...}} as raw
UTF-8 (no newline), read until JSON parses, server closes the connection
after each command. Reference: SiteSyncAR/Plugins/UnrealMCP/Python/unreal_mcp_server.py
"""

from __future__ import annotations
import json
import socket
import sys
from typing import Any

HOST = "127.0.0.1"
PORT = 55557
TIMEOUT_S = 30.0


def call(command: str, params: dict[str, Any] | None = None) -> dict[str, Any]:
    payload = json.dumps({"type": command, "params": params or {}}).encode("utf-8")
    with socket.create_connection((HOST, PORT), timeout=TIMEOUT_S) as s:
        s.sendall(payload)
        chunks: list[bytes] = []
        while True:
            buf = s.recv(65536)
            if not buf:
                break
            chunks.append(buf)
            try:
                return json.loads(b"".join(chunks).decode("utf-8"))
            except json.JSONDecodeError:
                continue
    raw = b"".join(chunks).decode("utf-8", errors="replace")
    raise RuntimeError(f"connection closed before JSON parsed: {raw!r}")


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print("usage: mcp_client.py <command> [json_params]", file=sys.stderr)
        return 2
    command = argv[1]
    params = json.loads(argv[2]) if len(argv) > 2 else {}
    result = call(command, params)
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
