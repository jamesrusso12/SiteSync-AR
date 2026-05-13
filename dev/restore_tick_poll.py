"""Restore the v17 Tick → GetInputTouchState → Branch → PrintString poll chain
in BP_ARPlayerController. v18 deleted it; this is the v19 fix.
"""
import json
import socket
import sys

HOST, PORT = "127.0.0.1", 55557
BP = "BP_ARPlayerController"


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


def must(label, resp):
    print(f"\n--- {label} ---")
    print(json.dumps(resp, indent=2)[:1500])
    if resp.get("status") != "success":
        print(f"\nFAILED at: {label}")
        sys.exit(1)
    return resp["result"]


# 1. Add Event Tick
r = must(
    "Add Event Tick",
    call("add_blueprint_event_node", {
        "blueprint_name": BP,
        "event_name": "ReceiveTick",
        "node_position": [-400, 600],
    }),
)
tick_id = r.get("node_id") or r.get("node_guid")
print(f"tick_id={tick_id}")

# 2. Add GetInputTouchState (BlueprintPure on PlayerController)
r = must(
    "Add GetInputTouchState",
    call("add_blueprint_function_node", {
        "blueprint_name": BP,
        "target": "self",
        "function_name": "GetInputTouchState",
        "node_position": [-100, 750],
    }),
)
touch_id = r.get("node_id") or r.get("node_guid")
print(f"touch_id={touch_id}")

# 3. Add Branch
r = must(
    "Add Branch",
    call("add_blueprint_function_node", {
        "blueprint_name": BP,
        "target": "self",
        "function_name": "Branch",
        "node_position": [200, 600],
    }),
)
branch_id = r.get("node_id") or r.get("node_guid")
print(f"branch_id={branch_id}")

# 4. Add Print String "Touch ACTIVE" (log-only)
r = must(
    "Add Print String",
    call("add_blueprint_function_node", {
        "blueprint_name": BP,
        "target": "self",
        "function_name": "PrintString",
        "params": {
            "InString": "Touch ACTIVE",
            "bPrintToScreen": False,
            "bPrintToLog": True,
        },
        "node_position": [500, 550],
    }),
)
print_id = r.get("node_id") or r.get("node_guid")
print(f"print_id={print_id}")

# 5. Connect Tick.then → Branch.execute
must(
    "Connect Tick.then -> Branch.execute",
    call("connect_blueprint_nodes", {
        "blueprint_name": BP,
        "source_node_id": tick_id,
        "source_pin": "then",
        "target_node_id": branch_id,
        "target_pin": "execute",
    }),
)

# 6. Connect GetInputTouchState.bIsCurrentlyPressed → Branch.Condition
must(
    "Connect GetInputTouchState.bIsCurrentlyPressed -> Branch.Condition",
    call("connect_blueprint_nodes", {
        "blueprint_name": BP,
        "source_node_id": touch_id,
        "source_pin": "bIsCurrentlyPressed",
        "target_node_id": branch_id,
        "target_pin": "Condition",
    }),
)

# 7. Connect Branch.True → PrintString.execute
must(
    "Connect Branch.True -> PrintString.execute",
    call("connect_blueprint_nodes", {
        "blueprint_name": BP,
        "source_node_id": branch_id,
        "source_pin": "True",
        "target_node_id": print_id,
        "target_pin": "execute",
    }),
)

# 8. Compile
must(
    "Compile BP_ARPlayerController",
    call("compile_blueprint", {"blueprint_name": BP}),
)

print("\n" + "=" * 80)
print("DONE. Now do Ctrl+S (Save All) in the UE5 editor to flush to .uasset.")
print("=" * 80)
