# Ant Kitchen level v2, part 2: the new living-room pieces. Same conventions as level_v2_p1.py.
import math
from level_v2_common import *

def shoe_rack():
    for sx in (-1, 1):
        UB(sx * 835 - 15, sx * 835 + 15, -300, 300, 0, 1100, "Butcher", 6, 1)
    UB(-850, 850, -300, 300, 1070, 1100, "Butcher", 6, 1)
    for ztop in (120, 420, 780):
        for k in range(5):
            y = -250 + k * 125
            UB(-820, 820, y - 40, y + 40, ztop - 24, ztop, "Butcher", 4, 1)
    UB(-820, 820, 270, 290, 60, 1070, "Walnut", 3, 1)                          # thin back rail
    piece("SM_L_ShoeRack", origin=(0, 0, 0), at=at("SM_L_ShoeRack"), note="open rack: slatted shelves with tops at 120 / 420 / 780, top board at 1100; ants pass between the slats (45 uu gaps)")

def toys():
    UR(240, 240, 240, 0, 0, 120, "Butcher", 12, 2)
    for (dx, dy, m) in ((0, -1, "Red"), (0, 1, "Blue"), (-1, 0, "Leaf"), (1, 0, "Honey")):
        UR(150 if dx == 0 else 6, 6 if dx == 0 else 150, 150, dx * 119, dy * 119, 120, m, 0, 1)
    piece("SM_L_ToyBlock", origin=(0, 0, 0), at=at("SM_L_ToyBlock"), note="240 cube: lower than the Human's step, stacks of them are stairs for dropped loot")
    arc = [(150 * math.cos(math.radians(a)), 150 * math.sin(math.radians(a))) for a in range(180, -1, -20)]
    UP([(-300, 0)] + arc + [(300, 0), (300, 240), (-300, 240)], -120, 120, "Blue", "Y", 6)
    piece("SM_L_ToyArch", origin=(0, 0, 0), at=at("SM_L_ToyArch"))
    UB(-150, 150, -340, 340, 70, 110, "Plinth", 6, 1)
    UB(-160, 160, -340, -110, 110, 240, "Red", 14, 2)
    UB(-130, 130, -344, -338, 150, 215, "ShelfGlass", 0, 1)
    UTRAY(-160, 160, -95, 345, 110, 200, 16, "Honey", "", 4)
    for sx in (-1, 1):
        for y in (-230, 220):
            UC(70, 52, sx * 172, y, 70, "Plinth", "X", 12)
            UC(30, 58, sx * 172, y, 70, "Enamel", "X", 8)
    piece("SM_L_ToyCar", origin=(0, 0, 0), at=at("SM_L_ToyCar"), note="toy truck, 240 high, nose at the front; 70 uu of clearance under the chassis")
    UB(-100, 100, -250, 250, 60, 110, "Plinth", 6, 1)
    UB(-100, 100, -250, 60, 110, 180, "Red", 10, 2)
    UC(72, 300, 0, -95, 190, "Blue", "Y", 12)
    UB(-105, 105, 60, 250, 110, 262, "Leaf", 10, 2)
    UC(30, 80, 0, -200, 270, "Plinth", n=8)
    UC(40, 20, 0, -200, 318, "Plinth", n=8)
    for sx in (-1, 1):
        for y in (-170, 0, 170):
            UC(60, 30, sx * 112, y, 60, "Honey", "X", 10)
    piece("SM_L_ToyTrain", origin=(0, 0, 0), at=at("SM_L_ToyTrain"), note="wooden engine, stands on the track (rail top 40)")
    US(70, 0, 0, 60, "Dust", (1, 2.1, 0.85), 10, 8)
    for sx in (-1, 1):
        UC(34, 8, sx * 44, -95, 112, "CakePink", "Y", 8, rot=(70, 0, 0))
    US(12, 0, -148, 52, "Plinth", nu=6, nv=4)
    UT([(0, 140, 40), (40, 230, 20), (-20, 300, 14), (30, 360, 10)], 8, "Yarn", 6)
    piece("SM_L_ToyMouse", origin=(0, 0, 0), at=at("SM_L_ToyMouse"))

