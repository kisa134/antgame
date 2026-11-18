# Ant Kitchen: the Human as a jointed "action figure" - one closed-solid FBX per rigid segment, origin at its joint.
# Run:  blender --background --factory-startup --python human_build.py -- <out_dir> [<HumanBodyData.inl>] [dry]
# Frame: metres, +Z up, the Human FACES +X, his LEFT side is +Y (a default UE FBX import turns a Blender point
# (x, y, z) m into mesh-local (100x, -100y, 100z): X forward, Y right, Z up). Game scale: component scale 19
# (1 m = 1900 uu), so the 1.76 m figure stands about 3350 uu tall.
# Bind pose: standing straight, arms hanging at the sides, palms towards the thighs, thumbs forward.
# Style: the cosy cartoon of the kitchen set (kitchen_lib.py helpers): chunky rounded solids, flat clean colours.
import bpy, bmesh, sys, os, math, json, random
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else os.path.join(HERE, "..", "..", "SourceArt", "Blender", "human")
INL = argv[1] if len(argv) > 1 and argv[1] != "dry" else None
DRY = "dry" in argv
bpy.ops.wm.read_factory_settings(use_empty=True)
import kitchen_lib as L
from mathutils import Vector, Matrix
mat = L.mat

# ---- palette (slot names are what the game maps to its surface looks: HumanBody.cpp)
mat("Skin", (0.90, 0.58, 0.44), 0.55, new=True)
mat("Shirt", (0.95, 0.90, 0.78), 0.9, new=True)          # cream T-shirt
mat("ShirtTrim", (0.13, 0.22, 0.42), 0.9, new=True)      # navy collar, sleeve and hem bands
mat("Pyjama", (0.27, 0.44, 0.70), 0.9, new=True)         # blue pyjama trousers...
mat("PyjamaStripe", (0.80, 0.86, 0.94), 0.9, new=True)   # ...with pale stripes
mat("Slipper", (0.95, 0.42, 0.60), 0.95, new=True)       # fluffy pink slippers
mat("Fluff", (0.98, 0.96, 0.92), 0.95, new=True)
mat("SlipperSole", (0.74, 0.62, 0.46), 0.8, new=True)
mat("Hair", (0.24, 0.13, 0.06), 0.7, new=True)
mat("EyeWhite", (0.97, 0.97, 0.95), 0.25, new=True)
mat("Pupil", (0.03, 0.03, 0.04), 0.2, new=True)
mat("Mouth", (0.42, 0.10, 0.10), 0.5, new=True)

HS = 1.30   # cartoon hands: a third bigger than life
SWAPXZ = Matrix(((0, 0, 1, 0), (0, 1, 0, 0), (1, 0, 0, 0), (0, 0, 0, 1)))

def _emit(bm, m=None, M=None, floor=None):
    if m is not None:
        i = L._slot(m)
        for f in bm.faces:
            f.material_index = i
    if M is not None:
        bm.transform(M)
    if floor is not None:
        for v in bm.verts:
            if v.co.z < floor:
                v.co.z = floor
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    L._merge(L.S.stack[-1], bm)

def _ring(bm, z, cx, cy, rx, ry, n, pw):
    out = []
    e = 2.0 / pw
    for i in range(n):
        t = 2 * math.pi * i / n
        c, s = math.cos(t), math.sin(t)
        out.append(bm.verts.new((cx + rx * math.copysign(abs(c) ** e, c), cy + ry * math.copysign(abs(s) ** e, s), z)))
    return out

