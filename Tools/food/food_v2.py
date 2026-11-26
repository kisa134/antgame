# Food V2 (KAN-36): every food of AntFood's table (Source/AntKitchen/AntFood.cpp) as a hero asset, the way
# Saved/hero_mop_bucket.py makes the mop: real geometry (the inner cut: cake layers and cream, cheese holes, fruit flesh
# and seeds), procedural node materials with the visible states, Cycles, warm area lights, AgX.
#
# Per food <Id> it writes (SourceArt/FoodV2/<Id>/):
#   SM_FoodV2_<Id>.fbx            the whole food, real size in metres (imported at 1 m = 1900 uu), centred, long side along X,
#                                 one material slot "Outer" (UV atlas)
#   SM_FoodV2_<Id>_P<k>.fbx       the pre-cut break pieces of its kind (crumbly: chunks, brittle: two halves, elastic: torn ...),
#                                 slots "Outer" (same atlas) and "Inner" (the cut faces: the inner strip texture)
#   T_FoodV2_<Id>_C/N/R.png       colour (sRGB), normal (OpenGL, flipped green in the importer), AO / roughness / 0
#   T_FoodV2_<Id>_Toast/Burnt/Spoil/Frozen.png   colour of that state, roughness in alpha
#   T_FoodV2_<Id>_InC/InN.png     the inner strip: U along the cut, V = height (or distance from the core) of the food
#   food.json                     size, pieces, offsets, material parameters for the UE importer (Tools/food/import_food_v2.py)
# and into Saved/food_review/: <Id>_before_after.jpg (the game's current model next to the new one) and <Id>_states.jpg.
#
# Run: blender --background --factory-startup --python Tools/food/food_v2.py -- <project dir> <mode> <Id,Id,...|all> [samples]
#   mode: model (export + bake), review (renders), all
# Nothing from the internet: every shape and texture is made here.
import bpy, bmesh, math, random, sys, os, json
from mathutils import Vector, Matrix, Euler

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
PROJ = argv[0] if argv else os.getcwd()
MODE = argv[1] if len(argv) > 1 else "all"
WHICH = argv[2] if len(argv) > 2 else "Cake"
SAMPLES = int(argv[3]) if len(argv) > 3 else 96
OUT_ART = os.path.join(PROJ, "SourceArt", "FoodV2")
OUT_REV = os.path.join(PROJ, "Saved", "food_review")
OLD_ROOT = os.path.join(os.path.dirname(PROJ), "AntKitchen")          # the owner's Meshy sources live in the main checkout
TEX = 1024
STEX = 512
os.makedirs(OUT_ART, exist_ok=True); os.makedirs(OUT_REV, exist_ok=True)

def log(*a):
    print("FOODV2", *a, flush=True)

S = None
def reset():
    global S
    bpy.ops.wm.read_factory_settings(use_empty=True)
    S = bpy.context.scene
    S.unit_settings.system = "METRIC"

def link(o):
    S.collection.objects.link(o); return o

def N(m, t, x=0, y=0):
    n = m.node_tree.nodes.new(t); n.location = (x, y); return n

def L(m, a, b):
    m.node_tree.links.new(a, b)

def active(o):
    for x in S.objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o

def apply_mods(o):
    active(o)
    for md in list(o.modifiers):
        try: bpy.ops.object.modifier_apply(modifier=md.name)
        except Exception as e: log("modifier", md.name, "on", o.name, "failed", e); o.modifiers.remove(md)

def mesh_obj(name, bm):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    return link(bpy.data.objects.new(name, me))

def smooth(o, sub=2, bev=None, seg=3):
    for p in o.data.polygons: p.use_smooth = True
    if bev:
        b = o.modifiers.new("bev", "BEVEL"); b.width = bev; b.segments = seg; b.limit_method = "ANGLE"
    if sub:
        s = o.modifiers.new("sub", "SUBSURF"); s.levels = sub; s.render_levels = sub
    return o

def displace(o, tex_type="CLOUDS", size=0.004, strength=0.0006, mid=0.5, depth=2, vg=None):
    t = bpy.data.textures.new(o.name + "_d", tex_type)
    if hasattr(t, "noise_scale"): t.noise_scale = size
    if hasattr(t, "noise_depth"): t.noise_depth = depth
    d = o.modifiers.new("disp", "DISPLACE"); d.texture = t; d.strength = strength; d.mid_level = mid; d.texture_coords = "OBJECT"
    if vg: d.vertex_group = vg
    return d

def uvsphere(name, r, loc=(0, 0, 0), seg=32, ring=16, scale=(1, 1, 1)):
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=ring, radius=r)
    for v in bm.verts: v.co = Vector((v.co.x * scale[0], v.co.y * scale[1], v.co.z * scale[2])) + Vector(loc)
    return mesh_obj(name, bm)

def cube(name, size, loc=(0, 0, 0)):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts: v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2])) + Vector(loc)
    return mesh_obj(name, bm)

def cyl(name, r, h, loc=(0, 0, 0), seg=48, rot=(0, 0, 0)):
    bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=seg, radius1=r, radius2=r, depth=h)
    o = mesh_obj(name, bm); o.location = loc; o.rotation_euler = rot
    return o

def prism(name, poly, z0, z1):
    """a closed prism from a 2D polygon (counter-clockwise) between heights z0 and z1"""
    bm = bmesh.new()
    lo = [bm.verts.new((x, y, z0)) for x, y in poly]
    hi = [bm.verts.new((x, y, z1)) for x, y in poly]
    n = len(poly)
    bm.faces.new(list(reversed(lo))); bm.faces.new(hi)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return mesh_obj(name, bm)

def screw(name, prof, steps=64, sub=1):
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    vs = [bm.verts.new((r, 0, z)) for r, z in prof]
    for a, b in zip(vs, vs[1:]): bm.edges.new((a, b))
    bm.to_mesh(me); bm.free()
    o = link(bpy.data.objects.new(name, me))
    m = o.modifiers.new("screw", "SCREW"); m.steps = steps; m.render_steps = steps; m.use_merge_vertices = True; m.use_smooth_shade = True; m.use_normal_calculate = True
    if sub:
        s = o.modifiers.new("sub", "SUBSURF"); s.levels = sub; s.render_levels = sub
    return o

