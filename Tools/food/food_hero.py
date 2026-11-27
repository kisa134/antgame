# Food pass 2 (KAN-36, 27.11): every food at the level of the owner's mop & bucket reference (Saved/claude/shots/hero_mop_bucket.jpg).
#
# THE OWNER'S FOOD (the models the game shows today: Meshy SM_Food_*, Map4 SM_M4_*) is NOT replaced by a new design. Each gets a
# "hero" version of the SAME object: his mesh (same shape, proportions, orientation), his colour / normal / roughness textures
# (same colours), and on top of them the detail the mop has - real pores, fibres, holes with a rough inside, sprinkles, pips, a rind,
# a glossy glaze, sugar crystals - as procedural layers and a little added geometry; then baked to the game's set:
#   SourceArt/FoodHero/<Id>/SM_FoodHero_<Id>.fbx          LOD0 (<= 12k triangles) + UCX_ convex hull(s), slot "Outer"
#                          SM_FoodHero_<Id>_LOD1/_LOD2.fbx
#                          SM_FoodHero_<Id>_P<k>.fbx      pre-cut pieces: slot Outer (same atlas) + Inner (the cut: sponge, flesh, ...)
#                          T_FoodHero_<Id>_C/N/R.png      2048: colour, normal (OpenGL), AO / roughness / metal
#                          T_FoodHero_<Id>_Toast/Burnt/Spoil/Frozen.png   colour of the state, roughness in alpha (M_FoodV2 format)
#                          T_FoodHero_<Id>_InC.png        the inner strip (U along the cut, V = height or radius)
#                          food.json                      for Tools/food/import_food_v2.py (ANT_FOOD_SET=hero)
# THE NEW FOOD (not in the game before: grape, bone, eggshell...) is the pass-1 model (food_v2.py) put through the same pipeline
# with its looks raised (hero_upgrades below).
# Review, one sheet per food: Saved/food_review/v4/<Id>.jpg - his model | the hero version, the ant (8 mm) for scale,
# the states (wet, toasted, burnt, spoiled, frozen), the pre-cut pieces.
#
# Run: blender --background --factory-startup --python Tools/food/food_hero.py -- <project dir> <model|review|all> <Id,Id|old|new|all> [samples]
# Nothing from the internet: the owner's own Meshy / Higgsfield files and procedural shaders made here.
import bpy, bmesh, math, random, sys, os, json, time, glob
from mathutils import Vector, Matrix, Euler
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import food_v2 as FV
from food_v2 import NT

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
PROJ = argv[0] if argv else os.getcwd()
MODE = argv[1] if len(argv) > 1 else "all"
WHICH = argv[2] if len(argv) > 2 else "Cake"
SAMPLES = int(argv[3]) if len(argv) > 3 else 128
OUT_ART = os.path.join(PROJ, "SourceArt", "FoodHero")
OUT_REV = os.path.join(PROJ, "Saved", "food_review", "v4")
WORK = os.path.join(PROJ, "Saved", "food_hero_work")
MESHY = os.path.join(os.path.dirname(PROJ), "AntKitchen", "SourceArt", "Meshy")   # the owner's Meshy sources: main checkout, read only
M4 = os.path.join(PROJ, "SourceArt", "Map4", "props")
TEX, STEX = 2048, 1024
LOD0_MAX = 12000
ANT_LEN = 15.0 / 1900.0          # the ant: 15 uu = 7.9 mm
for d in (OUT_ART, OUT_REV, WORK): os.makedirs(d, exist_ok=True)

def log(*a): print("FOODHERO", *a, flush=True)
def S(): return FV.S
def active(o): FV.active(o)
def link(o): S().collection.objects.link(o); return o
def tris(o): return FV.tris(o)
def bounds(objs): return FV.bounds(objs)
def dup(o, n=None): return FV.dup(o, n)

# ================================================================ the owner's model
def load_owner(src):
    """src = (kind, name[, tint]): meshy | m4 | m4look. Returns (object, images dict or {}, tint). The object is his mesh, joined,
    transforms applied, UV map 'Own' (his UVs)."""
    kind, name = src[0], src[1]
    before = set(bpy.data.objects)
    imgs = {}
    if kind == "meshy":
        f = glob.glob(os.path.join(MESHY, name, "*", "*.fbx"))[0]; d = os.path.dirname(f)
        g = lambda pat: (glob.glob(os.path.join(d, pat)) or [None])[0]
        imgs = dict(C=g("*_texture.png"), N=g("*_texture_normal.png"), R=g("*_texture_roughness.png"), rch=0)
    else:
        f = os.path.join(M4, name, name + ".fbx")
        if kind == "m4":
            p = lambda k: os.path.join(M4, name, "T_%s_%s.png" % (name, k))
            imgs = dict(C=p("C"), N=p("N"), R=p("R"), rch=1)
    bpy.ops.import_scene.fbx(filepath=f)
    new = [o for o in bpy.data.objects if o not in before]
    for o in new:
        if o.type != "MESH": bpy.data.objects.remove(o, do_unlink=True)
    ms = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    for o in ms:
        if not o.data.uv_layers: o.data.uv_layers.new(name="Own")
        o.data.uv_layers[0].name = "Own"
        while len(o.data.uv_layers) > 1: o.data.uv_layers.remove(o.data.uv_layers[1])
    if kind == "m4look" and len(ms) > 1:
        # his generic Map4 shapes carry a second, inner shell (the paint layer of M_M4): the food is the outer one only
        big = max(ms, key=lambda m: sum(p.area for p in m.data.polygons) * max(m.dimensions))
        for m in ms:
            if m is not big: bpy.data.objects.remove(m, do_unlink=True)
        ms = [big]; log(name, "kept the outer shell of 2+")
    for x in S().objects: x.select_set(False)
    for o in ms: o.select_set(True)
    bpy.context.view_layer.objects.active = ms[0]
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if len(ms) > 1: bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active; o.name = "Own_" + name
    # (his exports split the vertices along the UV seams: welded - the UVs stay per corner - so decimation and the cuts see one skin)
    bm = bmesh.new(); bm.from_mesh(o.data); mn, mx = bounds([o])
    # (welded after the shells are told apart: two coincident shells would weld into one doubled skin)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=max(mx - mn) * 1e-5)
    dup_faces, seenf = [], set()
    for f in bm.faces:
        key = tuple(sorted(v.index for v in f.verts))
        if key in seenf: dup_faces.append(f)
        else: seenf.add(key)
    if dup_faces:
        bmesh.ops.delete(bm, geom=dup_faces, context="FACES_ONLY"); log(name, "doubled faces removed:", len(dup_faces))
    bm.to_mesh(o.data); bm.free()
    for p in o.data.polygons: p.use_smooth = True
    o.data.materials.clear()
    return o, imgs, (src[2] if len(src) > 2 else None)

def fit(o, long_m, keep_floor=True):
    """uniform scale so the longest side is long_m; centred in x/y, standing on z = 0 (his orientation kept)"""
    mn, mx = bounds([o]); k = long_m / max(1e-9, max(mx - mn))
    o.data.transform(Matrix.Diagonal((k, k, k, 1.0)))
    mn, mx = bounds([o]); c = (mn + mx) * 0.5
    o.data.transform(Matrix.Translation((-c.x, -c.y, -mn.z if keep_floor else -c.z)))
    o.data.update()
    return k

def owner_material(name, imgs, tint):
    """his model as the game shows it today (the 'before' of the review): his textures, nothing added"""
    m = bpy.data.materials.new(name); m.use_nodes = True; p = m.node_tree.nodes["Principled BSDF"]; t = NT(m.node_tree)
    oc = own_nodes(t, imgs, tint)
    t.set(p.inputs["Base Color"], oc["col"]); t.set(p.inputs["Roughness"], oc["rough"])
    if oc["nrm"] is not None: t.set(p.inputs["Normal"], oc["nrm"])
    return m

