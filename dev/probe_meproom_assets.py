"""
After importing MEPRoom via import_meproom.py, run this to verify
all expected layers arrived as distinct named StaticMeshComponents.

    python3 dev/mcp_send.py exec_file dev/probe_meproom_assets.py

Expected layers:
  Structure_Floor, Structure_Walls, Structure_Roof, Structure_Beam,
  HVAC_Duct, Plumbing_Pipe, Electrical_Conduit
"""
import unreal

EXPECTED_LAYERS = [
    "Structure_Floor", "Structure_Walls", "Structure_Roof", "Structure_Beam",
    "HVAC_Duct", "Plumbing_Pipe", "Electrical_Conduit",
]


def run():
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = ar.get_assets_by_path("/Game/AR_SiteAnalysis/DatasmithAssets/MEPRoom", recursive=True)

    sm_names = []
    for a in assets:
        if a.asset_class_path.asset_name == "StaticMesh":
            sm_names.append(str(a.asset_name))

    unreal.log_warning("[probe_meproom] Static Meshes found: {}".format(sm_names))

    missing = [e for e in EXPECTED_LAYERS if not any(e in n for n in sm_names)]
    if missing:
        unreal.log_error("[probe_meproom] MISSING layers: {}".format(missing))
    else:
        unreal.log_warning("[probe_meproom] All expected layers present.")


run()
