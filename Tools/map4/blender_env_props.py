# Map v4: drop-in replacements for the code-built primitive parts. Each mesh lives in the frame of the engine shape it
# replaces (unit sphere d = 1 m centred, unit cylinder r 0.5 h 1 along Z centred, unit cube centred), so the code only
# swaps the mesh and keeps every transform. Weapons and props are at real size (metres; the code scales them).
#   blender --background --factory-startup --python blender_env_props.py -- <out dir>
import bpy, bmesh, sys, os, math, random, json
import mathutils
from mathutils import Vector, Matrix
from mathutils import noise
OUT = sys.argv[sys.argv.index("--") + 1]

def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)

def obj(bm, name):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(o); return o

def uv_project(ob, kind="cube"):
    bpy.context.view_layer.objects.active = ob
    for o in bpy.context.scene.objects: o.select_set(o == ob)
    bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="SELECT")
    if kind == "cube": bpy.ops.uv.cube_project(cube_size=1.0)
    else: bpy.ops.uv.smart_project(angle_limit=1.15)
    bpy.ops.object.mode_set(mode="OBJECT")

def export(ob, name, slots=None, ucx_points=None, smooth=True, extra=None, generic=False):
    """slots: list of (slot name, face predicate) ; None = one slot 'M4Shape' painted by code"""
    d = os.path.join(OUT, name); os.makedirs(d, exist_ok=True)
    me = ob.data
    me.materials.clear()
    names = [s[0] for s in slots] if slots else ["M4Shape"]
    for n in names:
        me.materials.append(bpy.data.materials.get(n) or bpy.data.materials.new(n))
    if slots:
        for p in me.polygons:
            c = sum((me.vertices[i].co for i in p.vertices), Vector()) / len(p.vertices)
            for k, (n, pred) in enumerate(slots):
                if pred is None or pred(c, p):
                    p.material_index = k
                    break
    for p in me.polygons: p.use_smooth = smooth
    objs = [ob]
    if ucx_points:
        bm = bmesh.new()
        for q in ucx_points: bm.verts.new(q)
        bmesh.ops.convex_hull(bm, input=bm.verts)
        u = obj(bm, "UCX_%s_00" % name); objs.append(u)
    for o in bpy.context.scene.objects: o.select_set(o in objs)
    bpy.context.view_layer.objects.active = ob
    ob.name = name
    bpy.ops.export_scene.fbx(filepath=os.path.join(d, name + ".fbx"), use_selection=True, object_types={"MESH"}, use_mesh_modifiers=True,
                             mesh_smooth_type="FACE", bake_anim=False, add_leaf_bones=False)
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    info = {"name": name, "tris": tris, "nanite": False, "slots": bool(slots), "generic": generic}
    if extra: info.update(extra)
    json.dump(info, open(os.path.join(d, name + ".json"), "w"))
    print("M4PROP", name, tris)

def double_side(bm, faces=None):
    faces = list(faces if faces is not None else bm.faces)
    d = bmesh.ops.duplicate(bm, geom=faces)
    nf = [g for g in d["geom"] if isinstance(g, bmesh.types.BMFace)]
    bmesh.ops.reverse_faces(bm, faces=nf)

def sphere(sub=3, r=0.5):
    bm = bmesh.new(); bmesh.ops.create_icosphere(bm, subdivisions=sub, radius=r, calc_uvs=True); return bm

def hull_sphere(scale=(1, 1, 1)):
    return [Vector((math.cos(a) * math.sin(b) * 0.5 * scale[0], math.sin(a) * math.sin(b) * 0.5 * scale[1], math.cos(b) * 0.5 * scale[2]))
            for a in [i * math.pi / 5 for i in range(10)] for b in [j * math.pi / 5 for j in range(6)]]

# ------------------------------------------------------------------ FX
clear(); random.seed(1)
bm = sphere(3)
for v in bm.verts:
    v.co *= 1.0 + noise.noise(v.co * 2.4 + Vector((5, 1, 3))) * 0.28 + noise.noise(v.co * 6.0) * 0.06
ob = obj(bm, "SM_M4_Puff"); export(ob, "SM_M4_Puff")

# leg pair of a trail worker: a bar across the body (Y), knees up, tips bent down: the unit cube's frame
clear()
bm = bmesh.new()
prof = [(-0.5, -0.42), (-0.36, 0.1), (-0.18, 0.38), (0.0, 0.3), (0.18, 0.38), (0.36, 0.1), (0.5, -0.42)]
w = 0.32
rows = []
for (y, z) in prof:
    t = 1.0 - abs(y) * 0.9
    rows.append([bm.verts.new((sx * w * t, y, z + sz * 0.12)) for (sx, sz) in ((-1, -1), (1, -1), (1, 1), (-1, 1))])
