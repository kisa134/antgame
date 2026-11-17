# Ant Kitchen pieces, part 4: loose props - jars, crockery, pot, kettle, board + knife, cutlery, matchbox, newspaper, tea bag,
# towel, cake, shakers, rolling pin, bone, ball, shoe, mouse trap.
import math
from kitchen_lib import *

def sx(X):
    return -X * K

def P(pts):
    return [(r * K, h * K) for (r, h) in pts]

def jars():
    lathe(P([(0, 0), (150, 0), (170, 20), (170, 232), (142, 262), (142, 272), (0, 272)]), "Honey", n=16)
    lathe(P([(0, 268), (152, 268), (152, 310), (142, 320), (0, 320)]), "Brass", n=16)
    piece("SM_K_JarHoney", origin=(0, 0, 0), at=[centered(G(-6760, 260, 2850), 320 * K, (0, 90, 0))], on="SM_K_WallShelf",
          replaces=[("BuildSecrets", "honey jar d340 x 300, tipped over on the wall shelf", (-6910, 90, 2680), (-6610, 430, 3020))],
          note="modelled upright; the instance lies on its side with the lid towards the wall (the honey drip is an engine effect)")
    lathe(P([(0, 0), (124, 0), (140, 16), (140, 205), (118, 232), (118, 240), (0, 240)]), "Jam", n=14)
    lathe(P([(0, 236), (128, 236), (128, 280), (118, 290), (0, 290)]), "Red", n=14)
    piece("SM_K_JarJam", origin=(0, 0, 0),
          at=[centered(G(3600, 4000, 1070), 290 * K, (0, 90, 0)), centered(G(980, 520, 1540), 290 * K, (0, 90, -20)), (*G(-6780, -150, 2680), 0, (0.8, 0.8, 0.95))], on=["SM_K_Fridge", "SM_K_Table", "SM_K_WallShelf"],
          replaces=[("BuildFridge / BuildSecrets", "jam jar: tipped in the fridge (shelf 1), tipped on the table, upright on the wall shelf", (3455, 3860, 930), (3745, 4140, 1210))],
          note="modelled upright; instances 1 and 2 lie on their side")
    lathe(P([(0, 0), (225, 0), (250, 25), (250, 400), (216, 432), (0, 432)]), "Sugar", n=18)
    lathe(P([(0, 428), (232, 428), (232, 462), (70, 478), (56, 500), (0, 500)]), "Walnut", n=18)
    gbox(-130, 130, -252, -244, 150, 300, "Paper", 0.0)
    piece("SM_K_JarSugar", origin=(0, 0, 0), at=[(*G(800, 4600, 1710), 0)], replaces=[("Build", "sugar jar Cyl r250 h500 on the counter", (550, 4350, 1710), (1050, 4850, 2210))])

