# Ant Kitchen: the living room pieces (see living_pieces.py). Dimensions come from
# AKitchenBuilder::BuildLivingRoom / BuildClutter / BuildShell: Unreal units / 1900 = metres.
from living_common import *
from living_common import _obj

ZN = "living"
LR = "AKitchenBuilder::BuildLivingRoom (KitchenBuilder.cpp)"
CL = "AKitchenBuilder::BuildClutter (KitchenHouse.cpp)"
SH = "AKitchenBuilder::BuildShell (KitchenHouse.cpp)"

def rep(fn, what, dims):
    return {"function": fn, "boxes": what, "matched_dims_m": [round(d, 4) for d in dims]}

# ---------------------------------------------------------------- sofa
def build_sofa():
    W, D, H = 7400 / S, 1700 / S, 2100 / S
    hw, hd, ax = W / 2, D / 2, 3300 / S
    zb0, zb1 = 260 / S, 700 / S
    begin()
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * 2900 / S, sy * 750 / S
            cyl(0.026, zb0 + 0.01, x, y, (zb0 + 0.01) / 2, WALNUT, verts=12, r2=0.038)
            cyl(0.029, 0.014, x, y, 0.007, BRASS, verts=12)
    bx(-ax, ax, -hd, hd, zb0, zb1, SOFA, 0.03, 2)                      # base
    for s in (-1, 1):
        bx(s * ax, s * hw, -hd, hd, zb0, 1350 / S, SOFA, 0.06, 3)       # arms
    bx(-ax, ax, hd - 0.15, hd, zb1 - 0.03, H, SOFA, 0.05, 3)           # back frame
    for y in (-hd + 0.045, hd - 0.045):                                # wooden rails under the base
        bx(-ax + 0.05, ax - 0.05, y - 0.015, y + 0.015, zb0 - 0.022, zb0 + 0.006, WALNUT, 0.004, 1)
    end("SM_L_Sofa", ZN, replaces=rep(LR, "sofa: 4 legs, base, backrest, 2 armrests", (W, D, H)),
        notes="Gap under the base 0.137 m (260 uu), 0.115 m under the two rails. Seat/back cushions and throw pillows are separate pieces.")
    begin()
    cw, cd, ch = 1960 / S - 0.006, 1260 / S, 250 / S
    rbox(cw, cd, ch * 0.8, 0, 0, ch * 0.4, CUSHION, 0.035, 3)
    rbox(cw - 0.10, cd - 0.10, ch * 0.4, 0, 0, ch * 0.8, CUSHION, 0.03, 3)
    end("SM_L_Sofa_SeatCushion", ZN, kind="part", replaces=rep(LR, "seat cushion x3", (1960 / S, cd, ch)))
    for i in (-1, 0, 1):
        attach("SM_L_Sofa_SeatCushion", "SM_L_Sofa", (i * 2000 / S, -hd + 40 / S + cd / 2, zb1))
    begin()
    bt, bh = 0.09, 0.66
    rbox(cw, bt, bh, 0, 0, bh / 2, CUSHION, 0.035, 3)
    rbox(cw - 0.12, 0.03, bh - 0.12, 0, -bt / 2, bh / 2, CUSHION, 0.012, 2)
    for xb in (-0.28, 0.0, 0.28):
        cyl(0.016, 0.012, xb, -bt / 2 - 0.015, bh * 0.6, SOFA, "Y", 10)
    end("SM_L_Sofa_BackCushion", ZN, kind="part", replaces=rep(LR, "front part of the backrest box (front face at y=1000 uu)", (cw, bt + 0.015, bh)))
    for i in (-1, 0, 1):
        attach("SM_L_Sofa_BackCushion", "SM_L_Sofa", (i * 2000 / S, -hd + 1300 / S + (bt + 0.015) / 2, zb1))
    for nm, m, m2 in (("SM_L_Pillow_A", FABRIC, RED), ("SM_L_Pillow_B", RED, FABRIC)):
        begin()
        rbox(0.38, 0.11, 0.33, 0, 0, 0.165, m, 0.05, 3)
        for s in (-1, 1):
            cyl(0.022, 0.012, 0, s * 0.054, 0.165, m2, "Y", 10)
        end(nm, ZN, kind="prop", replaces=rep(LR, "throw pillow (ellipsoid 760 x 320 x 660 uu)", (0.40, 0.168, 0.347)))
    attach("SM_L_Pillow_A", "SM_L_Sofa", (0.684, 0.125, zb1 + ch), rot=(-14, 0, 6))
    attach("SM_L_Pillow_B", "SM_L_Sofa", (-1.105, 0.125, zb1 + ch), rot=(-14, 0, -8))

