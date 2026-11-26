# KAN-23: the sink zone set (level v3.2 pilot, Tools/blender/level_v32_pieces.py) rebuilt at the mop's quality through the hero
# pipeline (hero_lib.py): each piece keeps its name, frame and box (the layout rows stay), gets LODs, a baked texture set, UCX hulls,
# break pieces where its class breaks, and review renders (overall, the ant's view, wet / charred / broken).
#   SM_V3_Pail        red 10 l bucket, a grey floor rag over the front rim to the floor; the handle swings (SM_V3_Pail_Handle)
#   SM_V3_WireRack    chrome wire rack on rubber feet with a clear shelf liner on the deck (ants do not fall through)
#   SM_V3_RagPile     three folded rags (terry, check, waffle)
#   SM_V3_Sprayer     window cleaner: PET bottle, trigger head, nozzle, label
#   SM_V3_Plunger     red rubber cup, beech stick, hanging cap
#   SM_V3_SoakBoard   beech cutting board soaking in the sink, knife-scored
#   SM_V3_DishSoap    green dish soap bottle, flip cap, label
# Run: blender --background --factory-startup --python hero_kitchen_sink.py -- <SourceArt/Blender/hero> <Saved/art_review> [names] [nobake]
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
K = H.K

def u(*v): return tuple(x * K for x in v)          # uu -> m (the v3.2 pieces are specified in uu)

def lathe(name, prof_uu, mat, steps=48, sub=1, at=(0, 0, 0)):
    o = H.screw(name, [(r * K, z * K) for r, z in prof_uu], steps, sub); o.location = u(*at); o.data.materials.append(mat); return o

def tube(name, pts_uu, r_uu, mat, res=6, bev=2, tip=1.0):
    o = H.tubes(name, [[Vector(u(*p)) for p in pts_uu]], r_uu * K, res_u=res, bevel_res=bev, tip=tip); o.data.materials.append(mat); return o

