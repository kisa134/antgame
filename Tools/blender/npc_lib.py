# Ant Kitchen: shared helpers for the jointed creatures (npc_cat.py, npc_dog.py, npc_grandma.py, npc_kid.py).
# Same idea as human_build.py: one closed-solid FBX per rigid segment, origin at its joint, modelled in a straight
# "bind" pose; the game (NpcBody / QuadBody / HumanBody) places the segments every frame.
# Frame: metres, +Z up, the creature FACES +X, its LEFT side is +Y (UE mesh-local = (100x, -100y, 100z)).
import bpy, bmesh, sys, os, math, json, random
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import kitchen_lib as L
from mathutils import Vector, Matrix, Euler
mat = L.mat
SWAPXZ = Matrix(((0, 0, 1, 0), (0, 1, 0, 0), (1, 0, 0, 0), (0, 0, 0, 1)))

def emit(bm, m=None, M=None, floor=None):
    if m is not None:
        i = L._slot(m)
        for f in bm.faces:
            f.material_index = i
    if M is not None:
        bm.transform(M)
    if floor is not None:
        for v in bm.verts:
            if v.co.z < floor:
                v.co.z = floor
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    L._merge(L.S.stack[-1], bm)

def _ring(bm, z, cx, cy, rx, ry, n, pw):
    out = []
    e = 2.0 / pw
    for i in range(n):
        t = 2 * math.pi * i / n
        c, s = math.cos(t), math.sin(t)
        out.append(bm.verts.new((cx + rx * math.copysign(abs(c) ** e, c), cy + ry * math.copysign(abs(s) ** e, s), z)))
    return out

def loft(rings, m, n=20, lo=None, hi=None, M=None, pw=2.0, stripe=None, bands=None, noise=0.0, seed=7, floor=None):
    # A closed solid through elliptic rings (z, cx, cy, rx, ry), bottom to top. lo / hi: height of a rounded cap below the
    # first / above the last ring (None = flat). pw > 2: boxier rings. stripe=(matA, matB, period): every period-th facet
    # column is matB. bands=[mat, ...]: one material per span between two rings (tail rings, socks, sleeves).
    bm = bmesh.new()
    full = []
    z0, cx0, cy0, rx0, ry0 = rings[0]
    if lo:
        for a in (67.5, 45.0, 22.5):
            k, d = math.cos(math.radians(a)), math.sin(math.radians(a))
            full.append((z0 - lo * d, cx0, cy0, rx0 * k, ry0 * k))
    full += list(rings)
    z1, cx1, cy1, rx1, ry1 = rings[-1]
    if hi:
        for a in (22.5, 45.0, 67.5):
            k, d = math.cos(math.radians(a)), math.sin(math.radians(a))
            full.append((z1 + hi * d, cx1, cy1, rx1 * k, ry1 * k))
    R = [_ring(bm, r[0], r[1], r[2], r[3], r[4], n, pw) for r in full]
    base = bands[0] if bands else (stripe[0] if stripe else m)
    ia = L._slot(base)
    ib = L._slot(stripe[1]) if stripe else ia
    per = stripe[2] if stripe else 1
    k0 = 3 if lo else 0
    for k, (a, b) in enumerate(zip(R, R[1:])):
        inside = k0 <= k < k0 + len(rings) - 1
        im = ia
        if bands:
            im = L._slot(bands[min(max(k - k0, 0), len(bands) - 1)])
        for i in range(n):
            j = (i + 1) % n
            f = bm.faces.new((a[i], a[j], b[j], b[i]))
            f.material_index = ib if (stripe and inside and i % per == 0) else im
    ifirst = L._slot(bands[0]) if bands else ia
    ilast = L._slot(bands[-1]) if bands else ia
    if lo:
        p = bm.verts.new((cx0, cy0, z0 - lo))
        for i in range(n):
            bm.faces.new((p, R[0][(i + 1) % n], R[0][i])).material_index = ifirst
    else:
        bm.faces.new(list(reversed(R[0]))).material_index = ifirst
    if hi:
        p = bm.verts.new((cx1, cy1, z1 + hi))
        for i in range(n):
            bm.faces.new((p, R[-1][i], R[-1][(i + 1) % n])).material_index = ilast
    else:
        bm.faces.new(R[-1]).material_index = ilast
    if noise > 0:
        rnd = random.Random(seed)
        for v in bm.verts:
            v.co += Vector((rnd.uniform(-noise, noise), rnd.uniform(-noise, noise), rnd.uniform(-noise, noise)))
    emit(bm, None, M, floor)

