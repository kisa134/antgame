# Ant Kitchen pieces, part 2: appliances - fridge, range, hood, dishwasher, microwave, toaster.
import math
from kitchen_lib import *

def sx(X):
    return -X * K

def hx(angle, note):
    return {"type": "hinge", "axis": (1, 0, 0), "angle_deg": angle, "note": note}

def hz(angle, note):
    return {"type": "hinge", "axis": (0, 0, 1), "angle_deg": angle, "note": note}

def fridge():
    # ---- body: rounded red shell, real interior (liner), feet with a 120 uu gap underneath
    push()
    gbox(2700, 4700, 3100, 4750, 120, 3700, "FridgeRed", 0.032, 3)
    body = pop()
    push()
    gbox(2780, 4620, 3050, 4670, 200, 2500, "FridgeLiner", 0.0)
    gbox(2780, 4620, 3050, 4670, 2560, 3620, "FridgeLiner", 0.0)
    gbox(3500, 3800, 4450, 4670, 2490, 2570, "FridgeLiner", 0.0)
    add(cut(body, pop()))
    for (X, Y) in ((2780, 3180), (4620, 3180), (2780, 4670), (4620, 4670)):
        gcyl(X, Y, 0, 50, 122, "Plinth", 12, 0.004)
    gbox(2780, 4620, 3300, 4670, 900, 930, "ShelfGlass", 0.002, 1)
    gbox(2780, 4620, 3300, 4670, 1700, 1730, "ShelfGlass", 0.002, 1)
    gbox(2780, 4620, 3520, 4670, 2100, 2130, "ShelfGlass", 0.002, 1)
    for X in (2780, 4600):                      # runners for the crisper drawer
        gbox(X, X + 20, 3300, 4660, 820, 850, "FridgeLiner", 0.0)
    gbox(2900, 4500, 4752, 4770, 400, 1500, "Iron", 0.004, 1)      # condenser grille on the back
    piece("SM_K_Fridge", replaces=[("BuildFridge", "feet, floor, top, sides, back, divider with the gap at the back, two shelves", (2700, 3100, 0), (4700, 4750, 3700))],
          note="cavities as in the game: fridge x 2780..4620 y 3100..4670 z 200..2500, freezer z 2560..3620, divider gap x 3500..3800 y 4450..4670; shelves at z 930 and 1730 (game) plus a third at 2130")
    # ---- main door (hinge = game-east front corner, as AKitchenDoor::Fridge), racks on the inside
    gbox(2700, 4700, 3000, 3100, 126, 2494, "FridgeRed", 0.026, 3)
    gbox(2800, 4600, 3098, 3110, 190, 2440, "FridgeLiner", 0.003, 1)
    for Z in (620, 1420, 2020):
        gbox(2850, 4550, 3100, 3250, Z, Z + 30, "FridgeLiner", 0.002, 1)
        gbox(2850, 4550, 3230, 3250, Z + 30, Z + 150, "ShelfGlass", 0.002, 1)
        gbox(2850, 2870, 3100, 3230, Z + 30, Z + 150, "FridgeLiner", 0.002, 1)
        gbox(4530, 4550, 3100, 3230, Z + 30, Z + 150, "FridgeLiner", 0.002, 1)
    bar_handle(sx(2830), 3000 * K, 1870 * K, 900 * K, "Chrome", "Z", 0.012, 0.045, 12)
    for (X, Z, col) in ((3700, 2200, "Fabric"), (3450, 2050, "Blue"), (3600, 1930, "Leaf")):
        cyl(0.02, 0.009, sx(X), 2996 * K, Z * K, col, "Y", 12)
    gbox(3350, 3560, 2990, 3002, 1560, 1800, "Paper", 0.0)
    piece("SM_K_Fridge_DoorMain", origin=G(4700, 3100, 120), parent="SM_K_Fridge",
          motion=hz(-100, "hinge on the game-east front corner exactly where AKitchenDoor(Fridge) sits (4700,3100,120); the game opens it with FQuat(Up, +100 deg)"),
          replaces=[("AKitchenDoor::Build Fridge", "door slab + handle + bins", (2700, 3000, 120), (4700, 3100, 2500))])
    # ---- freezer door (a fixed front in the game: now a real door)
    gbox(2700, 4700, 3000, 3100, 2506, 3700, "FridgeRed", 0.026, 3)
    gbox(2800, 4600, 3098, 3110, 2580, 3610, "FridgeLiner", 0.003, 1)
    gbox(2850, 4550, 3100, 3230, 2700, 2730, "FridgeLiner", 0.002, 1)
    gbox(2850, 4550, 3210, 3230, 2730, 2840, "ShelfGlass", 0.002, 1)
    bar_handle(sx(2830), 3000 * K, 2880 * K, 460 * K, "Chrome", "Z", 0.012, 0.045, 12)
    gbox(4150, 4420, 2990, 3004, 3480, 3550, "Chrome", 0.003, 1)
    piece("SM_K_Fridge_DoorFreezer", origin=G(4700, 3100, 2500), parent="SM_K_Fridge",
          motion=hz(-100, "same hinge line as the main door"),
          replaces=[("BuildFridge", "freezer front (fixed) + handle", (2700, 3000, 2500), (4700, 3100, 3700))])
    # ---- crisper drawer
    tray(sx(4594), sx(2806), 3290 * K, 4640 * K, 215 * K, 800 * K, 20 * K, "FridgeLiner", 0.003, 1)
    gbox(2790, 4610, 3270, 3292, 210, 815, "ShelfGlass", 0.004, 1)
    gbox(3300, 4100, 3252, 3272, 730, 780, "FridgeLiner", 0.004, 1)
    piece("SM_K_Fridge_Crisper", origin="back", parent="SM_K_Fridge", motion={"type": "slide", "vector_m": (0, -700 * K, 0), "note": "pulls out to the front (door must be open)"})

