# Ant Kitchen level v3.2, the pilot zone "Puchina" (the sink and the cupboard under it), docs: claude/art-principles-itt.md.
# The cupboard under the sink becomes a believable under-sink cupboard: a bucket with a floor rag over its rim, a wire rack on legs
# (a second floor over the mousetrap) with folded rags and a spray bottle on it, a plunger standing in the corner; in the sink a big
# cutting board left to soak, leaning from the bottom to the front wall (the slope a load can be walked up: 34 degrees), and a dish
# soap bottle on the rim. Every piece is a household thing in the place it has in a real flat; all of them go through the kitchen
# builder's table, so they wreck like the rest (AntWreck). Numbers are uu in the piece-local frame (level_v3_common.py). Run:
#   blender --background --factory-startup --python level_v32_pieces.py -- <out_dir> [dry]
import bpy, sys, os, time, math, random
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else os.path.join(HERE, "..", "..", "SourceArt", "Blender", "level_v32")
DRY = len(argv) > 1 and argv[1] == "dry"
bpy.ops.wm.read_factory_settings(use_empty=True)
import kitchen_lib as L
from level_v3_common import *
mat("Rubber", (0.50, 0.13, 0.09), 0.8, 0, False, 0)      # the game has a look for it (Map4Look)
R = random.Random(32)
t0 = time.time()

def cloth2(top, nu, nv, m, th=10):
    """a rag with thickness: the top sheet and its mirrored underside"""
    sheet(top, nu, nv, m)
    def bot(u, v):
        x, y, z = top(1 - u, v)
        return (x, y, max(0.5, z - th))
    sheet(bot, nu, nv, m)

# ------------------------------------------------------------------------------------------------ bucket with a floor rag over its rim
UL([(0, 0), (196, 0), (206, 14), (258, 470), (268, 482), (268, 494), (250, 494), (196, 30), (0, 30)], "Red", n=28)
for k in range(3):                                                  # ribs under the rim (chunky, readable from the floor)
    UL([(259 + k * 0, 0), (270, 0), (270, 10), (259, 10)], "Red", z=380 + k * 34, n=28, closed=True)
for s in (-1, 1):                                                    # handle lugs
    UB(s * 262 - 14, s * 262 + 14, -22, 22, 430, 478, "Red", 6)
arc = [(262 * math.cos(a), 34 + 250 * math.sin(a) * 0.55, 452 - 250 * math.sin(a) * 0.80) for a in [math.pi * i / 14.0 for i in range(15)]]
UT(arc, 8, "Steel", 8)                                               # the wire handle, hanging down the back
UCX(17, 120, 0, 34 + 250 * 0.55, 452 - 250 * 0.80, "Enamel", 12)     # its plastic grip
P = [(-120, 300), (-200, 420), (-262, 502), (-292, 470), (-300, 300), (-318, 120), (-352, 30), (-420, 12), (-466, 10)]   # (y, z): in, over the rim, down, on the floor
def along(t):
    s = t * (len(P) - 1); i = min(int(s), len(P) - 2); f = s - i
    return P[i][0] + (P[i + 1][0] - P[i][0]) * f, P[i][1] + (P[i + 1][1] - P[i][1]) * f
def rag(u, v):
    y, z = along(v)
    fold = 7.0 * math.sin(u * math.pi * 5 + v * 4) * (0.3 + 0.7 * v)
    return (-150 + 300 * u + 10 * math.sin(v * 9), y - abs(fold) * 0.4, z + 6 + fold * (0.2 if v < 0.45 else 1.0))
cloth2(rag, 18, 30, "Fabric")
done("SM_V3_Pail", "Ведро с половой тряпкой через край")

# ------------------------------------------------------------------------------------------------ wire rack on legs (the second floor)
W, D, H = 300, 240, 480
for sx in (-1, 1):
    for sy in (-1, 1):
        UT([(sx * (W - 12), sy * (D - 12), 0), (sx * (W - 12), sy * (D - 12), H)], 11, "Chrome", 10)
        UZ(17, 10, sx * (W - 12), sy * (D - 12), 0, "Plinth", 10)                     # rubber feet
UB(-W, W, -D, D, H - 16, H, "Steel", 5)                               # the deck: a solid sheet (an ant must not fall between wires)
for k in range(15):                                                   # the wires on top of it: thick enough to read, low enough to walk
    x = -W + 20 + k * 40
    UT([(x, -D + 6, H + 3), (x, D - 6, H + 3)], 5, "Chrome", 6)
for sy in (-1, 1):
    UT([(-W + 12, sy * (D - 12), H + 6), (W - 12, sy * (D - 12), H + 6)], 9, "Chrome", 8)   # rim rails
    UT([(-W + 12, sy * (D - 12), 150), (W - 12, sy * (D - 12), 150)], 7, "Chrome", 8)       # low stretchers
for sx in (-1, 1):
    UT([(sx * (W - 12), -D + 12, H + 6), (sx * (W - 12), D - 12, H + 6)], 9, "Chrome", 8)
done("SM_V3_WireRack", "Проволочная полка на ножках под мойкой (второй этаж)")

# ------------------------------------------------------------------------------------------------ folded rags (soft cover on the rack)
for k, (m, w, d) in enumerate((("SockWool", 400, 290), ("ClothCheck", 380, 270), ("Fabric", 350, 250))):
    a = math.radians(R.uniform(-7, 7))
    UR(w, d, 34, R.uniform(-8, 8), R.uniform(-8, 8), 17 + k * 34, m, 14, 2, (0, 0, a))
    UR(w - 6, 10, 36, 0, -d / 2.0 + 4, 18 + k * 34, m, 5, 1, (0, 0, a))                # the fold showing at the front
