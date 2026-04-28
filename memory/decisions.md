# Architectural & Technical Decisions

<!-- Log key decisions here so they don't get relitigated. Format: date, decision, rationale. -->

## 2026-04-28 — UnrealMCP "half-broken" was a stale plugin DLL (H1), not a config conflict (H2)

Symptom set on PC: `delete_blueprint_node` returned `"Unknown command: delete_blueprint_node"`, `delete_actor` and `find_actors_by_name` returned `"Actor not found"` / `[]` for actors that `get_actors_in_level` had just listed. Investigation followed the two-hypothesis protocol — H2 (user-scope config override) checked first since it's the cheapest test; H1 (binary stale relative to `fec1af2`) confirmed second.

**H1 confirmed (root cause):** `SiteSyncAR/Plugins/UnrealMCP/Binaries/Win64/UnrealEditor-UnrealMCP.dll` mtime was **2026-04-24 10:22:46**, but commit `fec1af2 feat(mcp): port in-place graph edits + Enhanced Input lookup` landed **2026-04-27 13:10:00** and added the C++ handlers for `delete_blueprint_node` / `disconnect_blueprint_nodes` / `K2Node_EnhancedInputAction` lookup. Source on disk had the handlers; the running DLL did not. UE5 had loaded the older binary on editor start and was still using it. UTF-16 string scan of the stale DLL would have shown the new command names absent.

**H2 ruled out:** `~/.claude.json` top-level `mcpServers` was `{}` and no project entry under `projects.*.mcpServers` had any non-empty server map — only `enabledMcpjsonServers` flags pointing at the project-scope `SiteSyncAR/.mcp.json`. The `.cursor/mcp.json` is untouched and irrelevant to Claude Code.

**Fix:** rebuilt with `C:\UE_5.6\Engine\Build\BatchFiles\Build.bat SiteSyncAREditor Win64 Development -project=...uproject -waitmutex` (13.4s, warnings only). New DLL at `09:00:08`; UTF-16 grep confirms `delete_blueprint_node`, `disconnect_blueprint_nodes`, `find_blueprint_nodes` are now embedded.

**How to detect the same condition in future sessions:**
1. `stat -c '%y'` on `SiteSyncAR/Plugins/UnrealMCP/Binaries/Win64/UnrealEditor-UnrealMCP.dll` and compare to `git log -1 --format=%ci -- SiteSyncAR/Plugins/UnrealMCP/Source/`. If the DLL mtime is older than the most recent source commit, the binary is stale.
2. Symptom shortcut: any MCP write returning `"Unknown command: <name>"` while the Python `node_tools.py` advertises `<name>` ⇒ DLL is from before the C++ handler was added. Always rebuild before debugging the handler logic itself.
3. Build command (PC): `"C:\UE_5.6\Engine\Build\BatchFiles\Build.bat" SiteSyncAREditor Win64 Development -project="C:\Dev\SiteSync-AR\SiteSyncAR\SiteSyncAR.uproject" -waitmutex` — UE editor must be closed first because UE holds the DLL locked.

**Symptom (C) (`delete_actor` + `find_actors_by_name` failing on actors that `get_actors_in_level` enumerates) did not reproduce after the rebuild.** Direct TCP probe `Plugins/UnrealMCP/Python/scripts/probe_post_rebuild.py` cleanly round-tripped a spawned cube through `spawn_actor → find_actors_by_name → delete_actor`; pre-existing `MCP_TerrainProxy_*` actors are also visible to lookup-by-name. Best read of the original 2026-04-28 morning observation: the actor under test (`StaticMeshActor_0`) had been manually renamed/replaced between the `get_actors_in_level` call and the `delete_actor` call as James ran his diagnostic, so the names captured at one moment didn't refer to the same UObject by the next. No code change required for (C).

**Second bug found while verifying (B):** `fec1af2` added `delete_blueprint_node` and `disconnect_blueprint_nodes` handlers in `UnrealMCPBlueprintNodeCommands.cpp` but never registered the new command names in the dispatcher in `UnrealMCPBridge.cpp::ExecuteCommand`. The "Blueprint Node Commands" branch only listed the original commands (`connect_blueprint_nodes`, `find_blueprint_nodes`, …), so the new commands fell through to the `else` clause and the bridge replied "Unknown command" before ever reaching the handler. Fixed by adding both names to that branch, then second rebuild. Probe now returns `"Blueprint not found"` for `delete_blueprint_node` with a bogus name — error from the handler, not from the dispatcher. Anyone porting future commands into the plugin must update **both** the per-class `HandleCommand` switch **and** the dispatcher in `UnrealMCPBridge.cpp` — the dispatcher is the gate, the per-class switch is the second-pass.

## 2026-04-28 — Node 1.4 cut/fill reference plane = slab BOTTOM face (subgrade)
Cut-and-fill volume math computes against the **slab bottom face** (subgrade elevation), not the slab top (Finished Floor Elevation). This matches the earthworks-industry convention used by AGTEK, Trimble, and field graders: "cut 47 yd³, fill 32 yd³" describes the dirt moved to bring a site to subgrade *before* the concrete pour. Including the slab's own volume in the fill number would inflate it by the slab volume and confuse anyone who's done site grading before.