def loftx(rings, m, **kw):
    # The same, lying along X: rings are (x, zc, y, rz, ry).
    loft(rings, m, M=SWAPXZ, **kw)

def digit(pts, r, m, n=10):
    P = [Vector(p) for p in pts]
    sp = L.spline(P, 3) if len(P) > 2 else P
    d0 = (sp[0] - sp[1]).normalized()
    d1 = (sp[-1] - sp[-2]).normalized()
    path = [sp[0] + d0 * r * 0.95, sp[0] + d0 * r * 0.7, sp[0] + d0 * r * 0.35] + sp + [sp[-1] + d1 * r * 0.35, sp[-1] + d1 * r * 0.7, sp[-1] + d1 * r * 0.95]
    rad = [r * 0.3, r * 0.72, r * 0.94] + [r] * len(sp) + [r * 0.94, r * 0.72, r * 0.3]
    L.tube(path, rad, m, n)

def fluff(r, x, y, z, m="Fluff", seed=1, nu=10, nv=6, scale=(1, 1, 1), amp=0.16):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=nu, v_segments=nv, radius=r)
    rnd = random.Random(seed)
    for v in bm.verts:
        k = 1.0 + rnd.uniform(-amp, amp)
        v.co = Vector((v.co.x * scale[0] * k, v.co.y * scale[1] * k, v.co.z * scale[2] * k))
    emit(bm, m, Matrix.Translation((x, y, z)))

def cone(r, h, x, y, z, m, n=14, rot=None, scale=(1, 1, 1), tip=0.12):
    # A cone with a rounded tip, base at (x, y, z), pointing +Z before rot (degrees XYZ).
    prof = [(0.0, 0.0), (r, 0.0), (r * 0.62, h * 0.45), (r * tip * 2.2, h * 0.9), (0.0, h)]
    bm = bmesh.new()
    rings = []
    for (rr, hh) in prof:
        if rr <= 1e-7:
            rings.append([bm.verts.new((0, 0, hh))])
        else:
            rings.append([bm.verts.new((rr * math.cos(2 * math.pi * i / n) * scale[0], rr * math.sin(2 * math.pi * i / n) * scale[1], hh)) for i in range(n)])
    for a, b in zip(rings, rings[1:]):
        for i in range(n):
            j = (i + 1) % n
            if len(a) == 1:
                bm.faces.new((a[0], b[i], b[j]))
            elif len(b) == 1:
                bm.faces.new((a[i], b[0], a[j]))
            else:
                bm.faces.new((a[i], b[i], b[j], a[j]))
    emit(bm, m, L._mx(x, y, z, rot))

def eball(r, x, y, z, m, scale=(1, 1, 1), nu=16, nv=8, rot=None, floor=None, ceil=None):
    # An ellipsoid; floor / ceil flatten it at a world height (flat soles).
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=nu, v_segments=nv, radius=r)
    for v in bm.verts:
        v.co = Vector((v.co.x * scale[0], v.co.y * scale[1], v.co.z * scale[2]))
    bm.transform(L._mx(x, y, z, rot))
    for v in bm.verts:
        if floor is not None and v.co.z < floor:
            v.co.z = floor
        if ceil is not None and v.co.z > ceil:
            v.co.z = ceil
    emit(bm, m)

def ring_x(x, cy, cz, ry, rz, m, w=0.012, k=1.03, n=20):
    # A thin stripe all the way round a body that lies along X: a flat disc a little wider than the body there.
    eball(1.0, x, cy, cz, m, scale=(w * 0.5, ry * k, rz * k), nu=n, nv=6)

def band_x(x, cy, cz, ry_at, rz_at, m, w=0.03, k=1.02, n=20):
    # A stripe round a body lying along X: a band w wide that follows the body's own outline (ry_at(x), rz_at(x)), standing k proud of it.
    e = w * 0.22
    xs = (x - w / 2, x - w / 2 + e, x + w / 2 - e, x + w / 2)
    ks = (0.985, k, k, 0.985)
    loftx([(xx, cz, cy, rz_at(xx) * kk, ry_at(xx) * kk) for xx, kk in zip(xs, ks)], m, n=n)

def ell_r(dx, rx, r):
    # Radius of an ellipsoid's cross-section dx from its centre along the axis whose radius is rx.
    return r * math.sqrt(max(0.0, 1.0 - (dx / rx) ** 2))

def grow(k, about):
    a = Vector(about)
    L.S.stack[0].transform(Matrix.Translation(a) @ Matrix.Scale(k, 4) @ Matrix.Translation(-a))

