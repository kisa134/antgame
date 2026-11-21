# Mess (branch code/ANT-mess): every asset the stains, the food splats and the dust need, made from this script. Nothing by hand.
#   /Game/Mess/M_MessStain     a deferred decal: one procedural stain (pool, splat, smear, scratches, crumbs, sole print), all by parameters
#   /Game/Mess/M_MessBlob      lit opaque, glossy: jam / cream / crumb blobs (per-instance colour + roughness on instanced meshes)
#   /Game/Mess/M_MessDust      lit translucent dust motes: a cloud of quads that wraps round the camera (all the motion is in the shader)
#   /Game/Mess/M_MessDustUnlit the same, unlit (brightness from the code)
# Run: UnrealEditor-Cmd.exe AntKitchen.uproject -run=pythonscript -script=Tools/make_mess_assets.py
import unreal
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/Mess"

def fresh(name):
    full = DIR + "/" + name
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    return AT.create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())

def custom(m, code, out_type, inputs, x, y, desc):
    c = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("code", code)
    c.set_editor_property("output_type", out_type)
    c.set_editor_property("description", desc)
    ins = []
    for n in inputs:
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
    c.set_editor_property("inputs", ins)
    return c

def vparam(m, name, default, x, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", unreal.LinearColor(*default))
    return p

def sparam(m, name, default, x, y):
    p = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", default)
    return p

def wire(c, pairs):
    for src, name in pairs:
        pin = "RGBA" if isinstance(src, unreal.MaterialExpressionVectorParameter) else ""
        MEL.connect_material_expressions(src, pin, c, name)

def chan(m, src, r, g, b, a, x, y):
    cm = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, x, y)
    cm.set_editor_property("r", r); cm.set_editor_property("g", g); cm.set_editor_property("b", b); cm.set_editor_property("a", a)
    MEL.connect_material_expressions(src, "", cm, "")
    return cm

def const(m, v, x, y):
    c = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, x, y)
    c.set_editor_property("r", v)
    return c

