# Ant Kitchen: cut the Meshy ant (one fused body, a tibia with its tarsus, a one-piece feeler) into the rigid segments
# of a jointed insect. Nothing is remodelled: every piece keeps the triangles and the UVs of the mesh it was cut from,
# so it is drawn with the same material instance as before (MI_Ant_Body, MI_Ant_Tibia, MI_Ant_Antenna_Flagellum).
# The cuts are closed with small domes so a bent joint shows chitin, not a hole.
#   body  -> Head, Jaw (the lower mandible plate), Thorax, Petiole, Gaster
#   tibia -> Tibia (shaft with its spurs), Tarsus (the five tarsomeres and the claws)
#   flagellum -> FlagA, FlagB
# Frame of the exported pieces ("ant-local", the units AAntCharacter::BuildBody always used: the body is 98 long,
# its box centre at (-6, 0, 6), the head towards +X): 1 unit = 1 cm in the FBX, origin of each piece = its joint.
# UE mesh-local = (x, -y, z). Limb pieces lie along +X from their joint and are 100 long.
# Run: blender --background --factory-startup --python ant_build.py -- <SourceArt/Meshy> <out dir> [preview]
import bpy, bmesh, sys, os, glob, math, json
from mathutils import Vector, Matrix, Euler

argv = sys.argv[sys.argv.index("--") + 1:]
SRC, OUT = argv[0], argv[1]
PREVIEW = len(argv) > 2
os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene

def load(part):
    fbx = glob.glob(os.path.join(SRC, part, "*", "*.fbx"))[0]
    before = set(bpy.data.objects)
    try:
        bpy.ops.wm.fbx_import(filepath=fbx)
    except Exception:
        bpy.ops.import_scene.fbx(filepath=fbx)
    obs = [o for o in bpy.data.objects if o not in before]
    ob = [o for o in obs if o.type == "MESH"][0]
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bm.transform(ob.matrix_world)
    mat = ob.material_slots[0].material if ob.material_slots else None
    tex = glob.glob(os.path.join(os.path.dirname(fbx), "*_texture.png"))
    for o in obs:
        bpy.data.objects.remove(o)
    return bm, (tex[0] if tex else None)

def bbox(bm):
    xs = [v.co for v in bm.verts]
    return (Vector((min(v.x for v in xs), min(v.y for v in xs), min(v.z for v in xs))),
            Vector((max(v.x for v in xs), max(v.y for v in xs), max(v.z for v in xs))))

def boundary_loops(bm, on_plane=None):
    """Ordered loops of boundary edges. on_plane=(co, no): only loops lying on that plane."""
    edges = [e for e in bm.edges if len(e.link_faces) == 1]
    if on_plane:
        co, no = on_plane
        edges = [e for e in edges if all(abs((v.co - co).dot(no)) < 2e-4 for v in e.verts)]
    left = set(edges)
    loops = []
    while left:
        e = left.pop()
        loop = [e.verts[0], e.verts[1]]
        grew = True
        while grew:
            grew = False
            for e2 in list(loop[-1].link_edges):
                if e2 in left:
                    left.discard(e2)
                    loop.append(e2.other_vert(loop[-1]))
                    grew = True
                    break
        if loop[0] == loop[-1]:
            loop.pop()
        loops.append(loop)
    return loops

