# Ant Kitchen: the kitchen set, modelled from code. Real proportions in metres; X along the wall, -Y towards the room.
# Run:  blender --background --factory-startup --python kitchen_set.py -- <out_dir>
import bpy, sys, os, math
from mathutils import Vector

OUT = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else "."
bpy.ops.wm.read_factory_settings(use_empty=True)
MATS = {}

def mat(name, color, rough=0.5, metal=0.0):
    if name in MATS:
        return MATS[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    m.diffuse_color = (*color, 1)
    MATS[name] = m
    return m

CREAM = mat("Cabinet", (0.42, 0.60, 0.44), 0.5)          # sage-cream painted wood
CREAM_D = mat("CabinetInset", (0.34, 0.51, 0.37), 0.55)
WOOD = mat("Butcher", (0.68, 0.40, 0.16), 0.4)
WOOD_D = mat("Walnut", (0.33, 0.20, 0.10), 0.5)
DARK = mat("Plinth", (0.08, 0.08, 0.09), 0.7)
BRASS = mat("Brass", (0.86, 0.62, 0.22), 0.3, 1.0)
STEEL = mat("Steel", (0.72, 0.74, 0.76), 0.28, 1.0)
CHROME = mat("Chrome", (0.9, 0.9, 0.92), 0.12, 1.0)
ENAMEL = mat("Enamel", (0.95, 0.94, 0.90), 0.25)
GLASS = mat("BlackGlass", (0.02, 0.02, 0.025), 0.08)
IRON = mat("Iron", (0.05, 0.05, 0.05), 0.6, 0.6)
MINT = mat("FridgeRed", (0.80, 0.16, 0.12), 0.2)
TILE = mat("Tile", (0.96, 0.95, 0.90), 0.12)
GROUT = mat("Grout", (0.55, 0.55, 0.52), 0.9)
RED = mat("Red", (0.75, 0.10, 0.08), 0.35)
HONEY = mat("Honey", (0.90, 0.55, 0.05), 0.15)
JAM = mat("Jam", (0.55, 0.05, 0.12), 0.15)
SUGAR = mat("Sugar", (0.95, 0.95, 0.95), 0.6)

def finish(o, m, name):
    o.name = name
    o.data.materials.append(m)
    try:
        bpy.ops.object.shade_smooth()
    except Exception:
        pass
    wn = o.modifiers.new("WN", "WEIGHTED_NORMAL")
    wn.keep_sharp = True
    return o

def rbox(name, sx, sy, sz, x, y, z, m, bev=0.008, seg=3):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z))
    o = bpy.context.object
    o.scale = (sx, sy, sz)
    bpy.ops.object.transform_apply(scale=True)
    b = o.modifiers.new("Bevel", "BEVEL")
    b.width = min(bev, 0.45 * min(sx, sy, sz))
    b.segments = seg
    b.limit_method = "ANGLE"
    return finish(o, m, name)

def cyl(name, r, depth, x, y, z, m, axis="Z", verts=32, bev=0.0):
    rot = {"Z": (0, 0, 0), "X": (0, math.pi / 2, 0), "Y": (math.pi / 2, 0, 0)}[axis]
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=depth, vertices=verts, location=(x, y, z), rotation=rot)
    o = bpy.context.object
    if bev > 0:
        b = o.modifiers.new("Bevel", "BEVEL")
        b.width = min(bev, 0.45 * min(r, depth))
        b.segments = 3
        b.limit_method = "ANGLE"
    return finish(o, m, name)

def bar_handle(name, x, y, z, length, m=BRASS, axis="X", r=0.007, stand=0.028):
    # A bar on two standoffs, y is the face of the door (the bar sits in front of it).
    cyl(name, r, length, x, y - stand, z, m, axis)
    d = length * 0.36
    for s in (-1, 1):
        px, pz = (x + s * d, z) if axis == "X" else (x, z + s * d)
        cyl(name + "_post", r * 0.8, stand, px, y - stand / 2, pz, m, "Y", 16)

