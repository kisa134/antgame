# Imports the Human's segments (SourceArt/Blender/human/*.fbx -> /Game/Human/<Name>/<Name>) with import_blender_art.py
# and builds /Game/Human/M_HumanSurface: the kitchen's M_Surface look (triplanar texture or plain paint, the same
# parameters), but projected in the mesh's LOCAL space, so the weave of the pyjamas does not swim while a leg moves.
# Run (see Saved/human/import_human.ps1):
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=%TEMP%\import_human.py -unattended -nosplash -nullrhi
import unreal, os
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
os.environ["ANT_ART_SETS"] = "human"
os.environ["ANT_ART_REPORT"] = "human_art_report.json"
if os.environ.get("ANT_HUMAN_MATERIAL_ONLY") != "1":   # set it to rebuild only the material
    exec(compile(open(os.path.join(PROJ, "Tools", "import_blender_art.py"), encoding="utf-8").read(), "import_blender_art.py", "exec"))

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
FULL = "/Game/Human/M_HumanSurface"
if EAL.does_asset_exist(FULL):
    EAL.delete_asset(FULL)
m = AT.create_asset("M_HumanSurface", "/Game/Human", unreal.Material, unreal.MaterialFactoryNew())
HLSL = r"""
float3 n = abs(normalize(N));   // the local-space normal is shortened by the component scale
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
return col;
"""
custom = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 0)
custom.set_editor_property("code", HLSL)
custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
ins = []
for n in ["Tex", "W", "N", "S", "Tint", "Mode"]:
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", n)
    ins.append(ci)
custom.set_editor_property("inputs", ins)
tex = MEL.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, -900, -200)
tex.set_editor_property("parameter_name", "Tex")
d = unreal.load_asset("/Game/Textures/T_Fabric_Grey")
if d:
    tex.set_editor_property("texture", d)
MEL.connect_material_expressions(tex, "", custom, "Tex")
def sp(name, val, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", val)
    return p
def vp(name, val, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", val)
    return p
wp = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1500, 0)
vn = MEL.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, -1500, 100)
space = "world"
try:
    tp = MEL.create_material_expression(m, unreal.MaterialExpressionTransformPosition, -1300, 0)
    tp.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    tp.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    tv = MEL.create_material_expression(m, unreal.MaterialExpressionTransform, -1300, 100)
    tv.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD)
    tv.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    mul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -1100, 0)
    ok = MEL.connect_material_expressions(wp, "", tp, "") and MEL.connect_material_expressions(vn, "", tv, "")
    ok = ok and MEL.connect_material_expressions(tp, "", mul, "A") and MEL.connect_material_expressions(sp("LocalScale", 19.0, -100), "", mul, "B")
    ok = ok and MEL.connect_material_expressions(mul, "", custom, "W") and MEL.connect_material_expressions(tv, "", custom, "N")
    if not ok:
        raise RuntimeError("a connection failed")
    space = "local"
except Exception as e:
    unreal.log_warning("ART: M_HumanSurface could not be made local-space (%s): WORLD-SPACE FALLBACK, textures will swim" % e)
    MEL.connect_material_expressions(wp, "", custom, "W")
    MEL.connect_material_expressions(vn, "", custom, "N")
MEL.connect_material_expressions(sp("TileSize", 300.0, 200), "", custom, "S")
MEL.connect_material_expressions(vp("Tint", unreal.LinearColor(1, 1, 1, 1), 300), "", custom, "Tint")
MEL.connect_material_expressions(sp("Mode", 0.0, 400), "", custom, "Mode")
MEL.connect_material_property(custom, "", unreal.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(sp("Roughness", 0.6, 500), "", unreal.MaterialProperty.MP_ROUGHNESS)
MEL.connect_material_property(sp("Metallic", 0.0, 600), "", unreal.MaterialProperty.MP_METALLIC)
MEL.connect_material_property(sp("Specular", 0.5, 700), "", unreal.MaterialProperty.MP_SPECULAR)
MEL.recompile_material(m)
EAL.save_asset(FULL)
unreal.log_warning("ART: M_HumanSurface saved, %s space, %d expressions" % (space, MEL.get_num_material_expressions(m)))
unreal.log_warning("ART: HUMAN_IMPORT_DONE")