def loft(rings, m, n=20, lo=None, hi=None, M=None, pw=2.0, stripe=None, noise=0.0, seed=7, floor=None):
    # A closed solid through elliptic rings (z, cx, cy, rx, ry), bottom to top. lo / hi: height of a rounded cap
    # below the first / above the last ring (None = flat). pw > 2: boxier rings. stripe=(matA, matB, period):
    # every period-th facet column is matB (pyjama stripes without extra geometry).
    bm = bmesh.new()
    full = []
    z0, cx0, cy0, rx0, ry0 = rings[0]
    if lo:
        for a in (67.5, 45.0, 22.5):
            k, d = math.cos(math.radians(a)), math.sin(math.radians(a))
            full.append((z0 - lo * d, cx0, cy0, rx0 * k, ry0 * k))
    full += list(rings)
    z1, cx1, cy1, rx1, ry1 = rings[-1]
    if hi:
        for a in (22.5, 45.0, 67.5):
            k, d = math.cos(math.radians(a)), math.sin(math.radians(a))
            full.append((z1 + hi * d, cx1, cy1, rx1 * k, ry1 * k))
    R = [_ring(bm, r[0], r[1], r[2], r[3], r[4], n, pw) for r in full]
    ia = L._slot(stripe[0] if stripe else m)
    ib = L._slot(stripe[1]) if stripe else ia
    per = stripe[2] if stripe else 1
    k0 = 3 if lo else 0
    for k, (a, b) in enumerate(zip(R, R[1:])):
        striped = stripe and k0 <= k < k0 + len(rings) - 1   # not on the rounded caps: the stripes would meet in a star
        for i in range(n):
            j = (i + 1) % n
            f = bm.faces.new((a[i], a[j], b[j], b[i]))
            f.material_index = ib if (striped and i % per == 0) else ia
    if lo:
        p = bm.verts.new((cx0, cy0, z0 - lo))
        for i in range(n):
            bm.faces.new((p, R[0][(i + 1) % n], R[0][i])).material_index = ia
    else:
        bm.faces.new(list(reversed(R[0]))).material_index = ia
    if hi:
        p = bm.verts.new((cx1, cy1, z1 + hi))
        for i in range(n):
            bm.faces.new((p, R[-1][i], R[-1][(i + 1) % n])).material_index = ia
    else:
        bm.faces.new(R[-1]).material_index = ia
    if noise > 0:
        rnd = random.Random(seed)
        for v in bm.verts:
            v.co += Vector((rnd.uniform(-noise, noise), rnd.uniform(-noise, noise), rnd.uniform(-noise, noise)))
    _emit(bm, None, M, floor)

def loftx(rings, m, **kw):
    # The same, lying along X: rings are (x, zc, y, rz, ry).
    loft(rings, m, M=SWAPXZ, **kw)

def digit(pts, r, m, n=10):
    # A finger: a tube along the points with both ends rounded.
    P = [Vector(p) for p in pts]
    sp = L.spline(P, 3) if len(P) > 2 else P
    d0 = (sp[0] - sp[1]).normalized()
    d1 = (sp[-1] - sp[-2]).normalized()
    path = [sp[0] + d0 * r * 0.95, sp[0] + d0 * r * 0.7, sp[0] + d0 * r * 0.35] + sp + [sp[-1] + d1 * r * 0.35, sp[-1] + d1 * r * 0.7, sp[-1] + d1 * r * 0.95]
    rad = [r * 0.3, r * 0.72, r * 0.94] + [r] * len(sp) + [r * 0.94, r * 0.72, r * 0.3]
    L.tube(path, rad, m, n)

def fluff(r, x, y, z, m="Fluff", seed=1, nu=10, nv=6, scale=(1, 1, 1)):
    # A lumpy ball: reads as a tuft of fluff.
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=nu, v_segments=nv, radius=r)
    rnd = random.Random(seed)
    for v in bm.verts:
        k = 1.0 + rnd.uniform(-0.16, 0.16)
        v.co = Vector((v.co.x * scale[0] * k, v.co.y * scale[1] * k, v.co.z * scale[2] * k))
    _emit(bm, m, Matrix.Translation((x, y, z)))

def grow(k, about):
    # Scales everything built so far for the current piece about a point (bigger head, bigger feet).
    a = Vector(about)
    L.S.stack[0].transform(Matrix.Translation(a) @ Matrix.Scale(k, 4) @ Matrix.Translation(-a))

SEGS = []   # (name, parent, pivot) in the order the game's enum uses
def seg(name, parent, pivot):
    L.piece("SM_H_" + name, origin=pivot)
    SEGS.append((name, parent, Vector(pivot)))

PJ = ("Pyjama", "PyjamaStripe", 3)

# ------------------------------------------------------------------ torso
# Pelvis: pyjama trousers from the crotch to the waist.
loft([(0.845, 0.0, 0, 0.105, 0.160), (0.90, 0.0, 0, 0.126, 0.184), (0.97, 0.006, 0, 0.136, 0.188), (1.035, 0.010, 0, 0.134, 0.182)],
     None, n=42, lo=0.05, hi=0.03, stripe=PJ)
seg("Pelvis", None, (0, 0, 0.92))

# Belly: the T-shirt over a comfortable paunch; the hem hangs over the waistband.
loft([(0.985, 0.012, 0, 0.144, 0.192), (1.03, 0.022, 0, 0.158, 0.197), (1.10, 0.038, 0, 0.172, 0.200), (1.17, 0.032, 0, 0.162, 0.198),
      (1.24, 0.016, 0, 0.142, 0.194), (1.275, 0.012, 0, 0.132, 0.190)], "Shirt", n=24, lo=0.012, hi=0.04)
