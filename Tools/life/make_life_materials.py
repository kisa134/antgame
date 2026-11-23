# Living flat (docs/level/LIFE.md): the materials of the screens, the sky, the house across the yard and the rain.
#   /Game/Life/Materials/M_Life_Screen   a TV / tablet screen: a media texture (external sampler), glass, glow, scan lines, static
#   /Game/Life/Materials/M_Life_Display  a small display or a phone: any texture (render target, a feed), scroll, glow
#   /Game/Life/Materials/M_Life_Clouds   the cloud layer high over the house: procedural, drifting, tinted by the time of day
#   /Game/Life/Materials/M_Life_Stars    the night dome: stars that twinkle and the moon (additive)
#   /Game/Life/Materials/M_Life_Facade   the house across the yard: plaster, window grid, lit windows by night (seeded)
#   /Game/Life/Materials/M_Life_Rain     drops running down the window opening (translucent)
#   /Game/Life/Media/MT_Life_Default     the default media texture of M_Life_Screen
# Run: UnrealEditor-Cmd.exe AntKitchen.uproject -run=pythonscript -script=Tools/life/make_life_materials.py
import unreal, os
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
MAT = "/Game/Life/Materials"
MED = "/Game/Life/Media"
ONLY = [s for s in os.environ.get("LIFE_MATS", "").split(",") if s]

def fresh(path, name):
    full = path + "/" + name
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    return AT.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())

