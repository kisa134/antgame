# KAN-22: the material of the hero props (Tools/blender/hero_lib.py): their own baked texture set through UV0, and the
# state layer of AntState.h on top (custom primitive data 0 wet, 1 charred, 2 dirty, 3 damaged), plus the loose parts' sway (4).
#   /Game/Kitchen/Hero/M_Hero            C (colour), N (tangent normal, DirectX), R (AO, roughness, metal, a = porosity)
#   /Game/Kitchen/Hero/MI_HeroBreak_*    the fresh fracture faces of the pre-cut break pieces (no texture: UseTex 0)
# Wet darkens by porosity (yarn a lot, plastic little) and glosses; char blackens through a noise threshold with embers at its
# front; dirt tints and roughens; damage draws cracks. All zero: the baked look. Sway: vertex colour R x data 4, a slow swing.
# Run: UnrealEditor-Cmd.exe AntKitchen.uproject -run=pythonscript -script=<copy of this file> (Saved/art/ue_py.cmd)
import unreal
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/Kitchen/Hero"

def fresh(name, cls, factory):
    full = DIR + "/" + name
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    return AT.create_asset(name, DIR, cls, factory)

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

def tex(m, name, default, sampler, x, y):
    t = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    t.set_editor_property("parameter_name", name)
    d = unreal.load_asset(default)
    if d: t.set_editor_property("texture", d)
    t.set_editor_property("sampler_type", sampler)
    return t

