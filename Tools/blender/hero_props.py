# KAN-22 (small props of the flat): the props the ant sees most, rebuilt at the mop's quality through the hero pipeline (hero_lib.py).
# Each piece keeps its name, frame and box (the layout rows and their scales stay), gets LOD0 (the ant's camera, <= 8k triangles),
# LOD1 / LOD2, a baked texture set (labels: Tools/art/make_labels.py, our own words and design), UCX hulls, pre-cut break pieces by
# its class (china and glass shatter into shards, a packet tears), and review renders (overall, the ant's view, wet / charred / broken).
# Glass is glass: its faces go to their own slot Hero_Glass (translucent M_HeroGlass in the game; Blender's glass in the renders),
# and they are moved out of the way while the set is baked (the contents behind them get no false occlusion).
# The rebuilt meshes go to /Game/Hero/<Name> (KitchenArt::Mesh looks there first; -AntHeroArt=0 brings the old ones back).
#   SM_K_Plate      white glazed plate, cobalt rim lines, unglazed foot ring            china: 4 shards
#   SM_K_SpiceJar   glass spice jar, paprika inside, knurled steel cap, paper label    glass: 3 pieces
#   SM_K_Mug        cobalt glazed mug "ÐŸÐÐŸÐ", coffee dregs inside                       china: handle off + 2 shards
#   SM_K_PastaBox   printed carton of penne                                            cardboard: torn in two
#   SM_K_JarJam     glass jar of strawberry jam, red painted lid, label               glass: 3 pieces
# Run: blender --background --factory-startup --python hero_props.py -- <SourceArt/Blender/hero_props> <Saved/art_review/props> [names] [nobake | shape]
import bpy, bmesh, sys, os, math, random, json, time
from mathutils import Vector, Matrix
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hero_lib as H

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else os.path.join(HERE, "..", "..", "SourceArt", "Blender", "hero_props")
REV = argv[1] if len(argv) > 1 else os.path.join(HERE, "..", "..", "Saved", "art_review", "props")
ONLY = argv[2].replace("+", ",").split(",") if len(argv) > 2 and argv[2] not in ("all", "") else None
NOBAKE = len(argv) > 3 and argv[3] == "nobake"
SHAPE = len(argv) > 3 and argv[3] == "shape"
LAB = os.path.join(OUT, "labels")
K = H.K


def u(*v): return tuple(x * K for x in v)

def lathe(name, prof_uu, mat, steps=64, sub=0, at=(0, 0, 0)):
    o = H.screw(name, [(r * K, z * K) for r, z in prof_uu], steps, sub); o.location = u(*at); o.data.materials.append(mat)
    return o

