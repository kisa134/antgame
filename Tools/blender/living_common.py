# Ant Kitchen: shared helpers for the living room / yard / street set. Same family as kitchen_set.py
# (rounded bevels, the same palette and material slot names), but every piece ends up as ONE joined,
# closed mesh with its own origin, ready for FBX export and for a manifest entry.
import bpy, bmesh, sys, os, math, json, random
from mathutils import Vector, Matrix, Euler

S = 1900.0   # Unreal units per metre in the game (KitchenBuilder.cpp box sizes / 1900 = metres)

bpy.ops.wm.read_factory_settings(use_empty=True)
MATS, MATINFO = {}, {}

def mat(name, color, rough=0.5, metal=0.0, emit=0.0):
    if name in MATS:
        return MATS[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if emit > 0:
        try:
            b.inputs["Emission Color"].default_value = (*color, 1)
            b.inputs["Emission Strength"].default_value = emit
        except Exception:
            pass
    m.diffuse_color = (*color, 1)
    MATS[name] = m
    MATINFO[name] = {"rgb": [round(c, 3) for c in color], "roughness": rough, "metallic": metal, "emission": emit}
    return m

# ---- the kitchen_set.py palette (same names, same values) ----
CABINET = mat("Cabinet", (0.42, 0.60, 0.44), 0.5)
CABINET_D = mat("CabinetInset", (0.34, 0.51, 0.37), 0.55)
BUTCHER = mat("Butcher", (0.68, 0.40, 0.16), 0.4)
WALNUT = mat("Walnut", (0.33, 0.20, 0.10), 0.5)
PLINTH = mat("Plinth", (0.08, 0.08, 0.09), 0.7)
BRASS = mat("Brass", (0.86, 0.62, 0.22), 0.3, 1.0)
STEEL = mat("Steel", (0.72, 0.74, 0.76), 0.28, 1.0)
CHROME = mat("Chrome", (0.9, 0.9, 0.92), 0.12, 1.0)
ENAMEL = mat("Enamel", (0.95, 0.94, 0.90), 0.25)
GLASS = mat("BlackGlass", (0.02, 0.02, 0.025), 0.08)
IRON = mat("Iron", (0.05, 0.05, 0.05), 0.6, 0.6)
RED = mat("Red", (0.75, 0.10, 0.08), 0.35)
FABRIC = mat("Fabric", (0.96, 0.72, 0.16), 0.9)
TERRA = mat("Terracotta", (0.72, 0.35, 0.20), 0.8)
LEAF = mat("Leaf", (0.16, 0.50, 0.18), 0.5)
PAPER = mat("Paper", (0.97, 0.96, 0.88), 0.9)
BULB = mat("Bulb", (1.0, 0.85, 0.55), 0.3, 0.0, 12.0)
# ---- new for this set ----
SOFA = mat("SofaFabric", (0.09, 0.33, 0.40), 0.95)
CUSHION = mat("Cushion", (0.14, 0.46, 0.50), 0.95)
CARPET = mat("Carpet", (0.66, 0.13, 0.12), 0.95)
CARPET_T = mat("CarpetTrim", (0.93, 0.85, 0.66), 0.95)
TEAL = mat("Teal", (0.05, 0.45, 0.48), 0.4)
NAVY = mat("Navy", (0.08, 0.14, 0.36), 0.5)
SCREEN = mat("Screen", (0.05, 0.17, 0.45), 0.15, 0.0, 0.7)
SHADE = mat("LampShade", (0.96, 0.86, 0.62), 0.8, 0.0, 0.6)
SOIL = mat("Soil", (0.22, 0.13, 0.07), 0.95)
LEAF_L = mat("LeafLight", (0.36, 0.62, 0.18), 0.55)
YARN = mat("Yarn", (0.78, 0.12, 0.30), 0.95)
WICKER = mat("Wicker", (0.74, 0.52, 0.26), 0.8)
SOCK = mat("SockWool", (0.42, 0.46, 0.62), 0.95)
SISAL = mat("Sisal", (0.80, 0.66, 0.42), 0.95)
CRISPS = mat("Crisps", (0.96, 0.76, 0.22), 0.6)
BISCUIT = mat("Biscuit", (0.74, 0.48, 0.20), 0.8)
GRASS = mat("Grass", (0.30, 0.56, 0.16), 0.9)
GRASS_D = mat("GrassDark", (0.15, 0.38, 0.10), 0.9)
BARK = mat("Bark", (0.36, 0.22, 0.12), 0.9)
STONE = mat("Stone", (0.60, 0.58, 0.54), 0.85)
FENCE = mat("FencePaint", (0.94, 0.91, 0.82), 0.6)
BIN = mat("BinGreen", (0.10, 0.34, 0.18), 0.45)
BAG = mat("TrashBag", (0.03, 0.03, 0.035), 0.35)
TENNIS = mat("TennisBall", (0.78, 0.86, 0.14), 0.95)
BONE = mat("Bone", (0.94, 0.90, 0.78), 0.6)
SIDING = mat("Siding", (0.95, 0.82, 0.56), 0.8)
CONCRETE = mat("Concrete", (0.66, 0.65, 0.62), 0.9)
KERB = mat("Kerb", (0.50, 0.49, 0.47), 0.9)
ASPHALT = mat("Asphalt", (0.09, 0.09, 0.10), 0.9)
ROADPAINT = mat("RoadPaint", (0.93, 0.91, 0.82), 0.7)
CARPAINT = mat("CarPaint", (0.08, 0.52, 0.58), 0.22)
RUBBER = mat("Rubber", (0.03, 0.03, 0.035), 0.85)
HEADLIGHT = mat("Headlight", (1.0, 0.94, 0.75), 0.2, 0.0, 2.0)
ICE = mat("IceCream", (0.97, 0.62, 0.74), 0.4)
WAFFLE = mat("Waffle", (0.82, 0.57, 0.26), 0.75)
GUM = mat("Gum", (0.95, 0.42, 0.66), 0.3)

def grain(m, stretch=9.0):
    # Streaks along X: reads as wood grain in the preview only (the engine has its own wood material).
    try:
        nt = m.node_tree
        b = nt.nodes["Principled BSDF"]
        base = tuple(b.inputs["Base Color"].default_value)
        tc = nt.nodes.new("ShaderNodeTexCoord")
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (0.7, stretch, stretch)
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = 7.0
        nz.inputs["Detail"].default_value = 7.0
        mix = nt.nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.inputs[6].default_value = (base[0] * 0.62, base[1] * 0.58, base[2] * 0.55, 1)
        mix.inputs[7].default_value = (min(1, base[0] * 1.12), min(1, base[1] * 1.1), min(1, base[2] * 1.05), 1)
        nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
        nt.links.new(mp.outputs["Vector"], nz.inputs["Vector"])
        nt.links.new(nz.outputs[0], mix.inputs[0])
        nt.links.new(mix.outputs[2], b.inputs["Base Color"])
    except Exception as e:
        print("grain skipped:", e)
grain(BUTCHER)
grain(WALNUT, 12.0)

# ------------------------------------------------------------------ parts of the piece being built
CUR, CUTTERS = [], []

def _obj(bm, m, name="p"):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(m)
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    CUR.append(o)
    return o

def _bevel(o, w, seg, lim):
    if w > 0 and seg > 0:
        b = o.modifiers.new("Bevel", "BEVEL")
        b.width = min(w, 0.45 * lim)
        b.segments = seg
        b.limit_method = "ANGLE"
        b.angle_limit = math.radians(35)
    return o

def place(o, loc=None, rot=None, scale=None):
    if loc is not None:
        o.location = loc
    if rot is not None:
        o.rotation_euler = rot
    if scale is not None:
        o.scale = scale
    return o

def _recalc(bm):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])