def train_track():
    P = V.PLACE["SM_L_TrainTrack"]
    cx, cy, _ = P["c"]
    hl, r = P["half"], P["r"]
    def band(r0, r1, z0, z1, m):
        for s in (-1, 1):                                                       # the two straights
            ya, yb = cy + s * r0, cy + s * r1
            GB(cx - hl, cx + hl, min(ya, yb), max(ya, yb), z0, z1, m, 0.0, 1)
        n = 14
        for e, ex in ((-1, cx - hl), (1, cx + hl)):                             # the two half circles
            for i in range(n):
                a0 = math.radians(-90 + 180.0 * i / n)
                a1 = math.radians(-90 + 180.0 * (i + 1) / n + 0.4)
                q = [(ex + e * rr * math.cos(a), cy + rr * math.sin(a)) for rr, a in ((r0, a0), (r1, a0), (r1, a1), (r0, a1))]
                GPXY(q, z0, z1, m)
    band(r - 90, r + 90, 0, 24, "Butcher")
    band(r - 62, r - 42, 24, 40, "Walnut")
    band(r + 42, r + 62, 24, 40, "Walnut")
    piece("SM_L_TrainTrack", note="wooden train set, an oval 3600 x 1980 on the carpet: 180 wide, 40 high; a clear lane round the toys")

def book_ramp():
    P = V.PLACE["SM_L_BookRamp"]
    (xa, yc, _), (xb, _, zb) = P["foot"], P["top"]
    w = P["width"] / 2.0
    ex, ez = P.get('edge', (12500.0, 980.0))                                    # the table edge the book lies on
    ln = math.hypot(ex - xa, ez)
    dx, dz = (ex - xa) / ln, ez / ln
    nx, nz = (-dz, dx) if dx > 0 else (dz, -dx)                                 # the side of the slab that faces up
    b = (xa + dx * (ln + 140), dz * (ln + 140))
    def slab(t0, t1, y0, y1, m):
        GPXZ([(xa + nx * t0, nz * t0), (b[0] + nx * t0, b[1] + nz * t0), (b[0] + nx * t1, b[1] + nz * t1), (xa + nx * t1, nz * t1)], y0, y1, m)
    slab(0, 12, yc - w, yc + w, "Blue")
    slab(12, 30, yc - w + 14, yc + w - 14, "Paper")
    slab(30, 42, yc - w, yc + w, "Blue")
    slab(42, 46, yc - w + 110, yc + w - 110, "Honey")
    g = L.G(xa + dx * ln * 0.55 + nx * 46, yc, dz * ln * 0.55 + nz * 46)
    L.cyl(170 * K, 6 * K, g[0], g[1], g[2], "Red", "Z", 16, 0.0, None, 1, (0, math.degrees(math.atan2(dz, dx)) if dx > 0 else math.degrees(math.atan2(dz, dx)) - 180.0, 0))
    piece("SM_L_BookRamp", note="board book leaning on the west edge of the coffee table: slope %.1f deg, %d wide; the way down for the main object" % (math.degrees(math.atan2(dz, abs(dx))), P["width"]))

def blanket():
    P = V.PLACE["SM_L_Blanket"]
    x0, x1, sy, sz, by, fy = P["x0"], P["x1"], P["seat_y"], P["seat_z"], P["back_y"], P["foot_y"]
    t = 26.0
    top = (sy - 40, sz + t)                                                     # over the seat's front edge
    ln = math.hypot(top[0] - fy, top[1] - t)
    GPYZ([(by, sz), (by, sz + t), top, (fy, t), (fy - 120, t), (fy - 120, 0), (fy, 0), (sy - 40 + t * (top[1] - t) / ln, sz)], x0, x1, "Yarn")
    for k in range(8):
        x = x0 + (k + 0.5) * (x1 - x0) / 8.0
        GB(x - 18, x + 18, fy - 170, fy - 120, 0, 10, "Sisal", 0.0, 1)         # fringe
    piece("SM_L_Blanket", note="throw hanging from the sofa seat to the floor: slope %.1f deg; a tent underneath" % math.degrees(math.atan2(top[1] - t, top[0] - fy)))