def own_nodes(t, imgs, tint):
    """sockets of his textures through his UVs: col, rough, nrm (a normal, or None), lum"""
    out = {}
    if imgs and imgs.get("C") and os.path.exists(imgs["C"]):
        uv = t.n("ShaderNodeUVMap"); uv.uv_map = "Own"
        def tex(path, data):
            n = t.n("ShaderNodeTexImage"); n.image = bpy.data.images.load(path, check_existing=True)
            if data: n.image.colorspace_settings.name = "Non-Color"
            t.set(n.inputs["Vector"], uv.outputs["UV"]); return n
        c = tex(imgs["C"], False); out["col"] = c.outputs["Color"]
        if tint: out["col"] = t.mixc(1.0, out["col"], tint, "MULTIPLY")
        if imgs.get("R") and os.path.exists(imgs["R"]):
            r = tex(imgs["R"], True); out["rough"] = _chan(t, r.outputs["Color"], imgs.get("rch", 0))
        else: out["rough"] = 0.5
        if imgs.get("N") and os.path.exists(imgs["N"]):
            n = tex(imgs["N"], True); nm = t.n("ShaderNodeNormalMap"); nm.uv_map = "Own"; nm.inputs["Strength"].default_value = 1.0
            t.set(nm.inputs["Color"], n.outputs["Color"]); out["nrm"] = nm.outputs["Normal"]
        else: out["nrm"] = None
    else:
        out = dict(col=t.rgb(tint or (0.8, 0.8, 0.8)), rough=0.5, nrm=None)
    out["lum"] = t.vmath("DOT_PRODUCT", out["col"], (0.2126, 0.7152, 0.0722), out=1)
    return out

def _chan(t, col, k):
    s = t.n("ShaderNodeSeparateColor"); t.set(s.inputs[0], col); return s.outputs[k]

# ================================================================ the hero material: his textures + the detail layers + the states
class Ctx:
    """what a detail layer can read: the object-space vector, his colour / roughness / luminance, the normalised height,
    the object-space normal, the food's size"""
    pass

def hero_mat(name, imgs, tint, detail, size, hmin=0.0, hmax=1.0, sss=0.0, sss_rad=(1.0, 0.45, 0.25), sss_scale=None, coat=0.0, coat_rough=0.08,
             sheen=0.0, bump=None, bump_str=0.7, trans=0.0, ior=1.45, nrm_str=1.0):
    """detail(t, c) -> (colour, roughness, height[, extra dict of socket overrides]) on top of c.col / c.rough"""
    m = bpy.data.materials.new(name); m.use_nodes = True
    p = m.node_tree.nodes["Principled BSDF"]; t = NT(m.node_tree)
    tc = t.n("ShaderNodeTexCoord")
    c = Ctx(); c.t = t; c.v = tc.outputs["Object"]; c.size = size
    own = own_nodes(t, imgs, tint)
    c.col, c.rough, c.lum = own["col"], own["rough"], own["lum"]
    x, y, z = t.sep(c.v); c.x, c.y, c.z = x, y, z
    c.h = t.mapr(z, hmin, hmax)                                       # 0 at the floor, 1 at the top
    geo = t.n("ShaderNodeNewGeometry")
    nob = t.n("ShaderNodeVectorTransform"); nob.vector_type = "NORMAL"; nob.convert_from = "WORLD"; nob.convert_to = "OBJECT"
    t.set(nob.inputs[0], geo.outputs["Normal"]); c.n = nob.outputs[0]; c.nx, c.ny, c.nz = t.sep(c.n)
    hsv = t.n("ShaderNodeSeparateColor"); hsv.mode = "HSV"; t.set(hsv.inputs[0], c.col); c.hue, c.sat, c.val = hsv.outputs[0], hsv.outputs[1], hsv.outputs[2]
    res = detail(t, c)
    col, rough, h = res[0], res[1], res[2]
    extra = res[3] if len(res) > 3 else {}
    gn = t.n("ShaderNodeGroup"); gn.node_tree = FV.states_group()
    t.set(gn.inputs["Color"], col); t.set(gn.inputs["Rough"], rough); t.set(gn.inputs["Height"], h); t.set(gn.inputs["Vector"], c.v)
    for k in FV.STATES:
        vn = t.n("ShaderNodeValue"); vn.name = "S_" + k; vn.outputs[0].default_value = 0.0; t.set(gn.inputs[k], vn.outputs[0])
    t.set(p.inputs["Base Color"], gn.outputs["Color"]); t.set(p.inputs["Roughness"], gn.outputs["Rough"])
    bp = t.n("ShaderNodeBump"); bp.inputs["Strength"].default_value = bump_str; bp.inputs["Distance"].default_value = bump or size * 0.004
    t.set(bp.inputs["Height"], gn.outputs["Height"])
    if own["nrm"] is not None:
        own["nrm"].node.inputs["Strength"].default_value = nrm_str
        t.set(bp.inputs["Normal"], own["nrm"])
    t.set(p.inputs["Normal"], bp.outputs["Normal"])
    p.inputs["Subsurface Weight"].default_value = sss; p.inputs["Subsurface Radius"].default_value = sss_rad
    p.inputs["Subsurface Scale"].default_value = sss_scale if sss_scale is not None else size * 0.05
    p.inputs["Coat Weight"].default_value = coat; p.inputs["Coat Roughness"].default_value = coat_rough
    p.inputs["Sheen Weight"].default_value = sheen; p.inputs["Transmission Weight"].default_value = trans; p.inputs["IOR"].default_value = ior
    for k, v in extra.items(): t.set(p.inputs[k], v)
    m["sss"] = sss; m["coat"] = coat
    return m

# ---------------------------------------------------------------- mask / look helpers (all sockets)
def cmask(t, col, ref, tol, soft=None):
    """1 where the colour is near ref (distance in linear RGB)"""
    d = t.vmath("DISTANCE", col, ref, out=1)
    return t.mapr(d, tol, tol + (soft or tol * 0.6), 1.0, 0.0)

def smoothband(t, h, a, b, soft=0.02):
    return t.mul(t.mapr(h, a - soft, a + soft, 0.0, 1.0), t.mapr(h, b - soft, b + soft, 1.0, 0.0))

def tone(t, col, k):
    """col times k (k a socket or number, 1 = unchanged)"""
    m = t.n("ShaderNodeMix", data_type="RGBA", blend_type="MULTIPLY"); t.set(m.inputs[0], 1.0); t.set(m.inputs[6], col)
    if isinstance(k, (int, float)): t.set(m.inputs[7], (k, k, k))
    else:
        cmb = t.n("ShaderNodeCombineColor"); t.set(cmb.inputs[0], k); t.set(cmb.inputs[1], k); t.set(cmb.inputs[2], k); t.set(m.inputs[7], cmb.outputs[0])
    return m.outputs[2]

def pores(t, v, scale, depth=1.0, wobble=0.25):
    """a foam of air cells (sponge, bread): 1 inside a pore, 0 on the crumb walls; the pore depth for the height"""
    sc = t.n("ShaderNodeVectorMath", operation="SCALE")
    t.set(sc.inputs[0], t.vmath("SUBTRACT", t.noise(v, scale * 0.25, 3.0, out="Color"), (0.5, 0.5, 0.5))); sc.inputs["Scale"].default_value = wobble / scale
    vd = t.vmath("ADD", v, sc.outputs[0])
    e1 = t.voronoi(vd, scale, "DISTANCE_TO_EDGE"); r1 = t.sep(t.voronoi(vd, scale, "F1", out="Color"))[0]
    e2 = t.voronoi(vd, scale * 0.37, "DISTANCE_TO_EDGE"); r2 = t.sep(t.voronoi(vd, scale * 0.37, "F1", out="Color"))[0]
    small = t.mul(t.mapr(e1, 0.04, 0.2, 0.0, 1.0, smooth=True), t.mapr(r1, 0.2, 0.7, 0.3, 1.0))
    big = t.mul(t.mapr(e2, 0.06, 0.24, 0.0, 1.0, smooth=True), t.mapr(r2, 0.78, 0.92, 0.0, 1.0))
    return t.math("MAXIMUM", small, big)

def specks(t, v, scale, amount, seed_off=(0, 0, 0)):
    """small round dots (lenticels, sugar crystals, sesame...): 1 on a dot; amount 0..1 = how many cells carry one"""
    vv = t.vmath("ADD", v, seed_off)
    d = t.voronoi(vv, scale, "F1"); r = t.sep(t.voronoi(vv, scale, "F1", out="Color"))[0]
    return t.mul(t.mapr(d, 0.06, 0.16, 1.0, 0.0, smooth=True), t.mapr(r, 1.0 - amount, 1.0 - amount + 0.02, 0.0, 1.0))

def crystals(t, v, scale):
    """faceted grains (sugar, salt, frost): grain edge mask and a per-grain random (sparkle)"""
    e = t.voronoi(v, scale, "DISTANCE_TO_EDGE"); r = t.sep(t.voronoi(v, scale, "F1", out="Color"))[0]
    return t.mapr(e, 0.0, 0.08, 1.0, 0.0), r

# ================================================================ added geometry
def flat_mat(name, col, rough=0.35, coat=0.0, sss=0.0, sheen=0.0):
    m = FV.food_mat(name, FV.flat_look(col, rough), sss=sss, coat=coat, sheen=sheen)
    return m

