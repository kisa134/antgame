# Ant Kitchen: Бадрик the French bulldog as a jointed figure - rigid segments, origin at each joint (see npc_lib.py, QuadBody.cpp).
# Run:  blender --background --factory-startup --python npc_dog.py -- <out_dir> [<QuadDogData.inl>] [preview]
# Fawn, white chest, dark mask; bat ears, flat muzzle with an underbite, a barrel on short bowed legs, a stub of a tail,
# a red collar with a gold tag. Bind pose: a plank - spine and neck straight along X, legs straight down, ears up.
import bpy, sys, os, math
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else os.path.join(HERE, "..", "..", "SourceArt", "Blender", "npc_dog")
INL = argv[1] if len(argv) > 1 and argv[1] not in ("dry", "preview") else None
bpy.ops.wm.read_factory_settings(use_empty=True)
import kitchen_lib as L
import npc_lib as N
from mathutils import Vector
mat = L.mat

mat("Fawn", (0.78, 0.55, 0.32), 0.85, new=True)
mat("FawnDark", (0.20, 0.15, 0.13), 0.8, new=True)
mat("DogWhite", (0.95, 0.93, 0.88), 0.85, new=True)
mat("EarPink", (0.93, 0.62, 0.62), 0.7, new=True)
mat("DogNose", (0.05, 0.04, 0.04), 0.35, new=True)
mat("PawPad", (0.30, 0.22, 0.22), 0.6, new=True)
mat("DogEye", (0.20, 0.11, 0.05), 0.2, new=True)
mat("EyeWhite", (0.97, 0.97, 0.95), 0.25, new=True)
mat("Pupil", (0.03, 0.03, 0.04), 0.2, new=True)
mat("Mouth", (0.42, 0.10, 0.10), 0.5, new=True)
mat("Tongue", (0.95, 0.45, 0.52), 0.4, new=True)
mat("Collar", (0.80, 0.10, 0.10), 0.6, new=True)
mat("CollarTag", (0.90, 0.72, 0.20), 0.3, new=True)
mat("Tooth", (0.97, 0.96, 0.90), 0.3, new=True)

R = N.Rig("SM_Dog_")
ZS = 0.208
XH, XS = -0.104, 0.104

# ------------------------------------------------------------------ the barrel
N.eball(1.0, -0.112, 0, ZS, "Fawn", scale=(0.088, 0.080, 0.086), nu=20, nv=10)
R.seg("Hips", None, (XH, 0, ZS))

N.eball(1.0, 0.0, 0, ZS - 0.006, "Fawn", scale=(0.108, 0.090, 0.090), nu=20, nv=10)
N.eball(1.0, 0.010, 0, ZS - 0.055, "DogWhite", scale=(0.075, 0.060, 0.045), nu=16, nv=8)     # pale tummy
R.seg("Belly", "Hips", (0, 0, ZS))

N.eball(1.0, 0.112, 0, ZS - 0.004, "Fawn", scale=(0.100, 0.108, 0.102), nu=22, nv=10)        # the big chest
N.eball(1.0, 0.180, 0, ZS - 0.036, "DogWhite", scale=(0.048, 0.074, 0.068), nu=16, nv=8)     # white blaze
R.seg("Chest", "Belly", (XS, 0, ZS))

XN, ZN = 0.162, 0.251
XHD = 0.212
N.loftx([(XN - 0.012, ZN, 0, 0.074, 0.074), (XN + 0.022, ZN, 0, 0.070, 0.070), (XHD + 0.006, ZN, 0, 0.066, 0.066)], "Fawn", n=20, lo=0.03, hi=0.03)
N.band_x(XN + 0.022, 0, ZN, lambda xx: 0.070, lambda xx: 0.070, "Collar", w=0.024, k=1.05)   # the red collar
N.eball(1.0, XN + 0.026, 0, ZN - 0.086, "CollarTag", scale=(0.005, 0.015, 0.015), nu=12, nv=6)
R.seg("Neck", "Chest", (XN, 0, ZN))