def shaker(name, xc, zc, w, h, y, handle="top", m=CREAM):
    # Flat slab with a raised frame: the classic shaker door.
    rbox(name, w, 0.018, h, xc, y, zc, CREAM_D, 0.004)
    fw, ft = 0.055, 0.010
    yf = y - 0.009 - ft / 2
    rbox(name + "_l", fw, ft, h, xc - w / 2 + fw / 2, yf, zc, m, 0.004)
    rbox(name + "_r", fw, ft, h, xc + w / 2 - fw / 2, yf, zc, m, 0.004)
    rbox(name + "_t", w - 2 * fw, ft, fw, xc, yf, zc + h / 2 - fw / 2, m, 0.004)
    rbox(name + "_b", w - 2 * fw, ft, fw, xc, yf, zc - h / 2 + fw / 2, m, 0.004)
    face = yf - ft / 2
    if handle == "top":
        bar_handle(name + "_h", xc, face, zc + h / 2 - fw / 2, min(0.14, w * 0.4))
    elif handle == "bottom":
        bar_handle(name + "_h", xc, face, zc - h / 2 + fw / 2, min(0.14, w * 0.4))
    elif handle == "mid":
        bar_handle(name + "_h", xc, face, zc, min(0.16, w * 0.4))

GAP = 0.004

def base_cabinet(name, x0, w, kind):
    xc = x0 + w / 2
    rbox(name + "_body", w - 0.002, 0.57, 0.76, xc, -0.285, 0.48, CREAM, 0.004)
    rbox(name + "_plinth", w, 0.50, 0.10, xc, -0.25, 0.05, DARK, 0.003)
    y = -0.582
    if kind == "door":
        shaker(name + "_door", xc, 0.48, w - 2 * GAP, 0.76 - 2 * GAP, y)
    elif kind == "2door":
        dw = (w - 3 * GAP) / 2
        shaker(name + "_doorL", x0 + GAP + dw / 2, 0.48, dw, 0.76 - 2 * GAP, y)
        shaker(name + "_doorR", x0 + w - GAP - dw / 2, 0.48, dw, 0.76 - 2 * GAP, y)
    elif kind == "drawers":
        z = 0.86
        for i, h in enumerate((0.17, 0.29, 0.29)):
            shaker("%s_drawer%d" % (name, i), xc, z - GAP - (h - GAP) / 2, w - 2 * GAP, h - 2 * GAP, y, "mid")
            z -= h + 0.0033

def upper_cabinet(name, x0, w, doors=1):
    xc = x0 + w / 2
    rbox(name + "_body", w - 0.002, 0.32, 0.72, xc, -0.16, 1.81, CREAM, 0.004)
    y = -0.332
    if doors == 1:
        shaker(name + "_door", xc, 1.81, w - 2 * GAP, 0.72 - 2 * GAP, y, "bottom")
    else:
        dw = (w - 3 * GAP) / 2
        shaker(name + "_doorL", x0 + GAP + dw / 2, 1.81, dw, 0.72 - 2 * GAP, y, "bottom")
        shaker(name + "_doorR", x0 + w - GAP - dw / 2, 1.81, dw, 0.72 - 2 * GAP, y, "bottom")
    # Moulding on top.
    rbox(name + "_crown", w + 0.004, 0.345, 0.035, xc, -0.17, 2.19, CREAM, 0.012, 4)

# ---------------------------------------------------------------- layout, left to right
X_FRIDGE, X_DRAW, X_SINK, X_BASE2, X_STOVE, X_BASE1, X_END = 0.0, 0.78, 1.23, 2.03, 2.63, 3.23, 3.68

# Fridge: retro, rounded, on feet (room for ants underneath).
fx = 0.375
rbox("Fridge_body", 0.72, 0.62, 1.80, fx, -0.33, 0.98, MINT, 0.055, 6)
for sx in (-1, 1):
    for sy in (-1, 1):
        cyl("Fridge_foot", 0.028, 0.08, fx + sx * 0.29, -0.33 + sy * 0.24, 0.04, DARK)