def fridge_food():
    # Milk carton (gable top).
    ubox(-200, 200, -200, 200, 0, 560, "Enamel", 0.006, 2)
    prism([(-200 * K, 552 * K), (200 * K, 552 * K), (22 * K, 690 * K), (22 * K, 725 * K), (-22 * K, 725 * K), (-22 * K, 690 * K)], -200 * K, 200 * K, "Enamel", "Y")
    ubox(-204, 204, -204, 204, 250, 400, "Blue", 0.004, 1)
    piece("SM_K_MilkCarton", at=[(*G(3100, 4300, 930), 0)], on="SM_K_Fridge", replaces=[("BuildFridge", "milk carton box 400x400x700 (was on the fridge floor, now on shelf 1 because of the crisper)", (2900, 4100, 200), (3300, 4500, 900))])
    # Cheese wedge.
    prism([(-250 * K, 0), (250 * K, -200 * K), (250 * K, 200 * K)], 0, 220 * K, "Cheese", "Z", bev=0.006)
    for (x, y, r) in ((60, 0, 34), (170, 90, 26), (150, -80, 22)):
        cyl(r * K, 8 * K, x * K, y * K, 221 * K, "Butcher", "Z", 10)
    piece("SM_K_CheeseWedge", at=[(*G(4250, 3800, 1730), 0)], on="SM_K_Fridge", replaces=[("BuildFridge", "cheese block", (4000, 3600, 1730), (4500, 4000, 1950))])
    # Egg tray with six eggs.
    ubox(-210, 210, -145, 145, 0, 60, "Cardboard", 0.008, 2)
    for i in range(3):
        for j in range(2):
            x, y = (-135 + 135 * i) * K, (-68 + 136 * j) * K
            cyl(52 * K, 20 * K, x, y, 66 * K, "Cardboard", "Z", 10, r2=60 * K)
            ball(50 * K, x, y, 118 * K, "Enamel", (1, 1, 1.28), 10, 6)
    piece("SM_K_EggTray", at=[(*G(3300, 4050, 2130), 0)], on="SM_K_Fridge", note="new prop (the task asks for eggs); sits on the third shelf")
    # Butter tub.
    lathe([(0, 0), (185 * K, 0), (205 * K, 20 * K), (218 * K, 300 * K), (0, 300 * K)], "Cheese", n=14)
    lathe([(0, 296 * K), (228 * K, 296 * K), (228 * K, 340 * K), (210 * K, 360 * K), (0, 360 * K)], "Enamel", n=14)
    piece("SM_K_ButterTub", at=[(*G(4250, 4250, 930), 0)], on="SM_K_Fridge", replaces=[("BuildFridge", "butter tub Cyl r220 h380 (was on the fridge floor)", (3980, 3980, 200), (4420, 4420, 580))])
    # Freezer: ice cream tub and an ice tray.
    lathe([(0, 0), (280 * K, 0), (300 * K, 20 * K), (330 * K, 440 * K), (0, 440 * K)], "CakePink", n=16)
    lathe([(0, 436 * K), (340 * K, 436 * K), (340 * K, 480 * K), (320 * K, 500 * K), (0, 500 * K)], "Enamel", n=16)
    piece("SM_K_IceCreamTub", at=[(*G(3600, 4000, 2560), 0)], on="SM_K_Fridge", replaces=[("BuildFridge", "ice cream tub", (3270, 3670, 2560), (3930, 4330, 3060))])
    tray(-150 * K, 150 * K, -150 * K, 150 * K, 0, 110 * K, 16 * K, "Blue", 0.003, 1)
    ubox(-8, 8, -134, 134, 16, 100, "Blue", 0.0)
    for j in (-45, 45):
        ubox(-134, 134, j - 8, j + 8, 16, 100, "Blue", 0.0)
    for i in (-71, 71):
        for j in (-94, 0, 94):
            ubox(i - 50, i + 50, j - 32, j + 32, 30, 130, "Ice", 0.006, 1)
    piece("SM_K_IceTray", at=[(*G(3050, 3450, 2560), 0)], on="SM_K_Fridge", replaces=[("BuildFridge", "ice tray", (2900, 3300, 2560), (3200, 3600, 2700))])