loft([(0.982, 0.012, 0, 0.148, 0.196), (1.004, 0.016, 0, 0.154, 0.199)], "ShirtTrim", n=24, lo=0.006, hi=0.006)
seg("Belly", "Pelvis", (0, 0, 1.03))

# Chest and shoulders, with the collar band.
loft([(1.19, 0.012, 0, 0.136, 0.190), (1.26, 0.012, 0, 0.136, 0.198), (1.34, 0.006, 0, 0.130, 0.214), (1.41, 0.0, 0, 0.116, 0.214),
      (1.452, 0.0, 0, 0.090, 0.176), (1.475, 0.0, 0, 0.066, 0.086)], "Shirt", n=24, lo=0.04, hi=0.012)
loft([(1.472, 0.0, 0, 0.072, 0.080), (1.492, 0.002, 0, 0.066, 0.072)], "ShirtTrim", n=20, lo=0.006, hi=0.006)
seg("Chest", "Belly", (0, 0, 1.23))

loft([(1.44, 0.0, 0, 0.052, 0.055), (1.55, 0.004, 0, 0.050, 0.053)], "Skin", n=14, lo=0.03, hi=0.03)
seg("Neck", "Chest", (0, 0, 1.465))

# ------------------------------------------------------------------ head: the face an ant sees looming over it
loft([(1.545, 0.012, 0, 0.062, 0.060), (1.575, 0.012, 0, 0.094, 0.089), (1.61, 0.008, 0, 0.108, 0.102), (1.65, 0.004, 0, 0.112, 0.104),
      (1.69, 0.0, 0, 0.111, 0.103), (1.725, -0.002, 0, 0.100, 0.094), (1.75, -0.004, 0, 0.078, 0.074)], "Skin", n=28, lo=0.012, hi=0.022)
for s in (1, -1):
    L.ball(0.0245, 0.093, s * 0.043, 1.672, "EyeWhite", scale=(0.8, 1.0, 1.05), nu=14, nv=8)
    L.ball(0.0125, 0.1075, s * 0.041, 1.666, "Pupil", scale=(0.6, 1.0, 1.0), nu=10, nv=6)
    L.rbox(0.020, 0.056, 0.017, 0.108, s * 0.044, 1.712, "Hair", 0.005, 2, rot=(s * 13.0, 0, 0))           # brows, stern
    L.ball(0.030, 0.0, s * 0.104, 1.652, "Skin", scale=(0.55, 0.4, 1.0), nu=12, nv=8)                      # ears
    L.ball(0.025, 0.110, s * 0.027, 1.607, "Hair", scale=(0.55, 1.35, 0.5), nu=12, nv=6, rot=(s * -14.0, 0, 0))   # moustache
L.ball(0.027, 0.120, 0.0, 1.640, "Skin", scale=(1.0, 0.92, 1.05), nu=14, nv=8)                              # nose
L.rbox(0.014, 0.042, 0.011, 0.099, 0.0, 1.581, "Mouth", 0.004, 2)
# Hair: a cap over the top and the back of the skull (a dome tilted backwards), and a quiff.
_hr = []
for a in (80.0, 62.0, 44.0, 24.0):
    _hr.append((0.140 * math.cos(math.radians(a)), 0, 0, 0.128 * math.sin(math.radians(a)), 0.119 * math.sin(math.radians(a))))
loft(_hr, "Hair", n=24, hi=0.140 * (1 - math.cos(math.radians(24.0))),
     M=Matrix.Translation((-0.004, 0, 1.652)) @ Matrix.Rotation(math.radians(-25.0), 4, "Y"), noise=0.0015, seed=3)
fluff(0.030, 0.082, 0.02, 1.744, "Hair", seed=5, nu=12, nv=8, scale=(1.0, 1.7, 0.6))
grow(1.12, (0, 0, 1.535))
seg("Head", "Neck", (0, 0, 1.535))

