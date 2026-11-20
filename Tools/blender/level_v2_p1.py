# Ant Kitchen level v2, part 1: the new kitchen pieces. Local frame of a placed piece: game units, front = -Y, origin on the floor
# (or on the surface it stands on) where level_v2_layout.PUT puts it. Pieces modelled in place read their key points from PLACE.
import math
from level_v2_common import *

def high_chair():
    for sx in (-1, 1):
        for sy in (-1, 1):
            UT([(sx * 430, sy * 430, 0), (sx * 300, sy * 300, 1450)], 30, "Butcher")
            UC(42, 30, sx * 430, sy * 430, 15, "Plinth", n=10)
    UB(-360, 360, -360, 360, 1440, 1500, "Butcher", 10, 2)                    # seat, top at 1500
    UB(-300, 300, -300, 300, 1500, 1530, "Teal", 12, 2)                       # cushion
    UB(-360, 360, 300, 360, 1500, 2640, "Butcher", 14, 2)                     # back
    UB(-280, 280, 286, 302, 1700, 2500, "Teal", 10, 2)
    UC(60, 720, 0, 330, 2640, "Butcher", "X", 12)
    for sx in (-1, 1):
        UB(sx * 330 - 30, sx * 330 + 30, -360, 360, 1690, 1740, "Butcher", 8, 1)   # arms
        UT([(sx * 330, -340, 1500), (sx * 330, -340, 1700)], 22, "Butcher")
    UB(-420, 420, -470, -300, 600, 640, "Butcher", 8, 1)                      # footrest
    UTRAY(-440, 440, -700, -340, 1710, 1790, 40, "Enamel", "", 6)            # tray: inside floor at 1750
    piece("SM_K_HighChair", origin=(0, 0, 0), at=at("SM_K_HighChair"),
          note="seat top z=1500, tray floor z=1750 (rim 1790); the tray side is the front; four splayed legs, footrest at 600")

def car_track():
    P = V.PLACE["SM_K_CarTrack"]
    (xt, yc, zt), (xf, _, _), (xe, _, _) = P["top"], P["foot"], P["end"]
    w, th = P["width"] / 2.0, 24.0
    top = [(xt + 200, zt + th), (xt, zt + th), (xf, th), (xe, th)]
    GPXZ(top + [(xe, 0), (xf, 0), (xt, zt), (xt + 200, zt)], yc - w, yc + w, "Honey")
    lip = [(x, z + 55) for x, z in top] + [(x, z) for x, z in reversed(top)]
    for s in (-1, 1):
        y0 = yc + s * w
        GPXZ(lip, min(y0, y0 - s * 16), max(y0, y0 - s * 16), "Honey")
    GB(xt - 60, xt + 8, yc - w - 20, yc + w + 20, zt - 170, zt, "Blue", 0.003, 1)            # clamp under the table edge
    GB(xt - 60, xt + 200, yc - w - 20, yc + w + 20, zt - 170, zt - 130, "Blue", 0.003, 1)
    for k in (1, 2):                                                                          # two trestles
        x = xt + (xf - xt) * k / 3.0
        z = zt * (x - xf) / (xt - xf)
        for s in (-1, 1):
            GB(x - 22, x + 22, yc + s * (w + 10) - 18, yc + s * (w + 10) + 18, 0, z + 10, "PlasticBlue", 0.003, 1)
        GB(x - 22, x + 22, yc - w - 28, yc + w + 28, z - 30, z + 2, "PlasticBlue", 0.003, 1)
    piece("SM_K_CarTrack", note="toy car track clamped to the west edge of the dining table: slope %.1f deg, %d uu wide between the lips; the ramp heavy loot is dragged down" %
          (math.degrees(math.atan2(zt, xt - xf)), P["width"] - 32))