def crockery():
    # sugar bowl (hollow, with sugar and a few cubes)
    lathe(P([(0, 0), (180, 0), (200, 15), (290, 150), (300, 240), (300, 300), (282, 300), (282, 240), (272, 160), (190, 36), (0, 36)]), "Enamel", n=18)
    lathe(P([(0, 150), (268, 150), (270, 236), (150, 258), (0, 266)]), "Sugar", n=14)
    for (x, y, r) in ((40, 30, 15), (-70, -40, 40), (90, -90, 70)):
        ubox(x - 34, x + 34, y - 34, y + 34, 250, 310, "Sugar", 0.004, 1)
    piece("SM_K_SugarBowl", origin=(0, 0, 0), at=[(*G(-1000, 300, 1400), 0)], on="SM_K_Table", replaces=[("Build", "sugar bowl Cyl r300 h300 on the table", (-1300, 0, 1400), (-700, 600, 1700))])
    # mug (hollow, handle)
    lathe(P([(0, 0), (165, 0), (180, 15), (180, 300), (160, 300), (160, 32), (0, 32)]), "MugBlue", n=16)
    tube(spline([(170 * K, 0, 240 * K), (255 * K, 0, 225 * K), (285 * K, 0, 150 * K), (255 * K, 0, 75 * K), (170 * K, 0, 60 * K)], 3), 18 * K, "MugBlue", 6)
    piece("SM_K_Mug", origin=(0, 0, 0), at=[(*G(-2200, 4200, 1710), -40), (*G(2300, 300, 1400), 150, (1, 1, 1.27))],
          replaces=[("Build", "mug Cyl r180 h300 on the counter (second instance: the cup on the table, h380)", (-2380, 4020, 1710), (-2020, 4380, 2010))])
    # plate
    lathe(P([(0, 0), (260, 0), (280, 8), (440, 34), (450, 42), (444, 50), (270, 26), (0, 22)]), "Enamel", n=24)
    at = [(*G(0, -800, 1400), 0), (*G(3200, 3700, 1730), 0, (0.58, 0.58, 0.8))]
    for i in range(4):
        at.append((*G(650, 3350 + i * 330, 716), (90, 0, 0), (0.667, 0.667, 0.667)))
    piece("SM_K_Plate", origin=(0, 0, 0), at=at, on=["SM_K_Table", "SM_K_Fridge"] + ["SM_K_Dishwasher_Rack"] * 4, replaces=[("Build", "plate Cyl r450 h30 on the table (others: leftovers plate in the fridge, four dirty plates on edge in the dishwasher's lower rack)", (-450, -1250, 1400), (450, -350, 1430))])
    # bowl
    lathe(P([(0, 0), (120, 0), (140, 10), (225, 100), (230, 120), (214, 120), (206, 100), (130, 26), (0, 26)]), "Enamel", n=18)
    piece("SM_K_Bowl", origin=(0, 0, 0), at=[(*G(1760, 4425, 1766), 0), (*G(900, 4100, 1066), 0, (1.13, 1.13, 1.0))], on=["SM_K_Microwave_Plate", "SM_K_Dishwasher_Rack"],
          replaces=[("BuildAppliances", "bowl of leftovers in the microwave (second: bowl in the dishwasher's upper rack)", (1530, 4195, 1766), (1990, 4655, 1856))])
    # pot + lid on the front burner (the game's stove gadget keeps its own soup / glow)
    lathe(P([(0, 0), (400, 0), (420, 20), (420, 500), (440, 505), (440, 522), (396, 522), (396, 40), (0, 40)]), "Steel", n=24)
    for s in (-1, 1):
        box(s * 418 * K, s * 530 * K, -110 * K, 110 * K, 400 * K, 440 * K, "Plinth", 0.006, 2)
    piece("SM_K_Pot", origin=(0, 0, 0), at=[(*G(-350, 3500, 1765), 0)], on="SM_K_Range_Burner", replaces=[("AKitchenKnob", "pot Cyl d840 h520 + rim d880 + two handles", (-880, 3060, 1765), (180, 3940, 2301))])
    lathe(P([(0, 56), (300, 44), (440, 14), (440, 0), (424, 0), (290, 28), (0, 40)]), "Steel", n=24)
    lathe(P([(0, 52), (30, 52), (26, 82), (56, 92), (56, 112), (0, 118)]), "Plinth", n=12)
    piece("SM_K_Pot_Lid", origin=(0, 0, 0), at=[(*G(-350, 3500, 2287), 0)], on="SM_K_Pot", note="separate prop sitting on the pot rim; lift it off for the open soup pot of the game")
    # kettle
    lathe(P([(0, 0), (168, 0), (194, 12), (210, 60), (216, 125), (209, 195), (188, 258), (152, 306), (114, 332), (0, 334)]), "Red", n=20)
    lathe(P([(0, 330), (114, 330), (108, 350), (60, 368), (0, 372)]), "Red", n=14)
    cyl(22 * K, 44 * K, 0, 0, 390 * K, "Plinth", "Z", 10, 0.004, seg=1)
    tube([(175 * K, 0, 140 * K), (265 * K, 0, 205 * K), (325 * K, 0, 305 * K)], [40 * K, 31 * K, 24 * K], "Red", 8)
    tube(spline([(-120 * K, 0, 325 * K), (-150 * K, 0, 430 * K), (0, 0, 500 * K), (150 * K, 0, 430 * K), (120 * K, 0, 325 * K)], 4), 15 * K, "Plinth", 6)
    piece("SM_K_Kettle", origin=(0, 0, 0), at=[(*G(2250, 3600, 1710), 30)], note="new prop (asked for in the task; not in the game code); stands on worktop B clear of the microwave door's swing")

