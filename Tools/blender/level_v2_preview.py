# Ant Kitchen level v2: assemble the WHOLE redesigned level from the exported FBX files (old sets + new pieces) and render previews.
# Run:  blender --background --factory-startup --python level_v2_preview.py -- <level_v2 dir> [shot ...]
# Reads level_v2_manifest.json (every placed mesh, written by Tools/level/build_level_v2.py) and docs/level/level_v2.json (walls, areas).
# Scene frame S = (-gameX, gameY, gameZ) / 1900: a true view of the game. Eevee, 1280 x 800, a few seconds a frame.
import bpy, sys, os, json, math, time
from mathutils import Vector, Matrix
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
DIR = argv[0]
WANT = argv[1:]
ROOT = os.path.abspath(os.path.join(DIR, "..", "..", ".."))
K = 1.0 / 1900.0
t0 = time.time()
bpy.ops.wm.read_factory_settings(use_empty=True)
doc = json.load(open(os.path.join(DIR, "level_v2_manifest.json"), encoding="utf-8"))
LVL = json.load(open(os.path.join(ROOT, "docs", "level", "level_v2.json"), encoding="utf-8"))
scene = bpy.context.scene
def S(p): return Vector((-p[0] * K, p[1] * K, p[2] * K))

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
for setname in ("kitchen", "living"):          # the palettes of the old sets, where their manifests carry one
    try:
        mm = json.load(open(os.path.join(DIR, "..", setname, setname + "_manifest.json"), encoding="utf-8")).get("materials", {})
        for n, i in mm.items():
            if n not in PAL and isinstance(i, dict) and "rgb" in i:
                PAL[n] = mk(n, i["rgb"], i.get("roughness", 0.5), i.get("metallic", 0.0), i.get("emission", 0.0))
    except Exception as ex:
        print("palette", setname, ex)

MESH = {}
for e in doc["pieces"]:
    fp = os.path.normpath(os.path.join(DIR, e["file"]))
    before = set(bpy.data.objects)
    try:
        bpy.ops.wm.fbx_import(filepath=fp)
    except Exception:
        bpy.ops.import_scene.fbx(filepath=fp)
    new = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    if not new:
        print("NO MESH IN", fp); continue
    ob = new[0]
    for sl in ob.material_slots:
        base = sl.material.name.split(".")[0] if sl.material else ""
        if base in PAL:
            sl.material = PAL[base]
    MESH[e["name"]] = (ob.data, ob.matrix_world.copy())
    for o in [o for o in bpy.data.objects if o not in before]:
        bpy.data.objects.remove(o)
print("IMPORTED %d in %.1fs" % (len(MESH), time.time() - t0))

col = bpy.data.collections.new("Level")
scene.collection.children.link(col)
N3 = Matrix.Diagonal((-1, 1, 1))
tris = 0
for e in doc["pieces"]:
    if e["name"] not in MESH: continue
    me, Mi = MESH[e["name"]]
    for inst in e["instances"]:
        ob = bpy.data.objects.new(e["name"], me)
        col.objects.link(ob)
        R = Matrix(inst["blender_rot_3x3"]).to_4x4()
        t = Vector(inst["blender_loc_m"])
        sc = inst.get("scale") or [1, 1, 1]
        W = Matrix.Translation(t) @ R @ Matrix.Diagonal((*sc, 1)) @ Mi
        mv = inst.get("mover")
        if mv and (mv.get("flags", 0) & 1) and "hinge_axis_game" in inst:        # parts that start open (the door to the living room, the yard gate...)
            ax = N3 @ Vector(inst["hinge_axis_game"])
            W = Matrix.Translation(t) @ Matrix.Rotation(math.radians(-inst["open_angle_game_deg"]), 4, ax) @ Matrix.Translation(-t) @ W
        ob.matrix_world = W
        tris += len(me.polygons)

# ---- stage: ground of every area and the walls of the house with their openings cut out (not part of the art set)
stage = bpy.data.collections.new("Stage")
scene.collection.children.link(stage)
WALLS = []
def sbox(name, g, m, wall=False):        # g = game box x0, y0, z0, x1, y1, z1
    a, b = S((g[0], g[1], g[2])), S((g[3], g[4], g[5]))
    x0, x1, y0, y1, z0, z1 = min(a.x, b.x), max(a.x, b.x), min(a.y, b.y), max(a.y, b.y), min(a.z, b.z), max(a.z, b.z)
    me = bpy.data.meshes.new(name)
    me.from_pydata([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], [],
                   [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)])
    me.materials.append(m)
    o = bpy.data.objects.new(name, me)
    stage.objects.link(o)
    if wall: WALLS.append((o, Vector(((x0 + x1) / 2, (y0 + y1) / 2, 0))))
    return o
