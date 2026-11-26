# Hero art, the living room and the furniture of the flat (owner's task 26.11: "everything as good as the mop", put in the game).
# Each piece replaces the mesh of the same name in /Game/Hero/<Name> and keeps the old frame, size and working surfaces
# (seat 0.368 m, coffee table top 0.516 m, shelf heights...: the numbers come from Tools/blender/living_room.py & co).
# Pipeline: hero_lib.py (bake C/N/R, UCX, LOD1/2, pre-cut break pieces), output SourceArt/Blender/hero_living/<Name>/ and
# hero_living_manifest.json (imported by Tools/import_blender_art.py, set hero_living). Slots the game drives by name
# (Screen, LampShade, Bulb) stay their own slots after the Hero_<Name> one.
# Review: Saved/art_review/living/<Name>.jpg = before | after | ant's view | wet | charred | broken.
# Run: blender --background --factory-startup --python hero_living.py -- <out> <review> <Name+Name|all> [nobake]
import bpy, bmesh, sys, os, math, random, json, time
from mathutils import Vector, Matrix
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hero_lib as H
import hero_owner as HO

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = argv[0] if argv else os.path.join(ROOT, "SourceArt", "Blender", "hero_living")
REV = argv[1] if len(argv) > 1 else os.path.join(ROOT, "Saved", "art_review", "living")
ONLY = argv[2].replace("+", ",").split(",") if len(argv) > 2 and argv[2] not in ("all", "") else None
NOBAKE = len(argv) > 3 and argv[3] == "nobake"
OLD = os.path.join(ROOT, "SourceArt", "Blender")
S = 1900.0
LIMIT = 14500                    # LOD0 triangles (the owner's bar: <= 15k)

# ================================================================================================ materials
def N(m, t, x=0, y=0): return H.N(m, t, x, y)
def L(m, a, b): H.L(m, a, b)

def _wrap_base(m, p, fn):
    """put a colour stage between whatever feeds Base Color and the BSDF: fn(m, src_socket) -> new socket"""
    lk = p.inputs["Base Color"].links
    if lk: src = lk[0].from_socket
    else:
        rgb = N(m, "ShaderNodeRGB", -900, 500); rgb.outputs[0].default_value = p.inputs["Base Color"].default_value; src = rgb.outputs[0]
    L(m, fn(m, src), p.inputs["Base Color"])

def dust(m, amount=0.22, col=(0.56, 0.53, 0.48)):
    """lived-in: a grey film of dust on what faces up, in patches; a little grime low down (where feet and the mop go)"""
    p = m.node_tree.nodes["Principled BSDF"]
    def fn(m, src):
        g = N(m, "ShaderNodeNewGeometry", -1400, 900); sp = N(m, "ShaderNodeSeparateXYZ", -1200, 900); L(m, g.outputs["Normal"], sp.inputs[0])
        up = N(m, "ShaderNodeMapRange", -1000, 900); up.inputs["From Min"].default_value = 0.55; up.inputs["From Max"].default_value = 0.95; L(m, sp.outputs["Z"], up.inputs["Value"])
        tc = N(m, "ShaderNodeTexCoord", -1400, 1100); no = N(m, "ShaderNodeTexNoise", -1200, 1100); no.inputs["Scale"].default_value = 9; no.inputs["Detail"].default_value = 8
        L(m, tc.outputs["Object"], no.inputs["Vector"])
        pr = N(m, "ShaderNodeMapRange", -1000, 1100); pr.inputs["From Min"].default_value = 0.42; pr.inputs["From Max"].default_value = 0.68; L(m, no.outputs["Fac"], pr.inputs["Value"])
        mu = N(m, "ShaderNodeMath", -800, 1000); mu.operation = "MULTIPLY"; L(m, up.outputs["Result"], mu.inputs[0]); L(m, pr.outputs["Result"], mu.inputs[1])
        mu2 = N(m, "ShaderNodeMath", -650, 1000); mu2.operation = "MULTIPLY"; mu2.inputs[1].default_value = amount; L(m, mu.outputs[0], mu2.inputs[0])
        # grime: the bottom 6 cm
        ps = N(m, "ShaderNodeSeparateXYZ", -1200, 1300); L(m, tc.outputs["Object"], ps.inputs[0])
        lo = N(m, "ShaderNodeMapRange", -1000, 1300); lo.inputs["From Min"].default_value = 0.0; lo.inputs["From Max"].default_value = 0.06
        lo.inputs["To Min"].default_value = 0.18; lo.inputs["To Max"].default_value = 0.0; L(m, ps.outputs["Z"], lo.inputs["Value"])
        mx = N(m, "ShaderNodeMix", -450, 900); mx.data_type = "RGBA"; mx.inputs[7].default_value = (*col, 1)
        L(m, mu2.outputs[0], mx.inputs[0]); L(m, src, mx.inputs[6])
        gr = N(m, "ShaderNodeMix", -300, 900); gr.data_type = "RGBA"; gr.blend_type = "MULTIPLY"; gr.inputs[7].default_value = (0.55, 0.5, 0.44, 1)
        L(m, lo.outputs["Result"], gr.inputs[0]); L(m, mx.outputs[2], gr.inputs[6])
        return gr.outputs[2]
    _wrap_base(m, p, fn)
    return m

def edge_wear(m, col=(1, 1, 1), amount=0.5, lo=0.565, hi=0.66):
    """worn edges (Cycles pointiness at bake time): the finish rubs through to a lighter tone"""
    p = m.node_tree.nodes["Principled BSDF"]
    def fn(m, src):
        g = N(m, "ShaderNodeNewGeometry", -1400, 1500)
        mr = N(m, "ShaderNodeMapRange", -1200, 1500); mr.inputs["From Min"].default_value = lo; mr.inputs["From Max"].default_value = hi
        L(m, g.outputs["Pointiness"], mr.inputs["Value"])
        tc = N(m, "ShaderNodeTexCoord", -1400, 1700); no = N(m, "ShaderNodeTexNoise", -1200, 1700); no.inputs["Scale"].default_value = 60; no.inputs["Detail"].default_value = 6
        L(m, tc.outputs["Object"], no.inputs["Vector"])
        mu = N(m, "ShaderNodeMath", -1000, 1600); mu.operation = "MULTIPLY"; L(m, mr.outputs["Result"], mu.inputs[0]); L(m, no.outputs["Fac"], mu.inputs[1])
        mu2 = N(m, "ShaderNodeMath", -850, 1600); mu2.operation = "MULTIPLY"; mu2.inputs[1].default_value = amount * 2.0; mu2.use_clamp = True; L(m, mu.outputs[0], mu2.inputs[0])
        mx = N(m, "ShaderNodeMix", -650, 1500); mx.data_type = "RGBA"; mx.inputs[7].default_value = (*col, 1)
        L(m, mu2.outputs[0], mx.inputs[0]); L(m, src, mx.inputs[6])
        return mx.outputs[2]
    _wrap_base(m, p, fn)
    return m

