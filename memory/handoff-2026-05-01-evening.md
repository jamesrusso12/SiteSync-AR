---
name: Node 1.4 handoff — 2026-05-01 evening
description: Session checkpoint after ~15 deploy cycles diagnosing the BP/C++ scale-mismatch chain. C++ math runs at 10Hz on device; foundation actor scale stuck at (1,1,1) due to broken InitFromCorners exec wire (suspected). Three active bugs documented with diagnoses and next steps.
type: project
---

# Handoff — 2026-05-01 evening

You are picking up SiteSync AR Node 1.4 work on Mac (`~/Developer/SiteSync-AR/`).
The project has been through ~15 deploy cycles today on a single bug chain — read
this whole document before doing anything.

## Session-start protocol

At start of EVERY session, read:
- `CLAUDE.md` (project root) — full project context
- `memory/user.md`, `people.md`, `preferences.md`, `decisions.md`

The decisions.md log especially — recent entries from 2026-04-29 and 2026-05-01
are critical lessons that took multiple deploy cycles to learn.

## Project in one paragraph

SiteSync AR is a UE 5.6.1 iOS app for AEC professionals. Phase 1 = Node 1.4 =
on-site cut-and-fill earthwork calculator: scan terrain via ARKit Scene
Reconstruction (LiDAR mesh = existing grade), tap two corners on the floor to
place a foundation pad (orange slab = design grade), get cubic-yards-cut +
cubic-yards-fill output. iPhone 16 Pro for device test. Free Personal Team
signing (Team ID PD29S4YQ4P, bundle `com.RussoCompany.SiteSyncAR`).

## Current state — what works as of 2026-05-01 13:23 PT (commit cc44a0b)

- App launches and stays alive
- Cyan LiDAR mesh wraps real surfaces (`BP_LiDARMeshManager` + C++ shim)
- AR Pawn camera tracks device pose (`BP_ARPawn` with `bLockToHmd`)
- Tap input fires reliably per-tap (`IA_TapPlace` = Digital(bool) + Tap trigger)
- Tap state machine: tap A → marker A → tap B → marker B + foundation spawn
- `BP_Foundation.BeginPlay` runs, 0.1s Delay loop fires
- C++ `CalculateCutFillVolumes` runs at 10Hz in tight loop
- Cut/Fill numbers display + update in `WBP_VolumeReadout` widget
- "Touch ACTIVE" prints prove iOS touch pipeline delivers per-frame
- App no longer crashes after slab placement (was the dominant bug all day)

## Active bugs — what's still broken

### Bug 1 (HIGH PRIORITY): Foundation actor scale reads (1,1,1) — slab is 1m³ cube

- Device log shows: `scale=(1.000,1.000,1.000)m raw_dims=(100.0,100.0,100.0)cm`
- Visual = engine default cube at SpawnTransform location, NOT a proper slab
- Cut/Fill numbers ARE non-zero but reflect the phantom 1m³ footprint, not the
  user's intended slab dimensions