def broom():
    P = V.PLACE["SM_K_Broom"]
    hx, hy, _ = P["head"]
    tx, ty, tz = P["top"]
    GB(hx - 280, hx + 280, hy - 65, hy + 65, 0, 190, "Sisal", 0.012, 2)        # bristles
    GB(hx - 290, hx + 290, hy - 75, hy + 75, 190, 250, "Red", 0.006, 2)        # head block
    a = (hx, hy, 235.0)
    k = (tz + 34 - a[2]) / (ty + 4 - a[1])                                     # the handle lies on the worktop edge (y = top y + 4)
    b = (tx - 17, ty + 154, tz + 34 + k * 150)
    GT([a, (a[0] + (b[0] - a[0]) * 0.5, a[1] + (b[1] - a[1]) * 0.5, a[2] + (b[2] - a[2]) * 0.5), b], 26, "Butcher", 10)
    GS(34, b, "Red", nu=10, nv=6)
    # A floor rag hung over the handle to dry (integration, 2026-11-20). In the game the handle alone was no way for a load: a
    # 52 uu tube that starts 235 up on a block with vertical sides (an ant with food in its jaws stuck at the bristles). The rag
    # is the ramp: 320 wide, from the floor up over the head (45 deg, 250 high), along the top of the handle and over the
    # worktop's edge.
    t = 12.0
    ye, ze = ty + 4, tz + 34 + 26 + t
    y_head, z_head = hy - 80, 250.0
    y_foot = y_head - z_head
    GPYZ([(ye + 46, tz), (ye + 46, tz + t), (ye, ze), (hy, a[2] + 26 + t), (y_head, z_head + t), (y_foot, t), (y_foot - 90, t),
          (y_foot - 90, 0), (y_foot, 0), (y_head, z_head), (hy, a[2] + 26), (ye, ze - t)], hx - 295, hx + 25, "Fabric")
    piece("SM_K_Broom", note="leans on the west end of the worktop: slope %.1f deg; the rag hung over its handle (320 wide, down to the floor) is the way up and down for dragged loot along the west wall" % math.degrees(math.atan(k)))

def dustpan():
    UTRAY(-260, 260, -320, 140, 0, 130, 14, "Red", "y-", 4)
    UB(-260, 260, 126, 140, 130, 190, "Red", 4, 1)
    UT([(0, 130, 120), (0, 300, 200), (0, 330, 215)], 24, "Red", 8)
    piece("SM_K_Dustpan", origin=(0, 0, 0), at=at("SM_K_Dustpan"), note="open towards the front: ants walk in; 240 uu high at the handle")

def slipper():
    UR(400, 950, 40, 0, 0, 20, "Leather", 18, 2)                               # sole
    for sx in (-1, 1):
        UB(sx * 185 - 15, sx * 185 + 15, -440, 40, 40, 190, "SockWool", 12, 2)
    UB(-200, 200, -470, -438, 40, 190, "SockWool", 12, 2)                      # toe
    UB(-200, 200, -470, 40, 186, 226, "SockWool", 16, 2)                       # roof
    US(46, 0, -250, 222, "Yarn", (1, 1, 0.5), 10, 6)                           # pompom
    piece("SM_K_Slipper", origin=(0, 0, 0), at=at("SM_K_Slipper"), note="felt slipper, open at the heel: a cave 370 x 480 x 146 inside; 230 high, the Human steps over it")

def grocery_bag():
    t = 14
    UB(-550, 550, -600, 600, 0, t, "Cardboard", 4, 1)
    UB(-550, -550 + t, -600, 600, t, 620, "Cardboard", 4, 1)
    UB(550 - t, 550, -600, 600, t, 620, "Cardboard", 4, 1)
    UB(-550, 550, 600 - t, 600, t, 620, "Cardboard", 4, 1)                     # the bag's bottom (lying on its side)
    UR(1100, 1215, t, 0, 0, 590, "Cardboard", 4, 1, rot=(-3.5, 0, 0))         # sagging top
    UB(-550, 550, -600, -560, 620, 650, "Cardboard", 4, 1)                     # folded rim
    for sx in (-1, 1):
        UT([(sx * 200, -590, 640), (sx * 200, -700, 560), (sx * 330, -720, 420), (sx * 380, -640, 320)], 12, "Sisal", 6)
    UC(60, 1150, 300, -80, 74, "Toast", "Y", 10)                               # a baguette inside
    US(110, -250, 250, 124, "Leaf", (1, 1, 0.9))
    piece("SM_K_GroceryBag", origin=(0, 0, 0), at=at("SM_K_GroceryBag"), note="paper bag on its side, mouth at the front (1070 x 590 opening): a cave 1170 deep")

