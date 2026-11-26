# KAN-36: the food materials of the V2 models (Tools/food/food_v2.py), made in the editor without hand work.
#   /Game/FoodV2/M_FoodV2        the outside of a food: baked colour / normal / AO-rough, the baked colour of each visible state
#                                (toasted, burnt, spoiled, frozen: roughness in alpha), soaked and dirty on top, jelly = subsurface;
#                                the deformation layer (world position offset): sag, tip droop, squash, stretch, wobble, melt, bite
#   /Game/FoodV2/M_FoodV2_Inner  the faces of a break: the inner strip (cake layers, flesh...) read by height in the piece's own space,
#                                the same states as tints, the same deformation
# Everything per piece comes from custom primitive data (AntState.h 0..2: wet, char, dirt; AntFood.h from 12 on), so there is no
# material instance per piece and the default (all zero) is the fresh food at rest.
# Run: UnrealEditor-Cmd.exe AntKitchen.uproject -run=pythonscript -script=Tools/food/make_food_materials.py
import unreal
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/FoodV2"

# custom primitive data (AntState.h / AntFood.h)
CPD = dict(Wet=0, Char=1, Dirt=2, Toast=12, Frozen=13, Spoil=14, Bite=15, Sag=16, Tip=17, TipSign=18, Squash=19, Stretch=20, StretchAxis=21, Wobble=22, Melt=23)

def fresh(name):
    full = DIR + "/" + name
    if EAL.does_asset_exist(full):
        # rebuilt in place: the food instances keep their parent
        m = unreal.load_asset(full)
        MEL.delete_all_material_expressions(m)
        return m
    return AT.create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())