# ------------------------------------------------------------------ the head: wide, flat-faced, worried
HX, HZ = XHD + 0.050, ZN + 0.014
N.eball(1.0, HX, 0, HZ, "Fawn", scale=(0.076, 0.090, 0.076), nu=24, nv=12)
N.eball(1.0, HX + 0.058, 0, HZ - 0.018, "FawnDark", scale=(0.034, 0.062, 0.042), nu=18, nv=10)                 # the dark mask: a flat muzzle
N.eball(1.0, HX + 0.090, 0, HZ + 0.000, "DogNose", scale=(0.012, 0.022, 0.014), nu=12, nv=8)
for s in (1, -1):
    N.eball(1.0, HX + 0.060, s * 0.036, HZ - 0.040, "FawnDark", scale=(0.032, 0.032, 0.030), nu=14, nv=8)      # the flews
    N.eball(1.0, HX + 0.056, s * 0.054, HZ + 0.020, "EyeWhite", scale=(0.016, 0.023, 0.023), nu=16, nv=10)
    N.eball(1.0, HX + 0.064, s * 0.054, HZ + 0.019, "DogEye", scale=(0.012, 0.019, 0.019), nu=14, nv=8)
    N.eball(1.0, HX + 0.073, s * 0.053, HZ + 0.021, "Pupil", scale=(0.006, 0.010, 0.010), nu=10, nv=6)
    L.rbox(0.014, 0.040, 0.010, HX + 0.054, s * 0.050, HZ + 0.050, "FawnDark", 0.004, 2, rot=(s * -18.0, 0, 0))  # worried brows
for k, dz in enumerate((0.036, 0.050)):                                                                         # wrinkles over the nose
    L.rbox(0.010, 0.050 - k * 0.012, 0.007, HX + 0.066 - k * 0.012, 0, HZ + dz, "FawnDark", 0.003, 2, rot=(0, 28, 0))
R.seg("Head", "Neck", (XHD, 0, ZN))

# The jaw: an underbite with two teeth showing.
N.eball(1.0, HX + 0.058, 0, HZ - 0.058, "FawnDark", scale=(0.034, 0.054, 0.018), nu=16, nv=8)
N.eball(1.0, HX + 0.052, 0, HZ - 0.047, "Mouth", scale=(0.026, 0.040, 0.007), nu=12, nv=6)
for s in (1, -1):
    N.cone(0.006, 0.016, HX + 0.080, s * 0.022, HZ - 0.052, "Tooth", n=8)
R.seg("Jaw", "Head", (HX + 0.020, 0, HZ - 0.052))

# Bat ears: tall, wide at the base, rounded, standing straight up.
for s, T in ((1, "L"), (-1, "R")):
    ex, ey, ez = HX - 0.020, s * 0.060, HZ + 0.050
    # (an upright rounded leaf: an ellipsoid standing on the skull, thin front to back, the pink inside facing forward)
    N.eball(1.0, ex, ey + s * 0.012, ez + 0.050, "Fawn", scale=(0.013, 0.040, 0.062), nu=16, nv=10, rot=(-s * 14.0, 0, 0))
    N.eball(1.0, ex + 0.008, ey + s * 0.012, ez + 0.048, "EarPink", scale=(0.009, 0.028, 0.048), nu=14, nv=8, rot=(-s * 14.0, 0, 0))
    R.seg("Ear" + T, "Head", (ex, ey, ez))

# The stub of a tail.
XT, ZT = -0.186, 0.238
N.loftx([(XT - 0.040, ZT, 0, 0.016, 0.016), (XT - 0.016, ZT, 0, 0.022, 0.022), (XT + 0.010, ZT, 0, 0.026, 0.026)], "Fawn", n=12, lo=0.014, hi=0.02)
R.seg("Tail1", "Hips", (XT, 0, ZT))

# The tongue: lies along +X from the back of the mouth; the game stretches it out to an ant and back.
TX, TZ = HX + 0.030, HZ - 0.044
TLEN = 0.100
N.loftx([(TX, TZ, 0, 0.005, 0.016), (TX + TLEN * 0.6, TZ, 0, 0.006, 0.020), (TX + TLEN - 0.012, TZ, 0, 0.005, 0.019)], "Tongue", n=12, lo=0.004, hi=0.012)
R.seg("Tongue", "Head", (TX, 0, TZ))

# ------------------------------------------------------------------ legs: short, thick, set wide
def paw(x, y, sole):
    N.eball(1.0, x + 0.014, y, sole + 0.021, "Fawn", scale=(0.040, 0.034, 0.022), nu=16, nv=8, floor=sole + 0.003)
    for dy in (-0.020, 0.0, 0.020):
        N.eball(1.0, x + 0.045 - abs(dy) * 0.35, y + dy, sole + 0.014, "Fawn", scale=(0.015, 0.012, 0.013), nu=10, nv=6, floor=sole + 0.003)
        N.eball(1.0, x + 0.042 - abs(dy) * 0.35, y + dy, sole + 0.005, "PawPad", scale=(0.008, 0.007, 0.005), nu=8, nv=6, floor=sole)
    N.eball(1.0, x + 0.010, y, sole + 0.006, "PawPad", scale=(0.017, 0.017, 0.006), nu=10, nv=6, floor=sole)

