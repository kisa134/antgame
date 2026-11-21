# Ant Kitchen level v3: assemble a level state (SourceArt/Blender/level_v3/state_<name>.json, written by Tools/level/build_level_v3.py)
# from the exported FBX and render previews in a time-of-day light (Tools/level/level_v3_layout.TIME_OF_DAY / LIGHTS).
# Run:  blender --background --factory-startup --python level_v3_preview.py -- <level_v3 dir> <state> <time> [shot ...]
# Eevee, 1280 x 800 (top: 1280 x 760), 24 samples: a few seconds a frame. Modest on purpose: the game may be running on this PC.
import bpy, sys, os, json, math, time
from mathutils import Vector, Matrix
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
DIR, STATE, TOD = argv[0], argv[1], argv[2]
WANT = argv[3:]
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "level"))
import level_v3_layout as V3
ROOT = os.path.abspath(os.path.join(DIR, "..", "..", ".."))
K = 1.0 / 1900.0
t0 = time.time()
bpy.ops.wm.read_factory_settings(use_empty=True)
doc = json.load(open(os.path.join(DIR, "state_%s.json" % STATE), encoding="utf-8"))
LVL = json.load(open(os.path.join(ROOT, "docs", "level", "level_v3.json"), encoding="utf-8"))
scene = bpy.context.scene
def S(p): return Vector((-p[0] * K, p[1] * K, p[2] * K))

