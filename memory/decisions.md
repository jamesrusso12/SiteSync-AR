# Architectural & Technical Decisions

<!-- Log key decisions here so they don't get relitigated. Format: date, decision, rationale. -->

## 2026-04-27 — OneDrive clone severed; canonical PC tree is C:\Dev\SiteSync-AR
The OneDrive working tree at `C:\Users\jruss\OneDrive\Desktop\Project Kickoff\SiteSync-AR\` was severed: `.git` renamed to `.git.DEPRECATED-20260427`, `_DEPRECATED_DO_NOT_USE.md` placed at root, history bundled to `C:\Dev\SiteSync-AR-onedrive-backup.bundle`. Two MCP commits that lived only in the OneDrive clone (`f510479`, `07de387`) were ported into canonical as commit `fec1af2 feat(mcp): port in-place graph edits + Enhanced Input lookup`.

**Why:** OneDrive sync had silently caused divergence — work committed in the OneDrive tree never reached origin while origin moved on with Node 1.3 work. The `f510479` MCP plugin features (`disconnect_blueprint_nodes`, `delete_blueprint_node`, `K2Node_EnhancedInputAction` lookup) sat invisible to the rest of the toolchain. James had corrected the same OneDrive trap multiple sessions running.

**How to apply:**
- PC: only ever work in `C:\Dev\SiteSync-AR\`. Mac canonical path is locked in `CLAUDE.md` "Canonical Working Trees" by the next Mac session (candidates: `~/Dev/SiteSync-AR/` vs `~/Developer/SiteSync-AR/` — pick one and update CLAUDE.md).
- Cross-machine handoff prompts must use the **receiving** machine's canonical path, never the sender's.
- If a tool result returns a path under `…\OneDrive\…\SiteSync-AR\…`, treat it as a bug.
- Do not delete the OneDrive folder via the OneDrive web UI; if removed, do it locally.

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

## 2026-04-21 — CONSTRAINT: No paid Apple Developer Program (retail BC); Personal Team IS OK
Apple retail Business Conduct rules bar James from enrolling in the **paid** Apple Developer Program ($99/yr) while employed in retail. Non-negotiable until he leaves retail.

**However — Personal Team (free, automatic with any Apple ID via Xcode sign-in) is fine.** James already uses Personal Team for his other apps (e.g. "Shift"). No enrollment, no payment, just Xcode → Settings → Accounts → sign-in. That path IS available for SiteSync AR.

**Impact on deploy path:**
- Wired Mac → iPhone deploy via Personal Team: YES
- Signing produces a 7-day provisioning profile; app must be re-deployed from Xcode weekly
- Limited to 3 Personal Team apps active on device at once
- No TestFlight: Cole can't get builds until either he enrolls ($99/yr paid) or James leaves retail
- Bundle ID must be unique but NOT registered anywhere (literally any string like `com.jamesrusso.SiteSyncAR` works)

**How to apply:**
- Edit Bundle ID off `com.yourcompany.SiteSyncAR` placeholder → use a James-owned reverse-DNS string
- In UE iOS Project Settings: sign-in with Apple ID, pick Personal Team from signing dropdown
- Proceed with wired deploy; plan for weekly re-deploy from Mac
- Do NOT suggest paid enrollment, TestFlight, or external-device distribution until retail status changes

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

## 2026-04-21 — Mac project relocated from Desktop to ~/Developer (iCloud xattr blocker)
macOS Sequoia+ auto-syncs `~/Desktop` and `~/Documents` to iCloud Drive when "Desktop & Documents" is on (James's Mac has `FXICloudDriveDesktop=1`). Every file in an iCloud-synced dir gets `com.apple.fileprovider.fpfs#P` + `com.apple.FinderInfo` xattrs auto-added by the fileprovider framework **and re-applied the instant they're removed**. `codesign` refuses to sign anything with those xattrs ("resource fork, Finder information, or similar detritus not allowed"). No amount of UE-side patching can beat the filesystem layer.

**Fix:** Project moved from `~/Desktop/Github/Xcode/SiteSync-AR/` to `~/Developer/SiteSync-AR/`. `~/Developer/` is Apple-standard for dev work and NOT iCloud-synced. A symlink remains at the old Desktop path for backward compatibility with stale references (tool cwds, Claude session paths).

**How to apply:** Always work out of `~/Developer/SiteSync-AR/` on Mac. If this path ever ends up under Desktop/Documents/iCloud again, codesign will fail irrecoverably. Engine patches (FinderInfo xattr strip) are irrelevant once the project is outside iCloud, but kept as defense in depth.

