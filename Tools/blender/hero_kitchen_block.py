# KAN-22 pass 2: the kitchen block (cabinets, doors, drawers, worktops, sink, fridge, range, hood, dishwasher, microwave,
# toaster, kettle, table, chairs, bin, radiator) brought up to the hero props' bar, through the same pipeline (hero_lib.py).
# Each piece starts from the owner's own model of it (SourceArt/Blender/kitchen|living|level_v2/<Name>.fbx, made by the
# project's kitchen scripts) and keeps its exact frame, origin and box (the level table, the hinges and the wreck still fit):
#   - high-poly: the model with rounded, hardened bevels (3 segments) - the bake source only;
#   - LOD0: the model with a small chamfer (what catches the light on an edge), <= 15k triangles; LOD1 / LOD2 decimated;
#   - its own procedural materials (painted wood, oiled butcher block, walnut, enamel, brushed steel, chrome, brass, cast
#     iron, glass, plastics) baked high -> low into T_<Name>_C/N/R, with grime where the AO collects it;
#   - UCX_ convex hulls, one per solid part of the model (the 16 largest);
#   - pre-cut break pieces where its class breaks (wood: cabinets, doors, drawers, worktops, table, chairs);
#   - a before / after sheet: Saved/art_review/v3/<Name>.jpg.
# Out: SourceArt/Blender/hero_kitchen/<Name>/ and hero_kitchen_manifest.json (imported to /Game/Hero/<Name>/<Name>).
# Run: blender --background --factory-startup --python hero_kitchen_block.py -- <out> <review dir> <names|all|group> [nobake]
import bpy, bmesh, sys, os, math, random, json, time
from mathutils import Vector, Matrix
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hero_lib as H

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else os.path.join(HERE, "..", "..", "SourceArt", "Blender", "hero_kitchen")
REV = argv[1] if len(argv) > 1 else os.path.join(HERE, "..", "..", "Saved", "art_review", "v3")
SEL = argv[2] if len(argv) > 2 else "all"
NOBAKE = len(argv) > 3 and argv[3] == "nobake"
# the owner's kitchen models (the main worktree keeps the source FBX of the base kitchen; level_v2 is in this one too)
SRC_ROOTS = [os.path.join(HERE, "..", "..", "SourceArt", "Blender"), r"C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen\SourceArt\Blender"]
MANIFEST = os.environ.get("HERO_MANIFEST", "hero_kitchen_manifest.json")