def custom(m, code, inputs, x, y, desc, out=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    c = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("code", code); c.set_editor_property("output_type", out); c.set_editor_property("description", desc)
    ins = []
    for n in inputs:
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
    c.set_editor_property("inputs", ins)
    return c

NOISE = r"""
float2 p3 = (W.xy + W.zz * 0.31 + W.yx * 0.11) / 41.0;
float2 i3 = floor(p3); float2 f3 = frac(p3); f3 = f3 * f3 * (3.0 - 2.0 * f3);
float k00 = frac(sin(dot(i3, float2(12.9898, 78.233))) * 43758.5453);
float k10 = frac(sin(dot(i3 + float2(1, 0), float2(12.9898, 78.233))) * 43758.5453);
float k01 = frac(sin(dot(i3 + float2(0, 1), float2(12.9898, 78.233))) * 43758.5453);
float k11 = frac(sin(dot(i3 + float2(1, 1), float2(12.9898, 78.233))) * 43758.5453);
float v3 = lerp(lerp(k00, k10, f3.x), lerp(k01, k11, f3.x), f3.y);
float2 p2 = (W.yz + W.xx * 0.21) / 310.0;
float2 i2 = floor(p2); float2 f2 = frac(p2); f2 = f2 * f2 * (3.0 - 2.0 * f2);
float g00 = frac(sin(dot(i2, float2(269.5, 183.3))) * 43758.5453);
float g10 = frac(sin(dot(i2 + float2(1, 0), float2(269.5, 183.3))) * 43758.5453);
float g01 = frac(sin(dot(i2 + float2(0, 1), float2(269.5, 183.3))) * 43758.5453);
float g11 = frac(sin(dot(i2 + float2(1, 1), float2(269.5, 183.3))) * 43758.5453);
float v2 = lerp(lerp(g00, g10, f2.x), lerp(g01, g11, f2.x), f2.y);
float fine = v3 * 0.6 + v2 * 0.4;
float cm = smoothstep(1.05 - Char * 1.25, 1.30 - Char * 1.25, fine + 0.15) * saturate(Char * 4.0);
"""

COLOR = NOISE + r"""
float3 col = lerp(Tint, C.rgb * Tint * lerp(1.0, R.r, 0.45), UseTex);
float por = lerp(0.3, R.a, UseTex);
col = lerp(col, float3(0.24, 0.20, 0.15), saturate(Dirt * (0.25 + fine)) * 0.85);
col *= lerp(1.0, lerp(0.78, 0.36, por), saturate(Wet));
col = lerp(col, float3(0.020, 0.017, 0.014), cm);
float cs = abs(frac((W.x * 0.83 + W.y * 0.61 + W.z * 0.47) / 19.0 + v3 * 2.5) - 0.5);
float crack = (1.0 - smoothstep(0.0, 0.03, cs)) * step(0.45, v2 + Dmg * 0.4);
col = lerp(col, col * 0.25, crack * saturate(Dmg * 1.5));
col = lerp(col, col * 0.8 + 0.06, saturate(Dmg) * fine * 0.5);
return max(col, 0.0);
"""

ORM = NOISE + r"""
float ao = lerp(1.0, R.r, UseTex);
float rough = lerp(BreakRough, R.g, UseTex);
float metal = R.b * UseTex;
float por = lerp(0.3, R.a, UseTex);
rough = lerp(rough, 0.90, saturate(Dirt) * 0.6);
rough = lerp(rough, lerp(0.18, 0.06, por), saturate(Wet) * 0.85);
rough = lerp(rough, 0.95, cm);
metal *= 1.0 - cm;
return float3(ao, rough, metal);
"""

EMBER = NOISE + r"""
float front = saturate(1.0 - abs(cm - 0.45) * 5.0) * saturate(Char * 2.0 - 0.2);
float fl = 0.65 + 0.35 * sin(T * 3.1 + W.x * 0.07 + W.y * 0.05) * sin(T * 1.7 + W.z * 0.09);
return float3(1.0, 0.25, 0.03) * front * fl * 9.0;
"""

SWAY = r"""
float s = Sway * VC;
float ph = dot(W, float3(0.011, 0.017, 0.013));
float a = sin(T * 6.3 + ph) * 0.7 + sin(T * 10.9 + ph * 1.7) * 0.3;
return float3(0.71, 0.71, 0.0) * a * s * Amp + float3(-0.5, 0.6, 0.0) * sin(T * 4.7 + ph * 2.3) * s * Amp * 0.5;
"""

def build():
    m = fresh("M_Hero", unreal.Material, unreal.MaterialFactoryNew())
    X = -1500
    c = tex(m, "C", "/Engine/EngineResources/WhiteSquareTexture", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, X, -600)
    n = tex(m, "N", "/Engine/EngineMaterials/FlatNormal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, X, -300)
    r = tex(m, "R", "/Engine/EngineResources/WhiteSquareTexture", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, X, 0)
    wp = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, X, 300)
    tm = MEL.create_material_expression(m, unreal.MaterialExpressionTime, X, 400)
    vc = MEL.create_material_expression(m, unreal.MaterialExpressionVertexColor, X, 500)
    tint = vp(m, "Tint", (1, 1, 1, 1), X, 650)
    use = sp(m, "UseTex", 1.0, X, 750)
    brough = sp(m, "BreakRough", 0.75, X, 850)
    amp = sp(m, "SwayAmp", 28.0, X, 950)
    wet = cpd(m, "StateWet", 0, X, 1100); char = cpd(m, "StateChar", 1, X, 1200); dirt = cpd(m, "StateDirt", 2, X, 1300); dmg = cpd(m, "StateDamage", 3, X, 1400)
    sway = cpd(m, "StateSway", 4, X, 1500)
    col = custom(m, COLOR, ["C", "R", "W", "Tint", "UseTex", "Wet", "Char", "Dirt", "Dmg"], -700, -400, "Hero colour + state")
    for src, pin, out in [(c, "C", "RGBA"), (r, "R", "RGBA"), (wp, "W", ""), (tint, "Tint", "RGB"), (use, "UseTex", ""), (wet, "Wet", ""), (char, "Char", ""), (dirt, "Dirt", ""), (dmg, "Dmg", "")]:
        MEL.connect_material_expressions(src, out, col, pin)
    orm = custom(m, ORM, ["C", "R", "W", "UseTex", "BreakRough", "Wet", "Char", "Dirt"], -700, 200, "Hero AO rough metal + state")
    for src, pin, out in [(c, "C", "RGBA"), (r, "R", "RGBA"), (wp, "W", ""), (use, "UseTex", ""), (brough, "BreakRough", ""), (wet, "Wet", ""), (char, "Char", ""), (dirt, "Dirt", "")]:
        MEL.connect_material_expressions(src, out, orm, pin)
    emb = custom(m, EMBER, ["W", "T", "Char"], -700, 700, "Hero embers")
    for src, pin in [(wp, "W"), (tm, "T"), (char, "Char")]:
        MEL.connect_material_expressions(src, "", emb, pin)
    sw = custom(m, SWAY, ["W", "T", "VC", "Sway", "Amp"], -700, 1100, "Hero sway (WPO)")
    for src, pin, out in [(wp, "W", ""), (tm, "T", ""), (vc, "VC", "R"), (sway, "Sway", ""), (amp, "Amp", "")]:
        MEL.connect_material_expressions(src, out, sw, pin)
    def mask(src, ch, x, y):
        k = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, x, y)
        k.set_editor_property("r", ch == 0); k.set_editor_property("g", ch == 1); k.set_editor_property("b", ch == 2); k.set_editor_property("a", False)
        MEL.connect_material_expressions(src, "", k, ""); return k
    MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(mask(orm, 0, -300, 200), "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    MEL.connect_material_property(mask(orm, 1, -300, 300), "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(mask(orm, 2, -300, 400), "", unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(emb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(sw, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    MEL.recompile_material(m)
    EAL.save_asset(DIR + "/M_Hero")
    unreal.log_warning("ART: HERO M_Hero built")

def break_mi(name, tint, rough):
    mi = fresh(name, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, unreal.load_asset(DIR + "/M_Hero"))
    MEL.set_material_instance_scalar_parameter_value(mi, "UseTex", 0.0)
    MEL.set_material_instance_scalar_parameter_value(mi, "BreakRough", rough)
    MEL.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*tint))
    EAL.save_asset(DIR + "/" + name)
    unreal.log_warning("ART: HERO " + name)

build()
break_mi("MI_HeroBreak_Plastic", (0.62, 0.70, 0.86, 1), 0.7)   # stress-whitened blue plastic at the crack
break_mi("MI_HeroBreak_Wood", (0.78, 0.62, 0.40, 1), 0.85)      # raw beech, lighter than the varnish
