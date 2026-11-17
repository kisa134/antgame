# Ant Kitchen pieces, part 1: the north run - base cabinets, drawers, under-sink, worktops, sink, faucet, splashback, wall cabinets.
# All numbers written in game units (as in KitchenBuilder.cpp) and converted by gbox/G; styling numbers in metres.
import bmesh, math
from mathutils import Vector, Matrix
import kitchen_lib as L
from kitchen_lib import *

FRONT, BACK, PLZ, TOP, WT = 3040.0, 4750.0, 120.0, 1650.0, 1710.0
T, DB, GAP = 36.0, 3036.0, 8.0     # panel thickness, door back face, gap between doors

def hinge(angle, note):
    return {"type": "hinge", "axis": (0, 0, 1), "angle_deg": angle, "note": note}

def door(name, parent, Xa, Xb, Z0, Z1, side, handle="top", yb=DB, state=None, angle=110):
    # side "E": hinged on the game-east edge (viewer's left when facing the front), "W": on the west edge.
    shaker(-Xb * K, -Xa * K, Z0 * K, Z1 * K, yb * K, handle)
    Xh = Xb if side == "E" else Xa
    piece(name, origin=G(Xh, yb, Z0), parent=parent, state=state,
          motion=hinge(-angle if side == "E" else angle, "vertical hinge on the game-%s edge, swings out towards the front (-Y)" % ("east" if side == "E" else "west")))

def carcass(X0, X1, shelf=True, back=True):
    gbox(X0, X0 + T, FRONT, BACK, PLZ, TOP, "Cabinet", 0.003, 1)
    gbox(X1 - T, X1, FRONT, BACK, PLZ, TOP, "Cabinet", 0.003, 1)
    gbox(X0 + T, X1 - T, FRONT, BACK, PLZ, 140, "CabinetInset", 0.002, 1)
    if back:
        gbox(X0 + T, X1 - T, BACK - 24, BACK, 140, TOP, "CabinetInset", 0.0)
    gbox(X0 + T, X1 - T, FRONT, FRONT + 160, TOP - T, TOP, "Cabinet", 0.002, 1)
    gbox(X0 + T, X1 - T, BACK - 200, BACK - 24, TOP - T, TOP, "Cabinet", 0.002, 1)
    if shelf:
        gbox(X0 + T, X1 - T, FRONT + 70, BACK - 24, 860, 890, "CabinetInset", 0.002, 1)
    gbox(X0, X1, 3100, BACK, 0, PLZ, "Plinth", 0.003, 1)

def base2(name, X0, X1, rep):
    carcass(X0, X1)
    piece(name, replaces=rep, note="carcass: sides, bottom, back, top rails, one shelf, plinth block; open front, 25 uu slot under the doors")
    Xm = (X0 + X1) / 2
    door(name + "_DoorL", name, Xm + GAP / 2, X1 - GAP, 165, 1600, "E")
    door(name + "_DoorR", name, X0 + GAP, Xm - GAP / 2, 165, 1600, "W")

def drawers(name, X0, X1, rep):
    carcass(X0, X1, shelf=False)
    piece(name, replaces=rep, note="drawer carcass; Drawer1 (top) matches the game's AKitchenDoor Drawer gadget (front 1300 x 270, tray 1600 deep) but is centred at X=-4900, not -4950")
    for i, (z0, z1, th) in enumerate(((1330, 1600, 240), (760, 1310, 430), (180, 740, 450)), 1):
        shaker(-(X1 - GAP) * K, -(X0 + GAP) * K, z0 * K, z1 * K, DB * K, "mid")
        tx0, tx1 = -(X1 - T - 12) * K, -(X0 + T + 12) * K
        tray(tx0, tx1, DB * K, (DB + 1600) * K, (z0 + 10) * K, (z0 + 10 + th) * K, 20 * K, "Butcher", 0.002, 1, open_sides="y-")
        piece("%s_Drawer%d" % (name, i), origin="back", parent=name,
              motion={"type": "slide", "vector_m": (0, -1100 * K, 0), "note": "pulls out towards the front; the game uses 1100 uu"})