def rbox(sx, sy, sz, x, y, z, m, bev=0.008, seg=2, rot=None):
    # Rounded box by size and centre.
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co.x *= sx
        v.co.y *= sy
        v.co.z *= sz
    o = place(_obj(bm, m, "box"), (x, y, z), rot)
    return _bevel(o, bev, seg, min(sx, sy, sz))

def bx(x0, x1, y0, y1, z0, z1, m, bev=0.008, seg=2):
    # Rounded box by min/max.
    return rbox(abs(x1 - x0), abs(y1 - y0), abs(z1 - z0), (x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2, m, bev, seg)

AXROT = {"Z": (0, 0, 0), "X": (0, math.pi / 2, 0), "Y": (math.pi / 2, 0, 0)}

def cyl(r, depth, x, y, z, m, axis="Z", verts=16, bev=0.0, r2=None, seg=2, rot=None):
    # Cylinder / cone by centre. r is the radius at the -axis end, r2 at the +axis end ("Y": r2 is at -Y).
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=verts, radius1=r, radius2=(r if r2 is None else r2), depth=depth)
    o = place(_obj(bm, m, "cyl"), (x, y, z), rot if rot is not None else AXROT[axis])
    return _bevel(o, bev, seg, min(r, depth))

def ball(r, x, y, z, m, scale=(1, 1, 1), seg=16, rings=10, rot=None):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=rings, radius=r)
    return place(_obj(bm, m, "ball"), (x, y, z), rot, scale)

