# Map v4: imports the packed texture sets (SourceArt/Map4/packed) and builds the materials of the new look.
#   /Game/Map4/Textures/T_M4_<Set>_{C,N,R}   C sRGB colour, N DirectX normal, R = AO / roughness / metal
#   /Game/Map4/Materials/M_M4      triplanar PBR in world space: colour, normal (+ ant-scale detail normal), AO/rough/metal,
#                                  macro variation against tiling, dust on tops, grime at the foot of walls, glow
#   /Game/Map4/Materials/M_M4_UV   the same inputs through the mesh's own UVs (generated props, prints, labels)
#   /Game/Map4/Decals/T_M4D_*      prints: kids' drawings, photos, labels (sRGB)
# Run: UnrealEditor-Cmd.exe AntKitchen.uproject -run=pythonscript -script=Tools/map4/make_m4_materials.py
import unreal, os, glob
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
PACKED = os.path.join(PROJ, "SourceArt", "Map4", "packed")
DECALS = os.path.join(PROJ, "SourceArt", "Map4", "higgsfield", "decal")
TEX = "/Game/Map4/Textures"
MAT = "/Game/Map4/Materials"
DEC = "/Game/Map4/Decals"
ONLY_MATS = os.environ.get("M4_ONLY_MATS", "") == "1"

def import_tex(f, dest, name, kind):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", False)
    AT.import_asset_tasks([t])
    a = unreal.load_asset(dest + "/" + name)
    if not a:
        unreal.log_warning("M4 import failed " + f)
        return
    if kind == "N":
        a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        a.set_editor_property("srgb", False)
    elif kind == "R":
        a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        a.set_editor_property("srgb", False)
    else:
        a.set_editor_property("srgb", True)
    a.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD if kind != "N" else unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    EAL.save_asset(dest + "/" + name)

if not ONLY_MATS:
    only_tex = [t for t in os.environ.get("M4_TEX", "").split(",") if t]   # e.g. M4_TEX=GinghamBlue,GinghamGreen
    for f in sorted(glob.glob(os.path.join(PACKED, "T_M4_*"))):
        if only_tex and not any(os.path.basename(f).startswith("T_M4_%s_" % t) for t in only_tex):
            continue
        name = os.path.splitext(os.path.basename(f))[0]
        import_tex(f, TEX, name, name[-1])
        unreal.log("M4TEX " + name)
    for f in sorted(glob.glob(os.path.join(DECALS, "*.png"))):
        name = "T_M4D_" + os.path.splitext(os.path.basename(f))[0]
        import_tex(f, DEC, name, "C")
        unreal.log("M4DECAL " + name)

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

def cpd(m, name, index, x, y):
    """a scalar read from the component's custom primitive data (no material instance per object needed): 0 by default"""
    p = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", 0.0)
    p.set_editor_property("use_custom_primitive_data", True); p.set_editor_property("primitive_data_index", index)
    return p

def vp(m, name, val, x, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", unreal.LinearColor(*val)); return p

def tp(m, name, default, x, y, sampler=None):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, x, y)
    p.set_editor_property("parameter_name", name)
    d = unreal.load_asset(default)
    if d: p.set_editor_property("texture", d)
    if sampler is not None: p.set_editor_property("sampler_type", sampler)
    return p

def mask(m, src, r, g, b, x, y):
    cm = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, x, y)
    cm.set_editor_property("r", r); cm.set_editor_property("g", g); cm.set_editor_property("b", b); cm.set_editor_property("a", False)
    MEL.connect_material_expressions(src, "", cm, "")
    return cm

W_HEAD = r"""
float3 bw = pow(abs(N), 6.0);
bw /= max(bw.x + bw.y + bw.z, 1e-4);
float2 uvX = W.zy / S, uvY = W.xz / S, uvZ = W.xy / S;
"""

