# Ant Kitchen: the yard pieces (see living_pieces.py). Dimensions from AKitchenBuilder::BuildYard and the
# fence/gate part of BuildWestStreet: Unreal units / 1900 = metres.
from living_common import *

ZN = "yard"
YD = "AKitchenBuilder::BuildYard (KitchenBuilder.cpp)"
WS = "AKitchenBuilder::BuildWestStreet (KitchenHouse.cpp)"
CL = "AKitchenBuilder::BuildClutter (KitchenHouse.cpp)"

def rep(fn, what, dims):
    return {"function": fn, "boxes": what, "matched_dims_m": [round(d, 4) for d in dims]}

BAY = 1100 / S

def picket(xc, z0=0.03, z1=1.22, w=0.075, y0=-0.039, y1=-0.021):
    h = w / 2
    prism([(xc - h, z0), (xc + h, z0), (xc + h, z1 - 0.045), (xc, z1), (xc - h, z1 - 0.045)], y0, y1, FENCE, plane="XZ", bev=0.003, seg=1)

def post(x, y=0.0, h=2600 / S):
    ps = 120 / S
    rbox(ps, ps, h - 0.04, x, y, (h - 0.04) / 2, FENCE, 0.006, 2)
    rbox(ps + 0.014, ps + 0.014, 0.012, x, y, h - 0.05, FENCE, 0.003, 1)
    cyl(ps * 0.74, 0.045, x, y, h - 0.0225, FENCE, verts=4, r2=0.004, rot=(0, 0, math.pi / 4))

def rails(x0, x1):
    for (za, zb) in ((600 / S, 760 / S), (1900 / S, 2060 / S)):
        bx(x0, x1, -0.021, 0.021, za, zb, FENCE, 0.004, 1)

def build_fence():
    for name, n in (("SM_Y_FencePanel", 2), ("SM_Y_FencePanel_Half", 1)):
        L = n * BAY
        begin()
        for k in range(n):
            post(-L / 2 + k * BAY)
        rails(-L / 2, L / 2)
        npk = int(round(L / 0.0965))
        for i in range(npk):
            picket(-L / 2 + (i + 0.5) * L / npk)
        end(name, ZN, origin=(0, 0, 0),
            replaces=rep(YD + " / " + WS, "fence: posts 120 x 120 x 2600 uu every 1100 uu, two rails 80 x 160 uu at 600 and 1900 uu", (L, 120 / S, 2600 / S)),
            notes="%d bay(s) = %d uu of fence. Origin on the ground at the middle of the span, on the post line. Posts at the LEFT end of each bay "
                  "(model -X), none at the right end: the next module brings it; finish a run with SM_Y_FencePost. Pickets (new, the code has only "
                  "posts and rails) stand on the front (-Y), 30 mm above the ground so ants walk under." % (n, n * 1100))
    begin()
    post(0)
    end("SM_Y_FencePost", ZN, origin=(0, 0, 0), replaces=rep(YD, "one fence post 120 x 120 x 2600 uu", (120 / S, 120 / S, 2600 / S)), notes="Closes the end of a fence run.")
    # the bay with the gate
    L = 2 * BAY
    xl = -L / 2
    gx1 = L / 2 - 60 / S
    gx0 = gx1 - 1300 / S
    begin()
    post(xl)
    post(gx0 - 60 / S)
    rails(xl, gx0 - 60 / S)
    k = 0
    while True:
        xc = xl + (k + 0.5) * 0.0965
        if xc - 0.0375 > gx0 - 120 / S:
            break
        picket(xc)
        k += 1
    for z in (0.34, 1.02):
        cyl(0.007, 0.07, gx0 + 0.004, -0.045, z, IRON, verts=8)
    end("SM_Y_FenceGateFrame", ZN, origin=(0, 0, 0),
        replaces=rep(WS, "fence bays y 2500..4700 with the 1300 uu gate opening (posts skipped at 3600)", (L, 120 / S, 2600 / S)),
        notes="2 bays (2200 uu). Opening 1300 uu wide as in the code but shifted 140 uu towards the next post (y 3340..4640 instead of 3200..4500), "
              "so the gate closes on the next module's post instead of needing a third one.")
    begin()
    lw = 1300 / S - 0.016
    x0 = gx0 + 0.008
    x1 = x0 + lw
    for (za, zb) in ((0.30, 0.38), (0.98, 1.06)):
        bx(x0, x1, -0.021, 0.009, za, zb, FENCE, 0.004, 1)
    dx, dz = lw - 0.06, 0.62
    rbox(math.hypot(dx, dz), 0.028, 0.07, (x0 + x1) / 2, -0.006, 0.68, FENCE, 0.004, 1, rot=(0, -math.atan2(dz, dx), 0))
    n = 6
    for i in range(n):
        picket(x0 + (i + 0.5) * lw / n, 0.06, 1.13 + 0.10 * math.sin(math.pi * (i + 0.5) / n), w=lw / n - 0.02)
    for z in (0.34, 1.02):
        rbox(0.16, 0.006, 0.03, x0 + 0.075, -0.042, z, IRON, 0.002, 1)
    rbox(0.05, 0.012, 0.03, x1 - 0.04, -0.045, 1.02, IRON, 0.003, 1)
    end("SM_Y_FenceGate_Leaf", ZN, kind="part", origin=(x0 - 0.004, -0.045, 0.06), parent="SM_Y_FenceGateFrame",
        motion=hinge((0, 0, 1), -110, "model -Y (the street side)"), preview=50.0 / 110.0,
        replaces=rep(WS, "gate box 80 x 1150 x 2500 uu swung open 50 deg", (lw, 0.06, 1.17)),
        notes="Built closed. Hinged on the left gate post; the code has it open 50 deg (preview pose). 60 mm gap under it.")

