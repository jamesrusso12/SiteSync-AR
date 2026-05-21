# SiteSync AR

> On-site spatial intelligence for civil engineers — built in Unreal Engine 5.6 with ARKit and iPhone 16 Pro LiDAR.

SiteSync AR is an iOS augmented reality application for AEC (Architecture, Engineering, Construction) professionals. Phase 1 uses LiDAR-scanned terrain to calculate real-time cut-and-fill earthwork volumes on site. Phase 2 overlays full BIM models from Revit and Rhino for 1:1 physical clash detection before ground is broken.

**Developers:** James Russo & Cole — Boise, Idaho

> **Working tree (devs):** PC = `C:\Dev\SiteSync-AR\`, Mac = locked per `CLAUDE.md`. Never use the deprecated OneDrive clone. See [`CLAUDE.md`](CLAUDE.md) → "Canonical Working Trees" for the full rule.

---

## Where the project is right now (May 2026)

Phase 1 — the cut-and-fill earthwork calculator — is **finished and working on a real iPhone 16 Pro**. Walk into a room, tap two points on the floor, and the app drops a digital concrete pad between those taps and tells you how many cubic yards of dirt you'd need to cut or fill to bring the ground level with the pad's bottom. The numbers update live. We just started **Phase 2**, which is the harder half: take a real Revit or Rhino BIM model from an architect, import it into the app, and stand it up at full scale on the actual construction site so an engineer can walk around it and see where the planned building would clash with what's actually there. We're at the very beginning of Phase 2 — the import pipeline is wired up but no BIM model has been brought through it yet. Next step is a sample model download to prove the pipeline works end-to-end before pulling in real Revit/Rhino files.

---

## Stack

| | |
|---|---|
| Engine | Unreal Engine 5.6.1 |
| AR Framework | ARKit · Apple ARKit Plugin (UE5) |
| LiDAR API | ARKit Scene Reconstruction (`meshWithClassification`) |
| Platform | iOS 18+ · iPhone 16 Pro / iPad Pro (LiDAR required) |
| Build Toolchain | Xcode 26 · macOS 15+ (requires `scripts/patch-ue56-xcode26.sh` on Mac) |
| BIM Ingestion | Datasmith (Revit / Rhino) |
| AI Workflow | Claude Code · Unreal Engine MCP |

> **Important API note:** The ARKit Scene Reconstruction API is used — NOT the Scene Depth API. Scene Reconstruction produces a persistent 3D triangular mesh suitable for volumetric math. Scene Depth only produces a per-frame 2D depth buffer with no persistent geometry and cannot be used for cut-and-fill calculations.

---

## Features

### Phase 1 — Cut-and-Fill AR Calculator
- Real-time LiDAR terrain meshing via ARKit Scene Reconstruction
- Touch-to-place foundation anchor with pinch-to-scale and rotate gestures
- Per-cell volumetric diff: foundation plane vs. scanned terrain
- Live cubic yardage output (cut and fill) displayed in AR HUD

### Phase 2 — 1:1 BIM Clash Overlay
- Full architectural model ingestion via Datasmith (Revit / Rhino → UE5)
- GPS + compass auto-alignment of BIM model to real-world site coordinates
- MEP layer toggle panel (structural, mechanical, electrical, plumbing)
- Visual clash highlighting where BIM geometry intersects scanned environment

### Phase 2 review modes — Site Scale vs Model Scale

The `PlaceBIMByCornerForward` placement primitive drives both production and demo workflows from a single C++ function — they differ only in the L/W/H scale parameters passed at the call site.

- **Site Scale (production, 1:1).** At a real construction site, a surveyor sets a corner stake and shoots a baseline. The app places the planned building at lifesize, anchored to that corner. Crews walk *through* the planned structure before any concrete is poured. Call site passes `LengthCm/WidthCm/HeightCm = 100` → actor scale `(1, 1, 1)` → mesh renders at native size.
- **Model Scale (review, ~0.3×).** For office reviews, client presentations, contractor walkthroughs, or trade-show demos. Building renders at ~30% native size so the viewer can step back and see the whole structure from outside. Same tap-to-place workflow, same survey-origin convention; just scaled to fit an indoor space. Call site passes `LengthCm/WidthCm/HeightCm = 30` → actor scale `(0.3, 0.3, 0.3)`.

Both modes are real workflows in AEC AR tooling — Trimble Sitevision and Autodesk Construction Cloud AR both ship with toggleable site/model review modes. Surfacing a runtime HUD toggle for the user is on the post-demo roadmap.

> **Demo-ready snapshot (Idaho Technology Council, postponed indefinitely as of May 19, 2026).** SiteSync AR was prepared for the May 19, 2026 Idaho Technology Council conference; the event was postponed mid-prep with no new date. The current `main` is retained as a demo-ready baseline for whenever the event reschedules. In that snapshot, SiteSync AR demos in **Model Scale**: a ~7.7m × 8.3m × 2.4m small house (Fab → Jimbogies, CC BY 4.0) renders at ~2.3m × 2.5m × 0.7m, dropped onto the venue floor via two taps. Lets attendees walk *around* the BIM model in the demo space instead of standing *inside* an opaque wall.

---

## Development Roadmap

### Phase 1 — Cut-and-Fill AR Calculator

| Node | Description | Status |
|---|---|---|
| 1.1 | Source control, Git LFS, iOS signing, project foundation | ✅ Complete |
| 1.2 | LiDAR environmental meshing via ARKit Scene Reconstruction | ✅ Device-validated |
| 1.3 | Digital foundation anchoring with touch gesture placement | ✅ Device-validated |
| 1.4 | Volumetric geometry scripting — cut-and-fill cubic yardage output | ✅ Device-validated (v20, 2026-05-11) |

### Phase 2 — 1:1 BIM Clash Overlay

| Node | Description | Status |
|---|---|---|
| 2.1 | Datasmith ingestion pipeline (Revit / Rhino → UE5, mobile LODs) | 🚧 In progress (started 2026-05-12) |
| 2.2 | Geospatial & compass anchoring (GPS auto-alignment) | ⏳ Pending |
| 2.3 | Engineering clash interface (MEP layer toggles + clash highlighting) | ⏳ Pending |

**Current gate:** Phase 1 is closed. Node 2.1 needs an end-to-end Datasmith import (sample BIM first, then real Revit/Rhino) cooked into the iOS package and rendering on iPhone 16 Pro at 60fps. Node-by-node detail (built assets, BP graphs, decisions, next actions) lives in [`CLAUDE.md`](CLAUDE.md) — the canonical session context for both human and AI contributors.

---

## Architecture

### Phase 1 — LiDAR Pipeline

```
iPhone 16 Pro (LiDAR)
        │
        ▼
