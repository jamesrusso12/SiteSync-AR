# Architectural & Technical Decisions

<!-- Log key decisions here so they don't get relitigated. Format: date, decision, rationale. -->

## 2026-05-21 — Cursor fully removed from the project

Cursor was only ever in this project as a second MCP client for driving the UnrealMCP server. Claude Code now drives the `unrealMCP` tools directly (and the C++ TCP bridge on port 55557 can be hit directly via `dev/mcp_client.py` for diagnostics), so Cursor has no remaining role.

**Removed:** `.cursor/mcp.json` (deleted; the `.cursor/` dir is now gone), the `.cursor/history/` rule + "Cursor / AI tooling" comment from `.gitignore`, and all Cursor mentions from `CLAUDE.md`, `README.md`, `memory/preferences.md`, `memory/people.md`.

**Safe because:** `.cursor/mcp.json` is a Cursor-only config — it has zero effect on the UE build, the UE project, or Claude Code's MCP (Claude Code connects via the repo-root `.mcp.json`; the C++ plugin auto-starts the TCP server regardless). Deleting it cannot break anything.

**Supersedes** the 2026-04-25 entry "UnrealMCP wired directly to Claude Code (Cursor demoted to optional)" — specifically its "How to apply" guidance to keep `.cursor/mcp.json` as a backup. That backup is no longer wanted. The 2026-04-25 entry is retained below as history; do not act on its `.cursor/mcp.json` advice.

## 2026-05-19 — Idaho Technology Council demo postponed indefinitely; current state retained as future demo baseline

Original demo was scheduled for Tuesday 2026-05-19 at the Idaho Technology Council conference. James notified mid-debug that the event was postponed "until further notice" (no new date provided). All "must ship today" pressure removed.

**How to apply:** Demo-state code paths (Model Scale, WBP_BIMPlacementHUD, wireframe-edge M_BIMOverlay, Fab "Free Small Old House" placeholder) remain valid as a known-good baseline for future demo events — keep them on `main`, don't strip. The pacing of remaining Node 2.x work shifts from "ship by Tuesday" to "land the proper Node 2.1 Datasmith deliverable per the gate criteria in CLAUDE.md." All `## Demo Deadline` framing in CLAUDE.md / README.md should be softened to "Demo-ready snapshot" language.

## 2026-05-19 — UE5 BP pin "weird save" — values persist on the editor surface but cooked bytecode keeps stale values; re-type + re-save clears it

Symptom: opened `BP_ARPlayerController_BIM`, changed `PlaceBIMByCornerForward` pin literals from 100 → 30, Compile (green), Cmd+S, File → Save All, Cmd+Q. Cooked + deployed. Device runtime log (`xcrun devicectl device copy from --domain-type appDataContainer`) showed `PlaceBIMByCornerForward: L=100.0cm W=100.0cm H=100.0cm` — the OLD values. The on-disk `.uasset` correctly contained `DefaultValue="30.000000"` (verified with `strings`).

Two possible explanations (not yet root-caused):
1. Compile + Save somehow flushed source pin values to disk but stale compiled bytecode also persisted, and the cook used the bytecode rather than recompiling from source.
2. The cook produced correct bytes but the install/launch step didn't replace the running app's cached package data.

**Fix that worked:** re-open the BP, **re-type the pin values** (even to the same value 30), re-compile, re-save. Next cook+deploy picked up the new values correctly. Suggests the editor's "is this asset dirty?" tracking missed the original change, so the actual save was a no-op despite the file's mtime updating from related metadata.

**How to apply:** When a runtime log shows pin values that don't match the on-disk source, don't trust either side. Re-type + re-save + re-cook. If the values STILL don't propagate, dig into `Saved/Cooked/IOS/` and confirm the cooked `.uasset` matches expected.

## 2026-05-19 — Static Mesh Build Settings: `Apply Changes` rebuilds in-memory only; you must Cmd+S the Static Mesh tab itself to persist

Symptom: changed Build Scale 3D on the Fab-imported house from `(1, 1, 1)` to `(0.1, 0.1, 0.1)` and clicked **Apply Changes**. Approx Size readout in the viewport updated. Closed the editor. Cooked + deployed. Device showed the house at full native size (~77m × 82m × 24m) — the Build Scale had not persisted to disk. CLI check confirmed: `stat -f "%Sm" scene.uasset` showed the import-time mtime, unchanged.

**Cause:** `Apply Changes` in the Build Settings panel rebuilds the cached mesh data (bounds, distance field, collision) in memory so the viewport updates, but does NOT mark the .uasset itself as dirty in a way that Cmd+S triggered from elsewhere would catch. You must have the Static Mesh tab focused and press Cmd+S explicitly to persist the new Build Settings to disk.

**Fix:** Re-open the Static Mesh, click into the Build Scale field (to re-dirty it), confirm `(0.1, 0.1, 0.1)`, click Apply Changes again, then immediately **Cmd+S with the Static Mesh tab focused**. Verify with `stat -f "%Sm" <path>.uasset` that the mtime updated.