# ---------------------------------------------------------------- coffee table and what lies on it
def build_coffee_table():
    W, D, H = 2500 / S, 1600 / S, 980 / S
    hw, hd, zt = W / 2, D / 2, 900 / S
    begin()
    bx(-hw, hw, -hd, hd, zt, H, BUTCHER, 0.012, 2)
    for sx in (-1, 1):
        for sy in (-1, 1):
            cyl(0.024, zt, sx * (hw - 100 / S), sy * (hd - 100 / S), zt / 2, WALNUT, verts=12, r2=0.033)
    for s in (-1, 1):
        y = s * (hd - 0.065)
        bx(-(hw - 0.07), hw - 0.07, y - 0.009, y + 0.009, zt - 0.06, zt, WALNUT, 0.003, 1)
        x = s * (hw - 0.065)
        bx(x - 0.009, x + 0.009, -(hd - 0.07), hd - 0.07, zt - 0.06, zt, WALNUT, 0.003, 1)
    bx(-(hw - 50 / S), hw - 50 / S, -(hd - 50 / S), hd - 50 / S, 300 / S, 340 / S, BUTCHER, 0.006, 2)
    end("SM_L_CoffeeTable", ZN, replaces=rep(LR, "coffee table: 4 legs, top, lower shelf", (W, D, H)),
        notes="Top surface 0.516 m, shelf surface 0.179 m (same as the code).")
    # snack bowl with crisps
    begin()
    R, h, t = 330 / S, 160 / S, 0.010
    lathe([(R * 0.55, 0), (R * 0.62, 0.006), (R * 0.95, h * 0.75), (R, h), (R - t, h), (R * 0.95 - t, h * 0.75), (R * 0.58, 0.016)], ENAMEL, 20)
    lathe([(R * 0.97 - 0.004, h * 0.8), (R * 0.97 + 0.0015, h * 0.8), (R + 0.0015, h * 0.93), (R - 0.004, h * 0.93)], RED, 20, closed=True)
    ball(R * 0.78, 0, 0, 0.062, CRISPS, (1, 1, 0.38), 12, 6)
    rnd = random.Random(5)
    for i in range(14):
        a, r = rnd.uniform(0, 6.283), rnd.uniform(0, R * 0.6)
        ico(0.034, r * math.cos(a), r * math.sin(a), 0.10 + rnd.uniform(0, 0.02) - r * 0.2, CRISPS, (1, 0.8, 0.22), 1,
            rot=(rnd.uniform(-0.6, 0.6), rnd.uniform(-0.6, 0.6), a))
    end("SM_L_SnackBowl", ZN, kind="prop", replaces=rep(LR, "snack bowl cylinder r330 h160", (2 * R, 2 * R, h)))
    attach("SM_L_SnackBowl", "SM_L_CoffeeTable", (550 / S, 100 / S, H))
    # biscuit plate
    begin()
    R = 200 / S
    lathe([(R * 0.6, 0), (R * 0.66, 0.004), (R, 0.012), (R, 0.016), (R * 0.64, 0.009)], ENAMEL, 20)
    for (x, y, z, rx) in ((-0.035, 0.02, 0.015, 0.0), (0.042, 0.028, 0.015, 0.0), (0.0, -0.03, 0.024, 0.22)):
        cyl(0.045, 0.012, x, y, z, BISCUIT, verts=12, bev=0.004, seg=1, rot=(rx, 0, 0))
        for k in range(4):
            a = k * 1.7 + x * 40
            ico(0.006, x + 0.024 * math.cos(a), y + 0.024 * math.sin(a), z + 0.006 + (0.004 if rx else 0), WALNUT, sub=1)
    end("SM_L_BiscuitPlate", ZN, kind="prop", replaces=rep(CL, "cookie plate r200 h20 + cookie r110 h25 (on the armchair armrest)", (2 * R, 2 * R, 45 / S)))
    attach("SM_L_BiscuitPlate", "SM_L_CoffeeTable", (-0.05, -0.22, H))
    # remote
    begin()
    L, Wd, T = 600 / S, 0.105, 30 / S
    rbox(L, Wd, T, 0, 0, T / 2, PLINTH, 0.006, 2)
    cyl(0.011, 0.004, -L / 2 + 0.03, 0, T + 0.001, RED, verts=10)
    for i in range(4):
        for j in (-1, 0, 1):
            cyl(0.009, 0.004, -L / 2 + 0.075 + i * 0.030, j * 0.03, T + 0.001, STEEL, verts=8)
    torus(0.026, 0.007, STEEL, (L / 2 - 0.075, 0, T), seg=12, rseg=4)
    cyl(0.011, 0.005, L / 2 - 0.075, 0, T + 0.0015, TEAL, verts=10)
    end("SM_L_Remote", ZN, kind="prop", replaces=rep(LR, "remote box 600 x 300 x 30 uu", (L, 300 / S, T)),
        notes="Narrower than the code box (0.105 m instead of 0.158 m) so it still reads as a remote; length and height match.")
    attach("SM_L_Remote", "SM_L_CoffeeTable", (-750 / S, 500 / S, H))
    # soda can
    begin()
    r, h = 120 / S, 380 / S
    lathe([(r * 0.8, 0), (r, 0.012), (r, h - 0.016), (r * 0.82, h), (r * 0.78, h), (r * 0.76, h - 0.006)], RED, 16)
    lathe([(r - 0.004, h * 0.34), (r + 0.0015, h * 0.34), (r + 0.0015, h * 0.62), (r - 0.004, h * 0.62)], ENAMEL, 16, closed=True)
    cyl(r * 0.76, 0.002, 0, 0, h - 0.005, CHROME, verts=16)
    rbox(0.03, 0.018, 0.003, 0.012, 0, h - 0.0025, CHROME, 0.002, 1)
    end("SM_L_SodaCan", ZN, kind="prop", replaces=rep(LR, "soda can cylinder r120 h380", (2 * r, 2 * r, h)))
    attach("SM_L_SodaCan", "SM_L_CoffeeTable", (-650 / S, -200 / S, H))
    # magazines on the lower shelf
    begin()
    rbox(1000 / S, 800 / S, 40 / S, 0, 0, 20 / S, RED, 0.004, 1)
    rbox(0.34, 0.07, 0.003, -0.05, -0.13, 40 / S + 0.0005, PAPER, 0, 0)
    a = math.radians(9)
    rbox(950 / S, 650 / S, 20 / S, 0.015, 0.02, 50 / S, PAPER, 0.003, 1, rot=(0, 0, a))
    rbox(0.30, 0.06, 0.003, -0.02, -0.07, 60 / S + 0.0005, TEAL, 0, 0, rot=(0, 0, a))
    rbox(0.16, 0.16, 0.003, 0.11, 0.07, 60 / S + 0.0005, FABRIC, 0, 0, rot=(0, 0, a))
    end("SM_L_Magazines", ZN, kind="prop", replaces=rep(LR, "two magazine boxes on the shelf", (1000 / S, 800 / S, 60 / S)))
    attach("SM_L_Magazines", "SM_L_CoffeeTable", (-350 / S, 100 / S, 340 / S))