def glossy_mat(name, col, rough=0.15, coat=0.6, sss=0.3, var=0.12):
    return FV.food_mat(name, FV.glossy_look(col, rough, var), sss=sss, coat=coat, coat_rough=0.05)

def surface_points(o, n, seed, where):
    """n random points on the surface of o where where(co, normal, h) is true (area weighted): [(co, normal)]"""
    rnd = random.Random(seed)
    mn, mx = bounds([o]); H = max(1e-6, mx.z - mn.z)
    me = o.data; me.calc_loop_triangles()
    tri = []
    for lt in me.loop_triangles:
        cen = sum((me.vertices[i].co for i in lt.vertices), Vector()) / 3.0
        if where(cen, lt.normal, (cen.z - mn.z) / H): tri.append((lt, lt.area))
    if not tri: return []
    tot = sum(a for _, a in tri); out = []
    for _ in range(n):
        r = rnd.uniform(0, tot); acc = 0.0
        for lt, a in tri:
            acc += a
            if acc >= r: break
        a_, b_ = rnd.random(), rnd.random()
        if a_ + b_ > 1: a_, b_ = 1 - a_, 1 - b_
        p0, p1, p2 = (me.vertices[i].co for i in lt.vertices)
        out.append((p0 + (p1 - p0) * a_ + (p2 - p0) * b_, lt.normal.copy()))
    return out

def capsule(name, r, ln, at, nrm, yaw, mat):
    """a sprinkle: a rounded rod lying on the surface"""
    o = FV.cyl(name, r, ln, (0, 0, 0), 8, (0, math.pi / 2, 0))
    active(o); bpy.ops.object.transform_apply(rotation=True)
    b = o.modifiers.new("b", "BEVEL"); b.width = r * 0.8; b.segments = 2; FV.apply_mods(o)
    q = Vector((0, 0, 1)).rotation_difference(nrm)
    o.matrix_world = Matrix.Translation(at + nrm * r * 0.55) @ q.to_matrix().to_4x4() @ Matrix.Rotation(yaw, 4, "Z")
    active(o); bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    FV.assign(o, mat); return o

def drop_shape(name, r, ln, at, axis, mat, sub=2):
    """a seed / pip: an egg drawn out to a point"""
    o = FV.uvsphere(name, r, (0, 0, 0), 20, 12, (1, 1, 1))
    for v in o.data.vertices:
        f = min(1.0, max(0.0, (v.co.z / r + 1) * 0.5))                     # 0 at the round end, 1 at the tip
        k = 1.0 - 0.75 * f ** 1.6
        v.co.x *= k; v.co.y *= k * 0.62; v.co.z = (f - 0.5) * ln
    q = Vector((0, 0, 1)).rotation_difference(Vector(axis).normalized())
    o.matrix_world = Matrix.Translation(at) @ q.to_matrix().to_4x4()
    active(o); bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    FV.smooth(o, sub); FV.apply_mods(o); FV.assign(o, mat); return o

def ring(name, r, tube, z, mat, seg=64):
    bpy.ops.mesh.primitive_torus_add(major_radius=r, minor_radius=tube, major_segments=seg, minor_segments=10, location=(0, 0, z))
    o = bpy.context.object; o.name = name
    for p in o.data.polygons: p.use_smooth = True
    FV.assign(o, mat); return o

def own_colour_at(o, imgs, pred, default=(0.8, 0.8, 0.8), colpred=None):
    """the mean colour (linear) of his texture over the vertices where pred(co, h) holds: what a cut face of that layer looks like"""
    import numpy as np
    if not imgs or not imgs.get("C") or not os.path.exists(imgs["C"]): return default
    im = bpy.data.images.load(imgs["C"], check_existing=True)
    w, h = im.size; px = np.empty(w * h * 4, np.float32); im.pixels.foreach_get(px); px = px.reshape(h, w, 4)
    uv = o.data.uv_layers["Own"].data
    mn, mx = bounds([o]); H = max(1e-9, mx.z - mn.z)
    acc, n = np.zeros(3), 0
    for li, lp in enumerate(o.data.loops):
        co = o.data.vertices[lp.vertex_index].co
        if not pred(co, (co.z - mn.z) / H): continue
        u, v = uv[li].uv
        q = px[min(h - 1, max(0, int(v % 1.0 * h))), min(w - 1, max(0, int(u % 1.0 * w))), :3]
        if colpred and not colpred(q): continue
        acc += q; n += 1
    if n == 0: return default
    c = acc / n
    # (a byte image's pixels are its stored sRGB values: to linear, as the shader sees them)
    lin = lambda s: s / 12.92 if s <= 0.04045 else ((s + 0.055) / 1.055) ** 2.4
    return tuple(lin(float(x)) for x in c)

# ================================================================ THE OWNER'S FOOD: one spec per food of AntFood's table that has his model
# spec(o, imgs, tint, L) -> dict(detail, mat (hero_mat kwargs), extras(o) -> [objs], inner(t, v, h) -> (col, rough, h),
#                                cuts [(origin as a fraction of the size from the centre, normal, ragged amp / L)], jelly, prep(o))
OWN = {}
def own_food(fid, src, kind, cm, pieces, axis="z", label=""):
    def deco(fn):
        OWN[fid] = dict(src=src, kind=kind, size=cm / 100.0, pieces=pieces, axis=axis, spec=fn, label=label)
        return fn
    return deco

def sub_smooth(o, levels=2, disp=0.0, disp_scale=0.3, seed=0):
    """his low-poly blob (the generic Map4 shapes, 80-360 triangles) made round: subdivided, a gentle displacement, applied"""
    s = o.modifiers.new("sub", "SUBSURF"); s.levels = levels; s.render_levels = levels
    if disp:
        tx = bpy.data.textures.new("d%d" % seed, "CLOUDS"); tx.noise_scale = disp_scale; tx.noise_depth = 2
        d = o.modifiers.new("disp", "DISPLACE"); d.texture = tx; d.strength = disp; d.mid_level = 0.5; d.texture_coords = "OBJECT"
    FV.apply_mods(o)
    for p in o.data.polygons: p.use_smooth = True

