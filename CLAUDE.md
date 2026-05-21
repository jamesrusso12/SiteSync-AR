# SiteSync AR — Claude Code Project Context

> Read this file first. It gives you full project context so you can pick up from any machine without prior conversation history.

## Session Memory Protocol

At the **start of every session**, read all four files in `memory/`:
- [`memory/user.md`](memory/user.md) — who James is and how to work with him
- [`memory/people.md`](memory/people.md) — team members, machines, roles
- [`memory/preferences.md`](memory/preferences.md) — response format, code style, git conventions
- [`memory/decisions.md`](memory/decisions.md) — key architectural and technical decisions

At the **end of every session**, update any file whose content changed — new decisions made, preferences clarified, people/roles changed. Keep entries dated. Do not duplicate existing entries; update in place.

## Canonical Working Trees — HARD RULE, BOTH MACHINES

There is **exactly one** working tree per machine. Every Read, Edit, Write, Glob, Grep, Bash, and `git` command must use the canonical path or be prefixed with `git -C <canonical>` / `cd <canonical>`. Tool harnesses sometimes default to other paths (an old clone, a stale cwd) — treat anything outside the canonical row below as wrong by default and switch before doing anything.

| Machine | Canonical path | Notes |
|---|---|---|
| Windows PC | `C:\Dev\SiteSync-AR\` | Use `git -C /c/Dev/SiteSync-AR …` from bash. Open `SiteSyncAR/SiteSyncAR.uproject` for UE5; open `SiteSyncAR/SiteSyncAR.sln` for VS2022. |
| Mac | `~/Developer/SiteSync-AR/` | NOT under `~/Desktop/` or `~/Documents/` — those auto-sync to iCloud and `codesign` rejects iCloud xattrs. See decisions.md 2026-04-21 "Mac project relocated from Desktop to ~/Developer". |

**On every prompt that crosses machines** (PC Prompt → Mac Prompt or vice versa): write file paths in the receiving machine's canonical form. Never paste a `C:\…` path into a Mac Prompt or a `~/…` / `/Users/…` path into a PC Prompt. The receiving Claude should not have to guess or translate.

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
- `## PC Prompt` — runs on the Windows PC (UE5 editor on PC, Visual Studio 2022, MCP server, PowerShell/cmd)
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
- MCP work → Claude Code drives the UE5 editor directly via the `unrealMCP` tools; no copy-paste prompt blocks needed for MCP-able work
- Ask clarifying questions before starting a Node if requirements are ambiguous

---

## Machine Responsibilities

| Machine | Role | Key Tools |
|---|---|---|
| Windows PC | UE5 Blueprints, Datasmith, asset management, MCP server | UE5.6.1, VS2022, Claude Code |
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
**Build:** Xcode 26 · macOS 15+ (requires `Apple_SDK.json` MaxVersion patch — see Toolchain Patch section)

> **Critical API note:** The original roadmap PDF says "ARKit Scene Depth API". This is WRONG for this use case. Use the **ARKit Scene Reconstruction API** — it produces a persistent 3D triangular mesh suitable for volume math. Scene Depth only produces a per-frame 2D depth buffer with no persistent geometry.

---

## Node Status

### Phase 1 — Cut-and-Fill AR Calculator

| Node | Description | Status |
|---|---|---|
| 1.1 | Source control, Git LFS, iOS config, plugin declarations | ✅ Complete |
| 1.2 | LiDAR environmental meshing via ARKit Scene Reconstruction | ✅ Complete · device-validated end-to-end 2026-04-28 (post BP_ARPawn fix) |
| 1.3 | Digital foundation anchoring with touch gesture placement | ✅ Complete · device-validated 2026-04-28 |
| 1.4 | Volumetric geometry scripting — cut-and-fill cubic yardage output | ✅ **Complete** · v20 device-validated 2026-05-11 (5 reset cycles · markers on mesh within ±3cm · all 4 log success patterns confirmed) |

### Phase 2 — 1:1 BIM Clash Overlay

| Node | Description | Status |
|---|---|---|
| 2.1 | Datasmith ingestion pipeline (Revit / Rhino → UE5, mobile LODs) | 🚧 **In progress** · 2026-05-21: Rhino `TestBuilding` imported (9 UE assets via headless `pythonscript`) and wired into `BP_BIMOverlay` as `SM_TestBuilding_Merged` (commit `2703f54`). Next: iOS device validation. |
| 2.2 | Geospatial & compass anchoring (GPS + compass auto-alignment) | ⏳ Pending |
| 2.3 | Engineering clash interface (MEP layer toggles + clash highlighting) | ⏳ Pending |

---

## Demo Snapshot — Idaho Technology Council (postponed indefinitely as of 2026-05-19)

James and Cole were scheduled to attend the **Idaho Technology Council** conference on Tuesday 2026-05-19 to network with potential clients and investors. The event was **postponed indefinitely** mid-debug on the morning of 2026-05-19; no rescheduled date provided. See `memory/decisions.md` 2026-05-19 "Idaho Technology Council demo postponed" for context.

Treat this section as a **demo-ready snapshot specification** rather than a deadline. The work in `main` (Model Scale, WBP_BIMPlacementHUD, wireframe-edge M_BIMOverlay, Fab "Free Small Old House" placeholder, scale-modes docs) is kept as a known-good baseline for whenever the event reschedules or another demo opportunity arrives. **Goal is unchanged:** get the current state into something **a stranger can understand without prior explanation.**

Original working calendar (kept as historical context for what the team was planning around):

| Day | James availability |
|---|---|
| Thu 2026-05-14 | Until ~10:30 AM Mountain, then work until 4:30 PM, evening session after |
| Fri 2026-05-15 | James's birthday — relaxed, partial day available |
| Sat 2026-05-16 | Relaxed, work time available |
| Sun 2026-05-17 | Roommate's birthday BBQ, maybe partial evening |
| Mon 2026-05-18 | Full work session expected |
| Tue 2026-05-19 | Originally demo day; now freed up — event postponed mid-morning |

### Current state — what a stranger sees today

After v22 (`commit 476752b`), the BIM mode on iPhone shows: cyan LiDAR mesh, then two taps drop a **room-sized 5m × 5m × 9m orange translucent placeholder** that fills the view and tints everything orange. **No HUD, no numbers, no on-screen prompts.** Reset/replace loop works perfectly, but it doesn't read as anything to an outsider.

Phase 1 (cut/fill) is functionally complete and visually richer (live cut/fill yd³ HUD) but currently NOT the boot map.

### Scale modes — Site Scale (1:1) vs Model Scale (~0.3×)

Phase 2's `PlaceBIMByCornerForward` placement primitive supports two scale modes via its `LengthCm/WidthCm/HeightCm` parameters:

- **Site Scale (production)** — `L=100, W=100, H=100` → actor scale `(1, 1, 1)` → mesh renders at native size. For real on-site 1:1 walkthrough where the planned building is overlaid lifesize on the survey corner.
- **Model Scale (demo / indoor review)** — `L=30, W=30, H=30` → actor scale `(0.3, 0.3, 0.3)` → mesh renders at ~30% native. For indoor design reviews, client presentations, or trade-show demos where the viewer needs to see the whole building from outside without standing inside opaque walls.

