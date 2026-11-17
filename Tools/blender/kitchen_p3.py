# Ant Kitchen pieces, part 3: furniture and the room - table, chairs, rug, bin, bucket + mop, clock, switch, window,
# curtains, wall shelf, cereal box, pet corner, skirting, vent grilles, ceiling lamp, utensil rail, spice shelves.
import math
from kitchen_lib import *

def sx(X):
    return -X * K

def hx(angle, note):
    return {"type": "hinge", "axis": (1, 0, 0), "angle_deg": angle, "note": note}

def furniture():
    # ---- dining table
    gbox(-1700, 2700, -1800, 800, 1320, 1400, "Butcher", 0.012, 2)
    gbox(-1580, 2580, -1680, -1630, 1170, 1322, "Walnut", 0.004, 1)
    gbox(-1580, 2580, 630, 680, 1170, 1322, "Walnut", 0.004, 1)
    gbox(-1580, -1530, -1630, 630, 1170, 1322, "Walnut", 0.004, 1)
    gbox(2530, 2580, -1630, 630, 1170, 1322, "Walnut", 0.004, 1)
    for (X, Y) in ((-1500, -1600), (2500, -1600), (-1500, 600), (2500, 600)):
        gcyl(X, Y, 0, 88, 1322, "Walnut", 14, 0.006, r2=112)
    piece("SM_K_Table", replaces=[("Build", "dining table top + four legs", (-1700, -1800, 0), (2700, 800, 1400))])
    # ---- chair (one mesh, two instances); the back is at +Y
    ubox(-450, 450, -450, 530, 800, 880, "Cabinet", 0.014, 2)
    for (x, y) in ((-380, -380), (380, -380)):
        cyl(36 * K, 802 * K, x * K, y * K, 401 * K, "Walnut", "Z", 10, 0.004, seg=1)
    for x in (-380, 380):
        cyl(36 * K, 2090 * K, x * K, 490 * K, 1045 * K, "Walnut", "Z", 10, 0.004, seg=1)
        ubox(x - 16, x + 16, -380, 490, 300, 350, "Walnut", 0.003, 1)
    ubox(-380, 380, -396, -364, 300, 350, "Walnut", 0.003, 1)
    ubox(-420, 420, 462, 518, 1730, 2100, "Cabinet", 0.012, 2)
    ubox(-380, 380, 470, 510, 1250, 1350, "Cabinet", 0.008, 2)
    piece("SM_K_Chair", origin=(0, 0, 0), at=[(*G(0, -2750, 0), 180), (*G(1200, 1650, 0), 0)],
          replaces=[("Chair()", "seat, four legs, back slab (first instance: back to the south)", (-450, -3280, 0), (450, -2300, 2100))])
    # ---- rug under the table
    gbox(-2500, 3500, -2600, 1600, 0, 15, "Red", 0.004, 1)
    for s in (-1, 0, 1):
        Xc = 500 + s * 1900
        gbox(Xc - 130, Xc + 130, -2603, 1603, 0, 17, "Paper", 0.002, 1)
    piece("SM_K_Rug", replaces=[("Build", "rug (4 uu thick in the game, 15 uu here)", (-2500, -2600, 0), (3500, 1600, 4))])