def worktop_props():
    gbox(-5600, -4500, 3250, 3950, 1710, 1760, "Walnut", 0.008, 2)
    piece("SM_K_CuttingBoard", replaces=[("Build", "cutting board", (-5600, 3250, 1710), (-4500, 3950, 1760))])
    prism([(-425 * K, 30 * K), (-330 * K, -30 * K), (175 * K, -30 * K), (175 * K, 30 * K)], 0, 16 * K, "Steel", "Z")
    ubox(175, 425, -40, 40, 0, 30, "Plinth", 0.006, 2)
    piece("SM_K_Knife", at=[(*G(-4975, 3480, 1760), 180)], on="SM_K_CuttingBoard", replaces=[("Build", "knife blade + handle on the board", (-5400, 3440, 1760), (-4550, 3520, 1790))])
    # tea bag on worktop B, its string hanging over the edge down to the floor
    rbox(200 * K, 200 * K, 50 * K, sx(2100), 3200 * K, 1735 * K, "Cardboard", 0.02, 2)
    tube(spline([G(2030, 3130, 1752), G(2020, 3010, 1748), G(2020, 2946, 1716), G(2020, 2938, 1560), G(2020, 2938, 150)], 4), 8 * K, "Paper", 6)
    gbox(1960, 2080, 2930, 2946, 0, 150, "Fabric", 0.003, 1)
    piece("SM_K_TeaBag", replaces=[("BuildClutter", "tea bag + string to the floor", (2000, 2940, 0), (2200, 3300, 1760))], note="one mesh: bag, string (16 uu thick) and tag; origin on the floor")
    # towel over the oven door handle, down to the floor
    gbox(-1050, -450, 2864, 2880, 0, 1404, "Fabric", 0.004, 1)
    gbox(-1050, -450, 2770, 2880, 0, 16, "Fabric", 0.004, 1)
    arc = [((2900 - 36 * math.cos(t)) * K, (1400 + 36 * math.sin(t)) * K) for t in [math.pi * i / 8 for i in range(9)]]
    arc += [((2900 - 20 * math.cos(t)) * K, (1400 + 20 * math.sin(t)) * K) for t in [math.pi * i / 8 for i in range(8, -1, -1)]]
    prism(arc, sx(-450), sx(-1050), "Fabric", "X")
    gbox(-1050, -450, 2920, 2936, 1040, 1404, "Fabric", 0.004, 1)
    gbox(-1052, -448, 2862, 2882, 250, 330, "Enamel", 0.0)
    piece("SM_K_Towel", on="SM_K_Range_OvenDoor", replaces=[("BuildClutter", "kitchen towel from the oven handle to the floor", (-1050, 2860, 0), (-450, 2900, 1640))],
          note="hangs over the oven door handle; static in the game (it does not follow the door)")

def table_props():
    lathe(P([(0, 0), (490, 0), (500, 10), (500, 446), (0, 446)]), "CakePink", n=24)
    lathe(P([(0, 440), (506, 440), (514, 452), (514, 488), (498, 502), (0, 506)]), "Enamel", n=24)
    for i in range(12):
        a = 2 * math.pi * i / 12
        cyl(40 * K, (70 + 40 * (i % 3)) * K, 505 * K * math.cos(a), 505 * K * math.sin(a), (440 - (70 + 40 * (i % 3)) / 2) * K, "Enamel", "Z", 8, 0.006, seg=1)
    for i in range(6):
        a = 2 * math.pi * i / 6
        ball(52 * K, 360 * K * math.cos(a), 360 * K * math.sin(a), 545 * K, "Red", (1, 1, 1), 10, 6)
    ball(60 * K, 0, 0, 552 * K, "Red", (1, 1, 1), 10, 6)
    piece("SM_K_Cake", origin=(0, 0, 0), at=[(*G(1600, -500, 1400), 0)], on="SM_K_Table", replaces=[("Build", "cake Cyl r500 h450 + icing + cherry", (1090, -1010, 1400), (2110, 10, 2020))])
    lathe(P([(0, 0), (72, 0), (80, 10), (70, 200), (56, 228), (0, 228)]), "Enamel", n=12)
    lathe(P([(0, 224), (60, 224), (62, 244), (42, 262), (0, 266)]), "Steel", n=12)
    piece("SM_K_Shaker", origin=(0, 0, 0), at=[(*G(-1450, -1200, 1400), 0), (*G(-1250, -1200, 1400), 0)], on="SM_K_Table", replaces=[("Build", "salt and pepper Cyl r80 h260 (one mesh, tint the second)", (-1530, -1280, 1400), (-1370, -1120, 1660))])

