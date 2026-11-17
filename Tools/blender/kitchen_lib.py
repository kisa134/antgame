# Ant Kitchen: shared helpers for kitchen_pieces.py (headless Blender, bmesh only, no bpy.ops while modelling).
# Frames. Game: X east, Y north, Z up, 1 m = 1900 uu (UE, left-handed).
# Blender scene frame "S" (metres, right-handed): S = (-gameX, gameY, gameZ) / 1900. A piece whose front faces -Y in
# Blender and that is imported with the default FBX settings (UE local = (100x, -100y, 100z)) lands in the game with
# yaw = 180 - phi, where phi is its rotation about Z in S.
import bpy, bmesh, math, os, json
from mathutils import Vector, Matrix, Euler

K = 1.0 / 1900.0
MATS, MATINFO = {}, {}

def mat(name, color, rough=0.5, metal=0.0, new=False, emit=0.0):
    if name in MATS:
        return name
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
    MATINFO[name] = {"rgb": [round(c, 3) for c in color], "roughness": rough, "metallic": metal, "emission": emit, "new_slot": bool(new)}
    return name

class _St:
    pass
S = _St()
S.stack = [bmesh.new()]
S.slots = []
PIECES, ORDER = {}, []

def G(X, Y, Z):
    return (-X * K, Y * K, Z * K)

def _slot(m):
    if m not in MATS:
        raise KeyError("unknown material " + str(m))
    if m not in S.slots:
        S.slots.append(m)
    return S.slots.index(m)

def _merge(dst, src):
    me = bpy.data.meshes.new("_t")
    src.to_mesh(me)
    src.free()
    dst.from_mesh(me)
    bpy.data.meshes.remove(me)

def _mx(x, y, z, rot=None):
    M = Matrix.Translation((x, y, z))
    if rot is not None:
        M = M @ Euler([math.radians(a) for a in rot], "XYZ").to_matrix().to_4x4()
    return M

def _emit(bm, m, M=None):
    i = _slot(m)
    for f in bm.faces:
        f.material_index = i
    if M is not None:
        bm.transform(M)
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    _merge(S.stack[-1], bm)

def push():
    S.stack.append(bmesh.new())

def pop():
    return S.stack.pop()

def add(bm):
    _merge(S.stack[-1], bm)

def _tmpobj(bm):
    me = bpy.data.meshes.new("_b")
    bm.to_mesh(me)
    bm.free()
    for n in S.slots:
        me.materials.append(MATS[n])
    o = bpy.data.objects.new("_b", me)
    bpy.context.scene.collection.objects.link(o)
    return o

def cut(base, cutter, op="DIFFERENCE"):
    oa, ob = _tmpobj(base), _tmpobj(cutter)
    md = oa.modifiers.new("b", "BOOLEAN")
    md.operation = op
    md.object = ob
    try:
        md.solver = "EXACT"
        md.use_self = True
    except Exception:
        pass
    dg = bpy.context.evaluated_depsgraph_get()
    dg.update()
    me = bpy.data.meshes.new_from_object(oa.evaluated_get(dg))
    bm = bmesh.new()
    bm.from_mesh(me)
    bpy.data.meshes.remove(me)
    for o in (oa, ob):
        d = o.data
        bpy.data.objects.remove(o)
        bpy.data.meshes.remove(d)
    return bm

# ------------------------------------------------------------------ primitives (metres, current frame)
def rbox(sx, sy, sz, x, y, z, m, bev=0.004, seg=2, rot=None):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * sx, v.co.y * sy, v.co.z * sz))
    if bev > 0:
        bmesh.ops.bevel(bm, geom=bm.edges[:], offset=min(bev, 0.45 * min(sx, sy, sz)), segments=seg, profile=0.5, affect="EDGES")
    _emit(bm, m, _mx(x, y, z, rot))