def apple():
    US(120, 0, 0, 112, "Red", (1, 1, 0.93), 14, 10)
    UT([(0, 0, 215), (8, 0, 262)], 7, "Walnut", 6)
    US(34, 30, 0, 252, "Leaf", (1.3, 0.6, 0.25), 8, 6)
    piece("SM_K_Apple", origin=(0, 0, 0), at=at("SM_K_Apple"))

def holed_wall(x0, x1, y0, y1, z0, z1, hx0, hx1, holes, m, along="X"):
    # a wall slab with door-like holes in one column: holes = [(z_from, z_to), ...] between hx0..hx1 along the wall
    def bx(a0, a1, b0, b1):
        if a1 - a0 > 1 and b1 - b0 > 1:
            if along == "X":
                UB(a0, a1, y0, y1, b0, b1, m, 3, 1)
            else:
                UB(x0, x1, a0, a1, b0, b1, m, 3, 1)
    lo, hi = (x0, x1) if along == "X" else (y0, y1)
    bx(lo, hx0, z0, z1)
    bx(hx1, hi, z0, z1)
    z = z0
    for (a, b) in holes:
        bx(hx0, hx1, z, a)
        z = b
    bx(hx0, hx1, z, z1)

def cardboard_box():
    W, D, H, t = 850.0, 575.0, 1100.0, 18.0
    UB(-W, W, -D, D, 0, t, "Cardboard", 3, 1)
    holed_wall(-W, W, -D, -D + t, t, H, -130, 130, [(t, 200)], "Cardboard")                   # front: gnawed hole at the floor
    holed_wall(-W, W, D - t, D, t, H, -380, -120, [(t, 190)], "Cardboard")                    # back: a second hole, towards the wall
    holed_wall(-W, -W + t, -D + t, D - t, t, H, -150, 150, [(700, 820)], "Cardboard", "Y")    # ends: hand holes
    holed_wall(W - t, W, -D + t, D - t, t, H, -150, 150, [(700, 820)], "Cardboard", "Y")
    UR(2 * W, 520, t, 0, -D - 54, H + 254, "Cardboard", 3, 1, rot=(102, 0, 0))                # open flaps: three stand up, one hangs down
    UR(2 * W, 520, t, 0, D + 54, H + 254, "Cardboard", 3, 1, rot=(78, 0, 0))
    UR(520, 2 * D - 40, t, W + 54, 0, H + 254, "Cardboard", 3, 1, rot=(0, -78, 0))
    UR(520, 2 * D - 40, t, -W - 56, 0, H - 256, "Cardboard", 3, 1, rot=(0, 100, 0))
    US(190, 380, 150, 190, "Paper", (1.1, 0.9, 0.85), 9, 6)                                   # crumpled packing paper
    US(150, -420, -180, 150, "Paper", (0.9, 1.2, 0.8), 9, 6)
    US(120, 60, 300, 120, "Paper", (1.2, 0.8, 0.9), 9, 6)
    piece("SM_K_CardboardBox", origin=(0, 0, 0), at=at("SM_K_CardboardBox"),
          note="delivery box, flaps open: inside 1664 x 1114 x 1080; holes: front 260 x 182 at the floor, back 260 x 172 at the floor, hand holes 300 x 120 at z 700 on both ends")

def bottle():
    UL([(120, 12), (120, 650), (60, 850), (45, 1040), (52, 1050), (52, 1100), (34, 1100), (34, 850), (108, 650), (108, 12)], "ShelfGlass", 0, 550, 120, 14, "Y", closed=True)
    UC(120, 12, 0, 544, 120, "ShelfGlass", "Y", 14)
    UC(122, 300, 0, 300, 120, "Paper", "Y", 14)
    piece("SM_K_Bottle", origin="bottom", at=at("SM_K_Bottle"), note="empty bottle on its side, neck towards the front: a tunnel 68 uu wide at the neck, 216 inside")