For Tuesday 2026-05-19, `BP_ARPlayerController_BIM` uses Model Scale. The imported Fab "Free Small Old House" mesh (native ~7.7m × 8.3m × 2.4m) renders at ~2.3m × 2.5m × 0.7m, and `ShowPostPlacement` HUD reads "(2.3 m × 2.5 m × 0.7 m)" to match.

Trimble Sitevision and Autodesk Construction Cloud AR both ship with toggleable site/model review modes — the distinction is AEC industry-standard. Surfacing as a runtime HUD toggle is post-demo roadmap. See `memory/decisions.md` 2026-05-18 for full rationale + demo talking-point framing.

### Pre-Tuesday goal: real BIM ingestion + intelligible demo flow

**The single biggest visual unlock** is replacing the orange cube with a **real-looking building model imported via Datasmith** (or a swap-in Static Mesh from a free Fab / Twinmotion / Epic sample). Plus a minimal HUD that walks the viewer through the workflow.

#### Tier A — must-have for the demo to read as a real product

1. **Real BIM model swapped into BP_BIMOverlay** — either via Datasmith Direct Link / `.udatasmith` import or a free Static Mesh asset that LOOKS like a building (sample office, hotel, residential pad). Replace the engine cube. Keep the corner-at-origin pivot convention so `PlaceBIMByCornerForward` math still works.
2. **`WBP_BIMPlacementHUD`** — three-state instruction overlay:
   - Pre-corner: "Tap the floor to mark a building corner"
   - Post-corner: "Tap a second point along the building's front edge"
   - Post-placement: "Building placed (L×W×H m). Tap anywhere to reset."
   - Plus a small dimensions readout
3. **Shrink placeholder defaults** in `PlaceBIMByCornerForward.h` (or override at the BP call site) to room-scale (e.g., 4m × 4m × 3m) so even the placeholder reads as an object before real BIM lands.

#### Tier B — nice-to-have if Tier A finishes early

- **`SiteSync_Menu.umap`** — main menu level with two buttons to launch Phase 1 (cut/fill) or Phase 2 (BIM) flows. Lets James demo *both* modes during a conversation. Today the boot map is `/Game/Maps/SiteSync_BIMTest` (Phase 2 only).
- BIM placeholder material polish (wireframe edges, dimension label hovering)
- Demo script with talking points

#### Explicitly NOT in scope for Tuesday

- TestFlight for Cole (Personal Team only, weekly re-deploy)
- Multi-pad layouts
- Width / thickness runtime sliders
- Node 2.2 GPS+compass anchoring
- Node 2.3 clash interface

---

## What Was Built in Node 1.1

- `.gitignore` — strict UE5 exclusions, source guards
- `.gitattributes` — Git LFS for all binary asset types
- `Config/DefaultEngine.ini` — iOS runtime settings, Bundle ID placeholder, Metal enabled, 60fps lock
- `Config/IOS/IOSEngine.ini` — disables Nanite, Lumen, Ray Tracing, Path Tracing for iOS
- `Config/DefaultGame.ini` — camera + location PList usage descriptions
- `SiteSyncAR.uproject` — plugins declared: `AppleARKit`, `AugmentedReality`, `GeometryScripting`, `ProceduralMeshComponent`, `EnhancedInput`, `ModelingToolsEditorMode`
- `.mcp.json` (repo root) — Claude Code ↔ UnrealMCP server connection config
- Git LFS 3.7.1 installed on Mac via Homebrew
- Remote: `git@github.com:jamesrusso12/SiteSync-AR.git` (SSH auth)

**Bundle ID:** `com.RussoCompany.SiteSyncAR` (final, set 2026-04-21).

**Apple Developer:** Personal Team via Apple ID, Team ID `PD29S4YQ4P`. **No paid Developer Program** — Apple retail Business Conduct rules bar James from paid enrollment until he leaves retail. Personal Team gives wired Mac → iPhone deploy with 7-day provisioning profiles; no TestFlight, so Cole can't get external builds yet. See decisions.md 2026-04-21 "CONSTRAINT: No paid Apple Developer Program".

## What Was Built in Node 1.2

### C++ (committed to origin/main)
- `Source/SiteSyncAR/SiteSyncAR.Build.cs` — added `AugmentedReality`, `ProceduralMeshComponent`, `GeometryCore`, `GeometryScriptingCore` modules; `AppleARKit` iOS-only
- `Source/SiteSyncAR/Public/ARMeshBlueprintLibrary.h` — C++ shim: `GetAllARMeshGeometries()` → `TArray<UARMeshGeometry*>`; `GetARMeshData(geometry, OutVertices, OutIndices)` → `bool`
- `Source/SiteSyncAR/Private/ARMeshBlueprintLibrary.cpp` — `GetAllARMeshGeometries` + `#if !PLATFORM_IOS` stub for `GetARMeshData`
- `Source/SiteSyncAR/Private/ARMeshBlueprintLibrary_iOS.mm` — iOS impl: matches `ARMeshAnchor` by `UniqueId` from `ARSession.currentFrame`, reads `geometry.vertices` + `geometry.faces`, converts ARKit RH-Y-up meters → UE LH-Z-up cm via `FAppleARKitConversion::ToUEScale()` and `FVector(-z, x, y)` mapping

**C++ compile status:** ✅ Verified Mac iOS toolchain 2026-04-21 (post UE 5.6.1 upgrade + `Apple_SDK.json` MaxVersion patch). PC compiles clean via VS2022. The 2026-04-24 fix in `ARMeshBlueprintLibrary_iOS.mm` applies `simd_mul(FoundAnchor.transform, vert)` so anchor-local vertices land in world space — required for any per-vertex `ARAnchor` data. See decisions.md 2026-04-24.

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
- Status: **✅ Compiled, wired, gate-cleared on device 2026-04-24** (cyan mesh wraps real surfaces correctly)
- Component: `TerrainMesh` (ProceduralMeshComponent)
- Variables: `UpdateInterval` (Float, default 0.2), `MeshTimerHandle` (Timer Handle), `DebugMaterial` (Material Interface)
- BeginPlay: `Event BeginPlay → StartARSession(DA_SiteSyncARConfig) → SetTimerByEvent(RebuildMesh, UpdateInterval, looping) → SET MeshTimerHandle`
- RebuildMesh: `ClearAllMeshSections(TerrainMesh) → GetAllARMeshGeometries → ForEachLoop → GetARMeshData → Branch[true] → CreateMeshSection_LinearColor (CreateCollision=false, Normals=empty) → SetMaterial(M_LiDARDebug)`

### Shim API Reference (use these nodes in BP_LiDARMeshManager)
```
GetAllARMeshGeometries()  → TArray<UARMeshGeometry*>   // replaces Get All Geometries
GetARMeshData(Geometry, OutVertices, OutIndices) → bool // vertices + indices in one call, no separate normals
```
Note: `GetARMeshData` has no `OutNormals` — pass empty array to Normals pin on Create Mesh Section.

