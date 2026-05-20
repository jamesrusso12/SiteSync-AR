# Node 2.1 / A3 — Handoff (2026-05-20 → next session)

**Paste this entire document into a fresh Claude Code session on Mac.** It is self-contained — the new session needs no prior conversation context.

---

## TL;DR — A3 IS DONE ✅

The A3 "real building model" work is **complete and device-validated 2026-05-20**, including the lighting fix. A real textured house (Fab → "Free Small Old House", Jimbogies, CC BY 4.0) imports, places via two taps at correct dollhouse scale (~2.3 m × 2.5 m × 0.7 m), renders fully lit with realistic textures, reset/replace loop works, HUD reads correctly.

**Committed** in `feat(node-2.1): A3 — real house BIM model, Model Scale, clamp fix, level lighting` (commit `b5747df`) + the docs commit that includes this file.

**This document is now a forward handoff** — the bug write-ups below are retained as reference; the "What's next" section at the bottom is the actual starting point for the next session.

---

## You are working with

- **James Russo** — Blueprint-first UE5 developer, Boise ID. See `memory/user.md`.
- Working tree: `~/Developer/SiteSync-AR/` (Mac canonical). HARD RULE — see `CLAUDE.md` "Canonical Working Trees."
- Auto mode is typically on — execute autonomously, prefer action, but ask before destructive operations.

---

## Session-start checklist

1. Read `CLAUDE.md` (project root) — full context. Note the "Bugs & Workflow Gotchas — Index" section.
2. Read all four `memory/` files: `user.md`, `people.md`, `preferences.md`, `decisions.md`. The `decisions.md` entries dated 2026-05-19 and 2026-05-20 are the directly relevant ones.
3. Confirm Mac, canonical tree `~/Developer/SiteSync-AR/`.
4. `git -C ~/Developer/SiteSync-AR status` — confirm the uncommitted state described below is intact.
5. Proceed to "The immediate task" below.

---

## Context — what happened in the 2026-05-14 → 2026-05-20 session

Demo-prep arc for the Idaho Technology Council event. The event **postponed indefinitely** mid-session (2026-05-19) — no rush now; the work is retained as a demo-ready baseline.

**Done, committed, pushed to `origin/main`:**
- `8fb93d3` — A1: BIM placeholder shrunk 5×5×9m → 3×3×2.5m
- `b7abe86` — A2: `WBP_BIMPlacementHUD` three-state placement HUD (Pre-corner / Post-corner / Post-placement)
- `90471e8` — Tier B: UV-edge wireframe glow on `M_BIMOverlay`

**A3 — real building model (DONE except lighting, UNCOMMITTED):**
- Imported Fab "Free Small Old House" (Jimbogies, CC BY 4.0) — 20,512 tris, glTF. Lives at `Content/Fab/Free_Small_Old_House/`.
- Swapped into `BP_BIMOverlay`'s `BIMMesh` component (was the engine cube).
- Retargeted to **Model Scale** (~0.3× — see `decisions.md 2026-05-18` for the Site vs Model scale-mode concept).

**Four bugs were hit and resolved during A3 (all written up in `decisions.md` 2026-05-19/20 + indexed in `CLAUDE.md`):**

1. **glTF imported 100× oversized.** Fab building assets arrive ~100× too large (a 7.7m house imports as 774m). Correct fix: Static Mesh → LOD 0 → Build Settings → **Build Scale `(0.01, 0.01, 0.01)`** (an initial `0.1` was a red herring — only got it to 77m). Apply Changes **+ Cmd+S with the Static Mesh tab focused** (Apply Changes alone does not persist).

2. **`PlaceBIMByCornerForward` C++ clamped L/W/H to min 100.0f** — and logged only the post-clamp value, so device logs always showed `L=100.0cm` no matter what the BP passed. Cost ~2 hours chasing a phantom "BP save bug." Fixed in `ARMeshBlueprintLibrary.cpp` — clamp min lowered `100.0f → 10.0f`. **This C++ change is built for iOS but UNCOMMITTED.**

3. **`BIMMesh` is the root component of `BP_BIMOverlay`** → `SetActorScale3D` (called by `PlaceBIMByCornerForward`) overwrites its `RelativeScale3D`. Component-level scale is a dead lever. Only Build Scale + actor scale move the size.

4. **The house renders pure black** — the current open problem. See below.

**Final working scale chain:** `774m glTF import × 0.01 Build Scale = 7.74m mesh → × 0.3 actor scale (L=30 pin) = 2.32m rendered`. Device-validated 2026-05-20: the house places at correct dollhouse scale.

---

## How the black-house problem was solved (reference)

