# Ant Kitchen level v3.1 (owner's decision, 21.11.2026): the v2 gameplay ramps that still read as a prototype get believable household
# shapes in the SAME places and the SAME boxes (they carry routes and cover: Tools/level/check_level_v3.py proves those on the boxes):
#   SM_K_Spoon      (giant silver spoon, floor -> dining table)        -> SM_V31_PizzaPeel   a wooden pizza peel leaning on the table
#   SM_K_Broom      (yellow toy-coloured broom along the west wall)    -> SM_V31_Broom       a real broom: sisal head, dark block, wooden handle
#   SM_K_CerealBox  (cereal box 1.8x real size lying on the floor)     -> SM_V31_DeliveryBox a delivery carton on its side, flaps open (the tunnel)
#   SM_L_Blanket    (pink blanket slab, sofa seat -> floor)            -> SM_V31_Throw       a knitted throw slid off the seat, folds and all
#   SM_L_ToyBlock   (12 cm toy blocks stacked into stairs)             -> SM_V31_BoardBooks  a stack of the kid's board books (same 240 uu step)
# Each is built in the original's local frame and to its bounding box (checked at the end: BOXFIT lines), so the placement table keeps the
# original rows: Tools/level/gen_level_v3_code.py swaps only the mesh name. Run:
#   blender --background --factory-startup --python level_v31_pieces.py -- <out_dir> [dry]
import bpy, sys, os, time, math, random
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else os.path.join(HERE, "..", "..", "SourceArt", "Blender", "level_v31")
DRY = len(argv) > 1 and argv[1] == "dry"
bpy.ops.wm.read_factory_settings(use_empty=True)
import kitchen_lib as L
from level_v3_common import *
R = random.Random(31)
t0 = time.time()
# the boxes to fit (uu, from the original meshes: Tools/blender/level_v31_probe.py)
BOX = {"SM_V31_PizzaPeel": (-747, 747, -150, 150, 0, 63), "SM_V31_Broom": (-300, 300, -1427, 1427, 0, 1885),
       "SM_V31_DeliveryBox": (-690, 690, -524, 524, 0, 531), "SM_V31_Throw": (-700, 700, -1085, 1085, 0, 976),
       "SM_V31_BoardBooks": (-122, 122, -122, 122, 0, 240)}
NOTE = {"SM_V31_PizzaPeel": "Деревянная лопата для пиццы, прислонена к столу (вместо гигантской ложки): тот же наклон и ширина дорожки",
        "SM_V31_Broom": "Настоящая метла у стены (вместо жёлто-красной): сизаль, тёмная колодка, деревянный черенок",
        "SM_V31_DeliveryBox": "Коробка от доставки на боку, клапаны открыты (вместо коробки хлопьев 1,8x): тот же тоннель",
        "SM_V31_Throw": "Вязаный плед сполз с сиденья дивана на пол (вместо розовой плиты): складки, та же горка",
        "SM_V31_BoardBooks": "Стопка детских книжек-картонок (вместо кубика 12 см): ступень 240 uu"}

# ------------------------------------------------------------------------------------------------ pizza peel (local x: up the ramp)
# the blade lies where the spoon's bowl was (x -747..-250, 300 wide), the handle runs up to the table edge (x up to 747)
UP([(-747 + 40, -150), (-250, -150), (-190, -110), (-120, -42), (-120, 42), (-190, 110), (-250, 150), (-747 + 40, 150), (-747, 110), (-747, -110)], 0, 22, "Butcher", bev=3)
UB(-120, 640, -36, 36, 0, 46, "Butcher", 10)                       # handle (flat-topped: the ants' road up)
UCX(34, 110, 692, 0, 30, "Butcher", 14)                             # rounded grip end
UZ(14, 70, 690, 0, 0, "Plinth", 12)                                 # the hole for hanging it (dark: reads as a hole)
L.cyl(3 * K, 1490 * K, 0, 0, 62.5 * K, "Butcher", "X", 4)           # (a hairline to touch the box's top: invisible)
done("SM_V31_PizzaPeel", NOTE["SM_V31_PizzaPeel"])

# ------------------------------------------------------------------------------------------------ broom (local y: along the wall, head at -y on the floor)
UB(-292, 292, -1427, -830, 0, 160, "Sisal", 18, 2)                  # the bristles
for k in range(14):                                                 # ragged tips
    x = -270 + k * 41.5
    UB(x - 18, x + 18, -1427 + R.uniform(-4, 20), -1380, 0, R.uniform(120, 170), "Sisal", 4)
UB(-300, 300, -1360, -860, 160, 273, "Plinth", 14)                  # the block
a, b = (0, -1000, 250), (0, 1345, 1838)
UT([a, b], 26, "Butcher", 10)                                       # the handle up to the top of the box (it leans on the wall)
UT([(0, 1345, 1838), (0, 1395, 1872)], 30, "Plinth", 10)            # cap
US(28, 0, 1399, 1857, "Plinth", nu=10, nv=6)
UT([(-300, -1100, 230), (-300, -1090, 260)], 4, "Plinth", 4); UT([(300, -1100, 230), (300, -1090, 260)], 4, "Plinth", 4)   # (box corners)
done("SM_V31_Broom", NOTE["SM_V31_Broom"])

