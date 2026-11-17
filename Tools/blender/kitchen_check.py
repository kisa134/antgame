# Ant Kitchen: measure the EXPORTED pieces - the passages ants need must be open, the walls must be there.
# Run:  blender --background --factory-startup --python kitchen_check.py -- <kitchen_dir>
import bpy, sys, os, json
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

DIR = sys.argv[sys.argv.index("--") + 1]
K = 1.0 / 1900.0
bpy.ops.wm.read_factory_settings(use_empty=True)
doc = json.load(open(os.path.join(DIR, "kitchen_manifest.json"), encoding="utf-8"))
BY = {e["name"]: e for e in doc["pieces"]}
TREES = {}

def tree(name):
    if name not in TREES:
        before = set(bpy.data.objects)
        fp = os.path.join(DIR, BY[name]["file"])
        try:
            bpy.ops.wm.fbx_import(filepath=fp)
        except Exception:
            bpy.ops.import_scene.fbx(filepath=fp)
        ob = [o for o in bpy.data.objects if o not in before and o.type == "MESH"][0]
        M = ob.matrix_world
        vs = [M @ v.co for v in ob.data.vertices]
        ps = [tuple(p.vertices) for p in ob.data.polygons]
        TREES[name] = BVHTree.FromPolygons(vs, ps)
    return TREES[name]

def seg(names, A, B, inst=0):
    # First hit of the game-space segment A -> B on any of the pieces (their instance `inst`), as a game-space point, or None.
    best = None
    for name in names:
        i = BY[name]["instances"][min(inst, len(BY[name]["instances"]) - 1)]
        R = Matrix(i["blender_rot_3x3"])
        t = Vector(i["blender_loc_m"])
        a = R.transposed() @ (Vector((-A[0] * K, A[1] * K, A[2] * K)) - t)
        b = R.transposed() @ (Vector((-B[0] * K, B[1] * K, B[2] * K)) - t)
        d = b - a
        hit = tree(name).ray_cast(a, d.normalized(), d.length)
        if hit[0] is not None:
            f = hit[3] / d.length
            if best is None or f < best:
                best = f
    if best is None:
        return None
    return [round(A[k] + (B[k] - A[k]) * best, 1) for k in range(3)]

