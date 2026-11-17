# Turns SourceArt/Blender/<set>/<set>_manifest.json into C++ tables for the game:
#   python Tools/gen_kitchen_art.py            -> Source/AntKitchen/KitchenArtData.inl   (kitchen)
#   python Tools/gen_kitchen_art.py --out X    -> writes to X instead (dry run / review)
# The game builds the level from code, so the manifest's placements are compiled in:
#   GKitchenArt[]    every placed instance (mesh name, origin, where the mesh axes point, scale, role)
#   GKitchenMovers[] every part that opens through the generic AKitchenMover (hinge / slide, limits, prompts)
# Roles: 0 static (placed by AKitchenBuilder), 1 generic mover, 2 built by an existing gadget actor
#        (AKitchenDoor, AKitchenKnob, ...), 3 rides a mover (Parent = index into GKitchenMovers).
# Plain python 3, no Unreal needed.
import json, os, sys, math

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "SourceArt", "Blender", "kitchen", "kitchen_manifest.json")
OUT = os.path.join(ROOT, "Source", "AntKitchen", "KitchenArtData.inl")
if "--out" in sys.argv:
    OUT = sys.argv[sys.argv.index("--out") + 1]

M = json.load(open(SRC, encoding="utf-8"))
P = {p["name"]: p for p in M["pieces"]}

# Moving parts that existing gadget classes already drive (they build the mesh themselves), as (piece, instance or None = all).
GADGET = [
    ("SM_K_Fridge_DoorMain", None), ("SM_K_Range_OvenDoor", None), ("SM_K_BaseDrawers_Drawer1", None),
    ("SM_K_Dishwasher_Door", None), ("SM_K_Microwave_Door", None),
    ("SM_K_Range_Knob", 2), ("SM_K_Range_Burner", 1),            # the stove gadget's knob and coil
    ("SM_K_Toaster_Lever", None), ("SM_K_Toast", None),
    ("SM_K_Bin_Lid", None), ("SM_K_Bin_Pedal", None),
    ("SM_K_Switch_Toggle", None), ("SM_K_MouseTrap_Bar", None),
    ("SM_K_Faucet_TapHot", None), ("SM_K_Faucet_TapCold", None),
]
# Generic movers: piece -> (open prompt, close prompt, flags, auto-close seconds)
F_DEFAULT_OPEN, F_NEEDS_FRIDGE, F_NEEDS_DISHWASHER, F_HUMAN_SHUTS, F_HUMAN_OPENS = 1, 2, 4, 8, 16
DOOR = ("Открыть дверцу шкафа", "Закрыть дверцу шкафа")
MOVERS = {
    "SM_K_Base1400_DoorL": DOOR + (F_HUMAN_SHUTS | F_HUMAN_OPENS, 0), "SM_K_Base1400_DoorR": DOOR + (F_HUMAN_SHUTS | F_HUMAN_OPENS, 0),
    "SM_K_Base1300_DoorL": DOOR + (F_HUMAN_SHUTS | F_HUMAN_OPENS, 0), "SM_K_Base1300_DoorR": DOOR + (F_HUMAN_SHUTS | F_HUMAN_OPENS, 0),
    "SM_K_Base1200_DoorL": DOOR + (F_HUMAN_SHUTS | F_HUMAN_OPENS, 0), "SM_K_Base1200_DoorR": DOOR + (F_HUMAN_SHUTS | F_HUMAN_OPENS, 0),
    "SM_K_BaseSink_DoorL": ("Открыть дверцу под мойкой", "Закрыть дверцу под мойкой", F_DEFAULT_OPEN, 0),
    "SM_K_BaseSink_DoorR": ("Открыть дверцу под мойкой", "Закрыть дверцу под мойкой", F_HUMAN_SHUTS, 0),
    "SM_K_BaseDrawers_Drawer2": ("Выдвинуть ящик", "Задвинуть ящик", F_HUMAN_SHUTS | F_HUMAN_OPENS, 0),
    "SM_K_BaseDrawers_Drawer3": ("Выдвинуть нижний ящик (мешает скалка)", "Задвинуть нижний ящик", F_HUMAN_SHUTS, 0),
    "SM_K_WallCab1250_DoorL": ("Открыть дверцу верхнего шкафа", "Закрыть дверцу верхнего шкафа", 0, 0),
    "SM_K_WallCab1250_DoorR": ("Открыть дверцу верхнего шкафа", "Закрыть дверцу верхнего шкафа", 0, 0),
    "SM_K_WallCab1500_DoorL": ("Открыть дверцу верхнего шкафа", "Закрыть дверцу верхнего шкафа", 0, 0),
    "SM_K_WallCab1500_DoorR": ("Открыть дверцу верхнего шкафа", "Закрыть дверцу верхнего шкафа", 0, 0),
    "SM_K_Fridge_DoorFreezer": ("Открыть морозилку", "Закрыть морозилку", 0, 15),
    "SM_K_Fridge_Crisper": ("Выдвинуть ящик для овощей", "Задвинуть ящик для овощей", F_NEEDS_FRIDGE, 0),
    "SM_K_Dishwasher_Rack": ("Выкатить корзину посудомойки", "Закатить корзину посудомойки", F_NEEDS_DISHWASHER, 0),
    "SM_K_Matchbox_Tray": ("Выдвинуть спичечный коробок", "Задвинуть спичечный коробок", F_DEFAULT_OPEN, 0),
}
# Placement changes against the manifest, each for a gameplay reason (see docs/ART_INTEGRATION.md).
MOVE = {
    ("SM_K_Pot_Lid", 0): (-4900.0, 4400.0, 1710.0),   # the game's pot is an open pot of soup: the lid lies on the worktop
    ("SM_K_Mug", 0): (-2450.0, 3650.0, 1710.0),       # the manifest spot is inside the (unreplaced) cereal box on the counter
}
SLIDE_SCALE = {
    "SM_K_BaseDrawers_Drawer3": 300.0 / 1100.0,       # the rolling pin on the floor lies in its sweep
    "SM_K_Matchbox_Tray": 200.0 / 300.0,              # "half open: 200 uu out (as the game shows it)"
}

