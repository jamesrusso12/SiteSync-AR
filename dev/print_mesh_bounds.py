"""
Print the local-space bounding box of a Static Mesh - SiteSync AR, Node 2.1.

Used to find the pivot-to-geometry offset so BP_BIMOverlay's BIMMesh can be
seated with the building's bottom corner on the actor origin.

Run from the Unreal Editor console (Cmd mode is fine):
    py "/Users/jamesrusso/Developer/SiteSync-AR/dev/print_mesh_bounds.py"

Output appears in the Output Log as [bounds] warning lines.
"""
import unreal

ASSET = "/Game/AR_SiteAnalysis/DatasmithAssets/TestBuilding/SM_TestBuilding_Merged"

m = unreal.load_asset(ASSET)
if m is None:
    unreal.log_error("[bounds] could not load " + ASSET)
else:
    bb = m.get_bounding_box()
    mn, mx = bb.min, bb.max
    unreal.log_warning("[bounds] MIN  x={:.1f} y={:.1f} z={:.1f}".format(mn.x, mn.y, mn.z))
    unreal.log_warning("[bounds] MAX  x={:.1f} y={:.1f} z={:.1f}".format(mx.x, mx.y, mx.z))
    unreal.log_warning("[bounds] SIZE x={:.1f} y={:.1f} z={:.1f}".format(mx.x - mn.x, mx.y - mn.y, mx.z - mn.z))
    unreal.log_warning("[bounds] => set BIMMesh Location to  x={:.1f} y={:.1f} z={:.1f}".format(-mn.x, -mn.y, -mn.z))