rbox("Fridge_doorTop", 0.72, 0.055, 0.50, fx, -0.668, 1.625, MINT, 0.035, 6)
rbox("Fridge_doorMain", 0.72, 0.055, 1.25, fx, -0.668, 0.735, MINT, 0.035, 6)
rbox("Fridge_seal", 0.70, 0.02, 1.78, fx, -0.632, 0.985, DARK, 0.004)
bar_handle("Fridge_hTop", 0.12, -0.696, 1.50, 0.22, CHROME, "Z", 0.012, 0.045)
bar_handle("Fridge_hMain", 0.12, -0.696, 1.10, 0.40, CHROME, "Z", 0.012, 0.045)
rbox("Fridge_badge", 0.13, 0.006, 0.032, 0.56, -0.698, 1.79, CHROME, 0.003)

base_cabinet("BaseDrawers", X_DRAW, X_SINK - X_DRAW, "drawers")
base_cabinet("BaseSink", X_SINK, X_BASE2 - X_SINK, "2door")
base_cabinet("BaseTwo", X_BASE2, X_STOVE - X_BASE2, "2door")
base_cabinet("BaseOne", X_BASE1, X_END - X_BASE1, "door")

# Worktops (butcher block), the left one with a hole for the sink.
top1 = rbox("Worktop_left", X_STOVE - X_DRAW, 0.63, 0.04, (X_DRAW + X_STOVE) / 2, -0.315, 0.88, WOOD, 0.012, 4)
rbox("Worktop_right", X_END - X_BASE1, 0.63, 0.04, (X_BASE1 + X_END) / 2, -0.315, 0.88, WOOD, 0.012, 4)
sx = (X_SINK + X_BASE2) / 2
bpy.ops.mesh.primitive_cube_add(size=1, location=(sx, -0.31, 0.88))
cutter = bpy.context.object
cutter.scale = (0.52, 0.38, 0.2)
cutter.name = "SinkCutter"
cutter.hide_render = True
cutter.display_type = "WIRE"
bm = top1.modifiers.new("SinkHole", "BOOLEAN")
bm.operation = "DIFFERENCE"
bm.object = cutter
top1.modifiers.move(top1.modifiers.find("SinkHole"), 0)
# Basin: bottom and four walls of steel, a rim on the worktop.
bw, bd, bh, t = 0.50, 0.36, 0.17, 0.008
bz = 0.90 - bh / 2
rbox("Sink_bottom", bw, bd, t, sx, -0.31, 0.90 - bh, STEEL, 0.003)
rbox("Sink_front", bw, t, bh, sx, -0.31 - bd / 2, bz, STEEL, 0.003)
rbox("Sink_back", bw, t, bh, sx, -0.31 + bd / 2, bz, STEEL, 0.003)
rbox("Sink_left", t, bd, bh, sx - bw / 2, -0.31, bz, STEEL, 0.003)
rbox("Sink_right", t, bd, bh, sx + bw / 2, -0.31, bz, STEEL, 0.003)
rbox("Sink_rimF", bw + 0.05, 0.03, 0.006, sx, -0.31 - bd / 2 - 0.008, 0.902, STEEL, 0.002)
rbox("Sink_rimB", bw + 0.05, 0.03, 0.006, sx, -0.31 + bd / 2 + 0.008, 0.902, STEEL, 0.002)
rbox("Sink_rimL", 0.03, bd + 0.04, 0.006, sx - bw / 2 - 0.008, -0.31, 0.902, STEEL, 0.002)
rbox("Sink_rimR", 0.03, bd + 0.04, 0.006, sx + bw / 2 + 0.008, -0.31, 0.902, STEEL, 0.002)
cyl("Sink_drain", 0.03, 0.006, sx, -0.31, 0.90 - bh + 0.006, IRON)
# Faucet: a swan neck.
cyl("Faucet_base", 0.028, 0.03, sx, -0.07, 0.915, CHROME, bev=0.004)
cu = bpy.data.curves.new("FaucetNeck", "CURVE")
cu.dimensions = "3D"
sp = cu.splines.new("BEZIER")
sp.bezier_points.add(3)
for p, co in zip(sp.bezier_points, ((0, 0, 0), (0, 0, 0.26), (0, -0.11, 0.36), (0, -0.22, 0.27))):
    p.co = co
    p.handle_left_type = p.handle_right_type = "AUTO"
