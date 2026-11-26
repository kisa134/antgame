# KAN-22: the mop, the bucket and the broom as game assets, from the owner's approved reference (Saved/hero_mop_bucket.py, 26.11).
# Each replaces the piece of the same name and keeps its game frame (origin, +Y = the wall behind it, metres):
#   SM_K_Bucket  20 l bucket, 342 mm tall (the old bucket's height: the water box, the loot spot and the wet zone stay right)
#                + SM_K_Bucket_Handle (swings on its pins: origin = the pin axis, along X)
#   SM_K_Mop     real 1.47 m mop; built upright, the yarn is laid on the floor of the LEANED mop (the table leans it 12 deg to the wall)
#   SM_V31_Broom long-handled sweeping broom in the box of the v3.1 broom (head on the floor at -Y, handle tip on the west wall)
# Run: blender --background --factory-startup --python hero_kitchen_a.py -- <SourceArt/Blender/hero> <Saved/art_review> [names] [nobake]
import bpy, bmesh, sys, os, math, random, json, time
from mathutils import Vector, Matrix
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hero_lib as H

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else os.path.join(HERE, "..", "..", "SourceArt", "Blender", "hero")
REV = argv[1] if len(argv) > 1 else os.path.join(HERE, "..", "..", "Saved", "art_review")
ONLY = argv[2].replace("+", ",").split(",") if len(argv) > 2 and argv[2] not in ("all", "") else None
NOBAKE = len(argv) > 3 and argv[3] == "nobake"
LEAN = 12.0                      # the placement table leans the mop this much towards +Y (the north wall)
F = 0.342 / 0.274                # the reference bucket scaled to the old bucket's height

# ------------------------------------------------------------------------------------------------ the bucket
def bucket_parts():
    f = F
    prof = [(0, 0.004), (0.090, 0.004), (0.098, 0.000), (0.103, 0.006), (0.1045, 0.014),
            (0.128, 0.200), (0.1335, 0.203), (0.1335, 0.210), (0.1295, 0.213),
            (0.1325, 0.232), (0.138, 0.235), (0.138, 0.242), (0.134, 0.245),
            (0.1365, 0.258), (0.1445, 0.262), (0.1470, 0.268), (0.1445, 0.274), (0.1380, 0.274),
            (0.1330, 0.268), (0.1010, 0.016), (0.094, 0.010), (0, 0.010)]
    prof = [(r * f, z * f) for r, z in prof]
    blue = H.plastic("PlasticBlue", (0.05, 0.16, 0.55))
    body = H.screw("Bucket", prof, 72, 1); body.data.materials.append(blue)
    parts = [(body, 0)]
    zinc = H.metal("Zinc", (0.6, 0.61, 0.62), 0.35)
    for s in (-1, 1):
        lug = H.cube("Lug", (0.012 * f, 0.030 * f, 0.034 * f), (s * 0.1415 * f, 0, 0.243 * f)); H.smooth(lug, 2, 0.004)
        lug.data.materials.append(blue); parts.append((lug, 0))
        pin = H.cyl("Pin", 0.0045 * f, 0.012 * f, (s * 0.150 * f, 0, 0.246 * f), (0, math.pi / 2, 0), 16); pin.data.materials.append(zinc); parts.append((pin, 0))
    # the handle, modelled standing up over the bucket (the game swings it about the pin axis)
    zp = 0.246 * f; ax = 0.156 * f
    pts = [(ax, 0, zp), (ax - 0.006, 0, zp + 0.012)]
    for i in range(1, 12):
        a = math.pi * i / 12.0
        pts.append(((ax - 0.004) * math.cos(a), 0, zp + 0.215 * math.sin(a)))
    pts += [(-ax + 0.006, 0, zp + 0.012), (-ax, 0, zp)]
    wire = H.tubes("Handle", [pts], 0.0028, res_u=10, bevel_res=3, tip=1.0)
    wire.data.materials.append(H.metal("Wire", (0.75, 0.76, 0.78), 0.22))
    grip = H.cyl("Grip", 0.0125, 0.13, (0, 0, zp + 0.215), (0, math.pi / 2, 0), 24); H.smooth(grip, 1, 0.003)
    grip.data.materials.append(H.plastic("GripWhite", (0.85, 0.85, 0.82), 0.5))
    parts += [(wire, 2), (grip, 2)]
    return parts, zp