for k in range(len(rows) - 1):
    for i in range(4):
        bm.faces.new((rows[k][i], rows[k][(i + 1) % 4], rows[k + 1][(i + 1) % 4], rows[k + 1][i]))
bm.faces.new(rows[0][::-1]); bm.faces.new(rows[-1])
ob = obj(bm, "SM_M4_LegPair"); uv_project(ob); export(ob, "SM_M4_LegPair", smooth=False)

# ------------------------------------------------------------------ builder blobs
# trash bag: wrinkled, a twisted neck on top (+Z)
clear(); random.seed(3)
bm = sphere(4)
for v in bm.verts:
    n = noise.noise(Vector((v.co.x * 3.0, v.co.y * 3.0, v.co.z * 9.0))) * 0.05 + noise.noise(v.co * 7.0) * 0.02
    v.co *= 1.0 + n
    v.co.z *= 0.92
    if v.co.z < -0.38: v.co.z = -0.38 - (v.co.z + 0.38) * 0.2      # sits flat
neck = bmesh.ops.create_cone(bm, cap_ends=True, segments=10, radius1=0.09, radius2=0.03, depth=0.18)
for v in neck["verts"]:
    a = (v.co.z + 0.09) * 6.0
    x, y = v.co.x, v.co.y
    v.co = Vector((x * math.cos(a) - y * math.sin(a), x * math.sin(a) + y * math.cos(a), v.co.z + 0.5))
ears = bmesh.ops.create_cone(bm, cap_ends=True, segments=6, radius1=0.05, radius2=0.005, depth=0.16)
for v in ears["verts"]:
    v.co = Matrix.Rotation(0.9, 3, "Y") @ v.co + Vector((0.07, 0, 0.62))
ob = obj(bm, "SM_M4_TrashBag"); uv_project(ob, "smart")
export(ob, "SM_M4_TrashBag", ucx_points=hull_sphere((1, 1, 0.9)))

# dust bunny: a fluffy tangle (fur shell + loose hairs)
clear(); random.seed(4)
bm = sphere(3)
for v in bm.verts:
    v.co *= 0.82 + noise.noise(v.co * 4.0) * 0.18 + random.random() * 0.08
    v.co.z *= 0.75
hairs = []
for i in range(70):
    d = Vector((random.uniform(-1, 1), random.uniform(-1, 1), random.uniform(-0.6, 0.8))).normalized()
    p = d * 0.38
    q = p + (d + Vector((random.uniform(-0.6, 0.6), random.uniform(-0.6, 0.6), random.uniform(-0.3, 0.3)))).normalized() * random.uniform(0.15, 0.32)
    s = (q - p).cross(Vector((0, 0, 1))).normalized() * 0.008
    a, b, c, e = bm.verts.new(p - s), bm.verts.new(p + s), bm.verts.new(q + s * 0.3), bm.verts.new(q - s * 0.3)
    f = bm.faces.new((a, b, c, e)); double_side(bm, [f])
ob = obj(bm, "SM_M4_DustBunny"); uv_project(ob, "smart"); export(ob, "SM_M4_DustBunny", extra={"two_sided": True})

# thread / silk: an open 6-sided unit cylinder
clear()
bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=False, segments=6, radius1=0.5, radius2=0.5, depth=1.0, calc_uvs=True)
ob = obj(bm, "SM_M4_Thread"); export(ob, "SM_M4_Thread")

# ------------------------------------------------------------------ insects (unit frames)
def chitin_sphere(name, bands=0, groove=0.0, bristle=0.0, flat=1.0, seed=0, sub=4):
    clear(); random.seed(seed)
    bm = sphere(sub)
    for v in bm.verts:
        k = 1.0
        if bands:
            t = (v.co.x + 0.5) * bands
            k -= groove * (1.0 - abs(math.cos(t * math.pi))) ** 6
        if bristle:
            k += noise.noise(v.co * 40.0) * bristle
        v.co *= k
        v.co.z *= flat
    ob = obj(bm, name); uv_project(ob, "smart"); export(ob, name, ucx_points=hull_sphere())
chitin_sphere("SM_M4_InsectThorax", bands=2, groove=0.06, bristle=0.012, seed=5)
chitin_sphere("SM_M4_InsectAbdomen", bands=5, groove=0.07, bristle=0.006, seed=6)
chitin_sphere("SM_M4_InsectHead", bands=0, bristle=0.01, seed=7)
# compound eye: faceted (each face pushed out a little)
clear()
bm = sphere(3)
bmesh.ops.inset_individual(bm, faces=list(bm.faces), thickness=0.008, depth=0.012)
ob = obj(bm, "SM_M4_CompoundEye"); export(ob, "SM_M4_CompoundEye", smooth=False)
# wing: flat, veined, in the unit sphere's XY (the code squashes Z)
clear()
bm = bmesh.new()
outline = []
N = 40
for i in range(N):
    a = 2 * math.pi * i / N
    r = 0.5 * (1.0 - 0.18 * math.cos(a) ** 2) * (0.92 + 0.08 * math.cos(3 * a))
    outline.append(bm.verts.new((math.cos(a) * r, math.sin(a) * r * 0.9, 0.0)))
