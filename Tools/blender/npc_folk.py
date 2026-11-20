# Ant Kitchen: grandma and the kid - the Human's 19-segment figure (human_build.py) with their own proportions and clothes.
# Run:  blender --background --factory-startup --python npc_folk.py -- gran|kid <out_dir> [<Human*Data.inl>] [preview]
# Both are modelled at the Dad's nominal height and scaled in the game (grandma x0.88, the kid x0.50): the pose code
# (HumanBody.cpp) works in the Dad's units, so its reaches and steps shrink with the figure.
# Same frame and bind pose as the Dad: metres, +Z up, faces +X, left is +Y; standing, arms hanging, palms to the thighs.
import bpy, sys, os, math
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
WHO = argv[0] if argv else "gran"
OUT = argv[1] if len(argv) > 1 else os.path.join(HERE, "..", "..", "SourceArt", "Blender", "npc_" + WHO)
INL = argv[2] if len(argv) > 2 and argv[2] not in ("dry", "preview") else None
bpy.ops.wm.read_factory_settings(use_empty=True)
import kitchen_lib as L
import npc_lib as N
from mathutils import Vector, Matrix
mat = L.mat
GRAN = WHO == "gran"

mat("Skin", (0.90, 0.60, 0.48) if GRAN else (0.95, 0.66, 0.52), 0.55, new=True)
mat("EyeWhite", (0.97, 0.97, 0.95), 0.25, new=True)
mat("Pupil", (0.03, 0.03, 0.04), 0.2, new=True)
mat("Mouth", (0.50, 0.14, 0.14), 0.5, new=True)
if GRAN:
    mat("Cardigan", (0.55, 0.30, 0.52), 0.9, new=True)       # plum cardigan
    mat("Blouse", (0.93, 0.90, 0.80), 0.9, new=True)
    mat("Skirt", (0.24, 0.30, 0.42), 0.9, new=True)          # long dark skirt
    mat("SkirtDot", (0.80, 0.82, 0.90), 0.9, new=True)
    mat("Stocking", (0.78, 0.62, 0.50), 0.8, new=True)
    mat("Slipper", (0.45, 0.62, 0.40), 0.95, new=True)       # green felt slippers
    mat("Fluff", (0.98, 0.96, 0.92), 0.95, new=True)
    mat("SlipperSole", (0.74, 0.62, 0.46), 0.8, new=True)
    mat("HairGrey", (0.86, 0.86, 0.88), 0.7, new=True)
    mat("Glasses", (0.16, 0.12, 0.10), 0.3, new=True)
    TOP, TRIM, LEGS, LEGS2, SHOE, SOLE, HAIR = "Cardigan", "Blouse", "Skirt", "SkirtDot", "Slipper", "SlipperSole", "HairGrey"
else:
    mat("KidShirt", (0.95, 0.75, 0.15), 0.9, new=True)       # yellow T-shirt
    mat("KidTrim", (0.90, 0.30, 0.20), 0.9, new=True)
    mat("KidShorts", (0.20, 0.42, 0.75), 0.9, new=True)
    mat("Sock", (0.96, 0.96, 0.94), 0.9, new=True)
    mat("Sneaker", (0.88, 0.20, 0.20), 0.7, new=True)
    mat("SneakerSole", (0.96, 0.96, 0.94), 0.6, new=True)
    mat("HairBrown", (0.45, 0.26, 0.10), 0.7, new=True)
    mat("Cheek", (0.96, 0.52, 0.48), 0.6, new=True)
    TOP, TRIM, LEGS, LEGS2, SHOE, SOLE, HAIR = "KidShirt", "KidTrim", "KidShorts", "KidShorts", "Sneaker", "SneakerSole", "HairBrown"

# ---- proportions (the Dad's nominal 1.76 m frame)
if GRAN:
    ZHIP, ZKNEE, ZBELLY, ZCHEST, ZNECK, ZHEAD = 0.86, 0.47, 0.97, 1.16, 1.385, 1.455
    YHIP, YSH, ZSH, ZEL, ZWR = 0.115, 0.232, 1.325, 1.035, 0.785
    HEADK, HS = 1.16, 1.25
    WAIST, CHESTW = 1.12, 1.02