def bucket_hulls(name):
    f = F; hs = []
    rb_o, rb_i, zb = 0.1045 * f, 0.101 * f, 0.014 * f
    rt_o, rt_i, zt = 0.1470 * f, 0.1330 * f, 0.274 * f
    hs.append(H.hull("UCX_%s_00" % name, H.ring_pts(0.103 * f, 0.0, 16) + H.ring_pts(0.101 * f, 0.012 * f, 16)))
    n = 8
    for k in range(n):
        pts = []
        for a in (2 * math.pi * k / n, 2 * math.pi * (k + 1) / n):
            c, s = math.cos(a), math.sin(a)
            for r, z in ((rb_o, zb), (rb_i, zb), (rt_o, zt), (rt_i, zt)):
                pts.append((r * c, r * s, z))
        hs.append(H.hull("UCX_%s_%02d" % (name, k + 1), pts))
    return hs

# ------------------------------------------------------------------------------------------------ the mop
# Where it stands (KAN-22 placement, game units): base (6600, 4400), the bucket at (6200, 3900), the east wall x 6970,
# the north wall y 5000 (the stick's top rests on it). Blender +X = game -X, +Y = game +Y. The yarn keeps out of them.
BUCKET_XY, BUCKET_R = Vector((400 * H.K, -500 * H.K)), 0.1835 + 0.012
WALL_E, WALL_N = -365 * H.K, 585 * H.K
H0 = 0.205
STICK = 1.18

def mop_parts(seed=7):
    R = random.Random(seed)
    lean = Matrix.Rotation(math.radians(-LEAN), 4, "X")            # local -> world (the table's lean)
    unlean = lean.inverted()
    parts = []
    stick = H.cyl("Stick", 0.0125, STICK, (0, 0, H0 + 0.07 + STICK / 2), seg=32); stick.data.materials.append(H.wood("Beech"))
    cap = H.screw("Cap", [(0, 0.012), (0.009, 0.011), (0.0128, 0.006), (0.0128, 0)], 32, 1); cap.location = (0, 0, H0 + 0.07 + STICK); cap.data.materials.append(stick.data.materials[0])
    parts += [(stick, 1), (cap, 1)]
    red = H.plastic("SocketRed", (0.62, 0.05, 0.05), 0.4)
    so = H.screw("Socket", [(0, 0.150), (0.0132, 0.150), (0.0150, 0.146), (0.0165, 0.070), (0.0185, 0.066), (0.0185, 0.058), (0.0165, 0.054), (0.0205, 0.020), (0.0440, 0.004), (0.0455, 0.000), (0.0440, -0.012), (0.0, -0.012)], 48, 1)
    so.location = (0, 0, H0); so.data.materials.append(red); parts.append((so, 0))
    for k in range(6):
        a = math.pi * 2 * k / 6.0
        rg = H.cyl("Ridge", 0.0022, 0.060, (0.0178 * math.cos(a), 0.0178 * math.sin(a), H0 + 0.105), seg=8); rg.data.materials.append(red); parts.append((rg, 0))
    band = H.screw("Band", [(0.0462, 0.002), (0.0472, 0.0015), (0.0472, -0.0105), (0.0462, -0.011)], 48, 0); band.location = (0, 0, H0); band.data.materials.append(H.metal("Band", (0.72, 0.73, 0.74), 0.3))
    parts.append((band, 0))
    # the yarn: designed on the floor in the world, then taken into the mop's own (upright) frame
    c = lean @ Vector((0, 0, H0 - 0.008)); cxy = Vector((c.x, c.y))
    def ok(p):
        return (p - BUCKET_XY).length > BUCKET_R and p.x > WALL_E and p.y < WALL_N
    strands, tips = [], []
    for i in range(130):
        a = R.uniform(0, math.pi * 2); r0 = 0.040 * math.sqrt(R.random())
        reach = R.uniform(0.06, 0.15); tail = R.uniform(0.02, 0.075)
        d = Vector((math.cos(a), math.sin(a))); side = Vector((-d.y, d.x)) * R.uniform(-0.05, 0.05)
        for _ in range(12):                                         # shorter where it would run into the bucket or a wall
            end = cxy + d * (r0 + reach + tail) + side * 3.0
            mid = cxy + d * (r0 + reach) + side
            if ok(end) and ok(mid): break
            reach *= 0.85; tail *= 0.8
        zf = 0.0045 + R.uniform(0, 0.012) * (1 if R.random() < 0.3 else 0)
        s0 = lean @ Vector((r0 * math.cos(a), r0 * math.sin(a), H0 - 0.008))
        s1 = lean @ Vector((r0 * math.cos(a) + d.x * 0.012, r0 * math.sin(a) + d.y * 0.012, H0 - 0.043))
        def w(v2, z): return Vector((v2.x, v2.y, z))
        pts = [s0, s1, w(cxy + d * (r0 + reach * 0.45) + side * 0.3, H0 * 0.42), w(cxy + d * (r0 + reach * 0.85) + side * 0.7, 0.03),
               w(cxy + d * (r0 + reach) + side, zf), w(cxy + d * (r0 + reach + tail * 0.6) + side * 1.8, zf),
               w(cxy + d * (r0 + reach + tail) + side * 3.0 + Vector((R.uniform(-.02, .02), R.uniform(-.02, .02))), zf)]
        strands.append([unlean @ p for p in pts]); tips.append(unlean @ pts[-1])
    y = H.tubes("Yarn", strands, 0.0042, res_u=4, bevel_res=1, tip=0.75)
    y.data.materials.append(H.fibre("Yarn"))
    parts.append((y, 0))
    return parts, tips

