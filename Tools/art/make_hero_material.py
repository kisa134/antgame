# KAN-22 pass 2: the material of the hero props (Tools/blender/hero_lib.py): their own baked set through UV0 and, on top, the
# SAME state layer as the common materials (Source/AntKitchen/AntState.h, Tools/map4/make_m4_materials.py): custom primitive
# data 0..12 = the overall value of each kind, 16..35 = five spots (local xyz, packed radius / kind / strength). So the fire, the
# water, the frost, the mess and the wreck of AntMatter / AntMess / AntWreck show on the hero props exactly as on the rest.
#   /Game/Kitchen/Hero/M_Hero            C (colour), N (tangent normal), R (AO, roughness, metal, a = porosity)
#   /Game/Kitchen/Hero/MI_HeroBreak_*    fresh fracture faces of the pre-cut break pieces (no texture: UseTex 0)
# The hero look of the states (the owner's reference, hero_mop_states.jpg):
#   wet    darker by porosity (yarn a lot, plastic a little), glossy, DROPS (domes in the normal, mirror-smooth) and running
#          STREAKS on the walls - readable from a metre away, not just a darker tint;
#   char   browning -> soot with a gradient (brown-grey -> black), alligator CRACKS in the deep char, cold: no glow;
#   burn   embers only where it is burning: the bright rim of a spreading fire (the spots' edge) and a faint glow in the cracks
#          inside the burning area, flickering - no orange blotches;
#   dirt   brown film, thicker in the grooves (the baked AO); damage: cracks and pale broken edges; heat, frost, charge, oil,
#          jam, cola, honey, spoil as in M_M4.
# Custom data 15 = sway of the loose parts (the mop's yarn: vertex colour R; set by AntWreck). (13, 14: free; 0..12 and 16..35
# belong to AntState.h. The old M_Hero read sway from 4 = AntState's Heat.)
# Run: UnrealEditor-Cmd.exe AntKitchen.uproject -run=pythonscript -script=<copy of this file> (Saved/art/ue_py.cmd)
import unreal
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/Kitchen/Hero"
SWAY_INDEX = 15