WALL = mk("Wall", (0.96, 0.80, 0.52), 0.9)
WALL2 = mk("WallLiving", (0.78, 0.86, 0.80), 0.9)
FLOOR = mk("Floor", (0.8, 0.8, 0.8), 0.35)
nt = FLOOR.node_tree
chk = nt.nodes.new("ShaderNodeTexChecker"); chk.inputs["Scale"].default_value = 1.9
chk.inputs["Color1"].default_value = (0.93, 0.90, 0.84, 1); chk.inputs["Color2"].default_value = (0.16, 0.20, 0.24, 1)
tc = nt.nodes.new("ShaderNodeTexCoord")
nt.links.new(tc.outputs["Object"], chk.inputs["Vector"]); nt.links.new(chk.outputs["Color"], nt.nodes["Principled BSDF"].inputs["Base Color"])
WOOD = mk("FloorWood", (0.55, 0.36, 0.20), 0.45)
GRASS = mk("Grass", (0.30, 0.56, 0.16), 0.95)
SOILM = mk("Dirt", (0.35, 0.26, 0.16), 0.95)
GROUND = {"kitchen": FLOOR, "living": WOOD, "yard": GRASS, "street": GRASS}
for a in LVL["areas"]:
    for k, r in enumerate(a["rects"]):
        if a["id"] == "street": r = [-9400, r[1], r[2], r[3]]          # the lawn strip; pavement and road are meshes
        sbox("G_%s%d" % (a["id"], k), [r[0], r[1], -60, r[2], r[3], 0], GROUND[a["id"]])
sbox("G_under", [-16600, -12400, -400, 20600, 5800, -262], SOILM)
OPEN = [o["box"] for o in LVL["openings"] if o["box"][5] - o["box"][2] >= 400]
for n, w in enumerate(LVL["walls"]):
    if w.get("kind"): continue
    r, (z0, z1) = w["rect"], w["z"]
    m = WALL2 if (r[0] >= 7000 and r[1] < 1800 and not (r[0] == 7000 and r[3] > -5000 and r[1] >= -5000)) else WALL
    alongx = (r[2] - r[0]) >= (r[3] - r[1])
    cuts = []
    for b in OPEN:
        if b[0] < r[2] + 60 and b[3] > r[0] - 60 and b[1] < r[3] + 60 and b[4] > r[1] - 60:
            cuts.append(((b[0], b[3]) if alongx else (b[1], b[4]), b[2], b[5]))
    cuts.sort()
    lo, hi = (r[0], r[2]) if alongx else (r[1], r[3])
    def seg(a0, a1, za, zb, tag):
        if a1 - a0 < 1 or zb - za < 1: return
        g = [a0, r[1], za, a1, r[3], zb] if alongx else [r[0], a0, za, r[2], a1, zb]
        sbox("W%d%s" % (n, tag), g, m, True)
    cur = lo
    for k, ((c0, c1), cz0, cz1) in enumerate(cuts):
        c0, c1 = max(lo, c0), min(hi, c1)
        seg(cur, c0, z0, z1, "a%d" % k); seg(c0, c1, z0, cz0, "b%d" % k); seg(c0, c1, cz1, z1, "c%d" % k); cur = c1
    seg(cur, hi, z0, z1, "z")

world = bpy.data.worlds.new("World")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.85, 0.92, 1.0, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.45
scene.world = world
def light(kind, loc, energy, rot=(0, 0, 0), size=1.0, color=(1, 1, 1)):
    d = bpy.data.lights.new(kind, kind); d.energy = energy; d.color = color
    if kind == "AREA": d.size = size
    if kind == "SUN": d.angle = math.radians(8)
    o = bpy.data.objects.new(kind, d); o.location = loc; o.rotation_euler = [math.radians(a) for a in rot]
    scene.collection.objects.link(o)
    return o
light("SUN", (0, -4, 6), 2.2, (48, 0, -28), color=(1.0, 0.95, 0.86))
for g, w in (((0, 0, 5200), 110), ((13700, -3700, 5200), 120), ((0, -3000, 4600), 40), ((16500, -7000, 4600), 40)):
    light("AREA", S(g), w, (0, 0, 0), 5.0, (1.0, 0.96, 0.9))