cu.bevel_depth = 0.012
cu.bevel_resolution = 6
cu.use_fill_caps = True
cu.materials.append(CHROME)
neck = bpy.data.objects.new("Faucet_neck", cu)
neck.location = (sx, -0.07, 0.92)
bpy.context.collection.objects.link(neck)
for s, col in ((-1, RED), (1, mat("Blue", (0.1, 0.25, 0.75), 0.35))):
    cyl("Faucet_tap", 0.016, 0.035, sx + s * 0.09, -0.07, 0.918, CHROME, bev=0.003)
    rbox("Faucet_lever", 0.05, 0.012, 0.008, sx + s * 0.10, -0.07, 0.94, CHROME, 0.003)
    cyl("Faucet_dot", 0.006, 0.004, sx + s * 0.09, -0.07, 0.937, col)

# Stove: enamel range, glass top, four burners, knobs, oven with a window.
stx = (X_STOVE + X_BASE1) / 2
rbox("Stove_body", 0.596, 0.60, 0.86, stx, -0.30, 0.43, ENAMEL, 0.014, 4)
rbox("Stove_top", 0.596, 0.60, 0.022, stx, -0.30, 0.871, GLASS, 0.006)
rbox("Stove_back", 0.596, 0.03, 0.10, stx, -0.015, 0.93, ENAMEL, 0.01, 4)
for i, (bx, by, r) in enumerate(((-0.15, -0.16, 0.085), (0.15, -0.16, 0.065), (-0.15, -0.44, 0.065), (0.15, -0.44, 0.085))):
    cyl("Burner_plate%d" % i, r, 0.006, stx + bx, by, 0.885, IRON)
    bpy.ops.mesh.primitive_torus_add(major_radius=r * 0.82, minor_radius=0.007, location=(stx + bx, by, 0.892))
    finish(bpy.context.object, mat("BurnerRing", (0.65, 0.12, 0.05), 0.4), "Burner_ring%d" % i)
rbox("Stove_panel", 0.596, 0.03, 0.10, stx, -0.605, 0.795, STEEL, 0.008)
for i in range(5):
    kx = stx - 0.22 + i * 0.11
    cyl("Knob%d" % i, 0.022, 0.03, kx, -0.632, 0.795, DARK, "Y", 24, 0.004)
    rbox("Knob%d_mark" % i, 0.005, 0.006, 0.018, kx, -0.648, 0.805, ENAMEL, 0.001)
rbox("Oven_door", 0.56, 0.03, 0.50, stx, -0.607, 0.47, ENAMEL, 0.012, 4)
rbox("Oven_window", 0.40, 0.008, 0.26, stx, -0.624, 0.46, GLASS, 0.02, 5)
bar_handle("Oven_handle", stx, -0.622, 0.685, 0.44, CHROME, "X", 0.011, 0.045)
rbox("Oven_drawer", 0.56, 0.025, 0.14, stx, -0.605, 0.115, ENAMEL, 0.01, 4)
rbox("Stove_feet", 0.52, 0.52, 0.03, stx, -0.29, 0.015, DARK, 0.003)

# Wall units: door, open shelves over the sink, two doors, hood, door.
upper_cabinet("UpperA", X_DRAW, X_SINK - X_DRAW)
upper_cabinet("UpperB", X_BASE2, X_STOVE - X_BASE2, 2)
upper_cabinet("UpperC", X_BASE1, X_END - X_BASE1)
shw = X_BASE2 - X_SINK
for i, z in enumerate((1.50, 1.86)):
    rbox("Shelf%d" % i, shw - 0.02, 0.26, 0.03, sx, -0.13, z, WOOD, 0.006)
    for s in (-1, 1):
        rbox("Shelf%d_bracket" % i, 0.02, 0.20, 0.02, sx + s * (shw / 2 - 0.08), -0.11, z - 0.025, IRON, 0.004)
        rbox("Shelf%d_bracketV" % i, 0.02, 0.02, 0.16, sx + s * (shw / 2 - 0.08), -0.02, z - 0.09, IRON, 0.004)
