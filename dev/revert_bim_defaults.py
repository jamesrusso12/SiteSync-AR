"""
Revert BP_BIMOverlay TargetLat/Long class defaults to 0.0 after manual-mode
test passed (2026-05-27). Capture mode is the production default; manual
mode is opt-in per-instance (level designers set non-zero TargetLat/Long
on individual BIM placements when they want surveyed-coord anchoring).

Run headless:
    UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this> -unattended -nullrhi
"""
import unreal

ASSET_PATH = "/Game/Blueprints/BP_BIMOverlay"


def run():
    asset = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    if asset is None:
        unreal.log_error("[revert] could not load asset")
        return

    bp_class = asset.generated_class()
    cdo = unreal.get_default_object(bp_class)

    lat_before = cdo.get_editor_property("TargetLatitude")
    lon_before = cdo.get_editor_property("TargetLongitude")
    unreal.log_warning("[revert] BEFORE  lat={}  lon={}".format(lat_before, lon_before))

    cdo.set_editor_property("TargetLatitude", 0.0)
    cdo.set_editor_property("TargetLongitude", 0.0)

    lat_after = cdo.get_editor_property("TargetLatitude")
    lon_after = cdo.get_editor_property("TargetLongitude")
    unreal.log_warning("[revert] AFTER   lat={}  lon={}".format(lat_after, lon_after))

    saved = unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.log_warning("[revert] save_loaded_asset -> {}".format(saved))


run()