else:
    ZHIP, ZKNEE, ZBELLY, ZCHEST, ZNECK, ZHEAD = 0.70, 0.385, 0.80, 0.955, 1.165, 1.215
    YHIP, YSH, ZSH, ZEL, ZWR = 0.105, 0.205, 1.105, 0.875, 0.665
    HEADK, HS = 1.95, 1.35
    WAIST, CHESTW = 1.0, 0.88

R = N.Rig("SM_G_" if GRAN else "SM_C_")
loft, loftx = N.loft, N.loftx
STR = (LEGS, LEGS2, 6) if GRAN else None

# ------------------------------------------------------------------ torso
w = WAIST
loft([(ZHIP - 0.075, 0.0, 0, 0.105 * w, 0.160 * w), (ZHIP - 0.02, 0.0, 0, 0.126 * w, 0.184 * w), (ZHIP + 0.05, 0.006, 0, 0.136 * w, 0.188 * w), (ZHIP + 0.115, 0.010, 0, 0.134 * w, 0.182 * w)],
     LEGS, n=36, lo=0.05, hi=0.03, stripe=STR)
R.seg("Pelvis", None, (0, 0, ZHIP))

zb = ZBELLY
loft([(zb - 0.045, 0.012, 0, 0.144 * w, 0.192 * w), (zb, 0.022, 0, 0.158 * w, 0.197 * w), (zb + 0.07, 0.034, 0, 0.168 * w, 0.200 * w), (zb + 0.14, 0.026, 0, 0.156 * w, 0.196 * w),
      (ZCHEST + 0.045, 0.012, 0, 0.134 * w, 0.190 * w)], TOP, n=24, lo=0.012, hi=0.04)
loft([(zb - 0.048, 0.012, 0, 0.148 * w, 0.196 * w), (zb - 0.026, 0.016, 0, 0.154 * w, 0.199 * w)], TRIM, n=24, lo=0.006, hi=0.006)
R.seg("Belly", "Pelvis", (0, 0, zb))

c = CHESTW
zc = ZCHEST
loft([(zc - 0.04, 0.012, 0, 0.136 * c, 0.190 * c), (zc + 0.03, 0.016 if GRAN else 0.012, 0, 0.142 * c, 0.198 * c), (zc + 0.11, 0.010, 0, 0.134 * c, 0.212 * c), (ZSH + 0.01, 0.0, 0, 0.116 * c, 0.212 * c),
      (ZSH + 0.052, 0.0, 0, 0.090 * c, 0.176 * c), (ZNECK + 0.010, 0.0, 0, 0.066, 0.086)], TOP, n=24, lo=0.04, hi=0.012)
loft([(ZNECK + 0.007, 0.0, 0, 0.072, 0.080), (ZNECK + 0.027, 0.002, 0, 0.066, 0.072)], TRIM, n=20, lo=0.006, hi=0.006)
if GRAN:
    for k in range(3):   # cardigan buttons
        N.eball(1.0, 0.150 * c - k * 0.004, 0, zc + 0.10 - k * 0.07, "Blouse", scale=(0.008, 0.014, 0.014), nu=10, nv=6)
R.seg("Chest", "Belly", (0, 0, zc))

loft([(ZNECK - 0.025, 0.0, 0, 0.052, 0.055), (ZHEAD + 0.015, 0.004, 0, 0.050, 0.053)], "Skin", n=14, lo=0.03, hi=0.03)
R.seg("Neck", "Chest", (0, 0, ZNECK))

# ------------------------------------------------------------------ head
z0 = ZHEAD
def hz(v):
    return z0 + (v - 1.535)
loft([(hz(1.545), 0.012, 0, 0.062, 0.060), (hz(1.575), 0.012, 0, 0.094, 0.089), (hz(1.61), 0.008, 0, 0.108, 0.102), (hz(1.65), 0.004, 0, 0.112, 0.104),
      (hz(1.69), 0.0, 0, 0.111, 0.103), (hz(1.725), -0.002, 0, 0.100, 0.094), (hz(1.75), -0.004, 0, 0.078, 0.074)], "Skin", n=28, lo=0.012, hi=0.022)
