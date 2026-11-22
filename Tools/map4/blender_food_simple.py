# Map v4: food / packaging props modelled in Blender with generated label art (SourceArt/Map4/higgsfield/decal/label_*):
# condensed milk can, biscuit pack, lollipop, macaroni. One material slot, one texture atlas each (the loot draws one material).
#   blender --background --factory-startup --python blender_food_simple.py -- <out dir> <label dir>
import bpy, bmesh, sys, os, math, json
import numpy as np
from mathutils import Vector
argv = sys.argv[sys.argv.index("--") + 1:]
OUT, LAB = argv[0], argv[1]

def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)

def load_rgb(path, w, h):
    img = bpy.data.images.load(path)
    img.scale(w, h)
    a = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1, :, :3]
    bpy.data.images.remove(img)
    return a

def save_png(arr, path):
    h, w = arr.shape[:2]
    img = bpy.data.images.new(os.path.basename(path), w, h, alpha=False)
    rgba = np.ones((h, w, 4), np.float32); rgba[..., :3] = arr[..., :3]
    img.pixels = rgba[::-1].ravel(); img.filepath_raw = path; img.file_format = "PNG"; img.save()

def write(ob, name, C, R, N=None, nanite=False):
    d = os.path.join(OUT, name); os.makedirs(d, exist_ok=True)
    save_png(C, os.path.join(d, "T_%s_C.png" % name))
    save_png(R, os.path.join(d, "T_%s_R.png" % name))
    if N is None:
        N = np.zeros_like(C); N[..., 0] = 0.5; N[..., 1] = 0.5; N[..., 2] = 1.0
    save_png(N, os.path.join(d, "T_%s_N.png" % name))
    me = ob.data
    me.materials.clear(); me.materials.append(bpy.data.materials.new("M4Prop"))
    for p in me.polygons: p.use_smooth = True
    for o in bpy.context.scene.objects: o.select_set(o == ob)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=os.path.join(d, name + ".fbx"), use_selection=True, object_types={"MESH"}, use_mesh_modifiers=True,
                             mesh_smooth_type="FACE", bake_anim=False, add_leaf_bones=False)
    co = np.array([v.co[:] for v in me.vertices])
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    json.dump({"name": name, "size_m": [round(float(x), 4) for x in co.max(0) - co.min(0)], "tris": tris, "nanite": nanite}, open(os.path.join(d, name + ".json"), "w"))
    print("M4PROP", name, tris)

def rough_map(w, h, ao, rough, metal):
    R = np.zeros((h, w, 3), np.float32); R[..., 0] = ao; R[..., 1] = rough; R[..., 2] = metal; return R

def obj(bm, name):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(o); return o

# ---------------- condensed milk can: 7.6 cm x 8 cm, rolled rims; atlas: label in v 0.3..1, metal below
clear()
S = 48
bm = bmesh.new(); uv = bm.loops.layers.uv.new()
r, h = 0.038, 0.08
prof = [(r * 0.96, 0.0), (r, 0.002), (r, 0.006), (r * 0.985, 0.008), (r * 0.985, h - 0.008), (r, h - 0.006), (r, h - 0.002), (r * 0.96, h)]
rings = []
for (pr, pz) in prof:
    rings.append([bm.verts.new((math.cos(2 * math.pi * i / S) * pr, math.sin(2 * math.pi * i / S) * pr, pz)) for i in range(S)])
for k in range(len(prof) - 1):
    for i in range(S):
        a, b = rings[k][i], rings[k][(i + 1) % S]; c, d = rings[k + 1][(i + 1) % S], rings[k + 1][i]
        f = bm.faces.new((a, b, c, d))
        label = k == 3
        for l in f.loops:
            ang = math.atan2(l.vert.co.y, l.vert.co.x) % (2 * math.pi)
            u = ang / (2 * math.pi)
            if l.vert in (b, c) and u < 1e-4: u = 1.0
            if label:
                l[uv].uv = (u, 0.3 + 0.7 * (l.vert.co.z - 0.008) / (h - 0.016))
            else:
                l[uv].uv = (u * 0.5, 0.05 + 0.2 * l.vert.co.z / h)
for ring, top in ((rings[0], False), (rings[-1], True)):
    c = bm.verts.new((0, 0, h - 0.001 if top else 0.001))
    for i in range(S):
        a, b = ring[i], ring[(i + 1) % S]
        f = bm.faces.new((a, b, c) if top else (b, a, c))
        for l in f.loops:
            l[uv].uv = (0.75 + l.vert.co.x / r * 0.12, 0.14 + l.vert.co.y / r * 0.12)
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
ob = obj(bm, "SM_M4_CondensedMilk")
W = 1024
C = np.zeros((W, W, 3), np.float32); C[:] = (0.72, 0.73, 0.74)
LH = W - int(W * 0.3)
lab = load_rgb(os.path.join(LAB, "label_condmilk.png"), W, LH)
C[:LH, :] = lab
# brushed tin rings on the lid area
yy, xx = np.mgrid[0:W, 0:W]
rr = np.hypot((xx - 0.75 * W), (yy - (1 - 0.14) * W))
C[(rr < 0.12 * W) & ((rr / (0.012 * W)).astype(int) % 2 == 0)] *= 0.92
R = rough_map(W, W, 1.0, 0.45, 0.0)
R[LH:, :, 1] = 0.3; R[LH:, :, 2] = 1.0
write(ob, "SM_M4_CondensedMilk", C, R)

