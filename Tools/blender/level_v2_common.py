# Ant Kitchen level v2: shared helpers for the new pieces (Tools/blender/level_v2_p1..p3.py). Built on kitchen_lib.py
# (bmesh only, closed solids, same export / manifest code as the kitchen set). Numbers are game units unless a name says otherwise.
import sys, os, math
HERE = os.path.dirname(os.path.abspath(__file__))
for _p in (HERE, os.path.join(HERE, "..", "level")):
    if _p not in sys.path:
        sys.path.insert(0, _p)
import bmesh
from mathutils import Vector, Matrix
import kitchen_lib as L
import level_v2_layout as V
K = L.K
piece, mat = L.piece, L.mat

# ---- palette: only slots that KitchenArt.cpp already has a look for (name, rgb, roughness, metallic, emission)
for _n, _c, _r, _m, _e in (
        ("Cabinet", (0.42, 0.60, 0.44), 0.5, 0, 0), ("Butcher", (0.68, 0.40, 0.16), 0.4, 0, 0), ("Walnut", (0.33, 0.20, 0.10), 0.5, 0, 0),
        ("Plinth", (0.08, 0.08, 0.09), 0.7, 0, 0), ("Brass", (0.86, 0.62, 0.22), 0.3, 1, 0), ("Steel", (0.72, 0.74, 0.76), 0.28, 1, 0),
        ("Chrome", (0.9, 0.9, 0.92), 0.12, 1, 0), ("Enamel", (0.95, 0.94, 0.90), 0.25, 0, 0), ("Red", (0.75, 0.10, 0.08), 0.35, 0, 0),
        ("Blue", (0.1, 0.25, 0.75), 0.35, 0, 0), ("MugBlue", (0.15, 0.35, 0.6), 0.3, 0, 0), ("Fabric", (0.96, 0.72, 0.16), 0.9, 0, 0),
        ("Terracotta", (0.72, 0.35, 0.20), 0.8, 0, 0), ("Leaf", (0.16, 0.50, 0.18), 0.5, 0, 0), ("Paper", (0.97, 0.96, 0.88), 0.9, 0, 0),
        ("Honey", (0.90, 0.55, 0.05), 0.15, 0, 0), ("Jam", (0.55, 0.05, 0.12), 0.15, 0, 0), ("Sugar", (0.95, 0.95, 0.95), 0.6, 0, 0),
        ("ShelfGlass", (0.62, 0.82, 0.88), 0.1, 0, 0), ("PlasticBlue", (0.12, 0.30, 0.75), 0.35, 0, 0), ("Cheese", (0.96, 0.78, 0.22), 0.5, 0, 0),
        ("Toast", (0.80, 0.55, 0.25), 0.85, 0, 0), ("CakePink", (0.96, 0.58, 0.70), 0.6, 0, 0), ("Cardboard", (0.70, 0.52, 0.32), 0.9, 0, 0),
        ("Leather", (0.24, 0.17, 0.13), 0.6, 0, 0), ("Ice", (0.78, 0.92, 1.0), 0.1, 0, 0), ("Teal", (0.05, 0.45, 0.48), 0.4, 0, 0),
        ("SockWool", (0.42, 0.46, 0.62), 0.95, 0, 0), ("Yarn", (0.78, 0.12, 0.30), 0.95, 0, 0), ("Sisal", (0.80, 0.66, 0.42), 0.95, 0, 0),
        ("Biscuit", (0.74, 0.48, 0.20), 0.8, 0, 0), ("Dust", (0.55, 0.53, 0.50), 0.95, 0, 0), ("Soil", (0.22, 0.13, 0.07), 0.95, 0, 0),
        ("Stone", (0.60, 0.58, 0.54), 0.85, 0, 0), ("BinGreen", (0.10, 0.34, 0.18), 0.45, 0, 0), ("CarPaint", (0.08, 0.52, 0.58), 0.22, 0, 0),
        ("LeafLight", (0.36, 0.62, 0.18), 0.55, 0, 0)):
    mat(_n, _c, _r, _m, False, _e)

def at(name):
    # layout placements (x, y, z, yaw[, scale]) -> kitchen_lib "at": the build-frame origin in the scene frame, phi = 180 - yaw
    out = []
    for p in V.PUT[name]:
        a = [-p[0] * K, p[1] * K, p[2] * K, 180.0 - p[3]]
        if len(p) > 4:
            a.append(p[4])
        out.append(tuple(a))
    return out

