# Ant Kitchen: the street pieces (see living_pieces.py). Dimensions from AKitchenBuilder::BuildWestStreet
# (KitchenHouse.cpp): Unreal units / 1900 = metres.
from living_common import *

ZN = "street"
WS = "AKitchenBuilder::BuildWestStreet (KitchenHouse.cpp)"
LAMPGLASS = mat("LampGlass", (1.0, 0.90, 0.62), 0.3, 0.0, 3.0)

def rep(what, dims):
    return {"function": WS, "boxes": what, "matched_dims_m": [round(d, 4) for d in dims]}

def build_house():
    L, T, Hh = 11000 / S, 60 / S, 3600 / S
    begin()
    bx(-L / 2, L / 2, -0.001, T / 2, 0, Hh, SIDING, 0, 0)
    n = 12
    bh = Hh / n
    for i in range(n):
        prism([(0.0, i * bh), (-T / 2, i * bh), (-0.004, (i + 1) * bh), (0.0, (i + 1) * bh)], -L / 2, L / 2, SIDING, plane="YZ")
    for s in (-1, 1):
        bx(s * L / 2, s * (L / 2 - 0.07), -0.024, T / 2, 0, Hh, ENAMEL, 0.004, 1)
    bx(-L / 2, L / 2, -0.022, T / 2, Hh - 0.10, Hh, ENAMEL, 0.004, 1)
    cyl(0.16, 0.03, 0, -0.012, Hh * 0.5, ENAMEL, "Y", 16, bev=0.004, seg=1)
    for k in range(4):
        rbox(0.22 - abs(k - 1.5) * 0.05, 0.006, 0.014, 0, -0.029, Hh * 0.5 - 0.075 + k * 0.05, PLINTH, 0, 0)
    end("SM_S_Facade", ZN, origin=(0, 0, 0), replaces=rep("upper facade box 60 x 11000 x 3600 uu (z 5400..9000)", (L, T, Hh)),
        notes="Clapboard strip above the kitchen wall; origin at the bottom, on the centre plane of the code's slab. Boards 16 mm proud at their lower edge, "
              "white corner boards and a frieze 24 mm proud. Flat back.")
    begin()
    rbox(100 / S, 60 / S, 5400 / S, 0, 0, 2700 / S, ENAMEL, 0.005, 1)
    rbox(100 / S + 0.012, 60 / S + 0.008, 0.16, 0, 0, 0.08, ENAMEL, 0.005, 1)
    end("SM_S_FacadeCorner", ZN, origin=(0, 0, 0), replaces=rep("corner boxes 60 x 100 x 5400 uu at both ends of the west wall", (100 / S, 60 / S, 5400 / S)))
    # roof gutter: a real channel, open to the sky, with the outlet into the downspout
    L, Wg, hb, Hg = 11200 / S, 300 / S, 60 / S, 250 / S
    begin()
    g = bx(-L / 2, L / 2, -Wg / 2, Wg / 2, 0, hb, STEEL, 0.003, 1)
    cut(g, cyl(0.039, 0.2, (-5250 - 200) / S, -30 / S, hb / 2, PLINTH, verts=12))
    bx(-L / 2, L / 2, -Wg / 2, -Wg / 2 + 0.012, hb - 0.002, Hg, STEEL, 0.003, 1)
    cyl(0.011, L, 0, -Wg / 2 + 0.008, Hg - 0.011, STEEL, "X", 8)
    bx(-L / 2, L / 2, Wg / 2 - 0.008, Wg / 2, hb - 0.002, Hg, STEEL, 0.002, 1)
    for s in (-1, 1):
        bx(s * L / 2, s * (L / 2 - 0.012), -Wg / 2, Wg / 2, 0, Hg, STEEL, 0.003, 1)
    for k in range(6):
        x = -L / 2 + L * (k + 0.5) / 6
        bx(x - 0.008, x + 0.008, -Wg / 2 + 0.004, Wg / 2, Hg - 0.012, Hg - 0.002, IRON, 0, 0)
    end("SM_S_Gutter", ZN, origin=(0, 0, 0), replaces=rep("gutter bottom 300 x 11200 x 60 uu + lip 60 x 190 uu (z 8900..9150)", (L, Wg, Hg)),
        notes="A channel 0.126 m wide, 0.10 m deep, closed at both ends, six hanger straps across the top. The bottom has a 78 mm hole over the downspout "
              "(at x=-2.868 m): ants can go from the gutter down the inside of the pipe.")
    begin()
    n = 19
    tw = L / n
    for i in range(n):
        rbox(tw - 0.006, 150 / S, 0.022, -L / 2 + (i + 0.5) * tw, 0, 0.011 + (0.005 if i % 2 else 0.0), TERRA, 0.008, 1)
    end("SM_S_RoofEdge", ZN, origin=(0, 0, 0), replaces=rep("roof tiles edge 300 x 11200 x 50 uu (z 9150..9200)", (L, 150 / S, 50 / S)),
        notes="Only HALF as deep as the code box (150 uu, the wall side): the code box lies on the gutter lip and roofs the gutter over completely. "
              "This one leaves the front half of the gutter open.")
    begin()
    r, w, Hh = 90 / S, 0.010, 8900 / S
    pts = smooth([(0, -0.13, 0.05), (0, -0.075, 0.06), (0, -0.03, 0.09), (0, -0.006, 0.15), (0, 0, 0.24)], 2) + [Vector((0, 0, 1.0)), Vector((0, 0, Hh))]
    tube(pts, r, STEEL, segs=12, wall=w, cap0=False, cap1=False)
    lathe([(r - w, Hh - 0.06), (r + 0.002, Hh - 0.06), (120 / S, Hh - 0.02), (120 / S, Hh), (120 / S - w, Hh), (r - w, Hh - 0.035)], STEEL, 12, closed=True)
    for z in (3600 / S, 7200 / S):
        lathe([(r + 0.001, z - 0.016), (r + 0.012, z - 0.016), (r + 0.012, z + 0.016), (r + 0.001, z + 0.016)], IRON, 12, closed=True)
        bx(-0.03, 0.03, r, 120 / S, z - 0.012, z + 0.012, IRON, 0.002, 1)
    end("SM_S_Drainpipe", ZN, origin=(0, 0, 0), replaces=rep("downspout cylinder r90 h8900 + 2 brackets + top collar r120", (2 * r, 2 * r, Hh)),
        notes="Hollow (10 mm wall, 75 mm bore), open at both ends: the shoe at the bottom kicks out 0.13 m towards the street (model -Y), the top opens "
              "into the gutter. Origin on the ground on the pipe axis. Brackets reach the wall 120 uu behind the axis (+Y).")

