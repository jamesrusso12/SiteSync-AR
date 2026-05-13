"""Roll back the partial MCP additions before handing off to manual editor work."""
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

# Delete GetInputTouchState (we know its GUID)
r = call("delete_blueprint_node", {
    "blueprint_name": bp,
    "node_id": "2BE1E70442208DF45AA7ACAD25B0FB94",
})
print("Delete GetInputTouchState:", json.dumps(r, indent=2))

# Re-probe Tick — the MCP plugin's find returns all-zeros, so try with real GUID
# discovered via a fresh find. If still all zeros, the new Tick is unaddressable
# via API and James must delete it manually.
r = call("find_blueprint_nodes", {"blueprint_name": bp, "node_type": "Event", "event_name": "ReceiveTick"})
print("\nReceiveTick search:", json.dumps(r, indent=2))

guids = r.get("result", {}).get("node_guids", [])
for guid in guids:
    if guid and guid != "00000000000000000000000000000000":
        dr = call("delete_blueprint_node", {"blueprint_name": bp, "node_id": guid})
        print(f"\nDelete Tick {guid}:", json.dumps(dr, indent=2))
    else:
        print(f"\nSkipping zero/empty GUID (unaddressable via API): {guid!r}")

# Final state
r = call("find_blueprint_nodes", {"blueprint_name": bp, "node_type": "Event", "event_name": "ReceiveTick"})
print("\nFinal ReceiveTick state:", json.dumps(r, indent=2))