def jar(name, x, z, r, h, content, lid):
    cyl(name, r, h, x, -0.13, z + h / 2, content, bev=0.006)
    cyl(name + "_lid", r * 1.04, 0.022, x, -0.13, z + h + 0.011, lid, bev=0.004)
jar("JarHoney", sx - 0.24, 1.515, 0.05, 0.13, HONEY, BRASS)
jar("JarJam", sx - 0.10, 1.515, 0.045, 0.10, JAM, RED)
jar("JarSugar", sx + 0.07, 1.515, 0.055, 0.15, SUGAR, WOOD_D)
jar("JarSmall", sx + 0.22, 1.515, 0.035, 0.08, HONEY, STEEL)
for i in range(4):
    rbox("Plate%d" % i, 0.19, 0.19, 0.012, sx - 0.15, -0.13, 1.881 + i * 0.014, ENAMEL, 0.006, 3)
cyl("Mug", 0.04, 0.09, sx + 0.16, -0.13, 1.92, mat("MugBlue", (0.15, 0.35, 0.6), 0.3), bev=0.006)
# Hood.
rbox("Hood_body", 0.596, 0.48, 0.10, stx, -0.25, 1.50, STEEL, 0.012, 4)
rbox("Hood_chimney", 0.26, 0.24, 0.66, stx, -0.13, 1.88, STEEL, 0.01, 3)
rbox("Hood_filter", 0.50, 0.38, 0.006, stx, -0.26, 1.448, IRON, 0.003)

# Tiled splashback: one tile, repeated.
bx0, bx1 = X_DRAW, X_END
rbox("Splash_grout", bx1 - bx0, 0.004, 0.56, (bx0 + bx1) / 2, -0.002, 1.17, GROUT, 0.0)
tile = rbox("Splash_tile", 0.096, 0.010, 0.096, bx0 + 0.05, -0.008, 0.95, TILE, 0.004, 3)
for axis, count in ((0, int(round((bx1 - bx0) / 0.1))), (2, 5)):
    a = tile.modifiers.new("Array%d" % axis, "ARRAY")
    a.use_relative_offset = False
    a.use_constant_offset = True
    off = [0.0, 0.0, 0.0]
    off[axis] = 0.1
    a.constant_offset_displace = off
    a.count = count

# A few things on the worktop and the stove.
rbox("Board", 0.32, 0.22, 0.02, 2.33, -0.34, 0.91, WOOD_D, 0.008, 3)
cyl("Pot", 0.09, 0.10, stx - 0.15, -0.16, 0.94, STEEL, bev=0.006)
cyl("Pot_lid", 0.092, 0.012, stx - 0.15, -0.16, 0.996, STEEL, bev=0.004)
cyl("Pot_knob", 0.014, 0.02, stx - 0.15, -0.16, 1.012, DARK, bev=0.003)
for s in (-1, 1):
    rbox("Pot_ear", 0.03, 0.05, 0.012, stx - 0.15 + s * 0.10, -0.16, 0.975, DARK, 0.004)
cyl("Kettle", 0.075, 0.11, 3.45, -0.30, 0.955, RED, bev=0.03)
cyl("Kettle_lid", 0.05, 0.02, 3.45, -0.30, 1.02, RED, bev=0.006)
cyl("Kettle_knob", 0.012, 0.02, 3.45, -0.30, 1.04, DARK)
cyl("Kettle_spout", 0.014, 0.09, 3.535, -0.30, 0.98, RED, "X")