def build_tree():
    begin()
    wob = lambda a: 1 + 0.06 * math.sin(3 * a + 1) + 0.04 * math.sin(5 * a)
    prof = [(0.42, 0), (0.33, 0.25), (0.285, 0.7), (520 / S, 1.5), (0.26, 3.0), (0.23, 4.2), (0.19, 9000 / S)]
    def rad(z):
        for (r0, z0), (r1, z1) in zip(prof, prof[1:]):
            if z <= z1:
                return r0 + (r1 - r0) * (z - z0) / (z1 - z0)
        return prof[-1][0]
    lathe(prof, BARK, 16, wob=wob)
    for i in range(5):
        a = i * 1.257 + 0.4
        c, s = math.cos(a), math.sin(a)
        tube([(0.2 * c, 0.2 * s, 0.42), (0.42 * c, 0.42 * s, 0.16), (0.66 * c, 0.66 * s, 0.02), (0.86 * c, 0.86 * s, -0.05)], [0.13, 0.10, 0.07, 0.035], BARK, 10)
    for i in range(7):                                              # bark ridges
        a0 = i * 0.898 + 0.2
        pts = []
        for z in (0.3, 0.9, 1.7, 2.6, 3.5, 4.3):
            a = a0 + 0.12 * math.sin(z * 1.3 + i)
            rr = rad(z) * wob(a) - 0.006
            pts.append((rr * math.cos(a), rr * math.sin(a), z))
        tube(smooth(pts, 2), 0.032, BARK, 5)
    ball(1.0, 0, -rad(1.6) * wob(-1.5708) + 0.01, 1.6, PLINTH, (0.085, 0.05, 0.12), 10, 8)          # a knot hole (dark, not a real cavity)
    torus(0.105, 0.028, BARK, (0, -rad(1.6) * wob(-1.5708) + 0.012, 1.6), rot=(math.pi / 2, 0, 0), seg=14, rseg=5, scale=(0.95, 1, 1.3))
    tube(smooth([(0.0, 0, 3.0), (0.28, 0, 2.88), (0.62, 0, 2.70), (0.93, 0, 2.55), (1.15, 0.02, 2.56), (1.32, 0.05, 2.66)], 2), [0.10 - 0.006 * i for i in range(11)], BARK, 8)
    torus(0.115, 0.03, BARK, (0.25, 0, 2.90), rot=(0, math.pi / 2 - 0.4, 0), seg=12, rseg=5)
    ico(0.34, 1.42, 0.05, 2.88, LEAF, (1.1, 1.05, 0.8), 3)
    ico(0.26, 1.20, -0.20, 2.94, LEAF_L, (1, 1, 0.8), 3)
    ico(0.24, 1.26, 0.28, 2.86, LEAF_L, (1, 1, 0.8), 3)
    ico(1.0, 0, 0, 5.45, LEAF, (1.55, 1.55, 1.15), 3)
    for i in range(6):
        a = i * 1.047 + 0.3
        ico(1.0, 1.25 * math.cos(a), 1.25 * math.sin(a), 5.25 + 0.15 * (-1) ** i, LEAF_L if i % 2 else LEAF, (0.96, 0.96, 0.75), 3)
    for i in range(3):
        a = i * 2.094 + 0.9
        ico(1.0, 0.6 * math.cos(a), 0.6 * math.sin(a), 6.17, LEAF if i % 2 else LEAF_L, (0.95, 0.95, 0.72), 3)
    ico(1.0, 0, 0, 4.76, LEAF, (1.3, 1.3, 0.6), 3)
    wob2 = lambda a: 1 + 0.08 * math.sin(4 * a) + 0.05 * math.sin(7 * a + 2)
    lathe([(0.66, -0.02), (0.60, 0.02), (0.30, 0.045), (0.05, 0.05)], SOIL, 14, loc=(-500 / S, 400 / S, 0), wob=wob2)
    end("SM_Y_Tree", ZN, origin=(0, 0, 0),
        replaces=rep(YD, "tree: trunk r520 h9000, branch box 1900 x 200 x 200 (25 deg down, +X), canopy ellipsoid 4200 x 4200 x 2600 at 10500, soil disc", (8400 / S, 8400 / S, 13100 / S)),
        notes="Origin on the ground on the trunk axis. The branch points along model +X (the code's +X), leaves the trunk at 3.0 m, dips to 2.55 m at 0.93 m out "
              "(the code's branch) and ends in a leaf cluster that reaches 1.8 m out. Bark ridges, a (closed) knot hole at 1.6 m on the -Y side, root flares "
              "and a soil patch that dip up to 5 cm below the ground. Canopy is a cluster of 11 blobs inside the code's ellipsoid.")