UB(-215, -214, -160, -159, 0, 1, "Fabric", 0); UB(214, 215, 159, 160, 103, 104, "Fabric", 0)
done("SM_V3_RagPile", "Стопка сложенных тряпок")

# ------------------------------------------------------------------------------------------------ spray bottle (window cleaner)
UB(-72, 72, -44, 44, 0, 250, "PlasticBlue", 22, 2)                    # the body
UL([(60, 0), (60, 20), (30, 62), (26, 96), (0, 96)], "PlasticBlue", z=244, n=16)      # shoulder and neck
UZ(34, 34, 0, 0, 336, "Enamel", 16, bev=5)                            # the collar
UB(-44, 96, -30, 30, 366, 428, "Red", 12)                             # the head
UCX(15, 26, 104, 0, 398, "Enamel", 10)                                # nozzle
UB(30, 50, -12, 12, 300, 366, "Red", 6)                               # trigger
UB(-50, 50, -45, -44.4, 60, 200, "Paper", 0)                          # label (flush on the front)
done("SM_V3_Sprayer", "Средство для стёкол с курком")

# ------------------------------------------------------------------------------------------------ plunger standing on its cup
UL([(0, 96), (40, 100), (96, 78), (150, 22), (156, 0), (146, 0), (138, 18), (88, 66), (40, 84), (0, 84)], "Rubber", n=24)
UZ(34, 46, 0, 0, 92, "Rubber", 14, bev=4)                             # the socket
UT([(0, 0, 130), (0, 0, 742)], 19, "Butcher", 12)                     # the stick
US(23, 0, 0, 748, "Butcher", nu=12, nv=8)
done("SM_V3_Plunger", "Вантуз, стоит на чашке")

# ------------------------------------------------------------------------------------------------ cutting board left to soak, leaning in the sink
# origin: the middle of its lower edge on the sink bottom; it rises toward -Y (the front wall of the sink): run 720, rise 490 (34 deg); its top edge rests on the wall just under the rim
RUN, RISE, TH = 720.0, 490.0, 34.0
ln = math.hypot(RUN, RISE); ny, nz = RISE / ln, RUN / ln              # the board's normal in (y, z) is (ny, nz) (leaning back toward +Y... up)
poly = [(0, 0), (-RUN, RISE), (-RUN + ny * TH, RISE + nz * TH), (ny * TH, nz * TH)]
UP(poly, -270, 270, "Butcher", "X", 5)
for k in range(5):                                                     # juice grooves near the edges and knife marks: grip for the eye
    t = 0.12 + k * 0.19
    y, z = -RUN * t + ny * (TH + 1), RISE * t + nz * (TH + 1)
    UT([(-200 + R.uniform(-30, 30), y, z), (160 + R.uniform(-40, 40), y - 14 * (RUN / ln), z + 14 * (RISE / ln))], 2.2, "Walnut", 4)
hy, hz = -RUN * 0.9 + ny * TH * 0.5, RISE * 0.9 + nz * TH * 0.5
UCY(46, 2, 0, hy, hz, "Plinth", 12)                                   # (a dark mark where the hanging hole is; flush)
done("SM_V3_SoakBoard", "Большая разделочная доска замочена в мойке: пандус со дна к борту (34°)")

# ------------------------------------------------------------------------------------------------ dish soap bottle
UZ(58, 228, 0, 0, 0, "Leaf", 20, bev=14)
UL([(50, 0), (30, 40), (24, 70), (0, 70)], "Leaf", z=222, n=16)
UZ(27, 34, 0, 0, 288, "Red", 14, bev=4)
UZ(10, 16, 0, 0, 322, "Enamel", 8)
UB(-40, 40, -59, -58.4, 60, 180, "Paper", 0)
done("SM_V3_DishSoap", "Средство для посуды на борту мойки")

# ------------------------------------------------------------------------------------------------ boxes for level_v3_layout.PIECES, export, manifest
for n in sorted(L.PIECES):
    p = L.PIECES[n]; mn, mx = p["mn"] / K, p["mx"] / K
    print("V32BOX '%s': (%d, %d, %d, %d, %d, %d)" % (n, math.floor(mn.x), math.ceil(mx.x), math.floor(mn.y), math.ceil(mx.y), math.floor(mn.z), math.ceil(mx.z)))
os.makedirs(OUT, exist_ok=True)
if not DRY:
    L.export_all(OUT)
HEADER = {"what": "Ant Kitchen level v3.2: the sink zone pilot (Tools/blender/level_v32_pieces.py).",
          "conventions": {"blender": "metres, +Z up, origin = the placement point", "game_scale": "1 m = 1900 uu (component scale 19)", "names": "SM_V3_Sink* etc. load from /Game/LevelV3"},
          "notes": ["All pieces are placed through the kitchen builder's table and wreck like the rest."]}
doc = L.write_manifest(OUT, HEADER)
src, dst = os.path.join(OUT, "kitchen_manifest.json"), os.path.join(OUT, "level_v32_new_pieces.json")
if os.path.exists(dst): os.remove(dst)
os.rename(src, dst)
print("LEVEL_V32_DONE %.1fs" % (time.time() - t0))