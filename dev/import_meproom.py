"""
Datasmith import helper - SiteSync AR, Node 2.3 MEPRoom

Imports BIM_Source/MEPRoom/MEPRoom.udatasmith into UE assets.
Run from UE editor console (mode: Cmd):
    py "/Users/jamesrusso/Developer/SiteSync-AR/dev/import_meproom.py"

Or via mcp_send.py:
    python3 dev/mcp_send.py exec_file dev/import_meproom.py
"""
import unreal

UDATASMITH = "/Users/jamesrusso/Developer/SiteSync-AR/BIM_Source/MEPRoom/MEPRoom.udatasmith"
DEST = "/Game/AR_SiteAnalysis/DatasmithAssets/MEPRoom"


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