def range_():
    push()
    gbox(-1500, 0, 3000, 4750, 28, 1710, "Enamel", 0.014, 2)
    body = pop()
    push()
    gbox(-1450, -50, 2950, 4700, 300, 1500, "OvenLiner", 0.0)
    gbox(-800, -600, 4690, 4760, 300, 430, "OvenLiner", 0.0)
    add(cut(body, pop()))
    for (X, Y) in ((-1400, 3120), (-100, 3120), (-1400, 4640), (-100, 4640)):
        gcyl(X, Y, 0, 45, 30, "Plinth", 10)
    gbox(-1470, -30, 2962, 3002, 70, 270, "Enamel", 0.010, 2)                 # warming-drawer front (fixed)
    bar_handle(sx(-750), 2962 * K, 215 * K, 500 * K, "Chrome", "X", 0.008, 0.03, 10)
    gbox(-1490, -10, 2962, 3002, 1535, 1695, "Steel", 0.006, 2)               # control panel
    gbox(-330, -90, 2954, 2964, 1575, 1650, "BlackGlass", 0.002, 1)
    gbox(-1500, 0, 2990, 5000, 1710, 1740, "BlackGlass", 0.004, 2)            # glass top, runs back to the wall
    gbox(-1500, 0, 4950, 5000, 1738, 1830, "Enamel", 0.008, 2)
    for z in (318, 1474):                                                     # heating elements
        for i in range(6):
            X = -1300 + i * 220
            tube([G(X, 3250, z), G(X, 4550, z)], 12 * K, "HeatGlow", 6)
        for i in range(5):
            Y = 3250 if i % 2 else 4550
            tube([G(-1300 + i * 220, Y, z), G(-1080 + i * 220, Y, z)], 12 * K, "HeatGlow", 6)
    for X in (-1450, -70):                                                    # rack ledges
        gbox(X, X + 20, 3100, 4650, 850, 880, "OvenLiner", 0.0)
    piece("SM_K_Range", replaces=[("BuildNorthRun", "oven shell boxes, oven back wall with the gnawed hole, stove top, control strip (+ plinth under it)", (-1500, 2950, 0), (0, 5000, 1740))],
          note="oven cavity as in the game: x -1450..-50, y 3000..4700, z 300..1500, hole in the back x -800..-600 z 300..430; stands on feet with a 28 uu gap underneath instead of the plinth")
    # ---- oven door: hinged at the bottom, drops into a ramp level with the oven floor
    gbox(-1480, -20, 2960, 3000, 300, 1515, "Enamel", 0.010, 2)
    gbox(-1200, -300, 2953, 2964, 620, 1260, "BlackGlass", 0.004, 1)
    gbox(-1440, -60, 2998, 3008, 316, 1490, "OvenLiner", 0.002, 1)
    bar_handle(sx(-750), 2960 * K, 1400 * K, 1100 * K, "Chrome", "X", 16 * K, 60 * K, 12)
    piece("SM_K_Range_OvenDoor", origin=G(-750, 3000, 300), parent="SM_K_Range",
          motion=hx(88, "bottom hinge along X at the game's AKitchenDoor(Oven) spot (-750,3000,300); the top falls forward (the game uses FQuat(Forward, +88 deg))"),
          replaces=[("AKitchenDoor::Build Oven", "door slab, window, handle", (-1450, 2930, 300), (-50, 3000, 1500))])
    # ---- knob: one mesh, five instances on the panel (the middle one is the game's AKitchenKnob at x=-920)
    cyl(44 * K, 12 * K, 0, -6 * K, 0, "Chrome", "Y", 14)
    cyl(34 * K, 40 * K, 0, -30 * K, 0, "Plinth", "Y", 14, 0.004, r2=38 * K, seg=1)
    box(-5 * K, 5 * K, -56 * K, -44 * K, 4 * K, 34 * K, "Enamel", 0.0)
    piece("SM_K_Range_Knob", origin=(0, 0, 0), parent="SM_K_Range", at=[(*G(X, 2962, 1612), 0) for X in (-1380, -1150, -920, -690, -460)],
          motion={"type": "hinge", "axis": (0, 1, 0), "angle_deg": 270, "note": "turns about its own axis (Y); origin on the panel face"},
          replaces=[("BuildNorthRun / AKitchenKnob", "knob cylinders d70 (game: four at x=-1250+330*i, the gadget one at -920)", (-955, 2930, 1565), (-885, 2960, 1635))])
    # ---- burner: one mesh, four instances
    lathe([(0, 0), (250 * K, 0), (260 * K, 8 * K), (260 * K, 19 * K), (250 * K, 25 * K), (0, 25 * K)], "Iron", n=20)
    for R in (95, 155, 215):
        lathe([((R + 10 * math.cos(a * math.pi / 3)) * K, (27 + 10 * math.sin(a * math.pi / 3)) * K) for a in range(6)], "BurnerRing", n=20, closed=True)
    piece("SM_K_Range_Burner", at=[(*G(X, Y, 1740), 0) for (X, Y) in ((-1150, 3500), (-350, 3500), (-1150, 4400), (-350, 4400))], on="SM_K_Range",
          replaces=[("BuildNorthRun", "burner Cyl r260 h25 (the one at -350,3500 is the stove gadget's coil)", (-1410, 3240, 1740), (-890, 3760, 1765))])
    # ---- wire rack and baking tray
    for X in (-1440, -76):
        gbox(X, X + 16, 3100, 4650, 880, 900, "Steel", 0.0)
    for Y in (3100, 4634):
        gbox(-1424, -76, Y, Y + 16, 880, 900, "Steel", 0.0)
    for i in range(11):
        X = -1340 + i * 118
        gbox(X - 8, X + 8, 3116, 4634, 882, 898, "Steel", 0.0)
    piece("SM_K_Range_Rack", on="SM_K_Range", replaces=[("BuildNorthRun", "rack (solid plate in the game, a wire rack here)", (-1450, 3100, 880), (-50, 4650, 900))])
    tray(sx(-400), sx(-1100), 3500 * K, 4300 * K, 900 * K, 945 * K, 16 * K, "Iron", 0.003, 1)
    piece("SM_K_Range_Tray", on="SM_K_Range", replaces=[("BuildNorthRun", "baking tray", (-1100, 3500, 900), (-400, 4300, 915))])