def tin_can():
    UL([(120, 10), (120, 400), (112, 400), (112, 10)], "Steel", 0, 200, 120, 16, "Y", closed=True)
    UC(120, 10, 0, 195, 120, "Steel", "Y", 16)
    UL([(123, 60), (123, 340), (120, 340), (120, 60)], "Red", 0, 200, 120, 16, "Y", closed=True)
    UC(108, 6, 0, -215, 205, "Steel", "Y", 16, rot=(20, 0, 0))                                 # the lid, bent up
    piece("SM_K_TinCan", origin="bottom", at=at("SM_K_TinCan"), note="open tin on its side, lid bent up at the front")

def bread_box():
    UB(-600, 600, -350, 350, 0, 30, "Butcher", 6, 1)
    UB(-600, 600, 320, 350, 30, 600, "Butcher", 6, 1)
    for sx in (-1, 1):
        UP([(-350, 0), (350, 0), (350, 600), (0, 600), (-350, 230)], min(sx * 570, sx * 600), max(sx * 570, sx * 600), "Butcher", "X", 5)
    UB(-570, 570, 0, 320, 570, 600, "Butcher", 6, 1)
    UB(-570, 570, -350, -320, 30, 230, "Butcher", 6, 1)
    piece("SM_K_BreadBox", origin=(0, 0, 0), at=at("SM_K_BreadBox"), note="floor top z=30, top z=600; the slanted front is SM_K_BreadBox_Lid")
    UP([(0, 596), (-346, 230), (-332, 216), (14, 582)], -566, 566, "Butcher", "X", 3)
    UC(26, 60, 0, -205, 415, "Brass", "Y", 10, rot=(46.6, 0, 0))
    piece("SM_K_BreadBox_Lid", origin=(0, 0, 598 * K), parent="SM_K_BreadBox",
          motion={"type": "hinge", "axis": (1, 0, 0), "angle_deg": -95, "note": "hinge along the top edge; swings up and back over the roof"}, state="closed")

def bun():
    US(190, 0, 0, 108, "Toast", (1, 1, 0.58), 12, 8)
    piece("SM_K_Bun", origin=(0, 0, 0), at=at("SM_K_Bun"))

def knife_block():
    UP([(-300, 0), (300, 0), (300, 500), (-100, 900), (-300, 760)], -210, 210, "Walnut", "X", 8)
    for x in (-120, 0, 120):
        UR(60, 46, 300, x, -150, 960, "Plinth", 8, 1, rot=(35, 0, 0))
        UC(10, 50, x, -150, 960, "Brass", "X", 8)
    piece("SM_K_KnifeBlock", origin=(0, 0, 0), at=at("SM_K_KnifeBlock"))

def dish_rack():
    UTRAY(-600, 600, -450, 450, 0, 46, 16, "Enamel", "", 4)
    for sy in (-1, 1):
        UT([(-570, sy * 400, 30), (-570, sy * 400, 300), (570, sy * 400, 300), (570, sy * 400, 30)], 12, "Chrome", 6)
    for sx in (-1, 1):
        UT([(sx * 570, -400, 300), (sx * 570, 400, 300)], 12, "Chrome", 6)
    for i in range(9):
        x = -480 + i * 120
        for y in (-230, 230):
            UT([(x, y, 30), (x, y, 330)], 9, "Chrome", 6)
        UT([(x, -230, 60), (x, 230, 60)], 9, "Chrome", 6)
    piece("SM_K_DishRack", origin=(0, 0, 0), at=at("SM_K_DishRack"), note="drip tray + wire rack; three SM_K_Plate stand in it (placed by the layout)")

def fruit_bowl():
    UL([(160, 30), (450, 300), (450, 330), (425, 330), (160, 62)], "MugBlue", 0, 0, 0, 20, closed=True)
    UC(170, 34, 0, 0, 17, "MugBlue", n=20)
    UC(160, 32, 0, 0, 46, "MugBlue", n=20)
    US(120, -150, 60, 250, "Red", (1, 1, 0.93))
    US(125, 130, -120, 250, "Honey")
    US(115, 150, 170, 255, "LeafLight", (0.9, 0.9, 1.15))
    US(110, -60, -180, 240, "Red", (1, 1, 0.93))
    UT([(-250, 40, 330), (-120, 150, 395), (60, 180, 405), (220, 80, 350)], [34, 48, 48, 30], "Cheese", 8)
    piece("SM_K_FruitBowl", origin=(0, 0, 0), at=at("SM_K_FruitBowl"))