# ---------------------------------------------------------------- CAKE: his Meshy slice (pink sponge, cream, glaze, a piped top)
@own_food("Cake", ("meshy", "SM_Food_CakePiece"), "Crumbly", 9.0, 4, label="Meshy SM_Food_CakePiece")
def cake(o, imgs, tint, L):
    B = dict(s1=(0.0, 0.27), cr=(0.27, 0.43), s2=(0.43, 0.66), gl=(0.66, 0.76))
    sponge = own_colour_at(o, imgs, lambda co, h: 0.05 < h < 0.24 or 0.47 < h < 0.62, (0.55, 0.16, 0.2))
    cream = own_colour_at(o, imgs, lambda co, h: 0.31 < h < 0.40, (0.85, 0.75, 0.72))
    glaze = own_colour_at(o, imgs, lambda co, h: 0.68 < h < 0.74, (0.75, 0.3, 0.38))
    top = own_colour_at(o, imgs, lambda co, h: h > 0.85, (0.9, 0.85, 0.85))
    log("cake colours", sponge, cream, glaze, top)
    def detail(t, c):
        v = c.v
        sp = t.add(smoothband(t, c.h, -0.2, B["s1"][1], 0.012), smoothband(t, c.h, *B["s2"], 0.012))
        cr = smoothband(t, c.h, *B["cr"], 0.012); gl = smoothband(t, c.h, *B["gl"], 0.012); tp = t.mapr(c.h, 0.74, 0.79)
        po = pores(t, v, 1500.0)                                                   # crumb: 0.7 mm cells, some 2 mm holes
        col = t.mixc(t.mul(sp, t.mul(po, 0.5)), c.col, tone(t, c.col, 0.42))       # the inside of a pore is in shadow
        col = t.mixc(t.mul(sp, t.mul(t.sub(1.0, po), 0.18)), col, (1.0, 0.86, 0.82))   # the crumb walls catch light
        rip = t.mul(t.wave(v, 300.0, "BANDS", 14.0, "Z"), t.mapr(t.noise(v, 80.0, 2.0), 0.35, 0.65))   # spread cream: knife ripples, broken up
        sw = t.wave(t.vmath("MULTIPLY", v, (1.0, 1.0, 0.35)), 520.0, "RINGS", 9.0)   # the piped top: swirl ridges
        fine = t.noise(v, 2400.0, 3.0)
        rough = t.mixf(sp, c.rough, t.mapr(po, 0.0, 1.0, 0.72, 0.95))
        rough = t.mixf(cr, rough, t.mapr(rip, 0.0, 1.0, 0.30, 0.42))
        rough = t.mixf(gl, rough, t.mapr(fine, 0.3, 0.7, 0.07, 0.16))           # glossy glaze
        rough = t.mixf(tp, rough, t.mapr(sw, 0.0, 1.0, 0.28, 0.42))
        h = t.add(t.mul(sp, t.mul(po, -1.0)), t.mul(cr, t.mul(rip, 0.35)))
        h = t.add(h, t.mul(tp, t.mul(sw, 0.6))); h = t.add(h, t.mul(gl, t.mul(fine, 0.08)))
        return col, rough, h, {"Coat Weight": t.add(t.mul(gl, 0.7), t.mul(tp, 0.15))}
    sprk = [glossy_mat("Sprinkle%d" % i, col, 0.22, 0.5, 0.2, 0.05) for i, col in enumerate(((0.42, 0.01, 0.07), (0.55, 0.03, 0.12), (0.92, 0.88, 0.86), (0.85, 0.35, 0.5)))]
    def extras(o):
        pts = surface_points(o, 50, 7, lambda co, n, h: h > 0.70 and n.z > 0.3)
        R = random.Random(11); out = []
        for i, (p, n) in enumerate(pts):
            out.append(capsule("Spr%d" % i, 0.00055, R.uniform(0.0026, 0.0038), p, n, R.uniform(0, math.pi), sprk[0 if i % 5 < 2 else (1 if i % 5 == 2 else (2 if i % 5 == 3 else 3))]))
        return out
    def inner(t, v, h):
        po = pores(t, v, 1500.0)
        spc = t.mixc(t.mul(po, 0.5), sponge, tuple(x * 0.42 for x in sponge))
        crm = t.mixc(t.mapr(t.voronoi(v, 400.0, "F1"), 0.0, 0.12, 0.8, 0.0), cream, (0.55, 0.05, 0.12))   # cream with berry bits (his has them)
        cr = smoothband(t, h, *B["cr"], 0.01); gl = smoothband(t, h, *B["gl"], 0.01); tp = t.mapr(h, 0.75, 0.78)
        col = t.mixc(cr, spc, crm); col = t.mixc(gl, col, glaze); col = t.mixc(tp, col, top)
        rough = t.mixf(t.add(t.add(cr, gl), tp, True), 0.88, 0.35)
        hh = t.mixf(t.add(t.add(cr, gl), tp, True), t.mul(po, -1.0), 0.0)
        return col, rough, hh
    return dict(detail=detail, mat=dict(sss=0.12, sss_rad=(1.0, 0.35, 0.3), bump_str=0.8), extras=extras, inner=inner,
                cuts=[((0, 0, 0), (0.08, 1.0, 0.1), 0.018), ((0, 0, -0.07), (0.1, 0.05, 1.0), 0.014)])   # along its length, then the top off at the cream

# ---------------------------------------------------------------- APPLE: his slice (Map4, from his Higgsfield apple.glb)
@own_food("Apple", ("m4", "SM_M4_AppleSlice"), "Juicy", 6.0, 2, label="SM_M4_AppleSlice (Higgsfield)")
def apple(o, imgs, tint, L):
    flesh = own_colour_at(o, imgs, lambda co, h: 0.2 < h < 0.8, (0.75, 0.7, 0.45), lambda q: q.max() - q.min() < 0.25 and q.mean() > 0.55)   # pale pixels only: the flesh, not the skin
    def detail(t, c):
        v = c.v
        skin = t.mapr(c.sat, 0.38, 0.6)
        seed = t.mapr(c.val, 0.16, 0.08)
        fl = t.mul(t.sub(1.0, skin), t.sub(1.0, seed))
        lent = specks(t, v, 700.0, 0.45)                                           # lenticels: pale dots in the skin
        streak = t.noise(t.vmath("MULTIPLY", v, (1.0, 1.0, 0.12)), 160.0, 4.0)       # streaks along the apple
        col = t.mixc(t.mul(skin, t.mul(t.sub(streak, 0.45), 0.6)), c.col, tone(t, c.col, (1.35)))
        col = t.mixc(t.mul(skin, t.mul(lent, 0.7)), col, (0.95, 0.80, 0.45))
        fib = t.noise(t.vmath("MULTIPLY", v, (1.0, 1.0, 0.15)), 900.0, 3.0)          # flesh: fibres
        cell = t.voronoi(v, 4200.0, "DISTANCE_TO_EDGE")                              # and the cells, wet
        col = t.mixc(t.mul(fl, t.mul(t.sub(fib, 0.5), 0.5)), col, (1.0, 0.98, 0.85))
        col = t.mixc(t.mul(fl, t.mapr(cell, 0.0, 0.06, 0.25, 0.0)), col, tone(t, c.col, 0.85))
        rough = t.mixf(skin, c.rough, t.mapr(streak, 0.3, 0.7, 0.26, 0.38))
        rough = t.mixf(fl, rough, t.mapr(cell, 0.0, 0.1, 0.12, 0.3))             # juicy: wet glints between the cells
        rough = t.mixf(seed, rough, 0.18)
        h = t.add(t.mul(skin, t.mul(lent, 0.25)), t.mul(fl, t.add(t.mul(fib, 0.3), t.mapr(cell, 0.0, 0.06, -0.4, 0.0))))
        return col, rough, h, {"Coat Weight": t.mul(skin, 0.45)}
    pip = glossy_mat("Pip", (0.14, 0.06, 0.02), 0.18, 0.7, 0.2, 0.2)
    def extras(o):
        import numpy as np
        # his pip is painted in the core: put a real one there (the darkest patch of his texture, along its long axis)
        im = bpy.data.images.load(imgs["C"], check_existing=True); w, hh = im.size
        px = np.empty(w * hh * 4, np.float32); im.pixels.foreach_get(px); px = px.reshape(hh, w, 4)
        uv = o.data.uv_layers["Own"].data; pts = []; nrm = Vector(); zmn, zmx = bounds([o]); zcut = zmn.z + (zmx.z - zmn.z) * 0.78
        for li, lp in enumerate(o.data.loops):
            u, vv = uv[li].uv; q = px[min(hh - 1, int(vv % 1 * hh)), min(w - 1, int(u % 1 * w)), :3]
            if q.mean() < 0.12 and o.data.vertices[lp.vertex_index].co.z < zcut:   # (not the stem: that is dark too, at the top)
                pts.append(o.data.vertices[lp.vertex_index].co.copy()); nrm += o.data.vertices[lp.vertex_index].normal
        if len(pts) < 8: log("apple: no dark pip patch"); return []
        P = np.array([tuple(p) for p in pts]); cen = P.mean(0); cov = np.cov((P - cen).T); ev, evec = np.linalg.eigh(cov)
        ax = Vector(evec[:, -1]); ln = max(0.006, min(0.011, 2.2 * math.sqrt(ev[-1]) * 2))
        n = nrm.normalized()
        log("apple pip at", cen, "len", ln, "dark verts", len(pts))
        return [drop_shape("Pip", ln * 0.32, ln, Vector(cen) + n * 0.0006, ax, pip)]
    def inner(t, v, h):
        fib = t.noise(t.vmath("MULTIPLY", v, (1.0, 1.0, 0.15)), 900.0, 3.0); cell = t.voronoi(v, 4200.0, "DISTANCE_TO_EDGE")
        col = t.mixc(t.mul(t.sub(fib, 0.5), 0.5), flesh, (1.0, 0.98, 0.85)); col = t.mixc(t.mapr(cell, 0.0, 0.06, 0.25, 0.0), col, tuple(x * 0.85 for x in flesh))
        col = t.mixc(t.mapr(h, 0.93, 0.97), col, (0.45, 0.03, 0.03))             # the skin at the edge of the cut
        return col, t.mapr(cell, 0.0, 0.1, 0.12, 0.3), t.add(t.mul(fib, 0.3), t.mapr(cell, 0.0, 0.06, -0.4, 0.0))
    return dict(detail=detail, mat=dict(sss=0.25, sss_rad=(1.0, 0.9, 0.6)), extras=extras, inner=inner,
                cuts=[((0.05, 0, 0), (1.0, 0.15, 0.05), 0.010)])

# ================================================================ PIPELINE (after the specs: food_hero_more.py is exec'd at the end)
def world_for_bake(L):
    if not S().world:
        wd = bpy.data.worlds.new("BakeWorld"); S().world = wd; wd.use_nodes = True
        wd.node_tree.nodes["Background"].inputs["Color"].default_value = (1, 1, 1, 1)
    S().world.light_settings.distance = max(0.004, L * 0.12)          # occlusion within an eighth of the food: creases, pores, under sprinkles