def mop_hulls(name, tips):
    hs = [H.hull("UCX_%s_00" % name, H.ring_pts(0.0125, H0 + 0.07, 10) + H.ring_pts(0.0125, H0 + 0.07 + STICK + 0.012, 10)),
          H.hull("UCX_%s_01" % name, H.ring_pts(0.046, H0 - 0.012, 12) + H.ring_pts(0.016, H0 + 0.15, 8))]
    hs.append(H.hull("UCX_%s_02" % name, [tuple(t) for t in tips] + H.ring_pts(0.045, H0 - 0.01, 8)))
    return hs

# ------------------------------------------------------------------------------------------------ the broom
# The v3.1 broom's box (uu): x -300..300, y -1427..1427, z 0..1885. Head on the floor at -Y, the handle rises along the wall (+X is the wall).
BROOM_TIP = Vector((0.150, 0.735, 0.985))
BROOM_BASE = Vector((0.0, -0.575, 0.150))

def broom_parts(seed=11):
    R = random.Random(seed)
    parts = []
    blk = H.wood("BlockLacquer", light=(0.16, 0.30, 0.20), dark=(0.07, 0.15, 0.09), grain=30, varnish=0.6)   # green-lacquered beech block
    head = H.cube("Block", (0.300, 0.068, 0.040), (0, -0.640, 0.125)); H.smooth(head, 1, 0.008); head.data.materials.append(blk)
    parts.append((head, 0))
    # tufts: 4 rows x 23, each a tapered bundle; a few loose fibres
    bris = H.fibre("Bristle", light=(0.80, 0.62, 0.36), dark=(0.55, 0.38, 0.20), dirt=(0.32, 0.26, 0.20), floor_h=0.05, twist=2400)
    strands = []
    for row in range(4):
        for k in range(23):
            x = -0.135 + k * (0.27 / 22) + R.uniform(-0.002, 0.002); yy = -0.640 - 0.024 + row * 0.016
            top = Vector((x, yy, 0.106))
            spread = Vector((x * 0.12 + R.uniform(-0.006, 0.006), (row - 1.5) * 0.006 + R.uniform(-0.005, 0.005), 0))
            bend = Vector((0, R.uniform(-0.004, 0.012), 0))
            strands.append([top, top + Vector((0, 0, -0.035)) + spread * 0.3, top + Vector((0, 0, -0.07)) + spread * 0.7 + bend, Vector((top.x, top.y, 0.004)) + spread + bend * 1.6])
    tufts = H.tubes("Tufts", strands, 0.0034, res_u=3, bevel_res=1, tip=0.85); tufts.data.materials.append(bris)
    loose = []
    for k in range(40):
        x = R.uniform(-0.14, 0.14); yy = -0.640 + R.uniform(-0.03, 0.03)
        loose.append([Vector((x, yy, 0.105)), Vector((x + R.uniform(-0.01, 0.01), yy + R.uniform(-0.01, 0.02), 0.05)), Vector((x + R.uniform(-0.02, 0.02), yy + R.uniform(-0.015, 0.03), 0.002))])
    fibres = H.tubes("Fibres", loose, 0.0011, res_u=3, bevel_res=0, tip=0.6); fibres.data.materials.append(bris)
    parts += [(tufts, 0), (fibres, 0)]
    # the handle: threaded into a metal ferrule set in the block at the handle's angle
    axis = (BROOM_TIP - BROOM_BASE).normalized(); ln = (BROOM_TIP - BROOM_BASE).length
    q = Vector((0, 0, 1)).rotation_difference(axis)
    def along(o, t):                                               # place a Z-aligned object along the handle at distance t
        o.rotation_mode = "QUATERNION"; o.rotation_quaternion = q; o.location = BROOM_BASE + axis * t
    beech = H.wood("HandleBeech", light=(0.70, 0.50, 0.28), dark=(0.42, 0.25, 0.12), grain=24, varnish=0.45)
    hd = H.cyl("Handle", 0.0115, ln - 0.06, (0, 0, 0), seg=28); along(hd, 0.06 + (ln - 0.06) / 2); hd.data.materials.append(beech)
    fe = H.screw("Ferrule", [(0, 0.075), (0.0128, 0.075), (0.0135, 0.070), (0.0135, 0.004), (0.0150, 0.0), (0.0150, -0.012), (0, -0.012)], 32, 1); along(fe, 0.0)
    fe.data.materials.append(H.metal("Ferrule", (0.70, 0.70, 0.72), 0.3))
    cp = H.screw("HangCap", [(0, 0.030), (0.0105, 0.028), (0.0125, 0.020), (0.0125, 0.0)], 24, 1); along(cp, ln - 0.02)
    cp.data.materials.append(H.plastic("CapRed", (0.62, 0.06, 0.05), 0.45))
    ring = H.cyl("HangHole", 0.006, 0.03, (0, 0, 0), (0, math.pi / 2, 0), 12); ring.location = BROOM_BASE + axis * (ln - 0.002); ring.data.materials.append(H.rubber("Hole"))
    parts += [(hd, 1), (fe, 0), (cp, 1), (ring, 1)]
    return parts, (BROOM_BASE, axis, ln)