**How to apply:** Any time you change Build Settings on a Static Mesh asset, the workflow is **Apply Changes → Cmd+S on that exact tab**, not just Apply Changes + global Save All. Save All has been observed to miss Static Mesh Build Settings changes.

## 2026-05-19/20 — glTF imports from Fab arrive 100× oversized; correct Build Scale is 0.01, not 0.1

Symptom: imported "Free Small Old House" (Jimbogies, CC BY 4.0) from Fab. Asset's `Approx Size` overlay in Static Mesh editor showed `77,431 × 82,643 × 24,407` cm — i.e., 774m × 826m × 244m. Visually obviously a small one-story house, so the bounds were ~100× too large.

**Cause:** glTF unit-scale mismatch between the source modeling tool and UE's glTF importer. The net result for this asset (and likely most Fab glTF building assets) is a **100× oversize** in UE units — a 7.7m house imports as 774m.

**Fix — and the trap that cost a full debugging session:** the correct Build Scale 3D is **`(0.01, 0.01, 0.01)`**, not `(0.1, 0.1, 0.1)`. An initial 0.1 was applied and looked plausible (774m → 77m) but 77m is still a skyscraper. It took a second pass to realize the oversize was 100×, not 10×, and the mesh needed `0.01`. After `0.01`: Approx Size drops to ~`774 × 826 × 244` cm = a realistic 7.7m house.

**How to apply:** First action after any glTF import from Fab — open the Static Mesh, check Approx Size against the asset's real-world expected size. Fab building assets have been ~100× oversized; expect to apply Build Scale `0.01`. Apply Changes + **Cmd+S with the Static Mesh tab focused** (Apply Changes alone does not persist — see the Static Mesh save entry above). Verify with `stat` that the mesh `.uasset` mtime updated.

## 2026-05-20 — PlaceBIMByCornerForward C++ silently clamped L/W/H to min 100.0f; logged only the post-clamp value, hiding it for ~2 hours

**The single worst debugging trap of the Node 2.1 work.** After importing the real house and switching to Model Scale, the BIM placement refused to render at dollhouse size no matter what `LengthCm/WidthCm/HeightCm` pin values were set in `BP_ARPlayerController_BIM`. Device logs always showed `PlaceBIMByCornerForward: L=100.0cm W=100.0cm H=100.0cm` regardless of whether the BP pins were 30, 17, or 100.

Hours were spent suspecting a UE5 "BP pin save bug" — re-typing values, re-compiling, full cooks, DDC theories, comparing cooked vs staged `.uasset` md5s. All a wild goose chase.

**Actual cause:** `ARMeshBlueprintLibrary.cpp::PlaceBIMByCornerForward` clamped the inputs:
```cpp
const float ClampedLength = FMath::Clamp(LengthCm, 100.0f, 50000.0f);  // min 100!
```
Any value below 100 was floored to 100. The BP was correctly passing 30 the whole time — C++ clamped it up. And the `UE_LOG` line printed `ClampedLength` (the post-clamp value), so the log showed `100` and gave no hint a clamp had occurred.

**Fix (commit pending):** lowered the clamp minimum to `10.0f` (supports actor scale down to 0.1× for tabletop/dollhouse Model Scale). Rebuilt iOS C++. Runtime then correctly showed `L=30.0cm scale_m=(0.300,...)`.

**How to apply — two durable lessons:**
1. **Any `BlueprintCallable` C++ function that clamps/transforms its inputs MUST log the raw input alongside the result** — e.g. `L=%.1f→%.1fcm` (raw→clamped). A log that prints only the post-transform value is actively misleading. Audit `InitFoundationFromCorners` and `CalculateCutFillVolumes` for the same pattern.
2. **Clamp bounds belong in the `.h` doc comment.** `ARMeshBlueprintLibrary.h`'s `PlaceBIMByCornerForward` comment said nothing about a 100cm floor. A BP author has no way to know `30` becomes `100`. Document all clamp ranges in the header.
3. When a runtime value contradicts the on-disk source, **suspect the C++ callee before suspecting the BP/cook pipeline.**

## 2026-05-20 — BIMMesh is the root component of BP_BIMOverlay → SetActorScale3D overwrites its RelativeScale3D

While debugging the oversized house, the BIMMesh component's `RelativeScale3D` was set to `0.1` as an attempted scale fix. It did nothing.

**Cause:** `BIMMesh` is the **root component** of `BP_BIMOverlay` (no separate scene-root). `PlaceBIMByCornerForward` calls `BIMActor->SetActorScale3D(ScaleMeters)`, which sets the **root component's** scale directly — overwriting whatever `RelativeScale3D` was authored on BIMMesh. Every placement clobbers it.

**Implication:** the only reliable mesh-size levers for the BIM overlay are (a) the Static Mesh's **Build Scale** (asset-level, baked into mesh data) and (b) the **actor scale** set by `PlaceBIMByCornerForward`'s L/W/H pins. The component `RelativeScale3D` is a dead lever — leave it at `1.0`.

**Final working scale chain for the house:** `774m glTF import × 0.01 Build Scale = 7.74m mesh → × 0.3 actor scale (L=30 pin) = 2.32m rendered` dollhouse. Component scale stays 1.0.

