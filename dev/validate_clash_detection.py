"""
Headless validation of GetBIMLayers + DetectBIMClashes against the synthetic
BP_ClashTestRig (built by dev/build_clash_test_rig.py). Fully Mac-only — no
editor GUI, no MEPRoom, no device.

Expected:
  GetBIMLayers      -> 5 layers (Floor, Beam, HVAC_Duct, Plumbing_Pipe, Conduit)
  DetectBIMClashes  -> 2 clashes:
                         HVAC_Duct x Structure_Beam   (duct through beam)
                         Plumbing_Pipe x Structure_Floor (pipe through floor)
                       Electrical_Conduit clear (control).

Run:
    UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this> -unattended -nullrhi
"""
import unreal

RIG = "/Game/AR_SiteAnalysis/Test/BP_ClashTestRig"


def log(m):
    unreal.log_warning("[validate] {}".format(m))


def run():
    bp_class = unreal.EditorAssetLibrary.load_blueprint_class(RIG)
    if bp_class is None:
        unreal.log_error("[validate] could not load class {}".format(RIG))
        return

    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = eas.spawn_actor_from_class(bp_class, unreal.Vector(0, 0, 0))
    if actor is None:
        unreal.log_error("[validate] spawn failed")
        return
    log("spawned rig instance: {}".format(actor.get_name()))

    # --- GetBIMLayers ---
    names, comps = unreal.ARMeshBlueprintLibrary.get_bim_layers(actor)
    log("GetBIMLayers -> {} layers: {}".format(len(names), [str(n) for n in names]))
    layers_ok = (len(names) == 5)

    # --- DetectBIMClashes (no exclusions) ---
    count, clashes = unreal.ARMeshBlueprintLibrary.detect_bim_clashes(actor, [])
    log("DetectBIMClashes -> {} clashes".format(count))
    pairs = set()
    for c in clashes:
        a = str(c.get_editor_property("layer_a"))
        b = str(c.get_editor_property("layer_b"))
        ctr = c.get_editor_property("approx_center")
        log("  clash: {} x {}  @ {}".format(a, b, ctr))
        pairs.add(frozenset((a, b)))

    expected = {
        frozenset(("HVAC_Duct", "Structure_Beam")),
        frozenset(("Plumbing_Pipe", "Structure_Floor")),
    }
    clashes_ok = (count == 2 and pairs == expected)

    log("=== RESULT: layers {} | clashes {} ===".format(
        "PASS" if layers_ok else "FAIL",
        "PASS" if clashes_ok else "FAIL (got {} expected {})".format(
            [tuple(p) for p in pairs], [tuple(p) for p in expected])))

    # cleanup the spawned instance (transient world, but be tidy)
    eas.destroy_actor(actor)


run()
