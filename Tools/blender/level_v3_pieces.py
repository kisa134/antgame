# Ant Kitchen level v3: every NEW mesh of level v3 as FBX + level_v3_new_pieces.json (kitchen-manifest format).
# Run:  blender --background --factory-startup --python level_v3_pieces.py -- <out_dir> [dry]
# Sizes: Tools/level/level_v3_layout.PIECES (each piece is built to its box; the script reports any piece that does not fit).
# Fasteners (screws, brackets, hinges, rings, nails, tape, needles, caps) are separate pieces with the origin at their pivot.
# Cloth (curtains, tablecloths, blanket fort) is a single-layer quad grid ready for simulation; curtains carry the vertex group 'pin'.
# Where pieces stand: Tools/level/build_level_v3.py writes SourceArt/Blender/level_v3/level_v3_manifest.json (the whole level).
import bpy, sys, os, time, json, math, random
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else os.path.join(HERE, "..", "..", "SourceArt", "Blender", "level_v3")
DRY = len(argv) > 1 and argv[1] == "dry"
bpy.ops.wm.read_factory_settings(use_empty=True)
import kitchen_lib as L
from level_v3_common import *
R = random.Random(7)
t0 = time.time()

# ------------------------------------------------------------------------------------------------ fasteners
UL([(0, -60), (3, -58), (5, -52), (5, 0), (11, 0), (11, 1.5), (9.5, 4.5), (6, 6.3), (0, 6.8)], "Steel", n=16, axis="Y")
UB(-7, 7, -7.6, -6.4, -1, 1, "Plinth", 0); UB(-1, 1, -7.6, -6.4, -7, 7, "Plinth", 0)
done("SM_V3_Screw")
UB(-16, 16, -8, 0, 0, 240, "Plinth", 2); UB(-16, 16, -380, 0, 232, 240, "Plinth", 2)
UP([(-8, 40), (-8, 232), (-200, 232)], -4, 4, "Plinth", "X")
for z in (60, 180): UCY(5, 2, 0, -8.6, z, "Steel", 10)
for y in (-120, -300): UZ(5, 1, 0, y, 240, "Plinth", 10)
done("SM_V3_ShelfBracket")
UCY(18, 14, 0, -7, 0, "Steel", 14); UB(-12, 12, -80, -10, -6, 6, "Steel", 2); UB(-30, 30, -6, 0, -50, 50, "Steel", 2)
for z in (-30, 0, 30): UCY(4, 1, 0, -6.5 - 0.5, z, "Plinth", 8)
done("SM_V3_Hinge")
UB(-40, 40, -10, 0, -90, 90, "Brass", 3); UB(-8, 8, -280, -10, -8, 8, "Brass", 2)
UT([(0, -260, -8), (0, -260, -30), (0, -235, -38), (0, -285, -38)], 5, "Brass", 8)
for z in (-55, 55): UCY(6, 1.5, 0, -10.8, z, "Steel", 8)
done("SM_V3_RodBracket")
UT([(0, 34 * math.cos(2 * math.pi * k / 16), 34 * math.sin(2 * math.pi * k / 16)) for k in range(17)], 4, "Brass", 6)
UB(-4, 4, -3, 3, -60, -34, "Brass", 1); UB(-10, 10, -7, 7, -75, -60, "Steel", 2)
done("SM_V3_CurtainRing")
UL([(0, 40), (9, 40), (9, 37), (3, 37), (3, -58), (0, -60)], "Steel", n=12, axis="Y")
done("SM_V3_Nail")
UB(-60, 60, -3, 0, -20, 20, "ShelfGlass", 0)
done("SM_V3_TapeStrip")
UCX(7, 600, 0, 0, 0, "Steel", 10); US(12, -305, 0, 0, "Red", nu=10, nv=6)
L.cyl(7 * K, 30 * K, 315 * K, 0, 0, "Steel", "X", 10, 0.0, 0.4 * K)
done("SM_V3_KnitNeedle")
UL([(0, 0), (28, 0), (28, 36), (24, 38), (0, 38)], "Red", n=24)
done("SM_V3_ColaCap")

