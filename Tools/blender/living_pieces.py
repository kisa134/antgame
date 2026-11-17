# Ant Kitchen: living room, yard and street furniture as game-ready pieces (one FBX each), a manifest and previews.
# Run:  blender --background --factory-startup --python living_pieces.py -- <out_dir> [noexport] [norender] [shots=a,b,c]
# The pieces themselves are in living_room.py / living_yard.py / living_street.py, the helpers in living_common.py.
import sys, os, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from living_common import *
import living_room, living_yard, living_street

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else "."
FLAGS = argv[1:]
SHOTS = None
for f in FLAGS:
    if f.startswith("shots="):
        SHOTS = f[6:].split(",")
os.makedirs(OUT, exist_ok=True)
t0 = time.time()
living_room.build_living()
living_yard.build_yard()
living_street.build_street()
for n in ORDER:
    p = PIECES[n]
    print("PIECE %-26s tris=%5d size=%.3f x %.3f x %.3f open=%d mats=%s" % (n, p["tris"], p["size"][0], p["size"][1], p["size"][2], p["open_edges"], ",".join(p["mats"])))
print("BUILT pieces=%d tris=%d in %.1fs" % (len(ORDER), sum(PIECES[n]["tris"] for n in ORDER), time.time() - t0))
if "noexport" not in FLAGS:
    export_all(OUT)
    print("export done %.1fs" % (time.time() - t0))
setup_scene()
L = living_room.layout_living()
Y = living_yard.layout_yard()
T = living_street.layout_street()
write_manifest(OUT)

def shot(name, cam, tgt, lens=30, hide=(), energy=6.0):
    if "norender" in FLAGS or (SHOTS is not None and name not in SHOTS):
        return
    render(OUT, "preview_%s.png" % name, cam, tgt, lens, hide, energy)
    print("shot %s %.1fs" % (name, time.time() - t0))

# Preview space = the game mirrored in Y, in metres: (x, y, z) = (X / 1900, -Y / 1900, Z / 1900).
YW = (Y["house1"], Y["house2"], Y["house3"], Y["window"])
shot("living", (2.6, 7.6, 4.3), (7.9, 1.3, 0.5), 24, (L["S"], L["W1"], L["W2"], L["W3"]) + YW)
shot("living_tv", (10.35, 0.45, 2.5), (6.0, 4.3, 0.4), 20, (L["N"], L["E"]))
shot("sofa", (9.9, 2.3, 1.5), (8.2, -0.15, 0.45), 30)
shot("sofa_under", (6.3, 0.95, 0.05), (7.4, -0.3, 0.065), 20, (), 10.0)
shot("bookshelf", (7.5, 2.4, 1.5), (10.4, 2.37, 1.22), 24)
shot("armchair", (6.9, 4.4, 1.5), (4.95, 2.85, 0.35), 28)
shot("door", (6.3, 3.0, 1.5), (3.85, 1.55, 1.0), 26)
shot("tvstand", (6.1, 2.7, 0.8), (6.9, 4.35, 0.36), 28)
shot("tvstand_back", (8.6, 6.0, 0.9), (7.3, 4.6, 0.33), 32, (L["S"],) + YW)
shot("yard", (10.0, 12.6, 6.6), (0.8, 5.4, 0.4), 24)
shot("yard_tree", (2.6, 3.4, 2.0), (-4.2, 6.6, 3.3), 20)
shot("yard_bin", (0.0, 4.7, 1.6), (2.1, 6.7, 0.6), 30)
shot("yard_toys", (-1.2, 3.1, 0.75), (-2.6, 4.15, 0.08), 28)
shot("street", (-12.2, 4.8, 3.8), (-5.7, -0.2, 1.7), 25)
shot("car", (-9.9, -4.0, 1.4), (-7.1, -0.3, 0.45), 35)
shot("car_under", (-9.0, -1.9, -0.05), (-7.1, 0.3, 0.03), 20, (), 10.0)
shot("street_props", (-5.75, 0.1, 1.25), (-5.15, -2.2, 0.55), 28)
shot("street_icecream", (-5.0, 1.6, 0.42), (-5.42, 0.74, 0.04), 35)
shot("drain", (-6.15, 2.75, 0.75), (-5.9, 1.58, -0.12), 35)
shot("gutter", (-6.3, 4.1, 5.9), (-3.9, 2.5, 4.72), 35)
if "norender" not in FLAGS:
    small = [n for n in ORDER if max(PIECES[n]["size"]) < 0.62]
    mid = [n for n in ORDER if 0.62 <= max(PIECES[n]["size"]) < 1.75]
    if SHOTS is None or "lineup_small" in SHOTS:
        lineup(OUT, "preview_lineup_small.png", small, 3.2, 0.10)
    if SHOTS is None or "lineup_mid" in SHOTS:
        lineup(OUT, "preview_lineup_mid.png", mid, 9.0, 0.25)
if "noblend" not in FLAGS:
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "living_set.blend"))
print("LIVING_DONE pieces=%d tris=%d in %.1fs" % (len(ORDER), sum(PIECES[n]["tris"] for n in ORDER), time.time() - t0))