COLOR = W_HEAD + r"""
float3 a = Texture2DSample(TC, TCSampler, uvX).rgb;
float3 b = Texture2DSample(TC, TCSampler, uvY).rgb;
float3 c = Texture2DSample(TC, TCSampler, uvZ).rgb;
float3 col = (a * bw.x + b * bw.y + c * bw.z) * Tint;
// macro variation (breaks the tiling): two octaves of value noise in world space
float2 p = (W.xy + W.zz * 0.37) / 1300.0;
float2 i = floor(p); float2 f = frac(p); f = f * f * (3.0 - 2.0 * f);
float h00 = frac(sin(dot(i, float2(127.1, 311.7))) * 43758.5453);
float h10 = frac(sin(dot(i + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
float h01 = frac(sin(dot(i + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
float h11 = frac(sin(dot(i + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
float v = lerp(lerp(h00, h10, f.x), lerp(h01, h11, f.x), f.y);
float2 p2 = (W.yz + W.xx * 0.21) / 310.0;
float2 i2 = floor(p2); float2 f2 = frac(p2); f2 = f2 * f2 * (3.0 - 2.0 * f2);
float g00 = frac(sin(dot(i2, float2(269.5, 183.3))) * 43758.5453);
float g10 = frac(sin(dot(i2 + float2(1, 0), float2(269.5, 183.3))) * 43758.5453);
float g01 = frac(sin(dot(i2 + float2(0, 1), float2(269.5, 183.3))) * 43758.5453);
float g11 = frac(sin(dot(i2 + float2(1, 1), float2(269.5, 183.3))) * 43758.5453);
float v2 = lerp(lerp(g00, g10, f2.x), lerp(g01, g11, f2.x), f2.y);
float vv = v * 0.65 + v2 * 0.35;
col *= 1.0 + (vv - 0.5) * Var;
// dust settles on what faces up; grime collects at the foot of walls
float up = saturate((N.z - 0.75) * 4.0);
col = lerp(col, float3(0.60, 0.58, 0.54), saturate(up * Dust * (0.35 + 0.9 * vv)));
float low = saturate(1.0 - (W.z - Base) / 110.0) * (1.0 - up);
col *= 1.0 - low * Grime * 0.4;
// states of the thing itself (per component: custom primitive data 0..3, see AntState.h): dirty, wet, charred, damaged
float2 p3 = (W.xy + W.zz * 0.31 + W.yx * 0.11) / 41.0;
float2 i3 = floor(p3); float2 f3 = frac(p3); f3 = f3 * f3 * (3.0 - 2.0 * f3);
float k00 = frac(sin(dot(i3, float2(12.9898, 78.233))) * 43758.5453);
float k10 = frac(sin(dot(i3 + float2(1, 0), float2(12.9898, 78.233))) * 43758.5453);
float k01 = frac(sin(dot(i3 + float2(0, 1), float2(12.9898, 78.233))) * 43758.5453);
float k11 = frac(sin(dot(i3 + float2(1, 1), float2(12.9898, 78.233))) * 43758.5453);
float v3 = lerp(lerp(k00, k10, f3.x), lerp(k01, k11, f3.x), f3.y);
float fine = v3 * 0.6 + v2 * 0.4;
col = lerp(col, float3(0.24, 0.20, 0.15), saturate(Dirt * (0.25 + fine)) * 0.85);
col *= lerp(1.0, 0.52, saturate(Wet));
float cm = smoothstep(1.05 - Char * 1.25, 1.30 - Char * 1.25, fine + 0.15) * saturate(Char * 4.0);
col = lerp(col, float3(0.020, 0.017, 0.014), cm);
float cs = abs(frac((W.x * 0.83 + W.y * 0.61 + W.z * 0.47) / 19.0 + v3 * 2.5) - 0.5);
float crack = (1.0 - smoothstep(0.0, 0.03, cs)) * step(0.45, v2 + Dmg * 0.4);
col = lerp(col, col * 0.25, crack * saturate(Dmg * 1.5));
col = lerp(col, col * 0.8 + 0.06, saturate(Dmg) * fine * 0.5);
return max(col, 0.0);
"""

NORMAL = W_HEAD + r"""
float2 dX = W.zy / DS, dY = W.xz / DS, dZ = W.xy / DS;
float3 tX, tY, tZ;
tX.xy = (Texture2DSample(TN, TNSampler, uvX).xy * 2.0 - 1.0) * NStr + (Texture2DSample(TD, TDSampler, dX).xy * 2.0 - 1.0) * DStr;
tY.xy = (Texture2DSample(TN, TNSampler, uvY).xy * 2.0 - 1.0) * NStr + (Texture2DSample(TD, TDSampler, dY).xy * 2.0 - 1.0) * DStr;
tZ.xy = (Texture2DSample(TN, TNSampler, uvZ).xy * 2.0 - 1.0) * NStr + (Texture2DSample(TD, TDSampler, dZ).xy * 2.0 - 1.0) * DStr;
tX.z = sqrt(saturate(1.0 - dot(tX.xy, tX.xy)));
tY.z = sqrt(saturate(1.0 - dot(tY.xy, tY.xy)));
tZ.z = sqrt(saturate(1.0 - dot(tZ.xy, tZ.xy)));
float3 nX = float3(tX.xy + N.zy, abs(tX.z) * N.x);
float3 nY = float3(tY.xy + N.xz, abs(tY.z) * N.y);
float3 nZ = float3(tZ.xy + N.xy, abs(tZ.z) * N.z);
return normalize(nX.zyx * bw.x + nY.xzy * bw.y + nZ.xyz * bw.z);
"""