FX, FY, FZ = 0.114, 0.076, 0.168
ZE, ZW = 0.100, 0.035
def front(s, T):
    y = s * FY
    N.loft([(ZE, FX, y, 0.033, 0.032), (0.135, FX, y, 0.039, 0.036), (FZ, FX, y, 0.042, 0.038)], "Fawn", n=16, lo=0.033, hi=0.034)
    N.eball(1.0, FX, y * 1.08, FZ - 0.006, "Fawn", scale=(0.046, 0.034, 0.050), nu=14, nv=8)
    R.seg("F%sU" % T, "Chest", (FX, y, FZ))
    N.loft([(ZW, FX, y, 0.028, 0.028), (0.070, FX, y, 0.030, 0.029), (ZE, FX, y, 0.033, 0.032)], "Fawn", n=16, lo=0.024, hi=0.033)
    R.seg("F%sL" % T, "F%sU" % T, (FX, y, ZE))
    paw(FX, y, 0.0)
    N.eball(0.029, FX, y, ZW, "Fawn", nu=12, nv=6)
    R.seg("F%sP" % T, "F%sL" % T, (FX, y, ZW))

BX, BY, BZ = -0.114, 0.064, 0.168
ZK, ZHK, ZSOLE = 0.098, 0.030, -0.020
def hind(s, T):
    y = s * BY
    N.loft([(ZK, BX, y, 0.032, 0.031), (0.135, BX, y, 0.040, 0.034), (BZ, BX, y, 0.044, 0.036)], "Fawn", n=16, lo=0.032, hi=0.03)
    N.eball(1.0, BX - 0.004, y * 1.12, 0.146, "Fawn", scale=(0.054, 0.038, 0.056), nu=16, nv=8)       # the haunch
    R.seg("H%sU" % T, "Hips", (BX, y, BZ))
    N.loft([(ZHK, BX, y, 0.024, 0.024), (0.060, BX, y, 0.027, 0.026), (ZK, BX, y, 0.032, 0.031)], "Fawn", n=16, lo=0.022, hi=0.032)
    R.seg("H%sL" % T, "H%sU" % T, (BX, y, ZK))
    N.loft([(ZSOLE + 0.018, BX, y, 0.023, 0.024), (ZHK, BX, y, 0.024, 0.024)], "Fawn", n=14, lo=0.008, hi=0.024)
    paw(BX + 0.004, y, ZSOLE)
    R.seg("H%sP" % T, "H%sL" % T, (BX, y, ZHK))

front(1, "L"); front(-1, "R")
hind(1, "L"); hind(-1, "R")

ORDER = ["Hips", "Belly", "Chest", "Neck", "Head", "Jaw", "EarL", "EarR", "Tail1", None, None, "Tongue",
         "FLU", "FLL", "FLP", "FRU", "FRL", "FRP", "HLU", "HLL", "HLP", "HRU", "HRL", "HRP"]
EXTRA = ("// How far the sole is below the paw's joint (front, hind), uu; the length of the tongue's mesh, uu.\n"
         "static const float GDogPawDrop[2] = { %.1ff, %.1ff };\nstatic const float GDogTongueLen = %.1ff;\n" % (ZW * 1900, (ZHK - ZSOLE) * 1900, TLEN * 1900))
R.finish(OUT, "Ant Kitchen: Бадрик the French bulldog as rigid segments (Tools/blender/npc_dog.py). One FBX per segment, origin at its joint.",
         inl=INL, table="GDogSegs", order=ORDER, dry=("dry" in argv), inl_extra=EXTRA)

if "preview" in argv:
    POSES = {
        "stand": {"Neck": (0, -34, 0), "Head": (0, 30, 0), "Tail1": (0, 50, 0), "Tongue": (0, 0, 0),
                  "HLU": (0, -32, 0), "HLL": (0, 64, 0), "HLP": (0, -32, 0), "HRU": (0, -32, 0), "HRL": (0, 64, 0), "HRP": (0, -32, 0),
                  "FLU": (4, 14, 0), "FLL": (0, -28, 0), "FLP": (-4, 14, 0), "FRU": (-4, 14, 0), "FRL": (0, -28, 0), "FRP": (4, 14, 0)},
    }
    N.preview(R, os.path.join(OUT, "preview_*.jpg"), [
        ("turn", [("stand", (0, 0.72, 0), 0), ("stand", (0, 0.24, 0), -90), ("stand", (0, -0.24, 0), -35), ("stand", (0, -0.72, 0), 180)], (2.5, 0, 0.34), (0, 0, 0.17), 62, (1600, 640)),
    ], POSES, floor_size=1.6, wall_x=-0.7)
print("NPC_DOG_DONE")
