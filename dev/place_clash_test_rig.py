"""
Places BP_ClashTestRig in SiteSync_BIMTest at origin as the toggle-panel test vehicle.
Run with the EDITOR OPEN and SiteSync_BIMTest loaded.

    python3 dev/mcp_send.py exec_file /Users/jamesrusso/Developer/SiteSync-AR/dev/place_clash_test_rig.py

BP_ClashTestRig has 5 named StaticMeshComponents:
  Structure_Floor, Structure_Beam, HVAC_Duct, Plumbing_Pipe, Electrical_Conduit
GetBIMLayers enumerates them - perfect scaffold for WBP_LayerTogglePanel.
"""
import unreal
import traceback

ACTOR_NAME = "ClashTestRig_ToggleTest"
BP_PATH = "/Game/Blueprints/BP_ClashTestRig"


def run():
    # Load the GENERATED CLASS (not the Blueprint asset) - spawn needs a class
    rig_class = unreal.EditorAssetLibrary.load_blueprint_class(BP_PATH)
    if rig_class is None:
        unreal.log_error("[place_rig] could not load blueprint class at {}".format(BP_PATH))
        return

    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    # Skip if already placed
    for a in actor_sub.get_all_level_actors():
        if a.get_actor_label() == ACTOR_NAME:
            unreal.log_warning("[place_rig] {} already in level - skipping.".format(ACTOR_NAME))
            return

    loc = unreal.Vector(0.0, 0.0, 10.0)
    rot = unreal.Rotator(0.0, 0.0, 0.0)
    actor = actor_sub.spawn_actor_from_class(rig_class, loc, rot)
    if actor is None:
        unreal.log_error("[place_rig] spawn failed.")
        return

    actor.set_actor_label(ACTOR_NAME)
    unreal.log_warning("[place_rig] Spawned {} at origin.".format(ACTOR_NAME))

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
    unreal.log_warning("[place_rig] Level saved.")


try:
    run()
except Exception:
    unreal.log_error("[place_rig] EXCEPTION:\n" + traceback.format_exc())