def ico(r, x, y, z, m, scale=(1, 1, 1), sub=2, rot=None):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=sub, radius=r)
    return place(_obj(bm, m, "ico"), (x, y, z), rot, scale)

def torus(R, r, m, loc=(0, 0, 0), rot=None, seg=20, rseg=6, scale=None):
    bm = bmesh.new()
    rings = []
    for i in range(seg):
        a = 2 * math.pi * i / seg
        ring = []
        for j in range(rseg):
            b = 2 * math.pi * j / rseg
            rr = R + r * math.cos(b)
            ring.append(bm.verts.new((rr * math.cos(a), rr * math.sin(a), r * math.sin(b))))
        rings.append(ring)
    for i in range(seg):
        A, B = rings[i], rings[(i + 1) % seg]
        for j in range(rseg):
            k = (j + 1) % rseg
            bm.faces.new((A[j], B[j], B[k], A[k]))
    _recalc(bm)
    return place(_obj(bm, m, "torus"), loc, rot, scale)

def lathe(prof, m, segs=20, loc=None, rot=None, closed=False, wob=None, scale=None):
    # Solid of revolution around Z from a profile [(radius, z), ...]. Open profile ends are capped with
    # n-gons; closed=True joins the last point to the first (a ring). wob(angle) scales the radius.
    bm = bmesh.new()
    rings = []
    for (r, z) in prof:
        if r < 1e-6:
            rings.append([bm.verts.new((0, 0, z))])
        else:
            ring = []
            for i in range(segs):
                a = 2 * math.pi * i / segs
                k = wob(a) if wob else 1.0
                ring.append(bm.verts.new((r * k * math.cos(a), r * k * math.sin(a), z)))
            rings.append(ring)
    n = len(rings)
    pairs = [(i, i + 1) for i in range(n - 1)] + ([(n - 1, 0)] if closed else [])
    for a, b in pairs:
        A, B = rings[a], rings[b]
        if len(A) == 1 and len(B) == 1:
            continue
        for i in range(segs):
            j = (i + 1) % segs
            if len(A) == 1:
                bm.faces.new((A[0], B[i], B[j]))
            elif len(B) == 1:
                bm.faces.new((A[i], A[j], B[0]))
            else:
                bm.faces.new((A[i], A[j], B[j], B[i]))
    if not closed:
        for ring in (rings[0], rings[-1]):
            if len(ring) > 1:
                bm.faces.new(ring)
    _recalc(bm)
    return place(_obj(bm, m, "lathe"), loc, rot, scale)

def smooth(pts, sub=4):
    # Catmull-Rom through the points.
    P = [Vector(p) for p in pts]
    out = []
    for i in range(len(P) - 1):
        p0, p1, p2, p3 = P[max(i - 1, 0)], P[i], P[i + 1], P[min(i + 2, len(P) - 1)]
        for s in range(sub):
            t = s / sub
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t))
    out.append(P[-1])
    return out

def tube(pts, rad, m, segs=8, wall=0.0, cap0=True, cap1=True, squash=1.0, up=None):
    # A round rod along a path; wall > 0 makes it a pipe (cap=False leaves that end open with a rim,
    # cap=True closes it like the toe of a sock). squash flattens it along the frame's "up".
    pts = [Vector(p) for p in pts]
    n = len(pts)
    rs = list(rad) if isinstance(rad, (list, tuple)) else [rad] * n
    tans = [(pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]).normalized() for i in range(n)]
    t0 = tans[0]
    ref = Vector(up) if up else (Vector((0, 0, 1)) if abs(t0.z) < 0.9 else Vector((1, 0, 0)))
    nrm = ref
    frames = []
    for i in range(n):
        t = tans[i]
        nrm = nrm - t * nrm.dot(t)
        if nrm.length < 1e-6:
            nrm = t.orthogonal()
        nrm = nrm.normalized()
        frames.append((nrm.copy(), t.cross(nrm).normalized()))
    bm = bmesh.new()
    def ring(i, r, shift=0.0):
        nn, bb = frames[i]
        c = pts[i] + tans[i] * shift
        return [bm.verts.new(c + bb * (r * math.cos(2 * math.pi * k / segs)) + nn * (r * squash * math.sin(2 * math.pi * k / segs))) for k in range(segs)]
    def skin(R):
        for A, B in zip(R, R[1:]):
            for k in range(segs):
                j = (k + 1) % segs
                bm.faces.new((A[k], A[j], B[j], B[k]))
    outer = [ring(i, rs[i]) for i in range(n)]
    skin(outer)
    if wall <= 0:
        bm.faces.new(outer[0])
        bm.faces.new(outer[-1])
    else:
        inner = []
        for i in range(n):
            sh = wall if (i == 0 and cap0) else (-wall if (i == n - 1 and cap1) else 0.0)
            inner.append(ring(i, max(rs[i] - wall, rs[i] * 0.2), sh))
        skin(inner)
        for e, cap in ((0, cap0), (n - 1, cap1)):
            if cap:
                bm.faces.new(outer[e])
                bm.faces.new(inner[e])
            else:
                A, B = outer[e], inner[e]
                for k in range(segs):
                    j = (k + 1) % segs
                    bm.faces.new((A[k], A[j], B[j], B[k]))
    _recalc(bm)
    return _obj(bm, m, "tube")

