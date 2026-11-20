# Ant Kitchen: the cat as a jointed figure - 23 rigid segments, origin at each joint (see npc_lib.py, QuadBody.cpp).
# Run:  blender --background --factory-startup --python npc_cat.py -- <out_dir> [<QuadCatData.inl>] [preview]
# A grey tabby: white socks, bib and muzzle, green eyes, pink toe beans (an ant sees the underside of the paws).
# Bind pose: a plank - spine, neck and tail straight along X, legs straight down, ears up.
import bpy, sys, os, math
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else os.path.join(HERE, "..", "..", "SourceArt", "Blender", "npc_cat")
INL = argv[1] if len(argv) > 1 and argv[1] not in ("dry", "preview") else None
bpy.ops.wm.read_factory_settings(use_empty=True)
import kitchen_lib as L
import npc_lib as N
from mathutils import Vector
mat = L.mat

mat("Fur", (0.47, 0.46, 0.52), 0.85, new=True)
mat("FurDark", (0.21, 0.20, 0.25), 0.85, new=True)
mat("FurWhite", (0.95, 0.94, 0.91), 0.85, new=True)
mat("EarPink", (0.93, 0.62, 0.62), 0.7, new=True)
mat("NosePink", (0.90, 0.48, 0.50), 0.45, new=True)
mat("PawPad", (0.88, 0.52, 0.55), 0.6, new=True)
mat("CatEye", (0.50, 0.80, 0.22), 0.2, new=True)
mat("Pupil", (0.03, 0.03, 0.04), 0.2, new=True)
mat("Whisker", (0.97, 0.97, 0.97), 0.5, new=True)
mat("Mouth", (0.42, 0.10, 0.10), 0.5, new=True)

R = N.Rig("SM_Cat_")
ZS = 0.200          # the spine's height in the bind pose
XH, XS = -0.130, 0.130

def stripes(cx, rx, ry, rz, cz, xs):
    for x in xs:
        N.band_x(x, 0.0, cz, lambda xx: N.ell_r(xx - cx, rx, ry), lambda xx: N.ell_r(xx - cx, rx, rz), "FurDark", w=0.03, k=1.018)

# ------------------------------------------------------------------ body
N.eball(1.0, -0.135, 0, ZS, "Fur", scale=(0.098, 0.074, 0.082), nu=20, nv=10)
stripes(-0.135, 0.098, 0.074, 0.082, ZS, (-0.170, -0.115))
R.seg("Hips", None, (XH, 0, ZS))

N.eball(1.0, 0.0, 0, ZS - 0.003, "Fur", scale=(0.118, 0.072, 0.080), nu=20, nv=10)
stripes(0.0, 0.118, 0.072, 0.080, ZS - 0.003, (-0.045, 0.010, 0.065))
N.eball(1.0, 0.0, 0, ZS - 0.045, "FurWhite", scale=(0.085, 0.050, 0.040), nu=16, nv=8)      # pale tummy
R.seg("Belly", "Hips", (0, 0, ZS))

N.eball(1.0, 0.135, 0, ZS, "Fur", scale=(0.094, 0.075, 0.087), nu=20, nv=10)
stripes(0.135, 0.094, 0.075, 0.087, ZS, (0.11,))
N.eball(1.0, 0.200, 0, ZS - 0.030, "FurWhite", scale=(0.040, 0.052, 0.056), nu=16, nv=8)    # the bib
R.seg("Chest", "Belly", (XS, 0, ZS))

XN, ZN = 0.195, 0.225
XHD = 0.270
N.loftx([(XN - 0.008, ZN, 0, 0.053, 0.053), (XN + 0.035, ZN, 0, 0.050, 0.050), (XHD + 0.004, ZN, 0, 0.047, 0.047)], "Fur", n=18, lo=0.03, hi=0.03)
N.band_x(XN + 0.035, 0, ZN, lambda xx: 0.050, lambda xx: 0.050, "FurDark", w=0.024, k=1.03)
R.seg("Neck", "Chest", (XN, 0, ZN))

