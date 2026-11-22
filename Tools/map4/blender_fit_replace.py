# Map v4: a generated model (GLB) made into a drop-in replacement of a level piece: same frame, pivot and footprint as the
# original Blender piece (SourceArt/Blender/<set>/<Name>.fbx), so the art table, movers, collision and wreck keep working.
#   blender --background --factory-startup --python blender_fit_replace.py -- <in.glb> <original.fbx> <out dir> <NewName> <max tris> [tex px] [yaw]
# Fit: rotated (yaw, or by the footprint's aspect), scaled per axis to the original's box but never distorted more than
# 20 % from a uniform scale, centred on the original's footprint, standing on its bottom. One material slot, textures in
# the game's C / N / R format. Writes <NewName>.json.
import bpy, bmesh, sys, os, json, math
import numpy as np
from mathutils import Vector, Matrix
argv = sys.argv[sys.argv.index("--") + 1:]
src, orig, out, name, max_tris = argv[0], argv[1], argv[2], argv[3], int(argv[4])
tex_px = int(argv[5]) if len(argv) > 5 else 1024
yaw_arg = argv[6] if len(argv) > 6 else "auto"
cut_top = float(argv[7]) if len(argv) > 7 else 0.0
fit_mode = argv[8] if len(argv) > 8 else "box"   # box | z (uniform, by height: tall thin tools) | axis (tilted in its file: match the long axis)   # drop this fraction of the model's height from the top (a lid that is its own piece in the game)
os.makedirs(out, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)

def bbox(objs):
    pts = [o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
    mn = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    mx = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return mn, mx

bpy.ops.import_scene.fbx(filepath=orig)
ometa = [o for o in bpy.context.scene.objects if o.type == "MESH"]
omn, omx = bbox(ometa)
opts = np.array([(o.matrix_world @ v.co)[:] for o in ometa for v in o.data.vertices])
for o in list(bpy.context.scene.objects):
    bpy.data.objects.remove(o, do_unlink=True)

bpy.ops.import_scene.gltf(filepath=src)
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
if cut_top > 0.0:
    zs = [v.co.z for v in me.vertices]
    zc = max(zs) - (max(zs) - min(zs)) * cut_top
    bmc = bmesh.new(); bmc.from_mesh(me)
    bmesh.ops.bisect_plane(bmc, geom=bmc.verts[:] + bmc.edges[:] + bmc.faces[:], plane_co=(0, 0, zc), plane_no=(0, 0, 1), clear_outer=True)
    bmc.to_mesh(me); bmc.free()
co = np.array([v.co[:] for v in me.vertices])
osz = np.array(omx - omn)
best, byaw = 1e9, 0
for yaw in ([0, 90, 180, 270] if yaw_arg == "auto" else [float(yaw_arg)]):
    R = np.array(Matrix.Rotation(math.radians(yaw), 3, "Z"))
    c2 = co @ R.T
    sz = c2.max(0) - c2.min(0)
    err = abs(math.log((sz[0] / max(sz[1], 1e-6)) / (osz[0] / max(osz[1], 1e-6))))
    if err < best - 1e-3: best, byaw = err, yaw
R = Matrix.Rotation(math.radians(byaw), 3, "Z")
for v in me.vertices: v.co = R @ v.co
co = np.array([v.co[:] for v in me.vertices])
mn, mx = co.min(0), co.max(0)
sz = mx - mn
k = osz / np.maximum(sz, 1e-6)
u = float(np.exp(np.log(k).mean()))
k = np.clip(k, u / 1.2, u * 1.2)
ctr_new = np.array([(mn[0] + mx[0]) / 2, (mn[1] + mx[1]) / 2, mn[2]])
ctr_old = np.array([(omn.x + omx.x) / 2, (omn.y + omx.y) / 2, omn.z])
if fit_mode == "z":
    k = np.full(3, osz[2] / max(sz[2], 1e-6))
elif fit_mode == "axis":
    # the original lies or leans in its own file: turn the model's long axis onto the original's, scale by length
    def pca(P):
        c = P.mean(0); w, V = np.linalg.eigh(np.cov((P - c).T)); a = V[:, np.argmax(w)]
        return c, a / np.linalg.norm(a)
    oc, oa = pca(opts)
    gc, ga = pca(co)
    if np.dot(ga, [0, 0, 1]) < 0: ga = -ga
    # the original's handle end is its higher end
    if np.dot(oa, [0, 0, 1]) < 0: oa = -oa
    Rm = Vector(ga.tolist()).rotation_difference(Vector(oa.tolist())).to_matrix()
    for v in me.vertices: v.co = Rm @ (v.co - Vector(gc.tolist()))
    co = np.array([v.co[:] for v in me.vertices])
    olen = np.ptp((opts - oc) @ oa); glen = np.ptp(co @ oa)
    sc = olen / max(glen, 1e-6)
    ext_o = ((opts - oc) @ oa).min(); ext_g = (co @ oa).min() * sc
    shift = oc + oa * (ext_o - ext_g)
    k = np.full(3, sc); ctr_new = np.zeros(3); ctr_old = shift
for v in me.vertices:
    p = (np.array(v.co[:]) - ctr_new) * k + ctr_old
    v.co = Vector(p.tolist())
me.update()
bm = bmesh.new(); bm.from_mesh(me)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
bm.to_mesh(me); bm.free()
tris = sum(len(p.vertices) - 2 for p in me.polygons)
if tris > max_tris:
    d = ob.modifiers.new("dec", "DECIMATE"); d.ratio = max_tris / tris; d.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier="dec")