c = bm.verts.new((0, 0, 0.0))
for i in range(N):
    bm.faces.new((c, outline[i], outline[(i + 1) % N]))
for k in range(5):        # veins: thin raised ridges from the root
    a0 = math.pi * (0.75 + 0.12 * k)
    p0 = Vector((math.cos(a0) * 0.45, math.sin(a0) * 0.4, 0)) * -1
    p1 = Vector((math.cos(a0) * 0.45, math.sin(a0) * 0.4, 0))
    s = Vector((-(p1 - p0).y, (p1 - p0).x, 0)).normalized() * 0.006
    vs = [bm.verts.new(p0 - s + Vector((0, 0, 0.02))), bm.verts.new(p0 + s + Vector((0, 0, 0.02))), bm.verts.new(p1 + s + Vector((0, 0, 0.02))), bm.verts.new(p1 - s + Vector((0, 0, 0.02)))]
    bm.faces.new(vs)
double_side(bm)
ob = obj(bm, "SM_M4_Wing"); uv_project(ob, "smart"); export(ob, "SM_M4_Wing", extra={"two_sided": True})
# leg segment: tapered, a knee bulge at the top, spines (unit cylinder frame)
clear(); random.seed(8)
bm = bmesh.new()
S = 10
rings = []
for k in range(9):
    z = -0.5 + k / 8.0
    r = 0.5 * (0.7 + 0.3 * (k / 8.0)) * (1.12 if k >= 7 else 1.0)
    rings.append([bm.verts.new((math.cos(2 * math.pi * i / S) * r, math.sin(2 * math.pi * i / S) * r, z)) for i in range(S)])
for k in range(8):
    for i in range(S):
        bm.faces.new((rings[k][i], rings[k][(i + 1) % S], rings[k + 1][(i + 1) % S], rings[k + 1][i]))
bm.faces.new(rings[0][::-1]); bm.faces.new(rings[-1])
for i in range(10):       # spines
    z = random.uniform(-0.4, 0.3); a = random.uniform(0, 2 * math.pi)
    base = Vector((math.cos(a) * 0.4, math.sin(a) * 0.4, z)); tip = base * 1.0 + Vector((math.cos(a) * 0.5, math.sin(a) * 0.5, 0.25))
    sd = Vector((-math.sin(a), math.cos(a), 0)) * 0.05
    bm.faces.new((bm.verts.new(base - sd), bm.verts.new(base + sd), bm.verts.new(tip)))
ob = obj(bm, "SM_M4_LegSeg"); uv_project(ob, "smart")
export(ob, "SM_M4_LegSeg", ucx_points=[Vector((math.cos(2 * math.pi * i / 8) * 0.5, math.sin(2 * math.pi * i / 8) * 0.5, z)) for i in range(8) for z in (-0.5, 0.5)])
# fang: curved, in the unit cone's frame (base at z -0.5, tip at +0.5)
clear()
bm = bmesh.new()
rings = []
for k in range(8):
    t = k / 7.0
    r = 0.5 * (1.0 - t) ** 0.8 + 0.01
    off = Vector((0.25 * t * t, 0, 0))
    rings.append([bm.verts.new(off + Vector((math.cos(2 * math.pi * i / 10) * r, math.sin(2 * math.pi * i / 10) * r, -0.5 + t))) for i in range(10)])
for k in range(7):
    for i in range(10):
        bm.faces.new((rings[k][i], rings[k][(i + 1) % 10], rings[k + 1][(i + 1) % 10], rings[k + 1][i]))
bm.faces.new(rings[0][::-1]); bm.faces.new(rings[-1])
ob = obj(bm, "SM_M4_Fang"); uv_project(ob); export(ob, "SM_M4_Fang")

# ------------------------------------------------------------------ weapons (real size, along +X from the grip)
def lathe(profile, segs=32, axis="X"):
    bm = bmesh.new()
    rings = []
    for (x, r) in profile:
        rings.append([bm.verts.new((x, math.cos(2 * math.pi * i / segs) * r, math.sin(2 * math.pi * i / segs) * r)) for i in range(segs)])
    for k in range(len(rings) - 1):
        for i in range(segs):
            bm.faces.new((rings[k][i], rings[k + 1][i], rings[k + 1][(i + 1) % segs], rings[k][(i + 1) % segs]))
    c0 = bm.verts.new((profile[0][0], 0, 0)); c1 = bm.verts.new((profile[-1][0], 0, 0))
    for i in range(segs):
        bm.faces.new((c0, rings[0][(i + 1) % segs], rings[0][i]))
        bm.faces.new((c1, rings[-1][i], rings[-1][(i + 1) % segs]))
    return bm