def rbox(name, x0, x1, y0, y1, z0, z1, mat, bev_uu=4, sub=0, seg=2):
    o = H.cube(name, u(x1 - x0, y1 - y0, z1 - z0), u((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)); H.smooth(o, sub, bev_uu * K, seg); o.data.materials.append(mat); return o

def glass(o):
    o["glass"] = 1; return o

def wrap_label(name, r_uu, z0, z1, half_deg, mat, th_uu=0.5, n=40, yscale=1.0):
    """a paper label round the front (-Y) of a round body: an arc of film with a hair of thickness"""
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    rows = []
    for rr in (r_uu, r_uu + th_uu):
        ring = []
        for i in range(n + 1):
            a = math.radians(-90 - half_deg + 2 * half_deg * i / n)
            ring.append([bm.verts.new(u(rr * math.cos(a), rr * math.sin(a) * yscale, z)) for z in (z0, z1)])
        rows.append(ring)
    inner, outer = rows
    for i in range(n):
        bm.faces.new((outer[i][0], outer[i + 1][0], outer[i + 1][1], outer[i][1]))
        bm.faces.new((inner[i][1], inner[i + 1][1], inner[i + 1][0], inner[i][0]))
        bm.faces.new((inner[i][1], outer[i][1], outer[i + 1][1], inner[i + 1][1]))
        bm.faces.new((inner[i + 1][0], outer[i + 1][0], outer[i][0], inner[i][0]))
    bm.faces.new((inner[0][0], outer[0][0], outer[0][1], inner[0][1])); bm.faces.new((inner[n][1], outer[n][1], outer[n][0], inner[n][0]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(me); bm.free()
    o = H.link(bpy.data.objects.new(name, me)); o.data.materials.append(mat)
    for p in o.data.polygons: p.use_smooth = True
    return o

# ------------------------------------------------------------------------------------------------ materials
def N(m, t, x=0, y=0): return H.N(m, t, x, y)
def L(m, a, b): H.L(m, a, b)

def _sep(m):
    tc = N(m, "ShaderNodeTexCoord", -1600); sp = N(m, "ShaderNodeSeparateXYZ", -1400); L(m, tc.outputs["Object"], sp.inputs["Vector"]); return tc, sp

def _math(m, op, a, b=None, x=-1200, y=0, clamp=False):
    n = N(m, "ShaderNodeMath", x, y); n.operation = op; n.use_clamp = clamp
    for i, v in enumerate((a, b)):
        if v is None: continue
        if isinstance(v, (int, float)): n.inputs[i].default_value = v
        else: L(m, v, n.inputs[i])
    return n.outputs[0]

def _maprange(m, v, a0, a1, b0=0.0, b1=1.0, clamp=False, x=-1000, y=0):
    n = N(m, "ShaderNodeMapRange", x, y); n.clamp = clamp
    n.inputs["From Min"].default_value = a0; n.inputs["From Max"].default_value = a1; n.inputs["To Min"].default_value = b0; n.inputs["To Max"].default_value = b1
    L(m, v, n.inputs["Value"]); return n.outputs["Result"]

def _img(m, path, uvec, vvec, x=-700, y=0, interp="Cubic"):
    cb = N(m, "ShaderNodeCombineXYZ", x - 200, y); L(m, uvec, cb.inputs["X"]); L(m, vvec, cb.inputs["Y"])
    t = N(m, "ShaderNodeTexImage", x, y); t.image = bpy.data.images.load(path, check_existing=True); t.extension = "CLIP"; t.interpolation = interp
    L(m, cb.outputs["Vector"], t.inputs["Vector"]); return t

def _over(m, base_sock_or_col, top_col, alpha, x=-300, y=0):
    mx = N(m, "ShaderNodeMix", x, y); mx.data_type = "RGBA"
    if isinstance(base_sock_or_col, tuple): mx.inputs[6].default_value = (*base_sock_or_col, 1)
    else: L(m, base_sock_or_col, mx.inputs[6])
    L(m, top_col, mx.inputs[7]); L(m, alpha, mx.inputs[0]); return mx.outputs[2]

def _bump(m, p, scale, strength, dist, x=-400, y=-500):
    tc = N(m, "ShaderNodeTexCoord", x - 400, y); no = N(m, "ShaderNodeTexNoise", x - 200, y); no.inputs["Scale"].default_value = scale; no.inputs["Detail"].default_value = 6
    L(m, tc.outputs["Object"], no.inputs["Vector"])
    bp = N(m, "ShaderNodeBump", x, y); bp.inputs["Strength"].default_value = strength; bp.inputs["Distance"].default_value = dist
    L(m, no.outputs["Fac"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"]); return bp

def phi_front(m, sp, x=-1200, y=0):
    """angle round Z measured from the front (-Y), positive towards +X: atan2(x, -y)"""
    ny = _math(m, "MULTIPLY", sp.outputs["Y"], -1.0, x - 200, y)
    return _math(m, "ARCTAN2", sp.outputs["X"], ny, x, y)

def ceramic(name, col, rough=0.06, bands=(), rmax_uu=450.0, foot_z_uu=None, biscuit=(0.80, 0.74, 0.64)):
    """glazed china: glossy coat, a hair of glaze waviness; cobalt rim bands (radius ranges in uu); the unglazed foot ring below foot_z"""
    m, p = H.mat(name)
    p.inputs["Roughness"].default_value = rough; p.inputs["Coat Weight"].default_value = 0.7; p.inputs["Coat Roughness"].default_value = 0.03
    tc, sp = _sep(m)
    colsock = None
    if bands:
        cxy = N(m, "ShaderNodeCombineXYZ", -1300, 300); L(m, sp.outputs["X"], cxy.inputs["X"]); L(m, sp.outputs["Y"], cxy.inputs["Y"])
        ln = N(m, "ShaderNodeVectorMath", -1150, 300); ln.operation = "LENGTH"; L(m, cxy.outputs["Vector"], ln.inputs[0])
        rr = _maprange(m, ln.outputs["Value"], 0.0, rmax_uu * K, 0.0, 1.0, True, -1000, 300)
        cr = N(m, "ShaderNodeValToRGB", -800, 300); cr.color_ramp.interpolation = "LINEAR"; e = cr.color_ramp.elements
        e[0].position = 0.0; e[0].color = (*col, 1); e[1].position = 1.0; e[1].color = (*col, 1)
        for (r0, r1, bc) in bands:
            for pos, c in ((r0 / rmax_uu - 0.002, col), (r0 / rmax_uu, bc), (r1 / rmax_uu, bc), (r1 / rmax_uu + 0.002, col)):
                el = e.new(max(0.0, min(1.0, pos))); el.color = (*c, 1)
        L(m, rr, cr.inputs["Fac"]); colsock = cr.outputs["Color"]
    if foot_z_uu is not None:
        fz = _maprange(m, sp.outputs["Z"], foot_z_uu * K, foot_z_uu * K * 0.5, 0.0, 1.0, True, -1000, 0)
        rgb = N(m, "ShaderNodeRGB", -700, -150); rgb.outputs[0].default_value = (*biscuit, 1)
        colsock = _over(m, colsock if colsock is not None else col, rgb.outputs[0], fz, -400, 100)
        rs = _maprange(m, fz, 0.0, 1.0, rough, 0.85, True, -400, -100); L(m, rs, p.inputs["Roughness"])
    if colsock is not None: L(m, colsock, p.inputs["Base Color"])
    else: p.inputs["Base Color"].default_value = (*col, 1)
    _bump(m, p, 40.0, 0.03, 0.0004)
    return H.tag(m, 0.0, 0.05)

def glass_mat(name, tint=(0.92, 0.96, 0.94), rough=0.02):
    m, p = H.mat(name)
    p.inputs["Base Color"].default_value = (*tint, 1); p.inputs["Roughness"].default_value = rough
    p.inputs["Transmission Weight"].default_value = 1.0; p.inputs["IOR"].default_value = 1.5
    _bump(m, p, 9.0, 0.02, 0.0003)        # the slight waviness of pressed glass
    return H.tag(m, 0.0, 0.0)

def powder(name, a=(0.62, 0.10, 0.04), b=(0.38, 0.05, 0.02)):
    m, p = H.mat(name)
    tc = N(m, "ShaderNodeTexCoord", -900); no = N(m, "ShaderNodeTexNoise", -700); no.inputs["Scale"].default_value = 900; no.inputs["Detail"].default_value = 4
    L(m, tc.outputs["Object"], no.inputs["Vector"])
    cr = N(m, "ShaderNodeValToRGB", -480); e = cr.color_ramp.elements; e[0].color = (*b, 1); e[1].color = (*a, 1); e[0].position = 0.35; e[1].position = 0.65
    L(m, no.outputs["Fac"], cr.inputs["Fac"]); L(m, cr.outputs["Color"], p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = 0.95
    bp = N(m, "ShaderNodeBump", -260, -200); bp.inputs["Strength"].default_value = 0.5; bp.inputs["Distance"].default_value = 0.0004
    L(m, no.outputs["Fac"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return H.tag(m, 0.0, 0.9)

def jam_mat(name):
    m, p = H.mat(name)
    tc = N(m, "ShaderNodeTexCoord", -1100)
    no = N(m, "ShaderNodeTexNoise", -900); no.inputs["Scale"].default_value = 80; no.inputs["Detail"].default_value = 6
    L(m, tc.outputs["Object"], no.inputs["Vector"])
    cr = N(m, "ShaderNodeValToRGB", -700); e = cr.color_ramp.elements; e[0].color = (0.16, 0.01, 0.02, 1); e[1].color = (0.42, 0.03, 0.05, 1)
    L(m, no.outputs["Fac"], cr.inputs["Fac"])
    vo = N(m, "ShaderNodeTexVoronoi", -900, -300); vo.inputs["Scale"].default_value = 700; L(m, tc.outputs["Object"], vo.inputs["Vector"])
    sd = N(m, "ShaderNodeValToRGB", -700, -300); s = sd.color_ramp.elements; s[0].position = 0.06; s[0].color = (1, 1, 1, 1); s[1].position = 0.08; s[1].color = (0, 0, 0, 1)
    L(m, vo.outputs["Distance"], sd.inputs["Fac"])
    rgb = N(m, "ShaderNodeRGB", -500, -450); rgb.outputs[0].default_value = (0.55, 0.32, 0.08, 1)
    seeds = _over(m, cr.outputs["Color"], rgb.outputs[0], sd.outputs["Color"], -300, 0)
    L(m, seeds, p.inputs["Base Color"]); p.inputs["Roughness"].default_value = 0.12; p.inputs["Coat Weight"].default_value = 0.6
    return H.tag(m, 0.0, 0.1)

def knurl_metal(name, col=(0.80, 0.81, 0.82), rough=0.3, ridges=90):
    m = H.metal(name, col, rough); nt = m.node_tree; p = nt.nodes["Principled BSDF"]
    tc, sp = _sep(m)
    ph = _math(m, "ARCTAN2", sp.outputs["Y"], sp.outputs["X"], -1200, -700)
    k = _math(m, "MULTIPLY", ph, float(ridges), -1050, -700); s = _math(m, "SINE", k, None, -900, -700)
    bp = N(m, "ShaderNodeBump", -300, -700); bp.inputs["Strength"].default_value = 0.6; bp.inputs["Distance"].default_value = 0.0006
    L(m, s, bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return m

def label_cyl(name, img, half_deg, z0_uu, z1_uu, rough=0.55, base=(0.95, 0.94, 0.9), porosity=0.6, lying=None, yscale=1.0):
    """a paper label: the image round the body, centred at the front, from -half to +half degrees, z0..z1.
    lying=(y0, zc): the body lies along +Y from y0 (lay()), its axis at height zc, the label's front up"""
    m, p = H.mat(name); tc, sp = _sep(m)
    if lying:
        zz = _math(m, "SUBTRACT", sp.outputs["Z"], lying[1] * K, -1400, 0)
        ph = _math(m, "ARCTAN2", sp.outputs["X"], zz, -1200, 0)
        vv = _maprange(m, sp.outputs["Y"], (lying[0] + z0_uu) * K, (lying[0] + z1_uu) * K, 0.0, 1.0, False, -1000, -200)
    else:
        ny = _math(m, "DIVIDE", sp.outputs["Y"], -yscale, -1400, 0)
        ph = _math(m, "ARCTAN2", sp.outputs["X"], ny, -1200, 0)
        vv = _maprange(m, sp.outputs["Z"], z0_uu * K, z1_uu * K, 0.0, 1.0, False, -1000, -200)
    uu = _maprange(m, ph, -math.radians(half_deg), math.radians(half_deg), 0.0, 1.0, False, -1000, 0)
    t = _img(m, os.path.join(LAB, img), uu, vv)
    L(m, _over(m, base, t.outputs["Color"], t.outputs["Alpha"]), p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = rough
    _bump(m, p, 600.0, 0.08, 0.0002)
    return H.tag(m, 0.0, porosity)

def print_box(name, imgs, x0, x1, y0, y1, z0, z1, rough=0.5, base=(0.85, 0.75, 0.35)):
    """printed carton: one image per face side, picked by the object normal (front -Y, back +Y, sides +-X, top +Z)"""
    m, p = H.mat(name); tc, sp = _sep(m)
    nrm = N(m, "ShaderNodeSeparateXYZ", -1400, 800); L(m, tc.outputs["Normal"], nrm.inputs["Vector"])
    X, Y, Z = sp.outputs["X"], sp.outputs["Y"], sp.outputs["Z"]
    def mr(v, a, b, yy): return _maprange(m, v, a * K, b * K, 0.0, 1.0, False, -1100, yy)
    faces = [("front", mr(X, x0, x1, 0), mr(Z, z0, z1, -100), nrm.outputs["Y"], -1.0),
             ("back", mr(X, x1, x0, -200), mr(Z, z0, z1, -300), nrm.outputs["Y"], 1.0),
             ("side", mr(Y, y0, y1, -400), mr(Z, z0, z1, -500), nrm.outputs["X"], 1.0),     # (seen from +X, +Y is to the right)
             ("side", mr(Y, y1, y0, -600), mr(Z, z0, z1, -700), nrm.outputs["X"], -1.0),
             ("top", mr(X, x0, x1, -800), mr(Y, y0, y1, -900), nrm.outputs["Z"], 1.0)]
    col = None
    for k, (key, uu, vv, nc, sgn) in enumerate(faces):
        if key == "side" and sgn < 0 and "side2" in imgs: key = "side2"
        t = _img(m, os.path.join(LAB, imgs[key]), uu, vv, -700, -k * 300)
        w = _math(m, "MULTIPLY", nc, sgn, -500, -k * 300 - 120); w = _math(m, "GREATER_THAN", w, 0.6, -400, -k * 300 - 120)
        col = _over(m, col if col is not None else base, t.outputs["Color"], w, -250, -k * 300)
    L(m, col, p.inputs["Base Color"]); p.inputs["Roughness"].default_value = rough; p.inputs["Coat Weight"].default_value = 0.1
    _bump(m, p, 300.0, 0.05, 0.0003)
    return H.tag(m, 0.0, 0.55)

def mug_glaze(name, col, img, z0_uu, z1_uu, r_print_uu):
    """cobalt glaze with a white overglaze print round the outside (the image covers the full turn, the front at its centre)"""
    m = ceramic(name, col, 0.05, (), 450.0, 5.0); nt = m.node_tree; p = nt.nodes["Principled BSDF"]
    base = p.inputs["Base Color"].links[0].from_socket
    tc, sp = _sep(m)
    ph = phi_front(m, sp, -1200, -900)
    uu = _maprange(m, ph, -math.pi, math.pi, 0.0, 1.0, False, -1000, -900)
    vv = _maprange(m, sp.outputs["Z"], z0_uu * K, z1_uu * K, 0.0, 1.0, False, -1000, -1100)
    t = _img(m, os.path.join(LAB, img), uu, vv, -700, -900)
    cxy = N(m, "ShaderNodeCombineXYZ", -1300, -1300); L(m, sp.outputs["X"], cxy.inputs["X"]); L(m, sp.outputs["Y"], cxy.inputs["Y"])
    ln = N(m, "ShaderNodeVectorMath", -1150, -1300); ln.operation = "LENGTH"; L(m, cxy.outputs["Vector"], ln.inputs[0])
    outside = _math(m, "GREATER_THAN", ln.outputs["Value"], r_print_uu * K, -1000, -1300)
    a = _math(m, "MULTIPLY", t.outputs["Alpha"], outside, -500, -1100)
    L(m, _over(m, base, t.outputs["Color"], a, -200, -900), p.inputs["Base Color"])
    return m

def mats():
    M = {}
    M["plate"] = ceramic("PlateGlaze", (0.93, 0.92, 0.88), 0.05, [(398, 404, (0.10, 0.18, 0.55)), (420, 432, (0.10, 0.18, 0.55))], 450.0, 4.0)
    M["glass"] = glass_mat("Glass", (0.93, 0.97, 0.95))
    M["steel"] = knurl_metal("CapSteel", (0.80, 0.81, 0.82), 0.36, 90)
    M["paprika"] = powder("Paprika")
    M["spiceLabel"] = label_cyl("LabelSpice", "spice_paprika.png", 150, 22, 98)
    M["mug"] = mug_glaze("MugCobalt", (0.07, 0.16, 0.50), "mug_print.png", 70, 270, 172)
    M["coffee"] = H.plastic("CoffeeDregs", (0.07, 0.035, 0.015), 0.08, peel=0.02, scuff=0.05); H.tag(M["coffee"], 0.0, 0.1)
    M["pasta"] = print_box("PastaPrint", {"front": "pasta_front.png", "back": "pasta_back.png", "side": "pasta_side.png", "top": "pasta_top.png"},
                           -150, 150, -102, 100, 0, 750)
    M["jam"] = jam_mat("Jam")
    M["lidRed"] = H.plastic("LidRedPaint", (0.55, 0.03, 0.03), 0.22, peel=0.02, scuff=0.18)
    M["jamLabel"] = label_cyl("LabelJam", "jam.png", 120, 50, 190)
    return mats12(mats11(mats10(mats9(mats8(mats7(mats6(mats5(mats4(mats2(M))))))))))

# ------------------------------------------------------------------------------------------------ the pieces (frames and boxes of the old ones)
def plate(M):
    prof = [(0, 7), (226, 7), (236, 3), (242, 0), (252, 0), (258, 4), (266, 9), (320, 14), (380, 25), (424, 37), (444, 44), (450, 48),
            (448, 50), (440, 50), (420, 44), (378, 34), (320, 24), (266, 18), (200, 17), (100, 16.5), (0, 16)]
    return [(lathe("Plate", prof, M["plate"], 72), 0)]

def spicejar(M):
    g = [(0, 0), (38, 0), (42, 2), (44, 7), (44, 108), (42, 113), (37, 117), (35, 119), (35, 132), (34, 134.5), (32, 134.5), (31, 133),
         (31, 120), (34, 116), (39, 112), (40, 108), (40, 10), (38, 6.5), (33, 5.5), (0, 5.5)]
    parts = [(glass(lathe("Glass", g, M["glass"], 64)), 0)]
    parts.append((lathe("Paprika", [(0, 6.2), (32, 6.2), (37.5, 7.5), (39.4, 11), (39.4, 88), (30, 91), (12, 93.5), (0, 94)], M["paprika"], 48), 2))
    parts.append((lathe("Cap", [(0, 150), (37, 150), (40, 148.5), (41, 146), (41, 122), (40, 119), (37, 119), (36.4, 121), (36.4, 146), (0, 146)], M["steel"], 64), 1))
    parts.append((wrap_label("Label", 44.15, 22, 98, 150, M["spiceLabel"], 0.4, 48), 0))
    return parts

def mug(M):
    prof = [(0, 4), (150, 4), (156, 1), (160, 0), (166, 0), (170, 4), (172, 12), (174, 40), (177, 150), (179.5, 292), (179, 298), (177, 300.5), (174, 300.5),
            (172, 298), (170.5, 292), (168, 150), (165, 42), (160, 33), (150, 30.5), (0, 30)]
    parts = [(lathe("Mug", prof, M["mug"], 64), 0)]
    parts.append((lathe("Dregs", [(0, 30.6), (150, 30.6), (161, 33), (164, 38), (150, 36), (0, 35.5)], M["coffee"], 48), 0))
    pts = [Vector(u(*p)) for p in ((166, 0, 242), (215, 0, 246), (262, 0, 228), (290, 0, 176), (284, 0, 118), (252, 0, 80), (205, 0, 68), (166, 0, 72))]
    h = H.tubes("Handle", [pts], 17 * K, res_u=10, bevel_res=3, tip=1.0); h.data.materials.append(M["mug"]); h.scale = (1, 0.82, 1)
    parts.append((h, 1))
    return parts

def pastabox(M):
    return [(rbox("Box", -150, 150, -102, 100, 0, 750, M["pasta"], 2.5, 0, 2), 0)]

def jarjam(M):
    g = [(0, 0), (118, 0), (130, 3), (137, 10), (140, 22), (140, 204), (137, 212), (127, 222), (119, 229), (118, 233), (118, 262), (116.5, 265),
         (113.5, 265), (112, 262), (112, 231), (120, 222), (129, 213), (134, 203), (134, 24), (131, 15), (120, 9.5), (0, 9)]
    parts = [(glass(lathe("Glass", g, M["glass"], 72)), 0)]
    parts.append((lathe("Jam", [(0, 9.6), (119, 9.6), (130.5, 15), (133.4, 24), (133.4, 196), (120, 199), (60, 201), (0, 202)], M["jam"], 56), 2))
    parts.append((lathe("Lid", [(0, 290), (84, 290), (88, 291.5), (98, 291.5), (102, 290), (122, 290), (127, 287), (128.5, 282), (128.5, 242), (127, 237),
                                (123, 237), (122.4, 240), (122.4, 284), (0, 284)], M["lidRed"], 72), 1))
    parts.append((wrap_label("Label", 140.15, 50, 190, 120, M["jamLabel"], 0.5, 56), 0))
    return parts

# ---- batch 2: the tin, the bottle, the mouse trap, the sponge, the floor cleaner
def lay(o, y0, zc, local=None):
    """a piece built round Z (front -Y) laid along +Y from y0, its axis at height zc, its front up"""
    T = Matrix(((1, 0, 0, 0), (0, 0, 1, y0 * K), (0, -1, 0, zc * K), (0, 0, 0, 1)))
    o.matrix_world = T @ (local if local is not None else o.matrix_world); return o

def lid_metal(name, col=(0.80, 0.80, 0.78), rough=0.24):
    """a tin lid: even sheen, pressed concentric rings (the lid lies in the XZ plane once laid: rings round the can's axis, Y)"""
    m, p = H.mat(name); p.inputs["Base Color"].default_value = (*col, 1); p.inputs["Metallic"].default_value = 1.0; p.inputs["Roughness"].default_value = rough
    tc, sp = _sep(m)
    zz = _math(m, "SUBTRACT", sp.outputs["Z"], 123 * K, -1300, -300)
    cxy = N(m, "ShaderNodeCombineXYZ", -1150, -300); L(m, sp.outputs["X"], cxy.inputs["X"]); L(m, zz, cxy.inputs["Y"])
    ln = N(m, "ShaderNodeVectorMath", -1000, -300); ln.operation = "LENGTH"; L(m, cxy.outputs["Vector"], ln.inputs[0])
    k = _math(m, "MULTIPLY", ln.outputs["Value"], 520.0, -850, -300); s = _math(m, "SINE", k, None, -700, -300)
    bp = N(m, "ShaderNodeBump", -300, -300); bp.inputs["Strength"].default_value = 0.0; bp.inputs["Distance"].default_value = 0.0005   # (rings off: the lid is bent away from the axis)
    L(m, s, bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return H.tag(m, 1.0, 0.0)

def tin_metal(name, zc_uu, col=(0.80, 0.80, 0.78), gold=(0.74, 0.52, 0.20), rough=0.22):
    """tinplate, gold lacquer inside (the faces whose normal points to the can's axis, the can lying along Y at zc)"""
    m = H.metal(name, col, rough); p = m.node_tree.nodes["Principled BSDF"]
    tc, sp = _sep(m)
    zz = _math(m, "SUBTRACT", sp.outputs["Z"], zc_uu * K, -1400, 400)
    rad = N(m, "ShaderNodeCombineXYZ", -1250, 400); L(m, sp.outputs["X"], rad.inputs["X"]); L(m, zz, rad.inputs["Z"])
    dt = N(m, "ShaderNodeVectorMath", -1100, 400); dt.operation = "DOT_PRODUCT"; L(m, tc.outputs["Normal"], dt.inputs[0]); L(m, rad.outputs["Vector"], dt.inputs[1])
    inner = _math(m, "LESS_THAN", dt.outputs["Value"], 0.0, -950, 400)
    rgb = N(m, "ShaderNodeRGB", -700, 600); rgb.outputs[0].default_value = (*gold, 1)
    L(m, _over(m, col, rgb.outputs[0], inner, -400, 400), p.inputs["Base Color"])
    return m

def wood_print(name, img, x0, x1, y0, y1, light=(0.80, 0.64, 0.42), dark=(0.58, 0.40, 0.22)):
    """pale pine with a stamp on the top face (the image's alpha), x0..x1 by y0..y1"""
    m = H.wood(name, light, dark, 30, 0.0); nt = m.node_tree; p = nt.nodes["Principled BSDF"]
    src = p.inputs["Base Color"].links[0].from_socket
    tc, sp = _sep(m)
    nrm = N(m, "ShaderNodeSeparateXYZ", -1400, -900); L(m, tc.outputs["Normal"], nrm.inputs["Vector"])
    top = _math(m, "GREATER_THAN", nrm.outputs["Z"], 0.6, -1200, -900)
    uu = _maprange(m, sp.outputs["X"], x0 * K, x1 * K, 0.0, 1.0, False, -1000, -1000)
    vv = _maprange(m, sp.outputs["Y"], y0 * K, y1 * K, 0.0, 1.0, False, -1000, -1100)
    t = _img(m, os.path.join(LAB, img), uu, vv, -700, -1000)
    a = _math(m, "MULTIPLY", t.outputs["Alpha"], top, -500, -1000)
    L(m, _over(m, src, t.outputs["Color"], a, -200, -900), p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = 0.7
    return H.tag(m, 0.0, 0.7)

def foam(name, col=(0.92, 0.70, 0.12)):
    m, p = H.mat(name)
    tc = N(m, "ShaderNodeTexCoord", -1100)
    vo = N(m, "ShaderNodeTexVoronoi", -900); vo.inputs["Scale"].default_value = 700
    L(m, tc.outputs["Object"], vo.inputs["Vector"])
    cr = N(m, "ShaderNodeValToRGB", -700); e = cr.color_ramp.elements; e[0].position = 0.0; e[0].color = (col[0] * 0.45, col[1] * 0.40, col[2] * 0.3, 1); e[1].position = 0.35; e[1].color = (*col, 1)
    L(m, vo.outputs["Distance"], cr.inputs["Fac"]); L(m, cr.outputs["Color"], p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = 0.92; p.inputs["Sheen Weight"].default_value = 0.3
    bp = N(m, "ShaderNodeBump", -300, -300); bp.inputs["Strength"].default_value = 0.9; bp.inputs["Distance"].default_value = 0.0012
    L(m, vo.outputs["Distance"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return H.tag(m, 0.0, 1.0)

def knurl(m, ridges=60, strength=0.5):
    """vertical grip ridges round Z (caps)"""
    p = m.node_tree.nodes["Principled BSDF"]; tc, sp = _sep(m)
    ph = _math(m, "ARCTAN2", sp.outputs["Y"], sp.outputs["X"], -1200, -900)
    k = _math(m, "MULTIPLY", ph, float(ridges), -1050, -900); s = _math(m, "SINE", k, None, -900, -900)
    bp = N(m, "ShaderNodeBump", -300, -900); bp.inputs["Strength"].default_value = strength; bp.inputs["Distance"].default_value = 0.0006
    L(m, s, bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return m

def mats2(M):
    M["tin"] = tin_metal("Tinplate", 123)
    M["tinLid"] = lid_metal("TinLid")
    M["peasLabel"] = label_cyl("LabelPeas", "peas.png", 179, 40, 360, 0.5, (0.16, 0.43, 0.2), 0.6, (-200, 123))
    M["bottleGlass"] = glass_mat("BottleGlass", (0.42, 0.68, 0.44), 0.03)
    M["lemonLabel"] = label_cyl("LabelLemonade", "lemonade.png", 150, 640, 960, 0.5, (0.97, 0.95, 0.9), 0.6, (-550, 122))
    M["trapWood"] = wood_print("TrapPine", "trap_stamp.png", -190, 190, -90, 90)
    M["brass"] = H.metal("Brass", (0.80, 0.60, 0.30), 0.3)
    M["springSteel"] = H.metal("SpringSteel", (0.62, 0.63, 0.64), 0.35)
    M["cheese"] = H.plastic("BaitCheese", (0.92, 0.72, 0.28), 0.55, peel=0.15, scuff=0.2); H.tag(M["cheese"], 0.0, 0.5)
    M["foam"] = foam("SpongeFoam")
    M["scourer"] = H.fibre("Scourer", light=(0.16, 0.48, 0.16), dark=(0.05, 0.24, 0.06), dirt=(0.1, 0.2, 0.1), floor_h=0.0, twist=3400)
    M["hdpe"] = H.plastic("HdpeGreen", (0.10, 0.42, 0.24), 0.42, peel=0.02, scuff=0.2)
    M["capWhite"] = knurl(H.plastic("CapWhite2", (0.90, 0.90, 0.86), 0.4), 70, 0.4)
    M["detLabel"] = label_cyl("LabelCleaner", "detergent.png", 72, 120, 440, 0.45, (0.98, 0.98, 0.96), 0.4, None, 0.8)
    return M

def beads(z):
    return sum(2.0 * max(0.0, 1.0 - abs(z - zb) / 6.0) for zb in range(70, 340, 34))

def tincan(M):
    zs = [8.0]
    for zb in range(70, 340, 34): zs += [zb - 6.0, zb, zb + 6.0]
    zs += [392.0]
    outer = [(0, 397), (108, 397), (116, 399), (121.5, 401), (123.5, 398.5), (122.5, 393)] + [(122.5 - beads(z), z) for z in reversed(zs)] + [(123.5, 3.5), (122.5, 0.3), (120.5, 0.3)]
    inner = [(121.0 - beads(z), z) for z in zs] + [(116, 394), (108, 395.5), (0, 395.5)]
    body = lay(lathe("Can", outer + inner, M["tin"], 56), -200, 123)
    lid = lathe("Lid", [(0, -1.6), (112, -1.6), (116, -1.2), (116.5, -0.4), (116, 0), (0, 0)], M["tinLid"], 56)
    p = Vector(u(0, -116, 0))
    lay(lid, -200, 123, Matrix.Translation(p) @ Matrix.Rotation(math.radians(-75), 4, "X") @ Matrix.Translation(-p))
    lab = lay(wrap_label("Label", 122.9, 40, 360, 179, M["peasLabel"], 0.4, 72), -200, 123)
    return [(body, 0), (lab, 0), (lid, 1)]

def bottle(M):
    g = [(0, 1082), (50, 1086), (96, 1094), (112, 1100), (119, 1097), (122, 1088), (122, 575), (119, 548), (104, 508), (78, 470), (56, 436), (47, 400),
         (45.5, 70), (46.5, 30), (51, 24), (52, 12), (50, 3), (46, 0), (41, 0), (39.5, 4), (39, 30), (38.5, 70), (40, 400), (48, 436), (70, 470),
         (97, 508), (112, 548), (115.5, 575), (115.5, 1086), (109, 1091), (92, 1087), (48, 1077), (0, 1074)]
    gl = lay(glass(lathe("Glass", g, M["bottleGlass"], 64)), -550, 122)
    lab = lay(wrap_label("Label", 122.45, 640, 960, 150, M["lemonLabel"], 0.4, 56), -550, 122)
    return [(gl, 0), (lab, 0)]

def wedge(name, pts_uu, z0, z1, mat, bev=1.0):
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    lo = [bm.verts.new(u(x, y, z0)) for x, y in pts_uu]; hi = [bm.verts.new(u(x, y, z1)) for x, y in pts_uu]
    bm.faces.new(lo[::-1]); bm.faces.new(hi)
    for i in range(len(pts_uu)):
        j = (i + 1) % len(pts_uu); bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); bm.to_mesh(me); bm.free()
    o = H.link(bpy.data.objects.new(name, me)); o.data.materials.append(mat); H.smooth(o, 0, bev * K, 2); return o

def wire(name, pts_uu, r_uu, mat, res=6):
    o = H.tubes(name, [[Vector(u(*q)) for q in pts_uu]], r_uu * K, res_u=res, bevel_res=2, tip=1.0); o.data.materials.append(mat); return o

def mousetrap(M):
    parts = [(rbox("Board", -190, 190, -90, 90, 0, 16, M["trapWood"], 1.2, 0, 2), 0)]
    hw = [rbox("Bait", 40, 110, -35, 35, 16, 19, M["brass"], 0.6, 0, 1),
          wedge("Cheese", [(52, -20), (100, 0), (52, 20)], 19, 44, M["cheese"], 1.5),
          wire("Axle", [(0, -86, 25), (0, 86, 25)], 2.4, M["springSteel"], 2),
          wire("Hold", [(-172, 0, 19), (-60, 0, 21), (36, 0, 21), (42, 0, 24)], 1.8, M["springSteel"], 6),
          wire("Catch", [(-178, -10, 16), (-178, -10, 24), (-178, 10, 24), (-178, 10, 16)], 1.8, M["springSteel"], 6)]
    for y in (-80, 80):
        hw.append(wire("Staple", [(-8, y, 16), (-8, y, 29), (8, y, 29), (8, y, 16)], 1.6, M["springSteel"], 6))
    for y0, y1 in ((-72, -24), (24, 72)):   # the springs round the axle
        n = 6; pts = []
        for k in range(n * 8 + 1):
            t = k / 8.0 * 2 * math.pi; y = y0 + (y1 - y0) * k / (n * 8)
            pts.append((9 * math.cos(t), y, 25 + 9 * math.sin(t)))
        hw.append(wire("Spring", pts, 1.6, M["springSteel"], 3))
    return parts + [(o, 1) for o in hw]

def trapbar(M):
    return [(wire("Bar", [(0, -83, 4), (-172, -83, 4), (-172, 83, 4), (0, 83, 4)], 3.6, M["springSteel"], 10), 0),
            (wire("CoilL", [(0, -83, 4), (5, -83, 0), (0, -83, -3), (-4, -83, 1)], 3.0, M["springSteel"], 6), 0),
            (wire("CoilR", [(0, 83, 4), (5, 83, 0), (0, 83, -3), (-4, 83, 1)], 3.0, M["springSteel"], 6), 0)]

def sponge(M):
    f = rbox("Foam", -150, 150, -150, 150, 0, 120, M["foam"], 16, 1, 3)
    s = rbox("Scourer", -149, 149, -149, 149, 118, 150, M["scourer"], 6, 1, 2)
    return [(f, 0), (s, 1)]

def cleaner(M):
    body = lathe("Body", [(0, 0), (148, 0), (158, 6), (162, 22), (162, 470), (156, 505), (130, 560), (96, 605), (70, 640), (64, 648), (64, 662), (0, 662)], M["hdpe"], 56)
    body.scale = (1.0, 0.8, 1.0)
    cap = lathe("Cap", [(0, 770), (56, 770), (64, 766), (66, 756), (66, 672), (68, 666), (68, 658), (0, 658)], M["capWhite"], 48)
    lab = wrap_label("Label", 162.3, 120, 440, 72, M["detLabel"], 0.5, 40, 0.8)
    return [(body, 0), (lab, 0), (cap, 1)]

# ---- batch 3: the family's books (SM_L_Book_A..H): hardcover, cloth over boards, printed cover and spine, a page block
BOOKDIM = {"A": (0.0566, 0.2648, 0.30), "B": (0.0766, 0.3048, 0.36), "C": (0.1016, 0.3248, 0.40), "D": (0.0516, 0.2248, 0.25),
           "E": (0.0666, 0.2848, 0.33), "F": (0.0866, 0.3048, 0.38), "G": (0.0496, 0.2448, 0.27), "H": (0.1116, 0.3048, 0.29)}

def pages_mat(name):
    m, p = H.mat(name)
    tc = N(m, "ShaderNodeTexCoord", -900)
    w = N(m, "ShaderNodeTexWave", -700); w.wave_type = "BANDS"; w.bands_direction = "X"; w.inputs["Scale"].default_value = 1400; w.inputs["Distortion"].default_value = 2.0; w.inputs["Detail"].default_value = 3
    L(m, tc.outputs["Object"], w.inputs["Vector"])
    cr = N(m, "ShaderNodeValToRGB", -480); e = cr.color_ramp.elements; e[0].color = (0.74, 0.70, 0.60, 1); e[1].color = (0.93, 0.90, 0.82, 1); e[0].position = 0.2
    L(m, w.outputs["Fac"], cr.inputs["Fac"]); L(m, cr.outputs["Color"], p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = 0.85
    bp = N(m, "ShaderNodeBump", -260, -200); bp.inputs["Strength"].default_value = 0.35; bp.inputs["Distance"].default_value = 0.0003
    L(m, w.outputs["Fac"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return H.tag(m, 0.0, 0.85)

def make_book(k):
    def build(M):
        t, d, h = (v / K for v in BOOKDIM[k])          # (uu)
        c = 8.5                                        # board thickness
        cloth = print_box("Book" + k, {"front": "book_%s_spine.png" % k, "back": "book_%s_spine.png" % k, "side": "book_%s_front.png" % k,
                                       "side2": "book_%s_back.png" % k, "top": "book_%s_spine.png" % k}, -t / 2, t / 2, -d / 2, d / 2, 0, h, 0.6, (0.5, 0.4, 0.3))
        H.tag(cloth, 0.0, 0.6)
        pg = M.get("pages") or pages_mat("Pages"); M["pages"] = pg
        parts = []
        for s in (-1, 1):
            parts.append((rbox("Board", min(s * (t / 2 - c), s * t / 2), max(s * (t / 2 - c), s * t / 2), -d / 2 + 2, d / 2, 0, h, cloth, 1.6, 0, 2), 0))
        parts.append((rbox("Spine", -t / 2, t / 2, -d / 2, -d / 2 + 2.2 * c, 0, h, cloth, c * 0.9, 0, 3), 0))
        parts.append((rbox("Pages", -t / 2 + c - 0.4, t / 2 - c + 0.4, -d / 2 + 1.5 * c, d / 2 - 5, 6, h - 6, pg, 0.8, 0, 1), 1))
        return parts
    return build

# ---- batch 4: cutlery, the phone, the remote, the matchbox, the kid's board books
def strip(name, x0, x1, wfun, zfun, th, mat, n=40, yc=0.0, bev=0.8):
    """a flat bar along X: half-width wfun(x), bottom height zfun(x), thickness th (uu); smooth, edges eased"""
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    xs = [x0 + (x1 - x0) * i / n for i in range(n + 1)]
    rows = []
    for x in xs:
        w = max(0.4, wfun(x)); z = zfun(x)
        rows.append([bm.verts.new(u(x, yc + sy * w, z + sz * th)) for sy, sz in ((-1, 0), (1, 0), (1, 1), (-1, 1))])
    for a, b in zip(rows, rows[1:]):
        for k in range(4): bm.faces.new((a[k], b[k], b[(k + 1) % 4], a[(k + 1) % 4]))
    bm.faces.new(rows[0][::-1]); bm.faces.new(rows[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); bm.to_mesh(me); bm.free()
    o = H.link(bpy.data.objects.new(name, me)); o.data.materials.append(mat); H.smooth(o, 0, bev * K, 2); return o

def rrect(name, x0, x1, y0, y1, z0, z1, r, mat, seg=6, bev=1.0):
    """a rounded-rectangle slab (corner radius r in plan), its top and bottom edges eased by bev"""
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    pts = []
    for cx, cy, a0 in ((x1 - r, y1 - r, 0), (x0 + r, y1 - r, 90), (x0 + r, y0 + r, 180), (x1 - r, y0 + r, 270)):
        for k in range(seg + 1):
            a = math.radians(a0 + 90 * k / seg); pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    lo = [bm.verts.new(u(x, y, z0)) for x, y in pts]; hi = [bm.verts.new(u(x, y, z1)) for x, y in pts]
    bm.faces.new(lo[::-1]); bm.faces.new(hi)
    for i in range(len(pts)):
        j = (i + 1) % len(pts); bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); bm.to_mesh(me); bm.free()
    o = H.link(bpy.data.objects.new(name, me)); o.data.materials.append(mat); H.smooth(o, 0, bev * K, 2); return o

def top_print(name, img, x0, x1, y0, y1, base=(0.05, 0.05, 0.055), rough=0.3, metal=0.0, coat=0.0, porosity=0.1):
    """a material whose top faces carry an image (x0..x1 by y0..y1; its alpha over the base colour)"""
    m, p = H.mat(name); tc, sp = _sep(m)
    nrm = N(m, "ShaderNodeSeparateXYZ", -1400, -900); L(m, tc.outputs["Normal"], nrm.inputs["Vector"])
    top = _math(m, "GREATER_THAN", nrm.outputs["Z"], 0.6, -1200, -900)
    uu = _maprange(m, sp.outputs["X"], x0 * K, x1 * K, 0.0, 1.0, False, -1000, -1000)
    vv = _maprange(m, sp.outputs["Y"], y0 * K, y1 * K, 0.0, 1.0, False, -1000, -1100)
    t = _img(m, os.path.join(LAB, img), uu, vv, -700, -1000)
    a = _math(m, "MULTIPLY", t.outputs["Alpha"], top, -500, -1000)
    L(m, _over(m, base, t.outputs["Color"], a, -200, -900), p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = rough; p.inputs["Metallic"].default_value = metal; p.inputs["Coat Weight"].default_value = coat
    return H.tag(m, metal, porosity)

def worn_keys(m):
    """rubber keys worn shiny and paler where thumbs press: the middle of each top (noise mask on the top faces)"""
    p = m.node_tree.nodes["Principled BSDF"]
    tc = N(m, "ShaderNodeTexCoord", -1600, 600); no = N(m, "ShaderNodeTexNoise", -1400, 600); no.inputs["Scale"].default_value = 260; no.inputs["Detail"].default_value = 4
    L(m, tc.outputs["Object"], no.inputs["Vector"])
    w = _maprange(m, no.outputs["Fac"], 0.45, 0.7, 0.0, 1.0, True, -1200, 600)
    r = _maprange(m, w, 0.0, 1.0, 0.62, 0.3, True, -1000, 600); L(m, r, p.inputs["Roughness"])
    return m

def mats4(M):
    M["cutlery"] = H.metal("Cutlery", (0.84, 0.85, 0.86), 0.18)
    M["handle"] = H.plastic("KnifeHandle", (0.03, 0.03, 0.03), 0.35, peel=0.02, scuff=0.15)
    M["phoneScreen"] = top_print("PhoneScreen", "phone_screen.png", -78, 78, -152, 152, (0.02, 0.02, 0.025), 0.05, 0.0, 1.0, 0.0)
    M["phoneFrame"] = H.metal("PhoneFrame", (0.55, 0.56, 0.60), 0.3)
    M["phoneBack"] = H.plastic("PhoneBack", (0.10, 0.12, 0.16), 0.15, peel=0.01, scuff=0.08)
    M["remote"] = top_print("RemotePrint", "remote_body.png", -300, 300, -100, 100, (0.035, 0.035, 0.04), 0.5, 0.0, 0.0, 0.1)
    M["keys"] = worn_keys(top_print("RemoteKeys", "remote_keys.png", -300, 300, -100, 100, (0.17, 0.17, 0.19), 0.6, 0.0, 0.0, 0.2))
    M["keysRed"] = worn_keys(top_print("RemoteKeysRed", "remote_keys.png", -300, 300, -100, 100, (0.55, 0.05, 0.04), 0.55, 0.0, 0.0, 0.2))
    for k, c in (("keyR", (0.6, 0.06, 0.05)), ("keyG", (0.08, 0.45, 0.12)), ("keyY", (0.75, 0.6, 0.08)), ("keyB", (0.08, 0.2, 0.6))):
        M[k] = H.rubber("Key" + k, c, 0.55)
    M["remoteBack"] = H.plastic("RemoteBack", (0.045, 0.045, 0.05), 0.55, peel=0.02, scuff=0.25)
    M["irWindow"] = H.plastic("IRWindow", (0.12, 0.01, 0.02), 0.06, peel=0.0, scuff=0.02); H.tag(M["irWindow"], 0.0, 0.0)
    M["rubber"] = H.rubber("ButtonRubber", (0.42, 0.43, 0.46), 0.55)
    M["redBtn"] = H.rubber("PowerRed", (0.55, 0.04, 0.03), 0.5)
    M["matchTop"] = top_print("MatchboxTop", "matchbox_top.png", -208, 208, -250, 250, (0.62, 0.48, 0.30), 0.7, 0.0, 0.0, 0.6)
    M["striker"] = H.rubber("Striker", (0.24, 0.13, 0.08), 0.95); _bump(M["striker"], M["striker"].node_tree.nodes["Principled BSDF"], 800.0, 0.6, 0.0004)
    M["card"] = H.plastic("TrayCard", (0.70, 0.58, 0.40), 0.85, peel=0.1, scuff=0.1); H.tag(M["card"], 0.0, 0.7)
    M["stick"] = H.wood("MatchWood", (0.86, 0.74, 0.52), (0.72, 0.58, 0.38), 60, 0.0)
    M["head"] = H.rubber("MatchHead", (0.50, 0.06, 0.04), 0.8)
    return M

def fork(M):
    def w(x):   # the outline: tines' head at -X, the handle widens to +X
        if x < -300: return 60
        if x < -220: return 60 - (x + 300) * 0.5
        if x < -60: return 20
        return 20 + (x + 60) / 510 * 22
    def z(x): return 2 + 10 * max(0.0, (x - 200) / 250) ** 2 + 6 * max(0.0, (-260 - x) / 190) ** 2
    parts = [(strip("Fork", -300, 450, w, z, 5.0, M["cutlery"], 60), 0)]
    for k in range(4):   # the tines
        yc = -45 + k * 30
        parts.append((strip("Tine", -450, -298, lambda x: 7 if x > -430 else 7 * max(0.2, (x + 450) / 20), z, 5.0, M["cutlery"], 16, yc, 0.6), 0))
    return parts

def knife(M):
    def w(x):
        if x < -300: return 40 * math.sqrt(max(0.0, (x + 425) / 125))
        return 40
    blade = strip("Blade", -425, 178, w, lambda x: 4 + (x + 425) / 600 * 9, 3.5, M["cutlery"], 50, -2, 0.6)
    hdl = rrect("Handle", 175, 425, -40, 40, 0, 30, 18, M["handle"], 6, 6)
    parts = [(blade, 0), (hdl, 0)]
    for x in (220, 300, 380):
        o = H.cyl("Rivet", 7 * K, 31 * K, u(x, 0, 15), seg=16); o.data.materials.append(M["cutlery"]); parts.append((o, 0))
    return parts

def phone(M):
    return [(rrect("Frame", -80, 80, -155, 155, 0.6, 17.5, 22, M["phoneFrame"], 8, 2.5), 0),
            (rrect("Screen", -78, 78, -153, 153, 17.4, 18.6, 21, M["phoneScreen"], 8, 0.6), 0),
            (rrect("Back", -78, 78, -153, 153, 0, 1.0, 21, M["phoneBack"], 8, 0.4), 0),
            (rrect("Camera", -66, -22, 96, 140, 0.2, 1.2, 10, M["phoneBack"], 4, 0.3), 0)]

def remote_ctl(M):
    parts = [(rrect("Lower", -300, 300, -98, 98, 0, 16.2, 70, M["remote"], 10, 6), 0),
             (rrect("Seam", -299, 299, -97, 97, 16.0, 18.0, 69, M["remote"], 8, 0.2), 0),     # the parting line between the halves
             (rrect("Upper", -300, 300, -98, 98, 17.8, 34, 70, M["remote"], 10, 7), 0),
             (rrect("IR", 286, 301.5, -26, 26, 13, 27, 8, M["irWindow"], 4, 1.5), 0)]
    # the battery cover underneath: a recessed lid with grip ridges (pops off when it breaks)
    cov = [rrect("Cover", -270, -40, -72, 72, -0.6, 1.2, 10, M["remoteBack"], 4, 0.4)]
    for k in range(6): cov.append(rbox("Rib", -70 + k * 4.5, -68 + k * 4.5, -40, 40, -1.4, -0.4, M["remoteBack"], 0.3, 0, 1))
    parts += [(o, 1) for o in cov]
    keys = ([("num", -80 - 60 * c, 50 - 50 * r) for r in range(3) for c in range(3)] + [("num", -255, 0)])
    # round keys as rounded squares bevelled to a disc: their flat tops carry the printed symbol (a turned key's top is a fan of
    # slivers round the axis, the bake smears the digit into streaks)
    for kind, x, y in keys:
        parts.append((rrect("Key", x - 18.5, x + 18.5, y - 18.5, y + 18.5, 30, 44.5, 18, M["keys"], 4, 3.5), 0))
    parts.append((rrect("Power", 232, 268, 37, 73, 30, 44, 18, M["keysRed"], 4, 3.5), 0))
    parts.append((rrect("Mute", 234, 266, -71, -39, 30, 43, 16, M["keys"], 4, 3.0), 0))
    parts.append((rrect("Ok", 116, 164, -24, 24, 30, 43.5, 24, M["keys"], 8, 3.5), 0))
    # the d-pad ring: a flat annulus (a turned ring's top is slivers, its printed arrows would smear)
    parts.append((slab("Ring", circle2(140, 0, 57, 32), [circle2(140, 0, 30, 24)], "z", 30, 11, M["keys"], 2.5), 0))
    for y in (60, -60):
        parts.append((rrect("Rock", 8, 72, y - 17, y + 17, 30, 41.5, 14, M["keys"], 4, 2.5), 0))
    for y, col in ((-66, "keyR"), (-22, "keyG"), (22, "keyY"), (66, "keyB")):
        parts.append((rrect("Col", -34, -6, y - 14, y + 14, 30, 39.5, 6, M[col], 3, 1.5), 0))
    for o, _ in parts:   # (the screw's default 1 cm merge swallows a key under 2 cm across whole)
        if o.modifiers.get("screw"): o.modifiers["screw"].merge_threshold = 1e-6
    return parts

def matchbox(M):
    t = 8.0
    return [(rbox("Top", -208, 208, -250, 250, 168 - t, 168, M["matchTop"], 1.5, 0, 1), 0),
            (rbox("Bottom", -208, 208, -250, 250, 0, t, M["card"], 1.5, 0, 1), 0),
            (rbox("SideL", -208, -208 + t, -250, 250, t - 0.5, 168 - t + 0.5, M["striker"], 1.0, 0, 1), 0),
            (rbox("SideR", 208 - t, 208, -250, 250, t - 0.5, 168 - t + 0.5, M["striker"], 1.0, 0, 1), 0)]

def matchtray(M):
    t = 5.0; R = random.Random(9)
    parts = [(rbox("Floor", -180, 180, -488, 0, 0, t, M["card"], 1.0, 0, 1), 0)]
    for x0, x1, y0, y1 in ((-180, -180 + t, -488, 0), (180 - t, 180, -488, 0), (-180, 180, -488, -488 + t), (-180, 180, -t, 0)):
        parts.append((rbox("Wall", x0, x1, y0, y1, t - 0.3, 108, M["card"], 1.0, 0, 1), 0))
    for row in range(2):
        for k in range(13):   # the matches, heads alternating ends
            x = -160 + k * 26 + R.uniform(-3, 3); z = t + 6 + row * 13; flip = (k + row) % 2
            y0, y1 = (-470, -40) if flip else (-450, -20)
            parts.append((rbox("Stick", x - 5.5, x + 5.5, y0, y1, z - 5.5, z + 5.5, M["stick"], 1.2, 0, 1), 0))
            hy = y0 - 6 if flip else y1 + 6
            hd = H.cyl("Head", 9 * K, 22 * K, u(x, hy, z), (math.pi / 2, 0, 0), 8); H.smooth(hd, 0, 2.5 * K, 1); hd.data.materials.append(M["head"])
            parts.append((hd, 0))
    return parts

def kidbooks_build(M):
    parts = []
    for i, (dx, dy) in enumerate(((-4, 3), (5, -4), (-2, -6))):
        z0 = i * 80
        mat = M.get("kid%d" % i) or print_box("KidBook%d" % i, {"front": "kidbook_edge.png", "back": "kidbook_edge.png", "side": "kidbook_edge.png",
                                                               "side2": "kidbook_spine_%d.png" % i, "top": "kidbook_%d.png" % i}, -120 + dx, 120 + dx, -120 + dy, 120 + dy, z0, z0 + 78, 0.45)
        M["kid%d" % i] = mat
        parts.append((rrect("Kid%d" % i, -120 + dx, 120 + dx, -120 + dy, 120 + dy, z0, z0 + 78, 16, mat, 6, 6), 0))
    return parts

def breaks_book(o, n):
    # torn across (paper: a ragged, fibrous line), cloth and pages each cut by the same tear
    return cut_groups(o, n, [lambda p, g: p == 0, lambda p, g: p == 1], [], [((0, 30, 300), (0.05, 0.35, 1.0), 0.008, 101, 0)])

# ------------------------------------------------------------------------------------------------ hulls and break pieces
def ring(r_uu, z_uu, n=12): return H.ring_pts(r_uu * K, z_uu * K, n)
def boxp(x0, x1, y0, y1, z0, z1): return [(x * K, y * K, z * K) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)]

def hulls_plate(n):
    hs = [H.hull("UCX_%s_00" % n, ring(250, 0, 16) + ring(266, 18, 16))]
    for k in range(8):   # the rim in eight segments (the dish stays open for whatever lands in it)
        pts = []
        for a in (2 * math.pi * k / 8, 2 * math.pi * (k + 1) / 8):
            for r, z in ((262, 8), (262, 19), (450, 47), (440, 51)):
                pts.append((r * K * math.cos(a), r * K * math.sin(a), z * K))
        hs.append(H.hull("UCX_%s_%02d" % (n, k + 1), pts))
    return hs

def hulls_round(n, r, h, cap_r=None, cap_z=None):
    hs = [H.hull("UCX_%s_00" % n, ring(r, 0, 16) + ring(r, h, 16))]
    if cap_r: hs.append(H.hull("UCX_%s_01" % n, ring(cap_r, cap_z[0], 16) + ring(cap_r, cap_z[1], 16)))
    return hs

def hulls_mug(n):
    return [H.hull("UCX_%s_00" % n, ring(170, 0, 16) + ring(180, 300, 16)),
            H.hull("UCX_%s_01" % n, boxp(170, 307, -15, 15, 60, 255))]

def _copy(o, name):
    c = o.copy(); c.data = o.data.copy(); c.name = name; c.data.name = name; H.link(c); return c

def shards(lod0, name, cuts):
    """cuts: [(co_uu, normal, jag_m, seed, which)] - each cut splits piece 'which' (index into the list so far) in two"""
    pcs = [_copy(lod0, name + "_Brk0")]
    for k, (co, no, jag, seed, which) in enumerate(cuts):
        a, b = H.split(pcs[which], H.cutter(u(*co), no, 0.6, jag, seed, sub=6), pcs[which].name, "%s_Brk%d" % (name, len(pcs)))
        pcs[which] = a; pcs.append(b)
    return [x for x in pcs if len(x.data.polygons)]

def keep_faces(src, pred, name):
    """a copy of src with only the faces pred(part, glass) keeps"""
    o = _copy(src, name); bm = bmesh.new(); bm.from_mesh(o.data)
    lp = bm.faces.layers.int.get("part"); lg = bm.faces.layers.int.get("glass")
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if not pred(f[lp] if lp else 0, f[lg] if lg else 0)], context="FACES")
    bm.to_mesh(o.data); bm.free()
    a = o.data.attributes.get("surf") or o.data.attributes.new("surf", "INT", "FACE")
    for i in range(len(o.data.polygons)): a.data[i].value = 1
    return o

def cut_groups(lod0, name, cut, keep, cuts, attach0=()):
    """every closed solid group in 'cut' (predicates) is split by the same cuts (so its pieces line up with the others'), then
    piece i of every group is one break piece; each group in 'keep' stays whole, a piece of its own (a cap, a handle).
    (Cut per group: overlapping solids - a cap over a neck - confuse the boolean when cut together.)"""
    sets = []
    for gi, pred in enumerate(cut):
        g = keep_faces(lod0, pred, "%s_g%d" % (name, gi))
        if not len(g.data.polygons): bpy.data.objects.remove(g); continue
        pcs = [g]
        for k, (co, no, jag, seed, which) in enumerate(cuts):
            src = pcs[which]
            if src is None or not len(src.data.polygons): pcs.append(None); continue
            a, b = H.split(src, H.cutter(u(*co), no, 0.6, jag, seed, sub=6), src.name, "%s_g%d_%d" % (name, gi, len(pcs)))
            pcs[which] = a; pcs.append(b)
        sets.append(pcs)
    out = []
    for i in range(len(cuts) + 1):
        objs = [s[i] for s in sets if s[i] is not None and len(s[i].data.polygons)]
        for s in sets:
            if s[i] is not None and not len(s[i].data.polygons): bpy.data.objects.remove(s[i])
        if i == 0:   # (whole parts that stay on the first piece: the trap's hardware on its board)
            for pred in attach0:
                g = keep_faces(lod0, pred, "%s_a" % name)
                if len(g.data.polygons): objs.append(g)
                else: bpy.data.objects.remove(g)
        if objs: out.append(H.join(objs, "%s_Brk%d" % (name, len(out))))
    for pred in keep:
        g = keep_faces(lod0, pred, "%s_Brk%d" % (name, len(out)))
        if len(g.data.polygons): out.append(g)
        else: bpy.data.objects.remove(g)
    return out

def _vol(o):
    if o is None or not len(o.data.polygons): return 0.0
    mn, mx = H.bounds([o]); d = mx - mn; return d.x * d.y * d.z + 1e-12

def shatter(lod0, name, shell, along=(), keep=(), n=16, seed=1, jag=0.0006, neck=None, chips=10, chip_mat_glass=True):
    """real shattering: 'shell' (the glass / the china, a predicate on (part, glass)) is split again and again by random planes through
    its biggest piece (sharp, small jag) into n+1 shards; the 'along' groups (the label on it) take the very same cuts, so its scraps stay
    on their shards; 'neck' = (co_uu, normal): first cut off and kept whole (a bottle's neck); 'keep' groups stay whole (a cap);
    plus one piece of small chips round about"""
    R = random.Random(seed)
    g = keep_faces(lod0, shell, name + "_sh")
    cuts = []
    pcs = [g]; locked = set()
    if neck:
        a, b = H.split(g, H.cutter(u(*neck[0]), neck[1], 0.6, jag, seed + 1, sub=5), g.name, name + "_neck")
        pcs = [a, b]; locked.add(1); cuts.append((neck[0], neck[1], jag, seed + 1, 0))
    for k in range(n):
        idx = max((i for i in range(len(pcs)) if i not in locked), key=lambda i: _vol(pcs[i]))
        src = pcs[idx]; mn, mx = H.bounds([src]); cc = (mn + mx) * 0.5; dd = (mx - mn) * 0.3
        co = Vector((cc.x + R.uniform(-dd.x, dd.x), cc.y + R.uniform(-dd.y, dd.y), cc.z + R.uniform(-dd.z, dd.z))) / K
        no = Vector((R.uniform(-1, 1), R.uniform(-1, 1), R.uniform(-1, 1))); no = no.normalized() if no.length > 0.1 else Vector((1, 0, 0))
        sd = seed * 100 + k
        a, b = H.split(src, H.cutter(u(*co), no, 0.6, jag, sd, sub=5), src.name, "%s_sh%d" % (name, len(pcs)))
        pcs[idx] = a; pcs.append(b); cuts.append((tuple(co), tuple(no), jag, sd, idx))
    sets = [pcs]
    for gi, pred in enumerate(along):   # the label: the same cuts, piece by piece
        lg = keep_faces(lod0, pred, "%s_al%d" % (name, gi))
        if not len(lg.data.polygons): bpy.data.objects.remove(lg); continue
        lp = [lg]
        for (co, no, j, sd, idx) in cuts:
            src = lp[idx]
            if src is None or not len(src.data.polygons): lp.append(None); continue
            a, b = H.split(src, H.cutter(u(*co), no, 0.6, j, sd, sub=5), src.name, "%s_al%d_%d" % (name, gi, len(lp)))
            lp[idx] = a; lp.append(b)
        sets.append(lp)
    out = []
    for i in range(len(pcs)):
        objs = [s[i] for s in sets if i < len(s) and s[i] is not None and len(s[i].data.polygons)]
        if objs: out.append(H.join(objs, "%s_Brk%d" % (name, len(out))))
    for pred in keep:
        kg = keep_faces(lod0, pred, "%s_Brk%d" % (name, len(out)))
        if len(kg.data.polygons): out.append(kg)
        else: bpy.data.objects.remove(kg)
    if chips:   # small chips of the shell round about (one piece, many islands)
        mn, mx = H.bounds([lod0]); bm = bmesh.new()
        for k in range(chips):
            r = R.uniform(0.15, 0.45) * min(0.012, 0.08 * (mx - mn).length)
            cc = Vector((R.uniform(mn.x, mx.x), R.uniform(mn.y, mx.y), R.uniform(0.002, 0.004)))
            ico = bmesh.ops.create_icosphere(bm, subdivisions=1, radius=r)
            sc = Matrix.Diagonal((R.uniform(0.6, 1.4), R.uniform(0.6, 1.4), 0.18, 1))
            bmesh.ops.transform(bm, matrix=Matrix.Translation(cc) @ Matrix.Rotation(R.uniform(0, 6.3), 4, "Z") @ sc, verts=ico["verts"])
        me = bpy.data.meshes.new(name + "_chips"); bm.to_mesh(me); bm.free()
        ch = H.link(bpy.data.objects.new("%s_Brk%d" % (name, len(out)), me))
        for m in lod0.data.materials: ch.data.materials.append(m)
        gl = ch.data.attributes.new("glass", "INT", "FACE")
        gi_ = 1 if chip_mat_glass else 0
        for i, p in enumerate(ch.data.polygons): gl.data[i].value = gi_; p.material_index = 1 if (chip_mat_glass and len(ch.data.materials) > 1) else 0
        uv = ch.data.uv_layers.new(name="UV0")
        for l in ch.data.loops: uv.data[l.index].uv = (0.02, 0.02)
        ch["cuts"] = []
        out.append(ch)
    print("HERO shatter %s: %d pieces (%d cuts)" % (name, len(out), len(cuts)))
    return out

def breaks_plate(o, n):
    return shatter(o, n, lambda p, g: True, n=7, seed=11, jag=0.0008, chips=6, chip_mat_glass=False)

def breaks_plate_old(o, n):
    return shards(o, n, [((10, 0, 0), (1.0, 0.25, 0.0), 0.0012, 31, 0), ((0, -20, 0), (0.15, 1.0, 0.0), 0.0012, 37, 0),
                         ((-10, 30, 0), (-0.2, 1.0, 0.0), 0.0012, 41, 1)])

GLASS_CUTS = [((0, 0, 0.55), (0.10, 0.06, 1.0), 0.0012, 43, 0), ((5, 0, 0.3), (1.0, 0.35, 0.25), 0.0014, 47, 0),
              ((-5, 0, 0.75), (0.9, -0.6, 0.3), 0.0014, 53, 1), ((0, 8, 0.4), (-0.3, 1.0, -0.2), 0.0012, 57, 0)]

def glass_cuts(h_uu, scale=1.0):
    """the glass cuts of a jar h_uu tall (z given as a fraction of it): a band round the shoulder, then vertical-ish splits"""
    return [((x * scale, y * scale, z * h_uu), no, jag * scale, seed, w) for (x, y, z), no, jag, seed, w in GLASS_CUTS]

def breaks_spice(o, n):
    # the glass and its label shatter together; the cap stays whole; the paprika (part 2) is not a piece: it spills (AntWreck)
    return shatter(o, n, lambda p, g: g == 1, [lambda p, g: g == 0 and p == 0], [lambda p, g: p == 1], n=13, seed=3, jag=0.0004, chips=8)

def breaks_mug(o, n):
    return shatter(o, n, lambda p, g: p == 0, [], [lambda p, g: p == 1], n=6, seed=13, jag=0.0008, chips=5, chip_mat_glass=False)

def breaks_pasta(o, n):
    # torn across (paper: a ragged, fibrous line); the glued flap goes with the top
    return cut_groups(o, n, [lambda p, g: p == 0], [], [((0, 0, 470), (0.12, 0.0, 1.0), 0.010, 59, 0)])

def breaks_jam(o, n):
    return shatter(o, n, lambda p, g: g == 1, [lambda p, g: g == 0 and p == 0], [lambda p, g: p == 1], n=16, seed=5, jag=0.0006, chips=10)

def ring_y(r_uu, y_uu, zc_uu, n=12):
    return [(r_uu * K * math.cos(2 * math.pi * k / n), y_uu * K, (zc_uu + r_uu * math.sin(2 * math.pi * k / n)) * K) for k in range(n)]

def hulls_tin(n):
    return [H.hull("UCX_%s_00" % n, ring_y(123, -200, 123, 16) + ring_y(123, 202, 123, 16)),
            H.hull("UCX_%s_01" % n, boxp(-116, 116, -430, -198, 170, 250))]

def hulls_bottle(n):
    return [H.hull("UCX_%s_00" % n, ring_y(122, 25, 122, 16) + ring_y(122, 550, 122, 16)),
            H.hull("UCX_%s_01" % n, ring_y(119, 25, 122, 12) + ring_y(56, -115, 122, 12)),
            H.hull("UCX_%s_02" % n, ring_y(56, -115, 122, 12) + ring_y(52, -550, 122, 12))]

def hulls_cleaner(n):
    oval = lambda r, z: [(p[0], p[1] * 0.8, p[2]) for p in ring(r, z, 16)]
    return [H.hull("UCX_%s_00" % n, oval(162, 0) + oval(162, 505) + oval(96, 605) + oval(64, 662)), H.hull("UCX_%s_01" % n, ring(68, 658, 12) + ring(66, 770, 12))]

def breaks_bottle(o, n):
    cuts = [((0, -120, 122), (0.15, 1.0, 0.2), 0.0016, 71, 0), ((8, 260, 122), (0.25, 1.0, -0.35), 0.0018, 73, 0),
            ((0, 120, 150), (0.1, 0.25, 1.0), 0.0016, 79, 2), ((10, -340, 122), (0.3, 1.0, 0.1), 0.0012, 83, 1)]
    return shatter(o, n, lambda p, g: g == 1, [lambda p, g: g == 0], [], n=18, seed=7, jag=0.0008, neck=((0, -380, 122), (0.0, 1.0, 0.05)), chips=12)

SPECS = {
    "SM_K_Plate": dict(build=plate, hulls=hulls_plate, lods=(0.45, 0.18), breaks=breaks_plate, cls="ceramic"),
    "SM_K_SpiceJar": dict(build=spicejar, hulls=lambda n: hulls_round(n, 44, 118, 41, (118, 150)), lods=(0.45, 0.18), breaks=breaks_spice, cls="glass", glass=0.14),
    "SM_K_Mug": dict(build=mug, hulls=hulls_mug, lods=(0.45, 0.18), breaks=breaks_mug, cls="ceramic"),
    "SM_K_PastaBox": dict(build=pastabox, hulls=lambda n: [H.hull("UCX_%s_00" % n, boxp(-150, 150, -102, 100, 0, 750))], lods=(0.5, 0.25), breaks=breaks_pasta, cls="cardboard"),
    "SM_K_JarJam": dict(build=jarjam, hulls=lambda n: hulls_round(n, 140, 236, 128.5, (236, 291.5)), lods=(0.45, 0.18), breaks=breaks_jam, cls="glass", glass=0.12),
    # batch 2
    "SM_K_TinCan": dict(build=tincan, hulls=hulls_tin, lods=(0.45, 0.2), cls="metal",
                        breaks=lambda o, n: cut_groups(o, n, [], [lambda p, g: p == 0, lambda p, g: p == 1], [])),          # the lid tears off
    "SM_K_Bottle": dict(build=bottle, hulls=hulls_bottle, lods=(0.45, 0.18), breaks=breaks_bottle, cls="glass", glass=0.22, glassmat="bottleGlass"),
    "SM_K_MouseTrap": dict(build=mousetrap, hulls=lambda n: [H.hull("UCX_%s_00" % n, boxp(-190, 190, -90, 90, 0, 16))], lods=(0.45, 0.2), cls="wood",
                           breaks=lambda o, n: cut_groups(o, n, [lambda p, g: p == 0], [], [((-70, 0, 8), (1.0, 0.18, 0.0), 0.0025, 89, 0)], [lambda p, g: p == 1])),
    "SM_K_MouseTrap_Bar": dict(build=trapbar, hulls=lambda n: [H.hull("UCX_%s_00" % n, boxp(-176, 4, -87, 87, -3, 8))], lods=(0.5, 0.25), cls="metal", breaks=None),
    "SM_K_Sponge": dict(build=sponge, hulls=lambda n: [H.hull("UCX_%s_00" % n, boxp(-150, 150, -150, 150, 0, 150))], lods=(0.45, 0.2), cls="foam",
                        breaks=lambda o, n: cut_groups(o, n, [lambda p, g: p == 0, lambda p, g: p == 1], [], [((0, 20, 75), (1.0, 0.2, 0.05), 0.008, 97, 0)])),
    "SM_K_Detergent": dict(build=cleaner, hulls=hulls_cleaner, lods=(0.45, 0.2), cls="plastic",
                           breaks=lambda o, n: cut_groups(o, n, [], [lambda p, g: p == 0, lambda p, g: p == 1], [])),     # the cap screws off
}
SPECS.update({   # batch 4
    "SM_K_Fork": dict(build=fork, hulls=lambda n: [H.hull("UCX_%s_00" % n, boxp(-450, 450, -60, 60, 0, 17))], lods=(0.5, 0.25), cls="metal", breaks=None),
    "SM_K_Knife": dict(build=knife, hulls=lambda n: [H.hull("UCX_%s_00" % n, boxp(-425, 175, -42, 42, 0, 18)), H.hull("UCX_%s_01" % n, boxp(175, 425, -40, 40, 0, 31))],
                       lods=(0.5, 0.25), cls="metal", breaks=None),
    "SM_V3_Phone": dict(build=phone, hulls=lambda n: [H.hull("UCX_%s_00" % n, boxp(-80, 80, -155, 155, 0, 18.6))], lods=(0.5, 0.25), cls="plastic", breaks=None),
    "SM_L_Remote": dict(build=remote_ctl, uv_cap=0.35, hulls=lambda n: [H.hull("UCX_%s_00" % n, boxp(-300, 300, -98, 98, 0, 42))], lods=(0.5, 0.25), cls="plastic",
                        breaks=lambda o, n: cut_groups(o, n, [], [lambda p, g: p == 0, lambda p, g: p == 1], [])),          # the battery cover pops off
    "SM_K_Matchbox": dict(build=matchbox, lods=(0.5, 0.25), cls="cardboard", breaks=None,
                          hulls=lambda n: [H.hull("UCX_%s_%02d" % (n, i), boxp(*b)) for i, b in enumerate(((-208, 208, -250, 250, 160, 168), (-208, 208, -250, 250, 0, 8),
                                                                                                             (-208, -200, -250, 250, 0, 168), (200, 208, -250, 250, 0, 168)))]),
    "SM_K_Matchbox_Tray": dict(build=matchtray, lods=(0.5, 0.25), cls="cardboard", breaks=None,
                               hulls=lambda n: [H.hull("UCX_%s_%02d" % (n, i), boxp(*b)) for i, b in enumerate(((-180, 180, -488, 0, 0, 30), (-180, -175, -488, 0, 0, 108),
                                                                                                                  (175, 180, -488, 0, 0, 108), (-180, 180, -488, -483, 0, 108), (-180, 180, -5, 0, 0, 108)))]),
    "SM_V31_BoardBooks": dict(build=kidbooks_build, hulls=lambda n: [H.hull("UCX_%s_00" % n, boxp(-125, 125, -126, 123, 0, 238))], lods=(0.5, 0.25), cls="cardboard",
                              breaks=lambda o, n: cut_groups(o, n, [lambda p, g: True], [], [((0, 0, 120), (1.0, 0.3, 0.1), 0.006, 103, 0)])),
})
for _k, (_t, _d, _h) in BOOKDIM.items():   # batch 3: the books
    SPECS["SM_L_Book_" + _k] = dict(build=make_book(_k), lods=(0.5, 0.25), cls="paper", breaks=breaks_book,
                                    hulls=lambda n, t=_t, d=_d, h=_h: [H.hull("UCX_%s_00" % n, [(x, y, z) for x in (-t / 2, t / 2) for y in (-d / 2, d / 2) for z in (0, h)])])

# ---- batch 5: dishes and pans (the bowl, the dog's bowl, the soup pot and its lid, the kettle, the salt shaker, the sugar jar)
def lathe_s(name, prof_uu, mat, steps=64, sub=0, at=(0, 0, 0)):
    """lathe() for small turned parts: the screw's default 1 cm merge would swallow a ring under 2 cm across whole"""
    o = lathe(name, prof_uu, mat, steps, sub, at); o.modifiers["screw"].merge_threshold = 1e-5; return o

def enamel(name, outer, inner=None, chips=0.74, soot_z=None, rough=0.1):
    """vitreous enamel on steel: glossy, its own colour inside (faces turned to the axis, the floor), chipped to the black iron here
    and there (chips: the noise level above which the enamel is off), soot from the burner below soot_z"""
    m, p = H.mat(name); p.inputs["Coat Weight"].default_value = 0.6; p.inputs["Coat Roughness"].default_value = 0.04
    tc, sp = _sep(m)
    col = outer
    if inner:
        rad = N(m, "ShaderNodeCombineXYZ", -1300, 400); L(m, sp.outputs["X"], rad.inputs["X"]); L(m, sp.outputs["Y"], rad.inputs["Y"])
        dt = N(m, "ShaderNodeVectorMath", -1150, 400); dt.operation = "DOT_PRODUCT"; L(m, tc.outputs["Normal"], dt.inputs[0]); L(m, rad.outputs["Vector"], dt.inputs[1])
        side_in = _math(m, "LESS_THAN", dt.outputs["Value"], 0.0, -1000, 400)
        nrm = N(m, "ShaderNodeSeparateXYZ", -1300, 600); L(m, tc.outputs["Normal"], nrm.inputs["Vector"])
        up = _math(m, "GREATER_THAN", nrm.outputs["Z"], 0.5, -1150, 600)
        above = _math(m, "GREATER_THAN", sp.outputs["Z"], 3 * K, -1150, 700)
        inn = _math(m, "MAXIMUM", side_in, _math(m, "MULTIPLY", up, above, -1000, 600), -850, 500)
        rgb = N(m, "ShaderNodeRGB", -700, 600); rgb.outputs[0].default_value = (*inner, 1)
        col = _over(m, outer, rgb.outputs[0], inn, -550, 400)
    if soot_z:
        sz = _maprange(m, sp.outputs["Z"], 0.0, soot_z * K, 0.85, 0.0, True, -1000, 800)
        rgb2 = N(m, "ShaderNodeRGB", -700, 900); rgb2.outputs[0].default_value = (0.025, 0.02, 0.018, 1)
        col = _over(m, col, rgb2.outputs[0], sz, -400, 700)
    no = N(m, "ShaderNodeTexNoise", -1300, -300); no.inputs["Scale"].default_value = 30; no.inputs["Detail"].default_value = 10
    L(m, tc.outputs["Object"], no.inputs["Vector"])
    ch = _maprange(m, no.outputs["Fac"], chips, chips + 0.012, 0.0, 1.0, True, -1100, -300)
    iron = N(m, "ShaderNodeRGB", -700, 200); iron.outputs[0].default_value = (0.10, 0.085, 0.075, 1)
    L(m, _over(m, col, iron.outputs[0], ch, -250, 200), p.inputs["Base Color"])
    L(m, _maprange(m, ch, 0.0, 1.0, rough, 0.7, True, -400, -400), p.inputs["Roughness"])
    bp = N(m, "ShaderNodeBump", -200, -600); bp.invert = True; bp.inputs["Strength"].default_value = 0.4; bp.inputs["Distance"].default_value = 0.0004
    L(m, ch, bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return H.tag(m, 0.0, 0.05)

def soup_mat(name):
    """broth: a warm orange, glossy, rings of fat floating on it"""
    m, p = H.mat(name); tc = N(m, "ShaderNodeTexCoord", -1100)
    no = N(m, "ShaderNodeTexNoise", -900, 200); no.inputs["Scale"].default_value = 25; no.inputs["Detail"].default_value = 5; L(m, tc.outputs["Object"], no.inputs["Vector"])
    cr = N(m, "ShaderNodeValToRGB", -700, 200); e = cr.color_ramp.elements; e[0].color = (0.36, 0.15, 0.04, 1); e[1].color = (0.58, 0.27, 0.07, 1)
    L(m, no.outputs["Fac"], cr.inputs["Fac"])
    vo = N(m, "ShaderNodeTexVoronoi", -900, -200); vo.inputs["Scale"].default_value = 60; L(m, tc.outputs["Object"], vo.inputs["Vector"])
    sd = N(m, "ShaderNodeValToRGB", -700, -200); s = sd.color_ramp.elements; s[0].position = 0.16; s[0].color = (1, 1, 1, 1); s[1].position = 0.22; s[1].color = (0, 0, 0, 1)
    L(m, vo.outputs["Distance"], sd.inputs["Fac"])
    fat = N(m, "ShaderNodeRGB", -500, -350); fat.outputs[0].default_value = (0.86, 0.58, 0.16, 1)
    L(m, _over(m, cr.outputs["Color"], fat.outputs[0], sd.outputs["Color"], -300, 0), p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = 0.04; p.inputs["Coat Weight"].default_value = 1.0
    return H.tag(m, 0.0, 0.0)

def holed_cap(name):
    """a steel shaker top: the holes are dark dots on its top face"""
    m = H.metal(name, (0.80, 0.80, 0.80), 0.25); p = m.node_tree.nodes["Principled BSDF"]
    tc, sp = _sep(m)
    vo = N(m, "ShaderNodeTexVoronoi", -1100, -500); vo.voronoi_dimensions = "2D"; vo.inputs["Scale"].default_value = 1.0 / (24 * K)
    L(m, tc.outputs["Object"], vo.inputs["Vector"])
    hole = _math(m, "LESS_THAN", vo.outputs["Distance"], 0.13, -900, -500)
    nrm = N(m, "ShaderNodeSeparateXYZ", -1300, -700); L(m, tc.outputs["Normal"], nrm.inputs["Vector"])
    up = _math(m, "GREATER_THAN", nrm.outputs["Z"], 0.8, -1100, -700)
    cxy = N(m, "ShaderNodeCombineXYZ", -1300, -900); L(m, sp.outputs["X"], cxy.inputs["X"]); L(m, sp.outputs["Y"], cxy.inputs["Y"])
    ln = N(m, "ShaderNodeVectorMath", -1150, -900); ln.operation = "LENGTH"; L(m, cxy.outputs["Vector"], ln.inputs[0])
    inr = _math(m, "LESS_THAN", ln.outputs["Value"], 40 * K, -1000, -900)
    a = _math(m, "MULTIPLY", _math(m, "MULTIPLY", hole, up, -800, -600), inr, -650, -600)
    blk = N(m, "ShaderNodeRGB", -500, -800); blk.outputs[0].default_value = (0.01, 0.01, 0.01, 1)
    L(m, _over(m, (0.80, 0.80, 0.80), blk.outputs[0], a, -300, -500), p.inputs["Base Color"])
    p.inputs["Metallic"].default_value = 1.0
    L(m, _math(m, "SUBTRACT", 1.0, a, -300, -700), p.inputs["Metallic"])
    return m

def mats5(M):
    M["bowl"] = ceramic("BowlGlaze", (0.90, 0.88, 0.80), 0.06, [(212, 224, (0.18, 0.42, 0.28))], 240.0, 4.0)
    M["buckwheat"] = powder("Leftovers", (0.46, 0.29, 0.14), (0.22, 0.12, 0.05))
    M["dogEnamel"] = enamel("DogBowlEnamel", (0.55, 0.05, 0.04), (0.93, 0.93, 0.90), 0.70)
    M["potSteel"] = H.metal("PotSteel", (0.82, 0.82, 0.81), 0.2)
    for _n in M["potSteel"].node_tree.nodes:   # (satin, not hammered: a big pot shows the fingerprint noise as blotches)
        if _n.bl_idname == "ShaderNodeMapRange": _n.inputs["To Max"].default_value = 0.26
    M["soup"] = soup_mat("Soup")
    M["carrot"] = H.plastic("Carrot", (0.85, 0.30, 0.04), 0.3, peel=0.05, scuff=0.1); H.tag(M["carrot"], 0.0, 0.4)
    M["potato"] = H.plastic("Potato", (0.86, 0.78, 0.55), 0.35, peel=0.08, scuff=0.1); H.tag(M["potato"], 0.0, 0.5)
    M["bakelite"] = H.plastic("Bakelite", (0.03, 0.025, 0.022), 0.3, peel=0.03, scuff=0.25)
    M["kettle"] = enamel("KettleEnamel", (0.60, 0.06, 0.04), None, 0.72, 70)
    M["shaker"] = mug_glaze("ShakerGlaze", (0.94, 0.93, 0.90), "salt.png", 40, 200, 74)
    M["shakerCap"] = holed_cap("ShakerCap")
    M["sugar"] = powder("SugarGrains", (0.98, 0.97, 0.95), (0.80, 0.79, 0.76))
    M["walnut"] = H.wood("WalnutLid", (0.42, 0.26, 0.14), (0.17, 0.09, 0.04), 26.0, 0.45)
    M["sugarLabel"] = label_cyl("LabelSugar", "sugar.png", 45, 140, 320, 0.75, (0.77, 0.63, 0.44), 0.75)
    return M

def bowl(M):
    prof = [(0, 6), (120, 6), (128, 2), (134, 0), (142, 0), (148, 4), (152, 12), (185, 40), (214, 80), (228, 112), (230, 118), (228, 121), (222, 121),
            (219, 116), (206, 82), (176, 46), (142, 26), (100, 21), (0, 20)]
    parts = [(lathe("Bowl", prof, M["bowl"], 72), 0)]
    parts.append((lathe("Leftovers", [(0, 41), (30, 40), (62, 36), (92, 31), (118, 26), (134, 22), (136, 19.5), (0, 19.5)], M["buckwheat"], 40, 0, (6, 5, 0)), 0))
    return parts

def petbowl(M):
    prof = [(0, 0), (320, 0), (334, 3), (342, 12), (420, 120), (462, 158), (476, 164), (480, 168), (477, 170.5), (470, 170), (458, 163), (414, 126),
            (336, 16), (326, 9), (0, 9)]
    return [(lathe("DogBowl", prof, M["dogEnamel"], 96), 0)]

def pot(M):
    prof = [(0, 0), (396, 0), (418, 4), (428, 14), (430, 32), (430, 504), (433, 508), (436, 512), (434, 515.5), (429, 514), (426, 508), (426, 32),
            (423, 24), (400, 18), (0, 18)]
    parts = [(lathe("Pot", prof, M["potSteel"], 96), 0)]
    parts.append((lathe("Soup", [(0, 300), (425.8, 300), (425.8, 394), (421, 390.5), (0, 390)], M["soup"], 72), 1))
    R = random.Random(23)
    for k in range(14):   # carrot coins and potato cubes afloat
        a = R.uniform(0, 6.28); r = R.uniform(40, 360); x, y = r * math.cos(a), r * math.sin(a)
        if k % 2:
            c = lathe("Carrot%d" % k, [(0, 395.5), (22, 395.5), (24, 392), (22, 386), (0, 386)], M["carrot"], 16, 0, (x, y, 0))
        else:
            c = rbox("Potato%d" % k, x - 18, x + 18, y - 18, y + 18, 380, 397, M["potato"], 4, 0, 2); c.rotation_euler.z = R.uniform(0, 1.5)
        parts.append((c, 1))
    for s in (1, -1):   # the loop handles, riveted
        pts = [Vector(u(s * x, y, z)) for x, y, z in ((420, -112, 448), (470, -104, 451), (514, -58, 454), (528, 0, 455), (514, 58, 454), (470, 104, 451), (420, 112, 448))]
        h = H.tubes("Handle", [pts], 13 * K, res_u=8, bevel_res=3, tip=1.0); h.data.materials.append(M["potSteel"]); parts.append((h, 0))
        for y in (-100, 100):
            parts.append((lathe_s("Rivet", [(0, 0), (9, 0), (8, 3), (5, 5), (0, 5.5)], M["potSteel"], 12, 0, (0, 0, 0)), 0))
            r = parts[-1][0]; r.rotation_euler = (0, s * math.pi / 2, 0); r.location = u(s * 431, y, 448)
    return parts

def potlid(M):
    dome = [(0, 64), (150, 61), (300, 49), (400, 27), (436, 9), (442, 3), (442, 0), (436, 0), (431, 5), (396, 22), (296, 44), (148, 56), (0, 59)]
    knob = [(0, 118), (48, 118), (60, 114), (66, 105), (62, 94), (40, 86), (32, 74), (40, 63), (0, 62)]
    return [(lathe("Lid", dome, M["potSteel"], 96), 0), (lathe_s("Knob", knob, M["bakelite"], 48), 1)]

def kettle(M):
    body = [(0, 0), (180, 0), (196, 4), (206, 12), (212, 30), (216, 80), (214, 160), (204, 230), (182, 290), (150, 330), (118, 350), (100, 358),
            (98, 366), (94, 366), (92, 358), (110, 348), (144, 326), (176, 286), (198, 228), (208, 160), (210, 80), (206, 32), (200, 18), (180, 10), (0, 10)]
    lid = [(18, 382), (50, 378), (84, 370), (100, 362), (102, 357), (96, 355), (84, 359), (50, 368), (0, 371)]
    knob = [(0, 412), (24, 412), (30, 406), (30, 396), (22, 388), (16, 381), (0, 380)]
    parts = [(lathe("Kettle", body, M["kettle"], 72), 0), (lathe_s("Lid", lid, M["kettle"], 56), 1), (lathe_s("Knob", knob, M["bakelite"], 32), 1)]
    sp = H.tubes("Spout", [[Vector(u(*p)) for p in ((150, 0, 100), (225, 0, 150), (288, 0, 232), (330, 0, 300), (343, 0, 318))]], 40 * K, res_u=10, bevel_res=4, tip=0.42)
    for pt, r in zip(sp.data.splines[0].points, (1.0, 0.82, 0.62, 0.48, 0.42)): pt.radius = r
    sp.data.materials.append(M["kettle"]); parts.append((sp, 0))
    hp = [(140, 0, 330), (136, 0, 410), (96, 0, 478), (20, 0, 498), (-60, 0, 497), (-126, 0, 468), (-160, 0, 400), (-166, 0, 322)]
    h = H.tubes("Handle", [[Vector(u(*p)) for p in hp]], 9 * K, res_u=10, bevel_res=3, tip=1.0); h.data.materials.append(M["kettle"]); parts.append((h, 0))
    g = H.tubes("Grip", [[Vector(u(*p)) for p in hp[2:6]]], 17 * K, res_u=10, bevel_res=3, tip=1.0); g.data.materials.append(M["bakelite"]); parts.append((g, 0))
    return parts

def shaker(M):
    body = [(0, 6), (60, 6), (66, 2), (72, 0), (77, 3), (80, 12), (80, 150), (78, 184), (72, 204), (64, 212), (62, 216), (0, 216)]
    cap = [(0, 266), (30, 266), (48, 263), (60, 256), (67, 245), (68, 232), (68, 214), (66, 211), (63, 212.5), (63, 218), (0, 219)]
    return [(lathe("Shaker", body, M["shaker"], 64), 0), (lathe_s("Cap", cap, M["shakerCap"], 64), 1)]

def jarsugar(M):
    g = [(0, 0), (220, 0), (238, 4), (248, 16), (250, 40), (250, 400), (246, 416), (236, 424), (232, 430), (232, 452), (229, 455), (226, 452),
         (226, 432), (230, 424), (240, 414), (244, 400), (244, 42), (240, 20), (228, 10), (0, 8)]
    parts = [(glass(lathe("Glass", g, M["glass"], 72)), 0)]
    parts.append((lathe("Sugar", [(0, 8.6), (226, 8.6), (239, 14), (243.4, 26), (243.4, 290), (200, 297), (100, 306), (0, 309)], M["sugar"], 56), 2))
    parts.append((lathe("Lid", [(0, 500), (232, 500), (248, 496), (256, 486), (257, 446), (254, 440), (240, 440), (237, 444), (237, 488), (0, 490)], M["walnut"], 72), 1))
    parts.append((wrap_label("Label", 250.15, 140, 320, 45, M["sugarLabel"], 0.5, 40), 0))
    return parts

def hulls_dish(n, base, rim, segs=8):
    """the floor as one hull (a ring pair), the wall in segments (the dish stays open for whatever lands in it)"""
    hs = [H.hull("UCX_%s_00" % n, ring(base[0], 0, 16) + ring(base[1], base[2], 16))]
    for k in range(segs):
        pts = []
        for a in (2 * math.pi * k / segs, 2 * math.pi * (k + 1) / segs):
            for r, z in rim: pts.append((r * K * math.cos(a), r * K * math.sin(a), z * K))
        hs.append(H.hull("UCX_%s_%02d" % (n, k + 1), pts))
    return hs

def hulls_pot(n):
    return [H.hull("UCX_%s_00" % n, ring(430, 0, 16) + ring(436, 515, 16))] + \
           [H.hull("UCX_%s_%02d" % (n, i + 1), boxp(*b)) for i, b in enumerate(((420, 530, -114, 114, 436, 468), (-530, -420, -114, 114, 436, 468)))]

def hulls_lid(n):
    return [H.hull("UCX_%s_00" % n, ring(442, 0, 16) + ring(400, 27, 16) + ring(300, 49, 12) + ring(150, 61, 8)),
            H.hull("UCX_%s_01" % n, ring(66, 60, 12) + ring(60, 118, 12))]

def hulls_kettle(n):
    hp = [(140, 330), (136, 410), (96, 478), (20, 498), (-60, 497), (-126, 468), (-160, 400), (-166, 322)]
    return [H.hull("UCX_%s_00" % n, ring(216, 0, 16) + ring(216, 120, 16) + ring(182, 290, 16) + ring(100, 370, 12) + ring(30, 412, 8)),
            H.hull("UCX_%s_01" % n, [(x * K, y * K, z * K) for x, z in ((150, 60), (150, 140), (345, 300), (345, 335), (300, 270)) for y in (-40, 40)]),
            H.hull("UCX_%s_02" % n, [(x * K, y * K, (z + d) * K) for x, z in hp for y in (-18, 18) for d in (-18, 18)])]

SPECS.update({   # batch 5
    "SM_K_Bowl": dict(build=bowl, lods=(0.45, 0.18), cls="ceramic", hulls=lambda n: hulls_dish(n, (142, 150, 22), ((148, 8), (152, 24), (230, 117), (222, 122))),
                      breaks=lambda o, n: shatter(o, n, lambda p, g: True, n=8, seed=19, jag=0.0008, chips=6, chip_mat_glass=False)),
    "SM_K_PetBowl": dict(build=petbowl, lods=(0.45, 0.2), cls="metal", breaks=None,
                         hulls=lambda n: hulls_dish(n, (330, 338, 10), ((330, 0), (338, 12), (480, 166), (470, 171)))),
    "SM_K_Pot": dict(build=pot, hulls=hulls_pot, lods=(0.45, 0.2), cls="metal", breaks=None),
    "SM_K_Pot_Lid": dict(build=potlid, hulls=hulls_lid, lods=(0.45, 0.2), cls="metal",
                         breaks=lambda o, n: cut_groups(o, n, [], [lambda p, g: p == 0, lambda p, g: p == 1], [])),      # the knob comes off
    "SM_K_Kettle": dict(build=kettle, hulls=hulls_kettle, lods=(0.45, 0.2), cls="metal",
                        breaks=lambda o, n: cut_groups(o, n, [], [lambda p, g: p == 0, lambda p, g: p == 1], [])),       # the lid jumps off
    "SM_K_Shaker": dict(build=shaker, hulls=lambda n: hulls_round(n, 80, 216, 68, (210, 266)), lods=(0.45, 0.2), cls="ceramic",
                        breaks=lambda o, n: shatter(o, n, lambda p, g: p == 0, [], [lambda p, g: p == 1], n=8, seed=29, jag=0.0006, chips=5, chip_mat_glass=False)),
    "SM_K_JarSugar": dict(build=jarsugar, hulls=lambda n: hulls_round(n, 250, 440, 257, (440, 500)), lods=(0.45, 0.18), cls="glass", glass=0.12,
                          breaks=lambda o, n: shatter(o, n, lambda p, g: g == 1, [lambda p, g: g == 0 and p == 0], [lambda p, g: p == 1], n=17, seed=31, jag=0.0007, chips=10)),
})

# ---- batch 6: kitchen tools and small ware (the ladle, the turner, the whisk on the rail, the rolling pin, the board, the sugar bowl,
#      the ice tray, the egg carton). The rail's tools hang from their hook point (the frame's origin), down -Z.
def lay_x(o, zc):
    """a piece turned round Z laid along X, its axis at height zc"""
    o.matrix_world = Matrix.Translation(Vector(u(0, 0, zc))) @ Matrix.Rotation(math.radians(90), 4, "Y") @ o.matrix_world; return o

def floured_wood(name, light, dark, grain, flour=0.62):
    """worn kitchen wood: pale, matte, a dusting of flour in patches"""
    m = H.wood(name, light, dark, grain, 0.05); p = m.node_tree.nodes["Principled BSDF"]
    src = p.inputs["Base Color"].links[0].from_socket
    tc = N(m, "ShaderNodeTexCoord", -1300, 600); no = N(m, "ShaderNodeTexNoise", -1100, 600); no.inputs["Scale"].default_value = 40; no.inputs["Detail"].default_value = 8
    L(m, tc.outputs["Object"], no.inputs["Vector"])
    f = _maprange(m, no.outputs["Fac"], flour, flour + 0.12, 0.0, 0.85, True, -900, 600)
    fl = N(m, "ShaderNodeRGB", -700, 800); fl.outputs[0].default_value = (0.95, 0.94, 0.90, 1)
    L(m, _over(m, src, fl.outputs[0], f, -300, 400), p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = 0.75
    return H.tag(m, 0.0, 0.7)

def board_wood(name):
    """an oiled walnut board: knife scores across the face (thin random grooves), a darker juice groove ring"""
    m = H.wood(name, (0.44, 0.28, 0.15), (0.20, 0.11, 0.05), 18.0, 0.1); p = m.node_tree.nodes["Principled BSDF"]
    tc = N(m, "ShaderNodeTexCoord", -1300, -900)
    mp = N(m, "ShaderNodeMapping", -1100, -900); mp.inputs["Rotation"].default_value = (0, 0, 0.5); mp.inputs["Scale"].default_value = (1.0, 0.03, 1.0)
    L(m, tc.outputs["Object"], mp.inputs["Vector"])
    no = N(m, "ShaderNodeTexNoise", -900, -900); no.inputs["Scale"].default_value = 900; no.inputs["Detail"].default_value = 2; L(m, mp.outputs["Vector"], no.inputs["Vector"])
    sc = _maprange(m, no.outputs["Fac"], 0.66, 0.70, 0.0, 1.0, True, -700, -900)
    bp = N(m, "ShaderNodeBump", -300, -900); bp.invert = True; bp.inputs["Strength"].default_value = 0.5; bp.inputs["Distance"].default_value = 0.0003
    L(m, sc, bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    p.inputs["Roughness"].default_value = 0.55
    return H.tag(m, 0.0, 0.6)

def pulp(name, col=(0.62, 0.55, 0.45)):
    """moulded paper pulp: grey-beige, matte, fibrous"""
    m, p = H.mat(name); tc = N(m, "ShaderNodeTexCoord", -1100)
    no = N(m, "ShaderNodeTexNoise", -900); no.inputs["Scale"].default_value = 500; no.inputs["Detail"].default_value = 10; L(m, tc.outputs["Object"], no.inputs["Vector"])
    cr = N(m, "ShaderNodeValToRGB", -700); e = cr.color_ramp.elements; e[0].color = (col[0] * 0.8, col[1] * 0.8, col[2] * 0.8, 1); e[1].color = (*col, 1)
    L(m, no.outputs["Fac"], cr.inputs["Fac"]); L(m, cr.outputs["Color"], p.inputs["Base Color"]); p.inputs["Roughness"].default_value = 0.95
    bp = N(m, "ShaderNodeBump", -300, -300); bp.inputs["Strength"].default_value = 0.6; bp.inputs["Distance"].default_value = 0.0005
    L(m, no.outputs["Fac"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return H.tag(m, 0.0, 0.9)

def mats6(M):
    M["rail"] = H.metal("RailSteel", (0.83, 0.83, 0.82), 0.18)
    M["beech"] = floured_wood("RollingBeech", (0.86, 0.72, 0.52), (0.66, 0.50, 0.32), 14.0)
    M["walnutHandle"] = H.wood("WalnutHandle", (0.40, 0.24, 0.12), (0.16, 0.08, 0.035), 30.0, 0.5)
    M["board"] = board_wood("BoardWalnut")
    M["sugarBowl"] = ceramic("SugarBowlGlaze", (0.95, 0.94, 0.90), 0.05, [(228, 236, (0.70, 0.52, 0.16)), (120, 126, (0.70, 0.52, 0.16))], 320.0, 4.0)
    M["icePlastic"] = H.plastic("IceTrayBlue", (0.12, 0.38, 0.72), 0.3, peel=0.02, scuff=0.15)
    M["ice"] = glass_mat("Ice", (0.86, 0.93, 0.98), 0.12)
    M["pulp"] = pulp("EggPulp")
    M["egg"] = H.plastic("EggShell", (0.80, 0.58, 0.40), 0.55, peel=0.15, scuff=0.08); H.tag(M["egg"], 0.0, 0.3)
    return M

def slab(name, outline, holes, axis, a0, th, mat, bev=0.8):
    """a flat part from a 2D outline (uu) with holes, th thick from a0 along the axis: 'y' -> (x, a0, z), 'z' -> (x, y, a0)"""
    me = bpy.data.meshes.new(name); bm = bmesh.new(); edges = []
    P = (lambda p, q, a: (p * K, a * K, q * K)) if axis == "y" else (lambda p, q, a: (p * K, q * K, a * K))
    for loop in [outline] + list(holes):
        vs = [bm.verts.new(P(p, q, a0)) for p, q in loop]
        edges += [bm.edges.new((vs[i], vs[(i + 1) % len(vs)])) for i in range(len(vs))]
    bmesh.ops.triangle_fill(bm, use_beauty=True, use_dissolve=False, edges=edges, normal=(0, 1, 0) if axis == "y" else (0, 0, 1))
    ex = bmesh.ops.extrude_face_region(bm, geom=bm.faces[:])
    d = Vector(P(0, 0, th)) - Vector(P(0, 0, 0))
    bmesh.ops.translate(bm, vec=d, verts=[v for v in ex["geom"] if isinstance(v, bmesh.types.BMVert)])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(me); bm.free()
    o = H.link(bpy.data.objects.new(name, me)); o.data.materials.append(mat)
    if bev: H.smooth(o, 0, bev * K, 1)
    return o

def circle2(cx, cy, r, n=16): return [(cx + r * math.cos(2 * math.pi * k / n), cy + r * math.sin(2 * math.pi * k / n)) for k in range(n)]

def rrect2(x0, x1, y0, y1, r, n=5):
    pts = []
    for cx, cy, a0 in ((x1 - r, y0 + r, -90), (x1 - r, y1 - r, 0), (x0 + r, y1 - r, 90), (x0 + r, y0 + r, 180)):
        pts += [(cx + r * math.cos(math.radians(a0 + 90 * k / n)), cy + r * math.sin(math.radians(a0 + 90 * k / n))) for k in range(n + 1)]
    return pts

def ladle(M):
    bowl = [(0, -110), (40, -106), (75, -90), (98, -64), (108, -32), (110, 0), (112, 3), (110, 5.5), (106, 2), (104, -32), (95, -62), (72, -86), (38, -102), (0, -106)]
    parts = [(lathe("Bowl", bowl, M["rail"], 64, 0, (0, -71, -650)), 0)]
    h = H.tubes("Handle", [[Vector(u(*p)) for p in ((0, 37, -652), (0, 46, -600), (0, 40, -480), (0, 26, -250), (0, 20, -40), (0, 18, -6))]], 8 * K, res_u=8, bevel_res=2, tip=1.0)
    h.scale = (2.0, 0.45, 1.0); h.data.materials.append(M["rail"]); parts.append((h, 0))
    k = H.tubes("Hook", [[Vector(u(*p)) for p in ((0, 18, -14), (0, 20, 16), (0, 8, 34), (0, -8, 28), (0, -11, 10))]], 5 * K, res_u=8, bevel_res=2, tip=1.0)
    k.data.materials.append(M["rail"]); parts.append((k, 0))
    return parts

def spatula(M):
    out = [(-100, -1010), (100, -1010), (96, -880), (70, -730), (24, -704), (-24, -704), (-70, -730), (-96, -880)]
    slots = [[(x - 9, -980), (x + 9, -980), (x + 8, -770), (x - 8, -770)] for x in (-54, -18, 18, 54)]
    parts = [(slab("Blade", out, slots, "y", -31.5, 3, M["rail"], 0.6), 0)]
    n = H.tubes("Neck", [[Vector(u(*p)) for p in ((0, 0, -430), (0, -4, -560), (0, -22, -660), (0, -30, -712))]], 9 * K, res_u=8, bevel_res=2, tip=1.0)
    n.scale = (2.3, 0.4, 1.0); n.data.materials.append(M["rail"]); parts.append((n, 0))
    parts.append((rbox("Handle", -24, 24, -12, 12, -470, 0, M["walnutHandle"], 9, 0, 3), 1))
    for z in (-150, -330):
        r = lathe_s("Rivet", [(0, 0), (7, 0), (6, 2), (3.5, 3.5), (0, 4)], M["rail"], 12); r.rotation_euler = (-math.pi / 2, 0, 0); r.location = u(0, 12, z); parts.append((r, 1))
    ring_pts = [Vector(u(0, 22 * math.cos(a), 12 + 22 * math.sin(a))) for a in [2 * math.pi * k / 10 for k in range(11)]]
    rg = H.tubes("Ring", [ring_pts], 2.5 * K, res_u=4, bevel_res=1, tip=1.0); rg.data.materials.append(M["rail"]); parts.append((rg, 0))
    return parts

def whisk(M):
    parts = [(lathe_s("Handle", [(0, -248), (9, -248), (15, -236), (15, -6), (13, 0), (0, 2)], M["rail"], 32), 0)]
    rg = H.tubes("Ring", [[Vector(u(0, 14 * math.cos(a), 18 + 16 * math.sin(a))) for a in [2 * math.pi * k / 10 - math.pi / 2 for k in range(11)]]], 2.2 * K, res_u=4, bevel_res=1, tip=1.0)
    rg.data.materials.append(M["rail"]); parts.append((rg, 0))
    loop = [(10, -230), (30, -290), (56, -380), (66, -450), (58, -520), (32, -560), (0, -568), (-32, -560), (-58, -520), (-66, -450), (-56, -380), (-30, -290), (-10, -230)]
    wires = []
    for i in range(8):
        a = math.pi * i / 8
        wires.append([Vector(u(s * math.cos(a), s * math.sin(a) * 0.88, z)) for s, z in loop])
    w = H.tubes("Wires", wires, 1.6 * K, res_u=3, bevel_res=1, tip=1.0); w.data.materials.append(M["rail"]); parts.append((w, 0))
    return parts

def rollingpin(M):
    prof = [(0, -700), (30, -698), (52, -690), (60, -670), (58, -620), (50, -560), (48, -500), (56, -470), (70, -462), (128, -455), (136, -448), (140, -430),
            (140, 430), (136, 448), (128, 455), (70, 462), (56, 470), (48, 500), (50, 560), (58, 620), (60, 670), (52, 690), (30, 698), (0, 700)]
    return [(lay_x(lathe("Pin", prof, M["beech"], 48), 140), 0)]

def cuttingboard(M):
    return [(slab("Board", rrect2(-550, 550, -350, 350, 70), [circle2(468, 0, 30, 20)], "z", 0, 50, M["board"], 3.0), 0)]

def sugarbowl(M):
    body = [(0, 6), (110, 6), (116, 2), (122, 0), (130, 0), (136, 4), (140, 14), (190, 40), (226, 90), (238, 150), (232, 200), (222, 214), (222, 222),
            (216, 222), (214, 212), (222, 196), (230, 150), (218, 94), (184, 48), (140, 26), (0, 24)]
    lid = [(0, 262), (100, 258), (180, 246), (226, 229), (232, 223), (230, 219), (214, 219), (212, 212), (208, 212), (206, 222), (178, 240), (100, 252), (0, 256)]
    knob = [(0, 310), (26, 310), (36, 302), (38, 290), (30, 278), (20, 268), (24, 261), (0, 261)]
    parts = [(lathe("Bowl", body, M["sugarBowl"], 72), 0), (lathe("Lid", lid, M["sugarBowl"], 72), 1), (lathe_s("Knob", knob, M["sugarBowl"], 40), 1)]
    for s in (1, -1):
        e = H.tubes("Ear", [[Vector(u(s * x, 0, z)) for x, z in ((226, 176), (268, 184), (296, 156), (288, 116), (246, 100), (228, 104))]], 12 * K, res_u=8, bevel_res=2, tip=0.8)
        e.scale = (1, 0.75, 1); e.data.materials.append(M["sugarBowl"]); parts.append((e, 2))   # (the ears: a piece of their own, they overlap the body)
    return parts

def icetray(M):
    parts = [(rbox("Floor", -150, 150, -150, 150, 0, 8, M["icePlastic"], 3, 0, 2), 0)]
    for k, c in enumerate((-150, -50, 50, 150)):   # the walls: the outer ones 6 thick inside the box, the dividers 6 thick round their line
        a0, a1 = max(-150, c - 3) if abs(c) < 150 else (-150 if c < 0 else 144), min(150, c + 3) if abs(c) < 150 else (-144 if c < 0 else 150)
        parts.append((rbox("WallX%d" % k, a0, a1, -150, 150, 0, 62, M["icePlastic"], 2, 0, 2), 0))
        parts.append((rbox("WallY%d" % k, -150, 150, a0, a1, 0, 62, M["icePlastic"], 2, 0, 2), 0))
    R = random.Random(37)
    for i in range(3):
        for j in range(3):
            if (i, j) == (2, 0): continue      # one cell empty: the cube on top came from it
            cx, cy = -100 + i * 100, -100 + j * 100; top = R.uniform(70, 88)
            parts.append((glass(rbox("Ice%d%d" % (i, j), cx - 42, cx + 42, cy - 42, cy + 42, 10, top, M["ice"], 12, 0, 3)), 1))
    c = glass(rbox("IceLoose", -40, 40, -40, 40, -38, 38, M["ice"], 12, 0, 3)); c.rotation_euler = (0.35, 0.2, 0.6); c.location = u(40, 30, 92)
    parts.append((c, 1))
    return parts

def eggtray(M):
    cup = [(0, 6), (40, 6), (50, 3), (57, 0), (62, 0), (66, 6), (70, 30), (70, 56), (64, 56), (64, 32), (60, 12), (50, 11), (0, 11)]
    parts = [(rbox("Rim", -210, 210, -145, 145, 50, 56, M["pulp"], 3, 0, 2), 0)]
    eggs = [(-138, -70), (0, -70), (138, -70), (-138, 70), (0, 70), (138, 70)]
    for k, (x, y) in enumerate(eggs):
        parts.append((lathe("Cup%d" % k, cup, M["pulp"], 32, 0, (x, y, 0)), 0))
        if k != 4:
            e = lathe("Egg%d" % k, [(0, 0), (20, 2), (32, 8), (39, 18), (41, 32), (40, 48), (36, 64), (29, 80), (19, 94), (9, 104), (0, 108)], M["egg"], 40, 0, (x, y, 12))
            e.rotation_euler = (random.Random(k).uniform(-0.08, 0.08), random.Random(k + 9).uniform(-0.08, 0.08), 0); parts.append((e, 1))
    for x in (-69, 69):
        parts.append((lathe("Post%d" % x, [(0, 150), (14, 150), (20, 144), (30, 100), (42, 56), (0, 56)], M["pulp"], 24, 0, (x, 0, 0)), 0))
    return parts

def hulls_ladle(n):
    bowl = [(p[0], p[1] - 71 * K, p[2] - 650 * K) for p in ring(112, 5, 12) + ring(75, -90, 8)] + [(0, -71 * K, -760 * K)]
    return [H.hull("UCX_%s_00" % n, bowl), H.hull("UCX_%s_01" % n, boxp(-16, 16, 10, 52, -640, 38))]

def hulls_pin(n):
    rx = lambda r, x: [(x * K, r * K * math.cos(2 * math.pi * k / 12), (140 + r * math.sin(2 * math.pi * k / 12)) * K) for k in range(12)]
    return [H.hull("UCX_%s_00" % n, rx(140, -455) + rx(140, 455)), H.hull("UCX_%s_01" % n, rx(60, -700) + rx(60, -455)), H.hull("UCX_%s_02" % n, rx(60, 455) + rx(60, 700))]

def _box_hulls(*bs): return lambda n: [H.hull("UCX_%s_%02d" % (n, i), boxp(*b)) for i, b in enumerate(bs)]

SPECS.pop("SM_K_Kettle", None)   # (the kettle is the kitchen set's: the owner's stovetop kettle)
SPECS.update({   # batch 6
    "SM_K_Ladle": dict(hang=True, build=ladle, hulls=hulls_ladle, lods=(0.45, 0.2), cls="metal", breaks=None),
    "SM_K_Spatula": dict(hang=True, build=spatula, hulls=_box_hulls((-100, 100, -34, -26, -1010, -700), (-22, 22, -32, 6, -720, -440), (-25, 25, -13, 13, -470, 36)),
                         lods=(0.45, 0.2), cls="metal", breaks=None),
    "SM_K_Whisk": dict(hang=True, build=whisk, lods=(0.5, 0.25), cls="metal", breaks=None,
                       hulls=lambda n: [H.hull("UCX_%s_00" % n, ring(12, -225, 8) + ring(58, -380, 12) + ring(66, -450, 12) + ring(32, -560, 8)),
                                        H.hull("UCX_%s_01" % n, ring(15, -248, 8) + ring(15, 36, 8))]),
    "SM_K_RollingPin": dict(build=rollingpin, hulls=hulls_pin, lods=(0.45, 0.2), cls="wood", breaks=None),
    "SM_K_CuttingBoard": dict(build=cuttingboard, hulls=_box_hulls((-550, 550, -350, 350, 0, 50)), lods=(0.5, 0.25), cls="wood",
                              breaks=lambda o, n: cut_groups(o, n, [lambda p, g: True], [], [((60, 0, 25), (1.0, 0.12, 0.0), 0.004, 107, 0)])),   # split along the grain
    "SM_K_SugarBowl": dict(build=sugarbowl, lods=(0.45, 0.2), cls="ceramic",
                           hulls=lambda n: hulls_round(n, 238, 222, 232, (222, 310)) + [H.hull("UCX_%s_%02d" % (n, 2 + i), boxp(*b)) for i, b in enumerate(((226, 300, -12, 12, 96, 190), (-300, -226, -12, 12, 96, 190)))],
                           breaks=lambda o, n: shatter(o, n, lambda p, g: p == 0, [], [lambda p, g: p == 1, lambda p, g: p == 2], n=9, seed=41, jag=0.0008, chips=6, chip_mat_glass=False)),
    "SM_K_IceTray": dict(build=icetray, hulls=_box_hulls((-150, 150, -150, 150, 0, 62), (-140, 140, -140, 140, 62, 100)), lods=(0.45, 0.2), cls="plastic", glass=0.3,
                         glassmat="ice", breaks=lambda o, n: cut_groups(o, n, [], [lambda p, g: p == 0, lambda p, g: p == 1], [])),    # the ice falls out
    "SM_K_EggTray": dict(build=eggtray, hulls=_box_hulls((-210, 210, -145, 145, 0, 56), (-180, 180, -112, 112, 56, 122)), lods=(0.45, 0.2), cls="paper",
                         breaks=lambda o, n: cut_groups(o, n, [lambda p, g: p == 0], [lambda p, g: p == 1], [((0, 0, 50), (1.0, 0.1, 0.0), 0.008, 109, 0)])),
})

# ---- batch 7: toys (the tennis ball, the block arch, the wooden truck, the engine, the cat's felt mouse)
def felt(name, col, bump=0.8, sheen=1.0):
    """felt / wool nap: matte, sheen at grazing angles, a fuzzy noise"""
    m, p = H.mat(name); tc = N(m, "ShaderNodeTexCoord", -1100)
    no = N(m, "ShaderNodeTexNoise", -900); no.inputs["Scale"].default_value = 1400; no.inputs["Detail"].default_value = 12; L(m, tc.outputs["Object"], no.inputs["Vector"])
    big = N(m, "ShaderNodeTexNoise", -900, 300); big.inputs["Scale"].default_value = 30; L(m, tc.outputs["Object"], big.inputs["Vector"])
    cr = N(m, "ShaderNodeValToRGB", -700, 300); e = cr.color_ramp.elements; e[0].color = (col[0] * 0.62, col[1] * 0.62, col[2] * 0.55, 1); e[1].color = (*col, 1)
    L(m, big.outputs["Fac"], cr.inputs["Fac"]); L(m, cr.outputs["Color"], p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = 1.0; p.inputs["Sheen Weight"].default_value = sheen; p.inputs["Sheen Roughness"].default_value = 0.4
    bp = N(m, "ShaderNodeBump", -300, -300); bp.inputs["Strength"].default_value = bump; bp.inputs["Distance"].default_value = 0.0006
    L(m, no.outputs["Fac"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return H.tag(m, 0.0, 1.0)

def paint(name, col, rough=0.32):
    """a child's painted wooden toy: gloss enamel paint, knocked edges"""
    m = H.plastic(name, col, rough, peel=0.03, scuff=0.35); return H.tag(m, 0.0, 0.2)

def mats7(M):
    M["tennis"] = felt("TennisFelt", (0.72, 0.84, 0.12), 0.9)
    M["seam"] = H.plastic("TennisSeam", (0.93, 0.93, 0.88), 0.6, peel=0.05, scuff=0.2)
    M["archBlue"] = paint("ArchBlue", (0.08, 0.30, 0.66))
    M["toyRed"] = paint("ToyRed", (0.70, 0.06, 0.04))
    M["toyBlue"] = paint("ToyBlue", (0.07, 0.24, 0.62))
    M["toyGreen"] = paint("ToyGreen", (0.12, 0.45, 0.16))
    M["toyYellow"] = paint("ToyYellow", (0.92, 0.66, 0.06))
    M["toyWood"] = H.wood("ToyBeech", (0.86, 0.68, 0.44), (0.62, 0.44, 0.24), 16.0, 0.5)
    M["rubber"] = H.plastic("TyreRubber", (0.03, 0.03, 0.03), 0.7, peel=0.1, scuff=0.15)
    M["toyGlass"] = glass_mat("ToyWindow", (0.80, 0.88, 0.95), 0.05)
    M["mouseFelt"] = felt("MouseFelt", (0.42, 0.40, 0.38), 0.7)
    M["pinkFelt"] = felt("PinkFelt", (0.86, 0.48, 0.55), 0.5)
    M["bead"] = H.plastic("EyeBead", (0.01, 0.01, 0.01), 0.08, peel=0.0, scuff=0.05)
    M["yarnTail"] = H.fibre("TailYarn", light=(0.82, 0.30, 0.34), dark=(0.55, 0.16, 0.2), dirt=(0.3, 0.2, 0.2), floor_h=0.0, twist=2400)
    return M

def sphere_prof(r, n=24, z0=0.0):
    return [(r * math.sin(math.pi * k / n), z0 + r * (1 - math.cos(math.pi * k / n))) for k in range(n + 1)]

def tennisball(M):
    parts = [(lathe("Ball", sphere_prof(190, 32), M["tennis"], 64), 0)]
    pts = []
    for k in range(97):   # the seam: the classic two-lobe curve on the sphere
        t = 2 * math.pi * k / 96
        v = Vector((math.cos(t) + 0.33 * math.cos(3 * t), math.sin(t) - 0.33 * math.sin(3 * t), 1.25 * math.sin(2 * t))).normalized() * 189.5
        pts.append(Vector(u(v.x, v.y, v.z + 190)))
    s = H.tubes("Seam", [pts], 4.5 * K, res_u=2, bevel_res=1, tip=1.0); s.data.materials.append(M["seam"]); parts.append((s, 0))
    return parts

def toyarch(M):
    out = [(-300, 0), (-160, 0)] + [(160 * math.cos(math.pi - math.pi * k / 20), 160 * math.sin(math.pi - math.pi * k / 20)) for k in range(1, 20)] + [(160, 0), (300, 0), (300, 240), (-300, 240)]
    return [(slab("Arch", out, [], "y", -120, 240, M["archBlue"], 10.0), 0)]

def wheel_x(name, x, y, z, r, w, M, hub="toyWood"):
    """a wheel on an axle along X: the tyre and its hub"""
    s = 1 if x > 0 else -1
    t = lathe(name, [(0, 0), (r * 0.55, 0), (r * 0.92, 2), (r, w * 0.2), (r, w * 0.8), (r * 0.92, w - 2), (r * 0.55, w), (0, w)], M["rubber"], 32)
    h = lathe_s(name + "Hub", [(0, w + 4), (r * 0.3, w + 4), (r * 0.45, w + 1), (r * 0.45, w - 1), (0, w - 1)], M[hub], 24)
    for o in (t, h):
        o.rotation_euler = (0, s * math.pi / 2, 0); o.location = u(x, y, z)
    return [(t, 0), (h, 0)]

def toycar(M):
    """a wooden toy lorry, its nose to -Y: red cab with windows, a beech bed with sides, black tyres"""
    P = [(rbox("Chassis", -150, 150, -345, 330, 50, 92, M["toyGreen"], 8, 0, 3), 0),
         (rbox("Cab", -150, 150, -345, -128, 92, 238, M["toyRed"], 14, 0, 3), 0),
         (rbox("Bed", -160, 160, -118, 340, 92, 112, M["toyWood"], 5, 0, 2), 0)]
    for x0, x1, y0, y1 in ((-160, -142, -118, 340), (142, 160, -118, 340), (-160, 160, 322, 340), (-160, 160, -118, -100)):
        P.append((rbox("Side", x0, x1, y0, y1, 112, 176, M["toyWood"], 4, 0, 2), 0))
    for x0, x1, y0, y1, z0, z1 in ((-153, -147, -320, -170, 150, 220), (147, 153, -320, -170, 150, 220), (-120, 120, -348, -342, 150, 222)):
        P.append((glass(rbox("Window", x0, x1, y0, y1, z0, z1, M["toyGlass"], 2, 0, 2)), 0))
    for x in (-105, 105):
        l = lathe_s("Lamp", [(0, 0), (20, 0), (22, 4), (18, 10), (0, 12)], M["toyYellow"], 20); l.rotation_euler = (math.pi / 2, 0, 0); l.location = u(x, -343, 120); P.append((l, 0))
    for x in (-150, 150):
        for y in (-230, 220):
            P += wheel_x("Wheel", x, y, 62, 62, 46, M)
    return P

def toytrain(M):
    """a wooden engine, its nose to -Y: blue boiler, red cab, green frame, beech wheels, a chimney"""
    boiler = lathe("Boiler", [(0, 0), (80, 0), (88, 6), (90, 16), (90, 300), (0, 300)], M["toyBlue"], 40)
    boiler.rotation_euler = (-math.pi / 2, 0, 0); boiler.location = u(0, -250, 160)
    P = [(rbox("Frame", -110, 110, -250, 250, 40, 80, M["toyGreen"], 8, 0, 3), 0), (boiler, 0),
         (rbox("Cab", -118, 118, 60, 250, 80, 300, M["toyRed"], 10, 0, 3), 0),
         (rbox("Roof", -127, 127, 46, 250, 300, 326, M["toyYellow"], 8, 0, 3), 0),
         (lathe_s("Chimney", [(0, 0), (34, 0), (34, 90), (48, 110), (48, 128), (0, 128)], M["toyRed"], 24, 0, (0, -170, 192)), 0),
         (lathe_s("Dome", sphere_prof(34, 10), M["toyYellow"], 24, 0, (0, -40, 236)), 0)]
    for x0, x1 in ((-121, -117), (117, 121)):
        P.append((glass(rbox("Window", x0, x1, 120, 220, 190, 270, M["toyGlass"], 2, 0, 2)), 0))
    for x in (-110, 110):
        for y in (-170, -40, 120):
            P += wheel_x("Wheel", x, y, 55, 55, 17, M, "toyRed")
    return P

def toymouse(M):
    """the cat's felt mouse: grey body, pink ears and nose, bead eyes, a yarn tail along +Y"""
    body = lathe("Body", [(0, 0), (40, 6), (66, 30), (78, 80), (74, 150), (60, 220), (36, 290), (12, 322), (0, 326)], M["mouseFelt"], 40)
    body.rotation_euler = (math.pi / 2, 0, 0); body.scale = (1.0, 0.88, 1.0); body.location = u(0, 166, 70)
    P = [(body, 0), (lathe_s("Nose", sphere_prof(11, 8), M["pinkFelt"], 16, 0, (0, -158, 62)), 0)]
    for s in (-1, 1):
        e = lathe_s("Ear", [(0, 0), (34, 0), (38, 3), (34, 6), (0, 6)], M["pinkFelt"], 24); e.rotation_euler = (math.radians(70), 0, s * 0.5); e.location = u(s * 36, -80, 118)
        P.append((e, 0))
        b = lathe_s("Eye", sphere_prof(7, 8), M["bead"], 12, 0, (s * 26, -128, 82)); P.append((b, 0))
    t = H.tubes("Tail", [[Vector(u(*p)) for p in ((0, 160, 40), (6, 200, 18), (-14, 250, 8), (10, 300, 6), (-8, 340, 6), (4, 362, 6))]], 6 * K, res_u=6, bevel_res=2, tip=0.7)
    t.data.materials.append(M["yarnTail"]); P.append((t, 0))
    return P

SPECS.update({   # batch 7
    "SM_K_TennisBall": dict(build=tennisball, hulls=lambda n: [H.hull("UCX_%s_00" % n, [(p[0], p[1], p[2] + 190 * K) for k in range(7) for p in ring(190 * math.sin(math.pi * k / 6) + 0.5, -190 * math.cos(math.pi * k / 6), 12)])],
                            lods=(0.45, 0.2), cls="plastic", breaks=None),
    "SM_L_ToyArch": dict(build=toyarch, lods=(0.5, 0.25), cls="wood", breaks=None,
                         hulls=lambda n: [H.hull("UCX_%s_00" % n, boxp(-300, -150, -120, 120, 0, 240)), H.hull("UCX_%s_01" % n, boxp(150, 300, -120, 120, 0, 240)),
                                          H.hull("UCX_%s_02" % n, boxp(-160, 160, -120, 120, 160, 240))]),
    "SM_L_ToyCar": dict(build=toycar, hulls=_box_hulls((-150, 150, -345, 340, 30, 112), (-150, 150, -345, -128, 112, 238), (-200, 200, -292, 282, 0, 124)), lods=(0.45, 0.2),
                        cls="plastic", glass=0.3, glassmat="toyGlass", breaks=None),
    "SM_L_ToyTrain": dict(build=toytrain, hulls=_box_hulls((-110, 110, -250, 250, 0, 250), (-127, 127, 46, 250, 80, 326), (-48, 48, -218, -122, 192, 320)), lods=(0.45, 0.2),
                          cls="plastic", glass=0.3, glassmat="toyGlass", breaks=None),
    "SM_L_ToyMouse": dict(build=toymouse, hulls=_box_hulls((-78, 78, -160, 166, 0, 145)), lods=(0.45, 0.2), cls="cloth", breaks=None),
})

# ---- batch 8: packets and electronics (the soda can, the milk carton, the cereal box, the game console, the pad)
def mats8(M):
    M["alu"] = H.metal("CanAlu", (0.86, 0.87, 0.89), 0.18)
    M["sodaLabel"] = label_cyl("LabelSoda", "soda.png", 180, 40, 330, 0.22, (0.69, 0.07, 0.09), 0.05)
    M["milk"] = print_box("MilkPrint", {"front": "milk_front.png", "back": "milk_front.png", "side": "milk_side.png", "top": "milk_side.png"},
                          -204, 204, -204, 204, 0, 560, 0.35, (0.95, 0.95, 0.96))
    M["milkTop"] = H.plastic("MilkTop", (0.93, 0.94, 0.95), 0.4, peel=0.04, scuff=0.2); H.tag(M["milkTop"], 0.0, 0.5)   # (the gable and the fin: plain coated board)
    M["cereal"] = print_box("CerealPrint", {"front": "cereal_front.png", "back": "cereal_back.png", "side": "cereal_side.png", "top": "cereal_top.png"},
                            -190, 190, -80, 70, 0, 637, 0.45, (0.92, 0.59, 0.12))
    M["consoleBody"] = H.plastic("ConsoleBody", (0.035, 0.035, 0.04), 0.45, peel=0.02, scuff=0.25)
    M["consoleGloss"] = H.plastic("ConsoleGloss", (0.015, 0.015, 0.018), 0.08, peel=0.0, scuff=0.35)
    M["teal"] = H.plastic("TealLight", (0.05, 0.62, 0.58), 0.2, peel=0.0, scuff=0.05)
    M["padBody"] = H.plastic("PadBody", (0.80, 0.80, 0.78), 0.4, peel=0.03, scuff=0.3)
    M["padDark"] = H.plastic("PadDark", (0.06, 0.06, 0.07), 0.55, peel=0.15, scuff=0.2)
    for k, c in (("padR", (0.75, 0.08, 0.06)), ("padT", (0.05, 0.55, 0.52)), ("padY", (0.90, 0.66, 0.08)), ("padG", (0.14, 0.52, 0.18))):
        M[k] = H.plastic("Pad" + k[-1], c, 0.25, peel=0.0, scuff=0.1)
    return M

def sodacan(M):
    prof = [(0, 14), (60, 22), (84, 8), (96, 1), (104, 0), (112, 3), (120, 14), (123, 30), (123, 340), (118, 352), (108, 364), (104, 372), (106, 378),
            (103, 380.5), (99, 378), (98, 370), (92, 366), (0, 368)]
    P = [(lathe("Can", prof, M["alu"], 64), 0), (wrap_label("Print", 123.15, 40, 330, 180, M["sodaLabel"], 0.3, 96), 0)]
    P.append((rbox("Tab", -14, 14, -10, 48, 368, 371, M["alu"], 1.5, 0, 2), 0))
    ring_pts = [Vector(u(14 * math.cos(a), 30 + 18 * math.sin(a), 371)) for a in [2 * math.pi * k / 12 for k in range(13)]]
    rg = H.tubes("PullRing", [ring_pts], 2.5 * K, res_u=3, bevel_res=1, tip=1.0); rg.data.materials.append(M["alu"]); P.append((rg, 0))
    return P

def gable(name, x, y, z0, z1, mat):
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    v = [bm.verts.new(u(*p)) for p in ((-x, -y, z0), (x, -y, z0), (x, y, z0), (-x, y, z0), (-x, 0, z1), (x, 0, z1))]
    for f in ((0, 3, 2, 1), (0, 1, 5, 4), (2, 3, 4, 5), (1, 2, 5), (3, 0, 4)): bm.faces.new([v[i] for i in f])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); bm.to_mesh(me); bm.free()
    o = H.link(bpy.data.objects.new(name, me)); o.data.materials.append(mat); H.smooth(o, 0, 1.5 * K, 1); return o

def _tilt(o, rot, at):
    o.rotation_euler = rot; o.location = u(*at); return o

def milkcarton(M):
    return [(rbox("Carton", -204, 204, -204, 204, 0, 562, M["milk"], 3, 0, 2), 0), (gable("Gable", 204, 204, 560, 690, M["milkTop"]), 0),
            (rbox("Fin", -204, 204, -5, 5, 684, 725, M["milkTop"], 1.5, 0, 2), 0),
            (_tilt(lathe_s("Cap", [(0, -6), (34, -6), (36, -2), (36, 30), (32, 34), (0, 34)], M["padT"], 32), (0.567, 0, 0), (0, -90, 633)), 1)]   # on the front slope

def cerealbox(M):
    return [(rbox("Box", -190, 190, -80, 70, 0, 637, M["cereal"], 2.5, 0, 2), 0)]

def gameconsole(M):
    P = [(rbox("Body", -290, 290, -195, 195, 12, 146, M["consoleBody"], 6, 0, 3), 0),
         (rbox("Top", -296, 296, -201, 201, 140, 154, M["consoleGloss"], 5, 0, 3), 0),
         (rbox("Strip", -200, 200, -203, -195, 64, 70, M["teal"], 1, 0, 1), 0),
         (rbox("Slot", -150, 150, -199, -194, 96, 101, M["padDark"], 1, 0, 1), 0)]
    for x in (-250, 250):
        for y in (-160, 160): P.append((lathe_s("Foot", [(0, 0), (22, 0), (24, 4), (24, 12), (0, 12)], M["padDark"], 20, 0, (x, y, 0)), 0))
    for k in range(14): P.append((rbox("Vent", 120 + k * 11, 125 + k * 11, -150, 150, 152, 155, M["padDark"], 1, 0, 1), 0))
    b = lathe_s("Power", [(0, 0), (12, 0), (11, 3), (0, 4)], M["consoleBody"], 20); b.rotation_euler = (math.pi / 2, 0, 0); b.location = u(240, -195, 100); P.append((b, 0))
    return P

def controller(M):
    """a pad: pale body, two grips, offset sticks (left high, right low), a d-pad, four coloured face buttons, two bumpers"""
    P = [(rrect("Body", -104, 104, -48, 62, 18, 72, 40, M["padBody"], 10, 10), 0)]
    for s in (1, -1):
        g = lathe("Grip", sphere_prof(48, 14), M["padBody"], 32); g.scale = (1.0, 1.45, 0.78); g.location = u(s * 106, -4, 0); P.append((g, 0))
        P.append((rbox("Bumper", 52 if s > 0 else -128, 128 if s > 0 else -52, 58, 72, 46, 66, M["padDark"], 6, 0, 2), 0))
    for x, y in ((-66, 16), (34, -26)):
        P.append((lathe_s("Stick", [(0, 66), (11, 66), (11, 90), (19, 93), (21, 99), (17, 104), (0, 105)], M["padDark"], 24, 0, (x, y, 0)), 0))
        P.append((lathe_s("StickWell", [(0, 70), (24, 70), (25, 73), (23, 74), (0, 74)], M["padDark"], 24, 0, (x, y, 0)), 0))
    P += [(rbox("DpadH", -58, -14, -34, -18, 70, 80, M["padDark"], 3, 0, 2), 0), (rbox("DpadV", -44, -28, -48, -4, 70, 80, M["padDark"], 3, 0, 2), 0)]
    for (dx, dy), k in zip(((0, 16), (16, 0), (0, -16), (-16, 0)), ("padY", "padR", "padG", "padT")):
        P.append((rrect("Btn", 74 + dx - 8, 74 + dx + 8, 18 + dy - 8, 18 + dy + 8, 70, 80, 7.5, M[k], 5, 2.0), 0))
    return P

SPECS.update({   # batch 8
    "SM_L_SodaCan": dict(build=sodacan, hulls=lambda n: hulls_round(n, 123, 380), lods=(0.45, 0.2), cls="metal", breaks=None),
    "SM_K_MilkCarton": dict(build=milkcarton, hulls=lambda n: [H.hull("UCX_%s_00" % n, boxp(-204, 204, -204, 204, 0, 562)),
                                                               H.hull("UCX_%s_01" % n, [(x * K, y * K, z * K) for x in (-204, 204) for y, z in ((-204, 560), (204, 560), (0, 725))])],
                            lods=(0.5, 0.25), cls="paper", breaks=lambda o, n: cut_groups(o, n, [lambda p, g: p == 0], [lambda p, g: p == 1], [((0, 0, 300), (0.15, 0.1, 1.0), 0.010, 113, 0)])),
    "SM_V3_CerealBox": dict(build=cerealbox, hulls=_box_hulls((-190, 190, -80, 70, 0, 637)), lods=(0.5, 0.25), cls="cardboard",
                            breaks=lambda o, n: cut_groups(o, n, [lambda p, g: True], [], [((0, 0, 380), (0.1, 0.05, 1.0), 0.010, 127, 0)])),
    "SM_L_GameConsole": dict(build=gameconsole, hulls=_box_hulls((-300, 300, -203, 203, 0, 155)), lods=(0.45, 0.2), cls="plastic", breaks=None),
    "SM_L_Controller": dict(build=controller, hulls=_box_hulls((-156, 156, -73, 73, 0, 80), (-60, 60, -40, 0, 80, 111)), lods=(0.45, 0.2), cls="plastic", breaks=None),
})

# ---- batch 9: packets, jars, bottles, tubs, boxes
def kraft(name, col=(0.60, 0.43, 0.26), flutes=0.0):
    """kraft paper / corrugated board: matte brown, fibres, optional flute ridges along Z"""
    m, p = H.mat(name); tc = N(m, "ShaderNodeTexCoord", -1300)
    no = N(m, "ShaderNodeTexNoise", -1100); no.inputs["Scale"].default_value = 700; no.inputs["Detail"].default_value = 10; L(m, tc.outputs["Object"], no.inputs["Vector"])
    cr = N(m, "ShaderNodeValToRGB", -900); e = cr.color_ramp.elements; e[0].color = (col[0] * 0.8, col[1] * 0.8, col[2] * 0.78, 1); e[1].color = (*col, 1)
    L(m, no.outputs["Fac"], cr.inputs["Fac"]); L(m, cr.outputs["Color"], p.inputs["Base Color"]); p.inputs["Roughness"].default_value = 0.92
    bp = N(m, "ShaderNodeBump", -300, -300); bp.inputs["Strength"].default_value = 0.25; bp.inputs["Distance"].default_value = 0.0004
    L(m, no.outputs["Fac"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return H.tag(m, 0.0, 0.85)

def checker_cloth(name, a=(0.75, 0.05, 0.05), b=(0.95, 0.93, 0.88), scale=900.0):
    m = felt(name, b, 0.5, 0.6); p = m.node_tree.nodes["Principled BSDF"]
    tc = N(m, "ShaderNodeTexCoord", -1300, -700); ch = N(m, "ShaderNodeTexChecker", -1100, -700); ch.inputs["Scale"].default_value = scale
    ch.inputs["Color1"].default_value = (*a, 1); ch.inputs["Color2"].default_value = (*b, 1)
    L(m, tc.outputs["Object"], ch.inputs["Vector"]); L(m, ch.outputs["Color"], p.inputs["Base Color"]); return m

def mats9(M):
    M["kraft"] = kraft("Kraft"); M["corr"] = kraft("Corrugated", (0.66, 0.48, 0.30))
    M["tinPaint"] = H.plastic("CookieTinLid", (0.06, 0.14, 0.42), 0.18, peel=0.02, scuff=0.25)
    M["cookieLabel"] = label_cyl("LabelCookieTin", "cookietin.png", 180, 20, 290, 0.2, (0.12, 0.24, 0.55), 0.05)
    M["cookieTop"] = top_print("CookieTinTop", "cookietin_top.png", -350, 350, -350, 350, (0.12, 0.24, 0.55), 0.2, 0.0, 0.3, 0.05)
    M["biscuit"] = H.plastic("Biscuit", (0.78, 0.55, 0.26), 0.7, peel=0.3, scuff=0.1); H.tag(M["biscuit"], 0.0, 0.7)
    M["teaPaper"] = kraft("TeaPaper", (0.90, 0.86, 0.76)); M["string"] = H.fibre("TeaString", light=(0.95, 0.94, 0.9), dark=(0.8, 0.78, 0.72), dirt=(0.5, 0.5, 0.5), floor_h=0.0, twist=3000)
    M["teaTag"] = print_box("TeaTag", {"front": "teatag.png", "back": "teatag.png", "side": "teatag.png", "top": "teatag.png"}, -80, 80, -2, 2, 1640, 1760, 0.6, (0.98, 0.97, 0.94))
    M["dogBag"] = print_box("DogFoodPrint", {"front": "dogfood.png", "back": "dogfood.png", "side": "cereal_top.png", "top": "cereal_top.png"}, -300, 300, -190, 190, 0, 800, 0.35, (0.75, 0.14, 0.11))
    M["flour"] = print_box("FlourPrint", {"front": "flour.png", "back": "flour.png", "side": "flour.png", "top": "flour.png"}, -280, 280, -190, 190, 0, 650, 0.85, (0.92, 0.89, 0.82))
    M["flourTop"] = kraft("FlourPaper", (0.92, 0.89, 0.82)); M["twine"] = H.fibre("Twine", light=(0.75, 0.62, 0.42), dark=(0.5, 0.38, 0.22), dirt=(0.3, 0.25, 0.2), floor_h=0.0, twist=2600)
    M["gingham"] = checker_cloth("Gingham")
    M["jamLabelS"] = label_cyl("LabelJamSmall", "jam.png", 120, 30, 120, 0.55)
    M["pickleLabel"] = label_cyl("LabelPickle", "pickle.png", 120, 120, 240, 0.5)
    M["brine"] = glass_mat("Brine", (0.80, 0.85, 0.55), 0.05)
    M["cuke"] = H.plastic("Pickle", (0.16, 0.30, 0.08), 0.45, peel=0.6, scuff=0.15); H.tag(M["cuke"], 0.0, 0.3)
    M["pet"] = glass_mat("PetClear", (0.90, 0.95, 0.93), 0.03)
    M["cola"] = H.plastic("Cola", (0.06, 0.02, 0.008), 0.05, peel=0.0, scuff=0.0); H.tag(M["cola"], 0.0, 0.0)
    M["colaLabel"] = label_cyl("LabelCola", "cola.png", 180, 250, 380, 0.25, (0.70, 0.08, 0.10), 0.05)
    M["capRed"] = knurl(H.plastic("CapRed", (0.70, 0.05, 0.05), 0.35), 60, 0.4)
    M["grocery"] = print_box("GroceryPrint", {"front": "grocery.png", "back": "grocery.png", "side": "grocery.png", "top": "grocery.png"}, -550, 550, -730, 600, 0, 650, 0.9, (0.70, 0.55, 0.37))
    M["bread"] = H.plastic("Baguette", (0.72, 0.42, 0.14), 0.6, peel=0.4, scuff=0.2); H.tag(M["bread"], 0.0, 0.7)
    M["apple"] = H.plastic("Apple", (0.62, 0.06, 0.05), 0.25, peel=0.1, scuff=0.2)
    M["box"] = print_box("BoxStamp", {"front": "boxstamp.png", "back": "boxstamp.png", "side": "boxstamp.png", "top": "boxstamp.png"}, -960, 960, -690, 690, 0, 1210, 0.9, (0.66, 0.48, 0.30))
    M["tape"] = H.plastic("PackTape", (0.70, 0.55, 0.30), 0.2, peel=0.05, scuff=0.1)
    M["butterTub"] = H.plastic("ButterTub", (0.96, 0.92, 0.70), 0.35, peel=0.02, scuff=0.2)
    M["butterLabel"] = label_cyl("LabelButter", "butter.png", 180, 60, 250, 0.3, (0.98, 0.88, 0.47), 0.1)
    M["butterTop"] = top_print("ButterTop", "butter_top.png", -230, 230, -230, 230, (0.98, 0.88, 0.47), 0.3, 0.0, 0.2, 0.1)
    M["iceLabel"] = label_cyl("LabelIceCream", "icecream.png", 180, 20, 420, 0.45, (0.98, 0.78, 0.82), 0.5)
    M["iceLid"] = H.plastic("IceCreamLid", (0.95, 0.70, 0.76), 0.4, peel=0.02, scuff=0.2)
    return M

def cookietin(M):
    """the open biscuit tin: a paper sheet over the lower layers, a ring of fluted butter biscuits fanned on it and a stack in the
    middle; the lid stands on the floor on its edge and leans on the rim, its printed side out (KAN-22 fix: it was print-to-the-
    tin and 94 uu under the floor, and the biscuits lay under the tin's own bottom)"""
    body = [(0, 3), (330, 3), (342, 0), (349, 6), (350, 16), (350, 286), (353, 292), (351, 297), (346, 296), (345, 290), (345, 16), (338, 9), (0, 8)]
    P = [(lathe("Tin", body, M["tinPaint"], 64), 0), (wrap_label("Print", 350.2, 20, 290, 180, M["cookieLabel"], 0.3, 64), 0)]
    P.append((outward(lathe("Sheet", [(0, 150), (340, 150), (343.5, 153), (343.5, 172), (342, 172), (341.5, 155), (0, 153)], M["teaPaper"], 32)), 1))
    R = random.Random(41)
    for k in range(10):   # the ring: each leans on the next
        a = 2 * math.pi * k / 10 + 0.1; tilt = 0.36 + R.uniform(-0.04, 0.04)
        c = _biscuit("Biscuit%d" % k, M["biscuitB"]); c.rotation_euler = (tilt, 0, a)
        c.location = u(218 * math.cos(a), 218 * math.sin(a), 154 + 66 * math.sin(tilt)); P.append((c, 1))
    for k in range(3):    # the stack in the middle, a little askew
        c = _biscuit("BiscuitMid%d" % k, M["biscuitB"]); c.rotation_euler = (R.uniform(-0.05, 0.05), R.uniform(-0.05, 0.05), R.uniform(0, 3))
        c.location = u(R.uniform(-14, 14), R.uniform(-14, 14), 154 + k * 24); P.append((c, 1))
    lid = outward(lathe("Lid", [(0, 60), (340, 60), (352, 56), (356, 46), (356, 0), (351, 0), (350, 44), (342, 54), (0, 55)], M["cookieLid"], 64))
    for k in ("lid_u", "lid_v", "lid_top"): lid.data.attributes.new(k, "FLOAT", "POINT")
    au, av, at = [lid.data.attributes[k] for k in ("lid_u", "lid_v", "lid_top")]
    for i, v in enumerate(lid.data.vertices):   # the print reads upright from the side the lid is turned to: up the lid is its -X
        au.data[i].value = (v.co.y / K + 350.0) / 700.0; av.data[i].value = (350.0 - v.co.x / K) / 700.0; at.data[i].value = 1.0 if v.co.z > 57.5 * K else 0.0
    lid.rotation_euler = (0, TIN_PHI, 0); lid.location = u(TIN_LX, 0, TIN_LZ); P.append((lid, 2))
    return P

def teabag(M):
    bag = rbox("Bag", -105, 105, -14, 14, 0, 270, M["teaPaper"], 12, 1, 2)
    P = [(bag, 0), (rbox("Fold", -105, 105, -9, 9, 262, 300, M["teaPaper"], 3, 0, 2), 0), (rbox("Staple", -10, 10, -10, 10, 286, 292, M["rail"], 1, 0, 1), 0)]
    s = H.tubes("String", [[Vector(u(*p)) for p in ((0, 0, 296), (8, 2, 700), (-6, -3, 1200), (0, 0, 1640))]], 1.8 * K, res_u=6, bevel_res=1, tip=1.0)
    s.data.materials.append(M["string"]); P.append((s, 0))
    P.append((rbox("Tag", -80, 80, -2, 2, 1640, 1760, M["teaTag"], 1, 0, 1), 0))
    return P

def dogfoodbag(M):
    return [(rbox("Bag", -300, 300, -190, 190, 0, 800, M["dogBag"], 60, 1, 3), 0), (rbox("Seal", -310, 310, -24, 24, 770, 890, M["dogBag"], 8, 0, 2), 0)]

def floursack(M):
    P = [(rbox("Sack", -280, 280, -190, 190, 0, 650, M["flour"], 50, 1, 3), 0)]
    P.append((lathe("Gather", [(0, 640), (185, 640), (120, 690), (70, 720), (90, 760), (140, 790), (100, 792), (0, 760)], M["flourTop"], 24), 1))
    tw = H.tubes("Twine", [[Vector(u(76 * math.cos(a), 76 * math.sin(a), 718)) for a in [2 * math.pi * k / 16 for k in range(17)]]], 5 * K, res_u=4, bevel_res=1, tip=1.0)
    tw.data.materials.append(M["twine"]); P.append((tw, 1))
    return P

def jarsmall(M):
    g = [(0, 0), (66, 0), (74, 3), (79, 10), (80, 20), (80, 140), (78, 148), (70, 156), (64, 162), (64, 196), (61, 199), (58, 196), (58, 162), (66, 154), (74, 146), (76, 138), (76, 22), (73, 12), (64, 7), (0, 6)]
    return [(glass(lathe("Glass", g, M["glass"], 56)), 0),
            (lathe("Jam", [(0, 6.6), (64, 6.6), (72, 11), (75.4, 22), (75.4, 128), (60, 131), (0, 132)], M["jam"], 40), 2),
            (wrap_label("Label", 80.15, 30, 120, 120, M["jamLabelS"], 0.4, 40), 0)]

def jarlid_cloth(M):
    P = [(lathe("Cloth", [(0, 32), (40, 31.5), (62, 29), (70, 26), (78, 18), (86, 6), (88, 0), (85, 0), (75, 16), (67, 24), (0, 28)], M["gingham"], 48), 0)]
    tw = H.tubes("Tie", [[Vector(u(69 * math.cos(a), 69 * math.sin(a), 22)) for a in [2 * math.pi * k / 16 for k in range(17)]]], 2.5 * K, res_u=4, bevel_res=1, tip=1.0)
    tw.data.materials.append(M["twine"]); P.append((tw, 0))
    return P

def picklejar(M):
    g = [(0, 0), (92, 0), (102, 4), (109, 14), (110, 30), (110, 260), (106, 272), (96, 280), (92, 284), (92, 300), (86, 300), (86, 286), (90, 278), (100, 270), (104, 258), (104, 32), (100, 16), (90, 8), (0, 7)]
    P = [(glass(lathe("Glass", g, M["glass"], 64)), 0)]   # (no brine body: glass inside glass reads as a mirror)
    R = random.Random(43)
    for k in range(7):
        c = lathe_s("Pickle%d" % k, [(0, 0), (12, 4), (20, 20), (22, 60), (20, 110), (12, 128), (0, 132)], M["cuke"], 16)
        a = 2 * math.pi * k / 7; c.location = u(60 * math.cos(a), 60 * math.sin(a), 14 + (k % 2) * 90); c.rotation_euler = (R.uniform(-0.3, 0.3), R.uniform(-0.3, 0.3), 0); P.append((c, 2))
    P.append((lathe("Lid", [(0, 330), (88, 330), (96, 326), (98, 318), (98, 292), (93, 290), (92.6, 296), (92.6, 324), (0, 325)], M["steel"], 64), 1))
    P.append((wrap_label("Label", 110.15, 120, 240, 120, M["pickleLabel"], 0.4, 48), 0))
    return P

def colabottle(M):
    g = [(0, 8), (30, 0), (50, 12), (62, 2), (80, 8), (96, 30), (101, 60), (101, 230), (96, 248), (101, 262), (101, 380), (96, 400), (78, 450), (50, 510), (30, 548), (28, 556),
         (30, 562), (30, 570), (27, 572), (27, 600), (24, 610), (22, 610), (24, 600), (24, 574), (26, 568), (26, 560), (46, 512), (74, 452), (92, 402), (97, 380), (97, 64), (92, 34), (78, 12), (62, 6), (50, 16), (30, 5), (0, 12)]
    return [(glass(lathe("Pet", g, M["pet"], 64)), 0),
            (lathe("Cola", [(0, 14), (30, 10), (48, 20), (62, 10), (76, 16), (92, 36), (96.4, 64), (96.4, 380), (92, 400), (80, 430), (0, 430)], M["cola"], 48), 2),
            (wrap_label("Label", 101.15, 250, 380, 180, M["colaLabel"], 0.3, 64), 0)]

def colacap(M):
    return [(lathe_s("Cap", [(0, 38), (24, 38), (28, 34), (28.5, 4), (27, 0), (24, 0), (24, 32), (0, 33)], M["capRed"], 40), 0)]

def grocerybag(M):
    """a paper bag on its side, the mouth to -Y: the upper side sags into the mouth, the side gussets fold in, the paper is
    creased; twisted paper handles, a baguette and apples spilling out. Inside it is a cave (the ants' way in).
    (KAN-22 fix: it was a rigid box of six flat boards.)"""
    W, HH, Y0, Y1, NJ, nf, nw, nr = 550.0, 640.0, -700.0, 600.0, 26, 10, 8, 14
    def sect(tau):   # 0 the mouth .. 1 the bag's own bottom: the roof's sag, the gusset's depth, how far the roof has slid to +X
        op = (1 - tau) ** 1.5; return 190.0 * op, 125.0 * op ** 0.6 * min(1.0, (1 - tau) * 6.0), 45.0 * op
    def dent(x, y):  # two long creases across the upper side
        d = 0.0
        for (ax, ay, bx, by), dep, wd in (((-200, -700, 350, 200), 24.0, 42.0), ((-520, -80, 520, 170), 17.0, 38.0)):
            ln = math.hypot(bx - ax, by - ay); d += dep * math.exp(-(((x - ax) * (by - ay) - (y - ay) * (bx - ax)) / ln / wd) ** 2)
        return d
    rings = []
    for j in range(NJ + 1):
        tau = (j / NJ) ** 1.2; y = Y0 + (Y1 - Y0) * tau; sag, gus, skew = sect(tau); zt = HH - 0.3 * sag; r = []
        for i in range(nf): r.append((-W + 2 * W * i / nf, 0.0))
        for i in range(nw): t = i / nw; r.append((W - gus * (1 - abs(2 * t - 1)) + skew * t, zt * t))
        for i in range(nr): t = i / nr; x = W - 2 * W * t + skew; r.append((x, zt - 0.7 * sag * math.sin(math.pi * t) ** 1.3 - dent(x, y)))
        for i in range(nw): t = 1 - i / nw; r.append((-W + gus * (1 - abs(2 * t - 1)) + skew * t, zt * t))
        edge = 28.0 * (1 - tau) ** 6     # the mouth's edge is not a straight cut
        rings.append([(x, y + edge * _nz(x / 260.0, z / 260.0, 5.0), z) for x, z in r])
    bag = skin("Bag", rings, M["bagPaper"], close_u=True, cap_end=True)
    crumple(bag, 13.0, 190.0, 3.0, lambda p: 0.0 if p.z < 1.0 else (0.35 if p.y > Y1 - 2 else min(1.0, p.z / 60.0)))
    for v in bag.data.vertices: v.co.z = max(v.co.z, 0.0)
    so = bag.modifiers.new("solid", "SOLIDIFY"); so.thickness = 5 * K; so.offset = -1.0
    P = [(bag, 0)]
    for pts in (((170, -650, 452), (176, -702, 432), (122, -724, 335), (0, -728, 292), (-122, -724, 335), (-176, -702, 432), (-170, -650, 452)),   # hangs from the upper edge
                ((-170, -664, 10), (-150, -612, 11), (0, -586, 11), (150, -612, 11), (170, -664, 10))):                                                # the lower one lies on the paper inside
        tw = H.tubes("Handle", [[Vector(u(*p)) for p in pts]], 7 * K, res_u=6, bevel_res=1, tip=1.0); tw.data.materials.append(M["twine"]); P.append((tw, 2))
    b = lathe("Baguette", [(0, 0), (22, 4), (44, 22), (56, 70), (60, 160), (60, 640), (54, 730), (40, 776), (20, 794), (0, 798)], M["crust"], 20)
    b.rotation_euler = (math.pi / 2, 0, -0.12); b.location = u(-200, 70, 67); P.append((b, 1))
    for k, (x, y, r, rz) in enumerate(((205, -520, 72, 0.4), (335, -648, 68, 2.1), (40, -320, 70, 4.0))):
        for o in _fruit("Apple%d" % k, _apple_prof(r), M["apple"], (x, y, 5.5), (0.12 * math.cos(rz), 0.12 * math.sin(rz), rz), 16, M["stem"], 1.6 * r): P.append((o, 1))
    return P

def _hinge(o):
    """the object's origin moved to the world origin (an rbox's is its centre): built round its hinge, it then turns about it"""
    o.data.transform(Matrix.Translation(o.location)); o.location = (0, 0, 0); return o

def cardboardbox(M):
    """the fort: an open box, flaps up and out, a door cut in the front, packing tape"""
    w, d, h, t = 880, 600, 1210, 10
    P = [(rbox("Bottom", -w, w, -d, d, 0, t, M["box"], 3, 0, 1), 0), (rbox("Back", -w, w, d - t, d, 0, h, M["box"], 3, 0, 1), 0),
         (rbox("Left", -w, -w + t, -d, d, 0, h, M["box"], 3, 0, 1), 0), (rbox("Right", w - t, w, -d, d, 0, h, M["box"], 3, 0, 1), 0),
         (rbox("FrontL", -w, -260, -d, -d + t, 0, h, M["box"], 3, 0, 1), 0), (rbox("FrontR", 260, w, -d, -d + t, 0, h, M["box"], 3, 0, 1), 0),
         (rbox("FrontTop", -260, 260, -d, -d + t, 640, h, M["box"], 3, 0, 1), 0)]
    door = _hinge(rbox("Door", 0, 260, 0, t, 0, 620, M["box"], 3, 0, 1)); door.rotation_euler = (0, 0, math.radians(-25)); door.location = u(-260, -d, 0)   # the door flap swung out
    P.append((door, 0))
    for nm, pivot, size, ax, ang in (("FlapF", (0, -d, h), (2 * w, t, 400), "X", -12), ("FlapB", (0, d, h), (2 * w, t, 400), "X", 12),
                                     ("FlapL", (-w, 0, h), (t, 2 * d, 400), "Y", -12), ("FlapR", (w, 0, h), (t, 2 * d, 400), "Y", 12)):
        f = _hinge(rbox(nm, -size[0] / 2, size[0] / 2, -size[1] / 2, size[1] / 2, 0, size[2], M["corr"], 3, 0, 1))
        f.rotation_euler = (math.radians(ang), 0, 0) if ax == "X" else (0, math.radians(ang), 0); f.location = u(*pivot); P.append((f, 0))
    P.append((rbox("Tape", -60, 60, -d - 2, -d + t + 2, 900, h + 2, M["tape"], 1, 0, 1), 0))
    return P

def buttertub(M):
    body = [(0, 0), (200, 0), (208, 4), (212, 14), (226, 300), (228, 304), (224, 306), (220, 302), (206, 16), (196, 8), (0, 8)]
    return [(lathe("Tub", body, M["butterTub"], 64), 0), (lathe("Band", [(215, 60), (224.2, 250), (224.6, 250), (215.4, 60)], M["butterLabel"], 64), 0),
            (lathe("Lid", [(0, 360), (210, 360), (226, 354), (232, 340), (232, 300), (228, 298), (226, 302), (226, 336), (0, 352)], M["butterTop"], 64), 1)]

def icecreamtub(M):
    body = [(0, 0), (270, 0), (280, 6), (286, 20), (338, 430), (340, 436), (336, 438), (332, 432), (280, 22), (270, 10), (0, 10)]
    return [(lathe("Tub", body, M["iceLabel"], 72), 0),
            (lathe("Lid", [(0, 500), (330, 500), (344, 494), (348, 480), (348, 420), (343, 418), (341, 424), (341, 476), (0, 488)], M["iceLid"], 72), 1)]

def _cutp(z): return [((0, 0, z), (0.12, 0.08, 1.0), 0.010, 131, 0)]

SPECS.update({   # batch 9
    "SM_K_CookieTin": dict(build=cookietin, hulls=lambda n: hulls_round(n, 352, 297) + [H.hull("UCX_%s_01" % n, boxp(358, 545, -356, 356, 0, 594))], lods=(0.45, 0.2), cls="metal",
                           breaks=lambda o, n: cut_groups(o, n, [], [lambda p, g: p == 0, lambda p, g: p == 1, lambda p, g: p == 2], [])),
    "SM_K_TeaBag": dict(build=teabag, hulls=_box_hulls((-105, 105, -16, 16, 0, 300), (-80, 80, -6, 6, 1640, 1760)), lods=(0.5, 0.25), cls="paper", breaks=None),
    "SM_V3_DogFoodBag": dict(build=dogfoodbag, hulls=_box_hulls((-300, 300, -190, 190, 0, 800), (-310, 310, -24, 24, 770, 890)), lods=(0.5, 0.25), cls="paper",
                             breaks=lambda o, n: cut_groups(o, n, [lambda p, g: True], [], _cutp(520))),
    "SM_K_FlourSack": dict(build=floursack, hulls=_box_hulls((-280, 280, -190, 190, 0, 650), (-140, 140, -140, 140, 640, 790)), lods=(0.5, 0.25), cls="paper",
                           breaks=lambda o, n: cut_groups(o, n, [lambda p, g: p == 0], [lambda p, g: p == 1], _cutp(420))),
    "SM_V3_JamJar": dict(build=jarsmall, hulls=lambda n: hulls_round(n, 80, 162, 64, (162, 199)), lods=(0.45, 0.18), cls="glass", glass=0.12,
                         breaks=lambda o, n: shatter(o, n, lambda p, g: g == 1, [lambda p, g: g == 0 and p == 0], [], n=14, seed=47, jag=0.0004, chips=8)),
    "SM_V3_JamJar_Lid": dict(build=jarlid_cloth, hulls=lambda n: hulls_round(n, 88, 32), lods=(0.5, 0.25), cls="cloth", breaks=None),
    "SM_V3_PickleJar": dict(build=picklejar, hulls=lambda n: hulls_round(n, 110, 290, 98, (290, 330)), lods=(0.45, 0.18), cls="glass", glass=0.14,
                            breaks=lambda o, n: shatter(o, n, lambda p, g: g == 1 and p == 0, [lambda p, g: g == 0 and p == 0], [lambda p, g: p == 1], n=15, seed=53, jag=0.0005, chips=9)),
    "SM_V3_ColaBottle": dict(build=colabottle, hulls=lambda n: hulls_round(n, 101, 380) + [H.hull("UCX_%s_01" % n, ring(97, 380, 12) + ring(28, 560, 12) + ring(27, 610, 8))],
                             lods=(0.45, 0.18), cls="plastic", glass=0.18, glassmat="pet",
                             breaks=lambda o, n: cut_groups(o, n, [lambda p, g: p == 0], [], [((0, 0, 300), (0.3, 0.1, 1.0), 0.002, 137, 0)])),
    "SM_V3_ColaCap": dict(build=colacap, hulls=lambda n: hulls_round(n, 28.5, 38), lods=(0.5, 0.25), cls="plastic", breaks=None),
    "SM_K_GroceryBag": dict(build=grocerybag, hulls=_box_hulls((-550, 550, -700, 600, 0, 6), (-550, 550, -700, 600, 640, 646), (-550, -544, -700, 600, 0, 646),
                                                                (544, 550, -700, 600, 0, 646), (-550, 550, 594, 600, 0, 646)), lods=(0.5, 0.25), cls="paper",
                            breaks=lambda o, n: cut_groups(o, n, [lambda p, g: p == 0], [lambda p, g: p == 1], [((0, -100, 320), (1.0, 0.1, 0.1), 0.012, 139, 0)])),
    "SM_K_CardboardBox": dict(build=cardboardbox, lods=(0.5, 0.25), cls="cardboard", breaks=None,
                              hulls=_box_hulls((-880, 880, -600, 600, 0, 10), (-880, 880, 590, 600, 0, 1210), (-880, -870, -600, 600, 0, 1210), (870, 880, -600, 600, 0, 1210),
                                               (-880, -260, -600, -590, 0, 1210), (260, 880, -600, -590, 0, 1210), (-260, 260, -600, -590, 640, 1210))),
    "SM_K_ButterTub": dict(build=buttertub, hulls=lambda n: hulls_round(n, 228, 306, 232, (300, 360)), lods=(0.45, 0.2), cls="plastic",
                           breaks=lambda o, n: cut_groups(o, n, [], [lambda p, g: p == 0, lambda p, g: p == 1], [])),
    "SM_K_IceCreamTub": dict(build=icecreamtub, hulls=lambda n: [H.hull("UCX_%s_00" % n, ring(286, 0, 16) + ring(340, 436, 16))] + [H.hull("UCX_%s_01" % n, ring(348, 420, 16) + ring(330, 500, 16))],
                             lods=(0.45, 0.2), cls="paper", breaks=lambda o, n: cut_groups(o, n, [], [lambda p, g: p == 0, lambda p, g: p == 1], [])),
})

# ---- batch 10: paper, cloth and cables (calendar, fridge note, kid's drawing, newspaper tent, magazines, towel, paper towel,
#      fly paper, dustpan, charger, power strip)
def sheet(name, path, x0, x1, mat, th=1.0, nx=24, wave=(0.0, 0.0), axis="x"):
    """a thin sheet: the cross-section path [(y, z)] swept along X from x0 to x1 (nx steps), waved in Y by amp*sin(x*freq);
    solidified th thick (applied by bake_mesh)"""
    me = bpy.data.meshes.new(name); bm = bmesh.new(); rows = []
    for i in range(nx + 1):
        x = x0 + (x1 - x0) * i / nx; dy = wave[0] * math.sin(x * wave[1])
        rows.append([bm.verts.new(u(x, y + dy, z)) for y, z in path])
    for i in range(nx):
        for j in range(len(path) - 1):
            bm.faces.new((rows[i][j], rows[i + 1][j], rows[i + 1][j + 1], rows[i][j + 1]))
    bm.to_mesh(me); bm.free()
    o = H.link(bpy.data.objects.new(name, me)); o.data.materials.append(mat)
    s = o.modifiers.new("solid", "SOLIDIFY"); s.thickness = th * K; s.offset = 0.0
    for p in o.data.polygons: p.use_smooth = True
    return o

def stripes_cloth(name, a, b, period_uu=60.0):
    m = felt(name, b, 0.7, 0.8); p = m.node_tree.nodes["Principled BSDF"]
    tc = N(m, "ShaderNodeTexCoord", -1300, -700); wv = N(m, "ShaderNodeTexWave", -1100, -700); wv.bands_direction = "Z"; wv.wave_profile = "SIN"
    wv.inputs["Scale"].default_value = 0.628 / (period_uu * K); wv.inputs["Distortion"].default_value = 0.0
    L(m, tc.outputs["Object"], wv.inputs["Vector"])
    mx = N(m, "ShaderNodeMix", -800, -700); mx.data_type = "RGBA"; mx.inputs[6].default_value = (*b, 1); mx.inputs[7].default_value = (*a, 1)
    L(m, _math(m, "GREATER_THAN", wv.outputs["Fac"], 0.72, -950, -800), mx.inputs[0]); L(m, mx.outputs[2], p.inputs["Base Color"]); return m

def sticky(name):
    """fly paper: amber, wet-glossy, a few dark flies stuck on"""
    m, p = H.mat(name); tc = N(m, "ShaderNodeTexCoord", -1100)
    vo = N(m, "ShaderNodeTexVoronoi", -900); vo.inputs["Scale"].default_value = 40; L(m, tc.outputs["Object"], vo.inputs["Vector"])
    sd = N(m, "ShaderNodeValToRGB", -700); e = sd.color_ramp.elements; e[0].position = 0.06; e[0].color = (0.02, 0.02, 0.02, 1); e[1].position = 0.075; e[1].color = (0.78, 0.48, 0.10, 1)
    L(m, vo.outputs["Distance"], sd.inputs["Fac"]); L(m, sd.outputs["Color"], p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = 0.05; p.inputs["Coat Weight"].default_value = 1.0
    return H.tag(m, 0.0, 0.0)

def mats10(M):
    M["calendar"] = print_box("CalendarPrint", {"front": "calendar.png", "back": "plain_white.png", "side": "plain_white.png", "top": "plain_white.png"}, -350, 350, -36, 0, 0, 1000, 0.6, (0.97, 0.96, 0.93))
    M["note"] = print_box("NotePrint", {"front": "fridgenote.png", "back": "plain_yellow.png", "side": "plain_yellow.png", "top": "plain_yellow.png"}, -200, 200, -6, 0, 0, 520, 0.7, (0.99, 0.92, 0.47))
    M["kid"] = print_box("KidPrint", {"front": "kiddrawing.png", "back": "plain_white.png", "side": "plain_white.png", "top": "plain_white.png"}, -200, 200, -9, 0, -285, 285, 0.8, (0.98, 0.98, 0.97))
    M["news"] = top_print("NewsPrint", "newspaper.png", -450, 450, -527, 527, (0.91, 0.89, 0.84), 0.85, 0.0, 0.0, 0.8)
    for k, (x0, y0) in enumerate(((-230, -290), (-190, -250), (-150, -300))):
        M["mag%d" % k] = top_print("Magazine%d" % k, "magazine%d.png" % k, x0, x0 + 405, y0, y0 + 540, (0.9, 0.9, 0.88), 0.25, 0.0, 0.4, 0.4)
    M["magPages"] = pages_mat("MagPages")
    M["towel"] = stripes_cloth("TowelStripes", (0.70, 0.12, 0.10), (0.95, 0.94, 0.90))
    M["paperTowel"] = felt("PaperTowel", (0.97, 0.97, 0.95), 0.4, 0.2)
    M["chrome"] = H.metal("Chrome", (0.92, 0.92, 0.93), 0.08)
    M["sticky"] = sticky("FlyPaper")
    M["dustpan"] = H.plastic("DustpanRed", (0.62, 0.08, 0.06), 0.4, peel=0.04, scuff=0.35)
    M["cable"] = H.plastic("CableWhite", (0.90, 0.90, 0.88), 0.35, peel=0.02, scuff=0.15)
    M["magnetRed"] = H.plastic("MagnetRed", (0.75, 0.06, 0.05), 0.2, peel=0.0, scuff=0.1)
    return M

def calendar(M):
    P = [(rbox("Back", -350, 350, -2, 0, 0, 1000, M["calendar"], 0.8, 0, 1), 0)]
    P.append((sheet("Page", [(-3, 1000), (-4, 600), (-8, 200), (-20, 60), (-36, 0)], -350, 350, M["calendar"], 1.2, 12), 0))   # the top page curls off the wall
    for k in range(14):   # the wire binding
        x = -325 + k * 50
        rg = H.tubes("Wire", [[Vector(u(x + 6 * math.cos(a) * 0, -6 + 12 * math.cos(a), 1000 + 14 * math.sin(a))) for a in [2 * math.pi * j / 10 for j in range(11)]]], 1.6 * K, res_u=2, bevel_res=0, tip=1.0)
        rg.data.materials.append(M["rail"]); P.append((rg, 0))
    P.append((lathe_s("Nail", [(0, 0), (8, 0), (8, 3), (2.5, 4), (2.5, 30), (0, 30)], M["rail"], 12, 0, (0, 0, 1035)), 0))
    P[-1][0].rotation_euler = (-math.pi / 2, 0, 0)   # into the wall
    return P

def fridgenote(M):
    mg = lathe_s("Magnet", [(0, 0), (30, 0), (32, 6), (30, 40), (22, 50), (0, 52)], M["magnetRed"], 32); mg.rotation_euler = (math.pi / 2, 0, 0); mg.location = u(150, -8, 478)
    return [(sheet("Note", [(-4, 520), (-4, 200), (-10, 60), (-30, 0)], -200, 200, M["note"], 1.0, 12), 0), (mg, 1)]

def kiddrawing(M):
    P = [(sheet("Paper", [(-2, 285), (-3, 100), (-2, -100), (-6, -240), (-9, -285)], -200, 200, M["kid"], 1.0, 16, (1.5, 0.03)), 0)]
    for x, c in ((-170, "padY"), (170, "padT")):
        mg = lathe_s("Magnet", [(0, 0), (18, 0), (20, 4), (18, 9), (0, 10)], M[c], 24); mg.rotation_euler = (math.pi / 2, 0, 0); mg.location = u(x, -3, 255); P.append((mg, 1))
    return P

def newspaper(M):
    return [(sheet("Tent", [(-527, 0), (-400, 80), (-260, 172), (-120, 270), (-30, 330), (0, 344), (30, 330), (120, 270), (260, 172), (400, 80), (527, 0)], -450, 450, M["news"], 2.0, 20, (4.0, 0.012)), 0)]

def magazines(M):
    P = []
    for k, (x0, y0) in enumerate(((-230, -290), (-190, -250), (-150, -300))):
        P.append((rbox("Pages%d" % k, x0 + 3, x0 + 402, y0 + 3, y0 + 537, k * 20, k * 20 + 19, M["magPages"], 1, 0, 1), 0))
        P.append((rbox("Cover%d" % k, x0, x0 + 405, y0, y0 + 540, k * 20 + 19, k * 20 + 21, M["mag%d" % k], 0.6, 0, 1), 0))
    return P

def towel(M):
    path = [(-40, 0), (-44, 300), (-38, 700), (-42, 1100), (-40, 1380), (-28, 1420), (0, 1436), (28, 1420), (40, 1380), (42, 1100), (44, 820), (48, 700)]
    return [(sheet("Towel", path, -300, 300, M["towel"], 6.0, 30, (10.0, 0.02)), 0)]

def papertowel(M):
    roll = lay_x(lathe("Roll", [(40, -250), (140, -250), (145, -246), (145, 246), (140, 250), (40, 250)], M["paperTowel"], 48), 145)
    P = [(roll, 0), (lay_x(lathe_s("Core", [(36, -252), (40, -252), (40, 252), (36, 252)], M["kraft"], 24), 145), 0)]
    P.append((lay_x(lathe_s("Rod", [(0, -275), (14, -275), (14, 275), (0, 275)], M["chrome"], 16), 145), 0))
    for x in (-270, 270): P.append((rbox("Stand", x - 5, x + 5, -60, 60, -25, 160, M["chrome"], 3, 0, 2), 0))
    P.append((sheet("Sheet", [(-145, 145), (-147, 400), (-142, 700), (-136, 1000), (-120, 1250)], -240, 240, M["paperTowel"], 1.0, 16, (3.0, 0.02)), 0))
    return P

def flypaper(M):
    me = bpy.data.meshes.new("Ribbon"); bm = bmesh.new(); rows = []; n = 120
    for i in range(n + 1):
        z = 1960 * i / n; a = z * 0.006
        rows.append([bm.verts.new(u(s * 22 * math.cos(a), s * 22 * math.sin(a), z)) for s in (-1, 1)])
    for i in range(n): bm.faces.new((rows[i][0], rows[i + 1][0], rows[i + 1][1], rows[i][1]))
    bm.to_mesh(me); bm.free()
    rb = H.link(bpy.data.objects.new("Ribbon", me)); rb.data.materials.append(M["sticky"]); s = rb.modifiers.new("solid", "SOLIDIFY"); s.thickness = 1.0 * K
    for p in rb.data.polygons: p.use_smooth = True
    P = [(rb, 0), (lathe_s("Tube", [(0, 1950), (22, 1950), (22, 2060), (0, 2060)], M["kraft"], 24), 0)]
    rg = H.tubes("Loop", [[Vector(u(0, 14 * math.cos(a), 2075 + 18 * math.sin(a))) for a in [2 * math.pi * j / 10 - math.pi / 2 for j in range(11)]]], 1.5 * K, res_u=3, bevel_res=1, tip=1.0)
    rg.data.materials.append(M["rail"]); P.append((rg, 0))
    return P

def dustpan(M):
    P = [(rbox("Floor", -260, 260, -320, 120, 0, 6, M["dustpan"], 2, 0, 1), 0), (rbox("Lip", -258, 258, -326, -300, 0, 3, M["padDark"], 1, 0, 1), 0)]
    for x0, x1 in ((-260, -252), (252, 260)): P.append((rbox("Side", x0, x1, -320, 120, 0, 120, M["dustpan"], 2, 0, 1), 0))
    P.append((rbox("Back", -260, 260, 112, 120, 0, 130, M["dustpan"], 2, 0, 1), 0))
    h = H.tubes("Handle", [[Vector(u(*p)) for p in ((0, 116, 60), (0, 200, 120), (0, 290, 190), (0, 336, 222))]], 18 * K, res_u=6, bevel_res=2, tip=0.8)
    h.scale = (1.6, 1.0, 0.7); h.data.materials.append(M["dustpan"]); P.append((h, 0))
    return P

def charger(M):
    P = [(rbox("Plug", -45, 45, -30, 30, 1260, 1370, M["cable"], 8, 0, 3), 0)]
    for x in (-12, 12): P.append((lathe_s("Pin", [(0, 0), (3, 0), (3, 40), (0, 40)], M["rail"], 8, 0, (x, 30, 1300)), 0)); P[-1][0].rotation_euler = (-math.pi / 2, 0, 0)
    pts = [(0, -30, 1280), (0, -70, 1100), (20, -60, 500), (60, -60, 40), (120, -100, 5), (180, -400, 5), (150, -900, 5), (40, -1250, 30), (-80, -1420, 300), (-120, -1500, 600), (-100, -1540, 700)]
    c = H.tubes("Cable", [[Vector(u(*p)) for p in pts]], 5 * K, res_u=6, bevel_res=1, tip=1.0); c.data.materials.append(M["cable"]); P.append((c, 0))
    P.append((rbox("Jack", -110, -88, -1552, -1530, 700, 740, M["cable"], 3, 0, 2), 0))
    return P

def powerstrip(M):
    P = [(rbox("Strip", -280, 280, -60, 60, 0, 70, M["cable"], 12, 0, 3), 0), (rbox("Switch", 230, 266, -26, 26, 66, 80, M["padR"], 4, 0, 2), 0)]
    for k, x in enumerate((-200, -110, -20, 70)):
        P.append((lathe_s("Socket", [(0, 70.5), (34, 70.5), (36, 68), (0, 66)], M["padDark"], 24, 0, (x, 0, 0)), 0))
        if k < 3:
            P.append((rbox("Plug", x - 30, x + 30, -26, 26, 70, 130, M["padDark"], 8, 0, 3), 0))
    for pts in (((-200, 0, 130), (-210, 60, 140), (-300, 200, 10), (-500, 300, 5)), ((-110, 0, 130), (-120, 80, 200), (-60, 300, 600), (100, 470, 1000), (180, 490, 1125)),
                ((-20, 0, 130), (0, 70, 150), (100, 200, 10), (400, 420, 5), (510, 480, 5)), ((-280, 0, 35), (-340, -30, 20), (-460, -200, 5), (-500, -480, 5))):
        c = H.tubes("Cord", [[Vector(u(*p)) for p in pts]], 6 * K, res_u=6, bevel_res=1, tip=1.0); c.data.materials.append(M["padDark"]); P.append((c, 0))
    return P

def _tear(z, axis=(0.1, 0.05, 1.0)): return [((0, 0, z), axis, 0.008, 151, 0)]

SPECS.update({   # batch 10
    "SM_K_Calendar": dict(build=calendar, hulls=_box_hulls((-350, 350, -36, 2, 0, 1015)), lods=(0.5, 0.25), cls="paper", breaks=None),
    "SM_K_FridgeNote": dict(build=fridgenote, hulls=_box_hulls((-200, 200, -32, -2, 0, 520), (118, 182, -60, -8, 446, 510)), lods=(0.5, 0.25), cls="paper", breaks=None),
    "SM_V3_KidDrawing": dict(build=kiddrawing, hulls=_box_hulls((-200, 200, -10, 0, -285, 285)), lods=(0.5, 0.25), cls="paper", breaks=None),
    "SM_K_Newspaper": dict(build=newspaper, lods=(0.5, 0.25), cls="paper", breaks=None,
                           hulls=lambda n: [H.hull("UCX_%s_%02d" % (n, i), [(x * K, y * K, z * K) for x in (-450, 450) for y, z in tri]) for i, tri in
                                            enumerate((((-527, 0), (0, 344), (-5, 344), (-520, 0)), ((527, 0), (0, 344), (5, 344), (520, 0))))]),
    "SM_L_Magazines": dict(build=magazines, hulls=_box_hulls((-230, 255, -300, 290, 0, 61)), lods=(0.5, 0.25), cls="paper", breaks=None),
    "SM_K_Towel": dict(build=towel, hulls=_box_hulls((-300, 300, -52, -30, 0, 1400), (-300, 300, -45, 45, 1380, 1440), (-300, 300, 32, 58, 700, 1400)), lods=(0.5, 0.25), cls="cloth", breaks=None),
    "SM_K_PaperTowel": dict(build=papertowel, hulls=lambda n: [H.hull("UCX_%s_00" % n, [(x * K, 145 * K * math.cos(a), (145 + 145 * math.sin(a)) * K) for x in (-250, 250) for a in [2 * math.pi * k / 12 for k in range(12)]])]
                            + [H.hull("UCX_%s_01" % n, boxp(-240, 240, -150, -138, 145, 1250))], lods=(0.5, 0.25), cls="paper", breaks=None),
    "SM_K_FlyPaper": dict(build=flypaper, hulls=lambda n: hulls_round(n, 24, 2096), lods=(0.5, 0.25), cls="paper", breaks=None),
    "SM_K_Dustpan": dict(build=dustpan, hulls=_box_hulls((-260, 260, -326, 120, 0, 6), (-260, -252, -320, 120, 0, 120), (252, 260, -320, 120, 0, 120), (-260, 260, 112, 120, 0, 130), (-30, 30, 116, 341, 50, 237)),
                         lods=(0.5, 0.25), cls="plastic", breaks=None),
    "SM_L_Charger": dict(build=charger, hulls=_box_hulls((-45, 45, -30, 30, 1260, 1370)), lods=(0.5, 0.25), cls="plastic", breaks=None),
    "SM_L_PowerStrip": dict(build=powerstrip, hulls=_box_hulls((-280, 280, -60, 60, 0, 130)), lods=(0.5, 0.25), cls="plastic", breaks=None),
})

# ---- batch 11: the rest of the flat's small things (fruit bowl, knife block, dish rack, biscuit plate, snack bowl, glasses, coin,
#      knitting needle, sock, yarn ball, car track, train track, balloon and its weight, gift box and its lid)
# ---- KAN-22 fix (27.11): the six weak pieces redone (sock, grocery bag, cookie tin, fruit bowl, glasses, knife block).
#      Soft cloth and creased paper are skinned surfaces (rings of points), not boxes and straight sweeps; fruit and biscuits
#      are turned profiles of their own.
from mathutils import noise as mnoise

def _nz(x, y, z=0.0):
    """smooth noise, -1..1"""
    return mnoise.noise(Vector((x, y, z)))

def _keys(keys, s):
    """smooth interpolation between rows (s, v0, v1, ...)"""
    if s <= keys[0][0]: return keys[0][1:]
    for a, b in zip(keys, keys[1:]):
        if s <= b[0]:
            t = (s - a[0]) / (b[0] - a[0]); t = t * t * (3 - 2 * t)
            return tuple(x + (y - x) * t for x, y in zip(a[1:], b[1:]))
    return keys[-1][1:]

def skin(name, rings, mat, close_u=True, cap_end=False, attrs=None):
    """a surface through rings of points (uu): quads ring to ring. close_u joins each ring's last point to its first; cap_end
    closes the last ring with a fan. attrs {name: [[value per point] per ring]} -> float point attributes (for the shader)."""
    me = bpy.data.meshes.new(name); bm = bmesh.new(); n = len(rings[0])
    V = [[bm.verts.new(u(*p)) for p in r] for r in rings]
    for a, b in zip(V, V[1:]):
        for j in range(n if close_u else n - 1): bm.faces.new((a[j], a[(j + 1) % n], b[(j + 1) % n], b[j]))
    if cap_end:
        c = bm.verts.new(sum((v.co for v in V[-1]), Vector((0, 0, 0))) / n)
        for j in range(n if close_u else n - 1): bm.faces.new((V[-1][j], V[-1][(j + 1) % n], c))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); bm.to_mesh(me); bm.free()
    for k, vals in (attrs or {}).items():
        a = me.attributes.new(k, "FLOAT", "POINT"); flat = [x for r in vals for x in r]
        for i, x in enumerate(flat): a.data[i].value = x
        if cap_end: a.data[len(flat)].value = flat[-1]
    o = H.link(bpy.data.objects.new(name, me)); o.data.materials.append(mat)
    for p in o.data.polygons: p.use_smooth = True
    return o

def crumple(o, amp, cell, seed, weight):
    """creased paper: every vertex moved along its normal by ridged noise (amp, cell in uu); weight(co in uu) -> 0..1"""
    for v in o.data.vertices:
        p = v.co / K; w = weight(p)
        if w <= 0: continue
        n1 = 1 - abs(_nz(p.x / cell + seed, p.y / cell, p.z / cell)) * 2
        n2 = _nz(p.x / (cell * 0.37) + seed, p.y / (cell * 0.37) + 7.0, p.z / (cell * 0.37))
        v.co += v.normal * (amp * K * w * (0.75 * n1 + 0.35 * n2))
    return o

def outward(o):
    """a turned thin part with its modifiers applied and its normals made to point out: the screw's own normal calculation turns
    some profiles inside out (a face seen from behind is culled in the game, and bakes black). The test: a ray along a face's
    normal that hits the part's own other side within 12 uu started inside the wall. Call it before the part is moved."""
    from mathutils.bvhtree import BVHTree
    m = H.bake_mesh(o) if (o.type != "MESH" or o.modifiers) else o; me = m.data
    bv = BVHTree.FromPolygons([v.co[:] for v in me.vertices], [p.vertices[:] for p in me.polygons])
    bad = sum(p.area for p in me.polygons if bv.ray_cast(p.center + p.normal * 1e-5, p.normal, 12 * K)[0] is not None)
    if bad > 0.5 * sum(p.area for p in me.polygons): me.flip_normals(); print("HERO %s: normals turned out" % m.name)
    return m

def _xf(pts, rot_y, loc_uu):
    """points (metres) turned about Y and moved: the frame of a part placed with rotation_euler (0, rot_y, 0) and location"""
    m = Matrix.Translation(Vector(u(*loc_uu))) @ Matrix.Rotation(rot_y, 4, "Y")
    return [tuple(m @ Vector(p)) for p in pts]

def _attr(m, name, x=-1900, y=0):
    n = N(m, "ShaderNodeAttribute", x, y); n.attribute_name = name; return n.outputs["Fac"]

def _fmix(m, fac, a, b, x=0, y=0):
    n = N(m, "ShaderNodeMix", x, y); n.data_type = "FLOAT"; L(m, fac, n.inputs[0]); L(m, a, n.inputs[2]); L(m, b, n.inputs[3]); return n.outputs[0]

def _curve(pts, n=4):
    """a smooth path through the points (Catmull-Rom, n steps per span)"""
    P = [Vector(p) for p in pts]; out = []
    for i in range(len(P) - 1):
        p0, p1, p2, p3 = P[max(i - 1, 0)], P[i], P[i + 1], P[min(i + 2, len(P) - 1)]
        for k in range(n):
            t = k / n; t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3)))
    out.append(tuple(P[-1])); return out

def ring_tube(name, pts_uu, r_uu, mat, nsec=8):
    """a round tube along a closed flat loop in the XZ plane (a spectacle rim): the section stays square to the loop all the
    way round (sweep()'s frame flips where the path runs straight up, and the rim got a notch there)"""
    n = len(pts_uu); rings = []
    for i in range(n + 1):
        p = Vector(pts_uu[i % n]); t = (Vector(pts_uu[(i + 1) % n]) - Vector(pts_uu[(i - 1) % n])).normalized(); out = Vector((t.z, 0, -t.x))
        rings.append([tuple(p + out * (r_uu * math.cos(b)) + Vector((0, r_uu * math.sin(b), 0))) for b in [2 * math.pi * k / nsec for k in range(nsec)]])
    return skin(name, rings, mat)

def _biscuit(name, mat, r=66.0, lobes=10):
    """a butter biscuit: a fluted round, domed (attribute bk_r for the shader: 0 the middle .. 1 the edge, baked darker)"""
    n = lobes * 2; rows = ((0.03, 0.0, 0.9), (0.90, 0.0, 1.0), (1.0, 6.0, 1.0), (0.97, 13.0, 0.9), (0.72, 19.0, 0.55), (0.38, 22.0, 0.25), (0.03, 23.0, 0.0))
    fl = lambda f, j: r * f * (0.89 if (j % 2 and f > 0.6) else 1.0)
    rings = [[(fl(f, j) * math.cos(2 * math.pi * j / n), fl(f, j) * math.sin(2 * math.pi * j / n), z) for j in range(n)] for f, z, e in rows]
    return skin(name, rings, mat, attrs={"bk_r": [[e] * n for f, z, e in rows]})

def biscuit_mat(name):
    m, p = H.mat(name); tc = N(m, "ShaderNodeTexCoord", -1300)
    cr = N(m, "ShaderNodeValToRGB", -700); e = cr.color_ramp.elements; e[0].position = 0.15; e[0].color = (0.66, 0.40, 0.14, 1); e[1].position = 1.0; e[1].color = (0.34, 0.14, 0.035, 1)
    md = cr.color_ramp.elements.new(0.7); md.color = (0.58, 0.32, 0.10, 1)
    L(m, _attr(m, "bk_r", -1000, 0), cr.inputs["Fac"])
    no = N(m, "ShaderNodeTexNoise", -1000, -300); no.inputs["Scale"].default_value = 160; no.inputs["Detail"].default_value = 6; L(m, tc.outputs["Object"], no.inputs["Vector"])
    mx = N(m, "ShaderNodeMix", -400, 0); mx.data_type = "RGBA"; mx.blend_type = "MULTIPLY"; mx.inputs[0].default_value = 0.35; L(m, cr.outputs["Color"], mx.inputs[6]); L(m, no.outputs["Color"], mx.inputs[7])
    L(m, mx.outputs[2], p.inputs["Base Color"]); p.inputs["Roughness"].default_value = 0.78
    gr = N(m, "ShaderNodeTexNoise", -1000, -600); gr.inputs["Scale"].default_value = 1100; L(m, tc.outputs["Object"], gr.inputs["Vector"])
    bp = N(m, "ShaderNodeBump", -300, -500); bp.inputs["Strength"].default_value = 0.35; bp.inputs["Distance"].default_value = 0.0005
    L(m, gr.outputs["Fac"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return H.tag(m, 0.0, 0.7)

def lid_print(name, img, base, rough=0.2, coat=0.3):
    """a lid's print by the lid's own coordinates (attributes lid_u, lid_v, lid_top, set before the lid is leaned on the tin):
    the print stays on its top however it stands (top_print() projects along the world's Z after the parts are joined)"""
    m, p = H.mat(name)
    t = _img(m, os.path.join(LAB, img), _attr(m, "lid_u", -1000, -1000), _attr(m, "lid_v", -1000, -1100), -700, -1000)
    a = _math(m, "MULTIPLY", t.outputs["Alpha"], _math(m, "GREATER_THAN", _attr(m, "lid_top", -1000, -1200), 0.5, -800, -1200), -500, -1000)
    L(m, _over(m, base, t.outputs["Color"], a, -200, -900), p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = rough; p.inputs["Coat Weight"].default_value = coat
    return H.tag(m, 0.0, 0.05)

def crust(name):
    """a baguette's crust: baked gold in patches, pale floury slashes across the loaf (it lies along Y) on its upper side"""
    m, p = H.mat(name); tc = N(m, "ShaderNodeTexCoord", -1500)
    no = N(m, "ShaderNodeTexNoise", -1100); no.inputs["Scale"].default_value = 40; no.inputs["Detail"].default_value = 5; L(m, tc.outputs["Object"], no.inputs["Vector"])
    cr = N(m, "ShaderNodeValToRGB", -900); e = cr.color_ramp.elements; e[0].position = 0.35; e[0].color = (0.40, 0.17, 0.04, 1); e[1].position = 0.7; e[1].color = (0.62, 0.34, 0.09, 1)
    L(m, no.outputs["Fac"], cr.inputs["Fac"])
    mp = N(m, "ShaderNodeMapping", -1300, -500); mp.inputs["Rotation"].default_value = (0, 0, math.radians(38)); L(m, tc.outputs["Object"], mp.inputs["Vector"])
    wv = N(m, "ShaderNodeTexWave", -1100, -500); wv.bands_direction = "Y"; wv.wave_profile = "SIN"; wv.inputs["Scale"].default_value = 0.314 / (150 * K); wv.inputs["Distortion"].default_value = 1.2
    wv.inputs["Detail"].default_value = 1.0; wv.inputs["Detail Scale"].default_value = 0.4; L(m, mp.outputs["Vector"], wv.inputs["Vector"])
    nrm = N(m, "ShaderNodeSeparateXYZ", -1100, -800); L(m, tc.outputs["Normal"], nrm.inputs["Vector"])
    sl = _math(m, "MULTIPLY", _maprange(m, wv.outputs["Fac"], 0.86, 0.95, 0.0, 1.0, True, -900, -500), _maprange(m, nrm.outputs["Z"], 0.25, 0.55, 0.0, 1.0, True, -900, -800), -700, -600)
    mx = N(m, "ShaderNodeMix", -400, 0); mx.data_type = "RGBA"; L(m, cr.outputs["Color"], mx.inputs[6]); mx.inputs[7].default_value = (0.80, 0.62, 0.36, 1); L(m, sl, mx.inputs[0])
    L(m, mx.outputs[2], p.inputs["Base Color"]); p.inputs["Roughness"].default_value = 0.6
    bp = N(m, "ShaderNodeBump", -300, -500); bp.invert = True; bp.inputs["Strength"].default_value = 0.6; bp.inputs["Distance"].default_value = 0.002
    L(m, sl, bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return H.tag(m, 0.0, 0.7)

def _orange_prof(r):
    return [(0, 0.05 * r)] + [(r * math.sin(math.pi * k / 10), r * (1 - math.cos(math.pi * k / 10))) for k in range(1, 10)] + [(0, 1.96 * r)]

SOCK_L2 = 120.0 * math.pi / 2     # the heel turn's length along the centre line

SOCK_R, SOCK_L1, SOCK_L3 = 120.0, 742.0, 370.0     # the heel turn's radius, the leg's and the foot's length along the centre line (uu)
SOCK_LEN = SOCK_L1 + SOCK_R * math.pi / 2 + SOCK_L3; SOCK_HEEL = SOCK_L1 + SOCK_R * math.pi / 4

def sock_knit(name, red, cream):
    """knitted cotton: rings of colour along the leg and the foot (attribute sk_t: the distance from the cuff's edge, uu), red
    cuff, heel and toe; the wales of the knit and the cuff's ribs as bump (sk_v: round the tube, 0..1, 0.25 = the outside of the turn)"""
    m = felt(name, cream, 0.35, 0.7); p = m.node_tree.nodes["Principled BSDF"]
    base = p.inputs["Base Color"].links[0].from_socket; fz = p.inputs["Normal"].links[0].from_socket
    t = _attr(m, "sk_t", -1900, -900); v = _attr(m, "sk_v", -1900, -1100)
    sq = lambda s, y: _math(m, "MULTIPLY", s, s, -1400, y)
    span = lambda a, b, y: _math(m, "MULTIPLY", _math(m, "GREATER_THAN", t, a, -1550, y), _math(m, "LESS_THAN", t, b, -1550, y - 60), -1400, y)
    fr = _math(m, "FRACT", _math(m, "DIVIDE", t, 72.0, -1700, -900), None, -1550, -900)
    ring = _math(m, "MULTIPLY", _math(m, "LESS_THAN", fr, 0.40, -1400, -900),
                 _math(m, "MAXIMUM", span(120.0, SOCK_L1 - 60.0, -1000), span(SOCK_L1 + SOCK_L2 + 62.0, SOCK_LEN - 150.0, -1150), -1250, -1000), -1100, -900)
    cuff = _math(m, "LESS_THAN", t, 66.0, -1400, -1300); toe = _math(m, "GREATER_THAN", t, SOCK_LEN - 118.0, -1400, -1360)
    hs = sq(_math(m, "DIVIDE", _math(m, "SUBTRACT", t, SOCK_HEEL, -1700, -1450), 112.0, -1550, -1450), -1450)
    hv = sq(_math(m, "DIVIDE", _math(m, "SUBTRACT", v, 0.25, -1700, -1550), 0.21, -1550, -1550), -1550)
    heel = _math(m, "LESS_THAN", _math(m, "ADD", hs, hv, -1250, -1450), 1.0, -1100, -1450)
    mask = ring
    for k, z in enumerate((cuff, toe, heel)): mask = _math(m, "MAXIMUM", mask, z, -950 + k * 120, -1000)
    mx = N(m, "ShaderNodeMix", -500, -900); mx.data_type = "RGBA"; L(m, base, mx.inputs[6]); mx.inputs[7].default_value = (*red, 1); L(m, mask, mx.inputs[0])
    L(m, mx.outputs[2], p.inputs["Base Color"])
    wale = _math(m, "SINE", _math(m, "MULTIPLY", v, 2 * math.pi * 54, -1700, -1700), None, -1550, -1700)
    rib = _math(m, "SINE", _math(m, "MULTIPLY", v, 2 * math.pi * 22, -1700, -1800), None, -1550, -1800)
    course = _math(m, "SINE", _math(m, "MULTIPLY", t, 2 * math.pi / 9.0, -1700, -1900), None, -1550, -1900)
    knit = _math(m, "ADD", _math(m, "MULTIPLY", wale, 0.5, -1400, -1700), _math(m, "MULTIPLY", course, 0.25, -1400, -1900), -1250, -1700)
    h = _fmix(m, _math(m, "LESS_THAN", t, 150.0, -1400, -2000), knit, _math(m, "MULTIPLY", rib, 1.7, -1400, -1800), -1050, -1700)
    bp = N(m, "ShaderNodeBump", -300, -1700); bp.inputs["Strength"].default_value = 0.6; bp.inputs["Distance"].default_value = 0.0008
    L(m, h, bp.inputs["Height"]); L(m, fz, bp.inputs["Normal"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return m

def bag_paper(name, img, x0, x1, y0, y1, zmin, col=(0.46, 0.29, 0.14), ink=(0.012, 0.12, 0.03)):
    """creased kraft paper with a print on its upper side: the label's dark ink only (the label's own flat ground would show as
    a patch), projected from above on what faces up over zmin; the creases of crumpled paper - long straight folds crossing each
    other (stretched cell edges, two directions) and fine ones - as bump, a little paler where the fibres broke"""
    m = kraft(name, col); p = m.node_tree.nodes["Principled BSDF"]
    base = p.inputs["Base Color"].links[0].from_socket; nb = p.inputs["Normal"].links[0].from_socket
    tc, sp = _sep(m)
    nrm = N(m, "ShaderNodeSeparateXYZ", -1400, -900); L(m, tc.outputs["Normal"], nrm.inputs["Vector"])
    top = _math(m, "MULTIPLY", _math(m, "GREATER_THAN", nrm.outputs["Z"], 0.35, -1200, -900), _math(m, "GREATER_THAN", sp.outputs["Z"], zmin * K, -1200, -800), -1050, -900)
    uu = _maprange(m, sp.outputs["X"], x0 * K, x1 * K, 0.0, 1.0, False, -1000, -1000)
    vv = _maprange(m, sp.outputs["Y"], y0 * K, y1 * K, 0.0, 1.0, False, -1000, -1100)
    t = _img(m, os.path.join(LAB, img), uu, vv, -700, -1000)
    sc = N(m, "ShaderNodeSeparateColor", -500, -1000); L(m, t.outputs["Color"], sc.inputs["Color"])
    inkm = _math(m, "MULTIPLY", _maprange(m, sc.outputs["Red"], 0.30, 0.10, 0.0, 1.0, True, -350, -1000), t.outputs["Alpha"], -200, -1000)
    inkm = _math(m, "MULTIPLY", inkm, top, -50, -1000)
    def creases(rot, stretch, scale, wdt, y):
        mp = N(m, "ShaderNodeMapping", -1150, y); mp.inputs["Rotation"].default_value = tuple(math.radians(a) for a in rot); mp.inputs["Scale"].default_value = stretch
        L(m, tc.outputs["Object"], mp.inputs["Vector"])
        vo = N(m, "ShaderNodeTexVoronoi", -900, y); vo.feature = "DISTANCE_TO_EDGE"; vo.inputs["Scale"].default_value = scale
        L(m, mp.outputs["Vector"], vo.inputs["Vector"]); return _maprange(m, vo.outputs["Distance"], 0.0, wdt, 1.0, 0.0, True, -700, y)
    cr = _math(m, "MAXIMUM", creases((10, 20, 24), (1.0, 0.26, 0.5), 8.0, 0.030, -1500), creases((-15, 5, -52), (0.28, 1.0, 0.5), 9.5, 0.030, -1700), -500, -1500)
    cr = _math(m, "MAXIMUM", cr, _math(m, "MULTIPLY", creases((40, 30, 75), (1.0, 1.0, 1.0), 27.0, 0.05, -1900), 0.3, -500, -1900), -350, -1500)
    bp = N(m, "ShaderNodeBump", -150, -1500); bp.invert = True; bp.inputs["Strength"].default_value = 0.75; bp.inputs["Distance"].default_value = 0.0016
    L(m, cr, bp.inputs["Height"]); L(m, nb, bp.inputs["Normal"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    worn = N(m, "ShaderNodeMix", 100, -900); worn.data_type = "RGBA"; L(m, base, worn.inputs[6]); worn.inputs[7].default_value = (min(1.0, col[0] * 1.35), min(1.0, col[1] * 1.4), min(1.0, col[2] * 1.5), 1)
    L(m, _math(m, "MULTIPLY", cr, 0.3, -100, -1300), worn.inputs[0])
    pr = N(m, "ShaderNodeMix", 300, -900); pr.data_type = "RGBA"; L(m, worn.outputs[2], pr.inputs[6]); pr.inputs[7].default_value = (*ink, 1); L(m, inkm, pr.inputs[0])
    L(m, pr.outputs[2], p.inputs["Base Color"])
    return m

def _apple_prof(r):
    return [(a * r, b * r) for a, b in ((0, 0.14), (0.20, 0.05), (0.45, 0.0), (0.72, 0.08), (0.93, 0.36), (1.0, 0.78), (0.96, 1.16), (0.80, 1.50), (0.52, 1.70), (0.26, 1.74), (0.09, 1.66), (0, 1.58))]

def _pear_prof(r):
    return [(a * r, b * r) for a, b in ((0, 0.10), (0.25, 0.02), (0.55, 0.0), (0.85, 0.18), (1.0, 0.55), (0.95, 0.95), (0.70, 1.35), (0.48, 1.70), (0.40, 2.05), (0.30, 2.30), (0.12, 2.42), (0, 2.44))]

def _lemon_prof(r):
    return [(a * r, b * r) for a, b in ((0, 0.0), (0.12, 0.03), (0.20, 0.12), (0.55, 0.38), (0.90, 0.80), (1.0, 1.20), (0.90, 1.60), (0.55, 2.02), (0.20, 2.28), (0.12, 2.37), (0, 2.40))]

def _fruit(name, prof, mat, at, rot=(0, 0, 0), steps=24, stem=None, top=0.0):
    """a turned fruit standing on 'at' (uu), turned by rot; stem: the stalk's material, set in the dimple at 'top' (uu up the axis)"""
    o = lathe_s(name, prof, mat, steps); o.rotation_euler = rot; o.location = u(*at); P = [o]
    if stem:
        s = lathe_s(name + "Stem", [(0, top - 6), (3.5, top - 6), (3.0, top + 22), (4.5, top + 30), (0, top + 31)], stem, 6); s.rotation_euler = rot; s.location = u(*at); P.append(s)
    return P

TIN_PHI = math.radians(72.0); TIN_LZ = 356.0 * math.sin(TIN_PHI) + 1.0     # the cookie tin's lid: its lean, its centre over the floor (its edge stands on it)
TIN_LX = 354.0 - (55.0 * math.cos(TIN_PHI) + TIN_LZ - 298.0) / math.tan(TIN_PHI) - 55.0 * math.sin(TIN_PHI)   # ... and so that its inside rests on the tin's rim

def sweep(name, pts_uu, sec_uu, mat, closed=False, caps=True, scale=None):
    """a cross-section (a closed loop of (side, up) points, uu) swept along a 3D path (uu); scale(i) -> section scale"""
    me = bpy.data.meshes.new(name); bm = bmesh.new(); P = [Vector(u(*p)) for p in pts_uu]; n = len(P); rings = []
    for i in range(n):
        t = (P[(i + 1) % n] - P[i - 1]) if closed else (P[min(i + 1, n - 1)] - P[max(i - 1, 0)])
        t.normalize(); side = t.cross(Vector((0, 0, 1)))
        if side.length < 1e-4: side = Vector((1, 0, 0))
        side.normalize(); up = side.cross(t)
        k = scale(i) if scale else 1.0
        rings.append([bm.verts.new(P[i] + side * s * K * k + up * v * K * k) for s, v in sec_uu])
    m = len(sec_uu)
    for i in range(n if closed else n - 1):
        a, b = rings[i], rings[(i + 1) % n]
        for j in range(m): bm.faces.new((a[j], a[(j + 1) % m], b[(j + 1) % m], b[j]))
    if caps and not closed:
        bm.faces.new(list(reversed(rings[0]))); bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); bm.to_mesh(me); bm.free()
    o = H.link(bpy.data.objects.new(name, me)); o.data.materials.append(mat)
    for p in o.data.polygons: p.use_smooth = True
    return o

def circ(r, n=12, flat=1.0): return [(r * math.cos(2 * math.pi * k / n), r * flat * math.sin(2 * math.pi * k / n)) for k in range(n)]

def coin_metal(name):
    m = H.metal(name, (0.86, 0.66, 0.34), 0.28); p = m.node_tree.nodes["Principled BSDF"]; tc, sp = _sep(m)
    uu = _maprange(m, sp.outputs["X"], -140 * K, 140 * K, 0.0, 1.0, False, -1000, -900); vv = _maprange(m, sp.outputs["Y"], -140 * K, 140 * K, 0.0, 1.0, False, -1000, -1000)
    t = _img(m, os.path.join(LAB, "coin_height.png"), uu, vv, -700, -900)
    bp = N(m, "ShaderNodeBump", -300, -900); bp.inputs["Strength"].default_value = 0.8; bp.inputs["Distance"].default_value = 0.0006
    L(m, t.outputs["Color"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"]); return m

def latex(name, col=(0.75, 0.04, 0.05)):
    m, p = H.mat(name); p.inputs["Base Color"].default_value = (*col, 1); p.inputs["Roughness"].default_value = 0.18
    p.inputs["Subsurface Weight"].default_value = 0.3; p.inputs["Subsurface Radius"].default_value = (0.02, 0.005, 0.005); p.inputs["Coat Weight"].default_value = 0.4
    return H.tag(m, 0.0, 0.0)

def mats11(M):
    M["fruitBowl"] = ceramic("FruitBowlGlaze", (0.10, 0.20, 0.52), 0.06, [(420, 450, (0.92, 0.90, 0.84))], 460.0, 4.0)
    M["banana"] = H.plastic("Banana", (0.88, 0.70, 0.12), 0.45, peel=0.2, scuff=0.15); H.tag(M["banana"], 0.0, 0.3)
    M["lemon"] = H.plastic("Lemon", (0.92, 0.80, 0.10), 0.35, peel=0.5, scuff=0.1); H.tag(M["lemon"], 0.0, 0.3)
    M["pear"] = H.plastic("Pear", (0.50, 0.62, 0.16), 0.4, peel=0.25, scuff=0.15); H.tag(M["pear"], 0.0, 0.3)
    M["stem"] = H.plastic("Stem", (0.25, 0.16, 0.08), 0.7, peel=0.1, scuff=0.1)
    M["chromeWire"] = H.metal("RackChrome", (0.90, 0.90, 0.91), 0.12)
    M["trayWhite"] = H.plastic("RackTray", (0.92, 0.92, 0.90), 0.3, peel=0.02, scuff=0.25)
    M["plateSmall"] = ceramic("SmallPlate", (0.95, 0.94, 0.90), 0.05, [(176, 186, (0.75, 0.55, 0.18))], 200.0, 3.0)
    M["crisps"] = H.plastic("Crisps", (0.90, 0.70, 0.30), 0.5, peel=0.8, scuff=0.1); H.tag(M["crisps"], 0.0, 0.5)
    M["snackBowl"] = ceramic("SnackBowl", (0.78, 0.12, 0.10), 0.07, [(320, 333, (0.95, 0.93, 0.88))], 340.0, 4.0)
    M["frame"] = H.plastic("GlassesFrame", (0.12, 0.06, 0.03), 0.2, peel=0.02, scuff=0.15)
    M["lens"] = glass_mat("Lens", (0.95, 0.97, 0.98), 0.01)
    M["coin"] = coin_metal("Coin")
    M["needle"] = H.metal("NeedleSteel", (0.82, 0.82, 0.84), 0.15)
    M["sock"] = stripes_cloth("SockStripes", (0.70, 0.10, 0.08), (0.92, 0.90, 0.84), 70.0)
    M["yarn"] = H.fibre("YarnBall", light=(0.35, 0.62, 0.70), dark=(0.18, 0.38, 0.45), dirt=(0.2, 0.2, 0.2), floor_h=0.0, twist=1800)
    M["trackOrange"] = H.plastic("CarTrack", (0.95, 0.45, 0.05), 0.3, peel=0.02, scuff=0.3)
    M["trackBlue"] = H.plastic("TrackStand", (0.10, 0.30, 0.70), 0.3, peel=0.02, scuff=0.3)
    M["beechTrack"] = H.wood("TrackBeech", (0.86, 0.70, 0.48), (0.66, 0.50, 0.30), 18.0, 0.3)
    M["balloon"] = latex("Balloon")
    M["gift"] = print_box("GiftWrap", {"front": "gift.png", "back": "gift.png", "side": "gift.png", "top": "gift.png"}, -452, 452, -352, 352, 0, 521, 0.35, (0.12, 0.27, 0.62))
    M["giftLid"] = print_box("GiftWrapLid", {"front": "gift.png", "back": "gift.png", "side": "gift.png", "top": "gift.png"}, -472, 472, -372, 372, 0, 149, 0.35, (0.12, 0.27, 0.62))
    M["ribbon"] = H.plastic("Ribbon", (0.72, 0.05, 0.06), 0.15, peel=0.02, scuff=0.05)
    return M

def fruit_skin(name, a, b, rough=0.3, scale=14.0):
    """fruit peel: two colours in soft patches (the blush of an apple, the green end of a lemon), fine pores"""
    m, p = H.mat(name); tc = N(m, "ShaderNodeTexCoord", -1100)
    no = N(m, "ShaderNodeTexNoise", -900); no.inputs["Scale"].default_value = scale; no.inputs["Detail"].default_value = 3; L(m, tc.outputs["Object"], no.inputs["Vector"])
    cr = N(m, "ShaderNodeValToRGB", -700); e = cr.color_ramp.elements; e[0].position = 0.40; e[0].color = (*a, 1); e[1].position = 0.64; e[1].color = (*b, 1)
    L(m, no.outputs["Fac"], cr.inputs["Fac"]); L(m, cr.outputs["Color"], p.inputs["Base Color"]); p.inputs["Roughness"].default_value = rough
    po = N(m, "ShaderNodeTexNoise", -900, -300); po.inputs["Scale"].default_value = 900; L(m, tc.outputs["Object"], po.inputs["Vector"])
    bp = N(m, "ShaderNodeBump", -300, -300); bp.inputs["Strength"].default_value = 0.12; bp.inputs["Distance"].default_value = 0.0003
    L(m, po.outputs["Fac"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return H.tag(m, 0.0, 0.3)

def mats12(M):
    """the six redone pieces (KAN-22 fix)"""
    M["sock"] = sock_knit("SockKnit", (0.70, 0.10, 0.08), (0.92, 0.90, 0.84))
    M["bagPaper"] = bag_paper("GroceryPaper", "grocery.png", -620, 620, -770, 640, 300)
    M["crust"] = crust("BaguetteCrust")
    M["apple"] = fruit_skin("AppleRed", (0.62, 0.05, 0.04), (0.78, 0.36, 0.08), 0.24)
    M["appleGreen"] = fruit_skin("AppleGreen", (0.42, 0.60, 0.10), (0.66, 0.70, 0.16), 0.26)
    M["appleGold"] = fruit_skin("AppleGold", (0.86, 0.66, 0.10), (0.80, 0.34, 0.08), 0.26)
    M["pear"] = fruit_skin("PearSkin", (0.56, 0.62, 0.16), (0.74, 0.62, 0.20), 0.4)
    M["orange"] = fruit_skin("OrangePeel", (0.90, 0.36, 0.02), (0.95, 0.48, 0.04), 0.42, 20.0)
    M["biscuitB"] = biscuit_mat("ButterBiscuit")
    M["cookieLid"] = lid_print("CookieTinLidPrint", "cookietin_top.png", (0.12, 0.24, 0.55))
    M["fruitBowl"] = ceramic("FruitBowlGlazeDeep", (0.10, 0.20, 0.52), 0.06, [(437, 452, (0.92, 0.90, 0.84))], 460.0, 4.0)
    return M

def _ball(name, r, at, mat, sx=1.0, sy=1.0, sz=1.0, n=24):
    o = lathe_s(name, sphere_prof(r, 12), mat, n); o.scale = (sx, sy, sz); o.location = u(*at); return o

def fruitbowl(M):
    """a deep glazed bowl heaped with fruit: six round the bowl (apples, a pear, an orange) resting on its wall, one under them
    in the middle, three on top, a banana across the top. (KAN-22 fix: the old bowl's size back - 900 across, 452 to the top of
    the heap; it had come out a 289 uu dish.)"""
    prof = [(0, 10), (150, 10), (156, 4), (164, 0), (178, 0), (184, 6), (186, 20), (230, 34), (300, 70), (360, 125), (405, 195), (432, 265), (446, 318), (450, 330),
            (446, 336), (438, 334), (424, 270), (396, 200), (352, 134), (294, 82), (226, 48), (170, 38), (0, 36)]
    P = [(lathe("Bowl", prof, M["fruitBowl"], 56), 0)]; F = []; R = random.Random(71)
    def put(k, kind, mat, r, ang, dist, z, lean=0.0):
        a = math.radians(ang); rot = (-lean * math.sin(a) + R.uniform(-0.07, 0.07), lean * math.cos(a) + R.uniform(-0.07, 0.07), R.uniform(0, 6))
        prof_, top = {"apple": (_apple_prof(r), 1.58 * r), "pear": (_pear_prof(r), 2.42 * r), "orange": (_orange_prof(r), None)}[kind]
        return _fruit("Fruit%d" % k, prof_, M[mat], (dist * math.cos(a), dist * math.sin(a), z), rot, 18, M["stem"] if top else None, top or 0.0)
    # (the heights: where each fruit's flank meets the bowl's inner wall at its distance from the axis - lower, it would come out through the china)
    for k, (kind, mat, r, z, lean) in enumerate((("apple", "apple", 96, 70, 0.0), ("orange", "orange", 90, 56, 0.0), ("pear", "pear", 82, 72, 0.22),
                                                 ("apple", "appleGreen", 95, 70, 0.0), ("apple", "apple", 97, 71, 0.0), ("apple", "appleGold", 94, 69, 0.0))):
        F += put(k, kind, mat, r, 60 * k, 215, z, lean)
    F += put(9, "apple", "appleGold", 90, 0, 0, 38)
    for k, (kind, mat, r, z) in enumerate((("apple", "apple", 92, 215), ("orange", "orange", 90, 196), ("apple", "appleGreen", 92, 215))):
        F += put(6 + k, kind, mat, r, 30 + 120 * k, 125, z)
    ridged = [(31 * (1 + 0.07 * math.cos(5 * a)) * math.cos(a), 31 * (1 + 0.07 * math.cos(5 * a)) * math.sin(a)) for a in [2 * math.pi * k / 15 for k in range(15)]]
    path = _curve([(-238, 82, 362), (-150, 66, 404), (-50, 60, 421), (50, 60, 421), (150, 66, 404), (238, 82, 362)], 3); n = len(path)
    F.append(sweep("Banana", path, ridged, M["banana"], scale=lambda i: min(1.0, 0.42 + 0.3 * i, 0.42 + 0.3 * (n - 1 - i))))
    F.append(sweep("BananaStalk", [(238, 82, 362), (262, 88, 346), (280, 92, 336)], circ(9, 6), M["stem"], scale=lambda i: (1.3, 0.95, 1.0)[i]))
    F.append(sweep("BananaTip", [(-238, 82, 362), (-248, 84, 355)], circ(9, 6), M["stem"], scale=lambda i: (1.35, 0.5)[i]))
    return P + [(o, 1) for o in F]

def knifeblock(M):
    """the block and its five knives, their handles leaning back with it; two brass rivets through each handle, a head on either
    side. (KAN-22 fix: the rivets were set off along Y with the wrong sign - a handle turned by -ang about X carries a point
    at height zz to +zz*sin(ang), not to -zz*sin(ang) - and hung in the air beside the handles.)"""
    blk = sweep("Block", [(-160, 0, 0), (160, 0, 0)], [(300, 0), (-300, 0), (-300, 760), (300, 480)], M["walnutHandle"])
    P = [(blk, 0)]
    ang = math.atan2(280, 600)
    for k, (x, y) in enumerate(((-95, 120), (-30, 150), (35, 90), (100, 140), (-60, -20))):
        z = 480 + (y + 300) * 280 / 600
        h = _hinge(rbox("Handle%d" % k, -16, 16, -26, 26, 0, 300 if k < 4 else 220, M["padDark"], 8, 0, 3)); h.rotation_euler = (-ang, 0, 0); h.location = u(x, y, z - 10); P.append((h, 1))
        for zz in (90, 190):
            for s in (-1, 1):
                P.append((_tilt(outward(lathe_s("Rivet", [(0, 0), (6, 0), (5, 2), (0, 3)], M["brass"], 10)), (0, s * math.pi / 2, 0),   # (outward: this profile comes out of the screw inside out)
 (x + s * 16, y + zz * math.sin(ang), z - 10 + zz * math.cos(ang))), 1))
    return P

def _wire(name, pts, M, r=5.0): return sweep(name, pts, circ(r, 6), M["chromeWire"])

def dishrack(M):
    P = [(rbox("Tray", -600, 600, -450, 450, 0, 30, M["trayWhite"], 8, 0, 2), 0), (rbox("TrayIn", -580, 580, -430, 430, 26, 32, M["trayWhite"], 3, 0, 1), 0)]
    rect = lambda z, w, d: [(-w, -d, z), (w, -d, z), (w, d, z), (-w, d, z)]
    P.append((sweep("Rim", rect(60, 570, 420), circ(6, 6), M["chromeWire"], closed=True), 0))
    P.append((sweep("RimTop", rect(300, 570, 420), circ(5, 6), M["chromeWire"], closed=True), 0))
    for k in range(-5, 6):
        x = k * 100
        P.append((_wire("Prong%d" % k, [(x, -420, 60), (x, -380, 200), (x, -330, 230)], M), 0))
        P.append((_wire("Rib%d" % k, [(x, -420, 60), (x, 420, 60)], M, 4.0), 0))
    for x in (-570, 570):
        for y in (-420, 420): P.append((_wire("Post", [(x, y, 30), (x, y, 300)], M), 0))
    for y in (-420, 420): P.append((_wire("Stay", [(-570, y, 180), (570, y, 180)], M, 4.0), 0))
    return P

def biscuitplate(M):
    prof = [(0, 4), (100, 4), (106, 0), (114, 0), (118, 4), (150, 14), (186, 26), (200, 30), (198, 33), (186, 31), (150, 20), (116, 12), (0, 11)]
    P = [(lathe("Plate", prof, M["plateSmall"], 64), 0)]
    for k, (x, y, z, rx) in enumerate(((-40, -20, 12, 0.0), (45, 20, 12, 0.0), (5, 0, 30, 0.12))):
        c = lathe_s("Biscuit%d" % k, [(0, 0), (62, 0), (68, 4), (68, 14), (62, 18), (0, 18)], M["biscuit"], 32); c.location = u(x, y, z); c.rotation_euler = (rx, 0.05 * k, 0); P.append((c, 1))
    return P

def snackbowl(M):
    prof = [(0, 5), (140, 5), (148, 1), (156, 0), (164, 3), (168, 12), (260, 70), (318, 150), (332, 186), (328, 190), (320, 188), (306, 152), (250, 76), (164, 30), (0, 26)]
    P = [(lathe("Bowl", prof, M["snackBowl"], 72), 0)]
    R = random.Random(61)
    for k in range(16):
        a, r = R.uniform(0, 6.28), R.uniform(0, 230); c = lathe_s("Crisp%d" % k, [(0, 0), (40, 6), (46, 10), (40, 12), (0, 6)], M["crisps"], 16)
        c.scale = (1.0, 0.75, 1.0); c.rotation_euler = (R.uniform(-0.7, 0.7), R.uniform(-0.7, 0.7), R.uniform(0, 3)); c.location = u(r * math.cos(a), r * math.sin(a), 40 + r * 0.35 + R.uniform(0, 50)); P.append((c, 1))
    return P

def glasses(M):
    """granny's glasses standing on a shelf: on the rims and on the tips of the temples.
    (KAN-22 fix: the rims went 18 uu under the surface the piece stands on; the frame's bottom is now at 0.)"""
    P = []; zc = 50.0; n = 40
    for s in (-1, 1):
        cx = s * 66; ell = [(cx + 56 * math.cos(2 * math.pi * k / n), zc + 44 * math.sin(2 * math.pi * k / n)) for k in range(n)]
        P.append((glass(slab("Lens", ell, [], "y", -3.5, 3, M["lens"], 0.5)), 0))
        P.append((ring_tube("Rim", [(cx + 58 * math.cos(2 * math.pi * k / n), -2, zc + 46 * math.sin(2 * math.pi * k / n)) for k in range(n)], 3.5, M["frame"]), 0))
        P.append((sweep("Temple", _curve([(s * 122, -2, zc + 12), (s * 128, 7, zc + 14), (s * 129, 34, zc + 13), (s * 129, 56, zc), (s * 128, 71, 24), (s * 127, 78, 4)], 3), circ(3, 8), M["frame"]), 0))
    P.append((sweep("Bridge", _curve([(-11, -2, zc + 15), (-4, -4, zc + 21), (4, -4, zc + 21), (11, -2, zc + 15)], 3), circ(3, 8), M["frame"]), 0))
    return P

def coin(M):
    return [(lathe_s("Coin", [(0, 17), (128, 17), (134, 16), (140, 12), (140, 5), (134, 1), (128, 0), (0, 0)], M["coin"], 64), 0)]

def knitneedle(M):
    n = lathe_s("Needle", [(0, -316), (3, -306), (8, -270), (9, -240), (9, 300), (0, 300)], M["needle"], 16)
    b = lathe_s("Bead", [(0, 300), (11, 302), (13, 314), (11, 326), (0, 330)], M["padR"], 16)
    for o in (n, b): o.rotation_euler = (0, math.pi / 2, 0)
    return [(n, 0), (b, 0)]

def sock(M):
    """a dropped tube sock, L-shaped on the floor: a ribbed cuff (open: the tunnel), the leg, the heel pocket, the foot, a round
    toe. Soft: the empty tube lies flat on the floor - flat on top, round at the sides - held open by the cuff's elastic, with
    folds across the leg and wrinkles on the inside of the turn. (KAN-22 fix: it was a faceted pipe of nine sections with no
    heel and no toe.)"""
    R, L1, L2, L3, LT = SOCK_R, SOCK_L1, SOCK_L2, SOCK_L3, SOCK_LEN; cx, cy = -527.0 + L1, -250.0 + R
    def centre(s):
        if s <= L1: return Vector((-527.0 + s, -250.0, 0)), Vector((1, 0, 0))
        if s <= L1 + L2:
            a = (s - L1) / R; return Vector((cx + R * math.sin(a), cy - R * math.cos(a), 0)), Vector((math.cos(a), math.sin(a), 0))
        return Vector((cx + R, cy + s - L1 - L2, 0)), Vector((0, 1, 0))
    size = [(0, 100, 158), (120, 100, 150), (330, 117, 92), (L1 - 80, 113, 98), (SOCK_HEEL, 108, 116), (L1 + L2 + 90, 110, 88), (LT, 104, 84)]   # half width, height
    tl = 85.0; rows = [(0.0, 1.0), (7.0, 1.0), (16.0, 1.0)]; s = 40.0
    while s < LT - tl - 1: rows.append((s, 1.0)); s += 30.0
    rows += [(LT - tl + tl * math.sin(math.radians(d)), math.cos(math.radians(d))) for d in (0, 16, 32, 46, 58, 68, 76, 82)]   # the toe: a round cap
    nv = 24; rings = []; at_t = []; at_v = []
    for s, f in rows:
        c, t = centre(s); side = Vector((t.y, -t.x, 0)); w, h = _keys(size, s)
        c = c + side * 9.0 * _nz(s / 260.0, 3.1)
        welt = 1.0 + 0.03 * max(0.0, min(1.0, (150.0 - s) / 30.0)) + (0.03 if s < 10 else 0.0)
        bend = math.exp(-((s - SOCK_HEEL) / 125.0) ** 2); ring_ = []
        fold = sum(d * math.exp(-((s - s0) / 32.0) ** 2) for s0, d in ((285.0, 0.20), (505.0, 0.26), (L1 + L2 + 170.0, 0.16)))   # where the leg has given way
        for j in range(nv + 1):
            v = j / nv; a = -math.pi / 2 + 2 * math.pi * v; cs, sn = math.cos(a), math.sin(a)
            sd = w * math.copysign(abs(cs) ** 0.62, cs); up = h * (0.5 + 0.5 * math.copysign(abs(sn) ** 0.62, sn))
            up *= (1 + 0.15 * _nz(s / 110.0, cs * 0.9, sn * 0.9) * max(sn + 0.3, 0.0)) * (1 - fold * max(sn, 0.0))
            wr = 0.11 * math.sin((s - L1) / 16.0) * bend * (0.35 + 0.65 * max(-cs, 0.0))       # wrinkles on the inside of the turn
            sd *= welt * (1 + wr); up *= 1 + 0.7 * wr
            sd += 44.0 * math.exp(-((s - SOCK_HEEL) / 75.0) ** 2) * max(cs, 0.0) ** 1.5         # the heel pocket
            p = c + side * (sd * f) + Vector((0, 0, up * f ** 0.55 + 0.5)); ring_.append((p.x, p.y, p.z))
        rings.append(ring_); at_t.append([s] * (nv + 1)); at_v.append([j / nv for j in range(nv + 1)])
    o = skin("Sock", rings, M["sock"], close_u=False, cap_end=True, attrs={"sk_t": at_t, "sk_v": at_v})
    so = o.modifiers.new("solid", "SOLIDIFY"); so.thickness = 6 * K; so.offset = -1.0     # the leg is open: a tunnel
    return [(o, 0)]

def yarnball(M):
    P = [(lathe("Ball", sphere_prof(220, 20), M["yarn"], 48), 0)]
    R = random.Random(67)
    for k in range(7):   # a few strands wound over the top
        ax = Vector((R.uniform(-1, 1), R.uniform(-1, 1), R.uniform(-1, 1))).normalized(); b1 = ax.orthogonal().normalized(); b2 = ax.cross(b1)
        pts = [tuple((b1 * math.cos(2 * math.pi * j / 24) + b2 * math.sin(2 * math.pi * j / 24)) * 224 + Vector((0, 0, 220))) for j in range(24)]
        P.append((sweep("Wrap%d" % k, pts, circ(5, 6), M["yarn"], closed=True), 0))
    P.append((sweep("Strand", [(150, 140, 60), (250, 220, 12), (380, 260, 8), (515, 300, 8)], circ(6, 6), M["yarn"]), 0))
    return P

def cartrack(M):
    pts = [(-1700, 0, 1440), (-1300, 0, 1440), (-900, 0, 1300), (-500, 0, 980), (-100, 0, 640), (300, 0, 330), (700, 0, 110), (1100, 0, 18), (1700, 0, 12)]
    sec = [(-118, 0), (118, 0), (118, 34), (106, 34), (106, 8), (-106, 8), (-106, 34), (-118, 34)]
    P = [(sweep("Track", pts, sec, M["trackOrange"]), 0)]
    for x, z in ((-500, 980), (-100, 640), (300, 330)):
        P.append((rbox("Leg", x - 20, x + 20, -60, 60, 0, z - 4, M["trackBlue"], 6, 0, 2), 0)); P.append((rbox("Foot", x - 70, x + 70, -100, 100, 0, 14, M["trackBlue"], 5, 0, 2), 0))
    return P

def traintrack(M):
    pts = []; a, b, r = 1850, 950, 600
    for cx, cy, a0 in ((a - r, b - r, 0), (-(a - r), b - r, 90), (-(a - r), -(b - r), 180), (a - r, -(b - r), 270)):
        pts += [(cx + r * math.cos(math.radians(a0 + 90 * k / 8)), cy + r * math.sin(math.radians(a0 + 90 * k / 8)), 0) for k in range(9)]
    sec = [(-38, 0), (38, 0), (38, 22), (26, 22), (26, 12), (14, 12), (14, 22), (-14, 22), (-14, 12), (-26, 12), (-26, 22), (-38, 22)]
    return [(sweep("Track", pts, sec, M["beechTrack"], closed=True), 0)]

def balloon(M):
    prof = [(0, 0), (14, 2), (18, 12), (10, 24), (8, 36), (16, 44), (40, 60), (100, 110), (160, 180), (205, 270), (228, 360), (232, 420), (222, 500), (190, 570), (130, 620), (60, 638), (0, 640)]
    return [(lathe("Balloon", prof, M["balloon"], 48), 0)]

def balloonweight(M):
    w = lathe_s("Weight", [(0, 0), (70, 0), (75, 8), (60, 40), (30, 70), (20, 80), (24, 88), (0, 92)], M["padR"], 32); w.scale = (1.0, 0.73, 1.0)
    return [(w, 0)]

def giftbox(M):
    P = [(rbox("Box", -452, 452, -352, 352, 0, 521, M["gift"], 4, 0, 2), 0)]
    P += [(rbox("RibbonX", -455, 455, -40, 40, -1, 523, M["ribbon"], 2, 0, 1), 0), (rbox("RibbonY", -40, 40, -355, 355, -1, 523, M["ribbon"], 2, 0, 1), 0)]
    return P

def giftlid(M):
    P = [(rbox("Top", -472, 472, -372, 372, 64, 74, M["giftLid"], 3, 0, 1), 0)]
    for b in ((-472, -464, -372, 372), (464, 472, -372, 372), (-472, 472, -372, -364), (-472, 472, 364, 372)):
        P.append((rbox("Skirt", b[0], b[1], b[2], b[3], 0, 74, M["giftLid"], 2, 0, 1), 0))
    P += [(rbox("RibbonX", -475, 475, -40, 40, 0, 76, M["ribbon"], 2, 0, 1), 0), (rbox("RibbonY", -40, 40, -375, 375, 0, 76, M["ribbon"], 2, 0, 1), 0)]
    for s in (-1, 1):
        lp = sweep("Loop", [(0, 0, 80), (s * 60, -20, 120), (s * 130, 0, 140), (s * 150, 20, 110), (s * 90, 10, 84), (0, 0, 80)], [(-30, -3), (30, -3), (30, 3), (-30, 3)], M["ribbon"])
        P.append((lp, 0))
    P.append((rbox("Knot", -26, 26, -30, 30, 74, 110, M["ribbon"], 10, 0, 3), 0))
    return P

SPECS.update({   # batch 11
    "SM_K_FruitBowl": dict(build=fruitbowl, hulls=lambda n: hulls_dish(n, (176, 184, 30), ((180, 10), (186, 32), (450, 200), (436, 206))) + [H.hull("UCX_%s_09" % n, boxp(-240, 240, -180, 180, 30, 260))],
                           lods=(0.45, 0.2), cls="ceramic", breaks=lambda o, n: shatter(o, n, lambda p, g: p == 0, [], [lambda p, g: p == 1], n=8, seed=71, jag=0.0008, chips=6, chip_mat_glass=False)),
    "SM_K_KnifeBlock": dict(build=knifeblock, hulls=lambda n: [H.hull("UCX_%s_00" % n, [(x * K, y * K, z * K) for x in (-160, 160) for y, z in ((-300, 0), (300, 0), (300, 760), (-300, 480))])],
                            lods=(0.45, 0.2), cls="wood", breaks=lambda o, n: cut_groups(o, n, [], [lambda p, g: p == 0, lambda p, g: p == 1], [])),
    "SM_K_DishRack": dict(build=dishrack, hulls=_box_hulls((-600, 600, -450, 450, 0, 32), (-575, 575, -425, -380, 32, 300), (-575, 575, 380, 425, 32, 300)), lods=(0.5, 0.25), cls="metal", breaks=None),
    "SM_L_BiscuitPlate": dict(build=biscuitplate, hulls=lambda n: hulls_round(n, 200, 33), lods=(0.45, 0.2), cls="ceramic",
                              breaks=lambda o, n: shatter(o, n, lambda p, g: p == 0, [], [lambda p, g: p == 1], n=6, seed=73, jag=0.0006, chips=5, chip_mat_glass=False)),
    "SM_L_SnackBowl": dict(build=snackbowl, hulls=lambda n: hulls_dish(n, (164, 168, 26), ((166, 8), (168, 28), (332, 186), (318, 192))), lods=(0.45, 0.2), cls="ceramic",
                           breaks=lambda o, n: shatter(o, n, lambda p, g: p == 0, [], [lambda p, g: p == 1], n=8, seed=79, jag=0.0008, chips=6, chip_mat_glass=False)),
    "SM_V3_Glasses": dict(build=glasses, hulls=_box_hulls((-132, 132, -6, 2, -18, 78), (-132, -122, -2, 80, 20, 50), (122, 132, -2, 80, 20, 50)), lods=(0.5, 0.25), cls="glass", glass=0.08, glassmat="lens",
                          breaks=lambda o, n: cut_groups(o, n, [], [lambda p, g: g == 0, lambda p, g: g == 1], [])),
    "SM_L_Coin": dict(build=coin, hulls=lambda n: hulls_round(n, 140, 17), lods=(0.5, 0.25), cls="metal", breaks=None),
    "SM_V3_KnitNeedle": dict(build=knitneedle, hulls=lambda n: [H.hull("UCX_%s_00" % n, [(x * K, y * K, z * K) for x in (-316, 330) for y in (-12, 12) for z in (-12, 12)])], lods=(0.5, 0.25), cls="metal", breaks=None),
    "SM_L_Sock": dict(build=sock, hulls=lambda n: [H.hull("UCX_%s_00" % n, boxp(-527, 140, -340, -160, 0, 180)), H.hull("UCX_%s_01" % n, boxp(140, 430, -340, 330, 0, 180))], lods=(0.5, 0.25), cls="cloth", breaks=None),
    "SM_L_YarnBall": dict(build=yarnball, hulls=lambda n: hulls_round(n, 220, 440), lods=(0.45, 0.2), cls="cloth", breaks=None),
    "SM_K_CarTrack": dict(build=cartrack, hulls=lambda n: [H.hull("UCX_%s_%02d" % (n, i), [(x * K, y * K, z * K) for x, z in seg for y in (-118, 118)]) for i, seg in
                                                           enumerate((((-1700, 1440), (-1300, 1440), (-1300, 1474), (-1700, 1474)), ((-1300, 1440), (-500, 980), (-500, 1014), (-1300, 1474)),
                                                                      ((-500, 980), (300, 330), (300, 364), (-500, 1014)), ((300, 330), (1100, 18), (1100, 52), (300, 364)), ((1100, 18), (1700, 12), (1700, 46), (1100, 52))))],
                          lods=(0.5, 0.25), cls="plastic", breaks=None),
    "SM_L_TrainTrack": dict(build=traintrack, hulls=_box_hulls((-1250, 1250, -990, -910, 0, 22), (-1250, 1250, 910, 990, 0, 22), (-1890, -1810, -350, 350, 0, 22), (1810, 1890, -350, 350, 0, 22)), lods=(0.5, 0.25), cls="wood", breaks=None),
    "SM_V3_Balloon": dict(build=balloon, hulls=lambda n: [H.hull("UCX_%s_00" % n, ring(18, 0, 8) + ring(160, 180, 12) + ring(232, 420, 16) + ring(130, 620, 12) + ring(10, 640, 6))], lods=(0.45, 0.2), cls="plastic", breaks=None),
    "SM_V3_BalloonWeight": dict(build=balloonweight, hulls=lambda n: [H.hull("UCX_%s_00" % n, [(p[0], p[1] * 0.73, p[2]) for p in ring(75, 0, 12) + ring(30, 70, 8) + ring(10, 92, 6)])], lods=(0.5, 0.25), cls="plastic", breaks=None),
    "SM_V3_GiftBox": dict(build=giftbox, hulls=_box_hulls((-455, 455, -355, 355, 0, 523)), lods=(0.5, 0.25), cls="cardboard",
                          breaks=lambda o, n: cut_groups(o, n, [lambda p, g: True], [], [((0, 0, 260), (0.1, 0.05, 1.0), 0.008, 157, 0)])),
    "SM_V3_GiftBox_Lid": dict(build=giftlid, hulls=_box_hulls((-475, 475, -375, 375, 0, 76), (-150, 150, -30, 30, 76, 145)), lods=(0.5, 0.25), cls="cardboard", breaks=None),
})

# the six redone pieces (KAN-22 fix): collision by their new shapes
def _hulls_bag(n):
    pts = lambda rows: [(x * K, y * K, z * K) for x, y, z in rows]
    front = [(s * x, -700, z) for s, x, z in ((1, 595, 570), (1, 595, 592), (-1, 505, 570), (-1, 505, 592))] + [(45, -700, 440), (45, -700, 462)]
    mid = [(s * 566, -50, z) for s in (-1, 1) for z in (606, 626)] + [(16, -50, 562), (16, -50, 584)]
    back = [(s * 550, 600, z) for s in (-1, 1) for z in (632, 646)]
    hs = [boxp(-550, 550, -700, 600, 0, 6), pts(front + mid), pts(mid + back), boxp(-550, 550, 594, 600, 0, 646)]
    hs.append(pts([(540, -700, 0), (566, -700, 0), (425, -700, 290), (455, -700, 290), (600, -700, 585), (578, -700, 585), (540, 600, 0), (552, 600, 0), (540, 600, 640), (552, 600, 640)]))
    hs.append(pts([(-540, -700, 0), (-566, -700, 0), (-425, -700, 290), (-455, -700, 290), (-505, -700, 585), (-527, -700, 585), (-540, 600, 0), (-552, 600, 0), (-540, 600, 640), (-552, 600, 640)]))
    return [H.hull("UCX_%s_%02d" % (n, i), p) for i, p in enumerate(hs)]

def _hulls_fruitbowl(n):
    hs = hulls_dish(n, (178, 186, 36), ((182, 10), (190, 38), (360, 125), (350, 136)))
    for k in range(8):   # the upper half of the wall (the bowl is deep: one straight segment would cut through it)
        pts = []
        for a in (2 * math.pi * k / 8, 2 * math.pi * (k + 1) / 8):
            for r, z in ((360, 125), (350, 136), (450, 330), (436, 338)): pts.append((r * K * math.cos(a), r * K * math.sin(a), z * K))
        hs.append(H.hull("UCX_%s_%02d" % (n, k + 9), pts))
    return hs + [H.hull("UCX_%s_17" % n, ring(215, 50, 12) + ring(335, 150, 12) + ring(345, 300, 12) + ring(250, 420, 12) + ring(110, 450, 8))]   # the heap of fruit

SPECS["SM_K_CookieTin"]["hulls"] = lambda n: hulls_round(n, 352, 297) + [H.hull("UCX_%s_01" % n, _xf(ring(356, 0, 12) + ring(356, 60, 12), TIN_PHI, (TIN_LX, 0, TIN_LZ)))]
SPECS["SM_K_GroceryBag"]["hulls"] = _hulls_bag
SPECS["SM_K_FruitBowl"]["hulls"] = _hulls_fruitbowl
SPECS["SM_V3_Glasses"]["hulls"] = _box_hulls((-132, 132, -6, 2, 0, 100), (-133, -122, -2, 80, 0, 68), (122, 133, -2, 80, 0, 68))
SPECS["SM_L_Sock"]["hulls"] = lambda n: [H.hull("UCX_%s_00" % n, boxp(-527, 215, -358, -142, 0, 172)), H.hull("UCX_%s_01" % n, boxp(215, 440, -345, -100, 0, 140)),
                                         H.hull("UCX_%s_02" % n, boxp(232, 438, -100, 244, 0, 118))]

def breaks_bag(o, n):
    """torn, not cut: the bag's paper parted along a ragged line down its length - its faces by the side of the line they lie
    on (the boolean cutter made nothing of a thin creased shell with the handles through it); the handles stay on the first
    half, the groceries are a piece of their own"""
    tear = lambda c: (c.x / K + 55.0 * _nz(c.y / K / 170.0, c.z / K / 170.0, 9.0) + 35.0 * math.sin(c.y / K / 50.0)) > 30.0
    out = []
    for i in (0, 1):
        x = _copy(o, "%s_Brk%d" % (n, i)); bm = bmesh.new(); bm.from_mesh(x.data); lp = bm.faces.layers.int.get("part")
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if not ((f[lp] == 0 and tear(f.calc_center_median()) == bool(i)) or (f[lp] == 2 and i == 0))], context="FACES")
        bm.to_mesh(x.data); bm.free()
        a = x.data.attributes.get("surf") or x.data.attributes.new("surf", "INT", "FACE")
        for k in range(len(x.data.polygons)): a.data[k].value = 1
        out.append(x)
    return out + [keep_faces(o, lambda p, g: p == 1, "%s_Brk2" % n)]

SPECS["SM_K_GroceryBag"]["breaks"] = breaks_bag

def shape_preview(name, sp, M):
    """argv 'shape': the piece joined with its own procedural materials (no UVs, no bake, no export) and three quick frames -
    the shape checked in a minute before the full run"""
    bpy.context.view_layer.update(); objs = []
    for o, part in sp["build"](M):
        m = H.bake_mesh(o) if (o.type != "MESH" or o.modifiers) else o
        objs.append(m)
    o = H.join(objs, name); mn, mx = H.bounds([o]); c = (mn + mx) * 0.5
    print("SHAPE %s: uu min (%.0f %.0f %.0f) max (%.0f %.0f %.0f), %d triangles" % (name, *(mn / K), *(mx / K), H.tris(o)))
    H.stage(wall_y=mx.y + 0.03 + 0.15 * (mx.x - mn.x)); shots = []
    for tag, d in (("a", Vector((math.sin(math.radians(35)), -math.cos(math.radians(35)), 0.55))), ("b", Vector((-0.75, -0.55, 0.42))), ("c", Vector((0.05, -0.35, 1.0)))):
        d = d.normalized(); loc, look = fit_camera([o], lambda dd: c + d * dd, c, 50, 0.86); H.camera(loc, look, 50, 11)
        p = os.path.join(REV, "_shape_%s_%s.jpg" % (name, tag)); H.render(p, 1120, 630, 40); shots.append(p)
    zc = 15 * K; look = Vector((c.x, c.y, max(zc, mn.z + 0.45 * (mx.z - mn.z)))); dh = Vector((-0.25, -1.0, 0.0)).normalized()
    loc, look = fit_camera([o], lambda dd: (look.x + dh.x * dd, look.y + dh.y * dd, zc), look, 24, 0.92); H.camera(loc, look, 24, 5.6)
    p = os.path.join(REV, "_shape_%s_ant.jpg" % name); H.render(p, 1120, 630, 40); shots.append(p)
    H.sheet(shots, os.path.join(REV, "shape_%s.jpg" % name), 2)

# ------------------------------------------------------------------------------------------------ one asset (hero_lib.produce + the glass slot)
def glass_faces(o):
    a = o.data.attributes.get("glass")
    return [a.data[i].value == 1 for i in range(len(o.data.polygons))] if a else [False] * len(o.data.polygons)

def unwrap_capped(o, angle, margin, cap):
    """smart project, then any island much longer than the rest (a long body's whole side band, smooth all round, stays one island)
    is shrunk to cap x the square root of the islands' total area before the pack: it would set the pack's scale and leave the
    top faces (the remote's printed keys) a tenth of the sheet"""
    from bpy_extras import bmesh_utils
    for x in H.S().objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(angle), island_margin=margin, area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
    bm = bmesh.from_edit_mesh(o.data); uv = bm.loops.layers.uv.active
    isl = bmesh_utils.bmesh_linked_uv_islands(bm, uv)
    boxes = []
    for fs in isl:
        us = [l[uv].uv for f in fs for l in f.loops]
        mn = Vector((min(v.x for v in us), min(v.y for v in us))); mx = Vector((max(v.x for v in us), max(v.y for v in us)))
        boxes.append((fs, mn, mx))
    S = math.sqrt(sum((mx.x - mn.x) * (mx.y - mn.y) for _, mn, mx in boxes)); n = 0
    for fs, mn, mx in boxes:
        long_ = max(mx.x - mn.x, mx.y - mn.y)
        if long_ > cap * S:
            k = cap * S / long_; c = (mn + mx) * 0.5; n += 1
            for f in fs:
                for l in f.loops: l[uv].uv = c + (l[uv].uv - c) * k
    bmesh.update_edit_mesh(o.data)
    try:
        bpy.ops.uv.pack_islands(rotate=True, margin=margin, shape_method="CONCAVE")
    except TypeError:
        bpy.ops.uv.pack_islands(rotate=True, margin=margin)
    bpy.ops.object.mode_set(mode="OBJECT")
    o.data.uv_layers[0].name = "UV0"
    print("HERO uv %s: %d islands, %d long ones shrunk" % (o.name, len(boxes), n))

def produce(name, sp, M):
    t0 = time.time()
    out = os.path.join(OUT, name); os.makedirs(out, exist_ok=True)
    objs = []
    for o, part in sp["build"](M):
        g = bool(o.get("glass"))
        m = H.bake_mesh(o) if (o.type != "MESH" or o.modifiers) else o
        H.set_part(m, part)
        a = m.data.attributes.get("glass") or m.data.attributes.new("glass", "INT", "FACE")
        for i in range(len(m.data.polygons)): a.data[i].value = 1 if g else 0
        ca = m.data.color_attributes.get("Col") or m.data.color_attributes.new("Col", "BYTE_COLOR", "POINT")
        for i in range(len(m.data.vertices)): ca.data[i].color = (0.0, 0.0, 0.0, 1.0)
        objs.append(m)
    lod0 = H.join(objs, name)
    if sp.get("uv_cap"): unwrap_capped(lod0, 66.0, 0.003, sp["uv_cap"])
    else: H.unwrap(lod0, 66.0, 0.003)
    gf = glass_faces(lod0)
    # bake: the glass out of the way (it would occlude what is behind it in the AO)
    gv = sorted({v for p, g in zip(lod0.data.polygons, gf) if g for v in p.vertices})
    for v in gv: lod0.data.vertices[v].co.z += 50.0
    tdir = os.path.join(out, "tex")
    if NOBAKE and os.path.exists(os.path.join(tdir, "T_%s_C.png" % name)):
        paths = {k: os.path.join(tdir, "T_%s_%s.png" % (name, k)) for k in "CNR"}
    else:
        paths = H.bake_set(lod0, name, tdir, 2048, 48)
    for v in gv: lod0.data.vertices[v].co.z -= 50.0
    H.game_material(lod0, name)
    if any(gf):
        gm = bpy.data.materials.get("Hero_Glass") or bpy.data.materials.new("Hero_Glass")
        lod0.data.materials.append(gm)
        for p, g in zip(lod0.data.polygons, gf):
            if g: p.material_index = 1
    man = {"name": name, "file": name + ".fbx", "set": "hero_props", "textures": {k: os.path.relpath(v, OUT).replace("\\", "/") for k, v in paths.items()},
           "collision_lod": 0, "movers": [], "breaks": [], "break_class": sp["cls"], "note": "KAN-22 small props: hero rebuild of the old piece (same frame and box)"}
    if any(gf): man["glass"] = {"opacity": sp.get("glass", 0.14)}
    hs = sp["hulls"](name)
    l1 = H.lod(lod0, sp["lods"][0], name + "_LOD1"); l2 = H.lod(lod0, sp["lods"][1], name + "_LOD2")
    H.export_fbx([lod0] + hs, os.path.join(out, name + ".fbx"))
    H.export_fbx([l1], os.path.join(out, name + "_LOD1.fbx")); H.export_fbx([l2], os.path.join(out, name + "_LOD2.fbx"))
    man.update(H.info(lod0)); man["lod_triangles"] = [H.tris(lod0), H.tris(l1), H.tris(l2)]; man["ucx"] = len(hs)
    pieces = sp["breaks"](lod0, name) if sp.get("breaks") else []
    for pc in pieces:
        break_slots(pc, name)
        H.export_fbx([pc], os.path.join(out, pc.name + ".fbx"))
        man["breaks"].append({"name": pc.name, "file": pc.name + ".fbx", **H.info(pc)})
    for o in hs + [l1, l2]: bpy.data.objects.remove(o)
    print("HERO %s: LOD %s, %d hulls, %d break pieces, glass faces %d, %.0f s" % (name, man["lod_triangles"], man["ucx"], len(pieces), sum(gf), time.time() - t0))
    return man, lod0, pieces, paths

def break_slots(o, name):
    """slot 0 Hero_<Name> (surface), slot 1 Hero_Glass (glass faces, if any), last Break (fresh fracture: the cutter's faces)"""
    gf = glass_faces(o)
    names = [m.name if m else "" for m in o.data.materials]
    brk_idx = [i for i, n in enumerate(names) if n.startswith("Break")]
    flags = [p.material_index in brk_idx or H.is_fracture(o, p) for p in o.data.polygons]
    ms = [bpy.data.materials["Hero_" + name]] + ([bpy.data.materials["Hero_Glass"]] if any(gf) else []) + [bpy.data.materials.get("Break")]
    o.data.materials.clear()
    for m in ms: o.data.materials.append(m)
    for p, cut, g in zip(o.data.polygons, flags, gf):
        p.material_index = len(ms) - 1 if cut else (1 if g and len(ms) == 3 else 0)
    for uv in [x for x in o.data.uv_layers if x.name != "UV0"]: o.data.uv_layers.remove(uv)
    print("HERO break %s: %d fracture faces, %d glass of %d" % (o.name, sum(flags), sum(1 for c, g in zip(flags, gf) if g and not c), len(flags)))

# ------------------------------------------------------------------------------------------------ review (overall, the ant's view, wet, charred, broken)
def fit_camera(objs, pos_fn, look, lens, fill=0.85, aspect=16 / 9):
    """the camera distance at which the objects' box fills 'fill' of the frame (the larger of width / height): pos_fn(dist) -> location"""
    mn, mx = H.bounds(objs)
    corners = [Vector((x, y, z)) for x in (mn.x, mx.x) for y in (mn.y, mx.y) for z in (mn.z, mx.z)]
    th = 18.0 / lens; tv = th / aspect; look = Vector(look)
    def worst(dist):
        cam = Vector(pos_fn(dist)); fwd = (look - cam).normalized()
        right = fwd.cross(Vector((0, 0, 1))); right = right.normalized() if right.length > 1e-6 else Vector((1, 0, 0)); up = right.cross(fwd)
        w = 0.0
        for p in corners:
            v = p - cam; z = v.dot(fwd)
            if z <= 1e-4: return 99.0
            w = max(w, abs(v.dot(right)) / z / th, abs(v.dot(up)) / z / tv)
        return w
    lo, hi = 0.0005, 40.0
    for _ in range(60):
        mid = (lo + hi) / 2
        if worst(mid) > fill: lo = mid
        else: hi = mid
    return Vector(pos_fn(hi)), look

def soot_glass():
    m = bpy.data.materials.get("GlassSoot")
    if m: return m
    m, p = H.mat("GlassSoot")
    tc = N(m, "ShaderNodeTexCoord", -900); no = N(m, "ShaderNodeTexNoise", -700); no.inputs["Scale"].default_value = 60; no.inputs["Detail"].default_value = 8
    L(m, tc.outputs["Object"], no.inputs["Vector"])
    cr = N(m, "ShaderNodeValToRGB", -480); e = cr.color_ramp.elements; e[0].position = 0.35; e[0].color = (0.03, 0.025, 0.02, 1); e[1].position = 0.7; e[1].color = (0.35, 0.33, 0.30, 1)
    L(m, no.outputs["Fac"], cr.inputs["Fac"]); L(m, cr.outputs["Color"], p.inputs["Base Color"])
    tr = N(m, "ShaderNodeMapRange", -300, -200); tr.inputs["To Min"].default_value = 0.1; tr.inputs["To Max"].default_value = 0.8; L(m, no.outputs["Fac"], tr.inputs["Value"])
    L(m, tr.outputs["Result"], p.inputs["Transmission Weight"]); p.inputs["Roughness"].default_value = 0.5
    return m

def wet_extras(o):
    """the wet review frame: drops on the upper surfaces, a runnel down the front to the floor, a small puddle at the base"""
    R = random.Random(3)
    wm = bpy.data.materials.get("Water") or H.water()
    mn, mx = H.bounds([o]); s = max((mx - mn).length, 0.03); c = (mn + mx) * 0.5
    rr = min(0.0035, max(0.0007, 0.012 * s))
    polys = [p for p in o.data.polygons if p.normal.z > -0.2 and p.area > 0]
    bm = bmesh.new()
    if polys:
        ws = [p.area for p in polys]
        for p in R.choices(polys, weights=ws, k=70):
            r = rr * R.uniform(0.4, 1.0); nrm = (o.matrix_world.to_3x3() @ p.normal).normalized()
            cen = o.matrix_world @ p.center + nrm * r * 0.25
            sph = bmesh.ops.create_uvsphere(bm, u_segments=10, v_segments=6, radius=r)
            q = Vector((0, 0, 1)).rotation_difference(nrm).to_matrix().to_4x4()
            bmesh.ops.transform(bm, matrix=Matrix.Translation(cen) @ q @ Matrix.Diagonal((1, 1, 0.45, 1)), verts=sph["verts"])
    me = bpy.data.meshes.new("Drops"); bm.to_mesh(me); bm.free()
    drops = H.link(bpy.data.objects.new("Drops", me)); drops.data.materials.append(wm)
    for p in drops.data.polygons: p.use_smooth = True
    # the puddle: an irregular flat pool at the front of the base
    bm = bmesh.new(); pc = Vector((c.x + 0.1 * (mx.x - mn.x), mn.y - 0.05 * s, 0.0003)); ring_ = []
    for k in range(48):
        a = 2 * math.pi * k / 48; rad = (0.32 + 0.06 * math.sin(3 * a + 1) + 0.04 * math.sin(7 * a)) * s
        ring_.append(bm.verts.new((pc.x + rad * math.cos(a) * 1.3, pc.y + rad * math.sin(a) * 0.6, 0.0003)))
    bm.faces.new(ring_); bmesh.ops.solidify(bm, geom=bm.faces[:], thickness=0.0006)
    me = bpy.data.meshes.new("Puddle"); bm.to_mesh(me); bm.free()
    pud = H.link(bpy.data.objects.new("Puddle", me)); pud.data.materials.append(wm)
    # the runnel: from the front, half way up, down to the puddle
    top = Vector((c.x, mn.y - rr * 0.3, mn.z + 0.55 * (mx.z - mn.z)))
    pts = [top, Vector((c.x + 0.02 * s, mn.y - rr * 0.3, mn.z + 0.3 * (mx.z - mn.z))), Vector((c.x + 0.03 * s, mn.y - rr * 0.6, 0.002)), Vector((pc.x, pc.y, 0.0006))]
    run = H.tubes("Runnel", [pts], rr * 0.5, res_u=8, bevel_res=2); run.data.materials.append(wm); run.scale = (1, 1, 1)
    return [drops, pud, run]

def review(name, o, pieces, prev, glassm):
    """the owner's reference framing (hero_mop_bucket.jpg / _ant.jpg): the piece fills the frame at 3/4; a close frame from the ant's
    height (15 uu over the floor); the states clean | wet | charred | broken side by side"""
    for x in list(H.S().objects):
        if x.type in ("LIGHT", "CAMERA") or (x.type == "MESH" and x not in [o] + pieces and not x.name.startswith(name)):
            bpy.data.objects.remove(x)
    for x in pieces: x.hide_render = True
    mn, mx = H.bounds([o]); c = (mn + mx) * 0.5
    H.stage(wall_y=mx.y + 0.03 + 0.15 * (mx.x - mn.x))
    def dress(x, base):
        if "slotnames" not in x: x["slotnames"] = [sl.material.name if sl.material else "" for sl in x.material_slots]
        for n, sl in zip(x["slotnames"], x.material_slots):
            sl.material = glassm if n == "Hero_Glass" else (bpy.data.materials["BreakPrev"] if n.startswith("Break") else base)
    if not bpy.data.materials.get("BreakPrev"):
        H.plastic("BreakPrev", (0.86, 0.84, 0.80), 0.8)
    dress(o, prev)
    d34 = Vector((math.sin(math.radians(35)), -math.cos(math.radians(35)), 0.55)).normalized()
    def three_quarter(objs, lens=50, fill=0.86):
        bm_, bx_ = H.bounds(objs); cc = (bm_ + bx_) * 0.5
        return fit_camera(objs, lambda dd: cc + d34 * dd, cc, lens, fill)
    def ant(objs, lens=24, fill=0.92):
        bm_, bx_ = H.bounds(objs); cc = (bm_ + bx_) * 0.5
        zc = 15 * K; look = Vector((cc.x, cc.y, max(zc, bm_.z + 0.45 * (bx_.z - bm_.z))))
        dh = Vector((-0.25, -1.0, 0.0)).normalized()
        return fit_camera(objs, lambda dd: (look.x + dh.x * dd, look.y + dh.y * dd, zc), look, lens, fill)
    def shot(tag, cam, lens, fstop, w, h):
        loc, look = cam; H.camera(loc, look, lens, fstop)
        p = os.path.join(REV, "_%s_%s.jpg" % (name, tag)); H.render(p, w, h, 128); return p
    main = [shot("wide", three_quarter([o]), 50, 11, 1600, 900), shot("ant", ant([o]), 24, 5.6, 1600, 900)]
    states = [shot("clean", three_quarter([o]), 50, 11, 960, 540)]
    for k, (w, ch) in enumerate(((1.0, 0.0), (0.0, 0.45))):
        sm = H.state_material(prev, w, ch)
        extras = []
        if w > 0:
            for nd in sm.node_tree.nodes:   # the paper and cloth soak darker than the default review layer
                if nd.type == "MATH" and nd.operation == "MULTIPLY" and abs(nd.inputs[1].default_value - 0.6 * w) < 1e-6: nd.inputs[1].default_value = 0.95
            extras = wet_extras(o)
        if ch > 0:   # charred = matte soot, no glow (embers are active burning, not this state); smoke-stained glass
            sm.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = 0.0
            for nd in sm.node_tree.nodes:
                if nd.type == "MAP_RANGE" and nd.inputs["From Min"].default_value < 1.0: nd.inputs["From Min"].default_value -= 0.25; nd.inputs["From Max"].default_value -= 0.15
            sm.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.95
        gsave = glassm
        if ch > 0:
            glassm = soot_glass()
        dress(o, sm)
        states.append(shot("state%d" % k, three_quarter([o] + extras[1:2]), 50, 11, 960, 540))
        glassm = gsave
        for x in extras: bpy.data.objects.remove(x)
    dress(o, prev)
    if pieces:
        o.hide_render = True
        R = random.Random(7); s = max((mx - mn).length, 0.02)
        for k, x in enumerate(pieces):
            x.hide_render = False; dress(x, prev)
            pm, px = H.bounds([x]); pc = (pm + px) * 0.5
            out = Vector((pc.x - c.x, pc.y - c.y, 0)); out = out.normalized() if out.length > 0.002 else Vector((0.0, -1.0, 0.0))
            x.matrix_world = Matrix.Translation(out * 0.13 * s * (1 + k * 0.2) - Vector((0, 0, pm.z))) @ Matrix.Translation(pc) @ Matrix.Rotation(R.uniform(-0.4, 0.4), 4, "Z") @ Matrix.Translation(-pc)
        states.append(shot("broken", three_quarter(pieces), 50, 11, 960, 540))
    else:
        states.append(shot("whole", three_quarter([o]), 50, 11, 960, 540))
    H.sheet(main, os.path.join(REV, name + "_blender.jpg"), 2)
    H.sheet(states, os.path.join(REV, name + "_states.jpg"), 4)

def main():
    t0 = time.time()
    H.reset()
    os.makedirs(OUT, exist_ok=True); os.makedirs(REV, exist_ok=True)
    mpath = os.path.join(OUT, "hero_props_manifest.json")
    doc = json.load(open(mpath, encoding="utf-8")) if os.path.exists(mpath) else {"pieces": []}
    H.plastic("Break", (0.8, 0.8, 0.78), 0.7)
    for name, sp in SPECS.items():
        if ONLY and name not in ONLY: continue
        for x in list(H.S().objects): bpy.data.objects.remove(x)
        M = mats()
        if SHAPE: shape_preview(name, sp, M); continue
        man, lod0, pieces, paths = produce(name, sp, M)
        doc["pieces"] = [p for p in doc["pieces"] if p["name"] != name] + [man]
        json.dump(doc, open(mpath, "w", encoding="utf-8"), indent=1, ensure_ascii=False)
        prev = H.preview_material(name, paths)
        if sp.get("hang"):   # the rail's tools hang from their hook point (the origin): stood on the review floor instead
            mn, mx = H.bounds([lod0]); dz = -mn.z + 0.0005
            for o in [lod0] + pieces: o.location.z += dz
        review(name, lod0, pieces, prev, M[sp.get("glassmat", "glass")])
        print("HERO_PROPS %s done" % name)
    print("HERO_PROPS_DONE %.0f s" % (time.time() - t0))

main()
