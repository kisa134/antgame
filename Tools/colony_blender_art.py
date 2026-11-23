# The colony screen's layered sprites (docs/COLONY.md), modelled and rendered in Blender: the Queen in five moods and her crowns,
# the tier overlays of the chambers (matchstick frames, bottle-cap / foil decorations), thrones, larvae, cocoons, food piles,
# furniture and trophies. Stylised 3D, warm light, transparent background, so they sit on the Higgsfield plates.
# Run: blender.exe -b --factory-startup -P Tools/colony_blender_art.py -- <out dir> [name ...]
import bpy, math, sys, os, random
from mathutils import Vector

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = ARGS[0] if ARGS else os.path.join(os.path.dirname(__file__), "..", "Art", "Colony", "blender")
ONLY = set(ARGS[1:])
os.makedirs(OUT, exist_ok=True)

# ------------------------------------------------------------------------------------------------ scene
def reset(res_x, res_y, ortho, center=(0.0, 0.0), samples=48):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    _mats.clear()
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    try:
        prefs = bpy.context.preferences.addons["cycles"].preferences
        for kind in ("OPTIX", "CUDA"):
            try:
                prefs.compute_device_type = kind
                prefs.get_devices()
                for d in prefs.devices:
                    d.use = True
                sc.cycles.device = "GPU"
                break
            except Exception:
                continue
    except Exception:
        pass
    sc.cycles.samples = samples
    sc.cycles.use_denoising = True
    sc.render.film_transparent = True
    sc.render.resolution_x = res_x
    sc.render.resolution_y = res_y
    sc.render.resolution_percentage = 100
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"
    try:
        sc.view_settings.view_transform = "Standard"
    except Exception:
        pass
    sc.view_settings.exposure = 0.0
    w = bpy.data.worlds.new("W")
    sc.world = w
    w.use_nodes = True
    bg = w.node_tree.nodes.get("Background")
    bg.inputs[0].default_value = (0.35, 0.22, 0.14, 1.0)
    bg.inputs[1].default_value = 0.55
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = ortho
    cam = bpy.data.objects.new("Cam", cam_data)
    sc.collection.objects.link(cam)
    cam.location = (center[0], -30.0, center[1])
    cam.rotation_euler = (math.radians(90), 0.0, 0.0)
    sc.camera = cam
    sun((0.55, 0.55, -0.62), 3.4, (1.0, 0.82, 0.6))    # warm key from the top left, in front
    sun((-0.75, 0.45, -0.25), 0.9, (0.75, 0.85, 1.0))  # cool fill from the right
    sun((0.25, -0.85, -0.35), 2.2, (1.0, 0.7, 0.45))   # rim from behind
    return sc

def sun(direction, strength, color):
    """A sun shining along direction (x right, y into the picture, z up)."""
    d = bpy.data.lights.new("Sun", "SUN")
    d.energy = strength
    d.color = color
    d.angle = math.radians(12)
    o = bpy.data.objects.new("Sun", d)
    bpy.context.scene.collection.objects.link(o)
    o.rotation_mode = "QUATERNION"
    o.rotation_quaternion = Vector(direction).normalized().to_track_quat("-Z", "Y")
    return o

def render(name):
    path = os.path.join(OUT, name + ".png")
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print("COLONYART wrote", path)

# ------------------------------------------------------------------------------------------------ materials
_mats = {}
def mat(name, color, rough=0.5, metal=0.0, sss=0.0, emit=None, emit_k=0.0, coat=0.0, alpha=1.0, trans=0.0):
    key = (name, color, rough, metal, sss, emit, emit_k, coat, alpha, trans)
    if key in _mats and _mats[key].name in bpy.data.materials:
        return _mats[key]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    p = m.node_tree.nodes.get("Principled BSDF")
    def setin(n, v):
        if n in p.inputs:
            p.inputs[n].default_value = v
    setin("Base Color", (*color, 1.0))
    setin("Roughness", rough)
    setin("Metallic", metal)
    setin("Subsurface Weight", sss)
    if sss > 0:
        setin("Subsurface Radius", (0.3, 0.15, 0.08))
        setin("Subsurface Scale", 0.15)
    setin("Coat Weight", coat)
    setin("Alpha", alpha)
    setin("Transmission Weight", trans)
    if emit:
        setin("Emission Color", (*emit, 1.0))
        setin("Emission Strength", emit_k)
    _mats[key] = m
    return m

