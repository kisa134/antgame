# Ant Kitchen level v3: shared helpers for the new v3 pieces (Tools/blender/level_v3_pieces.py). Built on kitchen_lib.py (not edited):
# bmesh only, same export / manifest code as the kitchen set. Numbers are game units (uu) in the piece-local frame: x right, y back (+Y = wall
# side, front = -Y), z up, origin = the placement point of Tools/level/level_v3_layout.PIECES. Cloth pieces are single-layer quad grids.
import sys, os, math, random
HERE = os.path.dirname(os.path.abspath(__file__))
for _p in (HERE, os.path.join(HERE, "..", "level")):
    if _p not in sys.path:
        sys.path.insert(0, _p)
import bmesh
from mathutils import Vector, Matrix
import kitchen_lib as L
import level_v3_layout as V3
K = L.K
piece, mat = L.piece, L.mat

# palette: slot names KitchenArt.cpp already has a look for, plus the new ones (new=True -> listed as new_slot in the manifest)
for _n, _c, _r, _m, _new, _e in (
        ("Cabinet", (0.42, 0.60, 0.44), 0.5, 0, False, 0), ("Butcher", (0.68, 0.40, 0.16), 0.4, 0, False, 0), ("Walnut", (0.33, 0.20, 0.10), 0.5, 0, False, 0),
        ("Plinth", (0.08, 0.08, 0.09), 0.7, 0, False, 0), ("Brass", (0.86, 0.62, 0.22), 0.3, 1, False, 0), ("Steel", (0.72, 0.74, 0.76), 0.28, 1, False, 0),
        ("Chrome", (0.9, 0.9, 0.92), 0.12, 1, False, 0), ("Enamel", (0.95, 0.94, 0.90), 0.25, 0, False, 0), ("Red", (0.75, 0.10, 0.08), 0.35, 0, False, 0),
        ("Blue", (0.1, 0.25, 0.75), 0.35, 0, False, 0), ("MugBlue", (0.15, 0.35, 0.6), 0.3, 0, False, 0), ("Fabric", (0.96, 0.72, 0.16), 0.9, 0, False, 0),
        ("Terracotta", (0.72, 0.35, 0.20), 0.8, 0, False, 0), ("Leaf", (0.16, 0.50, 0.18), 0.5, 0, False, 0), ("Paper", (0.97, 0.96, 0.88), 0.9, 0, False, 0),
        ("Honey", (0.90, 0.55, 0.05), 0.15, 0, False, 0), ("Jam", (0.55, 0.05, 0.12), 0.15, 0, False, 0), ("Sugar", (0.95, 0.95, 0.95), 0.6, 0, False, 0),
        ("ShelfGlass", (0.62, 0.82, 0.88), 0.1, 0, False, 0), ("PlasticBlue", (0.12, 0.30, 0.75), 0.35, 0, False, 0), ("Toast", (0.80, 0.55, 0.25), 0.85, 0, False, 0),
        ("CakePink", (0.96, 0.58, 0.70), 0.6, 0, False, 0), ("Cardboard", (0.70, 0.52, 0.32), 0.9, 0, False, 0), ("Leather", (0.24, 0.17, 0.13), 0.6, 0, False, 0),
        ("Teal", (0.05, 0.45, 0.48), 0.4, 0, False, 0), ("SockWool", (0.42, 0.46, 0.62), 0.95, 0, False, 0), ("Yarn", (0.78, 0.12, 0.30), 0.95, 0, False, 0),
        ("Sisal", (0.80, 0.66, 0.42), 0.95, 0, False, 0), ("Soil", (0.22, 0.13, 0.07), 0.95, 0, False, 0), ("Stone", (0.60, 0.58, 0.54), 0.85, 0, False, 0),
        # new slots (v3): need a look in KitchenArt.cpp (docs/level/LEVEL_V3_INTEGRATION.md)
        ("ClothCheck", (0.80, 0.22, 0.20), 0.9, 0, True, 0), ("Curtain", (0.86, 0.78, 0.60), 0.9, 0, True, 0), ("Cola", (0.16, 0.05, 0.02), 0.08, 0, True, 0),
        ("PetClear", (0.82, 0.90, 0.88), 0.05, 0, True, 0), ("GiftWrap", (0.20, 0.32, 0.70), 0.5, 0, True, 0), ("Photo", (0.55, 0.62, 0.70), 0.3, 0, True, 0),
        ("Bulb", (1.0, 0.86, 0.60), 0.3, 0, True, 4.0), ("Brine", (0.70, 0.78, 0.45), 0.1, 0, True, 0), ("Latex", (0.85, 0.12, 0.15), 0.25, 0, True, 0),
        ("Plaster", (0.93, 0.92, 0.88), 0.85, 0, True, 0), ("Potato", (0.70, 0.55, 0.32), 0.9, 0, True, 0)):
    mat(_n, _c, _r, _m, _new, _e)