def bin_bucket():
    X, Y = 6000, -1500
    lathe([(0, 0), (640 * K, 0), (650 * K, 10 * K), (650 * K, 1540 * K), (610 * K, 1540 * K), (610 * K, 30 * K), (0, 30 * K)], "Steel", sx(X), Y * K, 0, 24)
    lathe([(640 * K, 0), (662 * K, 8 * K), (662 * K, 70 * K), (650 * K, 82 * K)], "Plinth", sx(X), Y * K, 0, 24, closed=True)
    piece("SM_K_Bin", replaces=[("BuildAppliances", "pedal bin: floor disc + 12 wall panels (hollow)", (5350, -2150, 0), (6650, -850, 1540))],
          note="hollow: inner radius 610 uu, floor top at z=30 (as in the game)")
    lathe([(0, 1542 * K), (690 * K, 1542 * K), (690 * K, 1574 * K), (600 * K, 1600 * K), (0, 1612 * K)], "Steel", sx(X), Y * K, 0, 24)
    lathe([(0, 1606 * K), (200 * K, 1606 * K), (200 * K, 1634 * K), (180 * K, 1644 * K), (0, 1644 * K)], "Plinth", sx(X), Y * K, 0, 16)
    gbox(X - 150, X + 150, Y + 630, Y + 700, 1500, 1562, "Plinth", 0.006, 1)
    piece("SM_K_Bin_Lid", origin=G(X, Y + 660, 1540), parent="SM_K_Bin",
          motion=hx(-75, "hinge at the back (game +Y side), the pivot of AKitchenPedalBin (0,660,1540); the game uses FQuat(Forward, -75 deg)"),
          replaces=[("AKitchenPedalBin", "lid disc d1380 + knob + hinge barrel", (5310, -2190, 1520), (6690, -800, 1610))])
    gbox(X - 160, X + 160, Y - 880, Y - 640, 12, 60, "Plinth", 0.006, 2)
    gbox(X - 30, X + 30, Y - 690, Y - 590, 10, 45, "Plinth", 0.003, 1)
    piece("SM_K_Bin_Pedal", origin=G(X, Y - 640, 35), parent="SM_K_Bin", motion=hx(12, "pressed: the front edge goes down 12 deg"),
          replaces=[("AKitchenPedalBin", "pedal + lever", (5840, -2380, 0), (6160, -2090, 60))])
    # ---- bucket and mop
    X, Y = 6200, 3900
    lathe([(0, 0), (335 * K, 0), (345 * K, 10 * K), (420 * K, 630 * K), (434 * K, 630 * K), (434 * K, 650 * K), (400 * K, 650 * K), (324 * K, 22 * K), (0, 22 * K)], "PlasticBlue", sx(X), Y * K, 0, 20)
    hp = [(sx(X) + 432 * K * math.cos(a), Y * K - 380 * K * math.sin(a), (610 - 260 * math.sin(a)) * K) for a in [math.pi * i / 12 for i in range(13)]]
    tube(hp, 10 * K, "Steel", 6)
    piece("SM_K_Bucket", replaces=[("Build", "mop bucket Cyl r420 h650", (5780, 3480, 0), (6620, 4320, 650))], note="hollow, 20 uu wall; the water surface is left to the engine")
    cyl(30 * K, 4060 * K, 0, 0, 2270 * K, "Butcher", "Z", 10, 0.004, seg=1)
    ubox(-150, 150, -46, 46, 180, 270, "Blue", 0.008, 2)
    for i in range(9):
        x = (i - 4) * 32
        tube(spline([(x * K, 0, 200 * K), (x * 1.5 * K, (14 if i % 2 else -14) * K, 100 * K), (x * 2.0 * K, (26 if i % 2 else -26) * K, 12 * K)], 3), 17 * K, "Paper", 5)
    piece("SM_K_Mop", origin=(0, 0, 0), at=[(*G(6450, 3953, 25), (-12, 0, 0))], on="SM_K_Bucket",
          replaces=[("Build", "leaning mop stick 60x60x4300 (roll -12 deg)", (6420, 3953, 47), (6480, 4847, 4253))],
          note="modelled upright (head at the bottom); the instance leans 12 deg towards the north wall with its head in the bucket")