def hood():
    push()
    gbox(-1600, 100, 4200, 5000, 3300, 3600, "Steel", 0.012, 2)
    b = pop()
    push()
    gbox(-1540, 40, 4260, 4950, 3280, 3370, "Iron", 0.0)
    add(cut(b, pop()))
    gbox(-1000, -500, 4600, 5000, 3596, 5400, "Steel", 0.010, 2)
    for X in (-1480, -720):
        gbox(X, X + 700, 4320, 4900, 3354, 3372, "Steel", 0.003, 1)
    gbox(-1150, -350, 4188, 4204, 3410, 3480, "BlackGlass", 0.003, 1)
    piece("SM_K_Hood", replaces=[("Build", "range hood canopy + chimney", (-1600, 4200, 3300), (100, 5000, 5400))], note="hollow underside (70 uu recess) with two filter panels")

def dishwasher():
    push()
    gbox(0, 1300, 3000, 4750, 120, 1650, "Steel", 0.006, 2)
    b = pop()
    push()
    gbox(60, 1240, 2950, 4700, 160, 1600, "FridgeLiner", 0.0)
    add(cut(b, pop()))
    gbox(0, 1300, 3100, 4750, 0, 120, "Plinth", 0.003, 1)
    for Z in (380, 1030):
        for X in (60, 1220):
            gbox(X, X + 20, 3100, 4660, Z, Z + 20, "Steel", 0.0)
    piece("SM_K_Dishwasher", replaces=[("BuildAppliances", "dishwasher hollow box", (0, 3000, 120), (1300, 4750, 1650)), ("BuildNorthRun", "plinth under it", (0, 3100, 0), (1300, 4750, 120))],
          note="cavity as in the game: x 60..1240, y 3000..4700, z 160..1600; ledges for the two racks")
    gbox(10, 1290, 2962, 3000, 122, 1598, "Steel", 0.008, 2)
    gbox(60, 1240, 2998, 3006, 180, 1580, "FridgeLiner", 0.002, 1)
    for i in range(6):
        gbox(90, 1210, 3004, 3020, 270 + i * 220, 290 + i * 220, "FridgeLiner", 0.0)
    bar_handle(sx(650), 2962 * K, 1500 * K, 1000 * K, "Chrome", "X", 14 * K, 50 * K, 12)
    gbox(250, 500, 2954, 2964, 1360, 1410, "BlackGlass", 0.002, 1)
    gbox(880, 1050, 2954, 2964, 1370, 1400, "Leaf", 0.002, 1)
    piece("SM_K_Dishwasher_Door", origin=G(650, 3000, 120), parent="SM_K_Dishwasher",
          motion=hx(85, "bottom hinge at the game's AKitchenDoor(Dishwasher) spot (650,3000,120); drops into a ramp, ribs on the inside"),
          replaces=[("AKitchenDoor::Build Dishwasher", "door slab + handle + ribs", (10, 2940, 120), (1290, 3012, 1600))])
    for x in range(-530, 531, 212):
        ubox(x - 8, x + 8, -790, 790, 0, 16, "Steel", 0.0)
    for y in range(-770, 771, 385):
        ubox(-570, 570, y - 8, y + 8, 0, 16, "Steel", 0.0)
    for (x, y) in ((-570, -790), (570, -790), (-570, 790), (570, 790)):
        ubox(x - 9, x + 9, y - 9, y + 9, 0, 130, "Steel", 0.0)
    for y in (-790, 790):
        ubox(-579, 579, y - 8, y + 8, 114, 130, "Steel", 0.0)
    for x in (-570, 570):
        ubox(x - 8, x + 8, -790, 790, 114, 130, "Steel", 0.0)
    piece("SM_K_Dishwasher_Rack", origin="back", parent="SM_K_Dishwasher", at=[(*G(650, 3870, 400), 0), (*G(650, 3870, 1050), 0)],
          motion={"type": "slide", "vector_m": (0, -1200 * K, 0), "note": "racks roll out to the front over the open door"},
          replaces=[("BuildAppliances", "two wire racks (bars + posts)", (80, 3080, 400), (1220, 4660, 414))])

