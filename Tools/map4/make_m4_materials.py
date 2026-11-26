# Map v4: imports the packed texture sets (SourceArt/Map4/packed) and builds the materials of the new look.
#   /Game/Map4/Textures/T_M4_<Set>_{C,N,R}   C sRGB colour, N DirectX normal, R = AO / roughness / metal
#   /Game/Map4/Materials/M_M4      triplanar PBR in world space: colour, normal (+ ant-scale detail normal), AO/rough/metal,
#                                  macro variation against tiling, dust on tops, grime at the foot of walls, glow
#   /Game/Map4/Materials/M_M4_UV   the same inputs through the mesh's own UVs (generated props, prints, labels)
#   All four (M_M4, M_M4_2S, M_M4_UV, M_M4_UV2) draw the state layer of Source/AntKitchen/AntState.h (wet, char, burn, frost...)
#   from custom primitive data, and are rebuilt IN PLACE (the instances made from them stay parented).
#   M4_ONLY_MATS=1 skips the texture import; M4_ONLY_M4=1 skips the UV materials.
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
    """an empty material graph under this name: an existing material is emptied IN PLACE (KAN-21), so every instance made from it
    (the props' MI_*, MI_M4_Paint) stays parented to it and picks the new graph up; only a missing one is created"""
    full = path + "/" + name
    if EAL.does_asset_exist(full):
        m = unreal.load_asset(full)
        if isinstance(m, unreal.Material):
            MEL.delete_all_material_expressions(m)
            return m
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

def cpd4(m, name, index, x, y):
    """four floats read from the component's custom primitive data from index on (no material instance per object): 0 by default"""
    p = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", unreal.LinearColor(0, 0, 0, 0))
    p.set_editor_property("use_custom_primitive_data", True); p.set_editor_property("primitive_data_index", index)
    return p

# The state layer (Source/AntKitchen/AntState.h): custom primitive data 0..12 = the overall value of each kind, 16..35 = five spots
# (xyz in the component's local space, w packed radius / kind / strength). Pins every state custom node takes:
STATE_PINS = ["LP", "S0", "S1", "S2", "S3", "P0", "P1", "P2", "P3", "P4"]

def state_inputs(m, X, Y):
    """the expressions behind STATE_PINS (+ the world position W and time Tm the nodes also use): dict pin -> expression"""
    d = {}
    wp = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, X - 400, Y)
    tpn = MEL.create_material_expression(m, unreal.MaterialExpressionTransformPosition, X, Y)
    tpn.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    tpn.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    MEL.connect_material_expressions(wp, "", tpn, "")
    d["LP"] = tpn
    for i in range(4):
        d["S%d" % i] = cpd4(m, "State%d" % i, i * 4, X, Y + 100 + i * 100)
    for i in range(5):
        d["P%d" % i] = cpd4(m, "StateSpot%d" % i, 16 + i * 4, X, Y + 520 + i * 100)
    d["W"] = wp
    d["Tm"] = MEL.create_material_expression(m, unreal.MaterialExpressionTime, X, Y + 1050)
    return d