def prism(pts, d0, d1, m, plane="XY", bev=0.0, seg=1, loc=None, rot=None):
    # A polygon extruded between d0 and d1 along the axis normal to its plane ("XY", "XZ" or "YZ").
    bm = bmesh.new()
    def V(p, d):
        return (p[0], p[1], d) if plane == "XY" else ((p[0], d, p[1]) if plane == "XZ" else (d, p[0], p[1]))
    A = [bm.verts.new(V(p, d0)) for p in pts]
    B = [bm.verts.new(V(p, d1)) for p in pts]
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((A[i], A[j], B[j], B[i]))
    bm.faces.new(A)
    bm.faces.new(B)
    _recalc(bm)
    o = place(_obj(bm, m, "prism"), loc, rot)
    return _bevel(o, bev, seg, abs(d1 - d0))

def leaf(length, width, thick, m, loc=(0, 0, 0), rot=None, curl=0.15, segs=5):
    # A leaf along +X, flat side up, with real thickness (diamond cross-section), drooping by curl.
    bm = bmesh.new()
    base = bm.verts.new((0, 0, 0))
    tip = bm.verts.new((length, 0, -curl * length))
    rings = []
    for s in range(1, segs):
        t = s / segs
        x = length * t
        w = 0.5 * width * math.sin(math.pi * t ** 0.75)
        z = -curl * length * t * t
        h = 0.5 * thick * (0.6 + 0.4 * math.sin(math.pi * t))
        rings.append([bm.verts.new((x, -w, z)), bm.verts.new((x, 0, z + h)), bm.verts.new((x, w, z)), bm.verts.new((x, 0, z - h))])
    for k in range(4):
        j = (k + 1) % 4
        bm.faces.new((base, rings[0][k], rings[0][j]))
        bm.faces.new((rings[-1][k], tip, rings[-1][j]))
    for A, B in zip(rings, rings[1:]):
        for k in range(4):
            j = (k + 1) % 4
            bm.faces.new((A[k], A[j], B[j], B[k]))
    _recalc(bm)
    return place(_obj(bm, m, "leaf"), loc, rot)

def blade(x, y, h, m, ang, lean, w=0.016):
    # One low-poly grass blade: a three-sided spike leaning along ang.
    bm = bmesh.new()
    d = Vector((math.cos(ang), math.sin(ang), 0))
    side = Vector((-d.y, d.x, 0))
    secs = []
    for t, ww in ((0.0, 1.0), (0.45, 0.85), (0.8, 0.5)):
        c = Vector((x, y, h * t)) + d * (lean * h * t * t)
        secs.append([bm.verts.new(c - side * (w * ww / 2)), bm.verts.new(c + side * (w * ww / 2)), bm.verts.new(c + d * (w * ww * 0.5))])
    tip = bm.verts.new(Vector((x, y, h)) + d * (lean * h))
    bm.faces.new(secs[0])
    for A, B in zip(secs, secs[1:]):
        for k in range(3):
            j = (k + 1) % 3
            bm.faces.new((A[k], A[j], B[j], B[k]))
    for k in range(3):
        j = (k + 1) % 3
        bm.faces.new((secs[-1][k], secs[-1][j], tip))
    _recalc(bm)
    return _obj(bm, m, "blade")