for s in (1, -1):
    ek = 1.0 if GRAN else 1.25
    L.ball(0.0245 * ek, 0.093, s * 0.043, hz(1.672), "EyeWhite", scale=(0.8, 1.0, 1.05), nu=14, nv=8)
    L.ball(0.0125 * ek, 0.1085, s * 0.041, hz(1.668), "Pupil", scale=(0.6, 1.0, 1.0), nu=10, nv=6)
    L.ball(0.030, 0.0, s * 0.104, hz(1.652), "Skin", scale=(0.55, 0.4, 1.0), nu=12, nv=8)
    if GRAN:
        L.rbox(0.016, 0.050, 0.012, 0.106, s * 0.044, hz(1.716), "HairGrey", 0.004, 2, rot=(s * -8.0, 0, 0))    # kind brows
        # glasses: a round frame in front of each eye, the arms back to the ears
        L.lathe([(0.030, -0.004), (0.036, -0.004), (0.036, 0.004), (0.030, 0.004)], "Glasses", 0.117, s * 0.043, hz(1.672), n=18, axis="X", closed=True)
        L.rbox(0.110, 0.006, 0.006, 0.060, s * 0.090, hz(1.676), "Glasses", 0.002, 1)
    else:
        L.rbox(0.014, 0.040, 0.010, 0.110, s * 0.044, hz(1.722), "HairBrown", 0.004, 2, rot=(s * -10.0, 0, 0))
        N.eball(1.0, 0.092, s * 0.068, hz(1.628), "Cheek", scale=(0.012, 0.024, 0.020), nu=12, nv=6)
if GRAN:
    L.rbox(0.008, 0.016, 0.006, 0.126, 0.0, hz(1.676), "Glasses", 0.002, 1)
L.ball(0.027 if GRAN else 0.020, 0.120, 0.0, hz(1.640), "Skin", scale=(1.0, 0.92, 1.05), nu=14, nv=8)                  # nose
L.rbox(0.014, 0.046 if GRAN else 0.034, 0.011, 0.099, 0.0, hz(1.585), "Mouth", 0.004, 2)
hr = []
for a in (80.0, 62.0, 44.0, 24.0):
    hr.append((0.140 * math.cos(math.radians(a)), 0, 0, 0.128 * math.sin(math.radians(a)), 0.119 * math.sin(math.radians(a))))
loft(hr, HAIR, n=24, hi=0.140 * (1 - math.cos(math.radians(24.0))),
     M=Matrix.Translation((-0.004, 0, hz(1.652))) @ Matrix.Rotation(math.radians(-25.0 if GRAN else -8.0), 4, "Y"), noise=0.0015, seed=3)
if GRAN:
    N.fluff(0.050, -0.105, 0.0, hz(1.735), "HairGrey", seed=5, nu=14, nv=10, scale=(1.0, 1.1, 1.0), amp=0.08)           # the bun
else:
    N.fluff(0.034, 0.070, 0.0, hz(1.760), "HairBrown", seed=5, nu=12, nv=8, scale=(1.2, 1.5, 0.7))                      # a tuft
    N.fluff(0.022, 0.020, 0.03, hz(1.790), "HairBrown", seed=9, nu=10, nv=6, scale=(1.0, 1.0, 1.3))
N.grow(HEADK, (0, 0, z0))
R.seg("Head", "Neck", (0, 0, z0))