# ------------------------------------------------------------------------------------------------ kitchen objects
UB(-1100, 1100, -450, 0, 0, 40, "Butcher", 5, 2)
done("SM_V3_WallShelf")
UL([(0, 0), (74, 0), (78, 8), (78, 150), (64, 172), (64, 200), (0, 200)], "ShelfGlass", n=24)
UL([(0, 6), (72, 6), (72, 140), (0, 140)], "Jam", n=20)
UL([(0, 55), (80, 55), (80, 125), (0, 125)], "Paper", n=24)
done("SM_V3_JamJar")
UL([(0, 0), (86, 0), (86, 10), (70, 30), (0, 32)], "ClothCheck", n=24); UL([(0, 4), (80, 4), (80, 12), (0, 12)], "Red", n=24)
done("SM_V3_JamJar_Lid")
UL([(0, 0), (106, 0), (110, 10), (110, 270), (90, 300), (90, 330), (0, 330)], "ShelfGlass", n=24)
UL([(0, 6), (104, 6), (104, 250), (0, 250)], "Brine", n=20)
for k in range(7):
    a = 2 * math.pi * k / 7; UZ(26, 220, 62 * math.cos(a), 62 * math.sin(a), 15, "Leaf", 8)
UL([(0, 300), (92, 300), (92, 330), (0, 330)], "Steel", n=24)
done("SM_V3_PickleJar")
for k in range(16):
    x, y = R.uniform(-280, 280), R.uniform(-220, 220); s = R.uniform(30, 90); a = R.uniform(0, 6.28)
    pts = [(x + s * math.cos(a + d), y + s * 0.7 * math.sin(a + d)) for d in (0, 2.2, 4.0)]
    UP(pts, 0, R.uniform(4, 12), "ShelfGlass")
for k in range(3):
    a0 = R.uniform(0, 6.28); cx, cy = R.uniform(-150, 150), R.uniform(-120, 120)
    UT([(cx + 70 * math.cos(a0 + t * 0.25), cy + 70 * math.sin(a0 + t * 0.25), 18 + 6 * math.sin(t)) for t in range(7)], 9, "ShelfGlass", 6)
done("SM_V3_GlassShards")
UP(blob(0, 0, 360, 240, 28, 0.25, 1.3), 0, 6, "Jam")
for k in range(5): UP(blob(R.uniform(-300, 300), R.uniform(-200, 200), 40, 30, 10, 0.3, k), 0, 5, "Jam")
done("SM_V3_JamSplat")
UL([(0, 0), (70, 0), (90, 12), (100, 40), (100, 430), (92, 480), (55, 540), (32, 565), (30, 610), (0, 610)], "PetClear", n=24)
UL([(0, 8), (94, 8), (94, 470), (0, 470)], "Cola", n=20)
UL([(0, 230), (101.5, 230), (101.5, 380), (0, 380)], "Red", n=24)
done("SM_V3_ColaBottle")
UP(blob(0, 0, 900, 580, 40, 0.22, 2.1), 0, 5, "Cola")
for k in range(4): UP(blob(R.uniform(-800, 800), R.uniform(-500, 500), 90, 60, 12, 0.3, k + 3), 0, 4, "Cola")
done("SM_V3_ColaPuddle")
UCX(18, 4300, 0, 0, 0, "Brass", 16)
for s in (-1, 1): US(30, s * 2150, 0, 0, "Brass", nu=12, nv=8)
done("SM_V3_CurtainRod")

def curtain(u, v):
    x = -280 + 560 * u
    amp = 55 + 25 * (1 - v)
    y = amp * math.sin(2 * math.pi * 6 * u)
    return (x * (0.92 + 0.08 * (1 - v)), y, 5230 * v)
sheet(curtain, 24, 90, "Curtain")
done("SM_V3_CurtainPanel")
pin_group("SM_V3_CurtainPanel", lambda c: c.z > 5229)
def torn(u, v):
    x, y = -950 + 1900 * u, -750 + 1500 * v
    r2 = (x / 900.0) ** 2 + (y / 700.0) ** 2
    z = max(4.0, 290 * math.exp(-1.6 * r2) + 40 * math.sin(x / 140.0) * math.cos(y / 170.0) * math.exp(-r2))
    return (x, y, z)
