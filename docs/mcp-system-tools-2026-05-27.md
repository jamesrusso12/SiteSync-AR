# UnrealMCP — System Tools (added 2026-05-27)

Architectural expansion of the chongdashu plugin. Adds the missing pieces
the cwilcox0916 reference plugin highlights — chief among them, a
`execute_python` tool that lets future Claude sessions reach the full
`unreal` Python API without a C++ rebuild for every new operation.

## What's new

### `execute_python`
Run any Python inside the editor.

```
execute_python(script="import unreal; print(unreal.SystemLibrary.get_engine_version())")
execute_python(script_file="dev/import_datasmith.py", mode="file")
execute_python(script="unreal.EditorAssetLibrary.list_assets('/Game/Maps', recursive=True)", mode="eval")
```

Modes: `statement` (default, multi-line OK), `file` (path-based, same as
`UnrealEditor-Cmd -run=pythonscript -script=`), `eval` (single expression
returns result string).

Returns `{ success, result, log_output, errors }`. Python tracebacks land
in `errors`.

**Why this matters for SiteSync:** every one-off helper script under
`dev/` (`import_datasmith.py`, `set_ar_alignment.py`, `print_mesh_bounds.py`,
the future `merge_actors.py`, etc.) is now reachable as a single MCP tool
call. No more "I need to run a Python script" → close editor / cook detour.

### `save_all_dirty_packages`
Persist MCP edits to `.uasset` / `.umap` on disk.

```
save_all_dirty_packages()
save_all_dirty_packages(save_content_packages=True, save_map_packages=False)
```

Fixes the long-standing "MCP edits live in editor memory only; James must
Ctrl+S" hand-off step documented in CLAUDE.md.

### `save_package`
Save a single asset by package path.

```
save_package("/Game/Blueprints/BP_ARPlayerController_BIM")
```

### `reparent_actor_root`
Promote a scene component to be an actor's `RootComponent` — the C++
equivalent of the manual fix that landed in commit `116e4b5` for the
BIMMesh-is-root scale-clobber bug.

```
reparent_actor_root(actor_name="BP_BIMOverlay", new_root="OverlayRoot")
```

### `list_system_commands`
Returns every command the System dispatcher recognizes. Use this when an
`Unknown command` reply suggests the stale-DLL / missing-dispatcher-entry
failure mode the CLAUDE.md "MCP plugin health" section warns about.

## How to enable

Three files changed; one shipped Plugin dependency added; one C++ rebuild.

1. **Editor closed.** Required — the editor holds `UnrealEditor-UnrealMCP.dll`
   locked.

2. **Rebuild the plugin** (PC):
   ```
   "C:\UE_5.6\Engine\Build\BatchFiles\Build.bat" SiteSyncAREditor Win64 Development ^
     -project="C:\Dev\SiteSync-AR\SiteSyncAR\SiteSyncAR.uproject" -waitmutex
   ```
   Mac equivalent if working from Mac:
   ```
   "/Users/Shared/Epic Games/UE_5.6/Engine/Build/BatchFiles/Mac/Build.sh" \
     SiteSyncAREditor Mac Development \
     -project="$HOME/Developer/SiteSync-AR/SiteSyncAR/SiteSyncAR.uproject"
   ```

3. **Reopen the editor.** The C++ TCP server auto-starts on port 55557 as
   before. The new commands are available immediately.

4. **Verify** (in this Claude Code session, after editor is up):
   ```
   list_system_commands()
   ```
   Should return `["execute_python", "save_all_dirty_packages",
   "save_package", "reparent_actor_root", "list_commands"]`. If you get
   `Unknown command: list_commands`, the rebuild didn't take — re-check
   that the editor was closed before Build.bat ran.

   Smoke test execute_python:
   ```
   execute_python(script="import unreal; print('hello from python', unreal.SystemLibrary.get_engine_version())")
   ```

## Files changed

```
SiteSyncAR/Plugins/UnrealMCP/UnrealMCP.uplugin
  + PythonScriptPlugin dependency

SiteSyncAR/Plugins/UnrealMCP/Source/UnrealMCP/UnrealMCP.Build.cs
  + "PythonScriptPlugin" in editor-only PrivateDependencyModuleNames

SiteSyncAR/Plugins/UnrealMCP/Source/UnrealMCP/Public/UnrealMCPBridge.h
  + #include "Commands/UnrealMCPSystemCommands.h"
  + TSharedPtr<FUnrealMCPSystemCommands> SystemCommands

SiteSyncAR/Plugins/UnrealMCP/Source/UnrealMCP/Private/UnrealMCPBridge.cpp
  + SystemCommands instantiated / reset
  + dispatcher entry that delegates via FUnrealMCPSystemCommands::GetSupportedCommands()
    (single source of truth — future system commands need only be added in the
    handler, not in the bridge)

SiteSyncAR/Plugins/UnrealMCP/Source/UnrealMCP/Public/Commands/UnrealMCPSystemCommands.h   [NEW]
SiteSyncAR/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPSystemCommands.cpp [NEW]

SiteSyncAR/Plugins/UnrealMCP/Python/tools/system_tools.py   [NEW]
SiteSyncAR/Plugins/UnrealMCP/Python/unreal_mcp_server.py
  + register_system_tools(mcp)
  + info() prompt updated

docs/mcp-system-tools-2026-05-27.md   [NEW — this file]
```

## Design rationale — why these tools, why not more

The cwilcox0916/claude-ue-plugin reference (Lobehub-indexed; 215 tools, 13
resources, 10 prompts) reaches its breadth via a Connection Manager that
tiers Remote Control API → Python Executor → Native Plugin. Adopting that
whole architecture would mean rewriting a working server and breaking the
existing 35-ish tools James already relies on.

Instead this change picks the **highest-leverage subset**:
- `execute_python` collapses the dominant pain point (every new operation
  needs C++ + rebuild) to a single tool call.
- `save_all_dirty_packages` / `save_package` close the gap CLAUDE.md
  explicitly flags ("MCP edits do NOT trigger Save All").
- `reparent_actor_root` is the C++ codification of a class of structural
  fixes (commit `116e4b5`).
- The static `GetSupportedCommands()` registry pattern is a partial fix
  for the dispatcher-fragility failure mode CLAUDE.md documents ("Unwired
  dispatcher — adding a new handler is two edits, not one"). New system
  commands now need exactly one edit.

Future work — if the breadth gap matters — would extend the same
registry pattern to the other four handlers (Editor, Blueprint,
BlueprintNode, Project, UMG) so the bridge dispatcher collapses to a
single `TMap<FString, TFunction<...>>` and the `Unknown command` silent-
fail mode disappears entirely. Not in scope for this change.