def box(x0, x1, y0, y1, z0, z1, m, bev=0.004, seg=2):
    rbox(abs(x1 - x0), abs(y1 - y0), abs(z1 - z0), (x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2, m, bev, seg)

def gbox(X0, X1, Y0, Y1, Z0, Z1, m, bev=0.004, seg=2):
    # Box given in game units (X mirrored into the S frame).
    box(-X1 * K, -X0 * K, Y0 * K, Y1 * K, Z0 * K, Z1 * K, m, bev, seg)

def ubox(x0, x1, y0, y1, z0, z1, m, bev=0.004, seg=2):
    # Box given in game units in a piece-local frame (no mirror).
    box(x0 * K, x1 * K, y0 * K, y1 * K, z0 * K, z1 * K, m, bev, seg)

AX = {"Z": None, "X": (0, 90, 0), "Y": (90, 0, 0)}

def cyl(r, h, x, y, z, m, axis="Z", n=16, bev=0.0, r2=None, seg=2, rot=None):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=n, radius1=r, radius2=(r if r2 is None else r2), depth=h)
    if bev > 0:
        ed = [e for e in bm.edges if any(len(f.verts) > 4 for f in e.link_faces)]
        bmesh.ops.bevel(bm, geom=ed, offset=min(bev, 0.45 * min(r, h)), segments=seg, profile=0.5, affect="EDGES")
    _emit(bm, m, _mx(x, y, z, rot if rot is not None else AX[axis]))

def gcyl(X, Y, Z0, R, H, m, n=16, bev=0.0, r2=None):
    cyl(R * K, H * K, -X * K, Y * K, (Z0 + H / 2) * K, m, "Z", n, bev, None if r2 is None else r2 * K)

def ball(r, x, y, z, m, scale=(1, 1, 1), nu=16, nv=8, rot=None):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=nu, v_segments=nv, radius=r)
    for v in bm.verts:
        v.co = Vector((v.co.x * scale[0], v.co.y * scale[1], v.co.z * scale[2]))
    _emit(bm, m, _mx(x, y, z, rot))

def lathe(prof, m, x=0.0, y=0.0, z=0.0, n=16, axis="Z", rot=None, closed=False):
    # prof: (radius, height) points; start and end on the axis (r = 0), or closed=True for a ring section.
    bm = bmesh.new()
    rings = []
    for (r, h) in prof:
        if r <= 1e-7:
            rings.append([bm.verts.new((0, 0, h))])
        else:
            rings.append([bm.verts.new((r * math.cos(2 * math.pi * i / n), r * math.sin(2 * math.pi * i / n), h)) for i in range(n)])
    pairs = list(zip(rings, rings[1:]))
    if closed:
        pairs.append((rings[-1], rings[0]))
    for a, b in pairs:
        for i in range(n):
            j = (i + 1) % n
            if len(a) == 1 and len(b) == 1:
                continue
            if len(a) == 1:
                bm.faces.new((a[0], b[i], b[j]))
            elif len(b) == 1:
                bm.faces.new((a[i], b[0], a[j]))
            else:
                bm.faces.new((a[i], b[i], b[j], a[j]))
    _emit(bm, m, _mx(x, y, z, rot if rot is not None else AX[axis]))

def tube(pts, r, m, n=8, M=None):
    P = [Vector(p) for p in pts]
    L = len(P)
    rs = list(r) if isinstance(r, (list, tuple)) else [r] * L
    T = [(P[min(i + 1, L - 1)] - P[max(i - 1, 0)]).normalized() for i in range(L)]
    up = Vector((0, 0, 1)) if abs(T[0].z) < 0.9 else Vector((1, 0, 0))
    nrm = (up - T[0] * up.dot(T[0])).normalized()
    bm = bmesh.new()
    rings = []
    for i in range(L):
        if i > 0:
            ax = T[i - 1].cross(T[i])
            if ax.length > 1e-8:
                nrm = Matrix.Rotation(T[i - 1].angle(T[i]), 3, ax.normalized()) @ nrm
            nrm = (nrm - T[i] * nrm.dot(T[i])).normalized()
        bn = T[i].cross(nrm)
        rings.append([bm.verts.new(P[i] + (nrm * math.cos(2 * math.pi * k / n) + bn * math.sin(2 * math.pi * k / n)) * rs[i]) for k in range(n)])
    for a, b in zip(rings, rings[1:]):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((a[i], b[i], b[j], a[j]))
    bm.faces.new(rings[0])
    bm.faces.new(rings[-1])
    _emit(bm, m, M)