def broom_hulls(name, h):
    base, axis, ln = h
    hs = [H.hull("UCX_%s_00" % name, H.box_pts(-0.155, 0.155, -0.678, -0.602, 0.0, 0.146))]
    q = Vector((0, 0, 1)).rotation_difference(axis).to_matrix()
    pts = [tuple(base + q @ Vector(p)) for p in H.ring_pts(0.0125, 0.0, 8) + H.ring_pts(0.0125, ln, 8)]
    hs.append(H.hull("UCX_%s_01" % name, pts))
    return hs

# ------------------------------------------------------------------------------------------------ break pieces (AntWreck)
def _copy(o, name):
    c = o.copy(); c.data = o.data.copy(); c.name = name; c.data.name = name; H.link(c); return c

def bucket_breaks(lod0, name):
    """plastic: the wall splits along two ragged cracks and the bottom tears off -> 4 pieces"""
    src = _copy(lod0, name + "_Brk0")
    a, b = H.split(src, H.cutter((0.015, 0.01, 0.17), (1.0, 0.35, 0.12), 1.2, 0.010, 3), name + "_Brk0", name + "_Brk1")
    a, c = H.split(a, H.cutter((0.0, 0.02, 0.15), (0.25, 1.0, 0.2), 1.2, 0.012, 5), name + "_Brk0", name + "_Brk2")
    b, d = H.split(b, H.cutter((0.0, 0.0, 0.07), (0.12, 0.06, 1.0), 1.2, 0.008, 9), name + "_Brk1", name + "_Brk3")
    return [x for x in (a, b, c, d) if len(x.data.polygons)]