# ------------------------------------------------------------------ arms
def arm(s, T):
    y = s * YSH
    zs, ze, zw = ZSH, ZEL, ZWR
    ak = 1.0 if GRAN else 1.12
    if GRAN:
        # long cardigan sleeves down to the wrist
        loft([(ze - 0.01, 0, y, 0.066, 0.066), (zs - 0.11, 0, y, 0.076, 0.076), (zs, 0, y, 0.076, 0.078)], TOP, n=18, lo=0.05, hi=0.05)
        R.seg("UpperArm_" + T, "Chest", (0, y, zs))
        loft([(zw + 0.02, 0, y, 0.050, 0.047), (zw + 0.065, 0, y, 0.056, 0.053), (zw + 0.185, 0, y, 0.064, 0.062), (ze, 0, y, 0.064, 0.064)], TOP, n=16, lo=0.012, hi=0.058)
        loft([(zw - 0.005, 0, y, 0.044, 0.040), (zw + 0.03, 0, y, 0.046, 0.043)], "Skin", n=14, lo=0.02, hi=0.01)
        R.seg("Forearm_" + T, "UpperArm_" + T, (0, y, ze))
    else:
        loft([(zs - 0.12, 0, y, 0.074 * ak, 0.074 * ak), (zs - 0.07, 0, y, 0.076 * ak, 0.076 * ak), (zs, 0, y, 0.076 * ak, 0.078 * ak)], TOP, n=18, lo=0.006, hi=0.05)
        loft([(zs - 0.127, 0, y, 0.0775 * ak, 0.0775 * ak), (zs - 0.103, 0, y, 0.0785 * ak, 0.0785 * ak)], TRIM, n=18, lo=0.004, hi=0.004)
        loft([(ze, 0, y, 0.058 * ak, 0.058 * ak), (ze + 0.06, 0, y, 0.064 * ak, 0.064 * ak), (zs - 0.11, 0, y, 0.066 * ak, 0.066 * ak)], "Skin", n=16, lo=0.058, hi=0.01)
        R.seg("UpperArm_" + T, "Chest", (0, y, zs))
        loft([(zw, 0, y, 0.046, 0.042), (zw + 0.06, 0, y, 0.054, 0.050), (zw + 0.15, 0, y, 0.062 * ak, 0.060 * ak), (ze, 0, y, 0.058 * ak, 0.058 * ak)], "Skin", n=16, lo=0.03, hi=0.058)
        R.seg("Forearm_" + T, "UpperArm_" + T, (0, y, ze))
    L.rbox(0.086 * HS, 0.036 * HS, 0.100 * HS, 0.004, y, zw - 0.050 * HS, "Skin", 0.014 * HS, 3)
    L.ball(0.043, 0, y, zw, "Skin", nu=12, nv=6)
    b = Vector((0.036 * HS, y - s * 0.004, zw - 0.036 * HS))
    N.digit([b, b + Vector((0.030, 0, -0.028)) * HS, b + Vector((0.040, -s * 0.010, -0.062)) * HS], 0.0168 * HS, "Skin", 10)
    R.seg("Hand_" + T, "Forearm_" + T, (0, y, zw))
    zk = zw - 0.092 * HS
    for dx, ln in ((0.0315, 0.078), (0.0105, 0.088), (-0.0105, 0.081), (-0.0315, 0.064)):
        ln *= HS * (1.0 if GRAN else 0.82)
        x = dx * HS + 0.004
        N.digit([(x, y, zk + 0.014), (x, y - s * 0.004, zk - 0.50 * ln), (x, y - s * 0.022 * HS, zk - 0.93 * ln)], 0.0122 * HS, "Skin", 10)
    R.seg("Fingers_" + T, "Hand_" + T, (0, y, zk))
arm(1, "L")
arm(-1, "R")