def tube(name, pts, r, res=12, caps=True, radii=None):
    cu = bpy.data.curves.new(name, "CURVE"); cu.dimensions = "3D"; cu.bevel_depth = r; cu.bevel_resolution = max(2, res // 4); cu.resolution_u = res; cu.use_fill_caps = caps
    sp = cu.splines.new("NURBS"); sp.points.add(len(pts) - 1)
    for i, (p, c) in enumerate(zip(sp.points, pts)):
        p.co = (c[0], c[1], c[2], 1)
        if radii: p.radius = radii[i]
    sp.use_endpoint_u = True; sp.order_u = min(4, len(pts))
    o = link(bpy.data.objects.new(name, cu))
    active(o); bpy.ops.object.convert(target="MESH")
    return bpy.context.view_layer.objects.active

def metaball_mesh(name, balls, res=0.0012, thresh=0.6):
    """balls: (kind 'BALL'|'ELLIPSOID'|'CAPSULE', loc, radius, size(x,y,z), rot euler)"""
    mb = bpy.data.metaballs.new(name); mb.resolution = res; mb.render_resolution = res; mb.threshold = thresh
    for kind, loc, r, size, rot in balls:
        e = mb.elements.new(); e.type = kind; e.co = loc; e.radius = r
        if kind in ("ELLIPSOID", "CAPSULE"):
            e.size_x, e.size_y, e.size_z = size
        e.rotation = Euler(rot).to_quaternion()
    o = link(bpy.data.objects.new(name, mb))
    bpy.context.view_layer.update()
    active(o); bpy.ops.object.convert(target="MESH")
    m = bpy.context.view_layer.objects.active; m.name = name
    return m

def join(objs, name):
    objs = [o for o in objs if o]
    for o in objs:
        if o.type != "MESH":
            active(o); bpy.ops.object.convert(target="MESH")
    for o in objs: apply_mods(o)
    for x in S.objects: x.select_set(False)
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active; o.name = name
    return o

def dup(o, name=None):
    c = o.copy(); c.data = o.data.copy(); c.name = name or (o.name + "_c"); link(c)
    return c

def bounds(objs):
    mn = Vector((1e9, 1e9, 1e9)); mx = Vector((-1e9, -1e9, -1e9))
    bpy.context.view_layer.update()
    for o in objs:
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            mn = Vector((min(mn[i], w[i]) for i in range(3))); mx = Vector((max(mx[i], w[i]) for i in range(3)))
    return mn, mx

# ================================================================ node helpers
class NT:
    """small helper over a node tree (material or group)"""
    def __init__(s, tree): s.t = tree
    def n(s, typ, **kw):
        node = s.t.nodes.new(typ)
        for k, v in kw.items(): setattr(node, k, v)
        return node
    def set(s, inp, v):
        if isinstance(v, bpy.types.NodeSocket): s.t.links.new(v, inp)
        elif isinstance(v, (tuple, list)): inp.default_value = tuple(v) + ((1.0,) if len(v) == 3 and len(inp.default_value) == 4 else ())
        else: inp.default_value = v
    def math(s, op, a, b=0.0, c=0.0, clamp=False):
        m = s.n("ShaderNodeMath", operation=op, use_clamp=clamp)
        s.set(m.inputs[0], a); s.set(m.inputs[1], b)
        if len(m.inputs) > 2: s.set(m.inputs[2], c)
        return m.outputs[0]
    def mul(s, a, b, clamp=False): return s.math("MULTIPLY", a, b, clamp=clamp)
    def add(s, a, b, clamp=False): return s.math("ADD", a, b, clamp=clamp)
    def sub(s, a, b, clamp=False): return s.math("SUBTRACT", a, b, clamp=clamp)
    def mapr(s, v, a, b, c=0.0, d=1.0, clamp=True, smooth=False):
        m = s.n("ShaderNodeMapRange", clamp=clamp)
        if smooth: m.interpolation_type = "SMOOTHSTEP"
        s.set(m.inputs["Value"], v); s.set(m.inputs["From Min"], a); s.set(m.inputs["From Max"], b); s.set(m.inputs["To Min"], c); s.set(m.inputs["To Max"], d)
        return m.outputs["Result"]
    def mixc(s, fac, a, b, blend="MIX"):
        m = s.n("ShaderNodeMix", data_type="RGBA", blend_type=blend)
        s.set(m.inputs[0], fac); s.set(m.inputs[6], a); s.set(m.inputs[7], b)
        return m.outputs[2]
    def mixf(s, fac, a, b):
        m = s.n("ShaderNodeMix", data_type="FLOAT")
        s.set(m.inputs[0], fac); s.set(m.inputs[2], a); s.set(m.inputs[3], b)
        return m.outputs[0]
    def rgb(s, c):
        r = s.n("ShaderNodeRGB"); r.outputs[0].default_value = (c[0], c[1], c[2], 1.0); return r.outputs[0]
    def noise(s, vec, scale, detail=4.0, rough=0.55, dist=0.0, out="Fac", dim="3D"):
        t = s.n("ShaderNodeTexNoise"); t.noise_dimensions = dim
        s.set(t.inputs["Vector"], vec); t.inputs["Scale"].default_value = scale; t.inputs["Detail"].default_value = detail
        t.inputs["Roughness"].default_value = rough; t.inputs["Distortion"].default_value = dist
        return t.outputs[out]
    def voronoi(s, vec, scale, feature="F1", out="Distance", rand=1.0):
        t = s.n("ShaderNodeTexVoronoi", feature=feature)
        s.set(t.inputs["Vector"], vec); t.inputs["Scale"].default_value = scale; t.inputs["Randomness"].default_value = rand
        return t.outputs[out]
    def wave(s, vec, scale, kind="BANDS", dist=2.0, direction="X"):
        t = s.n("ShaderNodeTexWave", wave_type=kind); t.bands_direction = direction
        s.set(t.inputs["Vector"], vec); t.inputs["Scale"].default_value = scale; t.inputs["Distortion"].default_value = dist
        return t.outputs["Fac"]
    def ramp(s, v, stops):
        r = s.n("ShaderNodeValToRGB"); e = r.color_ramp.elements
        while len(e) < len(stops): e.new(0.5)
        for el, (pos, col) in zip(e, stops): el.position = pos; el.color = (col[0], col[1], col[2], 1.0)
        s.set(r.inputs[0], v); return r.outputs[0]
    def sep(s, vec):
        x = s.n("ShaderNodeSeparateXYZ"); s.set(x.inputs[0], vec); return x.outputs
    def vmath(s, op, a, b=(0, 0, 0), out=0):
        m = s.n("ShaderNodeVectorMath", operation=op); s.set(m.inputs[0], a); s.set(m.inputs[1], b)
        return m.outputs[out]
    def hsv(s, col, h=0.5, sat=1.0, val=1.0):
        m = s.n("ShaderNodeHueSaturation"); s.set(m.inputs["Hue"], h); s.set(m.inputs["Saturation"], sat); s.set(m.inputs["Value"], val); s.set(m.inputs["Color"], col)
        return m.outputs[0]

STATES = ("Toast", "Burn", "Soak", "Frozen", "Dirt", "Spoil")

def states_group():
    """the visible states of food, one group every food material goes through (the game's M_FoodV2 does the same in HLSL)"""
    g = bpy.data.node_groups.get("FoodStates")
    if g: return g
    g = bpy.data.node_groups.new("FoodStates", "ShaderNodeTree")
    I = g.interface
    I.new_socket("Color", in_out="INPUT", socket_type="NodeSocketColor")
    I.new_socket("Rough", in_out="INPUT", socket_type="NodeSocketFloat")
    I.new_socket("Height", in_out="INPUT", socket_type="NodeSocketFloat")
    I.new_socket("Vector", in_out="INPUT", socket_type="NodeSocketVector")
    for k in STATES: I.new_socket(k, in_out="INPUT", socket_type="NodeSocketFloat")
    I.new_socket("Color", in_out="OUTPUT", socket_type="NodeSocketColor")
    I.new_socket("Rough", in_out="OUTPUT", socket_type="NodeSocketFloat")
    I.new_socket("Height", in_out="OUTPUT", socket_type="NodeSocketFloat")
    t = NT(g)
    gi = t.n("NodeGroupInput"); go = t.n("NodeGroupOutput")
    v = gi.outputs["Vector"]; col = gi.outputs["Color"]; rough = gi.outputs["Rough"]; h = gi.outputs["Height"]
    toast, burn, soak, frozen, dirt, spoil = (gi.outputs[k] for k in STATES)
    n1 = t.noise(v, 60.0, 6.0); n2 = t.noise(v, 260.0, 4.0)
    # toasted: golden brown, unevenly
    tf = t.mul(toast, t.mapr(n1, 0.3, 0.7, 0.55, 1.0))
    col = t.mixc(tf, col, t.mixc(0.45, t.mixc(1.0, col, (0.78, 0.50, 0.24), "MULTIPLY"), (0.55, 0.30, 0.10)))
    rough = t.mixf(t.mul(toast, 0.5), rough, 0.62)
    # burnt: darker all over, charred black patches, crusty
    col = t.mixc(t.mul(burn, 0.75), col, t.mixc(1.0, col, (0.30, 0.18, 0.10), "MULTIPLY"))
    bm = t.mapr(t.add(t.mul(n1, 0.7), t.mul(n2, 0.3)), t.sub(1.05, t.mul(burn, 0.48)), t.sub(1.2, t.mul(burn, 0.48)), 0.0, 1.0)
    col = t.mixc(bm, col, (0.022, 0.018, 0.016))
    rough = t.mixf(bm, rough, 0.93)
    h = t.add(h, t.mul(bm, t.sub(n2, 0.5)))
    # soaked: dark and glossy, a little swollen
    col = t.mixc(t.mul(soak, 0.6), col, t.mixc(1.0, col, (0.58, 0.56, 0.54), "MULTIPLY"))
    rough = t.mixf(soak, rough, 0.16)
    # frozen: frost, white-blue, crystals
    fm = t.mul(frozen, t.mapr(n2, 0.25, 0.7, 0.45, 1.0))
    col = t.mixc(t.mul(fm, 0.7), col, (0.86, 0.93, 1.0))
    rough = t.mixf(frozen, rough, 0.5)
    h = t.add(h, t.mul(t.mul(frozen, 0.6), t.voronoi(v, 900.0, "F1")))
    # dirty: a grey film and grit specks
    dm = t.mapr(t.noise(v, 520.0, 3.0), t.sub(1.0, t.mul(dirt, 0.36)), t.sub(1.04, t.mul(dirt, 0.36)), 0.0, 1.0)      # grit specks
    sm = t.mul(dirt, t.mapr(n1, 0.45, 0.7, 0.0, 0.55))                                                                     # grey smudges
    col = t.mixc(t.mul(dirt, 0.15), col, t.mixc(1.0, col, (0.70, 0.66, 0.58), "MULTIPLY"))
    col = t.mixc(sm, col, t.mixc(1.0, col, (0.45, 0.40, 0.34), "MULTIPLY"))
    col = t.mixc(dm, col, (0.16, 0.13, 0.10))
    rough = t.mixf(t.mul(dirt, 0.6), rough, 0.86)
    h = t.add(h, t.mul(dm, 0.3))
    # spoiled: dull, greyer and greener, mould spots
    dull = t.mixc(1.0, t.hsv(col, 0.5, 0.45, 0.8), (0.82, 0.86, 0.66), "MULTIPLY")
    col = t.mixc(t.mul(spoil, 0.7), col, dull)
    sc = t.n("ShaderNodeVectorMath", operation="SCALE")
    t.set(sc.inputs[0], t.vmath("SUBTRACT", t.noise(v, 90.0, 4.0, out="Color"), (0.5, 0.5, 0.5))); sc.inputs["Scale"].default_value = 0.012
    vd = t.vmath("ADD", v, sc.outputs[0])                                                         # wobbly coordinates: no round dots
    vo = t.mul(t.voronoi(vd, 120.0, "F1"), t.mapr(t.noise(v, 30.0, 2.0), 0.3, 0.7, 1.6, 0.7))   # irregular colonies, some bigger
    spot = t.mul(t.mapr(vo, t.mul(spoil, 0.30), t.add(t.mul(spoil, 0.30), 0.07), 1.0, 0.0), t.mapr(spoil, 0.12, 0.3, 0.0, 1.0))
    ring = t.mul(t.mapr(vo, t.add(t.mul(spoil, 0.30), 0.03), t.add(t.mul(spoil, 0.30), 0.14), 1.0, 0.0), t.mapr(spoil, 0.12, 0.3, 0.0, 0.6))
    col = t.mixc(ring, col, t.mixc(1.0, col, (0.45, 0.42, 0.30), "MULTIPLY"))
    col = t.mixc(spot, col, t.mixc(t.noise(v, 900.0, 2.0), (0.86, 0.88, 0.80), (0.55, 0.66, 0.42)))
    rough = t.mixf(spot, rough, 0.97)
    h = t.add(h, t.mul(spot, t.mul(t.noise(v, 1200.0, 2.0), 1.2)))
    t.set(go.inputs["Color"], col); t.set(go.inputs["Rough"], rough); t.set(go.inputs["Height"], h)
    return g

def food_mat(name, build, sss=0.0, sss_rad=(1.0, 0.45, 0.25), sss_scale=0.004, coat=0.0, coat_rough=0.1, trans=0.0, ior=1.45,
             bump=0.0005, bump_str=0.6, sheen=0.0, spec=0.5, alpha=1.0):
    """a food material: build(t, vec) -> (colour, roughness, height) sockets; the states group on top; Value nodes S_<state>"""
    m = bpy.data.materials.new(name); m.use_nodes = True
    p = m.node_tree.nodes["Principled BSDF"]
    t = NT(m.node_tree)
    tc = t.n("ShaderNodeTexCoord"); vec = tc.outputs["Object"]
    col, rough, h = build(t, vec)
    gn = t.n("ShaderNodeGroup"); gn.node_tree = states_group()
    t.set(gn.inputs["Color"], col); t.set(gn.inputs["Rough"], rough); t.set(gn.inputs["Height"], h); t.set(gn.inputs["Vector"], vec)
    for k in STATES:
        vn = t.n("ShaderNodeValue"); vn.name = "S_" + k; vn.label = k; vn.outputs[0].default_value = 0.0
        t.set(gn.inputs[k], vn.outputs[0])
    t.set(p.inputs["Base Color"], gn.outputs["Color"]); t.set(p.inputs["Roughness"], gn.outputs["Rough"])
    bp = t.n("ShaderNodeBump"); bp.inputs["Strength"].default_value = bump_str; bp.inputs["Distance"].default_value = bump
    t.set(bp.inputs["Height"], gn.outputs["Height"]); t.set(p.inputs["Normal"], bp.outputs["Normal"])
    if sss > 0:
        p.inputs["Subsurface Weight"].default_value = sss; p.inputs["Subsurface Radius"].default_value = sss_rad; p.inputs["Subsurface Scale"].default_value = sss_scale
        t.set(p.inputs["Subsurface Weight"], sss)
    p.inputs["Coat Weight"].default_value = coat; p.inputs["Coat Roughness"].default_value = coat_rough
    p.inputs["Transmission Weight"].default_value = trans; p.inputs["IOR"].default_value = ior
    p.inputs["Sheen Weight"].default_value = sheen
    p.inputs["Specular IOR Level"].default_value = spec
    m["sss"] = sss; m["coat"] = coat; m["trans"] = trans
    return m

def set_state(objs, **kw):
    for o in objs:
        for slot in o.material_slots:
            m = slot.material
            if not m or not m.use_nodes: continue
            for k, v in kw.items():
                n = m.node_tree.nodes.get("S_" + k)
                if n: n.outputs[0].default_value = v

def unique_mats(o):
    """give the object its own copies of its materials (so its states can differ from the other copies)"""
    for slot in o.material_slots:
        if slot.material: slot.material = slot.material.copy()

def assign(o, m):
    o.data.materials.clear(); o.data.materials.append(m)

# ================================================================ stage (the reference's light: warm key like an evening window, cool fill, top)
def stage_mats():
    tiles = bpy.data.materials.new("Tiles"); tiles.use_nodes = True; p = tiles.node_tree.nodes["Principled BSDF"]; t = NT(tiles.node_tree)
    tc = t.n("ShaderNodeTexCoord"); br = t.n("ShaderNodeTexBrick"); br.offset = 0
    br.inputs["Scale"].default_value = 3.3; br.inputs["Mortar Size"].default_value = 0.012; br.inputs["Brick Width"].default_value = 1.0; br.inputs["Row Height"].default_value = 1.0
    br.inputs["Color1"].default_value = (0.74, 0.70, 0.62, 1); br.inputs["Color2"].default_value = (0.70, 0.66, 0.58, 1); br.inputs["Mortar"].default_value = (0.36, 0.33, 0.29, 1)
    t.set(br.inputs["Vector"], tc.outputs["Object"])
    grit = t.noise(tc.outputs["Object"], 900.0, 6.0)
    t.set(p.inputs["Base Color"], t.mixc(t.mapr(grit, 0.45, 0.75, 0.0, 0.25), br.outputs["Color"], (0.5, 0.46, 0.4)))
    t.set(p.inputs["Roughness"], t.mapr(br.outputs["Fac"], 0.0, 1.0, 0.18, 0.7))
    bp = t.n("ShaderNodeBump"); bp.invert = True; bp.inputs["Strength"].default_value = 0.6; bp.inputs["Distance"].default_value = 0.002
    t.set(bp.inputs["Height"], t.add(br.outputs["Fac"], t.mul(grit, 0.05))); t.set(p.inputs["Normal"], bp.outputs["Normal"])
    wall = bpy.data.materials.new("Wall"); wall.use_nodes = True; pw = wall.node_tree.nodes["Principled BSDF"]
    pw.inputs["Base Color"].default_value = (0.80, 0.76, 0.66, 1); pw.inputs["Roughness"].default_value = 0.85
    return tiles, wall

def stage(span=0.3):
    tiles, wall = stage_mats()
    bpy.ops.mesh.primitive_plane_add(size=12, location=(0, 0, 0)); f = bpy.context.object; f.data.materials.append(tiles)
    bpy.ops.mesh.primitive_plane_add(size=12, location=(0, span * 1.4, 3), rotation=(math.pi / 2, 0, 0)); w = bpy.context.object; w.data.materials.append(wall)
    wd = bpy.data.worlds.new("W"); S.world = wd; wd.use_nodes = True; bg = wd.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = (0.95, 0.80, 0.62, 1); bg.inputs["Strength"].default_value = 0.10
    def area(loc, rot, size, power, col):
        l = bpy.data.lights.new("a", "AREA"); l.size = size; l.energy = power; l.color = col
        o = link(bpy.data.objects.new("a", l)); o.location = loc; o.rotation_euler = [math.radians(v) for v in rot]
    k = max(0.25, span / 0.6)   # the lights scale with the subject (a 1 cm pea and a 9 cm cake get the same look)
    area((-1.6 * k, -1.8 * k, 2.0 * k), (52, 0, -42), 1.6 * k, 95 * k * k, (1.0, 0.84, 0.64))
    area((1.9 * k, -1.2 * k, 0.9 * k), (72, 0, 58), 1.2 * k, 16 * k * k, (0.70, 0.82, 1.0))
    area((0.3 * k, 0.2 * k, 2.4 * k), (0, 0, 0), 1.0 * k, 12 * k * k, (1.0, 0.95, 0.9))

def camera(loc, look, lens, fstop=8.0):
    c = bpy.data.cameras.new("c"); c.lens = lens; c.dof.use_dof = True; c.dof.aperture_fstop = fstop; c.clip_start = 0.001
    o = link(bpy.data.objects.new("c", c)); o.location = loc
    d = Vector(look) - Vector(loc); o.rotation_euler = d.to_track_quat("-Z", "Y").to_euler(); c.dof.focus_distance = d.length; S.camera = o
    return o

def gpu():
    S.render.engine = "CYCLES"
    pr = bpy.context.preferences.addons["cycles"].preferences
    try:
        pr.compute_device_type = "OPTIX"; pr.get_devices()
        for d in pr.devices: d.use = d.type != "CPU"
        S.cycles.device = "GPU"
    except Exception as e:
        log("GPU off", e)

def render(path, w=1600, h=900, samples=None):
    gpu()
    S.cycles.samples = samples or SAMPLES; S.cycles.use_denoising = True; S.render.resolution_x = w; S.render.resolution_y = h
    S.view_settings.view_transform = "AgX"; S.view_settings.look = "AgX - Medium High Contrast"
    S.render.image_settings.file_format = "JPEG"; S.render.image_settings.quality = 92; S.render.filepath = path
    bpy.ops.render.render(write_still=True); log("RENDERED", path)

def label(text, loc, size, rot=(math.radians(80), 0, 0), col=(0.08, 0.06, 0.05)):
    cu = bpy.data.curves.new("lbl", "FONT"); cu.body = text; cu.size = size; cu.align_x = "CENTER"; cu.extrude = size * 0.02
    o = link(bpy.data.objects.new("lbl", cu)); o.location = loc; o.rotation_euler = rot
    m = bpy.data.materials.new("lbl"); m.use_nodes = True; m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (*col, 1)
    m.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.6
    cu.materials.append(m)
    return o

# ================================================================ baking (Cycles) and export
def new_img(name, size, alpha=False):
    im = bpy.data.images.get(name)
    if im: bpy.data.images.remove(im)
    return bpy.data.images.new(name, size, size, alpha=alpha, float_buffer=False)

def bake_pass(o, kind, img, samples=1, margin=6, filt=None):
    gpu()
    S.cycles.samples = samples; S.cycles.use_denoising = False
    for slot in o.material_slots:
        m = slot.material
        if not m: continue
        nd = m.node_tree.nodes.get("BAKE")
        if not nd:
            nd = m.node_tree.nodes.new("ShaderNodeTexImage"); nd.name = "BAKE"
        nd.image = img
        m.node_tree.nodes.active = nd; nd.select = True
    active(o)
    S.render.bake.margin = margin
    S.render.bake.use_clear = True
    kw = {"type": kind, "margin": margin, "use_clear": True}
    if filt: kw["pass_filter"] = filt
    if kind == "NORMAL": kw["normal_space"] = "TANGENT"
    bpy.ops.object.bake(**kw)

def pix(img):
    import array
    a = array.array("f", [0.0]) * (img.size[0] * img.size[1] * 4); img.pixels.foreach_get(a); return a

def save_png(img, path, cs="sRGB"):
    # (no colour space change here: on a generated image it regenerates the buffer and the baked pixels are lost -
    #  every texture came out black. The colour space is set when the image is made, before the bake.)
    img.filepath_raw = path; img.file_format = "PNG"
    img.save()
    import array
    a = array.array("f", [0.0]) * (img.size[0] * img.size[1] * 4); img.pixels.foreach_get(a)
    mx = max(a[i] + a[i + 1] + a[i + 2] for i in range(0, len(a), 4 * 61))
    log("TEX", os.path.basename(path), "max %.3f" % mx)

def compose(path, size, rgb_from, a_from=None, chans=None):
    """chans: list of 3 (array, channel) for R G B (and A from a_from channel 0)"""
    import array
    out = new_img("compose", size, alpha=True)
    n = size * size
    buf = array.array("f", [1.0]) * (n * 4)
    if chans:
        for c, (src, ch) in enumerate(chans):
            if src is None: continue
            buf[c::4] = src[ch::4]
    else:
        for c in range(3): buf[c::4] = rgb_from[c::4]
    if a_from is not None: buf[3::4] = a_from[0::4]
    out.pixels.foreach_set(buf); out.update()
    save_png(out, path)

def bake_set(o, food_id, d, states, colour_only_states=False):
    """the game textures of the food: fresh colour / normal / AO-rough, and each visible state (colour + rough in alpha)"""
    p = lambda s: os.path.join(d, "T_FoodV2_%s_%s.png" % (food_id, s))
    set_state([o], **{k: 0.0 for k in STATES})
    c = new_img("C", TEX); bake_pass(o, "DIFFUSE", c, filt={"COLOR"}); save_png(c, p("C"))
    nimg = new_img("N", TEX); nimg.colorspace_settings.name = "Non-Color"; bake_pass(o, "NORMAL", nimg); save_png(nimg, p("N"), "Non-Color")
    ao = new_img("AO", TEX); ao.colorspace_settings.name = "Non-Color"; bake_pass(o, "AO", ao, samples=24)
    ro = new_img("RO", TEX); ro.colorspace_settings.name = "Non-Color"; bake_pass(o, "ROUGHNESS", ro)
    compose(p("R"), TEX, None, chans=[(pix(ao), 0), (pix(ro), 0), (None, 0)])
    for st, vals in states:
        set_state([o], **{k: 0.0 for k in STATES}); set_state([o], **vals)
        sc = new_img("SC", STEX); bake_pass(o, "DIFFUSE", sc, filt={"COLOR"})
        sr = new_img("SR", STEX); sr.colorspace_settings.name = "Non-Color"; bake_pass(o, "ROUGHNESS", sr)
        compose(p(st), STEX, pix(sc), a_from=pix(sr))
    set_state([o], **{k: 0.0 for k in STATES})

GAME_STATES = [("Toast", {"Toast": 1.0}), ("Burnt", {"Burn": 1.0, "Toast": 1.0}), ("Spoil", {"Spoil": 1.0}), ("Frozen", {"Frozen": 1.0})]

def unwrap(o, margin=0.006):
    active(o); bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=margin, area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
    try: bpy.ops.uv.pack_islands(margin=margin, rotate=True)
    except Exception as e: log("pack", e)
    bpy.ops.object.mode_set(mode="OBJECT")

def export_fbx(o, path):
    active(o)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, use_mesh_modifiers=True, mesh_smooth_type="FACE",
                             bake_anim=False, add_leaf_bones=False, path_mode="STRIP", embed_textures=False)
    log("FBX", os.path.basename(path), len(o.data.polygons), "faces")