def build_bin():
    Rr, Hh = 930 / S, 2600 / S
    begin()
    lathe([(0.40, 0.0), (0.43, 0.02), (0.45, 0.45), (0.462, 0.46), (0.462, 0.50), (0.452, 0.51), (0.468, 1.0), (0.48, 1.01), (0.48, 1.05), (0.47, 1.06),
           (900 / S, Hh - 0.04), (Rr, Hh - 0.033), (Rr, Hh), (0.455, Hh), (0.455, Hh - 0.04), (0.44, 1.0), (0.415, 0.035)], BIN, 20)
    for s in (-1, 1):
        x = s * 0.47
        tube(smooth([(x, -0.09, 1.12), (x + s * 0.05, -0.07, 1.14), (x + s * 0.06, 0, 1.14), (x + s * 0.05, 0.07, 1.14), (x, 0.09, 1.12)], 2), 0.014, IRON, 6)
    for sx in (-0.12, 0.12):
        cyl(0.016, 0.06, sx, Rr + 0.004, Hh - 0.005, IRON, "X", 8)
    end("SM_Y_Bin", ZN, replaces=rep(YD, "outdoor bin cylinder r900 h2600 + rim r930 h40", (2 * Rr, 2 * Rr, Hh)),
        notes="Hollow (15-20 mm wall, 35 mm bottom), two ribs, side handles. Hinge knuckles at the back (+Y).")
    begin()
    z = Hh + 0.003
    lathe([(0.50, z), (0.50, z + 0.03), (0.47, z + 0.05), (0.30, z + 0.10), (0.08, z + 0.125)], BIN, 20)
    lathe([(0.445, z - 0.03), (0.445, z + 0.01), (0.43, z + 0.01), (0.43, z - 0.03)], BIN, 20, closed=True)
    tube(smooth([(-0.09, 0, z + 0.11), (-0.06, 0, z + 0.16), (0, 0, z + 0.175), (0.06, 0, z + 0.16), (0.09, 0, z + 0.11)], 2), 0.014, IRON, 6)
    end("SM_Y_Bin_Lid", ZN, kind="part", origin=(0, Rr + 0.004, z), parent="SM_Y_Bin", motion=hinge((1, 0, 0), -110, "up and back (model +Y)"), preview=1.0,
        replaces=rep(YD, "open lid box 1900 x 80 x 1300 uu tilted 20 deg", (1.0, 1.0, 0.18)),
        notes="Round domed lid (the code had a square slab). Built closed; hinge axis along X at the back rim. Open = -110 deg about model +X (right-handed).")
    begin()
    for (x, y, zz, sx, sy, sz, sub) in ((-0.14, -0.10, 0.25, 0.24, 0.24, 0.21, 2), (0.16, -0.06, 0.27, 0.24, 0.24, 0.23, 2), (0.0, 0.17, 0.26, 0.24, 0.24, 0.22, 2),
                                        (-0.05, -0.02, 0.66, 0.33, 0.32, 0.24, 2), (0.08, 0.05, 1.03, 0.33, 0.33, 0.20, 2), (-0.06, 0.0, 1.33, 0.34, 0.34, 0.14, 3)):
        ico(1.0, x, y, zz, BAG, (sx, sy, sz), sub)
    ico(0.07, 0.15, 0.10, 1.44, PAPER, sub=1)
    cyl(0.035, 0.11, -0.18, 0.08, 1.45, STEEL, verts=8, rot=(1.2, 0.3, 0))
    cyl(0.02, 0.05, -0.06, 0.0, 1.48, BAG, verts=8, r2=0.035)
    end("SM_Y_Bin_Trash", ZN, kind="prop", origin=(0, 0, 0), parent="SM_Y_Bin",
        replaces=rep(YD, "trash mound ellipsoid 700 x 700 x 250 on top of the bin", (0.68, 0.68, 0.14)),
        notes="A pile of bags from the floor of the bin up to just above the rim (the mound the flies feed on), with gaps between the bags to crawl through.")
    begin()
    ico(1.0, 0, 0, 0.09, BAG, (500 / S, 380 / S, 180 / S), 3)
    cyl(0.02, 0.05, 0, 0, 0.20, BAG, verts=8, r2=0.035)
    end("SM_Y_TrashBag", ZN, kind="prop", replaces=rep(YD, "trash bag ellipsoid radii 500 x 380 x 180 on the grass", (1000 / S, 760 / S, 360 / S)))
    begin()
    k = 130 * 2 / S / 0.18
    lathe([(0.02, 0), (0.045, 0.01 * k), (0.05, 0.03 * k), (0.022, 0.06 * k), (0.018, 0.09 * k), (0.022, 0.12 * k), (0.05, 0.15 * k), (0.045, 0.17 * k), (0.02, 0.18 * k)], BONE, 10)
    lathe([(0.021, -0.001), (0.047, 0.009 * k), (0.052, 0.03 * k), (0.04, 0.036 * k)], RED, 10)
    lathe([(0.04, 0.144 * k), (0.052, 0.15 * k), (0.047, 0.171 * k), (0.021, 0.181 * k)], RED, 10)
    cyl(0.004, 0.03, 0, 0, 0.18 * k + 0.012, WALNUT, verts=6)
    end("SM_Y_AppleCore", ZN, kind="prop", replaces=rep(YD, "apple core ellipsoid radii 90 x 90 x 130", (180 / S, 180 / S, 260 / S)))