def fly_paper():
    P = V.PLACE["SM_K_FlyPaper"]
    x, y, zt = P["top"]
    zb = P["bottom"][2]
    GC(46, 170, (x, y, zt - 85), "Cardboard", n=10)
    n = 22
    h = (zt - 170 - zb) / n
    for i in range(n):
        g = L.G(x, y, zt - 170 - (i + 0.5) * h)
        L.rbox(62 * K, 5 * K, h * 1.04 * K, g[0], g[1], g[2], "Honey", 0.0, 1, rot=(0, 0, i * 26))
    GS(16, (x, y, zb), "Plinth", nu=6, nv=4)                                                   # the weight at the end
    piece("SM_K_FlyPaper", note="sticky fly ribbon from the lamp over the dining table down to 20 uu above the table top; a slow way up to the lamp")

def paper_towel():
    UC(130, 500, 0, 0, 1100, "Paper", "X", 16)
    UC(22, 560, 0, 0, 1100, "Chrome", "X", 8)
    for sx in (-1, 1):
        UB(sx * 268 - 12, sx * 268 + 12, -30, 150, 1060, 1250, "Chrome", 4, 1)
    UB(-280, 280, 120, 150, 1210, 1250, "Chrome", 4, 1)
    UB(-240, 240, -134, -128, 40, 1100, "Paper", 0, 1)                          # the hanging tail
    UR(480, 6, 90, 0, -122, 16, "Paper", 0, 1, rot=(-28, 0, 0))
    piece("SM_K_PaperTowel", origin=(0, 0, 0), at=at("SM_K_PaperTowel"), note="roll under the wall cabinet (holder top at 1250 = cabinet bottom), tail hanging to the worktop")

def chili_string():
    P = V.PLACE["SM_K_ChiliString"]
    pts = catenary(P["a"], P["b"], P["sag"], 28)
    GT(pts, 9, "Sisal", 6)
    for i in range(2, 27, 2):
        x, y, z = pts[i]
        g = L.G(x + 34 * (1 if i % 4 else -1), y, z - 150)
        L.lathe([(0, 0), (40 * K, 60 * K), (46 * K, 150 * K), (30 * K, 250 * K), (0, 262 * K)], "Red", g[0], g[1], g[2], 8, rot=(6 * (i % 5 - 2), 9, 0))
        GC(16, 46, (x + 34 * (1 if i % 4 else -1), y, z + 110), "Leaf", n=6)
    GS(22, P["a"], "Brass", nu=6, nv=4)
    GS(22, P["b"], "Brass", nu=6, nv=4)
    piece("SM_K_ChiliString", note="string of dried chilies along the west wall from the wall shelf to the side of the first wall cabinet")

def fridge_bits():
    UC(80, 36, 0, -18, 0, "Honey", "Y", 14)
    UC(46, 12, 0, -40, 0, "Enamel", "Y", 12)
    piece("SM_K_FridgeMagnet", origin=(0, 0, 0), at=at("SM_K_FridgeMagnet"), note="origin on the surface it sticks to; front = -Y")
    UR(400, 8, 500, 0, -32, 250, "Paper", 0, 1, rot=(-6, 0, 0))
    UC(50, 30, 0, -22, 470, "Red", "Y", 12)
    piece("SM_K_FridgeNote", origin=(0, 0, 0), at=at("SM_K_FridgeNote"), note="paper note under a magnet, its lower edge stands 55 uu off the surface: room to hide behind")

