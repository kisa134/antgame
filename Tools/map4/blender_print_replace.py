# Map v4: a printed level piece (kid's drawing on the fridge, framed photo, wall calendar) rebuilt as a drop-in
# replacement that shows a real picture instead of a few coloured 3D blobs: same frame, pivot and footprint as the
# original Blender piece. The "print" faces get a planar UV over the picture; the raised blobs that stood for the
# picture (and an opaque glass pane in front of a photo) are dropped; every other face keeps its colour as one flat
# cell of the same atlas.
#   blender --background --factory-startup --python blender_print_replace.py -- <orig.fbx> <out dir> <NewName> <picture.png>
#           <print slots,> <drop slots,> <front hint slots,> [print roughness]
import bpy, bmesh, sys, os, json
import numpy as np
from mathutils import Vector
argv = sys.argv[sys.argv.index("--") + 1:]
orig, out, name, pic = argv[0], argv[1], argv[2], argv[3]
prints = set(s for s in argv[4].split(",") if s)
drops = set(s for s in argv[5].split(",") if s)
hints = set(s for s in argv[6].split(",") if s)
print_rough = float(argv[7]) if len(argv) > 7 else 0.8
os.makedirs(out, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=orig)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
for o in bpy.context.scene.objects: o.select_set(o.type == "MESH")
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1: bpy.ops.object.join()
ob = bpy.context.view_layer.objects.active
mw = ob.matrix_world.copy(); ob.parent = None; ob.matrix_world = mw
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
for o in list(bpy.context.scene.objects):
    if o != ob: bpy.data.objects.remove(o, do_unlink=True)
me = ob.data
slot = [m.name if m else "-" for m in me.materials]
cols = {m.name: tuple(m.diffuse_color[:3]) for m in me.materials if m}

bm = bmesh.new(); bm.from_mesh(me)
bm.faces.ensure_lookup_table()
print("M4PRINT slots", slot, {s: sum(1 for f in bm.faces if slot[f.material_index] == s) for s in set(slot)})
pf = [f for f in bm.faces if slot[f.material_index] in prints]
hf = [f for f in bm.faces if slot[f.material_index] in hints]
area_n = np.zeros(3)
for f in pf: area_n += np.abs(np.array(f.normal[:])) * f.calc_area()
n_ax = int(np.argmax(area_n))
ax_u, ax_v = [a for a in range(3) if a != n_ax][0], 2 if n_ax != 2 else 1
if ax_u == ax_v: ax_u = [a for a in range(3) if a not in (n_ax, ax_v)][0]
pts = np.array([v.co[:] for f in pf for v in f.verts])
mn, mx = pts.min(0), pts.max(0)
pc = pts.mean(0)
front = 1.0
if hf:
    hc = np.array([f.calc_center_median()[:] for f in hf]).mean(0)
    front = 1.0 if hc[n_ax] >= pc[n_ax] else -1.0
# viewer on the front side looking back across n: does +ax_u run to the viewer's right?
right = np.cross(-front * np.eye(3)[n_ax], np.eye(3)[ax_v])
flip_u = float(np.dot(right, np.eye(3)[ax_u])) < 0
for f in [f for f in bm.faces if slot[f.material_index] in drops]:
    bm.faces.remove(f)
for v in [v for v in bm.verts if not v.link_faces]:
    bm.verts.remove(v)
bm.faces.ensure_lookup_table()

PX, PIC_W = 1024, 896
others = sorted(set(slot[f.material_index] for f in bm.faces if slot[f.material_index] not in prints))
cell = {s: i for i, s in enumerate(others)}
uvl = bm.loops.layers.uv.verify()
for f in bm.faces:
    s = slot[f.material_index]
    for l in f.loops:
        if s in prints:
            u = (l.vert.co[ax_u] - mn[ax_u]) / max(mx[ax_u] - mn[ax_u], 1e-9)
            if flip_u: u = 1.0 - u
            v = (l.vert.co[ax_v] - mn[ax_v]) / max(mx[ax_v] - mn[ax_v], 1e-9)
            l[uvl].uv = (min(max(u, 0), 1) * (PIC_W - 4) / PX + 2 / PX, min(max(v, 0), 1) * (PX - 4) / PX + 2 / PX)
        else:
            i = cell[s]
            l[uvl].uv = ((PIC_W + 64) / PX, (64 + 128 * i) / PX)
bm.to_mesh(me); bm.free()
for p in me.polygons: p.material_index = 0; p.use_smooth = False
me.materials.clear(); me.materials.append(bpy.data.materials.new("M4Prop"))

# atlas: picture on the left, one flat cell per kept slot on the right (rows bottom-up, as Blender stores pixels)
C = np.ones((PX, PX, 4), np.float32)
R = np.zeros((PX, PX, 4), np.float32); R[..., 0] = 1.0; R[..., 1] = print_rough; R[..., 3] = 1
N = np.zeros((PX, PX, 4), np.float32); N[..., 0] = 0.5; N[..., 1] = 0.5; N[..., 2] = 1.0; N[..., 3] = 1
img = bpy.data.images.load(pic); img.scale(PIC_W, PX)
C[:, :PIC_W] = np.array(img.pixels[:], np.float32).reshape(PX, PIC_W, 4)
C[:, :PIC_W, 3] = 1
METAL = ("Chrome", "Steel", "Brass", "Brushed")
ROUGH = {"Walnut": 0.45, "Oak": 0.55, "Enamel": 0.3, "Chrome": 0.15, "Steel": 0.3, "Plinth": 0.6}
for s, i in cell.items():
    c = cols.get(s, (0.8, 0.8, 0.8))
    C[128 * i:128 * (i + 1), PIC_W:, :3] = c
    R[128 * i:128 * (i + 1), PIC_W:, 1] = ROUGH.get(s, 0.7)
    R[128 * i:128 * (i + 1), PIC_W:, 2] = 1.0 if s in METAL else 0.0
def save(arr, path):
    im = bpy.data.images.new(os.path.basename(path), PX, PX, alpha=False)
    im.pixels = arr.ravel(); im.filepath_raw = path; im.file_format = "PNG"; im.save()
save(C, os.path.join(out, "T_%s_C.png" % name)); save(N, os.path.join(out, "T_%s_N.png" % name)); save(R, os.path.join(out, "T_%s_R.png" % name))
ob.name = name
for o in bpy.context.scene.objects: o.select_set(o == ob)
bpy.ops.export_scene.fbx(filepath=os.path.join(out, name + ".fbx"), use_selection=True, object_types={"MESH"},
                         mesh_smooth_type="FACE", bake_anim=False, add_leaf_bones=False)
tris = sum(len(p.vertices) - 2 for p in me.polygons)
json.dump({"name": name, "replaces": os.path.splitext(os.path.basename(orig))[0], "tris": tris, "nanite": False, "complex_simple": True},
          open(os.path.join(out, name + ".json"), "w"))
print("M4PRINT", name, "axis", n_ax, "front", front, "flip_u", flip_u, "cells", others, "tris", tris)