def stripes(name, c1, c2, scale=6.0, rough=0.35, sss=0.25):
    """Two colours in bands along X (the Queen's abdomen, a candle)."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    p = nt.nodes.get("Principled BSDF")
    tc = nt.nodes.new("ShaderNodeTexCoord")
    wave = nt.nodes.new("ShaderNodeTexWave")
    wave.inputs["Scale"].default_value = scale
    wave.inputs["Distortion"].default_value = 1.5
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].color = (*c1, 1)
    ramp.color_ramp.elements[1].color = (*c2, 1)
    ramp.color_ramp.elements[0].position = 0.45
    ramp.color_ramp.elements[1].position = 0.6
    nt.links.new(tc.outputs["Object"], wave.inputs["Vector"])
    nt.links.new(wave.outputs["Fac"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = rough
    if "Subsurface Weight" in p.inputs:
        p.inputs["Subsurface Weight"].default_value = sss
    if "Coat Weight" in p.inputs:
        p.inputs["Coat Weight"].default_value = 0.6
    return m

def wood(name="Wood", c=(0.78, 0.56, 0.3)):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    p = nt.nodes.get("Principled BSDF")
    tc = nt.nodes.new("ShaderNodeTexCoord")
    noise = nt.nodes.new("ShaderNodeTexWave")
    noise.wave_type = "BANDS"
    noise.inputs["Scale"].default_value = 3.0
    noise.inputs["Distortion"].default_value = 6.0
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].color = (c[0] * 0.75, c[1] * 0.7, c[2] * 0.6, 1)
    ramp.color_ramp.elements[1].color = (*c, 1)
    nt.links.new(tc.outputs["Object"], noise.inputs["Vector"])
    nt.links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], p.inputs["Base Color"])
    p.inputs["Roughness"].default_value = 0.7
    return m

# ------------------------------------------------------------------------------------------------ shapes
def smooth(o):
    for p in o.data.polygons:
        p.use_smooth = True

def link_mat(o, m):
    if m is not None:
        o.data.materials.clear()
        o.data.materials.append(m)
    return o

def sphere(loc, scale, m, seg=40):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=seg, ring_count=seg // 2, radius=1.0, location=loc)
    o = bpy.context.active_object
    o.scale = scale if isinstance(scale, (tuple, list)) else (scale, scale, scale)
    smooth(o)
    return link_mat(o, m)

def cyl(loc, r, depth, m, rot=(0, 0, 0), verts=32, bevel=0.0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=depth, location=loc, rotation=rot)
    o = bpy.context.active_object
    if bevel > 0:
        b = o.modifiers.new("B", "BEVEL")
        b.width = bevel
        b.segments = 3
    smooth(o)
    return link_mat(o, m)

def box(loc, size, m, rot=(0, 0, 0), bevel=0.02):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc, rotation=rot)
    o = bpy.context.active_object
    o.scale = size
    if bevel > 0:
        b = o.modifiers.new("B", "BEVEL")
        b.width = bevel
        b.segments = 2
        b.affect = "EDGES"
    return link_mat(o, m)

def torus(loc, R, r, m, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_torus_add(major_radius=R, minor_radius=r, location=loc, rotation=rot, major_segments=48, minor_segments=16)
    o = bpy.context.active_object
    smooth(o)
    return link_mat(o, m)

def tube(points, radius, m, res=12):
    cu = bpy.data.curves.new("Tube", "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = radius
    cu.bevel_resolution = 4
    cu.use_fill_caps = True
    sp = cu.splines.new("POLY")
    sp.points.add(len(points) - 1)
    for i, p in enumerate(points):
        sp.points[i].co = (p[0], p[1], p[2], 1.0)
    if len(points) > 2:
        sp.type = "NURBS"
        sp.order_u = min(4, len(points))
        sp.use_endpoint_u = True
    o = bpy.data.objects.new("Tube", cu)
    bpy.context.scene.collection.objects.link(o)
    cu.materials.append(m)
    return o

def cone(loc, r1, r2, depth, m, rot=(0, 0, 0), verts=32):
    bpy.ops.mesh.primitive_cone_add(vertices=verts, radius1=r1, radius2=r2, depth=depth, location=loc, rotation=rot)
    o = bpy.context.active_object
    smooth(o)
    return link_mat(o, m)

def displace(o, strength=0.05, scale=0.4, subdiv=2):
    s = o.modifiers.new("S", "SUBSURF")
    s.levels = subdiv
    s.render_levels = subdiv
    tex = bpy.data.textures.new("N", "CLOUDS")
    tex.noise_scale = scale
    d = o.modifiers.new("D", "DISPLACE")
    d.texture = tex
    d.strength = strength
    return o

def crimped_cap(loc, r, h, m, rot=(0, 0, 0), teeth=21):
    """A bottle cap: a short cylinder with a crimped skirt."""
    o = cyl(loc, r, h, m, rot=rot, verts=teeth * 2, bevel=0.01)
    me = o.data
    for v in me.vertices:
        ang = math.atan2(v.co.y, v.co.x)
        k = 1.0 + 0.09 * (1 if (int(round(ang / (2 * math.pi) * teeth * 2)) % 2 == 0) else -1) * (1.0 if v.co.z < h * 0.3 else 0.25)
        v.co.x *= k
        v.co.y *= k
    return o

def matchstick(a, b, m_wood, m_head, r=0.06):
    """A matchstick from a to b with its head at b (square section)."""
    a, b = Vector(a), Vector(b)
    d = b - a
    L = d.length
    mid = (a + b) / 2
    o = box(mid, (r * 2, r * 2, L), m_wood, bevel=0.01)
    o.rotation_mode = "QUATERNION"
    o.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(d.normalized())
    h = sphere(b + d.normalized() * r * 0.6, (r * 1.7, r * 1.7, r * 2.2), m_head, seg=20)
    h.rotation_mode = "QUATERNION"
    h.rotation_quaternion = o.rotation_quaternion
    return o

def px(x, y, z=0.0):
    """Image pixels of a 1024x688 chamber picture -> world (1 unit = 100 px)."""
    return (x / 100.0, z, -y / 100.0)

# ------------------------------------------------------------------------------------------------ the Queen
QUEEN_FACES = ["Furious", "Grumpy", "Happy", "Delighted", "Sleeping"]

def queen_body():
    """Her Sweetness, side view, facing right. Returns (all body objects, head colour material)."""
    objs = []
    amber = stripes("Abdomen", (0.98, 0.66, 0.18), (0.62, 0.3, 0.05), scale=5.0)
    chit = mat("Chitin", (0.55, 0.28, 0.08), rough=0.3, coat=0.7, sss=0.1)
    headm = mat("Head", (0.66, 0.36, 0.11), rough=0.3, coat=0.7, sss=0.12)
    leg = mat("Leg", (0.3, 0.14, 0.04), rough=0.4, coat=0.4)
    a = sphere((-0.78, 0.0, 0.78), (1.0, 0.82, 0.74), amber, seg=64)
    a.rotation_euler = (0.0, math.radians(-8), 0.0)
    objs.append(a)
    objs.append(sphere((0.06, 0.0, 0.88), 0.16, chit))                      # waist
    objs.append(sphere((0.3, 0.0, 0.98), (0.34, 0.3, 0.3), chit))           # thorax
    objs.append(sphere((0.74, 0.0, 1.33), (0.37, 0.35, 0.37), headm))       # head
    # legs: three near, three far
    for side, dy, mk in ((1, -0.16, leg), (-1, 0.18, mat("LegFar", (0.2, 0.09, 0.03), rough=0.5))):
        for (p0, p1, p2) in (((0.42, 0.8), (0.72, 0.52), (0.86, 0.03)), ((0.28, 0.78), (0.2, 0.42), (0.08, 0.03)), ((0.14, 0.8), (-0.3, 0.5), (-0.5, 0.03))):
            objs.append(tube([(p0[0], dy, p0[1]), (p1[0], dy * 1.3, p1[1]), (p2[0], dy * 1.4, p2[1])], 0.035, mk))
            objs.append(sphere((p2[0], dy * 1.4, p2[1]), 0.05, mk, seg=16))
    # antennae with a ball at the tip
    for dy, k in ((-0.08, 1.0), (0.1, 0.85)):
        objs.append(tube([(0.78, dy, 1.62), (0.86, dy, 1.95), (1.05 + 0.1 * k, dy, 2.12), (1.25 * k + 0.05, dy, 2.08)], 0.028, leg))
        objs.append(sphere((1.25 * k + 0.05, dy, 2.08), 0.06, chit, seg=16))
    # wing stubs, see-through
    w = sphere((-0.05, 0.12, 1.32), (0.5, 0.04, 0.2), mat("Wing", (0.92, 0.9, 0.85), rough=0.15, alpha=0.45, trans=0.6))
    w.rotation_euler = (0.0, math.radians(-25), 0.0)
    objs.append(w)
    # a necklace of tiny pearls round the neck
    pearl = mat("Pearl", (0.96, 0.94, 0.9), rough=0.12, coat=1.0)
    for i in range(7):
        t = -0.9 + i * 0.3
        objs.append(sphere((0.5 + 0.05 * math.sin(t), -0.22 * math.cos(t) - 0.02, 1.05 - 0.16 * math.cos(t) + 0.05 * i * 0), 0.045, pearl, seg=16))
    return objs, headm

def queen_face(face, headm):
    """Eyes, brows, mouth, blush of a mood. Returns the objects."""
    objs = []
    white = mat("EyeW", (0.97, 0.97, 0.95), rough=0.15, coat=1.0)
    black = mat("Pupil", (0.02, 0.01, 0.01), rough=0.1, coat=1.0)
    shine = mat("Shine", (1, 1, 1), emit=(1, 1, 1), emit_k=4.0)
    dark = mat("Line", (0.08, 0.03, 0.01), rough=0.5)
    redhead = mat("HeadRed", (0.75, 0.2, 0.08), rough=0.3, coat=0.7, sss=0.12)
    lid = redhead if face == "Furious" else headm
    eyes = [((0.84, -0.26, 1.4), 1.0), ((1.0, -0.12, 1.39), 0.72)]
    for (c, k) in eyes:
        objs.append(sphere(c, (0.15 * k, 0.07, 0.19 * k), white))
        if face != "Sleeping":
            look = 0.03 if face != "Furious" else 0.05
            objs.append(sphere((c[0] + look, c[1] - 0.06, c[2] - 0.01), (0.075 * k, 0.03, 0.095 * k), black))
            objs.append(sphere((c[0] + look + 0.025, c[1] - 0.09, c[2] + 0.05 * k), (0.022 * k, 0.01, 0.022 * k), shine, seg=12))
            if face == "Delighted":
                objs.append(sphere((c[0] + look - 0.03, c[1] - 0.09, c[2] - 0.04 * k), (0.012 * k, 0.01, 0.012 * k), shine, seg=12))
    # lids: grumpy half shut, sleeping shut
    if face in ("Grumpy", "Sleeping", "Furious"):
        for (c, k) in eyes:
            cover = {"Grumpy": 0.09, "Furious": 0.12, "Sleeping": 0.0}[face]
            objs.append(sphere((c[0], c[1] - 0.012, c[2] + cover * k), (0.158 * k, 0.074, (0.19 - cover * 0.6) * k), lid))
            if face == "Sleeping":
                objs.append(tube([(c[0] - 0.12 * k, c[1] - 0.09, c[2] - 0.02), (c[0], c[1] - 0.095, c[2] - 0.08 * k), (c[0] + 0.12 * k, c[1] - 0.09, c[2] - 0.02)], 0.014, dark))
    else:
        # eyelashes
        for i in range(3):
            x = 0.76 + i * 0.07
            objs.append(tube([(x, -0.32, 1.55 - abs(i - 1) * 0.02), (x + 0.03, -0.33, 1.64), (x + 0.07, -0.33, 1.67)], 0.012, dark))
    # brows
    if face == "Grumpy":
        objs.append(tube([(0.72, -0.31, 1.64), (0.84, -0.33, 1.62), (0.95, -0.32, 1.57)], 0.02, dark))
    if face == "Furious":
        objs.append(tube([(0.7, -0.31, 1.7), (0.84, -0.33, 1.63), (0.98, -0.32, 1.52)], 0.03, dark))
        objs.append(sphere((0.74, 0.0, 1.33), (0.375, 0.355, 0.375), mat("Flush", (0.85, 0.12, 0.05), rough=0.3, alpha=0.35)))
    # mouth
    mouth_y = -0.31
    if face == "Happy":
        objs.append(tube([(0.9, mouth_y, 1.19), (0.98, mouth_y - 0.02, 1.12), (1.07, mouth_y + 0.04, 1.17)], 0.018, dark))
    elif face == "Delighted":
        objs.append(sphere((0.98, mouth_y + 0.01, 1.13), (0.09, 0.04, 0.07), dark))
        objs.append(sphere((0.98, mouth_y - 0.01, 1.1), (0.055, 0.03, 0.03), mat("Tongue", (0.95, 0.35, 0.4), rough=0.4)))
    elif face == "Grumpy":
        objs.append(tube([(0.91, mouth_y, 1.12), (0.99, mouth_y - 0.02, 1.15), (1.06, mouth_y + 0.03, 1.12)], 0.017, dark))
    elif face == "Furious":
        objs.append(sphere((0.98, mouth_y + 0.01, 1.12), (0.08, 0.04, 0.05), dark))
        objs.append(box((0.98, mouth_y - 0.03, 1.145), (0.11, 0.02, 0.025), white, bevel=0.005))   # teeth
    else:
        objs.append(sphere((0.98, mouth_y, 1.14), (0.025, 0.02, 0.022), dark))
    if face in ("Happy", "Delighted"):
        objs.append(sphere((0.9, -0.3, 1.24), (0.08, 0.02, 0.05), mat("Blush", (1.0, 0.45, 0.45), rough=0.8, alpha=0.6)))
    return objs

def crown(kind):
    objs = []
    gold = mat("Gold", (1.0, 0.72, 0.22), rough=0.22, metal=1.0)
    c = crimped_cap((0.72, 0.0, 1.74), 0.24, 0.15, gold, rot=(0.0, math.radians(-12), 0.0))
    objs.append(c)
    if kind == 3:
        for i, col in enumerate(((0.9, 0.1, 0.15), (0.15, 0.4, 0.95), (0.1, 0.75, 0.3), (0.9, 0.1, 0.15), (0.15, 0.4, 0.95))):
            a = math.radians(-120 + i * 60)
            objs.append(sphere((0.72 + 0.25 * math.cos(a) * 0.95, -0.25 * abs(math.sin(a)) - 0.02, 1.74 + 0.02), 0.045, mat("Bead%d" % i, col, rough=0.1, coat=1.0, trans=0.3), seg=16))
        objs.append(sphere((0.7, 0.0, 1.92), 0.07, mat("Pearl", (0.96, 0.94, 0.9), rough=0.12, coat=1.0)))
        # a cape of foil down her back
        bpy.ops.mesh.primitive_plane_add(size=1.0, location=(-0.3, 0.32, 1.0))
        cape = bpy.context.active_object
        cape.scale = (1.2, 1.0, 0.9)
        cape.rotation_euler = (math.radians(90), math.radians(-25), 0.0)
        link_mat(cape, mat("Foil", (0.85, 0.82, 0.9), rough=0.25, metal=1.0))
        displace(cape, 0.08, 0.25, 4)
        objs.append(cape)
    return objs

def build_queen(names):
    for face in QUEEN_FACES:
        nm = "Queen_" + face
        if names and nm not in names:
            continue
        reset(768, 768, 3.5, (0.0, 1.1), samples=64)
        queen_body_objs, headm = queen_body()
        queen_face(face, headm)
        render(nm)
    for kind in (2, 3):
        nm = "Crown_%d" % kind
        if names and nm not in names:
            continue
        reset(768, 768, 3.5, (0.0, 1.1), samples=64)
        body, headm = queen_body()
        queen_face("Happy", headm)
        for o in bpy.context.scene.objects:
            if o.type in ("MESH", "CURVE"):
                o.is_holdout = True
        crown(kind)
        render(nm)

# ------------------------------------------------------------------------------------------------ chamber overlays (1024x688 framing)
def build_frame():
    reset(1024, 688, 10.24, (5.12, -3.44))
    wd = wood("Match", (0.86, 0.66, 0.38))
    head = mat("MatchHead", (0.75, 0.08, 0.05), rough=0.6)
    tooth = wood("Toothpick", (0.9, 0.78, 0.55))
    matchstick(px(182, 478), px(182, 212), wd, head, r=0.075)
    matchstick(px(842, 478), px(842, 212), wd, head, r=0.075)
    o = box(Vector(px(512, 204)), (7.1, 0.12, 0.13), tooth, bevel=0.02)   # the ceiling beam: a toothpick
    matchstick(px(182, 318), px(300, 212), wd, head, r=0.05)
    matchstick(px(842, 318), px(724, 212), wd, head, r=0.05)
    # planks of a match-wood floor at the front
    for i, x in enumerate(range(250, 790, 90)):
        box(Vector(px(x + 40, 512, -0.4)), (0.86, 0.5, 0.07), wd, rot=(math.radians(8), 0, math.radians(random.uniform(-2, 2))), bevel=0.01)
    # thread lashings where the beam meets the posts
    thread = mat("Thread", (0.85, 0.82, 0.75), rough=0.9)
    for x in (182, 842):
        torus(Vector(px(x, 210)), 0.11, 0.02, thread, rot=(0, math.radians(90), 0))
    render("Frame")

def build_deco():
    reset(1024, 688, 10.24, (5.12, -3.44))
    thread = mat("Thread", (0.85, 0.82, 0.75), rough=0.9)
    caps = [mat("CapRed", (0.8, 0.1, 0.08), rough=0.25, metal=0.8), mat("CapGold", (1.0, 0.72, 0.22), rough=0.25, metal=1.0)]
    glow = mat("Glow", (1.0, 0.7, 0.3), emit=(1.0, 0.65, 0.25), emit_k=18.0)
    for i, x in enumerate((372, 652)):
        tube([px(x, 208), px(x, 252)], 0.012, thread)
        crimped_cap(Vector(px(x, 262)), 0.28, 0.16, caps[i], rot=(math.radians(180), 0, 0))
        sphere(Vector(px(x, 290)), 0.12, glow, seg=20)
    # a garland of thread with buttons
    pts = [px(196, 232), px(330, 268), px(512, 286), px(694, 268), px(828, 232)]
    tube([(p[0], -0.3, p[2]) for p in pts], 0.012, thread)
    cols = [(0.15, 0.55, 0.6), (0.9, 0.55, 0.1), (0.6, 0.15, 0.5), (0.2, 0.5, 0.2), (0.85, 0.2, 0.2)]
    for i, t in enumerate((0.12, 0.3, 0.5, 0.7, 0.88)):
        x = 196 + (828 - 196) * t
        y = 232 + 54 * math.sin(t * math.pi) + 18
        b = cyl(Vector(px(x, y, -0.35)), 0.15, 0.05, mat("Btn%d" % i, cols[i], rough=0.3, coat=0.8), rot=(math.radians(90), 0, 0), verts=24, bevel=0.015)
        for hx, hz in ((-0.04, 0.04), (0.04, 0.04), (-0.04, -0.04), (0.04, -0.04)):
            cyl(Vector((b.location.x + hx, b.location.y - 0.03, b.location.z + hz)), 0.022, 0.03, mat("Hole", (0.05, 0.03, 0.02)), rot=(math.radians(90), 0, 0), verts=10)
    # foil scraps as rugs at the front corners
    foil = mat("Foil", (0.86, 0.84, 0.9), rough=0.22, metal=1.0)
    for x in (262, 768):
        bpy.ops.mesh.primitive_plane_add(size=1.0, location=Vector(px(x, 528, -0.6)))
        f = bpy.context.active_object
        f.scale = (1.4, 0.6, 1.0)
        f.rotation_euler = (math.radians(70), 0, math.radians(random.uniform(-6, 6)))
        link_mat(f, foil)
        displace(f, 0.05, 0.2, 4)
    render("Deco")

# ------------------------------------------------------------------------------------------------ thrones
def matchbox_throne():
    card = mat("Card", (0.78, 0.58, 0.34), rough=0.85)
    label = mat("Label", (0.78, 0.12, 0.08), rough=0.6)
    stripe = mat("Stripe", (0.98, 0.78, 0.2), rough=0.6)
    box((3.0, 0.2, -3.9), (2.5, 1.4, 0.9), card, bevel=0.03)          # the seat: a matchbox on its side
    box((3.0, -0.52, -3.9), (2.3, 0.02, 0.55), label, bevel=0.0)
    box((3.0, -0.54, -3.9), (2.3, 0.02, 0.1), stripe, bevel=0.0)
    box((3.0, 0.75, -2.65), (2.5, 0.4, 2.2), card, bevel=0.03)        # the back: another box standing
    box((3.0, 0.53, -2.5), (2.3, 0.02, 1.3), label, bevel=0.0)
    sphere((3.0, -0.05, -3.38), (1.0, 0.55, 0.16), mat("Velvet", (0.6, 0.08, 0.12), rough=0.9))   # the cushion
    gold = mat("Gold", (1.0, 0.72, 0.22), rough=0.22, metal=1.0)
    for x in (1.85, 4.15):
        sphere((x, 0.5, -1.55), 0.16, gold)
        box((x, -0.1, -3.25), (0.22, 1.2, 0.5), card, bevel=0.04)    # arm rests

def build_seats():
    reset(600, 500, 6.0, (3.0, -2.5))
    matchbox_throne()
    render("Seat_2")
    reset(600, 500, 6.0, (3.0, -2.5))
    matchbox_throne()
    tooth = wood("Toothpick", (0.9, 0.78, 0.55))
    for x in (1.75, 4.25):
        cyl((x, 0.75, -2.0), 0.05, 3.2, tooth, verts=12)
    crimped_cap((3.0, 0.75, -0.42), 1.45, 0.3, mat("CapRed", (0.8, 0.1, 0.08), rough=0.25, metal=0.8), rot=(math.radians(180), 0, 0), teeth=27)
    bpy.ops.mesh.primitive_plane_add(size=1.0, location=(3.0, 1.0, -1.4))
    f = bpy.context.active_object
    f.scale = (2.6, 1.0, 1.8)
    f.rotation_euler = (math.radians(90), 0, 0)
    link_mat(f, mat("Foil", (0.9, 0.85, 0.7), rough=0.25, metal=1.0))
    displace(f, 0.1, 0.25, 4)
    render("Seat_3")

# ------------------------------------------------------------------------------------------------ props
def small(name, w, h, scale_px=100.0):
    """A prop picture w x h pixels, origin at its centre (1 unit = scale_px pixels)."""
    reset(w, h, max(w, h) / scale_px, (0.0, 0.0))
    return name

def build_larva():
    small("Larva", 256, 160)
    cream = mat("Larva", (0.97, 0.93, 0.82), rough=0.35, sss=0.5, coat=0.4)
    for i in range(9):
        t = i / 8.0
        a = math.radians(200 - t * 160)
        r = 0.22 + 0.12 * math.sin(t * math.pi)
        sphere((0.62 * math.cos(a), 0.0, 0.42 * math.sin(a) - 0.12), r, cream, seg=24)
    sphere((0.62 * math.cos(math.radians(40)) + 0.08, -0.02, 0.42 * math.sin(math.radians(40)) - 0.12), 0.13, mat("LHead", (0.55, 0.35, 0.15), rough=0.3, coat=0.6))
    render("Larva")

def build_cocoon():
    small("Cocoon", 160, 256)
    c = sphere((0.0, 0.0, 0.0), (0.42, 0.42, 0.95), mat("Silk", (0.86, 0.76, 0.56), rough=0.75, sss=0.2))
    displace(c, 0.04, 0.12, 2)
    render("Cocoon")

def heap(kind):
    reset(512, 320, 5.12, (0.0, -0.2))
    random.seed({"Sugar": 1, "Protein": 2, "Fat": 3, "Crumbs": 4}[kind])
    if kind == "Sugar":
        mats = [mat("Sugar", (0.97, 0.96, 0.93), rough=0.35, sss=0.3), mat("Sugar2", (0.92, 0.9, 0.86), rough=0.4, sss=0.3)]
    elif kind == "Protein":
        mats = [mat("Meat", (0.72, 0.3, 0.25), rough=0.55, sss=0.2), mat("Seed", (0.22, 0.16, 0.1), rough=0.4, coat=0.4), mat("Nut", (0.65, 0.45, 0.25), rough=0.6)]
    elif kind == "Fat":
        mats = [mat("Cheese", (0.98, 0.78, 0.25), rough=0.45, sss=0.2), mat("Butter", (0.99, 0.9, 0.55), rough=0.3, sss=0.3), mat("Chip", (0.95, 0.72, 0.3), rough=0.5)]
    else:
        mats = [mat("Crumb", (0.72, 0.48, 0.22), rough=0.8), mat("Crust", (0.5, 0.28, 0.1), rough=0.8), mat("Crumb3", (0.85, 0.66, 0.38), rough=0.8)]
    base = -1.45
    n = 70
    for i in range(n):
        r = (random.random() ** 0.6) * 2.2
        a = random.uniform(-1, 1)
        x = r * a
        hmax = 1.55 * max(0.0, 1.0 - abs(x) / 2.3)
        z = base + random.random() * hmax * (1.0 - 0.35 * random.random())
        y = random.uniform(-0.6, 0.6)
        s = random.uniform(0.13, 0.24)
        m = random.choice(mats)
        rot = (random.uniform(0, 3), random.uniform(0, 3), random.uniform(0, 3))
        if kind == "Sugar":
            box((x, y, z), (s * 1.2, s * 1.2, s * 1.2), m, rot=rot, bevel=0.02)
        elif kind == "Fat" and m.name.startswith("Cheese"):
            box((x, y, z), (s * 1.5, s * 1.1, s), m, rot=rot, bevel=0.03)
        elif kind == "Protein" and m.name.startswith("Seed"):
            sphere((x, y, z), (s * 0.55, s * 0.35, s * 0.9), m, seg=16).rotation_euler = rot
        else:
            o = sphere((x, y, z), (s, s * random.uniform(0.7, 1.0), s * random.uniform(0.6, 0.9)), m, seg=16)
            o.rotation_euler = rot
            displace(o, 0.03, 0.1, 1)
    render("Pile_" + kind)

def thimble(with_jelly=True):
    silver = mat("Silver", (0.85, 0.85, 0.88), rough=0.3, metal=1.0)
    t = cyl((0.0, 0.0, -0.2), 0.62, 1.3, silver, verts=48, bevel=0.06)
    displace(t, 0.012, 0.03, 1)
    if with_jelly:
        sphere((0.0, 0.0, 0.48), (0.6, 0.6, 0.25), mat("Jelly", (0.75, 0.35, 0.95), rough=0.08, trans=0.6, emit=(0.8, 0.4, 1.0), emit_k=0.6))

def build_props(names):
    def want(n):
        return not names or n in names
    if want("Larva"): build_larva()
    if want("Cocoon"): build_cocoon()
    for k in ("Sugar", "Protein", "Fat", "Crumbs"):
        if want("Pile_" + k): heap(k)
    if want("Jelly"):
        small("Jelly", 256, 256, 70.0)
        thimble(True)
        render("Jelly")
    if want("CakeSlice"):
        small("CakeSlice", 320, 260, 90.0)
        sponge = mat("Sponge", (0.95, 0.78, 0.5), rough=0.8, sss=0.2)
        cream = mat("Cream", (1.0, 0.96, 0.92), rough=0.5, sss=0.3)
        pink = mat("Pink", (1.0, 0.62, 0.72), rough=0.4, sss=0.3)
        for i, (z, m, h) in enumerate(((-0.6, sponge, 0.35), (-0.36, cream, 0.12), (-0.12, sponge, 0.35), (0.12, cream, 0.12), (0.3, pink, 0.22))):
            bpy.ops.mesh.primitive_cylinder_add(vertices=3, radius=1.3, depth=h, location=(0.0, 0.0, z), rotation=(0, 0, math.radians(-90)))
            o = bpy.context.active_object
            o.scale = (1.0, 0.55, 1.0)
            b = o.modifiers.new("B", "BEVEL"); b.width = 0.05; b.segments = 3
            link_mat(o, m)
        sphere((0.1, 0.0, 0.55), 0.18, mat("Berry", (0.8, 0.05, 0.1), rough=0.2, coat=1.0))
        render("CakeSlice")
    if want("Bunk"):
        small("Bunk", 300, 160, 100.0)
        card = mat("Card", (0.78, 0.58, 0.34), rough=0.85)
        box((0.0, 0.0, -0.35), (2.6, 1.0, 0.5), card, bevel=0.03)
        blanket = sphere((0.2, -0.05, -0.02), (1.1, 0.5, 0.18), mat("Wool", (0.92, 0.92, 0.95), rough=0.95))
        displace(blanket, 0.05, 0.1, 1)
        sphere((-0.95, -0.05, -0.02), (0.32, 0.4, 0.16), mat("Pillow", (0.9, 0.3, 0.25), rough=0.9))
        render("Bunk")
    if want("Crate"):
        small("Crate", 300, 240, 100.0)
        card = mat("Card", (0.78, 0.58, 0.34), rough=0.85)
        box((0.0, 0.2, -0.5), (2.4, 1.2, 0.9), card, bevel=0.03)
        box((0.0, -0.45, -0.55), (2.2, 0.03, 0.5), mat("Label", (0.15, 0.35, 0.7), rough=0.6), bevel=0.0)
        for i in range(5):
            sphere((-0.8 + i * 0.4, 0.2, 0.05), 0.2, mat("Stuff%d" % i, [(0.9, 0.8, 0.2), (0.2, 0.6, 0.8), (0.8, 0.2, 0.3), (0.95, 0.95, 0.9), (0.4, 0.7, 0.3)][i], rough=0.4), seg=16)
        render("Crate")
    if want("Mushroom"):
        small("Mushroom", 256, 256, 80.0)
        cyl((0.0, 0.0, -0.6), 0.28, 1.4, mat("Stem", (0.95, 0.9, 0.8), rough=0.6, sss=0.3), verts=24, bevel=0.1)
        cap = sphere((0.0, 0.0, 0.25), (1.05, 1.05, 0.62), mat("Cap", (0.85, 0.42, 0.15), rough=0.4, sss=0.2, coat=0.3))
        spot = mat("Spot", (0.98, 0.95, 0.88), rough=0.6)
        for a, z in ((-0.6, 0.55), (0.1, 0.78), (0.6, 0.5), (-0.25, 0.4)):
            sphere((a, -0.75 + abs(a) * 0.3, z), (0.12, 0.04, 0.1), spot, seg=12)
        render("Mushroom")
    if want("Radio"):
        small("Radio", 256, 256, 80.0)
        silver = mat("Silver", (0.85, 0.85, 0.88), rough=0.3, metal=1.0)
        cone((0.3, 0.0, 0.35), 0.15, 0.75, 1.1, silver, rot=(0, math.radians(60), 0))
        cyl((-0.25, 0.0, -0.95), 0.9, 0.2, mat("BtnRed", (0.75, 0.15, 0.12), rough=0.3, coat=0.8), verts=40, bevel=0.04)
        tube([(-0.2, 0.0, -0.8), (-0.3, 0.0, -0.2), (-0.1, 0.0, 0.1)], 0.07, mat("Pole", (0.3, 0.3, 0.32), rough=0.4, metal=0.6))
        tube([(-0.6, 0.1, -0.9), (-1.2, 0.1, -0.6), (-1.4, 0.1, 0.2)], 0.035, mat("Wire", (0.8, 0.2, 0.1), rough=0.5))
        render("Radio")
    if want("Target"):
        small("Target", 256, 256, 80.0)
        cyl((0.0, 0.1, -1.0), 0.06, 0.9, wood("Toothpick", (0.9, 0.78, 0.55)), verts=12)
        crimped_cap((0.0, 0.0, 0.2), 0.95, 0.15, mat("CapW", (0.95, 0.93, 0.9), rough=0.4, metal=0.3), rot=(math.radians(90), 0, 0))
        for i, (r, c) in enumerate(((0.7, (0.85, 0.12, 0.1)), (0.45, (0.95, 0.93, 0.9)), (0.22, (0.85, 0.12, 0.1)))):
            cyl((0.0, -0.09 - i * 0.01, 0.2), r, 0.02, mat("Ring%d" % i, c, rough=0.5), rot=(math.radians(90), 0, 0), verts=48)
        render("Target")
    if want("Plaque"):
        reset(512, 320, 5.12, (0.0, 0.0))
        p = box((0.0, 0.0, 0.0), (4.7, 0.3, 2.7), mat("Bark", (0.2, 0.11, 0.06), rough=0.9), bevel=0.15)
        displace(p, 0.06, 0.3, 3)
        render("Plaque")

# ------------------------------------------------------------------------------------------------ trophies (256x256)
def build_trophies(names):
    def go(n):
        if names and ("Trophy_" + n) not in names:
            return False
        reset(256, 256, 2.6, (0.0, 0.0), samples=48)
        return True
    tilt = (math.radians(70), 0, math.radians(-15))
    if go("Button"):
        b = cyl((0, 0, 0), 0.95, 0.22, mat("Teal", (0.1, 0.55, 0.6), rough=0.25, coat=0.9), rot=tilt, verts=48, bevel=0.06)
        for hx, hz in ((-0.22, 0.22), (0.22, 0.22), (-0.22, -0.22), (0.22, -0.22)):
            cyl((hx, -0.2, hz * 0.95), 0.1, 0.1, mat("Hole", (0.03, 0.03, 0.03)), rot=tilt, verts=16)
        render("Trophy_Button")
    if go("Rose"):
        cream = mat("RoseCream", (1.0, 0.72, 0.8), rough=0.45, sss=0.4)
        for i in range(14):
            a = i * 2.4
            r = 0.1 + i * 0.05
            p = sphere((r * math.cos(a), -0.2 + r * 0.2, r * math.sin(a) * 0.6 + 0.1), (0.32, 0.12, 0.22), cream, seg=20)
            p.rotation_euler = (0, a, 0)
        render("Trophy_Rose")
    if go("Coin"):
        cyl((0, 0, 0), 1.0, 0.14, mat("Coin", (0.85, 0.6, 0.3), rough=0.25, metal=1.0), rot=tilt, verts=64, bevel=0.03)
        torus((0, -0.08, 0), 0.82, 0.04, mat("CoinRim", (0.75, 0.52, 0.25), rough=0.3, metal=1.0), rot=tilt)
        render("Trophy_Coin")
    if go("Sausage"):
        cyl((0, 0, 0), 0.95, 0.3, mat("Sausage", (0.78, 0.3, 0.3), rough=0.5, sss=0.3), rot=tilt, verts=48, bevel=0.04)
        torus((0, -0.14, 0), 0.92, 0.07, mat("Skin", (0.55, 0.2, 0.15), rough=0.4, coat=0.5), rot=tilt)
        random.seed(5)
        for i in range(12):
            a = random.uniform(0, 6.28); r = random.uniform(0.1, 0.75)
            sphere((r * math.cos(a), -0.16, r * math.sin(a) * 0.35), (0.07, 0.02, 0.05), mat("Fat", (0.98, 0.95, 0.9), rough=0.6), seg=12)
        render("Trophy_Sausage")
    if go("Slipper"):
        s = sphere((0, 0, 0.1), (1.0, 0.7, 0.6), mat("Fluff", (1.0, 0.55, 0.7), rough=1.0, sss=0.2))
        displace(s, 0.18, 0.08, 3)
        box((0.1, 0, -0.55), (1.8, 1.2, 0.25), mat("Sole", (0.85, 0.78, 0.65), rough=0.8), rot=(0, math.radians(-8), 0), bevel=0.08)
        render("Trophy_Slipper")
    if go("Plate"):
        bpy.ops.mesh.primitive_cylinder_add(vertices=3, radius=1.15, depth=0.12, location=(0, 0, 0), rotation=(math.radians(80), 0, math.radians(10)))
        o = bpy.context.active_object
        link_mat(o, mat("Porcelain", (0.97, 0.97, 0.95), rough=0.15, coat=1.0))
        displace(o, 0.03, 0.4, 2)
        tube([(-0.9, -0.1, 0.5), (0.0, -0.1, 0.62), (0.75, -0.1, 0.45)], 0.05, mat("BlueRim", (0.15, 0.3, 0.8), rough=0.2))
        render("Trophy_Plate")
    if go("Bear"):
        g = mat("Gummy", (1.0, 0.65, 0.1), rough=0.1, trans=0.5, sss=0.4, coat=0.8)
        sphere((0, 0, -0.3), (0.62, 0.45, 0.72), g)
        sphere((0, 0, 0.55), (0.45, 0.4, 0.4), g)
        for x in (-0.33, 0.33):
            sphere((x, 0, 0.92), 0.16, g)
            sphere((x * 1.6, -0.05, -0.1), (0.2, 0.2, 0.32), g)
            sphere((x * 1.2, -0.05, -0.95), (0.25, 0.22, 0.2), g)
        render("Trophy_Bear")
    if go("Foil"):
        f = sphere((0.15, 0, 0.1), 0.8, mat("FoilBall", (0.88, 0.86, 0.92), rough=0.2, metal=1.0))
        displace(f, 0.25, 0.15, 3)
        box((-0.6, -0.2, -0.6), (0.9, 0.05, 0.6), mat("Wrapper", (0.45, 0.15, 0.55), rough=0.5), rot=(0, math.radians(25), 0), bevel=0.0)
        render("Trophy_Foil")
    if go("Cheese"):
        bpy.ops.mesh.primitive_cylinder_add(vertices=3, radius=1.2, depth=0.9, location=(0, 0, 0), rotation=(math.radians(90), 0, math.radians(30)))
        o = bpy.context.active_object
        link_mat(o, mat("Cheese", (0.98, 0.8, 0.28), rough=0.45, sss=0.2))
        for hx, hz, r in ((-0.3, 0.1, 0.18), (0.25, -0.25, 0.14), (0.1, 0.35, 0.1)):
            h = sphere((hx, -0.45, hz), r, None, seg=16)
            bm = o.modifiers.new("Hole", "BOOLEAN")
            bm.operation = "DIFFERENCE"
            bm.object = h
            h.hide_render = True
        render("Trophy_Cheese")
    if go("Thimble"):
        thimble(True)
        render("Trophy_Thimble")
    if go("Earring"):
        sphere((0.0, 0.0, -0.35), 0.55, mat("Pearl", (0.96, 0.94, 0.9), rough=0.1, coat=1.0, sss=0.2))
        gold = mat("Gold", (1.0, 0.72, 0.22), rough=0.22, metal=1.0)
        tube([(0.0, 0.0, 0.2), (0.0, 0.0, 0.6), (0.35, 0.0, 0.95), (0.6, 0.0, 0.6), (0.5, 0.0, 0.2)], 0.04, gold)
        sphere((0.0, 0.0, 0.22), 0.1, gold, seg=16)
        render("Trophy_Earring")
    if go("Jaw"):
        red = mat("RedChitin", (0.65, 0.07, 0.04), rough=0.25, coat=0.8)
        for i in range(10):
            t = i / 9.0
            a = math.radians(200 - t * 150)
            sphere((0.75 * math.cos(a), 0.0, 0.75 * math.sin(a) * 0.8 - 0.1), 0.22 * (1.0 - t * 0.8), red, seg=20)
        render("Trophy_Jaw")
    if go("Candle"):
        cyl((0, 0, -0.3), 0.25, 1.6, stripes("CandleS", (0.98, 0.95, 0.95), (0.95, 0.45, 0.6), scale=4.0, rough=0.5, sss=0.3), verts=32, bevel=0.03)
        tube([(0, 0, 0.5), (0.02, 0, 0.68)], 0.025, mat("Wick", (0.1, 0.08, 0.06)))
        sphere((0.02, 0, 0.85), (0.12, 0.12, 0.22), mat("Flame", (1.0, 0.7, 0.2), emit=(1.0, 0.6, 0.15), emit_k=12.0))
        render("Trophy_Candle")

# ------------------------------------------------------------------------------------------------ main
ALL_TROPHIES = {"Trophy_" + n for n in ("Button", "Rose", "Coin", "Sausage", "Slipper", "Plate", "Bear", "Foil", "Cheese", "Thimble", "Earring", "Jaw", "Candle")}
def want_any(prefix_set):
    return not ONLY or bool(ONLY & prefix_set)

if want_any({"Queen_" + f for f in QUEEN_FACES} | {"Crown_2", "Crown_3", "queen"}):
    build_queen(set(n for n in ONLY if n.startswith(("Queen_", "Crown_"))) if ONLY and "queen" not in ONLY else set())
if not ONLY or "Frame" in ONLY:
    build_frame()
if not ONLY or "Deco" in ONLY:
    build_deco()
if not ONLY or ONLY & {"Seat_2", "Seat_3"}:
    build_seats()
props = {"Larva", "Cocoon", "Pile_Sugar", "Pile_Protein", "Pile_Fat", "Pile_Crumbs", "Jelly", "CakeSlice", "Bunk", "Crate", "Mushroom", "Radio", "Target", "Plaque"}
if want_any(props):
    build_props(ONLY & props if ONLY else set())
if want_any(ALL_TROPHIES):
    build_trophies(ONLY & ALL_TROPHIES if ONLY else set())
print("COLONYART done")