def dome(bm, loop, n, bulge):
    """Closes one cut with a dome that swells along n. Its UVs repeat the rim's, so it takes the rim's colour."""
    if len(loop) < 3:
        return
    uv = bm.loops.layers.uv.active
    c = sum((v.co for v in loop), Vector()) / len(loop)
    r = sum((v.co - c).length for v in loop) / len(loop)
    def vuv(v):
        for l in v.link_loops:
            return l[uv].uv.copy()
        return Vector((0.5, 0.5))
    uvs = [vuv(v) for v in loop]
    ring1 = [bm.verts.new(c + (v.co - c) * 0.80 + n * r * bulge * 0.55) for v in loop]
    ring2 = [bm.verts.new(c + (v.co - c) * 0.45 + n * r * bulge * 0.90) for v in loop]
    apex = bm.verts.new(c + n * r * bulge)
    N = len(loop)
    def face(vs, us):
        try:
            f = bm.faces.new(vs)
        except ValueError:
            return
        f.smooth = True
        for l, u in zip(f.loops, us):
            l[uv].uv = u
    for i in range(N):
        j = (i + 1) % N
        face((loop[i], loop[j], ring1[j], ring1[i]), (uvs[i], uvs[j], uvs[j], uvs[i]))
        face((ring1[i], ring1[j], ring2[j], ring2[i]), (uvs[i], uvs[j], uvs[j], uvs[i]))
        face((ring2[i], ring2[j], apex), (uvs[i], uvs[j], uvs[i]))

def slab(src, cuts, bulge=0.7):
    """A copy of src between planes. cuts: list of (co, no): the side no points to is thrown away and domed."""
    bm = src.copy()
    for co, no in cuts:
        no = no.normalized()
        geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
        bmesh.ops.bisect_plane(bm, geom=geom, dist=1e-6, plane_co=co, plane_no=no, clear_outer=True, clear_inner=False)
        for loop in boundary_loops(bm, (co, no)):
            dome(bm, loop, no, bulge)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm

def islands(bm):
    seen, out = set(), []
    for v in bm.verts:
        if v in seen:
            continue
        stack, isl = [v], []
        seen.add(v)
        while stack:
            a = stack.pop()
            isl.append(a)
            for e in a.link_edges:
                b = e.other_vert(a)
                if b not in seen:
                    seen.add(b)
                    stack.append(b)
        out.append(isl)
    return out

PIECES = {}   # name -> dict(bm, pivot (source frame), tex, src)
def piece(name, bm, pivot, src):
    PIECES[name] = {"bm": bm, "pivot": Vector(pivot), "src": src}

# ------------------------------------------------------------------------------------------------ the body
body, TEX_BODY = load("SM_Ant_Body")
bmn, bmx = bbox(body)
BC = (bmn + bmx) * 0.5
K = 98.0 / (bmx.x - bmn.x)
X = Vector((1, 0, 0))
X_GASTER, X_PET, X_NECK = -0.0880, -0.0215, 0.2313

def ring_centre(bm, x):
    vs = [v.co for v in bm.verts if abs(v.co.x - x) < 0.006]
    return Vector((x, sum(v.y for v in vs) / len(vs), (min(v.z for v in vs) + max(v.z for v in vs)) * 0.5)) if vs else Vector((x, 0, 0))
P_GASTER, P_PET, P_NECK = ring_centre(body, X_GASTER), ring_centre(body, X_PET), ring_centre(body, X_NECK)
P_GASTER.y = P_PET.y = P_NECK.y = 0.0

gaster = slab(body, [(P_GASTER, X)], 0.8)
petiole = slab(body, [(P_GASTER, -X), (P_PET, X)], 0.8)
thorax = slab(body, [(P_PET, -X), (P_NECK, X)], 0.55)
head_all = slab(body, [(P_NECK, -X)], 0.55)

# The jaw: the lower plate of the beak-like mouth. It hangs on the head along a crease that runs (seen from the side)
# from the corner of the mouth down and back to the underside of the head.
JA, JB = Vector((0.4000, 0, -0.0160)), Vector((0.3460, 0, -0.0890))
jn = Vector((-(JB.z - JA.z), 0, JB.x - JA.x)).normalized()
if jn.x < 0:
    jn = -jn
JP = JA + jn * 0.002
# the line between the upper beak and the jaw (side view): from the corner of the mouth forward and down
LC, LD = Vector((0.4025, 0, -0.0180)), Vector((0.0425, 0, -0.0440)).normalized()
def below_mouth(p):
    return LD.x * (p.z - LC.z) - LD.z * (p.x - LC.x)   # < 0: the jaw's side