def snap(lod0, name, handle_part, co, no):
    """wood: the stick snaps with splinters; lower piece keeps everything else"""
    stick = H.keep_parts(lod0, [handle_part], name + "_Brk1")
    rest = H.keep_parts(lod0, [p for p in set(a.value for a in lod0.data.attributes["part"].data) if p != handle_part], name + "_Brk0")
    n0 = len(stick.data.polygons)
    up, low = H.split(stick, H.cutter(co, no, 0.2, 0.004, 21, sub=6, splinter=0.007, depth=3.0), name + "_Brk1", name + "_low")
    print("HERO snap %s: stick %d faces -> up %d, low %d" % (name, n0, len(up.data.polygons), len(low.data.polygons)))
    low = H.join([rest, low], name + "_Brk0")
    return [low, up]

def mop_breaks(lod0, name):
    h = H0 + 0.07 + STICK * 0.62
    return snap(lod0, name, 1, (0, 0, h), (0, 0.25, 1.0))

def broom_breaks(lod0, name):
    ax = (BROOM_TIP - BROOM_BASE).normalized()
    return snap(lod0, name, 1, BROOM_BASE + ax * 0.85, ax + Vector((0.15, 0, 0.1)))

# ------------------------------------------------------------------------------------------------ review renders
def show(o, m, at=None):
    o.data.materials.clear(); o.data.materials.append(m)
    if len(o.data.materials) and o.data.polygons:
        for p in o.data.polygons: p.material_index = 0
    if at is not None: o.matrix_world = at

def review(name, items, scene, cams, rev, states=True):
    """items: [(obj, base_preview_material, matrix)], scene(): the studio, cams: {tag: (loc, look, lens)}"""
    shots = []
    H.stage(**scene)
    for o, m, at in items: show(o, m, at)
    for tag, (loc, look, lens) in cams.items():
        H.camera(loc, look, lens, 8 if tag == "ant" else 5.6)
        p = os.path.join(rev, "_%s_%s.jpg" % (name, tag)); H.render(p, 1280, 720, 128); shots.append(p)
    return shots