def recentre(o):
    """origin at the centre of its bounds; returns the offset that was taken out (metres)"""
    mn, mx = bounds([o]); c = (mn + mx) * 0.5
    o.data.transform(Matrix.Translation(-(c - o.location)))
    o.location = (0, 0, 0)
    return c

def tris(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)

def limit_tris(o, maxt):
    t = tris(o)
    if t > maxt:
        d = o.modifiers.new("dec", "DECIMATE"); d.ratio = maxt / float(t); apply_mods(o)
        log("decimated", o.name, t, "->", tris(o))

# ================================================================ cutting the pre-cut pieces
def cutter(name, origin, normal, size, rough_amp, rough_scale, seed, mat):
    """a big box whose one face is a rough sheet through origin facing normal: the half-space behind it"""
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=48, y_segments=48, size=size)
    R = random.Random(seed)
    tex = bpy.data.textures.new(name + "_t", "CLOUDS"); tex.noise_scale = rough_scale; tex.noise_depth = 3
    for v in bm.verts:
        n = tex.evaluate((v.co.x + seed * 0.37, v.co.y - seed * 0.11, seed * 0.07))[3] if hasattr(tex, "evaluate") else R.random()
        v.co.z = (n - 0.5) * 2.0 * rough_amp
    # make it a closed slab: extrude the sheet backwards
    ret = bmesh.ops.extrude_face_region(bm, geom=list(bm.faces))
    ev = [g for g in ret["geom"] if isinstance(g, bmesh.types.BMVert)]
    for v in ev: v.co.z -= size
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    o = mesh_obj(name, bm)
    q = Vector((0, 0, 1)).rotation_difference(Vector(normal).normalized())
    o.matrix_world = Matrix.Translation(Vector(origin)) @ q.to_matrix().to_4x4()
    assign(o, mat)
    return o