sheet(torn, 30, 24, "Curtain", keep=lambda i, j: i < 26 + 3 * math.sin(j * 0.9) and not (i > 12 and abs(j - 12) < 1 and i % 3 == 0))
done("SM_V3_CurtainTorn")

def cloth_on_table(a, b, d, top, folds=True, floor=None):
    W, Dd = a + d, b + d
    def f(u, v):
        U, Vv = -W + 2 * W * u, -Dd + 2 * Dd * v
        ex, ey = max(0.0, abs(U) - a), max(0.0, abs(Vv) - b)
        x = math.copysign(min(abs(U), a), U); y = math.copysign(min(abs(Vv), b), Vv)
        drop = math.hypot(ex, ey)
        z = top - drop
        if ex > 0: x = math.copysign(a + 28 + (18 * math.sin(Vv / 160.0) * min(1.0, ex / 300.0) if folds else 0), U)
        if ey > 0: y = math.copysign(b + 28 + (18 * math.sin(U / 160.0) * min(1.0, ey / 300.0) if folds else 0), Vv)
        if floor is not None and z < floor:
            spread = (floor - z) * 0.35; z = floor + 2
            if ex > 0: x += math.copysign(spread, U)
            if ey > 0: y += math.copysign(spread, Vv)
        return (x, y, z)
    return f
sheet(cloth_on_table(2200, 1300, 450, 456), 64, 40, "ClothCheck")
done("SM_V3_Tablecloth")
sheet(cloth_on_table(2200, 1300, 1400, 1366, floor=0.0), 64, 56, "ClothCheck")
done("SM_V3_TableclothLong")
def pulled(u, v):
    a, b, d, P, top = 2200, 1300, 450, 1500, 1406
    U, Vv = -(a + d) + 2 * (a + d) * u, -(b + d) + 2 * (b + d) * v - P
    ex = max(0.0, abs(U) - a)
    x = math.copysign(min(abs(U), a), U) if ex == 0 else math.copysign(a + 28, U)
    if Vv >= -b:
        y = min(Vv, b + 28); z = top - ex - max(0.0, Vv - b)
    else:
        drop = (-b - Vv); z = top - drop - ex; y = -b - 28
        if z < 0:
            y = -b - 28 - (-z) * 0.8; z = 3 + 60 * abs(math.sin(U / 230.0)) * math.exp(-(-z) / 900.0)
    return (x, y, max(2.0, z))
sheet(pulled, 64, 44, "ClothCheck")
done("SM_V3_TableclothPulled")
UL([(0, 0), (110, 0), (150, 40), (170, 150), (160, 250), (110, 300), (70, 310), (0, 310)], "Enamel", n=24)
UL([(0, 120), (171, 120), (171, 160), (0, 160)], "MugBlue", n=24)
UT([(150, 0, 120), (210, 0, 170), (260, 0, 250), (290, 0, 300)], 20, "Enamel", 10)
UT([(-150, 0, 260), (-235, 0, 250), (-260, 0, 170), (-230, 0, 90), (-160, 0, 70)], 16, "Enamel", 10)
done("SM_V3_Teapot")
UL([(0, 0), (95, 0), (90, 25), (40, 40), (25, 45), (30, 60), (0, 60)], "Enamel", n=20)
done("SM_V3_Teapot_Lid")
UL([(0, 0), (140, 0), (140, 10), (90, 18), (0, 18)], "Enamel", n=24)
UL([(0, 18), (50, 18), (72, 60), (78, 130), (0, 130)], "Enamel", n=24); UL([(0, 108), (78.5, 108), (78.5, 122), (0, 122)], "MugBlue", n=24)
UT([(75, 0, 110), (120, 0, 100), (125, 0, 65), (72, 0, 50)], 9, "Enamel", 8)
done("SM_V3_TeaCup")
for sx in (-1, 1):
    UP([(-450, 0), (450, 0), (450, 900), (-10, 900), (-10, 450), (-450, 450)], sx * 330 - 10, sx * 330 + 10, "Enamel", "X")