def build_ground():
    W, D, T = 1600 / S, 1184 / S, 200 / S
    begin()
    bx(-W / 2, W / 2, -D / 2, D / 2, 0, T, CONCRETE, 0.006, 2)
    end("SM_S_PavementSlab_A", ZN, replaces=rep("sidewalk slab 1600 x 1184 x 200 uu (top at z=0)", (W, D, T)), notes="Origin at the bottom: place it at z=-200 uu.")
    begin()
    g = 0.004
    prism([(-W / 2, -D / 2), (0.08 - g, -D / 2), (-0.12 - g, D / 2), (-W / 2, D / 2)], 0, T, CONCRETE, bev=0.005, seg=1)
    prism([(0.08 + g, -D / 2), (W / 2, -D / 2), (W / 2, D / 2), (-0.12 + g, D / 2)], 0, T, CONCRETE, bev=0.005, seg=1)
    end("SM_S_PavementSlab_B", ZN, replaces=rep("sidewalk slab 1600 x 1184 x 200 uu (top at z=0)", (W, D, T)),
        notes="Cracked variant: two halves with an 8 mm crevice right through (ants can drop into it).")
    begin()
    bx(-600 / S, 600 / S, -50 / S, 50 / S, 0, 230 / S, KERB, 0.012, 2)
    end("SM_S_Kerb", ZN, replaces=rep("curb 100 x 10800 x 230 uu (top at z=0)", (1200 / S, 100 / S, 230 / S)), notes="1200 uu module, one per pavement slab. Place at z=-230 uu.")
    begin()
    Wr, Lr, Tr = 5400 / S, 1600 / S, 30 / S
    bx(-Wr / 2, Wr / 2, -Lr / 2, Lr / 2, 0, Tr, ASPHALT, 0.003, 1)
    rbox(200 / S, 800 / S, 0.004, 0, 0, Tr + 0.0005, ROADPAINT, 0.001, 1)
    end("SM_S_Road", ZN, replaces=rep("road slab 5400 x 10800 x 30 uu + lane dashes 200 x 800 uu every 1600", (Wr, Lr, Tr)),
        notes="One 1600 uu module with one lane dash in the middle (2.5 mm proud). X is across the road. Place at z=-260 uu.")
    begin()
    R = 560 / S
    lathe([(R - 0.03, 0), (R, 0), (R, 0.012), (R - 0.03, 0.012)], IRON, 24, closed=True)
    cyl(R - 0.033, 0.010, 0, 0, 0.005, IRON, verts=24)
    lathe([(R * 0.55, 0.009), (R * 0.62, 0.009), (R * 0.62, 0.013), (R * 0.55, 0.013)], IRON, 24, closed=True)
    for i in range(12):
        a = i * math.pi / 6
        rbox(R * 0.24, 0.016, 0.004, R * 0.76 * math.cos(a), R * 0.76 * math.sin(a), 0.011, IRON, 0, 0, rot=(0, 0, a))
    cyl(R * 0.2, 0.004, 0, 0, 0.011, IRON, verts=6)
    for i in range(8):
        a = i * math.pi / 4 + 0.39
        cyl(0.012, 0.004, R * 0.38 * math.cos(a), R * 0.38 * math.sin(a), 0.011, IRON, verts=6)
    end("SM_S_Manhole", ZN, replaces=rep("manhole cover cylinder r560 h10", (2 * R, 2 * R, 10 / S)), notes="13 mm thick with a frame ring and a raised pattern (code 5 mm).")
    begin()
    Lg, Wg, d, fr = 800 / S, 200 / S, 0.11, 0.012
    zt = d + 0.012
    bx(-Lg / 2, Lg / 2, -Wg / 2, Wg / 2, 0, 0.012, KERB, 0.002, 1)
    for s in (-1, 1):
        bx(-Lg / 2, Lg / 2, s * Wg / 2, s * (Wg / 2 - 0.010), 0.010, d, KERB, 0.002, 1)
        bx(s * Lg / 2, s * (Lg / 2 - 0.010), -Wg / 2, Wg / 2, 0.010, d, KERB, 0.002, 1)
        bx(-Lg / 2, Lg / 2, s * Wg / 2, s * (Wg / 2 - fr), d - 0.002, zt, IRON, 0.002, 1)
        bx(s * Lg / 2, s * (Lg / 2 - fr), -Wg / 2, Wg / 2, d - 0.002, zt, IRON, 0.002, 1)
    for i in range(6):
        x = -Lg / 2 + (40 + 130 * i) / S
        bx(x - 20 / S, x + 20 / S, -Wg / 2 + 0.004, Wg / 2 - 0.004, d, zt, IRON, 0.003, 1)
    end("SM_S_StormDrain", ZN, replaces=rep("storm drain box 200 x 800 x 31 uu + 6 bars 40 uu wide every 130 uu", (Lg, Wg, 36 / S)),
        notes="Grate (6 bars, 47 mm gaps) on a hollow catch basin 0.11 m deep (10 mm walls): bbox height 0.122 m, the grate top is the top of the bbox. "
              "The road/landscape under it needs a hole. The code box sits INSIDE the curb volume (x -11100..-10900), where it cannot be seen; "
              "the preview puts it in the road next to the curb.")