tris = sum(len(p.vertices) - 2 for p in me.polygons)
for p in me.polygons: p.use_smooth = True

def node_image(mat, socket_name):
    bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
    if not bsdf: return None
    s = bsdf.inputs.get(socket_name)
    if not s or not s.links: return None
    n = s.links[0].from_node
    for _ in range(4):
        if n.type == "TEX_IMAGE": return n.image
        ins = [i for i in n.inputs if i.links]
        if not ins: return None
        n = ins[-1].links[0].from_node if n.type == "NORMAL_MAP" else ins[0].links[0].from_node
    return None
def pixels(img, px):
    if img is None: return None
    im = img.copy()
    if im.size[0] != px: im.scale(px, px)
    a = np.array(im.pixels[:], dtype=np.float32).reshape(px, px, 4)[::-1]
    bpy.data.images.remove(im); return a
def save(arr, path):
    h, w = arr.shape[:2]
    img = bpy.data.images.new(os.path.basename(path), w, h, alpha=False)
    rgba = np.ones((h, w, 4), np.float32); rgba[..., :3] = arr[..., :3]
    img.pixels = rgba[::-1].ravel(); img.filepath_raw = path; img.file_format = "PNG"; img.save()
mat = me.materials[0] if me.materials else None
C = pixels(node_image(mat, "Base Color"), tex_px) if mat else None
N = pixels(node_image(mat, "Normal"), tex_px) if mat else None
MR = pixels(node_image(mat, "Roughness") or node_image(mat, "Metallic"), tex_px) if mat else None
if C is None: C = np.full((tex_px, tex_px, 4), 0.8, np.float32)
if N is None:
    N = np.zeros((tex_px, tex_px, 4), np.float32); N[..., 0] = 0.5; N[..., 1] = 0.5; N[..., 2] = 1.0
else:
    N[..., 1] = 1.0 - N[..., 1]
Rm = np.zeros((tex_px, tex_px, 4), np.float32); Rm[..., 0] = 1.0
if MR is not None: Rm[..., 1] = MR[..., 1]; Rm[..., 2] = MR[..., 2]
else: Rm[..., 1] = 0.6
save(C, os.path.join(out, "T_%s_C.png" % name)); save(N, os.path.join(out, "T_%s_N.png" % name)); save(Rm, os.path.join(out, "T_%s_R.png" % name))
me.materials.clear(); me.materials.append(bpy.data.materials.new("M4Prop"))
for p in me.polygons: p.material_index = 0
ob.name = name
for o in bpy.context.scene.objects: o.select_set(o == ob)
bpy.ops.export_scene.fbx(filepath=os.path.join(out, name + ".fbx"), use_selection=True, object_types={"MESH"}, use_mesh_modifiers=True,
                         mesh_smooth_type="FACE", bake_anim=False, add_leaf_bones=False)
json.dump({"name": name, "replaces": os.path.splitext(os.path.basename(orig))[0], "yaw": byaw, "tris": tris, "nanite": False, "complex_simple": True,
           "orig_box": [list(omn), list(omx)], "scale_k": k.tolist()}, open(os.path.join(out, name + ".json"), "w"))
print("M4FIT", name, "yaw", byaw, "tris", tris, "k", k)