## 2026-04-21 — Node 1.2 GATE CLEARED — LiDAR mesh on device
Full iOS app (SiteSyncAR + cooked game content) installed on iPhone 16 Pro via `xcrun devicectl`. Bundle: `com.RussoCompany.SiteSyncAR`. Staged build path: `SiteSyncAR/Saved/StagedBuilds/IOS/SiteSyncAR.app`. Pipeline: UBT build → cook via `UnrealEditor -run=Cook` → UAT `-stage -skipbuild -skipcook`. Personal Team signing via Team ID PD29S4YQ4P. First on-device validation pending James's confirmation of cyan mesh render.

## 2026-04-24 — iOS shim must apply ARMeshAnchor.transform to vertices
`ARMeshAnchor.geometry.vertices` are stored in the **anchor's local coordinate space**, not ARKit world space. The original shim only did axis conversion (ARKit RH Y-up m → UE LH Z-up cm) and skipped the anchor transform entirely, so every anchor's mesh landed stacked at world origin. On-device symptom: cyan mesh rendered but appeared as floating blocks drifting in front of the camera instead of hugging surfaces, and the floor had no coverage because its anchor happened to land offset.

**Fix:** in `Source/SiteSyncAR/Private/ARMeshBlueprintLibrary_iOS.mm`, multiply each vertex by `FoundAnchor.transform` in ARKit space via `simd_mul(simd_float4x4, simd_float4)`, then apply the RH Y-up m → LH Z-up cm axis/scale conversion. Confirmed aligned on iPhone 16 Pro 2026-04-24 — mesh wraps walls, ceiling, floor, bed, and small furniture correctly.

**How to apply:** Any time we pull per-vertex data out of an `ARMeshAnchor`, `ARPlaneAnchor`, or any `ARAnchor` subtype, apply `anchor.transform` first. The engine's own `FAppleARKitConversion::ToFTransform` does the same thing for anchor poses — we just had to replicate it for raw geometry.

## 2026-04-24 — Node 1.2 VISUAL GATE CLEARED — mesh aligns with real surfaces
After the anchor.transform fix above, cyan translucent mesh accurately wraps the user's bedroom: floor, ceiling, walls, bed, hex wall panels, and the door all show correct surface-hugging geometry. Node 1.2 is now truly complete on-device. Ready to proceed to Node 1.3 (touch-gesture foundation anchoring).