tag = head_all.faces.layers.int.new("jaw")
cand = [f for f in head_all.faces if below_mouth(f.calc_center_median()) < 0.004]
geom = set(cand)
for f in cand:
    geom.update(f.edges)
    geom.update(f.verts)
res = bmesh.ops.bisect_plane(head_all, geom=list(geom), dist=1e-6, plane_co=JP, plane_no=jn, clear_outer=False, clear_inner=False)
cut_edges = {e for e in res["geom_cut"] if isinstance(e, bmesh.types.BMEdge)}
head_all.faces.ensure_lookup_table()
TIP = Vector((0.4750, 0, -0.1400))
seed = min(head_all.faces, key=lambda f: (f.calc_center_median() - TIP).length)
jawset, stack, leak = {seed}, [seed], 0
while stack:
    f = stack.pop()
    for e in f.edges:
        if e in cut_edges:
            continue
        for g in e.link_faces:
            if g in jawset:
                continue
            c = g.calc_center_median()
            if below_mouth(c) > 0.010 or (c - JP).dot(jn) < -0.004:
                leak += 1
                continue
            jawset.add(g)
            stack.append(g)
for f in head_all.faces:
    f[tag] = 1 if f in jawset else 0
print("ANTB tagged", sum(1 for f in head_all.faces if f[tag] == 1))
print("ANTB jaw faces %d of %d, cut edges %d, leak stops %d" % (len(jawset), len(head_all.faces), len(cut_edges), leak))
jaw = head_all.copy()
head = head_all
for bm, keep in ((jaw, 1), (head, 0)):
    t = bm.faces.layers.int["jaw"]
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f[t] != keep], context="FACES")
    loops = [l for l in boundary_loops(bm) if all((v.co - JP).dot(jn) > -0.012 and v.co.x > 0.33 for v in l)]
    for l in loops:
        if len(l) >= 3:
            dome(bm, l, jn * (-1 if keep == 1 else 1), 0.35)
    print("ANTB %s: capped %d loop(s) of %s verts" % ("jaw" if keep else "head", len(loops), [len(l) for l in loops]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
jmn, jmx = bbox(jaw)
print("ANTB jaw box", [round(c, 4) for c in jmn], [round(c, 4) for c in jmx])

piece("Thorax", thorax, (P_PET + P_NECK) * 0.5, "SM_Ant_Body")
piece("Head", head, P_NECK, "SM_Ant_Body")
piece("Jaw", jaw, JA, "SM_Ant_Body")
piece("Petiole", petiole, P_PET, "SM_Ant_Body")
piece("Gaster", gaster, P_GASTER, "SM_Ant_Body")

# ------------------------------------------------------------------------------------------------ the legs and the feelers
tib, TEX_TIB = load("SM_Ant_Tibia")
tmn, tmx = bbox(tib)
ttag = tib.verts.layers.int.new("tarsus")
isl = sorted(islands(tib), key=len, reverse=True)
info = []
for I in isl:
    xs = [v.co.x for v in I]
    info.append((len(I), round(min(xs), 4), round(max(xs), 4)))
print("ANTB tibia islands", info[:6], "... total", len(isl))
# the tarsus is the second big island (the five little segments and the claws at the +X end)
tarsus_isl = max(isl[1:], key=lambda I: len(I)) if len(isl) > 1 else []
X_TARSUS = min(v.co.x for v in tarsus_isl) if tarsus_isl else tmx.x - 0.2 * (tmx.x - tmn.x)
tarsus_set = set()
for I in isl:
    cx = sum(v.co.x for v in I) / len(I)
    if cx > X_TARSUS:
        tarsus_set.update(I)
for v in tib.verts:
    v[ttag] = 1 if v in tarsus_set else 0
tarsus = tib.copy()
bmesh.ops.delete(tarsus, geom=[v for v in tarsus.verts if v[tarsus.verts.layers.int["tarsus"]] == 0], context="VERTS")
bmesh.ops.delete(tib, geom=[v for v in tib.verts if v[tib.verts.layers.int["tarsus"]] == 1], context="VERTS")
t2mn, t2mx = bbox(tarsus)
s2mn, s2mx = bbox(tib)
print("ANTB tibia shaft x %.4f..%.4f, tarsus x %.4f..%.4f" % (s2mn.x, s2mx.x, t2mn.x, t2mx.x))
def end_centre(bm, x, w=0.02):
    vs = [v.co for v in bm.verts if abs(v.co.x - x) < w]
    return Vector((x, sum(v.y for v in vs) / len(vs), sum(v.z for v in vs) / len(vs))) if vs else Vector((x, 0, 0))
piece("Tibia", tib, end_centre(tib, s2mn.x + 0.02), "SM_Ant_Tibia")
PIECES["Tibia"]["end"] = Vector((X_TARSUS, 0, 0))
piece("Tarsus", tarsus, Vector((X_TARSUS, end_centre(tarsus, t2mn.x + 0.01).y, end_centre(tarsus, t2mn.x + 0.01).z)), "SM_Ant_Tibia")
PIECES["Tarsus"]["end"] = Vector((t2mx.x, 0, 0))

flag, TEX_FLAG = load("SM_Ant_Antenna_Flagellum")
fmn, fmx = bbox(flag)
# cut at the thinnest waist between two beads near the middle
best = None
for i in range(200):
    x = fmn.x + (fmx.x - fmn.x) * (0.35 + 0.30 * i / 200.0)
    vs = [v.co for v in flag.verts if abs(v.co.x - x) < 0.004]
    if len(vs) < 4:
        continue
    r = max(math.hypot(v.y, v.z) for v in vs)
    if best is None or r < best[0]:
        best = (r, x)
X_FLAG = best[1]
print("ANTB flagellum x %.4f..%.4f, cut at %.4f (radius %.4f)" % (fmn.x, fmx.x, X_FLAG, best[0]))
# which end is the base? the club (thick end) is the tip
def end_r(x0, x1):
    vs = [v.co for v in flag.verts if x0 <= v.co.x <= x1]
    return max(math.hypot(v.y, v.z) for v in vs)
club_plus = end_r(fmx.x - 0.12, fmx.x) > end_r(fmn.x, fmn.x + 0.12)
print("ANTB flagellum club at +X:", club_plus)
PF = Vector((X_FLAG, 0, 0))
fa = slab(flag, [(PF, X if club_plus else -X)], 0.6)
fb = slab(flag, [(PF, -X if club_plus else X)], 0.6)
piece("FlagA", fa, Vector((fmn.x if club_plus else fmx.x, 0, 0)), "SM_Ant_Antenna_Flagellum")
PIECES["FlagA"]["end"] = PF.copy()
piece("FlagB", fb, PF, "SM_Ant_Antenna_Flagellum")
PIECES["FlagB"]["end"] = Vector((fmx.x if club_plus else fmn.x, 0, 0))

# ------------------------------------------------------------------------------------------------ into the ant's frame, export
def body_local(p):
    return Vector(((p.x - BC.x) * K - 6.0, (p.y - BC.y) * K, (p.z - BC.z) * K + 6.0))

manifest = {"frame": "ant-local: body 98 long, box centre (-6,0,6), head +X, Z up; UE mesh-local = (x, -y, z); 1 unit = 1 uu at component scale 1",
            "k_body": K, "pieces": []}
OBJ = {}
for name, p in PIECES.items():
    bm = p["bm"]
    limb = "end" in p
    if limb:
        # along +X from the joint, 100 long
        d = p["end"].x - p["pivot"].x
        s = 100.0 / abs(d)
        sign = 1.0 if d > 0 else -1.0
        M = Matrix.Diagonal((s * sign, s * sign, s, 1.0)) @ Matrix.Translation(-p["pivot"])   # a half-turn about Z when it points to -X
        joint = Vector((0, 0, 0))
    else:
        M = Matrix.Diagonal((K, K, K, 1.0)) @ Matrix.Translation(-p["pivot"])
        joint = body_local(p["pivot"])
    bm.transform(M)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    mn, mx = bbox(bm)
    open_e = sum(1 for e in bm.edges if len(e.link_faces) != 2)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
    for f in bm.faces:
        f.smooth = True
    me = bpy.data.meshes.new("SM_AntB_" + name)
    cm = bm.copy()
    cm.transform(Matrix.Diagonal((0.01, 0.01, 0.01, 1.0)))   # 1 unit = 1 cm in the file
    cm.to_mesh(me)
    cm.free()
    mat = bpy.data.materials.get("M_" + p["src"]) or bpy.data.materials.new("M_" + p["src"])
    me.materials.append(mat)
    ob = bpy.data.objects.new("SM_AntB_" + name, me)
    scene.collection.objects.link(ob)
    OBJ[name] = ob
    rec = {"name": "SM_AntB_" + name, "from": p["src"], "joint": [round(joint.x, 3), round(-joint.y, 3), round(joint.z, 3)],
           "min": [round(mn.x, 3), round(-mx.y, 3), round(mn.z, 3)], "max": [round(mx.x, 3), round(-mn.y, 3), round(mx.z, 3)],
           "tris": sum(len(f.verts) - 2 for f in bm.faces), "open_edges": open_e, "limb": limb}
    manifest["pieces"].append(rec)
    print("ANTB PIECE %-8s joint %s box %s .. %s tris %d open %d" % (name, rec["joint"], rec["min"], rec["max"], rec["tris"], open_e))

if not PREVIEW:
    for name, ob in OBJ.items():
        for o in scene.objects:
            o.select_set(False)
        ob.select_set(True)
        bpy.context.view_layer.objects.active = ob
        bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "SM_AntB_" + name + ".fbx"), use_selection=True, object_types={"MESH"},
                                 use_mesh_modifiers=True, mesh_smooth_type="OFF", bake_anim=False, add_leaf_bones=False)
        print("EXPORTED", name)
    json.dump(manifest, open(os.path.join(OUT, "ant_manifest.json"), "w", encoding="utf-8"), indent=1)
    J = {p["name"][8:]: p["joint"] for p in manifest["pieces"]}
    B = {p["name"][8:]: (p["min"], p["max"]) for p in manifest["pieces"]}
    with open(os.path.join(OUT, "AntBodyData.inl"), "w", encoding="utf-8") as f:
        f.write("// Generated by Tools/blender/ant_build.py: where the cut pieces of the ant hinge (ant-local units, UE axes).\n")
        for n in ("Thorax", "Head", "Jaw", "Petiole", "Gaster"):
            f.write("static const FVector AntB_%s(%.3ff, %.3ff, %.3ff);\n" % (n, J[n][0], J[n][1], J[n][2]))
        for n in ("Tibia", "Tarsus", "FlagA", "FlagB"):
            mn, mx = B[n]
            f.write("static const float AntB_%sThick = %.3ff;   // across, for a piece 100 long\n" % (n, max(mx[1] - mn[1], mx[2] - mn[2])))
        f.write("static const float AntB_HeadTipX = %.3ff;\n" % (B["Head"][1][0]))
        f.write("static const float AntB_GasterTipX = %.3ff;\n" % (B["Gaster"][0][0]))
    print("BUILD_DONE")

