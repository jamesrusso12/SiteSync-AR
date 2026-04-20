# SiteSync AR

> On-site spatial intelligence for civil engineers — built in Unreal Engine 5.5 with ARKit and iPhone 16 Pro LiDAR.

SiteSync AR is an iOS augmented reality application for AEC (Architecture, Engineering, Construction) professionals. Phase 1 uses LiDAR-scanned terrain to calculate real-time cut-and-fill earthwork volumes on site. Phase 2 overlays full BIM models from Revit and Rhino for 1:1 physical clash detection before ground is broken.

**Developers:** James Russo & Cole — Boise, Idaho

---

## Stack

| | |
|---|---|
| Engine | Unreal Engine 5.5.4 |
| AR Framework | ARKit · Apple ARKit Plugin (UE5) |
| LiDAR API | ARKit Scene Reconstruction (`meshWithClassification`) |
| Platform | iOS 18+ · iPhone 16 Pro / iPad Pro (LiDAR required) |
| Build Toolchain | Xcode 16 · macOS 15+ |
| BIM Ingestion | Datasmith (Revit / Rhino) |
| AI Workflow | Claude Code · Cursor · Unreal Engine MCP |

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
| 1.2 | LiDAR environmental meshing via ARKit Scene Reconstruction | 🔄 In Progress |
| 1.3 | Digital foundation anchoring with touch gesture placement | ⏳ Pending |
| 1.4 | Volumetric geometry scripting — cut-and-fill cubic yardage output | ⏳ Pending |

### Phase 2 — 1:1 BIM Clash Overlay

| Node | Description | Status |
|---|---|---|
| 2.1 | Datasmith ingestion pipeline (Revit / Rhino → UE5, mobile LODs) | ⏳ Pending |
| 2.2 | Geospatial & compass anchoring (GPS auto-alignment) | ⏳ Pending |
| 2.3 | Engineering clash interface (MEP layer toggles + clash highlighting) | ⏳ Pending |

**Gate to Node 1.3:** LiDAR mesh visible as a stable cyan translucent overlay on iPhone 16 Pro at 60fps. ✓

---

## Current State — Node 1.2 Detail

Node 1.2 introduces live LiDAR terrain meshing. The C++ layer is committed and compiles. The UE5 editor assets below must be created on the PC before the iOS deploy can be validated on Mac.

### What is committed

**`Source/SiteSyncAR/SiteSyncAR.Build.cs`**
Added modules: `AugmentedReality`, `ProceduralMeshComponent`, `GeometryCore`, `GeometryScriptingCore`. `AppleARKit` is declared iOS-only.

**`Source/SiteSyncAR/Public/ARMeshBlueprintLibrary.h`**
C++ shim exposing two `BlueprintCallable` functions:
- `GetARMeshData(UARMeshGeometry*, OutVertices, OutIndices)` — extracts vertex and index arrays from a single ARKit mesh anchor
- `GetAllARMeshGeometries()` — returns all currently tracked `UARMeshGeometry` anchors from the active AR session

**`Source/SiteSyncAR/Private/ARMeshBlueprintLibrary.cpp`**
Implementation. Filters `UARBlueprintLibrary::GetAllGeometries()` to `UARMeshGeometry` only via Cast.

**`Config/DefaultEngine.ini`**
- `MinimumiOSVersion=IOS_18` (targets iPhone 16 Pro / iOS 18+)
- `FrameRateLock=TARGETFRAMERATE_60`
- `BundleIdentifier=com.yourcompany.SiteSyncAR` *(placeholder — update when Apple Developer paid account is active)*

**`Config/IOS/IOSEngine.ini`**
iOS-specific renderer overrides: Nanite off, Lumen off, Ray Tracing off, Path Tracing off, Virtual Shadow Maps off, Mobile HDR off, Bloom/MotionBlur/AutoExposure off.

### Still needed on PC (UE5 editor)

