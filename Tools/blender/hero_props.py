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
    return mats8(mats7(mats6(mats5(mats4(mats2(M))))))

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
