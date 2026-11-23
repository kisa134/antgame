# Living flat (docs/level/LIFE.md): the models of the living details, made here (no generated scans).
# Frame: 1 Blender unit = 100 uu (the map v4 convention); the flat is at ant scale, 1 real cm = 19 uu = 0.19 BU.
#   blender --background --factory-startup --python blender_life_props.py -- <SourceArt/Map4/props>
# Material slots are named after the game's looks (Map4Look.cpp); "M4Prop" slots get a printed texture from the code.
import bpy, bmesh, sys, os, math, random, json
from mathutils import Vector, Matrix, noise
OUT = sys.argv[sys.argv.index("--") + 1]
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "map4", "blender_m4_common.py")).read())
CM = 0.19   # one real centimetre in Blender units
ONLY = [s for s in os.environ.get("LIFE_PROPS", "").split(",") if s]
random.seed(11)

def want(n):
    return not ONLY or n in ONLY

def box(bm, c, s, rot=None):
    r = bmesh.ops.create_cube(bm, size=1.0)
    for v in r["verts"]:
        p = Vector((v.co.x * s[0], v.co.y * s[1], v.co.z * s[2]))
        if rot: p = rot @ p
        v.co = p + Vector(c)
    return r["verts"]

def cyl(bm, c, r, h, segs=16, axis="Z", r2=None):
    res = bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segs, radius1=r, radius2=r if r2 is None else r2, depth=h)
    for v in res["verts"]:
        p = v.co.copy()
        if axis == "X": p = Vector((p.z, p.y, p.x))
        elif axis == "Y": p = Vector((p.x, p.z, p.y))
        v.co = p + Vector(c)
    return res["verts"]

def sphere(bm, c, r, sx=1, sy=1, sz=1, segs=12, rings=8):
    res = bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=rings, radius=r)
    for v in res["verts"]:
        v.co = Vector((v.co.x * sx, v.co.y * sy, v.co.z * sz)) + Vector(c)
    return res["verts"]

def tube(bm, pts, r, segs=6):
    """A thin tube along a polyline (wires, hairs, threads)."""
    for a, b in zip(pts[:-1], pts[1:]):
        a, b = Vector(a), Vector(b)
        d = b - a
        if d.length < 1e-6: continue
        q = d.to_track_quat("Z", "Y")
        res = bmesh.ops.create_cone(bm, cap_ends=False, segments=segs, radius1=r, radius2=r, depth=d.length)
        for v in res["verts"]: v.co = q @ v.co + (a + b) * 0.5

def quad_uv(ob, axes=("x", "y"), flip_v=True, flip_u=False):
    """Planar UVs over the bounds: printed things lie in XY and face +Z, the print's top towards Blender +Y. In the game (the FBX
    import mirrors Y) that is: right = +X, up = -Y, front = +Z; the code orients them with MakeFromXY(Normal ^ Up, -Up)."""
    me = ob.data
    if not me.uv_layers: me.uv_layers.new()
    uvl = me.uv_layers.active.data
    co = [v.co for v in me.vertices]
    ia, ib = "xyz".index(axes[0]), "xyz".index(axes[1])
    amin, amax = min(c[ia] for c in co), max(c[ia] for c in co)
    bmin, bmax = min(c[ib] for c in co), max(c[ib] for c in co)
    for p in me.polygons:
        for li in p.loop_indices:
            c = me.vertices[me.loops[li].vertex_index].co
            u = (c[ia] - amin) / max(1e-6, amax - amin)
            v = (c[ib] - bmin) / max(1e-6, bmax - bmin)
            uvl[li].uv = (1.0 - u if flip_u else u, 1.0 - v if flip_v else v)

# ---------------------------------------------------------------- utility: a quad with real UVs (screens, notes, prints)
if want("SM_Life_Quad"):
    clear(); bm = bmesh.new()
    vs = [bm.verts.new((x, y, 0)) for x, y in ((-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5))]
    bm.faces.new(vs)
    ob = obj(bm, "SM_Life_Quad"); quad_uv(ob)
    export(ob, "SM_Life_Quad", slots=[("M4Prop", None)], smooth=False)