def loft(stations, m, n=4.5, k=24):
    # Rounded-rectangle (superellipse) sections in XZ at stations (y, half_width, z0, z1), skinned along Y.
    bm = bmesh.new()
    rings = []
    for (y, hw, z0, z1) in stations:
        zc, hh = (z0 + z1) / 2, (z1 - z0) / 2
        ring = []
        for i in range(k):
            a = 2 * math.pi * i / k
            c, s = math.cos(a), math.sin(a)
            ring.append(bm.verts.new((hw * math.copysign(abs(c) ** (2 / n), c), y, zc + hh * math.copysign(abs(s) ** (2 / n), s))))
        rings.append(ring)
    for A, B in zip(rings, rings[1:]):
        for i in range(k):
            j = (i + 1) % k
            bm.faces.new((A[i], A[j], B[j], B[i]))
    bm.faces.new(rings[0])
    bm.faces.new(rings[-1])
    _recalc(bm)
    return _obj(bm, m, "loft")

def cut(o, cutter):
    # Boolean difference, evaluated before the bevel. The cutter is not part of the piece.
    if cutter in CUR:
        CUR.remove(cutter)
    CUTTERS.append(cutter)
    b = o.modifiers.new("Cut", "BOOLEAN")
    b.operation = "DIFFERENCE"
    b.object = cutter
    idx = o.modifiers.find("Cut")
    while idx > 0:
        o.modifiers.move(idx, idx - 1)
        idx -= 1
    cutter.hide_render = True
    return o

# ------------------------------------------------------------------ pieces, attachments, the manifest
PIECES, ORDER, CHILDREN = {}, [], {}

def begin():
    CUR.clear()
    CUTTERS.clear()

def r4(v):
    return [round(float(c), 4) for c in v]

