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
# Run: blender --background --factory-startup --python hero_props.py -- <SourceArt/Blender/hero_props> <Saved/art_review/props> [names] [nobake]
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
LAB = os.path.join(OUT, "labels")
K = H.K


def u(*v): return tuple(x * K for x in v)

def lathe(name, prof_uu, mat, steps=64, sub=0, at=(0, 0, 0)):
    o = H.screw(name, [(r * K, z * K) for r, z in prof_uu], steps, sub); o.location = u(*at); o.data.materials.append(mat); return o

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
    return mats4(mats2(M))

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
    M["tinLid"] = H.metal("TinLid", (0.80, 0.80, 0.78), 0.22)
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
    lay(lid, -200, 123, Matrix.Translation(p) @ Matrix.Rotation(math.radians(-30), 4, "X") @ Matrix.Translation(-p))
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

def mats4(M):
    M["cutlery"] = H.metal("Cutlery", (0.84, 0.85, 0.86), 0.18)
    M["handle"] = H.plastic("KnifeHandle", (0.03, 0.03, 0.03), 0.35, peel=0.02, scuff=0.15)
    M["phoneScreen"] = top_print("PhoneScreen", "phone_screen.png", -78, 78, -152, 152, (0.02, 0.02, 0.025), 0.05, 0.0, 1.0, 0.0)
    M["phoneFrame"] = H.metal("PhoneFrame", (0.55, 0.56, 0.60), 0.3)
    M["phoneBack"] = H.plastic("PhoneBack", (0.10, 0.12, 0.16), 0.15, peel=0.01, scuff=0.08)
    M["remote"] = top_print("RemotePrint", "remote_print.png", -300, 300, -100, 100, (0.04, 0.04, 0.045), 0.45, 0.0, 0.0, 0.1)
    M["rubber"] = H.rubber("ButtonRubber", (0.22, 0.22, 0.24), 0.6)
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
    parts = [(rrect("Body", -300, 300, -98, 98, 0, 34, 70, M["remote"], 8, 10), 0),
             (rbox("Cover", -270, -40, -78, 78, -0.2, 2.5, M["remote"], 1.0, 0, 1), 1)]
    for (x, y) in [(x, y) for y in (-55, 0, 55) for x in (-200, -140, -80)] + [(-20, 0)]:
        parts.append((lathe("Btn", [(0, 42), (15, 41.5), (18, 38), (18, 30), (0, 30)], M["rubber"], 20, 0, (x, y, 0)), 0))
    parts.append((lathe("Ok", [(0, 42), (20, 41), (24, 37), (24, 30), (0, 30)], M["rubber"], 24, 0, (110, 0, 0)), 0))
    parts.append((lathe("Ring", [(30, 39), (52, 39), (56, 35), (56, 30), (30, 30)], M["rubber"], 32, 0, (110, 0, 0)), 0))
    for y in (60, -60):
        parts.append((rrect("Rock", 175, 225, y - 30, y + 30, 30, 41, 14, M["rubber"], 4, 2), 0))
    parts.append((lathe("Power", [(0, 41), (16, 40.5), (19, 37), (19, 30), (0, 30)], M["redBtn"], 20, 0, (255, 60, 0)), 0))
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
            hd = H.cyl("Head", 9 * K, 22 * K, u(x, hy, z), (math.pi / 2, 0, 0), 10); H.smooth(hd, 1, 2 * K, 1); hd.data.materials.append(M["head"])
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

def breaks_plate(o, n):
    return shards(o, n, [((10, 0, 0), (1.0, 0.25, 0.0), 0.0012, 31, 0), ((0, -20, 0), (0.15, 1.0, 0.0), 0.0012, 37, 0),
                         ((-10, 30, 0), (-0.2, 1.0, 0.0), 0.0012, 41, 1)])

GLASS_CUTS = [((0, 0, 0.55), (0.10, 0.06, 1.0), 0.0012, 43, 0), ((5, 0, 0.3), (1.0, 0.35, 0.25), 0.0014, 47, 0),
              ((-5, 0, 0.75), (0.9, -0.6, 0.3), 0.0014, 53, 1), ((0, 8, 0.4), (-0.3, 1.0, -0.2), 0.0012, 57, 0)]

def glass_cuts(h_uu, scale=1.0):
    """the glass cuts of a jar h_uu tall (z given as a fraction of it): a band round the shoulder, then vertical-ish splits"""
    return [((x * scale, y * scale, z * h_uu), no, jag * scale, seed, w) for (x, y, z), no, jag, seed, w in GLASS_CUTS]

def breaks_spice(o, n):
    # the glass and its label shatter together; the cap stays whole; the paprika (part 2) is not a piece: it spills (AntWreck)
    return cut_groups(o, n, [lambda p, g: g == 1, lambda p, g: g == 0 and p == 0], [lambda p, g: p == 1], glass_cuts(140))

def breaks_mug(o, n):
    return cut_groups(o, n, [lambda p, g: p == 0], [lambda p, g: p == 1],
                      [((8, 0, 150), (1.0, 0.4, 0.15), 0.0016, 53, 0), ((-30, 10, 120), (0.3, -1.0, 0.25), 0.0016, 59, 0)])

