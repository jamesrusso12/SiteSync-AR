"""
Probe how Datasmith preserved Rhino layer / object naming on the TestBuilding
import. Goal: learn what field GetBIMLayers should read (mesh asset name vs
Datasmith actor name vs Datasmith Layers vs metadata tags) BEFORE we model
MEPRoom — so MEPRoom is structured for addressable runtime layers the first
time.

Finding so far: the 6 mesh ASSETS imported as extrusion / extrusion_2..6
(generic), NOT as Rhino layer names. This script digs into the DatasmithScene
asset to see if layer/object names survived anywhere addressable.

Run headless:
    UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this> -unattended -nullrhi
"""
import unreal

SCENE_PATH = "/Game/AR_SiteAnalysis/DatasmithAssets/TestBuilding/TestBuilding/TestBuilding"


def dump(label, val):
    unreal.log_warning("[dlayers] {}: {}".format(label, val))


def run():
    scene = unreal.EditorAssetLibrary.load_asset(SCENE_PATH)
    if scene is None:
        unreal.log_error("[dlayers] could not load DatasmithScene at {}".format(SCENE_PATH))
        return

    dump("scene asset class", scene.get_class().get_name())

    # UDatasmithScene exposes its element tree via the Datasmith API in some
    # versions; try the common accessors defensively.
    for prop in ("element_count", "actor_count", "mesh_count"):
        try:
            dump(prop, scene.get_editor_property(prop))
        except Exception as e:
            dump(prop + " (n/a)", str(e))

    # Walk the imported Static Mesh assets and report any tags / metadata.
    mesh_dir = "/Game/AR_SiteAnalysis/DatasmithAssets/TestBuilding/TestBuilding/Geometries"
    assets = unreal.EditorAssetLibrary.list_assets(mesh_dir, recursive=True)
    dump("mesh assets found", len(assets))
    for a in assets:
        sm = unreal.EditorAssetLibrary.load_asset(a)
        if sm is None:
            continue
        name = sm.get_name()
        # Datasmith stuffs source info into asset import data / metadata.
        meta = "n/a"
        try:
            tags = unreal.EditorAssetLibrary.get_metadata_tag(sm, "Datasmith.LayerName")
            meta = tags
        except Exception:
            pass
        dump("  mesh", "{}  Datasmith.LayerName={}".format(name, meta))

    # Also list ALL metadata tags on the first mesh so we see what keys exist.
    if assets:
        sm0 = unreal.EditorAssetLibrary.load_asset(assets[0])
        if sm0:
            try:
                all_tags = unreal.EditorAssetLibrary.get_metadata_tag_values(sm0)
                dump("first-mesh ALL metadata keys", list(all_tags.keys()) if all_tags else "none")
            except Exception as e:
                dump("metadata enum failed", str(e))


run()
