# Ant Kitchen: the whole kitchen as separate game-ready FBX pieces (bodies + moving parts + props) and a manifest.
# Run:  blender --background --factory-startup --python kitchen_pieces.py -- <out_dir> [dry]
# Parts: kitchen_lib.py (helpers, export, manifest), kitchen_p1.py (north run), kitchen_p2.py (appliances),
#        kitchen_p3.py (furniture, room), kitchen_p4.py (props). Preview: kitchen_preview.py.
import bpy, sys, os, time
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else os.path.join(HERE, "..", "..", "SourceArt", "Blender", "kitchen")
DRY = len(argv) > 1 and argv[1] == "dry"
bpy.ops.wm.read_factory_settings(use_empty=True)
import kitchen_lib as L
mat = L.mat

# ---- palette: the same slots as kitchen_set.py
mat("Cabinet", (0.42, 0.60, 0.44), 0.5)
mat("CabinetInset", (0.34, 0.51, 0.37), 0.55)
mat("Butcher", (0.68, 0.40, 0.16), 0.4)
mat("Walnut", (0.33, 0.20, 0.10), 0.5)
mat("Plinth", (0.08, 0.08, 0.09), 0.7)
mat("Brass", (0.86, 0.62, 0.22), 0.3, 1.0)
mat("Steel", (0.72, 0.74, 0.76), 0.28, 1.0)
mat("Chrome", (0.9, 0.9, 0.92), 0.12, 1.0)
mat("Enamel", (0.95, 0.94, 0.90), 0.25)
mat("BlackGlass", (0.02, 0.02, 0.025), 0.08)
mat("Iron", (0.05, 0.05, 0.05), 0.6, 0.6)
mat("FridgeRed", (0.80, 0.16, 0.12), 0.2)
mat("Tile", (0.96, 0.95, 0.90), 0.12)
mat("Grout", (0.55, 0.55, 0.52), 0.9)
mat("Red", (0.75, 0.10, 0.08), 0.35)
mat("Fabric", (0.96, 0.72, 0.16), 0.9)
mat("Terracotta", (0.72, 0.35, 0.20), 0.8)
mat("Leaf", (0.16, 0.50, 0.18), 0.5)
mat("Paper", (0.97, 0.96, 0.88), 0.9)
# also present in kitchen_set.py
mat("Honey", (0.90, 0.55, 0.05), 0.15)
mat("Jam", (0.55, 0.05, 0.12), 0.15)
mat("Sugar", (0.95, 0.95, 0.95), 0.6)
mat("Blue", (0.1, 0.25, 0.75), 0.35)
mat("BurnerRing", (0.65, 0.12, 0.05), 0.4)
mat("MugBlue", (0.15, 0.35, 0.6), 0.3)
# ---- new slots (only where nothing above fits)
mat("FridgeLiner", (0.86, 0.90, 0.93), 0.35, new=True)
mat("ShelfGlass", (0.62, 0.82, 0.88), 0.1, new=True)
mat("OvenLiner", (0.035, 0.03, 0.03), 0.6, new=True)
mat("HeatGlow", (1.0, 0.30, 0.08), 0.5, new=True, emit=4.0)
mat("PlasticBlue", (0.12, 0.30, 0.75), 0.35, new=True)
mat("Sponge", (0.92, 0.80, 0.20), 0.95, new=True)
mat("Cheese", (0.96, 0.78, 0.22), 0.5, new=True)
mat("Toast", (0.80, 0.55, 0.25), 0.85, new=True)
mat("CakePink", (0.96, 0.58, 0.70), 0.6, new=True)
mat("Cardboard", (0.70, 0.52, 0.32), 0.9, new=True)
mat("Kibble", (0.42, 0.24, 0.10), 0.9, new=True)
mat("Rubber", (0.20, 0.28, 0.25), 0.9, new=True)
mat("TennisBall", (0.78, 0.90, 0.22), 0.9, new=True)
mat("Leather", (0.24, 0.17, 0.13), 0.6, new=True)
mat("LampShade", (0.98, 0.94, 0.82), 0.6, new=True)
mat("Ice", (0.78, 0.92, 1.0), 0.1, new=True)