def build_base():
    base2("SM_K_Base1400", -7000, -5600, [("BuildNorthRun", "west block, west half (2 of 4 decorative doors, fake drawer front)", (-7000, 3000, 0), (-5600, 4750, 1650))])
    drawers("SM_K_BaseDrawers", -5600, -4200, [("BuildNorthRun", "west block, east half with the drawer cavity", (-5600, 3000, 0), (-4200, 4750, 1650))])
    base2("SM_K_Base1300", -2800, -1500, [("BuildNorthRun", "cabinet left of the oven + Doors(-2800,-1500)", (-2800, 3000, 0), (-1500, 4750, 1650))])
    base2("SM_K_Base1200", 1300, 2500, [("BuildNorthRun", "right block + Doors(1300,2500)", (1300, 3000, 0), (2500, 4750, 1650))])

    # ---- under-sink cabinet: no shelf, back panel with the hole into the corridor, fixed fascia above the doors
    X0, X1 = -4200, -2800
    gbox(X0, X0 + T, FRONT, BACK, PLZ, TOP, "Cabinet", 0.003, 1)
    gbox(X1 - T, X1, FRONT, BACK, PLZ, TOP, "Cabinet", 0.003, 1)
    gbox(X0 + T, X1 - T, FRONT, BACK, PLZ, 140, "CabinetInset", 0.002, 1)
    gbox(X0 + T, -3600, BACK - 24, BACK, 140, TOP, "CabinetInset", 0.0)
    gbox(-3400, X1 - T, BACK - 24, BACK, 140, TOP, "CabinetInset", 0.0)
    gbox(-3600, -3400, BACK - 24, BACK, 330, TOP, "CabinetInset", 0.0)
    gbox(X0 + T, X1 - T, FRONT, FRONT + 40, 1104, TOP, "Cabinet", 0.002, 1)
    shaker(-(X1 - GAP) * K, -(X0 + GAP) * K, 1112 * K, 1600 * K, DB * K, None)
    gbox(X0, X1, 3100, BACK, 0, PLZ, "Plinth", 0.003, 1)
    piece("SM_K_BaseSink", replaces=[("BuildNorthRun", "hollow under-sink cabinet (floor, sides, back panel with a hole x -3600..-3400 z 140..330)", (-4200, 3000, 0), (-2800, 4750, 1650))],
          note="floor top at z=140 as in the game; the basin (SM_K_Sink) hangs in the top; fixed fascia z 1112..1600")
    door("SM_K_BaseSink_DoorL", "SM_K_BaseSink", -3500 + GAP / 2, X1 - GAP, 165, 1100, "E", state="open (the game leaves this east door hanging open at ~110 deg)")
    door("SM_K_BaseSink_DoorR", "SM_K_BaseSink", X0 + GAP, -3500 - GAP / 2, 165, 1100, "W", state="closed")

    # P-trap: tailpiece from the basin drain, U-bend, arm into the back panel.
    pth = [(-3500, 4000, 1100), (-3500, 4000, 760)]
    for i in range(1, 9):
        a = math.pi * i / 8
        pth.append((-3500, 4090 - 90 * math.cos(a), 760 - 90 * math.sin(a)))
    pth += [(-3500, 4180, 800), (-3500, 4195, 850), (-3500, 4240, 885), (-3500, 4300, 895), (-3500, 4740, 895)]
    tube([G(*p) for p in pth], 45 * K, "Enamel", 10)
    for (y, z, ax) in ((4000, 1060, "Z"), (4000, 770, "Z"), (4180, 790, "Z"), (4330, 895, "Y")):
        cyl(58 * K, 50 * K, 3500 * K, y * K, z * K, "Enamel", ax, 12, 0.003, seg=1)
    gcyl(-3500, 4090, 640, 30, 30, "Enamel", 8)
    piece("SM_K_SinkTrap", on="SM_K_BaseSink", replaces=[("BuildNorthRun", "drain pipe + U-trap cylinders", (-3545, 3955, 640), (-3455, 4720, 1100))])
    # Flexible hose: from a spigot on the trap, across the floor, out through the hole in the back panel.
    hp = spline([G(-3500, 4040, 800), G(-3560, 3900, 700), G(-3700, 3760, 420), G(-3850, 3900, 190), G(-3780, 4250, 172), G(-3560, 4500, 172), G(-3500, 4700, 180), G(-3500, 4800, 180)], 5)
    tube(hp, 30 * K, "Plinth", 8)
    piece("SM_K_SinkHose", on="SM_K_BaseSink", note="lies on the cabinet floor and leaves through the hole in the back panel into the corridor")
    # Detergent bottle and sponge.
    lathe([(0, 0), (150 * K, 0), (160 * K, 20 * K), (160 * K, 480 * K), (120 * K, 600 * K), (62 * K, 650 * K), (62 * K, 668 * K), (0, 668 * K)], "PlasticBlue", n=14)
    cyl(66 * K, 110 * K, 0, 0, 715 * K, "Enamel", "Z", 12, 0.004, seg=1)
    box(-164 * K, 164 * K, -0.004, 0.004, 150 * K, 420 * K, "Paper", 0.0)
    box(-0.004, 0.004, -164 * K, 164 * K, 150 * K, 420 * K, "Paper", 0.0)
    piece("SM_K_Detergent", at=[(*G(-3900, 3500, 140), 20)], on="SM_K_BaseSink", replaces=[("BuildNorthRun", "detergent bottle Cyl r160 h650 + cap", (-4060, 3340, 140), (-3740, 3660, 910))])
    ubox(-150, 150, -150, 150, 0, 120, "Sponge", 0.012, 2)
    ubox(-150, 150, -150, 150, 118, 150, "Leaf", 0.008, 2)
    piece("SM_K_Sponge", at=[(*G(-3150, 4250, 140), 0)], on="SM_K_BaseSink", replaces=[("BuildNorthRun", "sponge", (-3300, 4100, 140), (-3000, 4400, 290))])

    # ---- worktops (butcher block), the long one with the sink cut-out
    gridsolid([1500 * K, 2884 * K, 4116 * K, 7000 * K], [2950 * K, 3284 * K, 4716 * K, 5000 * K], TOP * K, WT * K, {(1, 1)}, "Butcher", 0.008, 2)
    piece("SM_K_WorktopA", replaces=[("BuildNorthRun", "countertop -7000..-1500 incl. the frame round the sink", (-7000, 2950, 1650), (-1500, 5000, 1710))],
          note="cut-out X -4116..-2884, Y 3284..4716 (the basin's outer size); covers the corridor behind the cabinets up to the wall")
    gbox(0, 2500, 2950, 5000, TOP, WT, "Butcher", 0.008, 2)
    piece("SM_K_WorktopB", replaces=[("BuildNorthRun", "countertop 0..2500 (the game's slab also runs under the stove top -1500..0)", (0, 2950, 1650), (2500, 5000, 1710))])

    # ---- sink basin: inner cavity X -4100..-2900, Y 3300..4700, floor top z 1150 (as in the game)
    gbox(-4116, -2884, 3284, 4716, 1100, 1150, "Steel", 0.003, 1)
    gbox(-4116, -4100, 3284, 4716, 1150, 1712, "Steel", 0.002, 1)
    gbox(-2900, -2884, 3284, 4716, 1150, 1712, "Steel", 0.002, 1)
    gbox(-4100, -2900, 3284, 3300, 1150, 1712, "Steel", 0.002, 1)
    gbox(-4100, -2900, 4700, 4716, 1150, 1712, "Steel", 0.002, 1)
    gridsolid([2850 * K, 2900 * K, 4100 * K, 4150 * K], [3250 * K, 3300 * K, 4700 * K, 4750 * K], 1708 * K, 1722 * K, {(1, 1)}, "Steel", 0.002, 1)
    gcyl(-3500, 4000, 1150, 78, 8, "Chrome", 14)
    gcyl(-3500, 4000, 1150, 50, 11, "Iron", 12)
    piece("SM_K_Sink", replaces=[("BuildNorthRun", "sink basin (bottom + four thick walls); inner cavity kept identical", (-4200, 3000, 1100), (-2800, 5000, 1650))],
          note="16 uu steel walls instead of the game's 100 uu blocks; the drain is a ring + dark disc (the passage is a teleport)")

    # ---- faucet: swan neck, tip over the basin at (-3500, 4400, 2060) where the game's stream starts
    gcyl(-3500, 4850, WT, 62, 36, "Chrome", 14, 0.003)
    nk = spline([G(-3500, 4850, 1730), G(-3500, 4850, 2030), G(-3500, 4830, 2200), G(-3500, 4740, 2300), G(-3500, 4620, 2330), G(-3500, 4500, 2290), G(-3500, 4420, 2190), G(-3500, 4400, 2080)], 4)
    tube(nk, 34 * K, "Chrome", 10)
    gcyl(-3500, 4400, 2056, 40, 44, "Chrome", 12, 0.002)
    piece("SM_K_Faucet", replaces=[("BuildNorthRun", "faucet riser + arm + nozzle", (-3560, 4350, 1710), (-3440, 4910, 2240))])
    for nm, X, dot in (("Hot", -3750, "Red"), ("Cold", -3250, "Blue")):
        lathe([(0, 0), (50 * K, 0), (50 * K, 30 * K), (34 * K, 50 * K), (30 * K, 100 * K), (40 * K, 112 * K), (40 * K, 126 * K), (0, 126 * K)], "Chrome", n=12)
        box(-13 * K, 13 * K, -110 * K, 0, 96 * K, 118 * K, "Chrome", 0.003, 1)
        cyl(16 * K, 8 * K, 0, 0, 129 * K, dot, "Z", 10)
        piece("SM_K_Faucet_Tap" + nm, at=[(*G(X, 4800, WT), 0)], parent=None, on="SM_K_Faucet",
              motion={"type": "hinge", "axis": (0, 0, 1), "angle_deg": 90, "note": "tap turns about its vertical axis (origin on the axis); lever points to the front when closed"},
              replaces=[("BuildNorthRun", "tap cylinder r50 h120", (X - 50, 4750, 1710), (X + 50, 4850, 1830))])

    # ---- tiled splashback: one closed mesh, 40 x 5 raised tiles on a grout slab
    nx, nz = 40, 5
    x0, x1, z0, z1 = -2500 * K, 7000 * K, WT * K, 2900 * K
    yg, yt, yb = 4982 * K, 4970 * K, 5000 * K
    bm = bmesh.new()
    vs = [[bm.verts.new((x0 + (x1 - x0) * i / nx, yg, z0 + (z1 - z0) * j / nz)) for j in range(nz + 1)] for i in range(nx + 1)]
    cells = [bm.faces.new((vs[i][j], vs[i + 1][j], vs[i + 1][j + 1], vs[i][j + 1])) for i in range(nx) for j in range(nz)]
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    if cells[0].normal.y > 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
    gi, ti = L._slot("Grout"), L._slot("Tile")
    for f in bm.faces:
        f.material_index = gi
    bmesh.ops.inset_individual(bm, faces=cells, thickness=0.004, depth=0.0, use_even_offset=True)
    r2 = bmesh.ops.inset_individual(bm, faces=cells, thickness=0.004, depth=yg - yt, use_even_offset=True)
    for f in cells + list(r2["faces"]):
        f.material_index = ti
    be = [e for e in bm.edges if len(e.link_faces) == 1]
    ex = bmesh.ops.extrude_edge_only(bm, edges=be)
    nv = [g for g in ex["geom"] if isinstance(g, bmesh.types.BMVert)]
    ne = [g for g in ex["geom"] if isinstance(g, bmesh.types.BMEdge)]
    for v in nv:
        v.co.y = yb
    bmesh.ops.holes_fill(bm, edges=ne, sides=0)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    add(bm)
    bm.free()
    piece("SM_K_Splashback", replaces=[("Build", "backsplash", (-7000, 4970, 1710), (2500, 5000, 2900))], note="tiles 237.5 x 238 uu (125 mm), 12 uu proud of the grout")