def calendar():
    UB(-350, 350, -14, 0, 0, 1000, "Paper", 3, 1)
    UB(-350, 350, -20, -14, 760, 1000, "Red", 2, 1)
    UB(-300, 300, -18, -14, 60, 700, "Enamel", 2, 1)
    UC(16, 640, 0, -20, 1000, "Chrome", "X", 8)
    UC(10, 30, 0, -10, 1040, "Plinth", "Y", 6)
    piece("SM_K_Calendar", origin=(0, 0, 0), at=at("SM_K_Calendar"))

def sandwich():
    UR(560, 560, 70, 0, 0, 35, "Toast", 14, 2)
    UR(600, 540, 16, 10, -6, 78, "Leaf", 4, 1, rot=(0, 0, 9))
    UR(580, 580, 24, 0, 0, 98, "CakePink", 6, 1, rot=(0, 0, -5))
    UR(600, 600, 20, 0, 0, 120, "Cheese", 4, 1, rot=(0, 0, 14))
    UR(560, 560, 70, 0, 0, 166, "Toast", 14, 2, rot=(0, 0, 4))
    piece("SM_K_Sandwich", origin=(0, 0, 0), at=at("SM_K_Sandwich"))

def spills():
    UP(blob(0, 0, 380, 880, 18, 0.22, 1.3), 0, 6, "Sugar")
    for i in range(16):
        a = i * 2.39996
        r = 0.25 + 0.7 * ((i * 7) % 10) / 10.0
        UR(34, 34, 30, 300 * r * math.cos(a), 780 * r * math.sin(a), 18, "Sugar", 3, 1, rot=(0, 0, i * 23))
    UR(120, 120, 120, 150, -520, 60, "Sugar", 8, 1, rot=(0, 0, 20))
    UR(120, 120, 120, -210, 340, 60, "Sugar", 8, 1, rot=(0, 0, -15))
    piece("SM_K_SugarSpill", origin=(0, 0, 0), at=at("SM_K_SugarSpill"), note="spilled sugar: 6 uu thick, two cubes of 120")
    UP(blob(0, 0, 340, 290, 16, 0.2, 0.4), 0, 8, "Jam")
    piece("SM_K_JamPuddle", origin=(0, 0, 0), at=at("SM_K_JamPuddle"), note="sticky: the game needs a sticky zone here (level_v2.json hazards)")
    UP(blob(0, 0, 250, 195, 14, 0.18, 2.1), 0, 5, "Ice")
    piece("SM_K_Puddle", origin=(0, 0, 0), at=at("SM_K_Puddle"))

def pantry():
    UR(600, 400, 700, 0, 0, 350, "Paper", 60, 3)
    UR(520, 330, 90, 0, 0, 745, "Paper", 30, 2)
    UC(150, 40, 0, 0, 700, "Sisal", n=10)
    UB(-240, 240, -206, -196, 250, 470, "Blue", 3, 1)
    piece("SM_K_FlourSack", origin=(0, 0, 0), at=at("SM_K_FlourSack"))
    UR(300, 200, 750, 0, 0, 375, "Blue", 8, 2)
    UB(-90, 90, -104, -98, 300, 560, "Cheese", 2, 1)
    piece("SM_K_PastaBox", origin=(0, 0, 0), at=at("SM_K_PastaBox"))
    UL([(350, 20), (350, 300), (338, 300), (338, 20)], "Red", 0, 0, 0, 20, closed=True)
    UC(350, 20, 0, 0, 10, "Red", n=20)
    for i, (x, y) in enumerate(((0, 0), (190, 60), (-170, 110), (-60, -190), (150, -170))):
        UC(105, 38, x, y, 39 + (i % 2) * 38, "Biscuit", n=12)
    UC(356, 44, 420, 0, 250, "Red", "X", 20, rot=(0, 72, 0))                                   # the lid, leaning on the side
    piece("SM_K_CookieTin", origin=(0, 0, 0), at=at("SM_K_CookieTin"), note="open biscuit tin, lid leaning against it")

def build():
    for f in (high_chair, car_track, broom, dustpan, slipper, grocery_bag, apple, cardboard_box, bottle, tin_can, bread_box, bun, knife_block,
              dish_rack, fruit_bowl, fly_paper, paper_towel, chili_string, fridge_bits, calendar, sandwich, spills, pantry):
        f()