# ---------------------------------------------------------------- second pass: the room around the set
FABRIC = mat("Fabric", (0.96, 0.72, 0.16), 0.9)
TERRA = mat("Terracotta", (0.72, 0.35, 0.20), 0.8)
LEAF = mat("Leaf", (0.16, 0.50, 0.18), 0.5)
PAPER = mat("Paper", (0.97, 0.96, 0.88), 0.9)
SKY = mat("WindowSky", (0.55, 0.80, 1.0), 0.3)
BULB = mat("Bulb", (1.0, 0.85, 0.55), 0.3)
for m_, strength in ((SKY, 2.5), (BULB, 12.0)):
    try:
        b_ = m_.node_tree.nodes["Principled BSDF"]
        b_.inputs["Emission Color"].default_value = b_.inputs["Base Color"].default_value
        b_.inputs["Emission Strength"].default_value = strength
    except Exception:
        pass

def grain(m, stretch=9.0):
    # Streaks along X: reads as wood grain in the preview. In the game the engine's own wood material takes over.
    try:
        nt = m.node_tree
        b = nt.nodes["Principled BSDF"]
        base = tuple(b.inputs["Base Color"].default_value)
        tc = nt.nodes.new("ShaderNodeTexCoord")
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (0.7, stretch, stretch)
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = 7.0
        nz.inputs["Detail"].default_value = 7.0
        mix = nt.nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.inputs[6].default_value = (base[0] * 0.62, base[1] * 0.58, base[2] * 0.55, 1)
        mix.inputs[7].default_value = (min(1, base[0] * 1.12), min(1, base[1] * 1.1), min(1, base[2] * 1.05), 1)
        nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
        nt.links.new(mp.outputs["Vector"], nz.inputs["Vector"])
        nt.links.new(nz.outputs[0], mix.inputs[0])
        nt.links.new(mix.outputs[2], b.inputs["Base Color"])
    except Exception as e:
        print("grain skipped:", e)
grain(WOOD)
grain(WOOD_D, 12.0)

def ball(name, r, x, y, z, m, scale=(1, 1, 1)):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=r, segments=24, ring_count=14, location=(x, y, z))
    o = bpy.context.object
    o.scale = scale
    return finish(o, m, name)

def legs4(name, cx, cy, w, d, h, r, m):
    for sx_ in (-1, 1):
        for sy_ in (-1, 1):
            cyl(name, r, h, cx + sx_ * w / 2, cy + sy_ * d / 2, h / 2, m, bev=0.004)

# Dining table, two chairs, a fruit bowl, a lamp over it.
TX, TY = 4.45, -1.70
rbox("Table_top", 1.15, 0.78, 0.04, TX, TY, 0.74, WOOD, 0.012, 4)
rbox("Table_apron", 1.0, 0.64, 0.08, TX, TY, 0.68, WOOD_D, 0.006)
legs4("Table_leg", TX, TY, 0.96, 0.60, 0.72, 0.03, WOOD_D)
def chair(name, cx, cy, face):
    rbox(name + "_seat", 0.42, 0.42, 0.04, cx, cy, 0.45, CREAM, 0.014, 4)
    legs4(name + "_leg", cx, cy, 0.34, 0.34, 0.44, 0.018, WOOD_D)
    by = cy + face * 0.19
    for s in (-1, 1):
        cyl(name + "_post", 0.018, 0.50, cx + s * 0.17, by, 0.70, WOOD_D, bev=0.004)
    rbox(name + "_back", 0.40, 0.025, 0.17, cx, by, 0.87, CREAM, 0.012, 4)
    rbox(name + "_rail", 0.36, 0.02, 0.05, cx, by, 0.66, CREAM, 0.008, 3)
chair("ChairA", TX - 0.28, TY + 0.64, 1)
chair("ChairB", TX + 0.22, TY - 0.64, -1)
cyl("Bowl", 0.15, 0.07, TX, TY, 0.795, ENAMEL, bev=0.03)
for i, (dx, dy, dz, col) in enumerate(((0.05, 0.02, 0, (0.95, 0.45, 0.04)), (-0.05, 0.04, 0, (0.78, 0.08, 0.08)), (0.0, -0.06, 0, (0.50, 0.72, 0.12)), (0.0, 0.0, 0.06, (0.95, 0.45, 0.04)))):
    ball("Fruit%d" % i, 0.046, TX + dx, TY + dy, 0.85 + dz, mat("Fruit%d" % i, col, 0.4))