def walls():
    # ---- wall clock, east wall (front faces game -X)
    cyl(300 * K, 36 * K, 0, -18 * K, 0, "Plinth", "Y", 24, 0.004, seg=1)
    cyl(270 * K, 42 * K, 0, -21 * K, 0, "Enamel", "Y", 24)
    for i in range(12):
        a = math.radians(30 * i)
        rbox(10 * K, 8 * K, (46 if i % 3 == 0 else 26) * K, 232 * K * math.sin(a), -42 * K, 232 * K * math.cos(a), "Plinth", 0.0, rot=(0, 30 * i, 0))
    for (L, w, ang) in ((150, 22, 60), (215, 14, 180)):
        a = math.radians(ang)
        rbox(w * K, 8 * K, L * K, (L / 2 - 20) * K * math.sin(a), -48 * K, (L / 2 - 20) * K * math.cos(a), "Plinth", 0.0, rot=(0, ang, 0))
    cyl(20 * K, 14 * K, 0, -53 * K, 0, "Brass", "Y", 10)
    piece("SM_K_Clock", at=[(*G(7000, 1500, 3600), 90)], replaces=[("Build", "clock disc d600 + two hands, east wall", (6960, 1200, 3300), (7000, 1800, 3900))], note="hands are part of the mesh (4 o'clock)")
    # ---- light switch, west wall (front faces game +X)
    ubox(-70, 70, -16, 0, -100, 100, "Enamel", 0.004, 2)
    ubox(-30, 30, -24, -14, -52, 52, "Paper", 0.002, 1)
    piece("SM_K_Switch", at=[(*G(-7000, 1900, 2300), -90)], replaces=[("AKitchenSwitch", "plate + frame", (-7000, 1830, 2200), (-6981, 1970, 2400))])
    ubox(-18, 18, -52, -18, -14, 14, "Enamel", 0.004, 1)
    piece("SM_K_Switch_Toggle", origin=(0, -18 * K, 0), parent="SM_K_Switch",
          motion=hx(25, "rocker: +25 deg / -25 deg about the horizontal axis along the wall (the game uses pitch +-25)"),
          replaces=[("AKitchenSwitch", "toggle 34x36x24", (-6983, 1882, 2288), (-6949, 1918, 2312))])
    # ---- window: frame lining the opening in the south wall (local x = game X, +y goes into the wall)
    for x0, x1 in ((-1500, -1440), (1440, 1500)):
        ubox(x0, x1, -20, 200, 0, 2000, "Enamel", 0.004, 1)
    ubox(-1440, 1440, -20, 200, 1940, 2000, "Enamel", 0.004, 1)
    ubox(-1440, 1440, -20, 200, 0, 60, "Enamel", 0.004, 1)
    ubox(-30, 30, 60, 120, 60, 1940, "Enamel", 0.004, 1)
    ubox(-1440, 1440, 60, 120, 970, 1030, "Enamel", 0.004, 1)
    for x0, x1 in ((-1610, -1500), (1500, 1610)):
        ubox(x0, x1, -24, 0, 0, 2110, "Enamel", 0.006, 2)
    ubox(-1610, 1610, -24, 0, 2000, 2110, "Enamel", 0.006, 2)
    piece("SM_K_Window", at=[(*G(0, -5000, 2200), 180)], replaces=[("Build", "window mullions in the 3000 x 2000 opening (the game has no frame)", (-1500, -5000, 2200), (1500, -4980, 4200))],
          note="no glass on purpose: in the game the window is an open hole that lights the room and that ants climb through")
    ubox(-1650, 1650, -180, 0, -80, 0, "Butcher", 0.008, 2)
    piece("SM_K_WindowSill", at=[(*G(0, -5000, 2200), 180), (*G(0, -5200, 2200), 0)],
          replaces=[("Build / BuildYard", "inner sill (second instance: the outer sill)", (-1650, -5000, 2120), (1650, -4820, 2200))])
    # ---- potted plant on the sill (as in kitchen_set.py)
    lathe([(0, 0), (92 * K, 0), (100 * K, 10 * K), (126 * K, 172 * K), (134 * K, 172 * K), (134 * K, 196 * K), (0, 196 * K)], "Terracotta", n=14)
    cyl(118 * K, 10 * K, 0, 0, 198 * K, "Walnut", "Z", 12)
    for i in range(6):
        a = i * math.pi / 3
        ball(1.0, 80 * K * math.cos(a), 64 * K * math.sin(a), (330 + (i % 2) * 70) * K, "Leaf", (58 * K, 94 * K, 150 * K), 14, 10, rot=(0, 0, math.degrees(a)))
    piece("SM_K_PlantPot", origin=(0, 0, 0), at=[(*G(-1250, -4905, 2200), 0)], on="SM_K_WindowSill", note="new prop from the style reference (uses the Terracotta and Leaf slots); not in the game code")
    # ---- curtain panel (one mesh, two instances) and the rod
    n = 24
    top = [((-180 + 360 * i / n) * K, (-60 + 22 * math.sin(i * math.pi / 2.0 * 4 / 4 * 0.5 * 2)) * K) for i in range(n + 1)]
    top = [(x, (-60 + 22 * math.sin(i * math.pi / 4)) * K) for i, (x, _) in enumerate(top)]
    poly = [(x, y - 8 * K) for (x, y) in top] + [(x, y + 8 * K) for (x, y) in reversed(top)]
    prism(poly, 0, 5290 * K, "Fabric", "Z")
    ball(1.0, 0, -150 * K, 100 * K, "Fabric", (280 * K, 190 * K, 100 * K), 12, 6)
    for x in (-120, 0, 120):
        lathe([((34 + 8 * math.cos(a * math.pi / 3)) * K, (8 * math.sin(a * math.pi / 3)) * K) for a in range(6)], "Brass", x * K, -60 * K, 5320 * K, 10, "X", closed=True)
    piece("SM_K_Curtain", origin=(0, 0, 0), at=[(*G(-1750, -5000, 0), 180), (*G(1750, -5000, 0), 180)],
          replaces=[("BuildClutter", "curtain slab 360x60x5400 + the pooled blob on the floor", (-1930, -5050, 0), (-1570, -4650, 5400))])
    tube([(-2000 * K, -60 * K, 5320 * K), (2000 * K, -60 * K, 5320 * K)], 20 * K, "Brass", 10)
    for x in (-2030, 2030):
        ball(46 * K, x * K, -60 * K, 5320 * K, "Brass", (1, 1, 1), 10, 6)
    for x in (-1900, 1900):
        ubox(x - 16, x + 16, -60, 0, 5304, 5336, "Brass", 0.003, 1)
    piece("SM_K_CurtainRod", at=[(*G(0, -5000, 0), 180)], replaces=[("BuildClutter", "curtain rod", (-2000, -4960, 5300), (2000, -4920, 5340))])
    # ---- open wall shelf with brackets, west wall
    ubox(-1100, 1100, -450, 0, 0, 80, "Butcher", 0.006, 2)
    for x in (-875, 875):
        ubox(x - 25, x + 25, -40, 0, -200, 0, "Iron", 0.004, 1)
        ubox(x - 25, x + 25, -380, 0, -40, 0, "Iron", 0.004, 1)
        rbox(40 * K, 250 * K, 30 * K, x * K, -150 * K, -110 * K, "Iron", 0.003, 1, rot=(-40, 0, 0))
    piece("SM_K_WallShelf", at=[(*G(-7000, 0, 2600), -90)], replaces=[("Build", "wall shelf board + two brackets, west wall", (-7000, -1100, 2400), (-6550, 1100, 2680))])
    # ---- skirting: a 1000 uu length, stretched along X per wall run
    prism([(0, 0), (-30 * K, 0), (-30 * K, 170 * K), (-22 * K, 196 * K), (-10 * K, 210 * K), (-10 * K, 220 * K), (0, 220 * K)], -500 * K, 500 * K, "Enamel", "X")
    runs = [(-7000, -4600, "S"), (-4400, -3300, "S"), (-2900, 7000, "S"), (-3160, 200, "E"), (600, 5000, "E"), (-5000, -4840, "E"), (-5000, -2750, "W"), (-2250, 5000, "W")]
    at = []
    for a, b, w in runs:
        c, s = (a + b) / 2, ((b - a) / 1000.0, 1, 1)
        at.append({"S": (*G(c, -5000, 0), 180, s), "E": (*G(7000, c, 0), 90, s), "W": (*G(-7000, c, 0), -90, s)}[w])
    piece("SM_K_Skirting", at=at, replaces=[("BuildShell", "baseboards 30 x 220 with gaps at the cracks and grilles (first run: south wall, west of the window crack)", (-7000, -5000, 0), (-4600, -4970, 220))],
          note="1000 uu module; every instance carries scale X = run length / 1000")