UB(-330, 330, -450, -10, 420, 450, "Butcher", 4); UB(-330, 330, -10, 450, 870, 900, "Butcher", 4)
UB(-330, 330, 420, 450, 20, 860, "Enamel", 3)
done("SM_V3_StepStool")
UB(-190, 190, -70, 70, 0, 540, "Cardboard", 6); UB(-170, 170, -72, -69, 60, 480, "Honey", 0)
UR(380, 8, 120, 0, -40, 590, "Cardboard", 2, 1, rot=(40, 0, 0))
for k in range(6): UZ(18, 4, R.uniform(-150, 150), R.uniform(-40, 40), 540 + R.uniform(0, 30), "Toast", 6)
done("SM_V3_CerealBox")
for k in range(130):
    r = 750 * math.sqrt(R.random()); a = R.uniform(-1.3, 1.3)
    x, y = r * math.cos(a) - 300, r * math.sin(a) * 0.62
    if abs(y) > 470 or abs(x) > 740: continue
    L.cyl(R.uniform(14, 22) * K, 5 * K, x * K, y * K, R.uniform(3, 25) * K, "Toast", "Z", 6, 0.0, None, 1, (R.uniform(-30, 30), R.uniform(-30, 30), 0))
done("SM_V3_CerealSpill")

# ------------------------------------------------------------------------------------------------ walls: pictures, drawings, outlets, cornice, lamp
for (x0, x1, z0, z1) in ((-450, -400, -700, -80), (400, 450, -700, -80), (-450, 450, -130, -80), (-450, 450, -700, -650)):
    UB(x0, x1, -45, 0, z0, z1, "Walnut", 4)
UB(-400, 400, -32, -28, -650, -130, "ShelfGlass", 0); UB(-400, 400, -26, -2, -650, -130, "Photo", 0)
for (x0, x1, z0, z1, m) in ((-400, 400, -390, -130, "Teal"), (-400, 400, -650, -390, "Honey"), (-200, -120, -560, -330, "CakePink"), (-60, 40, -600, -300, "CakePink"), (120, 180, -540, -380, "CakePink")):
    UB(x0, x1, -27.5, -26, z0, z1, m, 0)
UT([(-300, -2, -140), (0, -2, -10), (300, -2, -140)], 2, "Steel", 6)
done("SM_V3_PictureFrame", note="Рамка с фото: орех, стекло, фото (небо/песок/люди); шнурок до гвоздя в начале координат")
UB(-200, 200, -4, 0, -285, 285, "Paper", 0)
UT([(-150 + 25 * k, -4.5, -200 + 60 * math.sin(k * 0.8)) for k in range(13)], 5, "Leaf", 5)
UT([(60 + 60 * math.cos(k * 0.5), -4.5, 120 + 60 * math.sin(k * 0.5)) for k in range(14)], 5, "Honey", 5)
UT([(-120, -4.5, 60), (-60, -4.5, 160), (0, -4.5, 60), (-120, -4.5, 60)], 5, "Red", 5)
done("SM_V3_KidDrawing")
UB(-85, 85, -12, 0, -85, 85, "Enamel", 4); UCY(45, 6, 0, -14, 0, "Plaster", 20)
for x in (-18, 18): UCY(6, 4, x, -18, 0, "Plinth", 8)
UCY(7, 3, 0, -13.5, -70, "Steel", 8)
done("SM_V3_Outlet")
UP([(0, 0), (0, -130), (-15, -130), (-28, -96), (-62, -62), (-96, -28), (-130, -15), (-130, 0)], -1000, 1000, "Plaster", "X")
done("SM_V3_Cornice")
UL([(110, 300), (470, 0), (455, 0), (98, 290)], "Enamel", n=28, closed=True)
US(60, 0, 0, 170, "Bulb", nu=12, nv=8); UZ(28, 40, 0, 0, 260, "Brass", 12)
UZ(5, 1560, 0, 0, 300, "Plinth", 6); UZ(60, 40, 0, 0, 1860, "Enamel", 16)
done("SM_V3_PendantLamp")

