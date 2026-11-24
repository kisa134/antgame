# The cooking channel's footage (docs/level/LIFE.md, TV channel 5): our own birthday cake model (SourceArt/Map4/higgsfield/glb/cake.glb)
# turning on a cake stand in a warm studio kitchen, strawberries dropping onto it. Eevee, 960x540, 240 frames (10 s at 24 fps).
#   blender --background --factory-startup --python blender_cake_show.py -- <cake.glb> <out dir>
import bpy, sys, os, math, random
from mathutils import Vector
args = sys.argv[sys.argv.index("--") + 1:]
GLB, OUT = args[0], args[1]
os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
bpy.ops.import_scene.gltf(filepath=GLB)
objs = [o for o in sc.objects if o.type == "MESH"]
mn = Vector((1e9, 1e9, 1e9)); mx = Vector((-1e9, -1e9, -1e9))
for o in objs:
    for c in o.bound_box:
        w = o.matrix_world @ Vector(c)
        mn = Vector(map(min, mn, w)); mx = Vector(map(max, mx, w))
size = max(mx - mn)
root = bpy.data.objects.new("CakeRoot", None); sc.collection.objects.link(root)
for o in objs:
    if o.parent is None: o.parent = root
root.scale = (1.0 / size,) * 3
root.location = (-(mn.x + mx.x) / 2 / size, -(mn.y + mx.y) / 2 / size, -mn.z / size + 0.32)

def mat(name, col, rough=0.5, metal=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]; b.inputs["Base Color"].default_value = (*col, 1); b.inputs["Roughness"].default_value = rough; b.inputs["Metallic"].default_value = metal
    return m
bpy.ops.mesh.primitive_cylinder_add(radius=0.62, depth=0.04, location=(0, 0, 0.30)); st = bpy.context.object; st.data.materials.append(mat("stand", (0.95, 0.95, 0.97), 0.2))
bpy.ops.mesh.primitive_cylinder_add(radius=0.12, depth=0.28, location=(0, 0, 0.14)); st2 = bpy.context.object; st2.data.materials.append(mat("stand2", (0.95, 0.95, 0.97), 0.2))
bpy.ops.mesh.primitive_cylinder_add(radius=0.3, depth=0.02, location=(0, 0, 0.01)); st3 = bpy.context.object; st3.data.materials.append(mat("stand3", (0.95, 0.95, 0.97), 0.2))
for o in (st, st2, st3, root): pass
turn = bpy.data.objects.new("Turn", None); sc.collection.objects.link(turn)
for o in (root, st, st2, st3): o.parent = turn
bpy.ops.mesh.primitive_plane_add(size=8, location=(0, 0, 0)); fl = bpy.context.object; fl.data.materials.append(mat("counter", (0.92, 0.86, 0.78), 0.35))
bpy.ops.mesh.primitive_plane_add(size=8, location=(0, 2.2, 2.0), rotation=(math.radians(90), 0, 0)); wl = bpy.context.object; wl.data.materials.append(mat("tiles", (0.85, 0.93, 0.9), 0.4))
straw = mat("straw", (0.85, 0.05, 0.08), 0.25)
random.seed(4)
for k in range(9):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.045, location=(0, 0, 0)); s = bpy.context.object
    s.scale = (1, 1, 1.3); s.data.materials.append(straw)
    a = k / 9 * 2 * math.pi; r = 0.33
    tgt = Vector((math.cos(a) * r, math.sin(a) * r, 0.32 + (mx.z - mn.z) / size + 0.03))
    f0 = 20 + k * 22
    s.parent = turn
    s.location = tgt + Vector((0, 0, 1.6)); s.keyframe_insert("location", frame=1); s.keyframe_insert("location", frame=f0)
    s.location = tgt; s.keyframe_insert("location", frame=f0 + 10)
try:
    bpy.context.preferences.edit.keyframe_new_interpolation_type = "LINEAR"
except Exception:
    pass
turn.rotation_euler = (0, 0, 0); turn.keyframe_insert("rotation_euler", frame=1)
turn.rotation_euler = (0, 0, math.radians(120)); turn.keyframe_insert("rotation_euler", frame=241)
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam")); sc.collection.objects.link(cam); sc.camera = cam
cam.data.lens = 45
def aim(loc, tgt=(0, 0, 0.55)):
    cam.location = loc; d = Vector(tgt) - Vector(loc); cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
aim((0, -2.6, 1.5)); cam.keyframe_insert("location", frame=1); cam.keyframe_insert("rotation_euler", frame=1)
aim((0.4, -1.9, 1.15)); cam.keyframe_insert("location", frame=241); cam.keyframe_insert("rotation_euler", frame=241)
for kind, loc, e, col in (("AREA", (-1.5, -1.5, 2.5), 400, (1, 0.9, 0.8)), ("AREA", (2, -1, 1.5), 150, (0.8, 0.9, 1)), ("AREA", (0, 1.8, 2.5), 200, (1, 1, 1))):
    l = bpy.data.objects.new("L", bpy.data.lights.new("L", kind)); l.data.energy = e; l.data.color = col; l.data.size = 1.5
    l.location = loc; d = Vector((0, 0, 0.5)) - Vector(loc); l.rotation_euler = d.to_track_quat("-Z", "Y").to_euler(); sc.collection.objects.link(l)
world = bpy.data.worlds.new("W"); sc.world = world; world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.9, 0.8, 0.7, 1); world.node_tree.nodes["Background"].inputs[1].default_value = 0.6
try:
    sc.render.engine = "BLENDER_EEVEE_NEXT"
except Exception:
    sc.render.engine = "BLENDER_EEVEE"
sc.render.resolution_x, sc.render.resolution_y = 960, 540
sc.frame_start, sc.frame_end = 1, 240
sc.render.fps = 24
sc.render.image_settings.file_format = "PNG"
sc.render.filepath = os.path.join(OUT, "c")
bpy.ops.render.render(animation=True)
print("CAKE_DONE")
