# SiteSync AR — Claude Code Project Context

> Read this file first. It gives you full project context so you can pick up from any machine without prior conversation history.

## Session Memory Protocol

At the **start of every session**, read all four files in `memory/`:
- [`memory/user.md`](memory/user.md) — who James is and how to work with him
- [`memory/people.md`](memory/people.md) — team members, machines, roles
- [`memory/preferences.md`](memory/preferences.md) — response format, code style, git conventions
- [`memory/decisions.md`](memory/decisions.md) — key architectural and technical decisions

At the **end of every session**, update any file whose content changed — new decisions made, preferences clarified, people/roles changed. Keep entries dated. Do not duplicate existing entries; update in place.

## Canonical Working Tree (PC) — MUST

Always operate out of **`C:\Dev\SiteSync-AR\`** on PC. A stale clone used to live at `C:\Users\jruss\OneDrive\Desktop\Project Kickoff\SiteSync-AR\` — it's gone or on its way out; never edit, read, or commit from that path. UE 5.6 opens the Dev-folder project; every commit, LFS smudge, and memory file lives there. Any terminal work should `cd /c/Dev/SiteSync-AR` (or `git -C /c/Dev/SiteSync-AR …`) first.

## Xcode 26 / UE 5.6 Toolchain Patch (Mac only) — MUST re-apply after Launcher updates

UE 5.6.1's `Apple_SDK.json` caps `MaxVersion` at `16.9.0`, which predates Xcode 26. Mac has Xcode 26.4.1. Without the patch, UBT registers iOS/Mac as `buildable: False` and every iOS build fails with "Platform IOS is not a valid platform to build." UE 5.6's source is already clang-19-clean (modern `operator""_PrivateSV` spelling), so bumping the gate is safe.

**When to re-apply:** After any Epic Launcher update that touches UE 5.6 (point releases, reinstalls, "Verify"). The patch lives in a shared engine file outside git, so Launcher can silently reset it.

**Re-apply command (Mac):**
```bash
bash scripts/patch-ue56-xcode26.sh
```

The script is idempotent — safe to run every session. It edits `/Users/Shared/Epic Games/UE_5.6/Engine/Config/Apple/Apple_SDK.json`, bumping `"MaxVersion": "16.9.0"` → `"MaxVersion": "27.0.0"`, and drops a timestamped `.bak` alongside the original.

**How to tell it's needed:** Running `RunUBT.sh SiteSyncAR IOS Development ...` errors with `Platform IOS is not a valid platform to build`. Check `~/Library/Application Support/Epic/UnrealBuildTool/Log.txt` for `Found Sdk Version=..., MaxRequired=16.9.0` → patch missing.

This patch is Mac-only. PC does not need it (Windows build path doesn't use Apple_SDK.json).

## Workflow Rule — Prompt Block Labels

Every copy-paste prompt block appended to a response must carry an explicit header identifying where it runs. Pick the single most specific label; if a workflow truly spans machines, split into separate blocks (e.g. `## PC Prompt` for packaging + `## Mac Prompt` for deploy) rather than one ambiguous block.

- `## Mac Prompt` — runs on the MacBook Pro (iOS compile, Xcode, wired deploy, `scripts/patch-ue56-xcode26.sh`, Homebrew, git/gh CLI from Mac)
- `## PC Prompt` — runs on the Windows PC (UE5 editor on PC, Visual Studio 2022, Cursor, MCP server, PowerShell/cmd)
- `## UE5 Prompt` — UE5 editor / Blueprints / Claude Code inside UE5 when work is genuinely machine-agnostic. James decides which machine to run it on.
- `## Note` — content intended for `CLAUDE.md`, `README.md`, or other in-repo docs. Not executed — read by James or pasted into the doc.

Each block must be self-contained: include current node, what was just decided/built in the last session, and exactly what to do next. Do not assume the session has any prior context.

Example:

---
## UE5 Prompt
```
[copy-paste ready prompt for Claude Code in UE5 — machine-agnostic]
```
---

---

## Who You Are Working With

- **Developer:** James Russo — Blueprint-first UE5 developer. Escalate to C++ only when Blueprints cannot do the job.
- **Partner:** Cole
- **Location:** Boise, Idaho

**How James works:**
- Blueprint logic → deliver as ordered node-by-node walkthroughs (input pins, output pins, variable types, connection order)
- C++ → minimal files only, always show the `BlueprintCallable` declaration alongside so he can wire immediately
- MCP prompts → write as copy-paste-ready natural language for Cursor or Claude Desktop
- Ask clarifying questions before starting a Node if requirements are ambiguous

---

## Machine Responsibilities

| Machine | Role | Key Tools |
|---|---|---|
| Windows PC | UE5 Blueprints, Datasmith, asset management, MCP server | UE5.6.1, VS2022, Cursor, Claude Code |
| MacBook Pro 16" M5 Pro | UE5 Blueprints (remote sessions), iOS compilation, Xcode signing, wired device deploy | UE5.6.1, Xcode 26, Git, Homebrew, gh CLI |
| iPhone 16 Pro | LiDAR testing, AR session validation | — |

**UE5 is available on both PC and Mac.** James works on whichever machine is at hand. C++ compilation uses Visual Studio 2022 on PC; Mac uses the UE5 built-in toolchain. Always push and pull before switching machines — binary assets are LFS-tracked.

---

## Project Overview

Two-phase iOS AR app for AEC (Architecture, Engineering, Construction) professionals.

- **Phase 1** — On-site cut-and-fill earthwork calculator using LiDAR-scanned terrain
- **Phase 2** — 1:1 BIM clash detection overlay using Revit/Rhino models via Datasmith

**Engine:** Unreal Engine 5.6.1  
**AR Framework:** ARKit — Scene Reconstruction API (`meshWithClassification`)  
**Platform:** iOS 18+ · iPhone 16 Pro / iPad Pro (LiDAR required)  
**Build:** Xcode 16 · macOS 15+

> **Critical API note:** The original roadmap PDF says "ARKit Scene Depth API". This is WRONG for this use case. Use the **ARKit Scene Reconstruction API** — it produces a persistent 3D triangular mesh suitable for volume math. Scene Depth only produces a per-frame 2D depth buffer with no persistent geometry.

---

## Node Status

### Phase 1 — Cut-and-Fill AR Calculator

| Node | Description | Status |
|---|---|---|
| 1.1 | Source control, Git LFS, iOS config, plugin declarations | ✅ Complete |
| 1.2 | LiDAR environmental meshing via ARKit Scene Reconstruction | ✅ Complete (PC) · 🔄 iOS device deploy pending |
| 1.3 | Digital foundation anchoring with touch gesture placement | ✅ Complete (PC) · 🔄 iOS device deploy pending |
| 1.4 | Volumetric geometry scripting — cut-and-fill cubic yardage output | ⏳ Pending |

### Phase 2 — 1:1 BIM Clash Overlay

| Node | Description | Status |
|---|---|---|
| 2.1 | Datasmith ingestion pipeline (Revit / Rhino → UE5, mobile LODs) | ⏳ Pending |
| 2.2 | Geospatial & compass anchoring (GPS + compass auto-alignment) | ⏳ Pending |
| 2.3 | Engineering clash interface (MEP layer toggles + clash highlighting) | ⏳ Pending |

---

## What Was Built in Node 1.1