# ---------------------------------------------------------------- TV stand, TV, console, controller
def build_tv():
    W, D, H = 5500 / S, 1300 / S, 1100 / S
    hw, hd, t = W / 2, D / 2, 0.03
    zf = 0.02
    zb, zt = zf + 0.03, H - 0.028
    begin()
    for x in (-hw + 0.06, 0.0, hw - 0.06):
        for y in (-hd + 0.06, hd - 0.06):
            rbox(0.07, 0.07, zf + 0.004, x, y, zf / 2 + 0.002, PLINTH, 0.004, 1)
    bx(-hw, hw, -hd, hd, zf, zb, WALNUT, 0.004, 1)
    bx(-hw, hw, -hd, hd, zt, H, BUTCHER, 0.008, 2)
    for s in (-1, 1):
        bx(s * hw, s * (hw - t), -hd + 0.004, hd, zb - 0.002, zt + 0.002, WALNUT, 0.004, 1)
    bx(-t / 2, t / 2, -hd + 0.058, hd, zb - 0.002, zt + 0.002, WALNUT, 0.004, 1)
    back = bx(-hw + 0.002, hw - 0.002, hd - 0.012, hd, zb - 0.002, zt + 0.002, WALNUT, 0.003, 1)
    cut(back, cyl(0.035, 0.1, hw / 2, hd - 0.006, zb + 0.34, PLINTH, "Y", 16))       # cable hole, right bay
    cut(back, cyl(0.035, 0.1, -hw / 2, hd - 0.006, zb + 0.34, PLINTH, "Y", 16))      # cable hole, left bay
    cut(back, cyl(0.03, 0.1, -0.35, hd - 0.006, zb + 0.012, PLINTH, "Y", 12))        # gnawed hole at the floor of the left bay
    bx(t / 2 - 0.002, hw - t + 0.002, -hd + 0.06, hd - 0.012, 0.29, 0.31, WALNUT, 0.004, 1)   # shelf, right bay
    for z in (zb, zt - 0.006):
        bx(-hw + t, -t / 2, -hd + 0.026, -hd + 0.032, z, z + 0.006, PLINTH, 0, 0)    # rails between the two doors
    end("SM_L_TVStand", ZN, replaces=rep(LR, "TV stand: bottom, top, 2 sides, divider (hollow)", (W, D, H)),
        notes="Hollow. Stands on 20 mm feet (gap under it). Back panel 12 mm with two 70 mm cable holes and a gnawed hole at floor level. "
              "Left bay closed by two sliding doors, right bay open with a shelf.")
    begin()
    dw = (hw - t - t / 2) / 2 + 0.012
    dz0, dz1 = zb + 0.004, zt - 0.004
    dh = dz1 - dz0
    bx(-dw / 2, dw / 2, -0.016, 0, 0, dh, CABINET, 0.003, 1)
    bx(-dw / 2 + 0.045, dw / 2 - 0.045, -0.0175, -0.012, 0.045, dh - 0.045, CABINET_D, 0.002, 1)
    bx(-0.008, 0.008, -0.019, -0.012, dh / 2 - 0.07, dh / 2 + 0.07, BRASS, 0.002, 1)
    end("SM_L_TVStand_Door", ZN, kind="part", origin=(0, 0, 0), origin_note="back-bottom-centre (sliding part)", notes="Sliding door, origin back-bottom-centre. Two instances on two tracks.")
    attach("SM_L_TVStand_Door", "SM_L_TVStand", (-hw + t + dw / 2, -hd + 0.024, dz0), motion=slide((dw - 0.03, 0, 0)), preview=0.9)
    attach("SM_L_TVStand_Door", "SM_L_TVStand", (-t / 2 - dw / 2, -hd + 0.050, dz0), motion=slide((-(dw - 0.03), 0, 0)), preview=0.0)
    # the TV
    TW, TH, Dp = 3900 / S, 2200 / S, 400 / S
    yp = Dp / 2 - 75 / S
    z0 = 0.085
    begin()
    rbox(0.56, Dp, 0.018, 0, 0, 0.009, PLINTH, 0.006, 2)
    rbox(0.14, 0.045, 0.10, 0, yp + 0.012, 0.06, PLINTH, 0.008, 2)
    rbox(TW, 0.045, TH - z0, 0, yp - 0.012, (TH + z0) / 2, PLINTH, 0.012, 2)
    rbox(TW * 0.6, 0.035, (TH - z0) * 0.55, 0, yp + 0.022, (TH + z0) / 2 - 0.05, PLINTH, 0.012, 2)
    rbox(TW - 0.05, 0.006, TH - z0 - 0.06, 0, yp - 0.0355, (TH + z0) / 2 + 0.005, SCREEN, 0.002, 1)
    rbox(0.07, 0.004, 0.012, 0, yp - 0.0355, z0 + 0.016, CHROME, 0.0015, 1)
    cyl(0.004, 0.004, TW / 2 - 0.06, yp - 0.0355, z0 + 0.016, RED, "Y", 8)
    end("SM_L_TV", ZN, kind="prop", replaces=rep(LR, "TV box 3900 x 150 x 2200 uu + foot 500 x 400 x 80 + screen", (TW, Dp, TH)),
        notes="Screen is its own material slot (Screen, emissive) for the glow.")
    attach("SM_L_TV", "SM_L_TVStand", (0, 350 / S, H))
    # console and controller
    begin()
    L, Dd, T = 600 / S, 400 / S, 150 / S
    rbox(L, Dd, T - 0.006, 0, 0, 0.006 + (T - 0.006) / 2, ENAMEL, 0.012, 2)
    rbox(L - 0.03, Dd - 0.03, 0.008, 0, 0, 0.004, PLINTH, 0.002, 1)
    rbox(L * 0.5, 0.004, 0.006, -L * 0.15, -Dd / 2 - 0.001, T * 0.6, PLINTH, 0, 0)
    cyl(0.008, 0.004, L / 2 - 0.04, -Dd / 2 - 0.001, T * 0.6, TEAL, "Y", 10)
    for i in (0, 1):
        rbox(0.014, 0.004, 0.007, L / 2 - 0.09 - i * 0.03, -Dd / 2 - 0.001, T * 0.3, PLINTH, 0, 0)
    rbox(L - 0.06, 0.02, 0.003, 0, 0.04, T + 0.0005, PLINTH, 0, 0)
    end("SM_L_GameConsole", ZN, kind="prop", replaces=rep(LR, "game console box 600 x 400 x 150 uu", (L, Dd, T)))
    attach("SM_L_GameConsole", "SM_L_TVStand", (850 / S, 50 / S, H))
    begin()
    rbox(0.10, 0.055, 0.03, 0, 0, 0.03, ENAMEL, 0.012, 2)
    for s in (-1, 1):
        ball(0.03, s * 0.055, -0.012, 0.026, ENAMEL, (0.85, 1.25, 0.85), 12, 8, rot=(0, 0, s * 0.35))
        cyl(0.010, 0.012, s * 0.026, -0.008, 0.05, PLINTH, verts=10)
        cyl(0.013, 0.004, s * 0.026, -0.008, 0.057, PLINTH, verts=10)
    rbox(0.022, 0.007, 0.004, -0.052, 0.012, 0.046, PLINTH, 0, 0)
    rbox(0.007, 0.022, 0.004, -0.052, 0.012, 0.046, PLINTH, 0, 0)
    for (dx, dy, mm) in ((0.012, 0, RED), (-0.012, 0, TEAL), (0, 0.012, FABRIC), (0, -0.012, LEAF)):
        cyl(0.0055, 0.004, 0.052 + dx, 0.012 + dy, 0.046, mm, verts=8)
    end("SM_L_Controller", ZN, kind="prop", notes="New (not in the code): the console's gamepad, lying on the coffee table.")
    attach("SM_L_Controller", "SM_L_CoffeeTable", (0.42, -0.27, 980 / S), rot=(0, 0, 25))

# ---------------------------------------------------------------- bookshelf, books, vase
BOOKS = [("A", 0.055, 0.26, 0.30, RED, BRASS), ("B", 0.075, 0.30, 0.36, TEAL, ENAMEL), ("C", 0.10, 0.32, 0.40, WALNUT, BRASS),
         ("D", 0.05, 0.22, 0.25, FABRIC, RED), ("E", 0.065, 0.28, 0.33, CABINET, ENAMEL), ("F", 0.085, 0.30, 0.38, ENAMEL, RED),
         ("G", 0.048, 0.24, 0.27, NAVY, BRASS), ("H", 0.11, 0.30, 0.29, RED, ENAMEL)]