def wire(src_by_pin, node, pins):
    for pin in pins:
        out = "RGBA" if pin[0] in "SP" and pin[1:].isdigit() else ""   # a vector parameter's first pin is only RGB
        if not MEL.connect_material_expressions(src_by_pin[pin], out, node, pin):
            unreal.log_warning("M4 could not wire %s" % pin)

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
"""

# ---- the state layer (AntState.h). ONE custom node per material ("AntState") turns the overall values and the five spots into
# one value per kind at this pixel; the colour, ORM and emissive nodes only read its outputs. Nothing at all is computed for a
# thing that shows no state (a dynamic branch on the component's own data: the whole flat but a few things pays nothing).
# Output (float4): wet, char, dirt, damage; Hot: heat, burn, frost, charge; Soak: oil, jam, cola, honey;
# Misc: spoil, the bright rim of a spreading fire, the fine noise, the coarse noise; Aux: the finer noise, 1 if any state.
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

# what a reader of the state node unpacks first (the pins SA, SH, SS, SM, SX)
STATE_USE = ["SA", "SH", "SS", "SM", "SX"]
STATE_IN = r"""
float s_wet = SA.x, s_char = SA.y, s_dirt = SA.z, s_dmg = SA.w;
float s_heat = SH.x, s_burn = SH.y, s_frost = SH.z, s_chg = SH.w;
float s_oil = SS.x, s_jam = SS.y, s_cola = SS.z, s_honey = SS.w;
float s_spoil = SM.x, s_ring = SM.y, s_fine = SM.z, s_n1 = SM.w;
float s_n2 = SX.x;
float s_burnt = max(s_char, s_burn);
"""

# what the states do to the colour (col in, col out)
STATE_COLOR = STATE_IN + r"""
[branch] if (SX.y > 0.0)
{
    // soaked with a liquid
    col = lerp(col, col * float3(0.92, 0.78, 0.42) * 0.85, s_oil * 0.75);
    col = lerp(col, float3(0.30, 0.025, 0.05), s_jam * 0.8);
    col = lerp(col, col * float3(0.32, 0.18, 0.08), s_cola * 0.8);
    col = lerp(col, float3(0.78, 0.44, 0.07) * (0.6 + 0.4 * col), s_honey * 0.75);
    // dirty
    col = lerp(col, float3(0.24, 0.20, 0.15), saturate(s_dirt * (0.25 + s_fine)) * 0.85);
    // wet: darker (the gloss is in the roughness)
    col *= lerp(1.0, 0.5, s_wet);
    // going off: dull, then mould spots
    float s_g = dot(col, float3(0.3, 0.59, 0.11));
    col = lerp(col, s_g * float3(0.85, 0.82, 0.62), s_spoil * 0.65);
    float s_mould = smoothstep(0.66, 0.74, s_n2 + s_spoil * 0.35) * step(0.12, s_spoil);
    col = lerp(col, float3(0.62, 0.68, 0.52) * (0.7 + 0.3 * s_n1), s_mould);
    // char browns first (toast), then blackens through the noise
    col = lerp(col, col * float3(0.60, 0.40, 0.20), saturate(s_burnt / 0.35) * 0.85);
    float s_ck = saturate((s_burnt - 0.3) / 0.7);
    float s_cm = smoothstep(1.05 - s_ck * 1.25, 1.30 - s_ck * 1.25, s_fine + 0.15) * saturate(s_ck * 4.0);
    col = lerp(col, float3(0.020, 0.017, 0.014), s_cm);
    col = lerp(col, float3(0.05, 0.045, 0.04), s_burn * 0.6);
    // frost: white crystals, thicker in the noise
    col = lerp(col, float3(0.86, 0.90, 0.95), saturate(s_frost * (0.55 + 0.6 * s_fine)));
    // damaged: cracks and pale broken edges
    float s_cs = abs(frac((W.x * 0.83 + W.y * 0.61 + W.z * 0.47) / 19.0 + s_n1 * 2.5) - 0.5);
    float s_crack = (1.0 - smoothstep(0.0, 0.03, s_cs)) * step(0.45, s_n1 + s_dmg * 0.4);
    col = lerp(col, col * 0.25, s_crack * saturate(s_dmg * 1.5));
    col = lerp(col, col * 0.8 + 0.06, s_dmg * s_fine * 0.5);
}
return max(col, 0.0);
"""

# what the states do to roughness and metal (rough, metal in; the node returns float3(ao, rough, metal))
STATE_ORM = STATE_IN + r"""
[branch] if (SX.y > 0.0)
{
    rough = lerp(rough, 0.90, s_dirt * 0.6);
    rough = lerp(rough, 0.06, max(s_wet, max(s_oil, s_honey)) * 0.85);
    rough = lerp(rough, 0.25, max(s_jam, s_cola) * 0.8);
    rough = lerp(rough, 0.95, saturate(s_burnt * 1.6 - 0.4));
    rough = lerp(rough, 0.50, s_frost * 0.8);
    rough = lerp(rough, 0.92, s_spoil * 0.7);
    metal *= 1.0 - saturate(s_burnt * 1.6 - 0.4);
    metal *= 1.0 - s_frost * 0.7;
}
return float3(ao, rough, metal);
"""

# the light the states give off: hot metal glows (red, then orange), a burning surface has embers and a bright spreading rim,
# a live / charged thing has blue sparks crawling on it
STATE_EMIS = STATE_IN + r"""
float3 em = float3(0, 0, 0);
[branch] if (SX.y > 0.0)
{
    float s_gl = smoothstep(0.55, 1.0, s_heat);
    em += lerp(float3(0.9, 0.05, 0.0), float3(3.2, 1.3, 0.35), s_gl) * s_gl * s_gl * 6.0;
    float s_fl = frac(sin(dot(floor(W.xy / 7.0 + W.zz / 9.0) + floor(Tm * 9.0) * 3.17, float2(41.3, 289.1))) * 15731.7);
    float s_em = s_burn * saturate(s_fine * 1.6 - 0.35) * (0.55 + 0.45 * s_fl);
    em += float3(3.5, 0.9, 0.12) * s_em * 3.0;
    em += float3(5.0, 2.0, 0.4) * s_ring * (0.6 + 0.4 * s_fl) * 4.0;
    float s_sp = frac(sin(dot(floor(W.xy / 5.0 + W.zz / 5.0) + floor(Tm * 14.0) * 7.31, float2(12.9898, 78.233))) * 43758.5453);
    em += float3(0.5, 0.8, 3.0) * s_chg * step(0.965, s_sp) * 6.0;
}
return em;
"""

COLOR = COLOR + STATE_COLOR

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
""" + STATE_ORM

