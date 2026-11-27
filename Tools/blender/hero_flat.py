# Hero art of the new flat "Dvushka" (KAN-41): the furniture and the big things of the bedroom, the bathroom + WC and the hall.
# Same bar and pipeline as the kitchen and the living room (hero_lib.py, hero_living.py): built by hand in real metres,
# baked C/N/R, LOD0..2, UCX hulls, pre-cut break pieces, M_Hero + the AntState layer in the game.
# What opens is its own mesh with the pivot at its origin (a door on its hinge axis, a drawer at its front): the manifest
# says where it sits on its parent ("attach") and how it moves. Wall-hung things have their origin ON THE FLOOR at the wall.
# Frame of every piece: X to the right, the front is -Y (the back / the wall is +Y), Z up, metres. In the game Y is mirrored.
# Output: SourceArt/Blender/hero_<room>/<Name>/ + hero_<room>_manifest.json; review sheets: Saved/review/flat_art/<room>/<Name>.jpg
#   = overall | the ant's view | open (or the other side) | wet | charred | broken.
# Run: blender --background --factory-startup --python hero_flat.py -- <bedroom|bath|hall> <Name+Name|all> [nobake]
import sys, os
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
_a = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
_room = _a[0] if _a else "bedroom"
_only = _a[1].replace("+", ",").split(",") if len(_a) > 1 and _a[1] not in ("all", "") else None
_nobake = len(_a) > 2 and _a[2] == "nobake"
_root = os.path.abspath(os.path.join(HERE, "..", ".."))
_out = os.path.join(_root, "SourceArt", "Blender", "hero_" + _room)
_rev = os.path.join(_root, "Saved", "review", "flat_art", _room)
# hero_living.py is the library here: it runs its own main() on import, told to build nothing, into our folders
sys.argv = sys.argv[:sys.argv.index("--") + 1] + [_out, _rev, "__none__"] + (["nobake"] if _nobake else [])
import hero_living as HL
from hero_living import *
import bpy, bmesh, math, random, json, time
from mathutils import Vector, Matrix, noise
ROOM, ONLY, NOBAKE, ROOT, OUT, REV = _room, _only, _nobake, _root, _out, _rev
PI = math.pi

def Rz(d): return Matrix.Rotation(math.radians(d), 4, "Z")
def Rx(d): return Matrix.Rotation(math.radians(d), 4, "X")

# ================================================================================================ geometry helpers
def oval(cx, cy, rx, ry, z, n=32):
    return [(cx + rx * math.cos(2 * PI * k / n), cy + ry * math.sin(2 * PI * k / n), z) for k in range(n)]

def rr(hx, hy, z, r, cx=0.0, cy=0.0, n=6):
    """a rounded rectangle ring, centred"""
    return rect_loop(cx - hx, cx + hx, cy - hy, cy + hy, z, max(0.002, min(r, hx - 0.001, hy - 0.001)), n)