| Asset | Path | Description |
|---|---|---|
| `DA_SiteSyncARConfig` | `Content/AR_SiteAnalysis/` | ARSessionConfig data asset — World Tracking, Scene Reconstruction = Mesh With Classification |
| `M_LiDARDebug` | `Content/AR_SiteAnalysis/Materials/` | Translucent, Unlit, Two-Sided, cyan emissive (0, 1, 0.8), opacity 0.35 |
| `BP_LiDARMeshManager` | `Content/AR_SiteAnalysis/Blueprints/` | Actor Blueprint — ProceduralMeshComponent, 200ms looping timer, UpdateLiDARMesh event |

### BP_LiDARMeshManager logic

```
BeginPlay
  └─► Start AR Session (DA_SiteSyncARConfig)
  └─► Set Timer by Event (0.2s, looping) → UpdateLiDARMesh

UpdateLiDARMesh (Custom Event)
  └─► GetAllARMeshGeometries → Array<UARMeshGeometry>
  └─► Clear All Mesh Sections (TerrainMesh)
  └─► ForEach Loop
        └─► GetARMeshData (Geometry) → Vertices, Indices, Normals
        └─► Branch: Vertices.Num() > 0
              True ─► Create Mesh Section (SectionIndex=LoopIdx, CreateCollision=false)
                   ─► Set Material (M_LiDARDebug)
```

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
[Claude Code / Cursor] → MCP Host → Tool Calls → Python Server (UV) → TCP Socket → UE5 C++ Plugin → UE5 API
```

MCP is used for: test scene assembly, actor placement, batch operations, and terrain proxy simulation in the editor. Test scenes are stored under `Content/AR_SiteAnalysis/MCP_TestScenes/` and are not shipped.

---

## Hardware Setup

| Role | Machine | Responsibility |
|---|---|---|
| Primary workstation | Windows PC | UE5 Blueprints, Datasmith BIM import, asset management, MCP server |
| Build & deploy | MacBook Pro 16" M5 Pro | Xcode remote compilation, code signing, wired device deployment |
| Test device | iPhone 16 Pro | LiDAR scanning, AR session validation |

---

## Development Environment

### PC (Windows)
- Unreal Engine 5.5.4
- Visual Studio 2022 (`NativeGame` workload)
- Cursor IDE + Unreal Engine MCP plugin
- Claude Code

### Mac
- Xcode 16
- Git LFS 3.7.1 (installed via Homebrew)
- gh CLI (GitHub auth)
- SSH key configured for `git@github.com:jamesrusso12/SiteSync-AR.git`

### AI Toolchain

| Tool | Role |
|---|---|
| Claude Code | Architecture, documentation, git workflow, config, C++ shim authoring |
| Cursor | In-editor Blueprint and C++ authoring via MCP |
| Unreal Engine MCP | Direct UE5 API control via natural language — test scene assembly, actor placement, terrain proxy simulation |

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
SiteSyncAR/
├── Config/
│   ├── DefaultEngine.ini       # iOS runtime settings, Metal, 60fps lock, bundle ID
│   ├── DefaultGame.ini         # Camera + location PList usage descriptions
│   ├── DefaultInput.ini
│   └── IOS/
│       └── IOSEngine.ini       # iOS renderer overrides (Nanite/Lumen/RT all off)
├── Source/SiteSyncAR/
│   ├── SiteSyncAR.Build.cs     # Module deps: AugmentedReality, ProceduralMesh, GeometryCore
│   ├── Public/
│   │   └── ARMeshBlueprintLibrary.h
│   └── Private/
│       └── ARMeshBlueprintLibrary.cpp
└── Content/AR_SiteAnalysis/
    ├── Blueprints/              # BP_ prefix
    ├── UI/                      # WBP_ prefix
    ├── Meshes/                  # SM_ prefix
    ├── Materials/               # M_ prefix
    ├── DatasmithAssets/         # Imported BIM geometry (Phase 2)
    └── MCP_TestScenes/          # MCP-generated test environments (not shipped)
```

---

## MCP Test Scene — Node 1.2 Terrain Proxy

Paste into Cursor Agent mode to simulate a LiDAR terrain surface in the editor before device testing:

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

### Branch Naming

```
feature/node-1.1-source-control
feature/node-1.2-lidar-meshing
feature/node-1.3-foundation-anchor
feature/node-1.4-cut-fill-calc
feature/node-2.1-datasmith-pipeline
feature/node-2.2-geospatial-anchor
feature/node-2.3-clash-interface
```

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