- **Suspected root cause (just discovered, needs verification):**
  `BP_Foundation > InitFromCorners` function has a BROKEN EXEC WIRE.
  `K2Node_VariableSet_2.then` (SET CornerA's exec output) had no `LinkedTo`
  clause in the .uasset graph dump. Function fires SET CornerA, then RETURNS.
  SET CornerB → SetActorLocation → SetActorRotation → SetActorScale3D
  NEVER FIRE.
- User did "fix" the wire and saved at end of session, BUT v15 device log
  STILL shows (1,1,1) scale — so either the wire wasn't actually drawn, or
  wire is there but something else overrides it. UNVERIFIED.
- The math chain (sqrt for length, `WidthCm/100` for width,
  `abs(SlabThicknessCm)/100` for thickness) IS wired correctly into MakeVector
  → SetActorScale3D. Just needs the exec to fire.

### Bug 2 (MEDIUM): Reset cycle freezes after 2nd cycle

- Tap A → A spawns, Tap B → B + slab spawns, Tap 3 → reset works
- Then Tap 4 → marker A spawns again, Tap 5 → FREEZES, no marker B
- **Suspected cause:** ResetPlacement function on `BP_ARPlayerController`
  destroys MarkerA/B/Foundation actors but doesn't `SET ActiveFoundation = None`.
  The variable reference persists. On next tap, IsValid macro still sees
  Valid → routes to ResetPlacement again → infinite reset loop.
- **Fix:** open `BP_ARPlayerController`, find ResetPlacement function, ensure
  it does ALL of:
  - `DestroyActor(MarkerA)`; `SET MarkerA = None`
  - `DestroyActor(MarkerB)`; `SET MarkerB = None`
  - `DestroyActor(ActiveFoundation)`; **`SET ActiveFoundation = None`** ← critical
  - `SET bHasFirstTap = false`

### Bug 3 (MEDIUM): Markers float in air, not on floor

- `LineTraceTrackedObjects` (used in `GetWorldLocationFromTap`) hits AR plane
  anchors that aren't necessarily the floor — could be a wall, table edge,
  chair surface
- Plan from `memory/decisions.md` 2026-04-24: write a C++ raycast helper that
  iterates `ARMeshAnchor.geometry.faces`, does ray-triangle intersection,
  returns nearest hit. Expose as `BlueprintCallable`, swap into
  `GetWorldLocationFromTap` in place of `LineTraceTrackedObjects`.
- This is the cleanest fix. ~50 lines of Objective-C++ in
  `ARMeshBlueprintLibrary_iOS.mm`.

## The long debugging path (lessons learned today)

Don't repeat any of these:

### Lesson 1 — Axis2D + Pressed trigger latches on iOS touch

Tap input only fired ONCE per app session — root cause was `IA_TapPlace`
Value Type = Axis2D + Pressed trigger LATCHES on iOS touch (`Touch1.X/Y`
doesn't reset to (0,0) on finger release). **Fix:** changed to Digital(bool)
+ Tap trigger. Logged in `decisions.md` 2026-04-29.

### Lesson 2 — MCP `connect_blueprint_nodes` near macros poisons compile state

v7 broke entirely (no widget, no taps) — root cause was MCP-driven PrintString
inserts via `connect_blueprint_nodes` "Replace existing output connections"
mode poisoned the IsValid macro on `BP_ARPlayerController`. Compile error
survived in `.uasset` metadata. **Fix:** deleted the diag prints, refresh
nodes. **Lesson:** NEVER use MCP to insert nodes into exec wires near macros.
Manual editor edits only for that.

### Lesson 3 — Stale editor module → BP/C++ ABI mismatch crash

C++ argument-stack mismatch crash — the editor's loaded SiteSyncAR module
dylib was 3 days stale relative to source. Editor reflection saw OLD 5-param
signature (TerrainMesh, FoundationActor, SlabLengthCm, SlabWidthCm,
SlabThicknessCm) but iOS UBT compiled fresh 4-param signature. BP bytecode
pushed 5 args, C++ took 4 → SEGV in argument frame. **Fix:** closed editor,
ran `Build.sh SiteSyncAREditor Mac Development`, reopened, deleted+re-added
the BP node which then showed correct 4-param signature.

**LESSON:** any time C++ exposed function signatures change, REBUILD THE
EDITOR MODULE (`Build.sh SiteSyncAREditor Mac Development`) before editing
BPs that reference those functions. Otherwise stale editor reflection
produces silent BP/C++ ABI mismatches that only crash at iOS runtime.

### Lesson 4 — C++ scale read is wiring-agnostic

C++ math reads scale via `FoundationActor->GetActorScale3D()` ×
`SlabMesh.GetRelativeScale3D()` (both multiplied — wiring-agnostic).
The final piece needed is the BP `InitFromCorners` actually calling
`SetActorScale3D`. See Bug 1.

## Key files

### C++

- `Source/SiteSyncAR/Public/ARMeshBlueprintLibrary.h`
- `Source/SiteSyncAR/Private/ARMeshBlueprintLibrary.cpp`
  - Has `CalculateCutFillVolumes(TerrainMesh, FoundationActor, OutCut, OutFill)`
  - Reads slab scale from FoundationActor's actor scale × SlabMesh component
    relative scale. Clamps length/width to 50cm-50m, thickness to 5cm-50cm.
  - Has bounds checks, IsFinite guards, `MaxTrianglesPerCall=250000` cap.
- `Source/SiteSyncAR/Private/ARMeshBlueprintLibrary_iOS.mm`
  - iOS shim for `GetARMeshData`, applies anchor transform + AlignmentTransform
    + TrackingToWorld to vertex positions.

### Blueprints

- `Content/Blueprints/BP_Foundation.uasset`
  - Has `InitFromCorners` function with the math chain (computes center, yaw,
    length×width×thickness via Math Expressions). **Critical: exec wire from
    SET CornerA → SET CornerB MUST be connected for SetActorScale3D to fire.**
  - BeginPlay: cache `HUDWidget` ref → "Hello" print → Delay 0.1s → loop body
  - Loop body: `GetActorOfClass(BP_LiDARMeshManager)` → `GetComponentByClass(ProcMesh)`
    → `CalculateCutFillVolumes` → `SET CutCubicYards/FillCubicYards` → IsValid
    `CachedReadout` → `SetCutText/SetFillText` → loop
  - Class Defaults: `WidthCm=500`, `SlabThicknessCm=10`
- `Content/Blueprints/BP_ARPlayerController.uasset`
  - Owns `IMC_Placement` via `EnhancedInputLocalPlayerSubsystem` on BeginPlay
  - Creates `WBP_VolumeReadout` widget, stores in `HUDWidget` var (Public,
    Instance Editable)
  - `IA_TapPlace.Started` → `IsValid(ActiveFoundation)` macro:
    - Valid → `ResetPlacement` function (DESTROYS but maybe doesn't NULL refs)
    - NotValid → `GetWorldLocationFromTap` → `bHit` branch → `bHasFirstTap` branch:
      - true → spawn MarkerB + spawn Foundation + InitFromCorners +
        `SET ActiveFoundation` + `SET bHasFirstTap=false`
      - false → spawn MarkerA + `SET MarkerA` + `SET bHasFirstTap=true`
  - Has 7 diagnostic Print Strings (Tap Fired, Path: NotValid/Reset,
    bHit OK/MISS, Path: MarkerA/MarkerB)
- `Content/Input/IA_TapPlace.uasset`
  - Value Type = **Digital (bool)**, single Pressed trigger (works) OR Tap
    trigger (also works). Was Axis2D originally — **DON'T revert.**
- `Content/Input/IMC_Placement.uasset`
  - Maps Touch1 → IA_TapPlace
- `Content/User_Interface/WBP_VolumeReadout.uasset`
  - Has CutText + FillText TextBlocks (no per-frame property bindings —
    pushed values via SetCutText/SetFillText functions called from BP_Foundation)
  - SafeZone wrap for iPhone notch clearance
  - Font size 28

## Deploy pipeline (Mac, all commands proven)

Canonical tree: `~/Developer/SiteSync-AR/` (NOT Desktop/Documents — iCloud
xattrs break codesign)

iPhone UDID: `AE7F6A42-9908-58CC-897C-D82FBD14AA77` (devicectl)

### Full deploy (after C++ change)

```bash
cd ~/Developer/SiteSync-AR
bash scripts/patch-ue56-xcode26.sh   # idempotent, every session
"/Users/Shared/Epic Games/UE_5.6/Engine/Build/BatchFiles/RunUBT.sh" \
  SiteSyncAR IOS Development -project="$PWD/SiteSyncAR/SiteSyncAR.uproject"
"/Users/Shared/Epic Games/UE_5.6/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
  "$PWD/SiteSyncAR/SiteSyncAR.uproject" -run=Cook -targetplatform=IOS \
  -unversioned -stdout -FullStdOutLogOutput
"/Users/Shared/Epic Games/UE_5.6/Engine/Build/BatchFiles/RunUAT.sh" \
  BuildCookRun -project="$PWD/SiteSyncAR/SiteSyncAR.uproject" \
  -platform=IOS -configuration=Development -stage -skipbuild -skipcook -archive
xcrun devicectl device install app --device AE7F6A42-9908-58CC-897C-D82FBD14AA77 \
  "$PWD/SiteSyncAR/Saved/StagedBuilds/IOS/SiteSyncAR.app"
xcrun devicectl device process launch --device AE7F6A42-9908-58CC-897C-D82FBD14AA77 \
  --terminate-existing com.RussoCompany.SiteSyncAR
```

BP-only change: skip `RunUBT`, do cook+stage+install only (~30s total).

### After C++ EXPOSED FUNCTION SIGNATURE change (UFUNCTION decl in .h)

REQUIRED: close UE editor, run `Build.sh SiteSyncAREditor Mac Development`,
reopen editor. Otherwise editor reflection is stale and BP edits will produce
ABI mismatch crashes on iOS. See Lesson 3 above.

### Device log capture (use `brew install libimobiledevice` if not present)

```bash
pkill -f "idevicesyslog -u 00008140" 2>&1 || true
idevicesyslog -u 00008140-001615D12E90801C -m "SiteSyncAR" \
  -o ~/Developer/SiteSync-AR/logs/node-1.4-deploy-vNN.log --no-colors &
```

### Pull device sandbox (UE log lives in `Documents/SiteSyncAR/Saved/Logs/`)

```bash
mkdir -p ~/Developer/SiteSync-AR/logs/dev-pull-NN
xcrun devicectl device copy from --device AE7F6A42-9908-58CC-897C-D82FBD14AA77 \
  --domain-type appDataContainer --domain-identifier com.RussoCompany.SiteSyncAR \
  --source "/Documents/SiteSyncAR/Saved/Logs/" \
  --destination ~/Developer/SiteSync-AR/logs/dev-pull-NN/
```

## Working style with James

- Blueprint-first developer in Boise, Idaho. Partner is Cole.
- Two machines: Windows PC (`C:\Dev\SiteSync-AR\`) and Mac M5 Pro
  (`~/Developer/SiteSync-AR/`). Mac is iOS compile/Xcode/wired deploy.
  BP edits happen on either machine.
- Label every appended prompt block: `## Mac Prompt`, `## PC Prompt`,
  `## UE5 Prompt` (machine-agnostic), or `## Note` (for docs).
- Conventional Commits: `feat / fix / chore / docs / diag`
- Terse responses, no emojis in files unless asked
- 60fps on iPhone 16 Pro is non-negotiable
- LiDAR mesh updates throttled to 200ms minimum
- For BP graph copy-paste verification: ask James to Cmd+A → Cmd+C in the
  graph editor → paste back. The .uasset binary search is unreliable for pin
  connection state.
- DO NOT use chongdashu MCP for inserting Print String nodes into exec wires
  near macros. Manual editor work only. (See Lesson 2.)

## What to do right now

HEAD on origin/main is `cc44a0b`. Mac local should be at the same commit.
UE editor is open on Mac (PID may have changed). iPhone has v15 installed.

**Next action: RESOLVE BUG 1** (the (1,1,1) scale mystery).

The BP wire fix at the end of last session may or may not have actually
landed in the .uasset. Two paths:

### Path A (5 min — verify the fix)

1. Ask James to open `BP_Foundation > InitFromCorners`
2. Ask: "Is there a white wire connecting SET CornerA's `then` exec output
   to SET CornerB's `execute` exec input?"
3. If YES but scale still (1,1,1) → something else is overriding. Ask for
   a fresh graph dump (Cmd+A, Cmd+C, paste).
4. If NO → ask him to drag the wire, Compile, Save All. Then redeploy.

### Path B (30 min — replace fragile BP math with C++)

Write `UARMeshBlueprintLibrary::InitFoundationFromCorners(actor, A, B,
widthCm, thicknessCm)` that does all the math + `SetActorLocation/Rotation/
Scale3D` in C++. Then James swaps the InitFromCorners call in
`BP_ARPlayerController` to use this C++ instead. C++ has been the more
reliable code path in this project.

If Bug 1 resolves, move to Bug 2 (ResetPlacement nulls) then Bug 3
(C++ raw mesh raycast helper for floor-accurate marker placement).

## Gate for Node 1.4 closure

- [ ] Place markers on actual floor (not floating in air)
- [ ] Slab spawns as flat slab (~10cm thick), not 1m cube
- [ ] Cut/Fill numbers reflect real slab footprint
- [ ] On flat floor, Cut and Fill stay close to 0 yd³
- [ ] Reset → repeat works for 5+ cycles without freezing
- [ ] App stays alive 60+ seconds post-placement, no jetsam
- [ ] 60fps holds throughout