# ------------------------------------------------------------------ legs
def leg(s, T):
    y = s * YHIP
    zh, zk = ZHIP, ZKNEE
    lk = 1.0 if GRAN else 1.12
    if GRAN:
        # the skirt: each thigh is half of it, wide, down past the knee
        loft([(zk - 0.07, 0, y, 0.118, 0.110), (zk + 0.06, 0, y, 0.112, 0.106), (zh - 0.04, 0, y, 0.106, 0.108), (zh + 0.01, 0, y, 0.102, 0.104)], None, n=24, lo=0.01, hi=0.05, stripe=STR)
    else:
        loft([(zk + 0.07, 0, y, 0.094 * lk, 0.094 * lk), (zh - 0.06, 0, y, 0.102 * lk, 0.104 * lk), (zh + 0.01, 0, y, 0.102 * lk, 0.104 * lk)], LEGS, n=24, lo=0.01, hi=0.05)
        loft([(zk, 0, y, 0.074 * lk, 0.074 * lk), (zk + 0.08, 0, y, 0.082 * lk, 0.082 * lk)], "Skin", n=18, lo=0.07, hi=0.01)
    R.seg("Thigh_" + T, "Pelvis", (0, y, zh))
    leg_m = "Stocking" if GRAN else "Skin"
    loft([(0.10, 0, y, 0.048 * lk, 0.046 * lk), (0.20, 0, y, 0.060 * lk, 0.058 * lk), (zk - 0.06, 0, y, 0.070 * lk, 0.068 * lk), (zk, 0, y, 0.072 * lk, 0.072 * lk)], leg_m, n=18, lo=0.03, hi=0.07)
    if not GRAN:
        loft([(0.085, 0, y, 0.052 * lk, 0.050 * lk), (0.150, 0, y, 0.056 * lk, 0.054 * lk)], "Sock", n=16, lo=0.01, hi=0.008)
    R.seg("Shin_" + T, "Thigh_" + T, (0, y, zk))
    # the foot: the Dad's own (the pose code knows where its sole and its ankle are)
    pts = []
    for i in range(28):
        t = 2 * math.pi * i / 28
        cc, sn = math.cos(t), math.sin(t)
        pts.append((0.0775 + 0.1725 * math.copysign(abs(cc) ** 0.8, cc), y + (0.061 + 0.009 * cc) * math.copysign(abs(sn) ** 0.8, sn)))
    L.prism(pts, 0.0, 0.024, SOLE, "Z", bev=0.006)
    if GRAN:
        loftx([(-0.072, 0.052, y, 0.030, 0.032), (-0.04, 0.056, y, 0.037, 0.040), (0.0, 0.060, y, 0.042, 0.044), (0.05, 0.052, y, 0.036, 0.046)], "Stocking", n=16, lo=0.02, floor=0.02)
        loftx([(0.018, 0.040, y, 0.054, 0.060), (0.08, 0.040, y, 0.060, 0.066), (0.15, 0.038, y, 0.056, 0.067), (0.21, 0.034, y, 0.044, 0.058), (0.238, 0.030, y, 0.030, 0.040)],
              SHOE, n=20, hi=0.012, noise=0.0022, seed=11, floor=0.014)
        for i in range(9):
            a = math.radians(-12.0 + 204.0 * i / 8)
            N.fluff(0.020, 0.020, y + 0.062 * math.cos(a), 0.040 + 0.058 * math.sin(a), seed=20 + i)
    else:
        loftx([(-0.085, 0.050, y, 0.040, 0.046), (-0.02, 0.054, y, 0.050, 0.058), (0.08, 0.046, y, 0.050, 0.064), (0.17, 0.040, y, 0.044, 0.062), (0.238, 0.032, y, 0.030, 0.044)],
              SHOE, n=20, lo=0.02, hi=0.014, floor=0.014)
        L.rbox(0.07, 0.020, 0.012, 0.09, y, 0.094, SOLE, 0.004, 2)     # the velcro strap
    N.grow(1.08, (0, y, 0))
    L.ball(0.048, 0, y, 0.085, "Stocking" if GRAN else "Sock", nu=12, nv=6)
    R.seg("Foot_" + T, "Shin_" + T, (0, y, 0.085))
leg(1, "L")
leg(-1, "R")

TABLE = "GGranSegs" if GRAN else "GKidSegs"
R.finish(OUT, "Ant Kitchen: %s as rigid segments (Tools/blender/npc_folk.py). One FBX per segment, origin at its joint." % ("grandma" if GRAN else "the kid"),
         inl=INL, table=TABLE, struct="FHumanSegDef", dry=("dry" in argv))

if "preview" in argv:
    POSES = {"bind": {}, "walk": {"Pelvis": (0, 2, -6), "Chest": (0, 3, 12), "Head": (0, 6, -6),
             "Thigh_L": (0, -24, 0), "Shin_L": (0, 14, 0), "Foot_L": (0, -8, 0), "Thigh_R": (0, 16, 0), "Shin_R": (0, 30, 0), "Foot_R": (0, 14, 0),
             "UpperArm_R": (-6, -22, 0), "Forearm_R": (0, -28, 0), "UpperArm_L": (6, 18, 0), "Forearm_L": (0, -14, 0)}}
    N.preview(R, os.path.join(OUT, "preview_*.jpg"), [
        ("turn", [("bind", (0, 1.2, 0), 0), ("walk", (0, 0.4, 0), -90), ("bind", (0, -0.4, 0), -35), ("bind", (0, -1.2, 0), 180)], (6.6, 0, 1.0), (0, 0, 0.85), 60, (1600, 800)),
    ], POSES, floor_size=5.0, wall_x=-2.4)
print("NPC_FOLK_DONE")
