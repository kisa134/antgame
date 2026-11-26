# Hero props pipeline (KAN-22): the owner's reference (Saved/hero_mop_bucket.py, approved 26.11) turned into game assets.
# Every piece is built procedurally in real metres (origin = the game frame of the piece it replaces), then:
#   - LOD0 (the ant's camera), LOD1 / LOD2 (the Human's distance) by collapse decimation, one UV atlas shared by all LODs;
#   - its own procedural materials baked to T_<Name>_C (colour, A = porosity: how much water darkens it), _N (tangent normal,
#     the procedural bump included), _R (AO, roughness, metal) - our own shaders only, nothing downloaded;
#   - collision: UCX_ convex hulls (physics) in the LOD0 FBX; the triangles for ants come from the drawn mesh (LOD_COLLISION);
#   - pre-cut break pieces <Name>_Brk<i> (AntWreck shows them instead of slicing the collision): slot 0 = the surface (same atlas),
#     slot 1 "Break" = the fresh fracture face;
#   - moving parts as their own meshes with the pivot at the origin (the bucket handle on its pins);
#   - review renders: overall, the ant's view, the states (wet, charred, broken).
# Blender 5.3, Cycles GPU. Library only: the pieces are in hero_kitchen_a.py.
import bpy, bmesh, math, random, os, json, time
from mathutils import Vector, Matrix, noise

K = 1.0 / 1900.0          # metres per game unit
TEX = 2048

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)

def S():
    return bpy.context.scene

def link(o):
    S().collection.objects.link(o)
    return o

def N(m, t, x=0, y=0):
    n = m.node_tree.nodes.new(t)
    n.location = (x, y)
    return n

def L(m, a, b):
    m.node_tree.links.new(a, b)

def mat(name):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    return m, m.node_tree.nodes["Principled BSDF"]

def tag(m, metal=0.0, porosity=0.2):
    """what the bake needs besides the shader: the metal mask and how porous it is (wet darkens a porous thing more)"""
    m["hero_metal"] = float(metal)
    m["hero_porosity"] = float(porosity)
    return m

