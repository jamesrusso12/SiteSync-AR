# SiteSync-AR

AR Site Analysis Tool is an iOS AR app built in Unreal Engine 5.5.4 and ARKit. It uses iPhone 16 Pro LiDAR to mesh terrain, calculate cut-and-fill earthwork volumes, and overlay BIM models from Revit and Rhino for real-world clash detection, giving engineers on-site spatial intelligence before ground is broken.

---

## Engine & Platform

| | |
|---|---|
| Engine | Unreal Engine 5.5.4 |
| AR Framework | ARKit 4.0 (Apple ARKit Plugin) |
| Platform | iOS 18+ · iPhone 16 Pro / iPad Pro (LiDAR required) |
| Toolchain | Xcode 16 · macOS 14+ (build machine) |

---

## Technical Architecture Notes

### LiDAR API — Scene Reconstruction (not Scene Depth)

The roadmap document references the **ARKit Scene Depth API**. This has been corrected to use the **ARKit Scene Reconstruction API** (`ARWorldTrackingConfiguration.sceneReconstruction = .meshWithClassification`).

**Why this matters for cut-and-fill:**

| API | What it produces | Usable for volume math? |
|---|---|---|
| Scene Depth API | Per-frame 2D depth map (pixel buffer) | No — no persistent geometry |
| Scene Reconstruction | Persistent 3D mesh anchors updated in real time | Yes — triangulated mesh you can diff against a foundation plane |

The Scene Reconstruction API generates a live, classified triangular mesh of the physical environment via the LiDAR scanner. This mesh is what Node 1.4 (Volumetric Geometry Scripting) will compare against the imported foundation plane to compute cubic yardage for cut and fill.

**Plugin to enable in UE 5.5.4:**
- `Apple ARKit` plugin → exposes `ARKit Scene Reconstruction` via `UARBlueprintLibrary` and the `Handheld AR` module.

---

## Development Phases

### Phase 1 — Cut-and-Fill AR Calculator
- **Node 1.1** Source control & foundation setup *(complete — repo + LFS + free Apple ID wired deploy)*
- **Node 1.2** LiDAR Environmental Meshing via ARKit Scene Reconstruction
- **Node 1.3** Digital Foundation Anchoring (touch gesture placement)
- **Node 1.4** Volumetric Geometry Scripting — cut-and-fill cubic yardage output

### Phase 2 — 1:1 BIM Clash Overlay
- **Node 2.1** Datasmith ingestion pipeline (Revit / Rhino → UE5)
- **Node 2.2** Geospatial & compass anchoring (GPS + compass auto-alignment)
- **Node 2.3** Engineering clash interface (MEP layer toggles + visual clash highlighting)

---

## Apple Developer Account

| Phase | Account Type | Capability |
|---|---|---|
| Prototype (now) | Free Apple ID | Wired deploy to iPhone 16 Pro via Xcode · App re-sign required every 7 days · 3 device limit |
| Distribution (future) | Paid ($99/yr) | TestFlight for Cole · 1-year provisioning · App Store eligibility |

No paid membership required to reach a working prototype on device.

---

## Hardware Setup

| Role | Machine | Responsibility |
|---|---|---|
| Primary workstation | PC | UE5 Blueprints, BIM asset import, Datasmith, logic |
| Build & deploy | 16-in MacBook Pro M5 Pro | Xcode compilation, signing, device deploy |
| Test device | iPhone 16 Pro | LiDAR testing, AR session validation |