def spline(pts, sub=6):
    P = [Vector(p) for p in pts]
    out = []
    for i in range(len(P) - 1):
        p0, p1, p2, p3 = P[max(i - 1, 0)], P[i], P[i + 1], P[min(i + 2, len(P) - 1)]
        for s in range(sub):
            t = s / sub
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t))
    out.append(P[-1])
    return out

def prism(poly, a0, a1, m, axis="Z", M=None, bev=0.0):
    # Extrude a 2D polygon: axis Z -> poly is (x, y); X -> (y, z); Y -> (x, z).
    bm = bmesh.new()
    def P(p, t):
        return {"Z": (p[0], p[1], t), "X": (t, p[0], p[1]), "Y": (p[0], t, p[1])}[axis]
    A = [bm.verts.new(P(p, a0)) for p in poly]
    B = [bm.verts.new(P(p, a1)) for p in poly]
    n = len(poly)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((A[i], A[j], B[j], B[i]))
    bm.faces.new(A)
    bm.faces.new(B)
    if bev > 0:
        bm.normal_update()
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        ed = [e for e in bm.edges if len(e.link_faces) == 2 and e.calc_face_angle(0.0) > math.radians(30)]
        bmesh.ops.bevel(bm, geom=ed, offset=bev, segments=2, profile=0.5, affect="EDGES")
    _emit(bm, m, M)

def gridsolid(xs, ys, z0, z1, holes, m, bev=0.0, seg=2):
    # A slab on the grid xs * ys with the cells in `holes` (i, j) left out; one clean closed solid.
    bm = bmesh.new()
    nx, ny = len(xs) - 1, len(ys) - 1
    full = lambda i, j: 0 <= i < nx and 0 <= j < ny and (i, j) not in holes
    def q(pts):
        bm.faces.new([bm.verts.new(p) for p in pts])
    for i in range(nx):
        for j in range(ny):
            if not full(i, j):
                continue
            a, b, c, d = xs[i], xs[i + 1], ys[j], ys[j + 1]
            q([(a, c, z1), (b, c, z1), (b, d, z1), (a, d, z1)])
            q([(a, c, z0), (a, d, z0), (b, d, z0), (b, c, z0)])
            if not full(i - 1, j):
                q([(a, c, z0), (a, c, z1), (a, d, z1), (a, d, z0)])
            if not full(i + 1, j):
                q([(b, c, z0), (b, d, z0), (b, d, z1), (b, c, z1)])
            if not full(i, j - 1):
                q([(a, c, z0), (b, c, z0), (b, c, z1), (a, c, z1)])
            if not full(i, j + 1):
                q([(a, d, z0), (a, d, z1), (b, d, z1), (b, d, z0)])
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-6)
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(1), verts=bm.verts[:], edges=bm.edges[:])
    if bev > 0:
        ed = [e for e in bm.edges if len(e.link_faces) == 2 and e.calc_face_angle(0.0) > math.radians(30)]
        bmesh.ops.bevel(bm, geom=ed, offset=bev, segments=seg, profile=0.5, affect="EDGES")
    _emit(bm, m)

