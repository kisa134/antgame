# Map v4: cereal box (generated front art), candle, pea, rice grain.
#   blender --background --factory-startup --python blender_more.py -- <out dir> <label dir>
import bpy, bmesh, sys, os, math, random, json
import numpy as np
from mathutils import Vector, Matrix, noise
argv = sys.argv[sys.argv.index("--") + 1:]
OUT, LAB = argv[0], argv[1]
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "blender_m4_common.py")).read())

def load_rgb(path, w, h):
    img = bpy.data.images.load(path); img.scale(w, h)
    a = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1, :, :3]
    bpy.data.images.remove(img); return a

def save_png(arr, path):
    h, w = arr.shape[:2]
    img = bpy.data.images.new(os.path.basename(path), w, h, alpha=False)
    rgba = np.ones((h, w, 4), np.float32); rgba[..., :3] = arr[..., :3]
    img.pixels = rgba[::-1].ravel(); img.filepath_raw = path; img.file_format = "PNG"; img.save()

# ---- cereal box: unit cube frame (the builder's box lies on its back: front art on +Z, X the long side)
clear()
bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
bmesh.ops.bevel(bm, geom=list(bm.edges), offset=0.012, segments=2, affect="EDGES")
uvl = bm.loops.layers.uv.new()
for f in bm.faces:
    n = f.normal
    for l in f.loops:
        x, y, z = l.vert.co
        if n.z > 0.7: l[uvl].uv = (0.5 + x, 0.5 + (0.5 + y) * 0.5)          # front: top half of the atlas
        elif n.z < -0.7: l[uvl].uv = (0.5 + x, (0.5 + y) * 0.25)          # back: a strip of the same art, darker
        else: l[uvl].uv = (0.5 + (x if abs(n.y) > 0.5 else y), 0.25 + (0.5 + z) * 0.25)
ob = obj(bm, "SM_M4_CerealBox")
W = 1024
art = load_rgb(os.path.join(LAB, "label_cereal.png"), W, W // 2)
C = np.zeros((W, W, 3), np.float32)
C[: W // 2] = art
side = art[int(W * 0.02): int(W * 0.02) + W // 4]
C[W // 2: W // 2 + W // 4] = side
C[W // 2 + W // 4:] = art[::-1][: W // 4] * 0.85
R = np.zeros((W, W, 3), np.float32); R[..., 0] = 1; R[..., 1] = 0.6
d = os.path.join(OUT, "SM_M4_CerealBox"); os.makedirs(d, exist_ok=True)
save_png(C, os.path.join(d, "T_SM_M4_CerealBox_C.png")); save_png(R, os.path.join(d, "T_SM_M4_CerealBox_R.png"))
N = np.zeros_like(C); N[..., 0] = 0.5; N[..., 1] = 0.5; N[..., 2] = 1; save_png(N, os.path.join(d, "T_SM_M4_CerealBox_N.png"))
for p in ob.data.polygons: p.use_smooth = False
export(ob, "SM_M4_CerealBox", ucx=[Vector((x * 0.5, y * 0.5, z * 0.5)) for x in (-1, 1) for y in (-1, 1) for z in (-1, 1)], smooth=False)
# (export() wrote slots=False json: textured prop -> MI with these textures)

# ---- candle: unit cylinder frame, drips at the top, a wick
clear()
bm = lathe_z([(0.0, -0.5), (0.5, -0.5), (0.5, 0.38), (0.46, 0.45), (0.3, 0.42), (0.06, 0.45), (0.0, 0.45)], segs=32)
for v in bm.verts:
    if v.co.z > 0.2 and v.co.xy.length > 0.45:
        a = math.atan2(v.co.y, v.co.x)
        v.co.x *= 1.0 + 0.05 * max(0.0, math.sin(a * 5)); v.co.y *= 1.0 + 0.05 * max(0.0, math.sin(a * 5))
w = bmesh.ops.create_cone(bm, cap_ends=True, segments=6, radius1=0.04, radius2=0.03, depth=0.12)
for v in w["verts"]: v.co.z += 0.5
ob = obj(bm, "SM_M4_Candle"); uv(ob)
export(ob, "SM_M4_Candle", slots=[("Plinth", lambda c, p: c.z > 0.44 and Vector((c.x, c.y)).length < 0.05), ("Bone", None)])

# ---- pea (sphere frame): a pea with the seam where the pod held it
clear()
bm = bmesh.new(); bmesh.ops.create_icosphere(bm, subdivisions=3, radius=0.5, calc_uvs=True)
for v in bm.verts:
    v.co *= 1.0 + 0.03 * noise.noise(v.co * 3.0) - 0.06 * math.exp(-((v.co.x) / 0.06) ** 2) * (v.co.z > 0.3)
ob = obj(bm, "SM_M4_Pea"); export(ob, "SM_M4_Pea")
# ---- rice grain (sphere frame, long along X)
clear()
bm = bmesh.new(); bmesh.ops.create_icosphere(bm, subdivisions=3, radius=0.5, calc_uvs=True)
for v in bm.verts:
    t = abs(v.co.x) / 0.5
    k = 1.0 - 0.25 * t ** 3
    v.co.y *= k; v.co.z *= k
    v.co.x += 0.03 * (1.0 - t) * (1 if v.co.y > 0 else -1)
ob = obj(bm, "SM_M4_Rice"); export(ob, "SM_M4_Rice")
print("M4MORE DONE")