def boolean(o, cut, op):
    keep = o.data.copy()
    for solver in ("EXACT", "FLOAT", "MANIFOLD"):
        md = o.modifiers.new("bool", "BOOLEAN"); md.object = cut; md.operation = op
        try: md.solver = solver
        except Exception: o.modifiers.remove(md); continue
        try: md.material_mode = "TRANSFER"   # the cut faces get the cutter's material: "Inner"
        except Exception: pass
        try:
            md.use_self = False; md.use_hole_tolerant = True   # (every part is closed on its own: no self test, much faster)
        except Exception: pass
        apply_mods(o)
        if len(o.data.polygons) > 0:
            return True
        o.data = keep.copy()                                  # (that solver gave up on this mesh: the next one)
    return False

def make_pieces(whole, plan, inner_mat, seed=1):
    """plan: list of cuts (origin, normal, amp, scale); 2^k pieces, the empty ones dropped. Returns the piece objects."""
    pieces = [dup(whole, whole.name + "_P")]
    for ci, (org, nrm, amp, sc) in enumerate(plan):
        nxt = []
        for k, pc in enumerate(pieces):
            a = dup(pc, pc.name + "a"); b = pc
            ct = cutter("cut%d_%d" % (ci, k), org, nrm, 0.5, amp, sc, seed * 31 + ci * 7 + k, inner_mat)
            okA = boolean(a, ct, "INTERSECT")
            okB = boolean(b, ct, "DIFFERENCE")
            bpy.data.objects.remove(ct, do_unlink=True)
            for x, ok in ((a, okA), (b, okB)):
                if ok and len(x.data.polygons) > 20: nxt.append(x)
                else: bpy.data.objects.remove(x, do_unlink=True)
        pieces = nxt
    return pieces

# ================================================================ the game's current model (for "before"): the owner's Meshy / Higgsfield files, read only
OLD = {
    "Cake": ("meshy", "SM_Food_CakePiece"), "Gummy": ("meshy", "SM_Food_Gummy"), "GummyBear": ("meshy", "SM_Food_GummyBearGold"), "Cheese": ("meshy", "SM_Food_CheeseChunk"),
    "Chocolate": ("meshy", "SM_Food_Chocolate"), "Honey": ("meshy", "SM_Food_HoneyDrop"), "Sugar": ("meshy", "SM_Food_SugarCube"),
    "Apple": ("m4", "SM_M4_AppleSlice"), "IceCream": ("m4", "SM_M4_IceCream"), "Candy": ("m4", "SM_M4_Caramel"),
    "Marshmallow": ("m4", "SM_M4_Zefir"), "Condensed": ("m4", "SM_M4_CondensedMilk"),
    "Cookie": ("m4look", "SM_M4_Crumb", (0.55, 0.38, 0.22)), "Bread": ("m4look", "SM_M4_Crumb", (0.80, 0.60, 0.35)),
    "Pea": ("m4look", "SM_M4_Pea", (0.35, 0.62, 0.22)), "Crisp": ("m4look", "SM_M4_Speck", (0.95, 0.80, 0.40)),
    "Nut": ("m4look", "SM_M4_Pebble", (0.30, 0.17, 0.08)),
}

def tex_mat(name, png, tint=(1, 1, 1)):
    m = bpy.data.materials.new(name); m.use_nodes = True; p = m.node_tree.nodes["Principled BSDF"]; t = NT(m.node_tree)
    if png and os.path.exists(png):
        im = bpy.data.images.load(png); tx = t.n("ShaderNodeTexImage"); tx.image = im
        t.set(p.inputs["Base Color"], t.mixc(1.0, tx.outputs["Color"], tint, "MULTIPLY"))
    else:
        p.inputs["Base Color"].default_value = (*tint, 1)
    p.inputs["Roughness"].default_value = 0.5
    return m

def import_old(food_id, target_len, at):
    """the model the game shows today, scaled to the same size; None if the food is new"""
    src = OLD.get(food_id)
    if not src: return None, "нет в игре: новая еда"
    kind, name = src[0], src[1]
    fbx, png = None, None
    if kind == "meshy":
        d = os.path.join(OLD_ROOT, "SourceArt", "Meshy", name)
        for root, _, files in os.walk(d):
            for f in files:
                if f.lower().endswith(".fbx"): fbx = os.path.join(root, f)
                if f.lower().endswith("_texture.png"): png = os.path.join(root, f)
        what = "сейчас: Meshy " + name
    else:
        d = os.path.join(PROJ, "SourceArt", "Map4", "props", name)
        fbx = os.path.join(d, name + ".fbx")
        png = os.path.join(d, "T_%s_C.png" % name)
        what = "сейчас: " + name
    if not fbx or not os.path.exists(fbx):
        log("old model missing", food_id, fbx); return None, "старой модели нет на диске"
    before = set(S.objects)
    try:
        bpy.ops.import_scene.fbx(filepath=fbx)
    except Exception as e:
        log("fbx import failed", e); return None, "старая модель не читается"
    new = [o for o in S.objects if o not in before and o.type == "MESH"]
    for o in S.objects:
        if o not in before and o.type != "MESH": bpy.data.objects.remove(o, do_unlink=True)
    if not new: return None, "старая модель пустая"
    o = join(new, "Old_" + food_id) if len(new) > 1 else new[0]
    active(o); bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    tint = src[2] if kind == "m4look" else (1, 1, 1)
    assign(o, tex_mat("old_" + food_id, png if kind != "m4look" else None, tint))
    mn, mx = bounds([o]); size = max(mx - mn)
    c = (mn + mx) * 0.5
    o.data.transform(Matrix.Translation(-c)); o.location = (0, 0, 0)
    k = target_len / max(1e-6, size); o.scale = (k, k, k)
    active(o); bpy.ops.object.transform_apply(scale=True)
    mn, mx = bounds([o])
    o.location = (at[0], at[1], at[2] - mn.z)
    return o, what

# ================================================================ the foods
FOODS = {}
def food(fid, kind, size_cm, pieces, inner_axis="z"):
    def deco(fn):
        FOODS[fid] = dict(build=fn, kind=kind, size=size_cm / 100.0, pieces=pieces, inner_axis=inner_axis)
        return fn
    return deco

def band(t, h, a, b, soft=0.006):
    return t.mul(t.mapr(h, a - soft, a, 0.0, 1.0), t.mapr(h, b, b + soft, 1.0, 0.0))

# ---------------------------------------------------------------- shared material looks
def sponge_look(base=(1.0, 0.80, 0.42), pore=650.0, crust=None):
    def b(t, v, h=None):
        # crumb: a foam of air cells, small ones everywhere, a few big holes, darker and shaded inside
        sc = t.n("ShaderNodeVectorMath", operation="SCALE")
        t.set(sc.inputs[0], t.vmath("SUBTRACT", t.noise(v, pore * 0.2, 3.0, out="Color"), (0.5, 0.5, 0.5))); sc.inputs["Scale"].default_value = 1.2 / pore
        vd = t.vmath("ADD", v, sc.outputs[0])
        e1 = t.voronoi(vd, pore * 1.4, "DISTANCE_TO_EDGE")                       # a foam: bright crumb walls, dark open cells
        r1 = t.sep(t.voronoi(vd, pore * 1.4, "F1", out="Color"))[0]
        e2 = t.voronoi(vd, pore * 0.45, "DISTANCE_TO_EDGE")
        r2 = t.sep(t.voronoi(vd, pore * 0.45, "F1", out="Color"))[0]
        n = t.noise(v, pore * 0.4, 5.0)
        pm = t.mul(t.mapr(e1, 0.03, 0.16, 0.0, 1.0, smooth=True), t.mapr(r1, 0.15, 0.9, 0.2, 1.0))
        bg = t.mul(t.mapr(e2, 0.05, 0.2, 0.0, 1.0, smooth=True), t.mapr(r2, 0.75, 0.95, 0.0, 1.0))
        col = t.mixc(t.mul(pm, 0.55), base, t.mixc(1.0, base, (0.78, 0.60, 0.40), "MULTIPLY"))
        col = t.mixc(t.mul(bg, 0.75), col, t.mixc(1.0, base, (0.55, 0.40, 0.24), "MULTIPLY"))
        col = t.mixc(t.mul(t.sub(n, 0.5), 0.8), col, (1.0, 0.97, 0.84))
        if crust is not None:
            col = t.mixc(crust(t, v), col, (0.55, 0.32, 0.12))
        hh = t.add(t.mul(pm, -1.0), t.mul(bg, -1.6))
        return col, t.mapr(n, 0.3, 0.7, 0.74, 0.92), hh
    return b