def build_bookshelf():
    W, D, H = 5200 / S, 1000 / S, 4660 / S
    hw, hd, st = W / 2, D / 2, 100 / S
    ihw, yb = hw - st, hd - 0.02
    begin()
    bx(-hw + 0.004, hw - 0.004, yb, hd, 0, 4600 / S, WALNUT, 0.003, 1)
    for s in (-1, 1):
        bx(s * hw, s * ihw, -hd, hd, 0, 4600 / S, WALNUT, 0.006, 2)
    bx(-40 / S, 40 / S, -hd + 0.004, hd, 0, 4600 / S, WALNUT, 0.005, 2)
    for k in range(5):
        z = k * 900 / S
        bx(-ihw - 0.002, ihw + 0.002, -hd + 0.002, yb + 0.002, z, z + 60 / S, BUTCHER, 0.004, 1)
    bx(-hw, hw, -hd, hd, 4600 / S, H, BUTCHER, 0.008, 2)
    end("SM_L_Bookshelf", ZN, replaces=rep(LR, "bookshelf: back, 2 sides, divider, 6 shelves", (W, D, H)),
        notes="Shelf tops at k*0.4737+0.0316 m (k=0..4) and the top at 2.4526 m, as in the code. Back panel 20 mm (code 53 mm): the bays are deeper.")
    for (k, t, d, h, col, band) in BOOKS:
        begin()
        c = 0.005
        bx(-t / 2 + c, t / 2 - c, -d / 2 + 0.006, d / 2 - 0.005, 0.005, h - 0.005, PAPER, 0, 0)
        for s in (-1, 1):
            bx(s * t / 2, s * (t / 2 - c), -d / 2 + 0.004, d / 2, 0, h, col, 0.002, 1)
        bx(-t / 2, t / 2, -d / 2 - 0.004, -d / 2 + 0.008, 0, h, col, 0.005, 2)
        for zz in (0.18, 0.78):
            bx(-t / 2 - 0.0008, t / 2 + 0.0008, -d / 2 - 0.0048, -d / 2 + 0.004, h * zz - 0.006, h * zz + 0.006, band, 0, 0)
        end("SM_L_Book_" + k, ZN, kind="prop", replaces=rep(LR, "random book boxes (90-220 uu thick, 450-780 uu tall, 700 deep)", (t, d, h)),
            notes="Spine faces -Y. Stands on its bottom; origin bottom-centre.")
    rnd = random.Random(77)
    lean = math.radians(10)
    for k in range(5):
        ztop = k * 900 / S + 60 / S
        for bay in (0, 1):
            x = (-ihw + 0.012) if bay == 0 else (40 / S + 0.012)
            xend = (-40 / S - 0.012) if bay == 0 else (ihw - 0.012)
            stacked = False
            while x < xend:
                u = rnd.random()
                if u < 0.12:
                    x += rnd.uniform(0.10, 0.20)          # a gap to squeeze through
                    continue
                key, t, d, h, _c, _b = rnd.choice(BOOKS)
                y = yb - 0.025 - d / 2 - rnd.uniform(0, 0.03)
                if u < 0.20 and not stacked and x + 0.42 < xend:
                    zz = ztop                                 # a small stack lying flat
                    for j in range(rnd.randint(2, 3)):
                        key, t, d, h, _c, _b = rnd.choice(BOOKS)
                        attach("SM_L_Book_" + key, "SM_L_Bookshelf", (x + 0.005 * j, yb - 0.03 - d / 2, zz + t / 2), rot=(0, 90, 0))
                        zz += t
                    x += 0.42
                    stacked = True
                    continue
                if x + t > xend:
                    break
                if u > 0.90 and x + t + h * math.sin(lean) + 0.03 < xend:
                    attach("SM_L_Book_" + key, "SM_L_Bookshelf", (x + t / 2, y, ztop + t / 2 * math.sin(lean)), rot=(0, 10, 0))
                    x += t + h * math.sin(lean) + rnd.uniform(0.04, 0.10)
                    continue
                attach("SM_L_Book_" + key, "SM_L_Bookshelf", (x + t / 2, y, ztop))
                x += t + rnd.uniform(0.003, 0.008)
    begin()
    R, Hh, w = 220 / S, 700 / S, 0.010
    lathe([(R * 0.45, 0), (R * 0.5, 0.01), (R * 0.95, Hh * 0.3), (R, Hh * 0.42), (R * 0.8, Hh * 0.62), (R * 0.42, Hh * 0.82), (R * 0.4, Hh * 0.9), (R * 0.55, Hh),
           (R * 0.55 - w, Hh), (R * 0.4 - w, Hh * 0.9), (R * 0.42 - w, Hh * 0.82), (R * 0.8 - w, Hh * 0.62), (R - w, Hh * 0.42), (R * 0.95 - w, Hh * 0.3), (R * 0.45, 0.012)], TEAL, 16)
    lathe([(R * 0.9, Hh * 0.47), (R * 0.96, Hh * 0.47), (R * 0.9, Hh * 0.55), (R * 0.84, Hh * 0.55)], ENAMEL, 16, closed=True)
    end("SM_L_Vase", ZN, kind="prop", replaces=rep(LR, "vase cylinder r220 h700 on top of the bookshelf", (2 * R, 2 * R, Hh)),
        notes="Hollow, open neck. No flowers: the ceiling is 2 cm above it.")
    attach("SM_L_Vase", "SM_L_Bookshelf", (-1100 / S, 0, H))
    # a few books lying in the open bay of the TV stand (bottom board top at 0.05 m, shelf top at 0.31 m)
    zz = 0.05
    for key in ("D", "G", "A"):
        t = [b for b in BOOKS if b[0] == key][0][1]
        attach("SM_L_Book_" + key, "SM_L_TVStand", (0.22, 0.04, zz + t / 2), rot=(0, 90, 0))
        zz += t
    attach("SM_L_Book_H", "SM_L_TVStand", (0.85, 0.02, 0.31 + 0.055), rot=(0, 90, 0))
    attach("SM_L_Book_E", "SM_L_TVStand", (0.80, 0.03, 0.31 + 0.11 + 0.0325), rot=(0, 90, 8))

