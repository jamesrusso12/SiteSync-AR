"""Probe BP_ARPlayerController event graph state via UnrealMCP TCP."""
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


bp = "BP_ARPlayerController"

print("=" * 80)
print(f"Probing {bp} event graph")
print("=" * 80)

for event_name in ["ReceiveTick", "ReceiveBeginPlay"]:
    resp = call("find_blueprint_nodes", {"blueprint_name": bp, "node_type": "Event", "event_name": event_name})
    print(f"\nfind_blueprint_nodes Event/{event_name}:")
    print(json.dumps(resp, indent=2)[:2000])

resp = call("find_blueprint_nodes", {"blueprint_name": bp, "node_type": "InputAction", "input_action_name": "IA_TapPlace"})
print(f"\nfind_blueprint_nodes InputAction/IA_TapPlace:")
print(json.dumps(resp, indent=2)[:2000])
