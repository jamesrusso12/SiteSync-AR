"""
Set ARSessionConfig World Alignment to GravityAndHeading - SiteSync AR, Node 2.2b.

Flips DA_SiteSyncARConfig.WorldAlignment from Gravity to GravityAndHeading so
ARKit's tracking-space gets a known mapping to compass directions:
  ARKit:  +X = east,   +Y = up,    -Z = true north  (per Apple docs)
  UE:     +X = north,  +Y = east,  +Z = up           (after engine RH->LH conv)

This unlocks Node 2.2c (geo->local placement math) which depends on knowing
which UE axis is true north.

Run headless (UE editor must be CLOSED first - it locks the .uasset):
    "/Users/Shared/Epic Games/UE_5.6/Engine/Binaries/Mac/UnrealEditor-Cmd" \
        ~/Developer/SiteSync-AR/SiteSyncAR/SiteSyncAR.uproject \
        -run=pythonscript -script=/Users/jamesrusso/Developer/SiteSync-AR/dev/set_ar_alignment.py \
        -unattended -nullrhi

OR in-editor via the Cmd console:
    py "/Users/jamesrusso/Developer/SiteSync-AR/dev/set_ar_alignment.py"

Reads before/writes/reads after for verification, logs to UE log.
"""
import unreal

ASSET_PATH = "/Game/AR_SiteAnalysis/DA_SiteSyncARConfig"
TARGET_ALIGNMENT = unreal.ARWorldAlignment.GRAVITY_AND_HEADING


def run():
    unreal.log_warning("[2.2b] loading {}".format(ASSET_PATH))
    asset = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    if asset is None:
        unreal.log_error("[2.2b] could not load asset - check path")
        return

    before = asset.get_editor_property("world_alignment")
    unreal.log_warning("[2.2b] world_alignment BEFORE: {}".format(before))

    if before == TARGET_ALIGNMENT:
        unreal.log_warning("[2.2b] already set to GravityAndHeading - nothing to do")
        return

    asset.set_editor_property("world_alignment", TARGET_ALIGNMENT)
    after = asset.get_editor_property("world_alignment")
    unreal.log_warning("[2.2b] world_alignment AFTER:  {}".format(after))

    if after != TARGET_ALIGNMENT:
        unreal.log_error("[2.2b] set failed - value did not change")
        return

    saved = unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.log_warning("[2.2b] save_loaded_asset -> {}".format(saved))


run()