# ---------------------------------------------------------------- armchair, lamp, plant, radiator
def build_armchair():
    W, D, H = 2200 / S, 1700 / S, 1900 / S
    hw, hd, at, zl = W / 2, D / 2, 200 / S, 0.06
    begin()
    for sx in (-1, 1):
        for sy in (-1, 1):
            cyl(0.022, zl + 0.01, sx * (hw - 0.07), sy * (hd - 0.07), (zl + 0.01) / 2, WALNUT, verts=10, r2=0.032)
    bx(-(hw - at), hw - at, -hd, hd - at, zl, 650 / S, FABRIC, 0.03, 2)
    for s in (-1, 1):
        bx(s * hw, s * (hw - at), -hd, hd, zl, 1200 / S, FABRIC, 0.05, 3)
    bx(-hw, hw, hd - at, hd, zl, H - 0.008, FABRIC, 0.05, 3)
    bx(-(hw - at) + 0.012, hw - at - 0.012, -hd + 0.012, hd - at - 0.008, 650 / S - 0.012, 800 / S, CUSHION, 0.035, 3)
    bx(-(hw - at) + 0.03, hw - at - 0.03, hd - at - 0.05, hd - at + 0.01, 800 / S - 0.02, H - 0.07, FABRIC, 0.03, 3)
    rbox(0.42, at - 0.004, 0.008, 0, hd - at / 2, H - 0.004, PAPER, 0.003, 1)
    rbox(0.42, 0.006, 0.10, 0, hd - at - 0.053, H - 0.058, PAPER, 0.002, 1)
    end("SM_L_Armchair", ZN, replaces=rep(LR, "armchair: base, seat cushion, back, 2 arms", (W, D, H)),
        notes="On 60 mm legs (the code box sat on the floor): ants can get under it. Seat top 0.421 m, arm tops 0.632 m, as in the code.")
    attach("SM_L_BiscuitPlate", "SM_L_Armchair", (-1000 / S, -100 / S, 1200 / S), note="the code's cookie plate on the armrest")

def build_floor_lamp():
    begin()
    Rb, zt = 260 / S, 3860 / S
    lathe([(Rb, 0), (Rb, 0.014), (Rb * 0.8, 60 / S), (0.035, 0.04), (0.03, 0.06)], IRON, 20)
    cyl(35 / S, zt - 0.05, 0, 0, (zt + 0.05) / 2, IRON, verts=10)
    cyl(0.028, 0.03, 0, 0, 1.1, BRASS, verts=10, bev=0.004)
    z0, z1, R0, R1, w = 3700 / S, 4200 / S, 420 / S, 0.15, 0.008
    lathe([(R0, z0), (R1, z1), (R1 - w, z1), (R0 - w, z0)], SHADE, 20, closed=True)
    rr = R0 + (R1 - R0) * (zt - z0) / (z1 - z0) - w * 0.5
    for i in range(3):
        a = i * 2.094 + 0.5
        tube([(0, 0, zt - 0.012), (rr * math.cos(a), rr * math.sin(a), zt - 0.012)], 0.004, IRON, 5)
    cyl(0.02, 0.03, 0, 0, zt + 0.005, BRASS, verts=10)
    ball(0.045, 0, 0, zt + 0.05, BULB, seg=12, rings=8)
    tube([(0.04, 0, zt - 0.012), (0.04, 0, zt - 0.25)], 0.003, BRASS, 5)
    ball(0.01, 0.04, 0, zt - 0.26, BRASS, seg=8, rings=6)
    end("SM_L_FloorLamp", ZN, replaces=rep(LR, "floor lamp: base r260 h60, pole r35 h3800, shade r420 h500", (2 * R0, 2 * R0, z1)),
        notes="Shade is a hollow cone open at both ends (8 mm wall). Bulb has its own slot (Bulb, emissive).")

def build_plant():
    begin()
    R, Hh = 450 / S, 780 / S
    lathe([(R * 0.68, 0), (R * 0.72, 0.012), (R * 0.95, Hh * 0.86), (R, Hh * 0.87), (R, Hh), (R - 0.018, Hh), (R - 0.018, Hh * 0.9), (R * 0.72 - 0.006, 0.02)], TERRA, 18)
    cyl(R - 0.02, 0.03, 0, 0, Hh * 0.9, SOIL, verts=18)
    tube(smooth([(0, 0, Hh * 0.85), (0.01, 0.005, 0.58), (-0.008, 0.0, 0.76), (0.0, 0.0, 0.90)], 3), [0.02] * 4 + [0.016] * 3 + [0.012] * 3, WALNUT, 6)
    for i in range(13):
        a = i * 2.4 + 0.2
        zz = 0.46 + 0.036 * i
        L = 0.27 - 0.008 * i
        tilt = math.radians(28 - 1.2 * i + (8 if i % 2 else 0))
        leaf(L, L * 0.55, 0.016, LEAF if i % 3 else LEAF_L, loc=(0.012 * math.cos(a), 0.012 * math.sin(a), zz), rot=(0, -tilt, a), curl=0.22)
    leaf(0.12, 0.06, 0.014, LEAF_L, loc=(0, 0, 0.90), rot=(0, -math.radians(75), 1.0), curl=0.05)
    end("SM_L_PottedPlant", ZN, replaces=rep(LR, "pot cylinder r450 h780, soil, 7 leaf ellipsoids up to ~1960 uu", (0.53, 0.53, 1.03)),
        notes="Pot is hollow down to the soil (soil surface at 0.384 m, 26 mm below the rim). Leaves have real thickness (16 mm in the middle).")

def build_radiator():
    W, Dp, Hh = 1720 / S, 170 / S, 1100 / S
    fw, pitch = 120 / S, 200 / S
    begin()
    for i in range(9):
        rbox(fw, Dp, Hh, -W / 2 + fw / 2 + i * pitch, 0, Hh / 2, ENAMEL, 0.022, 2)
    for z in (0.05, Hh - 0.05):
        cyl(0.024, W - fw, 0, 0, z, ENAMEL, "X", 12)
    yb = Dp / 2
    for x in (-W / 2 + pitch * 1.5, W / 2 - pitch * 1.5):
        rbox(0.03, 30 / S + 0.01, 0.08, x, yb + 15 / S - 0.005, Hh * 0.75, STEEL, 0.003, 1)
    for x in (-W / 2 + fw / 2, W / 2 - fw / 2):
        cyl(0.012, 30 / S + 0.02, x, yb + 15 / S - 0.01, 0.05, CHROME, "Y", 10)
    end("SM_L_Radiator", ZN, replaces=rep(LR, "radiator: 9 fins 120 x 170 x 1100 uu at 200 uu pitch + bottom rail", (W, Dp, Hh)),
        notes="Gaps of 42 mm between the fins (open except at the two header tubes). Depth 0.105 m = fins 0.0895 + brackets reaching the wall. "
              "Hangs 0.158 m (300 uu) above the floor.")