# name: (source folder, break class or None, texture size)
W, P, M_ = "wood", "plastic", None
PIECES = {
    # floor cabinets, their doors and drawers
    "SM_K_Base1400": ("kitchen", W, 2048), "SM_K_Base1400_DoorL": ("kitchen", W, 1024), "SM_K_Base1400_DoorR": ("kitchen", W, 1024),
    "SM_K_Base1300": ("kitchen", W, 2048), "SM_K_Base1300_DoorL": ("kitchen", W, 1024), "SM_K_Base1300_DoorR": ("kitchen", W, 1024),
    "SM_K_Base1200": ("kitchen", W, 2048), "SM_K_Base1200_DoorL": ("kitchen", W, 1024), "SM_K_Base1200_DoorR": ("kitchen", W, 1024),
    "SM_K_BaseDrawers": ("kitchen", W, 2048), "SM_K_BaseDrawers_Drawer1": ("kitchen", W, 1024),
    "SM_K_BaseDrawers_Drawer2": ("kitchen", W, 1024), "SM_K_BaseDrawers_Drawer3": ("kitchen", W, 1024),
    # the sink cabinet and the sink
    "SM_K_BaseSink": ("kitchen", W, 2048), "SM_K_BaseSink_DoorL": ("kitchen", W, 1024), "SM_K_BaseSink_DoorR": ("kitchen", W, 1024),
    "SM_K_Sink": ("kitchen", M_, 2048), "SM_K_Faucet": ("kitchen", M_, 1024), "SM_K_Faucet_TapHot": ("kitchen", M_, 512),
    "SM_K_Faucet_TapCold": ("kitchen", M_, 512), "SM_K_SinkTrap": ("kitchen", P, 512),
    # worktops
    "SM_K_WorktopA": ("kitchen", W, 4096), "SM_K_WorktopB": ("kitchen", W, 2048),
    # wall cabinets
    "SM_K_WallCab1250": ("kitchen", W, 2048), "SM_K_WallCab1250_DoorL": ("kitchen", W, 1024), "SM_K_WallCab1250_DoorR": ("kitchen", W, 1024),
    "SM_K_WallCab1500": ("kitchen", W, 2048), "SM_K_WallCab1500_DoorL": ("kitchen", W, 1024), "SM_K_WallCab1500_DoorR": ("kitchen", W, 1024),
    # fridge
    "SM_K_Fridge": ("kitchen", M_, 4096), "SM_K_Fridge_DoorMain": ("kitchen", M_, 2048), "SM_K_Fridge_DoorFreezer": ("kitchen", M_, 2048),
    "SM_K_Fridge_Crisper": ("kitchen", P, 1024),
    # stove / range, hood
    "SM_K_Range": ("kitchen", M_, 2048), "SM_K_Range_OvenDoor": ("kitchen", M_, 1024), "SM_K_Range_Knob": ("kitchen", M_, 512),
    "SM_K_Range_Burner": ("kitchen", M_, 1024), "SM_K_Range_Rack": ("kitchen", M_, 1024), "SM_K_Range_Tray": ("kitchen", M_, 1024),
    "SM_K_Hood": ("kitchen", M_, 2048),
    # dishwasher
    "SM_K_Dishwasher": ("kitchen", M_, 2048), "SM_K_Dishwasher_Door": ("kitchen", M_, 2048), "SM_K_Dishwasher_Rack": ("kitchen", M_, 1024),
    # small appliances
    "SM_K_Microwave": ("kitchen", M_, 2048), "SM_K_Microwave_Door": ("kitchen", M_, 1024), "SM_K_Microwave_Plate": ("kitchen", P, 512),
    "SM_K_Toaster": ("kitchen", M_, 1024), "SM_K_Toaster_Lever": ("kitchen", M_, 256), "SM_K_Kettle": ("kitchen", M_, 1024),
    # table (the chairs, the high chair and the radiators are the living set's: Tools/blender/hero_living.py, code/ANT-art-living)
    "SM_K_Table": ("kitchen", W, 4096),
    # bin
    "SM_K_Bin": ("kitchen", M_, 2048), "SM_K_Bin_Lid": ("kitchen", M_, 1024), "SM_K_Bin_Pedal": ("kitchen", M_, 512),
}
GROUPS = {
    "cabinets": [n for n in PIECES if n.startswith(("SM_K_Base1", "SM_K_BaseDrawers"))],
    "sink": [n for n in PIECES if n.startswith(("SM_K_BaseSink", "SM_K_Sink", "SM_K_Faucet"))],
    "worktops": ["SM_K_WorktopA", "SM_K_WorktopB"],
    "wallcabs": [n for n in PIECES if n.startswith("SM_K_WallCab")],
    "fridge": [n for n in PIECES if n.startswith("SM_K_Fridge")],
    "range": [n for n in PIECES if n.startswith(("SM_K_Range", "SM_K_Hood"))],
    "dishwasher": [n for n in PIECES if n.startswith("SM_K_Dishwasher")],
    "small": [n for n in PIECES if n.startswith(("SM_K_Microwave", "SM_K_Toaster", "SM_K_Kettle"))],
    "table": ["SM_K_Table"],
    "bin": [n for n in PIECES if n.startswith("SM_K_Bin")],
}

# ------------------------------------------------------------------------------------------------ materials of the block
def _noise(m, scale, detail=6, x=-900, y=0, vec=None):
    n = H.N(m, "ShaderNodeTexNoise", x, y); n.inputs["Scale"].default_value = scale; n.inputs["Detail"].default_value = detail
    if vec is not None: H.L(m, vec, n.inputs["Vector"])
    return n

def _coords(m, stretch=None):
    tc = H.N(m, "ShaderNodeTexCoord", -1300)
    if not stretch: return tc.outputs["Object"]
    mp = H.N(m, "ShaderNodeMapping", -1150); mp.inputs["Scale"].default_value = stretch
    H.L(m, tc.outputs["Object"], mp.inputs["Vector"]); return mp.outputs["Vector"]

def _rough_from(m, p, fac, lo, hi):
    mr = H.N(m, "ShaderNodeMapRange", -400, -200); mr.inputs["To Min"].default_value = lo; mr.inputs["To Max"].default_value = hi
    H.L(m, fac, mr.inputs["Value"]); H.L(m, mr.outputs["Result"], p.inputs["Roughness"])