cyl("Lamp_cord", 0.004, 0.85, TX, TY, 2.47, DARK, verts=8)
bpy.ops.mesh.primitive_cone_add(radius1=0.25, radius2=0.05, depth=0.22, vertices=40, location=(TX, TY, 1.95))
finish(bpy.context.object, RED, "Lamp_shade")
ball("Lamp_bulb", 0.045, TX, TY, 1.88, BULB)

# Pedal bin by the last cabinet.
cyl("Bin", 0.15, 0.42, 3.92, -0.30, 0.21, STEEL, bev=0.012)
cyl("Bin_lid", 0.155, 0.03, 3.92, -0.30, 0.435, DARK, bev=0.008)
rbox("Bin_pedal", 0.09, 0.06, 0.015, 3.92, -0.48, 0.02, DARK, 0.004)

# Window with curtains, a plant on the sill.
WX = 4.75
rbox("Window_frame", 0.96, 0.06, 1.16, WX, -0.01, 1.60, ENAMEL, 0.012, 3)
rbox("Window_glass", 0.84, 0.02, 1.04, WX, -0.042, 1.60, SKY, 0.0)
rbox("Window_mullV", 0.03, 0.03, 1.04, WX, -0.055, 1.60, ENAMEL, 0.006)
rbox("Window_mullH", 0.84, 0.03, 0.03, WX, -0.055, 1.60, ENAMEL, 0.006)
rbox("Window_sill", 1.06, 0.17, 0.03, WX, -0.085, 1.005, WOOD, 0.008, 3)
cyl("Curtain_rod", 0.012, 1.36, WX, -0.11, 2.26, BRASS, "X")
for s in (-1, 1):
    for k in range(3):
        cyl("Curtain_fold", 0.045, 1.22, WX + s * (0.44 + k * 0.075), -0.11, 1.63, FABRIC, bev=0.02)
cyl("Plant_pot", 0.06, 0.09, WX - 0.25, -0.09, 1.065, TERRA, bev=0.008)
for i in range(6):
    a = i * math.pi / 3
    ball("Plant_leaf%d" % i, 0.055, WX - 0.25 + math.cos(a) * 0.05, -0.09 + math.sin(a) * 0.04, 1.16 + (i % 2) * 0.04, LEAF, (0.55, 0.9, 1.3))

# Wall clock over the fridge.
cyl("Clock_rim", 0.175, 0.03, 0.375, -0.02, 2.33, DARK, "Y", 48)
cyl("Clock_face", 0.155, 0.034, 0.375, -0.02, 2.33, ENAMEL, "Y", 48)
rbox("Clock_hour", 0.014, 0.006, 0.085, 0.375, -0.041, 2.37, DARK, 0.002)
rbox("Clock_min", 0.11, 0.006, 0.010, 0.425, -0.041, 2.33, DARK, 0.002)

# Utensil rail on the tiles, a towel on the oven, magnets on the fridge, a toaster, a rug.
cyl("Rail", 0.006, 0.50, 2.33, -0.035, 1.34, STEEL, "X", 12)
for i, x_ in enumerate((2.18, 2.33, 2.48)):
    rbox("Utensil%d_handle" % i, 0.014, 0.010, 0.17, x_, -0.04, 1.24, WOOD_D, 0.004)
    ball("Utensil%d_head" % i, 0.035, x_, -0.04, 1.125, STEEL, (1.0, 0.25, 1.3 if i != 1 else 0.9))
rbox("Towel", 0.17, 0.014, 0.30, stx + 0.11, -0.676, 0.55, FABRIC, 0.006, 3)
rbox("Towel_stripe", 0.172, 0.015, 0.03, stx + 0.11, -0.676, 0.45, ENAMEL, 0.003)
for i, (mx, mz, col) in enumerate(((0.46, 1.16, (0.95, 0.8, 0.1)), (0.56, 1.08, (0.1, 0.45, 0.85)), (0.50, 1.02, (0.2, 0.7, 0.3)))):
    cyl("Magnet%d" % i, 0.02, 0.008, mx, -0.699, mz, mat("Magnet%d" % i, col, 0.3), "Y", 20)