def metal_off(o):
    """a metal has no diffuse colour (it bakes black): metallic off while the colour bakes; returns the undo"""
    undo = []
    for s in o.material_slots:
        m = s.material; p = m.node_tree.nodes.get("Principled BSDF") if m and m.use_nodes else None
        if not p: continue
        lk = [(l.from_socket, l.to_socket) for l in m.node_tree.links if l.to_socket == p.inputs["Metallic"]]
        for l in [l for l in m.node_tree.links if l.to_socket == p.inputs["Metallic"]]: m.node_tree.links.remove(l)
        undo.append((m, p, p.inputs["Metallic"].default_value, lk, p.inputs["Transmission Weight"].default_value))
        p.inputs["Metallic"].default_value = 0.0
        p.inputs["Transmission Weight"].default_value = 0.0     # (glass - the ice - bakes dark too: its own colour, the game draws it opaque)
    return undo

def metal_on(undo):
    for m, p, v, lk, tw in undo:
        p.inputs["Metallic"].default_value = v; p.inputs["Transmission Weight"].default_value = tw
        for a, b in lk: m.node_tree.links.new(a, b)

def bake_hero(o, fid, d, L):
    """T_FoodHero_<fid>_C / N / R (2048) and the four states (1024, M_FoodV2 format)"""
    t0 = time.time()
    p = lambda s: os.path.join(d, "T_FoodHero_%s_%s.png" % (fid, s))
    world_for_bake(L)
    FV.set_state([o], **{k: 0.0 for k in FV.STATES})
    u = metal_off(o)
    c = FV.new_img("C", TEX); FV.bake_pass(o, "DIFFUSE", c, samples=32, filt={"COLOR"}); FV.save_png(c, p("C"))
    metal_on(u)
    nimg = FV.new_img("N", TEX); nimg.colorspace_settings.name = "Non-Color"; FV.bake_pass(o, "NORMAL", nimg, samples=8); FV.save_png(nimg, p("N"))
    ao = FV.new_img("AO", TEX); ao.colorspace_settings.name = "Non-Color"; FV.bake_pass(o, "AO", ao, samples=64)
    ro = FV.new_img("RO", TEX); ro.colorspace_settings.name = "Non-Color"; FV.bake_pass(o, "ROUGHNESS", ro, samples=4)
    FV.compose(p("R"), TEX, None, chans=[(FV.pix(ao), 0), (FV.pix(ro), 0), (None, 0)])
    for st, vals in FV.GAME_STATES:
        FV.set_state([o], **{k: 0.0 for k in FV.STATES}); FV.set_state([o], **vals)
        u = metal_off(o)
        sc = FV.new_img("SC", STEX); FV.bake_pass(o, "DIFFUSE", sc, samples=8, filt={"COLOR"})
        metal_on(u)
        sr = FV.new_img("SR", STEX); sr.colorspace_settings.name = "Non-Color"; FV.bake_pass(o, "ROUGHNESS", sr, samples=2)
        FV.compose(p(st), STEX, FV.pix(sc), a_from=FV.pix(sr))
    FV.set_state([o], **{k: 0.0 for k in FV.STATES})
    log(fid, "baked %.0f s" % (time.time() - t0))
    return {k: p(k) for k in ("C", "N", "R")}

def inner_strip(fid, d, inner_fn, mn, mx, axis):
    """the inner look on a sheet through the food: U along the cut, V = height (or radius): T_FoodHero_<fid>_InC"""
    ctr = (mn + mx) * 0.5; size = mx - mn
    bpy.ops.mesh.primitive_plane_add(size=1.0); pl = bpy.context.object
    pl.scale = (size.x, 1.0, size.z if axis == "z" else size.x * 0.5)
    pl.rotation_euler = (math.pi / 2, 0, 0); pl.location = (ctr.x, ctr.y, ctr.z)
    active(pl); bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    def strip(t, v):
        uv = t.n("ShaderNodeTexCoord").outputs["UV"]
        return inner_fn(t, v, t.sep(uv)[1])
    FV.assign(pl, FV.food_mat("InnerStrip_" + fid, strip, bump=0.0003))
    ic = FV.new_img("IC", STEX); FV.bake_pass(pl, "DIFFUSE", ic, samples=8, filt={"COLOR"}); FV.save_png(ic, os.path.join(d, "T_FoodHero_%s_InC.png" % fid))
    bpy.data.objects.remove(pl, do_unlink=True)

def keep_uv0(o):
    for u in [u for u in o.data.uv_layers if u.name != "UV0"]: o.data.uv_layers.remove(u)

def export_lod0(o, fid, path, hulls):
    """the whole food centred, slot Outer, with its UCX_ hulls (same centre); returns the centre"""
    keep_name = o.name; o.name = keep_name + "_src"
    c = dup(o, "SM_FoodHero_%s" % fid); keep_uv0(c)        # (UE pairs UCX_<name>_NN with the mesh of exactly that name in the FBX)
    o.name = keep_name if keep_name != c.name else keep_name + "_w"
    c.data.materials.clear(); c.data.materials.append(FV.slot_mat("Outer"))
    for p in c.data.polygons: p.material_index = 0
    mn, mx = bounds([c]); ctr = (mn + mx) * 0.5
    c.data.transform(Matrix.Translation(-ctr))
    hs = []
    for k, pts in enumerate(hulls):
        bm = bmesh.new()
        for q in pts: bm.verts.new(Vector(q) - ctr)
        bmesh.ops.convex_hull(bm, input=bm.verts[:])
        me = bpy.data.meshes.new("UCX"); bm.to_mesh(me); bm.free()
        h = link(bpy.data.objects.new("UCX_SM_FoodHero_%s_%02d" % (fid, k), me)); hs.append(h)
    for x in S().objects: x.select_set(False)
    for x in [c] + hs: x.select_set(True)
    bpy.context.view_layer.objects.active = c
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, use_mesh_modifiers=True, mesh_smooth_type="FACE",
                             bake_anim=False, add_leaf_bones=False, path_mode="STRIP", embed_textures=False, use_tspace=True)
    log("FBX", os.path.basename(path), tris(c), "tris,", len(hs), "UCX")
    for x in [c] + hs: bpy.data.objects.remove(x, do_unlink=True)
    o.name = keep_name
    return ctr

def export_at(o, path, ctr):
    """a LOD: slot Outer, moved by the LOD0's centre (not its own: LOD2 has no sprinkles and a smaller box)"""
    c = dup(o, o.name + "_x"); keep_uv0(c)
    c.data.materials.clear(); c.data.materials.append(FV.slot_mat("Outer"))
    for p in c.data.polygons: p.material_index = 0
    c.data.transform(Matrix.Translation(-Vector(ctr)))
    FV.export_fbx(c, path); bpy.data.objects.remove(c, do_unlink=True)

def export_plain(o, path, inner_prefix=None):
    c = dup(o, o.name + "_x"); keep_uv0(c)
    ctr, size = FV.export_copy(c, path, inner_prefix)
    bpy.data.objects.remove(c, do_unlink=True)
    return ctr, size

def hull_sets(o, k_split=1):
    """UCX point sets: the whole food's vertices, or split into k slabs along its long axis (concave things: a worm, a bear)"""
    vs = [v.co.copy() for v in o.data.vertices]
    if k_split <= 1: return [vs]
    mn, mx = bounds([o]); ax = max(range(3), key=lambda i: mx[i] - mn[i]); span = (mx[ax] - mn[ax]) / k_split
    out = []
    for i in range(k_split):
        a, b = mn[ax] + i * span - span * 0.08, mn[ax] + (i + 1) * span + span * 0.08
        s = [v for v in vs if a <= v[ax] <= b]
        if len(s) >= 4: out.append(s)
    return out

def lod_copy(o, ratio, name, drop_extras=False):
    """LOD1 / LOD2 (the Human's distance): decimated; the far one without the small added bits (sprinkles, crystals)"""
    c = dup(o, name); keep_uv0(c)
    n0 = tris(c)
    if drop_extras and "part" in c.data.attributes:
        bm = bmesh.new(); bm.from_mesh(c.data); lay = bm.faces.layers.int.get("part")
        ex = [f for f in bm.faces if f[lay] == 1]
        if ex and len(ex) < len(bm.faces) * 0.8: bmesh.ops.delete(bm, geom=ex, context="FACES")
        bm.to_mesh(c.data); bm.free()
    want = max(200, int(n0 * ratio))
    if tris(c) > want:
        d = c.modifiers.new("dec", "DECIMATE"); d.decimate_type = "COLLAPSE"; d.ratio = want / float(tris(c)); d.use_collapse_triangulate = True
        FV.apply_mods(c)
    return c

