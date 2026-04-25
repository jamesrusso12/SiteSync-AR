"""One-shot smoke test for UnrealMCP TCP server (port 55557).

Spawns MCP_ConnectionTest cube at (0, 0, 100), verifies it, then deletes it.
Used to confirm Claude Code -> UnrealMCP path is healthy after wiring .mcp.json.
"""
import json
import socket
import sys


def send(cmd: str, params: dict) -> dict | None:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5.0)
    try:
        s.connect(("127.0.0.1", 55557))
        s.sendall(json.dumps({"type": cmd, "params": params}).encode("utf-8"))
        chunks: list[bytes] = []
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            chunks.append(chunk)
            try:
                return json.loads(b"".join(chunks).decode("utf-8"))
            except json.JSONDecodeError:
                continue
        return None
    finally:
        s.close()


def main() -> int:
    import time
    name = f"MCP_SmokeTest_{int(time.time())}"

    print(f"[1/3] spawn {name} at (0, 0, 100) ...")
    r = send("create_actor", {
        "name": name,
        "type": "StaticMeshActor",
        "location": [0.0, 0.0, 100.0],
        "rotation": [0.0, 0.0, 0.0],
        "scale": [1.0, 1.0, 1.0],
    })
    print(f"    response: {r}")
    if not r or r.get("status") != "success":
        return 1

    print(f"[2/3] read back properties of {name} ...")
    r = send("get_actor_properties", {"name": name})
    print(f"    response: {r}")
    if not r or r.get("status") != "success":
        return 2

    print(f"[3/3] delete {name} (cleanup) ...")
    r = send("delete_actor", {"name": name})
    print(f"    response: {r}")
    if not r or r.get("status") != "success":
        return 3

    print("OK — UnrealMCP TCP path verified end-to-end.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