def strings():
    P = V.PLACE["SM_L_YarnBridge"]
    GT(catenary(P["a"], P["b"], P["sag"], 16), 9, "Yarn", 6)
    GS(20, P["a"], "Yarn", nu=6, nv=4)
    GS(20, P["b"], "Yarn", nu=6, nv=4)
    piece("SM_L_YarnBridge", note="a strand of yarn from the rim of the knitting basket to the coffee table top; ants only: the Human must walk through it")
    P = V.PLACE["SM_L_Garland"]
    pts = catenary(P["a"], P["b"], P["sag"], 54)
    GT(pts, 8, "Sisal", 6)
    cols = ("Red", "Honey", "Blue", "Leaf", "CakePink")
    for i in range(2, 53, 2):
        a, b = Vector(L.G(*pts[i - 1])), Vector(L.G(*pts[i + 1]))
        c = Vector(L.G(*pts[i]))
        h = (b - a)
        h.z = 0
        h.normalize()
        n = Vector((-h.y, h.x, 0)) * (3 * K)
        v0, v1, v2 = c - h * (110 * K), c + h * (110 * K), c - Vector((0, 0, 290 * K))
        solid([v0 - n, v1 - n, v2 - n, v0 + n, v1 + n, v2 + n], [(0, 1, 2), (5, 4, 3), (0, 3, 4, 1), (1, 4, 5, 2), (2, 5, 3, 0)], cols[(i // 2) % 5])
    GS(24, P["a"], "Brass", nu=6, nv=4)
    GS(24, P["b"], "Brass", nu=6, nv=4)
    piece("SM_L_Garland", note="birthday bunting from the wall above the door to the top of the bookshelf, lowest point about 3780 (the Human is 3400); ants only")

def side_table():
    UC(400, 40, 0, 0, 980, "Walnut", n=24, bev=8)
    UC(330, 30, 0, 0, 315, "Walnut", n=24, bev=6)
    for a in (90, 210, 330):
        c, s = math.cos(math.radians(a)), math.sin(math.radians(a))
        UT([(c * 350, s * 350, 0), (c * 310, s * 310, 960)], 24, "Walnut", 8)
    piece("SM_L_SideTable", origin=(0, 0, 0), at=at("SM_L_SideTable"), note="top z=1000, lower shelf z=330")

def cat_flap():
    P = V.PLACE["SM_L_CatFlap"]
    x0, x1, y0, y1, z0, z1 = P["x0"] - 10, P["x1"] + 10, P["y0"], P["y1"], P["z0"], P["z1"]
    GB(x0, x1, y0 - 22, y1 + 22, z0 - 22, z0, "Enamel", 0.003, 1)
    GB(x0, x1, y0 - 22, y1 + 22, z1, z1 + 22, "Enamel", 0.003, 1)
    GB(x0, x1, y0 - 22, y0, z0, z1, "Enamel", 0.003, 1)
    GB(x0, x1, y1, y1 + 22, z0, z1, "Enamel", 0.003, 1)
    piece("SM_L_CatFlap", note="liner of the opening x %d..%d, y %d..%d, z %d..%d in the living room's west wall (the wall needs this hole in the code)" % (P["x0"], P["x1"], y0, y1, z0, z1))
    xm = (P["x0"] + P["x1"]) / 2.0
    GB(xm - 9, xm + 9, y0 + 6, y1 - 6, z0 + 8, z1 - 4, "ShelfGlass", 0.002, 1)
    GB(xm - 16, xm + 16, y0 + 150, y1 - 150, z0 + 8, z0 + 40, "Enamel", 0.003, 1)
    piece("SM_L_CatFlap_Leaf", origin=L.G(xm, (y0 + y1) / 2.0, z1 - 4), parent="SM_L_CatFlap",
          motion={"type": "hinge", "axis": (0, 1, 0), "angle_deg": -80, "note": "hinged along its top edge; swings out towards the yard (game -X)"}, state="closed, latched from inside")

def power_strip():
    P = V.PLACE["SM_L_PowerStrip"]
    x, y, _ = P["c"]
    GB(x - 450, x + 450, y - 90, y + 90, 0, 90, "Enamel", 0.008, 2)
    for i in range(5):
        GC(52, 8, (x - 300 + i * 130, y, 92), "Plinth", n=12)
    GB(x + 340, x + 410, y - 30, y + 30, 90, 104, "Red", 0.002, 1)
    for i in (0, 2):
        GB(x - 345 + i * 130, x - 255 + i * 130, y - 45, y + 45, 92, 170, "Plinth", 0.006, 1)
    tx = P["tv_x"]
    GT([(x - 300, y, 160), (x - 300, y + 60, 200), (x + 200, y + 150, 30), (tx - 40, y + 60, 22), (tx - 16, y, 60), (tx - 16, y, 1085), (tx - 4, y, 1118), (tx + 90, y, 1112)], 14, "Plinth", 8)
    GT([(x - 40, y, 160), (x - 40, y + 80, 190), (x - 20, y + 500, 16), (x, y + 900, 14)], 14, "Plinth", 8)
    piece("SM_L_PowerStrip", note="extension lead by the south wall: one cable climbs the west end of the TV stand to its top (a way up), one runs to the floor lamp")

def build():
    for f in (shoe_rack, toys, train_track, book_ramp, blanket, strings, side_table, cat_flap, power_strip):
        f()