def cream_look(base, rough=0.38, swirl=180.0):
    def b(t, v, h=None):
        w = t.wave(v, swirl, "RINGS", 6.0)
        n = t.noise(v, 400.0, 3.0)
        col = t.mixc(t.mul(t.sub(n, 0.5), 0.3), base, (1.0, 1.0, 1.0))
        return col, rough, t.add(t.mul(w, 0.25), t.mul(n, 0.2))
    return b

def berry_cream_look(base):
    def b(t, v, h=None):
        c, r, hh = cream_look(base, 0.34, 160.0)(t, v)
        bits = t.voronoi(v, 380.0, "F1")
        bm = t.mapr(bits, 0.0, 0.16, 1.0, 0.0, smooth=True)                                                     # strawberry bits
        c = t.mixc(bm, c, (0.62, 0.05, 0.10))
        return c, r, t.add(hh, t.mul(bm, 0.4))
    return b

def glossy_look(base, rough=0.12, var=0.15, scale=300.0):
    def b(t, v, h=None):
        n = t.noise(v, scale, 3.0)
        col = t.mixc(t.mul(t.sub(n, 0.5), var * 2.0), base, (min(1, base[0] * 1.3), min(1, base[1] * 1.3), min(1, base[2] * 1.3)))
        return col, rough, t.mul(t.noise(v, scale * 3.0, 2.0), 0.15)
    return b

def flat_look(base, rough=0.5):
    def b(t, v, h=None):
        return base, rough, 0.0
    return b

# ---------------------------------------------------------------- CAKE: a slice of the birthday cake (crumbly)
@food("Cake", "Crumbly", 9.0, 4)
def build_cake():
    R, A = 0.09, math.radians(38)
    H = [0.0, 0.015, 0.0215, 0.0365, 0.043, 0.058]      # sponge / cream / sponge / cream / sponge
    TOP = 0.0625
    def wedge_poly(r, inset=0.0, jit=0.0, seed=0, nside=18, narc=30):
        Rg = random.Random(seed); pts = []
        a0, a1 = -A / 2, A / 2
        for i in range(nside):                                         # the cut side at -A/2, apex outwards
            f = i / nside; x = r * f * math.cos(a0); y = r * f * math.sin(a0)
            pts.append((x + Rg.uniform(-jit, jit), y + Rg.uniform(-jit, jit) - inset))
        for i in range(narc + 1):                                     # the outer arc
            a = a0 + (a1 - a0) * i / narc; pts.append((r * math.cos(a), r * math.sin(a)))
        for i in range(nside, 0, -1):                                  # the cut side at +A/2
            f = i / nside; x = r * f * math.cos(a1); y = r * f * math.sin(a1)
            pts.append((x + Rg.uniform(-jit, jit), y + Rg.uniform(-jit, jit) + inset))
        pts[0] = (0.004, 0.0)
        return pts
    sponge = food_mat("Sponge", sponge_look(crust=lambda t, v: t.mapr(t.sep(v)[2], 0.0, 0.0015, 0.7, 0.0)), bump=0.0004, bump_str=0.7)
    cream = food_mat("Cream", berry_cream_look((0.95, 0.34, 0.50)), sss=0.4, sss_rad=(1, 0.5, 0.5), sheen=0.2, bump=0.0003)
    frost = food_mat("Frosting", cream_look((0.99, 0.84, 0.86), 0.40, 70.0), sss=0.3, coat=0.15, bump=0.0004, bump_str=0.8)
    cherry = food_mat("Cherry", glossy_look((0.55, 0.02, 0.04), 0.1), sss=0.6, sss_rad=(1, 0.2, 0.1), coat=0.8, coat_rough=0.05)
    stem = food_mat("Stem", flat_look((0.30, 0.25, 0.10), 0.6))
    sprk = [food_mat("Sprinkle%d" % i, flat_look(c, 0.35)) for i, c in enumerate(((0.95, 0.2, 0.3), (0.2, 0.55, 0.95), (1.0, 0.85, 0.2), (0.4, 0.85, 0.4), (0.95, 0.95, 0.95)))]
    parts = []
    for i in range(5):
        isc = i % 2 == 1
        o = prism("Layer%d" % i, wedge_poly(R * (1.0 if not isc else 0.995), 0.0 if not isc else 0.0007, 0.0004 if not isc else 0.0002, i), H[i], H[i + 1] if i < 4 else H[5])
        assign(o, cream if isc else sponge)
        sm = o.modifiers.new("sub", "SUBSURF"); sm.subdivision_type = "SIMPLE"; sm.levels = 3
        displace(o, "CLOUDS", 0.0016 if not isc else 0.002, 0.0009 if not isc else 0.0009, 0.5, 3)   # torn crumb on the cut
        if isc:                                                         # cream squeezed out of the cut a little: rounded
            sm2 = o.modifiers.new("sub2", "SUBSURF"); sm2.levels = 1
        parts.append(o)
    # the frosting: a thin cap over the top and the outer side, rounded, with drips over the arc
    top = prism("FrostTop", wedge_poly(R + 0.003, 0.0, 0.0, 9), H[5], TOP)
    assign(top, frost); bv = top.modifiers.new("bev", "BEVEL"); bv.width = 0.0028; bv.segments = 5
    sm = top.modifiers.new("sub", "SUBSURF"); sm.levels = 2
    displace(top, "CLOUDS", 0.006, 0.0007, 0.5, 2)
    parts.append(top)
    arc = []
    a0, a1 = -A / 2, A / 2
    for i in range(41):
        a = a0 + (a1 - a0) * i / 40; arc.append((R * math.cos(a), R * math.sin(a)))
    inner_arc = [((R - 0.0002) * math.cos(a0 + (a1 - a0) * i / 40), (R - 0.0002) * math.sin(a0 + (a1 - a0) * i / 40)) for i in range(40, -1, -1)]
    outer_arc = [((R + 0.0032) * math.cos(a0 + (a1 - a0) * i / 40), (R + 0.0032) * math.sin(a0 + (a1 - a0) * i / 40)) for i in range(41)]
    side = prism("FrostSide", outer_arc + inner_arc, 0.0, H[5] + 0.001)
    assign(side, frost); sm = side.modifiers.new("sub", "SUBSURF"); sm.subdivision_type = "SIMPLE"; sm.levels = 3
    displace(side, "CLOUDS", 0.005, 0.0008, 0.5, 2); sm2 = side.modifiers.new("sub2", "SUBSURF"); sm2.levels = 1
    parts.append(side)
    Rg = random.Random(3)
    for i in range(5):                                                  # drips over the edge
        a = a0 + (a1 - a0) * (0.12 + 0.76 * i / 4.0) + Rg.uniform(-0.03, 0.03)
        ln = Rg.uniform(0.007, 0.018)
        x, y = (R + 0.0030) * math.cos(a), (R + 0.0030) * math.sin(a)
        d = tube("Drip%d" % i, [(x * 0.995, y * 0.995, TOP + 0.0004), (x, y, TOP - 0.002), (x * 1.004, y * 1.004, TOP - ln * 0.55), (x * 1.006, y * 1.006, TOP - ln)], 0.0021, 12, True, [1.25, 0.95, 0.7, 1.15])
        assign(d, frost); smooth(d, 1); parts.append(d)
    # piped rosettes along the arc: a star tip swirled up
    star = bpy.data.curves.new("star", "CURVE"); star.dimensions = "2D"; sp = star.splines.new("POLY"); sp.points.add(15)
    for k, p in enumerate(sp.points):
        rr = 0.0034 if k % 2 == 0 else 0.0021; a = math.pi * 2 * k / 16; p.co = (rr * math.cos(a), rr * math.sin(a), 0, 1)
    sp.use_cyclic_u = True
    starob = link(bpy.data.objects.new("starprof", star))
    def rosette(cx, cy, name):
        cu = bpy.data.curves.new(name, "CURVE"); cu.dimensions = "3D"; cu.bevel_mode = "OBJECT"; cu.bevel_object = starob; cu.use_fill_caps = True; cu.resolution_u = 10
        spl = cu.splines.new("NURBS"); pts = []
        for k in range(26):
            f = k / 25.0; a = f * math.pi * 2 * 2.2; rr = 0.0058 * (1 - f) ** 0.8
            pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a), TOP + 0.0008 + f * 0.0095))
        spl.points.add(len(pts) - 1)
        for k, (p, c) in enumerate(zip(spl.points, pts)): p.co = (*c, 1); p.radius = 1.0 - 0.75 * (k / 25.0) ** 2
        spl.use_endpoint_u = True; spl.order_u = 4
        o = link(bpy.data.objects.new(name, cu)); active(o); bpy.ops.object.convert(target="MESH")
        o = bpy.context.view_layer.objects.active; assign(o, frost); smooth(o, 1); return o
    for i in range(3):
        a = a0 + (a1 - a0) * (0.18 + 0.32 * i); rr = R - 0.0095
        parts.append(rosette(rr * math.cos(a), rr * math.sin(a), "Rosette%d" % i))
    bpy.data.objects.remove(starob, do_unlink=True)
    # a cherry on the middle rosette
    cx, cy = (R - 0.0095) * math.cos(0.0), 0.0
    ch = uvsphere("Cherry", 0.0058, (cx, cy, TOP + 0.0135), 32, 16, (1, 1, 0.92))
    for v in ch.data.vertices:
        if v.co.z > TOP + 0.0175: v.co.z -= 0.0012 * (v.co.z - TOP - 0.0175) / 0.001    # the dimple where the stem goes
    assign(ch, cherry); smooth(ch, 1); parts.append(ch)
    st = tube("Stem", [(cx, cy, TOP + 0.0178), (cx - 0.002, cy + 0.001, TOP + 0.024), (cx - 0.006, cy + 0.003, TOP + 0.029)], 0.0005, 10)
    assign(st, stem); parts.append(st)
    # sprinkles
    for i in range(46):
        r = Rg.uniform(0.012, R - 0.012); a = Rg.uniform(a0 + 0.04, a1 - 0.04)
        x, y = r * math.cos(a), r * math.sin(a)
        if math.hypot(x - cx, y) < 0.008: continue
        ln = 0.0024; yaw = Rg.uniform(0, math.pi)
        sp_ = cyl("Spr%d" % i, 0.00045, ln, (x, y, TOP + 0.0004), 8, (math.pi / 2, 0, yaw))
        assign(sp_, sprk[i % len(sprk)]); smooth(sp_, 1, 0.0003, 2); parts.append(sp_)
    def inner(t, v, h):
        # the cut through the cake: sponge, cream, sponge, cream, sponge, the frosting on top
        sp_c, sp_r, sp_h = sponge_look()(t, v)
        cr = band(t, h, 0.240, 0.344, 0.008); cr2 = band(t, h, 0.584, 0.688, 0.008); fr = t.mapr(h, 0.925, 0.94, 0.0, 1.0)
        cc, _, _ = berry_cream_look((0.95, 0.34, 0.50))(t, v)
        col = t.mixc(t.add(cr, cr2, True), sp_c, cc)
        col = t.mixc(fr, col, (0.99, 0.84, 0.86))
        rough = t.mixf(t.add(t.add(cr, cr2), fr, True), sp_r, 0.4)
        hh = t.mixf(t.add(t.add(cr, cr2), fr, True), sp_h, 0.0)
        return col, rough, hh
    cuts = [((0.0, 0.0, 0.0), (1.0, 0.05, 0.12), 0.0022, 0.006), ((0.0, 0.0, 0.0), (0.02, 1.0, 0.1), 0.0018, 0.006)]
    return dict(parts=parts, inner=inner, cuts=cuts, jelly=0.0, hmin=(0, 0, 0.0), hmax=(0, 0, TOP))

