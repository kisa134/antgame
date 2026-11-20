# Liquid prototype (branch code/ANT-fluid): every asset the liquid needs, made from this script. Nothing is authored by hand.
#   /Game/Fluid/M_AntLiquid   the surface of the shared flow layer (Single Layer Water, displaced and masked by the simulation's textures)
#   /Game/Fluid/M_AntDrop     a drop (Single Layer Water on a sphere)
#   /Game/Fluid/NS_Flip*      copies of the Niagara Fluids FLIP templates (the engine plugin's own systems), so that they cook with the game
# Run: UnrealEditor-Cmd.exe AntKitchen.uproject -run=pythonscript -script=Tools/make_fluid_assets.py
import unreal
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/Fluid"

def fresh(name, cls, factory):
    full = DIR + "/" + name
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    return AT.create_asset(name, DIR, cls, factory)

def custom(m, code, out_type, inputs, x, y, desc):
    c = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("code", code)
    c.set_editor_property("output_type", out_type)
    c.set_editor_property("description", desc)
    ins = []
    for n in inputs:
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
    c.set_editor_property("inputs", ins)
    return c

def vparam(m, name, default, x, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", unreal.LinearColor(*default))
    return p

def sparam(m, name, default, x, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", default)
    return p

def tparam(m, name, x, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    t = unreal.load_asset("/Engine/EngineResources/Black")
    if t: p.set_editor_property("texture", t)
    return p

# Shared by the vertex and the pixel code: the height of the surface at a texel.
# TexA: r = surface above Z0, g = depth of this layer ("edge depth": 0 on dry ground), b,a = velocity.
# A puddle's rim is a rounded bead (surface tension): below the contact thickness Hc the surface follows a quarter circle.
COMMON = r"""
#ifndef ANT_BEAD
#define ANT_BEAD(e, Hc) ((e) < (Hc) ? sqrt(max((e) * (2.0 * (Hc) - (e)), 0.0)) : (e))
#define ANT_Z(a, Hc) ((a).r - max((a).g, 0.0) + ANT_BEAD(max((a).g, 0.0), Hc))
#endif
"""

WPO = COMMON + r"""
float2 uv = (W.xy - Org.xy) * Org.zw;
float4 a = Texture2DSampleLevel(TexA, TexASampler, uv, 0);
float z = Par.x + ANT_Z(a, Par.y) + Par.z;
return float3(0.0, 0.0, z - W.z);
"""

# Mask: the depth through a cubic B-spline (four bilinear taps), so that the outline of a puddle is round, not a staircase of cells.
MASK = r"""
float2 st = (W.xy - Org.xy) * Org.zw * Size.xy - 0.5;
float2 f = frac(st); float2 i = floor(st);
float2 w0 = (1.0 - f) * (1.0 - f) * (1.0 - f) / 6.0;
float2 w1 = (4.0 - 6.0 * f * f + 3.0 * f * f * f) / 6.0;
float2 w2 = (1.0 + 3.0 * f + 3.0 * f * f - 3.0 * f * f * f) / 6.0;
float2 w3 = f * f * f / 6.0;
float2 g0 = w0 + w1, g1 = w2 + w3;
float2 p0 = (i - 0.5 + w1 / g0) / Size.xy;
float2 p1 = (i + 1.5 + w3 / g1) / Size.xy;
float e = g0.y * (g0.x * Texture2DSampleLevel(TexA, TexASampler, float2(p0.x, p0.y), 0).g + g1.x * Texture2DSampleLevel(TexA, TexASampler, float2(p1.x, p0.y), 0).g)
        + g1.y * (g0.x * Texture2DSampleLevel(TexA, TexASampler, float2(p0.x, p1.y), 0).g + g1.x * Texture2DSampleLevel(TexA, TexASampler, float2(p1.x, p1.y), 0).g);
return e;
"""

# Normal: the slope of the simulated surface, plus ripples that travel with the flow (two phases of a noise carried along the
# velocity and cross-faded, the usual flow-map trick), fading out where they would be smaller than a pixel.
NORMAL = COMMON + r"""
float2 uv = (W.xy - Org.xy) * Org.zw;
float2 tx = 1.0 / Size.xy;
float Hc = Par.y;
float4 a = Texture2DSampleLevel(TexA, TexASampler, uv, 0);
float zl = ANT_Z(Texture2DSampleLevel(TexA, TexASampler, uv - float2(tx.x, 0), 0), Hc);
float zr = ANT_Z(Texture2DSampleLevel(TexA, TexASampler, uv + float2(tx.x, 0), 0), Hc);
float zd = ANT_Z(Texture2DSampleLevel(TexA, TexASampler, uv - float2(0, tx.y), 0), Hc);
float zu = ANT_Z(Texture2DSampleLevel(TexA, TexASampler, uv + float2(0, tx.y), 0), Hc);
float dx = Size.z;
float2 slope = float2(zr - zl, zu - zd) / (2.0 * dx);
slope = clamp(slope, -4.0, 4.0);
float2 vel = a.ba;
float sp = length(vel);
float px = max(length(ddx(W.xy)), length(ddy(W.xy)));
float2 rip = 0;
{
    float amp = Rip.x + Rip.y * saturate(sp / 220.0);
    float T = 0.7;
    for (int o = 0; o < 2; o++)
    {
        float L = (o == 0) ? Rip.z : Rip.z * 5.3;
        float fade = saturate(L / (px * 3.0) - 0.6);
        if (fade <= 0.0) continue;
        float ph1 = frac(Time / T), ph2 = frac(Time / T + 0.5);
        float2 q1 = (W.xy - vel * (ph1 - 0.5) * T) / L, q2 = (W.xy - vel * (ph2 - 0.5) * T) / L + 17.3;
        float2 g = 0;
        for (int k = 0; k < 2; k++)
        {
            float2 q = (k == 0) ? q1 : q2;
            float wgt = (k == 0) ? (1.0 - abs(2.0 * ph1 - 1.0)) : (1.0 - abs(2.0 * ph2 - 1.0));
            // gradient of a smooth value noise
            float2 c = floor(q), fr = frac(q);
            float2 u = fr * fr * (3.0 - 2.0 * fr), du = 6.0 * fr * (1.0 - fr);
            float h00 = frac(sin(dot(c, float2(127.1, 311.7))) * 43758.5453);
            float h10 = frac(sin(dot(c + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
            float h01 = frac(sin(dot(c + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
            float h11 = frac(sin(dot(c + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
            g += wgt * float2(du.x * ((h10 - h00) + (h00 - h10 - h01 + h11) * u.y), du.y * ((h01 - h00) + (h00 - h10 - h01 + h11) * u.x));
        }
        rip += g * amp * fade * ((o == 0) ? 1.0 : 0.6);
    }
}
float body = saturate(max(a.g, 0.0) / max(Hc, 0.01));
float3 n = normalize(float3(-slope - rip * body, 1.0));
return n;
"""

# r = foam, g = dye (cola), b = oil on top (uu), a = speed
AUX = r"""
float2 uv = (W.xy - Org.xy) * Org.zw;
float4 a = Texture2DSampleLevel(TexA, TexASampler, uv, 0);
float4 b = Texture2DSampleLevel(TexB, TexBSampler, uv, 0);
float sp = length(a.ba);
float2 q = W.xy / 9.0 - a.ba * 0.02;
float2 c = floor(q), fr = frac(q);
float2 u = fr * fr * (3.0 - 2.0 * fr);
float h00 = frac(sin(dot(c, float2(12.9898, 78.233))) * 43758.5453);
float h10 = frac(sin(dot(c + float2(1, 0), float2(12.9898, 78.233))) * 43758.5453);
float h01 = frac(sin(dot(c + float2(0, 1), float2(12.9898, 78.233))) * 43758.5453);
float h11 = frac(sin(dot(c + float2(1, 1), float2(12.9898, 78.233))) * 43758.5453);
float nz = lerp(lerp(h00, h10, u.x), lerp(h01, h11, u.x), u.y);
float foam = saturate(b.r * 1.6 - nz * 0.9 * (1.0 - saturate(b.r - 0.6) * 2.5));
return float4(foam, b.g, b.b, sp);
"""

# Thin-film interference for the oil: the colour depends on the thickness of the film and on the viewing angle.
IRID = r"""
float th = max(Th, 0.0);
float cosv = saturate(dot(normalize(N), normalize(V)));
float ph = th * 9.0 * (0.55 + 0.45 * cosv) + 0.6 * (1.0 - cosv);
float3 col = 0.5 + 0.5 * cos(6.28318 * (ph * float3(1.0, 1.18, 1.42) + float3(0.0, 0.33, 0.67)));
return col;
"""

def build_liquid():
    m = fresh("M_AntLiquid", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("two_sided", False)
    m.set_editor_property("tangent_space_normal", False)
    m.set_editor_property("opacity_mask_clip_value", 0.5)
    try:
        m.set_editor_property("refraction_method", unreal.RefractionMode.RM_INDEX_OF_REFRACTION)
    except Exception as e:
        unreal.log_warning("FLUIDPY refraction_method: %s" % e)

    texA = tparam(m, "TexA", -1700, -200)
    texB = tparam(m, "TexB", -1700, 0)
    org = vparam(m, "Org", (0, 0, 0.001, 0.001), -1700, 200)      # origin.xy, 1/size.xy (world -> uv)
    size = vparam(m, "Size", (64, 64, 20, 0), -1700, 400)          # cells x, cells y, cell size
    par = vparam(m, "Par", (0, 5, 0.3, 0), -1700, 600)             # Z0, contact thickness, z bias
    rip = vparam(m, "Rip", (0.01, 0.22, 7.0, 0), -1700, 800)       # ripple slope at rest, extra at speed, wavelength
    absw = vparam(m, "Absorb", (0.02, 0.006, 0.004, 0), -1700, 1000)       # per uu
    absd = vparam(m, "AbsorbDye", (0.25, 0.42, 0.6, 0), -1700, 1200)
    sca = vparam(m, "Scatter", (0.0004, 0.0006, 0.0006, 0), -1700, 1400)
    scad = vparam(m, "ScatterDye", (0.004, 0.002, 0.001, 0), -1700, 1600)
    top = vparam(m, "TopColor", (0.9, 0.93, 0.95, 0), -1700, 1800)  # the colour of what lies on the surface (foam)
    rough = sparam(m, "Roughness", 0.03, -1700, 2000)
    irid = sparam(m, "Irid", 0.0, -1700, 2100)                      # 1: this layer is an oil film
    maskmin = sparam(m, "MaskMin", 0.03, -1700, 2200)

    wp = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1300, -400)
    wp.set_editor_property("world_position_shader_offset", unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    tm = MEL.create_material_expression(m, unreal.MaterialExpressionTime, -1300, -300)
    cam = MEL.create_material_expression(m, unreal.MaterialExpressionCameraVectorWS, -1300, -500)

    def wire(c, pairs):
        for src, name in pairs:
            MEL.connect_material_expressions(src, "", c, name)

    wpo = custom(m, WPO, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["TexA", "W", "Org", "Par"], -900, -500, "FluidWPO")
    wire(wpo, [(texA, "TexA"), (wp, "W"), (org, "Org"), (par, "Par")])
    MEL.connect_material_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    mask = custom(m, MASK, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["TexA", "W", "Org", "Size"], -900, -250, "FluidMask")
    wire(mask, [(texA, "TexA"), (wp, "W"), (org, "Org"), (size, "Size")])
    # opacity mask = e / MaskMin * 0.5  -> clips where the depth is under MaskMin
    div = MEL.create_material_expression(m, unreal.MaterialExpressionDivide, -600, -250)
    MEL.connect_material_expressions(mask, "", div, "A"); MEL.connect_material_expressions(maskmin, "", div, "B")
    half = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -450, -250)
    half.set_editor_property("const_b", 0.5)
    MEL.connect_material_expressions(div, "", half, "A")
    MEL.connect_material_property(half, "", unreal.MaterialProperty.MP_OPACITY_MASK)

    nrm = custom(m, NORMAL, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["TexA", "W", "Org", "Size", "Par", "Rip", "Time"], -900, 0, "FluidNormal")
    wire(nrm, [(texA, "TexA"), (wp, "W"), (org, "Org"), (size, "Size"), (par, "Par"), (rip, "Rip"), (tm, "Time")])
    MEL.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)

    aux = custom(m, AUX, unreal.CustomMaterialOutputType.CMOT_FLOAT4, ["TexA", "TexB", "W", "Org"], -900, 300, "FluidAux")
    wire(aux, [(texA, "TexA"), (texB, "TexB"), (wp, "W"), (org, "Org")])
    def chan(src, r, g, b, a, x, y):
        cm = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, x, y)
        cm.set_editor_property("r", r); cm.set_editor_property("g", g); cm.set_editor_property("b", b); cm.set_editor_property("a", a)
        MEL.connect_material_expressions(src, "", cm, "")
        return cm
    foam = chan(aux, True, False, False, False, -600, 250)
    dye = chan(aux, False, True, False, False, -600, 330)
    oil = chan(aux, False, False, True, False, -600, 410)

    def lerp(a, b, t, x, y):
        l = MEL.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, x, y)
        MEL.connect_material_expressions(a, "", l, "A"); MEL.connect_material_expressions(b, "", l, "B"); MEL.connect_material_expressions(t, "", l, "Alpha")
        return l
    def rgb(v, x, y):
        return chan(v, True, True, True, False, x, y)

    absorb = lerp(rgb(absw, -1400, 1000), rgb(absd, -1400, 1200), dye, -300, 1000)
    scatter = lerp(rgb(sca, -1400, 1400), rgb(scad, -1400, 1600), dye, -300, 1400)
    slw = MEL.create_material_expression(m, unreal.MaterialExpressionSingleLayerWaterMaterialOutput, 200, 1000)
    MEL.connect_material_expressions(scatter, "", slw, "ScatteringCoefficients")
    MEL.connect_material_expressions(absorb, "", slw, "AbsorptionCoefficients")
    pg = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, 0, 1300); pg.set_editor_property("r", 0.0)
    MEL.connect_material_expressions(pg, "", slw, "PhaseG")
    cs = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, 0, 1400); cs.set_editor_property("r", 1.0)
    MEL.connect_material_expressions(cs, "", slw, "ColorScaleBehindWater")

    # the oil film: its own thickness when this layer is the oil (Irid = 1), else the oil lying on the water (aux.b)
    th = lerp(oil, mask, irid, -300, 500)
    ir = custom(m, IRID, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["Th", "N", "V"], -100, 500, "FluidIrid")
    wire(ir, [(th, "Th"), (nrm, "N"), (cam, "V")])
    # how much of the surface the top layer covers: foam, or the film of oil
    oilc = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -100, 650); oilc.set_editor_property("const_b", 6.0)
    MEL.connect_material_expressions(th, "", oilc, "A")
    oils = MEL.create_material_expression(m, unreal.MaterialExpressionSaturate, 50, 650)
    MEL.connect_material_expressions(oilc, "", oils, "")
    oilk = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, 200, 650); oilk.set_editor_property("const_b", 0.32)
    MEL.connect_material_expressions(oils, "", oilk, "A")
    base = lerp(ir, rgb(top, -1400, 1800), foam, 200, 300)
    cover = MEL.create_material_expression(m, unreal.MaterialExpressionMax, 400, 500)
    MEL.connect_material_expressions(foam, "", cover, "A"); MEL.connect_material_expressions(oilk, "", cover, "B")
    MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(cover, "", unreal.MaterialProperty.MP_OPACITY)
    # foam is rough, water is a mirror
    rf = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, 200, 150); rf.set_editor_property("r", 0.55)
    rl = lerp(rough, rf, foam, 400, 100)
    MEL.connect_material_property(rl, "", unreal.MaterialProperty.MP_ROUGHNESS)
    sp = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, 400, 0); sp.set_editor_property("r", 0.255)
    MEL.connect_material_property(sp, "", unreal.MaterialProperty.MP_SPECULAR)
    ior = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, 400, -100); ior.set_editor_property("r", 1.333)
    MEL.connect_material_property(ior, "", unreal.MaterialProperty.MP_REFRACTION)

    MEL.layout_material_expressions(m)
    MEL.recompile_material(m)
    EAL.save_asset(DIR + "/M_AntLiquid")
    unreal.log("FLUIDPY M_AntLiquid saved")