# ---------------- biscuit pack: 15 x 6 x 4.5 cm, paper wrap with folded ends
clear()
bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
for v in bm.verts:
    v.co = Vector((v.co.x * 0.15, v.co.y * 0.06, v.co.z * 0.045 + 0.0225))
bmesh.ops.bevel(bm, geom=list(bm.edges), offset=0.002, segments=2, affect="EDGES")
uvl = bm.loops.layers.uv.new()
for f in bm.faces:
    n = f.normal
    for l in f.loops:
        x, y, z = l.vert.co
        if abs(n.z) > 0.7:     # front (top) panel = the art
            l[uvl].uv = (x / 0.15 + 0.5, (y / 0.06 + 0.5) * 0.5 + 0.5)
        elif abs(n.y) > 0.7:   # long sides: the art's lower strip
            l[uvl].uv = (x / 0.15 + 0.5, z / 0.045 * 0.25 + 0.25)
        else:                  # folded ends: plain paper
            l[uvl].uv = (0.05 + (y / 0.06 + 0.5) * 0.1, z / 0.045 * 0.2 + 0.02)
ob = obj(bm, "SM_M4_BiscuitPack")
W = 1024
C = np.zeros((W, W, 3), np.float32)
art = load_rgb(os.path.join(LAB, "label_biscuits.png"), W, W // 2)
C[: W // 2, :] = art
C[W // 2:, :] = art[::-1]
C[int(W * 0.75):, : int(W * 0.2)] = art[5:10, 5:10].mean((0, 1))
R = rough_map(W, W, 1.0, 0.75, 0.0)
write(ob, "SM_M4_BiscuitPack", C, R)

# ---------------- lollipop: 6 cm disc, 8 mm thick, on a 7 cm paper stick
clear()
bm = bmesh.new(); uvl = bm.loops.layers.uv.new()
S = 48; rd, th = 0.03, 0.008
top = [bm.verts.new((math.cos(2 * math.pi * i / S) * rd, 0.0, math.sin(2 * math.pi * i / S) * rd + 0.075)) for i in range(S)]
bot = [bm.verts.new((v.co.x, th, v.co.z)) for v in top]
cf = bm.verts.new((0, 0, 0.075)); cb = bm.verts.new((0, th, 0.075))
for i in range(S):
    a, b = top[i], top[(i + 1) % S]
    for f in (bm.faces.new((cf, a, b)), bm.faces.new((cb, bot[(i + 1) % S], bot[i])), bm.faces.new((a, bot[i], bot[(i + 1) % S], b))):
        for l in f.loops:
            l[uvl].uv = (0.5 + l.vert.co.x / rd * 0.48, 0.5 + (l.vert.co.z - 0.075) / rd * 0.48)
# stick
stick = bmesh.ops.create_cone(bm, cap_ends=True, segments=12, radius1=0.0022, radius2=0.0022, depth=0.075)
for v in stick["verts"]:
    v.co = Vector((v.co.x, v.co.y + th * 0.5, v.co.z + 0.0375))
for f in bm.faces:
    if all(l.vert in stick["verts"] for l in f.loops):
        for l in f.loops: l[uvl].uv = (0.02, 0.02)
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
ob = obj(bm, "SM_M4_Lollipop")
W = 1024
C = load_rgb(os.path.join(LAB, "label_lollipop.png"), W, W)
C[-40:, : 40] = (0.97, 0.97, 0.95)
R = rough_map(W, W, 1.0, 0.12, 0.0); R[-40:, :40, 1] = 0.8
write(ob, "SM_M4_Lollipop", C, R)

# ---------------- macaroni: elbow tube, 2.5 cm
clear()
bpy.ops.curve.primitive_bezier_circle_add(radius=0.012)
cu = bpy.context.active_object
cu.data.bevel_mode = "ROUND"; cu.data.bevel_depth = 0.0035; cu.data.bevel_resolution = 4; cu.data.resolution_u = 8
cu.data.splines[0].use_cyclic_u = False
bpy.ops.object.convert(target="MESH")
ob = bpy.context.active_object
bm = bmesh.new(); bm.from_mesh(ob.data)
keep = [v for v in bm.verts if math.atan2(v.co.y, v.co.x) > -0.2 and math.atan2(v.co.y, v.co.x) < 1.9]
bmesh.ops.delete(bm, geom=[v for v in bm.verts if v not in keep], context="VERTS")
uvl = bm.loops.layers.uv.verify()
for f in bm.faces:
    for l in f.loops:
        l[uvl].uv = (0.5 + l.vert.co.z * 20.0, 0.5)
bm.to_mesh(ob.data); bm.free()
mod = ob.modifiers.new("s", "SOLIDIFY"); mod.thickness = 0.0008
bpy.ops.object.modifier_apply(modifier="s")
W = 256
C = np.zeros((W, W, 3), np.float32); C[:] = (0.93, 0.8, 0.45)
C *= (0.94 + 0.06 * np.sin(np.arange(W) / W * 2 * math.pi * 12))[None, :, None]
R = rough_map(W, W, 1.0, 0.55, 0.0)
write(ob, "SM_M4_Macaroni", C, R)
print("M4FOOD DONE")