def build_street_furniture():
    begin()
    r, Hc, Ht = 180 / S, 900 / S, 1020 / S
    lathe([(0.121, 0), (0.121, 0.025), (r + 0.012, 0.035), (r, 0.05), (r, Hc * 0.62), (r + 0.01, Hc * 0.64), (r + 0.01, Hc * 0.70), (r, Hc * 0.72), (r, Hc - 0.03),
           (230 / S, Hc - 0.02), (230 / S, Hc + 0.01), (r * 0.95, Hc + 0.04), (r * 0.6, Ht - 0.045), (0.04, Ht - 0.03)], RED, 16)
    cyl(0.03, 0.04, 0, 0, Ht - 0.02, ENAMEL, verts=5)
    for i in range(6):
        a = i * 1.047 + 0.5
        cyl(0.009, 0.012, 0.105 * math.cos(a), 0.105 * math.sin(a), 0.031, IRON, verts=6)
    zn = 650 / S
    cyl(0.042, 700 / S - 0.05, 0, 0, zn, RED, "X", 12)
    for s in (-1, 1):
        cyl(0.052, 0.035, s * (350 / S - 0.0175), 0, zn, ENAMEL, "X", 8)
    cyl(0.055, 0.14, 0, -0.08, zn - 0.05, RED, "Y", 12)
    cyl(0.066, 0.04, 0, -0.14, zn - 0.05, ENAMEL, "Y", 8)
    end("SM_S_Hydrant", ZN, replaces=rep("hydrant: cylinder r180 h900, cap r230 h120, side nozzle part 700 uu", (700 / S, 460 / S, Ht)),
        notes="Side nozzles along model X (0.368 m over the caps), the big front nozzle towards -Y: depth 0.28 m instead of 0.24 m.")
    begin()
    Ht = 7450 / S
    zl = Ht - 0.30
    lathe([(220 / S, 0), (220 / S, 0.03), (0.09, 100 / S), (0.075, 0.12), (0.075, 0.45), (0.085, 0.47), (0.085, 0.50), (0.05, 0.54)], IRON, 14)
    cyl(90 / S, zl - 0.5, 0, 0, (zl + 0.5) / 2, IRON, verts=10, r2=0.034)
    cyl(0.05, 0.02, 0, 0, 2.2, IRON, verts=10, bev=0.004, seg=1)
    bx(-0.105, 0.105, -0.012, 0.012, zl - 0.16, zl - 0.136, IRON, 0.004, 1)
    for s in (-1, 1):
        ball(0.018, s * 0.105, 0, zl - 0.148, IRON, seg=8, rings=6)
    rbox(0.13, 0.13, 0.02, 0, 0, zl - 0.005, IRON, 0.004, 1)
    cyl(0.085, 0.19, 0, 0, zl + 0.095, LAMPGLASS, verts=4, r2=0.135, rot=(0, 0, math.pi / 4))
    cyl(0.148, 0.075, 0, 0, zl + 0.2275, IRON, verts=4, r2=0.02, rot=(0, 0, math.pi / 4))
    ball(0.016, 0, 0, Ht - 0.016, IRON, seg=8, rings=6)
    end("SM_S_StreetLamp", ZN, replaces=rep("lamp post: pole r90 h7200, base r220 h100, head box 400 x 400 x 250 uu", (440 / S, 440 / S, Ht)),
        notes="Lantern on a tapering pole; the lit glass is its own slot (LampGlass, emissive). Same total height (3.92 m); the lantern takes the top 0.30 m.")
    # dropped ice cream: built around the scoop (the code's ball), the cone lies where the code's cone lies
    begin()
    wobp = lambda a: 1 + 0.12 * math.sin(3 * a + 0.5) + 0.07 * math.sin(5 * a)
    lathe([(360 / S, 0), (360 / S * 0.97, 0.004), (360 / S * 0.6, 0.006), (0.02, 0.006)], ICE, 28, wob=wobp)
    ball(1.0, 0, 0, 0.062, ICE, (220 / S, 220 / S, 0.066), 20, 12)
    for (x, y, rr) in ((0.16, 0.09, 0.035), (-0.15, 0.11, 0.028), (-0.05, -0.17, 0.03)):
        ball(rr, x, y, 0.004, ICE, (1, 1, 0.3), 10, 6)
    Lc, rc = 520 / S, 120 / S
    ha = math.atan(rc / Lc)
    dv = Vector((300, -200, 0)).normalized()
    A = Vector((dv.x * math.cos(ha), dv.y * math.cos(ha), -math.sin(ha)))
    M = Vector((300 / S, -200 / S, 0)) - dv * (Lc / 2 * math.cos(ha))
    M.z = rc * math.cos(ha) + 0.002
    eul = Vector((0, 0, 1)).rotation_difference(A).to_euler()
    c = M + A * (Lc / 2)
    cyl(rc, Lc, c.x, c.y, c.z, WAFFLE, verts=14, r2=0.006, rot=eul)
    for f in (0.12, 0.38, 0.64):
        p = M + A * (Lc * f)
        torus(rc * (1 - f) + 0.003, 0.004, WAFFLE, (p.x, p.y, p.z), rot=eul, seg=14, rseg=4)
    rnd = random.Random(9)
    for i in range(10):
        a, b = rnd.uniform(0, 6.283), rnd.uniform(0.2, 1.1)
        rbox(0.016, 0.005, 0.005, 220 / S * math.sin(b) * math.cos(a), 220 / S * math.sin(b) * math.sin(a), 0.062 + 0.066 * math.cos(b) + 0.001,
             (RED, TEAL, FABRIC, LEAF)[i % 4], 0, 0, rot=(rnd.uniform(-0.5, 0.5), rnd.uniform(-0.5, 0.5), rnd.uniform(0, 3.14)))
    end("SM_S_IceCream", ZN, kind="prop", origin=(0, 0, 0), replaces=rep("ice cream: cone 240 x 520 uu on its side, scoop ellipsoid 220 x 220 x 140, puddle r360 h4", (0.47, 0.40, 0.14)),
        notes="Origin on the ground under the scoop (the code's ball/puddle centre). Puddle 6 mm thick (code 2 mm).")
    begin()
    R = 170 / S
    wobg = lambda a: 1 + 0.14 * math.sin(2 * a + 1) + 0.08 * math.sin(5 * a)
    lathe([(R, 0), (R * 0.96, 0.006), (R * 0.7, 0.011), (R * 0.2, 0.012)], GUM, 22, wob=wobg)
    tube(smooth([(R * 0.5, 0, 0.008), (R * 1.3, 0.03, 0.012), (R * 1.8, 0.02, 0.006), (R * 2.1, 0.05, 0.004)], 2), [0.008, 0.007, 0.006, 0.005, 0.004, 0.003, 0.003], GUM, 6)
    end("SM_S_Gum", ZN, kind="prop", origin=(0, 0, 0), replaces=rep("chewing gum cylinder r170 h12", (2 * R, 2 * R, 12 / S)), notes="A trodden blob 12 mm thick with a stretched string.")