HEADER = {
    "what": "Ant Kitchen: kitchen pieces modelled in Blender (Tools/blender/kitchen_pieces.py). One FBX per piece, default FBX export scale.",
    "conventions": {
        "blender": "metres, +Z up, each piece's front faces -Y, mesh origin as stated per piece",
        "game_scale": "1 m = 1900 uu; a default UE FBX import gives 1 m = 100 uu, so use component scale 19",
        "ue_local": "default UE FBX import maps a Blender point (x, y, z) m to mesh-local (100x, -100y, 100z)",
        "placement": "every instance lists game_loc_uu (location of the mesh origin) and game_axis_x/y/z (where the mesh-local UE axes point in the game); game_yaw_deg is given when the rotation is a pure yaw. North-run pieces use yaw 180.",
        "scene_frame": "blender_loc_m / blender_rot_3x3 are in the preview scene frame S = (-gameX, gameY, gameZ) / 1900 (a true, unmirrored view of the game)",
        "hinge": "motion.axis_blender + angle_deg use the right-hand rule in Blender piece space; axis_ue_local + angle_ue_local_deg are the same motion for FQuat(Axis, Radians) in the part's UE mesh-local space; hinge_axis_game + open_angle_game_deg per instance are in game space",
        "slide": "vector_m in Blender piece space, vector_ue_local_uu in UE mesh-local space at scale 19, slide_game_uu per instance in game space",
        "left_right": "DoorL / DoorR are named as seen from the front in the true view: L is the game-east half (hinge on its east edge), R the game-west half",
        "children": "offset_from_parent_m = child origin - parent origin in Blender piece space; offset_from_parent_ue_local_uu the same in the parent's UE mesh-local space at scale 19",
        "replaces": "the box / function in the C++ that the piece stands in for; bbox_minus_replaced_uu = (piece min - box min, piece max - box max) for the first instance",
        "sits_on": "per instance of a prop: the piece it stands on (and which instance of it) with the offset in that piece's Blender space"
    },
    "notes": [
        "Not imported into Unreal yet: the UE-side numbers (yaw, ue_local axes) follow the standard FBX import convention and were checked only by re-importing the FBX into Blender (kitchen_preview.py, kitchen_check.py).",
        "Collision: none is authored (no UCX hulls). Interiors are real geometry, so use complex-as-simple collision or build hulls in the engine.",
        "UVs: box projection in metres (1 UV unit = 1 m), meant for tiling / triplanar materials; no lightmap channel.",
        "Doors open 110 deg in the manifest; doors hinged next to the west wall (SM_K_Base1400_DoorR, the first SM_K_WallCab1250_DoorR) and neighbours that face each other should be limited in the engine (about 90 deg).",
        "The drawer unit is centred at X=-4900 (the game's drawer gadget sits at X=-4950); drawers 2 and 3 and the freezer door are new moving parts with no gadget in the C++ yet.",
        "Range: five knobs at X=-1380,-1150,-920,-690,-460 (the game has four at -1250+330*i); the stove gadget's knob (-920) is the middle one.",
        "Fridge: the crisper drawer takes the floor of the compartment, so milk and butter moved to shelf 1; loot spots on the fridge floor now fall inside the crisper tray.",
        "The floor rolling pin lies in the sweep of the two lower drawers (same spot as in the game)."
    ]
}

t0 = time.time()
import kitchen_p1
kitchen_p1.build_base()
kitchen_p1.build_wall()
for modname in ("kitchen_p2", "kitchen_p3", "kitchen_p4"):
    if os.path.exists(os.path.join(HERE, modname + ".py")):
        mod = __import__(modname)
        mod.build()
print("BUILT %d pieces in %.1fs" % (len(L.ORDER), time.time() - t0))
if not DRY:
    L.export_all(OUT)
    print("EXPORTED in %.1fs" % (time.time() - t0))
os.makedirs(OUT, exist_ok=True)
doc = L.write_manifest(OUT, HEADER)
L.report(doc)
print("KITCHEN_PIECES_DONE %.1fs" % (time.time() - t0))