ARKit Scene Reconstruction ──► Persistent 3D terrain mesh (UARMeshGeometry anchors)
        │
        ▼
ARMeshBlueprintLibrary (C++) ──► GetAllARMeshGeometries / GetARMeshData
        │
        ▼
UE5 ProceduralMeshComponent ──► TerrainMesh (200ms update throttle)
        │
        ▼
Geometry Scripting Plugin ──► Cubic yardage output ──► AR HUD (UMG)
```

### Phase 2 — BIM Pipeline

```
Revit / Rhino
      │
      ▼ Datasmith Export (.udatasmith)
      │
      ▼
UE5 BIM Scene ──► GPS + Compass Anchor ──► MEP Layer Toggles ──► Clash Detection
```

### MCP Architecture

```
[Claude Code]
       │
       ▼
   MCP Host → Tool Calls → Python Server (UV) → TCP Socket → UE5 C++ Plugin → UE5 API
```

The UnrealMCP C++ plugin auto-starts a TCP server on `127.0.0.1:55557` when the UE5 editor loads — no manual server start needed. The server accepts multiple concurrent connections, so a direct-TCP diagnostic script can run alongside the Claude Code MCP session.

**Config (checked into repo):**
- `.mcp.json` — Claude Code MCP server config (project-level)

After a fresh clone or first session, run `/mcp` inside Claude Code (or restart) so it registers the server. Tools then appear as `mcp__unrealMCP__*` and can be called directly from chat.

MCP is used for: test scene assembly, actor placement, component property edits (material slots, transforms), batch operations, and terrain proxy simulation in the editor. Test scenes are stored under `Content/AR_SiteAnalysis/MCP_TestScenes/` and are not shipped. Note: Blueprint graph node-by-node rewiring via MCP is unreliable — fall back to manual editor work for non-trivial graphs.

---

## Hardware Setup

| Role | Machine | Responsibility |
|---|---|---|
| Primary workstation | Windows PC | UE5 Blueprints, Datasmith BIM import, asset management, MCP server |
| Remote workstation + deploy | MacBook Pro 16" M5 Pro | UE5 Blueprints (remote sessions), Xcode compilation, code signing, wired device deployment |
| Test device | iPhone 16 Pro | LiDAR scanning, AR session validation |

UE5 5.6.1 is installed on both machines. James works on whichever is available — always push before switching. Canonical working trees are pinned per machine in [`CLAUDE.md`](CLAUDE.md).

---

## Development Environment

### PC (Windows) — canonical tree `C:\Dev\SiteSync-AR\`
- Unreal Engine 5.6.1
- Visual Studio 2022 (`NativeGame` workload)
- Claude Code (MCP client — `.mcp.json` at repo root)
- `uv` at `C:\Users\jruss\.local\bin\uv.exe` (required for the UnrealMCP Python server)

### Mac — canonical tree pinned in [`CLAUDE.md`](CLAUDE.md)
- Unreal Engine 5.6.1
- Xcode 26 (requires `scripts/patch-ue56-xcode26.sh` after every UE 5.6 reinstall)
- Git LFS 3.7.1 (installed via Homebrew)
- gh CLI (GitHub auth)
- SSH key configured for `git@github.com:jamesrusso12/SiteSync-AR.git`

### AI Toolchain

| Tool | Role |
|---|---|
| Claude Code | Primary IDE — architecture, documentation, git workflow, config, C++ shim authoring, AND direct UE5 control via UnrealMCP |
| Unreal Engine MCP | Direct UE5 API control via natural language — test scene assembly, actor placement, component edits, terrain proxy simulation |

---

## iOS Build & Deploy Workflow

UE5 runs on the PC. iOS apps must be compiled on macOS (Apple requirement). The connection between them is UE5's **Remote Build** feature, which SSH's into the Mac to run the iOS compile step.

### Remote Build Setup (one-time, on PC)

In UE5: `Project Settings → Platforms → iOS → Build`

| Setting | Value |
|---|---|
| Remote Server Name | Mac's local IP (`ipconfig getifaddr en0` on Mac) |
| Remote User Name | Mac username |
| SSH Key | Click "Generate SSH Key" and complete the pairing prompt on Mac |

### Mac Prerequisites (verify before first deploy)

```bash
# Confirm Xcode CLI tools are active
xcode-select --print-path
# Expected: /Applications/Xcode.app/Contents/Developer

