# M_Surface: one material for the whole procedurally built kitchen.
# Textures are projected in world space (triplanar), so stretched boxes still look right.
# Mode: 0 texture, 1 floor tiles with grout, 2 subway tiles, 3 granite speckle, 4 plain paint (subtle noise).
import unreal, os, glob
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())

# 1) textures
for f in glob.glob(os.path.join(PROJ, "SourceArt", "Textures", "*.png")):
    name = os.path.splitext(os.path.basename(f))[0]
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f)
    t.set_editor_property("destination_path", "/Game/Textures")
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    AT.import_asset_tasks([t])
    unreal.log("SURF tex " + name)

# 2) material
FULL = "/Game/Textures/M_Surface"
if EAL.does_asset_exist(FULL):
    EAL.delete_asset(FULL)
m = AT.create_asset("M_Surface", "/Game/Textures", unreal.Material, unreal.MaterialFactoryNew())

HLSL = r"""
float3 n = abs(N);
n = pow(n, 4.0);
n /= max(n.x + n.y + n.z, 1e-4);
float3 col = Tint.rgb;
if (Mode < 0.5)
{
    float3 a = Texture2DSample(Tex, TexSampler, W.yz / S).rgb;
    float3 b = Texture2DSample(Tex, TexSampler, W.xz / S).rgb;
    float3 c = Texture2DSample(Tex, TexSampler, W.xy / S).rgb;
    col = (a * n.x + b * n.y + c * n.z) * Tint.rgb;
}
else
{
    // planar coords on the dominant axis
    float2 uv = (n.z > n.x && n.z > n.y) ? W.xy : ((n.x > n.y) ? W.yz : W.xz);
    float h = frac(sin(dot(floor(uv / 7.0), float2(12.9898, 78.233))) * 43758.5453);
    if (Mode < 1.5)
    {
        float2 g = abs(frac(uv / S) - 0.5);
        float edge = max(g.x, g.y);
        float grout = smoothstep(0.485, 0.495, edge);
        float2 cell = floor(uv / S);
        float v = frac(sin(dot(cell, float2(7.1, 3.7))) * 1.0e4) * 0.06;
        col = lerp(Tint.rgb * (0.97 + v), float3(0.55, 0.53, 0.5), grout);
    }
    else if (Mode < 2.5)
    {
        float2 q = uv / float2(S, S / 3.0);
        q.x += floor(q.y) * 0.5;
        float2 g = abs(frac(q) - 0.5);
        float grout = max(smoothstep(0.47, 0.49, g.x), smoothstep(0.42, 0.47, g.y));
        col = lerp(Tint.rgb, float3(0.95, 0.95, 0.93), grout);
    }
    else if (Mode < 3.5)
    {
        float s = frac(sin(dot(floor(uv / 3.0), float2(127.1, 311.7))) * 43758.5453);
        col = Tint.rgb * (0.85 + 0.3 * h) + (s > 0.93 ? 0.35 : 0.0) + (s < 0.05 ? -0.05 : 0.0);
    }
    else
    {
        col = Tint.rgb * (0.97 + 0.04 * h);
    }
}
return col;
"""
custom = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 0)
custom.set_editor_property("code", HLSL)
custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
ins = []
for n in ["Tex", "W", "N", "S", "Tint", "Mode"]:
    ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
custom.set_editor_property("inputs", ins)

tex = MEL.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, -900, -200)
tex.set_editor_property("parameter_name", "Tex")
d = unreal.load_asset("/Game/Textures/T_Wood_Oak")
if d: tex.set_editor_property("texture", d)
MEL.connect_material_expressions(tex, "", custom, "Tex")
wp = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -900, 0)
MEL.connect_material_expressions(wp, "", custom, "W")
vn = MEL.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, -900, 100)
MEL.connect_material_expressions(vn, "", custom, "N")
def sp(name, val, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, y)
    p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", val); return p
def vp(name, val, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, y)
    p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", val); return p
MEL.connect_material_expressions(sp("TileSize", 400.0, 200), "", custom, "S")
MEL.connect_material_expressions(vp("Tint", unreal.LinearColor(1, 1, 1, 1), 300), "", custom, "Tint")
MEL.connect_material_expressions(sp("Mode", 0.0, 400), "", custom, "Mode")
MEL.connect_material_property(custom, "", unreal.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(sp("Roughness", 0.6, 500), "", unreal.MaterialProperty.MP_ROUGHNESS)
MEL.connect_material_property(sp("Metallic", 0.0, 600), "", unreal.MaterialProperty.MP_METALLIC)
MEL.connect_material_property(sp("Specular", 0.5, 700), "", unreal.MaterialProperty.MP_SPECULAR)
MEL.recompile_material(m)
EAL.save_asset(FULL)
unreal.log("SURF_DONE")
