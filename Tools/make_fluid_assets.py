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
// Foam is a lace of bubbles, not blots: the walls between cells of two cellular noises (bubbles of ~3 mm and ~7 mm at ant scale),
// carried a little by the flow. A little foam shows as a sparse lace, a lot closes into white water.
float amt = saturate(b.r);
float lace = 0.0;
for (int o = 0; o < 2; o++)
{
    float S = (o == 0) ? 5.5 : 13.0;
    float2 q = W.xy / S - a.ba * 0.01;
    float2 c = floor(q), f = frac(q);
    float d1 = 8.0, d2 = 8.0;
    for (int j = -1; j <= 1; j++)
    for (int i = -1; i <= 1; i++)
    {
        float2 g = float2(i, j);
        float2 h = frac(sin(float2(dot(c + g, float2(127.1, 311.7)), dot(c + g, float2(269.5, 183.3)))) * 43758.5453);
        float d = length(g + h - f);
        if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) { d2 = d; }
    }
    float wall = 1.0 - smoothstep(0.03, 0.16, d2 - d1);
    lace = max(lace, wall * ((o == 0) ? 1.0 : 0.75));
}
float foam = saturate(amt * 2.5) * lace * 0.85 + smoothstep(0.55, 1.0, amt) * 0.75;
return float4(saturate(foam), b.g, b.b, sp);
"""

# Caustics: the light a moving surface focuses on the bed, seen through the water (an emissive pattern under the surface, by depth).
CAUSTIC = r"""
float2 uv = (W.xy - Org.xy) * Org.zw;
float4 a = Texture2DSampleLevel(TexA, TexASampler, uv, 0);
float depth = max(a.g, 0.0);
float k = saturate(depth / 4.0) * saturate(1.0 - depth / 160.0);
float t = Time * 0.7;
float c = 0.0;
for (int o = 0; o < 2; o++)
{
    float S = (o == 0) ? 34.0 : 15.0;
    float2 p = W.xy / S - a.ba * Time * 0.004 * (o + 1);
    float2 w = p + 0.55 * float2(sin(p.y * 1.7 + t * (1.0 + o)), cos(p.x * 1.3 - t * 0.8));
    float r1 = 1.0 - abs(sin(w.x * 2.1 + sin(w.y * 1.1 + t)));
    float r2 = 1.0 - abs(sin(w.y * 2.3 + cos(w.x * 0.9 - t * 1.1)));
    c += (pow(saturate(r1), 7.0) + pow(saturate(r2), 7.0)) * ((o == 0) ? 1.0 : 0.5);
}
return c * k;
"""

# Thin-film interference for the oil: the colour depends on the thickness of the film and on the viewing angle.
# (KAN-21 run 3: the colours only where the film is really thin, i.e. at the rim of a puddle; a few soft bands, never more than
#  a pixel can show: the phase is faded out where it changes faster than the screen can draw, which was the moire.)
IRID = r"""
float th = max(Th, 0.0);
float cosv = saturate(dot(normalize(N), normalize(V)));
float ph = th * 0.9 * (0.6 + 0.4 * cosv) + 0.15 * (1.0 - cosv);
float3 col = 0.5 + 0.5 * cos(6.28318 * (ph * float3(1.0, 1.18, 1.42) + float3(0.0, 0.33, 0.67)));
float fw = fwidth(ph);
col = lerp(col, float3(0.62, 0.6, 0.55), saturate(fw * 3.0 - 0.3));
return lerp(float3(0.8, 0.78, 0.72), col, 0.55);
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
        # a vector parameter's first pin is RGB (float3): custom code that reads .w needs the RGBA pin
        for src, name in pairs:
            pin = "RGBA" if isinstance(src, unreal.MaterialExpressionVectorParameter) else ""
            MEL.connect_material_expressions(src, pin, c, name)

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
    # what lies on the water: foam (white) and the oil film (interference colours), each by its own coverage; clear water shows neither
    SURF = r"""
float oil = smoothstep(0.02, 0.25, Th) * (1.0 - smoothstep(0.6, 2.2, Th)) * 0.16;   // a thin film: only the rim, faint
float c = max(Foam, oil);
float3 col = (Ir * oil + Top * Foam) / max(oil + Foam, 1e-3);
return float4(col, c);
"""
    surf = custom(m, SURF, unreal.CustomMaterialOutputType.CMOT_FLOAT4, ["Ir", "Top", "Foam", "Th"], 200, 400, "FluidSurf")
    MEL.connect_material_expressions(ir, "", surf, "Ir")
    MEL.connect_material_expressions(top, "", surf, "Top")
    MEL.connect_material_expressions(foam, "", surf, "Foam")
    MEL.connect_material_expressions(th, "", surf, "Th")
    base = chan(surf, True, True, True, False, 400, 300)
    cover = chan(surf, False, False, False, True, 400, 500)
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

    # caustics on the bed under clear water (none under foam or cola); the game sets "Caustic" by the time of day
    caus = custom(m, CAUSTIC, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["TexA", "W", "Org", "Time"], -900, 700, "FluidCaustic")
    wire(caus, [(texA, "TexA"), (wp, "W"), (org, "Org"), (tm, "Time")])
    ccol = vparam(m, "Caustic", (0.09, 0.11, 0.12, 0), -1700, 2300)
    CMIX = r"""
return C * Col * (1.0 - saturate(Foam * 1.5)) * (1.0 - saturate(Dye * 1.5));
"""
    cmix = custom(m, CMIX, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["C", "Col", "Foam", "Dye"], -300, 700, "FluidCausticMix")
    MEL.connect_material_expressions(caus, "", cmix, "C")
    MEL.connect_material_expressions(rgb(ccol, -1400, 2300), "", cmix, "Col")
    MEL.connect_material_expressions(foam, "", cmix, "Foam")
    MEL.connect_material_expressions(dye, "", cmix, "Dye")
    MEL.connect_material_property(cmix, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    MEL.layout_material_expressions(m)
    MEL.recompile_material(m)
    EAL.save_asset(DIR + "/M_AntLiquid")
    unreal.log("FLUIDPY M_AntLiquid saved")

def glass(m, kind_from_instance, x0=-900):
    """Clear liquid as a lit translucent surface: refraction, Fresnel (the rim reflects, the middle is seen through), a hint of
    colour; per instance (custom data 0) 0 = water .. 0.5 = oil .. 1 = cola. Never black in shade: a little of the sky in it."""
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    try:
        m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    except Exception as e:
        unreal.log_warning("FLUIDPY translucency mode: %s" % e)
    try:
        m.set_editor_property("refraction_method", unreal.RefractionMode.RM_INDEX_OF_REFRACTION)
    except Exception as e:
        unreal.log_warning("FLUIDPY refraction_method: %s" % e)
    k = None
    if kind_from_instance:
        k = MEL.create_material_expression(m, unreal.MaterialExpressionPerInstanceCustomData, x0, 600)
        k.set_editor_property("data_index", 0)
    LOOK = r"""
float cola = saturate(K * 2.0 - 1.0), oil = 1.0 - abs(K * 2.0 - 1.0);
float3 nn = normalize(N), vv = normalize(V);
float fr = pow(1.0 - saturate(abs(dot(nn, vv))), 3.0);
float3 col = lerp(float3(0.80, 0.90, 0.95), float3(0.16, 0.06, 0.02), cola);
col = lerp(col, float3(0.95, 0.70, 0.25), oil);
float op = lerp(lerp(0.10, 0.85, cola), 0.75, fr);
return float4(col, op);
"""
    look = custom(m, LOOK, unreal.CustomMaterialOutputType.CMOT_FLOAT4, ["K", "N", "V"], x0 + 300, 300, "LiquidLook")
    if k:
        MEL.connect_material_expressions(k, "", look, "K")
    nrm = MEL.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, x0, 300)
    cam = MEL.create_material_expression(m, unreal.MaterialExpressionCameraVectorWS, x0, 400)
    MEL.connect_material_expressions(nrm, "", look, "N")
    MEL.connect_material_expressions(cam, "", look, "V")
    def mask(r, g, b, a, y):
        cm = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, x0 + 600, y)
        cm.set_editor_property("r", r); cm.set_editor_property("g", g); cm.set_editor_property("b", b); cm.set_editor_property("a", a)
        MEL.connect_material_expressions(look, "", cm, "")
        return cm
    col = mask(True, True, True, False, 200)
    op = mask(False, False, False, True, 400)
    MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
    em = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, x0 + 800, 100)
    em.set_editor_property("const_b", 0.06)
    MEL.connect_material_expressions(col, "", em, "A")
    MEL.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    r = sparam(m, "Roughness", 0.02, x0 + 600, -100)
    MEL.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    sp = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, x0 + 600, 0); sp.set_editor_property("r", 0.6)
    MEL.connect_material_property(sp, "", unreal.MaterialProperty.MP_SPECULAR)
    ior = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, x0 + 600, 500); ior.set_editor_property("r", 1.333)
    MEL.connect_material_property(ior, "", unreal.MaterialProperty.MP_REFRACTION)
    return look

