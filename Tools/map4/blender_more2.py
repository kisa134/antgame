# Map v4: a sharp-edged unit box for the shell (walls, ceilings, floors and their skins join without a bevel groove) and a
# better bin bag (deep film folds, a gathered neck, two tied ears).
#   blender --background --factory-startup --python blender_more2.py -- <out dir>
import bpy, bmesh, sys, os, math, random, json
from mathutils import Vector, Matrix, noise
argv = sys.argv[sys.argv.index("--") + 1:]
OUT = argv[0]
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "blender_m4_common.py")).read())

# ---- sharp unit box (1 m, centred), cube-projected UVs, the box itself as UCX
clear()
bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
ob = obj(bm, "SM_M4_BoxSharp"); uv(ob, "cube")
export(ob, "SM_M4_BoxSharp", ucx=[Vector((x, y, z)) * 0.5 for x in (-1, 1) for y in (-1, 1) for z in (-1, 1)], smooth=False, extra={"generic": True})

# ---- bin bag (unit sphere frame, d = 1): full bottom, film folds running up to a gathered neck, ears knotted on top
clear(); random.seed(11)
bm = bmesh.new(); bmesh.ops.create_icosphere(bm, subdivisions=5, radius=0.5)
for v in bm.verts:
    p = v.co.copy()
    h = (p.z + 0.5)                                   # 0 bottom .. 1 top
    ang = math.atan2(p.y, p.x)
    # vertical folds that tighten towards the neck, plus crumple
    fold = 0.035 * math.sin(ang * 9 + noise.noise(p * 3.0) * 3.0) * (0.3 + h) + noise.noise(Vector((p.x * 5, p.y * 5, p.z * 2))) * 0.045
    r = 1.0 + fold
    # pear shape: sag at the bottom, gathered at the top
    r *= 1.0 + 0.10 * math.exp(-((h - 0.3) / 0.25) ** 2) - 0.55 * max(0.0, h - 0.78) ** 1.3 * 3.0
    v.co = Vector((p.x * r, p.y * r, p.z))
    if v.co.z < -0.42: v.co.z = -0.42 - (v.co.z + 0.42) * 0.15     # sits on the floor
neck = bmesh.ops.create_cone(bm, cap_ends=True, segments=12, radius1=0.075, radius2=0.035, depth=0.14)
for v in neck["verts"]:
    a = (v.co.z + 0.07) * 9.0
    x, y = v.co.x, v.co.y
    w = 1.0 + 0.25 * math.sin(math.atan2(y, x) * 6)
    v.co = Vector(((x * math.cos(a) - y * math.sin(a)) * w, (x * math.sin(a) + y * math.cos(a)) * w, v.co.z + 0.5))
for side in (-1, 1):
    ear = bmesh.ops.create_cone(bm, cap_ends=True, segments=8, radius1=0.06, radius2=0.012, depth=0.2)
    for v in ear["verts"]:
        v.co.x *= 1.0; v.co.y *= 0.35
        v.co = Matrix.Rotation(side * 1.05, 3, "Y") @ v.co + Vector((side * 0.1, 0.0, 0.62))
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
ob = obj(bm, "SM_M4_TrashBag"); uv(ob, "smart")
export(ob, "SM_M4_TrashBag", ucx=[Vector((math.cos(a) * 0.5 * s, math.sin(a) * 0.5 * s, z)) for a in [i * math.pi / 6 for i in range(12)] for (s, z) in ((0.9, -0.43), (1.05, -0.1), (0.85, 0.3), (0.2, 0.6))])
print("M4MORE2 DONE")