### Build & Deploy Pipeline (Mac, proven 2026-04-21)

PC-driven SSH remote-build is NOT used; Mac builds directly. Personal Team signing via Team ID `PD29S4YQ4P`, Bundle `com.RussoCompany.SiteSyncAR`.

```bash
# 1. Toolchain patch (idempotent)
bash scripts/patch-ue56-xcode26.sh

# 2. UBT iOS Development
Engine/Build/BatchFiles/Mac/RunUBT.sh SiteSyncAR IOS Development \
  -project=~/Developer/SiteSync-AR/SiteSyncAR/SiteSyncAR.uproject

# 3. Cook content
UnrealEditor -run=Cook -targetplatform=IOS \
  -project=~/Developer/SiteSync-AR/SiteSyncAR/SiteSyncAR.uproject

# 4. UAT stage
Engine/Build/BatchFiles/RunUAT.sh BuildCookRun \
  -project=~/Developer/SiteSync-AR/SiteSyncAR/SiteSyncAR.uproject \
  -platform=IOS -configuration=Development \
  -stage -skipbuild -skipcook -archive

# 5. Wired install
xcrun devicectl device install app --device <UDID> \
  ~/Developer/SiteSync-AR/SiteSyncAR/Saved/StagedBuilds/IOS/SiteSyncAR.app
```