# Accept Xcode license
sudo xcodebuild -license accept

# Confirm Remote Login (SSH) is enabled
sudo systemsetup -getremotelogin

# Confirm iOS 18 SDK is present
xcodebuild -showsdks | grep iphoneos
```

### Deploy Flow

1. PC: `Platforms → iOS → Package Project` — UE5 SSH's into Mac, compiles, outputs `.ipa`
2. Mac: Install wired to iPhone 16 Pro

```bash
# Option A — drag .ipa onto Xcode Devices & Simulators "Installed Apps"

# Option B — command line
xcrun devicectl device install app --device <UDID> /path/to/SiteSyncAR.ipa
```

### iOS Signing

| Phase | Account | Capability |
|---|---|---|
| Prototype | Free Apple ID | Wired deploy only · Re-sign every 7 days |
| Distribution | Apple Developer ($99/yr) | TestFlight · 1-year provisioning · App Store |

Bundle ID in `DefaultEngine.ini` is currently a placeholder (`com.yourcompany.SiteSyncAR`). Update it when a paid Apple Developer account is activated.

---

## Performance Targets

| Target | Requirement |
|---|---|
| Frame rate | 60fps on iPhone 16 Pro — non-negotiable |
| LiDAR mesh update interval | 200ms minimum (enforced by Set Timer in BP_LiDARMeshManager) |
| Procedural mesh collision | `Create Collision = false` during prototype phase |
| Datasmith LODs | Required on all imported BIM assets (Phase 2) |
| Threading | Any heavy geometry operation must be flagged for off-game-thread execution |

---

## Project Structure

```
SiteSync-AR/                          # canonical: C:\Dev\SiteSync-AR\ on PC
├── CLAUDE.md                         # session context — read first
├── README.md
├── .mcp.json                         # Claude Code MCP server config
├── memory/                           # Claude Code session memory (committed)
├── scripts/
│   └── patch-ue56-xcode26.sh         # idempotent Xcode 26 / UE 5.6 toolchain patch (Mac)
└── SiteSyncAR/                       # the UE5 .uproject lives here
    ├── SiteSyncAR.uproject
    ├── Config/
    │   ├── DefaultEngine.ini         # iOS runtime, Metal, 60fps lock, bundle ID, GameMode
    │   ├── DefaultGame.ini           # Camera + location PList usage descriptions
    │   ├── DefaultInput.ini
    │   └── IOS/
    │       └── IOSEngine.ini         # iOS renderer overrides (Nanite/Lumen/RT all off)
    ├── Source/SiteSyncAR/
    │   ├── SiteSyncAR.Build.cs       # Module deps: AugmentedReality, ProceduralMesh, GeometryCore
    │   ├── Public/
    │   │   └── ARMeshBlueprintLibrary.h
    │   └── Private/
    │       ├── ARMeshBlueprintLibrary.cpp
    │       └── ARMeshBlueprintLibrary_iOS.mm
    ├── Plugins/UnrealMCP/            # community MCP plugin — auto-starts TCP server on 55557
    │   ├── UnrealMCP.uplugin
    │   ├── Source/UnrealMCP/         # C++ TCP server + command handlers
    │   └── Python/                   # MCP server entry + tool registrations
    │       ├── unreal_mcp_server.py
    │       └── tools/
    └── Content/
        ├── AR_SiteAnalysis/
        │   └── DA_SiteSyncARConfig
        ├── Blueprints/               # BP_ prefix (BP_LiDARMeshManager, BP_Foundation, BP_TapMarker, BP_ARPlayerController, BP_ARGameMode)
        ├── Materials/                # M_ prefix (M_LiDARDebug, M_FoundationDebug, M_MarkerDebug)
        ├── Input/                    # IA_TapPlace, IMC_Placement
        ├── Meshes/                   # SM_ prefix
        ├── UI/                       # WBP_ prefix
        ├── DatasmithAssets/          # Imported BIM geometry (Phase 2)
        └── MCP_TestScenes/           # MCP-generated test environments (not shipped)
