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

## 2026-04-21 — Xcode/UE 5.5 toolchain blocker (resolved via 5.6.1 upgrade)
Mac is on Xcode 26.4.1; UE 5.5 targets Xcode 15.x. SharedPCH fails on `-Werror,-Wdeprecated-literal-operator` (new clang diagnostic on engine `operator "" _Private*SV` operators in `StringView.h`). Blocks both local Mac iOS compile and PC-driven SSH remote-build. **Resolved by upgrading the project to UE 5.6.1** (commit 7bf6c15, 2026-04-21). Mac iOS compile verification still pending.

## 2026-04-21 — Apple_SDK.json MaxVersion patch for Xcode 26 (Mac only)
UE 5.6.1 ships `Apple_SDK.json` with `MaxVersion = "16.9.0"`, but Mac has Xcode 26.4.1. Without the bump, UBT refuses to register iOS/Mac as buildable. UE 5.6's source is already clang-19-clean, so the gate is the only blocker. Patched `MaxVersion → "27.0.0"` via `scripts/patch-ue56-xcode26.sh` (idempotent).

**Why:** Needed to unblock Mac iOS compile; Xcode downgrade would be sanctioned but costs ~30 min + ongoing toolchain juggling.

**How to apply:** `bash scripts/patch-ue56-xcode26.sh` — run after every Epic Launcher update to UE 5.6.x (the patched file lives outside git and Launcher can reset it). Script is idempotent and leaves a timestamped `.bak`.

**PC note:** PC does not need the patch — Windows build path does not consume `Apple_SDK.json`. Only Mac iOS compile (local or PC-driven remote-build) needs it.

## 2026-04-21 — Engine upgraded to UE 5.6.1 (from 5.5.4)
Bumped `EngineAssociation` "5.5" → "5.6" and `IncludeOrderVersion` `Unreal5_5` → `Unreal5_6` in both Target.cs files. PC C++ compile verified clean on 5.6 toolchain (UBT SiteSyncAREditor Win64 Development). UE 5.6 editor auto-rebuilt and resaved BP_LiDARMeshManager.uasset on first open. AppleARKit + ProceduralMeshComponent + GeometryScripting plugins all stable across the bump. Gate to Mac iOS remote-build verification (see Xcode 26 entry above).

## 2026-04-21 — ⚠️ Canonical working tree is C:\Dev\SiteSync-AR\ on PC (NOT the OneDrive path)
There are two project directories on the PC: the OneDrive copy at `C:\Users\jruss\OneDrive\Desktop\Project Kickoff\SiteSync-AR\` (stale clone, do NOT edit) and the real working tree at `C:\Dev\SiteSync-AR\` (where UE 5.6 actually opens, where commits are made, where LFS smudges land). Both point at the same GitHub remote but OneDrive tends to lag and can cause UE file-lock issues. **Always `cd /c/Dev/SiteSync-AR` (or use `git -C`) before terminal work on PC.** The OneDrive copy can be deleted at any time; it exists only because Claude Code was originally launched from the OneDrive path.