def build_drop():
    m = fresh("M_AntDrop", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    try:
        m.set_editor_property("refraction_method", unreal.RefractionMode.RM_INDEX_OF_REFRACTION)
    except Exception as e:
        unreal.log_warning("FLUIDPY refraction_method: %s" % e)
    absw = vparam(m, "Absorb", (0.012, 0.004, 0.003, 0), -700, 300)
    sca = vparam(m, "Scatter", (0.0006, 0.0008, 0.0008, 0), -700, 500)
    slw = MEL.create_material_expression(m, unreal.MaterialExpressionSingleLayerWaterMaterialOutput, 0, 400)
    MEL.connect_material_expressions(sca, "", slw, "ScatteringCoefficients")
    MEL.connect_material_expressions(absw, "", slw, "AbsorptionCoefficients")
    pg = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 700); pg.set_editor_property("r", 0.0)
    MEL.connect_material_expressions(pg, "", slw, "PhaseG")
    cs = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 800); cs.set_editor_property("r", 1.0)
    MEL.connect_material_expressions(cs, "", slw, "ColorScaleBehindWater")
    r = sparam(m, "Roughness", 0.02, -700, 0)
    MEL.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    sp = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 100); sp.set_editor_property("r", 0.255)
    MEL.connect_material_property(sp, "", unreal.MaterialProperty.MP_SPECULAR)
    ior = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 200); ior.set_editor_property("r", 1.333)
    MEL.connect_material_property(ior, "", unreal.MaterialProperty.MP_REFRACTION)
    bc = vparam(m, "TopColor", (1, 1, 1, 0), -700, -200)
    MEL.connect_material_property(bc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.recompile_material(m)
    EAL.save_asset(DIR + "/M_AntDrop")
    unreal.log("FLUIDPY M_AntDrop saved")

def copy_niagara():
    src = "/NiagaraFluids/Templates/Liquid/3D/Systems/"
    for a, b in [("Grid3D_Flip_Hose", "NS_FlipHose"), ("Grid3D_Flip_Pool", "NS_FlipPool"), ("Grid3D_Flip_Splash", "NS_FlipSplash")]:
        dst = DIR + "/" + b
        if EAL.does_asset_exist(dst):
            unreal.log("FLUIDPY keep " + dst)
            continue
        obj = unreal.load_asset(src + a)
        ok = AT.duplicate_asset(b, DIR, obj) if obj else None
        unreal.log("FLUIDPY copy %s -> %s : %s" % (a, dst, "ok" if ok else "FAILED"))
        if ok:
            EAL.save_asset(dst)

import os
what = os.environ.get("ANT_FLUID_MAKE", "liquid,drop,niagara").split(",")
if "liquid" in what: build_liquid()
if "drop" in what: build_drop()
if "niagara" in what: copy_niagara()
unreal.log("FLUIDPY done")