# ------------------------------------------------------------------------------------------------ living room
UL([(0, 0), (15, 0), (22, 25), (60, 60), (170, 150), (232, 300), (232, 420), (190, 540), (110, 615), (0, 640)], "Latex", n=24)
done("SM_V3_Balloon", note="Шарик: один слот Latex, цвет — по экземпляру (tint в манифесте уровня)")
UT([(28 * math.sin(k * 0.5), 18 * math.cos(k * 0.7), 4600.0 * k / 24) for k in range(25)], 3, "Paper", 5)
done("SM_V3_BalloonString")
US(45, -30, 0, 50, "Red", (1, 0.6, 1)); US(45, 30, 0, 50, "Red", (1, 0.6, 1)); L.cyl(55 * K, 60 * K, 0, 0, 30 * K, "Red", "Z", 12, 0.0, 5 * K)
done("SM_V3_BalloonWeight")
UB(-450, 450, -350, 350, 0, 520, "GiftWrap", 4)
UB(-60, 60, -352, 352, 0, 521, "Red", 1); UB(-452, 452, -60, 60, 0, 521, "Red", 1)
done("SM_V3_GiftBox")
UB(-470, 470, -370, 370, 0, 90, "GiftWrap", 4)
UB(-62, 62, -372, 372, 0, 91, "Red", 1); UB(-472, 472, -62, 62, 0, 91, "Red", 1)
US(70, -60, 0, 110, "Red", (1.0, 0.45, 0.55), 12, 8, rot=(0, 0, 25)); US(70, 60, 0, 110, "Red", (1.0, 0.45, 0.55), 12, 8, rot=(0, 0, -25))
done("SM_V3_GiftBox_Lid")
L.tray(-700 * K, 700 * K, -450 * K, 450 * K, 0, 780 * K, 40 * K, "Butcher", 4 * K, 1)
UB(-702, 702, -452, -440, 640, 700, "Red", 1); UB(-702, 702, 440, 452, 640, 700, "Red", 1)
for (x, m) in ((-450, "Blue"), (-150, "Honey"), (150, "Leaf"), (450, "Red")):
    UB(x - 90, x + 90, -453, -451, 250, 450, m, 0)
done("SM_V3_ToyBox")
UB(-720, 720, -920, 0, 0, 60, "Butcher", 5, 2); UB(-80, 80, -935, -920, 10, 50, "Brass", 2)
done("SM_V3_ToyBox_Lid", motion={"type": "hinge", "axis": (1, 0, 0), "angle_deg": 100})
UB(-380, 380, -230, 230, 0, 36, "Yarn", 10, 2)
for k in range(9): UCX(9, 740, 0, -200 + 50 * k, 44, "Yarn", 8)
done("SM_V3_KnitScarf")
for sx in (-1, 1):
    UT([(sx * 75 + 52 * math.cos(2 * math.pi * k / 16), 0, 30 + 34 * math.sin(2 * math.pi * k / 16)) for k in range(17)], 5, "Plinth", 6)
    UCY(48, 2, sx * 75, 0, 30, "ShelfGlass", 16)
    UB(sx * 128 - 4, sx * 128 + 4, 0, 80, 30, 40, "Plinth", 1)
UT([(-25, 0, 52), (0, 0, 60), (25, 0, 52)], 5, "Plinth", 6)
done("SM_V3_Glasses")
UB(-80, 80, -155, 155, 0, 18, "Plinth", 10, 2); UB(-72, 72, -145, 145, 18, 18.6, "ShelfGlass", 0)
done("SM_V3_Phone")
UB(-650, 650, -400, 400, 0, 18, "Sisal", 6); UB(-650, 650, -400, -340, 0, 19, "Walnut", 0); UB(-650, 650, 340, 400, 0, 19, "Walnut", 0)
done("SM_V3_DoorMat")
def fort(u, v):
    x, y = -1800 + 3600 * u, -1300 + 2600 * v
    top = 1000 - 150 * (1 - (y / 1300.0) ** 2)
    z = top if abs(x) <= 1300 else max(2.0, top - (abs(x) - 1300) * 2.1)
    xx = x if abs(x) <= 1300 else math.copysign(1300 + (abs(x) - 1300) * 0.45, x)
    return (xx + 20 * math.sin(y / 200.0) * (abs(x) > 1300), y, z + 15 * math.sin(x / 180.0))