clear()
bat = [(-0.02, 0.0), (-0.015, 0.024), (0.0, 0.026), (0.008, 0.014), (0.03, 0.0125), (0.2, 0.0135), (0.36, 0.017), (0.5, 0.025), (0.62, 0.031), (0.78, 0.0335), (0.83, 0.033), (0.845, 0.028), (0.85, 0.0)]
bm = lathe(bat)
ob = obj(bm, "SM_M4_Bat"); uv_project(ob, "smart")
export(ob, "SM_M4_Bat", slots=[("Rubber", lambda c, p: c.x < 0.2), ("Butcher", None)])
clear()
bm = lathe([(-0.12, 0.0), (-0.12, 0.11), (-0.115, 0.125), (0.0, 0.135), (0.012, 0.137), (0.014, 0.128), (0.006, 0.12), (-0.105, 0.11), (-0.11, 0.0)], segs=48)
for v in bm.verts:
    v.co = Vector((v.co.y, v.co.z, v.co.x))      # the pan's axis up (Z): a disc in XY
hdl = bmesh.ops.create_cube(bm, size=1.0)
for v in hdl["verts"]:
    v.co = Vector((v.co.x * 0.2 + 0.23, v.co.y * 0.026, v.co.z * 0.018 + 0.015))
ob = obj(bm, "SM_M4_Pan"); uv_project(ob, "smart")
export(ob, "SM_M4_Pan", slots=[("Plinth", lambda c, p: c.x > 0.135), ("Iron", None)])
clear()
prof = [(0.0, 0.0)] + [(0.004 + 0.236 * t, 0.0125 * math.sin(math.pi * min(1.0, 0.08 + t * 1.0)) ** 0.25 + 0.0005) for t in [i / 20 for i in range(21)]] + [(0.244, 0.0)]
bm = lathe(prof, segs=20)
for v in bm.verts:
    b = (v.co.x - 0.12) * 1.6
    v.co.z += 0.04 * (1 - b * b)
ob = obj(bm, "SM_M4_Sausage"); uv_project(ob, "smart")
export(ob, "SM_M4_Sausage", slots=[("Sausage", None)])

# ------------------------------------------------------------------ small props on the table / floor
clear()
bm = lathe([(0.0, 0.0), (0.0, 0.0145), (0.002, 0.0152), (0.009, 0.0152), (0.0105, 0.014), (0.0105, 0.0)], segs=21)
for v in bm.verts: v.co = Vector((v.co.y, v.co.z, v.co.x))
for v in bm.verts:     # crown teeth
    if v.co.z > 0.001 and v.co.z < 0.0095:
        a = math.atan2(v.co.y, v.co.x)
        k = 1.0 + 0.04 * math.cos(a * 21)
        v.co.x *= k; v.co.y *= k
ob = obj(bm, "SM_M4_BottleCap"); uv_project(ob, "smart"); export(ob, "SM_M4_BottleCap", slots=[("Red", None)])
clear()
bm = lathe([(0.0, 0.0), (0.0, 0.0115), (0.0006, 0.0123), (0.0018, 0.0123), (0.0021, 0.0115), (0.0021, 0.0)], segs=40)
for v in bm.verts: v.co = Vector((v.co.y, v.co.z, v.co.x))
ob = obj(bm, "SM_M4_Coin"); uv_project(ob, "smart"); export(ob, "SM_M4_Coin", slots=[("Brass", None)])
clear()
bm = lathe([(0.0, 0.0), (0.0, 0.0085), (0.018, 0.0075), (0.022, 0.006), (0.0245, 0.0035), (0.0255, 0.0)], segs=28)
for v in bm.verts: v.co = Vector((v.co.y, v.co.z, v.co.x))
bmesh.ops.inset_individual(bm, faces=[f for f in bm.faces if f.calc_center_median().z > 0.004 and f.calc_center_median().z < 0.02], thickness=0.0006, depth=-0.0004)
ob = obj(bm, "SM_M4_Thimble"); export(ob, "SM_M4_Thimble", slots=[("Chrome", None)])
clear()
bm = sphere(3, 0.011)
for v in bm.verts: v.co.z *= 1.1; v.co.z += 0.012
stem = bmesh.ops.create_cone(bm, cap_ends=True, segments=6, radius1=0.0012, radius2=0.0008, depth=0.006)
for v in stem["verts"]: v.co.z += 0.0265
ob = obj(bm, "SM_M4_Grape"); uv_project(ob, "smart")
export(ob, "SM_M4_Grape", slots=[("Bark", lambda c, p: c.z > 0.024), ("Grape", None)])
print("M4ENV DONE")