def sp(m, name, val, x, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", val); return p

def vp(m, name, val, x, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", unreal.LinearColor(*val)); return p

def cpd(m, name, index, x, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", 0.0)
    p.set_editor_property("use_custom_primitive_data", True); p.set_editor_property("primitive_data_index", index)
    return p

def cpd4(m, name, index, x, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", unreal.LinearColor(0, 0, 0, 0))
    p.set_editor_property("use_custom_primitive_data", True); p.set_editor_property("primitive_data_index", index)
    return p

def tex(m, name, default, sampler, x, y):
    t = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    t.set_editor_property("parameter_name", name)
    d = unreal.load_asset(default)
    if d: t.set_editor_property("texture", d)
    t.set_editor_property("sampler_type", sampler)
    return t

def custom(m, code, inputs, x, y, desc, out=unreal.CustomMaterialOutputType.CMOT_FLOAT3, extra=()):
    c = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("code", code); c.set_editor_property("output_type", out); c.set_editor_property("description", desc)
    ins = []
    for n in inputs:
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
    c.set_editor_property("inputs", ins)
    if extra:
        outs = []
        for name in extra:
            o = unreal.CustomOutput(); o.set_editor_property("output_name", name)
            o.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4); outs.append(o)
        c.set_editor_property("additional_outputs", outs)
    return c

def wire(m, node, pairs):
    for src, out, pin in pairs:
        if not MEL.connect_material_expressions(src, out, node, pin):
            unreal.log_warning("ART: HERO connect FAILED %s -> %s" % (out or "main", pin))

# ---- the state node: verbatim the decoding of Tools/map4/make_m4_materials.py (STATE_NODE) - one layout for the whole game
def _spot(i):
    return r"""
{
    float s_w = P%d.w;
    float s_rq = floor(s_w / 1024.0);
    float s_rem = s_w - s_rq * 1024.0;
    float s_kd = floor(s_rem / 64.0);
    float s_str = (s_rem - s_kd * 64.0) / 63.0 * step(0.5, s_w);
    float s_dd = length(LP - P%d.xyz) / max(s_rq * 0.25, 0.25) + (s_fine - 0.5) * 0.45;
    float s_v = saturate((1.0 - s_dd) * 3.0) * s_str;
    s_wet = max(s_wet, s_v * (s_kd == 0)); s_char = max(s_char, s_v * (s_kd == 1)); s_dirt = max(s_dirt, s_v * (s_kd == 2)); s_dmg = max(s_dmg, s_v * (s_kd == 3));
    s_heat = max(s_heat, s_v * (s_kd == 4)); s_burn = max(s_burn, s_v * (s_kd == 5)); s_frost = max(s_frost, s_v * (s_kd == 6)); s_chg = max(s_chg, s_v * (s_kd == 7));
    s_oil = max(s_oil, s_v * (s_kd == 8)); s_jam = max(s_jam, s_v * (s_kd == 9)); s_cola = max(s_cola, s_v * (s_kd == 10)); s_honey = max(s_honey, s_v * (s_kd == 11));
    s_spoil = max(s_spoil, s_v * (s_kd == 12));
    s_ring = max(s_ring, (1.0 - smoothstep(0.0, 0.16, abs(s_dd - 0.8))) * s_str * (s_kd == 5));
}
""" % (i, i)

STATE_NODE = r"""
Hot = float4(0, 0, 0, 0); Soak = float4(0, 0, 0, 0); Misc = float4(0, 0, 0, 0); Aux = float4(0, 0, 0, 0);
float s_any = dot(S0, float4(1, 1, 1, 1)) + dot(S1, float4(1, 1, 1, 1)) + dot(S2, float4(1, 1, 1, 1)) + S3.x + P0.w;
[branch] if (s_any <= 0.0)
{
    return float4(0, 0, 0, 0);
}
float2 s_p1 = (W.xy + W.zz * 0.31 + W.yx * 0.11) / 41.0;
float2 s_i1 = floor(s_p1); float2 s_f1 = frac(s_p1); s_f1 = s_f1 * s_f1 * (3.0 - 2.0 * s_f1);
float s_n1 = lerp(lerp(frac(sin(dot(s_i1, float2(12.9898, 78.233))) * 43758.5453), frac(sin(dot(s_i1 + float2(1, 0), float2(12.9898, 78.233))) * 43758.5453), s_f1.x),
                  lerp(frac(sin(dot(s_i1 + float2(0, 1), float2(12.9898, 78.233))) * 43758.5453), frac(sin(dot(s_i1 + float2(1, 1), float2(12.9898, 78.233))) * 43758.5453), s_f1.x), s_f1.y);
float2 s_p2 = (W.yz + W.xx * 0.23 + W.zy * 0.17) / 11.0;
float2 s_i2 = floor(s_p2); float2 s_f2 = frac(s_p2); s_f2 = s_f2 * s_f2 * (3.0 - 2.0 * s_f2);
float s_n2 = lerp(lerp(frac(sin(dot(s_i2, float2(269.5, 183.3))) * 43758.5453), frac(sin(dot(s_i2 + float2(1, 0), float2(269.5, 183.3))) * 43758.5453), s_f2.x),
                  lerp(frac(sin(dot(s_i2 + float2(0, 1), float2(269.5, 183.3))) * 43758.5453), frac(sin(dot(s_i2 + float2(1, 1), float2(269.5, 183.3))) * 43758.5453), s_f2.x), s_f2.y);
float s_fine = s_n1 * 0.6 + s_n2 * 0.4;
float s_wet = S0.x, s_char = S0.y, s_dirt = S0.z, s_dmg = S0.w;
float s_heat = S1.x, s_burn = S1.y, s_frost = S1.z, s_chg = S1.w;
float s_oil = S2.x, s_jam = S2.y, s_cola = S2.z, s_honey = S2.w;
float s_spoil = S3.x;
float s_ring = 0.0;
""" + "".join(_spot(i) for i in range(5)) + r"""
Hot = saturate(float4(s_heat, s_burn, s_frost, s_chg));
Soak = saturate(float4(s_oil, s_jam, s_cola, s_honey));
Misc = float4(saturate(s_spoil), saturate(s_ring), s_fine, s_n1);
Aux = float4(s_n2, 1.0, 0.0, 0.0);
return saturate(float4(s_wet, s_char, s_dirt, s_dmg));
"""
STATE_PINS = ["LP", "S0", "S1", "S2", "S3", "P0", "P1", "P2", "P3", "P4"]
STATE_USE = ["SA", "SH", "SS", "SM", "SX"]
STATE_IN = r"""
float s_wet = SA.x, s_char = SA.y, s_dirt = SA.z, s_dmg = SA.w;
float s_heat = SH.x, s_burn = SH.y, s_frost = SH.z, s_chg = SH.w;
float s_oil = SS.x, s_jam = SS.y, s_cola = SS.z, s_honey = SS.w;
float s_spoil = SM.x, s_ring = SM.y, s_fine = SM.z, s_n1 = SM.w;
float s_n2 = SX.x;
float s_burnt = max(s_char, s_burn);
"""

# ---- the hero states' own detail, once per pixel: WETFX (drops, streaks) and CHARFX (soot, cracks, embers)
# WETFX main: xyz = world-space tilt of the normal (drop domes, streak ridges), w = drop mask; Streak.x = streak mask
WETFX = STATE_IN + r"""
Streak = float4(0, 0, 0, 0);
float wet = max(s_wet, max(s_oil, s_honey) * 0.6);
[branch] if (SX.y <= 0.0 || wet < 0.12)
{
    return float4(0, 0, 0, 0);
}
float3 n = normalize(VN);
// drops: one per 16 uu cell (an 8 mm ant is two cells), more and bigger the wetter; a dome in the normal, mirror smooth
float G = 16.0;
float3 cell = floor(W / G);
float h1 = frac(sin(dot(cell, float3(12.9898, 78.233, 37.719))) * 43758.5453);
float h2 = frac(sin(dot(cell, float3(39.346, 11.135, 83.155))) * 24634.6345);
float h3 = frac(sin(dot(cell, float3(73.156, 52.235, 9.151))) * 56445.2345);
float h4 = frac(sin(dot(cell, float3(27.121, 91.623, 63.331))) * 35412.9137);
float3 c = (cell + 0.3 + 0.4 * float3(h1, h2, h3)) * G;
float3 d = W - c;
float3 dt = d - n * dot(d, n);
float r = G * (0.12 + 0.20 * h4) * (0.7 + 0.5 * wet);
// on a wall a drop sags: longer downwards
float3 up = float3(0, 0, 1) - n * n.z;
float wall = saturate(1.0 - abs(n.z) * 1.4);
float dz = dot(dt, normalize(up + 1e-4));
float3 dts = dt + normalize(up + 1e-4) * dz * (wall * 0.45 * step(dz, 0.0));
float q = length(dts) / r;
float present = step(h1 * 0.85 + 0.15, wet * 0.95);
float drop = (1.0 - smoothstep(0.78, 1.0, q)) * present;
float3 tilt = (dts / r) * 0.85 * drop;
// streaks: thin running lines on what faces sideways, broken in length, a bead at the bottom
float sm = 0.0;
[branch] if (wall > 0.05)
{
    float2 side = normalize(float2(-n.y, n.x) + 1e-4);
    float hh = dot(W.xy, side) / 6.0;
    float col = floor(hh);
    float k1 = frac(sin(col * 91.345) * 47453.5453);
    float k2 = frac(sin(col * 13.713) * 21753.2381);
    float prof = 1.0 - abs(frac(hh) - 0.5) * 2.0;
    float lnw = smoothstep(0.45, 0.85, prof);
    float seg = frac((W.z + k2 * 400.0) / (90.0 + 120.0 * k1));
    float run = smoothstep(0.0, 0.08, seg) * (1.0 - smoothstep(0.55, 0.62, seg));
    sm = lnw * run * step(k1, wet * 0.55) * wall;
    float bead = (1.0 - smoothstep(0.0, 0.06, abs(seg - 0.6))) * lnw * step(k1, wet * 0.55) * wall;
    sm = max(sm, bead);
    tilt += float3(side * (frac(hh) - 0.5), 0) * sm * 0.9;
}
Streak = float4(sm, 0, 0, 0);
return float4(tilt, max(drop, sm));
"""

# CHARFX main: x = soot (0 clean .. 1 black), y = crack lines in the deep char, z = ember glow (only where it burns), w = browning
CHARFX = STATE_IN + r"""
[branch] if (SX.y <= 0.0 || (s_burnt <= 0.0 && s_heat < 0.5))
{
    return float4(0, 0, 0, 0);
}
float x = s_burnt * 1.25 + (s_fine - 0.5) * 0.55 + (s_n1 - 0.5) * 0.25;
float brown = saturate(s_burnt / 0.3);
float soot = smoothstep(0.30, 1.0, x);
// alligator char: the cell edges of a 3D Voronoi of ~7 uu, only where the char is deep
float crack = 0.0;
float deep = smoothstep(0.62, 0.95, x);
[branch] if (deep > 0.0)
{
    float3 p = W / 7.0;
    float3 ip = floor(p); float3 fp = frac(p);
    float f1 = 8.0, f2 = 8.0;
    for (int k = 0; k < 27; k++)
    {
        float3 o = float3(k % 3, (k / 3) % 3, k / 9) - 1.0;
        float3 cc = ip + o;
        float3 rp = float3(frac(sin(dot(cc, float3(127.1, 311.7, 74.7))) * 43758.5453), frac(sin(dot(cc, float3(269.5, 183.3, 246.1))) * 43758.5453), frac(sin(dot(cc, float3(113.5, 271.9, 124.6))) * 43758.5453));
        float dd = length(o + rp - fp);
        if (dd < f1) { f2 = f1; f1 = dd; } else if (dd < f2) { f2 = dd; }
    }
    crack = (1.0 - smoothstep(0.0, 0.10, f2 - f1)) * deep;
}
// embers: the bright rim where a fire is spreading (the spots' burning edge) and a faint pulse in the cracks of what burns
float fl = 0.6 + 0.4 * sin(Tm * 7.3 + W.x * 0.11 + W.y * 0.07) * sin(Tm * 3.1 + W.z * 0.13);
float edge = s_ring * (0.55 + 0.45 * smoothstep(0.4, 0.8, s_fine));
float inside = s_burn * (crack * 0.8 + smoothstep(0.82, 0.95, s_fine) * 0.35) * soot;
return float4(soot, crack, (edge + inside) * fl, brown);
"""

COLOR = STATE_IN + r"""
float3 col = lerp(Tint.rgb, C.rgb * Tint.rgb * lerp(1.0, R.r, 0.45), UseTex);
float por = lerp(0.3, R.a, UseTex);
float ao = lerp(1.0, R.r, UseTex);
[branch] if (SX.y > 0.0)
{
    col = lerp(col, col * float3(0.92, 0.78, 0.42) * 0.85, s_oil * 0.75);
    col = lerp(col, float3(0.30, 0.025, 0.05), s_jam * 0.8);
    col = lerp(col, col * float3(0.32, 0.18, 0.08), s_cola * 0.8);
    col = lerp(col, float3(0.78, 0.44, 0.07) * (0.6 + 0.4 * col), s_honey * 0.75);
    // dirt: a brown film, thicker in the grooves the bake found
    col = lerp(col, float3(0.20, 0.16, 0.11), saturate(s_dirt * (0.25 + s_fine) * (0.6 + 0.9 * (1.0 - ao))) * 0.85);
    // wet: darker by how porous it is (yarn soaks dark, plastic only deepens), drops and streaks a little darker still
    col *= lerp(1.0, lerp(0.70, 0.34, por), saturate(s_wet));
    col *= 1.0 - WD.w * 0.18;
    float s_g = dot(col, float3(0.3, 0.59, 0.11));
    col = lerp(col, s_g * float3(0.85, 0.82, 0.62), s_spoil * 0.65);
    float s_mould = smoothstep(0.66, 0.74, s_n2 + s_spoil * 0.35) * step(0.12, s_spoil);
    col = lerp(col, float3(0.62, 0.68, 0.52) * (0.7 + 0.3 * s_n1), s_mould);
    // char: browning, then soot with a gradient (brown-grey to black), the cracks blacker
    col = lerp(col, col * float3(0.55, 0.40, 0.28), CF.w * 0.75);
    float3 sootc = lerp(float3(0.10, 0.085, 0.07), float3(0.016, 0.014, 0.012), smoothstep(0.35, 1.0, CF.x));
    col = lerp(col, sootc, CF.x);
    col = lerp(col, float3(0.004, 0.004, 0.004), CF.y * 0.9);
    col = lerp(col, float3(0.86, 0.90, 0.95), saturate(s_frost * (0.55 + 0.6 * s_fine)));
    float s_cs = abs(frac((W.x * 0.83 + W.y * 0.61 + W.z * 0.47) / 19.0 + s_n1 * 2.5) - 0.5);
    float s_crack = (1.0 - smoothstep(0.0, 0.03, s_cs)) * step(0.45, s_n1 + s_dmg * 0.4);
    col = lerp(col, col * 0.25, s_crack * saturate(s_dmg * 1.5));
    col = lerp(col, col * 0.8 + 0.06, s_dmg * s_fine * 0.5);
}
return max(col, 0.0);
"""

ORM = STATE_IN + r"""
float ao = lerp(1.0, R.r, UseTex);
float rough = lerp(BreakRough, R.g, UseTex);
float metal = R.b * UseTex;
[branch] if (SX.y > 0.0)
{
    rough = lerp(rough, 0.90, s_dirt * 0.6);
    rough = lerp(rough, 0.07, max(s_wet, max(s_oil, s_honey)) * 0.9);
    rough = lerp(rough, 0.02, WD.w);                                   // drops and streaks: mirror
    rough = lerp(rough, 0.25, max(s_jam, s_cola) * 0.8);
    rough = lerp(rough, 0.92, saturate(CF.x * 1.3));
    rough = lerp(rough, 0.50, s_frost * 0.8);
    rough = lerp(rough, 0.92, s_spoil * 0.7);
    metal *= 1.0 - saturate(CF.x * 1.3);
    metal *= 1.0 - s_frost * 0.7;
    ao *= 1.0 - CF.y * 0.6;
}
return float3(ao, rough, metal);
"""

NORMAL = STATE_IN + r"""
float3 n = normalize(NW);
[branch] if (SX.y > 0.0)
{
    n = normalize(n + WD.xyz);
    // deep char is matte and flat: the baked relief fades under the soot crust (the cracks are drawn in the colour and AO)
    n = normalize(lerp(n, normalize(VN), CF.x * 0.5));
}
return n;
"""

EMIS = STATE_IN + r"""
float3 em = float3(0, 0, 0);
[branch] if (SX.y > 0.0)
{
    float s_gl = smoothstep(0.55, 1.0, s_heat);
    em += lerp(float3(0.9, 0.05, 0.0), float3(3.2, 1.3, 0.35), s_gl) * s_gl * s_gl * 1.6;
    em += float3(2.2, 0.55, 0.06) * CF.z;                                  // embers: the burning edge only
    float s_sp = frac(sin(dot(floor(W.xy / 5.0 + W.zz / 5.0) + floor(Tm * 14.0) * 7.31, float2(12.9898, 78.233))) * 43758.5453);
    em += float3(0.5, 0.8, 3.0) * s_chg * step(0.965, s_sp) * 6.0;
}
return em;
"""

SWAY = r"""
float s = Sway * VC;
float ph = dot(W, float3(0.011, 0.017, 0.013));
float a = sin(T * 6.3 + ph) * 0.7 + sin(T * 10.9 + ph * 1.7) * 0.3;
return float3(0.71, 0.71, 0.0) * a * s * Amp + float3(-0.5, 0.6, 0.0) * sin(T * 4.7 + ph * 2.3) * s * Amp * 0.5;
"""

def build():
    # rebuilt in place (deleting it would orphan every MI_<Name> of the props)
    m = unreal.load_asset(DIR + "/M_Hero")
    if m: MEL.delete_all_material_expressions(m)
    else: m = AT.create_asset("M_Hero", DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("tangent_space_normal", False)       # the drops and cracks tilt the normal in world space
    X = -2400
    c = tex(m, "C", "/Game/Kitchen/SM_K_Bucket/T_SM_K_Bucket_C", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, X, -600)
    n = tex(m, "N", "/Game/Kitchen/SM_K_Bucket/T_SM_K_Bucket_N", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, X, -300)
    # (the R sets are imported as masks: the sampler must say so, and so must its default, or the material does not compile)
    r = tex(m, "R", "/Game/Kitchen/SM_K_Bucket/T_SM_K_Bucket_R", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, X, 0)
    wp = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, X, 300)
    tm = MEL.create_material_expression(m, unreal.MaterialExpressionTime, X, 400)
    vc = MEL.create_material_expression(m, unreal.MaterialExpressionVertexColor, X, 500)
    vn = MEL.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, X, 600)
    tint = vp(m, "Tint", (1, 1, 1, 1), X, 700)
    use = sp(m, "UseTex", 1.0, X, 800)
    brough = sp(m, "BreakRough", 0.75, X, 900)
    amp = sp(m, "SwayAmp", 28.0, X, 1000)
    sway = cpd(m, "StateSway", SWAY_INDEX, X, 1100)
    # tangent -> world normal of the baked map
    nw = MEL.create_material_expression(m, unreal.MaterialExpressionTransform, X + 300, -300)
    nw.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_TANGENT)
    nw.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    MEL.connect_material_expressions(n, "RGB", nw, "")
    # the AntState inputs
    lp = MEL.create_material_expression(m, unreal.MaterialExpressionTransformPosition, X, 1300)
    lp.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    lp.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    MEL.connect_material_expressions(wp, "", lp, "")
    st = {"LP": lp}
    for i in range(4): st["S%d" % i] = cpd4(m, "State%d" % i, i * 4, X, 1400 + i * 100)
    for i in range(5): st["P%d" % i] = cpd4(m, "StateSpot%d" % i, 16 + i * 4, X, 1850 + i * 100)
    sn = custom(m, STATE_NODE, ["W"] + STATE_PINS, X + 400, 1500, "AntState: the states at this pixel", unreal.CustomMaterialOutputType.CMOT_FLOAT4, ("Hot", "Soak", "Misc", "Aux"))
    wire(m, sn, [(wp, "", "W")] + [(st[p], "RGBA" if p[0] in "SP" and p[1:].isdigit() else "", p) for p in STATE_PINS])
    sv = [(sn, "", "SA"), (sn, "Hot", "SH"), (sn, "Soak", "SS"), (sn, "Misc", "SM"), (sn, "Aux", "SX")]
    wfx = custom(m, WETFX, ["W", "VN"] + STATE_USE, X + 900, 1200, "Hero wet: drops, streaks", unreal.CustomMaterialOutputType.CMOT_FLOAT4, ("Streak",))
    wire(m, wfx, [(wp, "", "W"), (vn, "", "VN")] + sv)
    cfx = custom(m, CHARFX, ["W", "Tm"] + STATE_USE, X + 900, 1700, "Hero char: soot, cracks, embers", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    wire(m, cfx, [(wp, "", "W"), (tm, "", "Tm")] + sv)
    fx = [(wfx, "", "WD"), (cfx, "", "CF")]
    col = custom(m, COLOR, ["C", "R", "W", "Tint", "UseTex", "WD", "CF"] + STATE_USE, -700, -500, "Hero colour + states")
    wire(m, col, [(c, "RGBA", "C"), (r, "RGBA", "R"), (wp, "", "W"), (tint, "", "Tint"), (use, "", "UseTex")] + fx + sv)
    orm = custom(m, ORM, ["R", "UseTex", "BreakRough", "WD", "CF"] + STATE_USE, -700, 100, "Hero AO rough metal + states")
    wire(m, orm, [(r, "RGBA", "R"), (use, "", "UseTex"), (brough, "", "BreakRough")] + fx + sv)
    nrm = custom(m, NORMAL, ["NW", "VN", "WD", "CF"] + STATE_USE, -700, -200, "Hero normal (world) + drops, cracks")
    wire(m, nrm, [(nw, "", "NW"), (vn, "", "VN")] + fx + sv)
    em = custom(m, EMIS, ["W", "Tm", "CF"] + STATE_USE, -700, 600, "Hero state light")
    wire(m, em, [(wp, "", "W"), (tm, "", "Tm"), (cfx, "", "CF")] + sv)
    sw = custom(m, SWAY, ["W", "T", "VC", "Sway", "Amp"], -700, 1100, "Hero sway (WPO)")
    wire(m, sw, [(wp, "", "W"), (tm, "", "T"), (vc, "R", "VC"), (sway, "", "Sway"), (amp, "", "Amp")])
    def mask(src, ch, x, y):
        k = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, x, y)
        k.set_editor_property("r", ch == 0); k.set_editor_property("g", ch == 1); k.set_editor_property("b", ch == 2); k.set_editor_property("a", False)
        MEL.connect_material_expressions(src, "", k, ""); return k
    MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(mask(orm, 0, -300, 100), "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    MEL.connect_material_property(mask(orm, 1, -300, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(mask(orm, 2, -300, 300), "", unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(sw, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    MEL.recompile_material(m)
    EAL.save_asset(DIR + "/M_Hero")
    unreal.log_warning("ART: HERO M_Hero built (AntState layer, sway in custom data %d)" % SWAY_INDEX)

def break_mi(name, tint, rough):
    mi = unreal.load_asset(DIR + "/" + name) or AT.create_asset(name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())   # (kept: the break pieces point at it)
    MEL.set_material_instance_parent(mi, unreal.load_asset(DIR + "/M_Hero"))
    MEL.set_material_instance_scalar_parameter_value(mi, "UseTex", 0.0)
    MEL.set_material_instance_scalar_parameter_value(mi, "BreakRough", rough)
    MEL.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*tint))
    EAL.save_asset(DIR + "/" + name)
    unreal.log_warning("ART: HERO " + name)

build()
break_mi("MI_HeroBreak_Plastic", (0.62, 0.70, 0.86, 1), 0.7)   # stress-whitened blue plastic at the crack
break_mi("MI_HeroBreak_Wood", (0.78, 0.62, 0.40, 1), 0.85)      # raw beech, lighter than the varnish
break_mi("MI_HeroBreak_Metal", (0.55, 0.56, 0.58, 1), 0.45)     # torn sheet metal