# ---------------------------------------------------------------- clock hand (pivot at the origin, points along +Z, lies in XZ, face +Y)
if want("SM_Life_ClockHand"):
    clear(); bm = bmesh.new()
    prof = [(-0.18, 0.05), (0.0, 0.07), (0.75, 0.045), (1.0, 0.0)]
    top = [bm.verts.new((w, 0.0, z)) for z, w in prof] + [bm.verts.new((-w, 0.0, z)) for z, w in reversed(prof)]
    f = bm.faces.new(top)
    ext = bmesh.ops.extrude_face_region(bm, geom=[f])
    for v in [g for g in ext["geom"] if isinstance(g, bmesh.types.BMVert)]: v.co.y += 0.02
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    cyl(bm, (0, 0.01, 0), 0.08, 0.03, 16, "Y")
    ob = obj(bm, "SM_Life_ClockHand"); uv(ob)
    export(ob, "SM_Life_ClockHand", slots=[("BlackGlass", None)], smooth=False)

# ---------------------------------------------------------------- a bird (body + head + beak + tail; wings apart). Unit: 1 BU long, nose +X
if want("SM_Life_Bird"):
    clear(); bm = bmesh.new()
    sphere(bm, (0, 0, 0), 0.22, 1.6, 1.0, 1.0, 16, 10)
    sphere(bm, (0.36, 0, 0.13), 0.14, 1.0, 0.95, 1.0, 14, 9)
    cone = bmesh.ops.create_cone(bm, cap_ends=True, segments=10, radius1=0.045, radius2=0.0, depth=0.12)
    for v in cone["verts"]: v.co = Vector((v.co.z + 0.54, v.co.y, v.co.x + 0.11))
    tp = ((-0.3, -0.06, 0.02), (-0.3, 0.06, 0.02), (-0.62, 0.13, 0.07), (-0.62, -0.13, 0.07))
    bm.faces.new([bm.verts.new(p) for p in tp]); bm.faces.new([bm.verts.new((p[0], p[1], p[2] - 0.01)) for p in reversed(tp)])
    for s in (-1, 1):
        tube(bm, [(0.03, s * 0.07, -0.2), (0.0, s * 0.08, -0.34), (0.06, s * 0.08, -0.36)], 0.012, 5)
    sphere(bm, (0.44, 0.085, 0.17), 0.022, 1, 1, 1, 8, 6); sphere(bm, (0.44, -0.085, 0.17), 0.022, 1, 1, 1, 8, 6)
    ob = obj(bm, "SM_Life_Bird"); uv(ob)
    export(ob, "SM_Life_Bird", slots=[("Plinth", lambda c, p: c.x > 0.47 or (abs(c.z - 0.17) < 0.03 and c.x > 0.42 and abs(abs(c.y) - 0.085) < 0.03)),
                                     ("Bone", lambda c, p: c.z < -0.25), ("M4Prop", None)])
if want("SM_Life_BirdWing"):
    clear(); bm = bmesh.new()
    pts = [(0.12, 0, 0), (0.06, 0.55, 0.02), (-0.05, 0.62, 0.03), (-0.2, 0.3, 0.0), (-0.16, 0, 0)]
    vs = [bm.verts.new(p) for p in pts]
    f = bm.faces.new(vs); bm.faces.new(list(reversed([bm.verts.new(Vector(p) + Vector((0, 0, -0.01))) for p in pts])))
    ob = obj(bm, "SM_Life_BirdWing"); uv(ob)
    export(ob, "SM_Life_BirdWing", slots=[("M4Prop", None)], smooth=False, extra={"two_sided": True})

# ---------------------------------------------------------------- the kitchen radio (28 x 10 x 16 cm, front -Y, sits on z 0)
if want("SM_Life_Radio"):
    clear(); bm = bmesh.new()
    W_, D_, H_ = 28 * CM, 10 * CM, 16 * CM
    body = box(bm, (0, 0, H_ / 2), (W_, D_, H_))
    bmesh.ops.bevel(bm, geom=[e for e in bm.edges], offset=1.2 * CM, segments=3, affect="EDGES")
    for k in range(9):
        box(bm, (-W_ * 0.2, -D_ / 2 - 0.05 * CM, H_ * 0.18 + k * H_ * 0.075), (W_ * 0.48, 0.3 * CM, 0.35 * CM))
    box(bm, (W_ * 0.27, -D_ / 2 - 0.1 * CM, H_ * 0.62), (W_ * 0.34, 0.4 * CM, H_ * 0.22))
    for x in (0.18, 0.36):
        cyl(bm, (W_ * x, -D_ / 2 - 0.9 * CM, H_ * 0.28), 1.5 * CM, 1.6 * CM, 20, "Y")
    tube(bm, [(-W_ * 0.38, 0, H_), (-W_ * 0.36, 0, H_ + 4 * CM), (W_ * 0.36, 0, H_ + 4 * CM), (W_ * 0.38, 0, H_)], 0.6 * CM, 8)
    tube(bm, [(W_ * 0.42, D_ * 0.2, H_), (W_ * 0.55, D_ * 0.2, H_ + 18 * CM)], 0.25 * CM, 6)
    ob = obj(bm, "SM_Life_Radio"); uv(ob)
    export(ob, "SM_Life_Radio", slots=[
        ("Brass", lambda c, p: c.y < -D_ / 2 - 0.4 * CM and c.x > W_ * 0.1 and c.z < H_ * 0.45),
        ("BlackGlass", lambda c, p: c.y < -D_ / 2 and c.x > W_ * 0.08 and c.z > H_ * 0.45 and c.z < H_ * 0.8),
        ("Steel", lambda c, p: c.z > H_ + 0.3 * CM),
        ("Paper", lambda c, p: c.y < -D_ / 2 + 0.1 * CM and c.x < W_ * 0.06 and c.z < H_ * 0.85),
        ("Teal", None)])