# ---------------------------------------------------------------- carpet, sock, coin, charger, yarn, basket
def build_carpet():
    W, D, t = 7000 / S, 5500 / S, 0.006
    fr = 0.028
    w, d = W - 2 * fr, D
    begin()
    bm = bmesh.new()
    def rect(ins, z):
        a, b = w / 2 - ins, d / 2 - ins
        return [bm.verts.new((-a, -b, z)), bm.verts.new((a, -b, z)), bm.verts.new((a, b, z)), bm.verts.new((-a, b, z))]
    loops = [rect(0, 0)]
    mats = []
    for ins, mi in ((0.012, 1), (0.11, 1), (0.19, 0), (0.22, 1), (0.52, 0), (0.55, 1)):
        loops.append(rect(ins, t))
        mats.append(mi)
    bm.faces.new(loops[0]).material_index = 1
    for (A, B), mi in zip(zip(loops, loops[1:]), mats):
        for k in range(4):
            j = (k + 1) % 4
            bm.faces.new((A[k], A[j], B[j], B[k])).material_index = mi
    bm.faces.new(loops[-1]).material_index = 0
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    o = _obj(bm, CARPET, "carpet")
    o.data.materials.append(CARPET_T)
    n = int(d / 0.045)
    for s in (-1, 1):
        for i in range(n):
            y = -d / 2 + 0.02 + i * (d - 0.04) / (n - 1)
            rbox(fr + 0.006, 0.010, 0.004, s * (w / 2 + fr / 2 - 0.003), y, 0.002, CARPET_T, 0, 0)
    end("SM_L_Carpet", ZN, replaces=rep(LR, "rug box 7000 x 5500 x 4 uu", (W, D, 4 / S)),
        notes="6 mm thick (code 2 mm) with a 12 mm ramped edge so ants can walk onto it; fringe on the two short ends. Bands are material slots, not texture.")

def build_small_things():
    # the dropped sock: a hollow fabric tunnel, open at the cuff
    begin()
    r = 0.062
    path = smooth([(-0.27, 0.02, 0), (-0.12, 0.0, 0), (0.02, 0.0, 0), (0.10, -0.03, 0), (0.17, -0.11, 0), (0.21, -0.20, 0), (0.225, -0.27, 0)], 3)
    path = [p + Vector((0, 0, r * 0.72)) for p in path]
    rs = [r] * len(path)
    rs[-1], rs[-2], rs[-3] = r * 0.45, r * 0.8, r * 0.95
    tube(path, rs, SOCK, segs=12, wall=0.009, cap0=False, cap1=True, squash=0.72)
    tube(path[0:3], r + 0.004, RED, segs=12, wall=0.013, cap0=False, cap1=False, squash=0.72)
    for k in (4, 6):
        tube(path[k:k + 2], r + 0.003, ENAMEL, segs=12, wall=0.012, cap0=False, cap1=False, squash=0.72)
    end("SM_L_Sock", ZN, kind="prop", replaces=rep(CL, "sock cylinder d260 x 1100 uu lying on the floor", (1100 / S, 260 / S, 0.09)),
        notes="Hollow (9 mm wall), open at the cuff, closed toe: a tunnel to hide in. L-shaped, so the footprint is wider than the code cylinder.")
    # the coin
    begin()
    R = 140 / S
    cyl(R, 0.006, 0, 0, 0.003, BRASS, verts=24, bev=0.0015, seg=1)
    torus(R - 0.006, 0.003, BRASS, (0, 0, 0.006), seg=24, rseg=4)
    cyl(R * 0.45, 0.002, 0, 0, 0.007, BRASS, verts=6)
    for i in range(8):
        cyl(0.005, 0.002, R * 0.68 * math.cos(i * 0.785), R * 0.68 * math.sin(i * 0.785), 0.007, BRASS, verts=6)
    end("SM_L_Coin", ZN, kind="prop", replaces=rep(CL, "coin cylinder r140 h10", (2 * R, 2 * R, 10 / S)), notes="9 mm thick (code 5 mm).")
    # phone charger: the brick on the floor, the cable up the front of the sofa's right arm, the plug on top of the arm
    begin()
    rbox(400 / S, 200 / S, 140 / S, 0, 0, 70 / S, ENAMEL, 0.012, 2)
    for s in (-1, 1):
        rbox(0.03, 0.004, 0.014, 200 / S + 0.013, s * 0.022, 70 / S, CHROME, 0.001, 1)
    yf = -1000 / S + 0.008
    xa, zt = -0.105, 1350 / S + 0.007
    path = [(0, -0.05, 0.03), (0.0, -0.12, 0.008), (-0.05, -0.30, 0.007), (-0.10, -0.46, 0.007), (xa, yf + 0.006, 0.03), (xa, yf, 0.20), (xa, yf, 0.45), (xa, yf, 0.63),
            (xa, yf - 0.010, 0.684), (xa, yf - 0.034, zt - 0.009), (xa, yf - 0.075, zt), (xa, yf - 0.16, zt), (xa, yf - 0.24, zt)]
    tube(smooth(path, 3), 0.006, ENAMEL, 6)
    rbox(0.018, 0.05, 0.012, xa, yf - 0.26, zt, ENAMEL, 0.004, 1)
    rbox(0.012, 0.014, 0.005, xa, yf - 0.29, zt, CHROME, 0.001, 1)
    end("SM_L_Charger", ZN, kind="prop", origin=(0, 0, 0), replaces=rep(CL, "charger box 400 x 200 x 140 uu + cable 16 x 1600 x 16 uu", (400 / S, 200 / S, 140 / S)),
        notes="Origin = centre of the brick on the floor. The cable is fitted to the sofa at its game position: up the front face of the right arm "
              "and along its top (the code cable ended in mid-air next to the arm). Cable 12 mm thick.")
    # grandma's ball of yarn and the thread up to her lap
    begin()
    r = 220 / S
    ball(r, 0, 0, r, YARN, seg=16, rings=10)
    rnd = random.Random(3)
    for i in range(6):
        torus(r * 0.985, 0.006, YARN, (0, 0, r), rot=(rnd.uniform(0, 3.14), rnd.uniform(0, 3.14), rnd.uniform(0, 3.14)), seg=20, rseg=5)
    tube(smooth([(r * 0.5, 0.03, 0.006), (r * 1.3, 0.06, 0.005), (r * 1.9, -0.02, 0.005), (r * 2.3, 0.07, 0.005), (r * 2.2, 0.16, 0.005)], 3), 0.005, YARN, 6)
    end("SM_L_YarnBall", ZN, kind="prop", origin=(0, 0, 0), replaces=rep(CL, "yarn ball r220", (2 * r, 2 * r, 2 * r)),
        notes="Origin under the centre of the ball (a loose end trails on the floor).")
    begin()
    A = Vector((0, 0, 440 / S))
    B = A + Vector((-930 / S, 500 / S, 780 / S))
    tube(smooth([A, (A + B) / 2 - Vector((0, 0, 0.03)), B], 4), 0.005, YARN, 6)
    end("SM_L_YarnThread", ZN, kind="prop", origin=(0, 0, 0), parent="SM_L_YarnBall",
        replaces=rep(CL, "yarn thread cylinder d14 from the ball top to the armchair (9670,-5500,1220)", (930 / S, 500 / S, 780 / S)),
        notes="Runs from the top of the yarn ball to the armchair seat (grandma's lap). Only valid for the ball and armchair at their game positions, ball yaw 0.")
    # dust under the sofa
    begin()
    DUST = mat("Dust", (0.55, 0.53, 0.50), 0.95)
    ico(1.0, 0, 0, 0.016, DUST, (80 / S, 60 / S, 30 / S), 2)
    ico(1.0, 0.03, 0.02, 0.022, DUST, (0.03, 0.035, 0.018), 1, rot=(0.3, 0.2, 0.5))
    ico(1.0, -0.035, -0.01, 0.02, DUST, (0.028, 0.022, 0.016), 1, rot=(0.1, 0.4, 1.5))
    end("SM_L_DustBunny", ZN, kind="prop", origin=(0, 0, 0), replaces=rep(LR, "dust ellipsoids 160 x 120 x 60 uu under the sofa (x5)", (160 / S, 120 / S, 60 / S)))
    # knitting basket (new)
    begin()
    Rb, Rt, Hh, t = 0.13, 0.165, 0.20, 0.012
    prof = [(Rb * 0.9, 0)]
    for i in range(9):
        prof.append((Rb + (Rt - Rb) * i / 8 + (0.004 if i % 2 else 0.0), 0.006 + (Hh - 0.006) * i / 8))
    prof += [(Rt + 0.006, Hh), (Rt + 0.006, Hh + 0.012), (Rt - t, Hh + 0.012), (Rb - t + 0.004, 0.014)]
    lathe(prof, WICKER, 18)
    for s in (-1, 1):
        x = s * (Rt + 0.004)
        tube(smooth([(x, -0.055, Hh), (x + s * 0.012, -0.035, Hh + 0.04), (x + s * 0.014, 0, Hh + 0.052), (x + s * 0.012, 0.035, Hh + 0.04), (x, 0.055, Hh)], 2), 0.007, WICKER, 6)
    ball(0.115, 0, 0, 0.075, PAPER, (1, 1, 0.42), 12, 6)
    ball(0.06, -0.045, 0.02, 0.15, TEAL, seg=12, rings=8)
    ball(0.055, 0.055, -0.03, 0.145, FABRIC, seg=12, rings=8)
    ball(0.05, 0.02, 0.07, 0.16, YARN, seg=12, rings=8)
    for (ex, ey) in ((0.45, 0.25), (0.38, -0.3)):
        e = Euler((ex, ey, 0), "XYZ")
        d = e.to_matrix() @ Vector((0, 0, 1))
        c = Vector((0.0, -0.02, 0.24))
        cyl(0.004, 0.34, c.x, c.y, c.z, STEEL, verts=6, rot=e)
        tip = c + d * 0.17
        ball(0.009, tip.x, tip.y, tip.z, BRASS, seg=8, rings=6)
    end("SM_L_KnittingBasket", ZN, kind="prop", notes="New (not in the code): wicker basket with yarn and needles next to the armchair. Hollow, 12 mm wall.")

