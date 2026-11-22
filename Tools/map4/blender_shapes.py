# Map v4: the game's own base shapes (replace /Engine/BasicShapes everywhere): same pivots and sizes as the engine ones
# (100 uu = 1 m in Blender at FBX import), but bevelled edges that catch the light, proper UVs, weighted normals, and
# UCX convex collision so simulated pieces keep a simple body. Plus small FX / micro-detail meshes.
#   blender --background --factory-startup --python blender_shapes.py -- <out dir>
import bpy, bmesh, sys, os, math, random, json
from mathutils import Vector, Matrix
out = sys.argv[sys.argv.index("--") + 1]
os.makedirs(out, exist_ok=True)

def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)

def finish(ob, name, ucx=None, smooth=True, weighted=True):
    me = ob.data
    if smooth:
        for p in me.polygons:
            p.use_smooth = True
    if weighted:
        m = ob.modifiers.new("wn", "WEIGHTED_NORMAL"); m.keep_sharp = True
    ob.name = name
    me.materials.clear()
    mat = bpy.data.materials.new("M4Shape"); me.materials.append(mat)
    objs = [ob]
    if ucx is not None:
        ucx.name = "UCX_" + name + "_00"
        objs.append(ucx)
    for o in bpy.context.scene.objects:
        o.select_set(o in objs)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=os.path.join(out, name, name + ".fbx"), use_selection=True, object_types={"MESH"},
                             use_mesh_modifiers=True, mesh_smooth_type="FACE", bake_anim=False, add_leaf_bones=False)
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    json.dump({"name": name, "tris": tris, "nanite": False, "generic": True}, open(os.path.join(out, name, name + ".json"), "w"))
    print("M4SHAPE", name, tris)

def obj_from_bm(bm, name):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob)
    return ob

def hull(points, name):
    bm = bmesh.new()
    for p in points:
        bm.verts.new(p)
    bmesh.ops.convex_hull(bm, input=bm.verts)
    return obj_from_bm(bm, name)

def uv_box(ob):
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.cube_project(cube_size=1.0)
    bpy.ops.object.mode_set(mode="OBJECT")

for n in ["SM_M4_Box", "SM_M4_Ball", "SM_M4_Cyl", "SM_M4_Cone", "SM_M4_Plane", "SM_M4_Droplet", "SM_M4_Speck", "SM_M4_Crumb", "SM_M4_Pheromone", "SM_M4_Pebble"]:
    os.makedirs(os.path.join(out, n), exist_ok=True)

# ---- Box: 1 m cube centred, 1.5 cm bevel, 3 segments
clear()
bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
bmesh.ops.bevel(bm, geom=list(bm.edges), offset=0.015, segments=3, profile=0.5, affect="EDGES")
ob = obj_from_bm(bm, "SM_M4_Box"); uv_box(ob)
finish(ob, "SM_M4_Box", hull([Vector((x, y, z)) * 0.5 for x in (-1, 1) for y in (-1, 1) for z in (-1, 1)], "ucx"))

# ---- Ball: d = 1 m
clear()
bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=32, v_segments=16, radius=0.5, calc_uvs=True)
ob = obj_from_bm(bm, "SM_M4_Ball")
pts = [Vector((math.cos(a) * math.sin(b), math.sin(a) * math.sin(b), math.cos(b))) * 0.5 for a in [i * math.pi / 6 for i in range(12)] for b in [j * math.pi / 6 for j in range(7)]]
finish(ob, "SM_M4_Ball", hull(pts, "ucx"), weighted=False)

# ---- Cylinder: r 0.5, h 1, centred; bevelled rims
clear()
bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=32, radius1=0.5, radius2=0.5, depth=1.0, calc_uvs=True)
rims = [e for e in bm.edges if abs(e.verts[0].co.z) > 0.49 and abs(e.verts[1].co.z) > 0.49 and abs(e.verts[0].co.z - e.verts[1].co.z) < 1e-4 and (e.verts[0].co.xy.length > 0.49)]
bmesh.ops.bevel(bm, geom=rims, offset=0.02, segments=3, profile=0.5, affect="EDGES")
ob = obj_from_bm(bm, "SM_M4_Cyl"); uv_box(ob)
pts = [Vector((math.cos(i * math.pi / 8) * 0.5, math.sin(i * math.pi / 8) * 0.5, z)) for i in range(16) for z in (-0.5, 0.5)]
finish(ob, "SM_M4_Cyl", hull(pts, "ucx"))

