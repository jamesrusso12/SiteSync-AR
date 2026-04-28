"""Direct TCP probe of the rebuilt UnrealMCP plugin.

One-off verification harness, not part of the MCP server runtime.
Spawns -> reads -> finds -> deletes a cube to exercise symptom (C),
and pings the new fec1af2 commands to exercise symptom (B).

Run with the UE editor open and a level loaded.
"""
import json
import socket

HOST, PORT = "127.0.0.1", 55557


def call(command_type, params=None):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
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


def show(label, resp):
    status = resp.get("status", "?")
    if status == "error":
        body = resp.get("error", "")
    else:
        result = resp.get("result", resp)
        if "actors" in result:
            actors = result["actors"]
            body = f"{len(actors)} actor(s): " + ", ".join(
                a.get("name", "<noname>") for a in actors[:8]
            )
            if len(actors) > 8:
                body += f", ... (+{len(actors) - 8} more)"
        else:
            keys = list(result.keys())
            body = f"keys={keys}"
    print(f"[{status:>7}] {label:<55} {body}")


print("=" * 80)
print("Direct TCP probe of UnrealMCP plugin (port 55557)")
print("=" * 80)

# Symptom (B): does the new command exist?
show(
    "delete_blueprint_node with bogus inputs (probe handler exists)",
    call("delete_blueprint_node", {"blueprint_name": "__nonexistent__", "node_id": "00000000000000000000000000000000"}),
)

# Symptom (C): can we round-trip an actor?
show(
    "spawn_actor StaticMeshActor MCP_FixCheck_01 at (0,0,300)",
    call("spawn_actor", {"type": "StaticMeshActor", "name": "MCP_FixCheck_01", "location": [0, 0, 300]}),
)

show(
    "find_actors_by_name pattern='MCP_FixCheck'",
    call("find_actors_by_name", {"pattern": "MCP_FixCheck"}),
)

show(
    "find_actors_by_name pattern='StaticMesh' (broad)",
    call("find_actors_by_name", {"pattern": "StaticMesh"}),
)

show(
    "get_actors_in_level (count of all actors)",
    call("get_actors_in_level"),
)

show(
    "delete_actor name='MCP_FixCheck_01'",
    call("delete_actor", {"name": "MCP_FixCheck_01"}),
)

show(
    "find_actors_by_name pattern='MCP_FixCheck' (post-delete)",
    call("find_actors_by_name", {"pattern": "MCP_FixCheck"}),
)