def ensure_own_uv(o):
    if not o.data.uv_layers: o.data.uv_layers.new(name="Own")
    o.data.uv_layers[0].name = "Own"
    while len(o.data.uv_layers) > 1: o.data.uv_layers.remove(o.data.uv_layers[1])

def finish_whole(parts, fid):
    """join, keep to the LOD0 budget, the UV0 atlas (the bake target; his UVs stay in 'Own' for the bake only)"""
    for p in parts:
        FV.apply_mods(p); ensure_own_uv(p)
    # the budget: his mesh is kept as it is; the added bits (sprinkles, pips...) are thinned to what is left
    base, extra = parts[0], parts[1:]
    left = LOD0_MAX - 300 - tris(base)
    et = sum(tris(e) for e in extra)
    if extra and et > max(600, left):
        r = max(600, left) / float(et)
        for e in extra:
            if tris(e) > 40:
                d = e.modifiers.new("dec", "DECIMATE"); d.ratio = max(0.04, r); FV.apply_mods(e)
        log(fid, "extras thinned", et, "->", sum(tris(e) for e in extra))
    for k, p in enumerate(parts):
        a = p.data.attributes.get("part") or p.data.attributes.new("part", "INT", "FACE")
        for i in range(len(p.data.polygons)): a.data[i].value = 0 if k == 0 else 1
    whole = FV.join(parts, "SM_FoodHero_" + fid) if len(parts) > 1 else parts[0]
    whole.name = "SM_FoodHero_" + fid
    for p in whole.data.polygons: p.use_smooth = True
    if tris(whole) > LOD0_MAX:
        d = whole.modifiers.new("dec", "DECIMATE"); d.decimate_type = "COLLAPSE"; d.ratio = (LOD0_MAX - 200) / float(tris(whole)); d.use_collapse_triangulate = True
        n0 = tris(whole); FV.apply_mods(whole); log(fid, "LOD0 decimated", n0, "->", tris(whole))
    # triangles out of Blender: the tangents the normal map is baked against are the ones the game gets
    bm = bmesh.new(); bm.from_mesh(whole.data)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3], quad_method="BEAUTY", ngon_method="BEAUTY")
    bm.to_mesh(whole.data); bm.free()
    uv0 = whole.data.uv_layers.new(name="UV0"); whole.data.uv_layers.active = uv0
    FV.unwrap(whole, 0.004)
    whole.data.uv_layers.active = whole.data.uv_layers["UV0"]
    return whole

def run_own(fid):
    FV.reset()
    F = OWN[fid]; L = F["size"]
    o, imgs, tint = load_owner(F["src"])
    fit(o, L)
    spec = F["spec"](o, imgs, tint, L)
    if spec.get("prep"):
        spec["prep"](o); fit(o, L)
    mn, mx = bounds([o])
    FV.assign(o, hero_mat("Hero_" + fid, imgs, tint, spec["detail"], L, mn.z, mx.z, **spec.get("mat", {})))
    extras = spec["extras"](o) if spec.get("extras") else []
    whole = finish_whole([o] + extras, fid)
    return produce(fid, F, spec, whole, L, imgs, tint)

def plane_pieces(whole, plan, inner):
    """the pieces by plane cuts: each piece is bisected, the open cut filled (triangle fill) and given the inner material;
    works on skins that are not closed, where a boolean drops parts"""
    pieces = [dup(whole, "pp")]
    if inner.name not in [m.name for m in whole.data.materials if m]:
        pieces[0].data.materials.append(inner)
    for (co, no, amp, sc) in plan:
        co, no = Vector(co), Vector(no).normalized(); nxt = []
        for pc in pieces:
            for side in (0, 1):
                o = dup(pc, pc.name + ("a" if side == 0 else "b"))
                ii = [m.name for m in o.data.materials].index(inner.name)
                bm = bmesh.new(); bm.from_mesh(o.data)
                r = bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], dist=1e-7, plane_co=co, plane_no=no,
                                           clear_inner=(side == 0), clear_outer=(side == 1))
                cut = [g for g in r["geom_cut"] if isinstance(g, bmesh.types.BMEdge)]
                if cut:
                    try:
                        f = bmesh.ops.triangle_fill(bm, use_beauty=True, use_dissolve=False, edges=cut, normal=(no if side == 1 else -no))
                        for fc in f["geom"]:
                            if isinstance(fc, bmesh.types.BMFace): fc.material_index = ii; fc.smooth = True
                    except Exception as e:
                        log("plane cut fill failed", e)
                n = len(bm.faces); bm.to_mesh(o.data); bm.free()
                if n > 30: nxt.append(o)
                else: bpy.data.objects.remove(o, do_unlink=True)
            bpy.data.objects.remove(pc, do_unlink=True)
        pieces = nxt
    pieces.sort(key=lambda o: -sum(p.area for p in o.data.polygons))
    return pieces

def produce(fid, F, spec, whole, L, imgs=None, tint=None):
    mn, mx = bounds([whole]); size = mx - mn
    log(fid, "size cm", ["%.2f" % (s * 100) for s in size], "LOD0 tris", tris(whole))
    axis = F.get("axis", "z")
    hmn, hmx = Vector(spec.get("hmin", mn)), Vector(spec.get("hmax", mx))
    inner = FV.food_mat("Inner_" + fid, lambda t, v: spec["inner"](t, v, FV.height_of(t, v, axis, hmn, hmx)), bump=L * 0.003, bump_str=0.7,
                        sss=spec.get("inner_sss", 0.0))
    d = os.path.join(OUT_ART, fid); os.makedirs(d, exist_ok=True)
    ctr = (mn + mx) * 0.5
    plan = [(tuple(ctr + Vector((o_[0] * size.x, o_[1] * size.y, o_[2] * size.z))), n_, a_ * L, L * 0.07) for (o_, n_, a_) in spec.get("cuts", [])]
    if spec.get("cuts_abs"):                       # (the pass-1 foods: offsets from the centre in metres, their own ragged amp / scale)
        plan = [(tuple(ctr + Vector(o_)), n_, a_, sc_) for (o_, n_, a_, sc_) in spec["cuts_abs"]]
    pieces = []
    if F["pieces"] > 0 and plan:
        # his mesh and the added bits (sprinkles, pips, rims) are cut as separate parts: a bit wholly on one side of a cut goes
        # with that piece uncut, and a failed boolean on his mesh cannot take the sprinkles with it
        src = []
        for pid in (0, 1):
            s = dup(whole, "cutsrc%d" % pid)
            if "part" in s.data.attributes:
                bm = bmesh.new(); bm.from_mesh(s.data); lay = bm.faces.layers.int.get("part")
                bmesh.ops.delete(bm, geom=[f for f in bm.faces if f[lay] != pid], context="FACES"); bm.to_mesh(s.data); bm.free()
            elif pid == 1:
                s.data.clear_geometry()
            if len(s.data.polygons): src.append(s)
            else: bpy.data.objects.remove(s, do_unlink=True)
        pieces = FV.pieces_from_parts(src, plan, inner, len(fid))[:F["pieces"]]
        for s in src: bpy.data.objects.remove(s, do_unlink=True)
        # a boolean on a skin that is not closed (some of his Meshy exports) loses whole parts: if the pieces do not add up to
        # the food, cut it by planes instead (straight cut, the cut capped with the inner face) - nothing is lost that way
        outer_area = lambda o: sum(p.area for p in o.data.polygons if not (o.material_slots[p.material_index].material and o.material_slots[p.material_index].material.name.startswith("Inner_")))
        got, want = sum(outer_area(p) for p in pieces), outer_area(whole)
        if len(pieces) < F["pieces"] or got < want * 0.9:
            log(fid, "boolean pieces lost %.0f %% of the skin (%d pieces): cutting by planes" % (100 * (1 - got / max(1e-12, want)), len(pieces)))
            for p in pieces: bpy.data.objects.remove(p, do_unlink=True)
            pieces = plane_pieces(whole, plan, inner)[:F["pieces"]]
        for k, pc in enumerate(pieces): pc.name = "SM_FoodHero_%s_P%d" % (fid, k)
        if len(pieces) != F["pieces"]: log("WARNING", fid, "pieces", len(pieces), "wanted", F["pieces"])
        for pc in pieces:                          # (the cutter's grid is flat-shaded: smooth, or the cut shows its squares)
            for p_ in pc.data.polygons: p_.use_smooth = True
    paths = {k: os.path.join(d, "T_FoodHero_%s_%s.png" % (fid, k)) for k in "CNR"}
    man = dict(id=fid, set="hero", kind=F["kind"], owner=F.get("label", ""), size_m=[size.x, size.y, size.z], jelly=spec.get("jelly", 0.0),
               coat=spec.get("coat", 0.0), inner_axis=axis, pieces=[])
    if MODE in ("model", "all"):
        paths = bake_hero(whole, fid, d, L)
        inner_strip(fid, d, spec["inner"], hmn, hmx, axis)
        c0 = export_lod0(whole, fid, os.path.join(d, "SM_FoodHero_%s.fbx" % fid), hull_sets(whole, spec.get("hulls", 1)))
        lods = [tris(whole)]
        for k, r in ((1, 0.45), (2, 0.18)):
            lc = lod_copy(whole, r, "SM_FoodHero_%s_LOD%d" % (fid, k), k == 2); lods.append(tris(lc))
            export_at(lc, os.path.join(d, lc.name + ".fbx"), c0); bpy.data.objects.remove(lc, do_unlink=True)
        man.update(centre_m=list(c0), lod_triangles=lods, ucx=len(hull_sets(whole, spec.get("hulls", 1))),
                   h_range_m=[hmn.z - c0.z, hmx.z - c0.z] if axis == "z" else [0.0, max(size.x, size.y) * 0.5])
        for pc in pieces:
            if tris(pc) > LOD0_MAX:
                dd = pc.modifiers.new("dec", "DECIMATE"); dd.ratio = (LOD0_MAX - 200) / float(tris(pc)); FV.apply_mods(pc)
            pcc, pcs = export_plain(pc, os.path.join(d, pc.name + ".fbx"), "Inner_")
            man["pieces"].append(dict(name=pc.name, centre_m=list(pcc - c0), size_m=list(pcs), tris=tris(pc)))
        with open(os.path.join(d, "food.json"), "w", encoding="utf-8") as f:
            json.dump(man, f, indent=1, ensure_ascii=False)
    if MODE in ("review", "all"):
        review(fid, F, spec, whole, pieces, inner, paths, L, imgs, tint)
    return man