**Why slab bottom specifically:**
- Slab top is derivable for free (`slabTop = slabBottom + SlabThicknessCm`) so we lose no information.
- Matches `BP_Foundation`'s pivot semantics: slab is centered on actor origin, so `slabBottomZ_cm = ActiveFoundation.GetActorLocation().Z - (SlabThicknessCm / 2.0)`. One subtraction, no ambiguity.
- User-settable datum offset is **deferred to Node 1.5+** as a polish pass (a "datum offset" slider, default 0, range ±5m, that shifts `slabBottomZ_cm`). Same math, just a shifted plane — no architectural cost to deferring.

**How to apply (Node 1.4 implementation spec):**
- Cut volume = terrain mesh volume **above** the plane, clipped to the slab's XY footprint.
- Fill volume = empty space **between** the plane and terrain **below** within the same footprint.
- Footprint clipping is in slab-local space: project each terrain triangle into `BP_Foundation`'s local XY, clip to `[-Length/2, +Length/2] × [-WidthCm/200, +WidthCm/200]` (meters), then signed Z-integrate against the plane. The slab's yaw rotation is already baked into the local frame.
- Unit conversion: UE works in cm. `1 yd³ = 91.44³ cm³ ≈ 764554.858 cm³`. Divide cm³ by that constant once to get yd³. **Do not apply an extra 27x factor** — the 27 is ft³→yd³, irrelevant when going straight from cm to yd via the 91.44 cm/yd direct conversion.
- Output: two scalar yd³ values (cut, fill), displayed in a minimal UMG widget over the AR view.

## 2026-04-28 — Issue B closed: AR app needs a custom Pawn with bLockToHmd CameraComponent
Issue B (cyan mesh + yellow markers + orange foundation slab all appearing to drift with the phone as the user walked) was misdiagnosed for two sessions as a coordinate-frame bug in either the iOS C++ shim or the BP graph. Bisection across `BP_LiDARMeshManager`, `BP_ARPlayerController`, `BP_ARGameMode`, the iOS shim, and a static-cube anchoring probe (`7037341`) — plus a null-pawn test (`cbe3c93`) — established the real cause: **the project had no Pawn whose camera tracked the ARKit device pose.** UE rendered virtual content against a static default world camera while ARKit correctly tracked the device, so passthrough shifted under stationary virtual content. Both the 2026-04-24 anchor-transform fix and the 0caf30c AlignmentTransform/TrackingToWorld fix were real and necessary, but invisible without a working camera rig.

**Fix (`66b70c9`):** `BP_ARPawn` (Pawn parent) with components `DefaultSceneRoot → ARCamera` where `ARCamera` is a `UCameraComponent` with `bLockToHmd = true`. Auto Possess Player 0. `BP_ARGameMode.DefaultPawnClass = BP_ARPawn`. `PlayerStart` in `SiteSync.umap` at world (0,0,0) so the pawn spawns coincident with statically-placed actors.

**Why:** `bLockToHmd` is what binds the CameraComponent to the live device pose on AR/XR platforms — there is no other automatic mechanism. Without it, even with `AppleARKit` plugin enabled and a session running, the engine has no idea which UE camera should follow the device.

**How to apply:**
- Any future AR-session level must use `BP_ARGameMode` (or a subclass) so `BP_ARPawn` is auto-spawned, OR manually possess a pawn with the same `bLockToHmd` CameraComponent setup.
- Never set `DefaultPawnClass = None` on an AR GameMode — that was the `cbe3c93` bisection state and it produces the same drift symptom even though the fix is in place.
- Keep `0caf30c` (iOS shim AlignmentTransform + TrackingToWorld application) — those transforms are non-identity once a real AR pawn drives the camera. Removing them re-opens the coordinate-frame mismatch even with `BP_ARPawn` correct.

## 2026-04-28 — Node 1.2 + Node 1.3 both fully device-validated end-to-end
After `66b70c9` landed, on iPhone 16 Pro: cyan LiDAR mesh stays glued to walls/floor/ceiling/furniture as the user walks; yellow Marker A and Marker B stay at their tap points; orange foundation slab stays anchored along the A→B edge with correct yaw rotation; third tap clears state cleanly; no crashes on empty-space taps; 3+ place→reset cycles ran without stale state. Both nodes are closed for real. Node 1.4 (volumetric cut-and-fill output) is the next active node.

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

