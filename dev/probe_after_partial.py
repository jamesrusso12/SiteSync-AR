"""Probe state after partial restore attempt."""
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

r = call("find_blueprint_nodes", {"blueprint_name": bp, "node_type": "Event", "event_name": "ReceiveTick"})
print("ReceiveTick:", json.dumps(r, indent=2))

# Try a few function_name variants for Branch
for fn in ["Branch", "K2_Branch", "IfThenElse", "K2Node_IfThenElse"]:
    r = call("add_blueprint_function_node", {
        "blueprint_name": bp,
        "target": "self",
        "function_name": fn,
        "node_position": [9999, 9999],
    })
    print(f"\nadd_blueprint_function_node fn='{fn}':", json.dumps(r, indent=2)[:300])
