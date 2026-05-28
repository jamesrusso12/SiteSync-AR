"""
Build BP_ClashTestRig — a synthetic Node 2.3 clash-test fixture, headless.
One Actor BP with 4 named StaticMeshComponents (engine cubes) positioned with
two DELIBERATE intersections, so DetectBIMClashes has a deterministic target
to validate against without MEPRoom / editor GUI / James.

Layout (cm; engine cube is 100cm at scale 1):
  Structure_Floor : scale (6, 5, 0.2)  at (0,0,0)
  Structure_Beam  : scale (0.3,5,0.4)  at (0,0,250)
  HVAC_Duct       : scale (4,0.4,0.4)  at (0,0,250)  -> INTERSECTS beam (clash #1)
  Plumbing_Pipe   : scale (0.1,0.1,3)  at (200,0,0)  -> INTERSECTS floor (clash #2)
  Electrical_Conduit: scale (3,0.1,0.1) at (0,200,300) -> clear (control)

Uses SubobjectDataSubsystem (the documented way to add SCS components to a BP
programmatically). If this API path fails, the log says exactly where.

Run headless:
    UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this> -unattended -nullrhi
"""
import unreal

DEST_DIR = "/Game/AR_SiteAnalysis/Test"
BP_NAME = "BP_ClashTestRig"
CUBE = "/Engine/BasicShapes/Cube"

# (component name, scale, location_cm)
COMPONENTS = [
    ("Structure_Floor",     unreal.Vector(6.0, 5.0, 0.2),   unreal.Vector(0, 0, 0)),
    ("Structure_Beam",      unreal.Vector(0.3, 5.0, 0.4),   unreal.Vector(0, 0, 250)),
    ("HVAC_Duct",           unreal.Vector(4.0, 0.4, 0.4),   unreal.Vector(0, 0, 250)),
    ("Plumbing_Pipe",       unreal.Vector(0.1, 0.1, 3.0),   unreal.Vector(200, 0, 0)),
    ("Electrical_Conduit",  unreal.Vector(3.0, 0.1, 0.1),   unreal.Vector(0, 200, 300)),
]


def log(m):
    unreal.log_warning("[rig] {}".format(m))


def run():
    cube = unreal.EditorAssetLibrary.load_asset(CUBE)
    if cube is None:
        unreal.log_error("[rig] cube not found")
        return

    # Create the Blueprint (Actor parent).
    if not unreal.EditorAssetLibrary.does_directory_exist(DEST_DIR):
        unreal.EditorAssetLibrary.make_directory(DEST_DIR)

    full_path = "{}/{}".format(DEST_DIR, BP_NAME)
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        unreal.EditorAssetLibrary.delete_asset(full_path)
        log("deleted existing {}".format(full_path))

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.Actor)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    bp = asset_tools.create_asset(BP_NAME, DEST_DIR, None, factory)
    if bp is None:
        unreal.log_error("[rig] create_asset returned None")
        return
    log("created BP {}".format(bp.get_name()))

    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    if subsystem is None:
        unreal.log_error("[rig] no SubobjectDataSubsystem")
        return

    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    log("root subobject handles: {}".format(len(handles)))
    if not handles:
        unreal.log_error("[rig] no root handle")
        return
    root_handle = handles[0]

    added = 0
    for (name, scale, loc) in COMPONENTS:
        params = unreal.AddNewSubobjectParams(
            parent_handle=root_handle,
            new_class=unreal.StaticMeshComponent,
            blueprint_context=bp)
        sub_handle, fail = subsystem.add_new_subobject(params)
        if not fail.is_empty():
            log("  add_new_subobject('{}') FAILED: {}".format(name, fail))
            continue
        subsystem.rename_subobject(sub_handle, unreal.Text(name))

        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(
            unreal.SubobjectDataBlueprintFunctionLibrary.get_data(sub_handle))
        smc = unreal.StaticMeshComponent.cast(obj)
        if smc:
            smc.set_static_mesh(cube)
            smc.set_editor_property("relative_scale3d", scale)
            smc.set_editor_property("relative_location", loc)
            added += 1
            log("  added '{}' scale={} loc={}".format(name, scale, loc))
        else:
            log("  '{}' added but object cast failed".format(name))

    log("components added: {}/{}".format(added, len(COMPONENTS)))

    unreal.EditorAssetLibrary.save_asset(full_path)
    log("saved {}".format(full_path))


run()