def end(name, zone, kind="static", origin=None, parent=None, rot=(0, 0, 0), motion=None, preview=None, **meta):
    # Applies every modifier, joins the parts into ONE mesh, fixes shading and UVs, moves the origin
    # (default: bottom-centre of the bounding box; origin=(x, y, z) in build coordinates otherwise).
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    bm = bmesh.new()
    slots = []
    flipped = 0
    for o in CUR:
        ev = o.evaluated_get(dg)
        me = ev.to_mesh()
        nv, nf = len(bm.verts), len(bm.faces)
        bm.from_mesh(me)
        bm.verts.ensure_lookup_table()
        bm.faces.ensure_lookup_table()
        newv, newf = bm.verts[nv:], bm.faces[nf:]
        bmesh.ops.transform(bm, matrix=o.matrix_world, verts=newv)
        names = [s.name if s else "Plinth" for s in me.materials] or ["Plinth"]
        vol = 0.0
        for f in newf:
            nm = names[min(f.material_index, len(names) - 1)]
            if nm not in slots:
                slots.append(nm)
            f.material_index = slots.index(nm)
            vs = f.verts
            for i in range(1, len(vs) - 1):
                vol += vs[0].co.dot(vs[i].co.cross(vs[i + 1].co))
        if vol < 0:
            bmesh.ops.reverse_faces(bm, faces=newf)
            flipped += 1
        ev.to_mesh_clear()
    bm.normal_update()
    xs = [v.co.x for v in bm.verts]
    ys = [v.co.y for v in bm.verts]
    zs = [v.co.z for v in bm.verts]
    mn, mx = Vector((min(xs), min(ys), min(zs))), Vector((max(xs), max(ys), max(zs)))
    org = Vector(origin) if origin is not None else Vector(((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, mn.z))
    bmesh.ops.translate(bm, vec=-org, verts=bm.verts[:])
    for f in bm.faces:
        f.smooth = True
    open_edges = 0
    lim = math.radians(50)
    for e in bm.edges:
        if len(e.link_faces) == 2:
            if e.calc_face_angle(0.0) > lim:
                e.smooth = False
        else:
            open_edges += 1
    uv = bm.loops.layers.uv.verify()
    for f in bm.faces:
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        for l in f.loops:
            c = l.vert.co
            l[uv].uv = (c.y, c.z) if ax == 0 else ((c.x, c.z) if ax == 1 else (c.x, c.y))
    tris = sum(len(f.verts) - 2 for f in bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for nm in slots:
        me.materials.append(MATS[nm])
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    wn = obj.modifiers.new("WN", "WEIGHTED_NORMAL")
    wn.keep_sharp = True
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    try:
        me2 = bpy.data.meshes.new_from_object(obj.evaluated_get(dg))
        obj.modifiers.clear()
        obj.data = me2
        bpy.data.meshes.remove(me)
        me2.name = name
    except Exception as ex:
        print("WN apply failed for", name, ex)
        obj.modifiers.clear()
    obj.hide_render = True
    for o in CUR + CUTTERS:
        d = o.data
        bpy.data.objects.remove(o)
        bpy.data.meshes.remove(d)
    CUR.clear()
    CUTTERS.clear()
    PIECES[name] = dict(name=name, zone=zone, kind=kind, obj=obj, origin_build=r4(org), bbox_min=r4(mn - org), bbox_max=r4(mx - org),
                        size=r4(mx - mn), tris=tris, mats=slots, open_edges=open_edges, flipped=flipped, attach=[], ue=[], meta=meta,
                        origin_rule=meta.pop("origin_note", None) or ("bottom-centre of the bounding box" if origin is None else
                                     ("hinge axis / slide reference (see attached)" if motion else "custom landmark, see notes (bbox_min_m / bbox_max_m are relative to it)")))
    ORDER.append(name)
    if open_edges or flipped:
        print("CHECK %s: open_edges=%d flipped_parts=%d" % (name, open_edges, flipped))
    if parent:
        attach(name, parent, org, rot, motion, preview)
    return obj

def attach(child, parent, at, rot=(0, 0, 0), motion=None, preview=None, note=None):
    # at = where the child's origin sits, in the PARENT's build coordinates (metres, model axes).
    off = Vector(at) - Vector(PIECES[parent]["origin_build"])
    a = dict(parent=parent, offset=r4(off), rot=[float(v) for v in rot], motion=motion, preview=preview, note=note)
    PIECES[child]["attach"].append(a)
    CHILDREN.setdefault(parent, []).append((child, a))

def hinge(axis, angle, toward):
    return {"type": "hinge", "axis": list(axis), "open_angle_deg": angle, "opens_toward": toward}

def slide(vec):
    return {"type": "slide", "vector_m": [round(float(v), 4) for v in vec]}

# ------------------------------------------------------------------ preview scene (laid out like the game)
STAGE = PV = CAM = TGT = SUN = FILL = None

def smat(name, color, rough=0.8):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Roughness"].default_value = rough
    return m

def setup_scene():
    global STAGE, PV, CAM, TGT, SUN, FILL
    sc = bpy.context.scene
    STAGE = bpy.data.collections.new("Stage")
    sc.collection.children.link(STAGE)
    PV = bpy.data.collections.new("Preview")
    sc.collection.children.link(PV)
    world = bpy.data.worlds.new("World")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.85, 0.92, 1.0, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.30
    sc.world = world
    sd = bpy.data.lights.new("Sun", "SUN")
    sd.energy = 2.6
    sd.angle = math.radians(6)
    sd.color = (1.0, 0.95, 0.86)
    SUN = bpy.data.objects.new("Sun", sd)
    sc.collection.objects.link(SUN)
    fd = bpy.data.lights.new("Fill", "AREA")
    fd.color = (0.8, 0.9, 1.0)
    FILL = bpy.data.objects.new("Fill", fd)
    sc.collection.objects.link(FILL)
    TGT = bpy.data.objects.new("Target", None)
    sc.collection.objects.link(TGT)
    cd = bpy.data.cameras.new("Cam")
    cd.clip_start = 0.01
    cd.clip_end = 500
    CAM = bpy.data.objects.new("Cam", cd)
    sc.collection.objects.link(CAM)
    tr = CAM.constraints.new("TRACK_TO")
    tr.target = TGT
    sc.camera = CAM
    for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        try:
            sc.render.engine = eng
            break
        except Exception:
            pass
    try:
        sc.eevee.taa_render_samples = 48
    except Exception:
        pass
    for attr, val in (("use_gtao", True), ("use_raytracing", True), ("use_shadows", True)):
        try:
            setattr(sc.eevee, attr, val)
        except Exception:
            pass
    try:
        sc.view_settings.view_transform = "Standard"
    except Exception:
        pass
    sc.render.resolution_x = 1400
    sc.render.resolution_y = 900

def stage_box(name, mn, mx, m):
    # A plain box of the stage, given in GAME coordinates (Unreal units, Unreal axes).
    x0, x1, y0, y1, z0, z1 = mn[0] / S, mx[0] / S, -mx[1] / S, -mn[1] / S, mn[2] / S, mx[2] / S
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co.x *= (x1 - x0)
        v.co.y *= (y1 - y0)
        v.co.z *= (z1 - z0)
    me = bpy.data.meshes.new("Stage_" + name)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(m)
    o = bpy.data.objects.new("Stage_" + name, me)
    STAGE.objects.link(o)
    o.location = ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)
    return o

def _inst(name, M):
    o = bpy.data.objects.new("PV_" + name, PIECES[name]["obj"].data)
    PV.objects.link(o)
    o.matrix_world = M
    return o

def _local(a):
    L = Matrix.Translation(a["offset"]) @ Euler([math.radians(v) for v in a["rot"]], "XYZ").to_matrix().to_4x4()
    mo, pv = a.get("motion"), a.get("preview") or 0.0
    if mo and pv:
        if mo["type"] == "hinge":
            L = L @ Matrix.Rotation(math.radians(mo["open_angle_deg"] * pv), 4, Vector(mo["axis"]))
        else:
            L = Matrix.Translation(Vector(mo["vector_m"]) * pv) @ L
    return L

def _children(name, M):
    for child, a in CHILDREN.get(name, []):
        C = M @ _local(a)
        _inst(child, C)
        _children(child, C)

def spawn(name, x, y, z=0.0, yaw=0.0, record=True, note=None):
    # Places a piece (and everything attached to it) where the GAME has it: Unreal location of the
    # origin and Unreal yaw. The preview is the game mirrored in Y, exactly like the FBX import.
    M = Matrix.Translation((x / S, -y / S, z / S)) @ Matrix.Rotation(math.radians(-yaw), 4, "Z")
    _inst(name, M)
    _children(name, M)
    if record:
        e = {"location_uu": [round(x, 1), round(y, 1), round(z, 1)], "yaw_deg": yaw, "scale": 19.0}
        if note:
            e["note"] = note
        PIECES[name]["ue"].append(e)

def spawn_build(name, x, y, z=0.0, yaw=0.0, **kw):
    # Same, but (x, y, z) is where the piece's BUILD zero sits (for pieces built around a landmark).
    ob = Vector(PIECES[name]["origin_build"])
    r = math.radians(-yaw)
    wx = ob.x * math.cos(r) - ob.y * math.sin(r)
    wy = ob.x * math.sin(r) + ob.y * math.cos(r)
    spawn(name, x + wx * S, y - wy * S, z + ob.z * S, yaw, **kw)

def render(out, fname, cam, tgt, lens=30, hide=(), energy=6.0):
    sc = bpy.context.scene
    cam, tgt = Vector(cam), Vector(tgt)
    CAM.location = cam
    TGT.location = tgt
    CAM.data.lens = lens
    for o in STAGE.objects:
        o.hide_render = False
    for o in hide:
        o.hide_render = True
    d = (cam - tgt).length
    h = Vector((tgt.x - cam.x, tgt.y - cam.y, 0))
    h = h.normalized() if h.length > 1e-6 else Vector((0, 1, 0))
    h = Matrix.Rotation(math.radians(-28), 3, "Z") @ h
    L = (h * math.sin(math.radians(52)) + Vector((0, 0, -math.cos(math.radians(52))))).normalized()
    SUN.rotation_euler = (-L).to_track_quat("Z", "Y").to_euler()
    FILL.location = (cam + tgt) / 2 + Vector((0, 0, 0.55 * d + 0.3))
    FILL.data.energy = energy * d * d
    FILL.data.size = max(0.3, 0.8 * d)
    sc.render.filepath = os.path.join(out, fname)
    bpy.ops.render.render(write_still=True)
    print("RENDERED", fname)

LINEUP_Y = [40.0]

def lineup(out, fname, names, row_w=4.0, gap=0.12, lens=35):
    # A contact sheet: the pieces side by side on a grey floor, fronts to the camera.
    rows, x = [[]], 0.0
    for n in names:
        w = PIECES[n]["size"][0]
        if x + w > row_w and rows[-1]:
            rows.append([])
            x = 0.0
        rows[-1].append((n, x + w / 2))
        x += w + gap
    y0 = LINEUP_Y[0]
    y, maxh, maxw = y0, 0.0, 0.0
    for r in rows:
        depth = max(PIECES[n]["size"][1] for n, _ in r)
        y += depth / 2
        for n, xc in r:
            p = PIECES[n]
            cx = (p["bbox_min"][0] + p["bbox_max"][0]) / 2
            cy = (p["bbox_min"][1] + p["bbox_max"][1]) / 2
            _inst(n, Matrix.Translation((xc - cx, y - cy, -p["bbox_min"][2])))
            maxh = max(maxh, p["size"][2])
            maxw = max(maxw, xc + p["size"][0] / 2)
        y += depth / 2 + gap * 1.5
    fl = stage_box("lineup", (-2 * S, -(y + 3) * S, -0.05 * S), ((maxw + 2) * S, -(y0 - 3) * S, 0), smat("LineupFloor", (0.78, 0.78, 0.76), 0.6))
    LINEUP_Y[0] = y + 20.0
    dist = max(maxw * 0.86, (y - y0) * 1.25, maxh * 1.6) + 0.2
    c = Vector((maxw / 2, (y0 + y) / 2, maxh * 0.3))
    render(out, fname, c + Vector((-0.18 * dist, -0.80 * dist, 0.62 * dist)), c, lens)

def export_all(out):
    for o in bpy.context.scene.objects:
        o.select_set(False)
    for name in ORDER:
        o = PIECES[name]["obj"]
        o.select_set(True)
        bpy.context.view_layer.objects.active = o
        bpy.ops.export_scene.fbx(filepath=os.path.join(out, name + ".fbx"), use_selection=True, object_types={"MESH"},
                                 mesh_smooth_type="FACE", use_mesh_modifiers=True, bake_anim=False, add_leaf_bones=False)
        o.select_set(False)
    print("EXPORTED", len(ORDER))

def write_manifest(out):
    used = []
    pieces = []
    for name in ORDER:
        p = PIECES[name]
        for m in p["mats"]:
            if m not in used:
                used.append(m)
        e = {"name": name, "file": name + ".fbx", "zone": p["zone"], "kind": p["kind"],
             "bbox_size_m": p["size"], "bbox_min_m": p["bbox_min"], "bbox_max_m": p["bbox_max"],
             "triangles": p["tris"], "materials": p["mats"], "open_edges": p["open_edges"], "origin": p["origin_rule"]}
        e.update(p["meta"])
        att = []
        for a in p["attach"]:
            o = a["offset"]
            d = {"parent": a["parent"], "offset_m": o, "offset_uu_game": [round(o[0] * S, 1), round(-o[1] * S, 1), round(o[2] * S, 1)],
                 "rot_deg_model_xyz": a["rot"], "yaw_ue_deg": -a["rot"][2]}
            if a["motion"]:
                mo = dict(a["motion"])
                if mo["type"] == "slide":
                    v = mo["vector_m"]
                    mo["vector_uu_game"] = [round(v[0] * S, 1), round(-v[1] * S, 1), round(v[2] * S, 1)]
                elif mo["axis"] == [0, 0, 1]:
                    mo["open_yaw_ue_deg"] = -mo["open_angle_deg"]
                d["motion"] = mo
            if a["note"]:
                d["note"] = a["note"]
            att.append(d)
        if att:
            e["attached"] = att
        if p["ue"]:
            e["game_placement"] = p["ue"]
        pieces.append(e)
    data = {
        "about": "Ant Kitchen: living room, yard and street pieces. Built by Tools/blender/living_pieces.py (Blender, headless).",
        "units": "metres, +Z up, the front of every piece faces -Y. One FBX = one piece = one mesh.",
        "game_scale": "1 m = 1900 Unreal units. FBX imports at 1 m = 100 uu, so the game scale of every piece is 19.",
        "axes_note": "Unreal's FBX import mirrors Y: model (x, y, z) m -> local (100x, -100y, 100z) uu, so the model front (-Y) is local +Y. "
                     "offset_m / rot_deg_model_xyz / motion axes are in MODEL axes (right-handed); offset_uu_game and yaw_ue_deg are the same "
                     "thing already converted to Unreal axes at game scale. game_placement is the Unreal location of the origin and the yaw "
                     "that puts the piece where the C++ boxes are (the previews are rendered from exactly these numbers).",
        "origins": "static: bottom-centre of the bounding box unless 'origin' says otherwise; hinged parts: on the hinge axis at the bottom of the part; "
                   "sliding parts: back-bottom-centre.",
        "materials": {m: MATINFO[m] for m in used},
        "totals": {"pieces": len(ORDER), "triangles": sum(PIECES[n]["tris"] for n in ORDER),
                   "by_zone": {z: sum(1 for n in ORDER if PIECES[n]["zone"] == z) for z in ("living", "yard", "street")}},
        "pieces": pieces,
    }
    with open(os.path.join(out, "living_manifest.json"), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1)
    print("MANIFEST pieces=%d tris=%d" % (len(ORDER), data["totals"]["triangles"]))