def grille(name, H, at, rep):
    for x0, x1 in ((-230, -200), (200, 230)):
        ubox(x0, x1, -10, 20, -30, H + 30, "Steel", 0.003, 1)
    ubox(-200, 200, -10, 20, -30, 0, "Steel", 0.003, 1)
    ubox(-200, 200, -10, 20, H, H + 30, "Steel", 0.003, 1)
    z = 56
    while z < H - 20:
        rbox(400 * K, 34 * K, 16 * K, 0, 5 * K, z * K, "Steel", 0.0, rot=(25, 0, 0))
        z += 80
    piece(name, at=at, replaces=rep, note="frame + tilted slats 16 uu thick, 80 uu pitch (about 50 uu clear between them): ants squeeze between them")

def floor_things():
    # ---- cereal box on its side, open end to the game-east: a tunnel
    gbox(-5800, -4600, 500, 1300, 0, 20, "Red", 0.002, 1)
    gbox(-5800, -4600, 500, 1300, 400, 420, "Red", 0.002, 1)
    gbox(-5800, -4600, 500, 520, 20, 400, "Red", 0.002, 1)
    gbox(-5800, -4600, 1280, 1300, 20, 400, "Red", 0.002, 1)
    gbox(-5800, -5780, 520, 1280, 20, 400, "Red", 0.002, 1)
    gbox(-5700, -4700, 492, 501, 120, 300, "Fabric", 0.002, 1)
    gbox(-5700, -4700, 600, 1200, 419, 428, "Fabric", 0.002, 1)
    gbox(-4600, -4420, 520, 1280, 0, 16, "Red", 0.002, 1)
    c35, s35 = math.cos(math.radians(35)), math.sin(math.radians(35))
    rbox(200 * K, 760 * K, 16 * K, (4600 - 100 * c35) * K, 900 * K, (412 + 100 * s35) * K, "Red", 0.002, 1, rot=(0, 35, 0))
    c40, s40 = math.cos(math.radians(40)), math.sin(math.radians(40))
    rbox(200 * K, 16 * K, 380 * K, (4600 - 100 * c40) * K, (508 - 100 * s40) * K, 210 * K, "Red", 0.002, 1, rot=(0, 0, 40))
    rbox(200 * K, 16 * K, 380 * K, (4600 - 100 * c40) * K, (1292 + 100 * s40) * K, 210 * K, "Red", 0.002, 1, rot=(0, 0, -40))
    piece("SM_K_CerealBox", replaces=[("Build", "cereal box on its side: five slabs + label", (-5800, 494, 0), (-4600, 1300, 420))],
          note="hollow, 20 uu walls, floor top at z=20; four flaps folded open at the game-east end")
    # ---- pet corner: mat, bowl (two instances), kibble
    gbox(-4500, -1800, -4500, -3200, 0, 15, "Rubber", 0.006, 2)
    piece("SM_K_PetMat", replaces=[("Build", "rubber mat (4 uu in the game)", (-4500, -4500, 0), (-1800, -3200, 4))])
    lathe([(0, 0), (470 * K, 0), (480 * K, 12 * K), (432 * K, 170 * K), (402 * K, 170 * K), (384 * K, 150 * K), (366 * K, 40 * K), (0, 40 * K)], "Red", n=20)
    piece("SM_K_PetBowl", origin=(0, 0, 0), at=[(*G(-3800, -3800, 15), 0), (*G(-2500, -3900, 15), 0, (0.875, 0.875, 0.82))],
          replaces=[("Build", "food bowl Cyl r480 h170 (second instance: the water bowl r420 h140)", (-4280, -4280, 0), (-3320, -3320, 170))], note="hollow; stands on the mat (z=15)")
    lathe([(0, 0), (360 * K, 0), (356 * K, 110 * K), (300 * K, 160 * K), (150 * K, 195 * K), (0, 205 * K)], "Kibble", n=14)
    for i in range(9):
        a = i * 2.4
        r = 60 + 30 * (i % 4)
        cyl(34 * K, 30 * K, r * K * math.cos(a), r * K * math.sin(a), (196 - r * 0.18) * K, "Kibble", "Z", 8, rot=(20 * (i % 3), 15 * (i % 2), 0))
    piece("SM_K_Kibble", origin=(0, 0, 0), at=[(*G(-3800, -3800, 55), 0)], on="SM_K_PetBowl", replaces=[("Build", "kibble heap", (-4140, -4140, 170), (-3460, -3460, 265))])