ORM = W_HEAD + r"""
float3 a = Texture2DSample(TR, TRSampler, uvX).rgb;
float3 b = Texture2DSample(TR, TRSampler, uvY).rgb;
float3 c = Texture2DSample(TR, TRSampler, uvZ).rgb;
float3 r = a * bw.x + b * bw.y + c * bw.z;
float up = saturate((N.z - 0.75) * 4.0);
float ao = lerp(1.0, r.r, AOStr);
float rough = saturate(r.g * RoughMul + RoughAdd);
rough = lerp(rough, 0.92, saturate(up * Dust * 0.8));
float metal = saturate(r.b * MetalTex + Metal);
rough = lerp(rough, 0.90, saturate(Dirt) * 0.6);
rough = lerp(rough, 0.06, saturate(Wet) * 0.85);
rough = lerp(rough, 0.95, saturate(Char));
metal *= 1.0 - saturate(Char);
return float3(ao, rough, metal);
"""

def build_m4(name="M_M4", two_sided=False):
    m = fresh(MAT, name)
    m.set_editor_property("tangent_space_normal", False)
    m.set_editor_property("two_sided", two_sided)
    m.set_editor_property("used_with_instanced_static_meshes", True)   # grass tufts, trail dots: else the grey default draws there
    m.set_editor_property("used_with_nanite", True)
    X = -1500
    tc = tp(m, "C", TEX + "/T_M4_Oak_C", X, -600)
    tn = tp(m, "N", TEX + "/T_M4_Oak_N", X, -350, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    tr = tp(m, "R", TEX + "/T_M4_Oak_R", X, -100, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    td = tp(m, "D", TEX + "/T_M4_Detail_N", X, 150, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    wp = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, X, 400)
    vn = MEL.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, X, 500)
    S = sp(m, "TileSize", 600.0, X, 600)
    tint = vp(m, "Tint", (1, 1, 1, 1), X, 700)
    var = sp(m, "Variation", 0.18, X, 800)
    dust = sp(m, "Dust", 0.0, X, 900)
    grime = sp(m, "Grime", 0.0, X, 1000)
    base = sp(m, "GrimeBase", 0.0, X, 1100)
    nstr = sp(m, "NormalStrength", 1.0, X, 1200)
    dstr = sp(m, "DetailStrength", 0.15, X, 1300)
    ds = sp(m, "DetailSize", 45.0, X, 1400)
    aos = sp(m, "AOStrength", 1.0, X, 1500)
    rmul = sp(m, "RoughMul", 1.0, X, 1600)
    radd = sp(m, "RoughAdd", 0.0, X, 1700)
    mtex = sp(m, "MetalTex", 1.0, X, 1800)
    metal = sp(m, "Metal", 0.0, X, 1900)
    glow = vp(m, "Glow", (0, 0, 0, 1), X, 2000)
    # the states of the thing (AntState.h): the water, the fire, the mess and the wreck set them on the component
    wet = cpd(m, "StateWet", 0, X, 2200); char = cpd(m, "StateChar", 1, X, 2300); dirt = cpd(m, "StateDirt", 2, X, 2400); dmg = cpd(m, "StateDamage", 3, X, 2500)

    col = custom(m, COLOR, ["TC", "W", "N", "S", "Tint", "Var", "Dust", "Grime", "Base", "Wet", "Char", "Dirt", "Dmg"], -700, -300, "M4 colour")
    for src, pin in [(tc, "TC"), (wp, "W"), (vn, "N"), (S, "S"), (var, "Var"), (dust, "Dust"), (grime, "Grime"), (base, "Base"), (wet, "Wet"), (char, "Char"), (dirt, "Dirt"), (dmg, "Dmg")]:
        MEL.connect_material_expressions(src, "", col, pin)
    colp = vp(m, "Color", (1, 1, 1, 1), X, 2100)
    tmul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -1100, 700)
    MEL.connect_material_expressions(tint, "RGB", tmul, "A")
    MEL.connect_material_expressions(colp, "RGB", tmul, "B")
    MEL.connect_material_expressions(tmul, "", col, "Tint")
    nrm = custom(m, NORMAL, ["TN", "TD", "W", "N", "S", "DS", "NStr", "DStr"], -700, 200, "M4 normal (world)")
    for src, pin in [(tn, "TN"), (td, "TD"), (wp, "W"), (vn, "N"), (S, "S"), (ds, "DS"), (nstr, "NStr"), (dstr, "DStr")]:
        MEL.connect_material_expressions(src, "", nrm, pin)
    orm = custom(m, ORM, ["TR", "W", "N", "S", "Dust", "AOStr", "RoughMul", "RoughAdd", "MetalTex", "Metal", "Wet", "Char", "Dirt"], -700, 700, "M4 AO rough metal")
    for src, pin in [(tr, "TR"), (wp, "W"), (vn, "N"), (S, "S"), (dust, "Dust"), (aos, "AOStr"), (rmul, "RoughMul"), (radd, "RoughAdd"), (mtex, "MetalTex"), (metal, "Metal"), (wet, "Wet"), (char, "Char"), (dirt, "Dirt")]:
        MEL.connect_material_expressions(src, "", orm, pin)
    MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(mask(m, orm, True, False, False, -300, 700), "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    MEL.connect_material_property(mask(m, orm, False, True, False, -300, 800), "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(mask(m, orm, False, False, True, -300, 900), "", unreal.MaterialProperty.MP_METALLIC)
    mul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 1000)
    MEL.connect_material_expressions(col, "", mul, "A")
    MEL.connect_material_expressions(glow, "RGB", mul, "B")
    MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    EAL.save_asset(MAT + "/" + name)
    unreal.log("M4MAT " + name)

