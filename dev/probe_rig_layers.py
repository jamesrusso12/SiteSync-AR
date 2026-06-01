"""
Verify GetBIMLayers returns the expected component names on the placed
ClashTestRig_ToggleTest actor. This is exactly what WBP_LayerTogglePanel's
PopulateFromActor will call at runtime.

    python3 dev/mcp_send.py exec_file /Users/jamesrusso/Developer/SiteSync-AR/dev/probe_rig_layers.py
"""
import unreal
import traceback

ACTOR_NAME = "ClashTestRig_ToggleTest"


def run():
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    target = None
    for a in actor_sub.get_all_level_actors():
        if a.get_actor_label() == ACTOR_NAME:
            target = a
            break
    if target is None:
        unreal.log_error("[probe_rig] {} not found in level.".format(ACTOR_NAME))
        return

    # GetBIMLayers(BIM) -> (OutLayerNames, OutComponents) as a tuple in Python
    result = unreal.ARMeshBlueprintLibrary.get_bim_layers(target)
    unreal.log_warning("[probe_rig] raw result: {}".format(result))

    # Result is a tuple: (names_array, components_array)
    if isinstance(result, tuple):
        names = result[0]
        comps = result[1]
        unreal.log_warning("[probe_rig] {} layers: {}".format(len(names), [str(n) for n in names]))
        for n, c in zip(names, comps):
            unreal.log_warning("    {} -> {}".format(n, c.get_name() if c else "None"))
    else:
        unreal.log_warning("[probe_rig] unexpected result shape: {}".format(type(result)))


try:
    run()
except Exception:
    unreal.log_error("[probe_rig] EXCEPTION:\n" + traceback.format_exc())
