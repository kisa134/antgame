# Close-ups of the Meshy ant head (to choose where the mandibles are cut off). Throwaway helper.
import bpy, sys, os, glob, math
from mathutils import Vector
argv = sys.argv[sys.argv.index("--") + 1:]
SRC, OUT = argv[0], argv[1]
os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
fbx = glob.glob(os.path.join(SRC, "SM_Ant_Body", "*", "*.fbx"))[0]
try:
    bpy.ops.wm.fbx_import(filepath=fbx)
except Exception:
    bpy.ops.import_scene.fbx(filepath=fbx)
world = bpy.data.worlds.new("World"); world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (1, 1, 1, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 1.2
scene.world = world
camd = bpy.data.cameras.new("Cam"); camd.type = "ORTHO"
cam = bpy.data.objects.new("Cam", camd); scene.collection.objects.link(cam); scene.camera = cam
for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
    try:
        scene.render.engine = eng; break
    except Exception:
        pass
try:
    scene.view_settings.view_transform = "Standard"
except Exception:
    pass
sun = bpy.data.lights.new("Sun", "SUN"); sun.energy = 2.5
so = bpy.data.objects.new("Sun", sun); so.rotation_euler = (math.radians(40), 0, math.radians(30)); scene.collection.objects.link(so)
# grid lines every 0.05 as thin emissive boxes
gm = bpy.data.materials.new("G"); gm.use_nodes = True
gm.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0, 0.3, 1, 1)
def box(c, s):
    bpy.ops.mesh.primitive_cube_add(size=1, location=c); o = bpy.context.object; o.scale = s; o.data.materials.append(gm)
for i in range(4, 11):
    x = i * 0.05
    box((x, 0, 0), (0.0008, 0.5, 0.0008))      # seen from the side as dots? no: lines along Y
    box((x, -0.2, 0), (0.0008, 0.0008, 0.5))   # vertical lines in front of the side view
for k in range(-4, 5):
    z = k * 0.05
    box((0.35, -0.2, z), (0.5, 0.0008, 0.0008))
for j in range(-3, 4):
    y = j * 0.05
    box((0.35, y, -0.3), (0.5, 0.0008, 0.0008))  # lines along X under the head (bottom view)
    box((0.6, y, 0), (0.0008, 0.0008, 0.5))      # in front (front view)
for i in range(4, 11):
    box((i * 0.05, 0, -0.3), (0.0008, 0.5, 0.0008))
for k in range(-4, 5):
    box((0.6, 0, k * 0.05), (0.0008, 0.5, 0.0008))
scene.render.resolution_x = 800; scene.render.resolution_y = 800
def shot(name, center, size, d):
    d = Vector(d).normalized()
    cam.location = Vector(center) + d * 2.0
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    camd.ortho_scale = size; camd.clip_start = 0.001; camd.clip_end = 10
    scene.render.filepath = os.path.join(OUT, name + ".png")
    bpy.ops.render.render(write_still=True); print("RENDERED", name)
shot("head_side", (0.35, 0, 0.0), 0.4, (0, -1, 0))
shot("head_bottom", (0.35, 0, 0.0), 0.4, (0, 0, -1))
shot("head_front", (0.35, 0, 0.0), 0.4, (1, 0, 0))
shot("head_q", (0.35, 0, -0.03), 0.4, (0.8, -0.6, -0.5))
print("LOOK_DONE")
