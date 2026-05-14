# Node 2 — Tuesday Demo Handoff (2026-05-14 → 2026-05-19)

**Paste this entire document into a fresh Claude Code session on Mac when you're ready to continue.** It's self-contained — the new session needs no prior conversation context.

---

## TL;DR

SiteSync AR is at v22 (commit `476752b`). Phase 1 (cut/fill) closed. Phase 2 Node 2.1 first-sighting cleared — BIM placement loop works end-to-end on iPhone, but the visible result is just a room-sized orange translucent cube that tints everything orange. Strangers can't tell what's happening.

**Driver:** Idaho Technology Council demo on **Tuesday 2026-05-19**. Networking with potential clients/investors. Goal is to make what we have **legible**, not to finish the app.

The single highest-impact unlock is **replacing the orange placeholder with a real-looking building model** (via Datasmith ingestion of a free Revit/Twinmotion/Fab sample, or just a swap-in Static Mesh asset that READS as a building). Plus a minimal state-driven HUD that walks the viewer through the workflow.

---

## You are working with

- **James Russo** — Blueprint-first UE5 developer, Boise ID. See `memory/user.md`.
- **Partner Cole** — also attending the conference.
- Working tree: `~/Developer/SiteSync-AR/` (Mac canonical) or `C:\Dev\SiteSync-AR\` (PC canonical). HARD RULE — see CLAUDE.md "Canonical Working Trees".
- **Today is 2026-05-14.** Demo is 5 days out.

### James's schedule between now and demo

| Day | Availability |
|---|---|
| Thu 2026-05-14 | Until ~10:30 AM Mountain, then work until 4:30 PM, evening session after |
| Fri 2026-05-15 | James's birthday — relaxed, partial day |
| Sat 2026-05-16 | Relaxed, work time available |
| Sun 2026-05-17 | Roommate's birthday BBQ, maybe partial evening |
| Mon 2026-05-18 | Full work session expected |
| Tue 2026-05-19 | **DEMO** — no further dev work |

Plan around this. Don't promise things that need 12-hour days.

---

## Where the code is right now

### Tip of `origin/main`

```
476752b docs(node-2.1): v22 first-sighting cleared — placeholder BIM loop validated
349271c fix(node-2.1): wire SpawnActor BP_BIMOverlay.ReturnValue → SET ActiveBIM
c180cf4 feat(node-2.1): Mac Part 4 — SiteSync_BIMTest.umap + BIM BP fixes
```

### What works on iPhone (v22 device-validated 2026-05-14)

- Boot directly into `/Game/Maps/SiteSync_BIMTest`
- Cyan LiDAR mesh wraps the room (`BP_LiDARMeshManager`, throttled 5 Hz)
- Tap 1 → yellow MarkerCorner sphere on the mesh (raycast via Phase 1's `RaycastTerrainFromScreen` C++ helper)
- Tap 2 → yellow MarkerForward + orange translucent BIM box drops, corner-at-Tap-A, +X axis pointing toward Tap B, dims 5m × 5m × 9m
- Tap 3 → reset, all three actors cleared
- Loop is breakable indefinitely (proven by 5/5/5 cycle symmetry in v22 log)
- 60 fps holds throughout

### What's missing for a demo-legible experience

1. **The BIM box is too big.** 5×5×9 m fills any indoor room. From inside, it just looks like the camera went orange. Needs to shrink to ~3×3×2.5 m OR get replaced with real geometry.
2. **No HUD / no on-screen text.** Stranger has no idea what the goal of the tapping is.
3. **No real BIM model.** The "BIM overlay" is an `/Engine/BasicShapes/Cube` Static Mesh tinted orange. Not what investors want to see.
4. **Phase 1 (cut/fill) is not accessible from the current boot path.** Boot map is `/Game/Maps/SiteSync_BIMTest`. The Phase 1 `/Game/SiteSync.umap` still exists and works but you can't switch into it at runtime without a menu level.

---

## Key project files for Node 2 work

### C++ (Mac builds clean for iOS)

- `SiteSyncAR/Source/SiteSyncAR/Public/ARMeshBlueprintLibrary.h` — function declarations
- `SiteSyncAR/Source/SiteSyncAR/Private/ARMeshBlueprintLibrary.cpp` — implementations

Existing BlueprintCallable functions:
- `GetARMeshData` / `GetAllARMeshGeometries` / `UpdateLiDARMeshes` (LiDAR mesh extraction, Node 1.2)
- `CalculateCutFillVolumes` (Phase 1 earthwork math, Node 1.4)
- `InitFoundationFromCorners` (Phase 1 slab placement, Node 1.4)
- `RaycastTerrainFromScreen` (Möller-Trumbore raycast against LiDAR mesh, Node 1.4)
- `PlaceBIMByCornerForward(BIMActor, CornerWorld, ForwardWorld, LengthCm=500, WidthCm=500, HeightCm=900)` — Node 2.1 BIM placement primitive. **Defaults currently 5m × 5m × 9m — shrink target for Tier A item 3.**

### Blueprints

- `Content/Blueprints/BP_ARPlayerController_BIM.uasset` — Phase 2 player controller. Event Graph has tap state machine (Tick rising-edge → MarkerCorner/MarkerForward branches → PlaceBIMByCornerForward → SET ActiveBIM). ResetPlacement function destroys markers + BIM and nulls refs.
- `Content/Blueprints/BP_ARGameMode_BIM.uasset` — game mode. Default Pawn Class = `BP_ARPawn`. Player Controller Class = `BP_ARPlayerController_BIM`.
- `Content/Blueprints/BP_BIMOverlay.uasset` — the BIM actor itself. Has one `BIMMesh` StaticMeshComponent pointing at `/Engine/BasicShapes/Cube` with `RelativeLocation=(50,50,50)` so the **corner sits at actor origin** (critical for `PlaceBIMByCornerForward` math). Material = `M_BIMOverlay` (translucent orange unlit).
- `Content/Blueprints/BP_LiDARMeshManager.uasset`, `BP_ARPawn.uasset`, `BP_TapMarker.uasset` — Phase 1 carryovers, unchanged.
- `Content/Materials/M_BIMOverlay.uasset` — translucent orange unlit, opacity 0.4.

### Levels

- `Content/Maps/SiteSync_BIMTest.umap` — current boot map. PlayerStart (0,0,0), BP_LiDARMeshManager, World Settings GameMode Override = BP_ARGameMode_BIM.
- `Content/SiteSync.umap` — Phase 1 level (cut/fill earthwork). Still works if you flip boot map back to `/Game/SiteSync.SiteSync`.

### Config

- `SiteSyncAR/Config/DefaultEngine.ini`:
  - `GameDefaultMap=/Game/Maps/SiteSync_BIMTest.SiteSync_BIMTest`
  - `EditorStartupMap=/Game/Maps/SiteSync_BIMTest.SiteSync_BIMTest`

### Plugins enabled (in `SiteSyncAR.uproject`)

- `DatasmithImporter` (editor-time)
- `DatasmithContent` (runtime — needed so cooked Datasmith assets work on device)
- `DatasmithFBXImporter` (fallback path for Revit FBX exports)

Datasmith Mac binaries verified present at `/Users/Shared/Epic Games/UE_5.6/Engine/Plugins/Enterprise/DatasmithImporter/Binaries/Mac/*.dylib`.

Per `decisions.md 2026-05-12`: **iOS does not support DatasmithRuntime in any version through 5.7.** All Datasmith work happens at cook time — import once into UE editor on Mac/PC, place into world or reference from a Blueprint, then cook for iOS. No `.udatasmith` file ever lives on the iPhone.

---

## Plan — Tier A items (must-have for demo)

### A1. Shrink BIM placeholder defaults (15 min)

`SiteSyncAR/Source/SiteSyncAR/Public/ARMeshBlueprintLibrary.h` — change `PlaceBIMByCornerForward`'s defaults from `LengthCm=500, WidthCm=500, HeightCm=900` to something like `LengthCm=300, WidthCm=300, HeightCm=250` (3m × 3m × 2.5m). Real BIM models will override these.

Then either:
- Pin override in `BP_ARPlayerController_BIM`'s `PlaceBIMByCornerForward` call node (so changes take effect without redeploying C++)
- OR change C++ default + rebuild iOS + recook

Recommend the BP pin override path — faster to iterate, doesn't need a C++ rebuild.

### A2. `WBP_BIMPlacementHUD` widget (half day)

UMG widget. Three-state instruction overlay driven by `bHasFirstTap` and `IsValid(ActiveBIM)` from `BP_ARPlayerController_BIM`:

| State | When | Instruction text |
|---|---|---|
| Pre-corner | `!bHasFirstTap && !IsValid(ActiveBIM)` | "Tap the floor to mark a building corner" |
| Post-corner | `bHasFirstTap && !IsValid(ActiveBIM)` | "Tap a second point along the building's front edge" |
| Post-placement | `IsValid(ActiveBIM)` | "Building placed (3 m × 3 m × 2.5 m). Tap anywhere to reset." |

Push-pattern updates (NOT property bindings — see `decisions.md 2026-04-28` on the iOS jetsam learning). Controller calls `SetState(EBIMHudState)` on the widget after each state transition.

Reference: `WBP_VolumeReadout` (Phase 1 cut/fill HUD) is the structural template — SafeZone-wrapped VerticalBox of TextBlocks with push-pattern setters.

Create the widget on `BeginPlay` of `BP_ARPlayerController_BIM` and `AddToViewport`. Cache the reference in a `HUDWidget` variable on the controller.

### A3. Real BIM model — TWO PATHS, pick by Saturday

**Path A1 — Free Static Mesh swap (lowest risk, half day):**
- Browse Fab / Marketplace / Quixel Bridge for a free building Static Mesh (small office, residential pad, kiosk). Things to search: "building", "house", "small office", "pad".
- Import to `Content/AR_SiteAnalysis/DatasmithAssets/<model_name>/` (or wherever fits).
- In `BP_BIMOverlay`, swap the `BIMMesh` component's Static Mesh asset from `/Engine/BasicShapes/Cube` to the imported model. Adjust `RelativeLocation` so the building's footprint corner aligns to the actor origin (critical for `PlaceBIMByCornerForward` math).
- Disable Nanite on the imported mesh (Mesh Editor → LOD Settings → uncheck Nanite Support).
- Material likely needs re-parenting from whatever the asset shipped with to a mobile-friendly translucent overlay (similar to `M_BIMOverlay` but applied to the new mesh).

**Path A2 — Real Datasmith ingest (higher risk, 1–2 days):**
- Source a `.udatasmith` file. Options:
  - Twinmotion sample scene (if Datasmith Twinmotion Content plugin is published for UE 5.6 — verify on Fab first)
  - Free Revit sample (Autodesk has them) → export via Datasmith Revit Exporter (free Epic plugin)
  - Rhino sample → Datasmith Rhino Exporter
- Import via Datasmith Importer (Window → Datasmith Direct Link OR drag the .udatasmith into Content Browser).
- Apply same mobile mitigations as Path A1: disable Nanite per mesh, re-parent materials to mobile-friendly translucent variants, generate auto-LODs if mesh is heavy.
- Swap into `BP_BIMOverlay`.

**Decision criterion:** if Path A2 can be done by Saturday end of day with a known-good model, do it. If you're still hunting for a model by Saturday evening, fall back to Path A1 with a generic Static Mesh — better a polished generic building than a broken real-BIM import.

### A4. Cook + deploy to iPhone

Standard pipeline (from `~/Developer/SiteSync-AR/`):

```bash
bash scripts/patch-ue56-xcode26.sh
"/Users/Shared/Epic Games/UE_5.6/Engine/Build/BatchFiles/Mac/Build.sh" SiteSyncAR IOS Development \
  -project="$PWD/SiteSyncAR/SiteSyncAR.uproject"

# Editor must be CLOSED for cook
"/Users/Shared/Epic Games/UE_5.6/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
  "$PWD/SiteSyncAR/SiteSyncAR.uproject" -run=Cook -targetplatform=IOS \
  -map=/Game/Maps/SiteSync_BIMTest -unversioned -stdout -FullStdOutLogOutput

"/Users/Shared/Epic Games/UE_5.6/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
  -project="$PWD/SiteSyncAR/SiteSyncAR.uproject" \
  -platform=IOS -configuration=Development -stage -skipbuild -skipcook -archive

xcrun devicectl device install app --device AE7F6A42-9908-58CC-897C-D82FBD14AA77 \
  "$PWD/SiteSyncAR/Saved/StagedBuilds/IOS/SiteSyncAR.app"

xcrun devicectl device process launch --device AE7F6A42-9908-58CC-897C-D82FBD14AA77 \
  --terminate-existing com.RussoCompany.SiteSyncAR
```

Cook size will matter once a real BIM model lands — keep an eye on `Saved/StagedBuilds/IOS/SiteSyncAR.app` total size. Target staying under ~200 MB for the .ipa per CLAUDE.md.

---

## Tier B — if Tier A finishes early

- **`SiteSync_Menu.umap`** — main menu level with two buttons launching `OpenLevel(/Game/SiteSync)` (Phase 1) and `OpenLevel(/Game/Maps/SiteSync_BIMTest)` (Phase 2). Update `GameDefaultMap` to point here. Lets James demo both modes.
- BIM material polish — wireframe edges on the placeholder, dimension label hovering above the box.
- Demo script with talking points.

---

## Watch out for these (this session's lessons, see `decisions.md 2026-05-14`)

1. **Mac editor save-as crash from stale Desktop paths.** If "Save Current Level As" crashes again with `LongPackageNameToFilename failed to convert '/Users/jamesrusso/Desktop/Github/Xcode/...'`, the configs need re-purging:
   ```bash
   grep -rl "Desktop/Github" "$HOME/Library/Application Support/Epic" SiteSyncAR/Saved/Config 2>/dev/null
   # purge any matches with sed -i.bak 's|...Desktop...|...Developer...|g'
   ```

2. **chongdashu MCP `create_blueprint` doesn't honor non-Actor parent classes.** Use Content Browser → Duplicate of a working asset of the same parent class instead.

3. **`SET <object var>` needs BOTH exec wire AND data-input wire.** Pin-by-pin verify in the `.uasset` dump if state vars stay None despite the spawn chain firing.

4. **Cook `-map=...` ≠ boot map.** If you add a new test level and want it to boot, edit `DefaultEngine.ini → [/Script/EngineSettings.GameMapsSettings] → GameDefaultMap`.

5. **No iOS DatasmithRuntime.** All Datasmith work is cook-time only. Never plan to load .udatasmith at runtime on iPhone — won't happen in 5.7 or earlier.

---

## Session-start checklist for the new chat

When you open a fresh Claude Code session:

1. Read `CLAUDE.md` (project root) — full context.
2. Read all four files in `memory/`: `user.md`, `people.md`, `preferences.md`, `decisions.md`.
3. Confirm you're on Mac, canonical tree `~/Developer/SiteSync-AR/`.
4. `git pull origin main && git lfs pull` — sync to `476752b` or whatever's on origin.
5. `git log --oneline -5` — confirm where HEAD is.
6. Pick a Tier A item and start. Recommend A1 (placeholder shrink) first as the cheapest 15-minute visible win, then A2 (HUD), then A3 (model swap).
7. Auto mode is on — execute autonomously, prefer action over planning, but ask before destructive operations.

Good luck. The demo Tuesday is real, the calendar is tight, and James has been doing this for weeks — keep the momentum, ship visible polish daily.