def floor_props():
    lathe(P([(0, -700), (50, -700), (58, -680), (58, -480), (40, -465), (40, -452), (130, -450), (140, -435), (140, 435), (130, 450), (40, 452), (40, 465), (58, 480), (58, 680), (50, 700), (0, 700)]), "Butcher", 0, 0, 140 * K, 16, "X")
    piece("SM_K_RollingPin", at=[(*G(-5400, 1900, 0), 90)], replaces=[("BuildClutter", "rolling pin Cyl d280 x 1400 lying along Y", (-5540, 1200, 0), (-5260, 2600, 280))])
    # matchbox: sleeve + sliding tray (slides towards -Y of the piece = game east for this instance)
    ubox(-200, 200, -250, 250, 0, 16, "Blue", 0.002, 1)
    ubox(-200, 200, -250, 250, 144, 160, "Blue", 0.002, 1)
    for x0, x1 in ((-200, -184), (184, 200)):
        ubox(x0, x1, -250, 250, 16, 144, "Blue", 0.002, 1)
    for x0, x1 in ((-208, -198), (198, 208)):
        ubox(x0, x1, -225, 225, 34, 126, "Walnut", 0.0)
    ubox(-150, 150, -190, 190, 158, 168, "Fabric", 0.002, 1)
    piece("SM_K_Matchbox", at=[(*G(-1950, 2600, 0), -90)], replaces=[("BuildClutter", "matchbox sleeve", (-2200, 2400, 0), (-1700, 2800, 160))], note="sleeve open at both ends")
    tray(-180 * K, 180 * K, -244 * K, 244 * K, 18 * K, 126 * K, 16 * K, "Cardboard", 0.002, 1)
    for i, x in enumerate((-140, -105, -70)):
        cyl(13 * K, 400 * K, x * K, -10 * K, 48 * K, "Butcher", "Y", 6)
        ball(20 * K, x * K, -212 * K, 48 * K, "Red", (1, 1.3, 1), 8, 5)
    piece("SM_K_Matchbox_Tray", origin="back", parent="SM_K_Matchbox", state="half open: 200 uu out (as the game shows it)",
          motion={"type": "slide", "vector_m": (0, -300 * K, 0), "note": "slides out of the sleeve; rest pose = fully inside"},
          replaces=[("BuildClutter", "matchbox tray sticking out", (-1700, 2420, 10), (-1500, 2780, 120))])
    # newspaper tent
    c, s = math.cos(math.radians(32)), math.sin(math.radians(32))
    rbox(900 * K, 620 * K, 16 * K, 0, -310 * c * K, (340 - 310 * s) * K, "Paper", 0.003, 1, rot=(32, 0, 0))
    rbox(900 * K, 620 * K, 16 * K, 0, 310 * c * K, (340 - 310 * s) * K, "Paper", 0.003, 1, rot=(-32, 0, 0))
    tube([(-450 * K, 0, 340 * K), (450 * K, 0, 340 * K)], 12 * K, "Paper", 6)
    for (y, w) in ((-420, 500), (-330, 700), (330, 700), (420, 500)):
        zz = 340 - abs(y) / c * s
        rbox(w * K, 40 * K, 18 * K, 0, y * K, (zz + 1) * K, "Grout", 0.0, rot=(32 if y < 0 else -32, 0, 0))
    piece("SM_K_Newspaper", at=[(*G(1225, -4325, 0), 165)], replaces=[("BuildClutter", "newspaper: two tented sheets", (700, -4950, 0), (1750, -3700, 340))], note="a tent: ridge 340 uu high, open at both ends")
    # spoon
    prism([(-380 * K, -22 * K), (700 * K, -40 * K), (750 * K, 0), (700 * K, 40 * K), (-380 * K, 22 * K)], 0, 16 * K, "Steel", "Z")
    push()
    ball(1.0, -545 * K, 0, 34 * K, "Steel", (205 * K, 150 * K, 44 * K), 14, 8)
    b = pop()
    push()
    ball(1.0, -545 * K, 0, 52 * K, "Steel", (190 * K, 135 * K, 44 * K), 14, 8)
    add(cut(b, pop()))
    piece("SM_K_Spoon", at=[(*G(-2000, -1500, 715), (0, -66.8, 180))], on="SM_K_Table",
          replaces=[("BuildClutter", "spoon leaning against the table (the game leans it at the south-west leg; here it rests on the west edge of the table top)", (-2110, -2030, 0), (-1655, -1870, 1300))],
          note="modelled lying flat, bowl at -X; the instance leans from the floor up to the table edge: a ramp")
    # fork
    prism([(-100 * K, -18 * K), (430 * K, -32 * K), (450 * K, 0), (430 * K, 32 * K), (-100 * K, 18 * K)], 0, 16 * K, "Steel", "Z")
    prism([(-100 * K, -18 * K), (-100 * K, 18 * K), (-200 * K, 60 * K), (-262 * K, 60 * K), (-262 * K, -60 * K), (-200 * K, -60 * K)], 0, 16 * K, "Steel", "Z")
    for y in (-50, -17, 17, 50):
        ubox(-450, -258, y - 10, y + 10, 0, 16, "Steel", 0.0)
    piece("SM_K_Fork", at=[(*G(1200, -2400, 0), 145)], replaces=[("BuildClutter", "fork on the floor 900x80x30, yaw 35", (750, -2700, 0), (1650, -2100, 30))])
    # bone, tennis ball
    lathe(P([(0, -300), (44, -300), (44, 300), (0, 300)]), "Paper", 0, 0, 72 * K, 10, "X")
    for sxn in (-1, 1):
        for syn in (-1, 1):
            ball(72 * K, sxn * 310 * K, syn * 44 * K, 72 * K, "Paper", (1, 1, 1), 10, 6)
    piece("SM_K_Bone", at=[(*G(-3100, -4300, 0), 0)], replaces=[("BuildClutter", "dog bone", (-3550, -4380, 0), (-2650, -4220, 130))])
    ball(190 * K, 0, 0, 190 * K, "TennisBall", (1, 1, 1), 16, 10)
    piece("SM_K_TennisBall", at=[(*G(500, -400, 0), 0)], replaces=[("BuildClutter", "tennis ball r190", (310, -590, 0), (690, -210, 380))])
    # shoe (toe towards -Y), hollow at the heel
    box(-220 * K, 220 * K, -500 * K, 500 * K, 0, 50 * K, "Plinth", 0.012, 2)
    box(-200 * K, 200 * K, -480 * K, 20 * K, 44 * K, 230 * K, "Leather", 0.05, 3)
    push()
    box(-200 * K, 200 * K, -60 * K, 480 * K, 44 * K, 390 * K, "Leather", 0.04, 3)
    b = pop()
    push()
    box(-165 * K, 165 * K, 10 * K, 440 * K, 80 * K, 500 * K, "Plinth", 0.0)
    add(cut(b, pop()))
    for y in (-110, -190, -270):
        box(-150 * K, 150 * K, (y - 12) * K, (y + 12) * K, 222 * K, 242 * K, "Paper", 0.004, 1)
    piece("SM_K_Shoe", at=[(*G(6550, -4150, 0), 0)], replaces=[("BuildClutter", "shoe: box + blob + sole", (6150, -4650, 0), (6950, -3650, 520))], note="hollow from the top at the heel (a hiding place)")
    # mouse trap (under the sink), bar is a separate hinged part
    ubox(-190, 190, -90, 90, 0, 16, "Butcher", 0.003, 1)
    ubox(40, 110, -35, 35, 16, 26, "Brass", 0.002, 1)
    prism([(52 * K, -20 * K), (100 * K, 0), (52 * K, 20 * K)], 26 * K, 56 * K, "Cheese", "Z")
    cyl(9 * K, 150 * K, 0, 0, 25 * K, "Steel", "Y", 6)
    piece("SM_K_MouseTrap", origin=(0, 0, 0), at=[(*G(-3150, 3450, 140), 90)], on="SM_K_BaseSink", replaces=[("AKitchenTrap", "trap base + bait plate", (-3240, 3260, 140), (-3060, 3640, 162))])
    tube([(0, -83 * K, 26 * K), (-172 * K, -83 * K, 26 * K), (-172 * K, 83 * K, 26 * K), (0, 83 * K, 26 * K)], 8 * K, "Steel", 5)
    piece("SM_K_MouseTrap_Bar", origin=(0, 0, 22 * K), parent="SM_K_MouseTrap",
          motion={"type": "hinge", "axis": (0, 1, 0), "angle_deg": 180, "note": "snaps over the spring axis onto the bait side"},
          replaces=[("AKitchenTrap", "the steel bar", (-3238, 3275, 162), (-3062, 3450, 170))])

def build():
    jars()
    crockery()
    worktop_props()
    table_props()
    floor_props()