def fabric(name, col, dark=None, scale=900.0, sheen=0.6, slub=0.25, pores=0.9):
    """woven upholstery: a plain weave (two crossing band waves), slub yarn colour, a fuzz bump; porous (wet darkens it a lot)"""
    m, p = H.mat(name)
    dark = dark or tuple(c * 0.72 for c in col)
    tc = N(m, "ShaderNodeTexCoord", -1600)
    w1 = N(m, "ShaderNodeTexWave", -1300, 200); w1.wave_type = "BANDS"; w1.bands_direction = "X"; w1.inputs["Scale"].default_value = scale; w1.inputs["Distortion"].default_value = 0.6
    w2 = N(m, "ShaderNodeTexWave", -1300, 0); w2.wave_type = "BANDS"; w2.bands_direction = "Z"; w2.inputs["Scale"].default_value = scale; w2.inputs["Distortion"].default_value = 0.6
    w3 = N(m, "ShaderNodeTexWave", -1300, -200); w3.wave_type = "BANDS"; w3.bands_direction = "Y"; w3.inputs["Scale"].default_value = scale; w3.inputs["Distortion"].default_value = 0.6
    for w in (w1, w2, w3): L(m, tc.outputs["Object"], w.inputs["Vector"]); w.inputs["Distortion"].default_value = 0.0
    a = N(m, "ShaderNodeMath", -1100, 100); a.operation = "MULTIPLY"; L(m, w1.outputs["Fac"], a.inputs[0]); L(m, w2.outputs["Fac"], a.inputs[1])
    b = N(m, "ShaderNodeMath", -950, 0); b.operation = "MAXIMUM"; L(m, a.outputs[0], b.inputs[0]); L(m, w3.outputs["Fac"], b.inputs[1])
    # heathered yarn: fine random flecks of the darker and lighter ply, a slow slub variation on top
    sl = N(m, "ShaderNodeTexNoise", -1300, 400); sl.inputs["Scale"].default_value = 14; sl.inputs["Detail"].default_value = 6; L(m, tc.outputs["Object"], sl.inputs["Vector"])
    ht = N(m, "ShaderNodeTexNoise", -1300, 600); ht.inputs["Scale"].default_value = 420; ht.inputs["Detail"].default_value = 1; L(m, tc.outputs["Object"], ht.inputs["Vector"])
    hm = N(m, "ShaderNodeMix", -1100, 500); hm.data_type = "FLOAT"; hm.inputs[0].default_value = 0.65; L(m, sl.outputs["Fac"], hm.inputs[2]); L(m, ht.outputs["Fac"], hm.inputs[3])
    cr = N(m, "ShaderNodeValToRGB", -1000, 400); e = cr.color_ramp.elements; e[0].position = 0.5 - slub; e[0].color = (*dark, 1); e[1].position = 0.5 + slub; e[1].color = (*col, 1)
    L(m, hm.outputs[0], cr.inputs["Fac"])
    sh = N(m, "ShaderNodeMix", -700, 300); sh.data_type = "RGBA"; sh.blend_type = "MULTIPLY"; sh.inputs[7].default_value = (0.85, 0.85, 0.85, 1)
    inv = N(m, "ShaderNodeMath", -850, 200); inv.operation = "MULTIPLY_ADD"; inv.inputs[1].default_value = -0.35; inv.inputs[2].default_value = 0.35; L(m, b.outputs[0], inv.inputs[0])
    L(m, inv.outputs[0], sh.inputs[0]); L(m, cr.outputs["Color"], sh.inputs[6]); L(m, sh.outputs[2], p.inputs["Base Color"])
    fz = N(m, "ShaderNodeTexNoise", -1300, -400); fz.inputs["Scale"].default_value = 900; fz.inputs["Detail"].default_value = 2; L(m, tc.outputs["Object"], fz.inputs["Vector"])
    hb = N(m, "ShaderNodeMath", -800, -300); hb.operation = "MULTIPLY_ADD"; hb.inputs[1].default_value = 0.25; L(m, fz.outputs["Fac"], hb.inputs[0]); L(m, b.outputs[0], hb.inputs[2])
    bp = N(m, "ShaderNodeBump", -500, -300); bp.inputs["Strength"].default_value = 0.3; bp.inputs["Distance"].default_value = 0.0004
    L(m, hb.outputs[0], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    p.inputs["Roughness"].default_value = 0.88; p.inputs["Sheen Weight"].default_value = sheen; p.inputs["Sheen Roughness"].default_value = 0.4
    if scale < 600: fabric_wear(m)
    return H.tag(m, 0.0, pores)


def scratches(m, col=(1, 1, 1), amount=0.6, scale=7.0, seed=0.0, dents=0.25):
    """use marks: fine straight scratches in two directions (thin wave bands, each switched on in short random segments),
    lighter and rougher; small dents in the bump. The thing has been lived with."""
    p = m.node_tree.nodes["Principled BSDF"]
    tc = N(m, "ShaderNodeTexCoord", -2400, 1900)
    masks = []
    for k, rz in enumerate((0.35 + seed, 1.9 + seed * 0.7)):
        mp = N(m, "ShaderNodeMapping", -2200, 1900 - k * 300); mp.inputs["Rotation"].default_value = (0.2 * k, 0.15, rz)
        L(m, tc.outputs["Object"], mp.inputs["Vector"])
        wv = N(m, "ShaderNodeTexWave", -2000, 1900 - k * 300); wv.wave_type = "BANDS"; wv.bands_direction = "X"; wv.inputs["Scale"].default_value = scale * 5.0
        wv.inputs["Distortion"].default_value = 0.0; L(m, mp.outputs["Vector"], wv.inputs["Vector"])
        ln = N(m, "ShaderNodeMapRange", -1800, 1900 - k * 300); ln.inputs["From Min"].default_value = 0.985; ln.inputs["From Max"].default_value = 1.0; L(m, wv.outputs["Fac"], ln.inputs["Value"])
        mp2 = N(m, "ShaderNodeMapping", -2200, 1750 - k * 300); mp2.inputs["Scale"].default_value = (scale * 9.0, scale * 0.35, scale * 0.35); L(m, mp.outputs["Vector"], mp2.inputs["Vector"])
        sp = N(m, "ShaderNodeTexNoise", -2000, 1750 - k * 300); sp.noise_dimensions = "4D"; sp.inputs["Scale"].default_value = 1.0; sp.inputs["Detail"].default_value = 1
        sp.inputs["W"].default_value = 3 + k + seed; L(m, mp2.outputs["Vector"], sp.inputs["Vector"])
        sm = N(m, "ShaderNodeMapRange", -1800, 1750 - k * 300); sm.inputs["From Min"].default_value = 0.60; sm.inputs["From Max"].default_value = 0.66; L(m, sp.outputs["Fac"], sm.inputs["Value"])
        mm = N(m, "ShaderNodeMath", -1350, 1850 - k * 300); mm.operation = "MULTIPLY"; L(m, ln.outputs["Result"], mm.inputs[0]); L(m, sm.outputs["Result"], mm.inputs[1])
        masks.append(mm.outputs[0])
    mx = N(m, "ShaderNodeMath", -1200, 1800); mx.operation = "MAXIMUM"; L(m, masks[0], mx.inputs[0]); L(m, masks[1], mx.inputs[1])
    sc = N(m, "ShaderNodeMath", -1100, 1800); sc.operation = "MULTIPLY"; sc.inputs[1].default_value = amount; sc.use_clamp = True; L(m, mx.outputs[0], sc.inputs[0])
    def fn(m, src):
        c = N(m, "ShaderNodeMix", -500, 1800); c.data_type = "RGBA"; c.inputs[7].default_value = (*col, 1); L(m, sc.outputs[0], c.inputs[0]); L(m, src, c.inputs[6])
        return c.outputs[2]
    _wrap_base(m, p, fn)
    # rougher in the scratch
    rl = p.inputs["Roughness"].links
    rsrc = rl[0].from_socket if rl else None
    ra = N(m, "ShaderNodeMath", -400, 1600); ra.operation = "MULTIPLY_ADD"; ra.inputs[1].default_value = 0.35
    L(m, sc.outputs[0], ra.inputs[0])
    if rsrc: L(m, rsrc, ra.inputs[2])
    else: ra.inputs[2].default_value = p.inputs["Roughness"].default_value
    L(m, ra.outputs[0], p.inputs["Roughness"])
    # dents: a few soft dimples in the bump
    nl = p.inputs["Normal"].links
    if dents and nl:
        bp = nl[0].from_node
        if bp.type == "BUMP":
            vo = N(m, "ShaderNodeTexVoronoi", -900, 1400); vo.inputs["Scale"].default_value = scale * 3.0; L(m, tc.outputs["Object"], vo.inputs["Vector"])
            dr = N(m, "ShaderNodeMapRange", -700, 1400); dr.inputs["From Min"].default_value = 0.0; dr.inputs["From Max"].default_value = 0.12; dr.inputs["To Min"].default_value = -dents; dr.inputs["To Max"].default_value = 0.0
            L(m, vo.outputs["Distance"], dr.inputs["Value"])
            hl = bp.inputs["Height"].links
            if hl:
                ad = N(m, "ShaderNodeMath", -500, 1400); ad.operation = "ADD"; L(m, hl[0].from_socket, ad.inputs[0]); L(m, dr.outputs["Result"], ad.inputs[1]); L(m, ad.outputs[0], bp.inputs["Height"])
            sk = N(m, "ShaderNodeMath", -500, 1500); sk.operation = "MULTIPLY"; sk.inputs[1].default_value = -0.4; L(m, sc.outputs[0], sk.inputs[0])
            ad2 = N(m, "ShaderNodeMath", -350, 1450); ad2.operation = "ADD"; L(m, sk.outputs[0], ad2.inputs[0]); L(m, (bp.inputs["Height"].links[0].from_socket), ad2.inputs[1]); L(m, ad2.outputs[0], bp.inputs["Height"])
    return m

def fabric_wear(m, amount=0.35):
    """upholstery wear: pilled, lighter and flattened where hands and backs go (high curvature + noise), a few darker spots"""
    p = m.node_tree.nodes["Principled BSDF"]
    def fn(m, src):
        g = N(m, "ShaderNodeNewGeometry", -1400, 2100); mr = N(m, "ShaderNodeMapRange", -1200, 2100); mr.inputs["From Min"].default_value = 0.53; mr.inputs["From Max"].default_value = 0.62
        L(m, g.outputs["Pointiness"], mr.inputs["Value"])
        tc = N(m, "ShaderNodeTexCoord", -1400, 2300); no = N(m, "ShaderNodeTexNoise", -1200, 2300); no.inputs["Scale"].default_value = 25; no.inputs["Detail"].default_value = 8
        L(m, tc.outputs["Object"], no.inputs["Vector"])
        mu = N(m, "ShaderNodeMath", -1000, 2200); mu.operation = "MULTIPLY"; L(m, mr.outputs["Result"], mu.inputs[0]); L(m, no.outputs["Fac"], mu.inputs[1])
        mu2 = N(m, "ShaderNodeMath", -850, 2200); mu2.operation = "MULTIPLY"; mu2.inputs[1].default_value = amount * 2; mu2.use_clamp = True; L(m, mu.outputs[0], mu2.inputs[0])
        sc = N(m, "ShaderNodeMix", -650, 2200); sc.data_type = "RGBA"; sc.blend_type = "SCREEN"; sc.inputs[7].default_value = (0.35, 0.33, 0.30, 1)
        L(m, mu2.outputs[0], sc.inputs[0]); L(m, src, sc.inputs[6])
        sp = N(m, "ShaderNodeTexVoronoi", -1200, 2500); sp.inputs["Scale"].default_value = 3.0; L(m, tc.outputs["Object"], sp.inputs["Vector"])
        sm = N(m, "ShaderNodeMapRange", -1000, 2500); sm.inputs["From Min"].default_value = 0.06; sm.inputs["From Max"].default_value = 0.0; sm.inputs["To Max"].default_value = 0.25
        L(m, sp.outputs["Distance"], sm.inputs["Value"])
        dk = N(m, "ShaderNodeMix", -450, 2300); dk.data_type = "RGBA"; dk.blend_type = "MULTIPLY"; dk.inputs[7].default_value = (0.55, 0.5, 0.45, 1)
        L(m, sm.outputs["Result"], dk.inputs[0]); L(m, sc.outputs[2], dk.inputs[6])
        return dk.outputs[2]
    _wrap_base(m, p, fn)
    return m

def wood(name, light=(0.62, 0.40, 0.19), dark=(0.30, 0.16, 0.07), axis="X", grain=24.0, varnish=0.35, pore=0.5, wear=0.35, rough=0.42):
    """sawn timber / veneer, the grain running along axis: rings (stretched noise), dark pores, varnish worn pale on the edges"""
    m, p = H.mat(name)
    grain = grain * 0.38          # (the first pass read as bamboo: real boards have a wider figure)
    sc = {"X": (1.0, grain, grain), "Y": (grain, 1.0, grain), "Z": (grain, grain, 1.0)}[axis]
    tc = N(m, "ShaderNodeTexCoord", -1600); mp = N(m, "ShaderNodeMapping", -1400); mp.inputs["Scale"].default_value = sc
    L(m, tc.outputs["Object"], mp.inputs["Vector"])
    wd = N(m, "ShaderNodeTexNoise", -1200, 0); wd.inputs["Scale"].default_value = 4.5; wd.inputs["Detail"].default_value = 12; wd.inputs["Roughness"].default_value = 0.62
    L(m, mp.outputs["Vector"], wd.inputs["Vector"])
    rg = N(m, "ShaderNodeTexWave", -1200, 250); rg.wave_type = "RINGS"; rg.inputs["Scale"].default_value = 1.4; rg.inputs["Distortion"].default_value = 9; rg.inputs["Detail"].default_value = 4
    L(m, mp.outputs["Vector"], rg.inputs["Vector"])
    mix = N(m, "ShaderNodeMix", -1000, 100); mix.data_type = "FLOAT"; mix.inputs[0].default_value = 0.45; L(m, wd.outputs["Fac"], mix.inputs[2]); L(m, rg.outputs["Fac"], mix.inputs[3])
    cr = N(m, "ShaderNodeValToRGB", -800, 100); e = cr.color_ramp.elements; e[0].position = 0.28; e[0].color = (*dark, 1); e[1].position = 0.78; e[1].color = (*light, 1)
    L(m, mix.outputs[0], cr.inputs["Fac"])
    sp = [1.0, 1.0, 1.0]; sp["XYZ".index(axis)] = 0.04
    mp2 = N(m, "ShaderNodeMapping", -1400, -300); mp2.inputs["Scale"].default_value = (1.0 / sp[0], 1.0 / sp[1], 1.0 / sp[2]); L(m, tc.outputs["Object"], mp2.inputs["Vector"])
    po = N(m, "ShaderNodeTexNoise", -1200, -300); po.inputs["Scale"].default_value = 9; po.inputs["Detail"].default_value = 2; L(m, mp2.outputs["Vector"], po.inputs["Vector"])
    pr = N(m, "ShaderNodeMapRange", -1000, -300); pr.inputs["From Min"].default_value = 0.62; pr.inputs["From Max"].default_value = 0.72; L(m, po.outputs["Fac"], pr.inputs["Value"])
    pm = N(m, "ShaderNodeMix", -600, 0); pm.data_type = "RGBA"; pm.blend_type = "MULTIPLY"; pm.inputs[7].default_value = (0.55, 0.45, 0.38, 1)
    pf = N(m, "ShaderNodeMath", -800, -200); pf.operation = "MULTIPLY"; pf.inputs[1].default_value = pore; L(m, pr.outputs["Result"], pf.inputs[0])
    L(m, pf.outputs[0], pm.inputs[0]); L(m, cr.outputs["Color"], pm.inputs[6]); L(m, pm.outputs[2], p.inputs["Base Color"])
    hb = N(m, "ShaderNodeMath", -700, -400); hb.operation = "MULTIPLY_ADD"; hb.inputs[1].default_value = -0.6; L(m, pr.outputs["Result"], hb.inputs[0]); L(m, mix.outputs[0], hb.inputs[2])
    bp = N(m, "ShaderNodeBump", -400, -400); bp.inputs["Strength"].default_value = 0.10; bp.inputs["Distance"].default_value = 0.0005
    L(m, hb.outputs[0], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    p.inputs["Roughness"].default_value = rough; p.inputs["Coat Weight"].default_value = varnish; p.inputs["Coat Roughness"].default_value = 0.18
    if wear: edge_wear(m, tuple(min(1, c * 1.35 + 0.06) for c in light), wear)
    if wear: scratches(m, tuple(min(1, c * 1.45 + 0.10) for c in light), 0.75, 7.0, len(name) * 0.37)
    return H.tag(m, 0.0, 0.45)

def paint(name, col, rough=0.42, peel=0.04, wear=0.3, under=None, metal_under=False, porosity=0.12):
    """painted / lacquered surface: orange peel, scuffs, the edges rubbed through to what is under (wood, primer, steel)"""
    m = H.plastic(name, col, rough, peel, 0.22)
    if wear: edge_wear(m, under or tuple(min(1, c * 1.2 + 0.12) for c in col), wear)
    if wear: scratches(m, under or tuple(min(1, c * 1.2 + 0.12) for c in col), 0.55, 9.0, len(name) * 0.29)
    return H.tag(m, 0.0, porosity)

def glossy(name, col, rough=0.06, coat=0.0, emit=None):
    m, p = H.mat(name); p.inputs["Base Color"].default_value = (*col, 1); p.inputs["Roughness"].default_value = rough
    p.inputs["Coat Weight"].default_value = coat
    tc = N(m, "ShaderNodeTexCoord", -900); no = N(m, "ShaderNodeTexNoise", -700); no.inputs["Scale"].default_value = 90; no.inputs["Detail"].default_value = 6
    L(m, tc.outputs["Object"], no.inputs["Vector"])
    mr = N(m, "ShaderNodeMapRange", -400); mr.inputs["From Min"].default_value = 0.45; mr.inputs["From Max"].default_value = 0.75; mr.inputs["To Min"].default_value = rough; mr.inputs["To Max"].default_value = rough + 0.18
    L(m, no.outputs["Fac"], mr.inputs["Value"]); L(m, mr.outputs["Result"], p.inputs["Roughness"])     # smudges, fingerprints
    return H.tag(m, 0.0, 0.0)

def plaster(name, col=(0.86, 0.84, 0.78), rough=0.8):
    m, p = H.mat(name); p.inputs["Base Color"].default_value = (*col, 1); p.inputs["Roughness"].default_value = rough
    tc = N(m, "ShaderNodeTexCoord", -900); no = N(m, "ShaderNodeTexNoise", -700); no.inputs["Scale"].default_value = 220; no.inputs["Detail"].default_value = 8
    L(m, tc.outputs["Object"], no.inputs["Vector"])
    bp = N(m, "ShaderNodeBump", -300); bp.inputs["Strength"].default_value = 0.18; bp.inputs["Distance"].default_value = 0.0005
    L(m, no.outputs["Fac"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return H.tag(m, 0.0, 0.6)

def emissive(name, col, strength=4.0):
    m, p = H.mat(name); p.inputs["Base Color"].default_value = (*col, 1); p.inputs["Emission Color"].default_value = (*col, 1)
    p.inputs["Emission Strength"].default_value = strength; p.inputs["Roughness"].default_value = 0.3
    return H.tag(m, 0.0, 0.0)

def brass(name="Brass"): return H.metal(name, (0.80, 0.58, 0.26), 0.30)
def steel(name="Steel"): return H.metal(name, (0.66, 0.67, 0.69), 0.32)
def chrome(name="Chrome"): return H.metal(name, (0.90, 0.90, 0.92), 0.10)

_MC = {}
def M(key, fn, *a, **k):
    """materials made once per run (same name -> same material across parts)"""
    if key not in _MC: _MC[key] = fn(key, *a, **k)
    return _MC[key]

# ================================================================================================ geometry
def _mod_bevel(o, w, seg, harden=True, angle=40):
    b = o.modifiers.new("bev", "BEVEL"); b.width = w; b.segments = seg; b.limit_method = "ANGLE"; b.angle_limit = math.radians(angle)
    try: b.harden_normals = harden
    except Exception: pass
    for p in o.data.polygons: p.use_smooth = True
    return o

def box(name, x0, x1, y0, y1, z0, z1, m, bev=0.004, seg=3, rot=None, pivot=None):
    """an object with chamfered edges (bevel modifier, hardened normals: flat faces stay flat)"""
    o = H.cube(name, (x1 - x0, y1 - y0, z1 - z0), ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
    if rot:
        pv = Vector(pivot) if pivot else o.location.copy()
        o.matrix_world = Matrix.Translation(pv) @ rot @ Matrix.Translation(-pv) @ o.matrix_world
    if bev: _mod_bevel(o, min(bev, 0.49 * min(x1 - x0, y1 - y0, z1 - z0)), seg)
    o.data.materials.append(m)
    return o

def soft(name, x0, x1, y0, y1, z0, z1, m, round_=0.04, seg=4, puff=0.0, sub=1):
    """upholstered block: big rounded edges, subdivided, the faces bulging a little (filled with foam)"""
    o = H.cube(name, (x1 - x0, y1 - y0, z1 - z0), ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
    bm = bmesh.new(); bm.from_mesh(o.data)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=6, use_grid_fill=True)
    if puff:
        hx, hy, hz = (x1 - x0) / 2, (y1 - y0) / 2, (z1 - z0) / 2; amt = puff * min(hx, hy, hz)
        for v in bm.verts:
            n = [v.co.x / hx, v.co.y / hy, v.co.z / hz]
            d = [0.0, 0.0, 0.0]
            for a in range(3):
                f = abs(n[a]) ** 8
                for b_ in range(3):
                    if b_ != a: f *= max(0.0, 1.0 - n[b_] ** 2)
                d[a] = math.copysign(amt * f, n[a])
            v.co += Vector(d)
    bm.to_mesh(o.data); bm.free()
    b = o.modifiers.new("bev", "BEVEL"); b.width = min(round_, 0.45 * min(x1 - x0, y1 - y0, z1 - z0)); b.segments = seg; b.limit_method = "ANGLE"; b.angle_limit = math.radians(35)
    if sub:
        s = o.modifiers.new("sub", "SUBSURF"); s.levels = sub; s.render_levels = sub
    for p in o.data.polygons: p.use_smooth = True
    o.data.materials.append(m)
    return o

def lathe(name, prof, m, loc=(0, 0, 0), steps=32, sub=0, rot=None):
    o = H.screw(name, prof, steps, sub); o.location = loc
    if rot: o.rotation_euler = rot
    o.data.materials.append(m)
    return o

def cyl(name, r, h, loc, m, axis="Z", seg=24, bev=0.0):
    rot = {"Z": (0, 0, 0), "X": (0, math.pi / 2, 0), "Y": (math.pi / 2, 0, 0)}[axis]
    o = H.cyl(name, r, h, loc, rot, seg)
    if bev: _mod_bevel(o, min(bev, 0.45 * min(r, h / 2)), 2)
    else:
        for p in o.data.polygons: p.use_smooth = True
        o.data.polygons[-1].use_smooth = False
    o.data.materials.append(m)
    return o

def pipe(name, polys, r, m, closed=False, res=4, round_corner=0.0):
    """piping / cord / wire along polylines (POLY splines: corners stay where they are)"""
    cu = bpy.data.curves.new(name, "CURVE"); cu.dimensions = "3D"; cu.bevel_depth = r; cu.bevel_resolution = 2; cu.use_fill_caps = True
    for pts in polys:
        sp = cu.splines.new("POLY"); sp.points.add(len(pts) - 1)
        for p, c in zip(sp.points, pts): p.co = (c[0], c[1], c[2], 1)
        sp.use_cyclic_u = closed
    o = H.link(bpy.data.objects.new(name, cu)); cu.materials.append(m)
    return o

def rect_loop(x0, x1, y0, y1, z, r=0.0, n=4):
    """a rectangle (rounded corners of radius r) in the XY plane at height z"""
    if r <= 0: return [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)]
    pts = []
    for cx, cy, a0 in ((x1 - r, y0 + r, -90), (x1 - r, y1 - r, 0), (x0 + r, y1 - r, 90), (x0 + r, y0 + r, 180)):
        for k in range(n + 1):
            a = math.radians(a0 + 90 * k / n); pts.append((cx + r * math.cos(a), cy + r * math.sin(a), z))
    return pts

def rot_xz(pts):
    """XY-plane points turned into the XZ plane (y <- z, z <- y)"""
    return [(x, z, y) for x, y, z in pts]

def screws_at(name, pts, normal, m, r=0.0045):
    """countersunk screw heads with a cross slot, flush on a face (normal: 'X','Y','Z', sign by the point list)"""
    out = []
    for i, (x, y, z, s) in enumerate(pts):
        ax = normal
        o = cyl("%s%d" % (name, i), r, 0.0016, (x, y, z), m, ax, 12)
        out.append(o)
        for k in (0, 1):
            d = (r * 1.5, 0.0009, 0.0009)
            sz = {"X": (0.0009, d[0], d[1]) if k == 0 else (0.0009, d[1], d[0]), "Y": (d[0], 0.0009, d[1]) if k == 0 else (d[1], 0.0009, d[0]),
                  "Z": (d[0], d[1], 0.0009) if k == 0 else (d[1], d[0], 0.0009)}[ax]
            off = {"X": (s * 0.0009, 0, 0), "Y": (0, s * 0.0009, 0), "Z": (0, 0, s * 0.0009)}[ax]
            sl = H.cube("%s%dsl%d" % (name, i, k), sz, (x + off[0], y + off[1], z + off[2])); sl.data.materials.append(M("ScrewSlot", H.rubber, (0.04, 0.04, 0.04), 0.7))
            out.append(sl)
    return out


def stitches(name, pts, m, pitch=0.009, dash=0.0055, r=0.0011, off=(0, 0, 0)):
    """topstitching: a row of short thread dashes along a polyline (one curve object, cheap)"""
    dashes = []
    pts = [Vector(p) + Vector(off) for p in pts]
    for a, b in zip(pts, pts[1:]):
        d = b - a; ln = d.length
        if ln < 1e-5: continue
        u = d / ln; n = int(ln / pitch)
        for k in range(n):
            s = a + u * (k * pitch); dashes.append([tuple(s), tuple(s + u * dash * 0.5), tuple(s + u * dash)])
    o = H.tubes(name, dashes, r, res_u=1, bevel_res=0, tip=1.0); o.data.materials.append(m)
    return o

def THREAD(): return M("Thread", lambda n: H.plastic(n, (0.80, 0.76, 0.66), 0.7, 0.0, 0.0))

# ================================================================================================ budget, hulls, breaks
def evaluated(o):
    return H.bake_mesh(o) if (o.type != "MESH" or o.modifiers) else o

def budget(parts, limit=LIMIT):
    """all parts as meshes; if the sum is over the limit, the densest parts are collapsed until it fits"""
    out = [(evaluated(o), p) for o, p in parts]
    tot = sum(H.tris(o) for o, _ in out)
    if tot > limit:
        print("HERO budget %d > %d: decimating" % (tot, limit))
        big = sorted(out, key=lambda t: -H.tris(t[0]))
        for o, _ in big:
            if tot <= limit: break
            t = H.tris(o)
            if t < 400: continue
            want = max(0.25, 1.0 - (tot - limit) / t)
            d = o.modifiers.new("dec", "DECIMATE"); d.ratio = want
            for x in H.S().objects: x.select_set(False)
            o.select_set(True); bpy.context.view_layer.objects.active = o; bpy.ops.object.modifier_apply(modifier="dec")
            tot += H.tris(o) - t
    print("HERO budget %d tris" % tot)
    return out

def pts_of(objs, step=1):
    out = []
    for o in objs:
        for v in o.data.vertices[::step]: out.append(tuple(o.matrix_world @ v.co))
    return out

class Hulls:
    """the hull groups of a piece: each a list of objects (as built), the hull is taken of their real vertices"""
    def __init__(self): self.groups = []
    def add(self, *objs): self.groups.append(list(objs)); return objs[0] if len(objs) == 1 else objs
    def box(self, x0, x1, y0, y1, z0, z1): self.groups.append(H.box_pts(x0, x1, y0, y1, z0, z1)); return None
    def freeze(self):
        """called before the parts are evaluated: point clouds of each group"""
        fz = []
        for g in self.groups:
            if g and isinstance(g[0], tuple): fz.append(g); continue
            pts = []
            for o in g:
                dg = bpy.context.evaluated_depsgraph_get(); ev = o.evaluated_get(dg)
                me = bpy.data.meshes.new_from_object(ev, depsgraph=dg)
                for v in me.vertices: pts.append(tuple(o.matrix_world @ v.co))
                bpy.data.meshes.remove(me)
            fz.append(pts)
        self.groups = fz
    def make(self, name):
        return [H.hull("UCX_%s_%02d" % (name, i), g) for i, g in enumerate(self.groups) if len(g) >= 4]

def _copy(o, name):
    c = o.copy(); c.data = o.data.copy(); c.name = name; c.data.name = name; H.link(c); return c

def cuts(cl):
    """break pieces by fracture planes: cl = [(which_piece, co, normal, jag, seed, splinter)]; piece 0 is the whole at start"""
    def f(lod0, name):
        ps = [_copy(lod0, name + "_Brk0")]
        mn, mx = H.bounds([lod0]); size = max(0.25, 2.4 * (mx - mn).length)
        for which, co, no, jag, seed, spl in cl:
            src = ps[which]
            k = len(ps)
            a, b = H.split(src, H.cutter(co, no, size, jag, seed, sub=6, splinter=spl, depth=size), src.name, name + "_Brk%d" % k)
            ps[which] = a; ps.append(b)
        ps = [p for p in ps if len(p.data.polygons) > 8]
        for i, p in enumerate(ps): p.name = name + "_Brk%d" % i; p.data.name = p.name
        return ps
    return f


def groups(gl, snaps=()):
    """break pieces = groups of parts (what comes apart along its joints: a leg, an arm, a board), then fractures through single
    closed parts: snaps = [(piece index, co, normal, jag, seed, splinter)] (a board snaps, a cushion tears)"""
    def f(lod0, name):
        have = set(a.value for a in lod0.data.attributes["part"].data)
        ps = []
        for g in gl:
            g = [p for p in g if p in have]
            if g: ps.append(H.keep_parts(lod0, g, name + "_Brk%d" % len(ps)))
        mn, mx = H.bounds([lod0]); size = max(0.25, 2.4 * (mx - mn).length)
        for which, co, no, jag, seed, spl in snaps:
            if which >= len(ps): continue
            src = ps[which]; n0 = len(src.data.polygons)
            a, b = H.split(src, H.cutter(co, no, size, jag, seed, sub=7, splinter=spl, depth=size), src.name, name + "_Brk%d" % len(ps))
            print("HERO snap %s piece %d: %d faces -> %d + %d" % (name, which, n0, len(a.data.polygons), len(b.data.polygons)))
            ps[which] = a; ps.append(b)
        ps = [p for p in ps if len(p.data.polygons) > 8]
        for i, p in enumerate(ps): p.name = name + "_Brk%d" % i; p.data.name = p.name
        return ps
    return f

_bake_orig = H._bake
def _bake_aa(o, kind, im, samples, **kw):
    """on top of hero_lib.bake_set (metals' colour and 16-sample colour since 42f2c49): roughness and the masks with
    16 samples a texel too (the fine procedural roughness of the weaves and grains otherwise aliases)"""
    return _bake_orig(o, kind, im, max(samples, 16) if kind in ("ROUGHNESS", "EMIT") else samples, **kw)
H._bake = _bake_aa

# ================================================================================================ one piece, start to end
KEEP = ("Screen", "LampShade", "Bulb")      # slots the game drives by name (AntLifeTV, the light switch, the glow looks)

def produce(name, parts, hulls, breaks=None, lods=(0.35, 0.12), tex=2048, collision_lod=0, note="", break_class="wood", uv_angle=60.0):
    t0 = time.time()
    out = os.path.join(OUT, name); os.makedirs(out, exist_ok=True)
    hulls.freeze()
    objs = budget(parts)
    for o, part in objs:
        H.set_part(o, part)
        ca = o.data.color_attributes.get("Col") or o.data.color_attributes.new("Col", "BYTE_COLOR", "POINT")
        for i in range(len(o.data.vertices)): ca.data[i].color = (0, 0, 0, 1)
    lod0 = H.join([o for o, _ in objs], name)
    if H.tris(lod0) > LIMIT:      # (the per-part budget could not get there: collapse the whole, the bar is 15k)
        d = lod0.modifiers.new("dec", "DECIMATE"); d.ratio = LIMIT / H.tris(lod0) * 0.98; d.use_collapse_triangulate = True
        for x in H.S().objects: x.select_set(False)
        lod0.select_set(True); bpy.context.view_layer.objects.active = lod0; bpy.ops.object.modifier_apply(modifier="dec")
        print("HERO %s collapsed to %d tris" % (name, H.tris(lod0)))
    H.unwrap(lod0, uv_angle)
    tdir = os.path.join(out, "tex")
    if NOBAKE and os.path.exists(os.path.join(tdir, "T_%s_C.png" % name)):
        paths = {k: os.path.join(tdir, "T_%s_%s.png" % (name, k)) for k in "CNR"}
    else:
        paths = H.bake_set(lod0, name, tdir, tex)
    # fix14: the owner's palette (his map v4 looks of the old model's slots) -> tex_owner/, MI_<Name>_Owner in the game
    paths_owner = None
    if name in OWNER_SLOTS and os.environ.get("HERO_OWNER_PALETTE", "1") != "0":
        odir = os.path.join(out, "tex_owner")
        if NOBAKE and os.path.exists(os.path.join(odir, "T_%s_C.png" % name)):
            paths_owner = {k: os.path.join(odir, "T_%s_%s.png" % (name, k)) for k in "CNR"}
        else:
            paths_owner = HO.bake_owner(lod0, name, out, tex, OWNER_SLOTS[name])
    # the Hero_ slot, then the driven slots kept by name
    face_mat = [lod0.data.materials[p.material_index].name.split(".")[0] if lod0.data.materials[p.material_index] else "" for p in lod0.data.polygons]
    kept = [k for k in KEEP if k in face_mat]
    H.game_material(lod0, name)
    for k in kept:
        lod0.data.materials.append(bpy.data.materials.get(k) or bpy.data.materials.new(k))
    if kept:
        for p, fm in zip(lod0.data.polygons, face_mat):
            if fm in kept: p.material_index = 1 + kept.index(fm)
    hs = hulls.make(name)
    l1 = H.lod(lod0, lods[0], name + "_LOD1"); l2 = H.lod(lod0, lods[1], name + "_LOD2")
    H.export_fbx([lod0] + hs, os.path.join(out, name + ".fbx"))
    H.export_fbx([l1], os.path.join(out, name + "_LOD1.fbx")); H.export_fbx([l2], os.path.join(out, name + "_LOD2.fbx"))
    man = {"name": name, "file": name + ".fbx", "set": "hero_living", "break_class": break_class,
           "textures": {k: os.path.relpath(v, OUT).replace("\\", "/") for k, v in paths.items()},
           "collision_lod": collision_lod, "movers": [], "breaks": [], "note": note, "kept_slots": kept}
    if paths_owner:
        man["textures_owner"] = {k: os.path.relpath(v, OUT).replace("\\", "/") for k, v in paths_owner.items()}
    man.update(H.info(lod0)); man["lod_triangles"] = [H.tris(lod0), H.tris(l1), H.tris(l2)]; man["ucx"] = len(hs)
    pieces = breaks(lod0, name) if breaks else []
    for pc in pieces:
        H.break_slots(pc)
        H.export_fbx([pc], os.path.join(out, pc.name + ".fbx"))
        man["breaks"].append({"name": pc.name, "file": pc.name + ".fbx", **H.info(pc)})
    for o in hs + [l1, l2]: bpy.data.objects.remove(o)
    print("HERO %s: LOD %s, %d hulls, %d break pieces, %.0f s" % (name, man["lod_triangles"], man["ucx"], len(pieces), time.time() - t0))
    return man, lod0, pieces, paths

# ================================================================================================ review: before | after | ant | wet | char | broken
COLS = {"Sisal": (0.8, 0.66, 0.42), "Plinth": (0.08, 0.08, 0.09), "Walnut": (0.35, 0.2, 0.1), "Butcher": (0.68, 0.40, 0.16), "Blue": (0.1, 0.25, 0.75),
        "Paper": (0.97, 0.96, 0.88), "Steel": (0.7, 0.7, 0.72), "Red": (0.75, 0.1, 0.08), "Fabric": (0.96, 0.72, 0.16), "SofaFabric": (0.09, 0.33, 0.40),
        "Cushion": (0.14, 0.46, 0.50), "Cabinet": (0.42, 0.60, 0.44), "CabinetInset": (0.34, 0.51, 0.37), "Brass": (0.86, 0.62, 0.22), "Enamel": (0.95, 0.94, 0.90),
        "Iron": (0.05, 0.05, 0.05), "Screen": (0.05, 0.17, 0.45), "LampShade": (0.96, 0.86, 0.62), "Bulb": (1, 0.85, 0.55), "Carpet": (0.66, 0.13, 0.12),
        "CarpetTrim": (0.93, 0.85, 0.66), "Chrome": (0.9, 0.9, 0.92), "Teal": (0.05, 0.45, 0.48), "Plaster": (0.9, 0.88, 0.84), "Honey": (0.85, 0.6, 0.2)}

def old_fbx(name):
    for sub in ("living", "kitchen", "level_v2", "level_v3", "level_v31", "level_v32"):
        f = os.path.join(OLD, sub, name + ".fbx")
        if os.path.exists(f): return f
    return None

def import_old(name):
    f = old_fbx(name)
    if not f: return []
    before = set(H.S().objects)
    bpy.ops.import_scene.fbx(filepath=f)
    obs = [o for o in H.S().objects if o not in before and o.type == "MESH" and not o.name.startswith("UCX")]
    for o in [o for o in H.S().objects if o not in before and o not in obs]: bpy.data.objects.remove(o)
    for o in obs:
        for s in o.material_slots:
            if s.material:
                n = s.material.name.split(".")[0]; s.material.use_nodes = True; p = s.material.node_tree.nodes.get("Principled BSDF")
                if p: p.inputs["Base Color"].default_value = (*COLS.get(n, (0.6, 0.6, 0.6)), 1); p.inputs["Roughness"].default_value = 0.6
    return obs

def clear_scene():
    for o in list(H.S().objects): bpy.data.objects.remove(o)

PREV = {}
def review(name, lod0, pieces, paths, cams, extra=None, scene=None, old_extra=None, lift=0.0):
    """cams: {"wide": (loc, look, lens), "ant": (...)}; extra: other objects to show with it (e.g. the cushions on the sofa)"""
    os.makedirs(REV, exist_ok=True)
    prev = PREV.get(name) or H.preview_material(name, paths); PREV[name] = prev
    def put(o, m):
        if o.data.materials: o.data.materials[0] = m
        else: o.data.materials.append(m)
    involved = set([lod0] + list(pieces) + list(extra or []))
    hidden = [o for o in H.S().objects if o not in involved and not o.hide_render]
    for o in hidden: o.hide_render = True
    for x in pieces: x.hide_render = True
    lod0.hide_render = False
    for x in (extra or []): x.hide_render = False
    put(lod0, prev)
    before_stage = set(H.S().objects)
    mats0 = {x: x.matrix_world.copy() for x in pieces}
    if scene is None:
        mn, mx = H.bounds([lod0] + list(extra or [])); scene = {"wall_y": mx.y + 0.003}
    H.stage(**scene)
    shots = []
    def cam(k):
        loc, look, lens = cams[k]; H.camera(loc, look, lens, 9 if k == "ant" else 5.6)
    # after: wide and the ant's view
    for k in ("wide", "ant"):
        cam(k); p = os.path.join(REV, "_%s_%s.jpg" % (name, k)); H.render(p, 1280, 720, 128); shots.append(p)
    # states
    for k, (w, c) in enumerate(((1.0, 0.0), (0.0, 0.5))):
        put(lod0, H.state_material(prev, w, c, 0.2)); cam("wide"); p = os.path.join(REV, "_%s_state%d.jpg" % (name, k)); H.render(p, 1280, 720, 96); shots.append(p)
    put(lod0, prev)
    # broken: the pieces fall apart a little
    lod0.hide_render = True
    for x in (extra or []): x.hide_render = True
    R = random.Random(5); mn0, mx0 = H.bounds([lod0]); c0 = (mn0 + mx0) * 0.5
    for x in pieces:
        x.hide_render = False; put(x, prev)
        mn, mx = H.bounds([x]); c = (mn + mx) * 0.5; d = (c - c0); d.z = 0
        d = d.normalized() * 0.06 * max(0.3, (mx0 - mn0).length) if d.length > 1e-4 else Vector((0, 0, 0))
        x.matrix_world = Matrix.Translation((d.x, d.y, 0)) @ Matrix.Translation(c) @ Matrix.Rotation(R.uniform(-0.12, 0.12), 4, "Z") @ Matrix.Translation(-c)
    cam("wide"); p = os.path.join(REV, "_%s_broken.jpg" % name); H.render(p, 1280, 720, 96); shots.append(p)
    for x, mw in mats0.items(): x.matrix_world = mw; x.hide_render = True
    # before: the old model (and the old parts that go with it), same camera
    old = [] if lift else import_old(name)
    for on, mw in (old_extra or []):
        for o in import_old(on):
            o.matrix_world = Matrix(mw) @ o.matrix_world; old.append(o)
    if old:
        cam("wide"); p = os.path.join(REV, "_%s_before.jpg" % name); H.render(p, 1280, 720, 96)
        for o in old: bpy.data.objects.remove(o)
    else:
        p = shots[0]
    H.sheet([p, shots[0], shots[1], shots[2], shots[3], shots[4]], os.path.join(REV, name + ".jpg"), 2)
    for o in [o for o in H.S().objects if o not in before_stage]: bpy.data.objects.remove(o)
    for o in hidden: o.hide_render = False
    lod0.hide_render = True
    for x in (extra or []): x.hide_render = True
    return shots

def auto_cams(mn, mx, ant_side=-1):
    c = (mn + mx) * 0.5; s = max((mx - mn).length, 0.25)
    e = mx - mn; s2 = max(e.x, e.y, e.z * 1.9, 0.25)     # (tall things: the frame has to take the height)
    wide = ((c.x + 0.45 * s2, mn.y - 1.25 * s2, c.z + 0.22 * s2 + 0.05), (c.x, c.y, c.z), 35)
    zb = max(0.0, mn.z)
    ant = ((c.x - 0.22 * s, mn.y - 0.55 * s - 0.05, zb + 0.014), (c.x + 0.02 * s, c.y, zb + 0.35 * (mx.z - zb)), 22)
    return {"wide": wide, "ant": ant}

# ================================================================================================ the pieces
# --- shared finishes of the flat (one palette: the old set's colours, as real materials)
def F_SOFA():   return M("SofaWeave", fabric, (0.075, 0.27, 0.32), (0.045, 0.18, 0.22), 260.0, 0.7)
def F_CUSH():   return M("CushionWeave", fabric, (0.10, 0.34, 0.38), (0.06, 0.23, 0.26), 260.0, 0.7)
def F_MUSTARD():return M("MustardWeave", fabric, (0.72, 0.50, 0.10), (0.52, 0.33, 0.06), 240.0, 0.5)
def W_WALNUT(axis="Z"): return M("Walnut" + axis, wood, (0.36, 0.21, 0.10), (0.14, 0.07, 0.03), axis, 26.0, 0.45)
def W_OAK(axis="X"):    return M("Oak" + axis, wood, (0.70, 0.47, 0.24), (0.42, 0.25, 0.11), axis, 20.0, 0.35)
def P_GREEN():  return M("GreenLacquer", paint, (0.30, 0.46, 0.33), 0.38, 0.03, 0.35, (0.62, 0.48, 0.30))
def P_WHITE():  return M("WhiteEnamel", paint, (0.86, 0.85, 0.80), 0.30, 0.05, 0.15, (0.70, 0.68, 0.62))
def BRASS():    return M("BrassOld", lambda n: H.metal(n, (0.78, 0.56, 0.25), 0.34))
def BLACKP():   return M("BlackPlastic", lambda n: H.plastic(n, (0.035, 0.035, 0.04), 0.42, 0.03, 0.25))
def DUSTCOVER():return M("Cambric", fabric, (0.03, 0.03, 0.03), (0.02, 0.02, 0.02), 500.0, 0.1)

def legs4(xs, ys, h, top_r=0.026, bot_r=0.018, wood_m=None, cap=True, hs=None):
    """tapered turned legs with a brass cup and a felt glide"""
    out = []
    for x in xs:
        for y in ys:
            o = lathe("Leg", [(0, 0.012), (bot_r, 0.012), (bot_r + 0.0015, 0.03), (top_r * 0.92, h - 0.02), (top_r, h - 0.012), (top_r, h), (0, h)], wood_m or W_WALNUT("Z"), (x, y, 0), 24)
            out.append(o)
            if cap:
                c = lathe("Cup", [(0, 0.004), (bot_r + 0.002, 0.004), (bot_r + 0.0025, 0.03), (bot_r + 0.0005, 0.032), (0, 0.032)], BRASS(), (x, y, 0), 24); out.append(c)
            g = cyl("Glide", bot_r * 0.95, 0.006, (x, y, 0.003 + 0.002), M("Felt", fabric, (0.30, 0.28, 0.25), None, 900.0, 0.2), "Z", 16); out.append(g)
            if hs: hs.add(o)
    return out

def build_sofa():
    """3-seat sofa, the old frame: 3.89 x 0.89 x 1.105 m, base top 0.368, arm tops 0.711, back to the +Y side"""
    W, D, Hh = 7400 / S, 1700 / S, 2100 / S
    hw, hd, ax = W / 2, D / 2, 3300 / S
    zb0, zb1, za = 260 / S, 700 / S, 1350 / S
    hs = Hulls(); parts = []
    fab = F_SOFA()
    for o in legs4((-2900 / S, 2900 / S), (-750 / S, 750 / S), zb0, hs=hs): parts.append((o, 1))
    # base box (sprung platform), its front a little rolled
    base = soft("Base", -ax, ax, -hd, hd, zb0, zb1, fab, 0.035, 3, 0.0, 1); parts.append((base, 0)); hs.add(base)
    # rolled arms: a box up to the roll, the roll on top (with piped round fronts)
    r = (hw - ax) / 2 + 0.006
    for s in (-1, 1):
        xc = s * (ax + hw) / 2
        arm = soft("Arm", min(s * ax, s * hw) + 0.0, max(s * ax, s * hw), -hd + 0.004, hd, zb0, za - r, fab, 0.03, 3, 0.0, 1); parts.append((arm, 2 + (s > 0)))
        roll = cyl("Roll", r, D - 0.004, (xc, 0.002, za - r), fab, "Y", 40); parts.append((roll, 2 + (s > 0))); hs.add(arm, roll)
        ring = [(xc + r * math.cos(2 * math.pi * k / 40), -hd + 0.002, za - r + r * math.sin(2 * math.pi * k / 40)) for k in range(40)]
        ring = [p for p in ring if p[2] > za - r - 0.001] + [(xc + s * 0.0 - r, -hd + 0.002, zb0 + 0.02), (xc + r, -hd + 0.002, zb0 + 0.02)]
        pts = [(xc + r * math.cos(math.pi * k / 24), -hd + 0.001, za - r + r * math.sin(math.pi * k / 24)) for k in range(25)]
        pts = [(xc + r, -hd + 0.001, zb0 + 0.025)] + pts + [(xc - r, -hd + 0.001, zb0 + 0.025)]
        parts.append((pipe("ArmPiping", [pts], 0.0042, fab), 2 + (s > 0)))
        # the arm's front panel (a separate upholstered face, inset 4 mm)
        fp = soft("ArmFront", xc - r + 0.012, xc + r - 0.012, -hd - 0.006, -hd + 0.01, zb0 + 0.03, za - r, fab, 0.012, 2, 0.0, 0); parts.append((fp, 2 + (s > 0)))
    # back
    back = soft("Back", -ax, ax, hd - 0.15, hd, zb1 - 0.03, Hh - 0.03, fab, 0.04, 3, 0.0, 1); parts.append((back, 4)); hs.add(back)
    top = cyl("BackRoll", 0.075, 2 * ax, (0, hd - 0.075, Hh - 0.075), fab, "X", 40); parts.append((top, 4))
    parts.append((pipe("BackPiping", [[(-ax, hd - 0.15, Hh - 0.075), (ax, hd - 0.15, Hh - 0.075)]], 0.0042, fab), 4))
    # piping round the platform, the deck seam
    parts.append((pipe("BasePiping", [rect_loop(-ax + 0.004, ax - 0.004, -hd - 0.0, hd - 0.15, zb1 - 0.004, 0.03)], 0.0045, fab, True), 0))
    parts.append((pipe("FrontSeam", [[(-ax + 0.02, -hd - 0.001, zb0 + 0.06), (ax - 0.02, -hd - 0.001, zb0 + 0.06)]], 0.0035, fab), 0))
    th = THREAD()
    parts.append((stitches("BaseStitch", [(-ax + 0.03, -hd - 0.0015, zb1 - 0.022), (ax - 0.03, -hd - 0.0015, zb1 - 0.022)], th), 0))
    for s in (-1, 1):
        xc = s * (ax + hw) / 2
        parts.append((stitches("ArmStitch", [(xc - r + 0.02, -hd - 0.0075, zb0 + 0.05), (xc - r + 0.02, -hd - 0.0075, za - r)], th), 2 + (s > 0)))
        parts.append((stitches("ArmStitch", [(xc + r - 0.02, -hd - 0.0075, zb0 + 0.05), (xc + r - 0.02, -hd - 0.0075, za - r)], th), 2 + (s > 0)))
        parts.append((stitches("RollStitch", [(xc, -hd + 0.01, za + 0.001), (xc, hd - 0.01, za + 0.001)], th), 2 + (s > 0)))
    parts.append((stitches("BackStitch", [(-ax + 0.03, hd - 0.152, Hh - 0.03), (ax - 0.03, hd - 0.152, Hh - 0.03)], th), 4))
    # show-wood rails under the base, the dust cover
    for y in (-hd + 0.045, hd - 0.045):
        parts.append((box("Rail", -ax + 0.05, ax - 0.05, y - 0.016, y + 0.016, zb0 - 0.024, zb0 + 0.004, W_WALNUT("X"), 0.004, 3), 0))
    parts.append((box("DustCover", -ax + 0.02, ax - 0.02, -hd + 0.02, hd - 0.02, zb0 - 0.003, zb0 + 0.002, DUSTCOVER(), 0, 0), 0))
    # back legs' brackets screwed into the rail, staples along the cover (what the ant sees from below)
    st = []
    for k in range(34):
        x = -ax + 0.06 + k * (2 * ax - 0.12) / 33
        for y in (-hd + 0.022, hd - 0.022):
            st.append(box("Staple", x - 0.006, x + 0.006, y - 0.0007, y + 0.0007, zb0 - 0.0045, zb0 - 0.003, M("Zinc", H.metal, (0.62, 0.63, 0.64), 0.35), 0, 0))
    parts.append((H.join(st, "Staples"), 0))
    return parts, hs

def build_seat_cushion():
    """loose seat cushion 1.026 x 0.663 x 0.132 m: boxed foam in the woven cover, piped top and bottom, a zip at the back"""
    cw, cd, ch = 1960 / S - 0.006, 1260 / S, 250 / S
    hs = Hulls(); parts = []; fab = F_CUSH()
    c = soft("Cushion", -cw / 2, cw / 2, -cd / 2, cd / 2, 0.002, ch - 0.004, fab, 0.05, 4, 0.55, 2); parts.append((c, 0)); hs.add(c)
    for z in (0.012, ch - 0.014):
        parts.append((pipe("Piping", [rect_loop(-cw / 2 + 0.006, cw / 2 - 0.006, -cd / 2 + 0.006, cd / 2 - 0.006, z, 0.05, 6)], 0.0062, fab, True), 1))
    zip_ = box("Zip", -cw / 2 + 0.08, cw / 2 - 0.08, cd / 2 - 0.006, cd / 2 + 0.002, ch / 2 - 0.006, ch / 2 + 0.006, BLACKP(), 0.002, 2); parts.append((zip_, 1))
    pull = box("ZipPull", cw / 2 - 0.10, cw / 2 - 0.075, cd / 2, cd / 2 + 0.005, ch / 2 - 0.004, ch / 2 + 0.004, M("Zinc", H.metal, (0.62, 0.63, 0.64), 0.35), 0.001, 2); parts.append((pull, 1))
    return parts, hs

def build_back_cushion():
    """back cushion 1.026 x 0.105 x 0.66 m (front -Y): tufted with three covered buttons"""
    cw, bt, bh = 1960 / S - 0.006, 0.09, 0.66
    hs = Hulls(); parts = []; fab = F_CUSH()
    c = soft("Cushion", -cw / 2, cw / 2, -bt / 2, bt / 2, 0.004, bh - 0.004, fab, 0.04, 4, 0.5, 2); parts.append((c, 0)); hs.add(c)
    for y in (-bt / 2 + 0.008, bt / 2 - 0.008):
        parts.append((pipe("Piping", [rot_xz(rect_loop(-cw / 2 + 0.006, cw / 2 - 0.006, 0.01, bh - 0.01, y, 0.045, 6))], 0.0048, fab, True), 1))
    for xb in (-0.28, 0.0, 0.28):
        b = lathe("Button", [(0, 0.0), (0.016, 0.0), (0.018, 0.004), (0.014, 0.009), (0, 0.010)], fab, (xb, -bt / 2 - 0.006, bh * 0.6), 20, 0, (math.pi / 2, 0, 0))
        parts.append((b, 1))
        # the dimple the button pulls into the face
        for k in range(6):
            a = k * math.pi / 3
            parts.append((pipe("Pleat", [[(xb + 0.02 * math.cos(a), -bt / 2 - 0.004, bh * 0.6 + 0.02 * math.sin(a)), (xb + 0.06 * math.cos(a), -bt / 2 - 0.007, bh * 0.6 + 0.06 * math.sin(a))]], 0.0022, fab), 1))
    return parts, hs

def lace(name):
    """crocheted doily: off-white cotton with an open mesh (voronoi cells read dark where the holes are)"""
    m, p = H.mat(name)
    tc = N(m, "ShaderNodeTexCoord", -1000); vo = N(m, "ShaderNodeTexVoronoi", -800); vo.inputs["Scale"].default_value = 160
    vo.feature = "DISTANCE_TO_EDGE"; L(m, tc.outputs["Object"], vo.inputs["Vector"])
    mr = N(m, "ShaderNodeMapRange", -600); mr.inputs["From Min"].default_value = 0.04; mr.inputs["From Max"].default_value = 0.12; L(m, vo.outputs["Distance"], mr.inputs["Value"])
    cr = N(m, "ShaderNodeMix", -400); cr.data_type = "RGBA"; cr.inputs[6].default_value = (0.93, 0.90, 0.82, 1); cr.inputs[7].default_value = (0.30, 0.20, 0.06, 1)
    L(m, mr.outputs["Result"], cr.inputs[0]); L(m, cr.outputs[2], p.inputs["Base Color"])
    bp = N(m, "ShaderNodeBump", -300, -300); bp.inputs["Strength"].default_value = 0.8; bp.inputs["Distance"].default_value = 0.0008; bp.invert = True
    L(m, mr.outputs["Result"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"]); p.inputs["Roughness"].default_value = 0.9
    return H.tag(m, 0.0, 1.0)

def build_armchair():
    """armchair 1.158 x 0.895 x 1.0 m (back +Y): seat top 0.421, arm tops 0.632, on 60 mm legs"""
    W, D, Hh = 2200 / S, 1700 / S, 1900 / S
    hw, hd, at, zl = W / 2, D / 2, 200 / S, 0.06
    hs = Hulls(); parts = []; fab = F_MUSTARD()
    for o in legs4((-(hw - 0.07), hw - 0.07), (-(hd - 0.07), hd - 0.07), zl + 0.01, 0.022, 0.016, hs=hs): parts.append((o, 1))
    seat = soft("SeatBox", -(hw - at), hw - at, -hd, hd - at, zl, 650 / S, fab, 0.03, 3, 0.0, 1); parts.append((seat, 0)); hs.add(seat)
    for s in (-1, 1):
        a = soft("Arm", min(s * hw, s * (hw - at)), max(s * hw, s * (hw - at)), -hd, hd, zl, 1200 / S, fab, 0.045, 4, 0.08, 1); parts.append((a, 2 + (s > 0))); hs.add(a)
        x0, x1 = min(s * hw, s * (hw - at)), max(s * hw, s * (hw - at))
        parts.append((pipe("ArmPiping", [[(x0 + 0.01, -hd - 0.002, zl + 0.02), (x0 + 0.01, -hd - 0.002, 1200 / S - 0.012), (x0 + 0.01, hd - 0.02, 1200 / S - 0.003)],
                                           [(x1 - 0.01, -hd - 0.002, zl + 0.02), (x1 - 0.01, -hd - 0.002, 1200 / S - 0.012), (x1 - 0.01, hd - 0.02, 1200 / S - 0.003)]], 0.0042, fab), 2 + (s > 0)))
    b = soft("Back", -hw, hw, hd - at, hd, zl, Hh - 0.008, fab, 0.045, 4, 0.06, 1); parts.append((b, 4)); hs.add(b)
    cu = soft("SeatCushion", -(hw - at) + 0.012, hw - at - 0.012, -hd + 0.012, hd - at - 0.008, 650 / S - 0.012, 800 / S, fab, 0.03, 4, 0.25, 2); parts.append((cu, 5))
    parts.append((pipe("CushPiping", [rect_loop(-(hw - at) + 0.018, hw - at - 0.018, -hd + 0.016, hd - at - 0.012, 800 / S - 0.012, 0.04, 6)], 0.0045, fab, True), 5))
    bc = soft("BackCushion", -(hw - at) + 0.03, hw - at - 0.03, hd - at - 0.05, hd - at + 0.01, 800 / S - 0.02, Hh - 0.07, fab, 0.03, 4, 0.22, 2); parts.append((bc, 6))
    hs.add(cu, bc)
    th = THREAD()
    for xb in (-0.2, 0.0, 0.2):
        for zb_ in (0.65, 0.82):
            parts.append((lathe("Button", [(0, 0.0), (0.014, 0.0), (0.016, 0.004), (0.012, 0.008), (0, 0.009)], fab, (xb, hd - at - 0.052, zb_), 20, 0, (math.pi / 2, 0, 0)), 6))
    for s in (-1, 1):
        x0, x1 = min(s * hw, s * (hw - at)), max(s * hw, s * (hw - at))
        parts.append((stitches("ArmTop", [((x0 + x1) / 2, -hd + 0.01, 1200 / S + 0.001), ((x0 + x1) / 2, hd - 0.02, 1200 / S + 0.001)], th), 2 + (s > 0)))
    parts.append((stitches("CushStitch", [(-(hw - at) + 0.03, -hd + 0.009, 800 / S - 0.025), (hw - at - 0.03, -hd + 0.009, 800 / S - 0.025)], th), 5))
    lc = M("Doily", lace)
    d1 = box("DoilyTop", -0.21, 0.21, hd - at, hd, Hh - 0.0005, Hh + 0.0015, lc, 0.0008, 1); d2 = box("DoilyFlap", -0.21, 0.21, hd - at - 0.0035, hd - at - 0.0015, Hh - 0.11, Hh, lc, 0.0008, 1)
    parts += [(d1, 4), (d2, 4)]
    parts.append((box("DustCover", -hw + 0.02, hw - 0.02, -hd + 0.02, hd - 0.02, zl - 0.002, zl + 0.002, DUSTCOVER(), 0, 0), 0))
    return parts, hs

def build_coffee_table():
    """coffee table 1.316 x 0.842 m, top 0.4737..0.5158, shelf top 0.179: solid oak top, walnut legs and aprons, ring marks"""
    W, D, Hh = 2500 / S, 1600 / S, 980 / S
    hw, hd, zt = W / 2, D / 2, 900 / S
    hs = Hulls(); parts = []
    top_m = M("OakTop", wood, (0.72, 0.49, 0.25), (0.44, 0.26, 0.11), "X", 18.0, 0.5, 0.5, 0.35)
    rings = top_m.node_tree; # (coffee rings are added below as a darker ring on the top: a separate thin decal)
    top = box("Top", -hw, hw, -hd, hd, zt, Hh, top_m, 0.008, 4); parts.append((top, 10)); hs.add(top)
    seam = M("Seam", H.rubber, (0.10, 0.06, 0.03), 0.8)
    for y in (-hd * 0.5, 0.0, hd * 0.5):   # board seams (the top is glued up of four boards): thin dark glue lines, top and edges
        parts.append((box("Seam", -hw + 0.004, hw - 0.004, y - 0.0006, y + 0.0006, Hh - 0.0004, Hh + 0.0002, seam, 0.0, 0), 10))
        for s in (-1, 1): parts.append((box("SeamEnd", s * hw - 0.0004, s * hw + 0.0002, y - 0.0006, y + 0.0006, zt + 0.003, Hh - 0.003, seam, 0.0, 0), 10))
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * (hw - 100 / S), sy * (hd - 100 / S)
            lg = lathe("Leg", [(0, 0.004), (0.020, 0.004), (0.022, 0.02), (0.031, zt - 0.06), (0.033, zt - 0.055), (0.033, zt), (0, zt)], W_WALNUT("Z"), (x, y, 0), 24)
            g = cyl("Glide", 0.019, 0.005, (x, y, 0.0025), M("Felt", fabric, (0.30, 0.28, 0.25), None, 900.0, 0.2), "Z", 16)
            k = 11 + (sx > 0) * 2 + (sy > 0); parts += [(lg, k), (g, k)]; hs.add(lg)
    for s in (-1, 1):
        y = s * (hd - 0.065)
        parts.append((box("ApronX", -(hw - 0.07), hw - 0.07, y - 0.009, y + 0.009, zt - 0.06, zt, W_WALNUT("X"), 0.002, 2), 15))
        x = s * (hw - 0.065)
        parts.append((box("ApronY", x - 0.009, x + 0.009, -(hd - 0.07), hd - 0.07, zt - 0.06, zt, W_WALNUT("Y"), 0.002, 2), 15))
        # corner blocks with their screws
        for t in (-1, 1):
            parts.append((box("Block", min(s * (hw - 0.12), s * (hw - 0.08)), max(s * (hw - 0.12), s * (hw - 0.08)), min(t * (hd - 0.12), t * (hd - 0.08)), max(t * (hd - 0.12), t * (hd - 0.08)), zt - 0.035, zt, W_WALNUT("Z"), 0.002, 1), 15))
            parts += [(o, 15) for o in screws_at("BlockScrew", [(s * (hw - 0.10), t * (hd - 0.10), zt - 0.0352, -1)], "Z", steel(), 0.004)]
    sh = box("Shelf", -(hw - 50 / S), hw - 50 / S, -(hd - 50 / S), hd - 50 / S, 300 / S, 340 / S, top_m, 0.005, 3); parts.append((sh, 16)); hs.add(sh)
    # coffee rings and a scorch from a hot mug: thin darker discs flush in the top (the ant reads them as stains)
    stain = M("Stain", lambda n: H.plastic(n, (0.30, 0.17, 0.07), 0.25, 0.02, 0.1))
    for (x, y, r) in ((0.32, 0.12, 0.038), (0.36, 0.16, 0.038), (-0.41, -0.22, 0.042)):
        rr = lathe("Ring", [(r - 0.004, Hh + 0.0002), (r, Hh + 0.0002), (r, Hh + 0.0004), (r - 0.004, Hh + 0.0004)], stain, (x, y, 0), 40)
        parts.append((rr, 10))
    return parts, hs

def build_tvstand():
    """TV stand 2.895 x 0.684 x 0.579 m (back +Y): walnut carcass, oak top, left bay two sliding doors (own meshes), right bay open"""
    W, D, Hh = 5500 / S, 1300 / S, 1100 / S
    hw, hd, t = W / 2, D / 2, 0.03
    zf = 0.02; zb, zt = zf + 0.03, Hh - 0.028
    hs = Hulls(); parts = []
    wal = W_WALNUT("X"); walz = W_WALNUT("Z"); oak = W_OAK("X")
    for x in (-hw + 0.06, 0.0, hw - 0.06):
        for y in (-hd + 0.06, hd - 0.06):
            parts.append((lathe("Foot", [(0, 0), (0.026, 0), (0.028, 0.003), (0.024, zf + 0.004), (0, zf + 0.004)], BLACKP(), (x, y, 0), 20), 20))
    bot = box("Bottom", -hw, hw, -hd, hd, zf, zb, wal, 0.003, 2); parts.append((bot, 20)); hs.add(bot)
    top = box("Top", -hw, hw, -hd, hd, zt, Hh, oak, 0.006, 4); parts.append((top, 21)); hs.add(top)
    for s in (-1, 1):
        sd = box("Side", min(s * hw, s * (hw - t)), max(s * hw, s * (hw - t)), -hd + 0.004, hd, zb - 0.002, zt + 0.002, walz, 0.003, 2); parts.append((sd, 22 + (s > 0))); hs.add(sd)
    dv = box("Divider", -t / 2, t / 2, -hd + 0.058, hd, zb - 0.002, zt + 0.002, walz, 0.003, 2); parts.append((dv, 24)); hs.add(dv)
    back = box("Back", -hw + 0.002, hw - 0.002, hd - 0.012, hd, zb - 0.002, zt + 0.002, M("HDF", wood, (0.55, 0.40, 0.25), (0.40, 0.28, 0.17), "Z", 60.0, 0.0, 0.2, 0.0, 0.7), 0, 0)
    for (x, z, r) in ((hw / 2, zb + 0.34, 0.035), (-hw / 2, zb + 0.34, 0.035)):
        c = cyl("Hole", r, 0.1, (x, hd - 0.006, z), BLACKP(), "Y", 24)
        md = back.modifiers.new("hole", "BOOLEAN"); md.object = c; md.operation = "DIFFERENCE"
        for x2 in H.S().objects: x2.select_set(False)
        back.select_set(True); bpy.context.view_layer.objects.active = back; bpy.ops.object.modifier_apply(modifier="hole"); bpy.data.objects.remove(c)
        # plastic grommet in the hole
        parts.append((lathe("Grommet", [(r - 0.004, -0.014), (r + 0.006, -0.014), (r + 0.006, -0.012), (r, -0.012), (r, 0.0), (r - 0.004, 0.0)], BLACKP(), (x, hd - 0.012, z), 32, 0, (-math.pi / 2, 0, 0)), 25))
    parts.append((back, 25)); hs.add(back)
    nl = M("Nail", H.metal, (0.45, 0.45, 0.47), 0.4)
    for k in range(18):     # the back panel's nails along the bottom, top and the divider
        x = -hw + 0.03 + k * (2 * hw - 0.06) / 17
        for z in (zb + 0.008, zt - 0.006):
            parts.append((cyl("Nail", 0.0018, 0.0008, (x, hd + 0.0003, z), nl, "Y", 8), 25))
    parts.append((box("Shelf", t / 2 - 0.002, hw - t + 0.002, -hd + 0.06, hd - 0.012, 0.29, 0.31, oak, 0.003, 2), 26))
    for k, x in enumerate((t / 2 + 0.05, hw - t - 0.05)):    # shelf pins
        for y in (-hd + 0.10, hd - 0.06):
            parts.append((cyl("Pin", 0.003, 0.012, (x + (0.006 if k == 0 else -0.006) * 0, y, 0.287), BRASS(), "Z", 10), 26))
    for z in (zb, zt - 0.006):     # aluminium tracks of the sliding doors
        alu = M("Alu", H.metal, (0.74, 0.75, 0.77), 0.38)
        parts.append((box("Track", -hw + t, -t / 2, -hd + 0.020, -hd + 0.058, z, z + 0.006, alu, 0.0008, 1), 20))
        for y in (-hd + 0.030, -hd + 0.048):
            parts.append((box("Groove", -hw + t, -t / 2, y - 0.0015, y + 0.0015, z + 0.006, z + 0.0075, BLACKP(), 0, 0), 20))
    # screws of the tracks, cam-lock covers on the sides (what you see inside the bay)
    sc = steel()
    for x in (-hw + 0.1, -hw / 2, -0.1):
        parts += [(o, 20) for o in screws_at("TrackScrew", [(x, -hd + 0.039, zb + 0.0062, 1)], "Z", sc, 0.0035)]
    for s in (-1, 1):
        for y in (-hd + 0.08, hd - 0.08):
            for z in (zb + 0.03, zt - 0.03):
                parts.append((cyl("CamCap", 0.008, 0.0015, (s * (hw - t - 0.0007), y, z), M("CapBrown", lambda n: H.plastic(n, (0.22, 0.13, 0.07), 0.5, 0.02, 0.1)), "X", 16), 22 + (s > 0)))
    return parts, hs

def build_tvstand_door():
    """sliding door 0.713 x 0.016 x 0.493 m, origin back-bottom-centre: green lacquered frame-and-panel, brass pull"""
    W, D, Hh = 5500 / S, 1300 / S, 1100 / S
    hw, t = W / 2, 0.03
    zb, zt = 0.05, Hh - 0.028
    dw = (hw - t - t / 2) / 2 + 0.012; dh = (zt - 0.004) - (zb + 0.004)
    hs = Hulls(); parts = []; g = P_GREEN()
    st = 0.055
    for (x0, x1, z0, z1) in ((-dw / 2, -dw / 2 + st, 0, dh), (dw / 2 - st, dw / 2, 0, dh), (-dw / 2 + st - 0.001, dw / 2 - st + 0.001, 0, st), (-dw / 2 + st - 0.001, dw / 2 - st + 0.001, dh - st, dh)):
        parts.append((box("Frame", x0, x1, -0.016, 0, z0, z1, g, 0.003, 3), 0))
    pan = box("Panel", -dw / 2 + st, dw / 2 - st, -0.012, -0.004, st, dh - st, g, 0.002, 2); parts.append((pan, 1))
    # the moulding round the panel
    parts.append((pipe("Bead", [rot_xz(rect_loop(-dw / 2 + st + 0.002, dw / 2 - st - 0.002, st + 0.002, dh - st - 0.002, -0.0125))], 0.0028, g, True), 0))
    b = BRASS()
    parts.append((box("Pull", -0.009, 0.009, -0.020, -0.012, dh / 2 - 0.075, dh / 2 + 0.075, b, 0.003, 3), 0))
    parts.append((box("PullRecess", -0.006, 0.006, -0.0215, -0.019, dh / 2 - 0.06, dh / 2 + 0.06, BLACKP(), 0.001, 1), 0))
    parts += [(o, 0) for o in screws_at("PullScrew", [(0, -0.0202, dh / 2 + 0.067, -1), (0, -0.0202, dh / 2 - 0.067, -1)], "Y", b, 0.0025)]
    for x in (-dw / 2 + 0.05, dw / 2 - 0.05):       # nylon rollers at the bottom edge
        parts.append((cyl("Roller", 0.008, 0.006, (x, -0.008, 0.002), M("Nylon", lambda n: H.plastic(n, (0.92, 0.9, 0.85), 0.5, 0.02, 0.1)), "Y", 16), 0))
    hs.box(-dw / 2, dw / 2, -0.016, 0, 0, dh)
    return parts, hs

def build_tv():
    """flat TV 2.05 x 0.21 x 1.158 m on its pedestal (front -Y): the Screen slot stays (AntLifeTV plays the channels on it)"""
    TW, TH, Dp = 3900 / S, 2200 / S, 400 / S
    yp = Dp / 2 - 75 / S; z0 = 0.085
    hs = Hulls(); parts = []
    bp = BLACKP(); gun = M("Gunmetal", H.metal, (0.20, 0.20, 0.22), 0.35)
    base = box("Base", -0.28, 0.28, -Dp / 2, Dp / 2, 0.0, 0.018, gun, 0.006, 4); parts.append((base, 0)); hs.add(base)
    neck = box("Neck", -0.07, 0.07, yp - 0.010, yp + 0.035, 0.016, 0.115, gun, 0.008, 3); parts.append((neck, 0))
    body = box("Bezel", -TW / 2, TW / 2, yp - 0.034, yp + 0.010, z0, TH, bp, 0.006, 4); parts.append((body, 1)); hs.add(body, neck)
    hump = box("Back", -TW * 0.3, TW * 0.3, yp + 0.005, yp + 0.040, z0 + 0.08, z0 + (TH - z0) * 0.62, bp, 0.02, 4); parts.append((hump, 2)); hs.add(hump)
    for k in range(12):      # vent slots on the back hump
        x = -0.30 + k * 0.055
        parts.append((box("Vent", x - 0.012, x + 0.012, yp + 0.0395, yp + 0.0415, z0 + 0.50, z0 + 0.58, M("VentDark", H.rubber, (0.01, 0.01, 0.012), 0.8), 0.001, 1), 2))
    scr = box("Screen", -TW / 2 + 0.012, TW / 2 - 0.012, yp - 0.0365, yp - 0.0335, z0 + 0.03, TH - 0.012, glossy("Screen", (0.012, 0.014, 0.018), 0.05, 0.6), 0, 0)
    parts.append((scr, 3))
    parts.append((box("Logo", -0.03, 0.03, yp - 0.0352, yp - 0.0340, z0 + 0.012, z0 + 0.020, chrome(), 0.0006, 1), 1))
    parts.append((cyl("Led", 0.003, 0.003, (TW / 2 - 0.06, yp - 0.035, z0 + 0.016), M("Led", emissive, (1.0, 0.08, 0.04), 3.0), "Y", 10), 1))
    for k, x in enumerate((-0.15, -0.10, -0.05)):    # ports on the back, the power cord leaving
        parts.append((box("Port", x - 0.008, x + 0.008, yp + 0.039, yp + 0.042, z0 + 0.15, z0 + 0.16, gun, 0.0008, 1), 2))
    parts.append((pipe("Cord", [[(0.10, yp + 0.036, z0 + 0.16), (0.12, yp + 0.036, z0 + 0.10), (0.14, yp + 0.036, 0.03), (0.15, yp + 0.036, 0.022)]], 0.0035, bp), 2))
    return parts, hs


def build_bookshelf():
    """bookshelf 2.737 x 0.526 x 2.4526 m (back +Y): walnut carcass, oak shelves (tops at k*0.4737+0.0316), the books are props"""
    W, D, Hh = 5200 / S, 1000 / S, 4660 / S
    hw, hd, st = W / 2, D / 2, 100 / S
    ihw, yb = hw - st, hd - 0.02
    hs = Hulls(); parts = []
    walz = W_WALNUT("Z"); oak = W_OAK("X")
    back = box("Back", -hw + 0.004, hw - 0.004, yb, hd, 0, 4600 / S, M("HDF", wood, (0.55, 0.40, 0.25), (0.40, 0.28, 0.17), "Z", 60.0, 0.0, 0.2, 0.0, 0.7), 0.001, 1)
    parts.append((back, 3)); hs.add(back)
    for s in (-1, 1):
        sd = box("Side", min(s * hw, s * ihw), max(s * hw, s * ihw), -hd, hd, 0, 4600 / S, walz, 0.004, 3); parts.append((sd, 1 + (s > 0))); hs.add(sd)
        # shelf-pin holes up the inside of the side (two rows), a kick plate at the bottom
        for k in range(40):
            z = 0.06 + k * 0.058
            for y in (-hd + 0.05, hd - 0.07):
                parts.append((cyl("PinHole", 0.0028, 0.002, (s * (ihw + 0.0005), y, z), M("Hole", H.rubber, (0.02, 0.015, 0.01), 0.9), "X", 8), 1 + (s > 0)))
    dv = box("Divider", -40 / S, 40 / S, -hd + 0.004, yb, 0, 4600 / S, walz, 0.003, 3); parts.append((dv, 4)); hs.add(dv)
    for k in range(5):
        z = k * 900 / S
        sh = box("Shelf", -ihw - 0.002, ihw + 0.002, -hd + 0.002, yb + 0.002, z, z + 60 / S, oak, 0.004, 3); parts.append((sh, 5 + k)); hs.add(sh)
        if k:     # a front lipping strip, the brass pins under the shelf
            parts.append((box("Lip", -ihw, ihw, -hd - 0.001, -hd + 0.004, z, z + 60 / S, walz, 0.0015, 2), 5 + k))
            for x in (-ihw + 0.02, -40 / S - 0.02, 40 / S + 0.02, ihw - 0.02):
                for y in (-hd + 0.05, hd - 0.07):
                    parts.append((cyl("Pin", 0.003, 0.010, (x, y, z - 0.004), BRASS(), "X", 8), 5 + k))
    top = box("Top", -hw, hw, -hd, hd, 4600 / S, Hh, oak, 0.006, 4); parts.append((top, 10)); hs.add(top)
    nl = M("Nail", H.metal, (0.45, 0.45, 0.47), 0.4)
    for k in range(5):
        for j in range(14):
            parts.append((cyl("Nail", 0.0018, 0.0008, (-hw + 0.03 + j * (2 * hw - 0.06) / 13, hd + 0.0003, k * 900 / S + 0.016), nl, "Y", 8), 3))
    cap = M("CapBrown", lambda n: H.plastic(n, (0.22, 0.13, 0.07), 0.5, 0.02, 0.1))
    for s in (-1, 1):
        for k in range(1, 5):
            for y in (-hd + 0.05, hd - 0.07):
                parts.append((cyl("CamCap", 0.007, 0.0012, (s * (ihw - 0.0006), y, k * 900 / S + 0.016), cap, "X", 14), 1 + (s > 0)))
    # the anti-tip strap to the wall, two screws
    parts.append((box("Strap", -0.03, 0.03, hd - 0.001, hd + 0.0, 2.35, 2.40, steel(), 0.0005, 1), 3))
    # shelf wear: a pale rub where books slide, rings of a vase on the top
    return parts, hs

def build_side_table():
    """round tripod side table, top 0.526 m (r 0.21), shelf top 0.174 m (r 0.174): walnut, turned splayed legs"""
    hs = Hulls(); parts = []
    walz = W_WALNUT("Z"); top_m = M("WalnutTop", wood, (0.40, 0.24, 0.12), (0.16, 0.08, 0.035), "X", 22.0, 0.55, 0.5, 0.3)
    r0, r1 = 400 / S, 330 / S
    top = lathe("Top", [(0, 940 / S), (r0 - 0.004, 940 / S), (r0, 940 / S + 0.004), (r0 + 0.001, 980 / S), (r0, 1000 / S - 0.004), (r0 - 0.005, 1000 / S), (0, 1000 / S)], top_m, (0, 0, 0), 64)
    parts.append((top, 1)); hs.add(top)
    sh = lathe("Shelf", [(0, 300 / S), (r1 - 0.003, 300 / S), (r1, 300 / S + 0.003), (r1, 330 / S - 0.003), (r1 - 0.003, 330 / S), (0, 330 / S)], top_m, (0, 0, 0), 56)
    parts.append((sh, 2)); hs.add(sh)
    for li, a in enumerate((90, 210, 330)):
        c, s = math.cos(math.radians(a)), math.sin(math.radians(a))
        p0, p1 = Vector((c * 350 / S, s * 350 / S, 0)), Vector((c * 310 / S, s * 310 / S, 960 / S))
        ax = (p1 - p0); ln = ax.length; ax.normalize()
        lg = lathe("Leg", [(0, 0.004), (0.0095, 0.004), (0.010, 0.02), (0.0135, ln * 0.55), (0.012, ln - 0.03), (0.0125, ln), (0, ln)], walz, (0, 0, 0), 20)
        lg.rotation_mode = "QUATERNION"; lg.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(ax); lg.location = p0
        g = cyl("Glide", 0.010, 0.004, tuple(p0 + Vector((0, 0, 0.002))), M("Felt", fabric, (0.30, 0.28, 0.25), None, 900.0, 0.2), "Z", 12)
        parts += [(lg, 3 + li), (g, 3 + li)]; hs.add(lg)
    # a water ring on the top
    stain = M("Stain", lambda n: H.plastic(n, (0.30, 0.17, 0.07), 0.25, 0.02, 0.1))
    parts.append((lathe("Ring", [(0.036, 1000 / S + 0.0002), (0.04, 1000 / S + 0.0002), (0.04, 1000 / S + 0.0004), (0.036, 1000 / S + 0.0004)], stain, (0.05, -0.06, 0), 40), 1))
    return parts, hs

def build_floor_lamp():
    """floor lamp 2.21 m: weighted iron base, pole with a brass coupling, linen cone shade (LampShade slot), bulb (Bulb slot), pull chain"""
    hs = Hulls(); parts = []
    iron = M("BlackIron", lambda n: H.metal(n, (0.06, 0.06, 0.065), 0.45)); br = BRASS()
    Rb, zt = 260 / S, 3860 / S
    base = lathe("Base", [(0, 0), (Rb - 0.004, 0), (Rb, 0.004), (Rb, 0.012), (Rb * 0.86, 0.022), (0.05, 0.034), (0.03, 0.05), (0, 0.05)], iron, (0, 0, 0), 64)
    parts.append((base, 0)); hs.add(base)
    parts.append((lathe("Felt", [(0, 0.0005), (Rb - 0.01, 0.0005), (Rb - 0.01, 0.0025), (0, 0.0025)], M("Felt", fabric, (0.30, 0.28, 0.25), None, 900.0, 0.2), (0, 0, -0.0005), 48), 0))
    pole = cyl("Pole", 35 / S * 0.5, zt - 0.05, (0, 0, (zt + 0.05) / 2), iron, "Z", 20); parts.append((pole, 1)); hs.add(pole)
    parts.append((lathe("Coupling", [(0, 1.08), (0.016, 1.08), (0.020, 1.085), (0.020, 1.115), (0.016, 1.12), (0, 1.12)], br, (0, 0, 0), 24), 1))
    parts.append((lathe("Socket", [(0, zt - 0.02), (0.022, zt - 0.02), (0.022, zt + 0.03), (0.018, zt + 0.035), (0, zt + 0.035)], br, (0, 0, 0), 24), 1))
    z0, z1, R0, R1, w = 3700 / S, 4200 / S, 420 / S, 0.15, 0.004
    shade_m = M("LampShade", fabric, (0.93, 0.84, 0.64), (0.82, 0.72, 0.52), 300.0, 0.3)
    shade = lathe("Shade", [(R0, z0), (R1, z1), (R1 - w, z1), (R0 - w, z0)], shade_m, (0, 0, 0), 64)
    shade.data.use_auto_texspace = True
    parts.append((shade, 2)); hs.add(shade)
    for z, r in ((z0, R0), (z1, R1)):     # the wire rings sewn in the hems
        parts.append((pipe("Hem", [[(r * math.cos(2 * math.pi * k / 48), r * math.sin(2 * math.pi * k / 48), z) for k in range(48)]], 0.0025, shade_m, True), 2))
    rr = R0 + (R1 - R0) * (zt - z0) / (z1 - z0) - w * 0.5
    for i in range(3):
        a = i * 2.094 + 0.5
        parts.append((pipe("Spoke", [[(0, 0, zt - 0.012), (rr * math.cos(a), rr * math.sin(a), zt - 0.012)]], 0.002, br), 1))
    bulb = lathe("Bulb", [(0, zt + 0.035), (0.014, zt + 0.035), (0.016, zt + 0.05), (0.030, zt + 0.075), (0.032, zt + 0.095), (0.024, zt + 0.12), (0, zt + 0.128)], M("Bulb", emissive, (1.0, 0.86, 0.6), 6.0), (0, 0, 0), 32)
    parts.append((bulb, 1))
    parts.append((pipe("Chain", [[(0.03, 0, zt - 0.012), (0.03, 0, zt - 0.25)]], 0.0012, br), 1))
    for k in range(26):
        parts.append((cyl("Bead", 0.0024, 0.004, (0.03, 0, zt - 0.02 - k * 0.009), br, "Z", 8), 1))
    parts.append((lathe("Pull", [(0, 0), (0.006, 0.004), (0.008, 0.018), (0.003, 0.026), (0, 0.026)], br, (0.03, 0, zt - 0.29), 16), 1))
    # the cord leaves the base to the power strip
    parts.append((pipe("Cord", [[(0, 0.03, 0.02), (0.0, Rb - 0.01, 0.006), (0.0, Rb + 0.0, 0.003)]], 0.003, BLACKP()), 0))
    return parts, hs

def build_radiator():
    """column radiator 0.905 x 0.105 x 0.579 m (wall +Y), hung 0.158 m up: 9 cast sections, header tubes, chrome valves, brackets"""
    W, Dp, Hh = 1720 / S, 170 / S, 1100 / S
    fw, pitch = 120 / S, 200 / S
    hs = Hulls(); parts = []
    en = M("RadiatorEnamel", paint, (0.88, 0.87, 0.82), 0.32, 0.06, 0.25, (0.55, 0.45, 0.35))
    for i in range(9):
        x = -W / 2 + fw / 2 + i * pitch
        for yy in (-Dp / 4, Dp / 4):     # two columns per section, joined top and bottom
            c = soft("Col", x - fw / 2 + 0.004, x + fw / 2 - 0.004, yy - Dp / 4 + 0.002, yy + Dp / 4 - 0.002, 0.03, Hh - 0.03, en, 0.012, 3, 0.0, 0)
            parts.append((c, 1 + (i > 4)))
        for z in (0.0, Hh - 0.05):
            parts.append((box("Hub", x - fw / 2, x + fw / 2, -Dp / 2, Dp / 2, z, z + 0.05, en, 0.012, 3), 1 + (i > 4)))
        # casting seam line down the front of each section
        parts.append((box("Seam", x - 0.0008, x + 0.0008, -Dp / 2 - 0.0008, -Dp / 2 + 0.002, 0.06, Hh - 0.06, en, 0.0, 0), 1 + (i > 4)))
    hs.box(-W / 2, W / 2, -Dp / 2, Dp / 2, 0, Hh)
    for z in (0.025, Hh - 0.025):
        parts.append((cyl("Nipple", 0.02, W - fw, (0, 0, z), en, "X", 20), 0))
    yb = Dp / 2
    for x in (-W / 2 + pitch * 1.5, W / 2 - pitch * 1.5):
        parts.append((box("Bracket", x - 0.015, x + 0.015, yb - 0.004, yb + 30 / S + 0.005, Hh * 0.75 - 0.04, Hh * 0.75 + 0.04, steel(), 0.002, 2), 0))
        parts += [(o, 0) for o in screws_at("BrScrew", [(x, yb + 30 / S + 0.0055, Hh * 0.75 + 0.02, 1)], "Y", steel(), 0.004)]
    ch = chrome()
    for s in (-1, 1):
        x = s * (W / 2 - fw / 2)
        parts.append((cyl("Pipe", 0.011, 0.05 + 300 / S, (x, yb + 15 / S - 0.01, (0.05 - 300 / S) / 2), ch, "Z", 16), 0))  # down into the floor
        parts.append((lathe("Valve", [(0, 0), (0.016, 0), (0.016, 0.03), (0.012, 0.034), (0.012, 0.05), (0.018, 0.052), (0.018, 0.07), (0, 0.072)], ch, (x, yb + 15 / S - 0.01, 0.03), 24, 0, (math.pi, 0, 0)), 0))
    parts.append((cyl("BleedValve", 0.006, 0.012, (W / 2 + 0.003, 0, Hh - 0.025), ch, "X", 12), 0))
    return parts, hs


def rug_mat(name, hw, hd, kind="persian"):
    """wool pile rug. persian: cream border with a diamond lattice, red field, a navy-and-cream medallion, a pale traffic path;
    kilim: flat-woven stripes (the kitchen rug)"""
    m, p = H.mat(name)
    tc = N(m, "ShaderNodeTexCoord", -2200); sp = N(m, "ShaderNodeSeparateXYZ", -2000); L(m, tc.outputs["Object"], sp.inputs[0])
    def op(o, a, b=None, x=0, y=0, v=None):
        n = N(m, "ShaderNodeMath", x, y); n.operation = o
        for i, s in enumerate((a, b)):
            if s is None: continue
            if isinstance(s, (int, float)): n.inputs[i].default_value = s
            else: L(m, s, n.inputs[i])
        return n.outputs[0]
    red, cream, navy, rust = (0.42, 0.06, 0.05), (0.80, 0.70, 0.52), (0.07, 0.10, 0.22), (0.62, 0.22, 0.08)
    if kind == "persian":
        ax = op("ABSOLUTE", sp.outputs["X"], None, -1800, 200); ay = op("ABSOLUTE", sp.outputs["Y"], None, -1800, 0)
        dx = op("SUBTRACT", hw, ax, -1600, 200); dy = op("SUBTRACT", hd, ay, -1600, 0); d = op("MINIMUM", dx, dy, -1400, 100)
        cr = N(m, "ShaderNodeValToRGB", -1200, 100); cr.color_ramp.interpolation = "CONSTANT"; e = cr.color_ramp.elements
        e[0].position = 0.0; e[0].color = (*cream, 1); e[1].position = 0.11; e[1].color = (*red, 1)
        for pos, c in ((0.19, cream), (0.22, red), (0.52, navy), (0.55, red)):
            x = e.new(pos); x.color = (*c, 1)
        L(m, d, cr.inputs["Fac"])
        # diamond lattice in the cream border
        s1 = op("ADD", ax, ay, -1600, -300); s2 = op("SUBTRACT", ax, ay, -1600, -400)
        w1 = op("SINE", op("MULTIPLY", s1, 120.0, -1400, -300), None, -1200, -300); w2 = op("SINE", op("MULTIPLY", s2, 120.0, -1400, -400), None, -1200, -400)
        lat = op("MULTIPLY", w1, w2, -1000, -350); lat = op("GREATER_THAN", lat, 0.35, -850, -350)
        inb = op("LESS_THAN", d, 0.10, -1000, -200); inb2 = op("GREATER_THAN", d, 0.02, -1000, -250)
        lm = op("MULTIPLY", op("MULTIPLY", lat, inb, -800, -300), inb2, -650, -300)
        mx1 = N(m, "ShaderNodeMix", -500, 100); mx1.data_type = "RGBA"; mx1.inputs[7].default_value = (*rust, 1); L(m, lm, mx1.inputs[0]); L(m, cr.outputs["Color"], mx1.inputs[6])
        # medallion: rings of an ellipse in the middle
        ex = op("DIVIDE", sp.outputs["X"], hw, -1800, 600); ey = op("DIVIDE", sp.outputs["Y"], hd, -1800, 500)
        r = op("SQRT", op("ADD", op("MULTIPLY", ex, ex, -1600, 600), op("MULTIPLY", ey, ey, -1600, 500), -1400, 550), None, -1250, 550)
        mr = N(m, "ShaderNodeValToRGB", -1050, 550); mr.color_ramp.interpolation = "CONSTANT"; e = mr.color_ramp.elements
        e[0].position = 0.0; e[0].color = (*cream, 1); e[1].position = 0.30; e[1].color = (0, 0, 0, 0)
        for pos, c in ((0.08, navy), (0.14, rust), (0.20, cream), (0.235, navy), (0.27, rust)):
            x = e.new(pos); x.color = (*c, 1)
        L(m, r, mr.inputs["Fac"])
        mx2 = N(m, "ShaderNodeMix", -300, 200); mx2.data_type = "RGBA"; L(m, mr.outputs["Alpha"], mx2.inputs[0]); L(m, mx1.outputs[2], mx2.inputs[6]); L(m, mr.outputs["Color"], mx2.inputs[7])
        col = mx2.outputs[2]
        # traffic path: lighter, flattened band across the middle (the way from the door to the sofa)
        tp = op("ABSOLUTE", op("SUBTRACT", sp.outputs["Y"], op("MULTIPLY", sp.outputs["X"], 0.25, -1600, 900), -1400, 900), None, -1250, 900)
        tpm = N(m, "ShaderNodeMapRange", -1050, 900); tpm.inputs["From Min"].default_value = 0.45; tpm.inputs["From Max"].default_value = 0.1; tpm.inputs["To Max"].default_value = 0.22
        L(m, tp, tpm.inputs["Value"])
        mx3 = N(m, "ShaderNodeMix", -150, 200); mx3.data_type = "RGBA"; mx3.blend_type = "SCREEN"; mx3.inputs[7].default_value = (0.45, 0.40, 0.35, 1)
        L(m, tpm.outputs["Result"], mx3.inputs[0]); L(m, col, mx3.inputs[6]); col = mx3.outputs[2]
    else:
        wv = N(m, "ShaderNodeValToRGB", -1200, 100); wv.color_ramp.interpolation = "CONSTANT"; e = wv.color_ramp.elements
        e[0].position = 0.0; e[0].color = (*red, 1); e[1].position = 0.92; e[1].color = (*red, 1)
        for pos, c in ((0.08, cream), (0.14, red), (0.30, (0.35, 0.22, 0.12)), (0.34, cream), (0.50, red), (0.62, cream), (0.68, (0.35, 0.22, 0.12)), (0.72, red), (0.86, cream)):
            x = e.new(pos); x.color = (*c, 1)
        fr = op("FRACT", op("MULTIPLY", op("ADD", sp.outputs["X"], hw, -1800, 100), 1.6, -1600, 100), None, -1400, 100)
        L(m, fr, wv.inputs["Fac"]); col = wv.outputs["Color"]
    # yarn: heather and pile
    ht = N(m, "ShaderNodeTexNoise", -900, -600); ht.inputs["Scale"].default_value = 380; ht.inputs["Detail"].default_value = 2; L(m, tc.outputs["Object"], ht.inputs["Vector"])
    hm = N(m, "ShaderNodeMapRange", -700, -600); hm.inputs["To Min"].default_value = 0.78; hm.inputs["To Max"].default_value = 1.12; L(m, ht.outputs["Fac"], hm.inputs["Value"])
    fin = N(m, "ShaderNodeMix", 0, 200); fin.data_type = "RGBA"; fin.blend_type = "MULTIPLY"; fin.inputs[0].default_value = 1.0
    L(m, col, fin.inputs[6]); cm = N(m, "ShaderNodeCombineColor", -300, -600); [L(m, hm.outputs["Result"], cm.inputs[i]) for i in range(3)]; L(m, cm.outputs[0], fin.inputs[7])
    L(m, fin.outputs[2], p.inputs["Base Color"])
    bp = N(m, "ShaderNodeBump", -300, -800); bp.inputs["Strength"].default_value = 0.5; bp.inputs["Distance"].default_value = 0.0006
    if kind != "persian":
        wb = N(m, "ShaderNodeTexWave", -900, -900); wb.wave_type = "BANDS"; wb.bands_direction = "Y"; wb.inputs["Scale"].default_value = 160; L(m, tc.outputs["Object"], wb.inputs["Vector"])
        L(m, wb.outputs["Fac"], bp.inputs["Height"])
    else:
        L(m, ht.outputs["Fac"], bp.inputs["Height"])
    L(m, bp.outputs["Normal"], p.inputs["Normal"])
    p.inputs["Roughness"].default_value = 0.95; p.inputs["Sheen Weight"].default_value = 0.8; p.inputs["Sheen Roughness"].default_value = 0.5
    return H.tag(m, 0.0, 1.0)

def build_carpet():
    """living-room rug 3.684 x 2.894 m, 6 mm wool pile with a ramped edge (ants walk on), fringe on the short ends"""
    W, D, t = 7000 / S, 5500 / S, 0.006
    fr = 0.028; w, d = W - 2 * fr, D
    hs = Hulls(); parts = []
    m = rug_mat("PersianRug", w / 2, d / 2)
    slab = box("Rug", -w / 2, w / 2, -d / 2, d / 2, 0, t, m, 0.003, 3)
    bm = bmesh.new(); bm.from_mesh(slab.data); bmesh.ops.subdivide_edges(bm, edges=[e for e in bm.edges], cuts=12, use_grid_fill=True); bm.to_mesh(slab.data); bm.free()
    # a slight ripple where it was kicked
    for v in slab.data.vertices:
        if v.co.z > t * 0.5 - 0.0001:
            v.co.z += 0.0035 * max(0.0, math.cos((v.co.x - 0.9) * 2.2)) * max(0.0, 1 - abs(v.co.y + 0.6) / 0.9)
    parts.append((slab, 0)); hs.box(-w / 2, w / 2, -d / 2, d / 2, 0, t)
    trim = M("Fringe", fabric, (0.86, 0.80, 0.66), (0.70, 0.62, 0.48), 400.0, 0.3)
    R = random.Random(4); strands = []
    n = int(d / 0.018)
    for s in (-1, 1):
        for i in range(n):
            y = -d / 2 + 0.012 + i * (d - 0.024) / (n - 1)
            x0 = s * (w / 2 - 0.004); ln = fr + R.uniform(0.0, 0.012)
            strands.append([(x0, y, 0.0035), (x0 + s * ln * 0.5, y + R.uniform(-0.003, 0.003), 0.0016), (x0 + s * ln, y + R.uniform(-0.008, 0.008), 0.0012)])
    fo = H.tubes("Fringe", strands, 0.0014, res_u=2, bevel_res=0, tip=0.8); fo.data.materials.append(trim); parts.append((fo, 1))
    for s in (-1, 1):   # the overcast binding along the ends where the fringe comes out
        parts.append((box("Binding", s * (w / 2) - 0.006, s * (w / 2) + 0.006, -d / 2, d / 2, 0, t + 0.0005, trim, 0.002, 2), 1))
    return parts, hs

def build_kitchen_rug():
    """kitchen rug under the table 3.158 x 2.214 m, 9 mm flat-woven wool kilim with whipped edges"""
    W, D, t = 3.1579, 2.2136, 0.0089
    hs = Hulls(); parts = []
    m = rug_mat("Kilim", W / 2, D / 2, "kilim")
    slab = box("Rug", -W / 2 + 0.01, W / 2 - 0.01, -D / 2 + 0.01, D / 2 - 0.01, 0, t, m, 0.004, 3); parts.append((slab, 0))
    hs.box(-W / 2, W / 2, -D / 2, D / 2, 0, t)
    edge = M("KilimEdge", fabric, (0.30, 0.16, 0.08), (0.20, 0.10, 0.05), 400.0, 0.2)
    parts.append((pipe("Whip", [rect_loop(-W / 2 + 0.008, W / 2 - 0.008, -D / 2 + 0.008, D / 2 - 0.008, t * 0.5, 0.0)], 0.0055, edge, True), 1))
    R = random.Random(6); strands = []
    for s in (-1, 1):
        for i in range(70):
            y = -D / 2 + 0.03 + i * (D - 0.06) / 69
            x0 = s * (W / 2 - 0.004)
            strands.append([(x0, y, 0.004), (x0 + s * 0.006, y, 0.002), (x0 + s * 0.012, y + R.uniform(-0.003, 0.003), 0.0012)])
    fo = H.tubes("Tassel", strands, 0.0024, res_u=2, bevel_res=0, tip=0.7); fo.data.materials.append(edge); parts.append((fo, 1))
    return parts, hs


U = 1.0 / S     # (the level v2/v3 builders are in game units)

def extrude(name, prof_yz, x0, x1, m, bev=0.0):
    """a profile in the YZ plane (closed polygon) extruded along X"""
    bm = bmesh.new()
    a = [bm.verts.new((x0, y, z)) for y, z in prof_yz]; b = [bm.verts.new((x1, y, z)) for y, z in prof_yz]
    n = len(prof_yz)
    bm.faces.new(a[::-1]); bm.faces.new(b)
    for i in range(n):
        j = (i + 1) % n; bm.faces.new((a[i], a[j], b[j], b[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = H.link(bpy.data.objects.new(name, me)); o.data.materials.append(m)
    if bev: _mod_bevel(o, bev, 2, True, 30)
    return o

def F_GREENDOOR(): return M("DoorGreen", paint, (0.33, 0.48, 0.36), 0.34, 0.03, 0.2, (0.80, 0.78, 0.70))

def build_door_frame():
    """door lining 0.884 x 0.147 x 2.021 m: jambs and head with a planted stop bead, butt-hinge leaves, a brass strike plate"""
    Wd, jt, Dp, Hh = 1600 / S, 40 / S, 280 / S, 3800 / S
    hw = Wd / 2; ys = -Dp / 2 + 0.040
    hs = Hulls(); parts = []; wz = W_OAK("Z"); wx = W_OAK("X")
    for s in (-1, 1):
        j = box("Jamb", min(s * hw, s * (hw + jt)), max(s * hw, s * (hw + jt)), -Dp / 2, Dp / 2, 0, Hh + jt, wz, 0.003, 3); parts.append((j, 1 + (s > 0))); hs.add(j)
        parts.append((box("Stop", min(s * hw, s * (hw - 0.012)), max(s * hw, s * (hw - 0.012)), ys, ys + 0.012, 0, Hh, wz, 0.002, 2), 1 + (s > 0)))
    hd_ = box("Head", -hw - jt, hw + jt, -Dp / 2, Dp / 2, Hh, Hh + jt, wx, 0.003, 3); parts.append((hd_, 3)); hs.add(hd_)
    parts.append((box("HeadStop", -hw, hw, ys, ys + 0.012, Hh - 0.012, Hh, wx, 0.002, 2), 3))
    br = BRASS()
    for z in (0.20, 1.0, 1.80):      # hinge leaves on the hinge jamb (-X), with their screws
        parts.append((box("HingeLeaf", -hw - 0.0005, -hw + 0.0015, -Dp / 2, -Dp / 2 + 0.03, z - 0.045, z + 0.045, br, 0.0008, 1), 1))
        parts += [(o, 1) for o in screws_at("HScrew", [(-hw + 0.0016, -Dp / 2 + 0.015, z + dz, 1) for dz in (-0.03, 0.0, 0.03)], "X", br, 0.0035)]
    parts.append((box("Strike", hw - 0.0015, hw + 0.0005, -Dp / 2 + 0.002, -Dp / 2 + 0.03, 0.96, 1.04, br, 0.0008, 1), 2))
    parts.append((box("StrikeHole", hw - 0.0018, hw - 0.0012, -Dp / 2 + 0.010, -Dp / 2 + 0.022, 0.99, 1.01, BLACKP(), 0.0, 0), 2))
    return parts, hs

def build_door_leaf():
    """panelled door leaf 0.834 x 1.968 m, built in the frame and moved to its hinge origin (front -Y): green paint,
    raised panels with ovolo beads, brass levers on roses both sides, keyhole escutcheons, three butt hinges"""
    Wd, Dp, Hh = 1600 / S, 280 / S, 3800 / S
    hw = Wd / 2; ys = -Dp / 2 + 0.040
    x0, x1, y0, y1, z0, z1 = -hw + 0.004, hw - 0.004, -Dp / 2, ys, 0.012, Hh - 0.004
    ax, ay = x0, y0 - 0.006
    hs = Hulls(); parts = []; g = F_GREENDOOR(); br = BRASS()
    st = 0.11
    core = box("Core", x0 + st - 0.01, x1 - st + 0.01, y0 + 0.012, y1 - 0.012, z0 + 0.19, z1 - 0.12, g, 0.0, 0); parts.append((core, 0))
    for (xa, xb) in ((x0, x0 + st), (x1 - st, x1)):
        parts.append((box("Stile", xa, xb, y0, y1, z0, z1, g, 0.003, 3), 0))
    for (za, zb) in ((z0, z0 + 0.20), (0.92, 1.08), (z1 - 0.13, z1)):
        parts.append((box("Rail", x0 + st - 0.002, x1 - st + 0.002, y0, y1, za, zb, g, 0.003, 3), 0))
    for (za, zb) in ((z0 + 0.20, 0.92), (1.08, z1 - 0.13)):     # raised and fielded panels both faces, a bead round each
        for side in (-1, 1):
            yf = y0 + 0.006 if side < 0 else y1 - 0.006
            pn = soft("Panel", x0 + st + 0.004, x1 - st - 0.004, min(yf, yf - side * 0.008), max(yf, yf - side * 0.008), za + 0.004, zb - 0.004, g, 0.02, 3, 0.0, 0)
            parts.append((pn, 1))
            yb = y0 + 0.0015 if side < 0 else y1 - 0.0015
            parts.append((pipe("Bead", [rot_xz(rect_loop(x0 + st + 0.003, x1 - st - 0.003, za + 0.003, zb - 0.003, yb))], 0.003, g, True), 1))
    hs.box(x0, x1, y0, y1, z0, z1)
    hx, hz = x1 - 0.06, 1.0
    for s, yy in ((-1, y0), (1, y1)):
        parts.append((lathe("Rose", [(0, 0), (0.027, 0), (0.028, 0.002), (0.024, 0.008), (0.010, 0.010), (0, 0.010)], br, (hx, yy, hz), 32, 0, (s * math.pi / 2, 0, 0)), 0))
        parts.append((cyl("Spindle", 0.009, 0.040, (hx, yy + s * 0.022, hz), br, "Y", 16), 0))
        lv = box("Lever", hx - 0.115, hx + 0.008, yy + s * 0.036, yy + s * 0.052, hz - 0.009, hz + 0.009, br, 0.006, 3); parts.append((lv, 0))
        parts.append((lathe("KeyRose", [(0, 0), (0.016, 0), (0.016, 0.003), (0, 0.004)], br, (hx, yy, hz - 0.08), 24, 0, (s * math.pi / 2, 0, 0)), 0))
        parts.append((box("Keyhole", hx - 0.003, hx + 0.003, yy + s * 0.0035, yy + s * 0.0045, hz - 0.09, hz - 0.074, BLACKP(), 0.0, 0), 0))
    # the latch in the edge, the hinge knuckles on the hinge side
    parts.append((box("Faceplate", x1 - 0.0005, x1 + 0.0008, y0 + 0.008, y1 - 0.008, hz - 0.09, hz + 0.09, br, 0.0006, 1), 0))
    parts.append((box("Latch", x1, x1 + 0.010, -Dp / 2 + 0.015, -Dp / 2 + 0.03, hz - 0.012, hz + 0.012, br, 0.002, 2), 0))
    for z in (0.20, 1.0, 1.80):
        parts.append((lathe("Knuckle", [(0, -0.045), (0.0075, -0.045), (0.008, -0.043), (0.008, 0.043), (0.0075, 0.045), (0, 0.045)], br, (ax, ay, z), 20), 0))
        parts.append((box("Leaf", ax, x0 + 0.03, ay - 0.001, ay + 0.0012, z - 0.045, z + 0.045, br, 0.0006, 1), 0))
        parts.append((lathe("Finial", [(0, 0), (0.007, 0), (0.0055, 0.008), (0, 0.010)], br, (ax, ay, z + 0.045), 16), 0))
    # scuffs at the bottom (kicked), a fingerprinted zone round the lever: in the paint's wear and dust
    return parts, hs, Vector((ax, ay, z0))

def build_skirting(length=1.0):
    """skirting board, ogee top, painted (1 m module, the game stretches it along X); back +Y against the wall"""
    b = 15 / S; h = 220 / S
    prof = [(b, 0.0), (-b, 0.0), (-b, 0.082), (-b + 0.002, 0.088), (-b + 0.005, 0.090), (-b + 0.005, 0.096), (-b + 0.008, 0.103), (-b + 0.012, 0.108), (-b + 0.015, 0.112), (0.0, h), (b, h)]
    hs = Hulls(); parts = []
    sk = M("SkirtingPaint", paint, (0.88, 0.87, 0.82), 0.36, 0.015, 0.0, None)
    o = extrude("Skirting", prof, -length / 2, length / 2, sk, 0.0015); parts.append((o, 0)); hs.add(o)
    return parts, hs

def build_outlet():
    """wall socket 89 x 89 mm (front -Y): white plastic plate with a soft edge, round well, two pin holes and shutter, earth clips, screw"""
    hs = Hulls(); parts = []
    wp = M("WhitePlastic", lambda n: H.plastic(n, (0.88, 0.87, 0.83), 0.35, 0.03, 0.2)); ch = chrome()
    plate = soft("Plate", -0.0447, 0.0447, -0.0063, 0.0, -0.0447, 0.0447, wp, 0.004, 3, 0.0, 0); parts.append((plate, 0)); hs.add(plate)
    well = lathe("Well", [(0.0, -0.0105 + 0.0040), (0.019, -0.0105 + 0.0040), (0.021, -0.0105 + 0.0002), (0.0237, -0.0105), (0.0250, -0.0105 + 0.001), (0.0250, 0.0)], wp, (0, 0, 0), 48, 0, (-math.pi / 2, 0, 0))
    well.location = (0, 0, 0); well.rotation_euler = (math.pi / 2, 0, 0)
    parts.append((well, 0))
    for x in (-0.0095, 0.0095):
        parts.append((cyl("PinHole", 0.0025, 0.0012, (x, -0.0066, 0), M("Hole", H.rubber, (0.02, 0.015, 0.01), 0.9), "Y", 12), 0))
    for z in (-0.0215, 0.0215):
        parts.append((box("Earth", -0.004, 0.004, -0.0105, -0.0068, z - 0.0012, z + 0.0012, ch, 0.0004, 1), 0))
    parts += [(o, 0) for o in screws_at("Screw", [(0, -0.0065, -0.0368, -1)], "Y", ch, 0.0037)]
    return parts, hs

def build_switch():
    """light switch plate 74 x 105 mm (front -Y), rocker frame; the rocker is SM_K_Switch_Toggle"""
    hs = Hulls(); parts = []
    wp = M("WhitePlastic", lambda n: H.plastic(n, (0.88, 0.87, 0.83), 0.35, 0.03, 0.2))
    plate = soft("Plate", -0.0368, 0.0368, -0.0021, 0.0063, 0.0, 0.1053, wp, 0.004, 3, 0.0, 0); parts.append((plate, 0)); hs.add(plate)
    fr = box("Frame", -0.0160, 0.0160, -0.0063, -0.0015, 0.0526 - 0.0274, 0.0526 + 0.0274, wp, 0.0015, 2); parts.append((fr, 0))
    parts.append((box("Recess", -0.0115, 0.0115, -0.0066, -0.0060, 0.0526 - 0.021, 0.0526 + 0.021, M("Hole", H.rubber, (0.02, 0.015, 0.01), 0.9), 0.0, 0), 0))
    parts.append((box("Logo", -0.004, 0.004, -0.0022, -0.0020, 0.006, 0.009, M("Grey", lambda n: H.plastic(n, (0.5, 0.5, 0.5), 0.4, 0.01, 0.1)), 0.0, 0), 0))
    return parts, hs

def build_switch_toggle():
    """the rocker 19 x 18 x 15 mm, pivot at the origin (the plate face)"""
    hs = Hulls(); parts = []
    wp = M("WhitePlastic", lambda n: H.plastic(n, (0.88, 0.87, 0.83), 0.35, 0.03, 0.2))
    r = soft("Rocker", -0.0095, 0.0095, -0.0179, -0.004, -0.0074, 0.0074, wp, 0.002, 3, 0.15, 0); parts.append((r, 0)); hs.add(r)
    parts.append((box("Stem", -0.006, 0.006, -0.005, 0.0, -0.004, 0.004, wp, 0.0005, 1), 0))
    parts.append((box("Dot", -0.0015, 0.0015, -0.0181, -0.0178, 0.003, 0.005, M("Led", emissive, (1.0, 0.45, 0.1), 2.0), 0.0, 0), 0))
    return parts, hs


def build_door_leaf_at_hinge():
    parts, hs, org = build_door_leaf()
    for o, _ in parts: o.matrix_world = Matrix.Translation(-org) @ o.matrix_world
    hs.groups = [[(p[0] - org.x, p[1] - org.y, p[2] - org.z) for p in g] if g and isinstance(g[0], tuple) else g for g in hs.groups]
    return parts, hs

def glass(name, col=(0.95, 0.93, 0.88), rough=0.35):
    """frosted glass for the shades: in the bake it reads as a milky, slightly rough white (the game's LampShade look lights it)"""
    m, p = H.mat(name); p.inputs["Base Color"].default_value = (*col, 1); p.inputs["Roughness"].default_value = rough
    p.inputs["Subsurface Weight"].default_value = 0.3; p.inputs["Coat Weight"].default_value = 0.5
    return H.tag(m, 0.0, 0.0)

def build_ceiling_lamp_l():
    """living-room ceiling light, origin at the bottom, top on the ceiling (r 0.184, h 0.132): frosted glass dome on a brass fitter"""
    R, Hh = 350 / S, 250 / S
    hs = Hulls(); parts = []; br = BRASS()
    shade = lathe("Shade", [(0.0, 0.0), (0.04, 0.0015), (R * 0.62, 0.012), (R * 0.9, 0.035), (R - 0.004, 0.065), (R, 0.08), (R - 0.006, 0.082), (R * 0.92 - 0.004, 0.068), (R * 0.6, 0.016), (0.04, 0.0055), (0.0, 0.004)],
                  glass("LampShade"), (0, 0, 0), 64)
    parts.append((shade, 0)); hs.add(shade)
    parts.append((lathe("Gallery", [(R - 0.002, 0.078), (R + 0.004, 0.078), (R + 0.004, 0.092), (R * 0.9, 0.096), (R * 0.9, 0.090), (R - 0.002, 0.088)], br, (0, 0, 0), 64), 0))
    parts.append((lathe("Canopy", [(R * 0.9, 0.096), (R * 0.75, 0.115), (0.06, Hh - 0.004), (0.065, Hh), (0.0, Hh)], br, (0, 0, 0), 64), 0))
    for k in range(3):
        a = 2 * math.pi * k / 3
        parts.append((cyl("Thumb", 0.004, 0.016, ((R + 0.012) * math.cos(a), (R + 0.012) * math.sin(a), 0.085), br, "Z", 10), 0))
        parts.append((lathe("ThumbKnob", [(0, 0), (0.007, 0.0), (0.008, 0.006), (0, 0.008)], br, ((R + 0.012) * math.cos(a), (R + 0.012) * math.sin(a), 0.093), 16), 0))
    parts.append((lathe("Bulb", [(0, 0.03), (0.024, 0.032), (0.030, 0.05), (0.022, 0.07), (0.014, 0.08), (0, 0.08)], M("Bulb", emissive, (1.0, 0.86, 0.6), 6.0), (0, 0, 0), 32), 0))
    return parts, hs

def build_ceiling_lamp_k():
    """kitchen ceiling light (r 0.184, h 0.132, origin at the bottom): opal drum shade (LampShade), enamel ceiling plate, a ball bulb"""
    R, Hh = 350 / S, 250 / S
    hs = Hulls(); parts = []
    en = P_WHITE()
    shade = lathe("Shade", [(R - 0.008, 0.0), (R, 0.0), (R + 0.0015, 0.004), (R + 0.0015, Hh - 0.012), (R, Hh - 0.010), (R - 0.006, Hh - 0.010), (R - 0.006, 0.004)], glass("LampShade"), (0, 0, 0), 64)
    parts.append((shade, 0)); hs.add(shade)
    parts.append((lathe("Plate", [(0.0, Hh - 0.012), (R + 0.004, Hh - 0.012), (R + 0.006, Hh - 0.008), (R + 0.004, Hh), (0.0, Hh)], en, (0, 0, 0), 64), 0))
    parts.append((lathe("Holder", [(0, Hh - 0.012), (0.018, Hh - 0.012), (0.018, Hh - 0.05), (0.015, Hh - 0.055), (0, Hh - 0.055)], BLACKP(), (0, 0, 0), 24), 0))
    parts.append((lathe("Bulb", [(0, Hh - 0.055), (0.012, Hh - 0.056), (0.036, Hh - 0.08), (0.037, Hh - 0.1), (0.02, Hh - 0.122), (0, Hh - 0.125)], M("Bulb", emissive, (1.0, 0.86, 0.6), 6.0), (0, 0, 0), 32), 0))
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        parts += [(o, 0) for o in screws_at("Screw", [((R - 0.02) * math.cos(a), (R - 0.02) * math.sin(a), Hh - 0.0125, -1)], "Z", steel(), 0.003)]
    return parts, hs

def build_pendant():
    """pendant over the table, 1.0 m, origin at the shade's rim: enamel cone shade (white inside), brass socket, cloth cord, ceiling cup"""
    hs = Hulls(); parts = []
    en = M("PendantEnamel", paint, (0.20, 0.36, 0.30), 0.30, 0.04, 0.3, (0.12, 0.12, 0.12)); br = BRASS()
    shade = lathe("Shade", [(470 * U, 0.0), (110 * U, 300 * U), (98 * U, 300 * U - 0.004), (458 * U, -0.0015)], en, (0, 0, 0), 64); parts.append((shade, 0)); hs.add(shade)
    parts.append((lathe("Inner", [(456 * U, 0.0005), (100 * U, 298 * U - 0.004), (99 * U, 298 * U - 0.005), (455 * U, -0.0005)], P_WHITE(), (0, 0, 0), 64), 0))
    parts.append((lathe("Rim", [(466 * U, -0.002), (474 * U, -0.002), (474 * U, 0.002), (466 * U, 0.002)], BLACKP(), (0, 0, 0), 64), 0))
    parts.append((lathe("Socket", [(0, 240 * U), (30 * U, 240 * U), (30 * U, 300 * U), (24 * U, 320 * U), (8 * U, 330 * U), (0, 330 * U)], br, (0, 0, 0), 24), 0))
    parts.append((lathe("Bulb", [(0, 240 * U), (16 * U, 236 * U), (55 * U, 190 * U), (58 * U, 160 * U), (40 * U, 118 * U), (0, 110 * U)], M("Bulb", emissive, (1.0, 0.86, 0.6), 6.0), (0, 0, 0), 32), 0))
    cord = M("ClothCord", fabric, (0.12, 0.10, 0.09), (0.05, 0.04, 0.04), 900.0, 0.2)
    c = cyl("Cord", 5 * U, 1530 * U, (0, 0, 1095 * U), cord, "Z", 10); parts.append((c, 0)); hs.add(c)
    parts.append((lathe("Cup", [(0, 1860 * U), (55 * U, 1860 * U), (62 * U, 1880 * U), (62 * U, 1900 * U), (0, 1900 * U)], br, (0, 0, 0), 32), 0))
    parts.append((lathe("Grip", [(0, 1830 * U), (12 * U, 1830 * U), (12 * U, 1862 * U), (0, 1862 * U)], br, (0, 0, 0), 16), 0))
    return parts, hs

def build_curtain_rod():
    """curtain pole 2.26 m (along X, origin on its axis): brass tube with a seam, acorn finials"""
    hs = Hulls(); parts = []; br = BRASS()
    t = cyl("Tube", 18 * U, 4300 * U, (0, 0, 0), br, "X", 32); parts.append((t, 0)); hs.add(t)
    parts.append((box("Seam", -2150 * U, 2150 * U, -0.0003, 0.0003, 18 * U - 0.0003, 18 * U + 0.0002, M("BrassDark", H.metal, (0.55, 0.38, 0.16), 0.4), 0.0, 0), 0))
    for s in (-1, 1):
        f = lathe("Finial", [(0, 0), (0.012, 0.0), (0.013, 0.006), (0.010, 0.010), (0.016, 0.018), (0.017, 0.028), (0.012, 0.040), (0.004, 0.048), (0, 0.050)], br, (s * 2150 * U - s * 0.004, 0, 0), 32, 0, (0, s * math.pi / 2, 0))
        parts.append((f, 0)); hs.add(f)
    return parts, hs

def build_rod_bracket():
    """pole bracket (wall +Y): brass back plate on two screws, an arm and a cradle"""
    hs = Hulls(); parts = []; br = BRASS()
    p = box("Plate", -40 * U, 40 * U, -10 * U, 0, -90 * U, 90 * U, br, 0.0025, 3); parts.append((p, 0)); hs.add(p)
    a = box("Arm", -8 * U, 8 * U, -270 * U, -8 * U, -8 * U, 8 * U, br, 0.003, 3); parts.append((a, 0)); hs.add(a)
    parts.append((pipe("Cradle", [[(0, -260 * U + 0.026 * math.cos(math.radians(d)), -0.012 - 0.026 * math.sin(math.radians(d)) + 0.004) for d in range(-10, 191, 12)]], 5 * U, br), 0))
    parts += [(o, 0) for o in screws_at("Screw", [(0, -10 * U - 0.0008, z * U, -1) for z in (-55, 55)], "Y", steel(), 0.004)]
    return parts, hs

def build_curtain_ring():
    """curtain ring round the pole (YZ plane, axis = the pole), an eye and a crocodile clip with teeth"""
    hs = Hulls(); parts = []; br = BRASS(); st = steel()
    parts.append((pipe("Ring", [[(0, 34 * U * math.cos(2 * math.pi * k / 24), 34 * U * math.sin(2 * math.pi * k / 24)) for k in range(24)]], 4 * U, br, True), 0))
    parts.append((pipe("Eye", [[(0, 0.0, -34 * U), (0, 0, -42 * U), (0, 0.003, -46 * U), (0, 0, -50 * U), (0, -0.003, -46 * U), (0, 0, -42 * U)]], 1.5 * U * 2, br), 0))
    c = box("Clip", -10 * U, 10 * U, -7 * U, 7 * U, -75 * U, -58 * U, st, 0.001, 2); parts.append((c, 0))
    for k in range(4):
        parts.append((box("Tooth", -9 * U + k * 6 * U, -7 * U + k * 6 * U, -1 * U, 1 * U, -77 * U, -75 * U, st, 0.0, 0), 0))
    hs.box(-0.006, 0.006, -0.02, 0.02, -0.042, 0.02)
    return parts, hs

def build_cornice():
    """plaster cove cornice (origin in the wall/ceiling corner, along X, 1.05 m module): a cove with two fillets"""
    hs = Hulls(); parts = []
    pl = M("Plaster", plaster, (0.90, 0.89, 0.85), 0.75)
    pts = [(0, 0), (0, -130 * U), (-6 * U, -130 * U), (-6 * U, -118 * U), (-15 * U, -114 * U)]
    for k in range(9):
        a = math.radians(5 + 80 * k / 8)
        pts.append((-15 * U - 100 * U * (1 - math.cos(a)) * 0.95, -114 * U + 100 * U * math.sin(a) * 0.95))
    pts += [(-118 * U, -15 * U), (-118 * U, -6 * U), (-130 * U, -6 * U), (-130 * U, 0)]
    pts = [(y, z) for y, z in pts]
    o = extrude("Cornice", pts, -1000 * U, 1000 * U, pl, 0.001); parts.append((o, 0)); hs.add(o)
    return parts, hs


def build_wall_shelf():
    """wall shelf board 1.158 x 0.237 x 0.021 m (wall +Y, origin back-bottom-centre): oak with eased edges"""
    hs = Hulls(); parts = []
    b = box("Board", -1100 * U, 1100 * U, -450 * U, 0, 0, 40 * U, W_OAK("X"), 0.003, 3); parts.append((b, 0)); hs.add(b)
    # the marks of the jars that stood on it (rings), a strip of wear along the front edge
    stain = M("Stain", lambda n: H.plastic(n, (0.30, 0.17, 0.07), 0.25, 0.02, 0.1))
    for x in (-0.3, 0.12, 0.38):
        parts.append((lathe("Ring", [(0.034, 40 * U + 0.0002), (0.038, 40 * U + 0.0002), (0.038, 40 * U + 0.0004), (0.034, 40 * U + 0.0004)], stain, (x, -0.11, 0), 40), 0))
    return parts, hs

def build_shelf_bracket():
    """L shelf bracket (wall +Y): black powder-coated steel, a gusset, two screws in the wall, two up into the board"""
    hs = Hulls(); parts = []
    bk = M("BlackSteel", lambda n: H.plastic(n, (0.03, 0.03, 0.035), 0.5, 0.05, 0.2)); st = steel()
    v = box("Upright", -16 * U, 16 * U, -8 * U, 0, 0, 240 * U, bk, 0.001, 2); parts.append((v, 0)); hs.add(v)
    h = box("Arm", -16 * U, 16 * U, -380 * U, 0, 232 * U, 240 * U, bk, 0.001, 2); parts.append((h, 0)); hs.add(h)
    g = extrude("Gusset", [(-8 * U, 40 * U), (-8 * U, 232 * U), (-200 * U, 232 * U)], -4 * U, 4 * U, bk, 0.0008); parts.append((g, 0))
    parts += [(o, 0) for o in screws_at("WScrew", [(0, -8 * U - 0.0008, z * U, -1) for z in (60, 180)], "Y", st, 0.0026)]
    parts += [(o, 0) for o in screws_at("BScrew", [(0, y * U, 240 * U + 0.0008, 1) for y in (-120, -300)], "Z", st, 0.0026)]
    return parts, hs

def build_shoe_rack():
    """open shoe rack 0.894 x 0.316 x 0.579 m: beech slatted shelves (tops 0.063 / 0.221 / 0.411), top board, walnut back rail"""
    hs = Hulls(); parts = []; beech = M("Beech", wood, (0.74, 0.55, 0.34), (0.52, 0.36, 0.20), "X", 26.0, 0.3, 0.3, 0.35)
    for sx in (-1, 1):
        sd = box("Side", sx * 835 * U - 15 * U, sx * 835 * U + 15 * U, -300 * U, 300 * U, 0, 1100 * U, M("BeechZ", wood, (0.74, 0.55, 0.34), (0.52, 0.36, 0.20), "Z", 26.0, 0.3, 0.3, 0.35), 0.003, 3)
        parts.append((sd, 1 + (sx > 0))); hs.add(sd)
    tp = box("Top", -850 * U, 850 * U, -300 * U, 300 * U, 1070 * U, 1100 * U, beech, 0.003, 3); parts.append((tp, 3)); hs.add(tp)
    for i, ztop in enumerate((120, 420, 780)):
        for k in range(5):
            y = -250 + k * 125
            sl = box("Slat", -820 * U, 820 * U, (y - 40) * U, (y + 40) * U, (ztop - 24) * U, ztop * U, beech, 0.002, 3); parts.append((sl, 4 + i)); hs.add(sl)
            for sx in (-1, 1):    # dowel ends showing through the sides
                parts.append((cyl("Dowel", 0.005, 0.002, (sx * (850 * U + 0.0003), y * U, (ztop - 12) * U), M("EndGrain", wood, (0.62, 0.44, 0.26), (0.45, 0.30, 0.17), "X", 6.0, 0.0, 0.0, 0.0, 0.6), "X", 12), 1 + (sx > 0)))
    parts.append((box("BackRail", -820 * U, 820 * U, 270 * U, 290 * U, 60 * U, 1070 * U, W_WALNUT("Z"), 0.002, 2), 3))
    return parts, hs

def build_kitchen_chair():
    """kitchen chair (back +Y): green-painted seat and back rails, turned walnut legs with stretchers, felt pads, seat top 0.463"""
    hs = Hulls(); parts = []; g = P_GREEN(); w = W_WALNUT("Z")
    seat = soft("Seat", -450 * U, 450 * U, -450 * U, 530 * U, 800 * U, 880 * U, g, 0.008, 3, 0.0, 0); parts.append((seat, 0)); hs.add(seat)
    for (x, y) in ((-380, -380), (380, -380)):
        lg = lathe("Leg", [(0, 0.004), (0.014, 0.004), (0.016, 0.03), (0.020, 0.18), (0.017, 0.24), (0.019, 0.30), (0.019, 802 * U), (0, 802 * U)], w, (x * U, y * U, 0), 20); parts.append((lg, 1 + (x > 0))); hs.add(lg)
        parts.append((cyl("Pad", 0.014, 0.004, (x * U, y * U, 0.002), M("Felt", fabric, (0.30, 0.28, 0.25), None, 900.0, 0.2), "Z", 12), 1 + (x > 0)))
    for x in (-380, 380):
        bl = lathe("BackLeg", [(0, 0.004), (0.014, 0.004), (0.016, 0.03), (0.019, 0.30), (0.019, 2050 * U), (0.015, 2090 * U), (0, 2090 * U)], w, (x * U, 490 * U, 0), 20); parts.append((bl, 3)); hs.add(bl)
        parts.append((cyl("Pad", 0.014, 0.004, (x * U, 490 * U, 0.002), M("Felt", fabric, (0.30, 0.28, 0.25), None, 900.0, 0.2), "Z", 12), 3))
        parts.append((cyl("SideStretcher", 0.009, 870 * U, (x * U, 55 * U, 325 * U), w, "Y", 14), 3))
    parts.append((cyl("FrontStretcher", 0.009, 760 * U, (0, -380 * U, 325 * U), w, "X", 14), 1))
    tr = soft("TopRail", -420 * U, 420 * U, 462 * U, 518 * U, 1730 * U, 2100 * U, g, 0.006, 3, 0.0, 0); parts.append((tr, 3)); hs.add(tr)
    parts.append((soft("MidRail", -380 * U, 380 * U, 470 * U, 510 * U, 1250 * U, 1350 * U, g, 0.005, 3, 0.0, 0), 3))
    for x in (-0.12, 0.0, 0.12):     # spindles between the rails
        parts.append((cyl("Spindle", 0.007, (1730 - 1350) * U, (x, 490 * U, (1730 + 1350) / 2 * U), w, "Z", 12), 3))
    # corner blocks and screws under the seat
    for (x, y) in ((-330, -330), (330, -330), (-330, 400), (330, 400)):
        parts.append((box("Block", (x - 40) * U, (x + 40) * U, (y - 40) * U, (y + 40) * U, 770 * U, 800 * U, w, 0.001, 1), 0))
    return parts, hs

def build_high_chair():
    """child's high chair: beech frame on four splayed legs, teal seat pad and back pad, footrest, white tray (floor 0.921 m) at the front"""
    hs = Hulls(); parts = []
    beech = M("Beech", wood, (0.74, 0.55, 0.34), (0.52, 0.36, 0.20), "X", 26.0, 0.3, 0.3, 0.35); bz = M("BeechZ", wood, (0.74, 0.55, 0.34), (0.52, 0.36, 0.20), "Z", 26.0, 0.3, 0.3, 0.35)
    teal = M("TealVinyl", lambda n: H.plastic(n, (0.06, 0.38, 0.40), 0.45, 0.05, 0.25))
    for sx in (-1, 1):
        for sy in (-1, 1):
            p0, p1 = Vector((sx * 430 * U, sy * 430 * U, 0)), Vector((sx * 300 * U, sy * 300 * U, 1450 * U))
            ax = p1 - p0; ln = ax.length; ax.normalize()
            lg = lathe("Leg", [(0, 0.0), (0.014, 0.0), (0.016, 0.03), (0.016, ln), (0, ln)], bz, (0, 0, 0), 16)
            lg.rotation_mode = "QUATERNION"; lg.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(ax); lg.location = p0
            parts.append((lg, 1)); hs.add(lg)
            parts.append((cyl("Foot", 0.020, 0.016, (sx * 430 * U, sy * 430 * U, 0.008), BLACKP(), "Z", 14), 1))
    seat = box("Seat", -360 * U, 360 * U, -360 * U, 360 * U, 1440 * U, 1500 * U, beech, 0.004, 3); parts.append((seat, 0)); hs.add(seat)
    parts.append((soft("Pad", -300 * U, 300 * U, -300 * U, 300 * U, 1500 * U, 1530 * U, teal, 0.008, 3, 0.3, 1), 0))
    bk = box("Back", -360 * U, 360 * U, 300 * U, 360 * U, 1500 * U, 2640 * U, bz, 0.005, 3); parts.append((bk, 2)); hs.add(bk)
    parts.append((soft("BackPad", -280 * U, 280 * U, 286 * U, 302 * U, 1700 * U, 2500 * U, teal, 0.006, 3, 0.25, 1), 2))
    parts.append((cyl("Roll", 60 * U, 720 * U, (0, 330 * U, 2640 * U), beech, "X", 24), 2))
    for sx in (-1, 1):
        parts.append((box("ArmRest", sx * 330 * U - 30 * U, sx * 330 * U + 30 * U, -360 * U, 360 * U, 1690 * U, 1740 * U, beech, 0.004, 3), 0))
        parts.append((cyl("ArmPost", 0.011, 200 * U, (sx * 330 * U, -340 * U, 1600 * U), bz, "Z", 14), 0))
    parts.append((box("Footrest", -420 * U, 420 * U, -470 * U, -300 * U, 600 * U, 640 * U, beech, 0.003, 3), 1))
    tw = M("TrayWhite", lambda n: H.plastic(n, (0.90, 0.89, 0.85), 0.30, 0.03, 0.25))
    tray = box("TrayFloor", -440 * U, 440 * U, -700 * U, -340 * U, 1710 * U, 1750 * U, tw, 0.004, 3); parts.append((tray, 3)); hs.add(tray)
    for (x0, x1, y0, y1) in ((-440, 440, -700, -660), (-440, 440, -380, -340), (-440, -400, -700, -340), (400, 440, -700, -340)):
        parts.append((soft("TrayRim", x0 * U, x1 * U, y0 * U, y1 * U, 1710 * U, 1790 * U, tw, 0.008, 3, 0.0, 0), 3))
    parts.append((pipe("Strap", [[(-0.05, -300 * U, 1530 * U), (-0.05, -0.12, 0.80), (0.0, -0.10, 0.84), (0.05, -0.12, 0.80), (0.05, -300 * U, 1530 * U)]], 0.004, M("Webbing", fabric, (0.35, 0.30, 0.25), None, 400.0, 0.2)), 0))
    return parts, hs

def build_dog_bed():
    """oval dog bed 0.89 x 0.69 x 0.17 m: a red quilted bolster round a cream cushion, a non-slip base"""
    hs = Hulls(); parts = []
    red = M("DogRed", fabric, (0.50, 0.08, 0.07), (0.32, 0.05, 0.04), 220.0, 0.6); cream = M("Sherpa", fabric, (0.86, 0.80, 0.68), (0.70, 0.64, 0.52), 500.0, 0.8)
    ring = []
    for k in range(48):
        a = 2 * math.pi * k / 48; ring.append((0.36 * math.cos(a), 0.36 * 0.78 * math.sin(a), 0.085))
    b = H.tubes("Bolster", [ring + [ring[0]]], 0.085, res_u=6, bevel_res=6, tip=1.0); b.data.materials.append(red); b.data.splines[0].use_cyclic_u = True
    parts.append((b, 1)); hs.add(b)
    c = lathe("Cushion", [(0.0, 0.004), (0.33, 0.004), (0.36, 0.03), (0.36, 0.05), (0.30, 0.075), (0.10, 0.088), (0.0, 0.09)], cream, (0, 0, 0), 48); c.scale = (1, 0.78, 1)
    parts.append((c, 0))
    parts.append((lathe("Base", [(0.0, 0.0), (0.38, 0.0), (0.385, 0.006), (0.0, 0.006)], H.rubber("NonSlip", (0.08, 0.07, 0.07), 0.9), (0, 0, 0), 48), 0))
    # quilting seams across the bolster
    for k in range(16):
        a = 2 * math.pi * k / 16
        cx, cy = 0.36 * math.cos(a), 0.36 * 0.78 * math.sin(a); dx, dy = math.cos(a), 0.78 * math.sin(a); l = math.hypot(dx, dy); dx /= l; dy /= l
        pts = [(cx + dx * 0.087 * math.cos(t), cy + dy * 0.087 * math.cos(t), 0.085 + 0.087 * math.sin(t)) for t in [math.pi * i / 10 for i in range(11)]]
        parts.append((pipe("Quilt", [pts], 0.003, red), 1))
    return parts, hs

def build_scratch_post():
    """cat scratching post 0.44 x 0.44 x 0.69 m: carpeted base, sisal-wound post, platform with a cushion, a ball on a cord"""
    hs = Hulls(); parts = []
    felt = M("CarpetGrey", fabric, (0.45, 0.47, 0.44), (0.32, 0.34, 0.31), 300.0, 0.8); sisal = M("SisalRope", fabric, (0.72, 0.60, 0.38), (0.50, 0.40, 0.24), 120.0, 0.2)
    bs = soft("Base", -0.22, 0.22, -0.22, 0.22, 0.0, 0.04, felt, 0.012, 3, 0.0, 0); parts.append((bs, 0)); hs.add(bs)
    turns = []
    for i in range(int(0.60 / 0.0085)):
        z = 0.04 + 0.0042 + i * 0.0085
        turns += [(0.052 * math.cos(2 * math.pi * k / 16), 0.052 * math.sin(2 * math.pi * k / 16), z + 0.0085 * k / 16) for k in range(16)]
    rope = H.tubes("Rope", [turns], 0.0045, res_u=1, bevel_res=1, tip=1.0); rope.data.materials.append(sisal); parts.append((rope, 1))
    core = cyl("Core", 0.048, 0.60, (0, 0, 0.34), M("Cardboard", lambda n: H.plastic(n, (0.55, 0.42, 0.28), 0.8, 0.02, 0.1)), "Z", 20); parts.append((core, 1)); hs.add(core)
    pf = soft("Platform", -0.17, 0.17, -0.17, 0.17, 0.64, 0.675, felt, 0.010, 3, 0.0, 0); parts.append((pf, 2)); hs.add(pf)
    parts.append((soft("Pad", -0.15, 0.15, -0.15, 0.15, 0.672, 0.69, M("Sherpa", fabric, (0.86, 0.80, 0.68), (0.70, 0.64, 0.52), 500.0, 0.8), 0.007, 3, 0.4, 1), 2))
    parts.append((pipe("String", [[(0.165, -0.10, 0.655), (0.19, -0.10, 0.62), (0.19, -0.10, 0.45)]], 0.0015, sisal), 2))
    ball = lathe("Ball", [(0, 0.0)] + [(0.03 * math.sin(math.pi * k / 12), 0.03 - 0.03 * math.cos(math.pi * k / 12)) for k in range(1, 12)] + [(0, 0.06)], M("BallRed", fabric, (0.6, 0.06, 0.05), None, 300.0, 0.6), (0.19, -0.10, 0.39), 24)
    parts.append((ball, 2))
    return parts, hs


def build_window():
    """window lining in the 3000 x 2000 uu opening (no glass on purpose: the game's window is an open hole), white-painted
    timber: lining, mullion and transom with putty beads, an architrave on the room side (-Y), a brass casement stay and handle"""
    hs = Hulls(); parts = []; wp = M("WindowPaint", paint, (0.90, 0.89, 0.85), 0.32, 0.03, 0.2, (0.70, 0.66, 0.58)); br = BRASS()
    oy = -88 * U
    def ub(x0, x1, y0, y1, z0, z1, m=wp, bev=0.003, part=0):
        o = box("W", x0 * U, x1 * U, y0 * U + oy, y1 * U + oy, z0 * U, z1 * U, m, bev, 3); parts.append((o, part)); return o
    for x0, x1 in ((-1500, -1440), (1440, 1500)): hs.add(ub(x0, x1, -20, 200, 0, 2000, part=1))
    hs.add(ub(-1440, 1440, -20, 200, 1940, 2000, part=1)); hs.add(ub(-1440, 1440, -20, 200, 0, 60, part=1))
    hs.add(ub(-30, 30, 60, 120, 60, 1940, part=2)); hs.add(ub(-1440, 1440, 60, 120, 970, 1030, part=2))
    for x0, x1 in ((-1610, -1500), (1500, 1610)): hs.add(ub(x0, x1, -24, 0, 0, 2110, bev=0.004, part=3))
    hs.add(ub(-1610, 1610, -24, 0, 2000, 2110, bev=0.004, part=3))
    # putty / glazing beads round the four lights (the glass is out), the casement stay and an espagnolette handle
    putty = M("Putty", lambda n: H.plastic(n, (0.82, 0.80, 0.74), 0.7, 0.05, 0.1))
    for (x0, x1, z0, z1) in ((-1440, -30, 60, 970), (30, 1440, 60, 970), (-1440, -30, 1030, 1940), (30, 1440, 1030, 1940)):
        parts.append((pipe("Bead", [rot_xz(rect_loop((x0 + 6) * U, (x1 - 6) * U, (z0 + 6) * U, (z1 - 6) * U, 60 * U + oy))], 0.004, putty, True), 2))
    parts.append((box("Stay", 400 * U, 700 * U, 50 * U + oy, 58 * U + oy, 1000 * U, 1014 * U, br, 0.001, 2), 2))
    parts.append((box("Handle", -12 * U, 12 * U, 30 * U + oy, 60 * U + oy, 1150 * U, 1350 * U, br, 0.003, 3), 2))
    parts.append((cyl("HandleRose", 0.012, 0.004, (0, 58 * U + oy, 1250 * U), br, "Y", 16), 2))
    return parts, hs

def build_window_sill():
    """inner window sill 1.737 x 0.095 x 0.042 m: oak with a rounded nosing, a drip groove under it, rings of the plant pot"""
    hs = Hulls(); parts = []
    b = box("Sill", -1650 * U, 1650 * U, -90 * U, 90 * U, 0, 80 * U, W_OAK("X"), 0.006, 4); parts.append((b, 0)); hs.add(b)
    parts.append((box("Drip", -1600 * U, 1600 * U, -80 * U, -76 * U, -0.0002, 0.003, M("Hole", H.rubber, (0.02, 0.015, 0.01), 0.9), 0.0, 0), 0))
    stain = M("Stain", lambda n: H.plastic(n, (0.30, 0.17, 0.07), 0.25, 0.02, 0.1))
    parts.append((lathe("Ring", [(0.031, 80 * U + 0.0002), (0.035, 80 * U + 0.0002), (0.035, 80 * U + 0.0004), (0.031, 80 * U + 0.0004)], stain, (-0.62, 0.0, 0), 40), 0))
    return parts, hs

def pillow_shell(name, w, h, dome, rim, m, nx=26, nz=22):
    """a stuffed pillow: two domed faces sewn together at the rim (corners pinched), standing in the XZ plane, z 0..h"""
    bm = bmesh.new()
    def f(u, v): return (max(0.0, 1 - u * u) * max(0.0, 1 - v * v)) ** 0.6
    grids = []
    for s in (-1, 1):
        g = []
        for i in range(nx + 1):
            row = []
            for k in range(nz + 1):
                u, v = 2 * i / nx - 1, 2 * k / nz - 1
                x = u * w / 2 * (1 - 0.06 * (1 - v * v)); z = h / 2 + v * h / 2 * (1 - 0.06 * (1 - u * u))   # (the seams pull in)
                row.append(bm.verts.new((x, s * (rim + dome * f(u, v)), z)))
            g.append(row)
        grids.append(g)
    for s, g in zip((-1, 1), grids):
        for i in range(nx):
            for k in range(nz):
                q = (g[i][k], g[i + 1][k], g[i + 1][k + 1], g[i][k + 1])
                bm.faces.new(q if s > 0 else q[::-1])
    a, b = grids
    ring = [(i, 0) for i in range(nx)] + [(nx, k) for k in range(nz)] + [(i, nz) for i in range(nx, 0, -1)] + [(0, k) for k in range(nz, 0, -1)]
    for j in range(len(ring)):
        (i0, k0), (i1, k1) = ring[j], ring[(j + 1) % len(ring)]
        bm.faces.new((a[i0][k0], a[i1][k1], b[i1][k1], b[i0][k0]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = H.link(bpy.data.objects.new(name, me)); o.data.materials.append(m)
    s_ = o.modifiers.new("sub", "SUBSURF"); s_.levels = 1; s_.render_levels = 1
    for p in o.data.polygons: p.use_smooth = True
    return o

def build_pillow(main, accent):
    """throw pillow 0.38 x 0.12 x 0.33 m (origin bottom centre): two domed faces, pinched corners, piped seam, a covered button each side"""
    hs = Hulls(); parts = []
    c = pillow_shell("Pillow", 0.38, 0.33, 0.052, 0.006, main); parts.append((c, 0)); hs.add(c)
    seam = [(x * (1 - 0.06 * (1 - v * v)), 0.0, 0.165 + 0.165 * v * (1 - 0.06 * (1 - (x / 0.19) ** 2))) for x, v in
            [(-0.19 + 0.38 * t / 40, -1.0) for t in range(41)] + [(0.19, -1 + 2 * t / 30) for t in range(1, 31)] +
            [(0.19 - 0.38 * t / 40, 1.0) for t in range(1, 41)] + [(-0.19, 1 - 2 * t / 30) for t in range(1, 30)]]
    parts.append((pipe("Piping", [seam], 0.0055, main, True), 0))
    for s in (-1, 1):
        parts.append((lathe("Button", [(0, 0.0), (0.020, 0.0), (0.022, 0.004), (0.016, 0.010), (0, 0.011)], accent, (0, s * 0.054, 0.165), 20, 0, (-s * math.pi / 2, 0, 0)), 0))
        for k in range(6):     # the pleats the button pulls in
            a_ = k * math.pi / 3
            parts.append((pipe("Pleat", [[(0.024 * math.cos(a_), s * 0.052, 0.165 + 0.024 * math.sin(a_)), (0.07 * math.cos(a_), s * 0.046, 0.165 + 0.07 * math.sin(a_))]], 0.002, main), 0))
    return parts, hs

def build_floor_cushion():
    """big floor cushion 0.58 x 0.58 x 0.16 m: honey corduroy, piped, a carry handle"""
    hs = Hulls(); parts = []
    cord = M("Corduroy", fabric, (0.72, 0.48, 0.16), (0.52, 0.32, 0.08), 120.0, 0.7)
    c = soft("Cushion", -0.2895, 0.2895, -0.2895, 0.2895, 0.026, 0.11, cord, 0.04, 4, 0.5, 2); parts.append((c, 0)); hs.add(c)
    for z in (0.034, 0.102):
        parts.append((pipe("Piping", [rect_loop(-0.283, 0.283, -0.283, 0.283, z, 0.06, 6)], 0.0055, cord, True), 0))
    parts.append((pipe("Handle", [[(-0.05, -0.29, 0.10), (-0.04, -0.31, 0.11), (0.04, -0.31, 0.11), (0.05, -0.29, 0.10)]], 0.006, cord), 0))
    return parts, hs


def build_cat_flap():
    """cat flap liner in the wall 0.116 x 0.286 x 0.318 m (the wall's thickness along X): white plastic tunnel with a rim on
    both faces, the hinge bosses at the top, a magnet strip at the bottom, four screw caps"""
    hs = Hulls(); parts = []
    wp = M("WhitePlastic", lambda n: H.plastic(n, (0.88, 0.87, 0.83), 0.35, 0.03, 0.2)); dk = M("Hole", H.rubber, (0.02, 0.015, 0.01), 0.9)
    X, Y, Z = 0.0579, 0.1432, 0.3179; t = 0.0116
    for (y0, y1, z0, z1) in ((-Y, Y, 0, t), (-Y, Y, Z - t, Z), (-Y, -Y + t, 0, Z), (Y - t, Y, 0, Z)):
        o = box("Tunnel", -X + 0.006, X - 0.006, y0, y1, z0, z1, wp, 0.002, 2); parts.append((o, 0)); hs.add(o)
    for s in (-1, 1):     # the rims (flanges) on both faces of the wall
        for (y0, y1, z0, z1) in ((-Y, Y, 0, t * 1.6), (-Y, Y, Z - t * 1.6, Z), (-Y, -Y + t * 1.6, 0, Z), (Y - t * 1.6, Y, 0, Z)):
            parts.append((box("Rim", min(s * X, s * (X - 0.006)), max(s * X, s * (X - 0.006)), y0, y1, z0, z1, wp, 0.0025, 3), 0))
        for (y, z) in ((-Y + 0.009, 0.009), (Y - 0.009, 0.009), (-Y + 0.009, Z - 0.009), (Y - 0.009, Z - 0.009)):
            parts.append((cyl("ScrewCap", 0.0045, 0.0012, (s * (X + 0.0004), y, z), wp, "X", 14), 0))
    for y in (-0.128, 0.128):
        parts.append((cyl("Boss", 0.006, 0.010, (0.0, y + (0.006 if y < 0 else -0.006), Z - t - 0.004), wp, "Y", 16), 0))
    parts.append((box("Magnet", -0.004, 0.004, -0.06, 0.06, t, t + 0.003, M("MagnetGrey", H.metal, (0.35, 0.35, 0.37), 0.5), 0.0008, 1), 0))
    parts.append((box("Shadow", -X + 0.0062, X - 0.0062, -Y + t, Y - t, t - 0.0003, t, dk, 0.0, 0), 0))
    return parts, hs

def build_cat_flap_leaf():
    """the flap 0.017 x 0.257 x 0.288 m, pivot on its top edge: smoked clear plastic in a white frame, a weighted bottom bar
    with the magnet, scratch marks of claws"""
    hs = Hulls(); parts = []
    wp = M("WhitePlastic", lambda n: H.plastic(n, (0.88, 0.87, 0.83), 0.35, 0.03, 0.2))
    smoke = M("SmokedPlastic", lambda n: H.tag(glossy(n, (0.18, 0.17, 0.15), 0.12, 0.6), 0.0, 0.0))
    W, Hh = 0.1284, 0.2884
    pn = box("Pane", -0.0035, 0.0035, -W + 0.012, W - 0.012, -Hh + 0.024, -0.012, smoke, 0.0015, 2); parts.append((pn, 0))
    for (y0, y1, z0, z1) in ((-W, W, -0.014, 0.0), (-W, -W + 0.012, -Hh, 0.0), (W - 0.012, W, -Hh, 0.0)):
        parts.append((box("Frame", -0.0060, 0.0060, y0, y1, z0, z1, wp, 0.002, 2), 0))
    parts.append((box("Weight", -0.0084, 0.0084, -W, W, -Hh, -Hh + 0.026, wp, 0.004, 3), 0))
    parts.append((cyl("Pivot", 0.0035, 2 * W + 0.012, (0, 0, -0.006), M("Steel", H.metal, (0.66, 0.67, 0.69), 0.32), "Y", 12), 0))
    for k in range(4):    # claw scratches on the pane
        y = -0.03 + k * 0.012
        parts.append((box("Claw", -0.0037, -0.0035, y - 0.0005, y + 0.0005, -Hh + 0.06, -Hh + 0.12, M("ClawMark", lambda n: H.plastic(n, (0.5, 0.5, 0.5), 0.8, 0.0, 0.0)), 0.0, 0), 0))
    hs.box(-0.0084, 0.0084, -W, W, -Hh, 0.0)
    return parts, hs

def build_toy_box():
    """wooden toy box 0.739 x 0.476 x 0.41 m (hinge side +Y): beech carcass with finger joints at the corners, painted panels
    (red, blue, honey, green) with stencilled letters, rope handles, a lid stay; the lid is SM_V3_ToyBox_Lid"""
    hs = Hulls(); parts = []
    bz = M("BeechZ", wood, (0.74, 0.55, 0.34), (0.52, 0.36, 0.20), "Z", 26.0, 0.3, 0.3, 0.35)
    bx_ = M("Beech", wood, (0.74, 0.55, 0.34), (0.52, 0.36, 0.20), "X", 26.0, 0.3, 0.3, 0.35)
    X, Y, Z, t = 0.3695, 0.2381, 0.4105, 0.016
    for s in (-1, 1):
        o = box("Side", min(s * X, s * (X - t)), max(s * X, s * (X - t)), -Y, Y, 0.0, Z, bz, 0.003, 3); parts.append((o, 1 + (s > 0))); hs.add(o)
        o = box("Front", -X + t, X - t, min(s * Y, s * (Y - t)), max(s * Y, s * (Y - t)), 0.0, Z, bx_, 0.003, 3); parts.append((o, 3 + (s > 0))); hs.add(o)
    o = box("Bottom", -X + t, X - t, -Y + t, Y - t, 0.012, 0.012 + t, bx_, 0.002, 2); parts.append((o, 5)); hs.add(o)
    for (x, y) in ((-X + 0.03, -Y + 0.03), (X - 0.03, -Y + 0.03), (-X + 0.03, Y - 0.03), (X - 0.03, Y - 0.03)):
        parts.append((cyl("Foot", 0.014, 0.012, (x, y, 0.006), M("Felt", fabric, (0.30, 0.28, 0.25), None, 900.0, 0.2), "Z", 12), 5))
    # finger joints: the end grain of the fingers shows on the corners
    eg = M("EndGrain", wood, (0.62, 0.44, 0.26), (0.45, 0.30, 0.17), "X", 6.0, 0.0, 0.0, 0.0, 0.6)
    for sx in (-1, 1):
        for sy in (-1, 1):
            for k in range(5):
                z0 = k * Z / 5 + (Z / 10 if (sx * sy > 0) else 0)
                parts.append((box("Finger", sx * X - (0.0003 if sx > 0 else -0.0003) - 0.0002, sx * X + 0.0002 - (0.0003 if sx > 0 else -0.0003), min(sy * Y, sy * (Y - t)), max(sy * Y, sy * (Y - t)), z0, z0 + Z / 10, eg, 0.0, 0), 1 + (sx > 0)))
    # painted panels with letters on the front (-Y) and the sides
    cols = [(0.62, 0.10, 0.07), (0.10, 0.24, 0.58), (0.80, 0.56, 0.14), (0.20, 0.46, 0.20)]
    for i, c in enumerate(cols):
        pm = M("ToyPaint%d" % i, paint, c, 0.38, 0.03, 0.45, (0.74, 0.55, 0.34))
        x0 = -X + t + 0.02 + i * (2 * (X - t) - 0.04) / 4; x1 = x0 + (2 * (X - t) - 0.04) / 4 - 0.012
        parts.append((box("Panel", x0, x1, -Y - 0.0012, -Y + 0.0002, 0.07, Z - 0.07, pm, 0.001, 1), 3))
        # a letter block shape stencilled in white (A B C D: blocky strokes)
        wh = M("StencilWhite", paint, (0.92, 0.90, 0.84), 0.4, 0.02, 0.3)
        cx, cz = (x0 + x1) / 2, Z / 2
        strokes = {0: [(-0.03, 0, 0.012, 0.12, 0.3), (0.03, 0, 0.012, 0.12, -0.3), (0, -0.01, 0.05, 0.012, 0)],
                   1: [(-0.025, 0, 0.012, 0.12, 0), (0.0, 0.05, 0.05, 0.012, 0), (0.0, 0.0, 0.05, 0.012, 0), (0.0, -0.05, 0.05, 0.012, 0), (0.025, 0.025, 0.012, 0.05, 0), (0.025, -0.025, 0.012, 0.05, 0)],
                   2: [(-0.025, 0, 0.012, 0.12, 0), (0.0, 0.05, 0.05, 0.012, 0), (0.0, -0.05, 0.05, 0.012, 0)],
                   3: [(-0.025, 0, 0.012, 0.12, 0), (0.0, 0.05, 0.04, 0.012, 0), (0.0, -0.05, 0.04, 0.012, 0), (0.025, 0, 0.012, 0.09, 0)]}[i]
        for (dx, dz, w, h, ang) in strokes:
            o = box("Letter", cx + dx - w / 2, cx + dx + w / 2, -Y - 0.0016, -Y - 0.0010, cz + dz - h / 2, cz + dz + h / 2, wh, 0.0, 0,
                    Matrix.Rotation(ang, 4, "Y"), (cx + dx, -Y, cz + dz))
            parts.append((o, 3))
    rope = M("Rope", fabric, (0.72, 0.62, 0.44), (0.52, 0.42, 0.28), 140.0, 0.2)
    for s in (-1, 1):     # rope handles through the sides, knotted inside
        pts = [(s * (X + 0.0005), -0.06, Z - 0.09)] + [(s * (X + 0.02 + 0.012 * math.sin(math.pi * k / 8)), -0.06 + 0.12 * k / 8, Z - 0.09 - 0.03 * math.sin(math.pi * k / 8)) for k in range(9)] + [(s * (X + 0.0005), 0.06, Z - 0.09)]
        parts.append((pipe("Handle", [pts], 0.006, rope), 1 + (s > 0)))
        for y in (-0.06, 0.06):
            parts.append((lathe("Knot", [(0, 0), (0.010, 0.002), (0.011, 0.01), (0.006, 0.016), (0, 0.017)], rope, (s * (X - t - 0.0005), y, Z - 0.09), 12, 0, (0, -s * math.pi / 2, 0)), 1 + (s > 0)))
    return parts, hs

def build_toy_box_lid():
    """the lid 0.758 x 0.492 x 0.032 m, origin on the hinge axis at the back edge (extends to -Y): beech boards with
    breadboard ends, a painted star, a finger pull, two hinge leaves screwed on"""
    hs = Hulls(); parts = []
    bx_ = M("Beech", wood, (0.74, 0.55, 0.34), (0.52, 0.36, 0.20), "X", 26.0, 0.3, 0.3, 0.35); by = M("BeechY", wood, (0.74, 0.55, 0.34), (0.52, 0.36, 0.20), "Y", 26.0, 0.3, 0.3, 0.35)
    X, D, T = 0.3789, 0.4921, 0.0316
    top = box("Boards", -X + 0.05, X - 0.05, -D, 0.0, 0.0, T, by, 0.003, 3); parts.append((top, 0)); hs.add(top)
    for s in (-1, 1):
        parts.append((box("End", min(s * X, s * (X - 0.05)), max(s * X, s * (X - 0.05)), -D, 0.0, 0.0, T, bx_, 0.004, 3), 0))
    seam = M("Seam", H.rubber, (0.10, 0.06, 0.03), 0.8)
    for x in (-0.14, 0.0, 0.14):
        parts.append((box("Seam", x - 0.0006, x + 0.0006, -D + 0.002, -0.002, T - 0.0003, T + 0.0002, seam, 0.0, 0), 0))
    st = M("ToyPaint2", paint, (0.80, 0.56, 0.14), 0.38, 0.03, 0.45, (0.74, 0.55, 0.34))
    star = [(0.09 * (1 if k % 2 == 0 else 0.42) * math.cos(math.pi / 2 + k * math.pi / 5), 0.09 * (1 if k % 2 == 0 else 0.42) * math.sin(math.pi / 2 + k * math.pi / 5)) for k in range(10)]
    bm = bmesh.new(); vs = [bm.verts.new((x, -D / 2 + y, T + 0.0003)) for x, y in star]; bm.faces.new(vs)
    me = bpy.data.meshes.new("Star"); bm.to_mesh(me); bm.free(); so = H.link(bpy.data.objects.new("Star", me)); so.data.materials.append(st)
    sol = so.modifiers.new("sol", "SOLIDIFY"); sol.thickness = 0.0006; parts.append((so, 0))
    parts.append((box("Pull", -0.05, 0.05, -D - 0.0005, -D + 0.012, T * 0.35, T * 0.75, M("Hole", H.rubber, (0.02, 0.015, 0.01), 0.9), 0.003, 2), 0))
    br = BRASS()
    for x in (-0.20, 0.20):
        parts.append((box("HingeLeaf", x - 0.025, x + 0.025, -0.032, 0.0, -0.0016, 0.0, br, 0.0006, 1), 0))
        parts += [(o, 0) for o in screws_at("HScrew", [(x + dx, -0.016, -0.0017, -1) for dx in (-0.015, 0.015)], "Z", br, 0.0028)]
    return parts, hs

def build_hinge():
    """concealed cup hinge (the toy box lid's): 35 mm cup, arm, mounting plate with three screws, pivot on the axis"""
    hs = Hulls(); parts = []; st = steel(); bk = BLACKP()
    parts.append((lathe("Cup", [(0, -0.0100), (0.0120, -0.0100), (0.0125, -0.0005), (0.0145, 0.0), (0.0145, 0.0012), (0, 0.0012)], st, (0, -0.030, 0), 32, 0, (math.pi / 2, 0, 0)), 0))
    arm = box("Arm", -0.012, 0.012, -0.040, -0.010, -0.006, 0.006, st, 0.002, 2); parts.append((arm, 0)); hs.add(arm)
    pl = box("Plate", -0.0158, 0.0158, -0.006, 0.0, -0.0263, 0.0263, st, 0.0015, 2); parts.append((pl, 0)); hs.add(pl)
    parts.append((cyl("CoverCap", 0.006, 0.026, (0, -0.040, 0), bk, "X", 14), 0))
    parts += [(o, 0) for o in screws_at("PScrew", [(0, -0.0062, z, -1) for z in (-0.017, 0.0, 0.017)], "Y", st, 0.0032)]
    return parts, hs


# ================================================================================================ tiling cloth (AntCloth pieces)
# AntCloth (curtains, the throw) simulates its own grid with UVs that repeat every 300 uu: these pieces get a seamless
# 300 uu tile (0.158 m) of their fabric instead of an atlas, and the static mesh is mapped the same way.
TILE = 300.0 / S

def tile_fabric(name, base, dark, threads=48, stripes=None, sheen=0.5, slub=0.3):
    """a seamless square of woven cloth on UV 0..1: plain weave of `threads` per tile, slub and heather from noise on a
    torus (periodic in u and v), optional woven stripes / checks: stripes = [(pos0, pos1, colour, axis 'u'|'v'|'uv')]"""
    m, p = H.mat(name)
    tc = N(m, "ShaderNodeTexCoord", -2600); sp = N(m, "ShaderNodeSeparateXYZ", -2400); L(m, tc.outputs["UV"], sp.inputs[0])
    def op(o, a, b=None, x=0, y=0):
        n = N(m, "ShaderNodeMath", x, y); n.operation = o
        for i, s in enumerate((a, b)):
            if s is None: continue
            if isinstance(s, (int, float)): n.inputs[i].default_value = s
            else: L(m, s, n.inputs[i])
        return n.outputs[0]
    tau = 2 * math.pi
    U_, V_ = sp.outputs["X"], sp.outputs["Y"]
    def torus_noise(scale, detail, y, seed=0.0):
        cu, su = op("COSINE", op("MULTIPLY", U_, tau, -2200, y), None, -2000, y), op("SINE", op("MULTIPLY", U_, tau, -2200, y - 40), None, -2000, y - 40)
        cv, sv = op("COSINE", op("MULTIPLY", V_, tau, -2200, y - 80), None, -2000, y - 80), op("SINE", op("MULTIPLY", V_, tau, -2200, y - 120), None, -2000, y - 120)
        cb = N(m, "ShaderNodeCombineXYZ", -1800, y); L(m, cu, cb.inputs[0]); L(m, su, cb.inputs[1]); L(m, cv, cb.inputs[2])
        no = N(m, "ShaderNodeTexNoise", -1600, y); no.noise_dimensions = "4D"; no.inputs["Scale"].default_value = scale; no.inputs["Detail"].default_value = detail
        L(m, cb.outputs[0], no.inputs["Vector"]); L(m, op("ADD", sv, seed, -1800, y - 120), no.inputs["W"])
        return no.outputs["Fac"]
    wu = op("SINE", op("MULTIPLY", U_, tau * threads, -2200, 600), None, -2000, 600)
    wv = op("SINE", op("MULTIPLY", V_, tau * threads, -2200, 500), None, -2000, 500)
    over = op("MULTIPLY", op("SINE", op("MULTIPLY", U_, tau * threads / 2, -2200, 400), None, -2000, 400), op("SINE", op("MULTIPLY", V_, tau * threads / 2, -2200, 300), None, -2000, 300), -1800, 350)
    weave = op("ADD", op("MULTIPLY", op("ABSOLUTE", wu, None, -1800, 600), op("GREATER_THAN", over, 0.0, -1600, 350), -1600, 600),
               op("MULTIPLY", op("ABSOLUTE", wv, None, -1800, 500), op("LESS_THAN", over, 0.0, -1600, 300), -1600, 500), -1400, 550)
    hz = torus_noise(2.5, 6, 0); fl = torus_noise(14.0, 2, -300, 3.0)
    mixn = N(m, "ShaderNodeMix", -1300, 0); mixn.data_type = "FLOAT"; mixn.inputs[0].default_value = 0.4; L(m, hz, mixn.inputs[2]); L(m, fl, mixn.inputs[3])
    cr = N(m, "ShaderNodeValToRGB", -1100, 0); e = cr.color_ramp.elements; e[0].position = 0.5 - slub; e[0].color = (*dark, 1); e[1].position = 0.5 + slub; e[1].color = (*base, 1)
    L(m, mixn.outputs[0], cr.inputs["Fac"]); col = cr.outputs["Color"]
    for i, (a, b, c, ax) in enumerate(stripes or []):
        msk = None
        for coord in ((U_,) if ax == "u" else (V_,) if ax == "v" else (U_, V_)):
            s_ = op("MULTIPLY", op("GREATER_THAN", coord, a, -1000, -400 - i * 90), op("LESS_THAN", coord, b, -1000, -440 - i * 90), -850, -420 - i * 90)
            msk = s_ if msk is None else op("MAXIMUM", msk, s_, -700, -420 - i * 90)
        mx = N(m, "ShaderNodeMix", -550, -200 - i * 90); mx.data_type = "RGBA"; mx.inputs[7].default_value = (*c, 1)
        mm = op("MULTIPLY", msk, 0.85, -650, -420 - i * 90)
        L(m, mm, mx.inputs[0]); L(m, col, mx.inputs[6]); col = mx.outputs[2]
    sh = N(m, "ShaderNodeMix", -300, 0); sh.data_type = "RGBA"; sh.blend_type = "MULTIPLY"; sh.inputs[7].default_value = (0.82, 0.82, 0.82, 1)
    L(m, op("SUBTRACT", 1.0, weave, -500, 200), sh.inputs[0]); L(m, col, sh.inputs[6]); L(m, sh.outputs[2], p.inputs["Base Color"])
    bp = N(m, "ShaderNodeBump", -300, -500); bp.inputs["Strength"].default_value = 0.6; bp.inputs["Distance"].default_value = 0.0006
    L(m, op("MULTIPLY_ADD", fl, 0.3, -600, -500) if False else weave, bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    p.inputs["Roughness"].default_value = 0.9; p.inputs["Sheen Weight"].default_value = sheen; p.inputs["Sheen Roughness"].default_value = 0.45
    return H.tag(m, 0.0, 1.0)

def old_mesh(name):
    obs = import_old(name)
    o = H.join(obs, name + "_src") if len(obs) > 1 else obs[0]
    for x in H.S().objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return o

def tile_uv(o, mode="xz"):
    """UV0 in tiles of 300 uu: xz = the curtain's face (u along X, v up), box = by the dominant normal"""
    me = o.data
    uv = me.uv_layers.get("UV0") or me.uv_layers.new(name="UV0")
    while len(me.uv_layers) > 1:
        me.uv_layers.remove([u for u in me.uv_layers if u.name != "UV0"][0])
    for p in me.polygons:
        n = p.normal
        for li in p.loop_indices:
            c = me.vertices[me.loops[li].vertex_index].co
            if mode == "xz" or (abs(n.y) >= abs(n.x) and abs(n.y) >= abs(n.z)): a, b = c.x, c.z
            elif abs(n.x) >= abs(n.z): a, b = c.y, c.z
            else: a, b = c.x, c.y
            uv.data[li].uv = (a / TILE, b / TILE)

def produce_tile(name, mat, mode, extra=None, note="", lods=(0.4, 0.15)):
    t0 = time.time()
    out = os.path.join(OUT, name); os.makedirs(out, exist_ok=True); tdir = os.path.join(out, "tex")
    # 1. the tile
    bpy.ops.mesh.primitive_plane_add(size=1, location=(0, 0, -5)); pl = bpy.context.object; pl.name = name + "_tile"
    pl.data.uv_layers[0].name = "UV0"; pl.data.materials.append(mat)
    if NOBAKE and os.path.exists(os.path.join(tdir, "T_%s_C.png" % name)):
        paths = {k: os.path.join(tdir, "T_%s_%s.png" % (name, k)) for k in "CNR"}
    else:
        paths = H.bake_set(pl, name, tdir, 1024, grime=0.0)
    bpy.data.objects.remove(pl)
    # 2. the mesh: the old cloth shape (AntCloth reads its bounds / samples it), hems added, mapped in tiles
    o = old_mesh(name)
    parts = [o] + (extra(o) if extra else [])
    lod0 = H.join(parts, name) if len(parts) > 1 else o
    lod0.name = name; lod0.data.name = name
    H.set_part(lod0, 0)
    tile_uv(lod0, mode)
    H.game_material(lod0, name)
    mn, mx = H.bounds([lod0])
    hs = [H.hull("UCX_%s_00" % name, H.box_pts(mn.x, mx.x, mn.y, mx.y, mn.z, mx.z))]
    l1 = H.lod(lod0, lods[0], name + "_LOD1"); l2 = H.lod(lod0, lods[1], name + "_LOD2")
    H.export_fbx([lod0] + hs, os.path.join(out, name + ".fbx"))
    H.export_fbx([l1], os.path.join(out, name + "_LOD1.fbx")); H.export_fbx([l2], os.path.join(out, name + "_LOD2.fbx"))
    man = {"name": name, "file": name + ".fbx", "set": "hero_living", "break_class": "cloth", "tiling": True,
           "textures": {k: os.path.relpath(v, OUT).replace("\\", "/") for k, v in paths.items()},
           "collision_lod": 0, "movers": [], "breaks": [], "note": note, "kept_slots": []}
    man.update(H.info(lod0)); man["lod_triangles"] = [H.tris(lod0), H.tris(l1), H.tris(l2)]; man["ucx"] = 1
    for x in hs + [l1, l2]: bpy.data.objects.remove(x)
    print("HERO %s (tiling cloth): LOD %s, %.0f s" % (name, man["lod_triangles"], time.time() - t0))
    return man, lod0, [], paths

def curtain_hems(o):
    """a turned hem at the bottom and a heading tape with its pleat hooks at the top (the panel's own width)"""
    mn, mx = H.bounds([o]); cm = M("CurtainHem", tile_fabric, (0.86, 0.80, 0.66), (0.74, 0.68, 0.54), 48)
    tape = M("HeadingTape", lambda n: H.plastic(n, (0.90, 0.89, 0.84), 0.7, 0.02, 0.1))
    out = []
    w = mx.x - mn.x
    out.append(box("Hem", mn.x + 0.002, mx.x - 0.002, (mn.y + mx.y) / 2 - 0.006, (mn.y + mx.y) / 2 + 0.006, mn.z, mn.z + 0.045, cm, 0.003, 2))
    out.append(box("Tape", mn.x + 0.002, mx.x - 0.002, (mn.y + mx.y) / 2 - 0.005, (mn.y + mx.y) / 2 + 0.005, mx.z - 0.08, mx.z - 0.002, tape, 0.002, 2))
    for k in range(6):
        x = mn.x + 0.02 + k * (w - 0.04) / 5
        out.append(pipe("Hook", [[(x, (mn.y + mx.y) / 2, mx.z - 0.02), (x, (mn.y + mx.y) / 2 - 0.008, mx.z + 0.0), (x, (mn.y + mx.y) / 2 - 0.004, mx.z + 0.006)]], 0.0012, steel()))
    return out

def throw_fringe(o):
    """the throw's tasselled ends: little twisted tassels along the two short edges of the old drape (its lowest rows)"""
    mn, mx = H.bounds([o]); out = []
    ym = M("ThrowYarn", fabric, (0.30, 0.34, 0.48), (0.18, 0.21, 0.32), 300.0, 0.4)
    vs = [o.matrix_world @ v.co for v in o.data.vertices]
    for side in (-1, 1):
        edge = [v for v in vs if (v.y - mn.y < 0.02 if side < 0 else mx.y - v.y < 0.02)]
        edge.sort(key=lambda v: v.x)
        R = random.Random(9 + side)
        strands = []
        for v in edge[::max(1, len(edge) // 24)]:
            d = Vector((0, side * 1.0, -0.4)).normalized()
            strands.append([tuple(v), tuple(v + d * 0.02 + Vector((R.uniform(-0.004, 0.004), 0, 0))), tuple(v + d * 0.045 + Vector((R.uniform(-0.006, 0.006), 0, -0.01)))])
        if strands:
            t = H.tubes("Tassels", strands, 0.0025, res_u=2, bevel_res=1, tip=0.6); t.data.materials.append(ym); out.append(t)
    return out

TILE_PIECES = {
    "SM_V3_CurtainPanel": (lambda: M("CurtainLinen", tile_fabric, (0.88, 0.82, 0.68), (0.70, 0.64, 0.50), 40, None, 0.5, 0.42), "xz", curtain_hems,
                           "linen curtain: a seamless 300 uu slub weave (AntCloth tiles it), turned hem, heading tape with hooks"),
    "SM_V31_Throw": (lambda: M("ThrowTartan", tile_fabric, (0.30, 0.34, 0.48), (0.20, 0.23, 0.34), 36,
                               [(0.10, 0.18, (0.74, 0.58, 0.30), "uv"), (0.55, 0.58, (0.80, 0.78, 0.70), "uv"), (0.80, 0.86, (0.14, 0.16, 0.22), "uv")], 0.7, 0.35), "box", throw_fringe,
                     "wool tartan throw: a seamless 300 uu check (AntCloth tiles it), tasselled ends"),
}

# ================================================================================================ the table of pieces
def T(x, y, z): return Matrix.Translation((x, y, z))
SOFA_HD = 1700 / S / 2
SEATS = [T(i * 2000 / S, -SOFA_HD + 40 / S + (1260 / S) / 2, 700 / S) for i in (-1, 0, 1)]
BACKS = [T(i * 2000 / S, -SOFA_HD + 1300 / S + (0.09 + 0.015) / 2, 700 / S) for i in (-1, 0, 1)]
_TW, _TT = 5500 / S, 0.03
_DW = (_TW / 2 - _TT - _TT / 2) / 2 + 0.012
DOORS = [T(-_TW / 2 + _TT + _DW / 2, -1300 / S / 2 + 0.024, 0.054), T(-_TT / 2 - _DW / 2, -1300 / S / 2 + 0.050, 0.054)]
TV_ON = T(0, 350 / S, 1100 / S)

# name: (builder, texture size, breaks, break class, note, review extras [(name, matrices)])
PIECES = {
    "SM_L_Sofa_SeatCushion": (build_seat_cushion, 2048, groups([[0], [1]], [(0, (0.05, 0, 0.07), (1, 0.2, 0), 0.015, 3, 0.0)]), "cloth",
                              "seat cushion: boxed foam in a woven cover, piped, zip at the back", None),
    "SM_L_Sofa_BackCushion": (build_back_cushion, 2048, groups([[0], [1]], [(0, (-0.1, 0, 0.33), (1, 0, 0.3), 0.015, 4, 0.0)]), "cloth",
                              "back cushion: tufted, three covered buttons, piped", None),
    "SM_L_Sofa": (build_sofa, 4096, groups([[0, 1], [2], [3], [4]]), "cloth",
                  "3-seat sofa: rolled piped arms, sprung platform, show-wood rails, turned walnut legs in brass cups, stapled dust cover",
                  [("SM_L_Sofa_SeatCushion", SEATS), ("SM_L_Sofa_BackCushion", BACKS)]),
    "SM_L_Armchair": (build_armchair, 4096, groups([[0, 1], [2], [3], [4], [5], [6]]), "cloth",
                      "mustard armchair: piped arms, loose seat and back cushions, crocheted doily on the back", None),
    "SM_L_CoffeeTable": (build_coffee_table, 4096, groups([[10], [11, 15], [12], [13], [14, 16]], [(0, (0.1, 0.04, 0.5), (0.05, 1, 0.0), 0.006, 11, 0.01)]), "wood",
                         "coffee table: solid oak top and shelf, turned walnut legs, aprons with corner blocks, coffee rings", None),
    "SM_L_TVStand_Door": (build_tvstand_door, 2048, groups([[0], [1]], [(1, (0.05, -0.008, 0.25), (1, 0, 0.5), 0.006, 13, 0.006)]), "wood",
                          "sliding door: green lacquered frame and panel, brass pull, nylon rollers", None),
    "SM_L_TV": (build_tv, 4096, groups([[0], [1], [2], [3]], [(3, (0.3, 0.03, 0.6), (1, 0, 0.4), 0.01, 14, 0.0)]), "plastic",
                "flat TV on a pedestal: bezel, vented back, ports and cord; the Screen slot is kept for the channels", None),
    "SM_L_TVStand": (build_tvstand, 4096, groups([[20, 24], [21], [22], [23], [25], [26]], [(1, (0.4, 0, 0.56), (1, 0.1, 0.0), 0.006, 15, 0.01)]), "wood",
                     "TV stand: walnut carcass, oak top, open bay with a shelf on brass pins, aluminium door tracks, cable grommets",
                     [("SM_L_TVStand_Door", DOORS), ("SM_L_TV", [TV_ON])]),
    "SM_L_Bookshelf": (build_bookshelf, 4096, groups([[1], [2], [3], [4], [5], [6], [7], [8], [9], [10]], [(9, (0.5, 0, 2.44), (1, 0.05, 0.0), 0.006, 17, 0.01)]), "wood",
                       "bookshelf: walnut sides with pin-hole rows, oak shelves with walnut lipping on brass pins, HDF back, anti-tip strap", None),
    "SM_L_SideTable": (build_side_table, 2048, groups([[1], [2], [3], [4], [5]], [(0, (0.03, 0, 0.51), (1, 0.1, 0.0), 0.005, 18, 0.008)]), "wood",
                       "round tripod side table: walnut, turned splayed legs, lower shelf, a water ring", None),
    "SM_L_FloorLamp": (build_floor_lamp, 2048, groups([[0], [1], [2]]), "metal",
                       "floor lamp: iron base on felt, pole with brass coupling, linen shade on wire hems (LampShade slot), bulb (Bulb slot), bead pull chain", None),
    "SM_L_Radiator": (build_radiator, 2048, groups([[0, 1], [2]]), "metal",
                      "cast column radiator: 9 two-column sections with casting seams, header nipples, chrome valves into the floor, wall brackets", None),
    "SM_L_Carpet": (build_carpet, 4096, groups([[0], [1]], [(0, (0.3, 0.1, 0.003), (1, 0.3, 0.0), 0.02, 19, 0.0)]), "cloth",
                    "persian-style wool rug: border lattice, medallion, traffic path, ripple, fringe on the ends", None),
    "SM_K_Rug": (build_kitchen_rug, 4096, groups([[0], [1]], [(0, (-0.2, 0.1, 0.004), (1, -0.2, 0.0), 0.02, 20, 0.0)]), "cloth",
                 "flat-woven kilim under the table: striped wool, whipped edges, tassels", None),
}
PIECES.update({
    "SM_L_DoorFrame": (build_door_frame, 2048, groups([[1], [2], [3]], [(2, (0.1, 0, 3800 / S + 0.01), (1, 0.1, 0.0), 0.004, 21, 0.01)]), "wood",
                       "door lining: oak jambs and head with a planted stop bead, brass hinge leaves and strike plate, screws", None),
    "SM_L_Door_Leaf": (build_door_leaf_at_hinge, 4096, groups([[0], [1]]), "wood",
                       "panelled door: green paint, raised and fielded panels with beads, brass levers on roses, escutcheons, latch, three butt hinges", [("SM_L_DoorFrame", [T(800 / S - 0.004, 280 / S / 2 + 0.006, -0.012)])]),
    "SM_L_Skirting": (lambda: build_skirting(1.0), 1024, groups([[0]], [(0, (0.02, 0, 0.06), (1, 0.15, 0.1), 0.004, 22, 0.008)]), "wood",
                      "skirting board, ogee top, painted (1 m module, stretched along X by the level)", None),
    "SM_K_Skirting": (lambda: build_skirting(0.5263), 1024, groups([[0]], [(0, (0.01, 0, 0.06), (1, -0.15, 0.1), 0.004, 23, 0.008)]), "wood",
                      "skirting board, ogee top, painted (0.53 m module, stretched along X by the level)", None),
    "SM_V3_Outlet": (build_outlet, 1024, groups([[0]]), "plastic", "wall socket: plate, round well with pin holes, earth clips, screw", None),
    "SM_K_Switch": (build_switch, 1024, groups([[0]]), "plastic", "light switch plate with the rocker frame", None),
    "SM_K_Switch_Toggle": (build_switch_toggle, 512, groups([[0]]), "plastic", "the rocker of the light switch (pivot at the origin)", None),
    "SM_L_CeilingLamp": (build_ceiling_lamp_l, 2048, groups([[0]]), "glass", "frosted glass dome on a brass gallery and canopy, thumb screws, bulb", None),
    "SM_K_CeilingLamp": (build_ceiling_lamp_k, 2048, groups([[0]]), "glass", "opal drum light on an enamel ceiling plate, ball bulb", None),
    "SM_V3_PendantLamp": (build_pendant, 2048, groups([[0]]), "metal", "enamel cone pendant: brass socket, cloth cord, ceiling cup", None),
    "SM_V3_CurtainRod": (build_curtain_rod, 1024, groups([[0]]), "metal", "brass curtain pole with acorn finials", None),
    "SM_V3_RodBracket": (build_rod_bracket, 512, groups([[0]]), "metal", "brass pole bracket: plate, arm, cradle, screws", None),
    "SM_V3_CurtainRing": (build_curtain_ring, 512, groups([[0]]), "metal", "brass curtain ring with an eye and a toothed clip", None),
    "SM_V3_Cornice": (build_cornice, 1024, groups([[0]], [(0, (0.05, -0.03, -0.03), (1, 0, 0.2), 0.004, 24, 0.0)]), "plaster", "plaster cove cornice with fillets", None),
    "SM_V3_WallShelf": (build_wall_shelf, 2048, groups([[0]], [(0, (0.1, -0.1, 0.01), (1, 0.1, 0.0), 0.004, 25, 0.01)]), "wood", "oak wall shelf with jar rings", None),
    "SM_V3_ShelfBracket": (build_shelf_bracket, 512, groups([[0]]), "metal", "black steel L bracket with gusset and screws", None),
    "SM_L_ShoeRack": (build_shoe_rack, 2048, groups([[1], [2], [3], [4], [5], [6]]), "wood", "beech shoe rack: slatted shelves doweled through the sides, top board, walnut back rail", None),
    "SM_K_Chair": (build_kitchen_chair, 2048, groups([[0], [1], [2], [3]]), "wood", "kitchen chair: green seat and rails, turned walnut legs, stretchers, spindles, corner blocks", None),
    "SM_K_HighChair": (build_high_chair, 2048, groups([[0], [1], [2], [3]]), "wood", "high chair: beech, splayed legs, teal pads, footrest, white tray, strap", None),
    "SM_L_DogBed": (build_dog_bed, 2048, groups([[0], [1]]), "cloth", "oval dog bed: quilted red bolster, sherpa cushion, non-slip base", None),
    "SM_L_CatScratchPost": (build_scratch_post, 2048, groups([[0], [1], [2]]), "cloth", "scratching post: carpeted base, sisal rope post, platform pad, ball on a string", None),
})

PIECES.update({
    "SM_K_Window": (build_window, 2048, groups([[1], [2], [3]]), "wood", "window lining, mullion and transom with putty beads (no glass on purpose), architrave, brass stay and handle", None),
    "SM_K_WindowSill": (build_window_sill, 2048, groups([[0]], [(0, (0.2, 0, 0.02), (1, 0.1, 0.0), 0.004, 26, 0.01)]), "wood", "oak window sill with nosing, drip groove, a pot ring", None),
    "SM_L_Pillow_A": (lambda: build_pillow(F_MUSTARD(), M("RedButton", fabric, (0.55, 0.07, 0.05), None, 300.0, 0.5)), 1024, groups([[0]], [(0, (0.0, 0, 0.17), (1, 0, 0.3), 0.01, 27, 0.0)]), "cloth", "throw pillow, mustard, piped, red buttons", None),
    "SM_L_Pillow_B": (lambda: build_pillow(M("PillowRed", fabric, (0.55, 0.07, 0.05), (0.38, 0.04, 0.03), 260.0, 0.6), F_MUSTARD()), 1024, groups([[0]], [(0, (0.0, 0, 0.17), (1, 0, -0.3), 0.01, 28, 0.0)]), "cloth", "throw pillow, red, piped, mustard buttons", None),
    "SM_V3_FloorCushion": (build_floor_cushion, 2048, groups([[0]], [(0, (0.0, 0.05, 0.07), (1, 0.4, 0.0), 0.01, 29, 0.0)]), "cloth", "floor cushion: honey corduroy, piped, carry handle", None),
})

PIECES.update({
    "SM_L_CatFlap": (build_cat_flap, 1024, groups([[0]]), "plastic", "cat flap liner: white tunnel with rims both faces, hinge bosses, magnet strip, screw caps", None),
    "SM_L_CatFlap_Leaf": (build_cat_flap_leaf, 1024, groups([[0]], [(0, (0.0, 0.02, -0.15), (0, 1, 0.4), 0.006, 30, 0.0)]), "plastic", "cat flap leaf: smoked pane in a white frame, weighted bar, claw scratches", None),
    "SM_V3_ToyBox": (build_toy_box, 2048, groups([[1], [2], [3], [4], [5]]), "wood", "beech toy box: finger joints, painted panels with stencilled letters, rope handles", None),
    "SM_V3_ToyBox_Lid": (build_toy_box_lid, 2048, groups([[0]], [(0, (0.05, -0.25, 0.015), (1, 0.15, 0.0), 0.005, 31, 0.01)]), "wood", "toy box lid: beech boards with breadboard ends, painted star, finger pull, hinge leaves", None),
    "SM_V3_Hinge": (build_hinge, 512, groups([[0]]), "metal", "concealed cup hinge: cup, arm, mounting plate, cover cap, screws", None),
})

# fix14: pieces of the kitchen that wear the owner's palette by default, as the kitchen block does (hero_kitchen_block.py
# pass 5): {hero material: the old model's slot} - the slot's map v4 look (Map4Look.cpp) is baked into tex_owner/.
# The old SM_K_Chair: Cabinet (seat, rails) + Walnut (legs); the old SM_K_HighChair: Butcher (frame), Teal (pads),
# Enamel (tray), Plinth (feet). Materials not listed (felt pads, the strap) keep their hero look.
OWNER_SLOTS = {
    "SM_K_Chair": {"GreenLacquer": "Cabinet", "WalnutZ": "Walnut", "WalnutX": "Walnut"},
    "SM_K_HighChair": {"Beech": "Butcher", "BeechZ": "Butcher", "TealVinyl": "Teal", "TrayWhite": "Enamel", "BlackPlastic": "Plinth"},
}

ORDER = list(PIECES.keys())
# review only: pieces that hang under the ceiling or sit round their origin are lifted off the studio floor
LIFT = {"SM_V3_Cornice": 0.45, "SM_V3_CurtainRod": 0.25, "SM_V3_RodBracket": 0.25, "SM_V3_CurtainRing": 0.15, "SM_V3_Outlet": 0.25, "SM_K_Switch_Toggle": 0.1, "SM_L_CatFlap_Leaf": 0.35, "SM_V3_Hinge": 0.1}
_HW, _DP = 800 / S, 280 / S
VIEW = {
    "SM_L_CatFlap": ({"wall_y": 3.0}, {"wide": ((-0.75, -0.35, 0.35), (0.0, 0.0, 0.16), 35), "ant": ((-0.30, -0.05, 0.014), (0.0, 0.0, 0.12), 22)}),
    "SM_L_CatFlap_Leaf": ({"wall_y": 3.0}, {"wide": ((-0.75, -0.35, 0.45), (0.0, 0.0, 0.21), 35), "ant": ((-0.30, -0.05, 0.014), (0.0, 0.0, 0.2), 22)}),
    "SM_L_DoorFrame": ({"wall_y": 3.0}, {"wide": ((1.3, -2.4, 1.3), (0.0, 0.0, 1.0), 35), "ant": ((0.25, -0.75, 0.014), (-0.1, 0.0, 0.45), 22)}),
    "SM_V3_CurtainPanel": ({"wall_y": 0.05}, {"wide": ((1.2, -3.2, 1.5), (0.0, 0.0, 1.35), 30), "ant": ((0.12, -0.32, 0.014), (0.0, 0.0, 0.35), 22)}),
    "SM_L_Door_Leaf": ({"wall_y": 3.0}, {"wide": ((1.7, -2.4, 1.3), (0.42, 0.0, 1.0), 35), "ant": ((0.65, -0.75, 0.014), (0.35, 0.0, 0.45), 22)}),
}

def save_manifest(mpath, man):
    """read, merge one piece, write (several Blender runs may share the manifest)"""
    json.dump(man, open(os.path.join(OUT, man["name"], "piece.json"), "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    doc = json.load(open(mpath, encoding="utf-8")) if os.path.exists(mpath) else {"pieces": []}
    for d in sorted(os.listdir(OUT)):      # (each piece also keeps its own entry: the manifest is rebuilt from them)
        f = os.path.join(OUT, d, "piece.json")
        if os.path.exists(f) and d != man["name"]:
            pj = json.load(open(f, encoding="utf-8")); doc["pieces"] = [p for p in doc["pieces"] if p["name"] != pj["name"]] + [pj]
    doc["pieces"] = [p for p in doc["pieces"] if p["name"] != man["name"]] + [man]
    doc["note"] = "Hero art, living room & furniture (Tools/blender/hero_living.py): LOD0 + UCX in <name>.fbx, _LOD1/_LOD2, _Brk<i> pieces; tex/T_<name>_{C,N,R}.png"
    tmp = mpath + ".%d.tmp" % os.getpid(); json.dump(doc, open(tmp, "w", encoding="utf-8"), indent=1, ensure_ascii=False); os.replace(tmp, mpath)

def main():
    t0 = time.time()
    H.reset()
    os.makedirs(OUT, exist_ok=True); os.makedirs(REV, exist_ok=True)
    mpath = os.path.join(OUT, "hero_living_manifest.json")
    doc = json.load(open(mpath, encoding="utf-8")) if os.path.exists(mpath) else {"pieces": []}
    want = [n for n in ORDER + list(TILE_PIECES.keys()) if ONLY is None or n in ONLY]
    done = {}
    for name in want:
        try:
            if name in TILE_PIECES:
                mf, mode, ext, note = TILE_PIECES[name]; extras = None
                man, lod0, pieces, paths = produce_tile(name, mf(), mode, ext, note)
            else:
                builder, tex, brk, cls, note, extras = PIECES[name]
                parts, hs = builder()
                man, lod0, pieces, paths = produce(name, parts, hs, brk, tex=tex, note=note, break_class=cls)
            done[name] = (lod0, pieces, paths)
            save_manifest(mpath, man)
            # its look in the review = the baked set
            PREV[name] = H.preview_material(name, paths)
            lod0.data.materials[0] = PREV[name]
            ex, old_ex = [], []
            for en, mats in (extras or []):
                for mw in mats:
                    old_ex.append((en, mw))
                    if en in done:
                        c = done[en][0].copy(); H.link(c); c.matrix_world = mw; ex.append(c)
            lift = LIFT.get(name, 0.0)
            if lift:
                for o in [lod0] + list(pieces) + ex: o.matrix_world = T(0, 0, lift) @ o.matrix_world
                old_ex = [(n_, T(0, 0, lift) @ Matrix(mw)) for n_, mw in old_ex] + [(name, T(0, 0, lift))]
            mn, mx = H.bounds([lod0] + ex)
            scene, cams = VIEW.get(name, (None, None))
            review(name, lod0, pieces, paths, cams or auto_cams(mn, mx), ex, scene, old_ex, lift)
            for c in ex: c.hide_render = True
        except Exception as e:
            import traceback; traceback.print_exc(); print("HERO_FAILED", name, e)
    print("HERO_DONE %s %.0f s" % (list(done.keys()), time.time() - t0))

main()