def build_uv(name, two_sided=False):
    m = fresh(MAT, name)
    m.set_editor_property("two_sided", two_sided)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    m.set_editor_property("used_with_nanite", True)
    X = -1200
    c = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, X, -300)
    c.set_editor_property("parameter_name", "C")
    d = unreal.load_asset(DEC + "/T_M4D_drawing_family")
    if d: c.set_editor_property("texture", d)
    n = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, X, 0)
    n.set_editor_property("parameter_name", "N")
    n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    dn = unreal.load_asset("/Engine/EngineMaterials/FlatNormal")
    if dn: n.set_editor_property("texture", dn)
    r = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, X, 300)
    r.set_editor_property("parameter_name", "R")
    r.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    dr = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    if dr: r.set_editor_property("texture", dr)
    tint = vp(m, "Tint", (1, 1, 1, 1), X, 600)
    rmul = sp(m, "RoughMul", 1.0, X, 700)
    radd = sp(m, "RoughAdd", 0.0, X, 800)
    metal = sp(m, "Metal", 0.0, X, 900)
    glow = vp(m, "Glow", (0, 0, 0, 1), X, 1000)
    colp = vp(m, "Color", (1, 1, 1, 1), X, 1100)
    tm = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -800, 600)
    MEL.connect_material_expressions(tint, "RGB", tm, "A"); MEL.connect_material_expressions(colp, "RGB", tm, "B")
    mc = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -600, -300)
    MEL.connect_material_expressions(c, "RGB", mc, "A"); MEL.connect_material_expressions(tm, "", mc, "B")
    MEL.connect_material_property(mc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(r, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    rm = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -600, 300)
    MEL.connect_material_expressions(r, "G", rm, "A"); MEL.connect_material_expressions(rmul, "", rm, "B")
    ra = MEL.create_material_expression(m, unreal.MaterialExpressionAdd, -450, 300)
    MEL.connect_material_expressions(rm, "", ra, "A"); MEL.connect_material_expressions(radd, "", ra, "B")
    MEL.connect_material_property(ra, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mm = MEL.create_material_expression(m, unreal.MaterialExpressionAdd, -450, 450)
    MEL.connect_material_expressions(r, "B", mm, "A"); MEL.connect_material_expressions(metal, "", mm, "B")
    MEL.connect_material_property(mm, "", unreal.MaterialProperty.MP_METALLIC)
    ge = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -450, 600)
    MEL.connect_material_expressions(mc, "", ge, "A"); MEL.connect_material_expressions(glow, "RGB", ge, "B")
    MEL.connect_material_property(ge, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    EAL.save_asset(MAT + "/" + name)
    unreal.log("M4MAT " + name)

build_m4()
build_m4("M_M4_2S", True)   # cloth sheets, leaves, blades, wings: one layer of polygons seen from both sides

def paint_instance():
    name = "MI_M4_Paint"
    if EAL.does_asset_exist(MAT + "/" + name):
        EAL.delete_asset(MAT + "/" + name)
    mi = AT.create_asset(name, MAT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, unreal.load_asset(MAT + "/M_M4"))
    for k in ("C", "N", "R"):
        MEL.set_material_instance_texture_parameter_value(mi, k, unreal.load_asset(TEX + "/T_M4_Enamel_" + k))
    MEL.set_material_instance_scalar_parameter_value(mi, "RoughAdd", 0.18)
    MEL.set_material_instance_scalar_parameter_value(mi, "NormalStrength", 0.7)
    EAL.save_asset(MAT + "/" + name)
    unreal.log("M4MAT " + name)
paint_instance()
if os.environ.get("M4_ONLY_M4", "") != "1":   # (rebuilding the UV materials orphans the props' instances: re-import the props after)
    build_uv("M_M4_UV")
    build_uv("M_M4_UV2", True)
unreal.log("M4_DONE")
