# Architectural & Technical Decisions

<!-- Log key decisions here so they don't get relitigated. Format: date, decision, rationale. -->

## 2026-04 — ARKit Scene Reconstruction over Scene Depth
Use `meshWithClassification` (Scene Reconstruction API), not Scene Depth API.
Scene Depth is a per-frame 2D depth buffer — no persistent geometry, no volume math.
Scene Reconstruction produces a persistent 3D triangular mesh required for cut-and-fill.

## 2026-04 — Blueprint-first, C++ only at boundaries
C++ reserved for: ARKit callbacks, MCP plugin bridge, performance-critical loops.
All C++ exposed as `BlueprintCallable` so James can wire logic in Blueprint graphs.

## 2026-04 — 200ms LiDAR mesh update throttle
Target is 60fps on iPhone 16 Pro. Mesh rebuilds on a 0.2s looping timer, not every frame.
`Create Collision = False` during prototype phase to avoid physics overhead.

## 2026-04 — Free Apple ID for prototype; paid account deferred
Paid $99/yr Apple Developer account only needed when Cole requires TestFlight access.
Bundle ID `com.yourcompany.SiteSyncAR` is placeholder until paid account is activated.

## 2026-04-21 — iOS GetARMeshData reads ARMeshAnchor directly, not FARKitMeshData
`FARKitMeshData::GetMeshData(FGuid)` returns a `MeshDataPtr` but its `Vertices`/`Indices` arrays are private — unreachable from our module without friend access or engine patching. Instead, the iOS shim pulls the live `ARMeshAnchor` from `ARSession.currentFrame` and matches it to the UE `UARMeshGeometry` by `UniqueId` (NSUUID ↔ FGuid via `FAppleARKitConversion::ToFGuid`). Reads `geometry.vertices` and `geometry.faces` directly and applies the same `FVector(-z, x, y) * 100` conversion the engine uses internally. Same axis/scale semantics, independent data path.

## 2026-04-21 — Xcode/UE 5.5 toolchain blocker (unresolved)
Mac is on Xcode 26.4.1; UE 5.5 targets Xcode 15.x. SharedPCH fails on `-Werror,-Wdeprecated-literal-operator` (new clang diagnostic on engine `operator "" _Private*SV` operators in `StringView.h`). Blocks both local Mac iOS compile and PC-driven SSH remote-build (both use the Mac's Xcode). Need to pick: install Xcode 15 alongside + xcode-select, or upgrade the project to UE 5.6+. Deferred until James decides.