# ================================================================ REVIEW: one sheet per food
_ANT = {}
def make_ant(at, yaw=0.0):
    """the game's ant (his Meshy SM_Ant_Body) at its real size, 15 uu = 7.9 mm, with six thin legs: the scale of the food"""
    f = (glob.glob(os.path.join(MESHY, "SM_Ant_Body", "*", "*.fbx")) or [None])[0]
    parts = []
    if f:
        before = set(bpy.data.objects); bpy.ops.import_scene.fbx(filepath=f)
        new = [o for o in bpy.data.objects if o not in before]
        for o in new:
            if o.type != "MESH": bpy.data.objects.remove(o, do_unlink=True)
        body = [o for o in bpy.data.objects if o not in before and o.type == "MESH"][0]
        active(body); bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        png = (glob.glob(os.path.join(os.path.dirname(f), "*_texture.png")) or [None])[0]
        m = bpy.data.materials.new("Ant"); m.use_nodes = True; p = m.node_tree.nodes["Principled BSDF"]
        if png:
            tx = m.node_tree.nodes.new("ShaderNodeTexImage"); tx.image = bpy.data.images.load(png, check_existing=True)
            m.node_tree.links.new(tx.outputs[0], p.inputs["Base Color"])
        p.inputs["Roughness"].default_value = 0.35; p.inputs["Coat Weight"].default_value = 0.4
        FV.assign(body, m)
        mn, mx = bounds([body]); ln = max(mx.x - mn.x, mx.y - mn.y); k = ANT_LEN / ln
        body.data.transform(Matrix.Diagonal((k, k, k, 1)))
        if (mx.y - mn.y) > (mx.x - mn.x): body.data.transform(Matrix.Rotation(math.pi / 2, 4, "Z"))
        mn, mx = bounds([body]); c = (mn + mx) * 0.5
        lift = ANT_LEN * 0.16
        body.data.transform(Matrix.Translation((-c.x, -c.y, -mn.z + lift)))
        parts.append(body)
        legm = bpy.data.materials.new("AntLeg"); legm.use_nodes = True; lp = legm.node_tree.nodes["Principled BSDF"]
        lp.inputs["Base Color"].default_value = (0.03, 0.02, 0.015, 1); lp.inputs["Roughness"].default_value = 0.4
        A = ANT_LEN
        for side in (-1, 1):
            for k_, x0 in enumerate((0.12, 0.0, -0.12)):
                sw = (k_ - 1) * 0.35
                pts = [(x0 * A, side * 0.06 * A, lift + 0.12 * A), ((x0 + sw * 0.25) * A, side * 0.30 * A, lift + 0.22 * A),
                       ((x0 + sw * 0.55) * A, side * 0.48 * A, 0.0004)]
                lg = FV.tube("leg", pts, 0.012 * A, 8, True); FV.assign(lg, legm); parts.append(lg)
    else:
        parts.append(FV.uvsphere("ant", ANT_LEN * 0.3, (0, 0, ANT_LEN * 0.3), 16, 8))
    ant = FV.join(parts, "Ant") if len(parts) > 1 else parts[0]
    ant.rotation_euler = (0, 0, yaw); ant.location = at
    return ant

def baked_material(name, paths, spec):
    """what the game gets: the baked C / N / R (+ the jelly subsurface M_FoodV2 has)"""
    m = bpy.data.materials.new("Baked_" + name); m.use_nodes = True; p = m.node_tree.nodes["Principled BSDF"]; t = NT(m.node_tree)
    uv = t.n("ShaderNodeUVMap"); uv.uv_map = "UV0"
    def tex(k, data):
        n = t.n("ShaderNodeTexImage"); n.image = bpy.data.images.load(paths[k], check_existing=True)
        if data: n.image.colorspace_settings.name = "Non-Color"
        t.set(n.inputs["Vector"], uv.outputs["UV"]); return n
    c = tex("C", False); r = tex("R", True); n = tex("N", True)
    sep = t.n("ShaderNodeSeparateColor"); t.set(sep.inputs[0], r.outputs["Color"])
    t.set(p.inputs["Base Color"], t.mixc(0.45, c.outputs["Color"], t.mixc(1.0, c.outputs["Color"], sep.outputs[0], "MULTIPLY")))   # (as M_FoodV2: AO 45 %)
    t.set(p.inputs["Roughness"], sep.outputs[1])
    nm = t.n("ShaderNodeNormalMap"); nm.uv_map = "UV0"; t.set(nm.inputs["Color"], n.outputs["Color"]); t.set(p.inputs["Normal"], nm.outputs["Normal"])
    j = spec.get("jelly", 0.0)
    if j > 0:
        p.inputs["Subsurface Weight"].default_value = j; p.inputs["Subsurface Scale"].default_value = 0.004; p.inputs["Coat Weight"].default_value = 0.4
    return m

def cam(loc, look, lens, fstop):
    return FV.camera(loc, look, lens, fstop)

def clear_scene_except(keep):
    for o in list(S().objects):
        if o not in keep: bpy.data.objects.remove(o, do_unlink=True)

def frame(objs, w, h, lens=35.0, d=(0.0, -1.0, 0.42), margin=1.12, fstop=8.0, look_z=0.45):
    """a camera that fits these objects in a w x h picture, looking along -d"""
    mn, mx = bounds(objs); c = (mn + mx) * 0.5; ext = mx - mn
    dv = Vector(d).normalized()
    right = Vector((0, 0, 1)).cross(-dv).normalized() if abs(dv.z) < 0.99 else Vector((1, 0, 0))
    up = (-dv).cross(right).normalized()
    pts = [Vector((x, y, z)) for x in (mn.x, mx.x) for y in (mn.y, mx.y) for z in (mn.z, mx.z)]
    hw = max(abs((p - c).dot(right)) for p in pts) * margin
    hh = max(abs((p - c).dot(up)) for p in pts) * margin
    tx = 18.0 / lens; ty = 18.0 * h / w / lens
    dist = max(hw / tx, hh / ty) + max(abs((p - c).dot(dv)) for p in pts)
    look = Vector((c.x, c.y, mn.z + ext.z * look_z))
    return cam(look + dv * dist, look, lens, fstop)