# ---------------------------------------------------------------- GUMMY: a gummy bear lying on its back (elastic)
def jelly_look(base, rough=0.14, bubbles=True):
    def b(t, v, h=None):
        n = t.noise(v, 160.0, 3.0)
        col = t.mixc(t.mul(t.sub(n, 0.5), 0.5), base, (min(1, base[0] * 1.25 + 0.05), min(1, base[1] * 1.3 + 0.02), min(1, base[2] * 1.3 + 0.02)))
        dents = t.voronoi(v, 700.0, "F1")
        hh = t.add(t.mul(t.noise(v, 900.0, 2.0), 0.12), t.mul(t.mapr(dents, 0.0, 0.08, -0.4, 0.0), 1.0 if bubbles else 0.0))
        return col, t.mapr(n, 0.3, 0.7, rough, rough + 0.08), hh
    return b

def blob(name, prims, voxel, smooth_it=8, sub=1):
    """a soft moulded shape: a union of round primitives melted into one closed skin (voxel remesh), then smoothed"""
    o = join(prims, name)
    rm = o.modifiers.new("remesh", "REMESH"); rm.mode = "VOXEL"; rm.voxel_size = voxel; rm.use_smooth_shade = True
    apply_mods(o)
    sm = o.modifiers.new("smooth", "SMOOTH"); sm.factor = 0.7; sm.iterations = smooth_it
    if sub:
        s2 = o.modifiers.new("sub", "SUBSURF"); s2.levels = sub
    return o

def gradient_jelly(stops, rough=0.12):
    """jelly whose colour runs along the length (two-tone and rainbow gummies)"""
    def b(t, v, h=None):
        x = t.sep(v)[0]
        c = t.ramp(t.mapr(x, stops[0], stops[1]), stops[2])
        n = t.noise(v, 160.0, 3.0)
        c = t.mixc(t.mul(t.sub(n, 0.5), 0.4), c, (1.0, 1.0, 0.9))
        sugar = t.mapr(t.voronoi(v, 1400.0, "F1"), 0.0, 0.05, 1.0, 0.0)          # a few sugar crystals stuck on it
        c = t.mixc(t.mul(sugar, 0.5), c, (1.0, 0.98, 0.95))
        return c, t.mixf(sugar, t.mapr(n, 0.3, 0.7, rough, rough + 0.08), 0.35), t.add(t.mul(t.noise(v, 900.0, 2.0), 0.12), t.mul(sugar, 0.8))
    return b

# ---------------------------------------------------------------- GUMMY: the game's «Мармеладка» is a gummy worm (the owner's Meshy model is one): two-tone, ribbed
@food("Gummy", "Elastic", 5.0, 2)
def build_gummy():
    jelly = food_mat("Jelly", gradient_jelly((-0.024, 0.024, [(0.0, (0.15, 0.62, 0.05)), (0.35, (0.85, 0.80, 0.10)), (0.6, (0.98, 0.42, 0.05)), (1.0, (0.85, 0.05, 0.07))])),
                     sss=1.0, sss_rad=(1.0, 0.45, 0.2), sss_scale=0.006, coat=0.6, coat_rough=0.08, trans=0.3, ior=1.42, bump=0.00015, bump_str=0.4)
    pts, radii = [], []
    n = 72
    for k in range(n):
        f = k / (n - 1.0); x = -0.024 + 0.048 * f
        y = 0.0065 * math.sin(f * math.pi * 1.6 + 0.4)
        r = 1.0 + 0.09 * math.sin(f * math.pi * 2 * 11)                         # the ribs
        r *= 0.78 + 0.22 * min(1.0, f * 3.0)                                      # the tail thins
        pts.append((x, y, 0.0041)); radii.append(r)
    body = tube("Worm", pts, 0.0041, 8, True, radii)
    head = uvsphere("Head", 0.0043, (0.0242, 0.0065 * math.sin(math.pi * 1.6 + 0.4), 0.0041), 24, 12)
    tail = uvsphere("Tail", 0.0032, (-0.0242, 0.0065 * math.sin(0.4), 0.0041), 20, 10)
    w = blob("Worm", [body, head, tail], 0.00022, 6, 1)
    for v in w.data.vertices:                                                     # it lies on the floor: a flat belly
        if v.co.z < 0.0008: v.co.z = 0.0008 + (v.co.z - 0.0008) * 0.3
    assign(w, jelly)
    parts = [w]
    hy = 0.0065 * math.sin(math.pi * 1.6 + 0.4)
    eye = food_mat("Eye", glossy_look((0.25, 0.01, 0.02), 0.1))
    for s_ in (1, -1):
        e = uvsphere("Eye", 0.0007, (0.0262, hy + s_ * 0.0018, 0.0062), 12, 6, (1, 1, 0.7)); assign(e, eye); smooth(e, 1); parts.append(e)
    def inner(t, v, h):
        c, r, hh = gradient_jelly((-0.024, 0.024, [(0.0, (0.3, 0.72, 0.15)), (0.5, (0.95, 0.75, 0.25)), (1.0, (0.95, 0.25, 0.22))]), 0.25)(t, v)
        return c, r, t.mul(t.noise(v, 500.0, 3.0), 0.5)                         # torn jelly: rough and wet
    cuts = [((0.002, 0.0, 0.0), (1.0, 0.1, 0.0), 0.0008, 0.003)]               # torn in the middle
    return dict(parts=parts, inner=inner, cuts=cuts, jelly=0.85, coat=0.6)

# ---------------------------------------------------------------- GUMMY BEAR: «Золотой мармеладный мишка», lying on its back
@food("GummyBear", "Elastic", 5.0, 2)
def build_bear():
    jelly = food_mat("JellyGold", jelly_look((0.95, 0.55, 0.04)), sss=1.0, sss_rad=(1.0, 0.6, 0.15), sss_scale=0.008, coat=0.6, coat_rough=0.08, trans=0.3, ior=1.42, bump=0.00015, bump_str=0.4)
    k = 1.9                                                                       # the giant one: 5 cm
    def el(name, c, r, sc): return uvsphere(name, r * k, tuple(x * k for x in c), 32, 16, sc)
    prims = [el("Belly", (0.0, 0.0, 0.0), 0.0062, (1.15, 0.95, 0.62)), el("Head", (0.0098, 0.0, 0.0006), 0.0054, (0.98, 1.05, 0.66)),
             el("EarL", (0.0136, 0.0045, 0.0006), 0.0021, (1, 1, 0.8)), el("EarR", (0.0136, -0.0045, 0.0006), 0.0021, (1, 1, 0.8)),
             el("Snout", (0.0121, 0.0, 0.0026), 0.0021, (1.0, 1.25, 0.6)),
             el("ArmL", (0.0036, 0.0063, 0.0006), 0.0021, (1.35, 0.8, 0.7)), el("ArmR", (0.0036, -0.0063, 0.0006), 0.0021, (1.35, 0.8, 0.7)),
             el("LegL", (-0.0072, 0.0042, 0.0002), 0.0029, (1.25, 0.95, 0.66)), el("LegR", (-0.0072, -0.0042, 0.0002), 0.0029, (1.25, 0.95, 0.66))]
    b = blob("Bear", prims, 0.0004, 8, 0)
    ls = b.modifiers.new("lap", "LAPLACIANSMOOTH"); ls.lambda_factor = 1.0; ls.iterations = 25; ls.use_volume_preserve = True
    s2 = b.modifiers.new("sub", "SUBSURF"); s2.levels = 1
    zc = -0.0024 * k
    for v in b.data.vertices:                                                    # the flat back of a moulded gummy
        if v.co.z < zc: v.co.z = zc + (v.co.z - zc) * 0.25
    assign(b, jelly)
    parts = [b]
    for (x, y, z, r) in ((0.0112, 0.0021, 0.0034, 0.00065), (0.0112, -0.0021, 0.0034, 0.00065), (0.0133, 0.0, 0.0040, 0.0008)):
        e = uvsphere("Face", r * k, (x * k, y * k, z * k), 16, 8, (1, 1, 0.6)); assign(e, jelly); smooth(e, 1); parts.append(e)
    def inner(t, v, h):
        c, r, hh = jelly_look((1.0, 0.68, 0.12), 0.25, False)(t, v)
        return c, r, t.mul(t.noise(v, 500.0, 3.0), 0.5)
    cuts = [((0.004, 0.0, 0.0), (1.0, 0.0, 0.06), 0.0009, 0.004)]               # torn at the neck
    return dict(parts=parts, inner=inner, cuts=cuts, jelly=0.85, coat=0.6)

# ================================================================ the rest of the foods live in food_v2_more.py (same helpers)
_more = os.path.join(os.path.dirname(os.path.abspath(__file__)), "food_v2_more.py")
if os.path.exists(_more):
    exec(compile(open(_more, encoding="utf-8").read(), _more, "exec"))

# ================================================================ one food: model, textures, pieces, export, renders
def height_of(t, v, axis, mn, mx):
    x, y, z = t.sep(v)
    if axis == "r":
        cx, cy = (mn.x + mx.x) * 0.5, (mn.y + mx.y) * 0.5
        d = t.vmath("LENGTH", t.vmath("SUBTRACT", v, (cx, cy, 0.0)), out=1)
        # (LENGTH of the xy only: drop z)
        xy = t.n("ShaderNodeCombineXYZ"); t.set(xy.inputs[0], t.sub(x, cx)); t.set(xy.inputs[1], t.sub(y, cy)); xy.inputs[2].default_value = 0.0
        d = t.vmath("LENGTH", xy.outputs[0], out=1)
        return t.mapr(d, 0.0, max(1e-4, max(mx.x - mn.x, mx.y - mn.y) * 0.5))
    return t.mapr(z, mn.z, mx.z)

def slot_mat(name):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    return m