def build_yard_things():
    for nm, R, seed in (("A", 300 / S, 1), ("B", 340 / S, 2)):
        begin()
        rnd = random.Random(seed)
        n = 12
        pts = []
        for i in range(n):
            rr = R * rnd.uniform(0.80, 1.0)
            pts.append((rr * math.cos(2 * math.pi * i / n), rr * math.sin(2 * math.pi * i / n)))
        prism(pts, 0, 22 / S, STONE, bev=0.004, seg=2)
        end("SM_Y_SteppingStone_" + nm, ZN, replaces=rep(YD, "stepping stone cylinder r%d h22" % round(R * S), (2 * R, 2 * R, 22 / S)))
    begin()
    r = 220 / S
    ball(r, 0, 0, r, TENNIS, seg=16, rings=10)
    for s in (-1, 1):
        torus(r * 0.72, 0.006, ENAMEL, (s * r * 0.694, 0, r), rot=(0, math.pi / 2, 0), seg=16, rseg=4)
    end("SM_Y_DogBall", ZN, kind="prop", replaces=rep(YD + " / " + CL, "dog ball sphere r220 (yard), tennis ball r190 (kitchen)", (2 * r, 2 * r, 2 * r)))
    begin()
    cyl(0.028, 0.37, 0, 0, 0.042, BONE, "X", 10)
    for sx in (-1, 1):
        for sy in (-1, 1):
            ball(0.042, sx * 0.185, sy * 0.026, 0.042, BONE, seg=10, rings=8)
    end("SM_Y_DogBone", ZN, kind="prop", replaces=rep(CL, "bone: cylinder d120 x 700 + 2 end ellipsoids (the kitchen one)", (900 / S, 160 / S, 140 / S)))
    begin()
    tube(smooth([(-0.2, 0, 0.024), (-0.07, 0.015, 0.024), (0.07, -0.015, 0.024), (0.2, 0, 0.024)], 3), 0.022, SISAL, 8)
    for sx in (-1, 1):
        torus(0.03, 0.02, RED if sx < 0 else TEAL, (sx * 0.14, 0, 0.034), rot=(0, math.pi / 2, 0.3 * sx), seg=10, rseg=6)
        ball(0.034, sx * 0.14, 0, 0.034, RED if sx < 0 else TEAL, seg=10, rings=8)
    cyl(0.022, 0.05, 0.222, 0, 0.026, SISAL, "X", 8, r2=0.034)
    cyl(0.034, 0.05, -0.222, 0, 0.026, SISAL, "X", 8, r2=0.022)
    end("SM_Y_DogRope", ZN, kind="prop", notes="New (not in the code): knotted rope toy, 0.49 m long.")
    begin()
    R, h = 500 / S, 160 / S
    lathe([(R, 0), (R, 0.012), (R * 0.84, h), (R * 0.78, h), (R * 0.70, 0.022)], RED, 20)
    end("SM_Y_DogBowl", ZN, kind="prop", replaces=rep(YD, "red dog bowl cylinder r500 h160", (2 * R, 2 * R, h)), notes="Hollow bowl, 22 mm bottom.")
    begin()
    lathe([(0.10, 0), (0.115, 0.03), (0.09, 0.055), (0.075, 0.06), (0.085, 0.085), (0.062, 0.11), (0.048, 0.115), (0.052, 0.135), (0.025, 0.165), (0.006, 0.19)], WALNUT, 12)
    end("SM_Y_DogPoop", ZN, kind="prop", notes="For the fly feeder at AKitchenBuilder::YardPoop() (its visual is not in the files I read): 0.23 m wide, 0.19 m tall.")
    for nm, n, hmin, hmax, spread, seed in (("A", 7, 0.10, 0.20, 0.03, 1), ("B", 9, 0.14, 0.27, 0.04, 2), ("C", 5, 0.06, 0.12, 0.035, 3)):
        begin()
        rnd = random.Random(seed)
        for i in range(n):
            a, rr = rnd.uniform(0, 6.283), rnd.uniform(0, spread)
            blade(rr * math.cos(a), rr * math.sin(a), rnd.uniform(hmin, hmax), GRASS if i % 2 else GRASS_D, a + rnd.uniform(-0.5, 0.5), rnd.uniform(0.15, 0.45))
        end("SM_Y_GrassTuft_" + nm, ZN, kind="prop", origin=(0, 0, 0),
            replaces=rep(YD + " / " + WS, "instanced grass blade cubes 14 x 3 x 180..520 uu (no collision)", (0.1, 0.1, hmax)),
            notes="Low-poly tuft of %d three-sided blades (%.2f-%.2f m tall, the code's 0.095-0.274 m range). Scatter with random yaw; no collision in the code." % (n, hmin, hmax))
    begin()
    W, D, T = 3300 / S, 220 / S, 80 / S
    bx(-W / 2, W / 2, -D / 2, D / 2, 0, T - 0.008, ENAMEL, 0.010, 2)
    bx(-W / 2, W / 2, D / 2 - 0.02, D / 2, T - 0.012, T, ENAMEL, 0.003, 1)
    end("SM_Y_WindowSill", ZN, replaces=rep(YD, "outer window sill box 3300 x 220 x 80 uu", (W, D, T)), notes="Back (+Y) goes against the house wall.")