# ---------------------------------------------------------------- door, skirting, ceiling lamp, pets' things
def build_door():
    Wd, jt, Dp, Hh = 1600 / S, 40 / S, 280 / S, 3800 / S
    hw = Wd / 2
    lt = 0.040
    ys = -Dp / 2 + lt
    begin()
    for s in (-1, 1):
        bx(s * hw, s * (hw + jt), -Dp / 2, Dp / 2, 0, Hh + jt, BUTCHER, 0.004, 1)
        bx(s * hw, s * (hw - 0.012), ys, ys + 0.012, 0, Hh, BUTCHER, 0.002, 1)
    bx(-hw - jt, hw + jt, -Dp / 2, Dp / 2, Hh, Hh + jt, BUTCHER, 0.004, 1)
    bx(-hw, hw, ys, ys + 0.012, Hh - 0.012, Hh, BUTCHER, 0.002, 1)
    end("SM_L_DoorFrame", ZN, replaces=rep(SH, "door frame: 2 jambs 40 x 280 x 3840 uu + head (doorway 1600 x 3800 uu)", (Wd + 2 * jt, Dp, Hh + jt)),
        notes="Lining only, same size as the code frame; a 12 mm stop bead inside. No threshold (the floor runs through).")
    begin()
    x0, x1, y0, y1, z0, z1 = -hw + 0.004, hw - 0.004, -Dp / 2, ys, 0.012, Hh - 0.004
    bx(x0, x1, y0 + 0.006, y1 - 0.006, z0, z1, CABINET_D, 0.003, 1)
    st = 0.11
    for (ya, yb) in ((y0, y0 + 0.008), (y1 - 0.008, y1)):
        bx(x0, x0 + st, ya, yb, z0, z1, CABINET, 0.003, 1)
        bx(x1 - st, x1, ya, yb, z0, z1, CABINET, 0.003, 1)
        for (za, zb) in ((z0, z0 + 0.20), (0.92, 1.08), (z1 - 0.13, z1)):
            bx(x0 + st - 0.002, x1 - st + 0.002, ya, yb, za, zb, CABINET, 0.003, 1)
    hx, hz = x1 - 0.06, 1.0
    for s, yy in ((-1, y0), (1, y1)):
        cyl(0.026, 0.008, hx, yy + s * 0.004, hz, BRASS, "Y", 12)
        cyl(0.009, 0.045, hx, yy + s * 0.022, hz, BRASS, "Y", 8)
        rbox(0.11, 0.014, 0.016, hx - 0.045, yy + s * 0.045, hz, BRASS, 0.005, 2)
        cyl(0.007, 0.004, hx, yy + s * 0.002, hz - 0.08, PLINTH, "Y", 8)
    ax, ay = x0, y0 - 0.006
    for z in (0.20, 1.0, 1.80):
        cyl(0.008, 0.09, ax, ay, z, BRASS, verts=8)
    end("SM_L_Door_Leaf", ZN, kind="part", origin=(ax, ay, z0), parent="SM_L_DoorFrame",
        motion=hinge((0, 0, 1), -165, "model -Y (the front of the frame), folding back against the wall on the hinge side (model -X)"), preview=1.0,
        notes="New (the code has an empty doorway). Built closed; 12 mm gap under the leaf for ants. Hinge axis in front of the frame face so it folds back flat.")
    begin()
    b = 15 / S
    prism([(b, 0), (-b, 0), (-b, 0.085), (-b + 0.004, 0.095), (-b + 0.004, 0.108), (0.0, 220 / S), (b, 220 / S)], -0.5, 0.5, ENAMEL, plane="YZ", bev=0.002, seg=1)
    end("SM_L_Skirting", ZN, replaces=rep(LR + " / " + SH, "baseboard boxes 30 x 220 uu, any length", (1.0, 30 / S, 220 / S)),
        notes="1 m module (1900 uu at game scale); scale along X or tile. Back face (+Y) goes against the wall.")
    begin()
    R, Hh = 350 / S, 250 / S
    lathe([(R * 0.22, Hh - 0.012), (R * 0.58, Hh * 0.86), (R * 0.9, Hh * 0.5), (R - 0.01, 0), (R, 0), (R * 0.93 + 0.004, Hh * 0.5 + 0.004), (R * 0.62, Hh * 0.9), (R * 0.26, Hh)], SHADE, 20)
    cyl(0.008, Hh * 0.5, 0, 0, Hh * 0.73, IRON, verts=6)
    ball(0.04, 0, 0, Hh * 0.42, BULB, seg=12, rings=8)
    end("SM_L_CeilingLamp", ZN, replaces=rep(LR, "ceiling lamp shade cylinder r350 h250 (x2)", (2 * R, 2 * R, Hh)),
        notes="Origin at the BOTTOM centre (the rule for static pieces); the top touches the ceiling.")
    begin()
    torus(0.36, 0.085, RED, (0, 0, 0.085), seg=20, rseg=8, scale=(1, 0.78, 1))
    lathe([(0.36, 0), (0.38, 0.02), (0.38, 0.05), (0.30, 0.075), (0.10, 0.085)], PAPER, 20, scale=(1, 0.78, 1))
    cyl(0.04, 0.004, 0, -0.02, 0.086, WALNUT, verts=10)
    for a in (-0.7, 0.0, 0.7):
        cyl(0.016, 0.004, 0.062 * math.sin(a), -0.02 + 0.062 * math.cos(a), 0.086, WALNUT, verts=8)
    end("SM_L_DogBed", ZN, notes="New (not in the code): oval dog bed, 0.89 x 0.69 x 0.17 m, soft bolster around a cushion.")
    begin()
    bx(-0.22, 0.22, -0.22, 0.22, 0, 0.04, CABINET, 0.012, 2)
    prof = [(0.048, 0.04)]
    for i in range(29):
        prof.append((0.05 + (0.006 if i % 2 else 0.0), 0.04 + 0.60 * i / 28))
    prof.append((0.048, 0.64))
    lathe(prof, SISAL, 14)
    bx(-0.17, 0.17, -0.17, 0.17, 0.64, 0.675, CABINET, 0.012, 2)
    bx(-0.15, 0.15, -0.15, 0.15, 0.672, 0.69, CUSHION, 0.007, 2)
    tube([(0.165, -0.10, 0.655), (0.19, -0.10, 0.62), (0.19, -0.10, 0.45)], 0.004, SISAL, 6)
    ball(0.03, 0.19, -0.10, 0.42, RED, seg=12, rings=8)
    end("SM_L_CatScratchPost", ZN, notes="New (not in the code): scratching post 0.44 x 0.44 x 0.69 m, rope-wound post, platform, a ball on a string.")

