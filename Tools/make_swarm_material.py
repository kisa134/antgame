# Builds /Game/Swarm/M_SwarmAnt: a material that moves instanced ants along trail segments on the GPU.
# Run:  UnrealEditor-Cmd.exe AntKitchen.uproject -run=pythonscript -script="<path>/make_swarm_material.py"
import unreal

PATH = "/Game/Swarm"

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

def build(NAME, textured):
  FULL = PATH + "/" + NAME
  if EAL.does_asset_exist(FULL):
    EAL.delete_asset(FULL)
  mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
  mat.set_editor_property("used_with_instanced_static_meshes", True)
  build_graph(mat, textured)
  MEL.recompile_material(mat)
  EAL.save_asset(FULL)
  unreal.log("SWARM_MATERIAL_OK " + FULL)

def build_graph(mat, textured):

    HLSL = r"""
    float3 A = float3(A0, A1, A2);
    float3 B = float3(B0, B1, B2);
    float3 N = float3(N0, N1, N2);
    N = (dot(N, N) > 0.0001) ? normalize(N) : float3(0, 0, 1);
    float3 D = B - A;
    float L = max(length(D), 1.0);
    float3 F = D / L;
    float sp = Speed;
    float t = frac(T * abs(sp) / L + Phase);
    if (sp < 0) { t = 1.0 - t; F = -F; }
    float3 R = cross(N, F);
    R = (dot(R, R) > 0.0001) ? normalize(R) : float3(0, 1, 0);
    float wig = sin(T * 7.0 + Phase * 61.0) * 2.5;
    float3 P = lerp(A, B, t) + R * (Lat + wig);
    float3 lp = LocalPos.xyz * PartScale.xyz * 0.01 + PartOffset.xyz;
    if (LegMode > 0.5)
    {
        float s = (LegMode > 1.5) ? -1.0 : 1.0;
        float a = 0.6 * s + sin(T * abs(sp) * 0.15 + Phase * 80.0) * 0.4 * s;
        float ca = cos(a);
        float sa = sin(a);
        lp = float3(lp.x * ca - lp.y * sa, lp.x * sa + lp.y * ca, lp.z);
    }
    float3 W = P + F * lp.x + R * lp.y + N * lp.z;
    if (Flags > 0.5) { W = P - N * 5000.0; }
    return W - WorldPos.xyz;
    """

    names = ["A0", "A1", "A2", "B0", "B1", "B2", "N0", "N1", "N2", "Speed", "Phase", "Lat", "Flags",
             "T", "LocalPos", "WorldPos", "PartOffset", "PartScale", "LegMode"]

    custom = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom, -500, 400)
    custom.set_editor_property("code", HLSL)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("description", "SwarmAnt")
    ins = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    custom.set_editor_property("inputs", ins)

    for i in range(13):
        e = MEL.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData, -1300, i * 70)
        e.set_editor_property("data_index", i)
        MEL.connect_material_expressions(e, "", custom, names[i])

    time = MEL.create_material_expression(mat, unreal.MaterialExpressionTime, -1500, 1000)
    toff = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1500, 1080)
    toff.set_editor_property("parameter_name", "TimeOffset")
    add = MEL.create_material_expression(mat, unreal.MaterialExpressionAdd, -1300, 1040)
    MEL.connect_material_expressions(time, "", add, "A")
    MEL.connect_material_expressions(toff, "", add, "B")
    MEL.connect_material_expressions(add, "", custom, "T")

    if hasattr(unreal, "MaterialExpressionLocalPosition"):
        local = MEL.create_material_expression(mat, unreal.MaterialExpressionLocalPosition, -1300, 1150)
    else:
        local = MEL.create_material_expression(mat, unreal.MaterialExpressionPreSkinnedPosition, -1300, 1150)
    MEL.connect_material_expressions(local, "", custom, "LocalPos")

    world = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1300, 1230)
    MEL.connect_material_expressions(world, "", custom, "WorldPos")

    def vparam(name, val, y):
        p = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -1300, y)
        p.set_editor_property("parameter_name", name)
        p.set_editor_property("default_value", val)
        return p

    po = vparam("PartOffset", unreal.LinearColor(0, 0, 0, 0), 1310)
    ps = vparam("PartScale", unreal.LinearColor(10, 10, 10, 0), 1390)
    MEL.connect_material_expressions(po, "", custom, "PartOffset")
    MEL.connect_material_expressions(ps, "", custom, "PartScale")
    leg = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1300, 1470)
    leg.set_editor_property("parameter_name", "LegMode")
    MEL.connect_material_expressions(leg, "", custom, "LegMode")

    MEL.connect_material_property(custom, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    color = vparam("Color", unreal.LinearColor(0.06, 0.025, 0.015, 1), -200)
    if textured:
        bc = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, -400)
        bc.set_editor_property("parameter_name", "BaseTex")
        t = unreal.load_asset("/Game/Meshy/SM_Ant_Body/T_Ant_Body_BaseColor")
        if t: bc.set_editor_property("texture", t)
        mul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, -300)
        MEL.connect_material_expressions(bc, "RGB", mul, "A")
        tint = vparam("Tint", unreal.LinearColor(1, 1, 1, 1), -500)
        MEL.connect_material_expressions(tint, "", mul, "B")
        MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
        nm = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, 200)
        nm.set_editor_property("parameter_name", "NormalTex")
        nm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        tn = unreal.load_asset("/Game/Meshy/SM_Ant_Body/T_Ant_Body_Normal")
        if tn: nm.set_editor_property("texture", tn)
        MEL.connect_material_property(nm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    else:
        MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = vparam("Glow", unreal.LinearColor(0, 0, 0, 0), -100)
    MEL.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 0)
    rough.set_editor_property("r", 0.3)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)



build("M_SwarmAnt", False)
build("M_SwarmAntArt", True)

# Low-poly copy of the ant body for tens of thousands of instances (no Nanite).
SRC = "/Game/Meshy/SM_Ant_Body/SM_Ant_Body"
DST = "/Game/Swarm/SM_SwarmAnt"
if EAL.does_asset_exist(SRC):
    if EAL.does_asset_exist(DST):
        EAL.delete_asset(DST)
    m = EAL.duplicate_asset(SRC, DST)
    ns = m.get_editor_property("nanite_settings")
    ns.set_editor_property("enabled", False)
    m.set_editor_property("nanite_settings", ns)
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    opt = unreal.MeshReductionSettings()
    opt.set_editor_property("percent_triangles", 0.04)
    try:
        sub.set_lod_reduction_settings(m, 0, opt)
    except Exception as e:
        unreal.log_warning("SWARM reduction failed: %s" % e)
    EAL.save_loaded_asset(m)
    unreal.log("SWARM_MESH_OK tris=%d" % m.get_num_triangles(0))
