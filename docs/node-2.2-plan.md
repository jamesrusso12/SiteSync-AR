# Node 2.2 — Geospatial & Compass Anchoring · Implementation Plan

**Read this at the start of the Node 2.2 session.** Self-contained — written 2026-05-21 at the end of the session that closed Node 2.1 + Tier B.

---

## Session-start context

- Working tree: Mac canonical `~/Developer/SiteSync-AR/` (HARD RULE — see `CLAUDE.md`).
- Read `CLAUDE.md` + the four `memory/` files first (session-start protocol).
- Node 2.1 (Datasmith ingestion) — ✅ complete, device-validated 2026-05-21. Tier B menu launcher — ✅ complete. Node 2.2 is the next roadmap node.
- James is Blueprint-first; escalate to C++ only where Blueprints can't do the job. Node-by-node BP walkthroughs with pin names.

---

## Goal

Auto-align the BIM model to real-world site coordinates: given a building's surveyed GPS corner and true-north orientation, place it at the correct geographic position and bearing **automatically** — replacing or augmenting the manual two-tap placement from Node 2.1.

---

## ⚠️ Outdoor / daytime testing required

Node 2.2 is fundamentally an **outdoor** feature. GPS gets no usable fix indoors (no sky view); compass heading needs distance from metal/interference. **Schedule the device-validation steps for an outdoor daytime session.** The scaffolding (C++, config, HUD) can be built and compiled indoors — accuracy validation cannot.

## ⚠️ GPS accuracy reality

Consumer iPhone GPS is **~3–10 m** horizontal accuracy. For 1:1 BIM clash detection that is coarse — a building placed 5 m off won't clash-check accurately. So Node 2.2's GPS anchoring delivers a **"right area, right bearing" coarse auto-placement**. Precise alignment likely keeps the Node 2.1 manual two-tap as a refinement layer on top, or needs RTK-GPS hardware (out of scope). Design 2.2d accordingly — GPS auto-place **+** manual nudge, not GPS alone.

---

## Architecture — four parts

### 2.2a — GPS plumbing
- UE `LocationServices` plugin (`ULocationServicesBPLibrary`) — already mounted in the editor; **verify it's enabled in `SiteSyncAR.uproject`**, enable if not.
- Already BlueprintCallable: `InitLocationServices`, `StartLocationService`, `GetLastKnownLocation` → `FLocationServicesData` (latitude, longitude, altitude, horizontal/vertical accuracy, timestamp). Blueprint-first — 2.2a can be pure BP.
- iOS plist: needs `NSLocationWhenInUseUsageDescription`. The project already declares location usage strings (`DefaultGame.ini`) — verify they cover location-when-in-use.
- Build: start location services on BeginPlay (BIM controller or a dedicated actor), poll `GetLastKnownLocation`.
- New debug HUD `WBP_GeoReadout`: lat / long / altitude / horizontal accuracy.
- Indoor check: compiles + deploys, LocationServices returns *a* value. Real accuracy check is outdoors.

### 2.2b — Compass / true-north alignment
- `DA_SiteSyncARConfig` → **World Alignment**: `Gravity` → **`Gravity and Heading`**. One field on the data asset — makes the ARKit world coordinate system true-north aligned.
- First device step: confirm **which UE world axis points true north** under GravityAndHeading (log the device transform against a known landmark bearing). Document it — 2.2c depends on it.
- Caveats: needs compass calibration; heading is unreliable near metal/interference; adds a small startup delay while ARKit acquires heading (may lengthen the known green-startup-flash window — see `decisions.md 2026-05-21`).

### 2.2c — Geo → local placement math (C++)
- New BlueprintCallable in `ARMeshBlueprintLibrary` (or a new `UGeoBlueprintLibrary`):
  `static FVector GeoToLocalOffsetCm(double DeviceLat, double DeviceLong, double TargetLat, double TargetLong)`
- Equirectangular projection (sub-metre error within ~1 km — fine at site scale):
  - `metersNorth = radians(TargetLat − DeviceLat) × 6378137`
  - `metersEast  = radians(TargetLong − DeviceLong) × 6378137 × cos(radians(DeviceLat))`
  - Map North / East → UE world axes per the 2.2b axis finding; × 100 → cm.
- Log raw inputs → output (per the raw→clamped logging standard set 2026-05-21).

### 2.2d — Workflow / UX
- Decide how the user supplies the building's **site datum** (real-world anchor lat/long + bearing). Candidates:
  - A "site origin" lat/long field on a data asset or on `BP_BIMOverlay`.
  - "Stand at the building corner, tap to capture device GPS as the corner."
  - A small input UI.
- Auto-place: compute `GeoToLocalOffsetCm(deviceGPS, buildingGPS)`, place `BP_BIMOverlay` at that offset, oriented to true north + the building's bearing.
- Keep Node 2.1's two-tap placement available as a manual refinement / fallback (see GPS-accuracy note above).
- GPS-status HUD: surface horizontal accuracy so the user knows if the fix is trustworthy.

---

## Suggested build order

1. **2.2a** — GPS shim + `WBP_GeoReadout` debug HUD. Build indoors, deploy.
2. **2.2b** — `DA_SiteSyncARConfig` World Alignment → Gravity and Heading. Config change.
3. **Outdoor session** — validate GPS fix + accuracy; confirm the true-north UE axis.
4. **2.2c** — geo→local C++ math.
5. **2.2d** — workflow + auto-place; outdoor-validate against a known coordinate.

---

## Gate to Node 2.3

The BIM model auto-places at a GPS-derived site location with true-north orientation, validated **outdoors** at a known coordinate, with GPS accuracy surfaced in the HUD. The Node 2.1 manual two-tap is retained as a refinement layer.
