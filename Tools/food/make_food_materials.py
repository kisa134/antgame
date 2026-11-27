# KAN-36: the food materials, made in the editor without hand work.
#   /Game/FoodV2/M_FoodV2        the new models (-AntFoodV2Models, new foods only): baked colour / normal / AO-rough, the baked colour of
#                                each visible state (toasted, burnt, spoiled, frozen: roughness in alpha), soaked and dirty on top,
#                                jelly = subsurface; the deformation layer (world position offset)
#   /Game/FoodV2/M_FoodV2_Inner  the faces of a break of a new model: the inner strip read by height, the same states, the same deformation
#   THE OLD MODELS (the owner's decision 26.11: every food that had a model keeps it) get the same deformation through a copy of their
#   own master with the deformation layer added - built by the very same code as the master, so the look at rest is the same:
#   /Game/Map4/Materials/M_M4_Food      = M_M4 + deformation      (SM_M4_Crumb, Pea, Speck... in a look: MI_M4_Paint, MI_M4_<Look>)
#   /Game/Map4/Materials/M_M4_UV_Food   = M_M4_UV + deformation   (SM_M4_AppleSlice, BirthdayCake, Caramel, CondensedMilk, Cookie...)
#   /Game/Meshy/M_MeshyPBR_Food         = M_MeshyPBR + the state layer of M_M4_UV + deformation (SM_Food_CakePiece, CheeseChunk,
#                                         Chocolate, Gummy, GummyBearGold, HoneyDrop, SugarCube)
#   AntFood (Source/AntKitchen/AntFood.cpp) swaps an old food's instance on the visible component for a shared dynamic instance of the
#   _Food master with the old instance's parameters copied (one per old instance, not per piece).
# Per piece everything comes from custom primitive data: the states are AntState's (AntState.h: 0 wet, 1 char = toast 0..0.35 /
# burnt above, 2 dirt, 6 frost, 12 spoil; 16..35 its spots), the deformation is packed into 13..15 (AntFood.h CpdDeform0).
# Run: UnrealEditor-Cmd.exe AntKitchen.uproject -run=pythonscript -script=Tools/food/make_food_materials.py
#   FOODMAT_ONLY=old | v2   builds only that part
import unreal, os
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
DIR = "/Game/FoodV2"
ONLY = os.environ.get("FOODMAT_ONLY", "")

# custom primitive data (AntState.h / AntFood.h)
CPD = dict(Wet=0, Char=1, Dirt=2, Frozen=6, Spoil=12, D0=13, D1=14, D2=15)

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
          "masks": unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, "linear": unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR,
          "gray": unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE}[kind]
    t.set_editor_property("sampler_type", st)
    t.set_editor_property("sampler_source", unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)   # one shared sampler
    if uv is not None: MEL.connect_material_expressions(uv, "", t, "UVs")
    return t

def mask(m, src, pin, r, g, b, a, x, y):
    cm = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, x, y)
    for k, v in (("r", r), ("g", g), ("b", b), ("a", a)): cm.set_editor_property(k, v)
    MEL.connect_material_expressions(src, pin, cm, "")
    return cm