**How to apply:** If a single-component BP actor needs a persistent base scale separate from its placement scale, either bake it into the mesh's Build Scale, or add a dedicated `USceneComponent` as root with the mesh as a child (then the child's RelativeScale3D survives SetActorScale3D).

## 2026-05-20 — Imported glTF house renders pure black on device — Lit materials + AR level has no lights

Symptom: after the scale was finally correct, the dollhouse-sized house rendered as a pure-black silhouette on iPhone.

**Cause:** the Fab house's 8 imported material slots (Door, Glass01, WindowMetal, Concrete02, Road, Wood, Block02, roof) are standard **Lit** PBR materials — they require scene lighting to be visible. `SiteSync_BIMTest.umap` was created minimal (PlayerStart + BP_LiDARMeshManager only) with **no lights**. Everything previously visible in this app — cyan LiDAR mesh, the orange placeholder box — used **Unlit** materials (M_LiDARDebug, M_BIMOverlay), which ignore lighting entirely. The real house's Lit materials got zero illumination → rendered black.

**Fix (in progress, chosen 2026-05-20):** add a **Movable Directional Light** + **Movable Sky Light** to `SiteSync_BIMTest.umap`. AR scenes require Movable lights (no baked lighting). James chose this over the alternative (re-parent all 8 slots to Unlit / M_BIMOverlay) to keep the realistic textures. Tuning of intensities pending device validation.