def tray(x0, x1, y0, y1, z0, z1, t, m, bev=0.002, seg=1, open_sides=""):
    # Open-top box out of five closed slabs. open_sides: any of "x-","x+","y-","y+" to leave a wall out.
    box(x0, x1, y0, y1, z0, z0 + t, m, bev, seg)
    if "x-" not in open_sides:
        box(x0, x0 + t, y0, y1, z0 + t, z1, m, bev, seg)
    if "x+" not in open_sides:
        box(x1 - t, x1, y0, y1, z0 + t, z1, m, bev, seg)
    if "y-" not in open_sides:
        box(x0 + t, x1 - t, y0, y0 + t, z0 + t, z1, m, bev, seg)
    if "y+" not in open_sides:
        box(x0 + t, x1 - t, y1 - t, y1, z0 + t, z1, m, bev, seg)

def bar_handle(x, y, z, length, m="Brass", axis="X", r=0.007, stand=0.028, n=10):
    # A bar on two standoffs; y is the face it is mounted on (the bar sits in front of it, towards -Y).
    cyl(r, length, x, y - stand, z, m, axis, n, bev=r * 0.4, seg=1)
    d = length * 0.36
    for s in (-1, 1):
        px, pz = (x + s * d, z) if axis == "X" else (x, z + s * d)
        cyl(r * 0.8, stand, px, y - stand / 2, pz, m, "Y", 8)

def shaker(x0, x1, z0, z1, yb, handle="top", m="Cabinet", mi="CabinetInset", hm="Brass"):
    # Shaker door/front: flat slab with a raised frame. yb = back face; the front is towards -Y. Returns the front face y.
    w, h = x1 - x0, z1 - z0
    xc, zc = (x0 + x1) / 2, (z0 + z1) / 2
    st, ft = 0.018, 0.010
    fw = min(0.055, 0.28 * min(w, h))
    box(x0, x1, yb - st, yb, z0, z1, mi, 0.003, 1)
    ya, yf = yb - st - ft, yb - st
    box(x0, x0 + fw, ya, yf, z0, z1, m, 0.003, 2)
    box(x1 - fw, x1, ya, yf, z0, z1, m, 0.003, 2)
    box(x0 + fw, x1 - fw, ya, yf, z1 - fw, z1, m, 0.003, 2)
    box(x0 + fw, x1 - fw, ya, yf, z0, z0 + fw, m, 0.003, 2)
    if handle == "top":
        bar_handle(xc, ya, z1 - fw / 2, min(0.14, w * 0.4), hm)
    elif handle == "bottom":
        bar_handle(xc, ya, z0 + fw / 2, min(0.14, w * 0.4), hm)
    elif handle == "mid":
        bar_handle(xc, ya, zc, min(0.16, w * 0.4), hm)
    return ya
