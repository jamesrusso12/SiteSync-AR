# SiteSync AR

> On-site spatial intelligence for civil engineers — built in Unreal Engine 5.6 with ARKit and iPhone 16 Pro LiDAR.

SiteSync AR is an iOS augmented reality application for AEC (Architecture, Engineering, Construction) professionals. Phase 1 uses LiDAR-scanned terrain to calculate real-time cut-and-fill earthwork volumes on site. Phase 2 overlays full BIM models from Revit and Rhino for 1:1 physical clash detection before ground is broken.

**Developers:** James Russo & Cole — Boise, Idaho

> **Working tree (devs):** PC = `C:\Dev\SiteSync-AR\`, Mac = locked per `CLAUDE.md`. Never use the deprecated OneDrive clone. See [`CLAUDE.md`](CLAUDE.md) → "Canonical Working Trees" for the full rule.

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
| AI Workflow | Claude Code · Unreal Engine MCP (Cursor optional) |

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

---

## Development Roadmap

### Phase 1 — Cut-and-Fill AR Calculator

| Node | Description | Status |
|---|---|---|
| 1.1 | Source control, Git LFS, iOS signing, project foundation | ✅ Complete |
| 1.2 | LiDAR environmental meshing via ARKit Scene Reconstruction | ✅ Complete (PC) · 🔄 iOS device deploy pending |
| 1.3 | Digital foundation anchoring with touch gesture placement | ✅ Complete (PC) · 🔄 iOS device deploy pending |
| 1.4 | Volumetric geometry scripting — cut-and-fill cubic yardage output | ⏳ Next |

### Phase 2 — 1:1 BIM Clash Overlay

| Node | Description | Status |
|---|---|---|
| 2.1 | Datasmith ingestion pipeline (Revit / Rhino → UE5, mobile LODs) | ⏳ Pending |
| 2.2 | Geospatial & compass anchoring (GPS auto-alignment) | ⏳ Pending |
| 2.3 | Engineering clash interface (MEP layer toggles + clash highlighting) | ⏳ Pending |

**Current gate:** Nodes 1.2 and 1.3 are PC-complete; remaining gate before Node 1.4 is on-device validation on iPhone 16 Pro at 60fps. Node-by-node detail (built assets, BP graphs, decisions, next actions) lives in [`CLAUDE.md`](CLAUDE.md) — that is the canonical session context for both human and AI contributors.

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
[Claude Code]   ← primary MCP client
[Cursor]        ← optional, kept as backup
       │
       ▼
   MCP Host → Tool Calls → Python Server (UV) → TCP Socket → UE5 C++ Plugin → UE5 API
```

The UnrealMCP C++ plugin auto-starts a TCP server on `127.0.0.1:55557` when the UE5 editor loads — no manual server start needed. Multiple MCP clients (Claude Code + Cursor) can connect to the same port simultaneously without conflict.

**Configs (both checked into repo):**
- `.mcp.json` — primary, used by Claude Code (project-level MCP config)
- `.cursor/mcp.json` — optional backup, used by Cursor

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
- Claude Code (primary MCP client — `.mcp.json` at repo root)
- `uv` at `C:\Users\jruss\.local\bin\uv.exe` (required for the UnrealMCP Python server)
- Cursor IDE (optional — same MCP server also registered in `.cursor/mcp.json`)

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
| Cursor (optional) | Secondary MCP client — kept available but no longer required |

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
├── .mcp.json                         # Claude Code MCP server config (primary)
├── .cursor/mcp.json                  # Cursor MCP server config (optional)
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

Paste into Claude Code (or Cursor Agent mode) to simulate a LiDAR terrain surface in the editor before device testing:

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