class Rig:
    # Collects the segments of one creature in the order the game's enum uses.
    def __init__(self, prefix):
        self.prefix = prefix
        self.segs = []     # (name, parent, pivot)
    def seg(self, name, parent, pivot):
        L.piece(self.prefix + name, origin=pivot)
        self.segs.append((name, parent, Vector(pivot)))
    def names(self):
        return [n for n, p, v in self.segs]
    def finish(self, out, what, inl=None, table=None, struct="FNpcSegDef", order=None, dry=False, extra=None, inl_extra=""):
        names = self.names()
        total = 0
        pieces = []
        for n, par, piv in self.segs:
            p = L.PIECES[self.prefix + n]
            total += p["tris"]
            pp = self.segs[names.index(par)][2] if par else Vector((0, 0, 0))
            d = piv - pp
            pieces.append({
                "name": self.prefix + n, "file": self.prefix + n + ".fbx", "parent": (self.prefix + par) if par else None,
                "pivot_m": [round(c, 5) for c in piv],
                "pivot_game_uu": [round(piv.x * 1900, 1), round(-piv.y * 1900, 1), round(piv.z * 1900, 1)],
                "offset_from_parent_game_uu": [round(d.x * 1900, 1), round(-d.y * 1900, 1), round(d.z * 1900, 1)],
                "bbox_min_m": [round(c, 4) for c in p["mn"]], "bbox_max_m": [round(c, 4) for c in p["mx"]],
                "size_game_uu": [round((p["mx"][i] - p["mn"][i]) * 1900, 1) for i in range(3)],
                "triangles": p["tris"], "materials": p["mats"], "open_edges": p["open_edges"]})
            print("PIECE %-14s tris=%5d open=%d inv=%d slots=%s" % (n, p["tris"], p["open_edges"], p["inverted"], ",".join(p["mats"])))
        print("TOTAL segments=%d tris=%d" % (len(self.segs), total))
        doc = {"what": what, "conventions": {
            "blender": "metres, +Z up, faces +X, left is +Y; bind pose: spine, neck and tail straight along X, legs straight down",
            "game": "default UE FBX import: mesh-local (100x, -100y, 100z) = X forward, Y right, Z up; component scale 19 (1 m = 1900 uu)"},
            "materials": L.MATINFO, "totals": {"segments": len(self.segs), "triangles": total}, "pieces": pieces}
        if extra:
            doc.update(extra)
        if dry:
            return doc
        os.makedirs(out, exist_ok=True)
        L.export_all(out)
        with open(os.path.join(out, "manifest.json"), "w", encoding="utf-8") as f:
            json.dump(doc, f, indent=1)
        if inl and table:
            # order: the game's enum order (names; None for a segment this creature does not have).
            order = order or names
            with open(inl, "w", encoding="utf-8-sig", newline="\n") as f:
                f.write("// Generated by Tools/blender/%s. Do not edit by hand.\n" % os.path.basename(sys.argv[sys.argv.index("--python") + 1] if "--python" in sys.argv else "npc_*.py"))
                f.write("// Segment mesh (null: this creature has no such segment), parent segment, joint in the bind pose: body frame, uu, X forward, Y right, Z up, floor at 0.\n")
                f.write("static const %s %s[] =\n{\n" % (struct, table))
                for n in order:
                    if n is None or n not in names:
                        f.write("\t{ nullptr, -1, FVector(0.0f, 0.0f, 0.0f) },\n")
                        continue
                    nm, par, piv = self.segs[names.index(n)]
                    pi = order.index(par) if par else -1
                    f.write("\t{ TEXT(\"%s%s\"), %d, FVector(%.1ff, %.1ff, %.1ff) },\n" % (self.prefix, nm, pi, piv.x * 1900, -piv.y * 1900, piv.z * 1900))
                f.write("};\n")
                f.write(inl_extra)
        print("EXPORTED %d to %s" % (len(self.segs), out))
        return doc