# ---------------------------------------------------------------- the music box (10 x 7 x 5 cm, lid open at the back) and its turning star
if want("SM_Life_MusicBox"):
    clear(); bm = bmesh.new()
    W_, D_, H_ = 10 * CM, 7 * CM, 5 * CM
    for c, s in (((0, 0, 0.4 * CM), (W_, D_, 0.8 * CM)), ((0, -D_ / 2 + 0.3 * CM, H_ / 2), (W_, 0.6 * CM, H_)), ((0, D_ / 2 - 0.3 * CM, H_ / 2), (W_, 0.6 * CM, H_)),
                 ((-W_ / 2 + 0.3 * CM, 0, H_ / 2), (0.6 * CM, D_, H_)), ((W_ / 2 - 0.3 * CM, 0, H_ / 2), (0.6 * CM, D_, H_))):
        box(bm, c, s)
    lid = Matrix.Rotation(math.radians(-70), 3, "X")
    box(bm, Vector((0, D_ / 2, H_)) + lid @ Vector((0, -D_ / 2 + 0.4 * CM, 0.3 * CM)) * -1 * 0 + Vector((0, D_ / 2 * 0.35, D_ * 0.47)), (W_, D_, 0.6 * CM), lid)
    box(bm, (0, 0, H_ * 0.55), (W_ - 1.2 * CM, D_ - 1.2 * CM, 0.3 * CM))
    cyl(bm, (W_ / 2 + 0.6 * CM, 0, H_ * 0.5), 0.25 * CM, 1.2 * CM, 8, "X")
    box(bm, (W_ / 2 + 1.2 * CM, 0, H_ * 0.5), (0.3 * CM, 2.4 * CM, 0.5 * CM))
    ob = obj(bm, "SM_Life_MusicBox"); uv(ob, "cube")
    export(ob, "SM_Life_MusicBox", slots=[("Brass", lambda c, p: c.x > W_ / 2 + 0.05 * CM), ("Gum", lambda c, p: abs(c.z - H_ * 0.55) < 0.2 * CM and abs(c.x) < W_ / 2 - 0.5 * CM), ("Walnut", None)])
if want("SM_Life_MusicStar"):
    clear(); bm = bmesh.new()
    pts = []
    for k in range(10):
        r = (1.4 if k % 2 == 0 else 0.6) * CM
        a = math.pi / 2 + k * math.pi / 5
        pts.append((r * math.cos(a), 0, 3.2 * CM + r * math.sin(a)))
    f = bm.faces.new([bm.verts.new(p) for p in pts])
    ext = bmesh.ops.extrude_face_region(bm, geom=[f])
    for v in [g for g in ext["geom"] if isinstance(g, bmesh.types.BMVert)]: v.co.y += 0.4 * CM
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    cyl(bm, (0, 0.2 * CM, 0.9 * CM), 0.15 * CM, 1.8 * CM, 8)
    ob = obj(bm, "SM_Life_MusicStar"); uv(ob)
    export(ob, "SM_Life_MusicStar", slots=[("Brass", lambda c, p: c.z < 1.85 * CM), ("Cheese", None)])

