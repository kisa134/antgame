# KAN-22 (small props): glass for the hero props. Glass is glass: a translucent surface (lit per pixel, forward shading),
# the prop's own baked set through UV0 (C tints it, R.g its roughness, N the moulding), a fresnel edge, and the state layer
# of AntState.h (custom primitive data 0 wet, 1 charred, 2 dirty, 3 damaged): soot and dirt make it more opaque and dull,
# damage draws cracks. Plus the break-face instances of the hero props' other classes (ceramic, glass, paper, metal).
#   /Game/Kitchen/Hero/M_HeroGlass         (MI_<Name>_Glass per prop: Tools/import_blender_art.py, set hero_props)
#   /Game/Kitchen/Hero/MI_HeroBreak_Ceramic, _Glass, _Paper, _Metal, _Cardboard
# Run: UnrealEditor-Cmd.exe AntKitchen.uproject -run=pythonscript -script=<copy of this file>
import unreal
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/Kitchen/Hero"

def sp(m, name, val, x, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", val); return p

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
float2 p3 = (W.xy + W.zz * 0.31) / 23.0; float2 i3 = floor(p3); float2 f3 = frac(p3); f3 = f3 * f3 * (3.0 - 2.0 * f3);
float k00 = frac(sin(dot(i3, float2(12.9898, 78.233))) * 43758.5453); float k10 = frac(sin(dot(i3 + float2(1, 0), float2(12.9898, 78.233))) * 43758.5453);
float k01 = frac(sin(dot(i3 + float2(0, 1), float2(12.9898, 78.233))) * 43758.5453); float k11 = frac(sin(dot(i3 + float2(1, 1), float2(12.9898, 78.233))) * 43758.5453);
float fine = lerp(lerp(k00, k10, f3.x), lerp(k01, k11, f3.x), f3.y);
float soot = saturate(Char * 1.6) * (0.6 + 0.4 * fine);
float dirt = saturate(Dirt) * (0.4 + 0.6 * fine);
float cs = abs(frac((W.x * 0.83 + W.y * 0.61 + W.z * 0.47) / 9.0 + fine * 2.5) - 0.5);
float crack = (1.0 - smoothstep(0.0, 0.025, cs)) * saturate(Dmg * 1.5);
"""
COLOR = NOISE + r"""
float3 col = C.rgb;
col = lerp(col, float3(0.30, 0.26, 0.20), dirt * 0.7);
col = lerp(col, float3(0.02, 0.018, 0.016), soot);
col = lerp(col, float3(0.95, 0.97, 0.98), crack * 0.8);
return col;
"""
OPAC = NOISE + r"""
float fr = pow(1.0 - saturate(abs(dot(normalize(Nw), normalize(V)))), 4.0);
float o = Opacity + fr * 0.55 + (1.0 - R.g) * 0.0;
o = lerp(o, 0.92, soot); o = lerp(o, 0.75, dirt * 0.6); o = max(o, crack * 0.9);
return saturate(o);
"""
ROUGH = NOISE + r"""
float r = R.g;
r = lerp(r, 0.02, saturate(Wet) * 0.8); r = lerp(r, 0.85, max(soot, dirt * 0.7));
return r;
"""

def build():
    m = unreal.load_asset(DIR + "/M_HeroGlass")
    if m: MEL.delete_all_material_expressions(m)
    else: m = AT.create_asset("M_HeroGlass", DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    try: m.set_editor_property("two_sided", False)
    except Exception: pass
    X = -1500
    c = tex(m, "C", "/Game/Kitchen/SM_K_Bucket/T_SM_K_Bucket_C", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, X, -600)
    n = tex(m, "N", "/Game/Kitchen/SM_K_Bucket/T_SM_K_Bucket_N", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, X, -300)
    r = tex(m, "R", "/Game/Kitchen/SM_K_Bucket/T_SM_K_Bucket_R", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, X, 0)
    wp = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, X, 300)
    vn = MEL.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, X, 380)
    cv = MEL.create_material_expression(m, unreal.MaterialExpressionCameraVectorWS, X, 460)
    op = sp(m, "Opacity", 0.16, X, 560)
    wet = cpd(m, "StateWet", 0, X, 700); char = cpd(m, "StateChar", 1, X, 800); dirt = cpd(m, "StateDirt", 2, X, 900); dmg = cpd(m, "StateDamage", 3, X, 1000)
    def wire(node, pairs, tag):
        for src, pin, out in pairs:
            if not MEL.connect_material_expressions(src, out, node, pin): unreal.log_warning("ART: GLASS connect FAILED %s %s" % (tag, pin))
    col = custom(m, COLOR, ["C", "W", "Char", "Dirt", "Dmg"], -700, -400, "Glass colour + state")
    wire(col, [(c, "C", "RGBA"), (wp, "W", ""), (char, "Char", ""), (dirt, "Dirt", ""), (dmg, "Dmg", "")], "col")
    opa = custom(m, OPAC, ["R", "W", "Nw", "V", "Opacity", "Char", "Dirt", "Dmg"], -700, 100, "Glass opacity", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    wire(opa, [(r, "R", "RGBA"), (wp, "W", ""), (vn, "Nw", ""), (cv, "V", ""), (op, "Opacity", ""), (char, "Char", ""), (dirt, "Dirt", ""), (dmg, "Dmg", "")], "opa")
    rou = custom(m, ROUGH, ["R", "W", "Wet", "Char", "Dirt", "Dmg"], -700, 500, "Glass roughness", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    wire(rou, [(r, "R", "RGBA"), (wp, "W", ""), (wet, "Wet", ""), (char, "Char", ""), (dirt, "Dirt", ""), (dmg, "Dmg", "")], "rough")
    MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(opa, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.connect_material_property(rou, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spc = sp(m, "Specular", 0.9, -300, 700)
    MEL.connect_material_property(spc, "", unreal.MaterialProperty.MP_SPECULAR)
    MEL.recompile_material(m)
    EAL.save_asset(DIR + "/M_HeroGlass")
    unreal.log_warning("ART: GLASS M_HeroGlass built")

def break_mi(name, tint, rough):
    mi = unreal.load_asset(DIR + "/" + name) or AT.create_asset(name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, unreal.load_asset(DIR + "/M_Hero"))
    MEL.set_material_instance_scalar_parameter_value(mi, "UseTex", 0.0)
    MEL.set_material_instance_scalar_parameter_value(mi, "BreakRough", rough)
    MEL.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*tint))
    EAL.save_asset(DIR + "/" + name)
    unreal.log_warning("ART: GLASS " + name)

build()
break_mi("MI_HeroBreak_Ceramic", (0.86, 0.82, 0.74, 1), 0.9)     # unglazed biscuit at the break
break_mi("MI_HeroBreak_Glass", (0.70, 0.82, 0.76, 1), 0.08)      # the green edge of broken glass
break_mi("MI_HeroBreak_Paper", (0.80, 0.74, 0.62, 1), 0.95)      # torn fibre
break_mi("MI_HeroBreak_Cardboard", (0.62, 0.48, 0.30, 1), 0.95)  # corrugated core
break_mi("MI_HeroBreak_Metal", (0.62, 0.63, 0.64, 1), 0.35)      # bright torn tin
break_mi("MI_HeroBreak_Foam", (0.86, 0.72, 0.28, 1), 0.95)       # torn sponge foam