def main():
    t0 = time.time()
    H.reset()                                                      # (--factory-startup still brings the default cube, light and camera)
    os.makedirs(OUT, exist_ok=True); os.makedirs(REV, exist_ok=True)
    mpath = os.path.join(OUT, os.environ.get("HERO_MANIFEST", "hero_kitchen_manifest.json"))
    doc = json.load(open(mpath, encoding="utf-8")) if os.path.exists(mpath) else {"pieces": []}
    done = {}
    want = lambda n: ONLY is None or n in ONLY
    lean = Matrix.Rotation(math.radians(-LEAN), 4, "X")
    if want("SM_K_Bucket") or want("SM_K_Mop"):
        parts, zp = bucket_parts()
        spec = {"name": "SM_K_Bucket", "parts": parts, "movers": {2: ("SM_K_Bucket_Handle", (0, 0, zp), (1, 0, 0), 112.0, 118.0)},
                "hulls": bucket_hulls, "lods": (0.4, 0.12), "collision_lod": 0, "breaks": bucket_breaks, "hi_sub": 1, "tris0": 12000, "grime": 0.8,
                "note": "20 l bucket (KAN-22, the owner's reference x1.25): hollow, 4 break pieces (plastic), handle on pins"}
        done["SM_K_Bucket"] = H.produce(spec, OUT, REV, NOBAKE)
        parts, tips = mop_parts()
        spec = {"name": "SM_K_Mop", "parts": parts, "hulls": lambda n: mop_hulls(n, tips), "lods": (0.35, 0.1), "collision_lod": 1, "tris0": 14500, "grime": 0.08, "ao_dist": 0.012,   # (the yarn's own AO is deep: more grime / longer AO made it charcoal)
                "breaks": mop_breaks, "sway": {"parts": (0,), "from": H0, "span": 0.2},
                "level_box": ((-0.02, -0.02, 0.0), (0.02, 0.02, H0 + 0.07 + STICK + 0.012)),   # the stick: the yarn is a 4 cm pad on the floor (a tilted box of it would reach through the wall)
                "note": "1.47 m cotton mop (KAN-22, the owner's reference): yarn laid on the floor of the 12 deg lean, sways (vertex colour R), stick snaps"}
        done["SM_K_Mop"] = H.produce(spec, OUT, REV, NOBAKE)
    if want("SM_V31_Broom"):
        parts, hb = broom_parts()
        spec = {"name": "SM_V31_Broom", "parts": parts, "hulls": lambda n: broom_hulls(n, hb), "lods": (0.35, 0.1), "collision_lod": 1, "tris0": 12000,
                "breaks": broom_breaks, "sway": {"parts": (0,), "from": 0.106, "span": 0.1},
                "note": "long-handled sweeping broom (KAN-22): sisal tufts in a lacquered block, beech handle in a ferrule; handle snaps"}
        done["SM_V31_Broom"] = H.produce(spec, OUT, REV, NOBAKE)
    for n, (man, *_r) in done.items():
        doc["pieces"] = [p for p in doc["pieces"] if p["name"] != n] + [man]
    doc["note"] = "Hero props (Tools/blender/hero_lib.py): LOD0 + UCX in <name>.fbx, _LOD1/_LOD2, movers, _Brk<i> pieces; textures tex/T_<name>_{C,N,R}.png (C.a porosity; R = AO, rough, metal; N OpenGL)"
    json.dump(doc, open(mpath, "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    # ---- review images
    prev = {n: H.preview_material(n, d[4]) for n, d in done.items()}
    def place_scene(n):
        man, o, movers, pieces, paths = done[n]
        return o, movers, pieces
    if "SM_K_Mop" in done and "SM_K_Bucket" in done:
        mo, _, mp = place_scene("SM_K_Mop"); bo, bm, bp = place_scene("SM_K_Bucket")
        handle = bm[0]
        bat = Matrix.Translation((BUCKET_XY.x, BUCKET_XY.y, 0))      # (both frames: Blender +X = game -X, +Y = game +Y)
        hat = bat @ Matrix.Translation((0, 0, 0.246 * F)) @ Matrix.Rotation(math.radians(-112.0), 4, "X")   # handle resting on the back
        for x in mp + bp: x.hide_render = True
        if "SM_V31_Broom" in done:
            for x in [done["SM_V31_Broom"][1]] + done["SM_V31_Broom"][3]: x.hide_render = True
        items = [(mo, prev["SM_K_Mop"], lean), (bo, prev["SM_K_Bucket"], bat), (handle, prev["SM_K_Bucket"], hat)]
        scene = {"wall_y": WALL_N + 0.016, "wall_x": WALL_E - 0.016}
        cams = {"wide": ((-0.12, -1.45, 0.50), (0.06, -0.06, 0.36), 38), "ant": ((-0.05, -0.62, 0.012), (0.10, -0.12, 0.14), 20),
                "mop_top": ((0.55, -0.45, 1.1), (0.0, 0.25, 1.25), 35)}
        shots = review("mop_bucket", items, scene, cams, REV)
        # states: wet, charred, broken
        st = []
        for k, (w, c) in enumerate(((1.0, 0.0), (0.0, 0.45))):
            show(mo, H.state_material(prev["SM_K_Mop"], w, c), lean); show(bo, H.state_material(prev["SM_K_Bucket"], w, c, 0.3), bat)
            show(handle, H.state_material(prev["SM_K_Bucket"], w, c), hat)
            H.camera((-0.10, -1.10, 0.32), (0.07, -0.08, 0.20), 40); p = os.path.join(REV, "_mop_bucket_state%d.jpg" % k); H.render(p, 1280, 720, 128); st.append(p)
        show(mo, prev["SM_K_Mop"], lean); show(bo, prev["SM_K_Bucket"], bat); show(handle, prev["SM_K_Bucket"], hat)
        mo.hide_render = True; bo.hide_render = True; handle.hide_render = True
        R = random.Random(3)
        for k, x in enumerate(bp):
            x.hide_render = False; show(x, prev["SM_K_Bucket"])
            x.material_slots[0].material = prev["SM_K_Bucket"]
            mn, mx = H.bounds([x]); c = (mn + mx) * 0.5; out = Vector((c.x, c.y, 0)).normalized() if Vector((c.x, c.y)).length > 0.01 else Vector((0, -1, 0))
            x.matrix_world = Matrix.Translation((BUCKET_XY.x + out.x * 0.09, BUCKET_XY.y + out.y * 0.09 - 0.05, 0)) @ Matrix.Rotation(R.uniform(-0.5, 0.5), 4, "Z")   # the pieces part outwards
        for k, x in enumerate(mp):
            x.hide_render = False
            x.material_slots[0].material = prev["SM_K_Mop"]
            if k:
                mn, mx = H.bounds([x]); c = (mn + mx) * 0.5
                x.matrix_world = Matrix.Translation((-0.02, -0.30, 0.0125)) @ Matrix.Rotation(math.radians(80), 4, "Z") @ Matrix.Rotation(math.radians(90), 4, "Y") @ Matrix.Translation(-c)
            else:
                x.matrix_world = lean   # the lower half stays, the snapped top lies in front

        H.camera((0.75, -1.05, 0.40), (0.05, -0.05, 0.12), 36); p = os.path.join(REV, "_mop_bucket_broken.jpg"); H.render(p, 1280, 720, 128); st.append(p)
        H.sheet([shots[0], shots[1]], os.path.join(REV, "SM_K_Mop_SM_K_Bucket.jpg"), 2)
        H.sheet([shots[2], st[0], st[1], st[2]], os.path.join(REV, "SM_K_Mop_SM_K_Bucket_states.jpg"), 2)
    if "SM_V31_Broom" in done:
        for o in list(bpy.context.scene.objects):
            if o.type in ("MESH", "LIGHT", "CAMERA") and not o.name.startswith("SM_V31_Broom"): bpy.data.objects.remove(o)
        man, o, movers, pieces, paths = done["SM_V31_Broom"]
        for x in pieces: x.hide_render = True
        o.hide_render = False
        scene = {"wall_y": 3.0, "wall_x": 0.163}
        cams = {"wide": ((-1.25, -1.55, 0.75), (0.02, -0.05, 0.40), 34), "ant": ((-0.32, -0.92, 0.012), (0.0, -0.62, 0.06), 20)}
        shots = review("broom", [(o, prev["SM_V31_Broom"], Matrix())], scene, cams, REV)
        st = []
        for k, (w, c) in enumerate(((1.0, 0.0), (0.0, 0.45))):
            show(o, H.state_material(prev["SM_V31_Broom"], w, c))
            H.camera((-0.6, -1.25, 0.25), (0.02, -0.55, 0.10), 40); p = os.path.join(REV, "_broom_state%d.jpg" % k); H.render(p, 1280, 720, 128); st.append(p)
        o.hide_render = True
        for k, x in enumerate(pieces):
            x.hide_render = False; x.material_slots[0].material = prev["SM_V31_Broom"]
            if k: x.matrix_world = Matrix.Translation((-0.25, -0.2, 0.0)) @ Matrix.Rotation(math.radians(-31), 4, "X")
        H.camera((-1.25, -1.55, 0.75), (0.02, -0.05, 0.30), 34); p = os.path.join(REV, "_broom_broken.jpg"); H.render(p, 1280, 720, 128); st.append(p)
        H.sheet([shots[0], shots[1]], os.path.join(REV, "SM_V31_Broom.jpg"), 2)
        H.sheet(st + [st[-1]], os.path.join(REV, "SM_V31_Broom_states.jpg"), 2)
    print("HERO_DONE %.0f s" % (time.time() - t0))
    if os.environ.get("HERO_SAVE"):
        bpy.ops.wm.save_as_mainfile(filepath=os.environ["HERO_SAVE"])

main()