# ------------------------------------------------------------------ head: big, round, wide-eyed
HX, HZ = XHD + 0.046, ZN + 0.010
N.eball(1.0, HX, 0, HZ, "Fur", scale=(0.067, 0.076, 0.063), nu=24, nv=12)
for s in (1, -1):
    N.eball(1.0, HX + 0.012, s * 0.047, HZ - 0.022, "Fur", scale=(0.036, 0.036, 0.030), nu=14, nv=8)             # cheeks
    N.eball(1.0, HX + 0.058, s * 0.017, HZ - 0.024, "FurWhite", scale=(0.022, 0.023, 0.019), nu=14, nv=8)        # whisker pads
    N.eball(1.0, HX + 0.049, s * 0.036, HZ + 0.012, "CatEye", scale=(0.014, 0.020, 0.022), nu=16, nv=10)
    N.eball(1.0, HX + 0.058, s * 0.036, HZ + 0.012, "Pupil", scale=(0.007, 0.008, 0.018), nu=12, nv=8)
    for k, (dz, ang) in enumerate(((0.004, 12.0), (-0.004, 0.0), (-0.012, -12.0))):
        a = math.radians(ang)
        p0 = Vector((HX + 0.062, s * 0.030, HZ - 0.022 + dz))
        p1 = p0 + Vector((0.006, s * 0.075 * math.cos(a), 0.075 * math.sin(a)))
        L.tube([p0, (p0 + p1) / 2 + Vector((0.004, 0, 0)), p1], [0.0020, 0.0016, 0.0009], "Whisker", 5)
N.eball(1.0, HX + 0.050, 0, HZ - 0.020, "FurWhite", scale=(0.026, 0.034, 0.024), nu=16, nv=8)                     # muzzle
N.eball(1.0, HX + 0.076, 0, HZ - 0.010, "NosePink", scale=(0.008, 0.013, 0.008), nu=12, nv=6)
for dy in (-0.020, 0.0, 0.020):                                                                                   # the tabby "M"
    L.rbox(0.040, 0.008, 0.010, HX + 0.012, dy, HZ + 0.060 - abs(dy) * 0.25, "FurDark", 0.003, 2, rot=(0, 20, 0))
R.seg("Head", "Neck", (XHD, 0, ZN))

N.eball(1.0, HX + 0.046, 0, HZ - 0.040, "FurWhite", scale=(0.024, 0.028, 0.013), nu=14, nv=8)                    # chin
N.eball(1.0, HX + 0.044, 0, HZ - 0.031, "Mouth", scale=(0.020, 0.022, 0.006), nu=12, nv=6)
R.seg("Jaw", "Head", (HX + 0.018, 0, HZ - 0.036))

for s, T in ((1, "L"), (-1, "R")):
    ex, ey, ez = HX - 0.012, s * 0.046, HZ + 0.044
    N.cone(0.034, 0.078, ex, ey, ez, "Fur", n=14, scale=(0.50, 1.0), rot=(-s * 12.0, 0, 0))
    N.cone(0.024, 0.060, ex + 0.008, ey, ez + 0.004, "EarPink", n=12, scale=(0.50, 1.0), rot=(-s * 12.0, 0, 0))
    R.seg("Ear" + T, "Head", (ex, ey, ez))

# ------------------------------------------------------------------ tail: three pieces, ringed, dark tip
XT, ZT = -0.215, 0.225
TL = 0.105
for i, (r0, r1) in enumerate(((0.026, 0.024), (0.024, 0.021), (0.021, 0.018))):
    x1 = XT - TL * i
    x0 = x1 - TL
    q = (x1 - x0) / 4
    rr = [r1 + (r0 - r1) * k / 4 for k in range(5)]
    bands = ["Fur", "FurDark", "Fur", "FurDark"] if i < 2 else ["FurDark", "FurDark", "Fur", "FurDark"]
    N.loftx([(x0 + q * k, ZT, 0, rr[k], rr[k]) for k in range(5)], None, n=14, lo=r1 * 0.9, hi=r0 * 0.9, bands=bands)
    R.seg("Tail%d" % (i + 1), "Hips" if i == 0 else "Tail%d" % i, (x1, 0, ZT))

# ------------------------------------------------------------------ legs
def paw(x, y, sole, s):
    # A white mitten with three toes; pink beans underneath.
    N.eball(1.0, x + 0.012, y, sole + 0.017, "FurWhite", scale=(0.033, 0.027, 0.018), nu=16, nv=8, floor=sole + 0.0025)
    for dy in (-0.016, 0.0, 0.016):
        N.eball(1.0, x + 0.037 - abs(dy) * 0.35, y + dy, sole + 0.012, "FurWhite", scale=(0.013, 0.010, 0.011), nu=10, nv=6, floor=sole + 0.0025)
        N.eball(1.0, x + 0.034 - abs(dy) * 0.35, y + dy, sole + 0.004, "PawPad", scale=(0.007, 0.006, 0.004), nu=8, nv=6, floor=sole)
    N.eball(1.0, x + 0.008, y, sole + 0.005, "PawPad", scale=(0.014, 0.014, 0.005), nu=10, nv=6, floor=sole)