## 2026-04-28 — UMG widget value display: use push-from-source, NOT poll-via-property-binding (iOS jetsam learning)
First Node 1.4 device deploy on iPhone 16 Pro got iOS-jetsammed within seconds of slab placement. Diagnosis (Mac session, commit 27c7464 + handoff): the WBP_VolumeReadout text bindings called `GetActorOfClass(BP_Foundation)` every frame and read `CutCubicYards`/`FillCubicYards` directly off the result. Pre-placement (and during widget reconstruction order) the result is None, so the engine emits "Accessed None trying to read property" warnings every frame, each followed by a full script-callstack dump. ~10 dumps/frame × 60fps = ~600 expensive script-stack dumps per second → main-thread saturation → iOS watchdog kill. Not a SEGV; `applicationWillTerminate:` callstack confirms iOS-initiated termination.

**Decision:** for any widget displaying live values from a transient/optional source actor, **push values from the source on update, do NOT poll via property bindings.**
- Widget owns its display state as plain text (`SetText`), initialized to a placeholder.
- Source actor (the one that owns the data) holds a typed reference to the widget instance and calls `SetX(value)` only when its value changes (or on its existing recalc tick).
- Acquire the widget reference via the natural owner — usually a PlayerController promotes `CreateWidget` output to a `Public, Instance Editable` variable, and other actors read it via `GetPlayerController(0) → Cast → GetHUDWidget` once on `BeginPlay` and cache.
- Guard with `IsValid` on the cached reference at call sites; never read properties off `None`.

**Why not property bindings:** they're convenient for static data tied to `self` but pathological for "actor that may not exist yet" lookups. UE re-evaluates bound functions every frame regardless of whether the source actor exists, and the cost of an "Accessed None" warning chain is 10–100× the cost of a clean read.

**How to apply:** any future Node (volume readout polish, GPS/compass HUD in 2.2, MEP layer toggles in 2.3) that displays values from a placed-by-tap actor follows the push model. Property bindings remain fine when the binding function reads from `self` only or from a guaranteed-resident actor (PlayerController, GameMode, etc.).

## 2026-04-28 — chongdashu UnrealMCP UMG tools have Python↔C++ param-name mismatches
The Python tool wrappers in `Plugins/UnrealMCP/Python/tools/umg_tools.py` send param keys that don't match what the C++ command handlers in `Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPUMGCommands.cpp` expect. Confirmed empirically 2026-04-28:

- `create_umg_widget_blueprint`: Python sends `widget_name`, C++ reads `name` → "Missing 'name' parameter" error.
- `add_text_block_to_widget`: Python sends `widget_name` + `text_block_name`, C++ reads `blueprint_name` + `widget_name` → "Missing 'blueprint_name' parameter" error.
- Likely affects `set_text_block_binding`, `add_widget_to_viewport`, `bind_widget_event` too — same files, same author, same issue class.

**Why:** chongdashu's Python tool layer was written with one naming scheme (everything is `widget_name`) and the C++ command dispatcher uses another (`blueprint_name` for the Widget Blueprint asset, `widget_name` for the child component inside it). The two never got reconciled.

**Workaround for now:** for any UMG operation, fall back to manual editor work — drag widgets in the Designer, set properties in the Details panel. MCP add-text-block etc. are unusable until the Python is patched.

**How to apply / fix permanently:** patch `Plugins/UnrealMCP/Python/tools/umg_tools.py` so each tool's `params = {...}` dict uses the keys the C++ side expects. Then the Python MCP server picks up the changes on next Claude Code session start (no rebuild needed since this is Python only). Also worth filing upstream to chongdashu, but our local copy is canonical for this project so the patch lands in our tree. Tagged for Node 1.5 polish alongside the variable-type fix below.

## 2026-04-28 — chongdashu UnrealMCP add_blueprint_variable creates Integer when asked for "Float"
Calling `mcp__unrealMCP__add_blueprint_variable(blueprint_name, name, variable_type="Float", is_exposed=false)` returns `{"status": "success", "result": {"variable_name": ..., "variable_type": "Float"}}` — but the variable actually created in the BP is **Integer**, not Float. Confirmed empirically on BP_Foundation.CutCubicYards and FillCubicYards (2026-04-28). When wired to a single-precision-float function output, UE auto-inserts a `Truncate (FTrunc)` node and the var holds the int-truncated value, silently losing all fractional yardage.

**Why:** the chongdashu Python `add_blueprint_variable` tool's type-string mapping doesn't recognize "Float" and falls back to Integer (or the C++ side's switch-case has the same gap). MCP returns success because the variable was created; the type check happens elsewhere.

**Workaround:** after MCP creates the var, manually change its type in My Blueprint → Variables → Details → "Variable Type" dropdown to `Float (single-precision)` (or `Float (double-precision)` if the caller wants double). Truncate nodes UE auto-inserted will then disconnect — delete them and re-wire data direct.

**How to apply:** never trust the `variable_type` echoed back from `add_blueprint_variable`. After creating any non-Integer variable via MCP, verify the actual type in the editor (or via a graph dump) before wiring. Better: fix the chongdashu mapping in `Plugins/UnrealMCP/Python/tools/blueprint_tools.py` so "Float" maps correctly — would close out the bug for the whole project. Tagged for Node 1.5 polish.

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