def mk(name, rgb, rough=0.5, metal=0.0, emit=0.0, alpha=1.0):
    m = bpy.data.materials.new("P_" + name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (*rgb, 1)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if emit > 0:
        b.inputs["Emission Color"].default_value = (*rgb, 1)
        b.inputs["Emission Strength"].default_value = emit
    if alpha < 1.0:
        try:
            b.inputs["Alpha"].default_value = alpha; m.blend_method = "BLEND"
        except Exception: pass
    return m
def checker(m, c1, c2, scale, coord="Object"):
    nt = m.node_tree; chk = nt.nodes.new("ShaderNodeTexChecker"); chk.inputs["Scale"].default_value = scale
    chk.inputs["Color1"].default_value = (*c1, 1); chk.inputs["Color2"].default_value = (*c2, 1)
    tc = nt.nodes.new("ShaderNodeTexCoord"); nt.links.new(tc.outputs[coord], chk.inputs["Vector"])
    nt.links.new(chk.outputs["Color"], nt.nodes["Principled BSDF"].inputs["Base Color"])
def noisy(m, c1, c2, scale, stretch=(1, 1, 1)):
    nt = m.node_tree; nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = scale
    tc = nt.nodes.new("ShaderNodeTexCoord"); mp = nt.nodes.new("ShaderNodeMapping"); mp.inputs["Scale"].default_value = stretch
    ramp = nt.nodes.new("ShaderNodeValToRGB"); ramp.color_ramp.elements[0].color = (*c1, 1); ramp.color_ramp.elements[1].color = (*c2, 1)
    nt.links.new(tc.outputs["Object"], mp.inputs["Vector"]); nt.links.new(mp.outputs["Vector"], nz.inputs["Vector"])
    nt.links.new(nz.outputs["Fac"], ramp.inputs["Fac"]); nt.links.new(ramp.outputs["Color"], nt.nodes["Principled BSDF"].inputs["Base Color"])

PAL = {}
for n, i in doc["materials"].items():
    if isinstance(i, dict) and "rgb" in i:
        PAL[n] = mk(n, i["rgb"], i.get("roughness", 0.5), i.get("metallic", 0.0), i.get("emission", 0.0), 0.35 if n in ("ShelfGlass", "PetClear") else 1.0)
for setname in ("kitchen", "living"):
    try:
        mm = json.load(open(os.path.join(DIR, "..", setname, setname + "_manifest.json"), encoding="utf-8")).get("materials", {})
        for n, i in mm.items():
            if n not in PAL and isinstance(i, dict) and "rgb" in i:
                PAL[n] = mk(n, i["rgb"], i.get("roughness", 0.5), i.get("metallic", 0.0), i.get("emission", 0.0))
    except Exception as ex:
        print("palette", setname, ex)
# v3 look: patterned cloth and a few tones taken down (one saturated accent per room)
if "ClothCheck" in PAL: checker(PAL["ClothCheck"], (0.78, 0.20, 0.17), (0.93, 0.90, 0.84), 140.0)
if "Curtain" in PAL: noisy(PAL["Curtain"], (0.80, 0.70, 0.52), (0.90, 0.82, 0.64), 40.0, (12, 1, 0.3))
if "Bulb" in PAL:
    PAL["Bulb"].node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = 6.0 if TOD != "day" else 0.5

for n, rgb in V3.PALETTE_V3["slots"].items():
    if n in PAL: PAL[n].node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (*rgb, 1)
PIECE_MAT = {}
for k, rgb in V3.PALETTE_V3["piece_slots"].items():
    PIECE_MAT[k] = mk("V3_" + k.replace("/", "_"), rgb, 0.9)
MESH = {}
files = {}
for i in doc["instances"]:
    files.setdefault(i["piece"], i["file"])
for name, f in files.items():
    fp = os.path.normpath(os.path.join(DIR, f))
    before = set(bpy.data.objects)
    try:
        bpy.ops.wm.fbx_import(filepath=fp)
    except Exception:
        try: bpy.ops.import_scene.fbx(filepath=fp)
        except Exception as ex: print("IMPORT FAIL", fp, ex); continue
    new = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    if not new:
        print("NO MESH IN", fp); continue
    ob = new[0]
    for sl in ob.material_slots:
        base = sl.material.name.split(".")[0] if sl.material else ""
        if (name + "/" + base) in PIECE_MAT: sl.material = PIECE_MAT[name + "/" + base]
        elif base in PAL: sl.material = PAL[base]
    MESH[name] = (ob.data, ob.matrix_world.copy())
    for o in [o for o in bpy.data.objects if o not in before]:
        bpy.data.objects.remove(o)
print("IMPORTED %d in %.1fs" % (len(MESH), time.time() - t0))

col = bpy.data.collections.new("Level")
scene.collection.children.link(col)
N3 = Matrix.Diagonal((-1, 1, 1))
tris = 0
TINT = {}
for i in doc["instances"]:
    if i["piece"] not in MESH: continue
    me, Mi = MESH[i["piece"]]
    if i.get("tint") and i["tint"] in PAL:
        key = (i["piece"], i["tint"])
        if key not in TINT:
            m2 = me.copy()
            for k in range(len(m2.materials)):
                nm = m2.materials[k].name.split(".")[0] if m2.materials[k] else ""
                if nm in ("P_Latex", "P_Curtain"): m2.materials[k] = PAL[i["tint"]]
            TINT[key] = m2
        me = TINT[key]
    ob = bpy.data.objects.new(i["piece"], me)
    col.objects.link(ob)
    R = Matrix(i["blender_rot_3x3"]).to_4x4()
    t = Vector(i["blender_loc_m"])
    sc = i.get("scale") or [1, 1, 1]
    W = Matrix.Translation(t) @ R @ Matrix.Diagonal((*sc, 1)) @ Mi
    mv = i.get("mover") or {}
    pose = i.get("pose")
    flags = mv.get("flags", 0) if isinstance(mv.get("flags", 0), int) else 0
    frac = None
    if pose is None and (flags & 1): frac = 1.0
    elif pose == "open": frac = 1.0
    elif pose and pose.startswith("ajar:") and "open_angle_game_deg" in i: frac = float(pose[5:]) / max(1.0, abs(i["open_angle_game_deg"]))
    if frac and "hinge_axis_game" in i:
        ax = N3 @ Vector(i["hinge_axis_game"])
        W = Matrix.Translation(t) @ Matrix.Rotation(math.radians(-i["open_angle_game_deg"] * min(1.0, frac)), 4, ax) @ Matrix.Translation(-t) @ W
    elif frac and "slide_game_uu" in i:
        W = Matrix.Translation(S(i["slide_game_uu"]) * min(1.0, frac)) @ W
    ob.matrix_world = W
    tris += len(me.polygons)

# ---- stage: floors, walls (openings cut), ceilings
stage = bpy.data.collections.new("Stage")
scene.collection.children.link(stage)
WALLS, CEIL = [], []
def sbox(name, g, m, wall=False):
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
LK, LL = V3.ROOM_LOOK["kitchen"], V3.ROOM_LOOK["living"]
WALL = mk("Wall", LK["wall"], 0.9); WALL2 = mk("WallLiving", LL["wall"], 0.9)
noisy(WALL2, tuple(c * 0.93 for c in LL["wall"]), LL["wall"], 6.0, (40, 1, 1))
FLOOR = mk("Floor", (0.8, 0.8, 0.8), 0.35); checker(FLOOR, LK["floor_a"], LK["floor_b"], 1900.0 / LK["floor_tile_uu"] * 1.0)
WOOD = mk("FloorWood", LL["floor_a"], 0.45); noisy(WOOD, LL["floor_b"], LL["floor_a"], 3.0, (1, 14, 1))
GRASS = mk("Grass", V3.ROOM_LOOK["yard"]["grass"], 0.95); noisy(GRASS, (0.22, 0.36, 0.12), (0.34, 0.50, 0.20), 8.0)
SOILM = mk("Dirt", (0.35, 0.26, 0.16), 0.95)
CEILM = mk("Ceiling", LK["ceiling"], 0.9)
GROUND = {"kitchen": FLOOR, "living": WOOD, "yard": GRASS, "street": GRASS}
for a in LVL["areas"]:
    for k, r in enumerate(a["rects"]):
        if a["id"] == "street": r = [-9400, r[1], r[2], r[3]]
        sbox("G_%s%d" % (a["id"], k), [r[0], r[1], -60, r[2], r[3], 0], GROUND[a["id"]])
        if a["id"] in ("kitchen", "living"): CEIL.append(sbox("C_%s%d" % (a["id"], k), [r[0] - 200, r[1] - 200, 5400, r[2] + 200, r[3] + 200, 5460], CEILM))
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

# ---- light for the time of day
T = V3.TIME_OF_DAY[TOD]
world = bpy.data.worlds.new("World"); world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (*T["sky"]["rgb"], 1)
world.node_tree.nodes["Background"].inputs[1].default_value = T["sky"]["strength"]
scene.world = world
def light(kind, loc, energy, rot=(0, 0, 0), size=1.0, color=(1, 1, 1), soft=0.05):
    d = bpy.data.lights.new(kind, kind); d.energy = energy; d.color = color
    if kind == "AREA": d.size = size
    if kind == "SUN": d.angle = math.radians(4)
    if kind == "POINT": d.shadow_soft_size = soft
    o = bpy.data.objects.new(kind, d); o.location = loc; o.rotation_euler = [math.radians(a) for a in rot]
    scene.collection.objects.link(o)
    return o
s = T["sun"]
if s["energy"] > 0:
    light("SUN", (0, -4, 6), s["energy"], (90 - s["elev"], 0, -s["azim"]), color=s["rgb"])
for key in T["lamps_on"]:
    Lp = V3.LIGHTS[key]
    w = {"pendant": 160, "hood": 90, "floor_lamp": 140, "garland": 90, "tv": 70, "street": 900, "fridge": 60}[key]
    if Lp["kind"] == "area": light("AREA", S(Lp["p"]), w, (0, 0, 0), 0.9, Lp["rgb"])
    else: light("POINT", S(Lp["p"]), w, color=Lp["rgb"], soft=0.06)
if TOD == "day":
    for g in ((0, 0, 5200), (13700, -3700, 5200)): light("AREA", S(g), 60, (0, 0, 0), 5.0, (1.0, 0.97, 0.92))
elif TOD == "evening":
    for g in ((0, 0, 5200), (13700, -3700, 5200)): light("AREA", S(g), 18, (0, 0, 0), 5.0, (1.0, 0.85, 0.7))
camd = bpy.data.cameras.new("Cam"); cam = bpy.data.objects.new("Cam", camd)
scene.collection.objects.link(cam); scene.camera = cam
for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
    try:
        scene.render.engine = eng; break
    except Exception: pass
for attr, val in (("taa_render_samples", 24), ("use_gtao", True), ("use_shadows", True), ("use_raytracing", False)):
    try: setattr(scene.eevee, attr, val)
    except Exception: pass
try:
    scene.view_settings.view_transform = "Standard"; scene.view_settings.look = "Medium High Contrast"
except Exception:
    try: scene.view_settings.view_transform = "Standard"
    except Exception: pass
scene.render.resolution_x, scene.render.resolution_y = 1280, 800
scene.render.image_settings.file_format = "JPEG"; scene.render.image_settings.quality = 86
# name: (camera, target, lens or ("ortho", width uu), cut walls?, fill W at camera, ceiling?)
SHOTS = {
    "top":          ((2550, -3300, 60000), (2550, -3300, 0), ("ortho", 37400), False, 0, False),
    "iso_kitchen":  ((-8500, -13500, 11500), (300, 300, 500), 30, True, 0, False),
    "iso_living":   ((22500, -17500, 12000), (13400, -3900, 500), 30, True, 0, False),
    "ant_shelf":    ((-6640, -950, 2700), (-6820, 900, 2790), 16, False, 2, True),
    "ant_screws":   ((-6750, -1180, 2330), (-6990, -800, 2500), 22, False, 1, True),
    "ant_table":    ((-3300, -1600, 50), (-1800, -400, 1000), 16, False, 3, True),
    "ant_fridge":   ((3150, 2350, 40), (3200, 3100, 2050), 20, False, 3, True),
    "ant_worktop":  ((1350, 3050, 1760), (2300, 3200, 2100), 18, False, 2, True),
    "ant_window":   ((-500, -4930, 2235), (-1750, -4760, 2900), 16, False, 2, True),
    "ant_sofa":     ((15600, -2300, 50), (17650, -650, 2400), 16, False, 3, True),
    "ant_toybox":   ((10400, -900, 50), (9080, 900, 600), 16, False, 3, True),
    "ant_gifts":    ((18950, -6900, 45), (19700, -8450, 450), 16, False, 3, True),
    "iso_chaos":    ((7500, 4200, 7800), (-3000, -1800, 500), 24, True, 0, False),
    "ant_knitting": ((11300, -5300, 60), (10250, -4450, 800), 17, False, 3, True),
    "ant_halldoor": ((17900, -1800, 50), (20150, -950, 1700), 16, False, 3, True),
    "ant_chaos":    ((-4800, -1600, 50), (-6600, -300, 900), 16, False, 3, True),
}
for a in WANT:
    if a.startswith("cam:"):
        p = a.split(":")
        SHOTS[p[1]] = (tuple(float(v) for v in p[2].split(",")), tuple(float(v) for v in p[3].split(",")), float(p[4]), False, 2, True)
names = [a.split(":")[1] if a.startswith("cam:") else a for a in WANT] or list(SHOTS)
extra = None
for n in names:
    c, tg, lens, hide, fill, ceil = SHOTS[n]
    cs, ts = S(c), S(tg)
    if isinstance(lens, tuple):
        camd.type = "ORTHO"; camd.ortho_scale = lens[1] * K; cam.location = cs; cam.rotation_euler = (0, 0, 0); scene.render.resolution_y = 760
    else:
        camd.type = "PERSP"; camd.lens = lens; cam.location = cs
        cam.rotation_euler = (ts - cs).to_track_quat("-Z", "Y").to_euler(); scene.render.resolution_y = 800
    camd.clip_start = 0.004 if n.startswith("ant") else 0.05
    camd.clip_end = 200
    dv = Vector((cs.x - ts.x, cs.y - ts.y, 0)); ln = dv.length or 1.0; dv /= ln
    for o, ctr in WALLS:
        o.hide_render = bool(hide) and (ctr - Vector((ts.x, ts.y, 0))).dot(dv) > 1.2
    for o in CEIL: o.hide_render = not ceil
    for o in col.objects:
        if o.name.startswith("SM_Y_Tree"): o.hide_render = bool(hide) or n == "top"
    if extra: bpy.data.objects.remove(extra); extra = None
    if fill: extra = light("POINT", cs + Vector((0, 0, 0.4)), fill * (1.0 if TOD == "day" else 0.6), color=(1.0, 0.93, 0.85), soft=0.2)
    scene.render.filepath = os.path.join(DIR, "preview_%s_%s_%s.jpg" % (STATE, n, TOD))
    bpy.ops.render.render(write_still=True)
    print("RENDERED %s %.1fs" % (n, time.time() - t0))
print("PREVIEW_DONE instances=%d polygons=%d" % (len(col.objects), tris))