- `.gitignore` — strict UE5 exclusions, source guards
- `.gitattributes` — Git LFS for all binary asset types
- `Config/DefaultEngine.ini` — iOS runtime settings, Bundle ID placeholder, Metal enabled, 60fps lock
- `Config/IOS/IOSEngine.ini` — disables Nanite, Lumen, Ray Tracing, Path Tracing for iOS
- `Config/DefaultGame.ini` — camera + location PList usage descriptions
- `SiteSyncAR.uproject` — plugins declared: `AppleARKit`, `AugmentedReality`, `GeometryScripting`, `ProceduralMeshComponent`, `EnhancedInput`, `ModelingToolsEditorMode`
- `.mcp.json` (repo root) — Claude Code ↔ UnrealMCP server connection config (primary)
- `.cursor/mcp.json` — Cursor ↔ UnrealMCP server connection config (optional backup)
- Git LFS 3.7.1 installed on Mac via Homebrew
- Remote: `git@github.com:jamesrusso12/SiteSync-AR.git` (SSH auth)

**Bundle ID:** Still placeholder `com.yourcompany.SiteSyncAR` in `DefaultEngine.ini` — needs updating when Apple Developer paid account is activated.

**Apple Developer:** Using free Apple ID for prototype. Paid ($99/yr) needed only when Cole needs TestFlight access.

## What Was Built in Node 1.2

### C++ (committed to origin/main)
- `Source/SiteSyncAR/SiteSyncAR.Build.cs` — added `AugmentedReality`, `ProceduralMeshComponent`, `GeometryCore`, `GeometryScriptingCore` modules; `AppleARKit` iOS-only
- `Source/SiteSyncAR/Public/ARMeshBlueprintLibrary.h` — C++ shim: `GetAllARMeshGeometries()` → `TArray<UARMeshGeometry*>`; `GetARMeshData(geometry, OutVertices, OutIndices)` → `bool`
- `Source/SiteSyncAR/Private/ARMeshBlueprintLibrary.cpp` — `GetAllARMeshGeometries` + `#if !PLATFORM_IOS` stub for `GetARMeshData`
- `Source/SiteSyncAR/Private/ARMeshBlueprintLibrary_iOS.mm` — iOS impl: matches `ARMeshAnchor` by `UniqueId` from `ARSession.currentFrame`, reads `geometry.vertices` + `geometry.faces`, converts ARKit RH-Y-up meters → UE LH-Z-up cm via `FAppleARKitConversion::ToUEScale()` and `FVector(-z, x, y)` mapping

**C++ compile status (Mac 2026-04-21):** NOT VERIFIED — blocked by Xcode/UE toolchain mismatch. Attempted iOS build via `RunUBT.sh` and it died in engine SharedPCH (`StringView.h`) with `-Werror,-Wdeprecated-literal-operator` on `operator "" _PrivateSV` etc. Root cause: Mac has Xcode 26.4.1 (clang too new); UE 5.5 officially targets Xcode 15.x. Same Xcode is invoked by PC-driven SSH remote-build, so remote-build is also blocked until toolchain is resolved. Options: (a) install Xcode 15 alongside and point via `xcode-select`, (b) upgrade project to UE 5.6+ (supports newer Xcode) on both machines, (c) patch UE 5.5 source to silence the warning (fragile).

### UE5 Editor Assets (PC session 2026-04-20)

**DA_SiteSyncARConfig** — `Content/AR_SiteAnalysis/`
- Status: **✅ Verified 2026-04-24** — all fields confirmed by James
- Confirmed: Session Type = World, World Alignment = Gravity, Enable Auto Focus ✅, Light Estimation = Ambient Light Estimate, Generate Mesh Data from Tracked Geometry ✅, Track Scene Objects ✅, Scene Reconstruction Method = **Mesh With Classification**, Generate Collision for Mesh Data = **false**
- Re-verify in future sessions only if LiDAR mesh overlay stops appearing on device or classification data becomes unreliable for Node 1.4

**M_LiDARDebug** — `Content/Materials/`
- Status: **Complete and saved**
- Blend Mode: Translucent · Shading Model: Unlit · Two Sided: true
- Graph: Constant3Vector (R=0, G=1, B=0.8) → Emissive Color; Constant (0.35) → Opacity
- Confirmed via screenshot: Apply pressed, preview sphere correct