# ------------------------------------------------------------------ arms
def arm(s, T):
    y = s * 0.248
    zs, ze, zw = 1.40, 1.095, 0.825
    # Upper arm: the sleeve with the shoulder ball, then the bare arm down to a rounded elbow.
    loft([(zs - 0.165, 0, y, 0.074, 0.074), (zs - 0.11, 0, y, 0.076, 0.076), (zs, 0, y, 0.076, 0.078)], "Shirt", n=18, lo=0.006, hi=0.05)
    loft([(zs - 0.172, 0, y, 0.0775, 0.0775), (zs - 0.148, 0, y, 0.0785, 0.0785)], "ShirtTrim", n=18, lo=0.004, hi=0.004)
    loft([(ze, 0, y, 0.058, 0.058), (ze + 0.075, 0, y, 0.064, 0.064), (ze + 0.165, 0, y, 0.066, 0.066)], "Skin", n=16, lo=0.058, hi=0.01)
    seg("UpperArm_" + T, "Chest", (0, y, zs))
    loft([(zw, 0, y, 0.044, 0.040), (zw + 0.065, 0, y, 0.050, 0.047), (zw + 0.185, 0, y, 0.060, 0.058), (ze, 0, y, 0.058, 0.058)], "Skin", n=16, lo=0.03, hi=0.058)
    seg("Forearm_" + T, "UpperArm_" + T, (0, y, ze))
    # Hand: palm (facing the thigh), wrist ball, thumb pointing forward and down.
    L.rbox(0.086 * HS, 0.036 * HS, 0.100 * HS, 0.004, y, zw - 0.050 * HS, "Skin", 0.014 * HS, 3)
    L.ball(0.043, 0, y, zw, "Skin", nu=12, nv=6)
    b = Vector((0.036 * HS, y - s * 0.004, zw - 0.036 * HS))
    digit([b, b + Vector((0.030, 0, -0.028)) * HS, b + Vector((0.040, -s * 0.010, -0.062)) * HS], 0.0168 * HS, "Skin", 10)
    seg("Hand_" + T, "Forearm_" + T, (0, y, zw))
    # Fingers: four in a row, slightly curled towards the palm; one rigid group hinged at the knuckles.
    zk = zw - 0.092 * HS
    for dx, ln in ((0.0315, 0.078), (0.0105, 0.088), (-0.0105, 0.081), (-0.0315, 0.064)):
        ln *= HS
        x = dx * HS + 0.004
        digit([(x, y, zk + 0.014), (x, y - s * 0.004, zk - 0.50 * ln), (x, y - s * 0.022 * HS, zk - 0.93 * ln)], 0.0122 * HS, "Skin", 10)
    seg("Fingers_" + T, "Hand_" + T, (0, y, zk))
arm(1, "L")
arm(-1, "R")

# ------------------------------------------------------------------ legs
def leg(s, T):
    y = s * 0.112
    loft([(0.50, 0, y, 0.082, 0.082), (0.60, 0, y, 0.088, 0.088), (0.75, 0, y, 0.100, 0.101), (0.88, 0, y, 0.106, 0.108), (0.93, 0, y, 0.102, 0.104)],
         None, n=24, lo=0.08, hi=0.05, stripe=PJ)
    seg("Thigh_" + T, "Pelvis", (0, y, 0.92))
    # Shin: the trouser leg down to a loose hem, then the bare ankle.
    loft([(0.165, 0, y, 0.087, 0.087), (0.20, 0, y, 0.083, 0.083), (0.36, 0, y, 0.080, 0.080), (0.50, 0, y, 0.082, 0.082)],
         None, n=24, lo=0.004, hi=0.08, stripe=PJ)
    loft([(0.05, 0, y, 0.046, 0.044), (0.10, 0, y, 0.044, 0.042), (0.19, 0, y, 0.048, 0.046)], "Skin", n=14, lo=0.02, hi=0.01)
    seg("Shin_" + T, "Thigh_" + T, (0, y, 0.50))
    # Foot in a fluffy mule slipper: sole, bare heel, pink toe box, white fluff cuff and pompom.
    pts = []
    for i in range(28):
        t = 2 * math.pi * i / 28
        c, sn = math.cos(t), math.sin(t)
        pts.append((0.0775 + 0.1725 * math.copysign(abs(c) ** 0.8, c), y + (0.061 + 0.009 * c) * math.copysign(abs(sn) ** 0.8, sn)))
    L.prism(pts, 0.0, 0.024, "SlipperSole", "Z", bev=0.006)
    loftx([(-0.072, 0.052, y, 0.030, 0.032), (-0.04, 0.056, y, 0.037, 0.040), (0.0, 0.060, y, 0.042, 0.044), (0.05, 0.052, y, 0.036, 0.046)],
          "Skin", n=16, lo=0.02, floor=0.02)
    L.ball(0.043, 0, y, 0.085, "Skin", nu=12, nv=6)
    loftx([(0.018, 0.040, y, 0.054, 0.060), (0.08, 0.040, y, 0.060, 0.066), (0.15, 0.038, y, 0.056, 0.067), (0.21, 0.034, y, 0.044, 0.058), (0.238, 0.030, y, 0.030, 0.040)],
          "Slipper", n=20, hi=0.012, noise=0.0022, seed=11, floor=0.014)
    for i in range(11):
        a = math.radians(-12.0 + 204.0 * i / 10)
        fluff(0.022, 0.020, y + 0.062 * math.cos(a), 0.040 + 0.058 * math.sin(a), seed=20 + i)
    fluff(0.031, 0.150, y, 0.104, seed=40, nu=12, nv=8)
    grow(1.08, (0, y, 0))
    L.ball(0.048, 0, y, 0.085, "Skin", nu=12, nv=6)
    seg("Foot_" + T, "Shin_" + T, (0, y, 0.085))