def is_gadget(name, idx):
    return any(n == name and (i is None or i == idx) for n, i in GADGET)

def f(v):
    s = "%.3f" % v
    s = s.rstrip("0").rstrip(".")
    if s in ("-0", ""):
        s = "0"
    return s + ("f" if "." in s else ".f")

def f6(v):
    s = ("%.6f" % v).rstrip("0").rstrip(".")
    if s in ("-0", ""):
        s = "0"
    return s + ("f" if "." in s else ".f")

def axis(v):
    # unit vectors: full precision (the engine checks that they are unit length)
    return "{%s, %s, %s}" % (f6(v[0]), f6(v[1]), f6(v[2]))

def vec(v):
    return "{%s, %s, %s}" % (f(v[0]), f(v[1]), f(v[2]))

insts = []      # dicts
for p in M["pieces"]:
    for i, inst in enumerate(p["instances"]):
        loc = list(MOVE.get((p["name"], i), inst["game_loc_uu"]))
        role = 0
        if p["name"] in MOVERS:
            role = 1
        elif is_gadget(p["name"], i):
            role = 2
        insts.append({"name": p["name"], "index": i, "role": role, "parent": -1, "loc": loc,
                      "ax": inst["game_axis_x"], "ay": inst["game_axis_y"], "az": inst["game_axis_z"],
                      "scale": inst.get("scale") or [1, 1, 1], "src": inst, "piece": p})

# Vertical hinges that would hit a wall or the neighbouring door when swung past 90 degrees.
def hinge_info(e):
    s = e["src"]
    if "hinge_axis_game" not in s or abs(s["hinge_axis_game"][2]) < 0.99:
        return None
    mn, mx = s["game_bbox_min_uu"], s["game_bbox_max_uu"]
    hx, hy = e["loc"][0], e["loc"][1]
    direction = 1 if (mn[0] + mx[0]) * 0.5 > hx else -1
    return hx, hy, mn[2], mx[2], direction
doors = [(e, hinge_info(e)) for e in insts if hinge_info(e)]
def limited(e):
    h = hinge_info(e)
    if not h:
        return False
    hx, hy, z0, z1, d = h
    if (hx - (-7000.0) < 120.0 and d > 0) or (7000.0 - hx < 120.0 and d < 0):
        return True
    for o, oh in doors:
        if o is e:
            continue
        ox, oy, oz0, oz1, od = oh
        if abs(ox - hx) < 60.0 and abs(oy - hy) < 60.0 and oz0 < z1 and z0 < oz1 and od != d:
            return True
    return False

movers = []
for n, e in enumerate(insts):
    if e["role"] != 1:
        continue
    p, s = e["piece"], e["src"]
    op, cl, flags, auto = MOVERS[p["name"]]
    mo = p["motion"]
    m = {"inst": n, "open": op, "close": cl, "flags": flags, "auto": auto, "axis": [0, 0, 1], "angle": 0.0, "slide": [0, 0, 0], "note": ""}
    if mo["type"] == "hinge":
        m["type"] = 0
        m["axis"] = s["hinge_axis_game"]
        a = float(s["open_angle_game_deg"])
        if limited(e) and abs(a) > 90.0:
            m["note"] = "limited from %g" % a
            a = math.copysign(90.0, a)
        m["angle"] = a
    else:
        m["type"] = 1
        k = SLIDE_SCALE.get(p["name"], 1.0)
        m["slide"] = [c * k for c in s["slide_game_uu"]]
        if k != 1.0:
            m["note"] = "slide scaled by %.3f" % k
    e["mover"] = len(movers)
    movers.append(m)

