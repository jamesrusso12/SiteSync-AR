# SiteSync AR — Claude Code Project Context

> Read this file first. It gives you full project context so you can pick up from any machine without prior conversation history.

## Session Memory Protocol

At the **start of every session**, read all four files in `memory/`:
- [`memory/user.md`](memory/user.md) — who James is and how to work with him
- [`memory/people.md`](memory/people.md) — team members, machines, roles
- [`memory/preferences.md`](memory/preferences.md) — response format, code style, git conventions
- [`memory/decisions.md`](memory/decisions.md) — key architectural and technical decisions

At the **end of every session**, update any file whose content changed — new decisions made, preferences clarified, people/roles changed. Keep entries dated. Do not duplicate existing entries; update in place.

## Workflow Rule — PC Prompts

James works across two machines. Whenever a response includes work that needs to be done on the **Windows PC** (anything involving UE5, Blueprints, Cursor, MCP, Visual Studio, Datasmith, or the UE5 editor), always append a ready-to-paste prompt block at the end of the response under this exact header:

---
## PC Prompt
```
[copy-paste ready prompt for Claude Code on the PC]
```
---

The PC prompt must be self-contained: include current node, what was just decided/built on Mac, and exactly what to do next in UE5. Do not assume the PC session has any prior context.

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
| Windows PC | UE5 Blueprints, Datasmith, asset management, MCP server | UE5.5.4, VS2022, Cursor, Claude Code |
| MacBook Pro 16" M5 Pro | iOS compilation, Xcode signing, wired device deploy | Xcode 16, Git, Homebrew, gh CLI |
| iPhone 16 Pro | LiDAR testing, AR session validation | — |

---

## Project Overview

Two-phase iOS AR app for AEC (Architecture, Engineering, Construction) professionals.

- **Phase 1** — On-site cut-and-fill earthwork calculator using LiDAR-scanned terrain
- **Phase 2** — 1:1 BIM clash detection overlay using Revit/Rhino models via Datasmith

**Engine:** Unreal Engine 5.5.4  
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
| 1.2 | LiDAR environmental meshing via ARKit Scene Reconstruction | 🔄 In Progress |
| 1.3 | Digital foundation anchoring with touch gesture placement | ⏳ Pending |
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
- `.cursor/mcp.json` — Cursor ↔ UnrealMCP server connection config
- Git LFS 3.7.1 installed on Mac via Homebrew
- Remote: `git@github.com:jamesrusso12/SiteSync-AR.git` (SSH auth)

**Bundle ID:** Still placeholder `com.yourcompany.SiteSyncAR` in `DefaultEngine.ini` — needs updating when Apple Developer paid account is activated.

**Apple Developer:** Using free Apple ID for prototype. Paid ($99/yr) needed only when Cole needs TestFlight access.

## What Was Built in Node 1.2

### C++ (committed to origin/main)
- `Source/SiteSyncAR/SiteSyncAR.Build.cs` — added `AugmentedReality`, `ProceduralMeshComponent`, `GeometryCore`, `GeometryScriptingCore` modules; `AppleARKit` iOS-only
- `Source/SiteSyncAR/Public/ARMeshBlueprintLibrary.h` — C++ shim: `GetAllARMeshGeometries()` → `TArray<UARMeshGeometry*>`; `GetARMeshData(geometry, OutVertices, OutIndices)` → `bool`
- `Source/SiteSyncAR/Private/ARMeshBlueprintLibrary.cpp` — implementation

**C++ compile status:** NOT VERIFIED — files exist in repo. On next session open, filter Output Log by `ARKit` and paste errors here.

### UE5 Editor Assets (PC session 2026-04-20)

**DA_SiteSyncARConfig** — `Content/AR_SiteAnalysis/`
- Status: **Created and configured — verify Scene Reconstruction setting**
- Confirmed via screenshot: Generate Mesh Data from Tracked Geometry ✅, Track Scene Objects ✅, Session Type = World, World Alignment = Gravity, Enable Auto Focus ✅, Light Estimation = Ambient Light Estimate
- ⚠️ Scene Reconstruction Method was `Mesh Only` in session — must be `Mesh With Classification` for Node 1.4 classification data. Verify before next session.
- ⚠️ Generate Collision for Mesh Data was checked — uncheck it (mobile perf overhead, not needed until Node 1.3)

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

**Branch naming:**
```
feature/node-1.1-source-control
feature/node-1.2-lidar-meshing
feature/node-1.3-foundation-anchor
feature/node-1.4-cut-fill-calc
feature/node-2.1-datasmith-pipeline
feature/node-2.2-geospatial-anchor
feature/node-2.3-clash-interface
```

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
[Claude / Cursor] → MCP Host → Tool Calls → Python Server (UV) → TCP Socket → UE5 C++ Plugin → UE5 API
```

**Plugin location (PATH B integration — not yet done):**
```
Plugins/UnrealMCP/
  UnrealMCP.uplugin
  Source/UnrealMCP/
  Python/server.py
```

**To start the MCP server (PC — run before using Cursor MCP):**
```bash
cd Plugins/UnrealMCP
uv run server.py
```

**Cursor config:** `.cursor/mcp.json` already in repo root — enable `unrealMCP` in Cursor Settings → Tools.

**MCP verification prompt (paste into Cursor Agent mode):**
```
Using the unrealMCP server, spawn a static mesh cube actor at world location 
X=0, Y=0, Z=100. Name it "MCP_ConnectionTest". Confirm the actor name and location.
```

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

## Immediate Next Actions (as of Node 1.2)

**On PC (UE5.5.4):**
1. Open `SiteSyncAR.uproject` → click **Yes** to rebuild C++ modules
2. Create `DA_SiteSyncARConfig` data asset (ARSessionConfig, Scene Reconstruction = Mesh With Classification)
3. Create `M_LiDARDebug` material (Translucent, Unlit, cyan emissive, 0.35 opacity)
4. Create `BP_LiDARMeshManager` Actor Blueprint:
   - Component: `ProceduralMeshComponent` (named `TerrainMesh`)
   - BeginPlay: `Start AR Session` (DA_SiteSyncARConfig) → `Set Timer by Event` (0.2s, looping) → `UpdateLiDARMesh`
   - Custom Event `UpdateLiDARMesh`: `GetAllARMeshGeometries` → `Clear All Mesh Sections` → ForEachLoop → `GetARMeshData` → Branch → `Create Mesh Section` → `Set Material`
5. Place `BP_LiDARMeshManager` in default level
6. Run MCP terrain proxy test scene to validate mesh section logic in editor

**On Mac (after PC work is committed and pulled):**
7. Open in Xcode → wired deploy to iPhone 16 Pro
8. Confirm LiDAR mesh appears as cyan translucent overlay in AR session
9. Confirm 60fps is maintained while scanning

**Gate to Node 1.3:** LiDAR mesh visible and stable on device. ✓