def custom(m, code, inputs, x, y, desc, out=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    c = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("code", code); c.set_editor_property("output_type", out); c.set_editor_property("description", desc)
    ins = []
    for n in inputs:
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
    c.set_editor_property("inputs", ins)
    return c

def sp(m, name, val, x, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", val); return p

def cpd(m, name, x, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property("parameter_name", "Cpd" + name); p.set_editor_property("default_value", 0.0)
    p.set_editor_property("use_custom_primitive_data", True); p.set_editor_property("primitive_data_index", CPD[name])
    return p

def tex(m, name, default, x, y, kind="color", uv=None):
    t = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    t.set_editor_property("parameter_name", name)
    d = unreal.load_asset(default)
    if d: t.set_editor_property("texture", d)
    st = {"color": unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "normal": unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
          "masks": unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, "linear": unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR}[kind]
    t.set_editor_property("sampler_type", st)
    t.set_editor_property("sampler_source", unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)   # one shared sampler
    if uv is not None: MEL.connect_material_expressions(uv, "", t, "UVs")
    return t

def mask(m, src, pin, r, g, b, a, x, y):
    cm = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, x, y)
    for k, v in (("r", r), ("g", g), ("b", b), ("a", a)): cm.set_editor_property(k, v)
    MEL.connect_material_expressions(src, pin, cm, "")
    return cm

# the deformation (local space, cm of the mesh): no soft bodies, a few numbers per component
WPO = r"""
float3 h = max((BMax - BMin) * 0.5, 0.01);
float3 c = (BMax + BMin) * 0.5;
float3 q = LP - c;
float3 p = q / h;                                   // -1..1 in the box
float3 o = 0;
// stretch (elastic between the carriers, squeezing through a gap): longer along the axis, thinner across, a neck in the middle
float3 ax = StretchAxis < 0.5 ? float3(1, 0, 0) : float3(0, 1, 0);
float al = dot(p, ax);
float3 across = q - ax * dot(q, ax);
o += ax * dot(q, ax) * Stretch;
o += across * (1.0 / sqrt(1.0 + Stretch) - 1.0);
o -= across * 0.35 * Stretch * saturate(1.0 - al * al);
// sag: held at both ends the middle hangs; held at one end the far end droops (and comes in a little: it bends, not stretches)
o.z -= h.z * 1.6 * Sag * saturate(1.0 - p.x * p.x);
float t = saturate((p.x * TipSign + 1.0) * 0.5);
o.z -= h.x * Tip * t * t * 1.1;
o.x -= TipSign * h.x * Tip * t * t * 0.28;
// squash and melt: the top comes down, the foot spreads
float zb = saturate((LP.z - BMin.z) / max(BMax.z - BMin.z, 0.01));
float sq = saturate(Squash + Melt * 0.55);
o.z -= (LP.z - BMin.z) * sq * 0.6;
o.xy += q.xy * sq * 0.45 * (1.0 - zb);
// wobble: jelly shakes after a jolt
float w = Wobble * sin(Time * 21.0 + p.x * 2.1 + p.y * 1.3);
o.z += h.z * 0.14 * w * zb;
o.x += h.x * 0.06 * w * zb;
// a bite out of the top corner
float3 bc = c + h * float3(0.95, 0.30, 0.75);
float d = length((LP - bc) / (h * float3(0.55, 0.55, 0.6)));
float k = saturate(1.0 - d) * Bite;
o += normalize(c - LP + 1e-3) * k * min(h.x, h.z) * 0.7;
return o;
"""

COLOR = r"""
float3 c = C.rgb;
float r = R.g;
c = lerp(c, T.rgb, saturate(Toast));  r = lerp(r, T.a, saturate(Toast));
c = lerp(c, B.rgb, saturate(Char));   r = lerp(r, B.a, saturate(Char));
c = lerp(c, S.rgb, saturate(Spoil));  r = lerp(r, S.a, saturate(Spoil));
c = lerp(c, F.rgb, saturate(Frozen)); r = lerp(r, F.a, saturate(Frozen));
// soaked / melting wet: darker and glossy
c *= lerp(1.0, 0.6, saturate(Wet)); r = lerp(r, 0.15, saturate(Wet));
// dirty: grit specks and a grey film (world space value noise, 2.5 uu cells)
float2 g = floor(WP.xy / 2.5 + WP.zz / 3.7);
float n = frac(sin(dot(g, float2(12.9898, 78.233))) * 43758.5453);
float dd = saturate(Dirt);
float speck = n > 1.0 - 0.36 * dd ? 1.0 : 0.0;
c = lerp(c, c * float3(0.70, 0.66, 0.58), dd * 0.3);
c = lerp(c, float3(0.16, 0.13, 0.10), speck * dd);
r = lerp(r, 0.86, dd * 0.6);
return float4(c, saturate(r));
"""

INNER_UV = r"""
float hh = Axis < 0.5 ? (LP.z - HMin) / max(HMax - HMin, 0.01) : length(LP.xy) / max(HMax, 0.01);
float u = (LP.x + LP.y * 0.7) / max(USize, 0.01);
return float2(u, 1.0 - saturate(hh));
"""

INNER_COLOR = r"""
float3 c = C.rgb;
float r = 0.8;
c = lerp(c, c * float3(0.80, 0.55, 0.30), saturate(Toast));
c = lerp(c, c * float3(0.20, 0.15, 0.12), saturate(Char));
c = lerp(c, lerp(c, float3(0.62, 0.66, 0.50), 0.55), saturate(Spoil));
c = lerp(c, float3(0.86, 0.93, 1.0), saturate(Frozen) * 0.55);
c *= lerp(1.0, 0.6, saturate(Wet)); r = lerp(r, 0.2, saturate(Wet));
r = lerp(r, 0.55, saturate(Frozen));
return float4(c, r);
"""

def wpo(m, X, Y):
    lp = MEL.create_material_expression(m, unreal.MaterialExpressionLocalPosition, X, Y)
    lb = MEL.create_material_expression(m, unreal.MaterialExpressionObjectLocalBounds, X, Y + 100)
    tm = MEL.create_material_expression(m, unreal.MaterialExpressionTime, X, Y + 220)
    names = ["Sag", "Tip", "TipSign", "Squash", "Stretch", "StretchAxis", "Wobble", "Melt", "Bite"]
    w = custom(m, WPO, ["LP", "BMin", "BMax", "Time"] + names, X + 600, Y, "food deformation (local)")
    MEL.connect_material_expressions(lp, "", w, "LP")
    MEL.connect_material_expressions(lb, "Min", w, "BMin")
    MEL.connect_material_expressions(lb, "Max", w, "BMax")
    MEL.connect_material_expressions(tm, "", w, "Time")
    for i, n in enumerate(names):
        MEL.connect_material_expressions(cpd(m, n, X, Y + 320 + i * 70), "", w, n)
    tr = MEL.create_material_expression(m, unreal.MaterialExpressionTransform, X + 900, Y)
    tr.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
    tr.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    MEL.connect_material_expressions(w, "", tr, "")
    MEL.connect_material_property(tr, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

def build_outer():
    m = fresh("M_FoodV2")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SUBSURFACE)
    X = -1600
    c = tex(m, "C", "/Engine/EngineResources/WhiteSquareTexture", X, -700)
    n = tex(m, "N", "/Engine/EngineMaterials/FlatNormal", X, -450, "normal")
    # (a masks sampler needs a masks texture as its default: the cake's own AO/rough, imported by import_food_v2.py)
    r = tex(m, "R", "/Game/FoodV2/SM_FoodV2_Cake/T_FoodV2_Cake_R", X, -200, "masks")
    st = {}
    for i, k in enumerate(("TToast", "TBurnt", "TSpoil", "TFrozen")):
        st[k] = tex(m, k, "/Engine/EngineResources/WhiteSquareTexture", X, 50 + i * 250)
    wp = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, X, 1100)
    col = custom(m, COLOR, ["C", "R", "T", "B", "S", "F", "Toast", "Char", "Spoil", "Frozen", "Wet", "Dirt", "WP"], -900, -300, "food colour + states",
                 unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    MEL.connect_material_expressions(c, "RGBA", col, "C"); MEL.connect_material_expressions(r, "RGB", col, "R")
    MEL.connect_material_expressions(st["TToast"], "RGBA", col, "T"); MEL.connect_material_expressions(st["TBurnt"], "RGBA", col, "B")
    MEL.connect_material_expressions(st["TSpoil"], "RGBA", col, "S"); MEL.connect_material_expressions(st["TFrozen"], "RGBA", col, "F")
    for i, k in enumerate(("Toast", "Char", "Spoil", "Frozen", "Wet", "Dirt")):
        MEL.connect_material_expressions(cpd(m, k, X + 300, 1200 + i * 70), "", col, k)
    MEL.connect_material_expressions(wp, "", col, "WP")
    rgb = mask(m, col, "", True, True, True, False, -500, -300)
    rough = mask(m, col, "", False, False, False, True, -500, -150)
    MEL.connect_material_property(rgb, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(r, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    # jelly (gummies, marshmallow, honey, ice): light goes in - subsurface in its own colour; the rest stays opaque
    jelly = sp(m, "Jelly", 0.0, X + 300, 1700)
    ssc = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -500, 300)
    MEL.connect_material_expressions(rgb, "", ssc, "A"); MEL.connect_material_expressions(jelly, "", ssc, "B")
    MEL.connect_material_property(ssc, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    op = MEL.create_material_expression(m, unreal.MaterialExpressionOneMinus, -500, 450)
    MEL.connect_material_expressions(jelly, "", op, "")
    MEL.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
    wpo(m, -1600, 1900)
    MEL.recompile_material(m)
    EAL.save_asset(DIR + "/M_FoodV2")
    unreal.log("FOODMAT M_FoodV2")

def build_inner():
    m = fresh("M_FoodV2_Inner")
    m.set_editor_property("two_sided", True)
    X = -1600
    lp = MEL.create_material_expression(m, unreal.MaterialExpressionLocalPosition, X, -600)
    uvc = custom(m, INNER_UV, ["LP", "HMin", "HMax", "Axis", "USize"], X + 400, -600, "inner strip uv", unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    MEL.connect_material_expressions(lp, "", uvc, "LP")
    for i, (k, v) in enumerate((("HMin", -50.0), ("HMax", 50.0), ("Axis", 0.0), ("USize", 100.0))):
        MEL.connect_material_expressions(sp(m, k, v, X, -450 + i * 80), "", uvc, k)
    c = tex(m, "InC", "/Engine/EngineResources/WhiteSquareTexture", X + 800, -600, "color", uvc)
    col = custom(m, INNER_COLOR, ["C", "Toast", "Char", "Spoil", "Frozen", "Wet"], -500, -300, "inner colour + states", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    MEL.connect_material_expressions(c, "RGBA", col, "C")
    for i, k in enumerate(("Toast", "Char", "Spoil", "Frozen", "Wet")):
        MEL.connect_material_expressions(cpd(m, k, X + 300, 200 + i * 70), "", col, k)
    MEL.connect_material_property(mask(m, col, "", True, True, True, False, -200, -300), "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(mask(m, col, "", False, False, False, True, -200, -150), "", unreal.MaterialProperty.MP_ROUGHNESS)
    wpo(m, -1600, 900)
    MEL.recompile_material(m)
    EAL.save_asset(DIR + "/M_FoodV2_Inner")
    unreal.log("FOODMAT M_FoodV2_Inner")

if not EAL.does_directory_exist(DIR): EAL.make_directory(DIR)
build_outer()
build_inner()
unreal.log("FOODMAT_DONE")
