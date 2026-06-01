"""
Places BP_ClashTestRig in SiteSync_BIMTest at origin as the toggle-panel test vehicle.
Run with the EDITOR OPEN and SiteSync_BIMTest loaded (or it will open it).

    python3 dev/mcp_send.py exec_file dev/place_clash_test_rig.py

BP_ClashTestRig has 5 named StaticMeshComponents:
  Structure_Floor, Structure_Beam, HVAC_Duct, Plumbing_Pipe, Electrical_Conduit
GetBIMLayers enumerates them — perfect scaffold for WBP_LayerTogglePanel.
"""
import unreal

ACTOR_NAME = "ClashTestRig_ToggleTest"
BP_PATH = "/Game/AR_SiteAnalysis/Test/BP_ClashTestRig"


def run():
    # Load the blueprint class
    rig_class = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    if rig_class is None:
        unreal.log_error("[place_rig] BP_ClashTestRig not found at {}".format(BP_PATH))
        return

    # Check if already in level
    world = unreal.EditorLevelLibrary.get_editor_world()
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, rig_class)
    for a in actors:
        if a.get_actor_label() == ACTOR_NAME:
            unreal.log_warning("[place_rig] {} already in level — skipping.".format(ACTOR_NAME))
            return

    # Spawn at origin, elevated so it sits above the floor plane
    loc = unreal.Vector(0.0, 0.0, 10.0)
    rot = unreal.Rotator(0.0, 0.0, 0.0)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(rig_class, loc, rot)
    if actor is None:
        unreal.log_error("[place_rig] spawn failed.")
        return

    actor.set_actor_label(ACTOR_NAME)
    unreal.log_warning("[place_rig] Spawned {} at origin.".format(ACTOR_NAME))

    # Save the level
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
    unreal.log_warning("[place_rig] Level saved.")


run()