EMIS = STATE_EMIS

# the UV materials (generated props, prints): the same states over the texture's own colour / ORM
UV_COLOR = r"""
float3 col = Base;
""" + STATE_COLOR
UV_ORM = r"""
float ao = R.r;
float rough = saturate(R.g * RoughMul + RoughAdd);
float metal = saturate(R.b + Metal);
""" + STATE_ORM

def state_node(m, X, Y):
    """the one state node of a material and the expressions behind it: (node, dict pin -> expression)"""
    d = state_inputs(m, X - 400, Y)
    n = custom(m, STATE_NODE, ["W"] + STATE_PINS, X, Y, "AntState: the states at this pixel", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    outs = []
    for name in ("Hot", "Soak", "Misc", "Aux"):
        o = unreal.CustomOutput()
        o.set_editor_property("output_name", name)
        o.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
        outs.append(o)
    n.set_editor_property("additional_outputs", outs)
    MEL.connect_material_expressions(d["W"], "", n, "W")
    wire(d, n, STATE_PINS)
    return n, d

def use_states(sn, node):
    """a reader of the state node: its pins SA, SH, SS, SM, SX from the node's outputs"""
    for pin, out in (("SA", ""), ("SH", "Hot"), ("SS", "Soak"), ("SM", "Misc"), ("SX", "Aux")):
        if not MEL.connect_material_expressions(sn, out, node, pin):
            unreal.log_warning("M4 could not wire the state output %s -> %s" % (out or "main", pin))

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
    # the states of the thing (AntState.h): the matter system, the mess, the wreck and the food set them on the component
    sn, st = state_node(m, X - 300, 2200)

    col = custom(m, COLOR, ["TC", "W", "N", "S", "Tint", "Var", "Dust", "Grime", "Base"] + STATE_USE, -700, -300, "M4 colour + states")
    for src, pin in [(tc, "TC"), (wp, "W"), (vn, "N"), (S, "S"), (var, "Var"), (dust, "Dust"), (grime, "Grime"), (base, "Base")]:
        MEL.connect_material_expressions(src, "", col, pin)
    use_states(sn, col)
    colp = vp(m, "Color", (1, 1, 1, 1), X, 2100)
    tmul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -1100, 700)
    MEL.connect_material_expressions(tint, "RGB", tmul, "A")
    MEL.connect_material_expressions(colp, "RGB", tmul, "B")
    MEL.connect_material_expressions(tmul, "", col, "Tint")
    nrm = custom(m, NORMAL, ["TN", "TD", "W", "N", "S", "DS", "NStr", "DStr"], -700, 200, "M4 normal (world)")
    for src, pin in [(tn, "TN"), (td, "TD"), (wp, "W"), (vn, "N"), (S, "S"), (ds, "DS"), (nstr, "NStr"), (dstr, "DStr")]:
        MEL.connect_material_expressions(src, "", nrm, pin)
    orm = custom(m, ORM, ["TR", "W", "N", "S", "Dust", "AOStr", "RoughMul", "RoughAdd", "MetalTex", "Metal"] + STATE_USE, -700, 700, "M4 AO rough metal + states")
    for src, pin in [(tr, "TR"), (wp, "W"), (vn, "N"), (S, "S"), (dust, "Dust"), (aos, "AOStr"), (rmul, "RoughMul"), (radd, "RoughAdd"), (mtex, "MetalTex"), (metal, "Metal")]:
        MEL.connect_material_expressions(src, "", orm, pin)
    use_states(sn, orm)
    MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(mask(m, orm, True, False, False, -300, 700), "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    MEL.connect_material_property(mask(m, orm, False, True, False, -300, 800), "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(mask(m, orm, False, False, True, -300, 900), "", unreal.MaterialProperty.MP_METALLIC)
    emit_states(m, col, glow, wp, sn, st, -300, 1000)
    MEL.recompile_material(m)
    EAL.save_asset(MAT + "/" + name)
    unreal.log("M4MAT " + name)

def emit_states(m, col, glow, wp, sn, st, x, y):
    """emissive = colour x Glow (lamps, screens) + the light of the states (hot metal, embers, sparks)"""
    mul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, x, y)
    MEL.connect_material_expressions(col, "", mul, "A")
    MEL.connect_material_expressions(glow, "RGB", mul, "B")
    em = custom(m, EMIS, ["W", "Tm"] + STATE_USE, x - 400, y + 200, "M4 state light")
    MEL.connect_material_expressions(wp, "", em, "W")
    MEL.connect_material_expressions(st["Tm"], "", em, "Tm")
    use_states(sn, em)
    add = MEL.create_material_expression(m, unreal.MaterialExpressionAdd, x + 200, y)
    MEL.connect_material_expressions(mul, "", add, "A")
    MEL.connect_material_expressions(em, "", add, "B")
    MEL.connect_material_property(add, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

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
    # the props' R textures are TC_MASKS, linear (import_m4_props.py): the sampler and its default must be masks too, or the
    # material does not compile (an sRGB default under a linear-colour sampler) and every prop falls back to the default material
    r.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    dr = unreal.load_asset(TEX + "/T_M4_Oak_R")
    if dr: r.set_editor_property("texture", dr)
    tint = vp(m, "Tint", (1, 1, 1, 1), X, 600)
    rmul = sp(m, "RoughMul", 1.0, X, 700)
    radd = sp(m, "RoughAdd", 0.0, X, 800)
    metal = sp(m, "Metal", 0.0, X, 900)
    glow = vp(m, "Glow", (0, 0, 0, 1), X, 1000)
    colp = vp(m, "Color", (1, 1, 1, 1), X, 1100)
    tm = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -800, 600)
    MEL.connect_material_expressions(tint, "RGB", tm, "A"); MEL.connect_material_expressions(colp, "RGB", tm, "B")
    mc = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -800, -300)
    MEL.connect_material_expressions(c, "RGB", mc, "A"); MEL.connect_material_expressions(tm, "", mc, "B")
    # the states of the thing (AntState.h), the same as M_M4's, over the texture's colour and ORM
    sn, st = state_node(m, X - 200, 1300)
    col = custom(m, UV_COLOR, ["Base", "W"] + STATE_USE, -500, -300, "UV colour + states")
    MEL.connect_material_expressions(mc, "", col, "Base")
    MEL.connect_material_expressions(st["W"], "", col, "W")
    use_states(sn, col)
    MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    orm = custom(m, UV_ORM, ["R", "RoughMul", "RoughAdd", "Metal"] + STATE_USE, -500, 300, "UV AO rough metal + states")
    MEL.connect_material_expressions(r, "RGB", orm, "R")
    MEL.connect_material_expressions(rmul, "", orm, "RoughMul")
    MEL.connect_material_expressions(radd, "", orm, "RoughAdd")
    MEL.connect_material_expressions(metal, "", orm, "Metal")
    use_states(sn, orm)
    MEL.connect_material_property(mask(m, orm, True, False, False, -200, 300), "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    MEL.connect_material_property(mask(m, orm, False, True, False, -200, 400), "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(mask(m, orm, False, False, True, -200, 500), "", unreal.MaterialProperty.MP_METALLIC)
    emit_states(m, mc, glow, st["W"], sn, st, -200, 700)
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
if os.environ.get("M4_ONLY_M4", "") != "1":   # (rebuilt in place since KAN-21: the props' instances stay parented to them)
    build_uv("M_M4_UV")
    build_uv("M_M4_UV2", True)
unreal.log("M4_DONE")