# Riders: props that sit on a generic mover travel with it.
for e in insts:
    so = e["src"].get("sits_on")
    if not so or e["role"] != 0:
        continue
    for o in insts:
        if o["name"] == so["parent"] and o["index"] == so.get("parent_instance", 0) and o["role"] == 1:
            e["role"] = 3
            e["parent"] = o["mover"]

# ---------------------------------------------------------------------------------------------
# The living room, the yard and the street (living_manifest.json): another schema, the same tables.
# Root pieces list game_placement (location, yaw); the others are "attached" to a parent with an offset in the parent's
# Unreal-local frame and a Blender XYZ Euler rotation in model axes.
# ---------------------------------------------------------------------------------------------
LSRC = os.path.join(ROOT, "SourceArt", "Blender", "living", "living_manifest.json")
F_BIG_OPENS = 32   # the Human and the pets push it open when they come near (the door between the rooms)
L_MOVERS = {
    "SM_L_TVStand_Door": ("Отодвинуть дверцу тумбы", "Задвинуть дверцу тумбы", 0, 0),
    "SM_L_Door_Leaf": ("Открыть дверь в гостиную", "Закрыть дверь в гостиную", F_DEFAULT_OPEN | F_BIG_OPENS, 0),
    "SM_Y_FenceGate_Leaf": ("Открыть калитку", "Закрыть калитку", F_DEFAULT_OPEN, 0),
    "SM_Y_Bin_Lid": ("Открыть крышку бака", "Закрыть крышку бака", F_DEFAULT_OPEN, 0),
}
L_SKIP = ("SM_Y_WindowSill",              # the kitchen set already has the outer sill
          "SM_Y_GrassTuft_A", "SM_Y_GrassTuft_B", "SM_Y_GrassTuft_C")   # scattered as instances by the builder

def mat_mul(A, B):
    return [[sum(A[i][k] * B[k][j] for k in range(3)) for j in range(3)] for i in range(3)]

def mat_vec(A, v):
    return [sum(A[i][k] * v[k] for k in range(3)) for i in range(3)]

def yaw_mat(deg):
    # columns = where the Unreal-local axes point (Unreal yaw turns +X towards +Y)
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return [[c, -s, 0.0], [s, c, 0.0], [0.0, 0.0, 1.0]]

def euler_model_to_ue(rx, ry, rz):
    # Blender XYZ Euler in model axes -> the same rotation in Unreal-local axes (Y mirrored): Sy * Rz*Ry*Rx * Sy
    a, b, c = (math.radians(v) for v in (rx, ry, rz))
    Rx = [[1, 0, 0], [0, math.cos(a), -math.sin(a)], [0, math.sin(a), math.cos(a)]]
    Ry = [[math.cos(b), 0, math.sin(b)], [0, 1, 0], [-math.sin(b), 0, math.cos(b)]]
    Rz = [[math.cos(c), -math.sin(c), 0], [math.sin(c), math.cos(c), 0], [0, 0, 1]]
    R = mat_mul(Rz, mat_mul(Ry, Rx))
    S = [1, -1, 1]
    return [[S[i] * R[i][j] * S[j] for j in range(3)] for i in range(3)]

def tiled(name, places):
    # Runs the manifest describes in a note ("one per 1200 uu from y=-5200 to 5600") written out.
    def at(x, y, z, yaw, sx=1.0):
        return {"location_uu": [x, y, z], "yaw_deg": yaw, "sx": sx}
    if name == "SM_L_Skirting":   # the three walls of the living room that had a baseboard: one stretched module per run
        return [at(13700, 1485, 0, 180, 13000 / 1900.0), at(13700, -8985, 0, 0, 13000 / 1900.0), at(20185, -3750, 0, 90, 10500 / 1900.0)]
    if name == "SM_Y_FencePanel":
        out = [at(19900 - 2200 * k, -16000, 0, 180) for k in range(15)]       # south fence
        out += [at(-12000, -14900 + 2200 * k, 0, 90) for k in range(4)]       # west fence of the yard
        out += [at(-9400, y, 0, 90) for y in (-4100, -1900, 300)]             # street fence
        return out
    if name == "SM_S_PavementSlab_A":
        return [at(-10200, -4600 + 1200 * k, -200, 0) for k in range(9) if k != 3]
    if name == "SM_S_Kerb":
        return [at(-11050, -4600 + 1200 * k, -230, 90) for k in range(9)]
    if name == "SM_S_Road":
        return [at(-13800, -4400 + 1600 * k, -260, 0) for k in range(7)]
    return places

