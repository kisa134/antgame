# Map v4: cleans a generated GLB (Higgsfield / Meshy image-to-3D) into a game prop.
#   blender --background --factory-startup --python blender_glb_prop.py -- <in.glb> <out dir> <Name> <size m> <max tris> [tex px]
# Real-world size (largest dimension = <size m>), pivot at the bottom centre, one material slot "M4Prop", merged doubles,
# no loose parts, outward normals, decimated to the budget; textures written next to the FBX in the game's format:
#   T_<Name>_C.png colour, T_<Name>_N.png normal (DirectX), T_<Name>_R.png (R = AO, G = roughness, B = metal).
# Writes <Name>.json (size, tris). Units: metres (the game imports x100 and places at 19x like the other Blender art).
import bpy, bmesh, sys, os, json, math
import numpy as np
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
src, out, name, size_m, max_tris = argv[0], argv[1], argv[2], float(argv[3]), int(argv[4])
tex_px = int(argv[5]) if len(argv) > 5 else 1024
os.makedirs(out, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
for o in bpy.context.scene.objects:
    o.select_set(o.type == "MESH")
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
ob = bpy.context.view_layer.objects.active
# unparent, apply transforms
mw = ob.matrix_world.copy()
ob.parent = None
ob.matrix_world = mw
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
for o in list(bpy.context.scene.objects):
    if o != ob:
        bpy.data.objects.remove(o, do_unlink=True)

me = ob.data
bm = bmesh.new()
bm.from_mesh(me)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
# loose bits: drop islands with < 1% of the faces
bm.faces.ensure_lookup_table()
islands, seen = [], set()
for f in bm.faces:
    if f.index in seen:
        continue
    stack, isl = [f], []
    seen.add(f.index)
    while stack:
        g = stack.pop(); isl.append(g)
        for e in g.edges:
            for h in e.link_faces:
                if h.index not in seen:
                    seen.add(h.index); stack.append(h)
    islands.append(isl)
total = len(bm.faces)
drop = [f for isl in islands if len(isl) < total * 0.01 for f in isl]
if drop and len(drop) < total * 0.2:
    bmesh.ops.delete(bm, geom=drop, context="FACES")
loose = [v for v in bm.verts if not v.link_faces]
bmesh.ops.delete(bm, geom=loose, context="VERTS")
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
bm.to_mesh(me)
bm.free()

# size and pivot
co = np.array([v.co[:] for v in me.vertices])
mn, mx = co.min(0), co.max(0)
k = size_m / max(1e-6, (mx - mn).max())
centre = Vector(((mn[0] + mx[0]) * 0.5, (mn[1] + mx[1]) * 0.5, mn[2]))
for v in me.vertices:
    v.co = (v.co - centre) * k
me.update()

# budget
tris = sum(len(p.vertices) - 2 for p in me.polygons)
if tris > max_tris:
    d = ob.modifiers.new("dec", "DECIMATE")
    d.ratio = max_tris / tris
    d.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier="dec")
bpy.ops.object.shade_smooth() if hasattr(bpy.ops.object, "shade_smooth") else None
tris = sum(len(p.vertices) - 2 for p in me.polygons)

# textures
def node_image(mat, socket_name):
    bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
    if not bsdf:
        return None
    s = bsdf.inputs.get(socket_name)
    if not s or not s.links:
        return None
    n = s.links[0].from_node
    for _ in range(4):
        if n.type == "TEX_IMAGE":
            return n.image
        ins = [i for i in n.inputs if i.links]
        if not ins:
            return None
        n = ins[-1].links[0].from_node if n.type == "NORMAL_MAP" else ins[0].links[0].from_node
    return None

def pixels(img, px):
    if img is None:
        return None
    im = img.copy()
    if im.size[0] != px:
        im.scale(px, px)
    a = np.array(im.pixels[:], dtype=np.float32).reshape(px, px, 4)[::-1]
    bpy.data.images.remove(im)
    return a

def save(arr, path):
    h, w = arr.shape[:2]
    img = bpy.data.images.new(os.path.basename(path), w, h, alpha=False)
    rgba = np.ones((h, w, 4), np.float32)
    rgba[..., :3] = arr[..., :3]
    img.pixels = rgba[::-1].ravel()
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()

mat = me.materials[0] if me.materials else None
C = pixels(node_image(mat, "Base Color"), tex_px) if mat else None
N = pixels(node_image(mat, "Normal"), tex_px) if mat else None
MR = pixels(node_image(mat, "Roughness") or node_image(mat, "Metallic"), tex_px) if mat else None
if C is None:
    C = np.full((tex_px, tex_px, 4), 0.8, np.float32)
if N is None:
    N = np.zeros((tex_px, tex_px, 4), np.float32); N[..., 0] = 0.5; N[..., 1] = 0.5; N[..., 2] = 1.0
else:
    N[..., 1] = 1.0 - N[..., 1]     # glTF (OpenGL) -> DirectX
R = np.zeros((tex_px, tex_px, 4), np.float32)
R[..., 0] = 1.0
if MR is not None:
    R[..., 1] = MR[..., 1]; R[..., 2] = MR[..., 2]
else:
    R[..., 1] = 0.6
save(C, os.path.join(out, "T_%s_C.png" % name))
save(N, os.path.join(out, "T_%s_N.png" % name))
save(R, os.path.join(out, "T_%s_R.png" % name))

# one slot
me.materials.clear()
m = bpy.data.materials.new("M4Prop")
me.materials.append(m)
for p in me.polygons:
    p.material_index = 0

bpy.ops.export_scene.fbx(filepath=os.path.join(out, name + ".fbx"), use_selection=False, object_types={"MESH"},
                         use_mesh_modifiers=True, mesh_smooth_type="FACE", bake_anim=False, add_leaf_bones=False)
dims = [round(float(x), 5) for x in (np.array([v.co[:] for v in me.vertices]).max(0) - np.array([v.co[:] for v in me.vertices]).min(0))]
json.dump({"name": name, "size_m": dims, "tris": tris, "source": os.path.basename(src)}, open(os.path.join(out, name + ".json"), "w"))
print("M4PROP", name, "tris", tris, "dims", dims)
