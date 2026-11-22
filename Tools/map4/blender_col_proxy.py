# Map v4: a light collision copy of a dense generated model (UE: the mesh's Complex Collision Mesh). Ants and the
# Human collide with these triangles instead of the 8-30k drawn ones: thin multi-shell scan geometry trapped the
# Human's capsule and cost physics time. Prints how far the copy strays from the drawn surface.
#   blender --background --factory-startup --python blender_col_proxy.py -- <prop.fbx> <out _Col.fbx> <max tris>
import bpy, bmesh, sys, os
from mathutils.bvhtree import BVHTree
argv = sys.argv[sys.argv.index("--") + 1:]
src, out, max_tris = argv[0], argv[1], int(argv[2])
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH" and not o.name.startswith("UCX_")]
for o in list(bpy.context.scene.objects):
    if o not in meshes: bpy.data.objects.remove(o, do_unlink=True)
for o in bpy.context.scene.objects: o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1: bpy.ops.object.join()
ob = bpy.context.view_layer.objects.active
mw = ob.matrix_world.copy(); ob.parent = None; ob.matrix_world = mw
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
dg = bpy.context.evaluated_depsgraph_get()
bm0 = bmesh.new(); bm0.from_mesh(ob.data); bmesh.ops.triangulate(bm0, faces=bm0.faces)
orig_tree = BVHTree.FromBMesh(bm0)
tris0 = len(bm0.faces)
# weld cracks between scan shells first, so the collapse does not open holes
bm = bmesh.new(); bm.from_mesh(ob.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=max(ob.dimensions) * 0.002)
bm.to_mesh(ob.data); bm.free()
tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
if tris > max_tris:
    d = ob.modifiers.new("dec", "DECIMATE"); d.ratio = max_tris / tris; d.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier="dec")
ob.data.materials.clear()
bm = bmesh.new(); bm.from_mesh(ob.data); bmesh.ops.triangulate(bm, faces=bm.faces)
far, tot = 0.0, 0.0
for v in bm.verts:
    hit = orig_tree.find_nearest(v.co)
    dist = hit[3] if hit and hit[3] is not None else 0.0
    far = max(far, dist); tot += dist
mean = tot / max(1, len(bm.verts))
tris1 = len(bm.faces)
bm.free()
name = os.path.splitext(os.path.basename(out))[0]
ob.name = name
bpy.ops.export_scene.fbx(filepath=out, use_selection=False, object_types={"MESH"}, mesh_smooth_type="FACE", bake_anim=False, add_leaf_bones=False)
# distances in the FBX units (cm of the prop); the game multiplies by the prop's placement scale
print("M4COL %s tris %d -> %d  mean %.3f  max %.3f  (dims %s)" % (name, tris0, tris1, mean, far, tuple(round(x, 2) for x in ob.dimensions)))