# ------------------------------------------------------------------ preview (same session, after finish)
def preview(rig, out_png, shots, poses=None, floor_size=3.0, wall_x=-1.2):
    # shots: [(name, [(pose, loc, yaw)], cam, target, lens, (w, h))]. poses: {pose: {seg: (rx, ry, rz) deg, "_root": (x, y, z)}}.
    scene = bpy.context.scene
    names = rig.names()
    PIV = {n: v for n, p, v in rig.segs}
    PAR = {n: p for n, p, v in rig.segs}
    for n in names:
        L.PIECES[rig.prefix + n]["ob"].hide_render = True
    kit = bpy.data.collections.new("Preview")
    scene.collection.children.link(kit)
    def Rm(spec):
        return Euler([math.radians(a) for a in spec], "XYZ").to_matrix().to_4x4()
    def figure(pose, loc, yaw):
        P = (poses or {}).get(pose, {})
        root = Matrix.Translation(Vector(loc)) @ Matrix.Rotation(math.radians(yaw), 4, "Z") @ Matrix.Translation(Vector(P.get("_root", (0, 0, 0))))
        W = {}
        for n in names:
            par = PAR[n]
            base = root @ Matrix.Translation(PIV[n]) if par is None else W[par] @ Matrix.Translation(PIV[n] - PIV[par])
            W[n] = base @ Rm(P.get(n, (0, 0, 0)))
            src = L.PIECES[rig.prefix + n]["ob"]
            ob = bpy.data.objects.new("pv_" + n, src.data)
            kit.objects.link(ob)
            ob.matrix_world = W[n]
    def mk(name, rgb, rough=0.5):
        m = bpy.data.materials.new("P_" + name)
        m.use_nodes = True
        b = m.node_tree.nodes.get("Principled BSDF")
        b.inputs["Base Color"].default_value = (*rgb, 1)
        b.inputs["Roughness"].default_value = rough
        return m
    def sbox(name, x0, x1, y0, y1, z0, z1, m):
        me = bpy.data.meshes.new(name)
        v = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
        f = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
        me.from_pydata(v, [], f)
        me.materials.append(m)
        o = bpy.data.objects.new(name, me)
        scene.collection.objects.link(o)
    FLOOR = mk("Floor", (0.8, 0.8, 0.8), 0.35)
    nt = FLOOR.node_tree
    chk = nt.nodes.new("ShaderNodeTexChecker")
    chk.inputs["Scale"].default_value = 4.0
    chk.inputs["Color1"].default_value = (0.93, 0.90, 0.84, 1)
    chk.inputs["Color2"].default_value = (0.30, 0.34, 0.38, 1)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    nt.links.new(tc.outputs["Object"], chk.inputs["Vector"])
    nt.links.new(chk.outputs["Color"], nt.nodes["Principled BSDF"].inputs["Base Color"])
    sbox("Floor", -floor_size, floor_size, -floor_size, floor_size, -0.05, 0.0, FLOOR)
    sbox("WallBack", wall_x - 0.1, wall_x, -floor_size, floor_size, 0, 3.0, mk("Wall", (0.96, 0.80, 0.52), 0.9))
    world = bpy.data.worlds.new("World")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.85, 0.92, 1.0, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.45
    scene.world = world
    def light(kind, loc, energy, rot=(0, 0, 0), size=1.0, color=(1, 1, 1)):
        d = bpy.data.lights.new(kind, kind)
        d.energy = energy
        d.color = color
        if kind == "AREA":
            d.size = size
        if kind == "SUN":
            d.angle = math.radians(6)
        o = bpy.data.objects.new(kind, d)
        o.location = loc
        o.rotation_euler = [math.radians(a) for a in rot]
        scene.collection.objects.link(o)
    light("SUN", (4, -4, 5), 2.2, (48, 0, 38), color=(1.0, 0.95, 0.86))
    light("AREA", (2.5, 0.0, 2.8), 120, (0, 40, 0), 4.5, (1.0, 0.96, 0.9))
    light("AREA", (2.0, 2.5, 1.2), 50, (0, 70, 50), 3.0, (0.8, 0.9, 1.0))
    camd = bpy.data.cameras.new("Cam")
    cam = bpy.data.objects.new("Cam", camd)
    scene.collection.objects.link(cam)
    scene.camera = cam
    for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        try:
            scene.render.engine = eng
            break
        except Exception:
            pass
    for attr, val in (("taa_render_samples", 32), ("use_gtao", True), ("use_shadows", True)):
        try:
            setattr(scene.eevee, attr, val)
        except Exception:
            pass
    try:
        scene.view_settings.view_transform = "Standard"
    except Exception:
        pass
    for (name, figs, c, tg, lens, res) in shots:
        for o in list(kit.objects):
            bpy.data.objects.remove(o)
        for (pose, loc, yaw) in figs:
            figure(pose, loc, yaw)
        cam.location = Vector(c)
        d = Vector(tg) - Vector(c)
        cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
        camd.lens = lens
        scene.render.resolution_x, scene.render.resolution_y = res
        scene.render.filepath = out_png.replace("*", name)
        scene.render.image_settings.file_format = "JPEG"
        scene.render.image_settings.quality = 88
        bpy.ops.render.render(write_still=True)
        print("RENDERED", scene.render.filepath)