def _bump(m, p, fac, strength, dist, x=-260, y=-420):
    bp = H.N(m, "ShaderNodeBump", x, y); bp.inputs["Strength"].default_value = strength; bp.inputs["Distance"].default_value = dist
    H.L(m, fac, bp.inputs["Height"]); H.L(m, bp.outputs["Normal"], p.inputs["Normal"]); return bp

def _tint(m, p, col, fac, amount):
    """colour with a slow mottling (+-amount): nothing real is one flat colour"""
    mx = H.N(m, "ShaderNodeMix", -300, 300); mx.data_type = "RGBA"
    mx.inputs[6].default_value = (col[0] * (1 - amount), col[1] * (1 - amount), col[2] * (1 - amount), 1)
    mx.inputs[7].default_value = (min(1, col[0] * (1 + amount)), min(1, col[1] * (1 + amount)), min(1, col[2] * (1 + amount)), 1)
    H.L(m, fac, mx.inputs[0]); H.L(m, mx.outputs[2], p.inputs["Base Color"])

def painted(name, col, rough=0.42):
    """painted (sprayed) cabinet wood: satin lacquer, faint brush / roller texture and the grain telegraphing through"""
    m, p = H.mat(name)
    v = _coords(m); vs = _coords(m, (1.0, 30.0, 1.0))
    mot = _noise(m, 3.0, 4, vec=v); fine = _noise(m, 300.0, 2, y=-250, vec=v); grain = _noise(m, 40.0, 8, y=-500, vec=vs)
    _tint(m, p, col, mot.outputs["Fac"], 0.05)
    _rough_from(m, p, mot.outputs["Fac"], rough - 0.06, rough + 0.10)
    add = H.N(m, "ShaderNodeMath", -500, -400); add.operation = "ADD"
    g = H.N(m, "ShaderNodeMath", -650, -500); g.operation = "MULTIPLY"; g.inputs[1].default_value = 1.5
    f2 = H.N(m, "ShaderNodeMath", -650, -300); f2.operation = "MULTIPLY"; f2.inputs[1].default_value = 0.3
    H.L(m, grain.outputs["Fac"], g.inputs[0]); H.L(m, fine.outputs["Fac"], f2.inputs[0]); H.L(m, f2.outputs[0], add.inputs[0]); H.L(m, g.outputs[0], add.inputs[1])
    _bump(m, p, add.outputs[0], 0.03, 0.0003)
    p.inputs["Coat Weight"].default_value = 0.2; p.inputs["Coat Roughness"].default_value = 0.3
    return H.tag(m, 0.0, 0.25)

def enamel(name, col, rough=0.16):
    """stove enamel / appliance paint: glossy with an orange-peel ripple and faint mottling"""
    m, p = H.mat(name)
    v = _coords(m)
    mot = _noise(m, 4.0, 3, vec=v); peel = _noise(m, 700.0, 2, y=-250, vec=v)
    _tint(m, p, col, mot.outputs["Fac"], 0.03)
    _rough_from(m, p, mot.outputs["Fac"], rough - 0.04, rough + 0.12)
    _bump(m, p, peel.outputs["Fac"], 0.05, 0.0003)
    p.inputs["Coat Weight"].default_value = 0.5; p.inputs["Coat Roughness"].default_value = 0.08
    return H.tag(m, 0.0, 0.05)

def brushed(name, col=(0.72, 0.73, 0.75), rough=0.3, along=(1.0, 220.0, 220.0)):
    """brushed stainless: fine streaks along one axis (X by default), fingerprints and dull spots"""
    m, p = H.mat(name); p.inputs["Metallic"].default_value = 1.0
    v = _coords(m); vs = _coords(m, along)
    st = _noise(m, 6.0, 10, vec=vs); spots = _noise(m, 12.0, 4, y=-250, vec=v)
    _tint(m, p, col, st.outputs["Fac"], 0.04)
    add = H.N(m, "ShaderNodeMath", -600, -200); add.operation = "ADD"
    H.L(m, st.outputs["Fac"], add.inputs[0]); H.L(m, spots.outputs["Fac"], add.inputs[1])
    _rough_from(m, p, add.outputs[0], rough - 0.12, rough + 0.12)
    _bump(m, p, st.outputs["Fac"], 0.06, 0.0002)
    return H.tag(m, 1.0, 0.0)

