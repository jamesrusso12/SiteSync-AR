"""
One-off fix: parallel-session resave of BP_BIMOverlay clobbered
TargetLongitude default back to 0.0 after commit 205904d. TargetLatitude
survived. Re-apply the 500m-north test target longitude so manual-mode
test fires correctly.

Run headless:
    UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this> -unattended -nullrhi

After this confirms, will be folded into a single commit + reverted to 0.0
after manual-mode validation passes.
"""
import unreal

ASSET_PATH = "/Game/Blueprints/BP_BIMOverlay"
TARGET_LONGITUDE = -116.1393521


def run():
    asset = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    if asset is None:
        unreal.log_error("[fix] could not load asset")
        return

    bp_class = asset.generated_class()
    cdo = unreal.get_default_object(bp_class)

    lat_before = cdo.get_editor_property("TargetLatitude")
    lon_before = cdo.get_editor_property("TargetLongitude")
    unreal.log_warning("[fix] BEFORE  lat={}  lon={}".format(lat_before, lon_before))

    cdo.set_editor_property("TargetLongitude", TARGET_LONGITUDE)

    lat_after = cdo.get_editor_property("TargetLatitude")
    lon_after = cdo.get_editor_property("TargetLongitude")
    unreal.log_warning("[fix] AFTER   lat={}  lon={}".format(lat_after, lon_after))

    if lon_after != TARGET_LONGITUDE:
        unreal.log_error("[fix] set failed - value did not change")
        return

    # CDO edits live on the Blueprint asset, so save the blueprint, not the CDO.
    saved = unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.log_warning("[fix] save_loaded_asset -> {}".format(saved))


run()