# ------------------------------------------------------------------------------------------------ materials (the reference's)
def plastic(name, col, rough=0.38, peel=0.06, scuff=0.32):
    m, p = mat(name)
    p.inputs["Base Color"].default_value = (*col, 1); p.inputs["Roughness"].default_value = rough
    p.inputs["Coat Weight"].default_value = 0.15
    tc = N(m, "ShaderNodeTexCoord", -900); no = N(m, "ShaderNodeTexNoise", -700); no.inputs["Scale"].default_value = 420; no.inputs["Detail"].default_value = 6
    sc = N(m, "ShaderNodeTexNoise", -700, -300); sc.inputs["Scale"].default_value = 14; sc.inputs["Detail"].default_value = 10
    L(m, tc.outputs["Object"], no.inputs["Vector"]); L(m, tc.outputs["Object"], sc.inputs["Vector"])
    ramp = N(m, "ShaderNodeValToRGB", -480, -300); ramp.color_ramp.elements[0].position = 0.52; ramp.color_ramp.elements[1].position = 0.75
    L(m, sc.outputs["Fac"], ramp.inputs["Fac"])
    mr = N(m, "ShaderNodeMapRange", -260, -300); mr.inputs["To Min"].default_value = rough; mr.inputs["To Max"].default_value = rough + scuff
    L(m, ramp.outputs["Color"], mr.inputs["Value"]); L(m, mr.outputs["Result"], p.inputs["Roughness"])
    # scuffs also lighten the colour a little (worn plastic goes chalky)
    mix = N(m, "ShaderNodeMix", -260, 200); mix.data_type = "RGBA"; mix.inputs[6].default_value = (*col, 1)
    mix.inputs[7].default_value = (min(1, col[0] * 1.25 + 0.04), min(1, col[1] * 1.25 + 0.04), min(1, col[2] * 1.25 + 0.04), 1)
    f = N(m, "ShaderNodeMath", -400, 200); f.operation = "MULTIPLY"; f.inputs[1].default_value = 0.35
    L(m, ramp.outputs["Color"], f.inputs[0]); L(m, f.outputs[0], mix.inputs[0]); L(m, mix.outputs[2], p.inputs["Base Color"])
    bp = N(m, "ShaderNodeBump", -260); bp.inputs["Strength"].default_value = peel; bp.inputs["Distance"].default_value = 0.0004
    L(m, no.outputs["Fac"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return tag(m, 0.0, 0.15)

def metal(name, col=(0.78, 0.79, 0.80), rough=0.28):
    m, p = mat(name); p.inputs["Base Color"].default_value = (*col, 1); p.inputs["Metallic"].default_value = 1; p.inputs["Roughness"].default_value = rough
    tc = N(m, "ShaderNodeTexCoord", -900); no = N(m, "ShaderNodeTexNoise", -700); no.inputs["Scale"].default_value = 160; no.inputs["Detail"].default_value = 8
    L(m, tc.outputs["Object"], no.inputs["Vector"])
    mr = N(m, "ShaderNodeMapRange", -400); mr.inputs["From Min"].default_value = 0.4; mr.inputs["From Max"].default_value = 0.7
    mr.inputs["To Min"].default_value = rough; mr.inputs["To Max"].default_value = rough + 0.2
    L(m, no.outputs["Fac"], mr.inputs["Value"]); L(m, mr.outputs["Result"], p.inputs["Roughness"])          # fingerprints, dull spots
    return tag(m, 1.0, 0.0)

def wood(name, light=(0.62, 0.40, 0.19), dark=(0.30, 0.16, 0.07), grain=22.0, varnish=0.35):
    m, p = mat(name)
    tc = N(m, "ShaderNodeTexCoord", -1100); mp = N(m, "ShaderNodeMapping", -900); mp.inputs["Scale"].default_value = (grain, grain, 1.2)
    wv = N(m, "ShaderNodeTexNoise", -700); wv.inputs["Scale"].default_value = 6; wv.inputs["Detail"].default_value = 12; wv.inputs["Roughness"].default_value = 0.65
    L(m, tc.outputs["Object"], mp.inputs["Vector"]); L(m, mp.outputs["Vector"], wv.inputs["Vector"])
    cr = N(m, "ShaderNodeValToRGB", -480); e = cr.color_ramp.elements; e[0].position = 0.3; e[0].color = (*dark, 1); e[1].position = 0.75; e[1].color = (*light, 1)
    L(m, wv.outputs["Fac"], cr.inputs["Fac"]); L(m, cr.outputs["Color"], p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = 0.42; p.inputs["Coat Weight"].default_value = varnish; p.inputs["Coat Roughness"].default_value = 0.2
    bp = N(m, "ShaderNodeBump", -260, -200); bp.inputs["Strength"].default_value = 0.12; bp.inputs["Distance"].default_value = 0.0006
    L(m, wv.outputs["Fac"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return tag(m, 0.0, 0.55)

def fibre(name, light=(0.86, 0.84, 0.78), dark=(0.50, 0.46, 0.38), dirt=(0.30, 0.25, 0.19), floor_h=0.16, twist=900.0, wet=0.0, char=0.0, axis_z=True):
    """twisted fibre: mop yarn, broom bristles. dark tips from the floor (floor_h: how high the grey reaches).
    wet / char only for the review renders: the game draws them with the state layer (M_Hero)."""
    m, p = mat(name)
    tc = N(m, "ShaderNodeTexCoord", -1300)
    tw = N(m, "ShaderNodeTexWave", -1000); tw.wave_type = "BANDS"; tw.inputs["Scale"].default_value = twist; tw.inputs["Distortion"].default_value = 1.5
    L(m, tc.outputs["Object"], tw.inputs["Vector"])
    no = N(m, "ShaderNodeTexNoise", -1000, -260); no.inputs["Scale"].default_value = 60; no.inputs["Detail"].default_value = 8
    L(m, tc.outputs["Object"], no.inputs["Vector"])
    sp = N(m, "ShaderNodeSeparateXYZ", -1000, 260); L(m, tc.outputs["Object"], sp.inputs["Vector"])
    hz = N(m, "ShaderNodeMapRange", -800, 260); hz.inputs["From Min"].default_value = 0.0; hz.inputs["From Max"].default_value = floor_h; hz.inputs["To Min"].default_value = 1; hz.inputs["To Max"].default_value = 0
    L(m, sp.outputs["Z"], hz.inputs["Value"])
    base = N(m, "ShaderNodeValToRGB", -760); e = base.color_ramp.elements; e[0].color = (*dark, 1); e[1].color = (*light, 1)
    L(m, no.outputs["Fac"], base.inputs["Fac"])
    dm_ = N(m, "ShaderNodeMix", -520); dm_.data_type = "RGBA"; dm_.inputs[7].default_value = (*dirt, 1)
    dm = N(m, "ShaderNodeMath", -700, 420); dm.operation = "MULTIPLY"; dm.inputs[1].default_value = 0.55
    L(m, hz.outputs["Result"], dm.inputs[0]); L(m, dm.outputs[0], dm_.inputs[0]); L(m, base.outputs["Color"], dm_.inputs[6])
    wetm = N(m, "ShaderNodeMix", -320); wetm.data_type = "RGBA"; wetm.blend_type = "MULTIPLY"; wetm.inputs[0].default_value = wet; wetm.inputs[7].default_value = (0.42, 0.45, 0.48, 1)
    L(m, dm_.outputs[2], wetm.inputs[6])
    ch = N(m, "ShaderNodeMath", -520, 420); ch.operation = "MULTIPLY"; ch.inputs[1].default_value = char * 1.6; ch.use_clamp = True
    cn = N(m, "ShaderNodeMath", -700, 620); cn.operation = "MULTIPLY"; L(m, hz.outputs["Result"], cn.inputs[0]); L(m, no.outputs["Fac"], cn.inputs[1])
    cn2 = N(m, "ShaderNodeMath", -520, 620); cn2.operation = "MULTIPLY"; cn2.inputs[1].default_value = 2.2; L(m, cn.outputs[0], cn2.inputs[0]); L(m, cn2.outputs[0], ch.inputs[0])
    charm = N(m, "ShaderNodeMix", -120); charm.data_type = "RGBA"; charm.inputs[7].default_value = (0.012, 0.010, 0.009, 1)
    L(m, ch.outputs[0], charm.inputs[0]); L(m, wetm.outputs[2], charm.inputs[6]); L(m, charm.outputs[2], p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = 0.92 - 0.55 * wet; p.inputs["Sheen Weight"].default_value = 0.6 * (1 - wet); p.inputs["Coat Weight"].default_value = 0.5 * wet
    if char > 0:
        em = N(m, "ShaderNodeValToRGB", -320, 620); e = em.color_ramp.elements; e[0].position = 0.80; e[0].color = (0, 0, 0, 1); e[1].position = 0.86; e[1].color = (1.0, 0.25, 0.03, 1)
        L(m, cn2.outputs[0], em.inputs["Fac"]); L(m, em.outputs["Color"], p.inputs["Emission Color"]); p.inputs["Emission Strength"].default_value = 6.0 * char
    bp = N(m, "ShaderNodeBump", -320, -300); bp.inputs["Strength"].default_value = 0.9; bp.inputs["Distance"].default_value = 0.0012
    L(m, tw.outputs["Fac"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return tag(m, 0.0, 1.0)

def rubber(name, col=(0.05, 0.05, 0.055), rough=0.8):
    m, p = mat(name); p.inputs["Base Color"].default_value = (*col, 1); p.inputs["Roughness"].default_value = rough
    tc = N(m, "ShaderNodeTexCoord", -900); no = N(m, "ShaderNodeTexNoise", -700); no.inputs["Scale"].default_value = 300; no.inputs["Detail"].default_value = 4
    L(m, tc.outputs["Object"], no.inputs["Vector"])
    bp = N(m, "ShaderNodeBump", -260); bp.inputs["Strength"].default_value = 0.1; bp.inputs["Distance"].default_value = 0.0003
    L(m, no.outputs["Fac"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"])
    return tag(m, 0.0, 0.1)

def water():
    m, p = mat("Water"); p.inputs["Base Color"].default_value = (0.55, 0.62, 0.60, 1); p.inputs["Transmission Weight"].default_value = 1; p.inputs["Roughness"].default_value = 0.03; p.inputs["IOR"].default_value = 1.33
    no = N(m, "ShaderNodeTexNoise", -500); no.inputs["Scale"].default_value = 30; bp = N(m, "ShaderNodeBump", -260); bp.inputs["Strength"].default_value = 0.08
    L(m, no.outputs["Fac"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"]); return m

# ------------------------------------------------------------------------------------------------ geometry helpers
def smooth(o, sub=1, bev=None, seg=3):
    for p in o.data.polygons: p.use_smooth = True
    if bev:
        b = o.modifiers.new("bev", "BEVEL"); b.width = bev; b.segments = seg; b.limit_method = "ANGLE"
    if sub:
        s = o.modifiers.new("sub", "SUBSURF"); s.levels = sub; s.render_levels = sub
    return o

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

def cyl(name, r, h, loc, rot=(0, 0, 0), seg=32, caps="NGON"):
    bpy.ops.mesh.primitive_cylinder_add(vertices=seg, radius=r, depth=h, location=loc, rotation=rot, end_fill_type=caps)
    o = bpy.context.object; o.name = name
    return o

def cube(name, size, loc, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc, rotation=rot)
    o = bpy.context.object; o.name = name; o.scale = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return o

def tubes(name, strands, radius, res_u=4, bevel_res=1, tip=0.75, caps=True):
    """strands: lists of points (NURBS through them, order 4). A curve object (converted to a mesh by bake_mesh)."""
    cu = bpy.data.curves.new(name, "CURVE"); cu.dimensions = "3D"; cu.bevel_depth = radius; cu.bevel_resolution = bevel_res
    cu.resolution_u = res_u; cu.use_fill_caps = caps
    for pts in strands:
        sp = cu.splines.new("NURBS"); sp.points.add(len(pts) - 1)
        for p, c in zip(sp.points, pts): p.co = (c[0], c[1], c[2], 1)
        sp.use_endpoint_u = True; sp.order_u = min(4, len(pts))
        sp.points[-1].radius = tip
    return link(bpy.data.objects.new(name, cu))

def bake_mesh(o, name=None):
    """the object with its modifiers applied (curves become meshes), transforms applied"""
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(o.evaluated_get(dg), preserve_all_data_layers=True, depsgraph=dg)
    me.transform(o.matrix_world)
    n = link(bpy.data.objects.new(name or o.name, me))
    if o.type == "CURVE" or not o.data.materials:
        pass
    for p in n.data.polygons: p.use_smooth = True
    bpy.data.objects.remove(o)
    return n

def set_part(o, part):
    """face attribute 'part' (which moving / breaking part a face belongs to), kept through the join"""
    a = o.data.attributes.get("part") or o.data.attributes.new("part", "INT", "FACE")
    for i in range(len(o.data.polygons)): a.data[i].value = part
    return o

def join(objs, name):
    objs = [bake_mesh(o) if (o.type != "MESH" or o.modifiers) else o for o in objs]
    cuts = [v for o in objs for v in o.get("cuts", [])]                 # (fracture planes of break pieces go with the join)
    for o in S().objects: o.select_set(False)
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if len(objs) > 1: bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    o.name = name; o.data.name = name
    if cuts: o["cuts"] = cuts
    return o

def tris(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)

def bounds(objs):
    mn = Vector((1e9, 1e9, 1e9)); mx = -mn
    for o in objs:
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            mn = Vector(map(min, mn, w)); mx = Vector(map(max, mx, w))
    return mn, mx

def keep_parts(src, parts, name):
    """a copy of src with only the faces of these parts"""
    o = src.copy(); o.data = src.data.copy(); o.name = name; o.data.name = name; link(o)
    bm = bmesh.new(); bm.from_mesh(o.data)
    lay = bm.faces.layers.int.get("part")
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f[lay] not in parts], context="FACES")
    bm.to_mesh(o.data); bm.free()
    a = o.data.attributes.get("surf") or o.data.attributes.new("surf", "INT", "FACE")   # (all drawn surface: see split / break_slots)
    for i in range(len(o.data.polygons)): a.data[i].value = 1
    return o

# ------------------------------------------------------------------------------------------------ UVs: one atlas for all LODs
def unwrap(o, angle=60.0, margin=0.004):
    for x in S().objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(angle), island_margin=margin, area_weight=0.0, correct_aspect=True, scale_to_bounds=False)
    try:
        bpy.ops.uv.pack_islands(rotate=True, margin=margin, shape_method="CONCAVE")
    except TypeError:
        bpy.ops.uv.pack_islands(rotate=True, margin=margin)
    bpy.ops.object.mode_set(mode="OBJECT")
    o.data.uv_layers[0].name = "UV0"

# ------------------------------------------------------------------------------------------------ bake: procedural -> game textures
def cycles(samples=64, gpu=True):
    sc = S(); sc.render.engine = "CYCLES"
    if gpu:
        pr = bpy.context.preferences.addons["cycles"].preferences
        try:
            pr.compute_device_type = "OPTIX"; pr.get_devices()
            for d in pr.devices: d.use = d.type != "CPU"
            sc.cycles.device = "GPU"
        except Exception as e:
            print("GPU off", e)
    sc.cycles.samples = samples

def _img(name, size, alpha=False, data=False):
    im = bpy.data.images.get(name)
    if im: bpy.data.images.remove(im)
    im = bpy.data.images.new(name, size, size, alpha=alpha, float_buffer=False)
    if data: im.colorspace_settings.name = "Non-Color"
    return im

def _target(o, im):
    for s in o.material_slots:
        m = s.material
        nt = m.node_tree
        t = nt.nodes.get("HERO_BAKE") or nt.nodes.new("ShaderNodeTexImage")
        t.name = "HERO_BAKE"; t.image = im; t.location = (400, 600)
        for n in nt.nodes: n.select = False
        t.select = True; nt.nodes.active = t

def _bake(o, kind, im, samples, **kw):
    _target(o, im)
    S().cycles.samples = samples
    for x in S().objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    S().render.bake.margin = 12; S().render.bake.use_clear = True
    bpy.ops.object.bake(type=kind, **kw)

def _emit_swap(o, key):
    """per material: the emission shows one constant (metal or porosity); returns the undo list"""
    undo = []
    for s in o.material_slots:
        m = s.material; p = m.node_tree.nodes.get("Principled BSDF")
        if not p: continue
        v = float(m.get(key, 0.0))
        lk = [l for l in m.node_tree.links if l.to_socket == p.inputs["Emission Color"]]
        old = (tuple(p.inputs["Emission Color"].default_value), p.inputs["Emission Strength"].default_value, [(l.from_socket, l.to_socket) for l in lk])
        for l in lk: m.node_tree.links.remove(l)
        p.inputs["Emission Color"].default_value = (v, v, v, 1); p.inputs["Emission Strength"].default_value = 1.0
        undo.append((m, p, old))
    return undo

def _emit_restore(undo):
    for m, p, (col, st, lk) in undo:
        p.inputs["Emission Color"].default_value = col; p.inputs["Emission Strength"].default_value = st
        for a, b in lk: m.node_tree.links.new(a, b)

def _px(im):
    import numpy as np
    a = np.empty(im.size[0] * im.size[1] * 4, dtype=np.float32); im.pixels.foreach_get(a)
    return a.reshape(im.size[1], im.size[0], 4)

def bake_set(o, name, out_dir, size=TEX, samples=48):
    """T_<name>_C (rgb colour), _N (tangent normal, OpenGL), _R (ao, rough, metal, a = porosity) as PNG in out_dir"""
    import numpy as np
    t0 = time.time()
    cycles(samples)
    os.makedirs(out_dir, exist_ok=True)
    if not S().world:                                              # (AO bakes against the world: none -> black)
        wd = bpy.data.worlds.new("BakeWorld"); S().world = wd; wd.use_nodes = True
        wd.node_tree.nodes["Background"].inputs["Color"].default_value = (1, 1, 1, 1)
    S().world.light_settings.distance = 0.06                       # occlusion within 6 cm: the creases, the inside of the bucket
    # colour: a metal has no diffuse and baked BLACK (brass, zinc, chrome) - metallic off for this pass (the metal mask is R.b);
    # 16 samples per texel, not 1 (one sample of a fine procedural pattern is moire)
    mu = []
    for s in o.material_slots:
        p = s.material.node_tree.nodes.get("Principled BSDF") if s.material else None
        if not p: continue
        lk = [(l.from_socket, l.to_socket) for l in s.material.node_tree.links if l.to_socket == p.inputs["Metallic"]]
        for l in [l for l in s.material.node_tree.links if l.to_socket == p.inputs["Metallic"]]: s.material.node_tree.links.remove(l)
        mu.append((s.material, p, p.inputs["Metallic"].default_value, lk)); p.inputs["Metallic"].default_value = 0.0
    col = _img(name + "_col", size); _bake(o, "DIFFUSE", col, 16, pass_filter={"COLOR"})
    for m_, p, v, lk in mu:
        p.inputs["Metallic"].default_value = v
        for a, b in lk: m_.node_tree.links.new(a, b)
    nrm = _img(name + "_nrm", size, data=True); _bake(o, "NORMAL", nrm, 4, normal_space="TANGENT")
    rgh = _img(name + "_rgh", size, data=True); _bake(o, "ROUGHNESS", rgh, 1)
    ao = _img(name + "_ao", size, data=True); _bake(o, "AO", ao, samples)
    u = _emit_swap(o, "hero_metal"); met = _img(name + "_met", size, data=True); _bake(o, "EMIT", met, 1); _emit_restore(u)
    u = _emit_swap(o, "hero_porosity"); por = _img(name + "_por", size, data=True); _bake(o, "EMIT", por, 1); _emit_restore(u)
    C = _px(col); C[..., 3] = 1.0
    # (porosity rides in R's alpha: an alpha in the colour map gets premultiplied on the way through PNG)
    R = np.stack([_px(ao)[..., 0], _px(rgh)[..., 0], _px(met)[..., 0], _px(por)[..., 0]], -1)
    paths = {}
    for kind, arr, src, data in (("C", C, None, False), ("N", None, nrm, True), ("R", R, None, True)):
        im = src or _img("T_%s_%s" % (name, kind), size, alpha=(kind == "R"), data=data)
        if arr is not None: im.pixels.foreach_set(arr.ravel())
        p = os.path.join(out_dir, "T_%s_%s.png" % (name, kind))
        im.filepath_raw = p; im.file_format = "PNG"
        if kind == "R": im.alpha_mode = "CHANNEL_PACKED"
        im.save()
        paths[kind] = p
    print("HERO bake %s %.0f s" % (name, time.time() - t0))
    return paths

def game_material(o, name):
    """after the bake: one slot named Hero_<name> (KitchenArt keeps the asset's own material for Hero_ slots) previewing the baked set"""
    m = bpy.data.materials.get("Hero_" + name) or bpy.data.materials.new("Hero_" + name)
    o.data.materials.clear(); o.data.materials.append(m)
    for p in o.data.polygons: p.material_index = 0
    return m

def preview_material(name, paths):
    """the baked set as a Blender material (review renders of what the game gets)"""
    m, p = mat("Baked_" + name)
    nt = m.node_tree
    def tex(k, data, x, y):
        t = N(m, "ShaderNodeTexImage", x, y); t.image = bpy.data.images.load(paths[k], check_existing=True)
        if data: t.image.colorspace_settings.name = "Non-Color"
        if k == "R": t.image.alpha_mode = "CHANNEL_PACKED"
        return t
    c = tex("C", False, -700, 300); r = tex("R", True, -700, 0); n = tex("N", True, -700, -300)
    sep = N(m, "ShaderNodeSeparateColor", -400, 0); L(m, r.outputs["Color"], sep.inputs["Color"])
    mul = N(m, "ShaderNodeMix", -250, 300); mul.data_type = "RGBA"; mul.blend_type = "MULTIPLY"; mul.inputs[0].default_value = 0.45   # (as M_Hero: AO darkens the colour by 45 %)
    L(m, c.outputs["Color"], mul.inputs[6]); L(m, sep.outputs["Red"], mul.inputs[7])
    L(m, mul.outputs[2], p.inputs["Base Color"]); L(m, sep.outputs["Green"], p.inputs["Roughness"]); L(m, sep.outputs["Blue"], p.inputs["Metallic"])
    nm = N(m, "ShaderNodeNormalMap", -250, -300); L(m, n.outputs["Color"], nm.inputs["Color"]); L(m, nm.outputs["Normal"], p.inputs["Normal"])
    return m

# ------------------------------------------------------------------------------------------------ LODs, hulls, break pieces
def lod(src, ratio, name):
    o = src.copy(); o.data = src.data.copy(); o.name = name; o.data.name = name; link(o)
    d = o.modifiers.new("dec", "DECIMATE"); d.decimate_type = "COLLAPSE"; d.ratio = ratio; d.use_collapse_triangulate = True
    for x in S().objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.object.modifier_apply(modifier="dec")
    return o

def hull(name, pts):
    """UCX_<name>_NN: the convex hull of a point cloud (metres, piece frame)"""
    bm = bmesh.new()
    for p in pts: bm.verts.new(p)
    bmesh.ops.convex_hull(bm, input=bm.verts[:])
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    return link(bpy.data.objects.new(name, me))

def ring_pts(r, z, n=12, cx=0.0, cy=0.0):
    return [(cx + r * math.cos(2 * math.pi * k / n), cy + r * math.sin(2 * math.pi * k / n), z) for k in range(n)]

def box_pts(x0, x1, y0, y1, z0, z1):
    return [(x, y, z) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)]

def cutter(co, no, size, jag, seed, sub=5, splinter=0.0, mat_name="Break", depth=None):
    """a big box whose face through co (normal no) is a ragged fracture: jag = noise height (m); splinter = spikes along the
    plane's in-plane axis (wood fibres)"""
    no = Vector(no).normalized()
    bpy.ops.mesh.primitive_cube_add(size=1)
    c = bpy.context.object; c.name = "cutter"
    bm = bmesh.new(); bm.from_mesh(c.data)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=2 ** sub, use_grid_fill=True)
    rnd = random.Random(seed)
    off = Vector((rnd.uniform(0, 50), rnd.uniform(0, 50), rnd.uniform(0, 50)))
    for v in bm.verts:
        if v.co.z > 0.499:                                         # the top face is the fracture
            p = v.co * 4.0 + off
            h = noise.noise(p) * jag + noise.noise(p * 3.1) * jag * 0.35
            if splinter:
                h += splinter * max(0.0, noise.noise(Vector((v.co.x * 40, v.co.y * 2.5, 0)) + off)) ** 2 * 4
            v.co.z = 0.5 + h / (depth or size)
    bm.to_mesh(c.data); bm.free()
    m = bpy.data.materials.get(mat_name) or plastic(mat_name, (0.8, 0.8, 0.78), 0.7)
    c.data.materials.append(m)
    # box below the plane: top face on the plane, normal along no
    q = Vector((0, 0, 1)).rotation_difference(no)
    c.matrix_world = Matrix.Translation(Vector(co)) @ q.to_matrix().to_4x4() @ Matrix.Translation((0, 0, -0.5 * (depth or size))) @ Matrix.Diagonal((size, size, depth or size, 1))
    c["cut"] = [co[0], co[1], co[2], no.x, no.y, no.z, abs(jag) * 2.5 + splinter * 6 + 0.002]   # where the fracture is (break_slots)
    return c

def split(o, ct, keep_name, other_name):
    """o minus the cutter -> keep; o and the cutter -> other. Both keep o's UVs; the fracture faces are those without the
    'surf' face attribute (the cutter has none): break_slots gives them the Break slot"""
    if "surf" not in o.data.attributes:
        a = o.data.attributes.new("surf", "INT", "FACE")
        for i in range(len(o.data.polygons)): a.data[i].value = 1
    b = o.copy(); b.data = o.data.copy(); link(b); b.name = other_name
    for x, op in ((o, "DIFFERENCE"), (b, "INTERSECT")):
        for solver in ("MANIFOLD", "EXACT"):                       # (MANIFOLD refuses open shells: then EXACT)
            before = (len(x.data.polygons), len(x.data.vertices))
            md = x.modifiers.new("cut", "BOOLEAN"); md.operation = op; md.object = ct
            try: md.solver = solver
            except Exception: md.solver = "EXACT"
            try: md.material_mode = "TRANSFER"
            except Exception: pass
            for y in S().objects: y.select_set(False)
            x.select_set(True); bpy.context.view_layer.objects.active = x
            bpy.ops.object.modifier_apply(modifier="cut")
            left = x.modifiers.get("cut")
            if left: x.modifiers.remove(left)                       # (a refused apply leaves the modifier on)
            if (len(x.data.polygons), len(x.data.vertices)) != before: break
    for x in (o, b): x["cuts"] = list(x.get("cuts", [])) + list(ct["cut"])
    o.name = keep_name
    bpy.data.objects.remove(ct)
    return o, b

def is_fracture(o, p):
    """a face on one of the cut planes of this piece (within the ragged band) and facing along it: a fresh fracture face"""
    c = o.get("cuts", [])
    for k in range(0, len(c) - 6, 7):
        n = Vector(c[k + 3:k + 6])
        if abs((p.center - Vector(c[k:k + 3])).dot(n)) < c[k + 6] and abs(p.normal.dot(n)) > 0.5:
            return True
    return False

def break_slots(o):
    """slot 0 = the surface (Hero_<name>), slot 1 = Break (fracture faces)"""
    names = [m.name if m else "" for m in o.data.materials]
    brk = [i for i, n in enumerate(names) if n.startswith("Break")]
    hero = [i for i, n in enumerate(names) if n.startswith("Hero_")]
    flags = [p.material_index in brk or is_fracture(o, p) for p in o.data.polygons]
    if not bpy.data.materials.get("Break"): plastic("Break", (0.8, 0.8, 0.78), 0.7)
    ms = [bpy.data.materials[names[hero[0]]] if hero else o.data.materials[0], bpy.data.materials.get("Break")]
    o.data.materials.clear()                                      # (resets the face indices: set them after)
    for m in ms: o.data.materials.append(m)
    for p, cut in zip(o.data.polygons, flags): p.material_index = 1 if cut else 0
    print("HERO break %s: %d fracture faces of %d" % (o.name, sum(flags), len(flags)))
    for u in [u for u in o.data.uv_layers if u.name != "UV0"]:     # (the cutter brings its own UV layer: UV0 must stay channel 0)
        o.data.uv_layers.remove(u)
    return o

# ------------------------------------------------------------------------------------------------ export + manifest
def export_fbx(objs, path):
    for x in S().objects: x.select_set(False)
    for x in objs: x.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, use_mesh_modifiers=True,
                             mesh_smooth_type="FACE", bake_anim=False, add_leaf_bones=False, use_tspace=True)

def info(o):
    mn, mx = bounds([o])
    return {"triangles": tris(o), "bbox_min_m": [round(v, 4) for v in mn], "bbox_max_m": [round(v, 4) for v in mx],
            "materials": [m.name for m in o.data.materials if m]}

# ------------------------------------------------------------------------------------------------ review renders (the reference's studio)
def tiles():
    m, p = mat("Tiles"); tc = N(m, "ShaderNodeTexCoord", -900); br = N(m, "ShaderNodeTexBrick", -650)
    br.offset = 0; br.inputs["Scale"].default_value = 3.3; br.inputs["Mortar Size"].default_value = 0.012; br.inputs["Brick Width"].default_value = 1.0; br.inputs["Row Height"].default_value = 1.0
    br.inputs["Color1"].default_value = (0.74, 0.70, 0.62, 1); br.inputs["Color2"].default_value = (0.70, 0.66, 0.58, 1); br.inputs["Mortar"].default_value = (0.36, 0.33, 0.29, 1)
    L(m, tc.outputs["Object"], br.inputs["Vector"]); L(m, br.outputs["Color"], p.inputs["Base Color"])
    mr = N(m, "ShaderNodeMapRange", -380, -260); mr.inputs["To Min"].default_value = 0.16; mr.inputs["To Max"].default_value = 0.7; L(m, br.outputs["Fac"], mr.inputs["Value"]); L(m, mr.outputs["Result"], p.inputs["Roughness"])
    bp = N(m, "ShaderNodeBump", -380, -480); bp.invert = True; bp.inputs["Strength"].default_value = 0.6; bp.inputs["Distance"].default_value = 0.002
    L(m, br.outputs["Fac"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"]); return m

def wall_mat():
    m, p = mat("Wall"); p.inputs["Base Color"].default_value = (0.80, 0.76, 0.66, 1); p.inputs["Roughness"].default_value = 0.85
    no = N(m, "ShaderNodeTexNoise", -500); no.inputs["Scale"].default_value = 180; bp = N(m, "ShaderNodeBump", -260); bp.inputs["Strength"].default_value = 0.15; bp.inputs["Distance"].default_value = 0.001
    L(m, no.outputs["Fac"], bp.inputs["Height"]); L(m, bp.outputs["Normal"], p.inputs["Normal"]); return m

def stage(wall_y=0.42, wall_x=None):
    bpy.ops.mesh.primitive_plane_add(size=12, location=(0, 0, 0)); f = bpy.context.object; f.name = "Floor"; f.data.materials.append(tiles())
    w = wall_mat(); sk = plastic("Skirting", (0.9, 0.88, 0.82), 0.45)
    bpy.ops.mesh.primitive_plane_add(size=12, location=(0, wall_y, 3), rotation=(math.pi / 2, 0, 0)); bpy.context.object.data.materials.append(w)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, wall_y - 0.015, 0.045)); o = bpy.context.object; o.scale = (12, 0.03, 0.09); o.data.materials.append(sk)
    if wall_x is not None:
        bpy.ops.mesh.primitive_plane_add(size=12, location=(wall_x, 0, 3), rotation=(0, math.pi / 2, 0)); bpy.context.object.data.materials.append(w)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(wall_x - 0.015 * (1 if wall_x > 0 else -1), 0, 0.045)); o = bpy.context.object; o.scale = (0.03, 12, 0.09); o.data.materials.append(sk)
    wd = bpy.data.worlds.new("W"); S().world = wd; wd.use_nodes = True; bg = wd.node_tree.nodes["Background"]; bg.inputs["Color"].default_value = (0.95, 0.80, 0.62, 1); bg.inputs["Strength"].default_value = 0.10
    def area(loc, rot, size, power, col):
        l = bpy.data.lights.new("a", "AREA"); l.size = size; l.energy = power; l.color = col
        o = link(bpy.data.objects.new("a", l)); o.location = loc; o.rotation_euler = [math.radians(v) for v in rot]
    area((-1.6, -1.8, 2.0), (52, 0, -42), 1.6, 95, (1.0, 0.84, 0.64))
    area((1.9, -1.2, 0.9), (72, 0, 58), 1.2, 16, (0.70, 0.82, 1.0))
    area((0.3, 0.2, 2.4), (0, 0, 0), 1.0, 12, (1.0, 0.95, 0.9))

def camera(loc, look, lens, fstop=5.6):
    c = bpy.data.cameras.new("c"); c.lens = lens; c.dof.use_dof = True; c.dof.aperture_fstop = fstop; c.clip_start = 0.002
    o = link(bpy.data.objects.new("c", c)); o.location = loc
    d = Vector(look) - Vector(loc); o.rotation_euler = d.to_track_quat("-Z", "Y").to_euler(); c.dof.focus_distance = d.length; S().camera = o
    return o

def frame_camera(mn, mx, kind="wide"):
    c = (mn + mx) * 0.5; s = max((mx - mn).length, 0.2)
    if kind == "ant":
        camera((c.x + 0.05, mn.y - 0.35 * s, 0.012), (c.x, c.y, mn.z + 0.3 * (mx.z - mn.z)), 20, 8)
    else:
        camera((c.x + 0.6 * s, mn.y - 1.1 * s, c.z + 0.25 * s), (c.x, c.y, c.z), 40)

def render(path, w=1600, h=900, samples=160):
    cycles(samples)
    sc = S(); sc.cycles.use_denoising = True; sc.render.resolution_x = w; sc.render.resolution_y = h
    sc.view_settings.view_transform = "AgX"; sc.view_settings.look = "AgX - Medium High Contrast"
    sc.render.image_settings.file_format = "JPEG"; sc.render.image_settings.quality = 90; sc.render.filepath = path
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.render.render(write_still=True); print("RENDERED", path)

def sheet(paths, out, cols=2):
    """several renders side by side on one review image (numpy, no PIL in Blender)"""
    import numpy as np
    ims = [bpy.data.images.load(p) for p in paths]
    w, h = ims[0].size
    rows = (len(ims) + cols - 1) // cols
    big = np.ones((rows * h, cols * w, 4), np.float32)
    for k, im in enumerate(ims):
        a = np.empty(w * h * 4, np.float32); im.pixels.foreach_get(a); a = a.reshape(h, w, 4)
        r, c = k // cols, k % cols
        big[(rows - 1 - r) * h:(rows - r) * h, c * w:(c + 1) * w] = a
    o = bpy.data.images.new("sheet", cols * w, rows * h); o.pixels.foreach_set(big.ravel())
    o.filepath_raw = out; o.file_format = "JPEG"; o.save()
    print("SHEET", out)

# ------------------------------------------------------------------------------------------------ the states, as the game draws them (M_Hero)
def state_material(base, wet=0.0, char=0.0, seed=0.0):
    """a copy of a baked preview material with the state layer of M_Hero: wet darkens by porosity (C alpha) and glosses;
    char blackens through a noise threshold, with embers at the front"""
    m = base.copy(); m.name = "%s_w%.0f_c%.0f" % (base.name, wet * 10, char * 10)
    nt = m.node_tree; p = nt.nodes["Principled BSDF"]
    ctex = [n for n in nt.nodes if n.type == "TEX_IMAGE" and n.image and n.image.filepath.endswith("_R.png")][0]   # (porosity: R's alpha)
    src = p.inputs["Base Color"].links[0].from_socket
    tc = N(m, "ShaderNodeTexCoord", -1200, 800); no = N(m, "ShaderNodeTexNoise", -1000, 800); no.inputs["Scale"].default_value = 24; no.inputs["Detail"].default_value = 6
    L(m, tc.outputs["Object"], no.inputs["Vector"])
    # wet
    pw = N(m, "ShaderNodeMath", -600, 900); pw.operation = "MULTIPLY"; pw.inputs[1].default_value = wet * 0.6
    L(m, ctex.outputs["Alpha"], pw.inputs[0])
    dk = N(m, "ShaderNodeMix", -400, 700); dk.data_type = "RGBA"; dk.blend_type = "MULTIPLY"; dk.inputs[7].default_value = (0.35, 0.36, 0.38, 1)
    L(m, pw.outputs[0], dk.inputs[0]); L(m, src, dk.inputs[6])
    rsrc = p.inputs["Roughness"].links[0].from_socket
    rw = N(m, "ShaderNodeMix", -400, 500); rw.data_type = "FLOAT"; rw.inputs[0].default_value = wet * 0.85; rw.inputs[3].default_value = 0.06
    L(m, rsrc, rw.inputs[2]); L(m, rw.outputs[0], p.inputs["Roughness"])
    # char
    cm = N(m, "ShaderNodeMapRange", -600, 1100); cm.inputs["From Min"].default_value = 1.05 - char * 1.25; cm.inputs["From Max"].default_value = 1.30 - char * 1.25
    ad = N(m, "ShaderNodeMath", -800, 1100); ad.operation = "ADD"; ad.inputs[1].default_value = 0.15 + seed
    L(m, no.outputs["Fac"], ad.inputs[0]); L(m, ad.outputs[0], cm.inputs["Value"])
    bl = N(m, "ShaderNodeMix", -200, 700); bl.data_type = "RGBA"; bl.inputs[7].default_value = (0.02, 0.017, 0.014, 1)
    if char > 0: L(m, cm.outputs["Result"], bl.inputs[0])
    else: bl.inputs[0].default_value = 0.0
    L(m, dk.outputs[2], bl.inputs[6]); L(m, bl.outputs[2], p.inputs["Base Color"])
    if char > 0:
        em = N(m, "ShaderNodeValToRGB", -400, 1300); e = em.color_ramp.elements; e[0].position = 0.40; e[0].color = (0, 0, 0, 1); e[1].position = 0.47; e[1].color = (1.0, 0.25, 0.03, 1)
        e2 = em.color_ramp.elements.new(0.54); e2.color = (0, 0, 0, 1)
        L(m, cm.outputs["Result"], em.inputs["Fac"]); L(m, em.outputs["Color"], p.inputs["Emission Color"]); p.inputs["Emission Strength"].default_value = 3.0 * char
    return m

# ------------------------------------------------------------------------------------------------ one asset, start to end
def produce(spec, out_root, rev_root, nobake=False):
    """spec: name, parts [(obj, part)], movers {part: (name, pivot, axis, rest_deg, limit_deg)}, hulls(name) -> [obj],
    lods (ratio1, ratio2), collision_lod, breaks(lod0_obj, name) -> [piece objs], sway (part ids whose vertex colour R sways)"""
    t0 = time.time()
    name = spec["name"]; out = os.path.join(out_root, name); os.makedirs(out, exist_ok=True)
    objs = []
    for o, part in spec["parts"]:
        m = bake_mesh(o) if (o.type != "MESH" or o.modifiers) else o
        set_part(m, part)
        objs.append(m)
    # sway weight (vertex colour R): 0 at the top of the swaying part, 1 at its far ends
    sway = spec.get("sway")
    for m in objs:
        ca = m.data.color_attributes.get("Col") or m.data.color_attributes.new("Col", "BYTE_COLOR", "POINT")
        part = m.data.attributes["part"].data[0].value if len(m.data.polygons) else 0
        for i, v in enumerate(m.data.vertices):
            wgt = 0.0
            if sway and part in sway.get("parts", ()):
                wgt = max(0.0, min(1.0, (sway["from"] - (m.matrix_world @ v.co).z) / sway["span"]))
            ca.data[i].color = (wgt, 0.0, 0.0, 1.0)
    lod0 = join(objs, name)
    unwrap(lod0, spec.get("uv_angle", 60.0))
    tdir = os.path.join(out, "tex")
    if nobake and os.path.exists(os.path.join(tdir, "T_%s_C.png" % name)):
        paths = {k: os.path.join(tdir, "T_%s_%s.png" % (name, k)) for k in "CNR"}
    else:
        paths = bake_set(lod0, name, tdir, spec.get("tex", TEX))
    game_material(lod0, name)
    man = {"name": name, "file": name + ".fbx", "set": "hero", "textures": {k: os.path.relpath(v, out_root).replace("\\", "/") for k, v in paths.items()},
           "collision_lod": spec.get("collision_lod", 0), "movers": [], "breaks": [], "note": spec.get("note", "")}
    if spec.get("level_box"):   # the box the level tools check with (Tools/level/level_v3_lib.local_box), when the mesh box would mislead
        man["level_bbox_min_m"], man["level_bbox_max_m"] = [list(v) for v in spec["level_box"]]
    # moving parts out into their own meshes (pivot at the origin)
    body_parts = sorted(set(a.value for a in lod0.data.attributes["part"].data))
    mov_objs = []
    for part, (mname, pivot, axis, rest, limit) in spec.get("movers", {}).items():
        mo = keep_parts(lod0, [part], mname)
        mo.data.transform(Matrix.Translation(-Vector(pivot)))
        mov_objs.append((mo, pivot, axis, rest))
        body_parts = [p for p in body_parts if p != part]
        man["movers"].append({"name": mname, "file": mname + ".fbx", "pivot_m": list(pivot), "axis": list(axis), "rest_deg": rest, "limit_deg": limit, **info(mo)})
    if spec.get("movers"):
        body = keep_parts(lod0, body_parts, name + "_body"); bpy.data.objects.remove(lod0); body.name = name; body.data.name = name; lod0 = body
    hs = spec["hulls"](name)
    l1 = lod(lod0, spec["lods"][0], name + "_LOD1"); l2 = lod(lod0, spec["lods"][1], name + "_LOD2")
    export_fbx([lod0] + hs, os.path.join(out, name + ".fbx"))
    export_fbx([l1], os.path.join(out, name + "_LOD1.fbx")); export_fbx([l2], os.path.join(out, name + "_LOD2.fbx"))
    man.update(info(lod0)); man["lod_triangles"] = [tris(lod0), tris(l1), tris(l2)]; man["ucx"] = len(hs)
    for mo, pivot, axis, rest in mov_objs:
        export_fbx([mo], os.path.join(out, mo.name + ".fbx"))
    pieces = spec["breaks"](lod0, name) if spec.get("breaks") else []
    for pc in pieces:
        break_slots(pc)
        export_fbx([pc], os.path.join(out, pc.name + ".fbx"))
        man["breaks"].append({"name": pc.name, "file": pc.name + ".fbx", **info(pc)})
    for o in hs + [l1, l2]: bpy.data.objects.remove(o)
    print("HERO %s: LOD %s, %d hulls, %d break pieces, movers %d, %.0f s" % (name, man["lod_triangles"], man["ucx"], len(pieces), len(mov_objs), time.time() - t0))
    return man, lod0, [m for m, *_ in mov_objs], pieces, paths