# ---- Cone: base r 0.5 at z -0.5, tip at +0.5 (the engine cone's frame), slightly rounded tip and base rim
clear()
bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=32, radius1=0.5, radius2=0.01, depth=1.0, calc_uvs=True)
ob = obj_from_bm(bm, "SM_M4_Cone"); uv_box(ob)
pts = [Vector((math.cos(i * math.pi / 8) * 0.5, math.sin(i * math.pi / 8) * 0.5, -0.5)) for i in range(16)] + [Vector((0, 0, 0.5))]
finish(ob, "SM_M4_Cone", hull(pts, "ucx"))

# ---- Plane: 1 x 1 m at z 0 facing up (the engine plane), a few subdivisions
clear()
bm = bmesh.new(); bmesh.ops.create_grid(bm, x_segments=4, y_segments=4, size=0.5, calc_uvs=True)
ob = obj_from_bm(bm, "SM_M4_Plane")
finish(ob, "SM_M4_Plane", hull([Vector((x * 0.5, y * 0.5, z)) for x in (-1, 1) for y in (-1, 1) for z in (-0.005, 0.005)], "ucx"), weighted=False)

# ---- Droplet: a water drop, d = 1 m round part, pointy top (fits where the engine sphere was: centred, ~1 m)
clear()
bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=24, v_segments=14, radius=0.5, calc_uvs=True)
for v in bm.verts:
    if v.co.z > 0:
        t = v.co.z / 0.5
        k = 1.0 - 0.75 * t * t
        v.co.x *= k; v.co.y *= k; v.co.z *= 1.0 + 0.35 * t
ob = obj_from_bm(bm, "SM_M4_Droplet")
finish(ob, "SM_M4_Droplet", hull(pts if False else [Vector((math.cos(a) * 0.5, math.sin(a) * 0.5, 0)) for a in [i * math.pi / 4 for i in range(8)]] + [Vector((0, 0, -0.5)), Vector((0, 0, 0.67))], "ucx"), weighted=False)

# ---- Speck: an irregular flake (dust, sugar grain, crumb chip), ~1 m across, thin
random.seed(11)
clear()
bm = bmesh.new(); bmesh.ops.create_icosphere(bm, subdivisions=2, radius=0.5, calc_uvs=True)
for v in bm.verts:
    v.co *= 0.75 + random.random() * 0.4
    v.co.z *= 0.35
ob = obj_from_bm(bm, "SM_M4_Speck")
finish(ob, "SM_M4_Speck", None, weighted=False)

# ---- Crumb: a porous lumpy chunk ~1 m (bread / cookie crumb, pebble at other tints)
random.seed(5)
clear()
bm = bmesh.new(); bmesh.ops.create_icosphere(bm, subdivisions=3, radius=0.5, calc_uvs=True)
import mathutils
for v in bm.verts:
    n = mathutils.noise.noise(v.co * 3.1) * 0.22 + mathutils.noise.noise(v.co * 9.0) * 0.07
    v.co *= 1.0 + n
    v.co.z *= 0.8
ob = obj_from_bm(bm, "SM_M4_Crumb")
finish(ob, "SM_M4_Crumb", hull([v.co.copy() for v in ob.data.vertices][::7], "ucx"), weighted=False)

random.seed(9)
clear()
bm = bmesh.new(); bmesh.ops.create_icosphere(bm, subdivisions=3, radius=0.5, calc_uvs=True)
for v in bm.verts:
    v.co *= 1.0 + mathutils.noise.noise(v.co * 1.7 + Vector((3, 1, 2))) * 0.18
    v.co.z *= 0.7
ob = obj_from_bm(bm, "SM_M4_Pebble")
finish(ob, "SM_M4_Pebble", hull([v.co.copy() for v in ob.data.vertices][::7], "ucx"), weighted=False)

# ---- Pheromone: a soft flat puddle blob (trail mark), 1 m, 2 cm thick, domed
clear()
bm = bmesh.new(); bmesh.ops.create_circle(bm, cap_ends=True, segments=28, radius=0.5)
bmesh.ops.triangulate(bm, faces=bm.faces)
ob = obj_from_bm(bm, "SM_M4_Pheromone")
for v in ob.data.vertices:
    r = v.co.xy.length
    v.co.x *= 1.0 + 0.08 * math.sin(math.atan2(v.co.y, v.co.x) * 3.0)
    v.co.z = 0.02 * (1.0 - (r / 0.5) ** 2)
finish(ob, "SM_M4_Pheromone", None, weighted=False)
print("M4SHAPES DONE")