def UB(x0, x1, y0, y1, z0, z1, m, bev=4, seg=1):
    L.box(x0 * K, x1 * K, y0 * K, y1 * K, z0 * K, z1 * K, m, bev * K, seg)
def UR(sx, sy, sz, x, y, z, m, bev=4, seg=1, rot=None):
    L.rbox(sx * K, sy * K, sz * K, x * K, y * K, z * K, m, bev * K, seg, rot)
def UZ(r, h, x, y, z0, m, n=16, r2=None, bev=0):            # upright cylinder, base at z0
    L.cyl(r * K, h * K, x * K, y * K, (z0 + h / 2.0) * K, m, "Z", n, bev * K, None if r2 is None else r2 * K, 1)
def UCX(r, h, x, y, z, m, n=12):                            # cylinder along X, centred
    L.cyl(r * K, h * K, x * K, y * K, z * K, m, "X", n)
def UCY(r, h, x, y, z, m, n=12):
    L.cyl(r * K, h * K, x * K, y * K, z * K, m, "Y", n)
def US(r, x, y, z, m, scale=(1, 1, 1), nu=12, nv=8, rot=None):
    L.ball(r * K, x * K, y * K, z * K, m, scale, nu, nv, rot)
def UL(prof, m, x=0, y=0, z=0, n=20, axis="Z", rot=None, closed=False):
    L.lathe([(r * K, h * K) for r, h in prof], m, x * K, y * K, z * K, n, axis, rot, closed)
def UT(pts, r, m, n=8):
    L.tube([(x * K, y * K, z * K) for x, y, z in pts], r * K, m, n)
def UP(poly, a0, a1, m, axis="Z", bev=0):
    L.prism([(a * K, b * K) for a, b in poly], a0 * K, a1 * K, m, axis, None, bev * K)

def sheet(fn, nu, nv, m, keep=None):
    """Single-layer quad grid (cloth): fn(u, v) -> (x, y, z) in uu, u, v in [0, 1]. keep(i, j) -> False drops a quad (tears)."""
    bm = bmesh.new()
    vs = [[bm.verts.new(tuple(c * K for c in fn(i / float(nu), j / float(nv)))) for i in range(nu + 1)] for j in range(nv + 1)]
    for j in range(nv):
        for i in range(nu):
            if keep and not keep(i, j): continue
            bm.faces.new((vs[j][i], vs[j][i + 1], vs[j + 1][i + 1], vs[j + 1][i]))
    loose = [v for v in bm.verts if not v.link_faces]
    if loose: bmesh.ops.delete(bm, geom=loose, context="VERTS")
    i = L._slot(m)
    for f in bm.faces: f.material_index = i
    bm.normal_update()
    L._merge(L.S.stack[-1], bm)

def blob(cx, cy, rx, ry, n, wob, seed):
    out = []
    for i in range(n):
        a = 2 * math.pi * i / n
        k = 1.0 + wob * math.sin(a * 3 + seed) * math.cos(a * 2 + seed * 1.7)
        out.append((cx + rx * k * math.cos(a), cy + ry * k * math.sin(a)))
    return out

def pin_group(name, test):
    """Vertex group 'pin' on a built piece (cloth): the vertices for which test(local uu co) is true."""
    ob = L.PIECES[name]["ob"]
    g = ob.vertex_groups.new(name="pin")
    idx = [v.index for v in ob.data.vertices if test(Vector(v.co) / K)]
    if idx: g.add(idx, 1.0, "REPLACE")
    L.PIECES[name]["note"] = (L.PIECES[name].get("note") or "") + " | cloth: vertex group 'pin' = %d vertices" % len(idx)
    return len(idx)

def done(name, note=None, motion=None, origin=(0, 0, 0)):
    pc = V3.PIECES.get(name, {})
    n = note or pc.get("ru")
    extra = []
    if pc.get("mat"): extra.append("break class: %s" % pc["mat"])
    if pc.get("cloth"): extra.append("cloth grid %s, pin: %s" % (pc["cloth"]["grid"], pc["cloth"]["pin"]))
    if pc.get("pivot"): extra.append("pivot: %s" % pc["pivot"])
    if pc.get("state"): extra.append("state: %s" % pc["state"])
    return piece(name, origin=origin, motion=motion, note=(n + (" | " + "; ".join(extra) if extra else "")))