**How to apply:** any imported asset with Lit materials placed into an AR level needs Movable lights in that level. Unlit materials (like the project's debug/overlay materials) are the exception that "just works" without lights. If a future imported BIM model renders black, check the level's lighting before suspecting the material itself.

## 2026-05-18 — Xcode loses Apple ID account between sessions; manifests as "No Accounts" / "No profiles" build error mid-deploy pipeline

Symptom: ran the standard cook + UAT stage + devicectl install pipeline (the same script that worked Friday). Cook succeeded. UAT BuildCookRun's internal xcodebuild step failed with:
```
error: No Accounts: Add a new account in Accounts settings.
error: No profiles for 'com.RussoCompany.SiteSyncAR' were found
```
UAT reported `BUILD SUCCESSFUL` anyway because it copied an older `Binaries/IOS/SiteSyncAR.app` to the archive directory, but `Saved/StagedBuilds/IOS/SiteSyncAR.app` was never produced. `devicectl install` then failed with `CoreDeviceError 3002`: "Access to the resource was denied by this process' sandbox" + "The file couldn't be opened because it doesn't exist."

**Cause:** Xcode 26's Accounts pane (Settings → Accounts) had no Apple ID, despite the macOS account (`MobileMeAccounts` defaults) being correctly signed in to `jrusso03@icloud.com`. Something between Friday's working deploy and Monday evening's broken one — possibly an Xcode update, possibly a Sign-Out + Sign-In on macOS, possibly a Keychain reset — evicted the Apple ID from Xcode specifically.

**Fix:** Open Xcode → Settings (Cmd+,) → Accounts tab → **+ → Apple ID** → sign in with the Personal Team Apple ID. After the account appears with Team ID `PD29S4YQ4P` listed, click **Download Manual Profiles** to refresh signing assets. Quit Xcode. Re-run the cook + deploy pipeline.

**How to apply:** If a deploy that worked previously suddenly fails with `No Accounts` / `No profiles`, check the Xcode Accounts pane first before debugging signing-cert paths or provisioning profile expiry. This will likely recur after future Xcode updates.

## 2026-05-18 — Phase 2 BIM overlay supports two scale modes (Site / Model); demo uses Model Scale

Phase 2's `PlaceBIMByCornerForward` placement primitive drives both production and demo workflows from a single C++ function. The only difference between modes is the `LengthCm/WidthCm/HeightCm` parameters passed at the call site, which set the actor's scale:

- **Site Scale (production, 1:1)** — `L=100, W=100, H=100` → actor scale `(1, 1, 1)` → mesh renders at native size. For a real construction site where a surveyor has set a corner stake and shot a baseline; the planned building is overlaid lifesize and crews can walk *through* the structure before any concrete is poured.
- **Model Scale (demo / indoor review, ~0.3×)** — `L=30, W=30, H=30` → actor scale `(0.3, 0.3, 0.3)` → mesh renders at ~30% native. For indoor design reviews, client presentations, contractor walkthroughs in an office, or trade-show demos where the viewer needs to see the whole building from outside.

For the Idaho Technology Council demo 2026-05-19, `BP_ARPlayerController_BIM` uses Model Scale. The imported Fab "Free Small Old House" mesh (Jimbogies, CC BY 4.0, native ~7.7m × 8.3m × 2.4m) renders at ~2.3m × 2.5m × 0.7m, and the `ShowPostPlacement` HUD readout uses (2.3, 2.5, 0.7) to match.

**Why:** A 1:1 lifesize building dropped at a tap point in an indoor conference space puts the user inside opaque walls — the model is rendering but invisible because the viewer's camera is inside the building. Model Scale lets them step back ~1–3m and see the whole structure. Both modes are legitimate workflows in AEC AR tooling — Trimble Sitevision and Autodesk Construction Cloud AR both ship with toggleable site/model review modes. The distinction is industry-standard, not a workaround.

**How to apply:** When demoing indoors or showing the workflow without a real construction site, pass `L/W/H` in the 30–60 range (~0.3–0.6× scale → 2–5m max building dimension). For production / real-site code paths, pass `L/W/H = 100` (native size) and trust the survey corner stake. Future work: surface as a runtime HUD toggle so the user can switch modes without recompiling.

**Demo talking-point framing (for the verbal pitch):**

> "What you're seeing is **model review mode** — a scaled-down BIM walkthrough. Construction professionals use this for indoor design reviews, client meetings, in a trailer on-site. The same app at a job site runs in **1:1 site scale** — the planned building renders at lifesize, anchored to the survey corner stake. Crews walk *inside* the planned building before any concrete is poured. Same `PlaceBIMByCornerForward` placement primitive in both modes — just driven by a scale parameter that's tied to the use case."

The distinction is AEC-native; investors and industry attendees will recognize it. For non-AEC audiences: "dollhouse review mode" vs "real-size site mode" lands the same point.

## 2026-05-14 — Mac editor "Save Current Level As" crashes when stale Desktop paths leak from old project location

Editor crashed every time on `File → Save Current Level As` for a new level. `Saved/Crashes/.../SiteSyncAR.log` showed:

```
LongPackageNameToFilename failed to convert
'/Users/jamesrusso/Desktop/Github/Xcode/SiteSync-AR/SiteSyncAR/Content/NewMap'.
Path does not map to any roots.
```

The project was moved from `~/Desktop/Github/Xcode/SiteSync-AR/` to `~/Developer/SiteSync-AR/` on 2026-04-21 (see entry below). A symlink remains at the Desktop path for backward-compat. UE's package system mounts `/Game/` → `<ProjectDir>/Content/` at editor startup based on the path UE used to open the project; if any cached config still references the Desktop path, Save-As path resolution can route through it and fail because UE didn't mount that path as a content root.

**Three configs had to be purged of the Desktop reference:**

1. `~/Library/Application Support/Epic/UnrealEngine/Editor/ProjectEditorRecords.json` — persistent project record list
2. `~/Library/Application Support/Epic/UnrealEngine/5.6/Saved/Config/MacEditor/EditorSettings.ini` — `RecentlyOpenedProjectFiles` array entry
3. `SiteSyncAR/Saved/Config/MacEditor/EditorPerProjectUserSettings.ini` — `SwarmIntermediateFolder` + last-used-folder paths (`BRUSH`, `FBX`, `FBXAnim`, `GenericImport`)

**Fix:** `sed -i.bak 's|/Users/jamesrusso/Desktop/Github/Xcode/SiteSync-AR|/Users/jamesrusso/Developer/SiteSync-AR|g'` on the two global files; deleted the project per-user file (UE regenerates clean on next open). Save-As immediately worked.

**How to apply:** If ever the Mac editor save-as / cook starts complaining about unknown content roots, `grep -rl "Desktop/Github" "$HOME/Library/Application Support/Epic" SiteSyncAR/Saved/Config` and purge any matches. Backups are kept at `.bak` so the fix is reversible.

## 2026-05-14 — chongdashu MCP `create_blueprint` does not honor `parent_class` for GameModeBase; duplicate from a working asset instead

PC session 2026-05-13 created `BP_ARGameMode_BIM` via `mcp__unrealMCP__create_blueprint(name="BP_ARGameMode_BIM", parent_class="GameModeBase")`. MCP returned success, but the resulting asset's Class Defaults panel had no `Default Pawn Class` or `Player Controller Class` properties visible, and `set_blueprint_property` returned "Property not found" for both. The asset was effectively parented to `UObject` (or `AActor`), not `AGameModeBase`.

**Fix (Mac, 2026-05-14):** deleted the broken `BP_ARGameMode_BIM`, right-click → Duplicate on the working Phase 1 `BP_ARGameMode` in Content Browser, renamed to `BP_ARGameMode_BIM`. The duplicate carried correct parent class + populated Class Defaults. Changed `Player Controller Class` from `BP_ARPlayerController` → `BP_ARPlayerController_BIM`. Compiled clean.

**How to apply:** When MCP `create_blueprint` is needed for any non-Actor parent class (GameModeBase, PlayerController, Pawn, UserWidget, etc.), verify Class Defaults populates correctly *before* moving on. If empty, fall back to Content Browser Duplicate of an existing working asset of the same parent class. Add to the Reliability Tier list in CLAUDE.md / MCP architecture section.

## 2026-05-14 — `SET <var>` nodes need BOTH exec wire AND data-input wire; an unconnected object-input pin silently sets the variable to None

v21 device test: `PlaceBIMByCornerForward` C++ helper logged 5 successful BIM placements with correct scale, BUT `IsValid(ActiveBIM)` returned false on every tap, so `Path: Reset` count = 0 and BIMs piled up forever.

Root cause: `BP_ARPlayerController_BIM.EventGraph` had `SET ActiveBIM` wired into the spawn-tap exec chain correctly (execute pin connected from `PlaceBIMByCornerForward.then`, then pin connected to `SET bHasFirstTap.execute`), but the **`ActiveBIM` data-input pin had no `LinkedTo` clause in the .uasset graph dump.** Every spawn fired `SET ActiveBIM = (default object reference) = None`. UE doesn't surface this as a compile warning because the pin has a valid default — it just silently uses None.

**Fix:** dragged a wire from `SpawnActor BP_BIMOverlay.ReturnValue` (BP_BIMOverlay_C blue object pin) → `SET ActiveBIM.ActiveBIM` input pin. UE auto-inserted a reroute knot for the long span. `SpawnActor.ReturnValue` now has two downstream connections: `PlaceBIMByCornerForward.BIMActor` (existing) and `SET ActiveBIM.ActiveBIM` (new via Knot_6). v22 device log: 5/5/5/5/5/10 perfect cycle symmetry.

**How to apply:** When debugging a BP state machine where the exec chain runs but state variables seem to retain default values, dump the graph and look for a `K2Node_VariableSet` whose data input pin has no `LinkedTo` clause. Pin-by-pin verification is the only way — UE compile is permissive about unconnected data inputs (they're "valid" by virtue of the pin default).

**Long-term mitigation idea:** add a BP best-practices section to onboarding docs warning that "every spawn-and-store pattern needs the data wire wired to the same SpawnActor.ReturnValue that feeds the consumer below — two destinations from one source pin is the correct topology."

## 2026-05-14 — Cook with `-map=...` does NOT update the boot map; `GameDefaultMap` must be flipped separately

v21 cook ran with `-map=/Game/Maps/SiteSync_BIMTest` to include the new BIM test level in the package, but the iPhone still booted into the Phase 1 `SiteSync.umap` because `DefaultEngine.ini → [/Script/EngineSettings.GameMapsSettings] → GameDefaultMap` still pointed at `/Game/SiteSync.SiteSync`. Cook's `-map` parameter only tells the cooker WHICH maps to include and their dependencies; it doesn't set boot behavior.

**Fix:** Edited `DefaultEngine.ini` to flip `GameDefaultMap` + `EditorStartupMap` to `/Game/Maps/SiteSync_BIMTest.SiteSync_BIMTest`. Recooked + redeployed. Phase 2 level became the boot map.

**How to apply:** When adding a new test level to the cook, decide if it should be the new boot map. If yes, edit DefaultEngine.ini's `GameMapsSettings` block before cooking. For Tier B "menu level" work, the boot map will eventually become a `SiteSync_Menu.umap` that offers Phase 1 / Phase 2 selection.

## 2026-05-12 — DatasmithRuntime is desktop-only (Win/Mac); iOS not supported in any version through 5.7

Earlier scoping in CLAUDE.md and Node 2.1 commit `88f98ba` framed DatasmithRuntime as a "Node 2.3 candidate" for runtime BIM loading on iPhone. Research pass on 2026-05-12 (Twinmotion + UE 5.6 deep dive) corrected this: per Epic's Datasmith Supported Platforms doc, DatasmithRuntime supports Windows + macOS only, with Linux experimental. **iOS and Android are not listed and have never been supported.** The "Beta" tag in the Plugins window refers to API maturity on desktop, not a path to mobile.

**Implication for SiteSync AR Phase 2:** there is no future-state where the iPhone app loads `.udatasmith` files at runtime. All Datasmith work must happen at editor cook time on Mac/PC, baked to standard UStaticMesh / UMaterialInstance assets, then cooked through the normal iOS pipeline. The `DatasmithContent` plugin (different — handles cooked Datasmith *content*, not runtime parsing) is the only Datasmith-family runtime piece that ships to iOS.

**Cook-time gotchas worth planning for** (not yet hit in our codebase):
- Datasmith-imported materials default to Translucent + complex shading models that mobile FL5 mis-renders. Re-parent to mobile-compatible MIs at import time.
- Datasmith meshes default Nanite-eligible in 5.5+. Per-mesh disable + auto-LOD. (Project-level Nanite already off in `Config/IOS/IOSEngine.ini`.)
- First iOS cook of a real archviz/BIM model: hours, not minutes (DDC + shader-permutation compile).
- Revit MEP models are 5–50M tris; iPhone 16 Pro AR wants <500k visible. Plan a Dataprep decimation + material-merge recipe between raw `.udatasmith` and shipped iOS content.
- **Datasmith Twinmotion Content** plugin for UE 5.6 must be verified published on Fab before using Twinmotion as archviz front-end — historically lags engine releases by months. If absent, Twinmotion materials arrive broken in the project.

**How to apply:** any Phase 2 design discussion that posits "user loads a `.udatasmith` on device" or "live Direct Link from Revit to the iPhone" is impossible — redirect to the cook-time pipeline. The runtime BIM-loading idea reappears every few months in Datasmith roadmap discussions; check the linked Epic doc before re-litigating.

## 2026-05-11 — EnhancedInput IA_TapPlace abandoned for iOS taps in favor of Tick rising-edge poll

v19 (commit `9dc54b2`) restored the Tick → GetInputTouchState poll thinking that would keep IA_TapPlace.Started alive. v19 device log: **102 `Touch ACTIVE` prints + 4+ distinct post-reset tap attempts, ZERO `Tap Fired` events**. The Tick chain proved the touch queue was flowing — `bIsCurrentlyPressed` was correctly toggling — but the EnhancedInput subsystem still refused to re-fire `IA_TapPlace.Started` after a state transition (specifically, after `Path: Reset → ResetPlacement → ActiveFoundation=null`).

**Workaround attempts that all failed:** changing Value Type (2026-04-29 fix), changing trigger from Pressed → Tap, restoring the Tick poll (v19), Refresh All Nodes on the BP. None made IA_TapPlace.Started re-arm post-reset. EnhancedInput's trigger state machine appears irrecoverable after certain state transitions on iOS — the underlying cause is in the Engine, not our BP.

**v20 fix:** abandon IA_TapPlace entirely. Implement tap detection in Event Tick via rising-edge poll on a `bWasPressed` Boolean variable:

```
Event Tick → Sequence
  .then_0 → Branch(bIsCurrentlyPressed AND NOT bWasPressed) → Tap Fired (existing state machine downstream)
  .then_1 → SET bWasPressed ← bIsCurrentlyPressed
```

The Sequence order matters: `.then_0` reads the PREVIOUS frame's `bWasPressed` to detect the rising edge, then `.then_1` updates `bWasPressed` for next frame. IA_TapPlace.Started link broken via right-click → Break Link. IA asset + IMC_Placement asset remain in the project as dead code for Node 1.5 cleanup.

**How to apply:** when iOS EnhancedInput stops firing IA events but `GetInputTouchState` shows touches arriving in the Tick log, **do not chase IA trigger configurations**. Implement Tick polling and move on. Long-term, consider removing IA_TapPlace + IMC_Placement assets and the BeginPlay `AddMappingContext` call.

## 2026-05-11 — RaycastTerrainFromScreen (C++ Möller-Trumbore) replaces LineTraceTrackedObjects for marker placement

Bug 3 from the original Node 1.4 handoff (decisions.md 2026-04-24) finally closed. `LineTraceTrackedObjects` hits ARKit plane anchors, not raw mesh triangles — plane anchors sit ~5-30cm above the actual scanned surface on uneven terrain (worse on outdoor sites). Markers visibly floated above the cyan LiDAR mesh in v15-v19.

**Mac C++ helper (commit `ae629f9`):** `URaycastTerrainFromScreen(APlayerController*, UProceduralMeshComponent* TerrainMesh, FVector2D ScreenPosition, FVector& OutHitLocation, FVector& OutHitNormal) → bool`. Möller-Trumbore ray-tri intersection against the live `TerrainMesh` ProcMeshSections (the same mesh `BP_LiDARMeshManager` rebuilds every 200ms from ARKit mesh anchors). Returns nearest-along-ray hit world location. UE_LOG line: `RaycastTerrainFromScreen: hit at (X,Y,Z)`.

**Performance:** ~150k triangles per typical AR scene, <10ms per tap on iPhone 16 Pro. No AABB cull yet — Node 1.5 polish if outdoor scenes push tri count higher.

**BP wiring inside `GetWorldLocationFromTap` function body (v20):**
```
Input(ScreenLocation).exec → Get Actor Of Class(BP_LiDARMeshManager).exec → Raycast Terrain From Screen.exec → Return Node
  Get Player Controller(Index=0).Return Value → Raycast.Player Controller
  Get Actor Of Class.Return Value → Get Component By Class(ProceduralMeshComponent).Target
  Get Component By Class.Return Value → Raycast.Terrain Mesh
  Input.ScreenLocation → Raycast.Screen Position
  Raycast.Out Hit Location → Return.WorldLocation
  Raycast.Return Value → Return.bHit
```

**Gotcha:** `Get Actor Of Class` is impure in UE 5.6 and has no "Convert to Pure" context-menu option. Must be wired into the exec chain (between Input and Raycast) — not just connected via pure data wires. Otherwise BP compile warns `"Get Actor Of Class was pruned because its Exec pin is not connected"` and the actor lookup returns the default value (null), making the raycast silently miss.

`Out Hit Normal` left unconnected — reserved for future surface-alignment work (rotating the marker to face the local terrain normal).

## 2026-05-11 — `probe_post_rebuild.py` uses fixed actor name `MCP_FixCheck_01`, do not call in a loop

While waiting for the editor to come up I used a Monitor loop running `probe_post_rebuild.py` to detect MCP readiness. The script spawns `StaticMeshActor` named `MCP_FixCheck_01`, then deletes it. If the editor becomes responsive mid-poll, the first iteration succeeds in spawning; before the delete fires on the next probe call, a second iteration tries to spawn another `MCP_FixCheck_01` and UE5.6 raises a **fatal** assertion:

```
Cannot generate unique name for 'MCP_FixCheck_01' in level 'Level /Game/SiteSync.SiteSync:PersistentLevel'
[Callstack] UnrealEditor-UnrealMCP.dll!FUnrealMCPEditorCommands::HandleSpawnActor() [...UnrealMCPEditorCommands.cpp:186]
```

The editor crashes immediately, no recovery dialog. SiteSync.umap was not corrupted (the crash happens before save), but the session is dead — must relaunch.

**Don't use that script in a polling loop.** Either: (a) just sleep a fixed timeout and let James confirm editor-ready, (b) write a different probe script that uses a random actor-name suffix, or (c) use a probe that doesn't spawn anything (e.g. just `get_actors_in_level` returns success-with-zero-keys once MCP is up).

**For future Monitor strategies:** prefer probes that are idempotent under repetition. Anything with a fixed-name side effect crashes on the second hit.

## 2026-05-11 — BP_ARPlayerController Tick → GetInputTouchState poll is load-bearing on iOS, do not delete

v18 (commit `838abf6`) deleted BP_ARPlayerController's `Event Tick → ExecutionSequence → Branch → GetInputTouchState → PrintString "Touch ACTIVE"` chain on the assumption it was diagnostic-only. v18 device log proved it was actually pumping the iOS touch queue:

- v17 device log: Tap 4 (post-reset MarkerA) fires correctly. "Tap Fired" event prints every tap.
- v18 device log: Tap 4 never fires. Zero `Tap Fired` events in the 8 seconds after `Path: Reset` before the app backgrounded. Foundation IS destroyed (`CalculateCutFillVolumes` call count drops to 0 after reset, confirming `ResetPlacement` worked), but `IA_TapPlace.Started` doesn't re-arm — same latching family as the 2026-04-29 Axis2D bug, but on a different cause.

**Root cause.** Per the 2026-04-29 entry, iOS does NOT clear `Touch1.X/Y` on finger release. EnhancedInput's `Pressed`/`Tap` triggers on `IA_TapPlace` rely on something inside UE driving the touch-state queue every frame. The Tick chain's `GetInputTouchState(Touch1)` call was performing exactly that drive — incidentally, not deliberately. Without it, the EnhancedInput subsystem's view of `bIsCurrentlyPressed` goes stale between taps and the trigger doesn't re-edge.

**Fix.** Restore the v17 chain with `bPrintToScreen = false` so the HUD stays clean (post-v18 spam cleanup intent). `bPrintToLog` stays on for device-log verification of the v19 build.

**How to apply.** Every active `APlayerController` (BP or C++) that consumes iOS Enhanced Input touches MUST contain a per-frame `GetInputTouchState` call. A log-only PrintString downstream is fine — the load-bearing thing is the call itself, not the print. Future cleanups must NOT delete this chain even if it looks like dead diagnostic code. Long-term fix is to rewire tap detection to a rising-edge Tick poll bypassing EnhancedInput entirely (Node 1.5 polish scope).

**Symptom shortcut.** If `IA_TapPlace` (or any iOS Enhanced Input action) fires the first 1–N times then goes silent — and Value Type is already `Digital (bool)` per the 2026-04-29 fix — check whether the active PlayerController has a Tick → GetInputTouchState call. Missing pump is the next most likely cause after stale Value Type.

## 2026-05-11 — Session lessons: MCP toolkit limits + cook-time silent BP fallback + Win64 rebuild discipline

Session covered v7→v8 regression fix and v17 WidthCm/ResetPlacement fix on PC. Four lessons worth not re-learning.

**1. chongdashu MCP cannot edit function-body graphs — only the Event Graph.** `find_blueprint_nodes`, `connect_blueprint_nodes`, `disconnect_blueprint_nodes`, `delete_blueprint_node` all target `EventGraph`. Functions like `InitFromCorners`, `ResetPlacement`, and `CalculateCutFillVolumes`'s caller-side wiring live in their own function graphs and are invisible to MCP. Diagnostic path for these: James `Ctrl+A → Ctrl+C` inside the open function graph and pastes to chat; Claude parses the dump and gives a precise pin-by-pin walkthrough. Don't burn time trying MCP first — go straight to text-paste workflow for function bodies.

**2. MCP toolkit has no Variable-Set node primitive.** `add_blueprint_function_node` adds `K2Node_CallFunction`; `add_blueprint_variable` adds a *member variable to the BP class*, not a `K2Node_VariableSet` in a graph. Any "drag in a Set X node" task is manual editor work. Documented in this session when James asked if MCP could bulk-add the ResetPlacement nulls.

**3. `mcp__unrealMCP__compile_blueprint` returns `compiled: true` even when `bHasCompilerMessage=True` persists in the .uasset metadata.** v7 broke widget + tap input on device because BP_ARPlayerController shipped with a persisted `ErrorType=1` on its `K2Node_MacroInstance_0` (IsValid macro). MCP compile reported success; editor compile UI also reported success; **cook caught the error and silently fell back to native `APlayerController`**, dropping all Blueprint logic in the cooked package. Symptom: cyan mesh works (separate BP), but widget never appears + taps never register + zero PrintStrings on device. Root cause was the K2 schema "Replace existing output connections" mode used by `connect_blueprint_nodes` when splicing side-branch Prints onto event-node exec outputs that already drove macros — replace-mode partial-fails during macro expansion, poisoning the compile state. **Fix:** delete the offending side-branch nodes via MCP + Refresh All Nodes in editor + recompile. **Prevention:** for any diagnostic side-branch near macros, do the wire manually in the editor, not via MCP.

**4. Pull workflow on PC after Mac C++ changes — rebuild SiteSyncAREditor Win64 DLL BEFORE opening the editor.** New `UFUNCTION`s and `UFUNCTION` signature changes don't reflect in BP reflection until the Win64 editor DLL is rebuilt. UE5 holds the DLL locked while running, so editor MUST be closed first. Command: `"C:\UE_5.6\Engine\Build\BatchFiles\Build.bat" SiteSyncAREditor Win64 Development -project="C:\Dev\SiteSync-AR\SiteSyncAR\SiteSyncAR.uproject" -waitmutex` (10-15s on clean tree). Skipping this gives "Function InitFoundationFromCorners not found" or stale pin signatures in BP. The Win64 DLL is binary-equivalent to Mac's iOS toolchain bin output — PC + Mac each compile their own platform-specific DLL from the shared source.

**Bonus — file lock detection.** `tasklist | grep UnrealEditor` truncates `Image Name` column and can miss a running editor. Reliable check is PowerShell: `Get-Process | Where-Object { $_.ProcessName -match 'Unreal|UE5|UE4|CrashReport' }`. A `CrashReportClientEditor` started at the same timestamp as `UnrealEditor` strongly implies the editor crashed mid-startup — main process is technically alive but UI is wedged. `git checkout HEAD -- file.uasset` will fail with `unable to unlink` / `Invalid argument` (bash) or `being used by another process` (PowerShell) while the editor or its watchdog has the file open. Save anything important, then End Task on both PIDs to release the lock.

## 2026-04-29 — IA_TapPlace must be Digital (bool), NOT Axis2D, on iOS

EnhancedInput's `Pressed` trigger latches on Axis2D + iOS touch. Symptom: first tap fires the IA event normally, every subsequent tap is silently dropped (event handler doesn't run at all). Marker A spawns, Marker B never does. ~12 deploy cycles burned diagnosing this through layers of red herrings (BP wire severance, IsValid macro corruption, C++ silent crash hypothesis, BP_ARPlayerController BeginPlay regression, etc.) before Print String diagnostics on every branch of the tap state machine proved that the IA event itself was firing exactly once per app session.

**Why it latches:** UE's `Pressed` trigger fires when the input value's magnitude crosses the actuation threshold (~0.5) from below. For Axis2D with iOS Touch1, the value is the screen coordinate. iOS does NOT reset `Touch1.X/Y` to (0,0) on finger release — those coordinates retain the last touched position. So magnitude `sqrt(x² + y²)` stays at e.g. 583px after a (300, 500) tap. Pressed sees the input as "still actuated" and never re-arms. The "is currently pressed" digital state IS reset on iOS — Digital(bool) value type uses that signal directly and re-arms cleanly per tap.

**Fix (commit pending):** Open `Content/Input/IA_TapPlace`, change Value Type from `Axis2D` to `Digital (bool)`. Keep the single `Pressed` trigger. No other changes — the BP graph reads tap screen coords via `GetInputTouchState(Touch1)` separately, not from the IA's `ActionValue` pin (which is unconnected in BP_ARPlayerController per direct graph inspection of K2Node_EnhancedInputAction_0 ActionValue PinId=9C2E8DA7).

**How to apply:** Any future iOS touch IA that uses a `Pressed` trigger and only needs "did the user tap?" semantics MUST be Digital(bool). Use Axis2D only when the BP actually needs the screen coordinates from the IA itself, AND configure with a `Tap` or `Released` trigger instead of `Pressed`. The `Tap` trigger is also a safe alternative for single-tap detection on Axis2D — it requires a complete down-and-up cycle within a time threshold.

**Symptom shortcut for future debugging:** if Print String diagnostics on the IA event handler fire exactly ONCE per app session and never again on subsequent taps, before going down BP-graph rabbit holes, check the IA's Value Type. Latched-Pressed-on-Axis2D is the most likely cause on iOS.

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
Two-tap placement: A→B is the pad's **long edge**, not the diagonal of an AABB. Rectangle auto-rotates to the tap vector (`yaw = atan2(Delta.Y, Delta.X)`). `WidthCm` drives the short edge (500 cm outdoor default; v17 lowered the pin to 100 cm for indoor demos — see `1bf9a9a`). Rejected alternative: axis-aligned AABB with A and B as opposite corners — simpler but can't match real site-pad orientation, which rarely aligns to world north/east.

**Why:** Cut-and-fill pads on actual sites run along streets, grade breaks, or property lines — none of which are compass-aligned. Forcing an AABB would produce visibly wrong pad orientation in ~every real tap. Matches the "two-stakes-define-one-edge" pattern used by Trimble Siteworks, AGTEK, and traditional surveyor construction-staking workflows.

**Re-confirmed 2026-05-11 post-Node-1.4 close** after Cole asked whether the slab should align corner-to-corner or extend beyond the markers. Answer: edge model retained — markers are the two endpoints of the long edge, slab extends `WidthCm/2` perpendicular on each side. Stays the design through Phase 2; revisit only if BIM overlay introduces a different placement semantic.

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