# ---------------------------------------------------------------- the kid's tablet (24 x 17 x 0.9 cm, screen +Z) in a chunky blue case
if want("SM_Life_Tablet"):
    clear(); bm = bmesh.new()
    box(bm, (0, 0, 0.45 * CM), (26 * CM, 19 * CM, 0.9 * CM))
    bmesh.ops.bevel(bm, geom=[e for e in bm.edges], offset=1.4 * CM, segments=4, affect="EDGES")
    box(bm, (0, 0, 0.92 * CM), (22.4 * CM, 15.6 * CM, 0.06 * CM))
    ob = obj(bm, "SM_Life_Tablet"); uv(ob, "cube")
    export(ob, "SM_Life_Tablet", slots=[("BlackGlass", lambda c, p: c.z > 0.89 * CM and abs(c.x) < 11.5 * CM and abs(c.y) < 8 * CM), ("PlasticBlue", None)])

# ---------------------------------------------------------------- a pennant of the birthday banner (18 x 24 cm, printed: one letter each)
if want("SM_Life_Pennant"):
    clear(); bm = bmesh.new()
    rows = 6
    grid = []
    for j in range(rows + 1):
        t = j / rows
        w = 9 * CM * (1 - t)
        row = [bm.verts.new((x, -24 * CM * t, 0.4 * CM * math.sin(t * 2.5) * (x / max(w, 1e-4)) ** 2)) for x in (-w, 0.0, w)] if j < rows else [bm.verts.new((0, -24 * CM, 0))]
        grid.append(row)
    for j in range(rows - 1):
        for i in range(2):
            bm.faces.new((grid[j][i], grid[j + 1][i], grid[j + 1][i + 1], grid[j][i + 1]))
    for i in range(2):
        bm.faces.new((grid[rows - 1][i], grid[rows][0], grid[rows - 1][i + 1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        if f.normal.z < 0: f.normal_flip()
    ob = obj(bm, "SM_Life_Pennant"); quad_uv(ob, ("x", "y"))
    export(ob, "SM_Life_Pennant", slots=[("M4Prop", None)], extra={"two_sided": True})

# ---------------------------------------------------------------- a sticky note (7.6 cm, the lower edge curls away from the door)
if want("SM_Life_Note"):
    clear(); bm = bmesh.new()
    n = 6; s = 7.6 * CM
    grid = [[bm.verts.new((-s / 2 + s * i / n, -s / 2 + s * j / n, (0.0 if j > 1 else 0.5 * CM * (2 - j) ** 1.6 / 2.0))) for i in range(n + 1)] for j in range(n + 1)]
    for j in range(n):
        for i in range(n):
            bm.faces.new((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]))
    for f in bm.faces:
        f.normal_update()
        if f.normal.z < 0: f.normal_flip()
    ob = obj(bm, "SM_Life_Note"); quad_uv(ob, ("x", "y"))
    export(ob, "SM_Life_Note", slots=[("M4Prop", None)], extra={"two_sided": True})

# ---------------------------------------------------------------- micro details at ant scale
if want("SM_Life_Earring"):
    clear(); bm = bmesh.new()
    pts = [(0.8 * CM * math.cos(a), 0.8 * CM * math.sin(a), 0.12 * CM) for a in [k * 2 * math.pi / 28 for k in range(26)]]
    tube(bm, pts, 0.11 * CM, 8)
    sphere(bm, (0.8 * CM, 0, 0.2 * CM), 0.22 * CM, 1, 1, 1, 10, 8)
    ob = obj(bm, "SM_Life_Earring"); uv(ob)
    export(ob, "SM_Life_Earring", slots=[("Gum", lambda c, p: c.x > 0.6 * CM and abs(c.y) < 0.25 * CM), ("Brass", None)])
if want("SM_Life_HairPin"):
    clear(); bm = bmesh.new()
    a = [(0, 0, 0.1 * CM), (5 * CM, 0, 0.1 * CM)]
    b = [(5 * CM, 0, 0.1 * CM)] + [(5 * CM - 0.4 * CM * k, 0.4 * CM + 0.12 * CM * (1 if k % 2 else -1), 0.1 * CM) for k in range(1, 12)]
    tube(bm, [(0, 0, 0.1 * CM), (5 * CM, 0, 0.1 * CM)], 0.06 * CM, 6)
    tube(bm, b, 0.06 * CM, 6)
    tube(bm, [(5 * CM, 0, 0.1 * CM), (5.25 * CM, 0.2 * CM, 0.1 * CM), (5 * CM, 0.4 * CM, 0.1 * CM)], 0.06 * CM, 6)
    ob = obj(bm, "SM_Life_HairPin"); uv(ob)
    export(ob, "SM_Life_HairPin", slots=[("Iron", None)])
if want("SM_Life_Brick"):
    clear(); bm = bmesh.new()
    L, Wd, Hh = 3.2 * CM, 1.6 * CM, 0.96 * CM
    box(bm, (0, 0, Hh / 2), (L, Wd, Hh))
    bmesh.ops.bevel(bm, geom=[e for e in bm.edges], offset=0.05 * CM, segments=1, affect="EDGES")
    for i in range(4):
        for j in range(2):
            cyl(bm, (-1.2 * CM + i * 0.8 * CM, -0.4 * CM + j * 0.8 * CM, Hh + 0.09 * CM), 0.24 * CM, 0.18 * CM, 12)
    ob = obj(bm, "SM_Life_Brick"); uv(ob, "cube")
    export(ob, "SM_Life_Brick", slots=[("Red", None)], extra={"complex_simple": True})
if want("SM_Life_Receipt"):
    clear(); bm = bmesh.new()
    n = 12; L = 20 * CM; Wd = 8 * CM
    grid = [[bm.verts.new((x, L * j / n, 0.05 * CM + 1.4 * CM * max(0.0, (j / n - 0.55)) ** 1.5 * 6 + 0.3 * CM * math.sin(j * 1.3) ** 2)) for x in (-Wd / 2, 0, Wd / 2)] for j in range(n + 1)]
    for j in range(n):
        for i in range(2):
            bm.faces.new((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]))
    ob = obj(bm, "SM_Life_Receipt"); quad_uv(ob, ("x", "y"))
    export(ob, "SM_Life_Receipt", slots=[("M4Prop", None)], extra={"two_sided": True})
if want("SM_Life_Sticker"):
    clear(); bm = bmesh.new()
    pts = []
    for k in range(10):
        r = (1.5 if k % 2 == 0 else 0.65) * CM
        a = math.pi / 2 + k * math.pi / 5
        pts.append(bm.verts.new((r * math.cos(a), r * math.sin(a), 0.03 * CM)))
    bm.faces.new(pts)
    ob = obj(bm, "SM_Life_Sticker"); uv(ob)
    export(ob, "SM_Life_Sticker", slots=[("Cheese", None)], smooth=False)
if want("SM_Life_HairTuft"):
    clear(); bm = bmesh.new()
    for k in range(26):
        a = random.uniform(0, 2 * math.pi); L = random.uniform(1.2, 2.6) * CM
        p0 = Vector((random.uniform(-0.3, 0.3) * CM, random.uniform(-0.3, 0.3) * CM, 0.05 * CM))
        d = Vector((math.cos(a), math.sin(a), 0))
        bend = Vector((-d.y, d.x, 0)) * random.uniform(-0.5, 0.5) * CM
        pts = [p0 + d * L * t + bend * math.sin(t * 3) + Vector((0, 0, 0.15 * CM * math.sin(t * 3.1))) for t in (0, 0.33, 0.66, 1.0)]
        tube(bm, pts, 0.025 * CM, 4)
    ob = obj(bm, "SM_Life_HairTuft"); uv(ob)
    export(ob, "SM_Life_HairTuft", slots=[("Biscuit", None)])
if want("SM_Life_NailClip"):
    clear(); bm = bmesh.new()
    outer = [(1.0 * CM * math.cos(a), 0.55 * CM * math.sin(a)) for a in [math.pi * (0.1 + 0.8 * k / 12) for k in range(13)]]
    inner = [(0.9 * CM * math.cos(a), 0.38 * CM * math.sin(a)) for a in [math.pi * (0.1 + 0.8 * k / 12) for k in reversed(range(13))]]
    f = bm.faces.new([bm.verts.new((x, y, 0.0)) for x, y in outer + inner])
    ext = bmesh.ops.extrude_face_region(bm, geom=[f])
    for v in [g for g in ext["geom"] if isinstance(g, bmesh.types.BMVert)]: v.co.z += 0.06 * CM
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = obj(bm, "SM_Life_NailClip"); uv(ob)
    export(ob, "SM_Life_NailClip", slots=[("Bone", None)])
if want("SM_Life_DeadSpider"):
    clear(); bm = bmesh.new()
    sphere(bm, (-0.25 * CM, 0, 0.3 * CM), 0.35 * CM, 1.3, 1.0, 0.8, 12, 8)
    sphere(bm, (0.3 * CM, 0, 0.25 * CM), 0.22 * CM, 1.1, 1.0, 0.8, 10, 6)
    for s in (-1, 1):
        for k in range(4):
            a = math.radians(-50 + k * 33)
            base = Vector((0.3 * CM + 0.05 * CM * k, s * 0.12 * CM, 0.25 * CM))
            dirv = Vector((math.cos(a), s * math.sin(abs(a)) * 0.3 + s * 0.6, 0)).normalized()
            knee = base + dirv * 0.6 * CM + Vector((0, 0, 0.25 * CM))
            tip = base + dirv * 0.35 * CM + Vector((0, -s * 0.15 * CM, 0.5 * CM))
            tube(bm, [base, knee, tip], 0.04 * CM, 5)
    ob = obj(bm, "SM_Life_DeadSpider"); uv(ob)
    export(ob, "SM_Life_DeadSpider", slots=[("SpiderLegDark", lambda c, p: c.z > 0.3 * CM and (c - Vector((0, 0, 0.3 * CM))).length > 0.45 * CM), ("Leather", None)])
if want("SM_Life_Web"):
    clear(); bm = bmesh.new()
    # a corner web: the walls are -X (x = 0 plane facing +X) and -Y (y = 0 facing +Y), the floor at z = 0; spans 8 cm
    R = 8 * CM
    spokes = []
    for k in range(7):
        a = math.radians(5 + k * 13.3)
        spokes.append(Vector((math.cos(a), math.sin(a), 0.35)).normalized())
    for d in spokes:
        tube(bm, [(0.02 * CM, 0.02 * CM, 0.1 * CM), tuple(d * R)], 0.012 * CM, 3)
    for ring in range(1, 7):
        r = R * ring / 7.0
        pts = [tuple(d * r * (1 + 0.04 * math.sin(ring * 3 + i))) for i, d in enumerate(spokes)]
        tube(bm, pts, 0.01 * CM, 3)
    ob = obj(bm, "SM_Life_Web"); uv(ob)
    export(ob, "SM_Life_Web", slots=[("Silk", None)])
if want("SM_Life_CableTie"):
    clear(); bm = bmesh.new()
    pts = [(2.0 * CM * math.cos(a), 2.0 * CM * math.sin(a), 0.1 * CM) for a in [k * 2 * math.pi / 24 for k in range(24)]]
    pts += [(2.0 * CM, -0.3 * CM, 0.1 * CM), (6.0 * CM, -1.0 * CM, 0.12 * CM), (11.0 * CM, -0.6 * CM, 0.1 * CM)]
    for a, b in zip(pts[:-1], pts[1:]):
        box(bm, ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, 0.1 * CM), (max(0.05 * CM, (Vector(b) - Vector(a)).length), 0.45 * CM, 0.12 * CM),
            Matrix.Rotation(math.atan2(b[1] - a[1], b[0] - a[0]), 3, "Z"))
    box(bm, (2.1 * CM, 0, 0.25 * CM), (0.6 * CM, 0.7 * CM, 0.4 * CM))
    ob = obj(bm, "SM_Life_CableTie"); uv(ob)
    export(ob, "SM_Life_CableTie", slots=[("Tint/Enamel", None)])