Staged app path: `SiteSyncAR/Saved/StagedBuilds/IOS/SiteSyncAR.app`. Find device UDID with `xcrun devicectl list devices`.

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
- `Input/IA_TapPlace.uasset` — **Digital (bool)** IA, single `Pressed` trigger. Was Axis2D in early Node 1.3 work but `Pressed` trigger LATCHES on Axis2D + iOS touch (Touch1.X/Y don't reset to (0,0) on finger release, so magnitude stays > actuation threshold and Pressed never re-arms — only first tap fires). Tap screen coords come from `GetInputTouchState(Touch1)` separately, not from this IA's ActionValue, so changing Value Type to Digital is graph-safe.
- `Input/IMC_Placement.uasset` — maps `Touch 1` → `IA_TapPlace`
- `Blueprints/BP_ARPlayerController.uasset` — owns `IMC_Placement` via EnhancedInputLocalPlayerSubsystem on BeginPlay; `IA_TapPlace (Started)` drives the state machine: Is Valid(ActiveFoundation) → reset, else tap state. Calls `GetWorldLocationFromTap(ScreenLocation) → WorldLocation, Hit` via `LineTraceTrackedObjects`. Vars: `bHasFirstTap`, `FirstTapLocation`, `MarkerA`, `MarkerB`, `ActiveFoundation`, `TapMarkerClass`, `FoundationClass`, `PlacementContext`.
- `Blueprints/BP_ARGameMode.uasset` — `GameModeBase` subclass with `PlayerControllerClass = BP_ARPlayerController`
- `SiteSyncAR/Config/DefaultEngine.ini` — `GlobalDefaultGameMode = /Game/Blueprints/BP_ARGameMode.BP_ARGameMode_C`

### Editor-test limitation (known)
PIE can't exercise this — `Start AR Session` returns false on Windows, so `LineTraceTrackedObjects` has nothing to hit. Validation happens on device only (wired Mac → iPhone 16 Pro).

### Gate to Node 1.4
Tap twice in an AR session on device → orange translucent slab appears spanning the two taps, rotated to the tap vector, sitting on the LiDAR-tracked surface. Third tap removes it. 60fps maintained.

---

## What Was Built in Node 1.4

Volumetric cut-and-fill cubic yardage output. Computes earthwork volumes between the LiDAR-tracked terrain and the slab subgrade plane, displays results in an AR-overlay HUD.

### C++ (committed to origin/main)

`Source/SiteSyncAR/Public/ARMeshBlueprintLibrary.h` + `Private/ARMeshBlueprintLibrary.cpp` added two `BlueprintCallable` UFUNCTIONs:

- **`CalculateCutFillVolumes(TerrainMesh, FoundationActor, OutCutCubicYards, OutFillCubicYards) → bool`** — earthwork math. Reference plane = slab BOTTOM face = `FoundationActor.GetActorLocation().Z − (SlabThicknessCm / 2)`. Iterates `ProceduralMeshComponent` triangles, projects each into the slab's local XY frame, clips to `[-Length/2, +Length/2] × [-Width/2, +Width/2]`, signed-Z-integrates against the plane. Cut = volume above. Fill = empty space below + above-plane terrain that's UNDER the slab (subgrade-fill convention). Unit conversion: cm³ → yd³ via `÷ 764554.858` (`91.44³`, NOT `÷27` — that's ft³→yd³). FoundationActor scale is interpreted as full extent in meters per `InitFromCorners` convention; multiplied by 100 internally for cm. Returns false on null inputs or zero scale on any axis. Logs `"CalculateCutFillVolumes: scale=(...)m"` at `Warning` level so it appears in default-verbosity device logs.

- **`InitFoundationFromCorners(FoundationActor, CornerA, CornerB, WidthCm, ThicknessCm) → bool`** — slab placement in C++, added 2026-05-11 (commit `52b905e`). Replaces the fragile BP exec chain in `BP_Foundation.InitFromCorners` that was leaving slab scale at (1,1,1). Sets actor location (midpoint of A→B), yaw (`atan2(deltaY, deltaX)`), and `SetActorScale3D` in meters. `WidthCm` clamped to [50, 5000], `ThicknessCm` to [5, 50]. Logs `"InitFoundationFromCorners: A=... B=... yaw=... L=... W=... T=... scale_m=(...)"` at `Warning` level. BP_ARPlayerController calls this after `SpawnActor BP_Foundation` instead of running the BP `InitFromCorners` math chain. The BP function still exists but is dead code; safe to remove in a future cleanup.

### BP changes — `BP_ARPlayerController.uasset`

- **Tap state machine** on `IA_TapPlace.Started` exec (Pressed trigger):
  - `IsValid(ActiveFoundation)` → valid: `ResetPlacement` function (destroys + nulls all 3 actor refs + clears `bHasFirstTap`). NotValid: continue.
  - `GetWorldLocationFromTap(ScreenLocation)` → traces via `LineTraceTrackedObjects`. (C++ raycast against raw `ARMeshGeometry` triangles is deferred — current BP plane-trace works for indoor floor demo.)
  - `Branch(bHit)` → true: `Branch(bHasFirstTap)` → false: spawn MarkerA + SET FirstTapLocation + SET bHasFirstTap=true. true: spawn MarkerB + spawn BP_Foundation + call `InitFoundationFromCorners(NewFoundation, FirstTapLocation, SecondTapLocation, WidthCm=100, ThicknessCm=10)` + SET ActiveFoundation + SET bHasFirstTap=false.
  - `InitFoundationFromCorners.WidthCm` pin = **100cm** (NOT 500cm). Indoor taps at ~30–100cm spacing produce a slab proportional to marker spacing instead of a 5m strip overflowing the room. Bump back to 500 for outdoor construction-site testing.
- **BeginPlay** on the controller wires `IMC_Placement` via `EnhancedInputLocalPlayerSubsystem.AddMappingContext`, then `CreateWidget(WBP_VolumeReadout) → SET HUDWidget → AddToViewport`. No Event Tick override (stripped in v18 cleanup).

### BP changes — `BP_Foundation.uasset`

- **Class Defaults:** `SlabThicknessCm = 10`, `WidthCm = 500`, `CornerA = CornerB = (0,0,0)`, `CachedReadout = None`. `Initial Life Span = 0` (immortal). `SlabMesh.Mobility = Movable`, `SlabMesh.Simulate Physics = false`, `SlabMesh.Collision Presets = NoCollision`.
- **BeginPlay chain:** `Cast to BP_ARPlayerController → SET CachedReadout = (cast).HUDWidget → Delay 0.1 → recalc loop body`. Loop body: `GetActorOfClass(BP_LiDARMeshManager) → GetComponentByClass(ProceduralMeshComponent) → CalculateCutFillVolumes(TerrainMesh, Self) → SET CutCubicYards / SET FillCubicYards → IsValid(CachedReadout) → SetCutText / SetFillText → loop back to Delay`. Delay duration is **0.1s (10Hz)** — empirically validated at v17 for smooth HUD updates without measurable perf cost; the `÷ 764554.858` math in `CalculateCutFillVolumes` is a single mesh-iteration pass and runs comfortably under one frame. Earlier doc said 1.0s; reality has been 0.1s since at least v15. The Cast Failed branch wires to a `BP_Foundation Cast FAILED` PrintString (silence target for v19 cleanup — log-only).
- **`InitFromCorners(A, B)` function** — still exists in BP but is dead code as of v17. BP_ARPlayerController calls the C++ `InitFoundationFromCorners` instead. Safe to delete in a future cleanup.
- **`ResetPlacement` function** — three IsValid-guarded blocks (ActiveFoundation, MarkerA, MarkerB), each: `IsValid → DestroyActor → SET to None → next block`. Final node: `SET bHasFirstTap = false`. v17 fix wired `SET MarkerB.then → SET bHasFirstTap.execute` (via two reroute knots) so the "MarkerB was valid" reset path converges on `bHasFirstTap=false` instead of leaving the flag latched. Both valid- and not-valid paths through Block C now end at the bHasFirstTap clear.

### BP changes — `WBP_VolumeReadout.uasset`

UMG widget that displays Cut and Fill values in cubic yards over the AR view. Two TextBlocks (`CutText`, `FillText`) inside a `SafeZone` wrapping a `VerticalBox` (auto-respects iPhone notch). Font size 28pt for outdoor readability. `SetCutText(float)` and `SetFillText(float)` are `BlueprintCallable` graph functions that update the text via the push pattern (NOT data binding — see decisions.md 2026-04-29 on the binding-vs-push tradeoff). BP_Foundation calls these once per recalc tick.

### IA_TapPlace + IMC_Placement config

- `IA_TapPlace.uasset`: **Value Type = Digital (bool)**, single `Pressed` trigger. NOT `Axis2D` — see decisions.md 2026-04-29 on the iOS Axis2D + Pressed latching bug. Tap screen coords come from `GetInputTouchState(Touch1)` separately, not from this IA's ActionValue.
- `IMC_Placement.uasset`: maps `Touch 1 → IA_TapPlace` with no per-mapping trigger (uses the IA-level Pressed trigger).

### Diagnostic cleanup (v18, commit `838abf6`, 2026-05-11)

After v17 validated the full chain end-to-end on device, the on-screen HUD was cluttered with diagnostic text accumulated during the v15–v17 bug hunt. v18 stripped them:

- Deleted: `BP_ARPlayerController` Event Tick → ExecutionSequence → Branch → GetInputTouchState → PrintString "Touch ACTIVE" probe (per-frame iOS-touch-delivery diagnostic, no longer needed).
- Routed to log-only (`bPrintToScreen → false`, `bPrintToLog` kept `true`): seven `IA_TapPlace`-path PrintStrings (`Tap Fired`, `Path: Reset`, `Path: NotValid`, `bHit OK`, `bHit MISS`, `Path: MarkerA (HasFirstTap=false)`, `Path: MarkerB (HasFirstTap=true)`).
- Deleted: BP_Foundation BeginPlay-chain `"Hello"` PrintString between `SET CachedReadout` and `Delay`.

### Known minor diagnostics still on-screen

Two `BP_Foundation` PrintStrings remain visible on device, both with `InString="Hello"` (default — the MCP `params` arg that should have set custom text didn't take when they were created earlier this session):

- **Recalc tick** — fires at 1Hz from inside the recalc loop body. Tells us the loop is running.
- **Cast FAILED** — fires only if `Cast to BP_ARPlayerController` fails on BeginPlay. Rare in practice.

Both are functional diagnostics worth keeping in v18 to confirm v17 fixes hold. Silence them (or rename `InString`) in a future cleanup if they become noise.

### Device validation milestones

- **v16 (2026-05-11):** Mac added `InitFoundationFromCorners` C++. Device log shows `InitFoundationFromCorners: yaw=82.8° L=50.0cm W=500.0cm T=10.0cm scale_m=(0.500,5.000,0.100)` and `CalculateCutFillVolumes: scale=(0.500,5.000,0.100)m` with non-zero cut/fill. **1m³ phantom slab bug killed.**
- **v17 (2026-05-11):** WidthCm pin 500→100 + ResetPlacement convergence wire. Indoor 30–100cm taps produce a slab proportional to marker spacing. Third tap clears slab; fourth tap fires fresh first-tap branch with `bHasFirstTap=false`. `CalculateCutFillVolumes` call count drops to zero after reset (foundation really destroyed, not orphaned ref). **Third-tap reset loop killed.** Full Node 1.4 chain device-validated end-to-end.
- **v18 (2026-05-11):** HUD spam cleanup pushed (commit `838abf6`). Pending visual verification on device that the WBP_VolumeReadout cut/fill numbers are the only on-screen text.

### Gate to Node 2.1

Indoor: 2 taps within ~30–100cm produce a flat slab proportional to tap distance (`L × 1m × 10cm`). Cut and fill cubic-yard values appear in the HUD widget. Third tap clears slab + markers. Fourth tap fires a fresh "first tap" branch (MarkerA spawn). Loop is breakable indefinitely without jetsam or crash. 60fps maintained.

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
[Claude Code]
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

**The C++ plugin auto-starts the TCP server on port 55557 when the editor loads — no manual start needed.** The server accepts multiple concurrent connections on that port, so a direct-TCP diagnostic script (e.g. `dev/mcp_client.py`) can run alongside the Claude Code MCP session without conflict.

**Claude Code config (primary):** `SiteSyncAR/.mcp.json` (next to `SiteSyncAR.uproject`) — `unrealMCP` server registered with `--directory Plugins/UnrealMCP/Python` so the same file works on PC and Mac. The path is relative to the launching cwd, which is the `SiteSyncAR/` UE project folder on both machines. After cloning or pulling for the first time, run `/mcp` inside Claude Code (or restart) so it picks up the server, and approve the project-scope server when prompted. Tools then surface as `mcp__unrealMCP__*`. **Do not** create local-scope (`-s local`) or user-scope (`-s user`) overrides — they shadow the project file and break portability; the project `.mcp.json` is the only source of truth.

**Prereq on both machines:** `uv` must be on PATH. PC: `C:\Users\jruss\.local\bin\uv.exe`. Mac: typically `~/.local/bin/uv` or via Homebrew.

**MCP verification prompt (paste into Claude Code):**
```
Using the unrealMCP server, spawn a static mesh cube actor at world location 
X=0, Y=0, Z=100. Name it "MCP_ConnectionTest". Confirm the actor name and location.
```

**Reliability tier (chongdashu/unreal-mcp is community-built, not Epic):**
- ✅ Solid: actor spawn/move/destroy, component property changes (material slot, scale, transform), level queries
- ⚠️ Hit-or-miss: Blueprint graph node-by-node rewiring with branches and struct-pin splits — fall back to manual editor work for non-trivial graphs
- ⚠️ MCP edits do NOT trigger `Save All`. After any structural edit (actor delete, BP compile, etc.) the change lives in the editor's in-memory world only. James must Ctrl+S in the editor before the change touches `.umap` / `.uasset` and the working tree.

**MCP plugin health & rebuild protocol — required reading.** Two failure modes have already burned a session each. Recognize them on sight:

1. **Stale plugin DLL.** UE5 loaded the plugin's compiled DLL once on editor start and won't reload it. If the source has commits the DLL doesn't, MCP write commands silently misbehave (most commonly: `delete_actor` returns `Actor not found` for actors `get_actors_in_level` just enumerated, or new commands return `Unknown command`). Detect with:
   ```bash
   stat -c '%y' SiteSyncAR/Plugins/UnrealMCP/Binaries/Win64/UnrealEditor-UnrealMCP.dll
   git -C . log -1 --format=%ci -- SiteSyncAR/Plugins/UnrealMCP/Source/
   ```
   If the DLL mtime is older than the source, rebuild.

2. **Unwired dispatcher.** Adding a new `*_Commands.cpp` handler is **two edits**, not one: the per-class `HandleCommand` switch AND the dispatcher in `UnrealMCPBridge.cpp::ExecuteCommand`. The dispatcher is the gate. If a command name isn't listed there, the bridge falls through to the `else` clause and replies `Unknown command` even though the handler is compiled in and the UTF-16 string is in the DLL. Symptom shortcut: any MCP call returning `"Unknown command: <name>"` while the Python `node_tools.py` (or equivalent) advertises that name → check `UnrealMCPBridge.cpp` for the missing branch entry.

**Rebuild command (PC).** UE editor must be closed first (it holds the DLL locked). Runs in ~10s on a clean tree:
```bash
"C:\UE_5.6\Engine\Build\BatchFiles\Build.bat" SiteSyncAREditor Win64 Development \
  -project="C:\Dev\SiteSync-AR\SiteSyncAR\SiteSyncAR.uproject" -waitmutex
```

**Mac equivalent** (when working on Mac after pulling C++ plugin source changes):
```bash
"/Users/Shared/Epic Games/UE_5.6/Engine/Build/BatchFiles/Mac/Build.sh" SiteSyncAREditor Mac Development \
  -project="$HOME/Developer/SiteSync-AR/SiteSyncAR/SiteSyncAR.uproject"
```

**Five-second sanity probe.** Bypasses MCP/stdio entirely — talks straight to the C++ TCP server on 55557. Works without a Claude Code session having unrealMCP loaded; only requires the UE editor to be open.
```bash
python SiteSyncAR/Plugins/UnrealMCP/Python/scripts/probe_post_rebuild.py
```
Expected output: `delete_blueprint_node` returns `"Blueprint not found: ..."` (handler reached), and `spawn_actor → find_actors_by_name → delete_actor` round-trips cleanly. A "Unknown command" or empty `find_actors_by_name` after spawn means one of the two failure modes above is back.

**Wire protocol** (for writing further diagnostic scripts): connect to `127.0.0.1:55557`, send `{"type": "<command>", "params": {...}}` as raw UTF-8 (no newline), read until JSON parses, server closes connection after each command. Reference impl: `Plugins/UnrealMCP/Python/unreal_mcp_server.py` `UnrealConnection.send_command`.

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
| MCP connection | UE5 Output Log (filter `MCP`) — C++ plugin auto-starts TCP server on port 55557; if missing, restart the editor |
| Datasmith import | Datasmith Import Log in Content Browser |
| iOS deployment | Xcode → Devices & Simulators console |
| C++ compile errors | Visual Studio 2022 Output window |

---

## What Was Built in Issue B Fix (2026-04-28)

Issue B was the "virtual content drifts with the camera" bug: cyan LiDAR mesh, yellow tap markers, and the orange foundation slab all appeared to follow the phone as the user walked. Bisection (commits `cbe3c93` null-pawn test, `7037341` static cube probe) proved virtual content was actually anchored correctly in UE world space — the **camera** was static, so passthrough shifted while virtual stayed put.

**Root cause:** project had no Pawn whose camera tracked the ARKit device pose. UE rendered against a default static world camera while ARKit correctly tracked the device.

**Fix (commit `66b70c9`):** added `BP_ARPawn` (Pawn parent class) with components `DefaultSceneRoot → ARCamera (UCameraComponent, bLockToHmd=true)`. Auto Possess Player 0. Wired `BP_ARGameMode.DefaultPawnClass = BP_ARPawn`. PlayerStart in `SiteSync.umap` is at world (0,0,0) so pawn spawns coincident with statically-placed actors.

**Companion fix (commit `0caf30c`):** iOS shim `GetARMeshData` applies `AlignmentTransform * TrackingToWorldTransform` to vertex positions, mirroring the engine's own `AppleARKitSystem.cpp:1457` chain. Required because those transforms are non-identity once a real AR pawn drives the camera; without this, mesh vertices end up in the wrong frame even with the pawn in place. **Do not remove** — re-enabling the bug.

**Cleanup (commit `b416ba2`):** stripped Tick Print String diagnostic from `BP_LiDARMeshManager`; removed `TestCube_Anchored` from `SiteSync.umap`.

---

## Bugs & Workflow Gotchas — Index

Consolidated quick-reference index of bugs / workflow snags hit during development. Each entry links to its full write-up in `memory/decisions.md` (which has cause, symptom, fix, and "how to apply" detail). When a NEW bug surfaces in a future session, log it to `decisions.md` first, then add a one-line pointer here. Format: **`YYYY-MM-DD — short headline`** — one-line "what bites you / how to recognize it" hook.

### UE5 Editor / Save Discipline

- **2026-05-19 — Static Mesh `Apply Changes` doesn't save to disk** — Build Settings changes (Build Scale, etc.) rebuild in-memory only; you MUST Cmd+S with the Static Mesh tab focused, not just global Save All. Verify with `stat` on the .uasset mtime.
- **2026-05-19 — BP pin "weird save" — re-type to clear** — pin literal changes sometimes don't propagate to cooked bytecode even after Compile + Save + cook. Re-typing the same value, re-compiling, re-saving clears it. Suspected dirty-tracking miss.
- **2026-05-14 — Mac editor `Save Current Level As` crashes from stale Desktop paths** — `LongPackageNameToFilename` errors when old `/Users/.../Desktop/Github/Xcode/...` paths leak into UE config. `grep -rl "Desktop/Github"` in `~/Library/Application Support/Epic` and `Saved/Config`, sed-replace with current canonical path.
- **2026-05-14 — `SET <object var>` needs BOTH exec wire AND data-input wire** — UE doesn't warn at compile time when the data input is unconnected; it silently sets the var to None. Symptom: state-machine path runs but state vars retain default values.

### Blueprint / Graph

- **2026-05-14 — chongdashu MCP `create_blueprint` doesn't honor non-Actor parent classes** — produces shells without expected Class Defaults (GameModeBase, PlayerController, Pawn, UserWidget). Fall back to Content Browser → Duplicate of a working asset with the right parent.
- **2026-05-11 — EnhancedInput `IA_TapPlace.Started` doesn't re-fire after state transitions on iOS** — abandoned for Tick rising-edge poll on `GetInputTouchState`. IA + IMC assets retained as dead code.
- **2026-05-20 — `BIMMesh` is the root component of `BP_BIMOverlay`; `SetActorScale3D` overwrites its `RelativeScale3D`** — component-level scale is a dead lever for the BIM overlay. Only Build Scale (mesh) + actor scale (PlaceBIMByCornerForward L/W/H pins) actually move the size.

### C++ / BlueprintCallable

- **2026-05-20 — `PlaceBIMByCornerForward` silently clamped L/W/H to min 100.0f and logged only the post-clamp value** — cost ~2 hours chasing a phantom "BP save bug." Fixed: clamp min lowered to 10. **Durable lesson: any BlueprintCallable that clamps inputs must log raw→clamped, and clamp ranges must be documented in the `.h` comment.** See `decisions.md 2026-05-20`.

### Asset Import

- **2026-05-21 — UE 5.6 has no working Datasmith GUI import** — no toolbar button; Content Browser Import greys out `.udatasmith`; drag-drop fails `Unknown extension 'udatasmith'`. Import via the `DatasmithSceneElement` Python API — headless `UnrealEditor-Cmd -run=pythonscript` or in-editor `py`. Reusable script: `dev/import_datasmith.py`. See `decisions.md 2026-05-21`.
- **2026-05-19/20 — glTF imports from Fab arrive ~100× oversized (NOT 10×)** — correct Build Scale is `(0.01, 0.01, 0.01)`, not `0.1`. A 7.7m house imports as 774m. Apply Build Scale 0.01 + **Cmd+S with the Static Mesh tab focused** (Apply Changes alone doesn't persist).

### Rendering / Lighting

- **2026-05-20 — imported glTF asset renders pure black on device** — its materials are **Lit** and the AR level (`SiteSync_BIMTest`) has no lights. The project's own debug/overlay materials are Unlit so they always render; imported Lit materials need Movable Directional + Sky lights in the level. See `decisions.md 2026-05-20`.

### iOS Deployment & Signing

- **2026-05-18 — Xcode loses Apple ID between sessions** — manifests mid-cook-pipeline as `No Accounts / No profiles`. Re-add Apple ID in Xcode → Settings → Accounts. Cook+deploy works again.
- **2026-05-12 — DatasmithRuntime is desktop-only (Win/Mac); iOS not supported through 5.7** — all Datasmith work must happen at editor cook time. No path to load `.udatasmith` at runtime on iPhone in any UE 5.x version. Don't relitigate.
- **2026-05-14 — Cook `-map=...` does NOT update boot map** — `-map` only tells the cooker WHICH maps to include. Boot map is `DefaultEngine.ini` → `[/Script/EngineSettings.GameMapsSettings] → GameDefaultMap`. Edit separately.

### MCP / Diagnostic Tooling

- **2026-05-11 — `probe_post_rebuild.py` uses fixed actor name** — spawning `MCP_FixCheck_01` in a polling loop causes a fatal "cannot generate unique name" assertion if the prior probe's delete is still pending. Don't `while`-loop the probe.
- **MCP plugin DLL/dispatcher hygiene** — see "MCP plugin health & rebuild protocol" section above. Two recurring failure modes: stale DLL after source-only updates, and missing dispatcher entry in `UnrealMCPBridge.cpp::ExecuteCommand` when adding new commands.

### Project Context

- **2026-05-19 — Idaho Technology Council demo postponed indefinitely** — all "ship by Tuesday" pressure removed; current `main` state retained as demo-ready baseline. See `decisions.md 2026-05-19` entry.
- **2026-05-18 — Phase 2 supports Site Scale (1:1, production) vs Model Scale (~0.3×, indoor demo)** — controlled by L/W/H pins on `PlaceBIMByCornerForward`. Same primitive, different use case. Trimble Sitevision / Autodesk Construction Cloud have same dual-mode pattern.
- **2026-04-21 — No paid Apple Developer Program until James leaves retail** — Personal Team Apple ID + 7-day provisioning profiles. No TestFlight. Wired Mac→iPhone deploy only.

---

## Immediate Next Actions (current, 2026-05-21 — TestBuilding imported)

**Node 2.1 Datasmith import works end-to-end.** A real Rhino-authored BIM model — `TestBuilding`, a 6-box test building (6 m × 5 m × 3.4 m, one box per Floor / 4 walls / Roof, each on its own layer) — was modeled in Rhino 8, exported to `.udatasmith`, and imported into the project as 9 UE assets (6 Static Meshes + 2 materials + Datasmith Scene). Committed + pushed in `bdf4998`. Raw source in `BIM_Source/TestBuilding/`; imported assets in `Content/AR_SiteAnalysis/DatasmithAssets/TestBuilding/TestBuilding/`.

**Key fact for all future imports:** UE 5.6 has no working Datasmith GUI import (no toolbar button; Content Browser import + drag-drop both fail "Unknown extension"). Imports run through the `DatasmithSceneElement` Python API — reusable script `dev/import_datasmith.py`. `PythonScriptPlugin` is now enabled in `SiteSyncAR.uproject`. See `decisions.md 2026-05-21`.

### Node 2.1 status — `TestBuilding` wired into `BP_BIMOverlay` ✅

`BP_BIMOverlay.BIMMesh` now renders `SM_TestBuilding_Merged` — the 6 imported `TestBuilding` pieces (Floor / 4 walls / Roof) combined via Merge Actors with **Pivot Point at Zero**, so the merged mesh origin sits at the building corner. Committed `2703f54`. The two-tap `PlaceBIMByCornerForward` flow now drops the real BIM model. Editor-wiring gate met.

**Next for Node 2.1 — iOS device validation (Mac):** cook + deploy `SiteSync_BIMTest`, confirm `TestBuilding` places via two taps on the iPhone. `PlaceBIMByCornerForward` L/W/H pins are at `30/30/30` (Model Scale — the 6 m building places at ~1.8 m dollhouse); decide 30 vs 100 during the device test. Deploy pipeline: `docs/node-2.1-a3-lighting-handoff.md`.

### Backlog (lower priority)
1. ✅ **C++ logging hygiene pass** — done 2026-05-21, commit `0c38146` (deployed to device). `PlaceBIMByCornerForward` / `InitFoundationFromCorners` / `CalculateCutFillVolumes` now log raw→clamped values; clamp ranges documented in `ARMeshBlueprintLibrary.h` doc comments.
2. ✅ **BP cleanup** on `BP_ARPlayerController_BIM` — done 2026-05-21, commit `a11f754`. Dead `AddMappingContext` chain + `PlacementContext` variable removed from BeginPlay; "Spawn Foundations" comment renamed "Spawn Markers + BIM Overlay".
3. ✅ **Cook-size check** — done 2026-05-21. Staged `.app` = 574 MB: 339 MB Development binary (code, not content) + 234 MB cooked. Fab glTF house = 91 MB cooked (4K textures). Not blocking wired deploy. No texture pass on the Fab house — it is superseded by the `TestBuilding` Datasmith model; apply texture/LOD discipline to `TestBuilding` instead.
4. **Tier B** — `SiteSync_Menu.umap` two-button launcher for Phase 1 (cut/fill) + Phase 2 (BIM). Part 1 walkthrough (create level + `WBP_MainMenu` layout) delivered 2026-05-21; Parts 2–3 pending.

### Session 2026-05-14 → 2026-05-20 summary (demo-prep arc — all complete)

Demo prep for the Idaho Technology Council event (which **postponed indefinitely** mid-session 2026-05-19 — see Demo Snapshot section + `decisions.md 2026-05-19`). All committed + pushed to `origin/main`:

- **A1** (`8fb93d3`) — BIM placeholder shrunk to 3×3×2.5m
- **A2** (`b7abe86`) — `WBP_BIMPlacementHUD` three-state placement HUD
- **Tier B wireframe** (`90471e8`) — UV-edge wireframe glow on `M_BIMOverlay`
- **A3** (`b5747df`) — real house BIM model: imported Fab "Free Small Old House" (Jimbogies, CC BY 4.0), Model Scale, C++ clamp fix, `SiteSync_BIMTest` lighting. Device-validated 2026-05-20 — house places via two taps at dollhouse scale (~2.3 m), fully lit with realistic textures.
- **docs** (`f367aa6`) — session bug write-ups, A3 handoff doc, demo-postponed reframe

The A3 arc surfaced and fixed four bugs — all in the Bugs Index + `decisions.md` (2026-05-19/20): glTF 100× oversize (Build Scale 0.01), the `PlaceBIMByCornerForward` min-100 C++ clamp, the BIMMesh root-component scale clobber, and the black-house lighting gap. Final scale chain: `774m glTF × 0.01 Build Scale × 0.3 actor = 2.3m dollhouse`.

**Phase 1 is closed.** v20 was the final cut/fill release. Phase 1 docs preserved below for reference / for if we revive the cut/fill path in a multi-mode menu (Tier B).

### Phase 1 (Node 1.4) recap

v20 device-validated end-to-end (2026-05-11, commit `af4c2ec`). All four success patterns confirmed in device log:

- `Tap Fired` = 16 events across 5 reset cycles, no post-reset death (Bug A fixed via Tick rising-edge bypassing EnhancedInput)
- `RaycastTerrainFromScreen: hit at` = 10 events, Z-cluster -111.3 to -113.9 cm (2.6 cm spread on the floor mesh — Bug 3 fixed)
- `InitFoundationFromCorners` = 5 spawns, slab lengths 67-304 cm proportional to taps, W=100 cm and T=10 cm consistent
- `Path: MarkerA (HasFirstTap=false)` correctly follows every `Path: Reset` — reset/replace loop is breakable indefinitely

### Node 1.4 future polish (deferred to Node 1.5, not blocking Node 2.1)

- **Datum offset slider** in WBP_VolumeReadout. Shifts `slabBottomZ_cm` by ±5m for as-built grade vs design subgrade comparison.
- **Width / thickness sliders** on the HUD. Right now both are baked at the BP node pin (W=100 cm, T=10 cm). Real construction estimating needs runtime adjustment.
- **Spot elevation / slope readouts** at marker positions. Z values are already captured (visible in `InitFoundationFromCorners` log lines) — just need WBP widget fields.
- **Delete dead BP code**: `BP_Foundation.InitFromCorners` (replaced by C++ `InitFoundationFromCorners` in commit `52b905e`), `IA_TapPlace` + `IMC_Placement` assets (replaced by Tick rising-edge in v20).
- **AABB cull before Möller-Trumbore** in `RaycastTerrainFromScreen` if scan size grows past current ~150k tris. Currently <10ms per tap, acceptable.
- **Triangle skip cap** in `CalculateCutFillVolumes` — currently hits the 250k cap routinely, skipping ~140k tris per pass. Add per-section AABB filter to limit work to triangles inside the slab footprint.
- **Surface classification visualization** — ARKit anchors are tagged (floor/wall/ceiling/table). Could color-code the cyan mesh to make the scan more readable.

### Volume math spec reference (implemented in C++ `CalculateCutFillVolumes`)

- Reference plane = slab BOTTOM face = `ActiveFoundation.GetActorLocation().Z − (SlabThicknessCm / 2)` (subgrade convention used by AGTEK / Trimble / field graders).
- Cut = terrain volume above the plane, clipped to slab XY footprint.
- Fill = empty space between the plane and terrain below within the same footprint.
- Footprint clipping in slab-local space; slab yaw baked into the local frame.
- Unit conversion: `cm³ ÷ 764554.858 → yd³` (NOT `÷27`).

### Node 2.1 — Datasmith ingestion pipeline (IN PROGRESS, started 2026-05-12)

Phase 2 starts here. BIM model overlay in AR for clash detection. The path is **cooked-Datasmith only** — there is no runtime alternative on iOS. `DatasmithRuntime` is "Beta" in 5.6's Plugins window, but Beta refers to API maturity on its supported desktop platforms (Windows + macOS only). **iOS and Android are not supported targets in any Datasmith version through 5.7** (per Epic's Datasmith Supported Platforms doc, confirmed via 2026-05-12 research). Earlier doc claims that DatasmithRuntime was "viable for Node 2.3" were wrong — there is no path to load `.udatasmith` files on the iPhone, ever. All Datasmith work happens at editor cook time.

**Tradeoff accepted:** every BIM swap requires re-cook + re-deploy from Mac (~5–10min). The runtime alternative does not exist on this platform.

**Cook-time gotchas to plan for** (per 2026-05-12 research, not yet hit in this project):
- Datasmith-imported materials often use Translucent + complex shading models that mobile FL5 won't render correctly. Re-parent to mobile-compatible material instances at import time.
- Datasmith-imported meshes default to Nanite-eligible in 5.5+. Disable Nanite per-mesh and generate auto-LODs. (Project-level Nanite is already disabled in `Config/IOS/IOSEngine.ini` — just verify per-mesh settings don't override.)
- First iOS cook of a real BIM model can be **hours**, not minutes (DDC + shader-permutation compile). Plan accordingly.
- Real Revit MEP models are 5–50M tris; iPhone 16 Pro AR wants <500k visible. Expect to need a **Dataprep recipe** for decimation + material consolidation between raw `.udatasmith` and shipped iOS content.
- The **Datasmith Twinmotion Content** plugin for UE 5.6 must be verified published on Fab before relying on Twinmotion as an archviz front-end — historically it lags the engine release by several months. If absent, materials from Twinmotion-exported `.udatasmith` arrive broken.

#### Plugins enabled (commit pending, 2026-05-12)

Added to `SiteSyncAR.uproject`:
- `DatasmithImporter` (editor-time importer + runtime translators)
- `DatasmithContent` (runtime support so cooked Datasmith assets work on device)
- `DatasmithFBXImporter` (fallback path for Revit FBX exports if Direct Link is unavailable)

No `SupportedTargetPlatforms` restriction — Datasmith plugin descriptors leave that field empty (all platforms), and UE's Editor/Runtime module typing already excludes editor modules from iOS cook automatically. Restricting in the project descriptor produces a `LogGameProjectGeneration` warning every editor load and offers no real isolation benefit.

`DatasmithCADImporter` and `DatasmithC4DImporter` deliberately NOT enabled — Revit/Rhino don't need them and they bloat the editor load.

#### Folder convention

`Content/AR_SiteAnalysis/DatasmithAssets/` — anchor with `.gitkeep`. All imported BIM `.uasset` content goes under here, one subfolder per source model (e.g. `DatasmithAssets/SampleOffice/`, `DatasmithAssets/SiteX/`). Datasmith importer creates `Geometries/`, `Materials/`, `Textures/` subfolders automatically per import.

`BIM_Source/` (repo root, NOT under `Content/`) — raw BIM source files: Rhino `.3dm` models and exported `.udatasmith` bundles, one subfolder per model. Kept outside `Content/` so they are never cooked into the iOS `.ipa`; tracked via Git LFS (`.3dm`, `.udatasmith`, `.udsmesh` per `.gitattributes`) so re-imports/re-cooks are reproducible from either machine.

#### Source app coverage

| Source | Path | Status |
|---|---|---|
| Revit | Datasmith Direct Link or `.udatasmith` export via Datasmith Revit Exporter (free Epic plugin) | Untested in this project |
| Rhino | Datasmith Direct Link or `.udatasmith` export via Datasmith Rhino Exporter (free Epic plugin) | Untested in this project |
| Revit FBX fallback | Revit → FBX → Datasmith FBX Importer | Available, lossy on materials |

James to confirm which source app he'll use for the first BIM ingest before we shape the importer config.

#### LOD strategy (Node 2.1 baseline)

Rely on UE5 auto-LOD on import (Datasmith importer's "Generate Lightmap UVs" + "Auto Generate Collisions" settings + per-static-mesh LOD0/1/2 chain). Hand-tuned HLODs deferred to Node 2.3 once we know the actual tri count of a real BIM model.

**Mobile target:** keep total cooked Datasmith content under 200MB per model so the .ipa stays manageable for Personal Team weekly re-deploy. Anything over needs LOD intervention before ingest.

#### Node 2.1 assets + key conventions

**C++ placement primitive — `UARMeshBlueprintLibrary::PlaceBIMByCornerForward`** — sets the BIM actor's location to CornerWorld, yaw to `atan2(ForwardY−CornerY, ForwardX−CornerX)`, scale to `(L/100, W/100, H/100)` meters. Mirrors the proven `InitFoundationFromCorners` pattern; logs raw→clamped values at Warning level (clamp ranges documented in the `.h` doc comment). Survey origin convention: **Tap A = building corner, Tap B = +X axis baseline shot** — Trimble Siteworks / AGTEK practice. BP_Foundation in Phase 1 deliberately uses center+edge instead — different semantic.

**Corner-at-origin pivot convention** — the BIM mesh's own origin must sit at the building corner so CornerWorld matches the tapped point. `SM_TestBuilding_Merged` satisfies this natively (Merge Actors run with Pivot Point at Zero). `BIMMesh` is the **root** component of `BP_BIMOverlay` so it carries no relative offset; `SetActorScale3D` (called by `PlaceBIMByCornerForward`) clobbers any component-level scale — component scale is a dead lever, only the mesh's Build Scale and the actor scale move size.

**Phase 2 BP assets (all on `main`):**
- `BP_BIMOverlay` — Actor; `BIMMesh` root StaticMeshComponent = `SM_TestBuilding_Merged` (commit `2703f54`). Earlier placeholders (engine cube → Fab glTF house) are superseded.
- `BP_ARPlayerController_BIM` — duplicated from BP_ARPlayerController v20, retargeted to BIM types (`BIMOverlayClass`, `ActiveBIM`, `MarkerCorner/Forward`, `CornerLocation`). Tap state machine + `ResetPlacement` 3-block IsValid pattern intact; spawn-tap branch calls `PlaceBIMByCornerForward`.
- `BP_ARGameMode_BIM` — DefaultPawnClass = `BP_ARPawn`, PlayerControllerClass = `BP_ARPlayerController_BIM`.
- `SiteSync_BIMTest.umap` — PlayerStart (0,0,0) + `BP_LiDARMeshManager` + Directional/Sky lights (A3 lighting fix) + GameMode override = `BP_ARGameMode_BIM`.
- `WBP_BIMPlacementHUD` — three-state placement HUD (A2, commit `b7abe86`).
- `M_BIMOverlay` — translucent orange unlit material with UV-edge wireframe glow (Tier B). Currently unused — `SM_TestBuilding_Merged` kept its plain white import material.

> **Build history** — the 2026-05-12/13 PC scaffold and the Mac Part-4 fixes (BeginPlay AddMappingContext restore, `BP_ARGameMode_BIM` parent-class re-create, `SiteSync_BIMTest` creation) are all resolved and committed. The A1/A2/A3/Tier-B demo-prep arc is summarized under "Session 2026-05-14 → 2026-05-20" above. One reusable gotcha from that arc: chongdashu MCP `create_blueprint` does not reliably honor `parent_class` — verify the parent in Class Settings after any MCP-created BP, or duplicate an existing correctly-parented BP instead.

#### Gate to Node 2.2

A real BIM model (or Epic sample) imports cleanly via Datasmith on PC, gets cooked into the iOS package, and renders on iPhone 16 Pro inside the AR session at 60fps with manual placement. Cook size + load time documented. No Datasmith Direct Link / runtime path required at this gate.