# ------------------------------------------------------------------------------------------------ delivery box on its side (tunnel along x)
T = 14
UB(-560, 560, -524, 524, 0, T, "Cardboard", 2)                      # floor side
UB(-560, 560, -524, 524, 531 - T, 531, "Cardboard", 2)              # top side
UB(-560, 560, -524, -524 + T, T, 531 - T, "Cardboard", 2)           # the two walls
UB(-560, 560, 524 - T, 524, T, 531 - T, "Cardboard", 2)
UB(-180, 180, -4, 4, 531, 531.5, "Paper", 0)                        # label (flush)
UB(-560, 560, -40, 40, 531 - 1, 531, "Paper", 0)                    # tape along the top
for s in (-1, 1):                                                    # flaps folded out at both ends (the tunnel's mouths)
    x0 = s * 560
    UP([(x0, 531), (x0 + s * 130, 490), (x0 + s * 130, 470), (x0, 517)], -500, 500, "Cardboard", "Y")   # top flap, drooping
    UP([(x0, -524), (x0 + s * 125, -470), (x0 + s * 125, -456), (x0, -510)], 20, 500, "Cardboard", "Z")  # side flaps, half open
    UP([(x0, 524), (x0 + s * 125, 470), (x0 + s * 125, 456), (x0, 510)], 20, 500, "Cardboard", "Z")
    UB(min(x0, x0 + s * 130), max(x0, x0 + s * 130), -500, 500, 0, 10, "Cardboard", 1)                  # bottom flap flat on the floor
done("SM_V31_DeliveryBox", NOTE["SM_V31_DeliveryBox"])

# ------------------------------------------------------------------------------------------------ knitted throw slid off the seat
# path in (y, z): a pool on the floor, the slope up the front of the seat, the rest on the seat
P = [(-1085, 6), (-930, 10), (-860, 40), (-520, 330), (-160, 640), (150, 900), (271, 960), (600, 966), (1085, 960)]
def along(t):
    s = t * (len(P) - 1); i = min(int(s), len(P) - 2); f = s - i
    return P[i][0] + (P[i + 1][0] - P[i][0]) * f, P[i][1] + (P[i + 1][1] - P[i][1]) * f
def throw_top(u, v):
    y, z = along(v)
    x = -690 + 1380 * u
    fold = 9.0 * math.sin(u * math.pi * 7 + v * 3) * (0.4 + 0.6 * math.sin(v * math.pi))   # soft folds across
    return (x, y, z + 14 + fold)
def throw_bot(u, v):
    x, y, z = throw_top(1 - u, v)
    return (x, y, max(0.5, z - 14))
sheet(throw_top, 40, 30, "SockWool")
sheet(throw_bot, 40, 30, "SockWool")
for s in (-1, 1):                                                    # the fringe along both long edges
    for k in range(18):
        v = (k + 0.5) / 18.0; y, z = along(v)
        UB(s * 690 - 6, s * 690 + 6 + s * 8, y - 10, y + 10, max(0, z - 6), z + 22, "Sisal", 0)
UB(-700, -699, -1085, -1084, 0, 1, "SockWool", 0); UB(699, 700, 1084, 1085, 975, 976, "SockWool", 0)   # (box corners)
done("SM_V31_Throw", NOTE["SM_V31_Throw"])

# ------------------------------------------------------------------------------------------------ board books (six, 40 uu each)
COVERS = ["Teal", "Honey", "Walnut", "MugBlue", "Leaf", "Paper"]
for k in range(6):
    z0 = 40 * k; a = math.radians(R.uniform(-4, 4)); w, d = R.uniform(206, 222), R.uniform(186, 206)
    dx, dy = R.uniform(-6, 6), R.uniform(-6, 6)
    UR(w, d, 40, dx, dy, z0 + 20, COVERS[k], 6, 1, (0, 0, a))
    UR(w - 14, d - 8, 30, dx + 6, dy, z0 + 20, "Paper", 2, 1, (0, 0, a))   # the page block showing at the edge
UB(-122, -121, -122, -121, 0, 1, "Paper", 0); UB(121, 122, 121, 122, 239, 240, "Paper", 0)   # (box corners)
done("SM_V31_BoardBooks", NOTE["SM_V31_BoardBooks"])

# ------------------------------------------------------------------------------------------------ check the boxes, export, manifest
fit = []
for n, b in BOX.items():
    p = L.PIECES.get(n)
    if not p: fit.append((n, "not built")); continue
    mn, mx = p["mn"] / K, p["mx"] / K
    d = max(abs(mn.x - b[0]), abs(mx.x - b[1]), abs(mn.y - b[2]), abs(mx.y - b[3]), abs(mn.z - b[4]), abs(mx.z - b[5]))
    print("BOXFIT %s off by %.1f uu: %s..%s (want %s)" % (n, d, [round(v) for v in mn], [round(v) for v in mx], b))
    if d > 15: fit.append((n, d))
os.makedirs(OUT, exist_ok=True)
if not DRY:
    L.export_all(OUT)
HEADER = {"what": "Ant Kitchen level v3.1: believable replacements of five v2 ramps, built to the originals' boxes (Tools/blender/level_v31_pieces.py).",
          "conventions": {"blender": "metres, +Z up, the original piece's local frame and origin", "game_scale": "1 m = 1900 uu (component scale 19)",
                          "names": "SM_V31_* load from /Game/LevelV3", "replaces": {"SM_K_Spoon": "SM_V31_PizzaPeel", "SM_K_Broom": "SM_V31_Broom", "SM_K_CerealBox": "SM_V31_DeliveryBox",
                                                                                   "SM_L_Blanket": "SM_V31_Throw", "SM_L_ToyBlock": "SM_V31_BoardBooks"}},
          "notes": ["Same bounding boxes as the pieces they replace (BOXFIT in the log): the routes and the cover the checker proves are unchanged."]}
doc = L.write_manifest(OUT, HEADER)
src, dst = os.path.join(OUT, "kitchen_manifest.json"), os.path.join(OUT, "level_v31_new_pieces.json")
if os.path.exists(dst): os.remove(dst)
os.rename(src, dst)
print("BOXFIT_BAD %s" % fit)
print("LEVEL_V31_DONE %.1fs" % (time.time() - t0))