def castiron(name, col=(0.045, 0.043, 0.042), rough=0.72):
    m, p = H.mat(name)
    v = _coords(m)
    mot = _noise(m, 30.0, 6, vec=v); pit = _noise(m, 600.0, 2, y=-250, vec=v)
    _tint(m, p, col, mot.outputs["Fac"], 0.25)
    _rough_from(m, p, mot.outputs["Fac"], rough - 0.1, rough + 0.12)
    _bump(m, p, pit.outputs["Fac"], 0.25, 0.0004)
    p.inputs["Metallic"].default_value = 0.4
    return H.tag(m, 0.4, 0.05)

def glassy(name, col, rough=0.05):
    m, p = H.mat(name); p.inputs["Base Color"].default_value = (*col, 1); p.inputs["Roughness"].default_value = rough
    p.inputs["Coat Weight"].default_value = 0.6
    v = _coords(m); sm = _noise(m, 25.0, 3, vec=v)
    _rough_from(m, p, sm.outputs["Fac"], rough, rough + 0.08)                  # smudges
    return H.tag(m, 0.0, 0.0)

def matte(name, col, rough=0.85, por=0.6):
    m, p = H.mat(name)
    v = _coords(m); mot = _noise(m, 8.0, 4, vec=v); fine = _noise(m, 500.0, 3, y=-250, vec=v)
    _tint(m, p, col, mot.outputs["Fac"], 0.06)
    _rough_from(m, p, mot.outputs["Fac"], rough - 0.08, min(1.0, rough + 0.08))
    _bump(m, p, fine.outputs["Fac"], 0.15, 0.0004)
    return H.tag(m, 0.0, por)

LIB = {}
def look(slot, rgb):
    """the hero material for an old slot name (the kitchen's palette: the manifest's colours)"""
    key = slot
    if key in LIB: return LIB[key]
    s = slot.lower()
    if s == "cabinet": m = painted("Hero_Cabinet", tuple(rgb))
    elif s == "cabinetinset": m = painted("Hero_CabinetInset", tuple(rgb), 0.46)   # (the owner's palette: the inside is white melamine)
    elif s == "butcher": m = H.wood("Hero_Butcher", light=(0.70, 0.43, 0.19), dark=(0.40, 0.22, 0.09), grain=16, varnish=0.15)
    elif s == "walnut": m = H.wood("Hero_Walnut", light=(0.36, 0.22, 0.12), dark=(0.14, 0.08, 0.04), grain=20, varnish=0.35)
    elif s == "plinth": m = H.plastic("Hero_Plinth", (0.06, 0.06, 0.065), 0.6, peel=0.08, scuff=0.25)
    elif s == "brass": m = H.metal("Hero_Brass", (0.86, 0.62, 0.24), 0.3)
    elif s == "steel": m = brushed("Hero_Steel")
    elif s == "chrome": m = H.metal("Hero_Chrome", (0.90, 0.90, 0.92), 0.07)
    elif s == "iron" or s == "burnerring" or s == "heatglow": m = castiron("Hero_Iron")
    elif s == "enamel": m = enamel("Hero_Enamel", (0.92, 0.91, 0.87))
    elif s == "fridgered": m = enamel("Hero_FridgeRed", (0.60, 0.07, 0.05), 0.14)
    elif s == "fridgeliner": m = H.plastic("Hero_Liner", (0.90, 0.90, 0.87), 0.3, peel=0.03, scuff=0.12)
    elif s == "ovenliner": m = enamel("Hero_OvenLiner", (0.07, 0.07, 0.08), 0.3)
    elif s == "shelfglass": m = glassy("Hero_ShelfGlass", (0.50, 0.68, 0.70))
    elif s == "blackglass": m = glassy("Hero_BlackGlass", (0.015, 0.015, 0.018))
    elif s in ("fabric", "paper", "leaf", "teal", "sisal", "cardboard"): m = matte("Hero_" + slot, tuple(rgb), 0.85, 0.8)
    else: m = H.plastic("Hero_" + slot, tuple(rgb), 0.38)
    LIB[key] = m
    return m

# ------------------------------------------------------------------------------------------------ one piece
def find_src(name, folder):
    for r in SRC_ROOTS:
        f = os.path.join(r, folder, name + ".fbx")
        if os.path.exists(f): return f
    return None

