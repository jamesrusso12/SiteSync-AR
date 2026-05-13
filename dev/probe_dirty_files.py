"""Inspect current in-editor state of BP_Foundation + SiteSync.umap actors
to figure out what diverged from HEAD's committed .uasset/.umap.
"""
import json
import socket

HOST, PORT = "127.0.0.1", 55557


def call(command_type, params=None):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10)
    sock.connect((HOST, PORT))
    payload = json.dumps({"type": command_type, "params": params or {}})
    sock.sendall(payload.encode("utf-8"))
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
    return json.loads(b"".join(chunks).decode("utf-8"))


print("=" * 80)
print("Level actors (SiteSync.umap):")
print("=" * 80)
r = call("get_actors_in_level")
for a in r.get("result", {}).get("actors", []):
    print(f"  {a.get('name'):40s}  class={a.get('class', a.get('type', '?'))}")

print()
print("=" * 80)
print("BP_Foundation EventGraph nodes:")
print("=" * 80)
for nt, en in [
    ("Event", "ReceiveBeginPlay"),
    ("Event", "ReceiveTick"),
    ("Event", "ReceiveEndPlay"),
]:
    r = call("find_blueprint_nodes", {
        "blueprint_name": "BP_Foundation",
        "node_type": nt,
        "event_name": en,
    })
    guids = r.get("result", {}).get("node_guids", [])
    print(f"  {nt}/{en:25s}: {len(guids)} found  guids={guids}")

# Also: try to find any InputAction nodes (shouldn't be any in BP_Foundation)
r = call("find_blueprint_nodes", {
    "blueprint_name": "BP_Foundation",
    "node_type": "InputAction",
    "input_action_name": "",
})
print(f"  InputAction/ANY            : {r.get('result', {}).get('node_guids', [])}")
