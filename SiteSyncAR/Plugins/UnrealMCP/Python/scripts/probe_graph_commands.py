"""Direct TCP probe of the graph-editing commands (added 2026-05-29).

One-off verification harness, not part of the MCP server runtime. Run with the
UE editor open after rebuilding the plugin. Confirms:

  1. list_commands now advertises the three graph commands (registry wired).
  2. Each graph command's handler is REACHED (returns a domain error like
     "Blueprint not found", NOT "Unknown command: ..." which would mean the
     dispatcher entry / DLL is stale).

It deliberately targets a nonexistent blueprint so it mutates nothing. To do a
live build test, point BP_NAME at a real, disposable Blueprint and uncomment the
live section at the bottom.
"""
import json
import socket

HOST, PORT = "127.0.0.1", 55557
BP_NAME = "__nonexistent_probe_bp__"


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
    return json.loads(b"".join(chunks).decode("utf-8"))


def show(label, resp):
    status = resp.get("status", "?")
    body = resp.get("error", "") if status == "error" else f"keys={list(resp.get('result', resp).keys())}"
    unknown = "Unknown command" in str(body)
    flag = "  <-- STALE DLL / UNWIRED DISPATCHER" if unknown else ""
    print(f"[{status:>7}] {label:<48} {body}{flag}")


print("=" * 80)
print("Graph-command probe (port 55557)")
print("=" * 80)

# 1. Registry introspection.
resp = call("list_commands")
result = resp.get("result", resp)
print(f"[ list ] system_commands: {result.get('system_commands')}")
print(f"[ list ] graph_commands:  {result.get('graph_commands')}")
expected = {"build_blueprint_graph", "describe_blueprint_node", "split_struct_pin"}
got = set(result.get("graph_commands", []))
print(f"[ check] graph registry complete: {expected.issubset(got)}")

# 2. Handler-reached checks (domain error expected, NOT 'Unknown command').
show("build_blueprint_graph (bogus bp)",
     call("build_blueprint_graph", {"blueprint_name": BP_NAME, "nodes": [{"ref": "a", "type": "self"}]}))
show("describe_blueprint_node (bogus bp)",
     call("describe_blueprint_node", {"blueprint_name": BP_NAME, "node_id": "all"}))
show("split_struct_pin (bogus bp)",
     call("split_struct_pin", {"blueprint_name": BP_NAME, "node_id": "x", "pin": "y"}))

# --- LIVE BUILD TEST (uncomment + set BP_NAME to a real disposable Blueprint) ---
# r = call("build_blueprint_graph", {
#     "blueprint_name": "BP_GraphProbe",
#     "nodes": [
#         {"ref": "begin", "type": "event", "event_name": "ReceiveBeginPlay", "position": [0, 0]},
#         {"ref": "print", "type": "call_function", "function_name": "PrintString",
#          "target": "KismetSystemLibrary", "position": [350, 0],
#          "pin_defaults": {"InString": "graph builder works"}},
#     ],
#     "connections": [
#         {"from": "begin", "from_pin": "then", "to": "print", "to_pin": "execute"},
#     ],
# })
# print(json.dumps(r, indent=2))
# call("save_package", {"package_path": "/Game/Blueprints/BP_GraphProbe"})
