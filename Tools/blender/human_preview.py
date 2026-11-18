# Ant Kitchen: assemble the EXPORTED Human segments (FBX + human_manifest.json), pose them like the game does
# (rigid segments turning about their joints) and render previews.
# Run:  blender --background --factory-startup --python human_preview.py -- <human_dir> [shot ...]
import bpy, sys, os, json, math, time
from mathutils import Vector, Matrix, Euler

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
DIR = argv[0]
WANT = argv[1:]
t0 = time.time()
bpy.ops.wm.read_factory_settings(use_empty=True)
doc = json.load(open(os.path.join(DIR, "human_manifest.json"), encoding="utf-8"))
scene = bpy.context.scene

def mk(name, rgb, rough=0.5, metal=0.0):
    m = bpy.data.materials.new("P_" + name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (*rgb, 1)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    return m
PAL = {n: mk(n, i["rgb"], i["roughness"], i["metallic"]) for n, i in doc["materials"].items()}

MESH, PIV, PAR, ORDER = {}, {}, {}, []
for e in doc["pieces"]:
    before = set(bpy.data.objects)
    fp = os.path.join(DIR, e["file"])
    try:
        bpy.ops.wm.fbx_import(filepath=fp)
    except Exception:
        bpy.ops.import_scene.fbx(filepath=fp)
    ob = [o for o in bpy.data.objects if o not in before and o.type == "MESH"][0]
    for sl in ob.material_slots:
        base = sl.material.name.split(".")[0] if sl.material else ""
        if base in PAL:
            sl.material = PAL[base]
    n = e["name"][5:]
    MESH[n] = (ob.data, ob.matrix_world.copy())
    PIV[n] = Vector(e["pivot_m"])
    PAR[n] = e["parent"][5:] if e["parent"] else None
    ORDER.append(n)
    for o in [o for o in bpy.data.objects if o not in before]:
        bpy.data.objects.remove(o)

def R(spec):
    return Euler([math.radians(a) for a in spec], "XYZ").to_matrix().to_4x4()

# Joint rotations, degrees about the Blender X, Y, Z axes, relative to the parent segment.
# About Y: a limb hanging down swings FORWARD for a negative angle; the spine leans forward for a positive one.
POSES = {
    "bind": {},
    "walk": {"Pelvis": (0, 2, -6), "Chest": (0, 3, 12), "Head": (0, 6, -6),
             "Thigh_L": (0, -24, 0), "Shin_L": (0, 14, 0), "Foot_L": (0, -8, 0),
             "Thigh_R": (0, 16, 0), "Shin_R": (0, 30, 0), "Foot_R": (0, 14, 0),
             "UpperArm_R": (-6, -22, 0), "Forearm_R": (0, -28, 0), "UpperArm_L": (6, 18, 0), "Forearm_L": (0, -14, 0)},
    "reach": {"_root": (0.12, 0, -0.42), "Pelvis": (0, 20, 0), "Belly": (0, 25, 0), "Chest": (0, 25, -10), "Head": (0, -38, 0),
              "Thigh_L": (0, -80, 0), "Shin_L": (0, 120, 0), "Foot_L": (0, -60, 0),
              "Thigh_R": (0, -80, 0), "Shin_R": (0, 120, 0), "Foot_R": (0, -60, 0),
              "UpperArm_R": (0, -104, 0), "Forearm_R": (0, -8, 0), "Hand_R": (0, 0, 0), "Fingers_R": (-8, 0, 0),
              "UpperArm_L": (14, -20, 0), "Forearm_L": (0, -30, 0)},
    "stomp": {"Pelvis": (4, -6, 0), "Belly": (0, -4, 0), "Chest": (0, -4, 0), "Head": (0, 22, 0),
              "Thigh_R": (0, -78, 0), "Shin_R": (0, 96, 0), "Foot_R": (0, -12, 0),
              "UpperArm_R": (-34, 10, 0), "Forearm_R": (0, -50, 0), "UpperArm_L": (30, -10, 0), "Forearm_L": (0, -60, 0),
              "Fingers_L": (-70, 0, 0), "Fingers_R": (70, 0, 0)},
}

kit = bpy.data.collections.new("Human")
scene.collection.children.link(kit)

def figure(pose, loc=(0, 0, 0), yaw=0.0):
    P = POSES[pose]
    root = Matrix.Translation(Vector(loc)) @ Matrix.Rotation(math.radians(yaw), 4, "Z") @ Matrix.Translation(Vector(P.get("_root", (0, 0, 0))))
    W = {}
    for n in ORDER:
        par = PAR[n]
        base = root @ Matrix.Translation(PIV[n]) if par is None else W[par] @ Matrix.Translation(PIV[n] - PIV[par])
        W[n] = base @ R(P.get(n, (0, 0, 0)))
        me, Mi = MESH[n]
        ob = bpy.data.objects.new(n, me)
        kit.objects.link(ob)
        ob.matrix_world = W[n] @ Mi
    return W

# ---- stage: the kitchen floor and a wall
stage = bpy.data.collections.new("Stage")
scene.collection.children.link(stage)
def sbox(name, x0, x1, y0, y1, z0, z1, m):
    me = bpy.data.meshes.new(name)
    v = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
    f = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    me.from_pydata(v, [], f)
    me.materials.append(m)
    o = bpy.data.objects.new(name, me)
    stage.objects.link(o)
    return o
WALL = mk("Wall", (0.96, 0.80, 0.52), 0.9)
FLOOR = mk("Floor", (0.8, 0.8, 0.8), 0.35)
nt = FLOOR.node_tree
chk = nt.nodes.new("ShaderNodeTexChecker")
chk.inputs["Scale"].default_value = 1.9
chk.inputs["Color1"].default_value = (0.93, 0.90, 0.84, 1)
chk.inputs["Color2"].default_value = (0.16, 0.20, 0.24, 1)
tc = nt.nodes.new("ShaderNodeTexCoord")
nt.links.new(tc.outputs["Object"], chk.inputs["Vector"])
nt.links.new(chk.outputs["Color"], nt.nodes["Principled BSDF"].inputs["Base Color"])
sbox("Floor", -6, 6, -6, 6, -0.05, 0.0, FLOOR)
sbox("WallBack", -2.6, -2.4, -6, 6, 0, 3.0, WALL)

world = bpy.data.worlds.new("World")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.85, 0.92, 1.0, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.42
scene.world = world
def light(kind, loc, energy, rot=(0, 0, 0), size=1.0, color=(1, 1, 1)):
    d = bpy.data.lights.new(kind, kind)
    d.energy = energy
    d.color = color
    if kind == "AREA":
        d.size = size
    if kind == "SUN":
        d.angle = math.radians(6)
    o = bpy.data.objects.new(kind, d)
    o.location = loc
    o.rotation_euler = [math.radians(a) for a in rot]
    scene.collection.objects.link(o)
    return o
light("SUN", (4, -4, 5), 2.0, (48, 0, 38), color=(1.0, 0.95, 0.86))
light("AREA", (2.5, 0.0, 2.8), 160, (0, 40, 0), 4.5, (1.0, 0.96, 0.9))
light("AREA", (2.0, 2.5, 1.2), 70, (0, 70, 50), 3.0, (0.8, 0.9, 1.0))

camd = bpy.data.cameras.new("Cam")
cam = bpy.data.objects.new("Cam", camd)
scene.collection.objects.link(cam)
scene.camera = cam
for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
    try:
        scene.render.engine = eng
        break
    except Exception:
        pass
for attr, val in (("taa_render_samples", 48), ("use_gtao", True), ("use_raytracing", True), ("use_shadows", True)):
    try:
        setattr(scene.eevee, attr, val)
    except Exception:
        pass
try:
    scene.view_settings.view_transform = "Standard"
except Exception:
    pass

# name: (figures [(pose, loc, yaw)], camera, target (point or "segment name of figure 0"), lens, (width, height))
SHOTS = {
    "turn":    ([("bind", (0, 1.35, 0), 0), ("bind", (0, 0.45, 0), -90), ("bind", (0, -0.45, 0), -35), ("bind", (0, -1.35, 0), 180)],
                (7.2, 0, 1.0), (0, 0, 0.9), 62, (1600, 900)),
    "ant":     ([("walk", (0, 0.55, 0), 0), ("reach", (0, -0.6, 0), 12)], (1.75, -0.1, 0.02), (0, 0, 0.95), 15, (900, 900)),
    "slipper": ([("bind", (0, 0, 0), 0)], (0.62, -0.42, 0.035), (0.08, -0.07, 0.07), 30, (900, 900)),
    "hand":    ([("reach", (0, 0, 0), 0)], (1.25, -0.72, 0.06), "Fingers_R", 30, (900, 900)),
    "poses":   ([("walk", (0, 0.9, 0), -70), ("stomp", (0, 0, 0), -30), ("reach", (0, -1.0, 0), -70)], (6.4, 0, 1.0), (0, 0, 0.85), 55, (1600, 900)),
    "face":    ([("bind", (0, 0, 0), 0)], (0.75, -0.35, 1.45), (0, 0, 1.64), 50, (900, 900)),
}
names = WANT or list(SHOTS)
for n in names:
    figs, c, tg, lens, res = SHOTS[n]
    for o in list(kit.objects):
        bpy.data.objects.remove(o)
    W0 = None
    for (pose, loc, yaw) in figs:
        W = figure(pose, loc, yaw)
        W0 = W0 or W
    if isinstance(tg, str):
        tg = tuple(W0[tg].translation)
    cam.location = c
    d = Vector(tg) - Vector(c)
    cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    camd.lens = lens
    camd.clip_start = 0.01
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.filepath = os.path.join(DIR, "preview_%s.png" % n)
    bpy.ops.render.render(write_still=True)
    print("RENDERED %s %.1fs" % (n, time.time() - t0))
print("PREVIEW_DONE")
