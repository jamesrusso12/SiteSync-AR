# Node 2.3 — Engineering Clash Interface · Implementation Plan

**Read this at the start of the Node 2.3 session.** Self-contained — written 2026-05-27 at the end of the session that closed Node 2.2 (full geo-anchoring stack device-validated).

---

## Session-start context

- Working tree: Mac canonical `~/Developer/SiteSync-AR/` and PC canonical `C:\Dev\SiteSync-AR\` (HARD RULE — see `CLAUDE.md`).
- Read `CLAUDE.md` + the four `memory/` files first (session-start protocol).
- Node 2.1 (Datasmith ingestion) — ✅ complete 2026-05-21. Node 2.2 (geo-anchoring) — ✅ complete 2026-05-27, full math + axis mapping device-validated. Node 2.3 closes Phase 2.
- James is Blueprint-first; escalate to C++ only where Blueprints can't do the job. Node-by-node BP walkthroughs with pin names.
- Datasmith ingest pattern is established: `dev/import_datasmith.py` (headless) or via the new MCP `execute_python` command (`77ad8ae`). Both work.

---

## Goal

Detect and visualize **clashes between BIM disciplines** (mechanical / electrical / plumbing / structural) when the model is placed in AR. Give the user a layer toggle UI so they can hide/show disciplines to inspect clashes from each angle. This is what makes the AR overlay actually useful for a contractor — "this duct hits this beam, we need to re-route" — not just a pretty placeholder.

---

## ⚠️ Source model is the blocker on day 1

Phase 2 so far has worked because we had Rhino's `TestBuilding` — 6 boxes (Floor / 4 Walls / Roof) on named layers. **That model has no MEP**, so it can't drive Node 2.3 testing. The very first 2.3 work is sourcing a new model with MEP separation.

**Decision (2026-05-27):** Rhino-built mockup. James already has Rhino 8 wired up + the `dev/import_datasmith.py` pipeline proven. Revit-on-PC was raised but is **not** set up yet — parked as a follow-up post-2.3.

## ⚠️ BP_BIMOverlay's merged-mesh design needs to change

Current `BP_BIMOverlay` wraps `SM_TestBuilding_Merged` — one mesh, no per-layer addressability (the merge happened via Merge Actors with Pivot Point at Zero per commit `116e4b5`). For 2.3, layer toggles need each MEP/structural discipline as a **separately-addressable Static Mesh Component**, not a merged single mesh. Two options:

1. **Replace `SM_TestBuilding_Merged` with a per-layer Static Mesh Component on `BP_BIMOverlay`.** One component per Datasmith layer, named to match (e.g., `Comp_HVAC`, `Comp_Plumbing`). Manual to set up but easy to iterate by name from BP.
2. **Spawn the Datasmith Scene Actor directly** instead of `BP_BIMOverlay`. The Datasmith importer creates a `DatasmithSceneActor` with all layer-named child components automatically — placing that actor preserves the hierarchy. Less manual work but `PlaceBIMByCornerForward` would need to operate on `DatasmithSceneActor` instead of `BP_BIMOverlay` (it just takes an `AActor*` so the C++ is fine; the BP wiring changes).

**Recommended:** Option 2 (use the Datasmith Scene Actor directly). It's the convention Datasmith expects, layer names propagate without manual remapping, and we get free upgrade paths when the model gains more layers. `BP_BIMOverlay` stays in the codebase as a thin BP wrapper around the Datasmith Scene Actor reference for organizational consistency.

---

## Architecture — four parts

### 2.3a — Sample MEP-bearing Rhino model + Datasmith import

Build a single-room construction mockup in Rhino with explicitly named layers:

| Layer | What | Why |
|---|---|---|
| `Structure_Floor` | Slab, 6×5×0.2 m | Baseline floor |
| `Structure_Walls` | 4 walls, 6×5×3 m room | Containment |
| `Structure_Roof` | Roof slab | Top |
| `Structure_Beam` | One horizontal beam mid-room | **Deliberate clash target** for HVAC |
| `HVAC_Duct` | Horizontal duct running through the wall AND through Structure_Beam | **Clash #1** |
| `Plumbing_Pipe` | Vertical pipe riser passing through the floor slab | **Clash #2** |
| `Electrical_Conduit` | Light horizontal line along ceiling | No clash (control case) |

Deliberate clashes are the whole point — we want Node 2.3c to *find* something. Without intentional intersections the test has no signal.

**⚠️ CRITICAL modeling rule — NAME each Rhino object, don't rely on layers.** Probed the existing TestBuilding import 2026-05-28 (`dev/probe_datasmith_layers.py`): its 6 meshes imported as `extrusion`, `extrusion_2`…`extrusion_6` with **zero** layer metadata (`Datasmith.LayerName` empty, no metadata keys at all). The Rhino objects were unnamed extrusions, so nothing addressable survived. Datasmith derives mesh-asset / actor names from the Rhino **object name**, NOT the layer. **For MEPRoom's layers to be toggleable at runtime, give every object an explicit name in Rhino** matching the table below (`HVAC_Duct`, `Structure_Beam`, etc.). Assigning to a layer is not enough — `GetBIMLayers` reads the component/mesh name, and an unnamed object becomes `extrusion_N` garbage. Name the objects; the layer assignment is secondary (still nice for Rhino-side organization, but not what we key on).

**Deliverables:**
- `BIM_Source/MEPRoom/MEPRoom.3dm` (Rhino source)
- `BIM_Source/MEPRoom/MEPRoom.udatasmith` (exported via Datasmith Rhino plug-in)
- Imported into `Content/AR_SiteAnalysis/DatasmithAssets/MEPRoom/` via `dev/import_datasmith.py`
- Verify each layer arrived as a distinct Static Mesh asset (not auto-merged)

**Validation:** open the imported Datasmith Scene asset in editor; confirm 7 named Static Meshes appear in the Static Mesh list, each tagged with its layer name.

### 2.3b — Layer toggle UI

Add a layer toggle panel to the BIM scene that lets the user hide/show each discipline at runtime.

**UI:** new widget `WBP_LayerTogglePanel` — a vertical list of checkbox + label rows, one per layer found on the placed BIM. Appears post-placement (after the corner+forward two-tap completes). Anchored bottom-right or as a slide-out drawer to stay out of the cyan-mesh view.

**BP wiring:** on BIM placement, iterate the placed actor's Static Mesh Components, read each one's tag/name, populate the panel rows. Each checkbox's OnChanged event calls `SetVisibility(bool)` on its mesh component. Should NOT destroy/respawn — just toggle visibility. Placement position/rotation persists.

**C++ helper (probably):** `UARMeshBlueprintLibrary::GetBIMLayers(AActor* BIM, TArray<FString>& OutLayerNames, TArray<UStaticMeshComponent*>& OutComponents)` — abstracts the "iterate child components, return their names + refs" lookup so the BP graph stays clean. Optional; could be pure BP if simpler.

**Validation:** place the BIM. Toggle HVAC off — duct disappears, rest stays. Toggle it back on — duct returns. Repeat for all layers. No flicker, no respawn.

### 2.3c — Clash detection (collision-based)

C++ `BlueprintCallable` in `UARMeshBlueprintLibrary`:
```cpp
struct FClashPair {
    UStaticMeshComponent* ComponentA;
    UStaticMeshComponent* ComponentB;
    FString LayerA;
    FString LayerB;
    FVector OverlapCenter;  // approximate; just for camera-pan UX
};
static int32 DetectBIMClashes(AActor* BIM, TArray<FClashPair>& OutClashes);
```

**Algorithm choice — start with engine collision overlap:** UE's built-in `UPrimitiveComponent::ComponentOverlapMultiAndQueryByObjectType` does AABB + convex-hull overlap detection between any two primitive components. Cheap (the engine does this millions of times per frame anyway for physics), well-tested. Each Datasmith-imported Static Mesh has a default collision mesh; we tell each layer's mesh component to use the appropriate channel and ask for overlaps with the others.

For each pair of layers `(A, B)` where `A != B`:
1. Get the component lists for layer A and layer B
2. For each component pair, call `ComponentOverlapMulti` → list of overlap hits
3. Each non-empty result → one `FClashPair`

**What this misses:** precise intersection geometry (volume of the overlap). For v1 that's fine — we just need "duct A clashes with beam B at approximately point P." Precise overlap volume can come later via GeometryScripting Boolean ops if a real workflow needs it.

**Edge case:** "trivial" overlaps where a wall touches the floor are technically clashes but not interesting clashes. Mitigation: add an optional "Excluded Pairs" set in C++ — `(Structure_Floor, Structure_Walls)` etc. don't count. Configurable per BIM model.

**Validation:** with the MEPRoom model placed, `DetectBIMClashes` returns exactly 2 hits — `(HVAC_Duct, Structure_Beam)` and `(Plumbing_Pipe, Structure_Floor)` — matching the intentional clashes from 2.3a.

### 2.3d — Clash visualization

**Material:** create `M_ClashHighlight` — translucent red unlit, 0.5 opacity (same pattern as the existing `M_BIMOverlay`). Materials swap, not actor overlays, so collision and tags stay intact.

**Apply:** for each clashing pair, call `SetMaterial(0, M_ClashHighlight)` on both components. Original materials cached and restored when "Show Clashes" is toggled off.

**HUD:** floating count badge "N clashes detected" in the top-right (similar position to where GeoReadout used to live before we moved it to bottom-right). Tappable → expands to a list `HVAC_Duct ⚠ Structure_Beam | Plumbing_Pipe ⚠ Structure_Floor`. Tapping a list row pans the camera to the clash's `OverlapCenter` (stretch goal — pan animation is straightforward UE camera blend).

**Validation:** place MEPRoom, see "2 clashes detected" badge, see two red-highlighted components. Toggle "Show Clashes" off → red goes away. Toggle the HVAC layer off → its red highlight goes with it (visibility is component-level, not material-level). Tap a clash row → camera pans, user sees clash from a closer angle.

---

## Suggested build order

1. **2.3a** — model + import + verify per-layer Static Meshes (PC editor session, mostly Rhino work). Save outputs to repo.
2. **Plumbing decision: `BP_BIMOverlay` vs Datasmith Scene Actor.** Pick Option 2 (Datasmith Scene Actor) unless something practical pushes back. Rewire `BP_ARPlayerController_BIM` to spawn that instead.
3. **2.3b** — `WBP_LayerTogglePanel`, BP wiring on placement, manual smoke test (toggle each layer). PC session.
4. **2.3c** — C++ `DetectBIMClashes` + logging. Mac C++ session. BP wires call after placement (and after each layer toggle). Validate against expected 2 clashes.
5. **2.3d** — material swap + clash HUD. PC BP session for material + widget; small C++ helper for the material-cache/restore if BP can't do it cleanly.

Each step ends with a device cook + stage + install + validation step. Same Mac pipeline as Node 2.2.

---

## Gate to Phase 1 cleanup

Node 2.3 closes Phase 2. The agreed first item in the post-Phase-2 Phase 1 cleanup is the **cut/fill HUD instability** — readout never settles because volume is recomputed at 10 Hz against the still-scanning LiDAR mesh (`decisions.md 2026-05-21`). Fix = freeze/finalize the measurement on slab placement, or a Recalculate button.

Other deferred items the cleanup pass should sweep:
- ~1s green flash at app startup (`decisions.md 2026-05-21`)
- AR camera passthrough not stopped when returning to the main menu (`decisions.md 2026-05-21`)
- Datum offset slider, width/thickness runtime sliders, spot elevation readouts (CLAUDE.md "Node 1.4 future polish")
- 2.2e auto-place — call `SetActorLocation` on the BIM using the computed `GeoToLocalOffsetCm` offset, so the building visually anchors at the GPS-derived spot rather than just logging the offset
- Revisit Revit-on-PC setup (per James 2026-05-27 — would unlock genuine Revit metadata for 2.3 demo models if the Rhino route runs out of steam)
- Demo polish items from the Idaho Technology Council snapshot (post-postponement, no longer time-pressured)

---

## Notes for future-Claude

- The placement primitive `PlaceBIMByCornerForward` takes `AActor*` and doesn't care about the actor's class. If Option 2 (Datasmith Scene Actor) is chosen, the existing C++ keeps working — only the BP spawn class changes.
- The new MCP `execute_python` (commit `77ad8ae`) means ad-hoc inspection (e.g., "list all components on the placed BIM with their tags") is a single tool call now. Use it before writing dev/ scripts.
- Datasmith Rhino import preserves layer **names** but flattens nested layers in default settings. If MEPRoom uses sub-layers (e.g., `HVAC/Duct/Trunk` vs `HVAC/Duct/Branch`), check the importer settings — by default they merge into the parent layer name.
- BP `SET ... on <foreign actor>` Target-pin auto-wire gotcha (`Bugs Index 2026-05-27`) will bite again when wiring component visibility on the spawned Datasmith Scene Actor. Hand-wire the Target each time.