```

---

## MCP Test Scene — Node 1.2 Terrain Proxy

Paste into Claude Code to simulate a LiDAR terrain surface in the editor before device testing:

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

## Git

- **Remote:** `git@github.com:jamesrusso12/SiteSync-AR.git`
- **Auth:** SSH on Mac · HTTPS + gh CLI on PC
- **Default branch:** `main`
- **LFS tracked:** `.uasset` `.umap` `.uproject` `.fbx` `.udatasmith` and all binary asset types

### Branch Policy

Commits go directly to `main`. No feature branches required.

### Commit Convention

```
feat:   new node work
fix:    bug fixes
docs:   README, CLAUDE.md updates
chore:  config, LFS, MCP test scenes
```

### First-time PC setup

```bash
git lfs install
git lfs pull
```

---

## Engineering Challenges

A non-exhaustive list of the hard problems we hit and what we did about them. We log each new gotcha to `memory/decisions.md` with cause / symptom / fix / "how to apply" detail, and surface a one-line index in `CLAUDE.md` → "Bugs & Workflow Gotchas — Index" for fast lookup. These are the ones worth surfacing for anyone reading the repo.

### iOS / Mac toolchain drift

- **Xcode 26 vs UE 5.6.1 SDK gate.** UE 5.6's `Apple_SDK.json` caps iOS SDK MaxVersion at 16.9.0, which predates the Xcode 26 we're building against. Without a patch, every iOS build fails with `Platform IOS is not a valid platform to build`. We ship [`scripts/patch-ue56-xcode26.sh`](scripts/patch-ue56-xcode26.sh) which bumps the MaxVersion in-place; it must be re-run after any Epic Launcher update that touches UE 5.6, because the patch lives in shared engine files outside git.
- **Xcode loses Apple ID between sessions.** A pipeline that worked one day can fail the next with `No Accounts / No profiles for 'com.RussoCompany.SiteSyncAR' were found`. The macOS-level Apple ID stays signed in, but Xcode's own Accounts pane evicts the account — likely after Xcode auto-updates. Fix: re-add the Apple ID in Xcode → Settings → Accounts. Worth checking before debugging signing-cert paths.
- **Stale path drift in UE config.** The project was moved from `~/Desktop/Github/Xcode/SiteSync-AR/` to `~/Developer/SiteSync-AR/` to escape iCloud xattrs that broke `codesign`. Old path references can linger in `~/Library/Application Support/Epic/UnrealEngine/Editor/ProjectEditorRecords.json` and the project's per-user `EditorPerProjectUserSettings.ini`, causing `LongPackageNameToFilename failed to convert` crashes on Save-As. We `grep -rl "Desktop/Github"` and sed-replace when this surfaces.

### UE5 editor save discipline

UE has multiple "dirty" states (in-memory rebuild, compiled bytecode, on-disk asset) that don't always travel together. Two save bugs caught us during demo prep:

- **Static Mesh `Apply Changes` doesn't persist to disk.** Build Settings changes rebuild cached mesh data (bounds, distance field, collision) in memory so the viewport updates. But the .uasset itself isn't marked dirty in a way `Save All` catches. You must Cmd+S with the Static Mesh tab focused. We verify with `stat -f "%Sm"` on the .uasset mtime.
- **BP pin literal changes occasionally need a re-type to "take".** Changed a pin from 100 → 30, compiled green, saved, cooked. Runtime still saw 100 even though `strings` on the .uasset showed `30.000000`. Re-opening the BP, re-typing 30 (same value), recompiling, re-saving, and re-cooking fixed it. Suspected dirty-tracking miss inside UE's compile pipeline.

### Platform-imposed constraints

- **No paid Apple Developer Program** (yet). The lead developer's day-job employer's Business Conduct rules bar paid enrollment until employment ends. The project runs on a Personal Team Apple ID with 7-day provisioning profiles and wired Mac → iPhone deploy. No TestFlight; co-developer can't get external builds. Influences: deploy cadence, can't share builds with prospects, partner has to physically come to a machine to see new builds.
- **DatasmithRuntime is desktop-only.** Common Phase 2 design discussions presume "user loads a `.udatasmith` on the iPhone at runtime" or "live Direct Link from Revit to the phone." These are impossible. Per Epic's Datasmith Supported Platforms doc, DatasmithRuntime supports Windows + macOS only — iOS and Android have never been supported and aren't on any 5.x roadmap. All Datasmith work happens at editor cook time on PC/Mac, baked to standard `UStaticMesh` / `UMaterialInstance`, then cooked through the normal iOS pipeline.
- **glTF imports often arrive 10× oversized.** Modelers commonly export in millimeters; UE's glTF importer reads positions as centimeters by default. A 7.7m house comes in as 77m. We apply Build Scale `(0.1, 0.1, 0.1)` on the Static Mesh's LOD 0 → Build Settings as the standard fix-on-import.

### iOS input + state machine subtleties

- **EnhancedInput `IA_TapPlace.Started` doesn't re-fire after some state transitions on iOS.** v19 of Node 1.4 chased this for a full session — restored Tick polling, changed trigger types, refreshed nodes — none of it made the IA re-arm post-reset. UE's EnhancedInput trigger state machine appears irrecoverable in this specific iOS scenario; the underlying cause is in the engine, not our BP. v20 abandoned IA_TapPlace entirely and switched to a Tick rising-edge poll on `bWasPressed`/`bIsCurrentlyPressed`. The IA + IMC assets remain in the project as dead code we'll clean up in a future pass.
- **`SET <object var>` nodes need BOTH exec wire AND data-input wire.** UE doesn't warn at compile time when the data input is unconnected — it silently sets the variable to None using the pin default. This burned us in `BP_ARPlayerController_BIM` where `SET ActiveBIM` had a valid exec chain but the data input pin was unwired, so every BIM spawn set ActiveBIM = None and the reset logic never fired. Always dump-and-verify the graph when state vars seem stuck.

### Diagnostic-tool footguns

- **MCP plugin DLL goes stale after source-only updates.** UE5 loads the compiled DLL once at editor start; subsequent commits to the plugin source don't take effect until the DLL is rebuilt. Symptom: MCP commands that should work return `Unknown command` or other silent misbehavior. See `CLAUDE.md` "MCP plugin health & rebuild protocol" for the rebuild command + 5-second sanity probe.
- **`probe_post_rebuild.py` uses a fixed actor name** (`MCP_FixCheck_01`). Polling it in a `while` loop can fatal-crash the editor when a delete from the prior probe is still pending and a new spawn collides on the same name. Don't loop the probe.

---

## Debugging Reference

| Issue | Where to look |
|---|---|
| ARKit / LiDAR | UE5 Output Log → filter `ARKit` |
| MCP connection | Python terminal running `server.py` first, then UE5 Output Log |
| Datasmith import | Datasmith Import Log in Content Browser |
| iOS deploy | Xcode → Devices & Simulators console |
| C++ compile errors | Visual Studio 2022 Output window |
| 60fps validation | Xcode Instruments → Core Animation / GPU profiler, wired to device |

---

## Code Style

- **Blueprint-first.** Escalate to C++ only for: ARKit callbacks, MCP plugin bridge, performance-critical loops
- All C++ exposed as `BlueprintCallable` `UFUNCTION`s so they can be wired immediately in Blueprint graphs
- Blueprint graphs organized into Functions and Macros with descriptive comment nodes
- No comments explaining what code does — only why, when non-obvious
- No speculative features, abstractions, or error handling for impossible states

---

*Built by James Russo & Cole · Boise, Idaho*