The Fab house's material slots are standard **Lit** PBR materials — they need scene lighting. `SiteSync_BIMTest.umap` was created minimal (PlayerStart + `BP_LiDARMeshManager`) with **no lights**. The project's own debug/overlay materials (`M_LiDARDebug`, `M_BIMOverlay`) are Unlit so they always rendered; the imported Lit materials got zero light → black silhouette.

**Fix applied:** added to `SiteSync_BIMTest.umap`:
- **Directional Light** — Movable, Pitch -50, Intensity 8 lux (primary).
- **Sky Light** — Movable, **Source Type = SLS Specified Cubemap**, Cubemap = `DaylightAmbientCubemap` (engine content, `/Engine/MapTemplates/Sky/`), Intensity Scale 3 (ambient fill).

Note: a Sky Light with **Real Time Capture** enabled throws a warning in an AR scene because it needs a SkyAtmosphere / VolumetricCloud / sky-mesh — none of which belong in an AR passthrough scene. `SLS Specified Cubemap` with `DaylightAmbientCubemap` is the correct AR-safe choice. If a future imported asset renders black, this is the playbook.

---

## Git state — A3 committed

A3 is committed on `main` as of this handoff:
- `b5747df` — `feat(node-2.1): A3 — real house BIM model, Model Scale, clamp fix, level lighting` (C++ clamp fix, BP retargeting, `SiteSync_BIMTest` lighting, the `Content/Fab/` house asset)
- A `docs(node-2.1)` commit alongside — `CLAUDE.md` / `README.md` / `memory/decisions.md` / this handoff file

`logs/` is intentionally never committed (deploy logs).

**Check `git -C ~/Developer/SiteSync-AR log origin/main..HEAD` at session start** — if those two commits are unpushed, `git push origin main`.

---

## Deploy pipeline reference (Mac)

No C++ change needed for the lighting fix (it's a level edit), so use the fast path — cook + stage + install + launch. From `~/Developer/SiteSync-AR/`:

```bash
bash scripts/patch-ue56-xcode26.sh

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

Editor MUST be fully closed before the cook step (it locks cook output dirs). Warm-cache deploys run ~40 s. If a C++ source file changes, insert a build step first:
`"/Users/Shared/Epic Games/UE_5.6/Engine/Build/BatchFiles/Mac/Build.sh" SiteSyncAR IOS Development -project="$PWD/SiteSyncAR/SiteSyncAR.uproject" -waitmutex`

**Pulling device logs** (to verify runtime values like `PlaceBIMByCornerForward: ... scale_m=(...)`):
```bash
xcrun devicectl device copy from --device AE7F6A42-9908-58CC-897C-D82FBD14AA77 \
  --source "/Documents/" --destination /tmp/sitesync-logs/ \
  --domain-type appDataContainer --domain-identifier com.RussoCompany.SiteSyncAR
grep "PlaceBIMByCornerForward" /tmp/sitesync-logs/SiteSyncAR/Saved/Logs/SiteSyncAR.log | tail -3
```

---

## What's next — START HERE (no rush, demo postponed)

A3 is done and committed. In rough priority order:

1. **Push if needed** — `git -C ~/Developer/SiteSync-AR log origin/main..HEAD`; if A3 + docs commits are unpushed, `git push origin main`.
2. **C++ logging hygiene pass** — the highest-value cleanup. Make `PlaceBIMByCornerForward` log raw→clamped values (`L=%.1f→%.1fcm`), audit `InitFoundationFromCorners` + `CalculateCutFillVolumes` for the same hidden-clamp pattern, and document all clamp ranges in `ARMeshBlueprintLibrary.h` doc comments. This is the durable fix for the bug class that cost ~2 hours on 2026-05-20.
3. **BP cleanup pass** on `BP_ARPlayerController_BIM` — remove the dead `AddMappingContext` BeginPlay chain (EnhancedInput abandoned in v20), rename the stale "Spawn Foundations" comment to "Spawn Markers + BIM Overlay," optionally consolidate the two `GetInputTouchState` calls. Low risk, improves readability.
4. **Texture/cook-size check** — `Content/Fab/` is ~213 MB of source textures (8 material sets). Verify the cooked iOS `.ipa` stays manageable for Personal Team weekly re-deploy (target <200 MB per `CLAUDE.md`). If heavy, a texture-resolution / LOD pass on the house.
5. **Node 2.1 proper deliverable** — real Datasmith ingest of a Revit/Rhino sample per the `Gate to Node 2.2` criteria in `CLAUDE.md`. The Fab glTF house was a tactical demo asset, not the true Node 2.1 exit artifact.
6. **Tier B** — `SiteSync_Menu.umap` two-button launcher for Phase 1 (cut/fill) + Phase 2 (BIM).