sheet(fort, 48, 36, "SockWool")
done("SM_V3_BlanketFort")
UR(1100, 1100, 300, 0, 0, 150, "Honey", 120, 3)
done("SM_V3_FloorCushion")

# ------------------------------------------------------------------------------------------------ floor props with a story
UR(600, 400, 760, 0, 0, 380, "Cardboard", 30, 2); UB(-240, 240, -202, -199, 200, 560, "Red", 0)
UCX(70, 620, 0, 0, 820, "Cardboard", 12)
done("SM_V3_DogFoodBag")
UR(600, 360, 760, 0, 0, 380, "Leather", 90, 3); UR(440, 120, 380, 0, -200, 260, "Leather", 50, 2)
UT([(-150, 170, 700), (-170, 260, 400), (-150, 200, 60)], 22, "Plinth", 8); UT([(150, 170, 700), (170, 260, 400), (150, 200, 60)], 22, "Plinth", 8)
UT([(-90, 0, 760), (0, 0, 810), (90, 0, 760)], 14, "Plinth", 8)
done("SM_V3_Backpack")
for (x0, x1) in ((-450, -410), (410, 450)):
    for (y0, y1) in ((-300, -260), (260, 300)): UB(x0, x1, y0, y1, 0, 450, "Butcher", 4)
for z in (30, 180, 330):
    UB(-450, 450, -300, -280, z, z + 90, "Butcher", 3); UB(-450, 450, 280, 300, z, z + 90, "Butcher", 3)
    UB(-450, -430, -300, 300, z, z + 90, "Butcher", 3); UB(430, 450, -300, 300, z, z + 90, "Butcher", 3)
UB(-430, 430, -280, 280, 20, 40, "Butcher", 2)
for k in range(16):
    US(R.uniform(60, 85), R.uniform(-340, 340), R.uniform(-190, 190), R.uniform(330, 400), "Potato" if k % 4 else "Honey", (1.2, 0.95, 0.85), 10, 6)
done("SM_V3_VeggieCrate")
UR(560, 400, 1150, 0, 0, 910, "ClothCheck", 80, 2); UR(560, 400, 60, 0, 0, 1490, "Teal", 20, 1)
UT([(-200, 230, 60), (-210, 250, 1780)], 14, "Steel", 8); UT([(200, 230, 60), (210, 250, 1780)], 14, "Steel", 8)
UT([(-210, 250, 1780), (0, 250, 1800), (210, 250, 1780)], 18, "Plinth", 8)
for sx in (-1, 1): UCX(140, 50, sx * 230, 200, 140, "Plinth", 16)
done("SM_V3_ShoppingTrolley")
z = 0.0
for k in range(8):
    UR(820 - R.uniform(0, 30), 580 - R.uniform(0, 30), 62, R.uniform(-10, 10), R.uniform(-10, 10), z + 31, "Paper", 4, 1, rot=(0, 0, R.uniform(-3, 3))); z += 63