def review(fid, F, spec, whole, pieces, inner, paths, L, imgs, tint):
    for o in list(S().objects): o.hide_render = True
    FV.stage(span=L * 2.6)
    S().render.resolution_percentage = 100
    shots = []
    fs = 11.0 * max(1.0, 0.06 / L)
    keep = lambda: [whole] + pieces + [o for o in S().objects if o.type == "LIGHT" or o.name.startswith(("Plane", "Floor", "Cube"))]
    base_objs = set(S().objects)
    # ---- A: his model | the hero version (baked, what the game gets) + the ant
    made = []
    if F.get("src"):
        own, oi, ot = load_owner(F["src"]); fit(own, L); FV.assign(own, owner_material("OwnerLook_" + fid, oi, ot))
        what = "твоя модель — " + F.get("label", F["src"][1])
    else:
        own = FV.cube("ph", (0.45 * L, 0.45 * L, 0.45 * L)); FV.assign(own, flat_mat("ph", (0.6, 0.6, 0.6), 0.6)); what = "твоей модели не было — новая еда"
    own.hide_render = False
    hero = dup(whole, "hero"); hero.hide_render = False
    if os.path.exists(paths["C"]): FV.assign(hero, baked_material(fid, paths, spec))
    wmn, wmx = bounds([whole]); gap = max(wmx.x - wmn.x, wmx.y - wmn.y) * 0.62 + 0.08 * L
    FV.place(own, -gap, 0.0, math.radians(-28)); FV.place(hero, gap, 0.0, math.radians(-28)); made += [own, hero]
    hmn, hmx = bounds([hero])
    made.append(make_ant((hmx.x + 0.06 * L, hmn.y - 0.02 * L, 0.0), math.radians(150)))
    ty = min(bounds([own])[0].y, hmn.y) - 0.12 * L
    made.append(FV.text(what, (-gap, ty, 0.0003), 0.065 * L))
    made.append(FV.text("hero-версия той же еды (в игре — эти текстуры)", (gap, ty, 0.0003), 0.065 * L))
    made.append(frame(made, 1800, 1000, 38, (0.0, -1.0, 0.36), 1.06, fs, 0.4))
    a = os.path.join(WORK, "%s_A.jpg" % fid); FV.render(a, 1800, 1000, SAMPLES); shots.append(a)
    # ---- A2: the ant's eye on the hero version (low, close, the ant in front)
    own.hide_render = True
    ant2 = make_ant((hmx.x - 0.05 * L, hmn.y - 0.16 * L, 0.0), math.radians(100)); made.append(ant2)
    hc = (hmn + hmx) * 0.5
    made.append(cam((hc.x + 0.35 * L, hmn.y - 0.55 * L, 0.012 + 0.02 * L), (hc.x, hc.y, (hmn.z + hmx.z) * 0.4), 22, fs * 0.5))
    a2 = os.path.join(WORK, "%s_A2.jpg" % fid); FV.render(a2, 900, 700, SAMPLES); shots.append(a2)
    for o in made: bpy.data.objects.remove(o, do_unlink=True)
    # ---- B: the states (AntState: wet, toasted, burnt, spoiled, frozen) on the hero material
    cells = [("свежее", {}), ("мокрое", {"Soak": 1.0}), ("подрумянено", {"Toast": 1.0}), ("подгорело", {"Toast": 1.0, "Burn": 1.0}),
             ("испортилось", {"Spoil": 1.0}), ("замёрзло", {"Frozen": 1.0})]
    gx = max(wmx.x - wmn.x, wmx.y - wmn.y) * 1.25 + 0.1 * L; made = []
    for i, (name, st) in enumerate(cells):
        cx = (i - 2.5) * gx
        c = dup(whole, "st"); c.hide_render = False; FV.unique_mats(c); FV.set_state([c], **st)
        FV.place(c, cx, 0.0, math.radians(-25)); made.append(c)
        made.append(FV.text(name, (cx, bounds([c])[0].y - 0.12 * L, 0.0003), 0.11 * L))
    made.append(frame(made, 1800, 640, 40, (0.0, -1.0, 0.5), 1.05, fs * 1.8, 0.35))
    b = os.path.join(WORK, "%s_B.jpg" % fid); FV.render(b, 1800, 640, SAMPLES); shots.append(b)
    for o in made: bpy.data.objects.remove(o, do_unlink=True)
    # ---- C: the pre-cut pieces pulled apart along the cut, seen into the cut
    made = []
    cuts = spec.get("cuts", []) or spec.get("cuts_abs", [])
    n0 = Vector(cuts[0][1]).normalized() if cuts else Vector((1, 0, 0))
    if pieces:
        # every piece in a row, turned so its cut faces the camera (the cut is what this picture is for)
        view = Vector((0.0, -1.0, 0.55)).normalized(); x = 0.0
        for k, pc in enumerate(pieces):
            c = dup(pc, "pc"); c.hide_render = False
            inner_ids = [i for i, s in enumerate(c.material_slots) if s.material and s.material.name.startswith("Inner_")]
            nsum = Vector()
            for p in c.data.polygons:
                if p.material_index in inner_ids: nsum += p.normal * p.area
            if nsum.length > 1e-9:
                # (the object turns, not its mesh: the cut's look is read in the piece's own space, as in the game)
                q = nsum.normalized().rotation_difference(view)
                c.rotation_mode = "QUATERNION"; c.rotation_quaternion = q; bpy.context.view_layer.update()
            pmn, pmx = bounds([c]); w = pmx.x - pmn.x
            c.location += Vector((x - pmn.x, -(pmn.y + pmx.y) * 0.5, -pmn.z + 0.0002)); x += w + 0.12 * L
            made.append(c)
        bpy.context.view_layer.update()
        for c in made: c.location.x -= x * 0.5
        title = "заранее нарезанные куски: %d (срез — %s)" % (len(pieces), spec.get("cut_ru", "внутренность"))
    else:
        c = dup(whole, "pc"); c.hide_render = False; FV.place(c, 0, 0); made.append(c); title = "не ломается на куски (так в таблице еды)"
    pm, px = bounds(made)
    made.append(make_ant((px.x + 0.05 * L, pm.y - 0.05 * L, 0.0), math.radians(140)))
    made.append(FV.text(title, ((pm.x + px.x) * 0.5, pm.y - 0.14 * L, 0.0003), 0.06 * L))
    made.append(frame(made, 900, 700, 35, (0.15, -1.0, 0.55), 1.08, fs, 0.3))
    cimg = os.path.join(WORK, "%s_C.jpg" % fid); FV.render(cimg, 900, 700, SAMPLES); shots.append(cimg)
    for o in made: bpy.data.objects.remove(o, do_unlink=True)
    stitch(fid, shots)

def stitch(fid, shots):
    """A (1800x1000) / B (1800x640) / C + A2 (900x700 each) -> Saved/food_review/v4/<fid>.jpg"""
    import numpy as np
    ims = {}
    for p in shots:
        im = bpy.data.images.load(p); w, h = im.size; a = np.empty(w * h * 4, np.float32); im.pixels.foreach_get(a); ims[os.path.basename(p)[len(fid) + 1:-4]] = a.reshape(h, w, 4)
    A, B, C, A2 = ims["A"], ims["B"], ims["C"], ims["A2"]
    W = 1800; H = A.shape[0] + B.shape[0] + C.shape[0]
    big = np.ones((H, W, 4), np.float32)
    # (Blender images are bottom-up: the last row block goes first)
    big[0:700, 0:900] = C; big[0:700, 900:1800] = A2
    big[700:700 + B.shape[0]] = B
    big[700 + B.shape[0]:] = A
    o = bpy.data.images.new("sheet_" + fid, W, H); o.pixels.foreach_set(big.ravel())
    out = os.path.join(OUT_REV, "%s.jpg" % fid)
    o.filepath_raw = out; o.file_format = "JPEG"; o.save()
    log("SHEET", out)

# ================================================================ the rest: food_hero_more.py (the other foods of his, the new foods)
NEW = {}
_more = os.path.join(HERE, "food_hero_more.py")
if os.path.exists(_more):
    exec(compile(open(_more, encoding="utf-8").read(), _more, "exec"))

def run(fid):
    if fid in OWN: return run_own(fid)
    if fid in NEW: return run_new(fid)
    log("unknown food", fid)

if __name__ == "__main__":
    if WHICH == "old": ids = list(OWN.keys())
    elif WHICH == "new": ids = list(NEW.keys())
    elif WHICH == "all": ids = list(OWN.keys()) + list(NEW.keys())
    else: ids = [x for x in WHICH.replace("+", ",").split(",") if x]
    for fid in ids:
        t0 = time.time()
        try:
            run(fid); log("DONE", fid, "%.0f s" % (time.time() - t0))
        except Exception as e:
            import traceback; traceback.print_exc(); log("FAILED", fid, e)
    log("ALL_DONE")