def load_old(name, folder):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=find_src(name, folder))
    objs = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    for o in [o for o in bpy.data.objects if o not in before and o.type != "MESH"]: bpy.data.objects.remove(o)
    o = H.join(objs, name + "_OLD") if len(objs) > 1 else objs[0]
    for x in S_objs(): x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    o.name = name + "_OLD"
    return o

def S_objs(): return list(H.S().objects)

def old_colours(o, mats):
    """the old flat look (the 'before' render): the manifest colour of each slot"""
    for s in o.material_slots:
        nm = s.material.name.split(".")[0] if s.material else "?"
        m = bpy.data.materials.get("Old_" + nm)
        if not m:
            m, p = H.mat("Old_" + nm); rgb = mats.get(nm, {}).get("rgb", (0.7, 0.7, 0.7))
            p.inputs["Base Color"].default_value = (*rgb, 1); p.inputs["Roughness"].default_value = mats.get(nm, {}).get("roughness", 0.5)
            p.inputs["Metallic"].default_value = mats.get(nm, {}).get("metallic", 0)
        s.material = m

def bevelled(src, name, width, segs, harden):
    o = src.copy(); o.data = src.data.copy(); o.name = name; o.data.name = name; H.link(o)
    for p in o.data.polygons: p.use_smooth = True
    b = o.modifiers.new("bev", "BEVEL"); b.width = width; b.segments = segs; b.limit_method = "ANGLE"; b.angle_limit = math.radians(35)
    b.use_clamp_overlap = True; b.harden_normals = harden; b.miter_outer = "MITER_ARC"
    if not harden:
        try: b.profile = 0.5
        except Exception: pass
    return H.bake_mesh(o, name)

def smooth_by_angle(o, deg=32.0):
    for x in S_objs(): x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    try: bpy.ops.object.shade_smooth_by_angle(angle=math.radians(deg))
    except Exception:
        for p in o.data.polygons: p.use_smooth = True

def island_hulls(o, name, keep=16):
    c = o.copy(); c.data = o.data.copy(); H.link(c)
    for x in S_objs(): x.select_set(False)
    c.select_set(True); bpy.context.view_layer.objects.active = c
    bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="SELECT"); bpy.ops.mesh.separate(type="LOOSE"); bpy.ops.object.mode_set(mode="OBJECT")
    isl = [x for x in bpy.context.selected_objects]
    mn, mx = H.bounds([o]); total = max(1e-9, (mx - mn).x * (mx - mn).y * (mx - mn).z)
    sized = []
    for x in isl:
        a, b = H.bounds([x]); d = b - a; vol = max(d.x, 0.002) * max(d.y, 0.002) * max(d.z, 0.002)
        sized.append((vol, x))
    sized.sort(key=lambda t: -t[0])
    hs = []
    for vol, x in sized:
        if len(hs) < keep and vol > total * 0.002 and len(x.data.vertices) >= 4:
            hs.append(H.hull("UCX_%s_%02d" % (name, len(hs)), [tuple(x.matrix_world @ v.co) for v in x.data.vertices]))
        bpy.data.objects.remove(x)
    if not hs:
        hs.append(H.hull("UCX_%s_00" % name, H.box_pts(mn.x, mx.x, mn.y, mx.y, mn.z, mx.z)))
    return hs

def split2(o, co, no, size, jag, seed, keep_name, other_name):
    """o cut by a ragged plane into two pieces, each by a DIFFERENCE (the old models are not all closed: an INTERSECT of an
    open shell comes back empty)"""
    b = o.copy(); b.data = o.data.copy(); H.link(b)
    a, _x = H.split(o, H.cutter(co, no, size, jag, seed, sub=6, splinter=jag * 0.6, depth=size), keep_name, "_tmp")
    if _x and _x.name in bpy.data.objects: bpy.data.objects.remove(_x)
    nn = [-v for v in no]
    b, _y = H.split(b, H.cutter(co, nn, size, jag, seed, sub=6, splinter=jag * 0.6, depth=size), other_name, "_tmp2")
    if _y and _y.name in bpy.data.objects: bpy.data.objects.remove(_y)
    return a, b