def ceiling_and_rail():
    lathe([(0, 0), (340 * K, 0), (350 * K, -10 * K), (350 * K, -250 * K), (334 * K, -250 * K), (334 * K, -20 * K), (0, -20 * K)], "LampShade", n=20)
    ball(70 * K, 0, 0, -150 * K, "Enamel", (1, 1, 1.2), 10, 6)
    cyl(16 * K, 80 * K, 0, 0, -60 * K, "Plinth", "Z", 8)
    piece("SM_K_CeilingLamp", at=[(*G(-3500, 0, 5400), 0), (*G(2200, -1500, 5400), 0), (*G(1000, 2800, 5400), 0)],
          replaces=[("Build", "lamp shade Cyl r350 h250 under the ceiling (three in the kitchen)", (-3850, -350, 5150), (-3150, 350, 5400))])
    # ---- utensil rail over the stove with a ladle, a spatula and a whisk hanging from it
    tube([G(-1500, 4900, 2760), G(0, 4900, 2760)], 15 * K, "Steel", 8)
    for X in (-1500, 0):
        gbox(X - 10, X + 10, 4900, 5000, 2735, 2785, "Steel", 0.003, 1)
    piece("SM_K_UtensilRail", replaces=[("BuildAppliances", "utensil rail + two brackets", (-1510, 4885, 2700), (10, 5000, 2800))])
    def hook():
        tube(spline([(0, 0, -70 * K), (0, -28 * K, -34 * K), (0, -28 * K, 8 * K), (0, 0, 30 * K), (0, 28 * K, 8 * K), (0, 28 * K, -8 * K)], 3), 8 * K, "Steel", 6)
    hook()
    cyl(12 * K, 640 * K, 0, 0, -380 * K, "Steel", "Z", 8)
    lathe([(0, -110 * K), (60 * K, -92 * K), (98 * K, -50 * K), (110 * K, 0), (94 * K, 0), (82 * K, -45 * K), (50 * K, -76 * K), (0, -92 * K)], "Steel", 0, -70 * K, -650 * K, 12, rot=(-25, 0, 0))
    piece("SM_K_Ladle", origin=(0, 0, 0), at=[(*G(-1150, 4900, 2760), 0)], on="SM_K_UtensilRail", replaces=[("BuildAppliances", "ladle handle + bowl", (-1260, 4750, 1970), (-1040, 4970, 2750))], note="origin on the rail axis")
    hook()
    cyl(14 * K, 800 * K, 0, 0, -460 * K, "Walnut", "Z", 8)
    ubox(-100, 100, -10, 10, -1010, -850, "Iron", 0.004, 1)
    piece("SM_K_Spatula", origin=(0, 0, 0), at=[(*G(-500, 4900, 2760), 0)], on="SM_K_UtensilRail", replaces=[("BuildAppliances", "spatula handle + blade (reaches the stove top)", (-600, 4887, 1750), (-400, 4913, 2750))], note="origin on the rail axis")
    hook()
    cyl(14 * K, 240 * K, 0, 0, -180 * K, "Steel", "Z", 8)
    for ph in (0, 60, 120):
        c, s = math.cos(math.radians(ph)), math.sin(math.radians(ph))
        tube([(60 * K * math.sin(t) * c, 60 * K * math.sin(t) * s, (-300 - 130 * (1 - math.cos(t))) * K) for t in [2 * math.pi * i / 12 for i in range(12)]] + [(0, 0, -300 * K)], 8 * K, "Steel", 5)
    piece("SM_K_Whisk", origin=(0, 0, 0), at=[(*G(-900, 4900, 2760), 0)], on="SM_K_UtensilRail", replaces=[("BuildAppliances", "whisk handle + head", (-970, 4830, 2210), (-830, 4970, 2750))], note="origin on the rail axis")
    # ---- spice shelves (one mesh, four instances) and spice jar (one mesh, nine instances)
    ubox(-300, 300, -130, 0, 0, 20, "Butcher", 0.003, 1)
    ubox(-300, 300, -130, -114, 20, 46, "Butcher", 0.003, 1)
    for x in (-240, 240):
        ubox(x - 10, x + 10, -100, 0, -60, 0, "Iron", 0.003, 1)
    piece("SM_K_SpiceShelf", at=[(*G(-2100, 4970, 1900 + 300 * i), 0) for i in range(4)], replaces=[("BuildAppliances", "spice rack shelf 600x130x20 (four of them)", (-2400, 4840, 1900), (-1800, 4970, 1920))])
    lathe([(0, 0), (42 * K, 0), (45 * K, 5 * K), (45 * K, 106 * K), (37 * K, 120 * K), (0, 120 * K)], "Terracotta", n=10)
    lathe([(0, 118 * K), (41 * K, 118 * K), (41 * K, 144 * K), (36 * K, 150 * K), (0, 150 * K)], "Steel", n=10)
    at = []
    for i in range(4):
        Z = 1920 + 300 * i
        at.append((*G(-2300 + (i % 2) * 150, 4905, Z), 0))
        at.append((*G(-1950 - (i % 2) * 100, 4905, Z), 0, (0.89, 0.89, 0.87)))
    at.append((*G(-6780, -950, 2680), 0, (1.33, 1.33, 1.47)))
    piece("SM_K_SpiceJar", origin=(0, 0, 0), at=at, on=["SM_K_SpiceShelf"] * 8 + ["SM_K_WallShelf"], replaces=[("BuildAppliances", "spice jars Cyl r45 h150 / r40 h130 (last instance: the small jar on the wall shelf)", (-2345, 4860, 1920), (-2255, 4950, 2070))])

def build():
    furniture()
    bin_bucket()
    walls()
    grille("SM_K_VentGrille", 300, [(*G(-3100, -5000, 80), 180), (*G(7000, 400, 80), 90)],
           [("BuildDucts", "floor-level vent grille 400 x 300 (south wall G1; second instance: east wall G3)", (-3330, -5020, 50), (-2870, -4990, 410))])
    grille("SM_K_VentGrilleSmall", 160, [(*G(-4500, 5000, 4480), 0), (*G(3700, 5000, 3700), 0)],
           [("BuildDucts", "high vent grille 400 x 160 (north wall G2 over the wall cabinets; second: G4 over the fridge)", (-4730, 4990, 4450), (-4270, 5020, 4670))])
    floor_things()
    ceiling_and_rail()