if want("SM_Life_Gum"):
    clear(); bm = bmesh.new()
    vs = sphere(bm, (0, 0, 0), 0.9 * CM, 1.3, 1.0, 0.4, 16, 10)
    for v in vs:
        v.co += v.co.normalized() * noise.noise(v.co * 9.0) * 0.25 * CM
    ob = obj(bm, "SM_Life_Gum"); uv(ob)
    export(ob, "SM_Life_Gum", slots=[("Gum", None)])
if want("SM_Life_Shaving"):
    clear(); bm = bmesh.new()
    pts = []
    for k in range(40):
        t = k / 39.0; a = t * 3.6 * math.pi; r = (0.2 + 0.6 * t) * CM
        pts.append((r * math.cos(a), r * math.sin(a), 0.08 * CM + 0.25 * CM * t))
    for a, b in zip(pts[:-1], pts[1:]):
        box(bm, ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2), (max(0.03 * CM, (Vector(b) - Vector(a)).length * 1.05), 0.05 * CM, 0.3 * CM),
            Matrix.Rotation(math.atan2(b[1] - a[1], b[0] - a[0]), 3, "Z"))
    ob = obj(bm, "SM_Life_Shaving"); uv(ob)
    export(ob, "SM_Life_Shaving", slots=[("Red", lambda c, p: math.hypot(c.x, c.y) > 0.6 * CM), ("Wicker", None)])
print("LIFE_PROPS_DONE")