def export_copy(o, path, inner_name=None):
    """a centred copy with the game's slots: Outer (everything) and Inner (the cut faces); returns the centre (metres)"""
    c = dup(o, o.name + "_x")
    inner_idx = set(i for i, s in enumerate(c.material_slots) if s.material and inner_name and s.material.name.startswith(inner_name))
    flags = [p.material_index in inner_idx for p in c.data.polygons]
    c.data.materials.clear(); c.data.materials.append(slot_mat("Outer"))
    if any(flags): c.data.materials.append(slot_mat("Inner"))
    for p, f in zip(c.data.polygons, flags): p.material_index = 1 if f else 0
    mn, mx = bounds([c]); ctr = (mn + mx) * 0.5
    c.data.transform(Matrix.Translation(-ctr))
    export_fbx(c, path)
    bpy.data.objects.remove(c, do_unlink=True)
    return ctr, mx - mn

def prepare_parts(parts, maxtris):
    """apply the modifiers, thin the parts to the budget together, unwrap them into one shared UV space"""
    ps = []
    for o in parts:
        if o.type != "MESH":
            active(o); bpy.ops.object.convert(target="MESH"); o = bpy.context.view_layer.objects.active
        apply_mods(o)
        for p in o.data.polygons: p.use_smooth = True
        ps.append(o)
    total = sum(tris(o) for o in ps)
    if total > maxtris:
        r = maxtris / float(total)
        for o in ps:
            if tris(o) > 200:
                d = o.modifiers.new("dec", "DECIMATE"); d.ratio = r; apply_mods(o)
        log("decimated parts", total, "->", sum(tris(o) for o in ps))
    for o in [x for x in S.objects if x.type == "META"]: bpy.data.objects.remove(o, do_unlink=True)   # (metaball sources)
    ps = [o for o in ps if o.name in S.objects and len(o.data.vertices) > 0]
    log("parts", len(ps), sum(len(o.data.vertices) for o in ps), "verts")
    for x in S.objects: x.select_set(False)
    for o in ps: o.select_set(True)
    bpy.context.view_layer.objects.active = ps[0]
    bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.004, area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
    try: bpy.ops.uv.pack_islands(margin=0.004, rotate=True)
    except Exception as e: log("pack", e)
    bpy.ops.object.mode_set(mode="OBJECT")
    return ps

def join_copies(parts, name):
    return join([dup(o, o.name + "_j") for o in parts], name)

def pieces_from_parts(parts, plan, inner_mat, seed):
    """cut every (closed) part on its own and join the bits of each piece: clean booleans, the atlas UVs kept"""
    groups = [[dup(o, o.name + "_p") for o in parts]]
    pmn, pmx = bounds(parts); span = max(pmx - pmn) * 1.6
    for ci, (org, nrm, amp, sc) in enumerate(plan):
        ct = cutter("cut%d" % ci, org, nrm, span, amp, sc, seed * 31 + ci * 7, inner_mat)
        nxt = []
        nv = Vector(nrm).normalized()
        for g in groups:
            A, B = [], []
            for o in g:
                # a part wholly on one side of the cut needs no boolean (the slab keeps what is behind the sheet: A)
                ds = [(o.matrix_world @ Vector(cc) - Vector(org)).dot(nv) for cc in o.bound_box]
                if max(ds) < -amp * 1.2: A.append(o); continue
                if min(ds) > amp * 1.2: B.append(o); continue
                a = dup(o, o.name + "a")
                if boolean(a, ct, "INTERSECT") and len(a.data.polygons) > 0: A.append(a)
                else: bpy.data.objects.remove(a, do_unlink=True)
                if boolean(o, ct, "DIFFERENCE") and len(o.data.polygons) > 0: B.append(o)
                else: bpy.data.objects.remove(o, do_unlink=True)
            for x in (A, B):
                if x and sum(len(o.data.polygons) for o in x) > 30: nxt.append(x)
                else:
                    for o in x: bpy.data.objects.remove(o, do_unlink=True)
        bpy.data.objects.remove(ct, do_unlink=True)
        groups = nxt
    out = []
    for k, g in enumerate(groups):
        out.append(join(g, "piece%d" % k))
    out.sort(key=lambda o: -sum(p.area for p in o.data.polygons))
    return out

def bite_copy(parts, inner, L, top=None):
    cp = [dup(o, o.name + "_b") for o in parts]
    mn, mx = bounds(cp); c = (mn + mx) * 0.5
    p = Vector((mx.x - 0.05 * L, c.y + 0.12 * L, (top if top is not None else mx.z) - 0.02 * L))
    for k, (dx, dy) in enumerate(((0, 0), (-0.11, 0.1), (-0.05, -0.12))):
        s = uvsphere("bite", 0.17 * L, (p.x + dx * L, p.y + dy * L, p.z), 24, 12, (1, 1, 0.9)); assign(s, inner)
        keep = []
        for o in cp:
            back = dup(o, o.name + "_u")
            if boolean(o, s, "DIFFERENCE") and len(o.data.polygons) > 0:
                keep.append(o); bpy.data.objects.remove(back, do_unlink=True)
            else:                                                    # (the solver gave up on this part: keep it whole)
                bpy.data.objects.remove(o, do_unlink=True); keep.append(back)
        cp = keep
        bpy.data.objects.remove(s, do_unlink=True)
    return join(cp, "bitten")

def run_food(fid):
    reset()
    F = FOODS[fid]
    spec = F["build"]()
    bmn, bmx = bounds(spec["parts"])
    if (bmx.y - bmn.y) > (bmx.x - bmn.x) * 1.05:
        # the game wants the long side along X: turn the whole food (and its cuts) a quarter turn
        R90 = Matrix.Rotation(math.pi / 2, 4, "Z")
        for o in spec["parts"]:
            o.matrix_world = R90 @ o.matrix_world
            active(o); bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        spec["cuts"] = [(tuple(R90.to_3x3() @ Vector(o)), tuple(R90.to_3x3() @ Vector(n)), a, sc) for (o, n, a, sc) in spec.get("cuts", [])]
        log(fid, "turned a quarter: long side along X")
    parts = prepare_parts(spec["parts"], spec.get("maxtris", 48000))
    whole = join_copies(parts, "SM_FoodV2_" + fid)
    for o in parts: o.hide_render = True
    mn, mx = bounds([whole]); ctr = (mn + mx) * 0.5; size = mx - mn
    log(fid, "size cm", ["%.2f" % (s * 100) for s in size], "table", F["size"] * 100, "tris", tris(whole))
    hmn, hmx = Vector(spec.get("hmin", mn)), Vector(spec.get("hmax", mx))
    d = os.path.join(OUT_ART, fid); os.makedirs(d, exist_ok=True)
    inner = food_mat("Inner_" + fid, lambda t, v: spec["inner"](t, v, height_of(t, v, F["inner_axis"], hmn, hmx)), bump=0.0004, bump_str=0.6,
                     sss=spec.get("inner_sss", 0.0))
    man = dict(id=fid, kind=F["kind"], size_m=[size.x, size.y, size.z], jelly=spec.get("jelly", 0.0), coat=spec.get("coat", 0.0),
               inner_axis=F["inner_axis"], pieces=[])
    pieces = []
    plan = [(tuple(ctr + Vector(o)), n, a, sc) for (o, n, a, sc) in spec.get("cuts", [])]
    if MODE in ("model", "all"):
        bake_set(whole, fid, d, GAME_STATES)
        # the inner strip: the inner look evaluated on a vertical sheet through the food, V = height (or radius)
        bpy.ops.mesh.primitive_plane_add(size=1.0)
        pl = bpy.context.object
        pl.scale = (size.x, 1.0, hmx.z - hmn.z if F["inner_axis"] == "z" else size.x * 0.5)
        pl.rotation_euler = (math.pi / 2, 0, 0); pl.location = (ctr.x, ctr.y, (hmn.z + hmx.z) * 0.5)
        active(pl); bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        def strip(t, v):
            uv = t.n("ShaderNodeTexCoord").outputs["UV"]
            return spec["inner"](t, v, t.sep(uv)[1])
        assign(pl, food_mat("InnerStrip_" + fid, strip, bump=0.0004))
        ic = new_img("IC", STEX); bake_pass(pl, "DIFFUSE", ic, filt={"COLOR"}); save_png(ic, os.path.join(d, "T_FoodV2_%s_InC.png" % fid))
        inn = new_img("IN", STEX); inn.colorspace_settings.name = "Non-Color"; bake_pass(pl, "NORMAL", inn); save_png(inn, os.path.join(d, "T_FoodV2_%s_InN.png" % fid), "Non-Color")
        bpy.data.objects.remove(pl, do_unlink=True)
        c0, s0 = export_copy(whole, os.path.join(d, "SM_FoodV2_%s.fbx" % fid))
        man["centre_m"] = list(c0)
        man["h_range_m"] = [hmn.z - c0.z, hmx.z - c0.z] if F["inner_axis"] == "z" else [0.0, size.x * 0.5]
        if F["pieces"] > 0 and plan:
            pieces = pieces_from_parts(parts, plan, inner, len(fid))[:F["pieces"]]
            for k, pc in enumerate(pieces):
                limit_tris(pc, 20000)
                pc.name = "SM_FoodV2_%s_P%d" % (fid, k)
                pcc, pcs = export_copy(pc, os.path.join(d, pc.name + ".fbx"), "Inner_")
                man["pieces"].append(dict(name=pc.name, centre_m=list(pcc - c0), size_m=list(pcs)))
            if len(pieces) != F["pieces"]:
                log("WARNING", fid, "pieces made", len(pieces), "wanted", F["pieces"])
        with open(os.path.join(d, "food.json"), "w", encoding="utf-8") as f:
            json.dump(man, f, indent=1, ensure_ascii=False)
    if MODE in ("review", "all"):
        if not pieces and F["pieces"] > 0 and plan:
            pieces = pieces_from_parts(parts, plan, inner, len(fid))[:F["pieces"]]
        spec["_parts"] = parts
        review(fid, F, spec, whole, pieces, inner, size)

# ================================================================ review renders: before / after, the state sheet
KIND_RU = dict(Crumbly="крошится", Elastic="тянется", Soft="мягкое, гнётся", Hard="твёрдое", Rolling="катится", Sticky="липкое",
               Melting="тает", Brittle="хрупкое", Juicy="сочное")
_FONT = []
def font():
    if _FONT:
        try: _FONT[0] and _FONT[0].name
        except Exception: _FONT.clear()            # (a new scene: the old font is gone)
    if not _FONT:
        f = None
        for p in (r"C:\Windows\Fonts\segoeui.ttf", r"C:\Windows\Fonts\arial.ttf"):     # (labels of the review pictures only)
            if os.path.exists(p):
                try: f = bpy.data.fonts.load(p); break
                except Exception: pass
        _FONT.append(f)
    return _FONT[0]