def microwave():
    push()
    gbox(1300, 2400, 4000, 4900, 1722, 2350, "Enamel", 0.012, 2)
    b = pop()
    push()
    gbox(1360, 2160, 3950, 4860, 1750, 2310, "FridgeLiner", 0.0)
    add(cut(b, pop()))
    for (X, Y) in ((1380, 4080), (2320, 4080), (1380, 4820), (2320, 4820)):
        gcyl(X, Y, 1710, 40, 16, "Plinth", 10)
    gbox(2190, 2370, 3990, 4002, 1800, 2300, "BlackGlass", 0.003, 1)
    for i in range(4):
        gbox(2215, 2345, 3980, 3992, 1850 + i * 100, 1900 + i * 100, "Steel", 0.003, 1)
    piece("SM_K_Microwave", replaces=[("BuildAppliances", "microwave hollow box + panel", (1300, 4000, 1710), (2400, 4900, 2350))],
          note="cavity as in the game: x 1360..2160, y 4000..4860, z 1750..2310; control panel on the game-east side")
    gbox(1304, 2172, 3966, 4000, 1738, 2322, "Enamel", 0.006, 2)
    gbox(1400, 2060, 3958, 3968, 1830, 2230, "BlackGlass", 0.004, 1)
    gbox(1380, 2140, 3998, 4006, 1770, 2290, "BlackGlass", 0.002, 1)
    bar_handle(sx(2110), 3966 * K, 2030 * K, 340 * K, "Chrome", "Z", 12 * K, 40 * K, 10)
    piece("SM_K_Microwave_Door", origin=G(1300, 4000, 1740), parent="SM_K_Microwave",
          motion=hz(115, "hinge on the game-west front edge at AKitchenDoor(Microwave) (1300,4000,1740); the game uses FQuat(Up, -115 deg)"),
          replaces=[("AKitchenDoor::Build Microwave", "door slab, window, handle", (1300, 3930, 1740), (2150, 4000, 2300))])
    lathe([(0, 0), (350 * K, 0), (370 * K, 6 * K), (370 * K, 16 * K), (0, 16 * K)], "ShelfGlass", n=20)
    piece("SM_K_Microwave_Plate", origin=(0, 0, 0), parent="SM_K_Microwave", at=[(*G(1760, 4425, 1750), 0)],
          motion={"type": "hinge", "axis": (0, 0, 1), "angle_deg": 360, "note": "turntable"},
          replaces=[("BuildAppliances", "glass plate", (1390, 4055, 1750), (2130, 4795, 1766))])