def loft(name, rings, m, cap0=True, cap1=True, cyclic=False, sharp=38.0):
    """a skin through rings of equal point count (a tub, a bowl, a coat): closed by caps, or cyclic (a seat ring)"""
    bm = bmesh.new()
    vr = [[bm.verts.new(p) for p in ring] for ring in rings]
    n = len(rings[0])
    pairs = list(zip(vr, vr[1:])) + ([(vr[-1], vr[0])] if cyclic else [])
    for a, b in pairs:
        for k in range(n): bm.faces.new((a[k], a[(k + 1) % n], b[(k + 1) % n], b[k]))
    if not cyclic:
        if cap0: bm.faces.new(vr[0][::-1])
        if cap1: bm.faces.new(vr[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    for f in bm.faces: f.smooth = True
    for e in bm.edges:
        if len(e.link_faces) == 2 and e.calc_face_angle(0.0) > math.radians(sharp): e.smooth = False
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = H.link(bpy.data.objects.new(name, me)); o.data.materials.append(m)
    return o

def strip(name, path, width, m, thick=0.010, nx=8, step=0.03, wave=0.006, seed=1, xc=0.0, gather=None):
    """a strip of cloth hanging along a path [(y, z)] (a towel over a rail, a mat over the tub's side), width along X"""
    pts = []
    for a, b in zip(path, path[1:]):
        a = Vector(a); b = Vector(b); n = max(1, int(round((b - a).length / step)))
        for k in range(n): pts.append(a + (b - a) * (k / n))
    pts.append(Vector(path[-1]))
    rnd = random.Random(seed); ph = [rnd.uniform(0, 6.28) for _ in range(4)]
    bm = bmesh.new(); rows = []
    for i, p in enumerate(pts):
        q = pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]
        nr = Vector((-q.y, q.x)).normalized() if q.length > 1e-9 else Vector((0, 1))
        row = []
        wd = width * (1.0 - gather[1] * math.exp(-((i / max(1, len(pts) - 1) - gather[0]) / 0.10) ** 2)) if gather else width
        for j in range(nx + 1):
            u = j / nx
            w = wave * (math.sin(u * 9.0 + ph[0] + i * 0.11) * 0.6 + math.sin(u * 4.0 + ph[1] - i * 0.07) * 0.4 + math.sin(i * 0.35 + ph[2]) * 0.5)
            row.append(bm.verts.new((xc + (u - 0.5) * wd, p.x + nr.x * w, max(0.004, p.y + nr.y * w))))
        rows.append(row)
    for a, b in zip(rows, rows[1:]):
        for j in range(nx): bm.faces.new((a[j], a[j + 1], b[j + 1], b[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = H.link(bpy.data.objects.new(name, me))
    sd = o.modifiers.new("solid", "SOLIDIFY"); sd.thickness = thick; sd.offset = 0.0
    for p in o.data.polygons: p.use_smooth = True
    o.data.materials.append(m)
    return o

def arc(cy, cz, r, a0, a1, n=6):
    return [(cy + r * math.cos(math.radians(a0 + (a1 - a0) * k / n)), cz + r * math.sin(math.radians(a0 + (a1 - a0) * k / n))) for k in range(n + 1)]

def closed(prof):
    """a lathe profile that is a closed loop off the axis (a ring, a gasket): back to its first point"""
    return list(prof) + [prof[0]]

def hole(o, r, loc, axis="Y", depth=0.2):
    """a round hole through a board (as the cable holes of the TV stand)"""
    c = H.cyl("Hole", r, depth, loc, {"Z": (0, 0, 0), "X": (0, PI / 2, 0), "Y": (PI / 2, 0, 0)}[axis], 48)
    c.data.materials.append(o.data.materials[0])          # (a cutter without a material leaves an empty slot: the bake stops on it)
    md = o.modifiers.new("hole", "BOOLEAN"); md.object = c; md.operation = "DIFFERENCE"
    for x in H.S().objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o; bpy.ops.object.modifier_apply(modifier="hole")
    bpy.data.objects.remove(c)
    return o

def ring_hulls(hs, cx, cy, rxo, ryo, rxi, ryi, z0, z1, n=8):
    """the wall of an oval bowl as n convex sectors (so the inside stays hollow for physics)"""
    for k in range(n):
        g = []
        for a in (2 * PI * k / n, 2 * PI * (k + 1) / n):
            for rx_, ry_ in ((rxo, ryo), (rxi, ryi)):
                for z in (z0, z1): g.append((cx + rx_ * math.cos(a), cy + ry_ * math.sin(a), z))
        hs.groups.append(g)

# ================================================================================================ materials of the three rooms
_fabric0 = fabric
def fabric(*a, **k):
    """the living room's weave without its dark stain spots: on a hanging towel or a duvet they read as big rings"""
    m = _fabric0(*a, **k)
    for n in m.node_tree.nodes:
        if n.type == "MAP_RANGE" and abs(n.inputs["From Min"].default_value - 0.06) < 1e-6 and abs(n.inputs["To Max"].default_value - 0.25) < 1e-6: n.inputs["To Max"].default_value = 0.0
    return m

def CERAMIC(): return M("Ceramic", glossy, (0.93, 0.93, 0.91), 0.07, 0.6)
def ENAMEL():  return M("TubEnamel", glossy, (0.94, 0.94, 0.92), 0.10, 0.5)
def WHITEP():  return M("WhitePlastic", lambda n: H.plastic(n, (0.90, 0.89, 0.86), 0.35, 0.03, 0.2))
def GREYP():   return M("GreyPlastic", lambda n: H.plastic(n, (0.42, 0.43, 0.45), 0.45, 0.03, 0.2))
def LACQ():    return M("WhiteLacquer", paint, (0.90, 0.90, 0.87), 0.25, 0.03, 0.18, (0.62, 0.50, 0.36))
def CHROME():  return M("ChromePlate", lambda n: H.metal(n, (0.90, 0.90, 0.92), 0.10))
def STEEL():   return M("SteelBrushed", lambda n: H.metal(n, (0.66, 0.67, 0.69), 0.32))
def _mirror(n):
    m, p = H.mat(n); p.inputs["Base Color"].default_value = (0.95, 0.96, 0.97, 1); p.inputs["Metallic"].default_value = 1; p.inputs["Roughness"].default_value = 0.015
    return H.tag(m, 1.0, 0.0)
def MIRROR():  return M("MirrorSilver", _mirror)
def calm(m, axis):
    """hero_living.wood leaves the growth rings round X whatever the grain axis: a board with the grain along Y or Z then shows
    zigzag bands ACROSS the grain (seen on a whole door). Here the rings go round the grain axis, as they do for X."""
    for n in m.node_tree.nodes:
        if n.type == "TEX_WAVE" and n.wave_type == "RINGS": n.rings_direction = axis
    return m
def W_WALNUT(axis="Z"): return M("WalnutCalm" + axis, lambda n: calm(wood(n, (0.36, 0.21, 0.10), (0.14, 0.07, 0.03), axis, 26.0, 0.45), axis))
def W_OAK(axis="X"):    return M("OakCalm" + axis, lambda n: calm(wood(n, (0.70, 0.47, 0.24), (0.42, 0.25, 0.11), axis, 20.0, 0.35), axis))
def RUB():     return M("RubberGrey", H.rubber, (0.26, 0.26, 0.27), 0.7)
def RUBK():    return M("RubberBlack", H.rubber, (0.04, 0.04, 0.045), 0.8)
def SMOKED():  return M("SmokedGlass", glossy, (0.025, 0.03, 0.035), 0.04, 0.8)
def WATERS():  return M("WaterStill", glossy, (0.30, 0.45, 0.50), 0.03, 0.6)
def LINEN():   return M("LinenWhite", fabric, (0.88, 0.86, 0.80), (0.78, 0.75, 0.68), 520.0, 0.3)
def TICK():    return M("Ticking", fabric, (0.86, 0.85, 0.80), (0.72, 0.72, 0.70), 420.0, 0.3)
def TERRY(key, col): return M(key, fabric, col, tuple(c * 0.72 for c in col), 300.0, 0.8, 0.3, 1.0)
def W_BEECH(axis="X"): return M("Beech" + axis, lambda n: calm(wood(n, (0.80, 0.62, 0.40), (0.60, 0.42, 0.24), axis, 22.0, 0.2), axis))
def HDF():     return M("HDF", lambda n: calm(wood(n, (0.55, 0.40, 0.25), (0.40, 0.28, 0.17), "Z", 60.0, 0.0, 0.2, 0.0, 0.7), "Z"))
def FELT():    return M("Felt", fabric, (0.30, 0.28, 0.25), None, 900.0, 0.2)
def CARD():    return M("Cardboard", lambda n: H.plastic(n, (0.58, 0.44, 0.28), 0.8, 0.05, 0.1))
def PAPER():   return M("PaperWhite", lambda n: H.tag(H.plastic(n, (0.92, 0.91, 0.87), 0.85, 0.02, 0.05), 0.0, 1.0))

def perforated(name):
    """the drum of the washing machine: stainless steel with rows of small holes"""
    m = H.metal(name, (0.76, 0.77, 0.79), 0.22); p = m.node_tree.nodes["Principled BSDF"]
    def fn(m, src):
        tc = N(m, "ShaderNodeTexCoord", -1400, 900); vo = N(m, "ShaderNodeTexVoronoi", -1200, 900); vo.inputs["Scale"].default_value = 140
        L(m, tc.outputs["Object"], vo.inputs["Vector"])
        mr = N(m, "ShaderNodeMapRange", -1000, 900); mr.inputs["From Min"].default_value = 0.10; mr.inputs["From Max"].default_value = 0.16
        mr.inputs["To Min"].default_value = 1.0; mr.inputs["To Max"].default_value = 0.0; L(m, vo.outputs["Distance"], mr.inputs["Value"])
        mx = N(m, "ShaderNodeMix", -700, 900); mx.data_type = "RGBA"; mx.inputs[7].default_value = (0.02, 0.02, 0.02, 1)
        L(m, mr.outputs["Result"], mx.inputs[0]); L(m, src, mx.inputs[6]); return mx.outputs[2]
    HL._wrap_base(m, p, fn)
    return m

def quilt(m, cell=0.22, depth=6.0, axes="XYZ"):
    """stitched channels of a quilt / a puffer jacket: grooves in the bump every `cell` metres along the given axes"""
    p = m.node_tree.nodes["Principled BSDF"]; bp = p.inputs["Normal"].links[0].from_node
    src = bp.inputs["Height"].links[0].from_socket
    tc = N(m, "ShaderNodeTexCoord", -2000, -900); acc = None
    for k, d in enumerate(axes):
        w = N(m, "ShaderNodeTexWave", -1800, -900 - k * 220); w.wave_type = "BANDS"; w.bands_direction = d
        w.inputs["Scale"].default_value = 0.314 / cell; w.inputs["Distortion"].default_value = 0.0; L(m, tc.outputs["Object"], w.inputs["Vector"])
        mr = N(m, "ShaderNodeMapRange", -1600, -900 - k * 220); mr.inputs["From Min"].default_value = 0.90; mr.inputs["From Max"].default_value = 1.0
        L(m, w.outputs["Fac"], mr.inputs["Value"])
        if acc is None: acc = mr.outputs["Result"]
        else:
            mx = N(m, "ShaderNodeMath", -1400, -900 - k * 220); mx.operation = "MAXIMUM"; L(m, acc, mx.inputs[0]); L(m, mr.outputs["Result"], mx.inputs[1]); acc = mx.outputs[0]
    ma = N(m, "ShaderNodeMath", -1200, -900); ma.operation = "MULTIPLY_ADD"; ma.inputs[1].default_value = -depth
    L(m, acc, ma.inputs[0]); L(m, src, ma.inputs[2]); L(m, ma.outputs[0], bp.inputs["Height"])
    def fn(m, s0):       # the seam is a little darker (shadow in the channel)
        dk = N(m, "ShaderNodeMix", -500, -1200); dk.data_type = "RGBA"; dk.blend_type = "MULTIPLY"; dk.inputs[7].default_value = (0.6, 0.6, 0.6, 1)
        L(m, acc, dk.inputs[0]); L(m, s0, dk.inputs[6]); return dk.outputs[2]
    HL._wrap_base(m, p, fn)
    return m

def knob(x, y, z, m=None, r=0.016):
    """a turned knob on a front face (pointing to -Y) with its rose"""
    k = r / 0.016
    a = lathe("Knob", [(0, 0.0), (0.007 * k, 0.0), (0.006 * k, 0.012), (0.014 * k, 0.018), (0.016 * k, 0.026), (0.010 * k, 0.031), (0, 0.032)], m or BRASS(), (x, y, z), 24, 0, (PI / 2, 0, 0))
    b = cyl("Rose", 0.011 * k, 0.0015, (x, y - 0.0008, z), m or BRASS(), "Y", 20)
    return [a, b]

def cup_hinges(xs, zs, y, parts, part):
    """concealed cup hinges on the back of a door (seen when it stands open): cup, arm"""
    for x in xs:
        for z in zs:
            parts.append((cyl("HingeCup", 0.0175, 0.004, (x, y + 0.002, z), STEEL(), "Y", 20), part))
            parts.append((box("HingeArm", x - 0.022, x + 0.022, y + 0.002, y + 0.012, z - 0.009, z + 0.009, STEEL(), 0.002, 2), part))

# ================================================================================================ BEDROOM
def F_HEAD():  return M("HeadboardWeave", fabric, (0.075, 0.27, 0.32), (0.045, 0.18, 0.22), 260.0, 0.7)        # (the sofa's teal)
def F_DUVET(): return M("DuvetCotton", lambda n: quilt(fabric(n, (0.80, 0.72, 0.56), (0.70, 0.62, 0.47), 520.0, 0.25), 0.24, 7.0))

def build_bed():
    """double bed 1.60 x 2.02 m (head +Y): mattress top 0.55, 0.25 clear under the rails (the ants' cover), padded headboard to 1.10"""
    hw, y0, y1 = 0.80, -1.01, 1.01
    zc, zr = 0.25, 0.40
    hs = Hulls(); parts = []
    oaky = W_OAK("Y"); oakx = W_OAK("X"); oakz = W_OAK("Z"); bee = W_BEECH("X")
    for sx in (-1, 1):
        for sy in (-1, 1):
            x = sx * (hw - 0.03); y = (y0 + 0.03) if sy < 0 else (y1 - 0.075)
            lg = box("Leg", x - 0.03, x + 0.03, y - 0.03, y + 0.03, 0.006, zr, oakz, 0.005, 3); parts.append((lg, 1)); hs.add(lg)
            parts.append((cyl("Glide", 0.022, 0.006, (x, y, 0.003), FELT(), "Z", 16), 1))
            parts += [(o, 1) for o in screws_at("Bolt", [(sx * (hw + 0.0004), y, 0.30, sx), (sx * (hw + 0.0004), y, 0.36, sx)], "X", BRASS(), 0.006)]
    for s in (-1, 1):
        a, b = sorted((s * hw, s * (hw - 0.025)))
        r = box("SideRail", a, b, y0 + 0.06, y1 - 0.105, zc, zr, oaky, 0.004, 3); parts.append((r, 2)); hs.add(r)
        a, b = sorted((s * (hw - 0.025), s * (hw - 0.047)))
        parts.append((box("Ledger", a, b, y0 + 0.07, y1 - 0.115, 0.315, 0.335, bee, 0.002, 2), 2))
    fr = box("FootRail", -(hw - 0.06), hw - 0.06, y0 + 0.018, y0 + 0.043, zc, zr, oakx, 0.004, 3); parts.append((fr, 2)); hs.add(fr)
    hr = box("HeadRail", -(hw - 0.06), hw - 0.06, y1 - 0.09, y1 - 0.065, zc, zr, oakx, 0.004, 3); parts.append((hr, 2)); hs.add(hr)
    beam = box("Beam", -0.02, 0.02, y0 + 0.043, y1 - 0.09, 0.285, 0.335, bee, 0.003, 2); parts.append((beam, 3)); hs.add(beam)
    cl = cyl("MidLeg", 0.02, 0.279, (0, 0, 0.006 + 0.1395), bee, "Z", 16); parts.append((cl, 3)); hs.add(cl)
    parts.append((cyl("MidGlide", 0.024, 0.006, (0, 0, 0.003), BLACKP(), "Z", 16), 3))
    for k in range(13):          # sprung slats (what the ant sees from under the bed), in their plastic caps
        y = y0 + 0.13 + k * (y1 - 0.20 - (y0 + 0.13)) / 12
        parts.append((box("Slat", -(hw - 0.03), hw - 0.03, y - 0.032, y + 0.032, 0.335, 0.346, bee, 0.002, 2), 3))
        for s in (-1, 1):
            a, b = sorted((s * (hw - 0.026), s * (hw - 0.05)))
            parts.append((box("SlatCap", a, b, y - 0.036, y + 0.036, 0.333, 0.349, BLACKP(), 0.002, 1), 3))
    hs.box(-(hw - 0.03), hw - 0.03, y0 + 0.09, y1 - 0.16, 0.335, 0.347)
    # mattress: ticking, piped top and bottom, side handles, the label
    mt = soft("Mattress", -(hw - 0.03), hw - 0.03, y0 + 0.03, y1 - 0.115, 0.349, 0.538, TICK(), 0.05, 4, 0.12, 0); parts.append((mt, 4)); hs.add(mt)
    for z in (0.362, 0.526):
        parts.append((pipe("MatPiping", [rect_loop(-(hw - 0.036), hw - 0.036, y0 + 0.036, y1 - 0.121, z, 0.05, 6)], 0.005, TICK(), True), 4))
    for s in (-1, 1):
        for yh in (-0.45, 0.35):
            parts.append((pipe("MatHandle", [[(s * (hw - 0.027), yh - 0.07, 0.44), (s * (hw - 0.014), yh - 0.04, 0.435), (s * (hw - 0.014), yh + 0.04, 0.435), (s * (hw - 0.027), yh + 0.07, 0.44)]], 0.006, FELT()), 4))
    parts.append((box("MatLabel", 0.42, 0.52, y0 + 0.027, y0 + 0.031, 0.42, 0.47, PAPER(), 0.001, 1), 4))
    # headboard: oak board to the floor rail, a padded panel with covered buttons
    hb = box("Headboard", -hw, hw, y1 - 0.045, y1, 0.20, 1.10, oakx, 0.008, 4); parts.append((hb, 5)); hs.add(hb)
    pad = soft("Pad", -(hw - 0.05), hw - 0.05, y1 - 0.105, y1 - 0.04, 0.60, 1.05, F_HEAD(), 0.035, 4, 0.22, 1); parts.append((pad, 6))
    parts.append((pipe("PadPiping", [rot_xz(rect_loop(-(hw - 0.056), hw - 0.056, 0.606, 1.044, y1 - 0.107, 0.03, 6))], 0.0045, F_HEAD(), True), 6))
    for xb in (-0.52, -0.26, 0.0, 0.26, 0.52):
        for zb_ in (0.75, 0.92):
            parts.append((lathe("Button", [(0, 0.0), (0.014, 0.0), (0.016, 0.004), (0.012, 0.008), (0, 0.009)], F_HEAD(), (xb, y1 - 0.112, zb_), 20, 0, (PI / 2, 0, 0)), 6))
    return parts, hs

def build_bed_pillow():
    """sleeping pillow 0.70 x 0.50 m in a linen case, piped round its seam (origin under its middle)"""
    hs = Hulls(); parts = []; ln = LINEN()
    c = soft("Pillow", -0.31, 0.31, -0.21, 0.21, 0.04, 0.12, ln, 0.035, 4, 1.0, 1); parts.append((c, 0))
    # the piping follows the filled pillow: its sides bulge 4 cm at their middle (the puff of soft()), none at the corners
    n = 10; loop = []
    loop += [(0.312 + 0.04 * (1 - (y / 0.21) ** 2), y, 0.08) for y in [-0.19 + 0.38 * k / n for k in range(n + 1)]]
    loop += [(x, 0.212 + 0.04 * (1 - (x / 0.31) ** 2), 0.08) for x in [0.29 - 0.58 * k / n for k in range(n + 1)]]
    loop += [(-0.312 - 0.04 * (1 - (y / 0.21) ** 2), y, 0.08) for y in [0.19 - 0.38 * k / n for k in range(n + 1)]]
    loop += [(x, -0.212 - 0.04 * (1 - (x / 0.31) ** 2), 0.08) for x in [-0.29 + 0.58 * k / n for k in range(n + 1)]]
    parts.append((pipe("Piping", [loop], 0.005, ln, True), 0))
    hs.box(-0.34, 0.34, -0.24, 0.24, 0.0, 0.15)
    return parts, hs

def build_duvet():
    """the duvet, in the bed's frame: lies on the mattress, goes over the foot end and down to the floor (the way up for the ants)"""
    top, hwf, nx = 0.566, 0.795, 30
    path = [(0.42, top + 0.012), (0.36, top + 0.022), (0.30, top + 0.006), (-0.93, top)] + arc(-0.955, top - 0.07, 0.07, 90, 180, 6) \
           + [(-1.03, 0.07)] + arc(-1.08, 0.07, 0.05, 0, -90, 4) + [(-1.22, 0.02)]
    pts = []
    for a, b in zip(path, path[1:]):
        a = Vector(a); b = Vector(b); n = max(1, int(round((b - a).length / 0.035)))
        for k in range(n): pts.append(a + (b - a) * (k / n))
    pts.append(Vector(path[-1]))
    bm = bmesh.new(); rows = []
    for i, p in enumerate(pts):
        hang = 1.0 if p.x > -0.90 else max(0.0, 1.0 - (-0.90 - p.x) / 0.10)       # (the sides hang down only along the mattress)
        down = p.x < -1.0
        row = []
        for j in range(nx + 1):
            xf = (j / nx - 0.5) * 2.04
            d = max(0.0, abs(xf) - hwf)
            w = noise.noise(Vector((xf * 3.0, i * 0.12, 1.7))) * 0.012 + noise.noise(Vector((xf * 9.0, i * 0.4, 5.1))) * 0.004
            if down:
                x = math.copysign(min(abs(xf), hwf) + 0.3 * d, xf); y = p.x + 0.022 * math.sin(xf * 11.0) * min(1.0, (top - p.y) / 0.2) + w; z = p.y
            else:
                x = math.copysign(min(abs(xf), hwf) + 0.014 * (1 - math.exp(-d * 40)) * hang + 0.3 * d * (1 - hang), xf)
                y = p.x; z = p.y + w - max(0.0, d - 0.015) * hang
            row.append(bm.verts.new((x, y, max(0.006, z))))
        rows.append(row)
    for a, b in zip(rows, rows[1:]):
        for j in range(nx): bm.faces.new((a[j], a[j + 1], b[j + 1], b[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new("Duvet"); bm.to_mesh(me); bm.free()
    o = H.link(bpy.data.objects.new("Duvet", me))
    sd = o.modifiers.new("solid", "SOLIDIFY"); sd.thickness = 0.03; sd.offset = 0.0
    for p_ in o.data.polygons: p_.use_smooth = True
    o.data.materials.append(F_DUVET())
    hs = Hulls(); hs.box(-0.82, 0.82, -0.95, 0.42, 0.545, 0.60); hs.box(-1.0, 1.0, -1.07, -1.01, 0.0, 0.56); hs.box(-1.0, 1.0, -1.22, -1.05, 0.0, 0.04)
    return [(o, 0)], hs

def drawer(w, h, d, front_m, knobs=(0.0,), stuff=None):
    """a drawer: origin = the middle of the bottom edge of its front face; the box goes back to +Y, it slides out to -Y"""
    hs = Hulls(); parts = []; t = 0.012; bw = w / 2 - 0.014
    fr = box("Front", -w / 2, w / 2, 0.0, 0.018, 0.0, h, front_m, 0.003, 3); parts.append((fr, 0)); hs.add(fr)
    for s in (-1, 1):
        a, b = sorted((s * bw, s * (bw - t)))
        sd = box("Side", a, b, 0.018, d, 0.012, h - 0.02, W_BEECH("Y"), 0.0015, 2); parts.append((sd, 1)); hs.add(sd)
        a, b = sorted((s * (bw + 0.0006), s * (bw - 0.001)))
        parts.append((box("Groove", a, b, 0.03, d - 0.01, h / 2 - 0.005, h / 2 + 0.005, RUBK(), 0, 0), 1))
    bk = box("BackBoard", -bw + t, bw - t, d - t, d, 0.012, h - 0.03, W_BEECH("X"), 0.0015, 2); parts.append((bk, 1)); hs.add(bk)
    bt = box("Bottom", -bw + 0.004, bw - 0.004, 0.012, d - 0.004, 0.016, 0.022, HDF(), 0, 0); parts.append((bt, 1)); hs.add(bt)
    parts.append((box("Liner", -bw + t + 0.003, bw - t - 0.003, 0.021, d - t - 0.003, 0.022, 0.0228, M("LinerPaper", lambda n: H.plastic(n, (0.76, 0.82, 0.74), 0.8, 0.0, 0.05)), 0, 0), 1))
    for kx in knobs: parts += [(o, 0) for o in knob(kx, 0.0, h / 2)]
    for o in (stuff(bw - t, d, h) if stuff else []): parts.append((o, 2))
    return parts, hs

def _socks(bw, d, h):
    out = []; cols = (((0.72, 0.50, 0.10), "SockMustard"), ((0.10, 0.34, 0.38), "SockTeal"), ((0.86, 0.84, 0.78), "SockWhite"), ((0.55, 0.07, 0.05), "SockRed"))
    n = max(2, int((2 * bw - 0.04) / 0.10))
    for i in range(n):
        x = -bw + 0.03 + (i + 0.5) * (2 * bw - 0.06) / n; c, key = cols[i % 4]
        out.append(box("Roll", x - 0.04, x + 0.04, 0.05, 0.05 + 0.11, 0.035, 0.085, TERRY(key, c), 0.022, 3))
        out.append(box("Fold", x - 0.045, x + 0.045, 0.19, d - 0.03, 0.03, 0.03 + 0.03 * (1 + i % 2), TERRY(key + "B", cols[(i + 2) % 4][0]), 0.01, 3))
    return out

def _nightstuff(bw, d, h):
    red = M("BookRed", paint, (0.50, 0.08, 0.06), 0.5, 0.02, 0.3, (0.75, 0.72, 0.62))
    return [box("Book", -0.13, 0.03, 0.06, 0.28, 0.024, 0.052, red, 0.003, 2), box("Pages", -0.127, 0.033, 0.064, 0.276, 0.027, 0.049, PAPER(), 0.0, 0),
            cyl("Tin", 0.035, 0.022, (0.11, 0.12, 0.035), BRASS(), "Z", 28, 0.003), box("Charger", 0.07, 0.12, 0.20, 0.25, 0.024, 0.05, WHITEP(), 0.006, 3)]

def build_nightstand():
    """bedside table 0.45 x 0.40 x 0.55 m (back +Y): walnut carcass on turned legs, oak top, a drawer (own mesh) over an open niche"""
    W, D, Hh = 0.45, 0.40, 0.55; hw, hd, t, zl = W / 2, D / 2, 0.018, 0.12
    hs = Hulls(); parts = []; wal = W_WALNUT("X"); walz = W_WALNUT("Z"); oak = W_OAK("X")
    for o in legs4((-(hw - 0.04), hw - 0.04), (-(hd - 0.04), hd - 0.04), zl + 0.004, 0.02, 0.013, hs=hs): parts.append((o, 1))
    for s in (-1, 1):
        a, b = sorted((s * hw, s * (hw - t)))
        sd = box("Side", a, b, -hd, hd, zl, Hh - 0.02, walz, 0.003, 2); parts.append((sd, 2 + (s > 0))); hs.add(sd)
    bot = box("Bottom", -hw + t - 0.001, hw - t + 0.001, -hd + 0.002, hd, zl, zl + t, wal, 0.002, 2); parts.append((bot, 4)); hs.add(bot)
    mid = box("Mid", -hw + t - 0.001, hw - t + 0.001, -hd + 0.002, hd - 0.006, 0.34, 0.34 + t, wal, 0.002, 2); parts.append((mid, 5)); hs.add(mid)
    top = box("Top", -hw - 0.008, hw + 0.008, -hd - 0.012, hd, Hh - 0.02, Hh, oak, 0.005, 4); parts.append((top, 6)); hs.add(top)
    back = box("Back", -hw + 0.004, hw - 0.004, hd - 0.006, hd, zl + 0.002, Hh - 0.02, HDF(), 0, 0); parts.append((back, 7)); hs.add(back)
    nl = M("Nail", H.metal, (0.45, 0.45, 0.47), 0.4)
    for k in range(6):
        for z in (zl + 0.01, 0.349, Hh - 0.03):
            parts.append((cyl("Nail", 0.0018, 0.0008, (-hw + 0.03 + k * (W - 0.06) / 5, hd + 0.0003, z), nl, "Y", 8), 7))
    for s in (-1, 1):                       # kickers above the drawer, stop blocks at the back
        a, b = sorted((s * (hw - t), s * (hw - t - 0.012)))
        parts.append((box("Kicker", a, b, -hd + 0.03, hd - 0.02, Hh - 0.032, Hh - 0.02, W_BEECH("Y"), 0.001, 1), 2 + (s > 0)))
        parts.append((box("Stop", a, b, hd - 0.05, hd - 0.03, 0.358, 0.38, W_BEECH("Y"), 0.001, 1), 5))
    stain = M("Stain", lambda n: H.plastic(n, (0.30, 0.17, 0.07), 0.25, 0.02, 0.1))
    parts.append((lathe("Ring", closed([(0.030, Hh + 0.0002), (0.034, Hh + 0.0002), (0.034, Hh + 0.0005), (0.030, Hh + 0.0005)]), stain, (0.09, 0.05, 0), 36), 6))
    # in the niche: two magazines and a tissue box
    parts.append((box("Mags", -0.16, 0.05, -0.12, 0.14, zl + t, zl + t + 0.022, M("MagBlue", paint, (0.10, 0.25, 0.55), 0.3, 0.01, 0.2, (0.9, 0.9, 0.85)), 0.002, 1, Rz(8)), 4))
    parts.append((box("Tissues", 0.07, 0.19, -0.06, 0.17, zl + t, zl + t + 0.075, M("TissueBox", paint, (0.72, 0.50, 0.10), 0.5, 0.01, 0.2, (0.6, 0.45, 0.3)), 0.004, 2), 4))
    parts.append((box("Tissue", 0.10, 0.16, 0.02, 0.09, zl + t + 0.072, zl + t + 0.11, PAPER(), 0.012, 3), 4))
    return parts, hs

def build_nightstand_drawer(): return drawer(0.410, 0.168, 0.34, W_WALNUT("X"), (0.0,), _nightstuff)

def build_dresser():
    """chest of drawers 0.90 x 0.45 x 0.85 m (back +Y): walnut carcass on turned legs, oak top, four drawers (own meshes) on rails"""
    W, D, Hh = 0.90, 0.45, 0.85; hw, hd, t, zl = W / 2, D / 2, 0.018, 0.10
    hs = Hulls(); parts = []; wal = W_WALNUT("X"); walz = W_WALNUT("Z"); oak = W_OAK("X")
    for o in legs4((-(hw - 0.05), hw - 0.05), (-(hd - 0.05), hd - 0.05), zl + 0.004, 0.024, 0.015, hs=hs): parts.append((o, 1))
    for s in (-1, 1):
        a, b = sorted((s * hw, s * (hw - t)))
        sd = box("Side", a, b, -hd, hd, zl, Hh - 0.022, walz, 0.003, 2); parts.append((sd, 2 + (s > 0))); hs.add(sd)
        a, b = sorted((s * (hw - t), s * (hw - t - 0.012)))
        for k in range(1, 4):                # runners the drawers ride on
            z = zl + t + k * 0.182
            parts.append((box("Runner", a, b, -hd + 0.03, hd - 0.02, z - 0.012, z, W_BEECH("Y"), 0.001, 1), 2 + (s > 0)))
    bot = box("Bottom", -hw + t - 0.001, hw - t + 0.001, -hd + 0.002, hd, zl, zl + t, wal, 0.002, 2); parts.append((bot, 4)); hs.add(bot)
    for k in range(1, 4):                    # rails between the drawers (front only), dust boards behind them
        z = zl + t + k * 0.182 - t
        rl = box("Rail", -hw + t - 0.001, hw - t + 0.001, -hd + 0.002, -hd + 0.06, z, z + t, wal, 0.002, 2); parts.append((rl, 5)); hs.add(rl)
    top = box("Top", -hw - 0.01, hw + 0.01, -hd - 0.014, hd, Hh - 0.022, Hh, oak, 0.006, 4); parts.append((top, 6)); hs.add(top)
    back = box("Back", -hw + 0.004, hw - 0.004, hd - 0.006, hd, zl + 0.002, Hh - 0.022, HDF(), 0, 0); parts.append((back, 7)); hs.add(back)
    nl = M("Nail", H.metal, (0.45, 0.45, 0.47), 0.4)
    for k in range(10):
        for z in (zl + 0.01, 0.47, Hh - 0.032):
            parts.append((cyl("Nail", 0.0018, 0.0008, (-hw + 0.03 + k * (W - 0.06) / 9, hd + 0.0003, z), nl, "Y", 8), 7))
    # on top: a crocheted runner and a ring from a vase
    parts.append((box("Runner", -0.30, 0.30, -0.12, 0.12, Hh + 0.0003, Hh + 0.002, M("Doily", HL.lace), 0.0008, 1), 6))
    return parts, hs

def build_dresser_drawer(): return drawer(0.860, 0.162, 0.39, W_WALNUT("X"), (-0.24, 0.24), _socks)
DRESSER_Z = [0.10 + 0.018 + k * 0.182 + 0.001 for k in range(4)]

def _hanger(x, yc, z, m):
    """a wooden hanger in the YZ plane, hooked over the rail at (x, yc, z)"""
    return [pipe("Hanger", [[(x, yc - 0.21, z - 0.125), (x, yc, z - 0.05), (x, yc + 0.21, z - 0.125)]], 0.007, m),
            pipe("HangerBar", [[(x, yc - 0.20, z - 0.125), (x, yc + 0.20, z - 0.125)]], 0.005, m),
            pipe("HangerHook", [[(x, yc, z - 0.05), (x, yc, z - 0.014), (x, yc + 0.016, z + 0.004), (x, yc, z + 0.016), (x, yc - 0.016, z + 0.004)]], 0.0022, CHROME())]

def _garment(x, yc, ztop, length, m, half=0.21, thick=0.035):
    """a shirt / a dress on a hanger, seen edge-on from the front of the wardrobe (shoulders along Y)"""
    zb = ztop - length
    rings = [oval(x, yc, thick * 0.9, half * 0.95, zb, 24), oval(x, yc, thick, half * 0.9, zb + length * 0.45, 24), oval(x, yc, thick * 1.15, half, ztop - 0.14, 24),
             oval(x, yc, thick, half * 0.97, ztop - 0.085, 24), oval(x, yc, thick * 0.7, 0.07, ztop - 0.05, 24), oval(x, yc, thick * 0.5, 0.035, ztop - 0.035, 24)]
    return loft("Garment", rings, m)

def build_wardrobe():
    """wardrobe 1.20 x 0.60 x 2.20 m (back +Y): walnut carcass on a recessed plinth, two green doors (own meshes). Inside: a
    hanging rail with hangers and clothes under a hat shelf, a column of shelves with folded linen, shoe boxes on the floor"""
    W, D, Hh = 1.20, 0.60, 2.20; hw, hd, t = W / 2, D / 2, 0.02
    yf, zb, xd = -hd + 0.022, 0.10, 0.20
    hs = Hulls(); parts = []; wal = W_WALNUT("X"); walz = W_WALNUT("Z"); oak = W_OAK("X")
    for s in (-1, 1):
        a, b = sorted((s * hw, s * (hw - t)))
        sd = box("Side", a, b, yf, hd, 0.0, Hh - 0.022, walz, 0.003, 2); parts.append((sd, 1 + (s > 0))); hs.add(sd)
        for k in range(30):                 # the rows of shelf-pin holes
            for y in (yf + 0.06, hd - 0.08):
                parts.append((cyl("PinHole", 0.0028, 0.002, (s * (hw - t - 0.0005), y, 0.20 + k * 0.058), M("Hole", H.rubber, (0.02, 0.015, 0.01), 0.9), "X", 8), 1 + (s > 0)))
    pl = box("Plinth", -hw + t, hw - t, yf + 0.03, yf + 0.048, 0.0, zb - t, BLACKP(), 0.002, 1); parts.append((pl, 3)); hs.add(pl)
    bot = box("Bottom", -hw + t - 0.001, hw - t + 0.001, yf, hd, zb - t, zb, wal, 0.002, 2); parts.append((bot, 3)); hs.add(bot)
    top = box("Top", -hw + t - 0.001, hw - t + 0.001, yf, hd, Hh - 0.042, Hh - 0.022, wal, 0.002, 2); parts.append((top, 4)); hs.add(top)
    crown = box("Crown", -hw - 0.012, hw + 0.012, -hd - 0.012, hd, Hh - 0.022, Hh, oak, 0.007, 4); parts.append((crown, 4)); hs.add(crown)
    back = box("Back", -hw + 0.004, hw - 0.004, hd - 0.006, hd, zb - t + 0.002, Hh - 0.024, HDF(), 0, 0); parts.append((back, 5)); hs.add(back)
    dv = box("Divider", xd - 0.009, xd + 0.009, yf + 0.004, hd - 0.006, zb, Hh - 0.042, walz, 0.002, 2); parts.append((dv, 6)); hs.add(dv)
    # left bay: hat shelf, the rail on its flanges, hangers, clothes
    x0, x1 = -hw + t, xd - 0.009; xc = (x0 + x1) / 2
    hsf = box("HatShelf", x0 - 0.001, x1 + 0.001, yf + 0.02, hd - 0.006, 1.90, 1.918, oak, 0.002, 2); parts.append((hsf, 7)); hs.add(hsf)
    zr = 1.82
    parts.append((cyl("Rail", 0.0125, x1 - x0, (xc, 0.03, zr), CHROME(), "X", 20), 7))
    for x in (x0 + 0.002, x1 - 0.002): parts.append((cyl("RailFlange", 0.03, 0.004, (x, 0.03, zr), CHROME(), "X", 24), 7))
    bee = W_BEECH("Y")
    clothes = (M("ShirtWhite", fabric, (0.86, 0.86, 0.84), None, 600.0, 0.2), M("DressMustard", fabric, (0.72, 0.50, 0.10), (0.52, 0.33, 0.06), 520.0, 0.4),
               M("CoatTeal", fabric, (0.10, 0.34, 0.38), (0.06, 0.23, 0.26), 300.0, 0.6), M("ShirtRed", fabric, (0.55, 0.07, 0.05), (0.38, 0.04, 0.03), 600.0, 0.3))
    for k, x in enumerate((-0.50, -0.39, -0.27, -0.15, -0.03, 0.10)):
        for o in _hanger(x, 0.03, zr + 0.0125, bee): parts.append((o, 8))
        if k not in (2, 5): parts.append((_garment(x, 0.03, zr - 0.035, (0.78, 1.25, 0.0, 1.05, 0.72, 0.0)[k], clothes[k % 4], 0.215, (0.03, 0.035, 0, 0.05, 0.03, 0)[k]), 9))
    hat = lathe("Hat", [(0, 0.11), (0.05, 0.108), (0.085, 0.09), (0.09, 0.02), (0.095, 0.012), (0.16, 0.004), (0.165, 0.0), (0.16, -0.002), (0.085, 0.008), (0.08, 0.085), (0, 0.10)], M("HatFelt", fabric, (0.20, 0.17, 0.14), None, 900.0, 0.4), (-0.30, 0.02, 1.922), 36)
    parts.append((hat, 9))
    for k, (bx, bw_, bh, col) in enumerate(((-0.42, 0.30, 0.11, (0.58, 0.44, 0.28)), (-0.40, 0.28, 0.10, (0.20, 0.22, 0.26)), (-0.06, 0.30, 0.12, (0.58, 0.44, 0.28)))):
        z = zb + (0.11 if k == 1 else 0.0); cm = M("ShoeBox%d" % k, lambda n, c=col: H.plastic(n, c, 0.8, 0.05, 0.1))
        parts.append((box("ShoeBox", bx - bw_ / 2, bx + bw_ / 2, -0.12, 0.08, z, z + bh - 0.01, cm, 0.002, 1, Rz((4, -6, 3)[k])), 10))
        parts.append((box("ShoeLid", bx - bw_ / 2 - 0.003, bx + bw_ / 2 + 0.003, -0.123, 0.083, z + bh - 0.03, z + bh, cm, 0.002, 1, Rz((4, -6, 3)[k])), 10))
    # right bay: shelves on pins, folded linen
    for k, z in enumerate((0.45, 0.80, 1.15, 1.50, 1.85)):
        sh = box("Shelf", xd + 0.009 - 0.001, hw - t + 0.001, yf + 0.02, hd - 0.006, z, z + 0.018, oak, 0.002, 2); parts.append((sh, 11 + k)); hs.add(sh)
        for x in (xd + 0.03, hw - t - 0.02):
            for y in (yf + 0.06, hd - 0.08): parts.append((cyl("Pin", 0.003, 0.010, (x, y, z - 0.004), BRASS(), "X", 8), 11 + k))
    lin = (LINEN(), TERRY("TowelTeal", (0.10, 0.34, 0.38)), TERRY("TowelMustard", (0.72, 0.50, 0.10)), TICK())
    for k, (z, n) in enumerate(((zb, 4), (0.468, 5), (0.818, 3), (1.168, 4), (1.518, 2))):
        for i in range(n):
            parts.append((box("Linen", xd + 0.03 + 0.004 * (i % 2), hw - t - 0.03 - 0.005 * ((i + 1) % 2), -0.20 + 0.006 * (i % 3), 0.18, z + i * 0.045, z + i * 0.045 + 0.043, lin[(k + i) % 4], 0.016, 3), 16))
    return parts, hs

def build_wardrobe_door(side):
    """wardrobe door 0.597 x 2.074 x 0.02 m, green frame and two fielded panels, brass knob, four cup hinges. Origin on the hinge
    axis at its bottom, the back face at y=0. side=-1: hinged on its left edge (the leaf goes to +X); +1: on its right (to -X)"""
    w, h, s = 0.597, 2.074, -side
    X = lambda a, b: sorted((s * a, s * b))
    hs = Hulls(); parts = []; g = P_GREEN()
    for (xa, xb, z0, z1) in ((0, 0.075, 0, h), (w - 0.075, w, 0, h), (0.074, w - 0.074, 0, 0.085), (0.074, w - 0.074, h - 0.085, h), (0.074, w - 0.074, 1.00, 1.075)):
        a, b = X(xa, xb); parts.append((box("Frame", a, b, -0.02, 0.0, z0, z1, g, 0.003, 3), 0))
    for (z0, z1) in ((0.085, 1.00), (1.075, h - 0.085)):
        a, b = X(0.075, w - 0.075); parts.append((box("Panel", a, b, -0.015, -0.005, z0, z1, g, 0.002, 2), 1))
        a, b = X(0.14, w - 0.14); parts.append((box("Field", a, b, -0.019, -0.014, z0 + 0.065, z1 - 0.065, g, 0.006, 3), 1))
        a, b = X(0.077, w - 0.077); parts.append((pipe("Bead", [rot_xz(rect_loop(a, b, z0 + 0.002, z1 - 0.002, -0.0155))], 0.0028, g, True), 0))
    parts += [(o, 0) for o in knob(s * (w - 0.037), -0.02, 1.0375)]
    cup_hinges([s * 0.036], [0.12, 0.70, 1.40, h - 0.12], 0.0, parts, 0)
    if side > 0:            # a dressing mirror inside the right door
        a, b = X(0.13, w - 0.13)
        parts.append((box("MirrorBack", a - 0.006, b + 0.006, 0.0, 0.004, 0.394, 1.756, BLACKP(), 0.001, 1), 2))
        parts.append((box("Mirror", a, b, 0.004, 0.0065, 0.40, 1.75, MIRROR(), 0.0, 0), 2))
        for z in (0.40, 1.75):
            for x in (a + 0.04, b - 0.04): parts.append((box("MirrorClip", x - 0.012, x + 0.012, 0.004, 0.009, z - 0.008, z + 0.008, CHROME(), 0.002, 2), 2))
    a, b = X(0, w); hs.box(a, b, -0.02, 0.0, 0.0, h)
    return parts, hs

# ================================================================================================ BATHROOM + WC
def RIBP():    return M("ScreenPlastic", lambda n: H.plastic(n, (0.88, 0.89, 0.88), 0.30, 0.02, 0.25))
def ALU():     return M("Alu", H.metal, (0.74, 0.75, 0.77), 0.38)

def _screen_panel(x0, x1, y0, y1, z0, z1, parts, part, grip_x=None):
    parts.append((box("Screen", x0, x1, y0, y1, z0, z1, RIBP(), 0.001, 1), part))
    n = int((x1 - x0) / 0.06)
    for k in range(1, n):       # the pressed ribs of the plastic sheet
        x = x0 + k * (x1 - x0) / n
        parts.append((box("Rib", x - 0.004, x + 0.004, y0 - 0.0012, y0 + 0.0005, z0 + 0.02, z1 - 0.02, RIBP(), 0.001, 1), part))
    if grip_x is not None:
        parts.append((box("Grip", grip_x - 0.012, grip_x + 0.012, y0 - 0.006, y0 + 0.0005, (z0 + z1) / 2 - 0.045, (z0 + z1) / 2 + 0.045, WHITEP(), 0.004, 3), part))
        parts.append((box("GripWell", grip_x - 0.006, grip_x + 0.006, y0 - 0.0066, y0 - 0.0055, (z0 + z1) / 2 - 0.035, (z0 + z1) / 2 + 0.035, RUBK(), 0, 0), part))

def build_tub():
    """bathtub 1.74 x 0.70 x 0.55 m built in between the walls (front -Y): enamelled shell, a screen of two sliding panels on
    aluminium rails (the right one is its own mesh), end panels; under it the feet, the trap and the overflow pipe (the ants'
    hideout). On the back side the wall mixer with its spout, the shower hose and the handset in its bracket (up to z 1.13)."""
    hx, hy, Hh = 0.87, 0.35, 0.55
    hs = Hulls(); parts = []; en = ENAMEL(); ch = CHROME()
    def R(i, z, r): return rr(hx - i, hy - i, z, r)
    shell = loft("Shell", [R(0.20, 0.10, 0.12), R(0.10, 0.16, 0.14), R(0.045, 0.46, 0.11), R(0.03, 0.515, 0.08), R(0.0, 0.522, 0.05), R(0.0, 0.545, 0.05),
                           R(0.006, 0.55, 0.05), R(0.05, 0.55, 0.07), R(0.062, 0.54, 0.085), R(0.085, 0.40, 0.10), R(0.13, 0.20, 0.13), R(0.19, 0.135, 0.14), R(0.22, 0.122, 0.10)], en)
    parts.append((shell, 0))
    hs.box(-hx, hx, -hy, -hy + 0.07, 0.13, Hh); hs.box(-hx, hx, hy - 0.07, hy, 0.13, Hh)
    hs.box(-hx, -hx + 0.09, -hy + 0.07, hy - 0.07, 0.13, Hh); hs.box(hx - 0.09, hx, -hy + 0.07, hy - 0.07, 0.13, Hh)
    hs.box(-hx + 0.09, hx - 0.09, -hy + 0.07, hy - 0.07, 0.10, 0.135)
    # the screen: rails, the fixed left panel, legs; the end panels
    for z in (0.0, 0.498):
        rl = box("ScreenRail", -hx + 0.004, hx - 0.004, -hy, -hy + 0.024, z, z + 0.022, ALU(), 0.002, 2); parts.append((rl, 1)); hs.add(rl)
        for y in (-hy + 0.008, -hy + 0.017): parts.append((box("RailGroove", -hx + 0.006, hx - 0.006, y - 0.002, y + 0.002, z + (0.0215 if z < 0.1 else -0.0005), z + (0.0225 if z < 0.1 else 0.0005), RUBK(), 0, 0), 1))
    _screen_panel(-hx + 0.006, 0.03, -hy + 0.015, -hy + 0.019, 0.021, 0.499, parts, 1, -hx + 0.07)
    hs.box(-hx + 0.006, 0.03, -hy + 0.015, -hy + 0.019, 0.021, 0.499)
    for s in (-1, 1):
        a, b = sorted((s * hx, s * (hx - 0.006)))
        ep = box("EndPanel", a, b, -hy + 0.024, hy, 0.0, 0.52, RIBP(), 0.001, 1); parts.append((ep, 1)); hs.add(ep)
    # under the tub: the cradles on threaded feet, the waste and trap, the overflow pipe
    for x in (-0.55, 0.50):
        parts.append((box("Cradle", x - 0.02, x + 0.02, -0.21, 0.21, 0.092, 0.108, STEEL(), 0.002, 1), 3))
        for y in (-0.18, 0.18):
            parts.append((cyl("Foot", 0.008, 0.086, (x, y, 0.049), STEEL(), "Z", 12), 3)); parts.append((cyl("FootPad", 0.028, 0.008, (x, y, 0.004), BLACKP(), "Z", 20), 3))
            parts.append((cyl("FootNut", 0.014, 0.008, (x, y, 0.086), STEEL(), "Z", 6), 3))
    wp = WHITEP()
    parts.append((cyl("WasteTail", 0.02, 0.07, (0.60, 0.0, 0.07), wp, "Z", 20), 3))
    parts.append((pipe("Trap", [[(0.60, 0, 0.045), (0.60, 0, 0.03), (0.66, 0, 0.026), (0.71, 0, 0.05), (0.76, 0, 0.05), (hx - 0.008, 0, 0.05)]], 0.02, wp), 3))
    parts.append((pipe("OverflowPipe", [[(0.835, 0, 0.42), (0.842, 0, 0.20), (0.76, 0, 0.075)]], 0.012, RUB()), 3))
    for z in (0.105, 0.035): parts.append((cyl("TrapNut", 0.026, 0.012, (0.60, 0, z), wp, "Z", 12), 3))
    # fittings inside: waste grate, overflow rose, plug on its chain
    parts.append((lathe("Waste", [(0, 0.0), (0.026, 0.0), (0.030, 0.002), (0.030, 0.0035), (0, 0.0045)], ch, (0.60, 0, 0.1215), 28), 2))
    for a in (0, 90): parts.append((box("WasteSlot", 0.60 - 0.016, 0.60 + 0.016, -0.0025, 0.0025, 0.126, 0.1265, RUBK(), 0, 0, Rz(a)), 2))
    parts.append((lathe("Overflow", [(0, 0.0), (0.030, 0.0), (0.034, 0.003), (0.030, 0.007), (0, 0.008)], ch, (0.792, 0, 0.42), 28, 0, (0, -PI / 2, 0)), 2))
    parts.append((pipe("Chain", [[(0.786, 0.0, 0.40), (0.74, 0.02, 0.22), (0.62, 0.07, 0.132), (0.47, 0.09, 0.127)]], 0.0016, STEEL()), 2))
    parts.append((lathe("Plug", [(0, 0.0), (0.022, 0.0), (0.025, 0.008), (0.012, 0.012), (0.006, 0.020), (0, 0.021)], RUBK(), (0.45, 0.09, 0.123), 24), 2))
    # the wall mixer over the tap end, spout, shower hose and handset
    yw, xm, zm = hy, 0.56, 0.70
    parts.append((cyl("MixerBody", 0.022, 0.17, (xm, yw - 0.07, zm), ch, "X", 24, 0.004), 4))
    for s in (-1, 1):
        parts.append((cyl("Union", 0.014, 0.07, (xm + s * 0.075, yw - 0.035, zm), ch, "Y", 16), 4)); parts.append((cyl("Rosette", 0.031, 0.008, (xm + s * 0.075, yw - 0.004, zm), ch, "Y", 28, 0.003), 4))
        parts.append((cyl("Handle", 0.021, 0.04, (xm + s * 0.105, yw - 0.07, zm), ch, "X", 20, 0.005), 4))
        parts.append((box("HandleLever", xm + s * 0.105 - 0.006, xm + s * 0.105 + 0.006, yw - 0.076, yw - 0.064, zm + 0.015, zm + 0.06, ch, 0.004, 3), 4))
        parts.append((cyl("HotCold", 0.006, 0.002, (xm + s * 0.126, yw - 0.07, zm), M("Dot%d" % (s > 0), lambda n, c=((0.1, 0.25, 0.75) if s < 0 else (0.75, 0.1, 0.08)): H.plastic(n, c, 0.4, 0.0, 0.0)), "X", 12), 4))
    parts.append((pipe("Spout", [[(xm, yw - 0.07, zm), (xm, yw - 0.11, zm + 0.03), (xm, yw - 0.23, zm + 0.03), (xm, yw - 0.26, zm - 0.005)]], 0.012, ch), 4))
    parts.append((pipe("ShowerHose", [[(xm, yw - 0.07, zm - 0.02), (xm - 0.01, yw - 0.06, 0.61), (xm - 0.10, yw - 0.05, 0.575), (xm - 0.20, yw - 0.045, 0.66), (xm - 0.26, yw - 0.055, 0.94)]], 0.007, STEEL()), 4))
    parts.append((pipe("Handset", [[(xm - 0.26, yw - 0.055, 0.93), (xm - 0.26, yw - 0.075, 1.08)]], 0.011, ch), 4))
    parts.append((lathe("ShowerHead", [(0, 0.0), (0.038, 0.0), (0.042, 0.006), (0.034, 0.02), (0.014, 0.032), (0, 0.034)], ch, (xm - 0.26, yw - 0.075, 1.085), 28, 0, (PI * 0.62, 0, 0)), 4))
    parts.append((box("Bracket", xm - 0.275, xm - 0.245, yw - 0.07, yw, 0.97, 1.0, ch, 0.004, 3), 4))
    return parts, hs

def build_tub_panel():
    """the sliding half of the bath screen, 0.89 x 0.478 m: origin = the middle of its bottom edge, back face y=0; slides along -X"""
    hs = Hulls(); parts = []
    _screen_panel(-0.445, 0.445, -0.004, 0.0, 0.0, 0.478, parts, 0, -0.38)
    hs.box(-0.445, 0.445, -0.004, 0.0, 0.0, 0.478)
    return parts, hs

def build_sink():
    """washbasin on a cabinet 0.52 x 0.34 x 0.85 m, tap to 0.975 (back +Y): ceramic countertop basin, chrome mixer and waste; white
    cabinet on chrome legs, one door (own mesh). Inside: the bottle trap, two braided hoses to the angle valves, a shelf, a
    bottle of cleaner, soap, a rag"""
    hw, hd, t, zl, zt = 0.25, 0.165, 0.016, 0.06, 0.72; yf = -hd + 0.018
    hs = Hulls(); parts = []; lq = LACQ(); ch = CHROME(); ce = CERAMIC(); wp = WHITEP()
    for sx in (-1, 1):
        for y in (yf + 0.03, hd - 0.03):
            lg = cyl("Leg", 0.014, zl, (sx * (hw - 0.03), y, zl / 2), ch, "Z", 16, 0.002); parts.append((lg, 1)); hs.add(lg)
    for s in (-1, 1):
        a, b = sorted((s * hw, s * (hw - t)))
        sd = box("Side", a, b, yf, hd, zl, zt, lq, 0.002, 2); parts.append((sd, 2 + (s > 0))); hs.add(sd)
    bot = box("Bottom", -hw + t - 0.001, hw - t + 0.001, yf, hd, zl, zl + t, lq, 0.002, 2); parts.append((bot, 4)); hs.add(bot)
    for (z0, z1) in ((zl + t, zl + 0.11), (zt - 0.12, zt)):
        br = box("BackRail", -hw + t - 0.001, hw - t + 0.001, hd - t, hd, z0, z1, lq, 0.002, 2); parts.append((br, 4)); hs.add(br)
    sh = box("Shelf", -hw + t - 0.001, hw - t + 0.001, 0.0, hd - t, 0.36, 0.376, lq, 0.002, 2); parts.append((sh, 5)); hs.add(sh)
    def B(ix, f, b, z, r): return rect_loop(-hw - 0.01 + ix, hw + 0.01 - ix, -hd - 0.012 + f, hd - b, z, r, 6)
    basin = loft("Basin", [B(0.03, 0.03, 0.02, zt, 0.02), B(0.0, 0.0, 0.0, zt + 0.03, 0.03), B(-0.004, -0.004, 0.0, 0.84, 0.035), B(0.0, 0.0, 0.0, 0.85, 0.035),
                           B(0.03, 0.03, 0.085, 0.85, 0.05), B(0.045, 0.045, 0.10, 0.832, 0.06), B(0.09, 0.09, 0.13, 0.76, 0.055), B(0.16, 0.13, 0.17, 0.742, 0.018)], ce)
    parts.append((basin, 6)); hs.box(-hw - 0.01, hw + 0.01, -hd - 0.012, hd, zt, 0.85)
    yb = -0.026
    parts.append((lathe("Waste", [(0, 0.0), (0.021, 0.0), (0.024, 0.002), (0.024, 0.0035), (0.010, 0.006), (0, 0.006)], ch, (0, yb, 0.7415), 28), 7))
    parts.append((box("OverflowSlot", -0.012, 0.012, hd - 0.133, hd - 0.129, 0.80, 0.806, RUBK(), 0.001, 1), 6))
    yt = hd - 0.045
    parts.append((lathe("TapBase", [(0, 0.0), (0.026, 0.0), (0.026, 0.004), (0.021, 0.009), (0, 0.009)], ch, (0, yt, 0.85), 28), 7))
    parts.append((cyl("TapBody", 0.019, 0.09, (0, yt, 0.90), ch, "Z", 24, 0.004), 7))
    parts.append((pipe("TapSpout", [[(0, yt, 0.92), (0, yt - 0.04, 0.955), (0, yt - 0.10, 0.955), (0, yt - 0.125, 0.935)]], 0.011, ch), 7))
    parts.append((box("TapLever", -0.007, 0.007, yt - 0.07, yt + 0.005, 0.962, 0.974, ch, 0.004, 3), 7))
    # inside the cabinet
    parts.append((cyl("Tail", 0.016, 0.13, (0, yb, 0.655), wp, "Z", 16), 8)); parts.append((cyl("Bottle", 0.032, 0.09, (0, yb, 0.55), wp, "Z", 24, 0.006), 8))
    for z in (0.60, 0.70): parts.append((cyl("Nut", 0.024, 0.014, (0, yb, z), wp, "Z", 12), 8))
    parts.append((pipe("Outlet", [[(0, yb, 0.565), (0, hd - 0.004, 0.565)]], 0.016, wp), 8)); parts.append((cyl("WallRose", 0.03, 0.006, (0, hd - 0.003, 0.565), wp, "Y", 24), 8))
    for s in (-1, 1):
        parts.append((pipe("Flex", [[(s * 0.012, yt, 0.72), (s * 0.022, yt + 0.005, 0.60), (s * 0.07, hd - 0.03, 0.50), (s * 0.09, hd - 0.022, 0.462)]], 0.0045, STEEL()), 8))
        parts.append((cyl("AngleValve", 0.011, 0.036, (s * 0.09, hd - 0.018, 0.447), ch, "Y", 14), 8))
        parts.append((cyl("ValveCap", 0.013, 0.012, (s * 0.09, hd - 0.042, 0.447), M("Dot%d" % (s > 0), lambda n, c=((0.1, 0.25, 0.75) if s < 0 else (0.75, 0.1, 0.08)): H.plastic(n, c, 0.4, 0.0, 0.0)), "Y", 14), 8))
    teal = M("BottleTeal", lambda n: H.plastic(n, (0.05, 0.45, 0.48), 0.3, 0.02, 0.15))
    parts.append((lathe("Cleaner", [(0, 0.0), (0.035, 0.0), (0.037, 0.01), (0.037, 0.13), (0.02, 0.17), (0.014, 0.175), (0.014, 0.20), (0.018, 0.20), (0.018, 0.225), (0, 0.225)], teal, (-0.14, -0.03, zl + t), 24), 9))
    parts.append((box("Rag", 0.05, 0.19, 0.02, 0.12, 0.376, 0.40, TERRY("RagMustard", (0.72, 0.50, 0.10)), 0.01, 3), 9))
    parts.append((box("Soap", -0.17, -0.09, 0.03, 0.08, 0.376, 0.40, M("SoapBox", paint, (0.86, 0.85, 0.80), 0.5, 0.01, 0.1), 0.004, 2), 9))
    return parts, hs

def build_sink_door():
    """vanity door 0.496 x 0.652 x 0.016 m: white lacquer, chrome bar pull, two cup hinges. Origin on the hinge axis (its left
    edge) at the bottom, back face y=0"""
    w, h = 0.496, 0.652; hs = Hulls(); parts = []
    parts.append((box("Door", 0.0, w, -0.016, 0.0, 0.0, h, LACQ(), 0.003, 3), 0))
    parts.append((pipe("Pull", [[(w - 0.04, -0.016, h - 0.06), (w - 0.04, -0.04, h - 0.06), (w - 0.04, -0.04, h - 0.20), (w - 0.04, -0.016, h - 0.20)]], 0.005, CHROME()), 0))
    cup_hinges([0.036], [0.09, h - 0.09], 0.0, parts, 0)
    for z in (0.03, h - 0.03): parts.append((cyl("Bumper", 0.004, 0.002, (w - 0.02, 0.001, z), RUB(), "Y", 10), 0))
    hs.box(0.0, w, -0.016, 0.0, 0.0, h)
    return parts, hs

def SEATP():   return M("SeatPlastic", glossy, (0.92, 0.92, 0.90), 0.12, 0.3)

def build_toilet():
    """close-coupled toilet 0.40 (0.50 with the supply valve) x 0.63 x 0.76 m (back +Y, the wall at y=0.30): ceramic pan with a
    real bowl and water in the sump, the hinge posts; the cistern is hollow (its lid is its own mesh): fill valve with its
    float, flush valve, water; braided supply hose from the wall valve. Seat and lid are their own meshes."""
    ce = CERAMIC(); ch = CHROME(); wp = WHITEP(); hs = Hulls(); parts = []
    O = lambda cy, rx, ry, z: oval(0.0, cy, rx, ry, z, 32)
    bowl = loft("Bowl", [O(-0.06, 0.105, 0.19, 0.0), O(-0.06, 0.11, 0.195, 0.02), O(-0.05, 0.095, 0.16, 0.16), O(-0.09, 0.13, 0.19, 0.27), O(-0.115, 0.178, 0.205, 0.36),
                         O(-0.12, 0.185, 0.21, 0.395), O(-0.12, 0.183, 0.208, 0.405), O(-0.12, 0.14, 0.17, 0.405), O(-0.12, 0.13, 0.16, 0.385), O(-0.115, 0.115, 0.14, 0.30),
                         O(-0.09, 0.065, 0.085, 0.20), O(-0.07, 0.04, 0.05, 0.17)], ce)
    parts.append((bowl, 0)); hs.box(-0.11, 0.11, -0.25, 0.13, 0.0, 0.27); ring_hulls(hs, 0.0, -0.12, 0.185, 0.21, 0.13, 0.16, 0.27, 0.405, 8)
    tr = box("Trunk", -0.09, 0.09, 0.03, 0.27, 0.0, 0.30, ce, 0.03, 4); parts.append((tr, 0)); hs.add(tr)
    dk = box("Deck", -0.175, 0.175, 0.07, 0.30, 0.28, 0.405, ce, 0.02, 4); parts.append((dk, 0)); hs.add(dk)
    parts.append((cyl("Water", 0.06, 0.004, (0, -0.085, 0.205), WATERS(), "Z", 24), 0))
    parts.append((pipe("Seal", [oval(0.0, -0.06, 0.107, 0.192, 0.004, 32)], 0.004, wp, True), 0))
    for s in (-1, 1): parts.append((cyl("HingePost", 0.011, 0.022, (s * 0.075, 0.10, 0.414), ch, "Z", 14, 0.002), 1))
    # the cistern: four walls and a floor, the works inside
    x0, x1, y0, y1, z0, z1, t = -0.19, 0.19, 0.14, 0.30, 0.405, 0.76, 0.012
    for nm, bx in (("TankFront", (x0, x1, y0, y0 + t)), ("TankBack", (x0, x1, y1 - t, y1)), ("TankL", (x0, x0 + t, y0 + t - 0.001, y1 - t + 0.001)), ("TankR", (x1 - t, x1, y0 + t - 0.001, y1 - t + 0.001))):
        w = box(nm, bx[0], bx[1], bx[2], bx[3], z0, z1, ce, 0.005, 3); parts.append((w, 2)); hs.add(w)
    fl = box("TankFloor", x0 + 0.002, x1 - 0.002, y0 + 0.002, y1 - 0.002, z0, z0 + 0.015, ce, 0, 0); parts.append((fl, 2))
    yc = (y0 + y1) / 2
    parts.append((cyl("FillValve", 0.014, 0.27, (-0.12, yc, z0 + 0.15), wp, "Z", 14), 3)); parts.append((cyl("Float", 0.03, 0.05, (-0.12, yc, 0.64), BLACKP(), "Z", 20, 0.004), 3))
    parts.append((cyl("FillCap", 0.02, 0.02, (-0.12, yc, z0 + 0.29), M("Dot0", lambda n: H.plastic(n, (0.1, 0.25, 0.75), 0.4, 0.0, 0.0)), "Z", 14), 3))
    parts.append((cyl("FlushValve", 0.036, 0.20, (0.03, yc, z0 + 0.115), wp, "Z", 24, 0.004), 3)); parts.append((cyl("FlushRod", 0.008, 0.11, (0.03, yc, z0 + 0.27), wp, "Z", 10), 3))
    parts.append((cyl("OverflowTube", 0.01, 0.26, (0.09, yc + 0.03, z0 + 0.145), wp, "Z", 10), 3))
    parts.append((box("TankWater", x0 + t, x1 - t, y0 + t, y1 - t, 0.655, 0.659, WATERS(), 0, 0), 3))
    parts.append((cyl("WallValve", 0.012, 0.04, (-0.235, 0.28, 0.20), ch, "Y", 14), 4)); parts.append((cyl("WallValveRose", 0.026, 0.006, (-0.235, 0.297, 0.20), ch, "Y", 24), 4))
    parts.append((pipe("Supply", [[(-0.235, 0.262, 0.20), (-0.235, 0.24, 0.23), (-0.21, 0.225, 0.34), (-0.165, yc, z0 + 0.002)]], 0.0045, STEEL()), 4))
    return parts, hs

def build_toilet_seat():
    """the seat ring 0.38 x 0.43 m: origin on the hinge axis (along X), the ring goes to -Y; it lifts by turning about X"""
    hs = Hulls(); parts = []; sp = SEATP()
    O = lambda rx, ry, z: oval(0.0, -0.22, rx, ry, z, 32)
    parts.append((loft("Seat", [O(0.188, 0.212, -0.009), O(0.192, 0.216, 0.0), O(0.185, 0.209, 0.009), O(0.130, 0.152, 0.009), O(0.124, 0.146, 0.0), O(0.130, 0.152, -0.009)], sp, cyclic=True), 0))
    parts.append((box("HingeTab", -0.10, 0.10, -0.03, 0.012, -0.009, 0.009, sp, 0.006, 3), 0))
    for s in (-1, 1):
        parts.append((cyl("Barrel", 0.009, 0.03, (s * 0.075, 0.0, 0.0), CHROME(), "X", 14), 0))
        for y in (-0.10, -0.34): parts.append((box("Buffer", s * 0.15 - 0.012, s * 0.15 + 0.012, y - 0.006, y + 0.006, -0.0125, -0.008, RUB(), 0.002, 1), 0))
    ring_hulls(hs, 0.0, -0.22, 0.19, 0.214, 0.126, 0.148, -0.009, 0.009, 8)
    return parts, hs

def build_toilet_lid():
    """the lid 0.38 x 0.43 m: origin on the hinge axis (along X), its underside at z=0, the lid goes to -Y"""
    hs = Hulls(); parts = []; sp = SEATP()
    O = lambda rx, ry, z: oval(0.0, -0.22, rx, ry, z, 32)
    parts.append((loft("Lid", [O(0.186, 0.210, 0.0), O(0.192, 0.216, 0.006), O(0.176, 0.200, 0.017), O(0.09, 0.105, 0.022)], sp), 0))
    parts.append((box("HingeTab", -0.10, 0.10, -0.03, 0.012, 0.0, 0.014, sp, 0.005, 3), 0))
    hs.box(-0.19, 0.19, -0.435, 0.012, 0.0, 0.022)
    return parts, hs

def build_tank_lid():
    """the cistern lid 0.40 x 0.18 m with the dual flush button: origin = the middle of its underside; it lifts off"""
    hs = Hulls(); parts = []; ce = CERAMIC()
    ld = box("TankLid", -0.20, 0.20, -0.09, 0.09, 0.0, 0.028, ce, 0.010, 4); parts.append((ld, 0)); hs.add(ld)
    parts.append((box("Lip", -0.176, 0.176, -0.066, 0.066, -0.010, 0.001, ce, 0.002, 1), 0))
    parts.append((lathe("Button", [(0, 0.0), (0.028, 0.0), (0.028, 0.004), (0.024, 0.007), (0, 0.007)], CHROME(), (0, 0, 0.028), 28), 0))
    parts.append((box("ButtonSplit", -0.0008, 0.0008, -0.024, 0.024, 0.035, 0.0353, RUBK(), 0, 0), 0))
    return parts, hs

def build_washer():
    """front-loading washing machine 0.60 x 0.60 x 0.85 m (+0.05 of hoses behind; front -Y): enamelled steel cabinet on four
    feet, laminate top, control strip (detergent drawer, programme dial, display, buttons), kick plate with the filter flap,
    the porthole with its rubber gasket; inside the perforated steel drum with three lifters and a towel left in it. The door
    is its own mesh."""
    hw, zc, rh = 0.30, 0.43, 0.155
    hs = Hulls(); parts = []; wp = WHITEP(); gp = GREYP()
    wh = M("WasherWhite", paint, (0.90, 0.90, 0.88), 0.22, 0.02, 0.12, (0.30, 0.30, 0.32), True, 0.02)
    for s in (-1, 1):
        a, b = sorted((s * hw, s * (hw - 0.012)))
        sd = box("Side", a, b, -hw + 0.012, hw, 0.02, 0.82, wh, 0.003, 2); parts.append((sd, 1 + (s > 0))); hs.add(sd)
    bk = box("Back", -hw + 0.012, hw - 0.012, hw - 0.012, hw, 0.02, 0.82, wh, 0.002, 1); parts.append((bk, 3)); hs.add(bk)
    bt = box("Bottom", -hw + 0.012, hw - 0.012, -hw + 0.012, hw - 0.012, 0.02, 0.035, wh, 0, 0); parts.append((bt, 3)); hs.add(bt)
    tp = box("Top", -hw, hw, -hw, hw, 0.82, 0.85, wp, 0.008, 4); parts.append((tp, 4)); hs.add(tp)
    fr = box("Front", -hw, hw, -hw, -hw + 0.012, 0.02, 0.82, wh, 0, 0); hole(fr, rh, (0, -hw + 0.006, zc), "Y", 0.1); HL._mod_bevel(fr, 0.003, 2); parts.append((fr, 5))
    hs.box(-hw, -0.16, -hw, -hw + 0.012, 0.02, 0.82); hs.box(0.16, hw, -hw, -hw + 0.012, 0.02, 0.82)
    hs.box(-0.16, 0.16, -hw, -hw + 0.012, 0.59, 0.82); hs.box(-0.16, 0.16, -hw, -hw + 0.012, 0.02, 0.27)
    yp = -hw
    parts.append((box("Panel", -hw + 0.004, hw - 0.004, yp - 0.006, yp + 0.001, 0.70, 0.815, wp, 0.004, 3), 5))
    parts.append((box("Dispenser", -0.28, -0.10, yp - 0.010, yp - 0.005, 0.715, 0.80, wp, 0.003, 2), 5))
    parts.append((box("DispenserGrip", -0.25, -0.13, yp - 0.0106, yp - 0.0095, 0.722, 0.742, RUBK(), 0, 0), 5))
    parts.append((lathe("Dial", [(0, 0.0), (0.036, 0.0), (0.036, 0.004), (0.030, 0.006), (0.028, 0.024), (0.024, 0.028), (0, 0.028)], CHROME(), (-0.02, yp - 0.006, 0.757), 32, 0, (PI / 2, 0, 0)), 5))
    parts.append((box("DialMark", -0.022, -0.018, yp - 0.0345, yp - 0.0335, 0.762, 0.782, RUBK(), 0, 0), 5))
    parts.append((box("Display", 0.05, 0.15, yp - 0.0072, yp - 0.0055, 0.74, 0.776, SMOKED(), 0.001, 1), 5))
    for k in range(4):
        parts.append((cyl("Button", 0.008, 0.004, (0.18 + k * 0.026, yp - 0.007, 0.75), gp, "Y", 14), 5))
        parts.append((cyl("Led", 0.002, 0.002, (0.18 + k * 0.026, yp - 0.0065, 0.768), M("LedGreen", emissive, (0.1, 1.0, 0.2), 2.0), "Y", 8), 5))
    parts.append((box("Kick", -hw + 0.004, hw - 0.004, yp - 0.003, yp + 0.001, 0.025, 0.11, gp, 0.003, 2), 5))
    parts.append((box("FilterFlap", 0.15, 0.27, yp - 0.0048, yp - 0.0025, 0.04, 0.095, gp, 0.002, 2), 5))
    parts.append((box("FlapSlot", 0.19, 0.23, yp - 0.0053, yp - 0.0045, 0.086, 0.09, RUBK(), 0, 0), 5))
    parts.append((lathe("Bezel", closed([(rh, 0.0), (rh + 0.004, 0.005), (0.208, 0.005), (0.214, 0.0)]), CHROME(), (0, yp, zc), 48, 0, (PI / 2, 0, 0)), 5))
    parts.append((lathe("Gasket", closed([(rh + 0.002, 0.0), (rh - 0.006, -0.012), (0.135, -0.022), (0.14, -0.036), (0.126, -0.05), (0.12, -0.05), (0.132, -0.036), (0.128, -0.02), (rh - 0.012, -0.008), (rh - 0.004, 0.0)]), RUB(), (0, yp, zc), 48, 0, (PI / 2, 0, 0)), 6))
    drum = lathe("Drum", [(0, -0.49), (0.19, -0.49), (0.20, -0.47), (0.20, -0.07), (0.128, -0.052), (0.128, -0.058), (0.205, -0.078), (0.205, -0.48), (0.195, -0.50), (0, -0.50)], M("DrumSteel", perforated), (0, yp, zc), 48, 0, (PI / 2, 0, 0))
    parts.append((drum, 6)); hs.box(-0.12, 0.12, -0.25, 0.19, zc - 0.205, zc - 0.185)
    for a in (30, 150, 270):
        parts.append((box("Lifter", -0.02, 0.02, -0.21, 0.15, zc + 0.165, zc + 0.20, gp, 0.008, 3, Matrix.Rotation(math.radians(a), 4, "Y"), (0, 0, zc)), 6))
    parts.append((box("Towel", -0.11, 0.10, -0.17, 0.07, zc - 0.196, zc - 0.13, TERRY("TowelTeal", (0.10, 0.34, 0.38)), 0.028, 3, Rz(12)), 7))
    for sx in (-1, 1):
        for sy in (-1, 1):
            ft = lathe("Foot", [(0, 0.0), (0.02, 0.0), (0.022, 0.004), (0.016, 0.012), (0.008, 0.014), (0.008, 0.022), (0, 0.022)], BLACKP(), (sx * (hw - 0.05), sy * (hw - 0.05), 0), 16); parts.append((ft, 3)); hs.add(ft)
    # the back: service cover, transport bolt caps, inlet and drain hoses, the mains cord
    parts.append((box("Service", -0.18, 0.18, hw - 0.001, hw + 0.003, 0.30, 0.62, wh, 0.002, 1), 3))
    for (x, z) in ((-0.20, 0.66), (0.0, 0.70), (0.20, 0.66)): parts.append((cyl("BoltCap", 0.014, 0.006, (x, hw + 0.003, z), wp, "Y", 14), 3))
    parts.append((pipe("Inlet", [[(0.22, hw, 0.77), (0.22, hw + 0.03, 0.75), (0.22, hw + 0.035, 0.30), (0.25, hw + 0.03, 0.08)]], 0.008, RUB()), 8))
    parts.append((pipe("Drain", [[(-0.22, hw, 0.12), (-0.22, hw + 0.03, 0.14), (-0.22, hw + 0.034, 0.66), (-0.25, hw + 0.03, 0.72), (-0.285, hw + 0.03, 0.60)]], 0.011, gp), 8))
    parts.append((pipe("Cord", [[(0.10, hw, 0.78), (0.10, hw + 0.02, 0.74), (0.12, hw + 0.02, 0.20), (0.16, hw + 0.03, 0.01)]], 0.0035, BLACKP()), 8))
    return parts, hs

def build_washer_door():
    """the porthole door, 0.43 m across: origin on its hinge axis (vertical) at the height of the porthole's centre, its back
    face at y=0; the door's centre is 0.215 to +X. Plastic ring, smoked glass bowl, grip, steel hinge"""
    cx = 0.215; hs = Hulls(); parts = []
    ringm = M("DoorSilver", lambda n: H.plastic(n, (0.66, 0.67, 0.68), 0.30, 0.02, 0.2))
    parts.append((lathe("DoorRing", closed([(0.108, 0.0), (0.212, 0.0), (0.214, 0.012), (0.19, 0.032), (0.15, 0.036), (0.108, 0.022)]), ringm, (cx, 0, 0), 48, 0, (PI / 2, 0, 0)), 0))
    parts.append((lathe("DoorGlass", [(0, 0.024), (0.108, 0.024), (0.15, 0.012), (0.15, 0.0), (0.126, -0.03), (0.07, -0.06), (0, -0.066)], SMOKED(), (cx, 0, 0), 40, 0, (PI / 2, 0, 0)), 1))
    parts.append((box("Grip", cx + 0.165, cx + 0.212, -0.03, -0.004, -0.05, 0.05, GREYP(), 0.006, 3), 0))
    parts.append((box("Hinge", -0.012, 0.03, -0.012, 0.004, -0.06, 0.06, STEEL(), 0.003, 2), 0))
    parts.append((cyl("HingePin", 0.006, 0.13, (0.0, -0.004, 0.0), STEEL(), "Z", 12), 0))
    hs.box(0.0, 0.43, -0.036, 0.0, -0.215, 0.215)
    return parts, hs

CAB_Z, RAIL_Z, PAPER_Z = 1.15, 0.85, 0.70

def build_bath_cabinet():
    """mirror cabinet 0.60 x 0.14 x 0.70 m on the wall (origin ON THE FLOOR at the wall; the cabinet hangs at 1.15..1.85 m):
    white carcass, two glass shelves, the door is its own mesh. Inside: tumbler with two toothbrushes, toothpaste, a cream jar,
    a pill bottle, a spray, a box of plasters, a roll of cotton"""
    hw, D, z0, z1, t = 0.30, 0.14, CAB_Z, CAB_Z + 0.70, 0.015; yf = -D + 0.018
    hs = Hulls(); parts = []; lq = LACQ()
    for s in (-1, 1):
        a, b = sorted((s * hw, s * (hw - t)))
        sd = box("Side", a, b, yf, 0.0, z0, z1, lq, 0.002, 2); parts.append((sd, 1 + (s > 0))); hs.add(sd)
    for nm, za in (("Bottom", z0), ("Top", z1 - t)):
        b_ = box(nm, -hw + t - 0.001, hw - t + 0.001, yf, 0.0, za, za + t, lq, 0.002, 2); parts.append((b_, 3)); hs.add(b_)
    bk = box("Back", -hw + 0.003, hw - 0.003, -0.005, 0.0, z0 + 0.002, z1 - 0.002, lq, 0, 0); parts.append((bk, 4)); hs.add(bk)
    gl = M("ShelfGlass", glossy, (0.72, 0.82, 0.80), 0.22, 0.4)
    for z in (z0 + 0.24, z0 + 0.47):
        sh = box("Shelf", -hw + t - 0.001, hw - t + 0.001, yf + 0.012, -0.005, z, z + 0.006, gl, 0.001, 1); parts.append((sh, 5)); hs.add(sh)
        for x in (-hw + t + 0.004, hw - t - 0.004):
            for y in (yf + 0.03, -0.02): parts.append((cyl("ShelfPin", 0.003, 0.008, (x, y, z - 0.003), CHROME(), "X", 8), 5))
    za, zb_, zc_ = z0 + t, z0 + 0.246, z0 + 0.476
    must = M("CupMustard", glossy, (0.72, 0.50, 0.10), 0.12, 0.5)
    parts.append((lathe("Tumbler", [(0, 0.0), (0.030, 0.0), (0.034, 0.085), (0.031, 0.085), (0.028, 0.006), (0, 0.006)], must, (-0.19, -0.06, za), 24), 6))
    for k, c in enumerate(((0.05, 0.45, 0.48), (0.75, 0.1, 0.08))):
        bm_ = M("Brush%d" % k, lambda n, c=c: H.plastic(n, c, 0.35, 0.02, 0.1)); dx = -0.012 + 0.03 * k
        parts.append((pipe("Brush", [[(-0.19, -0.06, za + 0.01), (-0.19 + dx, -0.05, za + 0.17)]], 0.004, bm_), 6))
        parts.append((box("Bristles", -0.19 + dx - 0.005, -0.19 + dx + 0.005, -0.062, -0.05, za + 0.15, za + 0.175, WHITEP(), 0.002, 1), 6))
    parts.append((box("Toothpaste", -0.10, 0.03, -0.085, -0.045, za, za + 0.028, WHITEP(), 0.012, 3, Rz(12)), 6))
    parts.append((cyl("PasteCap", 0.011, 0.018, (0.04, -0.052, za + 0.014), M("Brush1", lambda n: H.plastic(n, (0.75, 0.1, 0.08), 0.35, 0.02, 0.1)), "X", 12), 6))
    parts.append((box("Plasters", 0.10, 0.20, -0.09, -0.03, za, za + 0.05, M("PlasterBox", paint, (0.86, 0.85, 0.80), 0.5, 0.01, 0.2, (0.6, 0.5, 0.4)), 0.003, 2), 6))
    amber = M("Amber", glossy, (0.45, 0.20, 0.04), 0.10, 0.6)
    parts.append((lathe("Pills", [(0, 0.0), (0.022, 0.0), (0.024, 0.004), (0.024, 0.06), (0.016, 0.066), (0.016, 0.07), (0.019, 0.07), (0.019, 0.088), (0, 0.088)], amber, (0.17, -0.06, zb_), 20), 6))
    parts.append((lathe("Cream", [(0, 0.0), (0.032, 0.0), (0.034, 0.004), (0.034, 0.034), (0.035, 0.034), (0.035, 0.05), (0.03, 0.054), (0, 0.054)], WHITEP(), (0.04, -0.06, zb_), 24), 6))
    parts.append((cyl("Cotton", 0.03, 0.09, (-0.14, -0.06, zb_ + 0.03), LINEN(), "X", 20, 0.008), 6))
    teal = M("BottleTeal", lambda n: H.plastic(n, (0.05, 0.45, 0.48), 0.3, 0.02, 0.15))
    parts.append((lathe("Spray", [(0, 0.0), (0.024, 0.0), (0.026, 0.006), (0.026, 0.10), (0.012, 0.118), (0.012, 0.13), (0.015, 0.13), (0.015, 0.16), (0, 0.16)], teal, (-0.12, -0.06, zc_), 20), 6))
    parts.append((box("Razor", 0.02, 0.15, -0.075, -0.06, zc_, zc_ + 0.012, GREYP(), 0.004, 2, Rz(-20)), 6))
    return parts, hs

def build_bath_cabinet_door():
    """the mirror door 0.596 x 0.696 x 0.018 m: origin on the hinge axis (its left edge) at its bottom, back face y=0"""
    w, h = 0.596, 0.696; hs = Hulls(); parts = []
    parts.append((box("Door", 0.0, w, -0.014, 0.0, 0.0, h, LACQ(), 0.002, 2), 0))
    parts.append((box("Mirror", 0.004, w - 0.004, -0.018, -0.014, 0.004, h - 0.004, MIRROR(), 0.0015, 1), 1))
    cup_hinges([0.036], [0.10, h - 0.10], 0.0, parts, 0)
    parts.append((box("Pull", w - 0.11, w - 0.03, -0.012, -0.002, -0.006, 0.001, CHROME(), 0.002, 2), 0))
    hs.box(0.0, w, -0.018, 0.0, 0.0, h)
    return parts, hs

def build_towel_rail():
    """heated towel rail, a chrome ladder 0.53 x 0.80 m on the wall (origin ON THE FLOOR at the wall; the ladder at 0.85..1.65 m,
    its tails down to 0.80): two uprights with caps, eight bowed rungs in three groups, four wall stands, valves into the wall"""
    ch = CHROME(); z0, yr = RAIL_Z, -0.07; hs = Hulls(); parts = []
    for s in (-1, 1):
        x = s * 0.25
        v = cyl("Upright", 0.015, 0.80, (x, yr, z0 + 0.40), ch, "Z", 20); parts.append((v, 0)); hs.add(v)
        parts.append((lathe("Cap", [(0, 0.0), (0.015, 0.0), (0.013, 0.008), (0.006, 0.013), (0, 0.014)], ch, (x, yr, z0 + 0.80), 20), 0))
        for z in (z0 + 0.12, z0 + 0.70):
            parts.append((cyl("Stand", 0.009, 0.07, (x, yr / 2, z), ch, "Y", 14), 1)); parts.append((cyl("StandRose", 0.022, 0.006, (x, -0.003, z), ch, "Y", 24, 0.002), 1))
        parts.append((pipe("Tail", [[(x, yr, z0), (x, yr, z0 - 0.045), (x, 0.0, z0 - 0.045)]], 0.012, ch), 1))
        parts.append((cyl("Valve", 0.018, 0.028, (x, yr, z0 - 0.012), ch, "Z", 6), 1)); parts.append((cyl("TailRose", 0.026, 0.006, (x, -0.003, z0 - 0.045), ch, "Y", 24, 0.002), 1))
        parts.append((cyl("ValveKnob", 0.016, 0.022, (x + s * 0.03, yr, z0 - 0.045), WHITEP(), "X", 14, 0.003), 1))
    for dz in (0.06, 0.14, 0.22, 0.36, 0.44, 0.52, 0.66, 0.74):
        z = z0 + dz
        parts.append((pipe("Rung", [[(-0.25, yr, z), (-0.17, yr - 0.018, z), (0.17, yr - 0.018, z), (0.25, yr, z)]], 0.0105, ch), 0))
    hs.box(-0.25, 0.25, yr - 0.03, yr + 0.011, z0 + 0.05, z0 + 0.75)
    return parts, hs

def build_towel():
    """a terry towel 0.30 m wide over the top rung of the towel rail and down to the floor (the rail's frame: origin on the floor
    at the wall): the ants' way up"""
    path = [(-0.062, 1.20), (-0.066, 1.57)] + arc(-0.088, RAIL_Z + 0.74, 0.022, 0, 180, 6) + [(-0.118, 1.50), (-0.124, 1.30), (-0.124, 0.062)] + arc(-0.174, 0.062, 0.05, 0, -90, 4) + [(-0.31, 0.012)]
    o = strip("Towel", path, 0.32, TERRY("TowelTeal", (0.10, 0.34, 0.38)), 0.012, 10, 0.03, 0.010, 3, 0.0, (0.19, 0.22))
    hems = []
    hs = Hulls(); hs.box(-0.15, 0.15, -0.125, -0.05, 0.0, 1.62); hs.box(-0.15, 0.15, -0.30, -0.11, 0.0, 0.03)
    return [(o, 0)] + [(h_, 0) for h_ in hems], hs

def build_bath_mat():
    """the bath mat 0.40 m wide hung over the tub's front side and down to the floor (the tub's frame): the ants' way up to the rim"""
    path = [(-0.245, 0.36), (-0.262, 0.45), (-0.277, 0.53), (-0.287, 0.552), (-0.30, 0.558), (-0.335, 0.558), (-0.352, 0.553), (-0.36, 0.535), (-0.362, 0.50), (-0.362, 0.062)] \
           + arc(-0.412, 0.062, 0.05, 0, -90, 4) + [(-0.55, 0.012)]
    m = M("MatChenille", lambda n: quilt(fabric(n, (0.72, 0.50, 0.10), (0.52, 0.33, 0.06), 180.0, 0.6, 0.3, 1.0), 0.03, 4.0, "X"))
    o = strip("Mat", path, 0.40, m, 0.012, 10, 0.03, 0.007, 5, -0.07)
    hs = Hulls(); hs.box(-0.27, 0.13, -0.37, -0.355, 0.0, 0.56); hs.box(-0.27, 0.13, -0.55, -0.36, 0.0, 0.03)
    return [(o, 0)], hs

def build_paper_holder():
    """toilet-paper holder with a phone shelf on the wall (origin ON THE FLOOR at the wall, the roll 0.70 m up): chrome plate
    and arm, the roll on its cardboard core, its tail unrolled down to the floor (the ants' way up)"""
    z = PAPER_Z; ch = CHROME(); hs = Hulls(); parts = []
    parts.append((box("Plate", -0.07, 0.07, -0.006, 0.0, z + 0.03, z + 0.078, ch, 0.003, 2), 0))
    sh = box("Shelf", -0.10, 0.10, -0.12, 0.0, z + 0.078, z + 0.086, ch, 0.003, 2); parts.append((sh, 0)); hs.add(sh)
    parts.append((pipe("ShelfLip", [[(-0.098, -0.118, z + 0.09), (0.098, -0.118, z + 0.09)]], 0.003, ch), 0))
    parts.append((pipe("Arm", [[(0.078, -0.006, z + 0.05), (0.078, -0.065, z + 0.05), (0.078, -0.065, z), (-0.06, -0.065, z)]], 0.005, ch), 0))
    parts.append((lathe("Roll", closed([(0.02, -0.05), (0.055, -0.05), (0.057, -0.045), (0.057, 0.045), (0.055, 0.05), (0.02, 0.05)]), PAPER(), (0, -0.065, z), 32, 0, (0, PI / 2, 0)), 1))
    parts.append((lathe("Core", closed([(0.017, -0.05), (0.02, -0.05), (0.02, 0.05), (0.017, 0.05)]), CARD(), (0, -0.065, z), 20, 0, (0, PI / 2, 0)), 1))
    path = arc(-0.065, z, 0.058, 90, 180, 5) + [(-0.124, 0.05)] + arc(-0.154, 0.05, 0.03, 0, -90, 3) + [(-0.25, 0.012)]
    parts.append((strip("Tail", path, 0.10, PAPER(), 0.002, 4, 0.03, 0.004, 7), 2))
    hs.box(-0.057, 0.057, -0.123, -0.007, z - 0.057, z + 0.057); hs.box(-0.05, 0.05, -0.13, -0.118, 0.0, z); hs.box(-0.05, 0.05, -0.25, -0.12, 0.0, 0.02)
    return parts, hs

# ================================================================================================ HALL
HOOKS = (-0.46, -0.25, -0.02, 0.21, 0.44)       # the rack's hooks (x); the coat hangs on [1], the jacket on [3]
def BLKSTEEL(): return M("BlackSteel", paint, (0.03, 0.03, 0.035), 0.45, 0.05, 0.3, (0.45, 0.45, 0.47), True, 0.03)

def build_coat_rack():
    """coat rack 1.10 m wide on the wall (origin ON THE FLOOR at the wall): oak back board with five brass double hooks at
    1.62 m, a hat shelf 0.28 deep at 1.75..1.78 m on three black steel brackets, a brass gallery rail; a felt hat and a pair
    of gloves on the shelf"""
    hs = Hulls(); parts = []; oak = W_OAK("X"); br = BRASS(); bs = BLKSTEEL()
    bd = box("Board", -0.55, 0.55, -0.02, 0.0, 1.55, 1.69, oak, 0.005, 3); parts.append((bd, 0)); hs.add(bd)
    parts += [(o, 0) for o in screws_at("BoardScrew", [(x, -0.0202, 1.67, -1) for x in (-0.51, -0.13, 0.10, 0.51)], "Y", br, 0.0045)]
    for x in HOOKS:
        parts.append((lathe("HookRose", [(0, 0.0), (0.02, 0.0), (0.02, 0.003), (0.012, 0.008), (0, 0.008)], br, (x, -0.02, 1.62), 20, 0, (PI / 2, 0, 0)), 1))
        parts.append((pipe("HookTop", [[(x, -0.026, 1.62), (x, -0.06, 1.612), (x, -0.085, 1.63), (x, -0.09, 1.665)]], 0.005, br), 1))
        parts.append((pipe("HookLow", [[(x, -0.026, 1.61), (x, -0.05, 1.585), (x, -0.065, 1.59), (x, -0.07, 1.61)]], 0.0045, br), 1))
        for (y, z) in ((-0.09, 1.668), (-0.07, 1.613)):
            parts.append((lathe("HookTip", [(0, -0.008), (0.006, -0.006), (0.008, 0.0), (0.006, 0.006), (0, 0.008)], br, (x, y, z), 12), 1))
    sh = box("HatShelf", -0.55, 0.55, -0.28, 0.0, 1.75, 1.78, oak, 0.006, 4); parts.append((sh, 2)); hs.add(sh)
    for x in (-0.47, 0.09, 0.50):
        parts.append((box("BracketV", x - 0.012, x + 0.012, -0.026, -0.02, 1.57, 1.75, bs, 0.002, 1), 3))
        parts.append((box("BracketH", x - 0.012, x + 0.012, -0.24, -0.02, 1.744, 1.75, bs, 0.002, 1), 3))
        parts.append((pipe("Gusset", [[(x, -0.024, 1.60), (x, -0.20, 1.746)]], 0.004, bs), 3))
        parts += [(o, 3) for o in screws_at("BracketScrew", [(x, -0.0262, 1.59, -1), (x, -0.0262, 1.72, -1)], "Y", STEEL(), 0.0035)]
    parts.append((pipe("Gallery", [[(-0.53, -0.265, 1.812), (0.53, -0.265, 1.812)]], 0.005, br), 2))
    for x in (-0.53, 0.0, 0.53): parts.append((cyl("GalleryPost", 0.004, 0.034, (x, -0.265, 1.796), br, "Z", 10), 2))
    felt = M("HatFelt", fabric, (0.20, 0.17, 0.14), None, 900.0, 0.4)
    parts.append((lathe("Hat", [(0, 0.11), (0.05, 0.108), (0.085, 0.09), (0.09, 0.02), (0.095, 0.012), (0.125, 0.004), (0.13, 0.0), (0.125, -0.002), (0.085, 0.008), (0.08, 0.085), (0, 0.10)], felt, (-0.30, -0.145, 1.783), 36), 4))
    parts.append((pipe("HatBand", [oval(-0.30, -0.145, 0.092, 0.092, 1.808, 24)], 0.006, M("HatBandRed", fabric, (0.55, 0.07, 0.05), None, 600.0, 0.3), True), 4))
    lea = M("GloveLeather", lambda n: H.plastic(n, (0.16, 0.09, 0.05), 0.5, 0.08, 0.3))
    for k in (0, 1):
        parts.append((box("Glove", 0.16, 0.36, -0.20 + k * 0.03, -0.11 + k * 0.03, 1.78 + k * 0.018, 1.80 + k * 0.018, lea, 0.009, 3, Rz(15 - 35 * k)), 4))
    return parts, hs

def _coat(xh, hem, m, belt, key):
    """a coat hung by its loop on the rack's hook at x=xh (the rack's frame), its hem at `hem` metres"""
    yc = -0.085; hs = Hulls(); parts = []; Ln = 1.64 - hem
    def O(rx, ry, z, dx=0.0): return oval(xh + dx, yc, rx, ry, z, 28)
    body = loft("Body", [O(0.175, 0.050, hem), O(0.18, 0.056, hem + 0.02), O(0.165, 0.056, hem + Ln * 0.5), O(0.158, 0.06, 1.30), O(0.165, 0.062, 1.46), O(0.172, 0.058, 1.56),
                         O(0.13, 0.05, 1.60), O(0.07, 0.045, 1.63), O(0.055, 0.04, 1.655)], m)
    parts.append((body, 0))
    cuff = max(hem + 0.22, 0.88)
    for s in (-1, 1):
        sl = loft("Sleeve", [O(0.040, 0.038, cuff, s * 0.185), O(0.046, 0.044, cuff + 0.03, s * 0.183), O(0.050, 0.048, 1.28, s * 0.176), O(0.052, 0.05, 1.50, s * 0.165), O(0.04, 0.042, 1.57, s * 0.14)], m)
        parts.append((sl, 1))
        parts.append((pipe("CuffSeam", [O(0.047, 0.045, cuff + 0.05, s * 0.183)], 0.003, m, True), 1))
    parts.append((pipe("Collar", [O(0.08, 0.052, 1.632)], 0.014, m, True), 0))
    parts.append((pipe("Placket", [[(xh + 0.012, yc - 0.057, hem + 0.02), (xh + 0.012, yc - 0.063, 1.30), (xh + 0.014, yc - 0.062, 1.55)]], 0.004, m), 0))
    parts.append((pipe("HemSeam", [O(0.179, 0.056, hem + 0.03)], 0.003, m, True), 0))
    bt = M("ButtonDark", lambda n: H.plastic(n, (0.06, 0.04, 0.03), 0.35, 0.02, 0.1))
    n = 5 if hem < 0.3 else 4
    for k in range(n):
        z = hem + 0.30 + k * (1.47 - hem - 0.30) / (n - 1)
        parts.append((lathe("Button", [(0, 0.0), (0.011, 0.0), (0.012, 0.003), (0.008, 0.005), (0, 0.005)], bt, (xh - 0.012, yc - 0.0615, z), 14, 0, (PI / 2, 0, 0)), 0))
    zp = hem + Ln * 0.40
    for s in (-1, 1):
        parts.append((box("PocketFlap", xh + s * 0.095 - 0.05, xh + s * 0.095 + 0.05, yc - 0.062, yc - 0.054, zp, zp + 0.045, m, 0.004, 2), 0))
    if belt:
        parts.append((pipe("Belt", [O(0.165, 0.062, 1.08)], 0.011, m, True), 0))
        parts.append((box("Buckle", xh - 0.02, xh + 0.02, yc - 0.078, yc - 0.07, 1.062, 1.098, BRASS(), 0.003, 2), 0))
    parts.append((pipe("Loop", [[(xh - 0.014, yc + 0.02, 1.648), (xh, -0.089, 1.674), (xh + 0.014, yc + 0.02, 1.648)]], 0.0025, bt), 0))
    hs.box(xh - 0.225, xh + 0.225, -0.15, -0.02, hem, 1.66)
    return parts, hs

def build_coat():   return _coat(HOOKS[1], 0.05, M("CoatWool", fabric, (0.50, 0.36, 0.19), (0.36, 0.25, 0.12), 220.0, 0.5, 0.3, 1.0), True, "Coat")
def build_jacket(): return _coat(HOOKS[3], 0.62, M("Puffer", lambda n: quilt(fabric(n, (0.10, 0.34, 0.38), (0.06, 0.23, 0.26), 700.0, 0.3, 0.1, 0.3), 0.075, 9.0, "Z")), False, "Jacket")

def build_hall_mirror():
    """wall mirror 0.50 x 1.30 m in an oak frame (origin ON THE FLOOR at the wall; the mirror at 0.45..1.75 m); under it a key
    shelf with three brass hooks, a bunch of keys on one, a brass dish for coins"""
    z0, z1, hw, fw = 0.45, 1.75, 0.25, 0.045
    hs = Hulls(); parts = []; oak = W_OAK("Z"); oakx = W_OAK("X"); br = BRASS()
    for s in (-1, 1):
        a, b = sorted((s * hw, s * (hw - fw)))
        parts.append((box("Stile", a, b, -0.03, 0.0, z0, z1, oak, 0.004, 3), 1))
    for (za, zb_) in ((z0, z0 + fw), (z1 - fw, z1)):
        parts.append((box("FrameRail", -hw + fw - 0.001, hw - fw + 0.001, -0.03, 0.0, za, zb_, oakx, 0.004, 3), 1))
    parts.append((box("Backing", -hw + 0.01, hw - 0.01, -0.008, 0.0, z0 + 0.01, z1 - 0.01, HDF(), 0, 0), 1))
    parts.append((box("Glass", -hw + fw - 0.004, hw - fw + 0.004, -0.014, -0.010, z0 + fw - 0.004, z1 - fw + 0.004, MIRROR(), 0, 0), 2))
    parts.append((pipe("Bead", [rot_xz(rect_loop(-hw + fw, hw - fw, z0 + fw, z1 - fw, -0.029))], 0.004, oak, True), 1))
    hs.box(-hw, hw, -0.03, 0.0, z0, z1)
    sh = box("KeyShelf", -hw, hw, -0.11, 0.0, z0 - 0.03, z0 - 0.008, oakx, 0.004, 3); parts.append((sh, 3)); hs.add(sh)
    for x in (-0.15, 0.0, 0.15):
        parts.append((pipe("KeyHook", [[(x, -0.05, z0 - 0.03), (x, -0.05, z0 - 0.058), (x, -0.062, z0 - 0.072), (x, -0.076, z0 - 0.058)]], 0.003, br), 3))
    xk, zk = -0.15, z0 - 0.086
    parts.append((pipe("KeyRing", [[(xk + 0.014 * math.cos(a), -0.066, zk + 0.014 * math.sin(a)) for a in [2 * PI * k / 16 for k in range(16)]]], 0.0012, STEEL(), True), 3))
    for k, (dx, ln) in enumerate(((-0.008, 0.05), (0.006, 0.058))):
        parts.append((box("Key", xk + dx - 0.004, xk + dx + 0.004, -0.0675 - 0.002 * k, -0.066 - 0.002 * k, zk - 0.012 - ln, zk - 0.012, STEEL() if k else br, 0.001, 1, Matrix.Rotation(math.radians(8 - 16 * k), 4, "Y")), 3))
        parts.append((cyl("KeyBow", 0.011, 0.002, (xk + dx, -0.0667 - 0.002 * k, zk - 0.01), STEEL() if k else br, "Y", 14), 3))
    parts.append((box("Fob", xk - 0.012, xk + 0.012, -0.073, -0.069, zk - 0.085, zk - 0.02, M("FobLeather", lambda n: H.plastic(n, (0.72, 0.50, 0.10), 0.5, 0.08, 0.3)), 0.004, 2), 3))
    parts.append((lathe("Dish", [(0, 0.0), (0.05, 0.0), (0.06, 0.012), (0.057, 0.012), (0.048, 0.004), (0, 0.004)], br, (0.11, -0.055, z0 - 0.008), 28), 3))
    for (dx, dy) in ((0.0, 0.0), (0.02, 0.012), (-0.015, 0.018)):
        parts.append((cyl("Coin", 0.011, 0.0018, (0.11 + dx, -0.055 + dy, z0 - 0.003 + 0.0018 * abs(dx) * 50), STEEL(), "Z", 16), 3))
    return parts, hs

def DOORPAINT(): return M("DoorSteelBrown", paint, (0.16, 0.10, 0.07), 0.45, 0.06, 0.25, (0.45, 0.45, 0.47), True, 0.05)

def build_entry_frame():
    """steel frame of the entry door for a 0.88 x 2.05 m opening: origin = the middle of the threshold on the room-side face of
    the wall (y=0); the frame goes 0.09 m into the wall (+Y). Box-section jambs and head with the stop the leaf shuts on, a
    rubber seal, steel threshold, hinge knuckles and strike plate, walnut architrave on the room side"""
    hs = Hulls(); parts = []; dp = DOORPAINT(); wal = W_WALNUT("Z"); walx = W_WALNUT("X"); st = STEEL()
    for s in (-1, 1):
        a, b = sorted((s * 0.44, s * 0.50))
        j = box("Jamb", a, b, 0.0, 0.09, 0.0, 2.11, dp, 0.003, 2); parts.append((j, 1 + (s > 0))); hs.add(j)
        a, b = sorted((s * 0.425, s * 0.44))
        parts.append((box("Stop", a, b, 0.07, 0.09, 0.012, 2.05, dp, 0.002, 2), 1 + (s > 0)))
        a, b = sorted((s * 0.50, s * 0.57))
        parts.append((box("Architrave", a, b, -0.014, 0.0, 0.0, 2.18, wal, 0.004, 3), 4))
    hd = box("Head", -0.44, 0.44, 0.0, 0.09, 2.05, 2.11, dp, 0.003, 2); parts.append((hd, 3)); hs.add(hd)
    parts.append((box("HeadStop", -0.44, 0.44, 0.07, 0.09, 2.035, 2.05, dp, 0.002, 2), 3))
    parts.append((box("ArchitraveHead", -0.57, 0.57, -0.014, 0.0, 2.11, 2.18, walx, 0.004, 3), 4))
    th = box("Threshold", -0.44, 0.44, 0.0, 0.09, 0.0, 0.012, st, 0.003, 2); parts.append((th, 3)); hs.add(th)
    parts.append((pipe("Seal", [[(-0.43, 0.068, 0.014), (-0.43, 0.068, 2.04), (0.43, 0.068, 2.04), (0.43, 0.068, 0.014)]], 0.004, RUBK()), 3))
    for z in (0.25, 1.02, 1.80):        # the frame's hinge knuckles (the leaf's sit between them)
        parts.append((cyl("Knuckle", 0.009, 0.05, (-0.44, 0.0, z - 0.05), st, "Z", 14), 1))
        parts.append((box("KnuckleLeaf", -0.465, -0.44, -0.003, 0.004, z - 0.075, z - 0.025, st, 0.001, 1), 1))
    parts.append((box("Strike", 0.438, 0.4405, 0.02, 0.05, 0.85, 1.45, st, 0.0, 0), 2))
    for z in (1.0, 1.33, 1.36, 1.39): parts.append((box("StrikeHole", 0.4375, 0.4385, 0.026, 0.044, z - 0.012, z + 0.012, RUBK(), 0, 0), 2))
    parts += [(o, 2) for o in screws_at("StrikeScrew", [(0.4378, 0.035, 0.87, -1), (0.4378, 0.035, 1.43, -1)], "X", st, 0.004)]
    return parts, hs

def build_entry_leaf():
    """steel entry door leaf 0.86 x 2.02 x 0.06 m: origin on the hinge axis at the floor; the leaf goes to +X, its room face at
    y=0, the outer face at y=0.06; 10 mm clear of the floor (the ants' gap). Outside: painted steel with three pressed panels;
    inside: walnut panel with two beaded fields. Lever handles on long plates both sides, key cylinder, a second lock with a
    thumb turn, peephole, three hinge knuckles, bolts and latch in the lock edge, rubber seal"""
    W, z0, z1 = 0.87, 0.012 + 0.010, 2.04; hs = Hulls(); parts = []
    dp = DOORPAINT(); wal = W_WALNUT("Z"); ch = CHROME(); br = BRASS()
    core = box("Leaf", 0.01, W, 0.0, 0.06, z0, z1, dp, 0.003, 2); parts.append((core, 0)); hs.add(core)
    for (za, zb_) in ((z0 + 0.12, 0.72), (0.80, 1.36), (1.44, z1 - 0.12)):
        parts.append((box("Pressed", 0.13, W - 0.12, 0.06, 0.064, za, zb_, dp, 0.006, 3), 0))
    parts.append((box("Lining", 0.014, W - 0.004, -0.008, 0.0, z0 + 0.003, z1 - 0.003, wal, 0.003, 2), 1))
    for (za, zb_) in ((z0 + 0.14, 0.86), (1.16, z1 - 0.14)):
        parts.append((box("Field", 0.15, W - 0.14, -0.011, -0.008, za, zb_, wal, 0.004, 2), 1))
        parts.append((pipe("Bead", [rot_xz(rect_loop(0.15, W - 0.14, za, zb_, -0.0105))], 0.004, wal, True), 1))
    parts.append((pipe("LeafSeal", [rot_xz(rect_loop(0.022, W - 0.012, z0 + 0.012, z1 - 0.012, 0.061))], 0.004, RUBK(), True), 0))
    xl = W - 0.065
    for (y, s) in ((-0.008, -1), (0.064, 1)):       # hardware on both faces: s = -1 the room side, +1 the stairwell side
        parts.append((box("Plate", xl - 0.022, xl + 0.022, min(y, y + s * 0.004), max(y, y + s * 0.004), 0.89, 1.13, ch, 0.003, 2), 2))
        parts.append((cyl("LeverBoss", 0.012, 0.024, (xl, y + s * 0.016, 1.03), ch, "Y", 16), 2))
        parts.append((box("Lever", xl - 0.125, xl + 0.012, min(y + s * 0.022, y + s * 0.034), max(y + s * 0.022, y + s * 0.034), 1.022, 1.038, ch, 0.005, 3), 2))
        parts.append((cyl("Cylinder", 0.0085, 0.008, (xl, y + s * 0.008, 0.94), br, "Y", 14), 2))
        parts.append((box("KeySlot", xl - 0.0008, xl + 0.0008, min(y + s * 0.0118, y + s * 0.0125), max(y + s * 0.0118, y + s * 0.0125), 0.934, 0.946, RUBK(), 0, 0), 2))
        parts.append((lathe("Escutcheon", [(0, 0.0), (0.027, 0.0), (0.027, 0.003), (0.022, 0.006), (0, 0.006)], ch, (xl, y, 1.36), 24, 0, (-s * PI / 2, 0, 0)), 2))
        parts.append((lathe("Peephole", [(0, 0.0), (0.014, 0.0), (0.014, 0.003), (0.010, 0.006), (0.007, 0.006), (0.007, 0.004), (0, 0.004)], br, (W / 2, y, 1.50), 20, 0, (-s * PI / 2, 0, 0)), 2))
    parts.append((box("ThumbTurn", xl - 0.004, xl + 0.004, -0.034, -0.014, 1.345, 1.375, ch, 0.003, 2), 2))
    parts.append((cyl("Cylinder2", 0.0085, 0.008, (xl, 0.074, 1.36), br, "Y", 14), 2))
    parts.append((box("Faceplate", W - 0.0005, W + 0.0015, 0.016, 0.044, 0.85, 1.45, STEEL(), 0, 0), 2))
    for z in (1.33, 1.36, 1.39): parts.append((cyl("Bolt", 0.008, 0.006, (W + 0.003, 0.03, z), ch, "X", 14), 2))
    parts.append((box("Latch", W, W + 0.008, 0.022, 0.038, 0.99, 1.012, ch, 0.003, 2), 2))
    for z in (0.25, 1.02, 1.80):
        parts.append((cyl("Knuckle", 0.009, 0.05, (0.0, 0.0, z), STEEL(), "Z", 14), 0))
        parts.append((box("KnuckleLeaf", 0.0, 0.03, -0.003, 0.004, z - 0.025, z + 0.025, STEEL(), 0.001, 1), 0))
    return parts, hs

def build_doormat():
    """coir door mat 0.60 x 0.40 x 0.018 m in a ribbed rubber tray, two darker stripes woven in"""
    hs = Hulls(); parts = []
    def coir(n, c, d):
        m = fabric(n, c, d, 150.0, 0.2, 0.35, 1.0)
        bp = m.node_tree.nodes["Principled BSDF"].inputs["Normal"].links[0].from_node; bp.inputs["Strength"].default_value = 1.0; bp.inputs["Distance"].default_value = 0.002
        return m
    co = M("Coir", coir, (0.50, 0.36, 0.18), (0.30, 0.20, 0.09)); cd = M("CoirDark", coir, (0.20, 0.12, 0.06), (0.12, 0.07, 0.04))
    tray = box("Tray", -0.30, 0.30, -0.20, 0.20, 0.0, 0.008, RUBK(), 0.004, 2); parts.append((tray, 0)); hs.add(tray)
    parts.append((pipe("Rim", [rect_loop(-0.292, 0.292, -0.192, 0.192, 0.009, 0.015, 4)], 0.005, RUBK(), True), 0))
    for k in range(9):
        y = -0.186 + k * 0.0465
        for x in (-0.297, 0.297): parts.append((box("RimRib", x - 0.003, x + 0.003, y - 0.008, y + 0.008, 0.008, 0.0105, RUBK(), 0.001, 1), 0))
    pile = box("Pile", -0.283, 0.283, -0.183, 0.183, 0.006, 0.018, co, 0.004, 2); parts.append((pile, 1)); hs.add(pile)
    for y in (-0.12, 0.12): parts.append((box("Stripe", -0.283, 0.283, y - 0.014, y + 0.014, 0.0178, 0.0186, cd, 0.0, 0), 1))
    return parts, hs

# ================================================================================================ the tables of pieces
I4 = Matrix.Identity(4)
def P(ru, build, tex, breaks, cls, note, kids=None, attach=None):
    return {"ru": ru, "build": build, "tex": tex, "breaks": breaks, "cls": cls, "note": note, "kids": kids or [], "attach": attach}

def A(parent, at, move, axis=None, deg=None, slide=None):
    """where a part sits on its parent (the parent's frame, metres; one position per copy) and how it moves:
    hinge = turns about `axis` through its origin by up to `deg` (Blender's right-handed frame); slide / lift = moves by `slide`;
    rest = just lies / hangs there"""
    d = {"parent": parent, "at_m": [list(p) for p in at], "move": move}
    if axis: d["axis"] = axis; d["open_deg"] = deg
    if slide: d["slide_m"] = list(slide)
    return d

_NS = (0.0, -0.20, 0.36); _DR = [(0.0, -0.225, z) for z in DRESSER_Z]
_WL = (-0.5985, -0.278, 0.084); _WR = (0.5985, -0.278, 0.084)
BEDROOM = {
    "SM_B_Bed": P("Кровать двуспальная", build_bed, 4096, groups([[1], [2], [3], [4], [5, 6]]), "wood",
                  "double bed: oak frame bolted to square legs, beech slats in caps on ledgers and a centre beam, piped mattress with handles, oak headboard with a tufted teal pad",
                  [("SM_B_Pillow", T(-0.37, 0.62, 0.552) @ Rz(4), None), ("SM_B_Pillow", T(0.37, 0.63, 0.552) @ Rz(-5), None), ("SM_B_Duvet", I4, None)]),
    "SM_B_Pillow": P("Подушка", build_bed_pillow, 2048, groups([[0]], [(0, (0.0, 0, 0.08), (1, 0.2, 0), 0.01, 3, 0.0)]), "cloth",
                     "sleeping pillow in a linen case, piped round its seam", None, A("SM_B_Bed", [(-0.37, 0.62, 0.552), (0.37, 0.63, 0.552)], "rest")),
    "SM_B_Duvet": P("Одеяло (свисает до пола)", build_duvet, 4096, groups([[0]], [(0, (0.1, -0.3, 0.56), (1, 0.25, 0), 0.02, 5, 0.0)]), "cloth",
                    "quilted duvet on the bed, over the foot end and down to the floor (the ants climb it)", None, A("SM_B_Bed", [(0, 0, 0)], "rest")),
    "SM_B_Nightstand": P("Тумба прикроватная", build_nightstand, 2048, groups([[1], [2], [3], [4, 5], [6], [7]]), "wood",
                         "bedside table: walnut carcass on turned legs in brass cups, oak top with a glass ring, open niche with magazines and tissues",
                         [("SM_B_Nightstand_Drawer", T(*_NS), T(_NS[0], _NS[1] - 0.26, _NS[2]))]),
    "SM_B_Nightstand_Drawer": P("Ящик тумбы", build_nightstand_drawer, 2048, groups([[0], [1], [2]]), "wood",
                                "bedside drawer: walnut front, brass knob, beech box with a paper liner; a book, a tin, a charger inside",
                                None, A("SM_B_Nightstand", [_NS], "slide", slide=(0, -0.26, 0))),
    "SM_B_Dresser": P("Комод", build_dresser, 4096, groups([[1], [2], [3], [4, 5], [6], [7]]), "wood",
                      "chest of drawers: walnut carcass on turned legs, oak top with a crocheted runner, rails and runners for four drawers",
                      [("SM_B_Dresser_Drawer", T(*_DR[0]), None), ("SM_B_Dresser_Drawer", T(*_DR[1]), T(0, -0.225 - 0.30, DRESSER_Z[1])),
                       ("SM_B_Dresser_Drawer", T(*_DR[2]), None), ("SM_B_Dresser_Drawer", T(*_DR[3]), T(0, -0.225 - 0.14, DRESSER_Z[3]))]),
    "SM_B_Dresser_Drawer": P("Ящик комода (4 шт.)", build_dresser_drawer, 2048, groups([[0], [1], [2]]), "wood",
                             "dresser drawer: walnut front, two brass knobs, beech box; rolled socks and folded things inside",
                             None, A("SM_B_Dresser", _DR, "slide", slide=(0, -0.30, 0))),
    "SM_B_Wardrobe": P("Шкаф платяной", build_wardrobe, 4096, groups([[1], [2], [3], [4], [5], [6], [7, 8, 9], [10], [11, 12, 13, 14, 15], [16]]), "wood",
                       "wardrobe: walnut carcass on a plinth, oak crown; inside a chrome rail with hangers and clothes under a hat shelf, shelves on brass pins with folded linen, shoe boxes",
                       [("SM_B_Wardrobe_DoorL", T(*_WL), T(*_WL) @ Rz(-105)), ("SM_B_Wardrobe_DoorR", T(*_WR), T(*_WR) @ Rz(105))]),
    "SM_B_Wardrobe_DoorL": P("Дверца шкафа левая", lambda: build_wardrobe_door(-1), 2048, groups([[0], [1]]), "wood",
                             "wardrobe door, hinged on the left: green frame, two fielded panels with beads, brass knob, four cup hinges",
                             None, A("SM_B_Wardrobe", [_WL], "hinge", "Z", -105)),
    "SM_B_Wardrobe_DoorR": P("Дверца шкафа правая (с зеркалом внутри)", lambda: build_wardrobe_door(1), 2048, groups([[0], [1], [2]]), "wood",
                             "wardrobe door, hinged on the right: as the left one, with a dressing mirror on its inside",
                             None, A("SM_B_Wardrobe", [_WR], "hinge", "Z", 105)),
}
_TP = (0.415, -0.342, 0.021); _SD = (-0.248, -0.147, 0.064); _TS = (0.0, 0.10, 0.416); _TL = (0.0, 0.10, 0.426); _TK = (0.0, 0.22, 0.76)
_WD = (-0.215, -0.307, 0.43); _CD = (-0.298, -0.122, CAB_Z + 0.002)
BATH = {
    "SM_Bath_Tub": P("Ванна с экраном и смесителем", build_tub, 4096, groups([[0], [1], [2, 4], [3]], [(0, (0.1, 0, 0.3), (1, 0.15, 0), 0.01, 7, 0.0), (0, (-0.3, 0, 0.3), (0.3, 1, 0.1), 0.01, 8, 0.0)]), "ceramic",
                     "bathtub: enamelled shell with a rolled rim, screen on aluminium rails, end panels, cradles on feet, trap and overflow under it, waste, plug on a chain, wall mixer with shower",
                     [("SM_Bath_Tub_Panel", T(*_TP), T(_TP[0] - 0.80, _TP[1], _TP[2])), ("SM_Bath_Mat", I4, None)]),
    "SM_Bath_Tub_Panel": P("Сдвижная панель экрана ванны", build_tub_panel, 2048, groups([[0]], [(0, (0.05, -0.002, 0.24), (1, 0, 0.3), 0.006, 9, 0.0)]), "plastic",
                           "the sliding half of the bath screen: ribbed plastic sheet with a grip", None, A("SM_Bath_Tub", [_TP], "slide", slide=(-0.80, 0, 0))),
    "SM_Bath_Mat": P("Коврик на борту ванны (свисает до пола)", build_bath_mat, 2048, groups([[0]], [(0, (-0.07, -0.36, 0.3), (0.1, 0, 1), 0.01, 10, 0.0)]), "cloth",
                     "chenille bath mat hung over the tub's side down to the floor (the ants climb it)", None, A("SM_Bath_Tub", [(0, 0, 0)], "rest")),
    "SM_Bath_Sink": P("Раковина с тумбой", build_sink, 4096, groups([[1, 2], [3], [4], [5, 9], [6], [7], [8]], [(4, (0.05, 0, 0.80), (1, 0.2, 0), 0.006, 11, 0.0)]), "ceramic",
                      "washbasin on a cabinet: ceramic countertop basin, chrome mixer and waste, white carcass on chrome legs; bottle trap, braided hoses, angle valves, shelf and cleaning things inside",
                      [("SM_Bath_Sink_Door", T(*_SD), T(*_SD) @ Rz(-100))]),
    "SM_Bath_Sink_Door": P("Дверца тумбы раковины", build_sink_door, 2048, groups([[0]], [(0, (0.25, -0.008, 0.33), (1, 0, 0.4), 0.005, 12, 0.006)]), "wood",
                           "vanity door: white lacquer, chrome bar pull, cup hinges", None, A("SM_Bath_Sink", [_SD], "hinge", "Z", -100)),
    "SM_Bath_Toilet": P("Унитаз с бачком", build_toilet, 4096, groups([[0], [1, 4], [2], [3]], [(0, (0, -0.1, 0.25), (0.2, 1, 0.3), 0.01, 13, 0.0), (2, (0.02, 0.2, 0.6), (1, 0, 0.2), 0.008, 14, 0.0)]), "ceramic",
                        "toilet: ceramic pan with a real bowl and water, hollow cistern with fill valve, float and flush valve, supply hose from the wall valve",
                        [("SM_Bath_Toilet_Seat", T(*_TS), None), ("SM_Bath_Toilet_Lid", T(*_TL), T(*_TL) @ Rx(-92)), ("SM_Bath_Toilet_TankLid", T(*_TK), T(_TK[0], _TK[1], _TK[2] + 0.17))]),
    "SM_Bath_Toilet_Seat": P("Сиденье унитаза", build_toilet_seat, 2048, groups([[0]], [(0, (0.02, -0.22, 0), (1, 0.3, 0), 0.006, 15, 0.0)]), "plastic",
                             "toilet seat ring with rubber buffers and chrome hinge barrels", None, A("SM_Bath_Toilet", [_TS], "hinge", "X", -85)),
    "SM_Bath_Toilet_Lid": P("Крышка унитаза", build_toilet_lid, 2048, groups([[0]], [(0, (0.02, -0.22, 0.01), (1, -0.3, 0), 0.006, 16, 0.0)]), "plastic",
                            "toilet lid, domed", None, A("SM_Bath_Toilet", [_TL], "hinge", "X", -92)),
    "SM_Bath_Toilet_TankLid": P("Крышка бачка", build_tank_lid, 2048, groups([[0]], [(0, (0.05, 0, 0.014), (1, 0.2, 0), 0.006, 17, 0.0)]), "ceramic",
                                "cistern lid with the dual flush button", None, A("SM_Bath_Toilet", [_TK], "lift", slide=(0, 0, 0.17))),
    "SM_Bath_Washer": P("Стиральная машина", build_washer, 4096, groups([[1], [2], [3, 8], [4], [5], [6], [7]]), "metal",
                        "front-loading washing machine: enamelled cabinet, laminate top, control strip, kick plate with filter flap, gasket, perforated drum with lifters and a towel, hoses and cord behind",
                        [("SM_Bath_Washer_Door", T(*_WD), T(*_WD) @ Rz(-110))]),
    "SM_Bath_Washer_Door": P("Люк стиральной машины", build_washer_door, 2048, groups([[0], [1]], [(1, (0.215, 0, 0), (1, 0, 0.3), 0.006, 18, 0.0)]), "plastic",
                             "porthole door: plastic ring, smoked glass bowl, grip, steel hinge", None, A("SM_Bath_Washer", [_WD], "hinge", "Z", -110)),
    "SM_Bath_Cabinet": P("Шкафчик зеркальный настенный", build_bath_cabinet, 2048, groups([[1], [2], [3], [4], [5], [6]]), "wood",
                         "mirror cabinet on the wall: white carcass, glass shelves on pins; tumbler with toothbrushes, toothpaste, jars, a spray, plasters inside",
                         [("SM_Bath_Cabinet_Door", T(*_CD), T(*_CD) @ Rz(-110))]),
    "SM_Bath_Cabinet_Door": P("Дверца шкафчика с зеркалом", build_bath_cabinet_door, 2048, groups([[0], [1]], [(1, (0.3, -0.016, 0.35), (1, 0, 0.5), 0.004, 19, 0.0)]), "glass",
                              "mirror door of the bathroom cabinet", None, A("SM_Bath_Cabinet", [_CD], "hinge", "Z", -110)),
    "SM_Bath_TowelRail": P("Полотенцесушитель", build_towel_rail, 2048, groups([[0], [1]]), "metal",
                           "heated towel rail: chrome ladder, capped uprights, bowed rungs, wall stands, valves and tails into the wall", [("SM_Bath_Towel", I4, None)]),
    "SM_Bath_Towel": P("Полотенце на сушителе (свисает до пола)", build_towel, 2048, groups([[0]], [(0, (0, -0.114, 0.8), (0.1, 0, 1), 0.01, 20, 0.0)]), "cloth",
                       "terry towel over the top rung, down to the floor (the ants climb it)", None, A("SM_Bath_TowelRail", [(0, 0, 0)], "rest")),
    "SM_Bath_PaperHolder": P("Держатель туалетной бумаги с полкой", build_paper_holder, 2048, groups([[0], [1], [2]]), "paper",
                             "paper holder with a phone shelf: chrome plate and arm, the roll on its core, the tail unrolled to the floor (the ants climb it)"),
}
_EL = (-0.44, 0.0, 0.0)
HALL = {
    "SM_Hall_CoatRack": P("Вешалка настенная с полкой", build_coat_rack, 4096, groups([[0, 1], [2], [3], [4]]), "wood",
                          "coat rack: oak board with five brass double hooks, hat shelf on black steel brackets with a brass gallery, a felt hat and gloves",
                          [("SM_Hall_Coat", I4, None), ("SM_Hall_Jacket", I4, None)]),
    "SM_Hall_Coat": P("Пальто на вешалке (до пола)", build_coat, 2048, groups([[0], [1]]), "cloth",
                      "long belted wool coat hung by its loop on the rack's second hook, hem 5 cm off the floor (the ants climb it)", None, A("SM_Hall_CoatRack", [(0, 0, 0)], "rest")),
    "SM_Hall_Jacket": P("Куртка на вешалке", build_jacket, 2048, groups([[0], [1]]), "cloth",
                        "short quilted jacket on the rack's fourth hook, hem 0.62 m up", None, A("SM_Hall_CoatRack", [(0, 0, 0)], "rest")),
    "SM_Hall_Mirror": P("Зеркало с ключницей", build_hall_mirror, 2048, groups([[1], [2], [3]], [(1, (0, -0.012, 1.1), (0.2, 0, 1), 0.004, 21, 0.0)]), "glass",
                        "wall mirror in an oak frame, key shelf with brass hooks, keys and a coin dish"),
    "SM_Hall_EntryDoor_Frame": P("Входная дверь: коробка", build_entry_frame, 2048, groups([[1], [2], [3], [4]]), "metal",
                                 "steel entry door frame: box jambs and head with stops and seal, threshold, hinge knuckles, strike plate, walnut architrave",
                                 [("SM_Hall_EntryDoor_Leaf", T(*_EL), T(*_EL) @ Rz(-75))]),
    "SM_Hall_EntryDoor_Leaf": P("Входная дверь: полотно", build_entry_leaf, 4096, groups([[0], [1], [2]]), "metal",
                                "steel entry door leaf: pressed panels outside, walnut lining with beaded fields inside, lever handles, two locks, peephole, bolts, seal",
                                None, A("SM_Hall_EntryDoor_Frame", [_EL], "hinge", "Z", -95)),
    "SM_Hall_DoorMat": P("Коврик придверный", build_doormat, 2048, groups([[0], [1]], [(1, (0.05, 0, 0.012), (1, 0.3, 0), 0.01, 22, 0.0)]), "cloth",
                         "coir door mat in a ribbed rubber tray"),
}
ROOMS = {"bedroom": BEDROOM, "bath": BATH, "hall": HALL}

# ================================================================================================ manifest, review, main
def save_manifest(man):
    mpath = os.path.join(OUT, "hero_%s_manifest.json" % ROOM)
    json.dump(man, open(os.path.join(OUT, man["name"], "piece.json"), "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    pieces = []
    for d in sorted(os.listdir(OUT)):
        f = os.path.join(OUT, d, "piece.json")
        if os.path.exists(f): pieces.append(json.load(open(f, encoding="utf-8")))
    doc = {"note": "Hero art of the flat, %s (Tools/blender/hero_flat.py, KAN-41): LOD0 + UCX in <name>.fbx, _LOD1/_LOD2, _Brk<i> pieces; tex/T_<name>_{C,N,R}.png; "
                   "'attach' = where a part sits on its parent and how it moves (metres, the parent's frame, front -Y)" % ROOM, "pieces": pieces}
    tmp = mpath + ".%d.tmp" % os.getpid(); json.dump(doc, open(tmp, "w", encoding="utf-8"), indent=1, ensure_ascii=False); os.replace(tmp, mpath)

PREVM = {}
def _put(o, m):
    if o.data.materials: o.data.materials[0] = m
    else: o.data.materials.append(m)

def review(name, done, spec):
    """the sheet: overall | the ant's view | open (or from the other side) | wet | charred | broken"""
    lod0, pieces, paths = done[name]; prev = PREVM[name]
    kids = []
    for kn, mc, mo in spec["kids"]:
        if kn in done:
            c = done[kn][0].copy(); H.link(c); c.matrix_world = mc; kids.append((c, kn, mc, mo))
    back = []; at = spec["attach"]
    if at and at["move"] == "rest" and at["parent"] in done and not any(abs(v) > 1e-6 for v in at["at_m"][0]):   # (a part in its parent's frame: shown on it)
        c = done[at["parent"]][0].copy(); H.link(c); back.append(c)
    shown = [lod0] + [k[0] for k in kids] + back
    for o in H.S().objects: o.hide_render = o not in shown
    _put(lod0, prev)
    before = set(H.S().objects)
    mn, mx = H.bounds([lod0] + [k[0] for k in kids]); wmn, wmx = H.bounds(shown)
    lift = -mn.z if mn.z < -0.02 else 0.0          # (a part whose pivot is at its middle, the washer's door: stood on the studio floor)
    if lift:
        for o in [lod0] + list(pieces): o.matrix_world = Matrix.Translation((0, 0, lift)) @ o.matrix_world
        mn, mx = H.bounds([lod0] + [k[0] for k in kids]); wmn, wmx = H.bounds(shown)
    H.stage(wall_y=wmx.y + 0.003)
    cams = auto_cams(mn, mx); c0 = (mn + mx) * 0.5; e = mx - mn; s2 = max(e.x, e.y, e.z * 1.9, 0.25)
    def cam(k):
        loc, look, lens = cams[k]; H.camera(loc, look, lens, 9 if k == "ant" else 5.6)
    shots = []
    def shoot(tag, s=96):
        p = os.path.join(REV, "_%s_%s.jpg" % (name, tag)); H.render(p, 1280, 720, s); shots.append(p)
    cam("wide"); shoot("wide", 128)
    cam("ant"); shoot("ant", 128)
    if any(mo is not None for _, _, _, mo in kids):
        for c, kn, mc, mo in kids:
            if mo is not None: c.matrix_world = mo
        H.camera((c0.x - 0.55 * s2, mn.y - 1.35 * s2, c0.z + 0.35 * s2 + 0.05), (c0.x, c0.y - 0.1 * s2, c0.z), 35); shoot("open", 128)
        for c, kn, mc, mo in kids: c.matrix_world = mc
    else:
        H.camera((c0.x - 0.80 * s2, mn.y - 0.95 * s2, c0.z + 0.60 * s2 + 0.08), (c0.x, c0.y, c0.z), 35); shoot("side", 128)
    for tag, (w, ch_) in (("wet", (1.0, 0.0)), ("char", (0.0, 0.5))):
        _put(lod0, H.state_material(prev, w, ch_, 0.2))
        for c, kn, mc, mo in kids: c.data.materials[0] = H.state_material(PREVM[kn], w, ch_, 0.2)
        cam("wide"); shoot(tag)
    _put(lod0, prev)
    for c, kn, mc, mo in kids: c.data.materials[0] = PREVM[kn]; c.hide_render = True
    lod0.hide_render = True
    R = random.Random(5); mats0 = {x: x.matrix_world.copy() for x in pieces}
    for x in pieces:
        x.hide_render = False; _put(x, prev)
        a, b = H.bounds([x]); c = (a + b) * 0.5; d = c - c0; d.z = 0; d.y = min(d.y, 0.0)      # (never back into the studio wall)
        d = d.normalized() * 0.06 * max(0.3, e.length) if d.length > 1e-4 else Vector((0, 0, 0))
        x.matrix_world = Matrix.Translation((d.x, d.y, 0)) @ Matrix.Translation(c) @ Matrix.Rotation(R.uniform(-0.12, 0.12), 4, "Z") @ Matrix.Translation(-c)
    cam("wide"); shoot("broken")
    for x, mw in mats0.items(): x.matrix_world = mw; x.hide_render = True
    H.sheet(shots, os.path.join(REV, name + ".jpg"), 3)
    for o in [o for o in H.S().objects if o not in before]: bpy.data.objects.remove(o)
    for c in [k[0] for k in kids] + back: bpy.data.objects.remove(c)
    if lift:
        for o in [lod0] + list(pieces): o.matrix_world = Matrix.Translation((0, 0, -lift)) @ o.matrix_world

def main():
    t0 = time.time()
    os.makedirs(OUT, exist_ok=True); os.makedirs(REV, exist_ok=True)
    table = ROOMS[ROOM]; want = [n for n in table if ONLY is None or n in ONLY]
    done = {}
    for name in want:
        try:
            sp = table[name]; parts, hs = sp["build"]()
            man, lod0, pieces, paths = HL.produce(name, parts, hs, sp["breaks"], tex=sp["tex"], note=sp["note"], break_class=sp["cls"])
            man["set"] = "hero_" + ROOM; man["room"] = ROOM; man["ru"] = sp["ru"]
            if sp["attach"]: man["attach"] = sp["attach"]
            if sp["kids"]: man["children"] = sorted(set(k[0] for k in sp["kids"]))
            a, b = man["bbox_min_m"], man["bbox_max_m"]; man["size_uu"] = [int(round((b[i] - a[i]) * 1900)) for i in range(3)]
            save_manifest(man)
            PREVM[name] = H.preview_material(name, paths); lod0.data.materials[0] = PREVM[name]
            lod0.hide_render = True
            for x in pieces: x.hide_render = True
            done[name] = (lod0, pieces, paths)
        except Exception as ex:
            import traceback; traceback.print_exc(); print("HERO_FAILED", name, ex)
    for name in want:
        if name not in done: continue
        try:
            review(name, done, table[name]); print("HERO_REVIEWED", name)
        except Exception as ex:
            import traceback; traceback.print_exc(); print("REVIEW_FAILED", name, ex)
    print("FLAT_DONE %s %s %.0f s" % (ROOM, list(done.keys()), time.time() - t0))

main()