## 2026-04-21 — ⚠️ Canonical working tree is C:\Dev\SiteSync-AR\ on PC (NOT the OneDrive path)
There are two project directories on the PC: the OneDrive copy at `C:\Users\jruss\OneDrive\Desktop\Project Kickoff\SiteSync-AR\` (stale clone, do NOT edit) and the real working tree at `C:\Dev\SiteSync-AR\` (where UE 5.6 actually opens, where commits are made, where LFS smudges land). Both point at the same GitHub remote but OneDrive tends to lag and can cause UE file-lock issues. **Always `cd /c/Dev/SiteSync-AR` (or use `git -C`) before terminal work on PC.** The OneDrive copy can be deleted at any time; it exists only because Claude Code was originally launched from the OneDrive path.

## 2026-04-24 — Node 1.3 foundation uses edge-aligned rotated rectangle (Option B)
Two-tap placement: A→B is the pad's **long edge**, not the diagonal of an AABB. Rectangle auto-rotates to the tap vector (`yaw = atan2(Delta.Y, Delta.X)`). `WidthCm` (default 500cm) drives the short edge. Rejected alternative: axis-aligned AABB with A and B as opposite corners — simpler but can't match real site-pad orientation, which rarely aligns to world north/east. Width slider is deferred to a Node 1.4 polish pass; fixed default is acceptable for device validation.

**Why:** Cut-and-fill pads on actual sites run along streets, grade breaks, or property lines — none of which are compass-aligned. Forcing an AABB would produce visibly wrong pad orientation in ~every real tap.

**How to apply:** Any future foundation geometry (polygon support, second slab, etc.) should inherit the same "A→B is an edge" semantic — never "A and B are diagonal corners".

## 2026-04-24 — Node 1.3 hit test uses plane-tile LineTrace, not raw mesh raycast
UE 5.6's `LineTraceTrackedObjects` Blueprint API exposes only plane/feature channels (`TestFeaturePoints`, `TestGroundPlane`, `TestPlaneExtents`, `TestPlaneBoundaryPolygon`) — no direct `WorldMeshGeometry` flag. On LiDAR devices (iPhone 16 Pro) ARKit's plane detection derives from the scanned mesh, so plane-tile hits sit within a few cm of the true LiDAR surface. Good enough for Node 1.3 pad placement.

**Known gap:** sharply curved or micro-feature terrain may not be covered by any plane anchor → tap misses return `Hit=false`.

**How to apply:** If on-device testing shows frequent misses on uneven terrain, add a C++ helper in Node 1.4 that iterates `ARMeshAnchor.geometry.faces`, does ray-triangle intersection per triangle, returns nearest hit. Expose as `BlueprintCallable` and swap it into `GetWorldLocationFromTap` in place of `LineTraceTrackedObjects`.

## 2026-04-24 — Node 1.3 PC GATE CLEARED — all Blueprints compiled and saved
`feat(node-1.3): two-tap foundation anchoring with touch gesture placement` (commit 285207b on `main`). Assets: `BP_Foundation`, `BP_TapMarker`, `BP_ARPlayerController`, `BP_ARGameMode`, `M_FoundationDebug`, `M_MarkerDebug`, `IA_TapPlace`, `IMC_Placement`. `GlobalDefaultGameMode` flipped to `BP_ARGameMode` in `DefaultEngine.ini`. Level `SiteSync.umap` saved with `BP_LiDARMeshManager` already in scene. Editor PIE cannot test this (no AR session on Windows) — device validation pending on Mac.

## 2026-04-25 — chongdashu UnrealMCP can't set component materials (verified empirically)
While driving Fix 2 on `BP_TapMarker`, confirmed that the chongdashu plugin has **no material-handling tool at all** (none in `Plugins/UnrealMCP/Python/tools/*.py`), and the generic `set_component_property` rejects `OverrideMaterials` with `"Unsupported property type: ArrayProperty"`. The `[0]` indexer syntax (`OverrideMaterials[0]`) is also not parsed — returns `"Property OverrideMaterials[0] not found on component"`.

**What works via MCP for components:**
- `set_static_mesh_properties` — sets the StaticMesh asset reference (full path with `.Sphere` suffix, e.g. `/Engine/BasicShapes/Sphere.Sphere`)
- `set_component_property` for scalar / vector / rotator: pass values as **JSON arrays** (`[0.05, 0.05, 0.05]`), NOT UE struct syntax (`(X=0.05,Y=0.05,Z=0.05)` is rejected)
- `compile_blueprint` — reliable

**What does NOT work via MCP:**
- Any TArray property (materials, override arrays, child component lists) — ArrayProperty unsupported
- Branch / Break Struct / math operator nodes in BP graphs — no generic add-node primitive for control flow or struct ops
- Saving to disk — `compile_blueprint` recompiles in memory but does not trigger Save All; James must Ctrl+S in editor before `.uasset` changes hit the working tree

**How to apply:** when planning MCP work, scope to: actor spawn/move/destroy, component scalar/vector edits, static mesh swaps, blueprint compile, level queries. Anything material-related, any TArray edit, or any non-trivial BP graph rewire — hand off to manual editor work, don't half-attempt via MCP.

## 2026-04-25 — UnrealMCP wired directly to Claude Code (Cursor demoted to optional)
Cursor was the only MCP client until now; routing every UE5 control action through Cursor forced a context switch out of Claude Code mid-session. Same Python MCP server (`SiteSyncAR/Plugins/UnrealMCP/Python/unreal_mcp_server.py`) is registered in `.mcp.json` at repo root using a relative `--directory`, so the same file works on PC and Mac. Claude Code and Cursor can both hold connections to port 55557 simultaneously — adding Claude Code is purely additive, Cursor still works if needed.

**Why:** Removes Cursor as a required hop for UE5 edits done from Claude Code. Lets Claude Code do component property changes (material slots, scale, transforms) and verify them in the same chat where it's also doing git work and writing PC/Mac handoff prompts.

**How to apply:**
- After clone or first session in a new env, run `/mcp` inside Claude Code (or restart) so it registers the server. Tools then surface as `mcp__unrealMCP__*`.
- `uv` must be on PATH. PC: `C:\Users\jruss\.local\bin\uv.exe`. Mac: `~/.local/bin/uv` or Homebrew install.
- Reliability tier (chongdashu/unreal-mcp is community-built): solid for actor spawn/move/destroy and component property edits; hit-or-miss for Blueprint graph node-by-node rewiring with branches and struct-pin splits — fall back to manual editor work for non-trivial graphs.
- Don't delete `.cursor/mcp.json` — keeping it as backup costs nothing and lets Cursor still drive MCP if Claude Code is unavailable.

## 2026-04-21 — UnrealMCP installed and verified working
`chongdashu/unreal-mcp` cloned into `SiteSyncAR/Plugins/UnrealMCP/`. C++ plugin auto-starts TCP server on port 55557 when UE5 editor loads — no manual Python server start needed. Python MCP server at `Plugins/UnrealMCP/Python/unreal_mcp_server.py`; `.cursor/mcp.json` uses `uv --directory ... run unreal_mcp_server.py`. `uv` installed at `C:\Users\jruss\.local\bin\uv.exe`. One compile fix required: renamed file-scope `BufferSize` → `MCPBufferSize` in `MCPServerRunnable.cpp` (shadowed a UE5 engine global, `-Werror` treated it as fatal). Terrain proxy test (5 MCP_TerrainProxy actors) passed 2026-04-21 — MCP stack fully operational.