def rbox(name, x0, x1, y0, y1, z0, z1, mat, bev_uu=4, sub=1):
    o = H.cube(name, u(x1 - x0, y1 - y0, z1 - z0), u((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)); H.smooth(o, sub, bev_uu * K, 2); o.data.materials.append(mat); return o

def grid_sheet(name, fn, nu, nv, mat, thick_uu=8):
    """a cloth sheet with thickness: fn(u, v) -> (x, y, z) in uu; solidified, smooth"""
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    vs = [[bm.verts.new(u(*fn(i / nu, j / nv))) for j in range(nv + 1)] for i in range(nu + 1)]
    for i in range(nu):
        for j in range(nv):
            bm.faces.new((vs[i][j], vs[i + 1][j], vs[i + 1][j + 1], vs[i][j + 1]))
    bm.to_mesh(me); bm.free()
    o = H.link(bpy.data.objects.new(name, me)); o.data.materials.append(mat)
    s = o.modifiers.new("solid", "SOLIDIFY"); s.thickness = thick_uu * K; s.offset = -1; s.use_rim = True
    H.smooth(o, 1)
    return o

# ------------------------------------------------------------------------------------------------ materials of the set
def mats():
    M = {}
    M["red"] = H.plastic("PailRed", (0.55, 0.05, 0.04), 0.4)
    M["wire"] = H.metal("Wire", (0.80, 0.81, 0.83), 0.18)
    M["chrome"] = H.metal("Chrome", (0.86, 0.87, 0.88), 0.08)
    M["grip"] = H.plastic("GripGrey", (0.6, 0.6, 0.58), 0.55)
    M["ragGrey"] = H.fibre("RagGrey", light=(0.62, 0.60, 0.56), dark=(0.38, 0.36, 0.33), dirt=(0.30, 0.26, 0.21), floor_h=0.05, twist=1800)
    M["ragBlue"] = H.fibre("RagBlue", light=(0.32, 0.45, 0.70), dark=(0.20, 0.28, 0.48), dirt=(0.25, 0.24, 0.22), floor_h=0.0, twist=2400)
    M["ragCheck"] = H.fibre("RagCheck", light=(0.80, 0.30, 0.25), dark=(0.55, 0.16, 0.14), dirt=(0.3, 0.26, 0.2), floor_h=0.0, twist=2000)
    M["ragYellow"] = H.fibre("RagYellow", light=(0.92, 0.78, 0.25), dark=(0.70, 0.55, 0.12), dirt=(0.3, 0.26, 0.2), floor_h=0.0, twist=2200)
    M["rubber"] = H.rubber("FootRubber", (0.04, 0.04, 0.045), 0.85)
    M["liner"] = H.plastic("Liner", (0.80, 0.84, 0.86), 0.25, peel=0.2, scuff=0.15)
    M["petBlue"] = H.plastic("PetBlue", (0.25, 0.55, 0.85), 0.12, peel=0.01, scuff=0.1)
    M["white"] = H.plastic("CapWhite", (0.88, 0.88, 0.85), 0.42)
    M["label"] = H.plastic("LabelPaper", (0.93, 0.92, 0.86), 0.6, peel=0.02, scuff=0.1)
    M["cupRed"] = H.rubber("CupRed", (0.45, 0.05, 0.04), 0.55)
    M["beech"] = H.wood("Beech", light=(0.72, 0.53, 0.32), dark=(0.45, 0.28, 0.14), grain=24, varnish=0.25)
    M["board"] = H.wood("BoardBeech", light=(0.78, 0.62, 0.42), dark=(0.55, 0.38, 0.22), grain=14, varnish=0.0)
    M["soapGreen"] = H.plastic("SoapGreen", (0.18, 0.52, 0.18), 0.2, peel=0.01, scuff=0.1)
    M["capRed"] = H.plastic("CapRed", (0.62, 0.06, 0.05), 0.4)
    for k in ("ragGrey", "ragBlue", "ragCheck", "ragYellow"): H.tag(M[k], 0.0, 1.0)
    H.tag(M["board"], 0.0, 0.8)
    return M

def printed(name, base, ink, bands=7.0, rough=0.35):
    """a printed label: paper/film with bands of print (no real brand: colour fields and lines of 'text')"""
    m, p = H.mat(name)
    tc = H.N(m, "ShaderNodeTexCoord", -900); sep = H.N(m, "ShaderNodeSeparateXYZ", -700); H.L(m, tc.outputs["Object"], sep.inputs["Vector"])
    w = H.N(m, "ShaderNodeTexWave", -500); w.wave_type = "BANDS"; w.bands_direction = "Z"; w.inputs["Scale"].default_value = bands; w.inputs["Distortion"].default_value = 0.0
    H.L(m, tc.outputs["Object"], w.inputs["Vector"])
    t = H.N(m, "ShaderNodeTexWave", -500, -250); t.wave_type = "BANDS"; t.bands_direction = "Z"; t.inputs["Scale"].default_value = bands * 9; t.inputs["Detail"].default_value = 0
    H.L(m, tc.outputs["Object"], t.inputs["Vector"])
    r1 = H.N(m, "ShaderNodeValToRGB", -300); r1.color_ramp.elements[0].position = 0.55; r1.color_ramp.elements[1].position = 0.6
    H.L(m, w.outputs["Fac"], r1.inputs["Fac"])
    r2 = H.N(m, "ShaderNodeValToRGB", -300, -250); r2.color_ramp.elements[0].position = 0.82; r2.color_ramp.elements[1].position = 0.86
    H.L(m, t.outputs["Fac"], r2.inputs["Fac"])
    mx = H.N(m, "ShaderNodeMix", -100); mx.data_type = "RGBA"; mx.inputs[6].default_value = (*base, 1); mx.inputs[7].default_value = (*ink, 1)
    H.L(m, r1.outputs["Color"], mx.inputs[0])
    mx2 = H.N(m, "ShaderNodeMix", 50, -100); mx2.data_type = "RGBA"; mx2.inputs[7].default_value = (0.08, 0.08, 0.09, 1)
    H.L(m, mx.outputs[2], mx2.inputs[6]); H.L(m, r2.outputs["Color"], mx2.inputs[0])
    H.L(m, mx2.outputs[2], p.inputs["Base Color"]); p.inputs["Roughness"].default_value = rough
    return H.tag(m, 0.0, 0.5)

def scored(m):
    """knife scores on a cutting board: thin random lines in the bump"""
    nt = m.node_tree; p = nt.nodes["Principled BSDF"]
    tc = H.N(m, "ShaderNodeTexCoord", -1400, -600)
    lines = []
    for k, (rot, sc) in enumerate(((0.3, 90), (1.1, 70), (2.0, 110))):
        mp = H.N(m, "ShaderNodeMapping", -1200, -600 - k * 200); mp.inputs["Rotation"].default_value = (0, 0, rot); H.L(m, tc.outputs["Object"], mp.inputs["Vector"])
        w = H.N(m, "ShaderNodeTexWave", -1000, -600 - k * 200); w.wave_type = "BANDS"; w.inputs["Scale"].default_value = sc; w.inputs["Distortion"].default_value = 6; w.inputs["Detail"].default_value = 2
        H.L(m, mp.outputs["Vector"], w.inputs["Vector"])
        r = H.N(m, "ShaderNodeValToRGB", -800, -600 - k * 200); r.color_ramp.elements[0].position = 0.0; r.color_ramp.elements[1].position = 0.04
        H.L(m, w.outputs["Fac"], r.inputs["Fac"]); lines.append(r)
    mn = H.N(m, "ShaderNodeMath", -600, -700); mn.operation = "MINIMUM"; H.L(m, lines[0].outputs["Color"], mn.inputs[0]); H.L(m, lines[1].outputs["Color"], mn.inputs[1])
    mn2 = H.N(m, "ShaderNodeMath", -450, -700); mn2.operation = "MINIMUM"; H.L(m, mn.outputs[0], mn2.inputs[0]); H.L(m, lines[2].outputs["Color"], mn2.inputs[1])
    bp = H.N(m, "ShaderNodeBump", -250, -700); bp.inputs["Strength"].default_value = 0.5; bp.inputs["Distance"].default_value = 0.0008
    old = [l for l in nt.links if l.to_socket == p.inputs["Normal"]]
    if old:
        H.L(m, old[0].from_socket, bp.inputs["Normal"])
    H.L(m, mn2.outputs[0], bp.inputs["Height"]); H.L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return m

# ------------------------------------------------------------------------------------------------ the pieces (frames and boxes of level_v32_pieces.py)
PAIL_F = 0.26 / 0.274
def pail(M):
    f = PAIL_F
    prof = [(0, 0.004), (0.090, 0.004), (0.098, 0.000), (0.103, 0.006), (0.1045, 0.014), (0.128, 0.200), (0.1335, 0.203), (0.1335, 0.210), (0.1295, 0.213),
            (0.1325, 0.232), (0.138, 0.235), (0.138, 0.242), (0.134, 0.245), (0.1365, 0.258), (0.1445, 0.262), (0.1470, 0.268), (0.1445, 0.274), (0.1380, 0.274),
            (0.1330, 0.268), (0.1010, 0.016), (0.094, 0.010), (0, 0.010)]
    body = H.screw("Pail", [(r * f, z * f) for r, z in prof], 64, 1); body.data.materials.append(M["red"])
    parts = [(body, 0)]
    for s in (-1, 1):
        lug = H.cube("Lug", (0.012 * f, 0.030 * f, 0.034 * f), (s * 0.1415 * f, 0, 0.243 * f)); H.smooth(lug, 2, 0.004); lug.data.materials.append(M["red"]); parts.append((lug, 0))
        pin = H.cyl("Pin", 0.0045 * f, 0.012 * f, (s * 0.150 * f, 0, 0.246 * f), (0, math.pi / 2, 0), 12); pin.data.materials.append(M["wire"]); parts.append((pin, 0))
    zp = 0.246 * f; ax = 0.156 * f
    pts = [Vector((ax, 0, zp)), Vector((ax - 0.006, 0, zp + 0.012))] + [Vector(((ax - 0.004) * math.cos(math.pi * i / 12), 0, zp + 0.2 * math.sin(math.pi * i / 12))) for i in range(1, 12)] + [Vector((-ax + 0.006, 0, zp + 0.012)), Vector((-ax, 0, zp))]
    wire = H.tubes("Handle", [pts], 0.0026, res_u=10, bevel_res=3); wire.data.materials.append(M["wire"])
    grip = H.cyl("Grip", 0.011, 0.11, (0, 0, zp + 0.2), (0, math.pi / 2, 0), 20); H.smooth(grip, 1, 0.003); grip.data.materials.append(M["grip"])
    parts += [(wire, 2), (grip, 2)]
    # the floor rag over the front rim: in the bucket, over the rim, down the front, on the floor (the v3.2 path, uu)
    P = [(-120, 300), (-200, 420), (-262, 502), (-292, 470), (-300, 300), (-318, 120), (-352, 30), (-420, 12), (-466, 10)]
    def along(t):
        s = t * (len(P) - 1); i = min(int(s), len(P) - 2); fr = s - i
        return P[i][0] + (P[i + 1][0] - P[i][0]) * fr, P[i][1] + (P[i + 1][1] - P[i][1]) * fr
    def rag(uu, v):
        y, z = along(v)
        fold = 9.0 * math.sin(uu * math.pi * 5 + v * 4) * (0.3 + 0.7 * v) + 3.0 * math.sin(uu * 37 + v * 23)
        x = -150 + 300 * uu + 10 * math.sin(v * 9) + (8 * math.sin(v * 31) if uu in (0.0, 1.0) else 0)
        return (x, y - abs(fold) * 0.4, max(4.0, z + 8 + fold * (0.2 if v < 0.45 else 1.0)))
    parts.append((grid_sheet("Rag", rag, 30, 60, M["ragGrey"], 7), 0))
    return parts, zp

def wirerack(M):
    W, D, Hh = 300, 240, 480
    parts = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append((tube("Leg", [(sx * (W - 12), sy * (D - 12), 14), (sx * (W - 12), sy * (D - 12), Hh + 6)], 9, M["chrome"], 2, 5), 0))
            # (pass 2: a closed cylinder, not a lathe: the lathe's merged axis vertices made the feet drop out in the import and
            #  the rack stood 13 uu up on its legs' ends)
            ft = H.cyl("Foot", 16.5 * K, 18 * K, u(sx * (W - 12), sy * (D - 12), 9), seg=20); ft.data.materials.append(M["rubber"])
            parts.append((ft, 0))
    for sy in (-1, 1):
        parts.append((tube("Rail", [(-W + 12, sy * (D - 12), Hh + 6), (W - 12, sy * (D - 12), Hh + 6)], 7, M["chrome"], 2, 5), 0))
        parts.append((tube("Low", [(-W + 12, sy * (D - 12), 150), (W - 12, sy * (D - 12), 150)], 6, M["chrome"], 2, 5), 0))
    for sx in (-1, 1):
        parts.append((tube("RailX", [(sx * (W - 12), -D + 12, Hh + 6), (sx * (W - 12), D - 12, Hh + 6)], 7, M["chrome"], 2, 5), 0))
    for k in range(15):
        x = -W + 20 + k * 40
        parts.append((tube("Wire", [(x, -D + 10, Hh), (x, D - 10, Hh)], 3.5, M["wire"], 2, 4), 0))
    for k in range(3):                                                # cross wires under the deck
        y = -D + 60 + k * (2 * D - 120) / 2.0
        parts.append((tube("Cross", [(-W + 14, y, Hh - 7), (W - 14, y, Hh - 7)], 3.5, M["wire"], 2, 4), 0))
    # the clear shelf liner on the deck (ants walk on it, nothing falls through): a thin sheet with a dimpled grip pattern (bump)
    parts.append((rbox("Liner", -W + 16, W - 16, -D + 14, D - 14, Hh + 3.5, Hh + 7.5, M["liner"], 1.5, 0), 0))
    return parts

def ragpile(M):
    R = random.Random(5)
    parts = []
    for k, (m, w, d) in enumerate((("ragBlue", 400, 290), ("ragCheck", 380, 270), ("ragYellow", 350, 250))):
        a = math.radians(R.uniform(-7, 7)); z0 = k * 25
        def sheet_fn(uu, v, w=w, d=d, z0=z0, a=a):
            x = (uu - 0.5) * w; y = (v - 0.5) * d
            bulge = 26 * math.sin(math.pi * uu) ** 0.3 * math.sin(math.pi * v) ** 0.3 + 3 * math.sin(uu * 21 + v * 13)
            ca, sa = math.cos(a), math.sin(a)
            return (x * ca - y * sa, x * sa + y * ca, z0 + 4 + bulge)
        # (pass 2: no separate roll tube on the front edge - it read as a pipe. The sheet itself folds: flat on top, round the
        #  front edge (a 10 uu bend) and back under itself, with soft creases; a cloth, not tubes)
        def folded(uu, v, w=w, d=d, z0=z0, a=a, k=k):
            x = (uu - 0.5) * w
            L = d * 0.82; rb = 10.0                                        # top run, the bend radius at the front edge
            s = v * (L + math.pi * rb + L * 0.55)                          # arc length along the cloth
            crease = 3.5 * math.sin(uu * 17.0 + k) * math.sin(v * 9.0 + k * 2) + 2.0 * math.sin(uu * 41 + v * 23)
            sag = 5.0 * math.sin(math.pi * uu) ** 0.5
            if s <= L:
                y = d / 2 - s; z = z0 + 2 * rb + sag + crease * 0.6
            elif s <= L + math.pi * rb:
                t = (s - L) / rb
                y = d / 2 - L - math.sin(t) * rb * 1.05; z = z0 + rb + math.cos(t) * rb + sag * (1 - t / math.pi)
            else:
                y = d / 2 - L + (s - L - math.pi * rb); z = z0 + 1.0 + crease * 0.2
            ca, sa = math.cos(a), math.sin(a)
            return (x * ca - y * sa, x * sa + y * ca, max(z0 + 0.5, z))
        parts.append((grid_sheet("Rag%d" % k, folded, 30, 36, M[m], 3.5), 0))
        ca, sa = math.cos(a), math.sin(a)
        for t in range(4):                                            # a few frayed threads at the side, thin
            s = R.uniform(-0.4, 0.4) * d; x0 = w / 2 * (1 if t % 2 else -1)
            q0 = (x0 * ca - s * sa, x0 * sa + s * ca, z0 + 6); q1 = (q0[0] + R.uniform(-8, 8) + (9 if x0 > 0 else -9), q0[1] + R.uniform(-8, 8), z0 + 1)
            parts.append((tube("Thread", [q0, ((q0[0] + q1[0]) / 2, (q0[1] + q1[1]) / 2, z0 + 5), q1], 0.9, M[m], 4, 1), 0))
    return parts

def sprayer(M):
    parts = [(rbox("Body", -72, 72, -44, 44, 0, 250, M["petBlue"], 22, 2), 0),
             (lathe("Shoulder", [(0, 0), (62, 0), (60, 20), (30, 62), (26, 96), (0, 96)], M["petBlue"], 32, 1, (0, 0, 244)), 0),
             (lathe("Collar", [(0, 0), (34, 0), (35, 4), (35, 30), (33, 34), (0, 34)], M["white"], 32, 1, (0, 0, 336)), 1)]
    for k in range(12):                                               # collar ribs
        a = 2 * math.pi * k / 12
        o = H.cyl("Rib", 2.2 * K, 26 * K, u(35 * math.cos(a), 35 * math.sin(a), 353), seg=6); o.data.materials.append(M["white"]); parts.append((o, 1))
    parts += [(rbox("Head", -44, 96, -30, 30, 366, 428, M["red"], 12, 1), 1),
              (rbox("Trigger", 30, 54, -12, 12, 296, 366, M["red"], 6, 1), 1)]
    nz = H.cyl("Nozzle", 15 * K, 26 * K, u(104, 0, 398), (0, math.pi / 2, 0), 16); H.smooth(nz, 1, 3 * K); nz.data.materials.append(M["white"]); parts.append((nz, 1))
    lab = rbox("Label", -54, 54, -45, -43.6, 60, 200, printed("LabelSpray", (0.95, 0.95, 0.92), (0.15, 0.45, 0.80), 5.0), 0.4, 0)
    parts.append((lab, 0))
    return parts

def plunger(M):
    parts = [(lathe("Cup", [(0, 96), (40, 100), (96, 78), (150, 22), (156, 0), (146, 0), (138, 18), (88, 66), (40, 84), (0, 84)], M["cupRed"], 40, 1), 0),
             (lathe("Socket", [(0, 0), (34, 0), (34, 40), (26, 46), (0, 46)], M["cupRed"], 24, 1, (0, 0, 92)), 0)]
    st = H.cyl("Stick", 19 * K, 612 * K, u(0, 0, 130 + 306), seg=24); st.data.materials.append(M["beech"]); parts.append((st, 1))
    cap = lathe("Cap", [(0, 0), (20, 0), (23, 8), (22, 18), (12, 26), (0, 28)], M["capRed"], 20, 1, (0, 0, 738)); parts.append((cap, 1))
    return parts

def soakboard(M):
    RUN, RISE, TH = 720.0, 490.0, 34.0
    ln = math.hypot(RUN, RISE); ang = math.atan2(RISE, RUN)
    b = H.cube("Board", u(540, ln, TH), (0, 0, 0)); H.smooth(b, 1, 6 * K, 2)
    b.data.materials.append(scored(M["board"]))
    # lower edge on the sink bottom at the origin, rising toward -Y
    b.matrix_world = Matrix.Rotation(-ang, 4, "X") @ Matrix.Translation(u(0, -ln / 2, TH / 2))
    hole = H.cyl("Hole", 40 * K, 40 * K, (0, 0, 0), seg=24); hole.data.materials.append(M["rubber"])
    hole.matrix_world = Matrix.Rotation(-ang, 4, "X") @ Matrix.Translation(u(0, -ln * 0.9, TH / 2))
    groove = []
    return [(b, 0), (hole, 0)]

def dishsoap(M):
    body = lathe("Body", [(0, 0), (52, 0), (58, 8), (58, 214), (54, 226), (30, 262), (24, 292), (0, 292)], M["soapGreen"], 40, 1)
    body.scale = (1.0, 0.78, 1.0)
    parts = [(body, 0),
             (lathe("Cap", [(0, 0), (27, 0), (28, 4), (27, 30), (22, 36), (0, 36)], M["capRed"], 28, 1, (0, 0, 288)), 1),
             (lathe("Spout", [(0, 0), (10, 0), (9, 14), (5, 18), (0, 18)], M["white"], 16, 1, (0, 0, 322)), 1)]
    parts.append((wrap_label("Label", 58.6, 60, 180, 70, 0.78, printed("LabelSoap", (0.97, 0.96, 0.9), (0.85, 0.65, 0.1), 4.0)), 0))
    return parts

def wrap_label(name, r_uu, z0, z1, half_deg, yscale, mat, th_uu=0.8, n=24):
    """a label wrapped round the front of a (flattened) round bottle: an arc of film with a hair of thickness"""
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    rows = []
    for rr in (r_uu, r_uu + th_uu):
        ring = []
        for i in range(n + 1):
            a = math.radians(-90 - half_deg + 2 * half_deg * i / n)
            ring.append([bm.verts.new(u(rr * math.cos(a), rr * math.sin(a) * yscale, z)) for z in (z0, z1)])
        rows.append(ring)
    inner, outer = rows
    for i in range(n):
        bm.faces.new((outer[i][0], outer[i + 1][0], outer[i + 1][1], outer[i][1]))
        bm.faces.new((inner[i][1], inner[i + 1][1], inner[i + 1][0], inner[i][0]))
        bm.faces.new((inner[i][1], outer[i][1], outer[i + 1][1], inner[i + 1][1]))
        bm.faces.new((inner[i + 1][0], outer[i + 1][0], outer[i][0], inner[i][0]))
    for i in (0, n):
        bm.faces.new((inner[i][0], outer[i][0], outer[i][1], inner[i][1]) if i == 0 else (inner[i][1], outer[i][1], outer[i][0], inner[i][0]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(me); bm.free()
    o = H.link(bpy.data.objects.new(name, me)); o.data.materials.append(mat)
    return o

# ------------------------------------------------------------------------------------------------ hulls (UCX) and break pieces
def ring(r_uu, z_uu, n=12, cx=0, cy=0): return H.ring_pts(r_uu * K, z_uu * K, n, cx * K, cy * K)
def boxp(x0, x1, y0, y1, z0, z1): return [(x * K, y * K, z * K) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)]

def hulls_pail(n):
    f = PAIL_F; hs = [H.hull("UCX_%s_00" % n, H.ring_pts(0.103 * f, 0.0, 16) + H.ring_pts(0.101 * f, 0.012 * f, 16))]
    for k in range(8):
        pts = []
        for a in (2 * math.pi * k / 8, 2 * math.pi * (k + 1) / 8):
            for r, z in ((0.1045, 0.014), (0.101, 0.014), (0.147, 0.274), (0.133, 0.274)):
                pts.append((r * f * math.cos(a), r * f * math.sin(a), z * f))
        hs.append(H.hull("UCX_%s_%02d" % (n, k + 1), pts))
    hs.append(H.hull("UCX_%s_09" % n, boxp(-160, 160, -470, -290, 0, 40)))       # the rag on the floor
    return hs

def hulls_rack(n):
    W, D, Hh = 300, 240, 480
    hs = [H.hull("UCX_%s_%02d" % (n, k), boxp(sx * (W - 12) - 18, sx * (W - 12) + 18, sy * (D - 12) - 18, sy * (D - 12) + 18, 0, Hh))
          for k, (sx, sy) in enumerate(((-1, -1), (-1, 1), (1, -1), (1, 1)))]
    hs.append(H.hull("UCX_%s_04" % n, boxp(-W, W, -D, D, Hh - 12, Hh + 14)))
    return hs

def hulls_box(n, *b):
    return [H.hull("UCX_%s_00" % n, boxp(*b))]

def hulls_sprayer(n): return [H.hull("UCX_%s_00" % n, boxp(-72, 72, -44, 44, 0, 336)), H.hull("UCX_%s_01" % n, boxp(-44, 118, -30, 30, 336, 428))]
def hulls_plunger(n): return [H.hull("UCX_%s_00" % n, ring(156, 0, 16) + ring(40, 100, 8)), H.hull("UCX_%s_01" % n, ring(19, 130, 8) + ring(23, 766, 8))]
def hulls_soap(n): return [H.hull("UCX_%s_00" % n, [(p[0], p[1] * 0.78, p[2]) for p in ring(58, 0, 12) + ring(58, 226, 12)]), H.hull("UCX_%s_01" % n, ring(28, 288, 10) + ring(10, 340, 8))]
def hulls_board(n):
    RUN, RISE, TH = 720.0, 490.0, 34.0
    ln = math.hypot(RUN, RISE); ny, nz = RISE / ln, RUN / ln
    pts = []
    for x in (-270, 270):
        for (y, z) in ((0, 0), (-RUN, RISE), (-RUN + ny * TH, RISE + nz * TH), (ny * TH, nz * TH)):
            pts.append((x * K, y * K, z * K))
    return [H.hull("UCX_%s_00" % n, pts)]

def _copy(o, name):
    c = o.copy(); c.data = o.data.copy(); c.name = name; c.data.name = name; H.link(c); return c

def neck_snap(lod0, name, z_uu, jag, seed):
    """a plastic bottle snaps at the neck (pass 2: a real ragged fracture, so both pieces carry the Break slot - split by parts
    they had no fracture face and the slot was dropped in the export)"""
    src = _copy(lod0, name + "_Brk0")
    lo, up = H.split(src, H.cutter(u(0, 0, z_uu), (0.18, 0.08, 1.0), 0.4, jag * 0.25, seed, sub=6, depth=1.0), name + "_Brk0", name + "_Brk1")
    return [x for x in (lo, up) if len(x.data.polygons)]

def by_parts(lod0, name, groups):
    return [H.keep_parts(lod0, g, "%s_Brk%d" % (name, i)) for i, g in enumerate(groups)]

def breaks_pail(lod0, name):
    f = PAIL_F; src = _copy(lod0, name + "_Brk0")
    a, b = H.split(src, H.cutter((0.012, 0.01, 0.13), (1.0, 0.3, 0.12), 1.2, 0.009, 13), name + "_Brk0", name + "_Brk1")
    b, c = H.split(b, H.cutter((0.0, 0.0, 0.05), (0.1, 0.08, 1.0), 1.2, 0.007, 17), name + "_Brk1", name + "_Brk2")
    return [x for x in (a, b, c) if len(x.data.polygons)]

def snap(lod0, name, part, co, no):
    stick = H.keep_parts(lod0, [part], name + "_Brk1")
    rest = H.keep_parts(lod0, [p for p in set(a.value for a in lod0.data.attributes["part"].data) if p != part], name + "_Brk0")
    up, low = H.split(stick, H.cutter(co, no, 0.2, 0.004, 21, sub=6, splinter=0.007, depth=3.0), name + "_Brk1", name + "_low")
    return [H.join([rest, low], name + "_Brk0"), up]

def breaks_board(lod0, name):
    src = _copy(lod0, name + "_Brk0")
    a, b = H.split(src, H.cutter((0.03 * 1, -0.2, 0.15), (1.0, 0.04, 0.0), 1.5, 0.005, 23, sub=6, splinter=0.006), name + "_Brk0", name + "_Brk1")
    return [a, b]

SPECS = {
    "SM_V3_Pail": dict(build=pail, movers=True, hulls=hulls_pail, lods=(0.4, 0.12), collision_lod=0, breaks=breaks_pail, sway={"parts": (0,), "from": 0.30, "span": 0.25}),
    "SM_V3_WireRack": dict(build=wirerack, hulls=hulls_rack, lods=(0.4, 0.15), collision_lod=0, breaks=None),
    "SM_V3_RagPile": dict(build=ragpile, hulls=lambda n: hulls_box(n, -215, 215, -160, 160, 0, 105), lods=(0.35, 0.12), collision_lod=1, breaks=None),
    "SM_V3_Sprayer": dict(build=sprayer, hulls=hulls_sprayer, lods=(0.4, 0.15), collision_lod=0, breaks=lambda o, n: neck_snap(o, n, 332, 0.010, 23)),
    "SM_V3_Plunger": dict(build=plunger, hulls=hulls_plunger, lods=(0.4, 0.15), collision_lod=0, breaks=lambda o, n: snap(o, n, 1, u(0, 0, 470), (0.2, 0.1, 1.0))),
    "SM_V3_SoakBoard": dict(build=soakboard, hulls=hulls_board, lods=(0.4, 0.15), collision_lod=0, breaks=breaks_board),
    "SM_V3_DishSoap": dict(build=dishsoap, hulls=hulls_soap, lods=(0.4, 0.15), collision_lod=0, breaks=lambda o, n: neck_snap(o, n, 272, 0.006, 29)),
}

# ------------------------------------------------------------------------------------------------ review (overall, the ant's view, wet, charred, broken)
def review(name, o, movers, pieces, prev, mover_rest=None):
    for x in list(H.S().objects):
        if x.type in ("LIGHT", "CAMERA") or (x.type == "MESH" and x not in [o] + movers + pieces and not x.name.startswith(name)):
            bpy.data.objects.remove(x)
    for x in pieces: x.hide_render = True
    mn, mx = H.bounds([o]); c = (mn + mx) * 0.5; s = max((mx - mn).length, 0.12)
    H.stage(wall_y=mx.y + 0.012)
    for x in [o] + movers:
        x.data.materials.clear(); x.data.materials.append(prev)
    if movers and mover_rest is not None:
        for mv in movers: mv.matrix_world = mover_rest
    shots = []
    def shot(tag, loc, look, lens, fstop=5.6):
        H.camera(loc, look, lens, fstop); p = os.path.join(REV, "_%s_%s.jpg" % (name, tag)); H.render(p, 1280, 720, 128); shots.append(p)
    shot("wide", (c.x + 0.9 * s, mn.y - 1.5 * s, c.z + 0.55 * s), (c.x, c.y, c.z), 40)
    shot("ant", (c.x - 0.15 * s, mn.y - 0.45 * s, 0.012), (c.x, c.y, mn.z + 0.35 * (mx.z - mn.z)), 20, 8)
    for k, (w, ch) in enumerate(((1.0, 0.0), (0.0, 0.45))):
        m = H.state_material(prev, w, ch)
        for x in [o] + movers: x.material_slots[0].material = m
        shot("state%d" % k, (c.x + 0.7 * s, mn.y - 1.2 * s, c.z + 0.35 * s), (c.x, c.y, c.z), 40)
    for x in [o] + movers: x.material_slots[0].material = prev
    if pieces:
        o.hide_render = True
        for mv in movers: mv.hide_render = True
        R = random.Random(7)
        for k, x in enumerate(pieces):
            x.hide_render = False; x.material_slots[0].material = prev
            pm, px = H.bounds([x]); pc = (pm + px) * 0.5
            out = Vector((pc.x - c.x, pc.y - c.y, 0)); out = out.normalized() if out.length > 0.005 else Vector((0.0, -1.0, 0.0))
            x.matrix_world = Matrix.Translation(out * 0.05 * s * (1 + k * 0.3)) @ Matrix.Translation(pc) @ Matrix.Rotation(R.uniform(-0.3, 0.3), 4, "Z") @ Matrix.Translation(-pc)
        shot("broken", (c.x + 0.9 * s, mn.y - 1.5 * s, c.z + 0.55 * s), (c.x, c.y, c.z * 0.7), 40)
    H.sheet(shots[:2], os.path.join(REV, name + ".jpg"), 2)
    st = shots[2:] + ([shots[1]] if len(shots) == 4 else [])
    H.sheet(st[:4] if len(st) >= 4 else st + [shots[0]] * (4 - len(st)), os.path.join(REV, name + "_states.jpg"), 2)

def main():
    t0 = time.time()
    H.reset()
    os.makedirs(OUT, exist_ok=True); os.makedirs(REV, exist_ok=True)
    mpath = os.path.join(OUT, os.environ.get("HERO_MANIFEST", "hero_kitchen_manifest.json"))
    doc = json.load(open(mpath, encoding="utf-8")) if os.path.exists(mpath) else {"pieces": []}
    H.plastic("Break", (0.8, 0.8, 0.78), 0.7)
    for name, sp in SPECS.items():
        if ONLY and name not in ONLY: continue
        for x in list(H.S().objects): bpy.data.objects.remove(x)
        M = mats()
        res = sp["build"](M)
        parts, zp = res if isinstance(res, tuple) else (res, None)
        spec = {"name": name, "parts": parts, "hulls": sp["hulls"], "lods": sp["lods"], "collision_lod": sp["collision_lod"], "breaks": sp["breaks"],
                "note": "KAN-23 hero rebuild of the v3.2 piece (same frame and box)"}
        if sp.get("movers"): spec["movers"] = {2: (name + "_Handle", (0, 0, zp), (1, 0, 0), 112.0, 116.0)}
        if sp.get("sway"): spec["sway"] = sp["sway"]
        man, lod0, movers, pieces, paths = H.produce(spec, OUT, REV, NOBAKE)
        doc["pieces"] = [p for p in doc["pieces"] if p["name"] != name] + [man]
        json.dump(doc, open(mpath, "w", encoding="utf-8"), indent=1, ensure_ascii=False)
        prev = H.preview_material(name, paths)
        rest = (Matrix.Translation((0, 0, zp)) @ Matrix.Rotation(math.radians(-112), 4, "X")) if zp else None
        review(name, lod0, movers, pieces, prev, rest)
        print("HERO_SINK %s done" % name)
    print("HERO_SINK_DONE %.0f s" % (time.time() - t0))

main()