camd = bpy.data.cameras.new("Cam")
cam = bpy.data.objects.new("Cam", camd)
scene.collection.objects.link(cam); scene.camera = cam
for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
    try:
        scene.render.engine = eng; break
    except Exception:
        pass
for attr, val in (("taa_render_samples", 24), ("use_gtao", True), ("use_shadows", True)):
    try: setattr(scene.eevee, attr, val)
    except Exception: pass
try: scene.view_settings.view_transform = "Standard"
except Exception: pass
scene.render.resolution_x, scene.render.resolution_y = 1280, 800
scene.render.image_settings.file_format = "JPEG"
scene.render.image_settings.quality = 86

# name: (camera, target in game units, lens mm or ("ortho", width in uu), hide near walls?, fill light watts at the target)
SHOTS = {
    "top":          ((2550, -3300, 60000), (2550, -3300, 0), ("ortho", 37400), False, 0),
    "iso_kitchen":  ((-8500, -13500, 11500), (300, 300, 500), 30, True, 0),          # from the south-west; the tree is hidden in the iso shots
    "iso_living":   ((22500, -17500, 12000), (13400, -3900, 500), 30, True, 0),
    "iso_yard":     ((6000, -23000, 9000), (6000, -8600, 0), 24, True, 0),
    "ant_plintus":  ((-6750, -2450, 45), (-3000, -1200, 420), 17, False, 3),
    "ant_svalka":   ((3750, -1750, 70), (5900, 700, 520), 17, False, 3),
    "ant_plato":    ((-4300, -1500, 70), (-900, -350, 900), 17, False, 3),
    "ant_kladovye": ((-4650, 3120, 1800), (-6300, 3900, 2050), 17, False, 3),
    "ant_lednik":   ((5250, 1750, 290), (4850, 3600, 1500), 15, False, 3),
    "ant_porog":    ((9300, -2600, 60), (7600, -600, 520), 17, False, 3),
    "ant_babushka": ((11700, -6500, 70), (9300, -5600, 700), 17, False, 3),
    "ant_play":     ((17700, -5300, 60), (14500, -3400, 520), 17, False, 3),
    "ant_stenka":   ((14300, -6500, 60), (16900, -8500, 700), 17, False, 3),
    "ant_reddvor":  ((10300, -11900, 70), (12900, -10650, 330), 17, False, 0),
    "ant_yard_a":   ((2400, -8000, 70), (-300, -5800, 500), 17, False, 0),
}
for a in WANT:
    if a.startswith("cam:"):      # ad-hoc: cam:name:x,y,z:tx,ty,tz:lens
        p = a.split(":")
        SHOTS[p[1]] = (tuple(float(v) for v in p[2].split(",")), tuple(float(v) for v in p[3].split(",")), float(p[4]), False, 60)
names = [a.split(":")[1] if a.startswith("cam:") else a for a in WANT] or list(SHOTS)
extra = None
for n in names:
    c, tg, lens, hide, fill = SHOTS[n]
    cs, ts = S(c), S(tg)
    if isinstance(lens, tuple):
        camd.type = "ORTHO"; camd.ortho_scale = lens[1] * K
        cam.location = cs; cam.rotation_euler = (0, 0, 0)
        scene.render.resolution_y = 760
    else:
        camd.type = "PERSP"; camd.lens = lens
        cam.location = cs
        cam.rotation_euler = (ts - cs).to_track_quat("-Z", "Y").to_euler()
        scene.render.resolution_y = 800
    camd.clip_start = 0.004 if n.startswith("ant") else 0.05
    camd.clip_end = 200
    dv = Vector((cs.x - ts.x, cs.y - ts.y, 0)); ln = dv.length or 1.0; dv /= ln
    for o, ctr in WALLS:
        o.hide_render = bool(hide) and (ctr - Vector((ts.x, ts.y, 0))).dot(dv) > 1.2
    for o in col.objects:
        if o.name.startswith('SM_Y_Tree'): o.hide_render = bool(hide) and not n.endswith('yard')
    if extra: bpy.data.objects.remove(extra); extra = None
    if fill: extra = light("POINT", cs + Vector((0, 0, 0.5)), fill)
    scene.render.filepath = os.path.join(DIR, "preview_%s.jpg" % n)
    bpy.ops.render.render(write_still=True)
    print("RENDERED %s %.1fs" % (n, time.time() - t0))
print("PREVIEW_DONE instances=%d polygons=%d" % (len(col.objects), tris))