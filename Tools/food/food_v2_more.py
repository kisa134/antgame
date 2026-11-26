# The rest of the food of AntFood's table (KAN-36). Exec'd by food_v2.py: the same helpers, the same conventions -
# metres, real size, long side along X, lying on the floor, every part a closed mesh with its food material.

def sphere_wedge(name, R, ang, core=0.0, seg=64):
    """a slice of a round fruit (apple, tomato): a sphere between two planes through the vertical axis, the core cut away"""
    s = uvsphere(name, R, (0, 0, 0), seg, seg // 2)
    w = prism(name + "_w", [(0, 0), (R * 2 * math.cos(-ang / 2), R * 2 * math.sin(-ang / 2)), (R * 2.2, 0), (R * 2 * math.cos(ang / 2), R * 2 * math.sin(ang / 2))], -R * 1.5, R * 1.5)
    boolean(s, w, "INTERSECT"); bpy.data.objects.remove(w, do_unlink=True)
    if core > 0:
        c = cyl(name + "_c", core, R * 3.0, (0, 0, 0), 32); boolean(s, c, "DIFFERENCE"); bpy.data.objects.remove(c, do_unlink=True)
    return s

def lay(objs, rot):
    """turn a finished set of parts (about the origin) to lie the way it should"""
    for o in objs:
        o.rotation_euler = rot
        active(o); bpy.ops.object.transform_apply(rotation=True)

def radial(t, v, cx=0.0, cy=0.0, axis="z"):
    x, y, z = t.sep(v)
    xy = t.n("ShaderNodeCombineXYZ")
    a, b = {"z": (t.sub(x, cx), t.sub(y, cy)), "y": (x, z), "x": (y, z)}[axis]
    t.set(xy.inputs[0], a); t.set(xy.inputs[1], b)
    return t.vmath("LENGTH", xy.outputs[0], out=1)

# ---------------------------------------------------------------- looks
def crust_look(base=(0.80, 0.52, 0.24), dark=(0.45, 0.24, 0.08)):
    def b(t, v, h=None):
        n = t.noise(v, 120.0, 6.0); f = t.noise(v, 700.0, 3.0)
        cr = t.mul(t.mapr(t.voronoi(v, 140.0, "DISTANCE_TO_EDGE"), 0.0, 0.025, 1.0, 0.0), t.mapr(t.noise(v, 60.0, 2.0), 0.45, 0.65))   # a few cracks, not a net
        c = t.mixc(t.mapr(n, 0.35, 0.7), base, dark)
        c = t.mixc(t.mul(cr, 0.3), c, (0.40, 0.22, 0.08))
        return c, t.mapr(f, 0.3, 0.7, 0.55, 0.8), t.add(t.mul(f, 0.5), t.mul(cr, -0.35))
    return b

def crystal_look(base=(0.97, 0.97, 0.95)):
    def b(t, v, h=None):
        g = t.voronoi(v, 2600.0, "F1"); e = t.voronoi(v, 2600.0, "DISTANCE_TO_EDGE")
        c = t.mixc(t.mapr(e, 0.0, 0.06, 0.25, 0.0), base, (0.72, 0.72, 0.70))
        return c, t.mapr(g, 0.0, 0.5, 0.18, 0.5), t.mapr(e, 0.0, 0.08, 0.0, 1.0)
    return b

def choc_look(base=(0.22, 0.10, 0.05), rough=0.25):
    def b(t, v, h=None):
        n = t.noise(v, 300.0, 4.0)
        c = t.mixc(t.mul(t.sub(n, 0.5), 0.5), base, (0.36, 0.19, 0.10))
        return c, t.mapr(n, 0.3, 0.7, rough, rough + 0.12), t.mul(t.noise(v, 1400.0, 2.0), 0.15)
    return b

def fruit_skin(base, spots=(1.0, 0.95, 0.6), rough=0.3, streak=None):
    def b(t, v, h=None):
        n = t.noise(v, 90.0, 5.0); f = t.noise(v, 900.0, 2.0)
        c = t.mixc(t.mapr(n, 0.3, 0.75), base, streak or tuple(min(1, x * 1.25) for x in base))
        lent = t.mapr(t.voronoi(v, 700.0, "F1"), 0.0, 0.05, 1.0, 0.0)                  # lenticels
        c = t.mixc(t.mul(lent, 0.6), c, spots)
        return c, t.mapr(f, 0.3, 0.7, rough, rough + 0.12), t.add(t.mul(f, 0.15), t.mul(lent, 0.3))
    return b

def flesh_look(base, fibre=(1.0, 1.0, 0.95), wet=0.25):
    def b(t, v, h=None):
        n = t.noise(v, 400.0, 4.0); cells = t.voronoi(v, 1500.0, "DISTANCE_TO_EDGE")
        c = t.mixc(t.mapr(n, 0.3, 0.75), base, fibre)
        c = t.mixc(t.mapr(cells, 0.0, 0.05, 0.25, 0.0), c, tuple(x * 0.85 for x in base))
        return c, wet, t.add(t.mul(n, 0.3), t.mul(t.mapr(cells, 0.0, 0.05, 0.6, 0.0), 1.0))
    return b

# ================================================================ CRUMBLY
@food("Sponge", "Crumbly", 5.5, 3)
def build_sponge():
    sp = food_mat("Sponge", sponge_look((0.98, 0.80, 0.42)), bump=0.0005, bump_str=0.8)
    cr = food_mat("Crust", crust_look((0.78, 0.48, 0.20), (0.50, 0.27, 0.09)), bump=0.0004)
    body = cube("Body", (0.055, 0.04, 0.034), (0, 0, 0.017))
    sm = body.modifiers.new("sub", "SUBSURF"); sm.subdivision_type = "SIMPLE"; sm.levels = 5
    displace(body, "CLOUDS", 0.0018, 0.0012, 0.5, 3)
    smooth(body, 0, 0.002, 3); assign(body, sp)
    top = cube("Top", (0.0555, 0.0405, 0.004), (0, 0, 0.0345))                   # the baked top, domed a little
    for v in top.data.vertices:
        if v.co.z > 0.034: v.co.z += 0.003 * (1 - (v.co.x / 0.028) ** 2) * (1 - (v.co.y / 0.021) ** 2)
    sm = top.modifiers.new("sub", "SUBSURF"); sm.levels = 3
    displace(top, "CLOUDS", 0.004, 0.0008, 0.5, 2); assign(top, cr)
    def inner(t, v, h):
        c, r, hh = sponge_look((0.98, 0.80, 0.42))(t, v)
        return t.mixc(t.mapr(h, 0.88, 0.95), c, (0.62, 0.36, 0.13)), r, hh
    return dict(parts=[body, top], inner=inner, cuts=[((0, 0, 0), (1, 0.2, 0.1), 0.002, 0.005), ((0.008, 0, 0), (0.1, 1, 0.2), 0.002, 0.005)],
                hmin=(0, 0, 0.0), hmax=(0, 0, 0.038))

@food("Cookie", "Crumbly", 3.2, 3)
def build_cookie():
    dough = food_mat("Cookie", crust_look((0.82, 0.56, 0.28), (0.58, 0.34, 0.14)), bump=0.0005, bump_str=0.9)
    choc = food_mat("Chip", choc_look((0.16, 0.07, 0.03), 0.3))
    R = 0.0175; Rg = random.Random(11)
    poly = []
    for i in range(40):                                                              # a half cookie: the round edge ...
        a = -math.pi / 2 + math.pi * i / 39
        rr = R * (1 + Rg.uniform(-0.03, 0.03)); poly.append((rr * math.cos(a), rr * math.sin(a)))
    for i in range(1, 14):                                                           # ... and the broken edge, jagged
        f = i / 14.0; poly.append((Rg.uniform(-0.0015, 0.0015) + 0.0012 * math.sin(f * 17), R - 2 * R * f))
    body = prism("Cookie", list(reversed(poly)) if False else poly, 0.0, 0.0055)
    sm = body.modifiers.new("sub", "SUBSURF"); sm.subdivision_type = "SIMPLE"; sm.levels = 3
    for v in body.data.vertices:
        if v.co.z > 0.004: v.co.z += 0.0012 * (1 - (v.co.x ** 2 + v.co.y ** 2) / R ** 2)
    displace(body, "CLOUDS", 0.0025, 0.0009, 0.5, 3); smooth(body, 1, None); assign(body, dough)
    parts = [body]
    for i in range(7):                                                               # chocolate chips, half sunk
        a = Rg.uniform(-1.4, 1.4); r = Rg.uniform(0.003, R - 0.004)
        ch = uvsphere("Chip", Rg.uniform(0.0018, 0.0027), (0.002 + r * math.cos(a), r * math.sin(a), 0.0058), 12, 8, (1, 1, 0.7))
        displace(ch, "CLOUDS", 0.002, 0.0004, 0.5, 1); assign(ch, choc); parts.append(ch)
    def inner(t, v, h):
        c, r, hh = sponge_look((0.86, 0.62, 0.32), 1100.0)(t, v)
        return c, 0.8, hh
    return dict(parts=parts, inner=inner, cuts=[((0.0, 0.0, 0.0), (1, 0.6, 0.0), 0.0012, 0.003), ((0.0, 0.0, 0.0), (1, -0.8, 0.0), 0.0012, 0.003)])

@food("Bread", "Crumbly", 2.2, 2)
def build_bread():
    crumb = food_mat("Crumb", sponge_look((0.96, 0.86, 0.62), 900.0), bump=0.0004, bump_str=0.9)
    crust = food_mat("BreadCrust", crust_look((0.72, 0.42, 0.16), (0.48, 0.25, 0.08)), bump=0.0003)
    b = cube("Crumb", (0.022, 0.016, 0.013), (0, 0, 0.0065))
    sm = b.modifiers.new("sub", "SUBSURF"); sm.subdivision_type = "SIMPLE"; sm.levels = 4
    displace(b, "CLOUDS", 0.004, 0.0022, 0.5, 2); displace(b, "CLOUDS", 0.0012, 0.0008, 0.5, 2)
    assign(b, crumb)
    c = cube("Crust", (0.023, 0.0165, 0.0028), (0, 0, 0.0135))
    for v in c.data.vertices:
        v.co.z += 0.0015 * (1 - (v.co.x / 0.0115) ** 2)
    sm = c.modifiers.new("sub", "SUBSURF"); sm.levels = 3
    displace(c, "CLOUDS", 0.003, 0.0008, 0.5, 2); assign(c, crust)
    def inner(t, v, h):
        cc, r, hh = sponge_look((0.96, 0.86, 0.62), 900.0)(t, v)
        return t.mixc(t.mapr(h, 0.82, 0.9), cc, (0.55, 0.30, 0.10)), r, hh
    return dict(parts=[b, c], inner=inner, cuts=[((0, 0, 0), (1, 0.3, 0.2), 0.0014, 0.003)], hmin=(0, 0, 0.0), hmax=(0, 0, 0.016))

# ================================================================ ELASTIC
def ridged_dome(name, R, H, ridges, depth, seg=64):
    s = uvsphere(name, R, (0, 0, 0), seg, seg // 2, (1, 1, H / R))
    for v in s.data.vertices:
        if v.co.z < 0: v.co.z *= 0.12                                                # a flat foot
        a = math.atan2(v.co.y, v.co.x); k = 1 + depth * math.cos(a * ridges) * min(1.0, (H - abs(v.co.z)) / H * 2.0)
        v.co.x *= k; v.co.y *= k
    tw = s.modifiers.new("tw", "SIMPLE_DEFORM"); tw.deform_method = "TWIST"; tw.deform_axis = "Z"; tw.angle = math.radians(40)
    return s

def powder_look(base, rough=0.75):
    def b(t, v, h=None):
        n = t.noise(v, 600.0, 3.0); d = t.noise(v, 2500.0, 2.0)
        c = t.mixc(t.mapr(d, 0.4, 0.8, 0.0, 0.45), base, (1.0, 0.99, 0.97))       # icing sugar dust
        return c, rough, t.add(t.mul(n, 0.3), t.mul(d, 0.3))
    return b

@food("Marshmallow", "Elastic", 3.5, 2)
def build_marsh():
    pink = food_mat("ZefirPink", powder_look((0.98, 0.72, 0.78)), sss=0.8, sss_rad=(1, 0.6, 0.6), sss_scale=0.004, sheen=0.4, bump=0.0003)
    white = food_mat("ZefirWhite", powder_look((0.99, 0.96, 0.92)), sss=0.8, sss_rad=(1, 0.8, 0.7), sss_scale=0.004, sheen=0.4, bump=0.0003)
    a = ridged_dome("Half1", 0.0165, 0.012, 14, 0.06); a.location = (0, 0, 0.0)
    b = ridged_dome("Half2", 0.0165, 0.012, 14, 0.06); b.rotation_euler = (math.pi, 0, math.radians(13)); b.location = (0, 0, -0.0004)
    for o in (a, b): smooth(o, 1)
    assign(a, white); assign(b, pink)
    lay([a, b], (math.pi / 2, 0, 0))                                               # two halves stuck together, lying on its side
    def inner(t, v, h):
        x = t.sep(v)[1]
        c = t.mixc(t.mapr(x, -0.0008, 0.0008), (0.99, 0.74, 0.80), (0.99, 0.96, 0.92))
        return c, 0.8, t.mul(t.voronoi(v, 1800.0, "F1"), -0.8)                       # a fine foam
    return dict(parts=[a, b], inner=inner, cuts=[((0, 0, 0), (1, 0, 0.15), 0.0012, 0.004)], jelly=0.4)

@food("Gum", "Elastic", 2.0, 0)
def build_gum():
    gum = food_mat("Gum", jelly_look((0.98, 0.52, 0.70), 0.45, False), sss=0.6, sss_rad=(1, 0.5, 0.6), sss_scale=0.003, bump=0.0003, bump_str=0.8)
    g = uvsphere("Gum", 0.008, (0, 0, 0.0035), 48, 24, (1.25, 0.9, 0.45))
    sm = g.modifiers.new("sub", "SUBSURF"); sm.levels = 1
    displace(g, "CLOUDS", 0.003, 0.0018, 0.5, 2)                                    # chewed: lumpy, teeth marks
    displace(g, "STUCCI", 0.0012, 0.0004, 0.5, 1)
    for v in g.data.vertices:
        if v.co.z < 0.0012: v.co.z = 0.0012 + (v.co.z - 0.0012) * 0.2
    assign(g, gum); smooth(g, 1)
    st = tube("String", [(0.009, 0, 0.002), (0.013, 0.001, 0.0025), (0.017, 0.0, 0.0012)], 0.0006, 10, True, [1.0, 0.6, 0.9])   # a stretched string
    assign(st, gum)
    def inner(t, v, h): return (0.98, 0.58, 0.74), 0.5, 0.0
    return dict(parts=[g, st], inner=inner, cuts=[], jelly=0.4)

# ================================================================ SOFT
def cheese_look(base=(1.0, 0.74, 0.18)):
    def b(t, v, h=None):
        n = t.noise(v, 250.0, 4.0); p = t.voronoi(v, 3000.0, "F1")
        c = t.mixc(t.mapr(n, 0.3, 0.7, 0.0, 0.3), base, (1.0, 0.9, 0.55))
        c = t.mixc(t.mapr(p, 0.0, 0.06, 0.4, 0.0), c, (0.9, 0.65, 0.2))
        return c, t.mapr(n, 0.3, 0.7, 0.32, 0.45), t.add(t.mul(n, 0.2), t.mapr(p, 0.0, 0.06, -0.6, 0.0))
    return b

@food("Cheese", "Soft", 7.0, 2)
def build_cheese():
    ch = food_mat("Cheese", cheese_look(), sss=0.15, sss_rad=(1, 0.7, 0.3), sss_scale=0.004, bump=0.0003)
    rind = food_mat("Rind", glossy_look((0.95, 0.62, 0.10), 0.35, 0.1), bump=0.0002)
    L_, W_, T_ = 0.07, 0.045, 0.012
    s = prism("Slice", [(0, -W_ / 2), (L_, 0), (0, W_ / 2)], 0, T_)               # a wedge slice of a holey cheese
    sm = s.modifiers.new("sub", "SUBSURF"); sm.subdivision_type = "SIMPLE"; sm.levels = 4
    apply_mods(s)
    Rg = random.Random(4)
    for i in range(24):                                                              # the holes: real craters in the faces of the slice
        x = Rg.uniform(0.006, L_ * 0.82); w = W_ / 2 * (1 - x / L_)
        y = Rg.uniform(-w * 0.85, w * 0.85); z = (0.0, T_)[i % 2]
        r = Rg.uniform(0.0018, 0.0045)
        hole = uvsphere("Hole", r, (x, y, z), 20, 10); boolean(s, hole, "DIFFERENCE"); bpy.data.objects.remove(hole, do_unlink=True)
    smooth(s, 1, None); assign(s, ch)
    rd = prism("Rind", [(-0.0012, -W_ / 2 - 0.0005), (0.0, -W_ / 2), (0.0, W_ / 2), (-0.0012, W_ / 2 + 0.0005)], 0.0, T_)
    smooth(rd, 1, 0.0003, 2); assign(rd, rind)
    def inner(t, v, h):
        c, r, hh = cheese_look()(t, v)
        holes = t.mapr(t.voronoi(v, 260.0, "F1"), 0.0, 0.1, 1.0, 0.0)                 # the cut shows holes too
        return t.mixc(t.mul(holes, 0.7), c, (0.75, 0.52, 0.12)), r, t.add(hh, t.mul(holes, -1.0))
    return dict(parts=[s, rd], inner=inner, cuts=[((0.004, 0, 0), (1, 0.25, 0), 0.0012, 0.004)])

def meat_look(base=(0.80, 0.42, 0.38)):
    def b(t, v, h=None):
        n = t.noise(v, 300.0, 4.0); fat = t.mapr(t.voronoi(v, 900.0, "F1"), 0.0, 0.12, 1.0, 0.0)
        c = t.mixc(t.mapr(n, 0.3, 0.7, 0.0, 0.3), base, (0.95, 0.6, 0.55))
        c = t.mixc(t.mul(fat, 0.8), c, (0.98, 0.92, 0.86))
        return c, 0.45, t.add(t.mul(n, 0.4), t.mul(fat, 0.3))
    return b

@food("Sausage", "Soft", 6.0, 2)
def build_sausage():
    skin = food_mat("Skin", fruit_skin((0.70, 0.30, 0.20), (0.78, 0.40, 0.28), 0.3, (0.55, 0.22, 0.14)), sss=0.4, sss_rad=(1, 0.4, 0.3), coat=0.3, bump=0.0002)
    meat = food_mat("Meat", meat_look(), bump=0.0004)
    pts = [(-0.028 + 0.056 * i / 9.0, 0.0, 0.011 + 0.002 * math.sin(i / 9.0 * math.pi)) for i in range(10)]
    body = tube("Sausage", pts, 0.0105, 24, True)
    end = uvsphere("End", 0.0105, (0.028, 0, 0.011), 32, 16, (0.7, 1, 1))
    s = blob("Sausage", [body, end], 0.0003, 4, 1)
    cutp = cube("Cut", (0.02, 0.05, 0.05), (-0.035, 0, 0.011)); boolean(s, cutp, "DIFFERENCE"); bpy.data.objects.remove(cutp, do_unlink=True)
    for v in s.data.vertices:
        if v.co.z < 0.0015: v.co.z = 0.0015 + (v.co.z - 0.0015) * 0.3
    assign(s, skin)
    face = cyl("Face", 0.0102, 0.0006, (-0.0247, 0, 0.0112), 48, (0, math.pi / 2, 0)); assign(face, meat)   # the cut end shows the meat
    def inner(t, v, h):
        return meat_look()(t, v)
    return dict(parts=[s, face], inner=inner, cuts=[((0.002, 0, 0), (1, 0.05, 0), 0.0008, 0.003)])

@food("Noodle", "Soft", 8.0, 2)
def build_noodle():
    nd = food_mat("Noodle", flesh_look((0.97, 0.86, 0.52), (1.0, 0.94, 0.70), 0.3), sss=0.5, sss_rad=(1, 0.8, 0.4), sss_scale=0.002, coat=0.4, bump=0.0001)
    pts = []
    for i in range(40):
        f = i / 39.0; x = -0.035 + 0.07 * f
        pts.append((x, 0.009 * math.sin(f * math.pi * 3.0) + 0.002 * math.sin(f * 23), 0.0016 + 0.0012 * max(0.0, math.sin(f * math.pi * 2.2))))
    n = tube("Noodle", pts, 0.0015, 10, True)
    for v in n.data.vertices:
        if v.co.z < 0.0002: v.co.z = 0.0002
    smooth(n, 1); assign(n, nd)
    def inner(t, v, h): return (0.98, 0.92, 0.66), 0.4, 0.0
    return dict(parts=[n], inner=inner, cuts=[((0.0, 0, 0), (1, 0.1, 0), 0.0003, 0.002)])

@food("Pancake", "Soft", 7.0, 2)
def build_pancake():
    def pan_look(t, v, h=None):
        lace = t.voronoi(v, 160.0, "DISTANCE_TO_EDGE"); n = t.noise(v, 90.0, 5.0)
        c = t.mixc(t.mapr(lace, 0.0, 0.08, 1.0, 0.0), (0.98, 0.84, 0.50), (0.62, 0.36, 0.12))   # the lacy browning of a thin pancake
        c = t.mixc(t.mapr(n, 0.4, 0.8, 0.0, 0.4), c, (0.50, 0.28, 0.08))
        return c, 0.55, t.add(t.mul(n, 0.3), t.mapr(lace, 0.0, 0.08, 0.6, 0.0))
    pc = food_mat("Pancake", pan_look, sss=0.3, sss_rad=(1, 0.7, 0.4), bump=0.0003)
    R = 0.07
    poly = [(0, 0)] + [(R * math.cos(a), R * math.sin(a)) for a in [math.radians(-30 + 60 * i / 24.0) for i in range(25)]]
    p = prism("Pancake", poly, 0, 0.0035)                                             # a folded quarter: two layers
    sm = p.modifiers.new("sub", "SUBSURF"); sm.subdivision_type = "SIMPLE"; sm.levels = 4
    displace(p, "CLOUDS", 0.012, 0.0025, 0.5, 2); smooth(p, 1, 0.001, 2)
    assign(p, pc)
    def inner(t, v, h): return t.mixc(t.mapr(h, 0.2, 0.8), (0.98, 0.88, 0.60), (0.96, 0.82, 0.50)), 0.6, t.mul(t.noise(v, 900.0, 3.0), 0.4)
    return dict(parts=[p], inner=inner, cuts=[((0.005, 0, 0), (1, 0.4, 0), 0.0015, 0.004)])

# ================================================================ HARD
@food("Sugar", "Hard", 1.6, 4)
def build_sugar():
    sg = food_mat("Sugar", crystal_look(), sss=0.5, sss_rad=(1, 1, 1), sss_scale=0.002, bump=0.0002, bump_str=1.0)
    c = cube("Cube", (0.016, 0.016, 0.016), (0, 0, 0.008))
    sm = c.modifiers.new("sub", "SUBSURF"); sm.subdivision_type = "SIMPLE"; sm.levels = 5
    smooth(c, 0, 0.0006, 2)
    displace(c, "CLOUDS", 0.0009, 0.00035, 0.5, 3)                                   # pressed grains, the corners worn
    assign(c, sg)
    def inner(t, v, h): return crystal_look((0.99, 0.99, 0.98))(t, v)
    return dict(parts=[c], inner=inner, cuts=[((0, 0, 0), (1, 0.1, 0.3), 0.0005, 0.0015), ((0, 0, 0), (0.1, 1, -0.2), 0.0005, 0.0015)])

@food("Nut", "Hard", 2.2, 2, "r")
def build_nut():
    ch = food_mat("ChocCoat", choc_look((0.20, 0.09, 0.04), 0.22), coat=0.3, bump=0.0002)
    n = uvsphere("Nut", 0.0095, (0, 0, 0.0095), 48, 24, (1.15, 1.0, 0.95))
    displace(n, "CLOUDS", 0.004, 0.0007, 0.5, 2)                                      # the chocolate takes the shape of the nut, lumpy
    for v in n.data.vertices:
        if v.co.z < 0.0012: v.co.z = 0.0012 + (v.co.z - 0.0012) * 0.2
    smooth(n, 1); assign(n, ch)
    def inner(t, v, h):
        nut = t.mixc(t.noise(v, 600.0, 4.0), (0.85, 0.70, 0.45), (0.72, 0.55, 0.32))
        c = t.mixc(t.mapr(h, 0.68, 0.74), nut, (0.24, 0.11, 0.05))                    # the nut inside, the chocolate shell outside
        return c, t.mixf(t.mapr(h, 0.68, 0.74), 0.7, 0.3), t.mul(t.noise(v, 1500.0, 3.0), 0.4)
    return dict(parts=[n], inner=inner, cuts=[((0, 0, 0), (1, 0.1, 0.2), 0.0006, 0.002)])

@food("Candy", "Hard", 2.5, 3)
def build_candy():
    def candy(t, v, h=None):
        x, y, z = t.sep(v)
        sw = t.wave(v, 120.0, "RINGS", 8.0)
        c = t.mixc(t.mapr(sw, 0.55, 0.62), (0.80, 0.05, 0.06), (0.98, 0.95, 0.90))   # a red and white swirl
        return c, 0.06, t.mul(t.noise(v, 900.0, 2.0), 0.05)
    cm = food_mat("Candy", candy, sss=0.8, sss_rad=(1, 0.3, 0.3), sss_scale=0.003, coat=1.0, coat_rough=0.03, trans=0.3, bump=0.0001)
    c = uvsphere("Candy", 0.0125, (0, 0, 0.0055), 48, 24, (1.0, 0.85, 0.42))
    smooth(c, 1); assign(c, cm)
    def inner(t, v, h): return (0.85, 0.15, 0.12), 0.15, t.mul(t.voronoi(v, 2000.0, "F1"), 0.5)
    return dict(parts=[c], inner=inner, cuts=[((0, 0, 0), (1, 0.3, 0), 0.0008, 0.002), ((0.003, 0, 0), (0.2, 1, 0), 0.0008, 0.002)], jelly=0.3)

@food("Bone", "Hard", 7.0, 2)
def build_bone():
    def bone(t, v, h=None):
        n = t.noise(v, 200.0, 5.0); pore = t.voronoi(v, 2200.0, "F1")
        x = t.sep(v)[0]
        ends = t.mapr(t.math("ABSOLUTE", x), 0.022, 0.032)
        c = t.mixc(t.mapr(n, 0.3, 0.7), (0.93, 0.88, 0.76), (0.84, 0.76, 0.60))
        c = t.mixc(t.mul(ends, 0.6), c, (0.78, 0.50, 0.42))                          # the knuckles still pinkish
        return c, 0.55, t.add(t.mul(n, 0.3), t.mapr(pore, 0.0, 0.05, -0.5, 0.0))
    bm = food_mat("Bone", bone, bump=0.0003)
    shaft = tube("Shaft", [(-0.026, 0, 0.005), (0.0, 0.001, 0.0048), (0.026, 0, 0.005)], 0.0034, 16, True, [1.1, 0.9, 1.1])
    k1 = uvsphere("Knob1", 0.0058, (-0.03, 0.0015, 0.0058), 24, 12, (1, 1.2, 0.9)); k2 = uvsphere("Knob2", 0.0048, (-0.031, -0.003, 0.005), 20, 10)
    k3 = uvsphere("Knob3", 0.0055, (0.03, 0.0, 0.0056), 24, 12, (1.1, 1.0, 0.9))
    b = blob("Bone", [shaft, k1, k2, k3], 0.0003, 6, 1); displace(b, "CLOUDS", 0.003, 0.0003, 0.5, 2)
    for v in b.data.vertices:
        if v.co.z < 0.0008: v.co.z = 0.0008 + (v.co.z - 0.0008) * 0.3
    assign(b, bm)
    def inner(t, v, h):
        marrow = t.mapr(t.voronoi(v, 1400.0, "DISTANCE_TO_EDGE"), 0.0, 0.08, 1.0, 0.0)  # spongy bone inside
        return t.mixc(marrow, (0.90, 0.84, 0.70), (0.55, 0.30, 0.22)), 0.7, t.mul(marrow, -0.8)
    return dict(parts=[b], inner=inner, cuts=[((0.002, 0, 0), (1, 0.2, 0.3), 0.0006, 0.002)])

# ================================================================ ROLLING
@food("Grape", "Rolling", 2.2, 2, "r")
def build_grape():
    def skin(t, v, h=None):
        n = t.noise(v, 70.0, 4.0); bloom = t.noise(v, 1200.0, 3.0)
        c = t.mixc(t.mapr(n, 0.3, 0.7), (0.42, 0.62, 0.12), (0.62, 0.78, 0.25))
        c = t.mixc(t.mapr(bloom, 0.4, 0.8, 0.0, 0.35), c, (0.85, 0.88, 0.80))       # the waxy bloom
        return c, t.mapr(bloom, 0.4, 0.8, 0.18, 0.55), t.mul(bloom, 0.1)
    gm = food_mat("Grape", skin, sss=1.0, sss_rad=(0.7, 1.0, 0.4), sss_scale=0.006, coat=0.3, bump=0.0001)
    stem = food_mat("GStem", crust_look((0.45, 0.40, 0.18), (0.30, 0.25, 0.10)))
    g = uvsphere("Grape", 0.0095, (0, 0, 0.0092), 48, 24, (1.12, 1.0, 0.98)); smooth(g, 1); assign(g, gm)
    s = tube("GStem", [(0.0105, 0, 0.0092), (0.0118, 0.0003, 0.0095), (0.0126, 0.0006, 0.0101)], 0.0007, 8, True, [1.4, 1.0, 0.8]); assign(s, stem)
    def inner(t, v, h):
        seeds = t.mapr(t.voronoi(v, 260.0, "F1"), 0.0, 0.12, 1.0, 0.0)
        c = t.mixc(t.mapr(h, 0.85, 0.95), (0.78, 0.86, 0.48), (0.50, 0.68, 0.15))
        c = t.mixc(t.mul(seeds, t.mapr(h, 0.25, 0.4, 1.0, 0.0)), c, (0.55, 0.40, 0.15))
        return c, 0.12, t.mul(t.noise(v, 800.0, 2.0), 0.3)
    return dict(parts=[g, s], inner=inner, cuts=[((0, 0, 0), (1, 0, 0.1), 0.0005, 0.002)], jelly=0.6)

@food("Pea", "Rolling", 0.9, 2, "r")
def build_pea():
    pm = food_mat("Pea", fruit_skin((0.30, 0.62, 0.18), (0.45, 0.72, 0.28), 0.32, (0.42, 0.72, 0.25)), sss=0.8, sss_rad=(0.6, 1, 0.4), sss_scale=0.002, bump=0.0001)
    p = uvsphere("Pea", 0.0044, (0, 0, 0.0043), 32, 16, (1.03, 1.0, 0.97))
    displace(p, "CLOUDS", 0.002, 0.00015, 0.5, 2)                                    # a little wrinkled
    smooth(p, 1); assign(p, pm)
    def inner(t, v, h): return t.mixc(t.mapr(h, 0.85, 0.95), (0.70, 0.85, 0.40), (0.35, 0.65, 0.20)), 0.4, t.mul(t.noise(v, 2000.0, 2.0), 0.3)
    return dict(parts=[p], inner=inner, cuts=[((0, 0, 0), (1, 0, 0), 0.0002, 0.001)], jelly=0.3)

@food("Olive", "Rolling", 2.2, 2, "r")
def build_olive():
    om = food_mat("Olive", fruit_skin((0.10, 0.06, 0.08), (0.18, 0.10, 0.12), 0.12, (0.20, 0.10, 0.14)), sss=0.4, sss_rad=(0.6, 0.3, 0.4), coat=0.6, bump=0.0001)
    o = uvsphere("Olive", 0.0085, (0, 0, 0.0085), 48, 24, (1.28, 1.0, 1.0)); smooth(o, 1); assign(o, om)
    def inner(t, v, h):
        c = t.mixc(t.mapr(h, 0.35, 0.42), (0.55, 0.40, 0.25), (0.42, 0.30, 0.22))      # the stone in the middle, the flesh round it
        c = t.mixc(t.mapr(h, 0.9, 0.97), c, (0.12, 0.06, 0.08))
        return c, t.mixf(t.mapr(h, 0.35, 0.42), 0.6, 0.3), t.mul(t.noise(v, 900.0, 2.0), 0.3)
    return dict(parts=[o], inner=inner, cuts=[((0, 0, 0), (1, 0, 0.05), 0.0004, 0.002)])

# ================================================================ STICKY
def drop(name, R, H, wob=0.0004):
    d = uvsphere(name, R, (0, 0, 0), 48, 24, (1.0, 0.92, H / R))
    for v in d.data.vertices:
        if v.co.z < 0: v.co.z *= 0.05                                                 # it sits flat on the floor
        v.co.x *= 1.0 + 0.12 * max(0.0, -v.co.z / H) ; v.co.y *= 1.0
    d.location = (0, 0, 0.0002)
    displace(d, "CLOUDS", 0.008, wob, 0.5, 1)
    return smooth(d, 1)

def syrup(base, rough=0.05, bits=None):
    def b(t, v, h=None):
        n = t.noise(v, 140.0, 3.0)
        c = t.mixc(t.mapr(n, 0.3, 0.7, 0.0, 0.25), base, tuple(min(1, x * 1.35) for x in base))
        if bits:
            bb = t.mapr(t.voronoi(v, 500.0, "F1"), 0.0, 0.1, 1.0, 0.0)
            c = t.mixc(t.mul(bb, 0.8), c, bits)
        return c, rough, t.mul(n, 0.1)
    return b

@food("Honey", "Sticky", 2.4, 0)
def build_honey():
    m = food_mat("Honey", syrup((0.75, 0.38, 0.02)), sss=1.0, sss_rad=(1, 0.6, 0.1), sss_scale=0.01, trans=0.6, ior=1.5, coat=1.0, coat_rough=0.02)
    d = drop("Honey", 0.012, 0.007); assign(d, m)
    st = tube("Thread", [(0.012, 0, 0.003), (0.016, 0.001, 0.006), (0.019, 0.0005, 0.0012)], 0.0005, 10, True, [1.2, 0.5, 1.0]); assign(st, m)
    return dict(parts=[d, st], inner=lambda t, v, h: ((0.8, 0.45, 0.05), 0.05, 0.0), cuts=[], jelly=0.9)

@food("Jam", "Sticky", 2.4, 0)
def build_jam():
    m = food_mat("Jam", syrup((0.42, 0.02, 0.05), 0.08, (0.25, 0.01, 0.03)), sss=1.0, sss_rad=(1, 0.2, 0.2), sss_scale=0.006, trans=0.3, coat=1.0, coat_rough=0.04)
    d = drop("Jam", 0.012, 0.0065, 0.0008); assign(d, m)
    seeds = food_mat("Seed", flat_look((0.85, 0.70, 0.30), 0.4))
    parts = [d]; Rg = random.Random(2)
    for i in range(14):
        a = Rg.uniform(0, 6.28); r = Rg.uniform(0.0, 0.009)
        x, y = r * math.cos(a), r * math.sin(a); z = 0.0062 * math.sqrt(max(0.0, 1 - (r / 0.0125) ** 2))
        s = uvsphere("Seed", 0.00045, (x, y, z), 8, 5, (1.4, 1, 0.7)); assign(s, seeds); parts.append(s)
    return dict(parts=parts, inner=lambda t, v, h: ((0.5, 0.03, 0.06), 0.08, 0.0), cuts=[], jelly=0.7)

@food("Condensed", "Sticky", 2.4, 0)
def build_condensed():
    m = food_mat("Condensed", syrup((0.96, 0.90, 0.74), 0.12), sss=1.0, sss_rad=(1, 0.9, 0.7), sss_scale=0.006, coat=0.8, coat_rough=0.05)
    d = drop("Condensed", 0.012, 0.0055, 0.0006); assign(d, m)
    rib = tube("Ribbon", [(-0.006, -0.004, 0.0055), (-0.001, 0.002, 0.0062), (0.004, -0.002, 0.006), (0.007, 0.003, 0.0055)], 0.0011, 12, True); assign(rib, m)
    return dict(parts=[d, rib], inner=lambda t, v, h: ((0.96, 0.90, 0.76), 0.12, 0.0), cuts=[], jelly=0.5)

# ================================================================ MELTING
@food("IceCream", "Melting", 7.0, 3)
def build_icecream():
    def ice(t, v, h=None):
        n = t.noise(v, 140.0, 5.0); cr = t.voronoi(v, 1800.0, "F1")
        bits = t.mapr(t.voronoi(v, 260.0, "F1"), 0.0, 0.08, 1.0, 0.0)
        c = t.mixc(t.mapr(n, 0.3, 0.7, 0.0, 0.3), (0.95, 0.55, 0.64), (1.0, 0.80, 0.84))
        c = t.mixc(t.mul(bits, 0.85), c, (0.70, 0.05, 0.12))                          # strawberry bits
        return c, t.mapr(n, 0.3, 0.7, 0.45, 0.6), t.add(t.mul(n, 0.5), t.mapr(cr, 0.0, 0.1, 0.3, 0.0))
    m = food_mat("IceCream", ice, sss=0.3, sss_rad=(1, 0.7, 0.7), sss_scale=0.006, bump=0.0005, bump_str=0.8)
    s = uvsphere("Scoop", 0.03, (0, 0, 0.022), 64, 32, (1.15, 1.0, 0.82))
    for v in s.data.vertices:
        if v.co.z < 0.002: v.co.z = 0.002 + (v.co.z - 0.002) * 0.1
        a = math.atan2(v.co.y, v.co.x)
        if v.co.z < 0.012: k = 1.0 + 0.06 * math.cos(a * 9); v.co.x *= k; v.co.y *= k   # the ruffled foot the scoop leaves
    displace(s, "CLOUDS", 0.01, 0.0025, 0.5, 2); displace(s, "CLOUDS", 0.003, 0.0008, 0.5, 2); smooth(s, 1)
    assign(s, m)
    parts = [s]; Rg = random.Random(8)
    foot = uvsphere("Foot", 0.036, (0, 0, 0.0012), 64, 16, (1.12, 1.0, 0.05))          # it has started to melt: a soft foot spreading out
    displace(foot, "CLOUDS", 0.012, 0.0015, 0.5, 2); smooth(foot, 1); assign(foot, m); parts.append(foot)
    def inner(t, v, h): return ice(t, v)
    return dict(parts=parts, inner=inner, cuts=[((0, 0, 0), (1, 0.3, 0.1), 0.002, 0.006), ((0.005, 0, 0), (0.3, 1, 0.1), 0.002, 0.006)], jelly=0.3)

@food("Butter", "Melting", 3.0, 2)
def build_butter():
    def butter(t, v, h=None):
        n = t.noise(v, 200.0, 4.0); curl = t.wave(v, 90.0, "BANDS", 3.0, "Y")
        c = t.mixc(t.mapr(n, 0.3, 0.7, 0.0, 0.25), (1.0, 0.88, 0.50), (1.0, 0.95, 0.70))
        return c, t.mapr(n, 0.3, 0.7, 0.3, 0.42), t.add(t.mul(curl, 0.3), t.mul(n, 0.2))  # knife marks
    m = food_mat("Butter", butter, sss=0.9, sss_rad=(1, 0.9, 0.5), sss_scale=0.004, bump=0.0003)
    b = cube("Butter", (0.03, 0.022, 0.016), (0, 0, 0.008))
    sm = b.modifiers.new("sub", "SUBSURF"); sm.subdivision_type = "SIMPLE"; sm.levels = 4
    smooth(b, 0, 0.0018, 4); displace(b, "CLOUDS", 0.006, 0.0005, 0.5, 2); assign(b, m)
    return dict(parts=[b], inner=lambda t, v, h: butter(t, v), cuts=[((0, 0, 0), (1, 0.2, 0.2), 0.0007, 0.003)], jelly=0.3)

@food("Chocolate", "Melting", 5.0, 4)
def build_chocolate():
    m = food_mat("Chocolate", choc_look((0.20, 0.09, 0.04), 0.2), coat=0.2, bump=0.0002)
    parts = []
    base = cube("Base", (0.05, 0.032, 0.004), (0, 0, 0.002)); smooth(base, 0, 0.0005, 2); assign(base, m); parts.append(base)
    for i in range(2):                                                                 # the raised squares of a bar, bevelled
        for j in range(2):
            sq = cube("Sq", (0.022, 0.0145, 0.0035), (-0.0118 + i * 0.0236, -0.0078 + j * 0.0156, 0.0056))
            for v in sq.data.vertices:
                if v.co.z > 0.006: v.co.x *= 0.88; v.co.y *= 0.84
            smooth(sq, 0, 0.0005, 2); assign(sq, m); parts.append(sq)
    def inner(t, v, h):
        c, r, hh = choc_look((0.25, 0.12, 0.06), 0.6)(t, v)
        return c, 0.65, t.mul(t.noise(v, 1600.0, 4.0), 0.8)                            # the dull snapped face
    return dict(parts=parts, inner=inner, cuts=[((0, 0, 0), (1, 0, 0), 0.0006, 0.002), ((0, 0, 0), (0, 1, 0), 0.0006, 0.002)])

@food("Ice", "Melting", 2.6, 3)
def build_ice():
    def ice(t, v, h=None):
        n = t.noise(v, 300.0, 3.0); bub = t.mapr(t.voronoi(v, 900.0, "F1"), 0.0, 0.08, 1.0, 0.0)
        c = t.mixc(t.mul(bub, 0.5), (0.82, 0.90, 0.97), (1.0, 1.0, 1.0))
        return c, t.mapr(n, 0.3, 0.7, 0.03, 0.12), t.mul(n, 0.1)
    m = food_mat("Ice", ice, trans=0.92, ior=1.31, coat=0.5, coat_rough=0.02, bump=0.0001)
    c = cube("Ice", (0.026, 0.024, 0.022), (0, 0, 0.011))
    sm = c.modifiers.new("sub", "SUBSURF"); sm.subdivision_type = "SIMPLE"; sm.levels = 3
    smooth(c, 0, 0.0025, 4); displace(c, "CLOUDS", 0.008, 0.0004, 0.5, 2)
    for v in c.data.vertices:
        if v.co.z > 0.02: v.co.z -= 0.0015 * (1 - (v.co.x ** 2 + v.co.y ** 2) / 0.00017)   # the dimple of a tray cube
    assign(c, m)
    return dict(parts=[c], inner=lambda t, v, h: ((0.85, 0.92, 0.98), 0.08, 0.0), cuts=[((0, 0, 0), (1, 0.3, 0.2), 0.0008, 0.002), ((0.003, 0, 0), (0.2, 1, -0.3), 0.0008, 0.002)], jelly=0.9)

# ================================================================ BRITTLE
def thin_sheet(name, outline, th, bend=None, seg=4):
    p = prism(name, outline, 0, th)
    sm = p.modifiers.new("sub", "SUBSURF"); sm.subdivision_type = "SIMPLE"; sm.levels = seg
    apply_mods(p)
    if bend:
        for v in p.data.vertices: v.co.z += bend(v.co.x, v.co.y)
    return p

@food("Crisp", "Brittle", 4.5, 2)
def build_crisp():
    def crisp(t, v, h=None):
        n = t.noise(v, 120.0, 5.0); bub = t.mapr(t.voronoi(v, 380.0, "F1"), 0.0, 0.1, 1.0, 0.0)
        salt = t.mapr(t.voronoi(v, 2400.0, "F1"), 0.0, 0.04, 1.0, 0.0)
        c = t.mixc(t.mapr(n, 0.4, 0.8), (0.98, 0.80, 0.32), (0.85, 0.55, 0.15))        # browner where it fried more
        c = t.mixc(t.mul(salt, 0.8), c, (1, 1, 1))
        return c, t.mixf(salt, 0.35, 0.2), t.add(t.mul(bub, 0.8), t.mul(salt, 0.5))   # oil-shiny, bubbled, salt grains
    m = food_mat("Crisp", crisp, sss=0.3, sss_rad=(1, 0.8, 0.3), sss_scale=0.002, bump=0.0003, bump_str=0.9)
    Rg = random.Random(5); outline = []
    for i in range(48):
        a = math.pi * 2 * i / 48; r = 0.0215 * (1 + 0.06 * math.sin(a * 3 + 1) + Rg.uniform(-0.03, 0.03))
        outline.append((r * math.cos(a) * 1.05, r * math.sin(a)))
    c = thin_sheet("Crisp", outline, 0.0011, lambda x, y: 0.006 + 180.0 * (x * x - y * y) * 0.06, 5)   # a saddle, like every crisp
    displace(c, "CLOUDS", 0.006, 0.0012, 0.5, 2); smooth(c, 1); assign(c, m)
    return dict(parts=[c], inner=lambda t, v, h: ((0.98, 0.88, 0.55), 0.6, 0.0), cuts=[((0.002, 0, 0), (1, 0.4, 0), 0.002, 0.004)])

@food("Cracker", "Brittle", 4.0, 2)
def build_cracker():
    m = food_mat("Cracker", crust_look((0.90, 0.66, 0.34), (0.70, 0.44, 0.18)), bump=0.0003, bump_str=0.8)
    c = cube("Cracker", (0.04, 0.04, 0.004), (0, 0, 0.002))
    sm = c.modifiers.new("sub", "SUBSURF"); sm.subdivision_type = "SIMPLE"; sm.levels = 5
    smooth(c, 0, 0.0008, 2); displace(c, "CLOUDS", 0.006, 0.0004, 0.5, 2)
    apply_mods(c)
    for i in range(3):                                                                 # the docking holes
        for j in range(3):
            hole = cyl("Hole", 0.0009, 0.003, (-0.012 + i * 0.012, -0.012 + j * 0.012, 0.0048), 12)
            boolean(c, hole, "DIFFERENCE"); bpy.data.objects.remove(hole, do_unlink=True)
    assign(c, m)
    return dict(parts=[c], inner=lambda t, v, h: sponge_look((0.95, 0.80, 0.52), 1600.0)(t, v), cuts=[((0.002, 0, 0), (1, 0.3, 0), 0.0015, 0.003)])

@food("Wafer", "Brittle", 4.5, 2)
def build_wafer():
    m = food_mat("Wafer", crust_look((0.95, 0.80, 0.50), (0.85, 0.62, 0.30)), bump=0.0003)
    cr = food_mat("WCream", cream_look((0.62, 0.38, 0.20), 0.45, 200.0))
    parts = []
    hs = [(0.0, 0.0028, m), (0.0028, 0.0042, cr), (0.0042, 0.007, m), (0.007, 0.0084, cr), (0.0084, 0.0112, m)]
    for k, (z0, z1, mm) in enumerate(hs):
        sl = cube("W%d" % k, (0.045, 0.012, z1 - z0), (0, 0, (z0 + z1) * 0.5))
        if mm is m:
            sm = sl.modifiers.new("sub", "SUBSURF"); sm.subdivision_type = "SIMPLE"; sm.levels = 4
            apply_mods(sl)
            for v in sl.data.vertices:                                                 # the grid of a wafer sheet
                if abs(v.co.z - z1) < 1e-5 and k == 4 or abs(v.co.z - z0) < 1e-5 and k == 0:
                    g = (abs(math.sin(v.co.x * 520)) < 0.25) or (abs(math.sin(v.co.y * 520)) < 0.25)
                    v.co.z += (-0.0004 if g else 0.0) * (1 if k == 4 else -1)
        smooth(sl, 0, 0.0002, 1); assign(sl, mm); parts.append(sl)
    def inner(t, v, h):
        layers = t.add(band(t, h, 0.25, 0.375, 0.01), band(t, h, 0.625, 0.75, 0.01), True)
        c, r, hh = crust_look((0.97, 0.86, 0.60), (0.9, 0.7, 0.4))(t, v)
        return t.mixc(layers, c, (0.62, 0.38, 0.20)), r, t.mul(t.voronoi(v, 2000.0, "F1"), -0.6)
    return dict(parts=parts, inner=inner, cuts=[((0.002, 0, 0), (1, 0.2, 0.1), 0.001, 0.003)], hmin=(0, 0, 0.0), hmax=(0, 0, 0.0112))

@food("Eggshell", "Brittle", 3.0, 2)
def build_eggshell():
    def shell(t, v, h=None):
        n = t.noise(v, 90.0, 4.0); sp = t.mapr(t.voronoi(v, 500.0, "F1"), 0.0, 0.06, 1.0, 0.0)
        c = t.mixc(t.mapr(n, 0.3, 0.7), (0.80, 0.58, 0.40), (0.88, 0.70, 0.52))       # a brown egg, speckled
        c = t.mixc(t.mul(sp, 0.5), c, (0.62, 0.40, 0.26))
        return c, 0.55, t.mul(t.noise(v, 2200.0, 2.0), 0.2)
    m = food_mat("Shell", shell, bump=0.0002)
    memb = food_mat("Membrane", flat_look((0.96, 0.95, 0.90), 0.6))
    outer = uvsphere("Out", 0.022, (0, 0, 0), 64, 32, (1.0, 0.78, 0.78))
    inn = uvsphere("In", 0.0216, (0, 0, 0), 64, 32, (1.0, 0.775, 0.775))
    boolean(outer, inn, "DIFFERENCE"); bpy.data.objects.remove(inn, do_unlink=True)
    Rg = random.Random(3); outline = []
    for i in range(40):                                                                # a jagged broken piece of it
        a = math.pi * 2 * i / 40; r = 0.0125 * (1 + Rg.uniform(-0.18, 0.18))
        outline.append((r * math.cos(a), r * math.sin(a)))
    cutter_ = prism("Keep", outline, -0.03, 0.03); cutter_.rotation_euler = (0, math.pi / 2, 0); cutter_.location = (0.0, 0, 0)
    boolean(outer, cutter_, "INTERSECT"); bpy.data.objects.remove(cutter_, do_unlink=True)
    keep = cube("Half", (0.05, 0.05, 0.05), (0.025 - 0.006, 0, 0)); boolean(outer, keep, "INTERSECT"); bpy.data.objects.remove(keep, do_unlink=True)
    assign(outer, m)
    lay([outer], (0, -math.pi / 2, 0))                                                 # lying like a little bowl
    return dict(parts=[outer], inner=lambda t, v, h: ((0.95, 0.93, 0.88), 0.6, 0.0), cuts=[((0, 0, 0), (0.2, 1, 0), 0.001, 0.003)])

# ================================================================ JUICY
@food("Apple", "Juicy", 6.0, 2)
def build_apple():
    R = 0.035
    def apple(t, v, h=None):
        r = radial(t, v, axis="x"); sk = t.mapr(r, R * 0.86, R * 0.89)
        skin = fruit_skin((0.70, 0.06, 0.06), (0.95, 0.85, 0.55), 0.3, (0.85, 0.55, 0.15))(t, v)
        fl = flesh_look((0.98, 0.93, 0.72), (1.0, 0.98, 0.85), 0.2)(t, v)
        c = t.mixc(sk, fl[0], skin[0])
        core = t.mapr(r, 0.0065, 0.0095, 1.0, 0.0)
        c = t.mixc(t.mul(core, 0.4), c, (0.92, 0.85, 0.60))
        return c, t.mixf(sk, 0.18, 0.32), t.mixf(sk, fl[2], skin[2])
    m = food_mat("Apple", apple, sss=0.6, sss_rad=(1, 0.9, 0.6), sss_scale=0.005, coat=0.2, bump=0.0002)
    s = sphere_wedge("Apple", R, math.radians(52), 0.0055)
    for v in s.data.vertices: v.co.z *= 0.92
    smooth(s, 1); assign(s, m)
    seedm = food_mat("ASeed", glossy_look((0.25, 0.12, 0.05), 0.2))
    sd = uvsphere("Seed", 0.0028, (0.0072, 0.0, 0.004), 16, 8, (1.0, 0.55, 0.4)); assign(sd, seedm)
    parts = [s, sd]
    lay(parts, (math.pi / 2, 0, 0))                                                    # on its side, the skin up
    def inner(t, v, h):
        fl = flesh_look((0.98, 0.93, 0.72), (1.0, 0.98, 0.85), 0.2)(t, v)
        return t.mixc(t.mapr(h, 0.86, 0.89), fl[0], (0.70, 0.06, 0.06)), 0.25, fl[2]
    return dict(parts=parts, inner=inner, cuts=[((0.0, 0, 0), (1, 0, 0.2), 0.0015, 0.004)])

@food("Watermelon", "Juicy", 8.0, 2)
def build_watermelon():
    R = 0.08
    def wm(t, v, h=None):
        x, y, z = t.sep(v)
        r = radial(t, v)
        n = t.noise(v, 200.0, 4.0)
        c = t.mixc(t.mapr(n, 0.3, 0.7), (0.85, 0.06, 0.10), (0.95, 0.22, 0.25))         # red flesh
        c = t.mixc(t.mapr(r, R * 0.80, R * 0.84), c, (0.92, 0.95, 0.78))                # white rind
        stripes = t.wave(v, 40.0, "BANDS", 6.0, "Y")
        c = t.mixc(t.mapr(r, R * 0.93, R * 0.95), c, t.mixc(t.mapr(stripes, 0.4, 0.6), (0.10, 0.30, 0.08), (0.30, 0.55, 0.15)))
        return c, t.mixf(t.mapr(r, R * 0.93, R * 0.95), 0.2, 0.35), t.mul(t.voronoi(v, 1500.0, "DISTANCE_TO_EDGE"), 0.5)
    m = food_mat("Watermelon", wm, sss=0.15, sss_rad=(1, 0.4, 0.4), sss_scale=0.006, bump=0.0003)
    pts = [(0, 0)] + [(R * math.cos(a), R * math.sin(a)) for a in [math.radians(-22 + 44 * i / 30.0) for i in range(31)]]
    w = prism("Wedge", pts, -0.009, 0.009)
    sm = w.modifiers.new("sub", "SUBSURF"); sm.subdivision_type = "SIMPLE"; sm.levels = 3
    displace(w, "CLOUDS", 0.01, 0.0008, 0.5, 2); smooth(w, 1, 0.001, 2); assign(w, m)
    seedm = food_mat("WSeed", glossy_look((0.06, 0.04, 0.03), 0.15))
    parts = [w]; Rg = random.Random(9)
    for i in range(16):
        r = Rg.uniform(0.022, 0.06); a = Rg.uniform(-0.3, 0.3); side = Rg.choice((-1, 1))
        sd = uvsphere("Seed", 0.0022, (r * math.cos(a), r * math.sin(a), side * 0.0088), 12, 6, (1.4, 0.8, 0.45)); assign(sd, seedm); parts.append(sd)
    lay(parts, (0, 0, math.pi))                                                         # the rind towards -X
    def inner(t, v, h):
        n = t.noise(v, 300.0, 3.0)
        return t.mixc(t.mapr(n, 0.3, 0.7), (0.92, 0.18, 0.22), (0.98, 0.40, 0.40)), 0.2, t.mul(t.voronoi(v, 1500.0, "DISTANCE_TO_EDGE"), 0.6)
    return dict(parts=parts, inner=inner, cuts=[((0.0, 0, 0), (1, 0.15, 0), 0.002, 0.005)])

@food("Tomato", "Juicy", 4.0, 2)
def build_tomato():
    R = 0.03
    def tom(t, v, h=None):
        r = radial(t, v, axis="x"); sk = t.mapr(r, R * 0.80, R * 0.83)
        loc = t.mapr(t.voronoi(v, 120.0, "F1"), 0.18, 0.3)                              # the seed chambers with their gel
        seeds = t.mapr(t.voronoi(v, 900.0, "F1"), 0.0, 0.1, 1.0, 0.0)
        fl = t.mixc(loc, (0.85, 0.45, 0.20), (0.92, 0.18, 0.10))
        fl = t.mixc(t.mul(t.mul(seeds, t.sub(1.0, loc)), 0.8), fl, (0.95, 0.85, 0.50))
        c = t.mixc(sk, fl, (0.82, 0.08, 0.04))
        return c, t.mixf(sk, 0.15, 0.12), t.mul(seeds, 0.4)
    m = food_mat("Tomato", tom, sss=0.7, sss_rad=(1, 0.3, 0.2), sss_scale=0.005, coat=0.5, coat_rough=0.05, bump=0.0002)
    s = sphere_wedge("Tomato", R, math.radians(60), 0.0)
    for v in s.data.vertices: v.co.z *= 0.85
    smooth(s, 1); assign(s, m)
    lay([s], (math.pi / 2, 0, 0))
    return dict(parts=[s], inner=lambda t, v, h: ((0.90, 0.25, 0.12), 0.12, 0.0), cuts=[((0.0, 0, 0), (1, 0, 0.2), 0.0012, 0.003)])