def build_car():
    W, L, H = 3300 / S, 8500 / S, 2850 / S
    hw, hl = W / 2, L / 2
    zb, zd = 600 / S, 1900 / S
    wr, ww, wx, wy = 650 / S, 0.22, 0.72, 2950 / S
    c = 50 / S
    NH = 6.0
    def side(z):
        zc, b = (zb + zd) / 2, (zd - zb) / 2
        return hw * max(0.0, 1 - abs((z - zc) / b) ** NH) ** (1 / NH)
    begin()
    hull = loft([(-hl + 0.03, 0.70, zb + 0.11, zd - 0.24), (-hl + 0.08, 0.82, zb + 0.04, zd - 0.15), (-hl + 0.25, hw, zb, zd - 0.08), (-1.25, hw, zb, zd - 0.015),
                 (-0.3, hw, zb, zd), (1.3, hw, zb, zd), (hl - 0.25, hw, zb, zd - 0.03), (hl - 0.08, 0.83, zb + 0.04, zd - 0.10), (hl - 0.03, 0.72, zb + 0.11, zd - 0.18)], CARPAINT, n=NH, k=24)
    for sx in (-1, 1):
        for sy in (-1, 1):
            cut(hull, cyl(wr + 0.06, 0.50, sx * (hw - 0.05), sy * wy, wr, PLINTH, "X", 20))
    hc = 1150 / S
    loft([(-1.18 + c, 0.60, zd - 0.05, zd + 0.02), (-0.75 + c, hc, zd - 0.05, H - 0.03), (-0.3 + c, hc, zd - 0.05, H), (0.7 + c, hc, zd - 0.05, H),
          (1.05 + c, 0.60, zd - 0.05, H - 0.04), (1.24 + c, 0.58, zd - 0.05, zd + 0.02)], ENAMEL, n=8.0, k=24)
    for (y0, z0, y1, z1, wd) in ((-1.18 + c, zd + 0.02, -0.75 + c, H - 0.03, 0.94), (1.05 + c, H - 0.04, 1.24 + c, zd + 0.02, 0.90)):
        rbox(wd, math.hypot(y1 - y0, z1 - z0) - 0.09, 0.03, 0, (y0 + y1) / 2, (z0 + z1) / 2, GLASS, 0.008, 2, rot=(math.atan2(z1 - z0, y1 - y0), 0, 0))
    for sx in (-1, 1):
        for (ya, yb) in ((-0.62 + c, 0.10 + c), (0.18 + c, 0.90 + c)):
            rbox(0.03, yb - ya, 0.30, sx * hc, (ya + yb) / 2, zd + 0.21, GLASS, 0.008, 2)
        rbox(0.10, 0.06, 0.07, sx * (hc + 0.075), -0.80 + c, zd + 0.10, CARPAINT, 0.02, 2)
        rbox(0.05, 0.03, 0.025, sx * (hc + 0.02), -0.80 + c, zd + 0.085, CARPAINT, 0.006, 1)
        rbox(0.014, 2.2, 0.03, sx * (hw + 0.001), 0, (zb + zd) / 2, CHROME, 0.005, 1)
        for y in (-0.30, 0.50):
            rbox(0.02, 0.10, 0.02, sx * side(zd - 0.10), y, zd - 0.10, CHROME, 0.005, 1)
        cyl(0.075, 0.04, sx * 0.50, -hl + 0.03, 0.64, HEADLIGHT, "Y", 14)
        torus(0.078, 0.010, CHROME, (sx * 0.50, -hl + 0.012, 0.64), rot=(math.pi / 2, 0, 0), seg=14, rseg=4)
        rbox(0.20, 0.03, 0.09, sx * 0.52, hl - 0.035, 0.68, RED, 0.01, 2)
        bx(sx * 0.38 - 0.03, sx * 0.38 + 0.03, -1.9, 1.9, zb - 0.035, zb + 0.01, PLINTH, 0.004, 1)
    rbox(1.60, 0.09, 0.12, 0, -hl + 0.05, zb + 0.17, CHROME, 0.03, 2)
    rbox(1.60, 0.09, 0.12, 0, hl - 0.05, zb + 0.17, CHROME, 0.03, 2)
    rbox(0.62, 0.03, 0.11, 0, -hl + 0.03, 0.62, PLINTH, 0.01, 2)
    for k in range(3):
        rbox(0.58, 0.008, 0.012, 0, -hl + 0.013, 0.588 + k * 0.032, CHROME, 0, 0)
    rbox(0.36, 0.010, 0.09, 0, -hl + 0.005, zb + 0.17, PAPER, 0.002, 1)
    rbox(0.36, 0.010, 0.10, 0, hl - 0.022, 0.60, PAPER, 0.002, 1)
    for sy in (-1, 1):
        cyl(0.03, 2 * wx - ww + 0.02, 0, sy * wy, wr, IRON, "X", 8)
    ball(0.10, 0, wy, wr - 0.02, IRON, (1, 1, 0.9), 10, 8)
    bx(-0.5, 0.5, -wy - 0.04, -wy + 0.04, zb - 0.05, zb + 0.01, PLINTH, 0.004, 1)
    tube(smooth([(0.30, -0.6, zb - 0.04), (0.30, 0.6, zb - 0.04), (0.34, 1.2, zb - 0.045), (0.45, 1.75, zb - 0.05), (0.45, hl - 0.03, zb - 0.03)], 2), 0.028, STEEL, 8)
    cyl(0.07, 0.5, 0.30, 0.2, zb - 0.055, STEEL, "Y", 10, bev=0.01, seg=1)
    rbox(0.34, 0.5, 0.09, 0, -1.2, zb - 0.03, IRON, 0.02, 2)
    cyl(0.30, 0.06, 0, 1.78, zb - 0.02, PLINTH, verts=16, bev=0.01, seg=1)
    rbox(0.6, 0.5, 0.08, -0.3, 0.9, zb - 0.03, PLINTH, 0.02, 2)
    end("SM_S_Car", ZN, origin=(0, 0, 0),
        replaces=rep("parked car: body 3300 x 8500 x 1300 uu at z 600, cabin 2300 x 4600 x 950 uu, glass, lights, plate", (W, L, H)),
        notes="Origin on the GROUND under the centre (the wheels are separate and touch z=0); the model front (-Y) is the code's +Y end with the headlights. "
              "Underside 0.316 m (600 uu) above the ground as in the code, with frame rails, axles, exhaust, tank and sump hanging down to 0.23 m. "
              "Solid body (no interior). The code's car floats: its wheels end at z=0 while the road is at z=-230 uu; the preview stands it on the road.")
    begin()
    lathe([(0.20, -ww / 2), (0.30, -ww / 2), (0.335, -ww / 2 + 0.025), (wr, -ww / 2 + 0.06), (wr, ww / 2 - 0.06), (0.335, ww / 2 - 0.025), (0.30, ww / 2), (0.20, ww / 2)],
          RUBBER, 24, closed=True, rot=(0, math.pi / 2, 0))
    lathe([(0.205, -ww / 2 + 0.01), (0.205, ww / 2 - 0.02), (0.18, ww / 2 - 0.012), (0.12, ww / 2 - 0.03), (0.06, ww / 2 - 0.012), (0.05, ww / 2 - 0.012)],
          STEEL, 24, rot=(0, math.pi / 2, 0))
    for i in range(5):
        a = i * 1.2566
        cyl(0.012, 0.012, ww / 2 - 0.02, 0.085 * math.cos(a), 0.085 * math.sin(a), CHROME, "X", 6)
    end("SM_S_Car_Wheel", ZN, kind="part", origin=(0, 0, 0), origin_note="wheel centre, on the axle",
        replaces=rep("wheel cylinder d1300 x 450 uu", (ww, 2 * wr, 2 * wr)),
        notes="Origin at the wheel CENTRE on the axle (it spins about model X); hubcap on +X. 0.22 m wide (code 0.237). "
              "Moved 0.115 m (218 uu) outwards from the code's x so the tyres sit near the body sides.")
    for sx in (-1, 1):
        for sy in (-1, 1):
            attach("SM_S_Car_Wheel", "SM_S_Car", (sx * wx, sy * wy, wr), rot=(0, 0, 0 if sx > 0 else 180), motion={"type": "spin", "axis": [1, 0, 0]})