def build_yard():
    build_fence()
    build_tree()
    build_bin()
    build_yard_things()

def layout_yard():
    wall, lawn = smat("Wall", (0.96, 0.80, 0.52), 0.9), smat("Lawn", (0.33, 0.58, 0.20), 0.95)
    st = {}
    st["lawn1"] = stage_box("Y_lawn1", (-12000, -16000, -200), (7000, -5200, 0), lawn)
    st["lawn2"] = stage_box("Y_lawn2", (7000, -16000, -200), (21000, -9200, 0), lawn)
    st["house1"] = stage_box("Y_house1", (-7200, -5200, 0), (7200, -5000, 5400), wall)
    st["house2"] = stage_box("Y_house2", (7000, -9200, 0), (21000, -9000, 5400), wall)
    st["house3"] = stage_box("Y_house3", (7000, -9200, 0), (7200, -5000, 5400), wall)
    st["window"] = stage_box("Y_window", (-1500, -5215, 2200), (1500, -5190, 4200), smat("WindowSky", (0.55, 0.80, 1.0), 0.3))
    for k in range(15):
        spawn("SM_Y_FencePanel", 21000 - 1100 - 2200 * k, -16000, 0, 180, record=(k == 0), note="south fence x 21000..-12000: 15 modules, then SM_Y_FencePost at x=-12000")
    spawn("SM_Y_FencePost", -12000, -16000, 0, 0)
    for k in range(4):
        spawn("SM_Y_FencePanel", -12000, -16000 + 1100 + 2200 * k, 0, 90, record=(k == 0), note="west fence of the yard y -16000..-7200: 4 modules, then 2 half modules to -5000")
    for k in range(2):
        spawn("SM_Y_FencePanel_Half", -12000, -7200 + 550 + 1100 * k, 0, 90)
    spawn("SM_Y_Tree", -8000, -12500, 0, 0)
    spawn("SM_Y_Bin", 4200, -13000, 0, 180, note="hinge side towards +Y (north), where the code's open lid stands")
    spawn("SM_Y_TrashBag", 2900, -13500, 0, 20)
    spawn("SM_Y_AppleCore", 3300, -14100, 0, 0)
    for i in range(7):
        spawn("SM_Y_SteppingStone_" + ("B" if i % 2 else "A"), 600 + i * 520, -6400 - i * 950, 0, i * 50)
    spawn("SM_Y_DogBowl", -6000, -7200, 0, 0)
    spawn("SM_Y_DogBall", -4300, -7800, 0, 0)
    spawn("SM_Y_DogBone", -5200, -8300, 0, 30, note="free choice (the code's bone is in the kitchen at (-3100,-4300))")
    spawn("SM_Y_DogRope", -5000, -7000, 0, -20, note="free choice")
    spawn("SM_Y_DogPoop", -3000, -9500, 0, 0, note="AKitchenBuilder::YardPoop()")
    spawn("SM_Y_WindowSill", 0, -5310, 2120, 180)
    rnd = random.Random(4242)
    keep = [(4200, -13000, 1100), (-8000, -12500, 800), (-3000, -9500, 500), (-6000, -7200, 700), (-4300, -7800, 400), (-5200, -8300, 500), (-5000, -7000, 500), (2900, -13500, 600)]
    keep += [(600 + i * 520, -6400 - i * 950, 420) for i in range(7)]
    n = 0
    while n < 520:
        x, y = rnd.uniform(-11800, 20800), rnd.uniform(-15800, -5500)
        if x > 6800 and y > -9400:
            continue
        if any((x - a) ** 2 + (y - b) ** 2 < r * r for a, b, r in keep):
            continue
        spawn("SM_Y_GrassTuft_" + "ABC"[n % 3], x, y, 0, rnd.uniform(0, 360), record=False)
        n += 1
    return st