def text(s, loc, size, col=(0.07, 0.05, 0.04)):
    o = label(s, loc, size, (0, 0, 0), col)
    if font(): o.data.font = font()
    return o

def place(o, x, y, yaw=0.0):
    mn, mx = bounds([o])
    o.location = (o.location.x + x - (mn.x + mx.x) * 0.5, o.location.y + y - (mn.y + mx.y) * 0.5, o.location.z - mn.z + 0.0002)
    if yaw:
        piv = Vector((x, y, 0)); o.rotation_euler.z += yaw
        bpy.context.view_layer.update()
        mn, mx = bounds([o]); o.location.x += x - (mn.x + mx.x) * 0.5; o.location.y += y - (mn.y + mx.y) * 0.5; o.location.z += -mn.z + 0.0002

def puddle(x, y, r, col, rough=0.05):
    bm = bmesh.new(); bmesh.ops.create_circle(bm, cap_ends=True, segments=48, radius=r)
    R = random.Random(int(r * 1e5))
    for v in bm.verts:
        a = math.atan2(v.co.y, v.co.x); k = 1 + 0.22 * math.sin(a * 3 + R.random()) + 0.1 * math.cos(a * 5)
        v.co.x *= k; v.co.y *= k * 0.8
    o = mesh_obj("puddle", bm); o.location = (x, y, 0.00015)
    m = bpy.data.materials.new("puddle"); m.use_nodes = True; p = m.node_tree.nodes["Principled BSDF"]
    p.inputs["Base Color"].default_value = (*col, 1); p.inputs["Roughness"].default_value = rough; p.inputs["Coat Weight"].default_value = 1.0
    assign(o, m); return o

def bite(o, inner, L, where=None):
    mn, mx = bounds([o]); c = (mn + mx) * 0.5
    p = Vector(where) if where else Vector((mx.x - 0.05 * L, c.y + 0.12 * L, mx.z))
    for k, (dx, dy) in enumerate(((0, 0), (-0.11, 0.1), (-0.05, -0.12))):
        s = uvsphere("bite", 0.16 * L, (p.x + dx * L, p.y + dy * L, p.z), 24, 12, (1, 1, 0.9)); assign(s, inner)
        boolean(o, s, "DIFFERENCE"); bpy.data.objects.remove(s, do_unlink=True)

def stretch_mesh(o, k):
    """pulled apart along X: longer, thinner, a neck in the middle (what the game's material does with Stretch)"""
    bb = [Vector(b) for b in o.bound_box]                      # (local: the mesh's own coordinates)
    mn = Vector((min(b.x for b in bb), min(b.y for b in bb), min(b.z for b in bb))); mx = Vector((max(b.x for b in bb), max(b.y for b in bb), max(b.z for b in bb)))
    c = (mn + mx) * 0.5; hx = max(1e-6, (mx.x - mn.x) * 0.5)
    for v in o.data.vertices:
        a = (v.co.x - c.x) / hx; thin = (1.0 / math.sqrt(1 + k)) * (1 - 0.35 * k * max(0.0, 1 - a * a))
        v.co.x = c.x + (v.co.x - c.x) * (1 + k); v.co.y = c.y + (v.co.y - c.y) * thin; v.co.z = mn.z + (v.co.z - mn.z) * thin
    o.data.update()

def sag_mesh(o, k):
    """held at one end: the far end droops (what the game's material does with Tip)"""
    bb = [Vector(b) for b in o.bound_box]
    mnx = min(b.x for b in bb); L = max(1e-6, max(b.x for b in bb) - mnx)
    for v in o.data.vertices:
        f = (v.co.x - mnx) / L
        v.co.z -= k * L * 0.5 * f * f; v.co.x -= k * L * 0.12 * f * f
    o.data.update()

def review(fid, F, spec, whole, pieces, inner, size):
    L = max(size)
    for o in list(S.objects): o.hide_render = True; o.hide_viewport = False
    stage(span=L * 2.5)
    kind = F["kind"]
    # ---- before / after
    new = dup(whole, "new"); new.hide_render = False; place(new, 0.62 * L, 0.0, math.radians(-28))
    old, what = import_old(fid, L, (-0.62 * L, 0.0, 0.0))
    shown = [new]
    if old:
        place(old, -0.62 * L, 0.0, math.radians(-28)); shown.append(old)
    else:
        ph = uvsphere("placeholder", 0.25 * L, (0, 0, 0), 24, 12) if kind in ("Rolling", "Sticky") else cube("placeholder", (0.5 * L, 0.5 * L, 0.5 * L))
        m = bpy.data.materials.new("ph"); m.use_nodes = True; m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.6, 0.6, 0.6, 1)
        assign(ph, m); place(ph, -0.62 * L, 0.0); shown.append(ph)
    t1 = text("было — " + what, (-0.62 * L, -0.7 * L, 0.0003), 0.085 * L)
    t2 = text("стало — Blender, " + KIND_RU.get(kind, kind), (0.62 * L, -0.7 * L, 0.0003), 0.085 * L)
    cam = camera((0.0, -2.75 * L, 1.15 * L), (0.0, 0.05 * L, 0.22 * L), 38, 16.0 * max(1.0, 0.06 / L))
    render(os.path.join(OUT_REV, "%s_before_after.jpg" % fid))
    for o in shown[1:] + [t1, t2, cam]: bpy.data.objects.remove(o, do_unlink=True)
    bpy.data.objects.remove(new, do_unlink=True)
    # ---- the states
    cells = [("свежее", {}), ("подрумянено", {"Toast": 1.0}), ("подгорело", {"Toast": 1.0, "Burn": 1.0}), ("размокло", {"Soak": 1.0}),
             ("замёрзло", {"Frozen": 1.0}), ("грязное", {"Dirt": 1.0}), ("испортилось", {"Spoil": 1.0}), ("надкусано", "bite")]
    extra = {"Crumbly": [("раскрошилось", "pieces"), ("крошки при беге", "crumbs")], "Elastic": [("растянуто", "stretch"), ("порвано", "pieces")],
             "Soft": [("провисло", "sag"), ("порвано", "pieces")], "Hard": [("расколото", "pieces"), ("скатилось", "plain")],
             "Rolling": [("раздавлено", "pieces"), ("катится, в пыли", "dirtroll")], "Sticky": [("тянется нитью", "stretch"), ("липкая лужа", "puddle")],
             "Melting": [("тает", "melt"), ("растаяло наполовину", "melt2")], "Brittle": [("сломано пополам", "pieces"), ("в крошку", "pieces2")],
             "Juicy": [("выжато, сок", "juice"), ("разломано", "pieces")]}
    cells += extra.get(kind, [])
    cols = 5 if len(cells) > 8 else 4
    gx, gy = 1.35 * L, 2.3 * L
    made = []
    for i, (name, st) in enumerate(cells):
        cx = (i % cols - (cols - 1) * 0.5) * gx; cy = (i // cols) * gy
        if st == "pieces" or st == "pieces2":
            src = pieces if pieces else [whole]
            for k, pc in enumerate(src):
                c = dup(pc, "cell"); c.hide_render = False; unique_mats(c)
                if st == "pieces2": set_state([c], Toast=0.4)
                mn, mx = bounds([pc]); off = ((mn + mx) * 0.5 - (bounds([whole])[0] + bounds([whole])[1]) * 0.5) * 1.35
                place(c, cx + off.x, cy + off.y, math.radians(-25)); made.append(c)
        elif st == "crumbs":
            c = dup(whole, "cell"); c.hide_render = False; unique_mats(c); c.scale = (0.85, 0.85, 0.85); place(c, cx, cy, math.radians(-25)); made.append(c)
            R = random.Random(5)
            for k in range(18):
                cr = uvsphere("crumb", R.uniform(0.012, 0.03) * L, (0, 0, 0), 8, 5, (1, R.uniform(0.6, 1.0), 0.8))
                displace(cr, "CLOUDS", 0.002, 0.004 * L, 0.5, 1); assign(cr, inner)
                place(cr, cx - 0.5 * L - k * 0.035 * L + R.uniform(-0.05, 0.05) * L, cy - 0.25 * L + R.uniform(-0.12, 0.12) * L); made.append(cr)
        else:
            c = dup(whole, "cell"); c.hide_render = False; unique_mats(c)
            if isinstance(st, dict): set_state([c], **st)
            if st == "bite":
                bpy.data.objects.remove(c, do_unlink=True); c = bite_copy(spec["_parts"], inner, L, spec.get("hmax", (0, 0, None))[2]); c.hide_render = False; c.hide_viewport = False; unique_mats(c)
            if st == "stretch": stretch_mesh(c, 0.9 if kind == "Sticky" else 0.6)
            if st == "sag": sag_mesh(c, 0.45)
            if st in ("melt", "melt2"):
                k = 0.75 if st == "melt" else 0.45
                c.scale = (1.0 + (1 - k) * 0.5, 1.0 + (1 - k) * 0.5, k)
                made.append(puddle(cx, cy, (0.45 if st == "melt" else 0.7) * L, F.get("puddle", (0.9, 0.8, 0.75)), 0.08))
                set_state([c], Soak=0.6)
            if st == "puddle": made.append(puddle(cx + 0.35 * L, cy, 0.4 * L, F.get("puddle", (0.9, 0.55, 0.1)), 0.04))
            if st == "juice":
                c.scale = (1.0, 0.92, 0.85); made.append(puddle(cx + 0.3 * L, cy - 0.1 * L, 0.38 * L, F.get("puddle", (0.85, 0.5, 0.3)), 0.03))
            if st == "dirtroll": set_state([c], Dirt=0.6)
            place(c, cx, cy, math.radians(-25)); made.append(c)
            if st == "puddle" or st == "juice" or st in ("melt", "melt2"): place(c, cx, cy, 0.0)
        made.append(text(name, (cx, cy - 0.8 * L, 0.0003), 0.12 * L))
    rows = (len(cells) + cols - 1) // cols
    depth = (rows - 1) * gy
    cam = camera((0.0, -2.9 * L - depth * 0.35, 3.2 * L + depth * 0.75), (0.0, depth * 0.5 - 0.15 * L, 0.0), 34 if cols == 4 else 30, 22.0 * max(1.0, 0.06 / L))
    S.render.resolution_percentage = 100
    render(os.path.join(OUT_REV, "%s_states.jpg" % fid), 1800, 1100)

if __name__ == "__main__":
    ids = list(FOODS.keys()) if WHICH == "all" else [x for x in WHICH.replace("+", ",").split(",") if x]
    for fid in ids:
        if fid not in FOODS:
            log("unknown food", fid); continue
        try:
            run_food(fid)
            log("DONE", fid)
        except Exception as e:
            import traceback; traceback.print_exc(); log("FAILED", fid, e)
    log("ALL_DONE")