def build_living():
    build_sofa()
    build_coffee_table()
    build_tv()
    build_bookshelf()
    build_armchair()
    build_floor_lamp()
    build_plant()
    build_radiator()
    build_carpet()
    build_small_things()
    build_door()

def layout_living():
    # Everything where the game has it (Unreal coordinates of the origins).
    wall, par = smat("Wall", (0.96, 0.80, 0.52), 0.9), smat("Parquet", (0.74, 0.55, 0.36), 0.45)
    grain(par, 3.0)
    st = {}
    st["floor"] = stage_box("L_floor", (7000, -9200, -100), (20400, 1700, 0), par)
    st["N"] = stage_box("L_wallN", (7200, 1500, 0), (20400, 1700, 5400), wall)
    st["E"] = stage_box("L_wallE", (20200, -9000, 0), (20400, 1500, 5400), wall)
    st["S"] = stage_box("L_wallS", (7000, -9200, 0), (20400, -9000, 5400), wall)
    st["W1"] = stage_box("L_wallW1", (7000, -9000, 0), (7200, -4800, 5400), wall)
    st["W2"] = stage_box("L_wallW2", (7000, -3200, 0), (7200, 1700, 5400), wall)
    st["W3"] = stage_box("L_wallW3", (7000, -4800, 3800), (7200, -3200, 5400), wall)
    spawn("SM_L_Carpet", 14000, -4050, 0, 0)
    spawn("SM_L_Sofa", 13500, 550, 0, 180)
    spawn("SM_L_CoffeeTable", 13750, -3400, 0, 0)
    spawn("SM_L_TVStand", 13750, -8350, 0, 0)
    spawn("SM_L_Bookshelf", 19700, -4500, 0, 90)
    spawn("SM_L_Armchair", 9150, -5800, 0, -90)
    spawn("SM_L_FloorLamp", 17800, 700, 0, 0)
    spawn("SM_L_PottedPlant", 18900, 900, 0, 0)
    spawn("SM_L_Radiator", 8660, 1400, 300, 180)
    spawn("SM_L_Sock", 9000, -2000, 0, 30)
    spawn("SM_L_Coin", 11500, -5500, 0, 0)
    spawn("SM_L_Charger", 17200, -1300, 0, 0)
    spawn("SM_L_YarnBall", 10600, -5000, 0, 0)
    spawn("SM_L_KnittingBasket", 10250, -4450, 0, 0, note="free choice, next to the armchair")
    spawn("SM_L_DoorFrame", 7100, -4000, 0, -90, note="doorway kitchen -> living room; hinge on the north jamb, opens into the living room")
    spawn("SM_L_CeilingLamp", 11000, -4000, 5150, 0)
    spawn("SM_L_CeilingLamp", 16500, -4000, 5150, 0)
    spawn("SM_L_DogBed", 17800, -8000, 0, 20, note="free choice: between the TV stand and the bookshelf")
    spawn("SM_L_CatScratchPost", 8000, -8300, 0, 0, note="free choice: the south-west corner")
    for k in range(7):
        spawn("SM_L_Skirting", 8150 + 1900 * k, 1485, 0, 180, record=(k == 0), note="north wall run x 7200..20200: one module per 1900 uu")
    for i in range(5):
        spawn("SM_L_DustBunny", 11000 + i * 1200, 600 + (i % 2) * 300, 0, i * 70)
    for k in (0, 1, 5):
        spawn("SM_L_Skirting", 8150 + 1900 * k, -8985, 0, 0, record=False)     # none behind the TV stand: its back is on the wall plane
    for k in (0, 4):
        spawn("SM_L_Skirting", 20185, -8050 + 1900 * k, 0, 90, record=False)    # none behind the bookshelf
    return st