def custom(m, code, inputs, x, y, desc, out=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    c = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("code", code)
    c.set_editor_property("output_type", out)
    c.set_editor_property("description", desc)
    ins = []
    for n in inputs:
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
    c.set_editor_property("inputs", ins)
    return c

def sp(m, name, val, x, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", val); return p

def vp(m, name, val, x, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", unreal.LinearColor(*val)); return p

def const(m, v, x, y):
    c = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, x, y)
    c.set_editor_property("r", v); return c

def wire(m, node, pins):
    for src, pin in pins:
        MEL.connect_material_expressions(src, "", node, pin)

def finish(m, name):
    MEL.recompile_material(m)
    EAL.save_asset(MAT + "/" + name)
    unreal.log("LIFEMAT " + name)

def time(m, x, y):
    return MEL.create_material_expression(m, unreal.MaterialExpressionTime, x, y)

def texcoord(m, x, y):
    return MEL.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, x, y)

# ------------------------------------------------------------------ default media texture
if not EAL.does_asset_exist(MED + "/MT_Life_Default"):
    AT.create_asset("MT_Life_Default", MED, unreal.MediaTexture, unreal.MediaTextureFactoryNew())
    EAL.save_asset(MED + "/MT_Life_Default")
mt = unreal.load_asset(MED + "/MT_Life_Default")

# ------------------------------------------------------------------ the screen
SCREEN = r"""
float2 uv = UV * float2(TileU, TileV) + float2(OffU, OffV);
uv = lerp(uv, 1.0 - uv, float2(FlipU, FlipV));
float3 v = Texture2DSample(Tex, TexSampler, uv).rgb;
float2 cell = floor(uv * float2(240.0, 135.0)) + floor(T * 24.0) * float2(17.0, 31.0);
float n = frac(sin(dot(cell, float2(12.9898, 78.233))) * 43758.5453);
float roll = 0.7 + 0.3 * sin(uv.y * 40.0 + T * 18.0);
float3 col = v * Bright;
col = lerp(col, (n * roll).xxx * 0.9, saturate(Static));
float scan = 0.93 + 0.07 * sin(uv.y * 540.0 * 3.14159);
float2 d = uv - 0.5;
float vig = saturate(1.0 - dot(d, d) * 0.8);
return max(col * scan * vig * Power * Tint, 0.0);
"""

def build_screen():
    m = fresh(MAT, "M_Life_Screen")
    X = -1200
    tex = MEL.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, X, -300)
    tex.set_editor_property("parameter_name", "Video")
    tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_EXTERNAL)
    if mt: tex.set_editor_property("texture", mt)
    c = custom(m, SCREEN, ["Tex", "UV", "T", "Bright", "Static", "Power", "Tint", "FlipU", "FlipV", "TileU", "TileV", "OffU", "OffV"], -600, -200, "life screen")
    wire(m, c, [(tex, "Tex"), (texcoord(m, X, -100), "UV"), (time(m, X, 0), "T"), (sp(m, "Bright", 2.5, X, 100), "Bright"),
                (sp(m, "Static", 0.0, X, 200), "Static"), (sp(m, "Power", 0.0, X, 300), "Power"), (sp(m, "FlipU", 0.0, X, 500), "FlipU"),
                (sp(m, "FlipV", 0.0, X, 600), "FlipV"), (sp(m, "TileU", 1.0, X, 1000), "TileU"), (sp(m, "TileV", 1.0, X, 1100), "TileV"),
                (sp(m, "OffU", 0.0, X, 1200), "OffU"), (sp(m, "OffV", 0.0, X, 1300), "OffV")])
    tint = vp(m, "Tint", (1, 1, 1, 1), X, 400)
    MEL.connect_material_expressions(tint, "RGB", c, "Tint")
    MEL.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(vp(m, "Glass", (0.012, 0.013, 0.015, 1), X, 700), "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(sp(m, "Rough", 0.07, X, 800), "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(const(m, 0.6, X, 900), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m, "M_Life_Screen")

# ------------------------------------------------------------------ a small display (render target, phone feed, a printed note)
DISPLAY = r"""
float2 uv = lerp(UV, 1.0 - UV, float2(FlipU, FlipV));
uv = uv * float2(TileU, TileV) + float2(OffU, OffV + T * Scroll);
float3 c = Texture2DSample(Tex, TexSampler, uv).rgb;
return max(c * Bright * Power * Tint, 0.0);
"""
BASEC = r"""
float2 uv = lerp(UV, 1.0 - UV, float2(FlipU, FlipV));
uv = uv * float2(TileU, TileV) + float2(OffU, OffV + T * Scroll);
float3 c = Texture2DSample(Tex, TexSampler, uv).rgb;
return lerp(c * Tint, Glass, Power);
"""

def build_display():
    m = fresh(MAT, "M_Life_Display")
    X = -1200
    tex = MEL.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, X, -300)
    tex.set_editor_property("parameter_name", "Tex")
    d = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    if d: tex.set_editor_property("texture", d)
    uvn = texcoord(m, X, -100); tn = time(m, X, 0)
    names = [("Bright", 2.0), ("Power", 1.0), ("Scroll", 0.0), ("TileU", 1.0), ("TileV", 1.0), ("OffU", 0.0), ("OffV", 0.0), ("FlipU", 0.0), ("FlipV", 0.0)]
    params = {}
    for i, (n, v) in enumerate(names):
        params[n] = sp(m, n, v, X, 100 + i * 90)
    tint = vp(m, "Tint", (1, 1, 1, 1), X, 1000)
    glass = vp(m, "Glass", (0.015, 0.016, 0.018, 1), X, 1100)
    e = custom(m, DISPLAY, ["Tex", "UV", "T", "Bright", "Power", "Scroll", "TileU", "TileV", "OffU", "OffV", "FlipU", "FlipV", "Tint"], -600, -200, "display glow")
    b = custom(m, BASEC, ["Tex", "UV", "T", "Power", "Scroll", "TileU", "TileV", "OffU", "OffV", "FlipU", "FlipV", "Tint", "Glass"], -600, 300, "display base")
    for node in (e, b):
        wire(m, node, [(tex, "Tex"), (uvn, "UV"), (tn, "T")] + [(params[k], k) for k in params if k != "Bright" or node is e])
        MEL.connect_material_expressions(tint, "RGB", node, "Tint")
    MEL.connect_material_expressions(glass, "RGB", b, "Glass")
    MEL.connect_material_property(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(b, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(sp(m, "Rough", 0.25, X, 1200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(m, "M_Life_Display")

# ------------------------------------------------------------------ clouds
CLOUD = r"""
float2 p = W.xy / Scale + float2(T * Speed, T * Speed * 0.37);
float a = 0.0, amp = 0.55, tot = 0.0;
for (int k = 0; k < 5; k++)
{
    float2 i = floor(p); float2 f = frac(p); f = f * f * (3.0 - 2.0 * f);
    float h00 = frac(sin(dot(i, float2(127.1, 311.7))) * 43758.5453);
    float h10 = frac(sin(dot(i + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
    float h01 = frac(sin(dot(i + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
    float h11 = frac(sin(dot(i + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
    a += lerp(lerp(h00, h10, f.x), lerp(h01, h11, f.x), f.y) * amp;
    tot += amp; amp *= 0.5; p = p * 2.03 + float2(5.2, 1.3);
}
a /= tot;
float d = saturate((a - (1.0 - Cover)) / 0.22);
float edge = saturate(1.0 - length(W.xy - C.xy) / Radius);
return d * edge * edge * Opacity;
"""
CLOUDCOL = r"""
float2 p = W.xy / (Scale * 0.5) + float2(T * Speed * 2.0, 0.0);
float2 i = floor(p); float2 f = frac(p); f = f * f * (3.0 - 2.0 * f);
float h00 = frac(sin(dot(i, float2(12.9, 78.2))) * 43758.5453);
float h10 = frac(sin(dot(i + float2(1, 0), float2(12.9, 78.2))) * 43758.5453);
float h01 = frac(sin(dot(i + float2(0, 1), float2(12.9, 78.2))) * 43758.5453);
float h11 = frac(sin(dot(i + float2(1, 1), float2(12.9, 78.2))) * 43758.5453);
float v = lerp(lerp(h00, h10, f.x), lerp(h01, h11, f.x), f.y);
return lerp(Shade, Lit, v);
"""

def build_clouds():
    m = fresh(MAT, "M_Life_Clouds")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    X = -1200
    wp = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, X, -300)
    ap = MEL.create_material_expression(m, unreal.MaterialExpressionObjectPositionWS, X, -200)
    tn = time(m, X, -100)
    scale = sp(m, "Scale", 60000.0, X, 0); speed = sp(m, "Speed", 0.012, X, 100)
    o = custom(m, CLOUD, ["W", "C", "T", "Scale", "Speed", "Cover", "Opacity", "Radius"], -600, -200, "cloud density", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    wire(m, o, [(wp, "W"), (ap, "C"), (tn, "T"), (scale, "Scale"), (speed, "Speed"),
                (sp(m, "Cover", 0.45, X, 200), "Cover"), (sp(m, "Opacity", 0.9, X, 300), "Opacity"), (sp(m, "Radius", 400000.0, X, 400), "Radius")])
    c = custom(m, CLOUDCOL, ["W", "T", "Scale", "Speed", "Lit", "Shade"], -600, 300, "cloud colour")
    wire(m, c, [(wp, "W"), (tn, "T"), (scale, "Scale"), (speed, "Speed")])
    MEL.connect_material_expressions(vp(m, "Lit", (1.6, 1.55, 1.5, 1), X, 700), "RGB", c, "Lit")
    MEL.connect_material_expressions(vp(m, "Shade", (0.8, 0.84, 0.92, 1), X, 800), "RGB", c, "Shade")
    MEL.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(o, "", unreal.MaterialProperty.MP_OPACITY)
    finish(m, "M_Life_Clouds")

# ------------------------------------------------------------------ stars and the moon (additive, on a huge sphere round the flat)
STARS = r"""
float3 d = normalize(W - Cam);
if (d.z < -0.05) return float3(0, 0, 0);
float3 p = d * 420.0;
float3 i = floor(p);
float h = frac(sin(dot(i, float3(127.1, 311.7, 74.7))) * 43758.5453);
float3 f = frac(p) - 0.5;
float star = step(0.985, h) * saturate(1.0 - length(f) * 3.2);
float tw = 0.65 + 0.35 * sin(T * (2.0 + h * 9.0) + h * 40.0);
float3 col = star * tw * lerp(float3(0.8, 0.85, 1.0), float3(1.0, 0.9, 0.75), frac(h * 13.0)) * StarB;
float md = acos(saturate(dot(d, normalize(Moon))));
float disc = saturate((MoonR - md) / (MoonR * 0.08));
float glow = exp(-md / (MoonR * 2.5)) * 0.25;
float3 mc = float3(1.0, 0.97, 0.9) * (disc * (0.85 + 0.15 * sin(d.x * 900.0) * sin(d.y * 700.0)) * MoonB + glow * MoonB * 0.3);
return (col + mc) * Opacity;
"""

def build_stars():
    m = fresh(MAT, "M_Life_Stars")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    X = -1200
    c = custom(m, STARS, ["W", "Cam", "T", "StarB", "Moon", "MoonR", "MoonB", "Opacity"], -600, -200, "stars and moon")
    wire(m, c, [(MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, X, -300), "W"),
                (MEL.create_material_expression(m, unreal.MaterialExpressionCameraPositionWS, X, -200), "Cam"), (time(m, X, -100), "T"),
                (sp(m, "StarB", 6.0, X, 0), "StarB"), (sp(m, "MoonR", 0.035, X, 200), "MoonR"), (sp(m, "MoonB", 8.0, X, 300), "MoonB"), (sp(m, "Opacity", 1.0, X, 400), "Opacity")])
    MEL.connect_material_expressions(vp(m, "Moon", (-0.3, -0.8, 0.35, 1), X, 100), "RGB", c, "Moon")
    MEL.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m, "M_Life_Stars")

# ------------------------------------------------------------------ the house across the yard
FAC_HEAD = r"""
float2 q = abs(N.x) > abs(N.y) ? float2(W.y, W.z) : float2(W.x, W.z);
float2 cell = floor(q / Cell);
float2 f = frac(q / Cell);
float h = frac(sin(dot(cell + Seed * 7.13, float2(127.1, 311.7))) * 43758.5453);
float h2 = frac(h * 91.7 + 0.37);
float win = step(0.18, f.x) * step(f.x, 0.82) * step(0.22, f.y) * step(f.y, 0.86);
float top = step(Top, W.z);
win *= (1.0 - top);
float frame = win * (1.0 - step(0.49, f.x) * step(f.x, 0.51));
float lit = step(1.0 - Lit, h) * win;
float tv = step(0.86, h2) * lit;
float curtain = step(0.5, h2) * step(0.62, f.y + (h2 - 0.5) * 0.3);
"""
FAC_COL = FAC_HEAD + r"""
float3 wall = Wall * (0.92 + 0.08 * frac(sin(dot(floor(q / 60.0), float2(12.9, 78.2))) * 43758.5));
float3 glass = lerp(float3(0.05, 0.06, 0.07), float3(0.35, 0.4, 0.45), saturate(f.y));
float3 cur = lerp(float3(0.75, 0.6, 0.45), float3(0.55, 0.65, 0.7), h2);
float3 c = lerp(wall, lerp(glass, cur * 0.6, curtain), frame);
c = lerp(c, Wall * 0.55, top);
return c;
"""
FAC_EM = FAC_HEAD + r"""
float3 warm = lerp(float3(1.0, 0.72, 0.42), float3(1.0, 0.86, 0.66), h2);
float flick = 0.6 + 0.4 * sin(T * (3.0 + h * 5.0)) * sin(T * 7.3 + h * 20.0);
float3 tvc = float3(0.35, 0.5, 1.0) * flick;
float3 e = lerp(warm * lerp(1.0, 0.55, curtain), tvc, tv) * lit * frame * Glow;
return e;
"""
FAC_R = FAC_HEAD + r"""
return lerp(0.85, 0.08, frame * (1.0 - curtain));
"""

def build_facade():
    m = fresh(MAT, "M_Life_Facade")
    X = -1200
    wp = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, X, -300)
    vn = MEL.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, X, -200)
    tn = time(m, X, -100)
    cell = vp(m, "Cell", (1900, 5600, 0, 1), X, 0)
    seed = sp(m, "Seed", 1.0, X, 100); lit = sp(m, "Lit", 0.0, X, 200); top = sp(m, "Top", 30000.0, X, 300); glow = sp(m, "Glow", 6.0, X, 400)
    wall = vp(m, "Wall", (0.78, 0.7, 0.6, 1), X, 500)
    outs = []
    for code, desc, kind in ((FAC_COL, "facade colour", unreal.CustomMaterialOutputType.CMOT_FLOAT3), (FAC_EM, "facade windows", unreal.CustomMaterialOutputType.CMOT_FLOAT3),
                             (FAC_R, "facade rough", unreal.CustomMaterialOutputType.CMOT_FLOAT1)):
        c = custom(m, code, ["W", "N", "T", "Cell", "Seed", "Lit", "Top", "Glow", "Wall"], -600, -300 + len(outs) * 400, desc, kind)
        wire(m, c, [(wp, "W"), (vn, "N"), (tn, "T"), (seed, "Seed"), (lit, "Lit"), (top, "Top"), (glow, "Glow")])
        MEL.connect_material_expressions(cell, "RG", c, "Cell")
        MEL.connect_material_expressions(wall, "RGB", c, "Wall")
        outs.append(c)
    MEL.connect_material_property(outs[0], "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(outs[1], "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(outs[2], "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(m, "M_Life_Facade")

# ------------------------------------------------------------------ rain on the window
RAIN = r"""
float2 uv = UV * float2(Cols, Cols * Aspect);
float col = floor(uv.x);
float h = frac(sin(col * 127.1) * 43758.5453);
float y = frac(uv.y * 0.25 - T * (0.25 + h * 0.6) + h);
float x = frac(uv.x) - 0.5 + (h - 0.5) * 0.4;
float drop = saturate(1.0 - length(float2(x * 3.0, (y - 0.5) * 2.0)) * 3.0);
float trail = saturate(1.0 - abs(x) * 14.0) * step(0.5, y) * ((1.0 - y) * 1.2) * 0.35;
float2 spc = floor(UV * float2(Cols * 3.0, Cols * 3.0 * Aspect));
float sh = frac(sin(dot(spc, float2(12.9898, 78.233))) * 43758.5453);
float2 sf = frac(UV * float2(Cols * 3.0, Cols * 3.0 * Aspect)) - 0.5;
float spot = step(0.9, sh) * saturate(1.0 - length(sf) * 4.0) * 0.7;
return saturate(drop + trail + spot) * Rain;
"""

def build_rain():
    m = fresh(MAT, "M_Life_Rain")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    X = -1200
    c = custom(m, RAIN, ["UV", "T", "Cols", "Aspect", "Rain"], -600, -200, "rain drops", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    wire(m, c, [(texcoord(m, X, -300), "UV"), (time(m, X, -200), "T"), (sp(m, "Cols", 26.0, X, -100), "Cols"), (sp(m, "Aspect", 0.66, X, 0), "Aspect"), (sp(m, "Rain", 0.0, X, 100), "Rain")])
    col = vp(m, "Color", (0.75, 0.82, 0.9, 1), X, 200)
    MEL.connect_material_property(col, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 100)
    MEL.connect_material_expressions(c, "", mul, "A"); MEL.connect_material_expressions(sp(m, "Opacity", 0.45, X, 300), "", mul, "B")
    MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_OPACITY)
    finish(m, "M_Life_Rain")

def build_decal():
    m = fresh(MAT, "M_Life_Decal")
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    X = -1000
    t = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, X, -200)
    t.set_editor_property("parameter_name", "C")
    d = unreal.load_asset("/Game/Life/Textures/T_Life_CoffeeRing")
    if d: t.set_editor_property("texture", d)
    tint = vp(m, "Tint", (1, 1, 1, 1), X, 100)
    mc = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -500, -200)
    MEL.connect_material_expressions(t, "RGB", mc, "A"); MEL.connect_material_expressions(tint, "RGB", mc, "B")
    MEL.connect_material_property(mc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mo = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -500, 100)
    MEL.connect_material_expressions(t, "A", mo, "A"); MEL.connect_material_expressions(sp(m, "Opacity", 1.0, X, 200), "", mo, "B")
    MEL.connect_material_property(mo, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.connect_material_property(sp(m, "Rough", 0.6, X, 300), "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(m, "M_Life_Decal")

for n, f in (("Screen", build_screen), ("Decal", build_decal), ("Display", build_display), ("Clouds", build_clouds), ("Stars", build_stars), ("Facade", build_facade), ("Rain", build_rain)):
    if not ONLY or n in ONLY:
        f()

# ------------------------------------------------------------------ what the code needs to know of a few meshes (the screen of the TV...)
def sections(path):
    sm = unreal.load_asset(path)
    if not sm:
        unreal.log("LIFEMESH %s missing" % path); return
    b = sm.get_bounding_box()
    unreal.log("LIFEMESH %s bounds min (%.1f %.1f %.1f) max (%.1f %.1f %.1f)" % (path, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z))
    for i, mm in enumerate(sm.get_editor_property("static_materials")):
        unreal.log("LIFEMESH %s slot %d %s" % (path, i, mm.get_editor_property("material_slot_name")))
    for s in range(sm.get_num_sections(0)):
        try:
            v, t, nr, uvs, tg = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(sm, 0, s)
        except Exception as e:
            unreal.log("LIFEMESH %s section %d: %s" % (path, s, e)); continue
        if not v: continue
        xs = [p.x for p in v]; ys = [p.y for p in v]; zs = [p.z for p in v]
        unreal.log("LIFEMESH %s section %d verts %d min (%.1f %.1f %.1f) max (%.1f %.1f %.1f) uv0 (%.2f..%.2f, %.2f..%.2f)" % (path, s, len(v), min(xs), min(ys), min(zs), max(xs), max(ys), max(zs),
            min(u.x for u in uvs) if uvs else 0, max(u.x for u in uvs) if uvs else 0, min(u.y for u in uvs) if uvs else 0, max(u.y for u in uvs) if uvs else 0))

for p in os.environ.get("LIFE_MESHES", "/Game/Living/SM_L_TV/SM_L_TV,/Game/Living/SM_L_Remote/SM_L_Remote,/Game/Kitchen/SM_K_Clock/SM_K_Clock,/Game/Kitchen/SM_K_Microwave/SM_K_Microwave,/Game/LevelV3/SM_V3_Phone/SM_V3_Phone,/Game/Kitchen/SM_K_Fridge/SM_K_Fridge,/Game/Map4/Props/SM_M4_Plane/SM_M4_Plane,/Game/Map4/Props/SM_M4_Phone/SM_M4_Phone,/Game/Kitchen/SM_K_Window/SM_K_Window,/Game/Kitchen/SM_K_Kettle/SM_K_Kettle,/Game/Map4/Props/SM_M4R_K_Calendar/SM_M4R_K_Calendar,/Game/Kitchen/SM_K_Dishwasher/SM_K_Dishwasher,/Game/Living/SM_L_FloorLamp/SM_L_FloorLamp").split(","):
    sections(p)
unreal.log("LIFE_DONE")
