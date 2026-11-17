# Ant Kitchen: assemble the EXPORTED kitchen pieces (FBX + kitchen_manifest.json) and render previews.
# Run:  blender --background --factory-startup --python kitchen_preview.py -- <kitchen_dir> [shot ...]
# The scene frame is S = (-gameX, gameY, gameZ) / 1900: a true (unmirrored) view of the game.
import bpy, sys, os, json, math, time
from mathutils import Vector, Matrix

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
DIR = argv[0]
WANT = argv[1:]
K = 1.0 / 1900.0
t0 = time.time()
bpy.ops.wm.read_factory_settings(use_empty=True)
doc = json.load(open(os.path.join(DIR, "kitchen_manifest.json"), encoding="utf-8"))
scene = bpy.context.scene

def mk(name, rgb, rough=0.5, metal=0.0, emit=0.0):
    m = bpy.data.materials.new("P_" + name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (*rgb, 1)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if emit > 0:
        b.inputs["Emission Color"].default_value = (*rgb, 1)
        b.inputs["Emission Strength"].default_value = emit
    return m
PAL = {n: mk(n, i["rgb"], i["roughness"], i["metallic"], i.get("emission", 0.0)) for n, i in doc["materials"].items()}

# ---- import every FBX once
MESH = {}
for e in doc["pieces"]:
    before = set(bpy.data.objects)
    fp = os.path.join(DIR, e["file"])
    try:
        bpy.ops.wm.fbx_import(filepath=fp)
    except Exception:
        bpy.ops.import_scene.fbx(filepath=fp)
    new = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    ob = new[0]
    for sl in ob.material_slots:
        base = sl.material.name.split(".")[0] if sl.material else ""
        if base in PAL:
            sl.material = PAL[base]
    MESH[e["name"]] = (ob.data, ob.matrix_world.copy())
    for o in [o for o in bpy.data.objects if o not in before]:
        bpy.data.objects.remove(o)
print("IMPORTED %d in %.1fs" % (len(MESH), time.time() - t0))

kit = bpy.data.collections.new("Kitchen")
scene.collection.children.link(kit)
PLACED = []
for e in doc["pieces"]:
    me, Mi = MESH[e["name"]]
    for inst in e["instances"]:
        ob = bpy.data.objects.new(e["name"], me)
        kit.objects.link(ob)
        PLACED.append((ob, e, inst, Mi))

BY = {e["name"]: e for e in doc["pieces"]}
def carry(pname, idx, k, depth=0):
    # World-space motion of a moving piece, so that whatever sits on it follows.
    pe = BY[pname]
    inst = pe["instances"][idx]
    M = Matrix.Identity(4)
    so = inst.get("sits_on")
    if so and depth < 4:
        M = carry(so["parent"], so["parent_instance"], k, depth + 1)
    mo = pe.get("motion")
    if not mo:
        return M
    R = Matrix(inst["blender_rot_3x3"]).to_4x4()
    T = Matrix.Translation(Vector(inst["blender_loc_m"]))
    if mo["type"] == "hinge":
        A = T @ R @ Matrix.Rotation(math.radians(mo["angle_deg"] * k), 4, Vector(mo["axis_blender"])) @ R.inverted() @ T.inverted()
    else:
        A = Matrix.Translation(R.to_3x3() @ (Vector(mo["vector_m"]) * k))
    return M @ A

def pose(k):
    for ob, e, inst, Mi in PLACED:
        R = Matrix(inst["blender_rot_3x3"]).to_4x4()
        t = Vector(inst["blender_loc_m"])
        Sc = Matrix.Diagonal((*inst["scale"], 1))
        Mo = Matrix.Identity(4)
        mo = e.get("motion")
        if mo and k > 0:
            f = k
            for i, ff in (("Drawer1", 0.45), ("Drawer2", 0.72)):
                if e["name"].endswith(i):
                    f = k * ff
            if mo["type"] == "hinge":
                Mo = Matrix.Rotation(math.radians(mo["angle_deg"] * f), 4, Vector(mo["axis_blender"]))
            else:
                t = t + R.to_3x3() @ (Vector(mo["vector_m"]) * f)
        W = Matrix.Translation(t) @ R @ Mo @ Sc @ Mi
        so = inst.get("sits_on")
        if so and k > 0:
            W = carry(so["parent"], so["parent_instance"], k) @ W
        ob.matrix_world = W

# ---- stage (not part of the set): floor, walls of the game's kitchen
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
u = lambda v: v * K
sbox("Floor", u(-7200), u(7200), u(-5200), u(5200), -0.05, 0.0, FLOOR)
sbox("WallN", u(-7200), u(7200), u(5000), u(5200), 0, u(5400), WALL)
sbox("WallW", u(7000), u(7200), u(-5000), u(5000), 0, u(5400), WALL)      # game west = S +x
sbox("WallE", u(-7200), u(-7000), u(-5000), u(5000), 0, u(5400), WALL)
sbox("WallS1", u(-7200), u(-1500), u(-5200), u(-5000), 0, u(5400), WALL)
sbox("WallS2", u(1500), u(7200), u(-5200), u(-5000), 0, u(5400), WALL)
sbox("WallS3", u(-1500), u(1500), u(-5200), u(-5000), 0, u(2200), WALL)
sbox("WallS4", u(-1500), u(1500), u(-5200), u(-5000), u(4200), u(5400), WALL)

world = bpy.data.worlds.new("World")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.85, 0.92, 1.0, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.38
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
light("SUN", (0, -4, 5), 1.9, (50, 0, -24), color=(1.0, 0.95, 0.86))
light("AREA", (0.0, -0.3, 2.75), 110, (0, 0, 0), 4.5, (1.0, 0.96, 0.9))
light("AREA", (2.0, -2.0, 2.4), 60, (40, 0, 30), 3.0, (0.8, 0.9, 1.0))

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
scene.render.resolution_x = 1400
scene.render.resolution_y = 900

# name: (camera, target, lens, open amount, stage objects to hide, extra point lights [(loc, watts)])
SHOTS = {
    "assembled":      ((0.55, -2.35, 2.25), (0.55, 2.2, 1.05), 17, 0.0, [], []),
    "assembled_room": ((3.35, 1.25, 2.45), (-1.2, -1.6, 0.5), 17, 0.0, [], []),
    "top":            ((0.4, -6.4, 7.2), (0.2, 0.35, 0.6), 30, 0.0, ["WallS1", "WallS2", "WallS3", "WallS4"], []),
    "open":           ((0.55, -2.35, 2.25), (0.55, 2.2, 1.05), 17, 1.0, [], [((-1.95, 2.0, 1.2), 30), ((1.84, 2.0, 0.5), 12), ((0.4, 2.0, 0.5), 12)]),
    "open_left":      ((-0.2, -0.9, 1.75), (-1.0, 2.2, 0.95), 22, 1.0, [], [((-1.95, 2.0, 1.2), 30), ((0.4, 2.0, 0.5), 12)]),
    "open_right":     ((1.3, -0.9, 1.75), (2.3, 2.2, 0.95), 22, 1.0, [], [((1.84, 2.0, 0.5), 12)]),
    "fridge":         ((-1.55, -0.55, 1.45), (-1.95, 2.0, 1.02), 26, 1.0, [], [((-1.95, 1.85, 1.15), 25), ((-1.95, 1.9, 1.7), 10), ((-1.95, 1.9, 0.3), 8)]),
    "undersink":      ((1.5, 0.35, 0.75), (1.9, 2.0, 0.32), 26, 1.0, [], [((1.84, 1.9, 0.45), 6)]),
    "oven":           ((0.7, 0.25, 1.0), (0.4, 2.0, 0.5), 26, 1.0, [], [((0.4, 2.0, 0.5), 14)]),
    "drawers":        ((2.2, 0.15, 1.75), (2.75, 1.7, 0.55), 26, 1.0, [], [((2.6, 1.9, 0.5), 10)]),
    "worktop":        ((1.0, 0.2, 1.85), (1.2, 2.3, 1.05), 20, 0.0, [], []),
    "worktop_left":   ((-0.5, 0.3, 1.8), (-0.6, 2.3, 1.05), 20, 1.0, [], [((-0.9, 2.3, 1.05), 6)]),
    "table":          ((1.3, -2.3, 1.7), (-0.25, -0.3, 0.6), 24, 0.0, [], []),
    "floor_west":     ((1.6, -0.9, 0.9), (2.9, 0.6, 0.1), 24, 1.0, [], []),
    "south":          ((0.3, 1.2, 1.9), (0.2, -2.6, 1.2), 17, 0.0, [], []),
    "east":           ((-0.6, -0.2, 1.5), (-3.6, 0.2, 0.7), 18, 1.0, [], []),
    "west":           ((0.8, -0.3, 1.6), (3.7, 0.2, 1.1), 18, 1.0, [], []),
}
SHOTS.update({
    "c_ovenback":    ((0.39, 1.15, 0.27), (0.37, 2.5, 0.17), 30, 1.0, [], [((0.37, 2.56, 0.2), 6), ((0.4, 2.0, 0.5), 5)]),
    "c_underrange":  ((0.2, 1.0, 0.03), (0.5, 2.2, 0.02), 14, 0.0, [], []),
    "c_underfridge": ((-1.3, 0.9, 0.05), (-2.0, 2.2, 0.03), 16, 0.0, [], []),
    "c_trap":        ((1.53, 1.72, 0.44), (1.86, 2.32, 0.42), 15, 1.0, [], [((1.62, 1.8, 0.5), 4)]),
    "c_bin":         ((-2.3, -1.9, 1.6), (-3.16, -0.79, 0.6), 24, 1.0, [], []),
    "c_toaster":     ((3.0, 1.5, 1.35), (3.29, 2.2, 1.0), 28, 0.0, [], []),
    "c_matchbox":    ((0.75, 0.75, 0.3), (1.02, 1.37, 0.03), 30, 1.0, [], []),
    "c_sill":        ((0.2, -1.6, 1.5), (0.5, -2.6, 1.15), 26, 0.0, [], []),
    "c_microwave":   ((-0.75, 1.2, 1.25), (-1.0, 2.3, 1.05), 26, 1.0, [], [((-0.93, 2.3, 1.1), 4)]),
    "c_dishwasher":  ((0.12, 1.05, 1.2), (-0.42, 2.0, 0.42), 20, 1.0, [], [((-0.34, 2.0, 0.5), 8)]),
    "c_wallcab":     ((2.2, 0.9, 1.9), (2.7, 2.5, 1.95), 22, 1.0, [], [((2.7, 2.35, 1.95), 5)]),
    "c_floorprops":  ((-0.2, -1.3, 0.55), (-0.63, -2.25, 0.05), 22, 0.0, [], []),
    "c_bucket":      ((-2.6, 1.3, 1.5), (-3.3, 2.1, 0.5), 22, 0.0, [], []),
})
for a in WANT:
    if a.startswith("cam:"):      # ad-hoc shot: cam:name:x,y,z:tx,ty,tz:lens:open
        p = a.split(":")
        SHOTS[p[1]] = (tuple(float(v) for v in p[2].split(",")), tuple(float(v) for v in p[3].split(",")), float(p[4]), float(p[5]), [], [(tuple(float(v) for v in p[3].split(",")), 6)])
names = [a.split(":")[1] if a.startswith("cam:") else a for a in WANT] or list(SHOTS)
extra = []
for n in names:
    c, tg, lens, k, hide, lts = SHOTS[n]
    pose(k)
    for o in stage.objects:
        o.hide_render = o.name in hide
    for o in extra:
        bpy.data.objects.remove(o)
    extra = [light("POINT", loc, w) for loc, w in lts]
    cam.location = c
    d = Vector(tg) - Vector(c)
    cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    camd.lens = lens
    camd.clip_start = 0.02
    scene.render.filepath = os.path.join(DIR, "preview_%s.png" % n)
    bpy.ops.render.render(write_still=True)
    print("RENDERED %s %.1fs" % (n, time.time() - t0))
print("PREVIEW_DONE tris=%d" % sum(len(ob.data.polygons) for ob, e, i, m in PLACED))