rbox("Fridge_note", 0.10, 0.003, 0.12, 0.52, -0.697, 0.92, PAPER, 0.0)
rbox("Toaster", 0.24, 0.14, 0.15, 1.0, -0.26, 0.975, STEEL, 0.035, 6)
for s in (-1, 1):
    rbox("Toaster_slot", 0.16, 0.022, 0.006, 1.0, -0.26 + s * 0.028, 1.051, DARK, 0.002)
rbox("Toaster_lever", 0.022, 0.03, 0.035, 1.128, -0.26, 1.0, DARK, 0.006)
rbox("Rug", 1.0, 0.55, 0.012, sx, -1.10, 0.006, RED, 0.004)
for s in (-1, 0, 1):
    rbox("Rug_stripe", 0.07, 0.552, 0.013, sx + s * 0.32, -1.10, 0.0065, PAPER, 0.002)
rbox("Baseboard", 8.0, 0.02, 0.09, 2.6, -0.012, 0.045, ENAMEL, 0.006)
# ---------------------------------------------------------------- stage for the preview (not part of the set)
stage = bpy.data.collections.new("Stage")
bpy.context.scene.collection.children.link(stage)
def to_stage(o):
    for c in o.users_collection:
        c.objects.unlink(o)
    stage.objects.link(o)
to_stage(rbox("Stage_wall", 8.0, 0.05, 2.9, 2.6, 0.027, 1.45, mat("Wall", (0.96, 0.80, 0.52), 0.9), 0.0))
floor = rbox("Stage_floor", 8.0, 5.0, 0.05, 2.6, -2.45, -0.025, mat("Floor", (0.8, 0.8, 0.8), 0.35), 0.0)
to_stage(floor)
fm = floor.data.materials[0]
nt = fm.node_tree
chk = nt.nodes.new("ShaderNodeTexChecker")
chk.inputs["Scale"].default_value = 14
chk.inputs["Color1"].default_value = (0.93, 0.90, 0.84, 1)
chk.inputs["Color2"].default_value = (0.16, 0.20, 0.24, 1)
tc = nt.nodes.new("ShaderNodeTexCoord")
nt.links.new(tc.outputs["Object"], chk.inputs["Vector"])
nt.links.new(chk.outputs["Color"], nt.nodes["Principled BSDF"].inputs["Base Color"])

scene = bpy.context.scene
world = bpy.data.worlds.new("World")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.85, 0.92, 1.0, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.30
scene.world = world
bpy.ops.object.light_add(type="SUN", location=(0, -4, 5), rotation=(math.radians(52), 0, math.radians(-28)))
sun = bpy.context.object
sun.data.energy = 2.6
sun.data.angle = math.radians(6)
sun.data.color = (1.0, 0.95, 0.86)
bpy.ops.object.light_add(type="AREA", location=(3.6, -2.6, 2.4))
fill = bpy.context.object
fill.data.energy = 90
fill.data.size = 3.0
fill.data.color = (0.8, 0.9, 1.0)
bpy.ops.object.empty_add(location=(2.45, -0.5, 1.10))
target = bpy.context.object
bpy.ops.object.camera_add(location=(0.6, -5.9, 2.0))
cam = bpy.context.object
cam.data.lens = 30
tr = cam.constraints.new("TRACK_TO")
tr.target = target
scene.camera = cam
for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
    try:
        scene.render.engine = eng
        break
    except Exception:
        pass
try:
    scene.eevee.taa_render_samples = 64
except Exception:
    pass
for attr, val in (("use_gtao", True), ("use_raytracing", True), ("use_shadows", True)):
    try:
        setattr(scene.eevee, attr, val)
    except Exception:
        pass
try:
    scene.view_settings.view_transform = "Standard"
except Exception:
    pass
scene.render.resolution_x = 1600
scene.render.resolution_y = 900
scene.render.filepath = os.path.join(OUT, "kitchen_set.png")
bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "kitchen_set.blend"))
print("KITCHEN_DONE objects=%d engine=%s" % (len(bpy.data.objects), scene.render.engine))