UT([(0, -300, 0), (0, -302, 505), (0, 302, 505), (0, 300, 0)], 6, "Sisal", 6); UT([(-420, 0, 0), (-422, 0, 505), (422, 0, 505), (420, 0, 0)], 6, "Sisal", 6)
done("SM_V3_NewspaperStack")
US(300, 0, 0, 292, "Plinth", (1.2, 1.0, 0.95), 14, 10); UL([(0, 540), (40, 560), (60, 600), (30, 620), (0, 620)], "Plinth", n=10)
done("SM_V3_TrashBag")
UZ(30, 2900, 0, 0, 0, "Steel", 12); UB(-200, 200, -20, 20, 2840, 2880, "Steel", 3); UZ(60, 20, 0, 0, 0, "Steel", 12)
done("SM_V3_LaundryPole")
UT([(6000.0 * k / 24, 0, -80 * 4 * (k / 24.0) * (1 - k / 24.0)) for k in range(25)], 5, "Paper", 6)
done("SM_V3_LaundryLine")
US(380, 0, 0, 0, "Soil", (1.0, 0.85, 0.3), 16, 8)
for k in range(5):
    a = R.uniform(0, 6.28); UT([(260 * math.cos(a + t * 0.15), 260 * math.sin(a + t * 0.15), 20 + 10 * t) for t in range(5)], 16, "Terracotta", 6)
done("SM_V3_SoilHeap")
for k in range(5):
    UL([(0, 0), (240, 0), (250, 18), (0, 22)], "Enamel", n=20, x=R.uniform(-60, 60), y=R.uniform(-60, 60), z=20 + 24 * k, rot=(R.uniform(-8, 8), R.uniform(-8, 8), 0))
UL([(0, 0), (200, 0), (210, 220), (0, 220)], "Steel", n=20, x=180, y=150, z=60, rot=(0, 25, 0))
UL([(0, 0), (60, 0), (75, 120), (0, 120)], "Enamel", n=14, x=-250, y=200, z=10)
done("SM_V3_DirtyDishStack")

print("BUILT %d pieces in %.1fs" % (len(L.ORDER), time.time() - t0))
# fit report against the layout boxes
fit = []
for n in L.ORDER:
    p = L.PIECES[n]; b = V3.PIECES.get(n, {}).get("box")
    if not b: fit.append((n, "not in layout")); continue
    mn, mx = p["mn"] / K, p["mx"] / K
    d = max(abs(mn.x - b[0]), abs(mx.x - b[1]), abs(mn.y - b[2]), abs(mx.y - b[3]), abs(mn.z - b[4]), abs(mx.z - b[5]))
    if d > 40: fit.append((n, "box off by %.0f uu: got %s..%s" % (d, [round(v) for v in mn], [round(v) for v in mx])))
for n, w in fit: print("FIT", n, w)
os.makedirs(OUT, exist_ok=True)
if not DRY:
    L.export_all(OUT)
    print("EXPORTED in %.1fs" % (time.time() - t0))
HEADER = {
    "what": "Ant Kitchen level v3: the NEW pieces (Tools/blender/level_v3_pieces.py). One FBX per piece, default FBX export scale. Same conventions as kitchen_manifest.json.",
    "conventions": {"blender": "metres, +Z up, each piece's front faces -Y, mesh origin = the placement point of level_v3_layout.PIECES (fasteners: their pivot)",
                    "game_scale": "1 m = 1900 uu; a default UE FBX import gives 1 m = 100 uu, so use component scale 19",
                    "names": "SM_V3_* load from /Game/LevelV3 (new folder: see LEVEL_V3_INTEGRATION.md)",
                    "materials": "slots with new_slot true need a look in KitchenArt.cpp; cloth pieces are single-sided: two-sided material"},
    "notes": ["Not imported into Unreal. Collision: complex-as-simple like the kitchen set; fasteners, strings, rings, tape, pictures are ants_only (see level_v3.json).",
              "Cloth pieces (CurtainPanel, Tablecloth*, BlanketFort, CurtainTorn) are open single-layer grids: open_edges is expected for them."],
}
doc = L.write_manifest(OUT, HEADER)
src, dst = os.path.join(OUT, "kitchen_manifest.json"), os.path.join(OUT, "level_v3_new_pieces.json")
if os.path.exists(dst): os.remove(dst)
os.rename(src, dst)
L.report(doc)
missing = [n for n in V3.PIECES if n not in L.PIECES]
print("MISSING_PIECES %s" % missing)
print("LEVEL_V3_PIECES_DONE %.1fs" % (time.time() - t0))
