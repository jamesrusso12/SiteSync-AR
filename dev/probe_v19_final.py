"""Verify v19 BP state via MCP — Event/InputAction node presence only.
(MCP can't read PrintString params or K2Node error flags.)
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


for bp in ["BP_ARPlayerController", "BP_Foundation"]:
    print(f"\n=== {bp} ===")
    for ev in ["ReceiveBeginPlay", "ReceiveTick", "ReceiveEndPlay"]:
        r = call("find_blueprint_nodes", {"blueprint_name": bp, "node_type": "Event", "event_name": ev})
        guids = r.get("result", {}).get("node_guids", [])
        print(f"  {ev:20s}: {len(guids)} found  {guids}")
    r = call("find_blueprint_nodes", {"blueprint_name": bp, "node_type": "InputAction", "input_action_name": ""})
    guids = r.get("result", {}).get("node_guids", [])
    print(f"  InputAction(any)    : {len(guids)} found  {guids}")