def build_street():
    build_house()
    build_ground()
    build_street_furniture()
    build_car()

def layout_street():
    wall, lawn = smat("Wall", (0.96, 0.80, 0.52), 0.9), smat("Lawn", (0.33, 0.58, 0.20), 0.95)
    st = {}
    st["lawn"] = stage_box("S_lawn", (-9400, -5200, -200), (-7200, 5600, 0), lawn)
    st["house"] = stage_box("S_house", (-7200, -5200, 0), (-7000, 5600, 5400), wall)
    st["base"] = stage_box("S_base", (-17000, -5600, -700), (-9400, 6100, -470), smat("Dirt", (0.25, 0.17, 0.10), 0.95))
    spawn("SM_S_Facade", -7230, 200, 5400, 90)
    spawn("SM_S_FacadeCorner", -7230, 5650, 0, 90)
    spawn("SM_S_FacadeCorner", -7230, -5250, 0, 90)
    spawn("SM_S_Gutter", -7350, 200, 8900, 90)
    spawn("SM_S_RoofEdge", -7275, 200, 9150, 90)
    spawn("SM_S_Drainpipe", -7380, -5250, 0, 90)
    for yc in (-4100, -1900, 300):
        spawn("SM_Y_FencePanel", -9400, yc, 0, 90, note="street fence along x=-9400")
    spawn("SM_Y_FencePanel_Half", -9400, 1950, 0, 90)
    spawn("SM_Y_FenceGateFrame", -9400, 3600, 0, 90, note="bays y 2500..4700; the leaf opens towards the street")
    spawn("SM_Y_FencePanel_Half", -9400, 5250, 0, 90)
    k = 0
    y = -5200.0
    while y < 5600:
        spawn("SM_S_PavementSlab_" + ("B" if k in (3, 6) else "A"), -10200, y + 600, -200, 0, record=(k in (0, 3)), note="one per 1200 uu from y=-5200 to 5600")
        spawn("SM_S_Kerb", -11050, y + 600, -230, 90, record=(k == 0), note="one per 1200 uu from y=-5200 to 5600")
        y += 1200
        k += 1
    for k in range(7):
        spawn("SM_S_Road", -13800, -4400 + 1600 * k, -260, 0, record=(k == 0), note="one per 1600 uu from y=-5200; the dash lands where the code's dashes are")
    spawn("SM_S_Manhole", -12300, 2600, -244, 0)
    spawn("SM_S_StormDrain", -11200, -3000, -439, 90, note="moved out of the curb into the road (code x -11100..-10900); shown 12 mm proud because the preview road has no hole. "
                                                           "With a hole cut, z=-462 puts the grate flush with the road.")
    spawn("SM_S_Car", -13550, 50, -230, 0, note="on the road (the code's boxes hover 230 uu above it)")
    spawn("SM_S_Hydrant", -10300, 4400, 0, 90)
    spawn("SM_S_StreetLamp", -10700, -4600, 0, 0)
    spawn("SM_S_IceCream", -10400, -1500, 0, 0)
    spawn("SM_S_Gum", -10000, 2500, 0, 0)
    rnd = random.Random(777)
    n = 0
    while n < 160:
        x, y = rnd.uniform(-9250, -7350), rnd.uniform(-5100, 5500)
        if (x + 7900) ** 2 + (y + 2500) ** 2 < 950 ** 2:
            continue
        spawn("SM_Y_GrassTuft_" + "ABC"[n % 3], x, y, 0, rnd.uniform(0, 360), record=False)
        n += 1
    return st