**BP_LiDARMeshManager** — `Content/Blueprints/`
- Status: **In progress — BeginPlay complete, RebuildMesh body not yet wired, not compiled**
- Component: `TerrainMesh` (ProceduralMeshComponent)
- Variables: `UpdateInterval` (Float, default 0.2), `MeshTimerHandle` (Timer Handle), `DebugMaterial` (Material Interface)
- BeginPlay chain ✅ wired: `Event BeginPlay` → `Start AR Session` (DA_SiteSyncARConfig) → `Set Timer by Event` (RebuildMesh delegate, Time=UpdateInterval, Looping=true) → `SET MeshTimerHandle`
- RebuildMesh body nodes specified but NOT confirmed wired — resume here:
  1. `RebuildMesh` custom event → `Clear All Mesh Sections` (Target: TerrainMesh)
  2. `GetAllARMeshGeometries` (ARMeshBlueprintLibrary) → For Each Loop
  3. For Each Loop → `GetARMeshData` (MeshGeometry=Array Element, OutVertices, OutIndices)
  4. On true → `Create Mesh Section` (Target: TerrainMesh, SectionIndex=Array Index, Vertices=OutVertices, Triangles=OutIndices, Normals=empty, CreateCollision=false)
  5. → `Set Material` (Target: TerrainMesh, ElementIndex=Array Index, Material=DebugMaterial)
  6. `Event EndPlay` → `Stop AR Session`
- After wiring: Compile → assign DebugMaterial in Details panel → place actor in level

### Shim API Reference (use these nodes in BP_LiDARMeshManager)
```
GetAllARMeshGeometries()  → TArray<UARMeshGeometry*>   // replaces Get All Geometries
GetARMeshData(Geometry, OutVertices, OutIndices) → bool // vertices + indices in one call, no separate normals
```
Note: `GetARMeshData` has no `OutNormals` — pass empty array to Normals pin on Create Mesh Section.

### Remote Build & Deploy
- **SSH key generation:** NOT ATTEMPTED — Project Settings → Platforms → iOS → Build → Generate SSH Key. Mac IP: 192.168.6.235, user: jamesrusso
- **IPA packaging:** NOT ATTEMPTED — blocked on SSH + BP compile

## What Was Built in Node 1.3

Digital foundation anchoring — user places a rectangular pad on the LiDAR-tracked terrain via two taps.

### Placement model (decided in session)
- **Option B — edge-aligned rotated rectangle.** Tap A = one end of the pad's long edge; Tap B = the other end. Pad rotates to `atan2(Delta.Y, Delta.X)` yaw. `WidthCm` (default 500cm) drives the short edge. `SlabThicknessCm` (default 10cm) cosmetic only.
- Third tap after placement resets the scene (destroys markers + foundation).
- Hit detection via `LineTraceTrackedObjects` against ARKit plane anchors (LiDAR-derived on iPhone 16 Pro). **Polish item deferred to Node 1.4:** C++ helper that raycasts against raw `ARMeshGeometry` triangles for non-planar terrain accuracy.

### Assets (all under `SiteSyncAR/Content/`)