CHECKS = [
    ("oven: gnawed hole in the back wall is open", ["SM_K_Range"], (-700, 4600, 365), (-700, 4800, 365), "clear"),
    ("oven: back wall is solid elsewhere", ["SM_K_Range"], (-300, 4600, 800), (-300, 4800, 800), "hit"),
    ("oven: cavity is open from the front to the back wall (door excluded)", ["SM_K_Range"], (-750, 2900, 1000), (-750, 4690, 1000), "clear"),
    ("range: 28 uu gap underneath, front to back", ["SM_K_Range"], (-750, 2900, 14), (-750, 4800, 14), "clear"),
    ("under-sink: hole in the back panel is open", ["SM_K_BaseSink"], (-3500, 4600, 230), (-3500, 4800, 230), "clear"),
    ("under-sink: back panel is solid above the hole", ["SM_K_BaseSink"], (-3500, 4600, 600), (-3500, 4800, 600), "hit"),
    ("base cabinet: slot under the closed doors leads inside", ["SM_K_Base1300", "SM_K_Base1300_DoorL", "SM_K_Base1300_DoorR"], (-2450, 2900, 152), (-2450, 3600, 152), "clear"),
    ("base cabinet: closed door blocks at mid height", ["SM_K_Base1300_DoorL", "SM_K_Base1300_DoorR"], (-2450, 2900, 800), (-2450, 3600, 800), "hit"),
    ("drawers: slot under the bottom drawer front", ["SM_K_BaseDrawers", "SM_K_BaseDrawers_Drawer3"], (-4900, 2900, 160), (-4900, 3600, 160), "clear"),
    ("drawer 1: open-top tray (ray from above reaches the tray floor)", ["SM_K_BaseDrawers_Drawer1"], (-4900, 3800, 1640), (-4900, 3800, 1300), "hit"),
    ("fridge: gap in the divider at the back is open", ["SM_K_Fridge"], (3650, 4560, 2400), (3650, 4560, 2700), "clear"),
    ("fridge: divider is solid in the middle", ["SM_K_Fridge"], (3000, 4000, 2400), (3000, 4000, 2700), "hit"),
    ("fridge: 120 uu gap underneath, front to back", ["SM_K_Fridge", "SM_K_Fridge_DoorMain"], (3700, 2900, 60), (3700, 4900, 60), "clear"),
    ("fridge: compartment is hollow from the door plane to the back wall", ["SM_K_Fridge"], (3700, 3110, 1300), (3700, 4660, 1300), "clear"),
    ("fridge: shelf 1 is there (ray down hits it)", ["SM_K_Fridge"], (3700, 4000, 1300), (3700, 4000, 700), "hit"),
    ("fridge: freezer is hollow", ["SM_K_Fridge"], (3700, 3110, 3000), (3700, 4660, 3000), "clear"),
    ("toaster: slot is a real cavity", ["SM_K_Toaster"], (-6250, 4085, 2300), (-6250, 4085, 1820), "clear"),
    ("cereal box: tunnel from the open end to the closed end", ["SM_K_CerealBox"], (-4500, 900, 200), (-5760, 900, 200), "clear"),
    ("cereal box: the far end is closed", ["SM_K_CerealBox"], (-5700, 900, 200), (-5900, 900, 200), "hit"),
    ("bin: hollow down to its floor", ["SM_K_Bin"], (6000, -1500, 1700), (6000, -1500, 40), "clear"),
    ("bin: has a floor", ["SM_K_Bin"], (6000, -1500, 200), (6000, -1500, -50), "hit"),
    ("bucket: hollow", ["SM_K_Bucket"], (6200, 3900, 800), (6200, 3900, 40), "clear"),
    ("microwave: cavity open from the front", ["SM_K_Microwave"], (1760, 3900, 2000), (1760, 4850, 2000), "clear"),
    ("dishwasher: cavity open from the front", ["SM_K_Dishwasher"], (650, 2900, 800), (650, 4690, 800), "clear"),
    ("window: the opening is free (no glass)", ["SM_K_Window"], (700, -4700, 3600), (700, -5300, 3600), "clear"),
    ("vent grille: a ray between two slats passes", ["SM_K_VentGrille"], (-3100, -4900, 176), (-3100, -5100, 176), "clear"),
    ("vent grille: a slat is there", ["SM_K_VentGrille"], (-3100, -4900, 136), (-3100, -5100, 136), "hit"),
    ("sink: basin is open down to its floor (z 1150)", ["SM_K_Sink"], (-3300, 3600, 1800), (-3300, 3600, 1160), "clear"),
    ("sink: basin floor", ["SM_K_Sink"], (-3300, 3600, 1800), (-3300, 3600, 1050), "hit"),
    ("worktop: sink cut-out is open", ["SM_K_WorktopA"], (-3500, 4000, 1800), (-3500, 4000, 1500), "clear"),
    ("worktop: solid next to the cut-out", ["SM_K_WorktopA"], (-5000, 4000, 1800), (-5000, 4000, 1500), "hit"),
    ("wall cabinet: hollow behind the doors", ["SM_K_WallCab1500"], (1400, 4350, 3200), (1400, 4960, 3200), "clear"),
    ("hood: hollow underside", ["SM_K_Hood"], (-750, 4600, 3200), (-750, 4600, 3340), "clear"),
    ("matchbox: sleeve is a tunnel", ["SM_K_Matchbox"], (-1600, 2600, 80), (-2300, 2600, 80), "clear"),
    ("shoe: hollow at the heel", ["SM_K_Shoe"], (6550, -3900, 600), (6550, -3900, 120), "clear"),
    ("pet bowl: hollow", ["SM_K_PetBowl"], (-3800, -3800, 300), (-3800, -3800, 70), "clear"),
    ("mug: hollow", ["SM_K_Mug"], (-2200, 4200, 2100), (-2200, 4200, 1760), "clear"),
    ("pot: hollow", ["SM_K_Pot"], (-350, 3500, 2400), (-350, 3500, 1820), "clear"),
    ("crisper drawer: open-top tray", ["SM_K_Fridge_Crisper"], (3700, 4000, 880), (3700, 4000, 260), "clear"),
]
bad = 0
for what, names, A, B, want in CHECKS:
    h = seg(names, A, B)
    ok = (h is None) == (want == "clear")
    bad += 0 if ok else 1
    print("CHECK %s | %s | want %s, got %s" % ("PASS" if ok else "FAIL", what, want, "clear" if h is None else "hit at %s" % h))
print("CHECKS_DONE %d checks, %d failed" % (len(CHECKS), bad))