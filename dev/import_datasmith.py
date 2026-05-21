"""
Datasmith import helper - SiteSync AR, Node 2.1

Imports a .udatasmith bundle into the project as UE assets, using Unreal's
official Datasmith importer (the same code path the editor's importer uses).
It only creates new assets in the destination folder - nothing is deleted.

Run from the Unreal Editor console (mode dropdown set to "Cmd"):
    py "/Users/jamesrusso/Developer/SiteSync-AR/dev/import_datasmith.py"

To import a different model later, edit UDATASMITH and DEST below.
"""
import unreal

UDATASMITH = "/Users/jamesrusso/Developer/SiteSync-AR/BIM_Source/TestBuilding/TestBuilding.udatasmith"
DEST = "/Game/AR_SiteAnalysis/DatasmithAssets/TestBuilding"


def run():
    unreal.log_warning("[Datasmith] importing: {}".format(UDATASMITH))
    unreal.log_warning("[Datasmith] destination: {}".format(DEST))

    scene = unreal.DatasmithSceneElement.construct_datasmith_scene_from_file(UDATASMITH)
    if scene is None:
        unreal.log_error("[Datasmith] could not load .udatasmith - check the file path.")
        return

    try:
        result = scene.import_scene(DEST)
    finally:
        unreal.DatasmithSceneElement.destroy_scene(scene)

    if result is None or not getattr(result, "import_succeed", False):
        unreal.log_error("[Datasmith] import FAILED - see messages above.")
        return

    unreal.log_warning("[Datasmith] SUCCESS - assets created in {}".format(DEST))

    try:
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        unreal.log_warning("[Datasmith] saved to disk.")
    except Exception as e:
        unreal.log_warning("[Datasmith] auto-save skipped ({}) - do File > Save All manually.".format(e))


run()