# ---------------------------------------------------------------------------------------------------------------------
# The stain field. p in [-1, 1]^2 over the decal (x along the decal's length for smears and prints). F() is a signed field:
# > 0 inside, roughly the distance to the edge in units of the decal's half-width. Shapes: 0 pool, 1 splat, 2 smear,
# 3 scratches, 4 crumbs ground in, 5 a sole print. Returns (coverage, thickness, d thickness / dx, d thickness / dy).
# ---------------------------------------------------------------------------------------------------------------------
FIELD = r"""
struct MessF
{
    float H(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
    float N(float2 p)
    {
        float2 i = floor(p), f = frac(p); float2 u = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(H(i), H(i + float2(1, 0)), u.x), lerp(H(i + float2(0, 1)), H(i + float2(1, 1)), u.x), u.y);
    }
    float FBM(float2 p) { float s = 0.0, a = 0.5; for (int i = 0; i < 4; i++) { s += a * N(p); p = p * 2.07 + float2(13.7, 7.1); a *= 0.5; } return s; }
    float SegD(float2 p, float2 a, float2 b) { float2 pa = p - a, ba = b - a; float t = saturate(dot(pa, ba) / max(dot(ba, ba), 1e-6)); return length(pa - ba * t); }

    float F(float2 p, float sd, float shape, float asp, float spk)
    {
        float2 o = float2(sd * 17.31 + 3.7, sd * 9.73 + 1.1);
        if (shape < 0.5)          // a pool: a lumpy round puddle with a lobe or two
        {
            float a = atan2(p.y, p.x); float2 dir = float2(cos(a), sin(a));
            float w = 0.70 + 0.30 * (FBM(dir * 1.5 + o) - 0.5) + 0.05 * (N(dir * 7.0 + o) - 0.5);
            float s = w - length(p);
            for (int i = 0; i < 3; i++)
            {
                float ang = H(o + float2(i, 3.1)) * 6.2832; float2 c = float2(cos(ang), sin(ang)) * (0.3 + 0.3 * H(o + float2(i, 7.7)));
                s = max(s, 0.2 + 0.16 * H(o + float2(i, 1.3)) - length(p - c));
            }
            return s;
        }
        if (shape < 1.5)          // a splat: a core with a spiky crown and droplets thrown outwards
        {
            float a = atan2(p.y, p.x); float2 dir = float2(cos(a), sin(a));
            float sp = (pow(N(float2(a * 2.4, 0.0) + o * 3.0), 3.0) * 0.30 + pow(N(float2(a * 6.3, 4.0) + o), 5.0) * 0.28) * spk;
            float s = 0.38 + 0.14 * (1.0 - spk) + (0.24 + 0.16 * (1.0 - spk)) * (FBM(dir * 1.8 + o) - 0.5) + sp - length(p);
            for (int i = 0; i < 6 + (int)(8.0 * spk); i++)
            {
                float ang = H(o + float2(i * 1.7, 2.3)) * 6.2832; float2 rd = float2(cos(ang), sin(ang));
                float dist = 0.48 + 0.46 * H(o + float2(i * 2.9, 5.1));
                float r = 0.018 + 0.06 * H(o + float2(i * 3.3, 8.9)) * (1.15 - dist);
                float2 q = p - rd * dist;
                float along = dot(q, rd), across = dot(q, float2(-rd.y, rd.x));
                s = max(s, r * (1.3 - 0.3 * spk) - length(float2(along * lerp(0.9, 0.5, spk), across)));
            }
            return s;
        }
        if (shape < 2.5)          // a smear: thick at the head (+x), thinning and streaky towards the tail
        {
            float t = p.x * 0.5 + 0.5;
            float hw = 0.78 * (0.45 + 0.55 * t) * (0.82 + 0.36 * N(float2(p.x * 2.2, 0.5) + o));
            float sy = hw - abs(p.y + 0.12 * (N(float2(p.x * 1.3, 9.0) + o) - 0.5));
            float sx = (0.94 - abs(p.x)) * asp;
            float s = min(sy, sx);
            float k = 0.12; float hh = saturate(0.5 + 0.5 * (sx - sy) / k); s = lerp(sx, sy, hh) - k * hh * (1.0 - hh);   // smooth min
            return s;
        }
        if (shape < 3.5)          // scratches: thin grooves from shards dragged over the floor
        {
            float s = -1.0;
            for (int i = 0; i < 7; i++)
            {
                float ang = H(o + float2(i, 11.0)) * 3.1416; float2 d = float2(cos(ang), sin(ang));
                float2 c = (float2(H(o + float2(i, 12.0)), H(o + float2(i, 13.0))) - 0.5) * 0.9;
                float L = 0.15 + 0.45 * H(o + float2(i, 14.0));
                float w = 0.006 + 0.012 * H(o + float2(i, 15.0));
                s = max(s, w - SegD(p, c - d * L, c + d * L));
            }
            return s;
        }
        if (shape < 4.5)          // crumbs ground into the floor: specks, denser in the middle
        {
            float g = 11.0; float2 q = p * g; float2 cell = floor(q); float s = -1.0;
            for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++)
            {
                float2 c = cell + float2(i, j); float h1 = H(c + o), h2 = H(c + o + 5.3), h3 = H(c + o + 9.1);
                float2 cp = (c + 0.2 + 0.6 * float2(h1, h2)) / g;
                float dens = saturate(1.15 - length(cp) * 1.15);
                if (h3 > dens) continue;
                float r = (0.08 + 0.22 * frac(h1 * 7.0 + h2 * 3.0)) / g;
                s = max(s, r - length(p - cp));
            }
            return s;
        }
        // a sole print: the ball of the foot and the heel, the tread breaks it up
        {
            float2 b = (p - float2(0.32, 0.0)) / float2(0.62, 0.86);
            float2 h = (p - float2(-0.6, 0.0)) / float2(0.34, 0.66);
            float s = max((1.0 - length(b)) * 0.7, (1.0 - length(h)) * 0.55);
            s -= 0.10 * step(0.55, N(p * float2(5.0, 9.0) + o));
            return s;
        }
    }
};
MessF f;
float s = f.F(P, Seed, Shape, Asp, Spk);
float aa = max(fwidth(s) * 0.8, 0.002);
float cov = smoothstep(-aa, aa, s);
float e = 0.012;
float sx1 = f.F(P + float2(e, 0), Seed, Shape, Asp, Spk), sx0 = f.F(P - float2(e, 0), Seed, Shape, Asp, Spk);
float sy1 = f.F(P + float2(0, e), Seed, Shape, Asp, Spk), sy0 = f.F(P - float2(0, e), Seed, Shape, Asp, Spk);
float ed = max(Edge, 0.005);
float h = sqrt(saturate(s / ed));
float2 g = float2(sqrt(saturate(sx1 / ed)) - sqrt(saturate(sx0 / ed)), sqrt(saturate(sy1 / ed)) - sqrt(saturate(sy0 / ed))) / (2.0 * e);
// inner variation: a pool is uneven, a smear streaky
float2 o = float2(Seed * 17.31 + 3.7, Seed * 9.73 + 1.1);
float var = (Shape > 1.5 && Shape < 2.5) ? (0.45 + 0.55 * f.N(float2(P.x * 2.0, P.y * 13.0) + o)) : (0.78 + 0.22 * f.FBM(P * 3.0 + o));
h *= var;
return float4(cov, h, g * var);
"""

