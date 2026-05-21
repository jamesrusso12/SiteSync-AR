# Node 2.1 — Cook + Deploy Handoff (PC → Mac, 2026-05-21)

**Paste this entire document into a fresh Claude Code session on the Mac.** Self-contained — no prior conversation context needed.

---

## TL;DR — the task

`TestBuilding` (the first real Datasmith BIM model) is wired into `BP_BIMOverlay` and committed. It has **not** been on-device yet. Cook + deploy `SiteSync_BIMTest` to the iPhone 16 Pro and confirm the real building places via the two-tap flow.

This is a **content-only deploy** — no C++ changed since `0c38146` (already on the device). Use the fast path: cook → stage → install → launch. **No `Build.sh` step.**

---

## You are working with

- **James Russo** — Blueprint-first UE5 developer, Boise ID. See `memory/user.md`.
- Working tree (Mac canonical, HARD RULE): `~/Developer/SiteSync-AR/` — NOT `~/Desktop`, NOT iCloud. See `CLAUDE.md` "Canonical Working Trees."
- Auto mode is typically on — execute autonomously, ask before destructive ops.

## Session-start checklist

1. Read `CLAUDE.md` (project root), especially "Immediate Next Actions."
2. Read the four `memory/` files. Relevant: `decisions.md` 2026-05-21 entries.
3. Confirm Mac, canonical tree `~/Developer/SiteSync-AR/`.

---

## What just happened on PC (2026-05-21)

- `2703f54` — **`TestBuilding` wired into `BP_BIMOverlay`.** The 6 imported Datasmith pieces (Floor / 4 walls / Roof) were combined via Merge Actors (Pivot Point at Zero) into `SM_TestBuilding_Merged`, saved at `Content/AR_SiteAnalysis/DatasmithAssets/TestBuilding/SM_TestBuilding_Merged.uasset`. `BP_BIMOverlay.BIMMesh` (root component) now points at it. Compile verified clean.
- `e0338e4` — Cursor removed from the project (config + doc mentions).
- `4d42a18` — CLAUDE.md Node 2.1 section cleaned of stale handoff cruft.

`BP_BIMOverlay` no longer references the Fab glTF house — the cook of `SiteSync_BIMTest` is reference-driven, so the ~91 MB Fab house in `Content/Fab/` should not be cooked into this build.

---

## Pre-flight

```bash
cd ~/Developer/SiteSync-AR
git pull --ff-only && git lfs pull          # expect to land on 4d42a18 or later
bash scripts/patch-ue56-xcode26.sh          # idempotent; required if Launcher updated UE 5.6
```

**Close the UE editor fully before cooking** — it locks the cook output dirs.

---

## Cook + deploy — fast path (content-only, no C++ build)

Run from `~/Developer/SiteSync-AR/`:

```bash
# 1. Cook the BIM test level
"/Users/Shared/Epic Games/UE_5.6/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
  "$PWD/SiteSyncAR/SiteSyncAR.uproject" -run=Cook -targetplatform=IOS \
  -map=/Game/Maps/SiteSync_BIMTest -unversioned -stdout -FullStdOutLogOutput

# 2. Stage into a deployable .app
"/Users/Shared/Epic Games/UE_5.6/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
  -project="$PWD/SiteSyncAR/SiteSyncAR.uproject" \
  -platform=IOS -configuration=Development -stage -skipbuild -skipcook -archive

# 3. Wired install
xcrun devicectl device install app --device AE7F6A42-9908-58CC-897C-D82FBD14AA77 \
  "$PWD/SiteSyncAR/Saved/StagedBuilds/IOS/SiteSyncAR.app"

# 4. Launch
xcrun devicectl device process launch --device AE7F6A42-9908-58CC-897C-D82FBD14AA77 \
  --terminate-existing com.RussoCompany.SiteSyncAR
```

- Device UDID: `AE7F6A42-9908-58CC-897C-D82FBD14AA77` (confirm with `xcrun devicectl list devices`).
- Bundle ID: `com.RussoCompany.SiteSyncAR`.
- Warm-cache deploys run ~40 s; first cook of new content is slower.
- If `git log 0c38146..HEAD -- SiteSyncAR/Source/` returns **any** commit, a C++ file changed — insert a build step before step 1:
  `"/Users/Shared/Epic Games/UE_5.6/Engine/Build/BatchFiles/Mac/Build.sh" SiteSyncAR IOS Development -project="$PWD/SiteSyncAR/SiteSyncAR.uproject" -waitmutex`
  As of this handoff that range is empty — fast path applies.

---

## Validation gate — Node 2.1 device sighting

On the iPhone 16 Pro, in the launched app:

1. Scan a room so the cyan LiDAR mesh builds.
2. **Tap once on the floor** → a yellow `MarkerCorner` sphere appears at the tap.
3. **Tap a second point ~1 m away** → a yellow `MarkerForward` sphere appears, and **`TestBuilding` — a small white 6-box building (floor + 4 walls + roof)** drops with its corner at the first tap, rotated to face the second tap.
4. **Tap a third time** → markers + building clear.
5. 60 fps holds throughout.

**Expected scale:** `PlaceBIMByCornerForward` L/W/H pins are `30/30/30` (Model Scale) → actor scale `0.3` → the native 6 m building renders at **~1.8 m dollhouse**. That is correct/expected, not a bug.

**Device-log check** (confirms the C++ placement ran):
```bash
xcrun devicectl device copy from --device AE7F6A42-9908-58CC-897C-D82FBD14AA77 \
  --source "/Documents/" --destination /tmp/sitesync-logs/ \
  --domain-type appDataContainer --domain-identifier com.RussoCompany.SiteSyncAR
grep "PlaceBIMByCornerForward" /tmp/sitesync-logs/SiteSyncAR/Saved/Logs/SiteSyncAR.log | tail -3
```
Expect a `PlaceBIMByCornerForward: ... scale_m=(0.300,0.300,0.300)` line per placement.

---

## Decision point during the test

**30 vs 100 scale.** If ~1.8 m dollhouse reads too small in the room, bump it to 1:1 Site Scale: open `BP_ARPlayerController_BIM`, find the `PlaceBIMByCornerForward` node in the spawn-tap branch, change the `Length/Width/Height Cm` pins `30 → 100` (→ actor scale `1.0` → full 6 m building). Per `decisions.md 2026-05-19`, re-type the pin values even if they look right, compile, save, re-cook — there is a known stale-pin-value bug.

---

## Known-issue playbook

- **Building renders black** — the TestBuilding material slot is a plain white **Lit** default material (Rhino layers had no materials). `SiteSync_BIMTest` already has a Directional Light + Sky Light (SLS Specified Cubemap = `DaylightAmbientCubemap`) from the A3 lighting fix, so it should render lit. If it's black, confirm those two lights survived in the level. See `docs/node-2.1-a3-lighting-handoff.md`.
- **Cook hangs for a long time** — first iOS cook of new content does DDC + shader-permutation compile; can take many minutes. Not stuck unless zero log progress for ~10 min.
- **Editor-locked cook dir** — fully quit UE before step 1.

---

## When done

- If the gate passes: update `CLAUDE.md` "Immediate Next Actions" — mark Node 2.1 device-validated, record the deploy. Commit + push.
- If a fix is needed (scale pin, lighting, etc.): make it, re-cook, re-deploy, then commit.
- Node 2.1's formal Gate to Node 2.2 (see `CLAUDE.md`): real BIM imports cleanly, cooks, renders on device at 60 fps with manual placement. This deploy is what clears it.