leg(1, "L")
leg(-1, "R")

# ------------------------------------------------------------------ export, manifest, the C++ table
NAMES = [n for n, p, v in SEGS]
total = 0
pieces = []
for n, par, piv in SEGS:
    p = L.PIECES["SM_H_" + n]
    total += p["tris"]
    pp = SEGS[NAMES.index(par)][2] if par else Vector((0, 0, 0))
    d = piv - pp
    pieces.append({
        "name": "SM_H_" + n, "file": "SM_H_" + n + ".fbx", "parent": ("SM_H_" + par) if par else None,
        "pivot_m": [round(c, 5) for c in piv],
        "pivot_game_uu": [round(piv.x * 1900, 1), round(-piv.y * 1900, 1), round(piv.z * 1900, 1)],
        "offset_from_parent_game_uu": [round(d.x * 1900, 1), round(-d.y * 1900, 1), round(d.z * 1900, 1)],
        "bbox_min_m": [round(c, 4) for c in p["mn"]], "bbox_max_m": [round(c, 4) for c in p["mx"]],
        "size_game_uu": [round((p["mx"][i] - p["mn"][i]) * 1900, 1) for i in range(3)],
        "triangles": p["tris"], "materials": p["mats"], "open_edges": p["open_edges"]})
    print("PIECE %-18s tris=%5d open=%d inv=%d slots=%s" % (n, p["tris"], p["open_edges"], p["inverted"], ",".join(p["mats"])))
print("TOTAL segments=%d tris=%d" % (len(SEGS), total))
doc = {
    "what": "Ant Kitchen: the Human as rigid segments (Tools/blender/human_build.py). One FBX per segment, origin at its joint.",
    "conventions": {
        "blender": "metres, +Z up, the Human faces +X, his left is +Y; bind pose: standing, arms down, palms to the thighs",
        "game": "default UE FBX import: mesh-local (100x, -100y, 100z) = X forward, Y right, Z up; component scale 19 (1 m = 1900 uu)",
        "pivot_game_uu": "the joint in the bind pose, in the Human's body frame (origin on the floor between the feet)",
        "limbs": "limbs hang along -Z from their joint: upper arm 0.305 m, forearm 0.27 m, thigh 0.42 m, shin 0.415 m, ankle 0.085 m above the sole",
        "fingers": "Fingers_* is one rigid group hinged at the knuckles about the mesh-local X axis (curl towards the palm)"},
    "materials": L.MATINFO, "totals": {"segments": len(SEGS), "triangles": total}, "pieces": pieces}
if not DRY:
    os.makedirs(OUT, exist_ok=True)
    L.export_all(OUT)
    with open(os.path.join(OUT, "human_manifest.json"), "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=1)
    if INL:
        with open(INL, "w", encoding="utf-8-sig", newline="\n") as f:
            f.write("// Generated by Tools/blender/human_build.py from the model (SourceArt/Blender/human). Do not edit by hand.\n")
            f.write("// Segment mesh, parent segment (-1: the pelvis is the root), joint in the bind pose: body frame, uu, X forward, Y right, Z up, floor at 0.\n")
            f.write("static const FHumanSegDef GHumanSegs[] =\n{\n")
            for n, par, piv in SEGS:
                f.write("\t{ TEXT(\"SM_H_%s\"), %d, FVector(%.1ff, %.1ff, %.1ff) },\n" % (n, NAMES.index(par) if par else -1, piv.x * 1900, -piv.y * 1900, piv.z * 1900))
            f.write("};\n")
    print("EXPORTED %d to %s" % (len(SEGS), OUT))
print("HUMAN_BUILD_DONE")