- `Materials/M_FoundationDebug.uasset` — translucent orange unlit, 0.4 opacity
- `Materials/M_MarkerDebug.uasset` — translucent yellow unlit, 0.85 opacity
- `Blueprints/BP_TapMarker.uasset` — 5cm sphere, `M_MarkerDebug`
- `Blueprints/BP_Foundation.uasset` — static mesh cube (`M_FoundationDebug`) with `InitFromCorners(A, B)` function; uses Math Expression nodes for center `(A+B)/2`, delta `B-A`, yaw `atan2(Y,X)*57.29578`, length `sqrt(X²+Y²)/100`, width `WidthCm/100`, thickness `abs(V)/100`
- `Input/IA_TapPlace.uasset` — Axis2D IA (returns tap screen coords in pixels)
- `Input/IMC_Placement.uasset` — maps `Touch 1` → `IA_TapPlace`
- `Blueprints/BP_ARPlayerController.uasset` — owns `IMC_Placement` via EnhancedInputLocalPlayerSubsystem on BeginPlay; `IA_TapPlace (Started)` drives the state machine: Is Valid(ActiveFoundation) → reset, else tap state. Calls `GetWorldLocationFromTap(ScreenLocation) → WorldLocation, Hit` via `LineTraceTrackedObjects`. Vars: `bHasFirstTap`, `FirstTapLocation`, `MarkerA`, `MarkerB`, `ActiveFoundation`, `TapMarkerClass`, `FoundationClass`, `PlacementContext`.
- `Blueprints/BP_ARGameMode.uasset` — `GameModeBase` subclass with `PlayerControllerClass = BP_ARPlayerController`
- `SiteSyncAR/Config/DefaultEngine.ini` — `GlobalDefaultGameMode = /Game/Blueprints/BP_ARGameMode.BP_ARGameMode_C`

### Editor-test limitation (known)
PIE can't exercise this — `Start AR Session` returns false on Windows, so `LineTraceTrackedObjects` has nothing to hit. Validation happens on device only (wired Mac → iPhone 16 Pro).

### Gate to Node 1.4
Tap twice in an AR session on device → orange translucent slab appears spanning the two taps, rotated to the tap vector, sitting on the LiDAR-tracked surface. Third tap removes it. 60fps maintained.

---

## UE5 Asset Conventions

```
Content/AR_SiteAnalysis/
├── Blueprints/       BP_ prefix
├── UI/               WBP_ prefix
├── Meshes/           SM_ prefix
├── Materials/        M_ prefix
├── DatasmithAssets/  imported BIM geometry
└── MCP_TestScenes/   MCP-generated test environments (NOT shipped)
```

---

## Code Style Rules

- **Blueprint-first.** Only escalate to C++ for: ARKit callbacks, MCP plugin bridge, performance-critical loops
- All C++ exposed as `BlueprintCallable` `UFUNCTION`s
- Blueprint graphs organized into Functions/Macros with descriptive comment nodes
- No comments explaining WHAT code does — only WHY if non-obvious
- No speculative features, abstractions, or error handling for impossible states

---

## Performance Targets

- 60fps on iPhone 16 Pro — non-negotiable
- LiDAR mesh updates throttled to **200ms minimum intervals**
- Flag any operation that must move off the game thread
- LOD strategy required for all Datasmith assets (Phase 2)
- `Create Collision = False` on procedural mesh sections during prototype

---

## Git Workflow

**Remote:** `git@github.com:jamesrusso12/SiteSync-AR.git`  
**Auth:** SSH key on Mac. HTTPS + gh CLI on PC (if configured).  
**Default branch:** `main`

**Branch:** Commit directly to `main`. No feature branches required.

**Commit convention:** Conventional Commits
```
feat:   new node work
fix:    bug fixes
docs:   README, CLAUDE.md updates
chore:  config, LFS, MCP test scenes
```

**LFS:** Git LFS 3.7.1 (Mac). Must be installed on PC before first pull.
```bash
# PC setup (run once)
git lfs install
git lfs pull
```

---

## Unreal Engine MCP — Architecture

```
[Claude Code]   ← primary
[Cursor]        ← optional, kept as backup
       │
       ▼
   MCP Host → Tool Calls → Python Server (UV) → TCP Socket → UE5 C++ Plugin → UE5 API
```

**Plugin location (installed 2026-04-21):**
```
SiteSyncAR/Plugins/UnrealMCP/
  UnrealMCP.uplugin
  Source/UnrealMCP/      ← C++ TCP server, auto-starts on editor load
  Python/                ← MCP server for any MCP client
    unreal_mcp_server.py
```

