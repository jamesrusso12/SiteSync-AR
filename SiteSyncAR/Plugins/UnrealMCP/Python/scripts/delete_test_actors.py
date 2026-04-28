"""Delete the leftover MCP test StaticMeshActors from the level.

Targets actors with class=StaticMeshActor whose name starts with
MCP_ — leaves WorldSettings, BP_*, PlayerStart, etc. alone.
"""
import json
import socket

HOST, PORT = "127.0.0.1", 55557


def call(command_type, params=None):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    sock.connect((HOST, PORT))
    sock.sendall(json.dumps({"type": command_type, "params": params or {}}).encode("utf-8"))
    chunks = []
    while True:
        chunk = sock.recv(65536)
        if not chunk:
            break
        chunks.append(chunk)
        try:
            return json.loads(b"".join(chunks).decode("utf-8"))
        except json.JSONDecodeError:
            continue


print("--- Before ---")
before = call("get_actors_in_level")
actors = before.get("result", before).get("actors", [])
for a in actors:
    print(f"  {a.get('class','?'):<22} {a.get('name','?')}")

targets = [
    a for a in actors
    if a.get("class") == "StaticMeshActor" and a.get("name", "").startswith("MCP_")
]
print(f"\n--- Deleting {len(targets)} MCP_* StaticMeshActor(s) ---")
for a in targets:
    name = a["name"]
    resp = call("delete_actor", {"name": name})
    status = resp.get("status", "?")
    detail = resp.get("error", "ok")
    print(f"  [{status:>7}] {name} — {detail}")

print("\n--- After ---")
after = call("get_actors_in_level")
remaining = after.get("result", after).get("actors", [])
for a in remaining:
    print(f"  {a.get('class','?'):<22} {a.get('name','?')}")