def toaster():
    push()
    gbox(-6600, -5900, 3900, 4500, 1724, 2150, "Steel", 0.030, 3)
    b = pop()
    push()
    for Yc in (4085, 4315):
        gbox(-6450, -6050, Yc - 35, Yc + 35, 1800, 2200, "Iron", 0.0)
    gbox(-5916, -5880, 4185, 4215, 1790, 2090, "Iron", 0.0)
    add(cut(b, pop()))
    for (X, Y) in ((-6520, 3980), (-5980, 3980), (-6520, 4420), (-5980, 4420)):
        gcyl(X, Y, 1710, 44, 18, "Plinth", 10)
    gbox(-6560, -5940, 3893, 3903, 1790, 1850, "Plinth", 0.003, 1)
    piece("SM_K_Toaster", replaces=[("Build", "toaster box + two slot decals", (-6600, 3900, 1710), (-5900, 4500, 2150))],
          note="two real slots 400 x 70 uu, 350 deep; lever slot on the game-east end as in AKitchenToaster")
    gbox(-5900, -5840, 4155, 4245, 2038, 2082, "Plinth", 0.006, 2)
    gbox(-5920, -5896, 4190, 4210, 2050, 2070, "Plinth", 0.0)
    piece("SM_K_Toaster_Lever", origin=G(-5900, 4200, 2060), parent="SM_K_Toaster",
          motion={"type": "slide", "vector_m": (0, 0, -240 * K), "note": "armed = pushed down 240 uu (the game moves it 2060 -> 1820)"},
          replaces=[("AKitchenToaster", "lever knob", (-5900, 4155, 2038), (-5845, 4245, 2082))])
    prism([(-170 * K, 0), (170 * K, 0), (170 * K, 130 * K), (140 * K, 178 * K), (60 * K, 190 * K), (-60 * K, 190 * K), (-140 * K, 178 * K), (-170 * K, 130 * K)], -25 * K, 25 * K, "Toast", "Y", bev=0.003)
    piece("SM_K_Toast", origin=(0, 0, 0), parent="SM_K_Toaster", at=[(*G(-6250, 4085, 1950), 0), (*G(-6250, 4315, 1950), 0)],
          motion={"type": "slide", "vector_m": (0, 0, 320 * K), "note": "pops up 320 uu"},
          replaces=[("AKitchenToaster", "toast slices", (-6420, 4060, 1950), (-6080, 4110, 2140))])

def build():
    fridge()
    fridge_food()
    range_()
    hood()
    dishwasher()
    microwave()
    toaster()