# SiteSync AR

> On-site spatial intelligence for civil engineers — built in Unreal Engine 5.5 with ARKit and iPhone 16 Pro LiDAR.

SiteSync AR is an iOS augmented reality application for AEC (Architecture, Engineering, Construction) professionals. Phase 1 uses LiDAR-scanned terrain to calculate real-time cut-and-fill earthwork volumes on site. Phase 2 overlays full BIM models from Revit and Rhino for 1:1 physical clash detection before ground is broken.

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
| 1.2 | LiDAR environmental meshing via ARKit Scene Reconstruction | 🔄 In Progress — M_LiDARDebug ✅ · DA_SiteSyncARConfig ⚠️ verify Scene Reconstruction setting · BP_LiDARMeshManager BeginPlay ✅ RebuildMesh body incomplete |
| 1.3 | Digital foundation anchoring with touch gesture placement | ⏳ Pending |
| 1.4 | Volumetric geometry scripting — cut-and-fill cubic yardage output | ⏳ Pending |

### Phase 2 — 1:1 BIM Clash Overlay
| Node | Description | Status |
|---|---|---|
| 2.1 | Datasmith ingestion pipeline (Revit / Rhino → UE5, mobile LODs) | ⏳ Pending |
| 2.2 | Geospatial & compass anchoring (GPS auto-alignment) | ⏳ Pending |
| 2.3 | Engineering clash interface (MEP layer toggles + clash highlighting) | ⏳ Pending |

---

## Architecture

```
iPhone 16 Pro (LiDAR)
        │
        ▼
ARKit Scene Reconstruction ──► Persistent 3D terrain mesh
        │
        ▼
UE5 Procedural Mesh Component ──► Blueprint cut/fill math
        │
        ▼
Geometry Scripting Plugin ──► Cubic yardage output ──► AR HUD (UMG)
```

```
Revit / Rhino
      │
      ▼ Datasmith Export (.udatasmith)
      │
      ▼
UE5 BIM Scene ──► GPS + Compass Anchor ──► MEP Layer Toggles ──► Clash Detection
```

---

## Hardware Setup

| Role | Machine | Responsibility |
|---|---|---|
| Primary workstation | Windows PC | UE5 Blueprints, Datasmith BIM import, asset management |
| Build & deploy | MacBook Pro 16" M5 Pro | Xcode compilation, code signing, wired device deployment |
| Test device | iPhone 16 Pro | LiDAR scanning, AR session validation |

---

## Development Environment

**PC (Windows)**
- Unreal Engine 5.5.4
- Visual Studio 2022 (`NativeGame` workload)
- Cursor IDE + Unreal Engine MCP plugin
- Claude Code

**Mac**
- Xcode 16
- Git LFS 3.7.1
- Homebrew

**AI Toolchain**

| Tool | Role |
|---|---|
| Claude Code | Architecture, documentation, git workflow, code review |
| Cursor | In-editor Blueprint and C++ via MCP |
| Unreal Engine MCP | Direct UE5 API control via natural language — test scene assembly, actor placement, batch operations |

---

## iOS Deployment

| Phase | Account | Capability |
|---|---|---|
| Prototype | Free Apple ID | Wired deploy to iPhone 16 Pro via Xcode · Re-sign every 7 days |
| Distribution | Apple Developer ($99/yr) | TestFlight · 1-year provisioning · App Store |

---

## Project Structure

```
Content/AR_SiteAnalysis/
├── Blueprints/          # BP_ prefix
├── UI/                  # WBP_ prefix
├── Meshes/              # SM_ prefix
├── Materials/           # M_ prefix
├── DatasmithAssets/     # Imported BIM geometry
└── MCP_TestScenes/      # MCP-generated test environments (not shipped)
```

---

## Git

- LFS tracked: `.uasset` `.umap` `.uproject` `.fbx` `.udatasmith` and all binary asset types
- Branch per node: `feature/node-X.Y-description`
- Commits follow [Conventional Commits](https://www.conventionalcommits.org): `feat:` `fix:` `docs:` `chore:`

---

*Built by James Russo & Cole · Boise, Idaho*