# ---- primitives in game units, piece-local frame (front = -Y, origin where the layout puts the piece)
def UB(x0, x1, y0, y1, z0, z1, m, bev=6, seg=1):
    L.box(x0 * K, x1 * K, y0 * K, y1 * K, z0 * K, z1 * K, m, bev * K, seg)
def UR(sx, sy, sz, x, y, z, m, bev=6, seg=1, rot=None):
    L.rbox(sx * K, sy * K, sz * K, x * K, y * K, z * K, m, bev * K, seg, rot)
def UC(r, h, x, y, z, m, axis="Z", n=16, bev=0, r2=None, rot=None):
    L.cyl(r * K, h * K, x * K, y * K, z * K, m, axis, n, bev * K, None if r2 is None else r2 * K, 1, rot)
def US(r, x, y, z, m, scale=(1, 1, 1), nu=12, nv=8, rot=None):
    L.ball(r * K, x * K, y * K, z * K, m, scale, nu, nv, rot)
def UL(prof, m, x=0, y=0, z=0, n=16, axis="Z", rot=None, closed=False):
    L.lathe([(r * K, h * K) for r, h in prof], m, x * K, y * K, z * K, n, axis, rot, closed)
def UT(pts, r, m, n=8):
    L.tube([(x * K, y * K, z * K) for x, y, z in pts], [v * K for v in r] if isinstance(r, (list, tuple)) else r * K, m, n)
def UP(poly, a0, a1, m, axis="Z", bev=0):
    L.prism([(a * K, b * K) for a, b in poly], a0 * K, a1 * K, m, axis, None, bev * K)
def UTRAY(x0, x1, y0, y1, z0, z1, t, m, open_sides="", bev=3):
    L.tray(x0 * K, x1 * K, y0 * K, y1 * K, z0 * K, z1 * K, t * K, m, bev * K, 1, open_sides)

# ---- the same in the game frame (pieces modelled in place: X is mirrored into the scene frame by kitchen_lib.G)
GB = L.gbox
def GT(pts, r, m, n=8):
    L.tube([L.G(*p) for p in pts], [v * K for v in r] if isinstance(r, (list, tuple)) else r * K, m, n)
def GPXZ(poly, Y0, Y1, m):      # polygon in game (X, Z), extruded along Y
    L.prism([(-x * K, z * K) for x, z in poly], Y0 * K, Y1 * K, m, "Y")
def GPYZ(poly, X0, X1, m):      # polygon in game (Y, Z), extruded along X
    L.prism([(y * K, z * K) for y, z in poly], -X1 * K, -X0 * K, m, "X")
def GPXY(poly, Z0, Z1, m):      # polygon in game (X, Y), extruded along Z
    L.prism([(-x * K, y * K) for x, y in poly], Z0 * K, Z1 * K, m, "Z")
def GS(r, p, m, scale=(1, 1, 1), nu=12, nv=8):
    g = L.G(*p)
    L.ball(r * K, g[0], g[1], g[2], m, scale, nu, nv)
def GC(r, h, p, m, axis="Z", n=16, r2=None, rot=None):
    g = L.G(*p)
    L.cyl(r * K, h * K, g[0], g[1], g[2], m, axis, n, 0.0, None if r2 is None else r2 * K, 1, rot)

def catenary(a, b, sag, n=24):
    return [(a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t - sag * 4 * t * (1 - t)) for t in [i / n for i in range(n + 1)]]

def solid(verts, faces, m):
    # a closed solid from explicit vertices (scene frame, metres) and faces
    bm = bmesh.new()
    vs = [bm.verts.new(v) for v in verts]
    for f in faces:
        bm.faces.new([vs[i] for i in f])
    L._emit(bm, m)

def blob(cx, cy, rx, ry, n, wob, seed):
    # an irregular flat outline (puddles, spills): n points around an ellipse, radius wobbling by +-wob
    out = []
    for i in range(n):
        a = 2 * math.pi * i / n
        k = 1.0 + wob * math.sin(a * 3 + seed) * math.cos(a * 2 + seed * 1.7)
        out.append((cx + rx * k * math.cos(a), cy + ry * k * math.sin(a)))
    return out