def breaks_pasta(o, n):
    # torn across (paper: a ragged, fibrous line); the glued flap goes with the top
    return cut_groups(o, n, [lambda p, g: p == 0], [], [((0, 0, 470), (0.12, 0.0, 1.0), 0.010, 59, 0)])

def breaks_jam(o, n):
    return cut_groups(o, n, [lambda p, g: g == 1, lambda p, g: g == 0 and p == 0], [lambda p, g: p == 1], glass_cuts(240, 3.0))

def ring_y(r_uu, y_uu, zc_uu, n=12):
    return [(r_uu * K * math.cos(2 * math.pi * k / n), y_uu * K, (zc_uu + r_uu * math.sin(2 * math.pi * k / n)) * K) for k in range(n)]

def hulls_tin(n):
    return [H.hull("UCX_%s_00" % n, ring_y(123, -200, 123, 16) + ring_y(123, 202, 123, 16)),
            H.hull("UCX_%s_01" % n, boxp(-116, 116, -320, -198, 0, 240))]

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
    return cut_groups(o, n, [lambda p, g: g == 1, lambda p, g: g == 0], [], cuts)

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
    "SM_L_Remote": dict(build=remote_ctl, hulls=lambda n: [H.hull("UCX_%s_00" % n, boxp(-300, 300, -98, 98, 0, 42))], lods=(0.5, 0.25), cls="plastic",
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

# ------------------------------------------------------------------------------------------------ one asset (hero_lib.produce + the glass slot)
def glass_faces(o):
    a = o.data.attributes.get("glass")
    return [a.data[i].value == 1 for i in range(len(o.data.polygons))] if a else [False] * len(o.data.polygons)

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
    H.unwrap(lod0, 66.0, 0.003)
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
def review(name, o, pieces, prev, glassm):
    for x in list(H.S().objects):
        if x.type in ("LIGHT", "CAMERA") or (x.type == "MESH" and x not in [o] + pieces and not x.name.startswith(name)):
            bpy.data.objects.remove(x)
    for x in pieces: x.hide_render = True
    mn, mx = H.bounds([o]); c = (mn + mx) * 0.5; s = max((mx - mn).length, 0.05)
    H.stage(wall_y=mx.y + 0.03)
    def dress(x, base):
        if "slotnames" not in x: x["slotnames"] = [sl.material.name if sl.material else "" for sl in x.material_slots]
        for n, sl in zip(x["slotnames"], x.material_slots):
            sl.material = glassm if n == "Hero_Glass" else (bpy.data.materials["BreakPrev"] if n.startswith("Break") else base)
    if not bpy.data.materials.get("BreakPrev"):
        bm = H.plastic("BreakPrev", (0.86, 0.84, 0.80), 0.8)
    dress(o, prev)
    shots = []
    def shot(tag, loc, look, lens, fstop=5.6):
        H.camera(loc, look, lens, fstop); p = os.path.join(REV, "_%s_%s.jpg" % (name, tag)); H.render(p, 1280, 720, 128); shots.append(p)
    shot("wide", (c.x + 1.1 * s, mn.y - 1.9 * s, c.z + 0.7 * s), (c.x, c.y, c.z), 45)
    shot("ant", (c.x - 0.18 * s, mn.y - 0.55 * s, 0.006), (c.x, c.y, mn.z + 0.45 * (mx.z - mn.z)), 18, 11)
    for k, (w, ch) in enumerate(((1.0, 0.0), (0.0, 0.45))):
        dress(o, H.state_material(prev, w, ch))
        shot("state%d" % k, (c.x + 1.1 * s, mn.y - 1.9 * s, c.z + 0.7 * s), (c.x, c.y, c.z), 45)
    dress(o, prev)
    if pieces:
        o.hide_render = True
        R = random.Random(7)
        for k, x in enumerate(pieces):
            x.hide_render = False; dress(x, prev)
            pm, px = H.bounds([x]); pc = (pm + px) * 0.5
            out = Vector((pc.x - c.x, pc.y - c.y, 0)); out = out.normalized() if out.length > 0.002 else Vector((0.0, -1.0, 0.0))
            x.matrix_world = Matrix.Translation(out * 0.3 * s * (1 + k * 0.25) - Vector((0, 0, pm.z))) @ Matrix.Translation(pc) @ Matrix.Rotation(R.uniform(-0.4, 0.4), 4, "Z") @ Matrix.Translation(-pc)
        shot("broken", (c.x + 1.1 * s, mn.y - 1.9 * s, c.z + 0.7 * s), (c.x, c.y, c.z * 0.5), 45)
    H.sheet(shots[:2], os.path.join(REV, name + "_blender.jpg"), 2)
    st = shots[2:] + ([shots[1]] if len(shots) == 4 else [])
    H.sheet(st[:4] if len(st) >= 4 else st + [shots[0]] * (4 - len(st)), os.path.join(REV, name + "_states.jpg"), 2)

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
        man, lod0, pieces, paths = produce(name, sp, M)
        doc["pieces"] = [p for p in doc["pieces"] if p["name"] != name] + [man]
        json.dump(doc, open(mpath, "w", encoding="utf-8"), indent=1, ensure_ascii=False)
        prev = H.preview_material(name, paths)
        review(name, lod0, pieces, prev, M[sp.get("glassmat", "glass")])
        print("HERO_PROPS %s done" % name)
    print("HERO_PROPS_DONE %.0f s" % (time.time() - t0))

main()