**The C++ plugin auto-starts the TCP server on port 55557 when the editor loads — no manual start needed.** Multiple MCP clients can connect to the same port at the same time; Claude Code and Cursor do not conflict.

**Claude Code config (primary):** `.mcp.json` at repo root — `unrealMCP` server registered with relative `--directory` so the same file works on PC and Mac. After cloning or pulling for the first time, run `/mcp` inside Claude Code (or restart) so it picks up the server. Tools then surface as `mcp__unrealMCP__*`.

**Cursor config (optional backup):** `.cursor/mcp.json` in repo root — same server, registered for Cursor specifically. Kept so Cursor Agent mode still works if needed; safe to delete if Cursor is fully retired.

**Prereq on both machines:** `uv` must be on PATH. PC: `C:\Users\jruss\.local\bin\uv.exe`. Mac: typically `~/.local/bin/uv` or via Homebrew.

**MCP verification prompt (paste into Claude Code or Cursor — works in either):**
```
Using the unrealMCP server, spawn a static mesh cube actor at world location 
X=0, Y=0, Z=100. Name it "MCP_ConnectionTest". Confirm the actor name and location.
```

**Reliability tier (chongdashu/unreal-mcp is community-built, not Epic):**
- ✅ Solid: actor spawn/move/destroy, component property changes (material slot, scale, transform), level queries
- ⚠️ Hit-or-miss: Blueprint graph node-by-node rewiring with branches and struct-pin splits — fall back to manual editor work for non-trivial graphs

**MCP test scene prompt for Node 1.2 (terrain proxy simulation):**
```
Using unrealMCP, spawn five flat box meshes at these world locations to simulate 
LiDAR terrain patches at varying heights:
  Box 1: X=0,   Y=0,   Z=0,   Scale=(3,3,0.1)
  Box 2: X=300, Y=0,   Z=20,  Scale=(3,3,0.1)
  Box 3: X=0,   Y=300, Z=-15, Scale=(3,3,0.1)
  Box 4: X=300, Y=300, Z=35,  Scale=(3,3,0.1)
  Box 5: X=150, Y=150, Z=10,  Scale=(3,3,0.1)
Name them MCP_TerrainProxy_1 through 5. Apply a translucent cyan material.
```

---

## Debugging Reference

| Issue | Where to look |
|---|---|
| ARKit / LiDAR | UE5 Output Log → filter `ARKit` |
| MCP connection | Python terminal running `server.py` first, then UE5 Output Log |
| Datasmith import | Datasmith Import Log in Content Browser |
| iOS deployment | Xcode → Devices & Simulators console |
| C++ compile errors | Visual Studio 2022 Output window |

---

## Immediate Next Actions (after Node 1.3 PC complete)

Node 1.3 is fully built on PC (commit `285207b`, `feat(node-1.3): two-tap foundation anchoring...`). Device verification is the remaining gate before Node 1.4.

**On Mac (Node 1.3 device gate):**
1. `cd ~/Developer/SiteSync-AR/ && git pull && git lfs pull`
2. `bash scripts/patch-ue56-xcode26.sh` (idempotent Xcode 26 toolchain patch)
3. Re-stage and wired-deploy via the Node 1.2 pipeline (UBT → cook → UAT `-stage -skipbuild -skipcook`), install with `xcrun devicectl` to iPhone 16 Pro
4. Launch app → tap A on the LiDAR mesh → yellow marker appears; tap B elsewhere → second marker + orange translucent slab spans A→B, rotated to the tap vector; third tap resets everything
5. Confirm 60fps maintained during tap + spawn (Xcode Instruments or on-screen stat fps)
6. If hit tests miss on non-planar terrain → log for Node 1.4 polish (exact `ARMeshGeometry` ray-cast helper)

**Gate to Node 1.4:** foundation placement visibly correct on device.