def wallcab(name, X0, X1, at, rep):
    Y0, Y1, Z0, Z1 = 4340, 5000, 3000, 4340
    gbox(X0, X0 + T, Y0, Y1, Z0, Z1, "Cabinet", 0.003, 1)
    gbox(X1 - T, X1, Y0, Y1, Z0, Z1, "Cabinet", 0.003, 1)
    gbox(X0 + T, X1 - T, Y0, Y1, Z0, Z0 + T, "Cabinet", 0.002, 1)
    gbox(X0 + T, X1 - T, Y0, Y1, Z1 - T, Z1, "Cabinet", 0.002, 1)
    gbox(X0 + T, X1 - T, Y1 - 24, Y1, Z0 + T, Z1 - T, "CabinetInset", 0.0)
    for z in (3440, 3880):
        gbox(X0 + T, X1 - T, Y0 + 40, Y1 - 24, z, z + 30, "CabinetInset", 0.002, 1)
    gbox(X0, X1, 4280, 5000, Z1, 4400, "Cabinet", 0.012, 2)
    piece(name, at=at, replaces=rep, note="carcass with back, two shelves, crown moulding; doors are separate")
    Xm = (X0 + X1) / 2
    door(name + "_DoorL", name, Xm + GAP / 2, X1 - GAP, 3010, 4330, "E", "bottom", yb=4336)
    door(name + "_DoorR", name, X0 + GAP, Xm - GAP / 2, 3010, 4330, "W", "bottom", yb=4336)

def build_wall():
    wallcab("SM_K_WallCab1250", -625, 625, [(-Xc * K, 0, 0, 0) for Xc in (-6375, -5125, -3875, -2625)],
            [("Build", "upper cabinets, west box (four 1250-wide modules replace it)", (-7000, 4300, 3000), (-2000, 5000, 4400))])
    wallcab("SM_K_WallCab1500", 1000, 2500, None, [("Build", "upper cabinet, east box", (1000, 4300, 3000), (2500, 5000, 4400))])