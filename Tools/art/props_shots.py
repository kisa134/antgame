# KAN-22 (small props): the packaged-game shots of the hero props - one close frame per piece from the ant's height (15 uu over
# the surface it stands on), in front of the instance (the side its label faces), filling the frame. Writes the -AntFilmDo script
# for Saved/art_props/play.cmd. Run: python Tools/art/props_shots.py > Saved/art_props/filmdo.txt
import json, math, os, re, sys
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
art = open(os.path.join(R, "Source/AntKitchen/LevelV3Art.inl"), encoding="utf-8", errors="ignore").read()
man = {p["name"]: p for p in json.load(open(os.path.join(R, "SourceArt/Blender/hero_props/hero_props_manifest.json"), encoding="utf-8"))["pieces"]}
PICK = {"SM_K_Plate": 0, "SM_K_SpiceJar": 0, "SM_K_Mug": 0, "SM_K_PastaBox": 2, "SM_K_JarJam": 1, "SM_K_TinCan": 0, "SM_K_Bottle": 0,
        "SM_K_MouseTrap": 0, "SM_K_Sponge": 0, "SM_K_Detergent": 0, "SM_K_Fork": 0, "SM_K_Knife": 0, "SM_V3_Phone": 0, "SM_L_Remote": 0,
        "SM_K_Matchbox": 0, "SM_V31_BoardBooks": 0, "SM_L_Book_A": 1, "SM_L_Book_B": 1, "SM_L_Book_C": 1, "SM_L_Book_D": 1,
        "SM_L_Book_E": 1, "SM_L_Book_F": 1, "SM_L_Book_G": 1, "SM_L_Book_H": 1}
num = r"(-?[\d.]+)f?"
vec = r"\{%s, %s, %s\}" % (num, num, num)
rx = re.compile(r'TEXT\("(\w+)"\), (\d+), \d+, -?\d+, %s, %s, %s, %s, %s' % (vec, vec, vec, vec, vec))
inst = {}
for m in rx.finditer(art):
    g = m.groups(); v = [float(x) for x in g[2:]]
    inst[(g[0], int(g[1]))] = (v[0:3], v[3:6], v[6:9], v[9:12], v[12:15])
cmds = []; t = 30; names = []
for name, idx in PICK.items():
    if (name, idx) not in inst or name not in man: continue
    loc, ax, ay, az, sc = inst[(name, idx)]
    mn, mx = man[name]["bbox_min_m"], man[name]["bbox_max_m"]
    # Blender metres -> the mesh in UE units (Y flips on import) -> scaled by the instance
    lmn = [mn[0] * 1900 * sc[0], -mx[1] * 1900 * sc[1], mn[2] * 1900 * sc[2]]
    lmx = [mx[0] * 1900 * sc[0], -mn[1] * 1900 * sc[1], mx[2] * 1900 * sc[2]]
    cl = [(a + b) / 2 for a, b in zip(lmn, lmx)]; ext = [b - a for a, b in zip(lmn, lmx)]
    def w(p): return [loc[i] + ax[i] * p[0] + ay[i] * p[1] + az[i] * p[2] for i in range(3)]
    c = w(cl)
    # the front: Blender -Y = the mesh's +Y = the instance's AY (labels and fronts face it); the lowest world z of the box = its floor
    corners = [w([x, y, z]) for x in (lmn[0], lmx[0]) for y in (lmn[1], lmx[1]) for z in (lmn[2], lmx[2])]
    zfloor = min(p[2] for p in corners); ztop = max(p[2] for p in corners)
    front = [ay[0], ay[1]]; fl = math.hypot(*front) or 1.0; front = [front[0] / fl, front[1] / fl]
    span = max(math.hypot(ext[0] * ax[0] + 0, ext[0] * ax[1]), abs(ztop - zfloor), 60.0)
    span = max(span, max(abs(e) for e in ext) * 0.8)
    dist = span / (2 * math.tan(math.radians(35))) * 1.15 + 20
    cam = [c[0] + front[0] * dist, c[1] + front[1] * dist, zfloor + 15]
    look = [c[0], c[1], zfloor + 0.45 * (ztop - zfloor)]
    yaw = math.degrees(math.atan2(look[1] - cam[1], look[0] - cam[0]))
    pitch = math.degrees(math.atan2(look[2] - cam[2], math.hypot(look[0] - cam[0], look[1] - cam[1])))
    short = name.split("_", 1)[1].replace("_", "")
    cmds.append("%d:at %.0f %.0f %.0f %.1f %.1f 70" % (t, cam[0], cam[1], cam[2], yaw, pitch)); cmds.append("%d:shot TAG_%s" % (t + 3, short))
    names.append("%s=%s" % (name, short)); t += 4
print(";".join(cmds))
print(" ".join(names), file=sys.stderr)
print("END %d" % t, file=sys.stderr)