# colour, opacity: thick is the deep colour, thin is the light one; a drying liquid gets a darker rim (the coffee ring)
LOOK = r"""
float cov = Fd.x, h = Fd.y;
float3 col = lerp(C2.rgb, C1.rgb, saturate(h * 1.5));
float ring = Par.y * (1.0 - saturate(h * 3.0));
col = lerp(col, C1.rgb * 0.55, saturate(ring * 0.8));
float op = cov * Par.w * lerp(Opac.y, Opac.x, saturate(h * 2.5));
return float4(col, op);
"""

NRM = r"""
float k = Par.x * (1.0 - 0.75 * Par.y);
return normalize(float3(-Fd.z * k, -Fd.w * k, 1.0));
"""

ROUGH = r"""
float r = lerp(R.x, R.y, Par.y);
return saturate(r + (1.0 - saturate(Fd.y * 2.0)) * R.z);
"""

def build_stain():
    m = fresh("M_MessStain")
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)   # a decal: translucent (DBuffer colour, normal, roughness)
    uv = MEL.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1900, -300)
    p2 = MEL.create_material_expression(m, unreal.MaterialExpressionConstantBiasScale, -1700, -300)
    p2.set_editor_property("bias", -0.5); p2.set_editor_property("scale", 2.0)
    MEL.connect_material_expressions(uv, "", p2, "")
    seed = sparam(m, "Seed", 0.37, -1900, -150)
    shape = sparam(m, "Shape", 0.0, -1900, -50)
    asp = sparam(m, "Asp", 1.0, -1900, 50)
    edge = sparam(m, "Edge", 0.09, -1900, 150)
    c1 = vparam(m, "Color", (0.42, 0.02, 0.06, 1), -1900, 300)
    c2 = vparam(m, "Color2", (0.75, 0.12, 0.18, 1), -1900, 500)
    par = vparam(m, "Par", (0.05, 0.0, 0.0, 1.0), -1900, 700)          # height of the relief, dryness 0..1, -, fade 0..1
    opac = vparam(m, "Opac", (1.0, 0.6, 0.0, 0.0), -1900, 900)         # opacity where thick, where thin
    rgh = vparam(m, "Rough", (0.08, 0.45, 0.1, 0.0), -1900, 1100)       # wet, dry, extra at the thin edge

    fd = custom(m, FIELD, unreal.CustomMaterialOutputType.CMOT_FLOAT4, ["P", "Seed", "Shape", "Asp", "Edge", "Spk"], -1300, -200, "MessField")
    MEL.connect_material_expressions(p2, "", fd, "P")
    spk = sparam(m, "Spike", 1.0, -1900, 1300)   # 1 a thin liquid (a spiky crown, thrown drops), 0 a viscous one (round lobes)
    wire(fd, [(seed, "Seed"), (shape, "Shape"), (asp, "Asp"), (edge, "Edge"), (spk, "Spk")])
    look = custom(m, LOOK, unreal.CustomMaterialOutputType.CMOT_FLOAT4, ["Fd", "C1", "C2", "Par", "Opac"], -900, 200, "MessLook")
    MEL.connect_material_expressions(fd, "", look, "Fd")
    wire(look, [(c1, "C1"), (c2, "C2"), (par, "Par"), (opac, "Opac")])
    MEL.connect_material_property(chan(m, look, True, True, True, False, -600, 150), "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(chan(m, look, False, False, False, True, -600, 300), "", unreal.MaterialProperty.MP_OPACITY)
    nrm = custom(m, NRM, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["Fd", "Par"], -900, 500, "MessNormal")
    MEL.connect_material_expressions(fd, "", nrm, "Fd"); wire(nrm, [(par, "Par")])
    MEL.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)
    rf = custom(m, ROUGH, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["Fd", "Par", "R"], -900, 700, "MessRough")
    MEL.connect_material_expressions(fd, "", rf, "Fd"); wire(rf, [(par, "Par"), (rgh, "R")])
    MEL.connect_material_property(rf, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(const(m, 0.5, -600, 800), "", unreal.MaterialProperty.MP_SPECULAR)
    MEL.connect_material_property(const(m, 0.0, -600, 900), "", unreal.MaterialProperty.MP_METALLIC)
    MEL.layout_material_expressions(m)
    MEL.recompile_material(m)
    EAL.save_asset(DIR + "/M_MessStain")
    unreal.log("MESSPY M_MessStain saved")

def build_blob():
    # Glossy blobs: on an instanced mesh the colour comes from per-instance data 0..2 and the roughness from 3; on a plain mesh
    # from the parameters (the defaults of those nodes).
    m = fresh("M_MessBlob")
    col = vparam(m, "Color", (0.5, 0.04, 0.08, 1), -900, -100)
    rough = sparam(m, "Roughness", 0.12, -900, 200)
    MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(const(m, 0.55, -300, 400), "", unreal.MaterialProperty.MP_SPECULAR)
    try:
        m.set_editor_property("used_with_instanced_static_meshes", True)
    except Exception as e:
        unreal.log_warning("MESSPY instanced flag: %s" % e)
    MEL.recompile_material(m)
    EAL.save_asset(DIR + "/M_MessBlob")
    unreal.log("MESSPY M_MessBlob saved")

# ---------------------------------------------------------------------------------------------------------------------
# Dust: a mesh of N quads; every vertex of a quad sits at the quad's own random point P0 (local, in [0, Box]^3), UV0 is the
# corner (-1..1), the vertex colour is the mote's randoms. The shader drifts P0, wraps it into a box round the camera (so a
# mote stays put in the world while the camera moves, and there is always dust round the viewer), and turns the quad to
# face the camera. A mote is never drawn smaller than ~1.5 pixels; one that had to grow fades by the same factor.
# ---------------------------------------------------------------------------------------------------------------------
DUSTPOS = r"""
float ph = VC.g * 6.2832;
float3 turb = float3(sin(T * 0.37 + ph * 3.1), sin(T * 0.29 + ph * 5.3 + 1.7), sin(T * 0.23 + ph * 2.1 + 0.4))
            + 0.35 * float3(sin(T * 1.13 + ph * 7.0), sin(T * 0.91 + ph * 11.0), sin(T * 1.31 + ph * 13.0));
float3 Q = P0 + Wind.xyz * T + turb * Wind.w;
float3 rel = frac((Q - Cam) / Box.xyz + 0.5) - 0.5;
float3 W = Cam + rel * Box.xyz;
if (Band.x > 0.5) { float hgt = max(Band.z - Band.y, 1.0); W.z = Band.y + frac(Q.z / hgt + VC.b) * hgt; }
float3 sd = W - Stir.xyz; float sdl = length(sd);
float push = Stir2.y * exp(-Stir.w * 1.4) * saturate(1.0 - sdl / max(Stir2.x, 1.0)) * (1.0 - exp(-Stir.w * 7.0));
W += (sd / max(sdl, 1.0) * 0.7 + float3(0, 0, 0.9)) * push;
"""

DUSTWPO = DUSTPOS + r"""
float dist = length(W - Cam);
float sz = Box.w * (0.35 + VC.r * VC.r * 1.3);
float px = Pix.x * dist;
float grow = max(1.0, Pix.y * px / sz);
float size = sz * grow;
// a mote right at the lens is faded out anyway: collapse it, or its quad would cover half the screen at zero opacity
if (dist < Pix.z * 1.5) size = 0.0;
size = min(size, 14.0 * px);
float3 right = normalize(cross(CamF, float3(0, 0, 1)) + float3(1e-4, 0, 0));
float3 up = cross(right, CamF);
W += (right * UV.x + up * UV.y) * size;
return W - AWP;
"""

DUSTFADE = DUSTPOS + r"""
float dist = length(W - Cam);
float sz = Box.w * (0.35 + VC.r * VC.r * 1.3);
float px = Pix.x * dist;
float grow = max(1.0, Pix.y * px / sz);
float3 a = abs(rel) * 2.0;
float edge = saturate((1.0 - max(a.x, max(a.y, a.z))) * 5.0);
if (Band.x > 0.5) { float2 a2 = abs(rel.xy) * 2.0; edge = saturate((1.0 - max(a2.x, a2.y)) * 5.0); }
float nearf = saturate((dist - Pix.z) / max(Pix.z, 0.1));
float tw = 0.65 + 0.35 * sin(T * (0.8 + VC.b * 2.6) + ph * 9.0);
float spark = pow(saturate(sin(T * (0.5 + VC.b * 1.9) + ph * 23.0)), 40.0) * Pix.w;
float stir = 1.0 + 2.5 * Stir2.y * exp(-Stir.w * 1.1) * saturate(1.0 - sdl / max(Stir2.x, 1.0)) / max(Stir2.y, 1.0);
// hot spots (fridge, radiator): Spot.w is the radius; outside every spot the layer keeps Wind's share only
float spot = 1.0;
if (Spot.w > 0.0) { spot = saturate(Spot2.x + (1.0 - length((W - Spot.xyz) / float3(Spot.w, Spot.w, Spot.w * 3.0)))); }
float k = edge * nearf * (0.25 + 0.75 * VA) * (tw + spark) * stir * spot / grow;   // a mote grown to stay visible keeps most of its brightness (it is a glint, not a disc)
// The light the mote is in, worked out here and not by the renderer (it is cheaper and it always reads):
// a sun shaft = the mote, traced back along the sun to a window plane, lands inside the window; a lamp cone; a little ambient.
float beam = 0.0;
if (Sun.w > 0.0 && Sun.y > 0.05)
{
    float t1 = (W.y - WinY.x) / Sun.y;
    if (t1 > 0.0) { float3 q = W - Sun.xyz * t1; beam = max(beam, saturate(min(q.x - Win1.x, Win1.y - q.x) / WinY.z) * saturate(min(q.z - Win1.z, Win1.w - q.z) / WinY.z) * saturate(1.0 - t1 / 14000.0)); }
    float t2 = (W.y - WinY.y) / Sun.y;
    if (t2 > 0.0) { float3 q = W - Sun.xyz * t2; beam = max(beam, saturate(min(q.x - Win2.x, Win2.y - q.x) / WinY.z) * saturate(min(q.z - Win2.z, Win2.w - q.z) / WinY.z) * saturate(1.0 - t2 / 14000.0)); }
    beam *= Sun.w;
}
float lamp = 0.0;
if (Lamp.w > 0.0)
{
    float3 L = W - Lamp.xyz; float d = max(length(L), 1.0);
    lamp = Lamp.w * saturate((dot(L / d, LampD.xyz) - LampD.w) / max(1.0 - LampD.w, 0.01) * 2.5) * saturate(1.0 - d / 6000.0);
}
return float4(k, beam, lamp, spark);
"""

DUSTSPRITE = r"""
float r = length(UV);
float c = saturate(1.0 - r);
return c * c * (3.0 - 2.0 * c);
"""

def build_dust(lit):
    name = "M_MessDust" if lit else "M_MessDustUnlit"
    m = fresh(name)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT if lit else unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT if lit else unreal.MaterialShadingModel.MSM_UNLIT)
    if lit:
        try:
            m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_PER_VERTEX_NON_DIRECTIONAL)
        except Exception as e:
            unreal.log_warning("MESSPY dust lighting mode: %s" % e)
    m.set_editor_property("two_sided", True)
    p0 = MEL.create_material_expression(m, unreal.MaterialExpressionLocalPosition, -1900, -500)
    awp = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1900, -400)
    awp.set_editor_property("world_position_shader_offset", unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    uv = MEL.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1900, -300)
    vc = MEL.create_material_expression(m, unreal.MaterialExpressionVertexColor, -1900, -200)
    cam = MEL.create_material_expression(m, unreal.MaterialExpressionCameraPositionWS, -1900, -100)
    camf = MEL.create_material_expression(m, unreal.MaterialExpressionCameraVectorWS, -1900, 0)
    # the camera's forward: -CameraVector is per vertex, so use the view's own forward (view -> world of (0,0,1))
    fwd = MEL.create_material_expression(m, unreal.MaterialExpressionTransform, -1700, 0)
    fwd.set_editor_property("transform_source_type", unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_VIEW)
    fwd.set_editor_property("transform_type", unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    zc = MEL.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -1900, 80)
    zc.set_editor_property("constant", unreal.LinearColor(0, 0, 1, 0))
    MEL.connect_material_expressions(zc, "", fwd, "")
    tm = MEL.create_material_expression(m, unreal.MaterialExpressionTime, -1900, 150)
    box = vparam(m, "Box", (900, 900, 900, 0.5), -1900, 250)           # xyz: the box round the camera, w: mote size
    wind = vparam(m, "Wind", (6, 3, 1.5, 25), -1900, 450)              # xyz: drift uu/s, w: turbulence uu
    band = vparam(m, "Band", (0, 0, 120, 0), -1900, 650)               # x 1: a layer between z = y and z (floor-level fibres)
    stir = vparam(m, "Stir", (0, 0, -1e6, 100), -1900, 850)            # xyz the last stir, w seconds since
    stir2 = vparam(m, "Stir2", (600, 0, 0, 0), -1900, 1050)            # x radius, y strength (uu)
    pix = vparam(m, "Pix", (0.0011, 1.5, 4.0, 1.0), -1900, 1250)       # x world size of a pixel per uu of distance, y min pixels, z near fade, w sparkle
    spot = vparam(m, "Spot", (0, 0, 0, 0), -1900, 1450)
    spot2 = vparam(m, "Spot2", (1, 0, 0, 0), -1900, 1650)
    col = vparam(m, "Color", (0.9, 0.88, 0.82, 1), -1900, 1850)
    inten = sparam(m, "Intensity", 1.0, -1900, 2050)

    common = [(p0, "P0"), (vc, "VC"), (cam, "Cam"), (tm, "T"), (box, "Box"), (wind, "Wind"), (band, "Band"), (stir, "Stir"), (stir2, "Stir2"), (pix, "Pix")]
    w = custom(m, DUSTWPO, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["P0", "VC", "Cam", "T", "Box", "Wind", "Band", "Stir", "Stir2", "Pix", "UV", "CamF", "AWP"], -1200, -300, "DustWPO")
    wire(w, common + [(uv, "UV"), (fwd, "CamF"), (awp, "AWP")])
    MEL.connect_material_property(w, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    fd = custom(m, DUSTFADE, unreal.CustomMaterialOutputType.CMOT_FLOAT4, ["P0", "VC", "Cam", "T", "Box", "Wind", "Band", "Stir", "Stir2", "Pix", "Spot", "Spot2", "VA", "Sun", "Win1", "Win2", "WinY", "Lamp", "LampD"], -1200, 300, "DustFade")
    sun = vparam(m, "Sun", (0, 1, -0.4, 0), -2200, 300)             # xyz: the way sunlight travels, w: shaft strength
    win1 = vparam(m, "Win1", (-1500, 1500, 2200, 4200), -2200, 450)  # window 1: x from, to, z from, to
    win2 = vparam(m, "Win2", (16785, 19785, 2200, 4200), -2200, 600)
    winy = vparam(m, "WinY", (-5000, -9000, 60, 0.12), -2200, 750)   # x, y: the windows' wall planes (y), z: soft edge, w: ambient share
    lampp = vparam(m, "Lamp", (0, 0, -1e6, 0), -2200, 900)          # xyz: a spot lamp, w: its strength
    lampd = vparam(m, "LampD", (0, 0, -1, 0.7), -2200, 1050)        # xyz: its axis, w: cos of its outer cone
    wire(fd, common + [(spot, "Spot"), (spot2, "Spot2"), (sun, "Sun"), (win1, "Win1"), (win2, "Win2"), (winy, "WinY"), (lampp, "Lamp"), (lampd, "LampD")])
    MEL.connect_material_expressions(vc, "A", fd, "VA")
    vi = MEL.create_material_expression(m, unreal.MaterialExpressionVertexInterpolator, -900, 300)
    MEL.connect_material_expressions(fd, "", vi, "")
    k = chan(m, vi, True, False, False, False, -700, 300)
    spr = custom(m, DUSTSPRITE, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["UV"], -900, 500, "DustSprite")
    MEL.connect_material_expressions(uv, "", spr, "UV")
    a = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -500, 400)
    MEL.connect_material_expressions(k, "", a, "A"); MEL.connect_material_expressions(spr, "", a, "B")
    a2 = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -350, 400)
    MEL.connect_material_expressions(a, "", a2, "A"); MEL.connect_material_expressions(inten, "", a2, "B")
    if lit:
        sat = MEL.create_material_expression(m, unreal.MaterialExpressionSaturate, -200, 400)
        MEL.connect_material_expressions(a2, "", sat, "")
        MEL.connect_material_property(sat, "", unreal.MaterialProperty.MP_OPACITY)
        MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        sunc = vparam(m, "SunColor", (1, 0.9, 0.75, 1), -900, 700)
        lampc = vparam(m, "LampColor", (1, 0.76, 0.48, 1), -900, 850)
        ambc = vparam(m, "AmbColor", (1, 0.95, 0.9, 1), -900, 1000)
        LIGHT = r"""
float3 c = Col.rgb * (Ln.y * SunC.rgb + Ln.z * LampC.rgb + WinY.w * AmbC.rgb);
c += Ln.w * Ln.y * SunC.rgb * 3.0;   // glints only where the sun is
return c * K;
"""
        li = custom(m, LIGHT, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["Col", "Ln", "SunC", "LampC", "AmbC", "WinY", "K"], -150, 300, "DustLight")
        wire(li, [(col, "Col"), (sunc, "SunC"), (lampc, "LampC"), (ambc, "AmbC"), (winy, "WinY")])
        MEL.connect_material_expressions(vi, "", li, "Ln")
        MEL.connect_material_expressions(a2, "", li, "K")
        MEL.connect_material_property(li, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.layout_material_expressions(m)
    MEL.recompile_material(m)
    EAL.save_asset(DIR + "/" + name)
    unreal.log("MESSPY %s saved" % name)

# A droplet / pressed crumb on a plane: a masked disc with a lumpy edge and a domed normal (a bead of jam catches the light
# like a bead). Per instance: 0..2 colour, 3 roughness, 4 dome (0 a flat smudge, 1 a round bead).
DROP = r"""
float2 p = UV * 2.0 - 1.0;
float a = atan2(p.y, p.x);
float sd = Rnd * 91.7;
float wob = 0.12 * sin(a * 3.0 + sd) + 0.07 * sin(a * 5.0 + sd * 1.7) + 0.05 * sin(a * 9.0 + sd * 2.3);
float r = length(p) / (0.82 + wob * (0.6 + 0.4 * Dome));
float h = sqrt(saturate(1.0 - r * r));
float k = lerp(0.35, 1.6, Dome);
float3 n = normalize(float3(p.x * k * (0.4 + 0.6 * (1.0 - h)), p.y * k * (0.4 + 0.6 * (1.0 - h)), 1.0));
return float4(n, 1.0 - r);
"""

def build_drop():
    m = fresh("M_MessDrop")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("opacity_mask_clip_value", 0.0)
    try:
        m.set_editor_property("used_with_instanced_static_meshes", True)
    except Exception as e:
        unreal.log_warning("MESSPY instanced flag: %s" % e)
    col = vparam(m, "Color", (0.5, 0.04, 0.08, 1), -900, -200)
    rough = sparam(m, "Roughness", 0.1, -900, 0)
    dome = sparam(m, "Dome", 0.7, -900, 100)
    pc = MEL.create_material_expression(m, unreal.MaterialExpressionPerInstanceCustomData3Vector, -600, -200)
    pc.set_editor_property("data_index", 0)
    MEL.connect_material_expressions(col, "", pc, "DefaultValue")
    pr = MEL.create_material_expression(m, unreal.MaterialExpressionPerInstanceCustomData, -600, 0)
    pr.set_editor_property("data_index", 3)
    MEL.connect_material_expressions(rough, "", pr, "DefaultValue")
    pd = MEL.create_material_expression(m, unreal.MaterialExpressionPerInstanceCustomData, -600, 100)
    pd.set_editor_property("data_index", 4)
    MEL.connect_material_expressions(dome, "", pd, "DefaultValue")
    # custom data read in the vertex shader, carried to the pixel shader
    vi = MEL.create_material_expression(m, unreal.MaterialExpressionVertexInterpolator, -400, 100)
    MEL.connect_material_expressions(pd, "", vi, "")
    uv = MEL.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -600, 300)
    rnd = MEL.create_material_expression(m, unreal.MaterialExpressionPerInstanceRandom, -600, 400)
    d = custom(m, DROP, unreal.CustomMaterialOutputType.CMOT_FLOAT4, ["UV", "Rnd", "Dome"], -200, 200, "MessDrop")
    MEL.connect_material_expressions(uv, "", d, "UV")
    MEL.connect_material_expressions(rnd, "", d, "Rnd")
    MEL.connect_material_expressions(vi, "", d, "Dome")
    MEL.connect_material_property(chan(m, d, True, True, True, False, 100, 150), "", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(chan(m, d, False, False, False, True, 100, 300), "", unreal.MaterialProperty.MP_OPACITY_MASK)
    vc = MEL.create_material_expression(m, unreal.MaterialExpressionVertexInterpolator, -400, -200)
    MEL.connect_material_expressions(pc, "", vc, "")
    MEL.connect_material_property(vc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    vr = MEL.create_material_expression(m, unreal.MaterialExpressionVertexInterpolator, -400, 0)
    MEL.connect_material_expressions(pr, "", vr, "")
    MEL.connect_material_property(vr, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(const(m, 0.55, 100, 450), "", unreal.MaterialProperty.MP_SPECULAR)
    MEL.layout_material_expressions(m)
    MEL.recompile_material(m)
    EAL.save_asset(DIR + "/M_MessDrop")
    unreal.log("MESSPY M_MessDrop saved")

import os
what = os.environ.get("ANT_MESS_MAKE", "stain,blob,drop,dust,dustunlit").split(",")
if "stain" in what: build_stain()
if "blob" in what: build_blob()
if "drop" in what: build_drop()
if "dust" in what: build_dust(True)
if "dustunlit" in what: build_dust(False)
unreal.log("MESSPY done")
