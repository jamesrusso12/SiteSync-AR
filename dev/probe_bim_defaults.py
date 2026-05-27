"""
Quick probe: read BP_BIMOverlay's CDO (class default object) TargetLatitude
and TargetLongitude so we can confirm whether the parallel-session save
(bf01a62) clobbered our test defaults set in 205904d.

Run headless:
    UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this> -unattended -nullrhi
"""
import unreal

ASSET_PATH = "/Game/Blueprints/BP_BIMOverlay"


def run():
    asset = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    if asset is None:
        unreal.log_error("[probe] could not load asset")
        return

    # asset is the UBlueprint; CDO lives on its GeneratedClass.
    bp_class = asset.generated_class()
    cdo = unreal.get_default_object(bp_class)

    lat = cdo.get_editor_property("TargetLatitude")
    lon = cdo.get_editor_property("TargetLongitude")

    unreal.log_warning("[probe] BP_BIMOverlay class defaults:")
    unreal.log_warning("[probe]   TargetLatitude  = {}".format(lat))
    unreal.log_warning("[probe]   TargetLongitude = {}".format(lon))


run()