# ------------------------------------------------------------------------------------------------ preview
if PREVIEW:
    def textured(name, path):
        m = bpy.data.materials.get(name)
        m.use_nodes = True
        nt = m.node_tree
        b = nt.nodes.get("Principled BSDF")
        if path:
            t = nt.nodes.new("ShaderNodeTexImage")
            t.image = bpy.data.images.load(path)
            nt.links.new(t.outputs["Color"], b.inputs["Base Color"])
        b.inputs["Roughness"].default_value = 0.35
    textured("M_SM_Ant_Body", TEX_BODY)
    textured("M_SM_Ant_Tibia", TEX_TIB)
    textured("M_SM_Ant_Antenna_Flagellum", TEX_FLAG)
    world = bpy.data.worlds.new("World"); world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (1, 1, 1, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 1.1
    scene.world = world
    camd = bpy.data.cameras.new("Cam"); camd.type = "ORTHO"
    cam = bpy.data.objects.new("Cam", camd); scene.collection.objects.link(cam); scene.camera = cam
    for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        try:
            scene.render.engine = eng; break
        except Exception:
            pass
    try:
        scene.view_settings.view_transform = "Standard"
    except Exception:
        pass
    sun = bpy.data.lights.new("Sun", "SUN"); sun.energy = 2.5
    so = bpy.data.objects.new("Sun", sun); so.rotation_euler = (math.radians(40), 0, math.radians(30)); scene.collection.objects.link(so)
    scene.render.resolution_x = 1000; scene.render.resolution_y = 560
    J = {p["name"][8:]: Vector((p["joint"][0], -p["joint"][1], p["joint"][2])) * 0.01 for p in manifest["pieces"]}
    def R(spec):
        return Euler([math.radians(a) for a in spec], "XYZ").to_matrix().to_4x4()
    def pose(P, off):
        T = Matrix.Translation(Vector(off))
        W = {}
        W["Thorax"] = T @ Matrix.Translation(J["Thorax"]) @ R(P.get("Thorax", (0, 0, 0)))
        W["Head"] = W["Thorax"] @ Matrix.Translation(J["Head"] - J["Thorax"]) @ R(P.get("Head", (0, 0, 0)))
        W["Jaw"] = W["Head"] @ Matrix.Translation(J["Jaw"] - J["Head"]) @ R(P.get("Jaw", (0, 0, 0)))
        W["Petiole"] = W["Thorax"] @ Matrix.Translation(J["Petiole"] - J["Thorax"]) @ R(P.get("Petiole", (0, 0, 0)))
        W["Gaster"] = W["Petiole"] @ Matrix.Translation(J["Gaster"] - J["Petiole"]) @ R(P.get("Gaster", (0, 0, 0)))
        for n in ("Thorax", "Head", "Jaw", "Petiole", "Gaster"):
            o = bpy.data.objects.new(n, OBJ[n].data)
            scene.collection.objects.link(o)
            o.matrix_world = W[n]
        # limb pieces laid out below
        x = -0.5
        for n, sc in (("Tibia", 0.2), ("Tarsus", 0.12), ("FlagA", 0.14), ("FlagB", 0.14)):
            o = bpy.data.objects.new(n, OBJ[n].data)
            scene.collection.objects.link(o)
            o.matrix_world = T @ Matrix.Translation(Vector((x, 0, -0.42))) @ Matrix.Diagonal((sc, sc, sc, 1))
            x += sc + 0.06
    for o in OBJ.values():
        o.hide_render = True
    pose({}, (0, 0, 0))
    pose({"Head": (0, -22, 28), "Jaw": (0, 38, 0), "Petiole": (0, -14, 0), "Gaster": (0, -22, -18)}, (0, 0, -1.0))
    pose({"Head": (20, 25, -30), "Jaw": (0, 20, 0), "Petiole": (0, 12, 10), "Gaster": (0, 25, 20)}, (1.3, 0, 0))
    pose({"Head": (0, 0, 0), "Jaw": (0, 45, 0)}, (1.3, 0, -1.0))
    def shot(name, center, size, d):
        d = Vector(d).normalized()
        cam.location = Vector(center) + d * 8.0
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        camd.ortho_scale = size; camd.clip_start = 0.01; camd.clip_end = 40
        scene.render.filepath = os.path.join(OUT, name + ".png")
        bpy.ops.render.render(write_still=True); print("RENDERED", name)
    shot("prev_side", (0.6, 0, -0.55), 2.9, (0, -1, 0))
    shot("prev_q", (0.6, 0, -0.55), 2.9, (0.7, -0.7, 0.45))
    shot("prev_front", (0.6, 0, -0.55), 2.9, (1, -0.25, -0.2))
    shot("prev_top", (0.6, 0, -0.55), 2.9, (0.05, -0.5, 1))
    print("PREVIEW_DONE")