def wood_breaks(lo, name, seed):
    """a cabinet / door / board splits along the grain with splinters: two ragged cuts -> up to 3 pieces"""
    mn, mx = H.bounds([lo]); c = (mn + mx) * 0.5; d = mx - mn; size = max(d.x, d.y, d.z) * 3
    R = random.Random(seed)
    src = lo.copy(); src.data = lo.data.copy(); src.name = name + "_Brk0"; H.link(src)
    long = max(range(3), key=lambda i: d[i])                       # cut across the longest side
    no = [0.0, 0.0, 0.0]; no[long] = 1.0; no[(long + 1) % 3] = R.uniform(-0.25, 0.25)
    co = [c.x, c.y, c.z]; co[long] = mn[long] + d[long] * R.uniform(0.38, 0.55)
    jag = max(0.002, min(0.012, d[long] * 0.02))
    a, b = split2(src, co, no, size, jag, seed, name + "_Brk0", name + "_Brk1")
    out = [a, b]
    if d[long] > 0.5 and len(a.data.polygons):
        co2 = list(co); co2[long] = mn[long] + d[long] * R.uniform(0.7, 0.8)
        no2 = list(no); no2[(long + 2) % 3] = R.uniform(-0.2, 0.2)
        e, a = split2(a, co2, no2, size, jag, seed + 7, name + "_Brk2", name + "_Brk0")
        out = [a, b, e]
    return [x for x in out if len(x.data.polygons)]

def review(name, old, lo, prev, path):
    """before | after, the same camera and studio"""
    for x in S_objs():
        if x.type in ("LIGHT", "CAMERA") or (x.type == "MESH" and x not in (old, lo)): bpy.data.objects.remove(x)
    mn, mx = H.bounds([lo]); c = (mn + mx) * 0.5; s = max((mx - mn).length, 0.15)
    base = Matrix.Translation((-c.x, -c.y, -mn.z))
    H.stage(wall_y=(mx.y - c.y) + 0.02 + s * 0.3)
    lo.data.materials.clear(); lo.data.materials.append(prev)
    shots = []
    for o, tag in ((old, "before"), (lo, "after")):
        old.hide_render = o is not old; lo.hide_render = o is not lo
        o.matrix_world = base
        H.camera((0.55 * s, -1.55 * s, (mx.z - mn.z) * 0.6 + 0.22 * s), (0, 0, (mx.z - mn.z) * 0.45), 35, 11)
        p = os.path.join(REV, "_%s_%s.jpg" % (name, tag)); H.render(p, 960, 640, 64); shots.append(p)
    H.sheet(shots, path, 2)
    for p in shots:
        try: os.remove(p)
        except Exception: pass

