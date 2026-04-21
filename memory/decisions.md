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