l_count = 0
if os.path.exists(LSRC):
    LM = json.load(open(LSRC, encoding="utf-8"))
    LP = {p["name"]: p for p in LM["pieces"]}
    cache = {}
    def placements(name):
        # -> list of (loc, R, scale, motion-in-game or None)
        if name in cache:
            return cache[name]
        p = LP[name]
        out = []
        for g in tiled(name, p.get("game_placement", [])):
            out.append((list(map(float, g["location_uu"])), yaw_mat(g["yaw_deg"]), [g.get("sx", 1.0), 1.0, 1.0], None))
        for a in p.get("attached", []):
            Rl = euler_model_to_ue(*a["rot_deg_model_xyz"])
            for (ploc, pR, ps, _) in placements(a["parent"]):
                off = mat_vec(pR, a["offset_uu_game"])
                mo = None
                if "motion" in a and a["motion"]["type"] in ("hinge", "slide"):
                    m = a["motion"]
                    if m["type"] == "slide":
                        mo = ("slide", mat_vec(pR, m["vector_uu_game"]))
                    else:
                        ax = m["axis"]
                        mo = ("hinge", mat_vec(pR, [ax[0], -ax[1], ax[2]]), -float(m["open_angle_deg"]))   # mirrored Y: the angle flips
                out.append(([ploc[i] + off[i] for i in range(3)], mat_mul(pR, Rl), [1.0, 1.0, 1.0], mo))
        cache[name] = out
        return out

    for p in LM["pieces"]:
        name = p["name"]
        if name in L_SKIP:
            continue
        for i, (loc, R, sc, mo) in enumerate(placements(name)):
            col = lambda j: [R[0][j], R[1][j], R[2][j]]
            e = {"name": name, "index": i, "role": 1 if name in L_MOVERS else 0, "parent": -1, "loc": loc,
                 "ax": col(0), "ay": col(1), "az": col(2), "scale": sc}
            n = len(insts)
            insts.append(e)
            l_count += 1
            if name in L_MOVERS:
                op, cl, flags, auto = L_MOVERS[name]
                m = {"inst": n, "open": op, "close": cl, "flags": flags, "auto": auto, "axis": [0, 0, 1], "angle": 0.0, "slide": [0, 0, 0], "note": "", "type": 1}
                if mo and mo[0] == "hinge":
                    m["type"], m["axis"], m["angle"] = 0, mo[1], mo[2]
                elif mo:
                    m["slide"] = mo[1]
                e["mover"] = len(movers)
                movers.append(m)

L = []
L.append("// Generated by Tools/gen_kitchen_art.py from SourceArt/Blender/kitchen/kitchen_manifest.json. Do not edit by hand.")
L.append("// Plus SourceArt/Blender/living/living_manifest.json (living room, yard, street) when it exists: %d of the instances." % l_count)
L.append("// Included by KitchenArt.cpp only. %d instances, %d generic movers." % (len(insts), len(movers)))
L.append("")
L.append("static const FKitchenArtInst GKitchenArt[] =")
L.append("{")
for n, e in enumerate(insts):
    L.append("\t{ TEXT(\"%s\"), %d, %d, %d, %s, %s, %s, %s, %s },   // %d" % (
        e["name"], e["index"], e["role"], e["parent"], vec(e["loc"]), axis(e["ax"]), axis(e["ay"]), axis(e["az"]), vec(e["scale"]), n))
L.append("};")
L.append("")
L.append("// Inst, Type (0 hinge, 1 slide), hinge axis (game space), open angle, slide (game space), flags, auto-close seconds, prompts.")
L.append("// Flags: 1 starts open, 2 needs the fridge door open, 4 needs the dishwasher door open, 8 the Human pushes it shut, 16 the Human may open it, 32 the Human and the pets push it open.")
L.append("static const FKitchenArtMover GKitchenMovers[] =")
L.append("{")
for n, m in enumerate(movers):
    e = insts[m["inst"]]
    L.append("\t{ %d, %d, %s, %s, %s, %d, %s, TEXT(\"%s\"), TEXT(\"%s\") },   // %d %s#%d %s" % (
        m["inst"], m["type"], vec(m["axis"]), f(m["angle"]), vec(m["slide"]), m["flags"], f(m["auto"]), m["open"], m["close"],
        n, e["name"], e["index"], m["note"]))
L.append("};")
L.append("")
text = "\r\n".join(L) + "\r\n"
with open(OUT, "w", encoding="utf-8-sig", newline="") as fo:
    fo.write(text)
roles = [sum(1 for e in insts if e["role"] == r) for r in range(4)]
print("wrote %s: %d instances (static %d, movers %d, gadget %d, riders %d)" % (OUT, len(insts), roles[0], roles[1], roles[2], roles[3]))
for m in movers:
    if m["note"]:
        e = insts[m["inst"]]
        print("  %s#%d: %s" % (e["name"], e["index"], m["note"]))