FX, FY, FZ = 0.135, 0.050, 0.185
ZE, ZW = 0.100, 0.028
def front(s, T):
    y = s * FY
    N.loft([(ZE, FX, y, 0.024, 0.023), (0.140, FX, y, 0.030, 0.028), (FZ, FX, y, 0.034, 0.030)], "Fur", n=16, lo=0.024, hi=0.030)
    N.eball(1.0, FX, y * 1.12, FZ - 0.010, "Fur", scale=(0.040, 0.028, 0.046), nu=14, nv=8)
    R.seg("F%sU" % T, "Chest", (FX, y, FZ))
    N.loft([(ZW, FX, y, 0.020, 0.020), (0.052, FX, y, 0.021, 0.021), (0.066, FX, y, 0.022, 0.022), (ZE, FX, y, 0.024, 0.023)], None, n=16, lo=0.018, hi=0.024,
           bands=["FurWhite", "FurWhite", "Fur"])
    R.seg("F%sL" % T, "F%sU" % T, (FX, y, ZE))
    paw(FX, y, 0.0, s)
    N.eball(0.021, FX, y, ZW, "FurWhite", nu=12, nv=6)
    R.seg("F%sP" % T, "F%sL" % T, (FX, y, ZW))

BX, BY, BZ = -0.135, 0.052, 0.185
ZK, ZHK, ZSOLE = 0.085, -0.010, -0.070
def hind(s, T):
    y = s * BY
    N.loft([(ZK, BX, y, 0.027, 0.026), (0.130, BX, y, 0.036, 0.030), (BZ, BX, y, 0.040, 0.032)], "Fur", n=16, lo=0.027, hi=0.030)
    N.eball(1.0, BX - 0.004, y * 1.15, 0.150, "Fur", scale=(0.052, 0.036, 0.062), nu=16, nv=8)       # the haunch
    R.seg("H%sU" % T, "Hips", (BX, y, BZ))
    N.loft([(ZHK, BX, y, 0.018, 0.018), (0.030, BX, y, 0.020, 0.020), (ZK, BX, y, 0.026, 0.025)], "Fur", n=16, lo=0.018, hi=0.026)
    R.seg("H%sL" % T, "H%sU" % T, (BX, y, ZK))
    N.loft([(ZSOLE + 0.016, BX, y, 0.017, 0.018), (ZHK, BX, y, 0.018, 0.018)], "FurWhite", n=14, lo=0.008, hi=0.018)
    paw(BX + 0.004, y, ZSOLE, s)
    R.seg("H%sP" % T, "H%sL" % T, (BX, y, ZHK))

front(1, "L"); front(-1, "R")
hind(1, "L"); hind(-1, "R")

ORDER = ["Hips", "Belly", "Chest", "Neck", "Head", "Jaw", "EarL", "EarR", "Tail1", "Tail2", "Tail3", None,
         "FLU", "FLL", "FLP", "FRU", "FRL", "FRP", "HLU", "HLL", "HLP", "HRU", "HRL", "HRP"]
EXTRA = "// How far the sole is below the paw's joint (front, hind), uu.\nstatic const float GCatPawDrop[2] = { %.1ff, %.1ff };\n" % (ZW * 1900, (ZHK - ZSOLE) * 1900)
R.finish(OUT, "Ant Kitchen: the cat as rigid segments (Tools/blender/npc_cat.py). One FBX per segment, origin at its joint.",
         inl=INL, table="GCatSegs", order=ORDER, dry=("dry" in argv), inl_extra=EXTRA)

if "preview" in argv:
    POSES = {
        "stand": {"_root": (0, 0, 0.0), "Neck": (0, -48, 0), "Head": (0, 40, 0), "Tail1": (0, 50, 0), "Tail2": (0, 20, 0), "Tail3": (0, -25, 0),
                  "HLU": (0, -50, 0), "HLL": (0, 100, 0), "HLP": (0, -50, 0), "HRU": (0, -50, 0), "HRL": (0, 100, 0), "HRP": (0, -50, 0),
                  "FLU": (0, 12, 0), "FLL": (0, -24, 0), "FLP": (0, 12, 0), "FRU": (0, 12, 0), "FRL": (0, -24, 0), "FRP": (0, 12, 0)},
        "bind": {},
    }
    N.preview(R, os.path.join(OUT, "preview_*.jpg"), [
        ("turn", [("stand", (0, 0.75, 0), 0), ("stand", (0, 0.25, 0), -90), ("stand", (0, -0.25, 0), -35), ("stand", (0, -0.75, 0), 180)], (2.6, 0, 0.32), (0, 0, 0.16), 60, (1600, 700)),
        ("ant", [("stand", (0, 0, 0), -25)], (0.62, -0.10, 0.012), (0.1, 0, 0.19), 22, (1000, 800)),
    ], POSES, floor_size=1.6, wall_x=-0.7)
print("NPC_CAT_DONE")