# ------------------------------------------------------------------ pieces, export, manifest
def piece(name, origin="bottom", parent=None, motion=None, replaces=None, at=None, on=None, note=None, state=None):
    # Close the piece under construction. origin: "bottom" (bottom-centre of the bounding box), "back"
    # (back-bottom-centre, back = +Y) or an explicit point in the build frame (hinge axis).
    # at: None -> built in place in the S frame; else a list of placements (x, y, z, rot[, scale]) of the build-frame origin,
    # rot = degrees about Z or an (rx, ry, rz) Euler XYZ in degrees. Children (parent=...) share the parent's build frame.
    if len(S.stack) != 1:
        raise RuntimeError("unbalanced push/pop at " + name)
    bm = S.stack[0]
    if not bm.verts:
        raise RuntimeError("empty piece " + name)
    mn = Vector([min(v.co[i] for v in bm.verts) for i in range(3)])
    mx = Vector([max(v.co[i] for v in bm.verts) for i in range(3)])
    if origin == "bottom":
        o, kind = Vector(((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, mn.z)), "bottom-centre of bounding box"
    elif origin == "back":
        o, kind = Vector(((mn.x + mx.x) / 2, mx.y, mn.z)), "back-bottom-centre (back = +Y)"
    else:
        o, kind = Vector(origin), "explicit (hinge axis / pivot)"
    bm.transform(Matrix.Translation(-o))
    bm.normal_update()
    open_e = 0
    for f in bm.faces:
        f.smooth = True
    for e in bm.edges:
        if len(e.link_faces) != 2:
            open_e += 1
            e.smooth = False
        else:
            e.smooth = e.calc_face_angle(0.0) < math.radians(38)
    inv = _fix_shells(bm)
    if inv:
        print('FIXED_INVERTED_SHELLS', name, inv)
    uv = bm.loops.layers.uv.verify()
    for f in bm.faces:
        n = f.normal
        a = max(range(3), key=lambda i: abs(n[i]))
        for l in f.loops:
            c = l.vert.co
            l[uv].uv = (c.y, c.z) if a == 0 else ((c.x, c.z) if a == 1 else (c.x, c.y))
    used = sorted({f.material_index for f in bm.faces})
    remap = {old: new for new, old in enumerate(used)}
    for f in bm.faces:
        f.material_index = remap[f.material_index]
    slots = [S.slots[i] for i in used]
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for n in slots:
        me.materials.append(MATS[n])
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    wn = ob.modifiers.new("WN", "WEIGHTED_NORMAL")
    wn.keep_sharp = True
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    PIECES[name] = dict(name=name, ob=ob, o=o, mn=mn - o, mx=mx - o, tris=tris, mats=slots, parent=parent, motion=motion,
                        replaces=replaces or [], at=at, on=on, note=note, state=state, origin_kind=kind, open_edges=open_e, inverted=inv)
    ORDER.append(name)
    S.stack = [bmesh.new()]
    S.slots = []
    return name

def _R(spec):
    if isinstance(spec, (int, float)):
        return Matrix.Rotation(math.radians(spec), 3, "Z")
    return Euler([math.radians(a) for a in spec], "XYZ").to_matrix()

def _mul(v, s):
    return Vector((v.x * s[0], v.y * s[1], v.z * s[2]))

def instances(name):
    p = PIECES[name]
    if "_inst" in p:
        return p["_inst"]
    out = []
    if p["parent"]:
        d = child_offsets(p)[0][0]
        for (t, R, s) in instances(p["parent"]):
            out.append((t + R @ _mul(d, s), R, s))
    elif p["at"] is None:
        out.append((p["o"].copy(), Matrix.Identity(3), (1, 1, 1)))
    else:
        for a in p["at"]:
            R = _R(a[3] if len(a) > 3 else 0)
            s = a[4] if len(a) > 4 else (1, 1, 1)
            out.append((Vector(a[:3]) + R @ _mul(p["o"], s), R, s))
    p["_inst"] = out
    return out

N3 = Matrix.Diagonal((-1, 1, 1))
M3 = Matrix.Diagonal((1, -1, 1))

def _r(v, d=4):
    return [round(float(x), d) for x in v]

def _inst_json(p, t, R, s):
    Rg = N3 @ R @ M3
    d = {"blender_loc_m": _r(t, 5), "blender_rot_3x3": [_r(row, 6) for row in R], "scale": list(s),
         "game_loc_uu": _r((-t.x / K, t.y / K, t.z / K), 1),
         "game_axis_x": _r(Rg.col[0], 5), "game_axis_y": _r(Rg.col[1], 5), "game_axis_z": _r(Rg.col[2], 5)}
    if abs(Rg[2][2] - 1) < 1e-6:
        d["game_yaw_deg"] = round(math.degrees(math.atan2(Rg[1][0], Rg[0][0])), 3)
    cs = []
    for i in range(8):
        b = Vector((p["mx"].x if i & 1 else p["mn"].x, p["mx"].y if i & 2 else p["mn"].y, p["mx"].z if i & 4 else p["mn"].z))
        w = t + R @ _mul(b, s)
        cs.append((-w.x / K, w.y / K, w.z / K))
    d["game_bbox_min_uu"] = _r([min(c[i] for c in cs) for i in range(3)], 1)
    d["game_bbox_max_uu"] = _r([max(c[i] for c in cs) for i in range(3)], 1)
    mo = p["motion"]
    if mo and mo["type"] == "hinge":
        ax = Rg @ (M3 @ Vector(mo["axis"]))
        d["hinge_axis_game"] = _r(ax, 5)
        d["open_angle_game_deg"] = -mo["angle_deg"]
    elif mo and mo["type"] == "slide":
        v = R @ Vector(mo["vector_m"])
        d["slide_game_uu"] = _r((-v.x / K, v.y / K, v.z / K), 1)
    return d

def export_all(out):
    os.makedirs(out, exist_ok=True)
    vl = bpy.context.view_layer
    for name in ORDER:
        ob = PIECES[name]["ob"]
        for o in bpy.context.scene.objects:
            o.select_set(False)
        ob.select_set(True)
        vl.objects.active = ob
        bpy.ops.export_scene.fbx(filepath=os.path.join(out, name + ".fbx"), use_selection=True, object_types={"MESH"},
                                 use_mesh_modifiers=True, mesh_smooth_type="FACE", bake_anim=False, add_leaf_bones=False)

def write_manifest(out, header):
    pieces, total = [], 0
    for name in ORDER:
        p = PIECES[name]
        total += p["tris"]
        e = {"name": name, "file": name + ".fbx", "origin": p["origin_kind"],
             "bbox_size_m": _r(p["mx"] - p["mn"]), "bbox_min_m": _r(p["mn"]), "bbox_max_m": _r(p["mx"]),
             "bbox_size_game_uu": _r((p["mx"] - p["mn"]) / K, 1), "triangles": p["tris"], "materials": p["mats"]}
        if p.get('inverted'):
            e['inverted_shells_fixed'] = p['inverted']
        if p["open_edges"]:
            e["open_edges"] = p["open_edges"]
        if p["parent"]:
            d = child_offsets(p)[0][0]
            e["parent"] = p["parent"]
            e["offset_from_parent_m"] = _r(d, 5)
            e["offset_from_parent_ue_local_uu"] = _r((d.x / K, -d.y / K, d.z / K), 1)
        mo = p["motion"]
        if mo:
            mj = dict(mo)
            if mo["type"] == "hinge":
                a = mo["axis"]
                mj["axis_blender"] = list(a)
                del mj["axis"]
                mj["axis_ue_local"] = [a[0], -a[1], a[2]]
                mj["angle_ue_local_deg"] = -mo["angle_deg"]
            else:
                v = mo["vector_m"]
                mj["vector_m"] = _r(v, 5)
                mj["vector_ue_local_uu"] = _r((v[0] / K, -v[1] / K, v[2] / K), 1)
            e["motion"] = mj
        if p["state"]:
            e["default_state"] = p["state"]
        if p["note"]:
            e["note"] = p["note"]
        if p["replaces"]:
            e["replaces"] = [{"function": r[0], "what": r[1], "game_box_min_uu": list(r[2]), "game_box_max_uu": list(r[3])} for r in p["replaces"]]
        ins = instances(name)
        e["instances"] = [_inst_json(p, t, R, s) for (t, R, s) in ins]
        if p["on"]:
            _sits(p, e, ins)
            pass
        if p["replaces"]:
            b0 = [min(r[2][i] for r in p["replaces"]) for i in range(3)]
            b1 = [max(r[3][i] for r in p["replaces"]) for i in range(3)]
            g0, g1 = e["instances"][0]["game_bbox_min_uu"], e["instances"][0]["game_bbox_max_uu"]
            e["replaced_box_union_uu"] = [b0, b1]
            e["bbox_minus_replaced_uu"] = [_r([g0[i] - b0[i] for i in range(3)], 1), _r([g1[i] - b1[i] for i in range(3)], 1)]
        pieces.append(e)
    doc = dict(header)
    doc["materials"] = MATINFO
    doc["totals"] = {"pieces": len(pieces), "triangles": total, "placed_instances": sum(len(e["instances"]) for e in pieces),
                     "placed_triangles": sum(e["triangles"] * len(e["instances"]) for e in pieces)}
    doc["pieces"] = pieces
    with open(os.path.join(out, "kitchen_manifest.json"), "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=1)
    return doc

def report(doc):
    for e in doc["pieces"]:
        d = e.get("bbox_minus_replaced_uu")
        print("PIECE %-34s tris=%5d inst=%d size_uu=%s%s%s" % (e["name"], e["triangles"], len(e["instances"]), e["bbox_size_game_uu"],
              (" OPEN_EDGES=%d" % e["open_edges"]) if e.get("open_edges") else "", (" d=%s" % d) if d else ""))
    print("TOTAL pieces=%d tris=%d placed_inst=%d placed_tris=%d" % (doc["totals"]["pieces"], doc["totals"]["triangles"],
          doc["totals"]["placed_instances"], doc["totals"]["placed_triangles"]))
def child_offsets(p):
    # (offset of the child's origin from the parent's origin, child rotation), in the parent's build frame.
    # A child with at=[...] is placed several times inside the parent (knobs, burners, racks).
    po = PIECES[p["parent"]]["o"]
    if p["at"] is None:
        return [(p["o"] - po, Matrix.Identity(3))]
    out = []
    for a in p["at"]:
        Ra = _R(a[3] if len(a) > 3 else 0)
        out.append((Vector(a[:3]) + Ra @ p["o"] - po, Ra))
    return out

def instances(name):
    p = PIECES[name]
    if "_inst" in p:
        return p["_inst"]
    out = []
    if p["parent"]:
        for (t, R, s) in instances(p["parent"]):
            for (d, Ra) in child_offsets(p):
                out.append((t + R @ _mul(d, s), R @ Ra, s))
    elif p["at"] is None:
        out.append((p["o"].copy(), Matrix.Identity(3), (1, 1, 1)))
    else:
        for a in p["at"]:
            R = _R(a[3] if len(a) > 3 else 0)
            s = a[4] if len(a) > 4 else (1, 1, 1)
            out.append((Vector(a[:3]) + R @ _mul(p["o"], s), R, s))
    p["_inst"] = out
    return out

def centered(c, h, rot):
    # Placement for a prop modelled upright with its origin at the base centre, so that its middle (h/2 up the axis) sits at c.
    R = _R(rot)
    b = Vector(c) - R @ Vector((0, 0, h / 2))
    return (b.x, b.y, b.z, rot)
def _sits(p, e, ins):
    # Per instance: what the prop stands on (nearest instance of that piece) and its offset in that piece's Blender space.
    ons = p["on"] if isinstance(p["on"], (list, tuple)) else [p["on"]] * len(ins)
    for (t, R, s), pn, ij in zip(ins, ons, e["instances"]):
        if pn is None:
            continue
        pin = instances(pn)
        k = min(range(len(pin)), key=lambda q: (pin[q][0] - t).length)
        tp, Rp, sp = pin[k]
        ij["sits_on"] = {"parent": pn, "parent_instance": k, "offset_m": _r(Rp.transposed() @ (t - tp), 5)}
def _fix_shells(bm):
    # Every connected shell must be a closed solid with outward normals (positive signed volume); flip the ones that are not.
    bm.faces.index_update()
    seen = [False] * len(bm.faces)
    inv = 0
    for f0 in bm.faces:
        if seen[f0.index]:
            continue
        stack, isl = [f0], []
        seen[f0.index] = True
        while stack:
            f = stack.pop()
            isl.append(f)
            for e in f.edges:
                for g in e.link_faces:
                    if not seen[g.index]:
                        seen[g.index] = True
                        stack.append(g)
        vol = 0.0
        for f in isl:
            v = [l.vert.co for l in f.loops]
            for i in range(1, len(v) - 1):
                vol += v[0].dot(v[i].cross(v[i + 1]))
        if vol < 0:
            inv += 1
            bmesh.ops.reverse_faces(bm, faces=isl)
    if inv:
        bm.normal_update()
    return inv