# The deformation (local space, cm of the mesh): no soft bodies, a few numbers per component, packed by AntFood::PackDeform into
# custom primitive data 13..15 (exact integers in a float):
#   D0 = Sag*1023 * 1024 + Tip*1023;  D1 = Stretch*1023 * 1024 + Squash*1023;
#   D2 = Wobble*255 * 65536 + Melt*255 * 256 + Bite*63 * 4 + Axis * 2 + (TipSign < 0)
# All zero: no offset at all (a food at rest looks exactly like its model).
WPO = r"""
[branch] if (D0 + D1 + D2 <= 0.0)
{
    return float3(0, 0, 0);
}
float Sag = floor(D0 / 1024.0); float Tip = (D0 - Sag * 1024.0) / 1023.0; Sag /= 1023.0;
float Stretch = floor(D1 / 1024.0); float Squash = (D1 - Stretch * 1024.0) / 1023.0; Stretch /= 1023.0;
float wq = floor(D2 / 65536.0); float rr = D2 - wq * 65536.0;
float mq = floor(rr / 256.0); rr -= mq * 256.0;
float bq = floor(rr / 4.0); rr -= bq * 4.0;
float StretchAxis = floor(rr / 2.0);
float TipSign = (rr - StretchAxis * 2.0) > 0.5 ? -1.0 : 1.0;
float Wobble = wq / 255.0, Melt = mq / 255.0, Bite = bq / 63.0;
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
// sag: held at both ends the middle hangs; held at one end the far end droops (and comes in a little: it bends, not stretches).
// The bend keeps its lowest point where the model's was (the ends / the held end come up instead of the rest going down): a
// piece dragged along the floor would otherwise bend into the floor and nothing would show.
o.z += h.z * 1.6 * Sag * min(p.x * p.x, 1.0);
float t = saturate((p.x * TipSign + 1.0) * 0.5);
o.z += h.x * Tip * (1.0 - t * t) * 1.1;
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

# the new models' colour: the baked state textures by the shared layer's values (char 0..0.35 is toasted, above that burnt)
COLOR = r"""
float Toast = saturate(Char / 0.35);
float Burnt = saturate((Char - 0.35) / 0.65);
float3 c = C.rgb;
float r = R.g;
c = lerp(c, T.rgb, Toast);            r = lerp(r, T.a, Toast);
c = lerp(c, B.rgb, Burnt);            r = lerp(r, B.a, Burnt);
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
// (UYWeight 0.7, UOffset 0: every food as before. A food whose inner strip is a real cross-section - the tomato: its core must
//  come out in the middle of the cut - sets UYWeight 0 and UOffset so that U = 0.5 at the whole food's centre: import_food_v2.py)
float u = (LP.x + LP.y * UYWeight) / max(USize, 0.01) + UOffset;
return float2(u, 1.0 - saturate(hh));
"""

INNER_COLOR = r"""
float Toast = saturate(Char / 0.35);
float Burnt = saturate((Char - 0.35) / 0.65);
float3 c = C.rgb;
float r = 0.8;
c = lerp(c, c * float3(0.80, 0.55, 0.30), Toast);
c = lerp(c, c * float3(0.20, 0.15, 0.12), Burnt);
c = lerp(c, lerp(c, float3(0.62, 0.66, 0.50), 0.55), saturate(Spoil));
c = lerp(c, float3(0.86, 0.93, 1.0), saturate(Frozen) * 0.55);
c *= lerp(1.0, 0.6, saturate(Wet)); r = lerp(r, 0.2, saturate(Wet));
r = lerp(r, 0.55, saturate(Frozen));
return float4(c, r);
"""

def wpo(m, X, Y):
    """the deformation layer on m's world position offset (reads custom primitive data 13..15)"""
    lp = MEL.create_material_expression(m, unreal.MaterialExpressionLocalPosition, X, Y)
    lb = MEL.create_material_expression(m, unreal.MaterialExpressionObjectLocalBounds, X, Y + 100)
    tm = MEL.create_material_expression(m, unreal.MaterialExpressionTime, X, Y + 220)
    w = custom(m, WPO, ["LP", "BMin", "BMax", "Time", "D0", "D1", "D2"], X + 600, Y, "food deformation (local, AntFood::PackDeform)")
    MEL.connect_material_expressions(lp, "", w, "LP")
    MEL.connect_material_expressions(lb, "Min", w, "BMin")
    MEL.connect_material_expressions(lb, "Max", w, "BMax")
    MEL.connect_material_expressions(tm, "", w, "Time")
    for i, n in enumerate(("D0", "D1", "D2")):
        p = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, X, Y + 320 + i * 70)
        p.set_editor_property("parameter_name", "FoodDeform%d" % i); p.set_editor_property("default_value", 0.0)
        p.set_editor_property("use_custom_primitive_data", True); p.set_editor_property("primitive_data_index", CPD[n])
        MEL.connect_material_expressions(p, "", w, n)
    tr = MEL.create_material_expression(m, unreal.MaterialExpressionTransform, X + 900, Y)
    tr.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
    tr.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    MEL.connect_material_expressions(w, "", tr, "")
    MEL.connect_material_property(tr, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    try:
        m.set_editor_property("max_world_position_offset_displacement", 40.0)   # (Nanite culls by the bounds grown by this much)
    except Exception as e:
        unreal.log_warning("FOODMAT no max WPO displacement: %s" % e)

# glass (the ice cube): what is not clear is the blue of _R (the white heart of trapped air, frost, cracks, bubbles); the states
# cloud it (frozen: frosted over; burnt, dirty, spoiled: a film); at a glancing angle it mirrors rather than shows through
GLASS_OPACITY = r"""
float o = lerp(Clear, 1.0, saturate(R.b));
o = max(o, saturate(Frozen) * 0.85);
o = max(o, saturate(Char / 0.35) * 0.35 + saturate((Char - 0.35) / 0.65) * 0.55);
o = max(o, saturate(Spoil) * 0.45);
o = max(o, saturate(Dirt) * 0.5);
return saturate(o + Fr * 0.45);
"""

def build_outer(name="M_FoodV2", glass=False):
    m = fresh(name)
    if glass:
        # M_FoodV2 is an opaque subsurface material: its Opacity (1 - Jelly) is how far light scatters in it, never transparency -
        # the ice came out as milk glass. This copy is translucent, lit per pixel, and bends what is seen through it.
        m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        tl = getattr(unreal.TranslucencyLightingMode, "TLM_SURFACE_PER_PIXEL_LIGHTING", None) or getattr(unreal.TranslucencyLightingMode, "TLM_SURFACE", None)
        for k, v in (("translucency_lighting_mode", tl), ("refraction_method", getattr(getattr(unreal, "RefractionMode", None), "RM_INDEX_OF_REFRACTION", None)),
                     ("screen_space_reflections", True), ("cast_dynamic_shadow_as_masked", True)):
            try:
                if v is not None: m.set_editor_property(k, v)
            except Exception as e:
                unreal.log_warning("FOODMAT glass: no %s (%s)" % (k, e))
    else:
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
    col = custom(m, COLOR, ["C", "R", "T", "B", "S", "F", "Char", "Spoil", "Frozen", "Wet", "Dirt", "WP"], -900, -300, "food colour + states",
                 unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    MEL.connect_material_expressions(c, "RGBA", col, "C"); MEL.connect_material_expressions(r, "RGB", col, "R")
    MEL.connect_material_expressions(st["TToast"], "RGBA", col, "T"); MEL.connect_material_expressions(st["TBurnt"], "RGBA", col, "B")
    MEL.connect_material_expressions(st["TSpoil"], "RGBA", col, "S"); MEL.connect_material_expressions(st["TFrozen"], "RGBA", col, "F")
    for i, k in enumerate(("Char", "Spoil", "Frozen", "Wet", "Dirt")):
        MEL.connect_material_expressions(cpd(m, k, X + 300, 1200 + i * 70), "", col, k)
    MEL.connect_material_expressions(wp, "", col, "WP")
    rgb = mask(m, col, "", True, True, True, False, -500, -300)
    rough = mask(m, col, "", False, False, False, True, -500, -150)
    MEL.connect_material_property(rgb, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(r, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    if glass:
        fr = MEL.create_material_expression(m, unreal.MaterialExpressionFresnel, -900, 500)
        for k, v in (("exponent", 4.0), ("base_reflect_fraction", 0.02)):
            try: fr.set_editor_property(k, v)
            except Exception as e: unreal.log_warning("FOODMAT glass fresnel %s: %s" % (k, e))
        op = custom(m, GLASS_OPACITY, ["R", "Clear", "Frozen", "Char", "Spoil", "Dirt", "Fr"], -500, 450, "glass opacity", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        MEL.connect_material_expressions(r, "RGB", op, "R"); MEL.connect_material_expressions(sp(m, "OpacityClear", 0.10, X + 300, 1700), "", op, "Clear")
        for i, k in enumerate(("Frozen", "Char", "Spoil", "Dirt")):
            MEL.connect_material_expressions(cpd(m, k, X + 600, 1200 + i * 70), "", op, k)
        MEL.connect_material_expressions(fr, "", op, "Fr")
        MEL.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
        MEL.connect_material_property(sp(m, "IOR", 1.31, X + 300, 1780), "", unreal.MaterialProperty.MP_REFRACTION)
        MEL.connect_material_property(sp(m, "Specular", 0.5, X + 300, 1860), "", unreal.MaterialProperty.MP_SPECULAR)
    else:
        # jelly (gummies, marshmallow, honey): light goes in - subsurface in its own colour; the rest stays opaque
        jelly = sp(m, "Jelly", 0.0, X + 300, 1700)
        ssc = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -500, 300)
        MEL.connect_material_expressions(rgb, "", ssc, "A"); MEL.connect_material_expressions(jelly, "", ssc, "B")
        MEL.connect_material_property(ssc, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
        op = MEL.create_material_expression(m, unreal.MaterialExpressionOneMinus, -500, 450)
        MEL.connect_material_expressions(jelly, "", op, "")
        MEL.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
    wpo(m, -1600, 1900)
    MEL.recompile_material(m)
    EAL.save_asset(DIR + "/" + name)
    unreal.log("FOODMAT " + name)

def build_inner():
    m = fresh("M_FoodV2_Inner")
    m.set_editor_property("two_sided", True)
    X = -1600
    lp = MEL.create_material_expression(m, unreal.MaterialExpressionLocalPosition, X, -600)
    uvc = custom(m, INNER_UV, ["LP", "HMin", "HMax", "Axis", "USize", "UYWeight", "UOffset"], X + 400, -600, "inner strip uv", unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    MEL.connect_material_expressions(lp, "", uvc, "LP")
    for i, (k, v) in enumerate((("HMin", -50.0), ("HMax", 50.0), ("Axis", 0.0), ("USize", 100.0), ("UYWeight", 0.7), ("UOffset", 0.0))):
        MEL.connect_material_expressions(sp(m, k, v, X, -450 + i * 80), "", uvc, k)
    c = tex(m, "InC", "/Engine/EngineResources/WhiteSquareTexture", X + 800, -600, "color", uvc)
    col = custom(m, INNER_COLOR, ["C", "Char", "Spoil", "Frozen", "Wet"], -500, -300, "inner colour + states", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    MEL.connect_material_expressions(c, "RGBA", col, "C")
    for i, k in enumerate(("Char", "Spoil", "Frozen", "Wet")):
        MEL.connect_material_expressions(cpd(m, k, X + 300, 200 + i * 70), "", col, k)
    MEL.connect_material_property(mask(m, col, "", True, True, True, False, -200, -300), "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(mask(m, col, "", False, False, False, True, -200, -150), "", unreal.MaterialProperty.MP_ROUGHNESS)
    wpo(m, -1600, 900)
    MEL.recompile_material(m)
    EAL.save_asset(DIR + "/M_FoodV2_Inner")
    unreal.log("FOODMAT M_FoodV2_Inner")

# ---------------------------------------------------------------- the old models' masters + deformation
def m4_namespace():
    """the definitions of Tools/map4/make_m4_materials.py (its builders, the state layer), without running its main part: the _Food
    masters are made by the same code as M_M4 / M_M4_UV, so an old food at rest looks exactly as before"""
    path = os.path.join(PROJ, "Tools", "map4", "make_m4_materials.py")
    with open(path, encoding="utf-8") as f:
        src = f.read()
    cut = src.find("\nONLY_FX = ")
    if cut < 0:
        raise RuntimeError("FOODMAT make_m4_materials.py changed: no 'ONLY_FX =' line to stop at")
    os.environ["M4_ONLY_MATS"] = "1"   # (no texture import)
    ns = {"__name__": "m4defs", "__file__": path}
    exec(compile(src[:cut], path, "exec"), ns)
    # food does not glow red-hot like metal (a chocolate by the fire stays brown; its embers when it burns stay)
    hot = "float s_gl = smoothstep(0.55, 1.0, s_heat);"
    if hot not in ns["STATE_EMIS"]:
        raise RuntimeError("FOODMAT make_m4_materials.py changed: no heat glow line to take out for food")
    for k in ("EMIS", "STATE_EMIS"):
        ns[k] = ns[k].replace(hot, "float s_gl = 0.0;   // (food: no red-hot glow)")

    class FoodMEL(object):
        """MaterialEditingLibrary, except that a *_Food material gets the deformation layer just before it is compiled"""
        def __getattr__(self, k):
            return getattr(MEL, k)
        def recompile_material(self, m):
            if m.get_name().endswith("_Food"):
                wpo(m, -2600, 3200)
            return MEL.recompile_material(m)
    ns["MEL"] = FoodMEL()
    return ns

MESHY = "/Game/Meshy"

def build_meshy_food(ns):
    """M_MeshyPBR (Tools/import_meshy.py: BaseColor x Tint, Normal, Metallic, Roughness, emissive = colour x Glow) with the state layer
    of M_M4_UV (wet, toast / burnt, frost, dirt, spoil, the spots of AntMatter) and the deformation"""
    m = ns["fresh"](MESHY, "M_MeshyPBR_Food")
    m.set_editor_property("used_with_nanite", True)
    X = -1400
    def t(name, y, sampler, default):
        e = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, X, y)
        e.set_editor_property("parameter_name", name); e.set_editor_property("sampler_type", sampler)
        d = unreal.load_asset(default)
        if d: e.set_editor_property("texture", d)
        return e
    body = MESHY + "/SM_Ant_Body/T_Ant_Body_"
    bc = t("BaseColor", -400, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, body + "BaseColor")
    nm = t("Normal", -100, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, body + "Normal")
    mt = t("Metallic", 200, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, body + "Metallic")
    rg = t("Roughness", 500, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, body + "Roughness")
    tint = ns["vp"](m, "Tint", (1, 1, 1, 1), X, -600)
    glow = ns["sp"](m, "Glow", 0.0, X, 800)
    mc = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -1000, -400)
    MEL.connect_material_expressions(bc, "RGB", mc, "A"); MEL.connect_material_expressions(tint, "RGB", mc, "B")
    sn, st = ns["state_node"](m, X - 200, 1200)
    col = ns["custom"](m, ns["UV_COLOR"], ["Base", "W"] + ns["STATE_USE"], -600, -400, "Meshy colour + states")
    MEL.connect_material_expressions(mc, "", col, "Base")
    MEL.connect_material_expressions(st["W"], "", col, "W")
    ns["use_states"](sn, col)
    MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(nm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    # ORM: no AO in a Meshy model (1), its roughness and metal maps as they are (RoughMul 1, RoughAdd 0, Metal 0), then the states
    orm_in = ns["custom"](m, "return float3(1.0, Rg, Mt);", ["Rg", "Mt"], -1000, 300, "Meshy ORM")
    MEL.connect_material_expressions(rg, "R", orm_in, "Rg"); MEL.connect_material_expressions(mt, "R", orm_in, "Mt")
    orm = ns["custom"](m, ns["UV_ORM"], ["R", "RoughMul", "RoughAdd", "Metal"] + ns["STATE_USE"], -600, 300, "Meshy rough metal + states")
    MEL.connect_material_expressions(orm_in, "", orm, "R")
    MEL.connect_material_expressions(ns["sp"](m, "RoughMul", 1.0, X, 900), "", orm, "RoughMul")
    MEL.connect_material_expressions(ns["sp"](m, "RoughAdd", 0.0, X, 980), "", orm, "RoughAdd")
    MEL.connect_material_expressions(ns["sp"](m, "Metal", 0.0, X, 1060), "", orm, "Metal")
    ns["use_states"](sn, orm)
    MEL.connect_material_property(ns["mask"](m, orm, False, True, False, -300, 300), "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(ns["mask"](m, orm, False, False, True, -300, 400), "", unreal.MaterialProperty.MP_METALLIC)
    # emissive: colour x Glow (as before) + the light of the states (embers of a burning patch)
    gm = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -600, 700)
    MEL.connect_material_expressions(mc, "", gm, "A"); MEL.connect_material_expressions(glow, "", gm, "B")
    em = ns["custom"](m, ns["STATE_EMIS"], ["W", "Tm"] + ns["STATE_USE"], -600, 900, "Meshy state light")
    MEL.connect_material_expressions(st["W"], "", em, "W"); MEL.connect_material_expressions(st["Tm"], "", em, "Tm")
    ns["use_states"](sn, em)
    add = MEL.create_material_expression(m, unreal.MaterialExpressionAdd, -300, 700)
    MEL.connect_material_expressions(gm, "", add, "A"); MEL.connect_material_expressions(em, "", add, "B")
    MEL.connect_material_property(add, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    wpo(m, -2600, 3200)
    MEL.recompile_material(m)
    EAL.save_asset(MESHY + "/M_MeshyPBR_Food")
    unreal.log("FOODMAT M_MeshyPBR_Food")

def build_old():
    ns = m4_namespace()
    ns["build_m4"]("M_M4_Food")
    ns["build_uv"]("M_M4_UV_Food")
    build_meshy_food(ns)

if ONLY == "glass":
    # pass 3 (KAN-36, 27.11): only what the ice and the tomato need - the glass master (new) and the inner master (two new
    # parameters whose defaults give every other food the very same picture); M_FoodV2 and the old models' masters are not touched
    build_outer("M_FoodV2_Glass", True)
    build_inner()
else:
    if ONLY != "old":
        if not EAL.does_directory_exist(DIR): EAL.make_directory(DIR)
        build_outer()
        build_outer("M_FoodV2_Glass", True)
        build_inner()
    if ONLY != "v2":
        build_old()
unreal.log("FOODMAT_DONE")
