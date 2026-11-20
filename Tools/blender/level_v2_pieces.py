# Ant Kitchen level v2: every NEW piece of the redesigned level as game-ready FBX + a manifest in the kitchen-set format.
# Run:  blender --background --factory-startup --python level_v2_pieces.py -- <out_dir> [dry]
# Parts: level_v2_common.py (helpers, palette), level_v2_p1.py (kitchen), level_v2_p2.py (living room), level_v2_p3.py (yard, street).
# Where things stand comes from Tools/level/level_v2_layout.py (PUT / PLACE). Existing pieces that only move are not re-exported:
# Tools/level/build_level_v2.py writes the unified manifest of the whole v2 level (level_v2_manifest.json).
import bpy, sys, os, time, json
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else os.path.join(HERE, "..", "..", "SourceArt", "Blender", "level_v2")
DRY = len(argv) > 1 and argv[1] == "dry"
bpy.ops.wm.read_factory_settings(use_empty=True)
import kitchen_lib as L
import level_v2_common as C
import level_v2_p1, level_v2_p2, level_v2_p3
HEADER = {
    "what": "Ant Kitchen level v2: the NEW pieces (Tools/blender/level_v2_pieces.py). One FBX per piece, default FBX export scale. Same conventions as kitchen_manifest.json.",
    "conventions": {
        "blender": "metres, +Z up, each piece's front faces -Y, mesh origin as stated per piece",
        "game_scale": "1 m = 1900 uu; a default UE FBX import gives 1 m = 100 uu, so use component scale 19",
        "placement": "every instance lists game_loc_uu and game_axis_x/y/z (where the mesh-local UE axes point in the game); game_yaw_deg when the rotation is a pure yaw; pieces modelled in place have yaw 180",
        "names": "SM_K_* load from /Game/Kitchen, everything else from /Game/Living (KitchenArt::Mesh picks the folder by the prefix)",
        "materials": "only slot names that KitchenArt.cpp already has a look for",
    },
    "notes": ["Not imported into Unreal. Collision: none authored, use complex-as-simple like the kitchen set.",
              "Pieces whose layout entry says collision ants_only (yarn, garland, chili string, fly ribbon) must not block the Human's capsule."],
}
t0 = time.time()
for mod in (level_v2_p1, level_v2_p2, level_v2_p3):
    mod.build()
print("BUILT %d pieces in %.1fs" % (len(L.ORDER), time.time() - t0))
os.makedirs(OUT, exist_ok=True)
if not DRY:
    L.export_all(OUT)
    print("EXPORTED in %.1fs" % (time.time() - t0))
doc = L.write_manifest(OUT, HEADER)
src, dst = os.path.join(OUT, "kitchen_manifest.json"), os.path.join(OUT, "level_v2_new_pieces.json")
if os.path.exists(dst):
    os.remove(dst)
os.rename(src, dst)
L.report(doc)
bad = [e["name"] for e in doc["pieces"] if e.get("open_edges")]
missing = [n for n in C.V.MESH if n not in L.PIECES]
print("OPEN_EDGE_PIECES %s" % bad)
print("MISSING_PIECES %s" % missing)
print("LEVEL_V2_PIECES_DONE %.1fs" % (time.time() - t0))