def produce(name, folder, brk, size, mats):
    t0 = time.time()
    out = os.path.join(OUT, name); os.makedirs(out, exist_ok=True)
    old = load_old(name, folder)
    mn, mx = H.bounds([old]); d = mx - mn; small = max(0.02, min(d.x, d.y, d.z) if min(d.x, d.y, d.z) > 0.004 else sorted((d.x, d.y, d.z))[1])
    slots = [s.material.name.split(".")[0] if s.material else "Default" for s in old.material_slots]
    # the hero look per old slot, on a working copy (the old stays with its flat colours for the before render)
    work = old.copy(); work.data = old.data.copy(); H.link(work); work.name = name + "_W"
    for i, s in enumerate(work.material_slots):
        s.material = look(slots[i], mats.get(slots[i], {}).get("rgb", (0.7, 0.7, 0.7)))
    old_colours(old, mats)
    bw = max(0.0006, min(0.004, small * 0.12))
    hi = bevelled(work, name + "_HI", bw, 3, True)
    lo = bevelled(work, name, bw * 0.6, 1, False)
    bpy.data.objects.remove(work)
    if H.tris(lo) > 15000:
        dd = lo.modifiers.new("dec", "DECIMATE"); dd.ratio = 14000.0 / H.tris(lo); dd.use_collapse_triangulate = True
        lo = H.bake_mesh(lo, name)
    tr = lo.modifiers.new("tri", "TRIANGULATE"); tr.keep_custom_normals = True
    lo = H.bake_mesh(lo, name)
    lo.name = name; lo.data.name = name                            # (UCX_<name>_NN must name the mesh exactly)
    smooth_by_angle(lo, 32.0)
    H.unwrap(lo, 50.0, 0.003)
    tdir = os.path.join(out, "tex")
    if NOBAKE and os.path.exists(os.path.join(tdir, "T_%s_C.png" % name)):
        paths = {k: os.path.join(tdir, "T_%s_%s.png" % (name, k)) for k in "CNR"}
    else:
        paths = H.bake_set(lo, name, tdir, size, 32, hi=hi, ext=max(0.0015, bw * 1.5), grime=0.3, ao_dist=max(0.015, min(0.035, d.length * 0.03)))
    bpy.data.objects.remove(hi)
    H.game_material(lo, name)
    hs = island_hulls(lo, name)
    l1 = H.lod(lo, 0.5, name + "_LOD1"); l2 = H.lod(lo, 0.25, name + "_LOD2")
    H.export_fbx([lo] + hs, os.path.join(out, name + ".fbx"))
    H.export_fbx([l1], os.path.join(out, name + "_LOD1.fbx")); H.export_fbx([l2], os.path.join(out, name + "_LOD2.fbx"))
    man = {"name": name, "file": name + ".fbx", "set": "hero_kitchen", "dest": "/Game/Hero/" + name, "break_class": brk or "",
           "textures": {k: os.path.relpath(v, OUT).replace("\\", "/") for k, v in paths.items()}, "collision_lod": 0, "movers": [], "breaks": [],
           "source": "%s/%s.fbx (the owner's kitchen model, same frame)" % (folder, name), "old_slots": slots,
           "note": "KAN-22 pass 2: the kitchen block on the hero bar (bevels and materials baked high -> low, grime, UCX per part, break pieces)"}
    man.update(H.info(lo)); man["lod_triangles"] = [H.tris(lo), H.tris(l1), H.tris(l2)]; man["ucx"] = len(hs)
    pieces = []
    if brk == W:
        try:
            pieces = wood_breaks(lo, name, len(name))
        except Exception as e:
            print("HERO break FAILED", name, e)
        for pc in pieces:
            H.break_slots(pc)
            H.export_fbx([pc], os.path.join(out, pc.name + ".fbx"))
            man["breaks"].append({"name": pc.name, "file": pc.name + ".fbx", **H.info(pc)})
    for o in hs + [l1, l2] + pieces: bpy.data.objects.remove(o)
    prev = H.preview_material(name, paths)
    os.makedirs(REV, exist_ok=True)
    review(name, old, lo, prev, os.path.join(REV, name + ".jpg"))
    print("HERO %s: LOD %s, %d hulls, %d break pieces, %.0f s" % (name, man["lod_triangles"], man["ucx"], len(man["breaks"]), time.time() - t0))
    return man

def main():
    t0 = time.time()
    H.reset()
    os.makedirs(OUT, exist_ok=True)
    lv = json.load(open(os.path.join(SRC_ROOTS[0], "level_v3", "level_v3_manifest.json"), encoding="utf-8"))
    mats = {}
    for r in reversed(SRC_ROOTS):                                   # (the base kitchen's own palette has the slots v3 lacks: FridgeRed...)
        for f in ("kitchen/kitchen_manifest.json", "living/living_manifest.json", "level_v2/level_v2_manifest.json"):
            pth = os.path.join(r, f)
            if os.path.exists(pth):
                try: mats.update(json.load(open(pth, encoding="utf-8")).get("materials", {}))
                except Exception: pass
    mats.update(lv["materials"])
    names = list(PIECES) if SEL == "all" else sum([GROUPS.get(s, [s]) for s in SEL.replace("+", ",").split(",")], [])
    mpath = os.path.join(OUT, MANIFEST)
    for name in names:
        folder, brk, size = PIECES[name]
        if not find_src(name, folder):
            print("HERO MISSING source", name); continue
        H.reset(); LIB.clear()
        H.plastic("Break", (0.8, 0.8, 0.78), 0.7)
        try:
            man = produce(name, folder, brk, size, mats)
        except Exception as e:
            import traceback; traceback.print_exc(); print("HERO FAILED", name, e); continue
        doc = json.load(open(mpath, encoding="utf-8")) if os.path.exists(mpath) else {"pieces": []}
        doc["pieces"] = [p for p in doc["pieces"] if p["name"] != man["name"]] + [man]
        doc["note"] = "Kitchen hero art (Tools/blender/hero_kitchen_a.py, hero_kitchen_sink.py, hero_kitchen_block.py): -> /Game/Hero/<Name>/<Name>"
        json.dump(doc, open(mpath, "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    print("HERO_DONE %.0f s" % (time.time() - t0))

main()