def build_drop():
    m = fresh("M_AntDrop", unreal.Material, unreal.MaterialFactoryNew())
    try:
        m.set_editor_property("used_with_instanced_static_meshes", True)
    except Exception as e:
        unreal.log_warning("FLUIDPY ism usage: %s" % e)
    glass(m, True)
    MEL.recompile_material(m)
    EAL.save_asset(DIR + "/M_AntDrop")
    unreal.log("FLUIDPY M_AntDrop saved")

# A falling stream (the tap's jet, a sheet off an edge): clear, refracting, bright rim, ripples running down it.
STREAM_N = r"""
float v = UV.y * 6.0 - Time * 9.0;
float a = sin(v * 6.2832 + sin(UV.x * 6.2832 * 3.0) * 0.6) * 0.22 + sin(v * 2.3 * 6.2832 + UV.x * 19.0) * 0.1;
float b = sin(UV.x * 6.2832 * 2.0 + v * 3.1) * 0.12;
return normalize(float3(b, a, 1.0));
"""

def build_stream():
    m = fresh("M_AntStream", unreal.Material, unreal.MaterialFactoryNew())
    try:
        m.set_editor_property("used_with_instanced_static_meshes", True)
    except Exception as e:
        unreal.log_warning("FLUIDPY ism usage: %s" % e)
    glass(m, True)
    uv = MEL.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1300, -300)
    tm = MEL.create_material_expression(m, unreal.MaterialExpressionTime, -1300, -200)
    n = custom(m, STREAM_N, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["UV", "Time"], -1000, -300, "StreamNormal")
    MEL.connect_material_expressions(uv, "", n, "UV"); MEL.connect_material_expressions(tm, "", n, "Time")
    MEL.connect_material_property(n, "", unreal.MaterialProperty.MP_NORMAL)
    MEL.recompile_material(m)
    EAL.save_asset(DIR + "/M_AntStream")
    unreal.log("FLUIDPY M_AntStream saved")

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
what = os.environ.get("ANT_FLUID_MAKE", "liquid,drop,stream,niagara").split(",")
if "liquid" in what: build_liquid()
if "drop" in what: build_drop()
if "stream" in what: build_stream()
if "niagara" in what: copy_niagara()
unreal.log("FLUIDPY done")
