# UnrealMCP Improvements — Reliable Blueprint Graph Editing (2026-05-29)

Builds on the 2026-05-27 System-tools expansion (`docs/mcp-system-tools-2026-05-27.md`).
This pass targets the project's #1 documented MCP pain point:

> CLAUDE.md, Reliability tier: "Hit-or-miss: Blueprint graph node-by-node rewiring
> with branches and struct-pin splits — fall back to manual editor work for
> non-trivial graphs."

Project velocity has been repeatedly bottlenecked on falling back to manual
editor node walkthroughs. This change makes MCP-driven BP graph editing reliable.

## TL;DR — what to use now

Prefer **`build_blueprint_graph`** for all non-trivial graph work. It builds an
entire event/function graph in ONE tool call from a node + connection spec, wires
through the same K2 schema validation the editor UI uses, splits struct pins, sets
pin literals, and returns a per-connection success report so failures are visible
instead of silent. Use **`describe_blueprint_node`** to read real pin names before/
after, and **`split_struct_pin`** when you need to break out a struct (FVector → X/Y/Z).

## Why pure-Python (execute_python) was NOT the answer

The brief asked whether reliable BP graph editing is achievable via UE 5.6 Python
(now that `execute_python` exists). **Researched and rejected for the core path:**
stock UE 5.6's `PythonScriptPlugin` does **not** expose the K2 graph-building surface
to Python. `unreal.K2Node` appears only as an abstract type stub; the methods that
actually build graphs — `AllocateDefaultPins`, `UEdGraphPin::MakeLinkTo`,
`UEdGraphSchema_K2::TryCreateConnection` / `CanSplitStructPin` / `SplitPin` /
`TrySetDefaultValue` — are not callable from Python. The old 20tab `UnrealEnginePython`
plugin exposed `graph_add_node_*` helpers, but that plugin is abandoned and not present
here. So the reliable route is **C++ using the K2 schema**, exposed as a high-level
atomic command. (`execute_python` remains the right tool for asset/DataAsset/Datasmith
ops, which Python *does* cover well — it just can't reach the graph schema.)

## Root causes of the old unreliability (and the fix for each)

| Old failure mode | Fix in this change |
|---|---|
| Caller had to **guess pin names** ("exec"/"then"/"Target"/"ReturnValue") with no way to read them | `describe_blueprint_node` dumps real pins; failed connections return the node's available-pin list in the error |
| Raw `UEdGraphPin::MakeLinkTo` **bypasses the K2 schema** → type-mismatched / silently-bad links | `build_blueprint_graph` wires via `UEdGraphSchema_K2::CanCreateConnection` + `TryCreateConnection` (autocasts, type checks, array/struct promotion — same as the editor UI) |
| **Struct pins couldn't be split** (FVector .X/.Y/.Z, hit-result breaks) | `split_struct_pin` + dotted pin syntax `"ReturnValue.X"` in the build spec (auto-splits on demand) |
| State spread across **many round-trips**; one bad pin strands the whole graph, no atomicity | One atomic call: all nodes created first (refs resolve in any order), then literals, then wires; **per-connection** `ok`/`error` report instead of all-or-nothing silence |

## New commands

All three live in the new `FUnrealMCPGraphCommands` handler and use the static
`GetSupportedCommands()` registry pattern, so the bridge dispatcher needs exactly
**one line** for the whole handler (no per-command edit — kills the "two-edit
dispatcher fragility" failure mode for this handler).

### `build_blueprint_graph(blueprint_name, nodes, connections, graph="EventGraph")`

`nodes[]` — each:
```jsonc
{ "ref": "begin",            // YOUR alias, referenced by connections
  "type": "event",           // event | call_function | variable_get | variable_set | self | branch | sequence
  "event_name": "ReceiveBeginPlay",
  "position": [0, 0],        // optional
  "pin_defaults": { "Duration": "1.0" } }  // optional input-pin literals
```
- `call_function`: `function_name` [+ `target`]. `target` omitted/"self" searches the
  BP's own + inherited functions; or a class name ("KismetSystemLibrary",
  "GameplayStatics") or script path ("/Script/Engine.GameplayStatics").
- `branch` = If/Then/Else, `sequence` = ExecutionSequence.

`connections[]` — each: `{ "from": "<ref>", "from_pin": "then", "to": "<ref>", "to_pin": "execute" }`.
Split-struct sub-pins use dotted syntax: `"to_pin": "NewLocation.Z"`.

Returns:
```jsonc
{ "success": true,
  "node_ids": { "begin": "<guid>", "print": "<guid>" },
  "connections": [ { "from": "begin.then", "to": "print.execute", "ok": true } ],
  "errors": [] }
```

Example — BeginPlay → Delay(1.0) → PrintString("ready"):
```python
build_blueprint_graph(
  blueprint_name="BP_Demo",
  nodes=[
    {"ref":"begin","type":"event","event_name":"ReceiveBeginPlay","position":[0,0]},
    {"ref":"delay","type":"call_function","function_name":"Delay","target":"KismetSystemLibrary",
     "position":[300,0],"pin_defaults":{"Duration":"1.0"}},
    {"ref":"print","type":"call_function","function_name":"PrintString","target":"KismetSystemLibrary",
     "position":[600,0],"pin_defaults":{"InString":"ready"}},
  ],
  connections=[
    {"from":"begin","from_pin":"then","to":"delay","to_pin":"execute"},
    {"from":"delay","from_pin":"then","to":"print","to_pin":"execute"},
  ],
)
```

### `describe_blueprint_node(blueprint_name, node_id="all", graph="EventGraph")`
Returns real pins (name, direction, category, sub_type, is_array, default_value,
link_count, can_split, sub_pins) for one node GUID or every node. The introspection
the old flow lacked — wire by fact, not guess.

### `split_struct_pin(blueprint_name, node_id, pin, graph="EventGraph")`
Splits a struct pin into sub-pins; returns the new sub-pin names. The one operation
the upstream README explicitly calls out as needing manual editor work.

## Files changed

| File | Change |
|---|---|
| `Plugins/UnrealMCP/Source/UnrealMCP/Public/Commands/UnrealMCPGraphCommands.h` | **NEW** — handler declaration + registry accessor + helper signatures |
| `Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPGraphCommands.cpp` | **NEW** — the three commands; schema-validated wiring, struct-pin split, atomic build + per-connection report |
| `Plugins/UnrealMCP/Source/UnrealMCP/Public/UnrealMCPBridge.h` | include + `TSharedPtr<FUnrealMCPGraphCommands> GraphCommands` member |
| `Plugins/UnrealMCP/Source/UnrealMCP/Private/UnrealMCPBridge.cpp` | include, ctor/dtor init/reset, **one-line** registry dispatcher entry |
| `Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPSystemCommands.cpp` | `list_commands` now also reports `graph_commands` (true introspection surface) |
| `Plugins/UnrealMCP/Python/tools/graph_tools.py` | **NEW** — `build_blueprint_graph`, `describe_blueprint_node`, `split_struct_pin` MCP tools with spec docs |
| `Plugins/UnrealMCP/Python/unreal_mcp_server.py` | register `register_graph_tools`; `info()` prompt promotes graph tools, demotes legacy per-node flow |
| `Plugins/UnrealMCP/Python/tools/system_tools.py` | `list_system_commands` returns `{system_commands, graph_commands}` |
| `Plugins/UnrealMCP/Python/scripts/probe_graph_commands.py` | **NEW** — non-destructive reachability probe + commented live build test |
| `docs/mcp-improvements-2026-05-29.md` | **NEW** — this file |

No `UnrealMCP.Build.cs` change needed — `BlueprintGraph`, `Kismet`, `KismetCompiler`
were already private deps, and they supply `UEdGraphSchema_K2`, `UK2Node_IfThenElse`,
`UK2Node_ExecutionSequence`.

## Build + verify (when the editor is FREE)

The UE editor holds `UnrealEditor-UnrealMCP.dylib` locked, so it must be **closed**
before building. (Per the brief, this worktree did not build — the main-tree editor
is open.)

1. **Close the UE editor.**
2. **Rebuild the editor module (Mac):**
   ```bash
   "/Users/Shared/Epic Games/UE_5.6/Engine/Build/BatchFiles/Mac/Build.sh" \
     SiteSyncAREditor Mac Development \
     -project="$HOME/Developer/SiteSync-AR/SiteSyncAR/SiteSyncAR.uproject"
   ```
   (PC equivalent: `Build.bat SiteSyncAREditor Win64 Development -project=... -waitmutex`.)
3. **Reopen the editor.** The TCP server auto-starts on port 55557.
4. **Reachability probe (direct TCP, no MCP session needed):**
   ```bash
   python SiteSyncAR/Plugins/UnrealMCP/Python/scripts/probe_graph_commands.py
   ```
   Expect: `list_commands` shows the three graph commands; each graph command returns
   a domain error ("Blueprint not found") — **NOT** "Unknown command" (which would mean
   stale DLL / unwired dispatcher).
5. **Live build test:** edit `probe_graph_commands.py`, set `BP_NAME` to a real
   disposable Blueprint, uncomment the LIVE BUILD TEST block, and re-run. It builds a
   BeginPlay → PrintString graph and saves it. Open the BP in-editor to eyeball the wiring.
6. Existing regression: `python SiteSyncAR/Plugins/UnrealMCP/Python/scripts/probe_post_rebuild.py`
   should still pass (no existing command was modified).

## Risks / human judgment before merge

- **Not compiled in this worktree** (hard constraint — editor was open on main tree).
  All API signatures were verified against the UE 5.6 engine headers
  (`FindFirstObject`, `EdGraphSchema_K2::{CanCreateConnection,TryCreateConnection,
  TrySetDefaultValue,CanSplitStructPin,SplitPin}`, `K2Node_IfThenElse`,
  `K2Node_ExecutionSequence`, `TryGetObjectField`), but a clean compile is the real gate.
- `ReconstructNode()` is called on every built node to settle pin state. For most node
  types this is a no-op-safe refresh; if a specific node misbehaves, drop the reconstruct
  loop and rely on `AllocateDefaultPins` (already called at creation).
- `build_blueprint_graph` does **not** auto-compile the Blueprint — call
  `compile_blueprint` and `save_package` afterward (kept separate so you can inspect
  the graph before committing it to bytecode).
- The handler edits in-memory only until a save command runs (same as every other MCP
  write) — finish with `save_package` / `save_all_dirty_packages`.
- Node-type coverage is intentionally the high-frequency set (event, call_function,
  variable get/set, self, branch, sequence). Macros, custom events with params, cast
  nodes, and delegate binds are not yet first-class — extend `CreateNodeFromSpec` (one
  function, no dispatcher edit needed) as needs arise.
