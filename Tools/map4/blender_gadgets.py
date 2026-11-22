# Map v4: the gadgets' own models (fan, bottle, drain, pipe, socket, soup bits, robot vacuum, pizza box, gem, button).
# Frames: "unit" meshes replace an engine shape in place (sphere/cylinder/cube of 1 m = 100 uu); "uu" meshes are built
# in the actor's own frame at 1 m = 100 uu (the code places them with scale 1 at the old pivot).
#   blender --background --factory-startup --python blender_gadgets.py -- <out dir>
import bpy, bmesh, sys, os, math, random, json
import numpy as np
from mathutils import Vector, Matrix, noise
OUT = sys.argv[sys.argv.index("--") + 1]
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "blender_m4_common.py")).read())

# ---------------- desk fan (uu frame: Root at the base centre, Head pivot at z 480; the fan blows along -Y)
clear()
bm = lathe_z([(0.0, 0.0), (1.9, 0.0), (1.9, 0.15), (1.75, 0.35), (1.2, 0.45), (0.35, 0.5), (0.0, 0.5)], segs=48)
btn = bmesh.ops.create_cube(bm, size=1.0)
for v in btn["verts"]: v.co = Vector((v.co.x * 1.1, v.co.y * 0.7 - 1.35, v.co.z * 0.3 + 0.5))
ob = obj(bm, "SM_M4_FanBase"); uv(ob)
export(ob, "SM_M4_FanBase", slots=[("Red", lambda c, p: c.z > 0.45 and c.y < -1.0 and abs(c.x) < 0.7), ("BlackGlass", None)])
clear()
bm = lathe_z([(0.0, 0.0), (0.28, 0.0), (0.25, 0.4), (0.22, 4.3), (0.3, 4.4), (0.3, 4.8), (0.0, 4.8)], segs=24)
ob = obj(bm, "SM_M4_FanPole"); uv(ob); export(ob, "SM_M4_FanPole", slots=[("Chrome", None)])
clear()
bm = lathe_z([(0.0, -1.3), (0.55, -1.25), (0.8, -0.9), (0.85, -0.2), (0.75, 0.5), (0.45, 0.85), (0.0, 0.9)], segs=32)
for v in bm.verts: v.co = Vector((v.co.x, -v.co.z + 0.4, v.co.y))          # axis along Y (front towards -Y)
ob = obj(bm, "SM_M4_FanMotor"); uv(ob); export(ob, "SM_M4_FanMotor", slots=[("BlackGlass", None)])
clear()
bm = bmesh.new()
def ring(r, y, t=0.06, segs=48):
    tor = bmesh.ops.create_circle(bm, cap_ends=False, segments=segs, radius=r)
    pts = [v for v in tor["verts"]]
    for v in pts: v.co = Vector((v.co.x, y, v.co.y))
    return pts
def tube(a, b, r=0.035):
    d = b - a
    q = d.to_track_quat("Z", "Y")
    c = bmesh.ops.create_cone(bm, cap_ends=False, segments=6, radius1=r, radius2=r, depth=d.length)
    for v in c["verts"]: v.co = q @ v.co + (a + b) * 0.5
for r, y in ((3.1, -0.8), (2.0, -0.85), (1.0, -0.88), (3.1, 0.6)):
    n = 48
    for i in range(n):
        a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
        tube(Vector((math.cos(a0) * r, y, math.sin(a0) * r)), Vector((math.cos(a1) * r, y, math.sin(a1) * r)), 0.05 if r > 3 else 0.035)
for k in range(16):
    a = 2 * math.pi * k / 16
    d = Vector((math.cos(a), 0, math.sin(a)))
    tube(Vector((0, -0.9, 0)) + d * 0.3, Vector((0, -0.8, 0)) + d * 3.1)
    tube(Vector((0, 0.6, 0)) + d * 0.6, Vector((0, 0.6, 0)) + d * 3.1)
    tube(Vector((0, -0.8, 0)) + d * 3.1, Vector((0, 0.6, 0)) + d * 3.1)
hub = bmesh.ops.create_cone(bm, cap_ends=True, segments=24, radius1=0.45, radius2=0.25, depth=0.12)
for v in hub["verts"]: v.co = Vector((v.co.x, -0.92 - v.co.z, v.co.y))
ob = obj(bm, "SM_M4_FanCage"); uv(ob); export(ob, "SM_M4_FanCage", slots=[("Chrome", None)])
clear()
bm = bmesh.new()
for k in range(3):
    a0 = 2 * math.pi * k / 3
    rows = []
    for j in range(9):
        t = j / 8.0
        r = 0.35 + t * 2.25
        w = 0.35 + 0.75 * math.sin(math.pi * min(1.0, t * 1.15)) 
        row = []
        for s in (-1, 1):
            a = a0 + s * w / max(r, 0.5) * 0.5
            twist = 0.18 * s * (1.0 - t * 0.5)
            row.append(bm.verts.new((math.cos(a) * r, twist, math.sin(a) * r)))
        rows.append(row)
    for j in range(8):
        f = bm.faces.new((rows[j][0], rows[j][1], rows[j + 1][1], rows[j + 1][0]))
hub = bmesh.ops.create_cone(bm, cap_ends=True, segments=24, radius1=0.42, radius2=0.42, depth=0.3)
for v in hub["verts"]: v.co = Vector((v.co.x, v.co.z, v.co.y))
double_side(bm, [f for f in bm.faces if len(f.verts) == 4 and f.calc_area() > 0.001 and abs(f.normal.y) > 0.5])
ob = obj(bm, "SM_M4_FanBlades"); uv(ob); export(ob, "SM_M4_FanBlades", slots=[("PlasticBlue", None)])

# ---------------- PET bottle (unit-free: height 1, diameter 0.36 at the body; the code scales to Height / Radius)
clear()
prof = [(0.0, 0.0), (0.16, 0.0), (0.18, 0.02), (0.18, 0.62), (0.17, 0.67), (0.12, 0.76), (0.075, 0.82), (0.07, 0.93), (0.08, 0.935), (0.08, 0.95), (0.0825, 0.95), (0.0825, 1.0), (0.0, 1.0)]
bm = lathe_z(prof, segs=40)
for v in bm.verts:   # petaloid foot
    if v.co.z < 0.03 and v.co.xy.length > 0.1:
        a = math.atan2(v.co.y, v.co.x); v.co.z += 0.012 * (1 + math.cos(a * 5))
ob = obj(bm, "SM_M4_PetBottle"); uv(ob, "cyl")
export(ob, "SM_M4_PetBottle", slots=[("Red", lambda c, p: c.z > 0.94), ("Paper", lambda c, p: 0.24 < c.z < 0.5 and Vector((c.x, c.y)).length > 0.17), ("PetClear", None)],
       ucx=[Vector((math.cos(2 * math.pi * i / 12) * 0.18, math.sin(2 * math.pi * i / 12) * 0.18, z)) for i in range(12) for z in (0.0, 0.66)] + [Vector((0.08, 0, 1.0)), Vector((-0.08, 0, 1.0)), Vector((0, 0.08, 1.0)), Vector((0, -0.08, 1.0))])

# ---------------- drain (unit cylinder frame: d 1, h 1 -> squashed), pipe end, socket, button
clear()
bm = lathe_z([(0.0, 0.3), (0.38, 0.3), (0.5, 0.45), (0.5, 0.5), (0.42, 0.5), (0.36, 0.42), (0.0, 0.42)], segs=40)
for k in range(4):
    a = k * math.pi / 4
    c = bmesh.ops.create_cube(bm, size=1.0)
    for v in c["verts"]: v.co = Matrix.Rotation(a, 3, "Z") @ Vector((v.co.x * 0.72, v.co.y * 0.06, v.co.z * 0.04 + 0.44))
for v in bm.verts: v.co.z -= 0.5
ob = obj(bm, "SM_M4_Drain"); uv(ob); export(ob, "SM_M4_Drain", slots=[("Chrome", None)])
clear()
bm = lathe_z([(0.0, 0.45), (0.4, 0.45), (0.4, -0.5), (0.5, -0.5), (0.5, 0.45), (0.48, 0.5), (0.0, 0.5)], segs=32)
ob = obj(bm, "SM_M4_PipeEnd"); uv(ob); export(ob, "SM_M4_PipeEnd", slots=[("BlackGlass", lambda c, p: c.z > 0.44 and Vector((c.x, c.y)).length < 0.39), ("Enamel", None)])
clear()
bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
bmesh.ops.bevel(bm, geom=list(bm.edges), offset=0.06, segments=3, affect="EDGES")
for v in bm.verts: v.co.y *= 1.0
for dz in (-0.18, 0.18):
    h = bmesh.ops.create_cone(bm, cap_ends=True, segments=12, radius1=0.07, radius2=0.07, depth=0.2)
    for v in h["verts"]: v.co = Vector((v.co.x + dz * 0.0, -0.55 + v.co.z * 0.2, v.co.y + dz))
ob = obj(bm, "SM_M4_Socket"); uv(ob)
export(ob, "SM_M4_Socket", slots=[("BlackGlass", lambda c, p: c.y < -0.5 and Vector((c.x, c.z)).length < 0.3 and abs(c.x) < 0.09), ("Enamel", None)])
clear()
bm = lathe_z([(0.0, -0.5), (0.5, -0.5), (0.5, 0.1), (0.42, 0.4), (0.25, 0.5), (0.0, 0.5)], segs=32)
ob = obj(bm, "SM_M4_Button"); uv(ob); export(ob, "SM_M4_Button")

# ---------------- soup bits
clear()
bm = lathe_z([(0.0, 0.0), (0.46, 0.0), (0.5, 0.35), (0.5, 0.5), (0.0, 0.5)], segs=48)
for v in bm.verts:
    if v.co.z > 0.45: v.co.z += 0.04 * noise.noise(Vector((v.co.x * 4, v.co.y * 4, 0)))
for v in bm.verts: v.co.z -= 0.25
ob = obj(bm, "SM_M4_LiquidDisc"); uv(ob); export(ob, "SM_M4_LiquidDisc")
clear()
prof = []
bm = bmesh.new()
N = 36
top, bot = [], []
for i in range(N):
    a = 2 * math.pi * i / N
    r = 0.5 * (0.93 + 0.07 * math.cos(a * 9))
    top.append(bm.verts.new((math.cos(a) * r, math.sin(a) * r, 0.5)))
    bot.append(bm.verts.new((math.cos(a) * r, math.sin(a) * r, -0.5)))
bm.faces.new(top); bm.faces.new(bot[::-1])
for i in range(N):
    bm.faces.new((bot[i], bot[(i + 1) % N], top[(i + 1) % N], top[i]))
ob = obj(bm, "SM_M4_CarrotSlice"); uv(ob); export(ob, "SM_M4_CarrotSlice")
clear()
bm = bmesh.new()
rows = []
for j in range(11):
    t = j / 10.0
    x = -0.5 + t
    w = 0.5 * math.sin(math.pi * t) ** 0.8
    rows.append([bm.verts.new((x, -w, 0.12 * math.sin(math.pi * t) - 0.06 * w)), bm.verts.new((x, 0, 0.12 * math.sin(math.pi * t) + 0.08)), bm.verts.new((x, w, 0.12 * math.sin(math.pi * t) - 0.06 * w))])
for j in range(10):
    bm.faces.new((rows[j][0], rows[j][1], rows[j + 1][1], rows[j + 1][0])); bm.faces.new((rows[j][1], rows[j][2], rows[j + 1][2], rows[j + 1][1]))
double_side(bm)
ob = obj(bm, "SM_M4_BayLeaf"); uv(ob); export(ob, "SM_M4_BayLeaf")
clear()
bm = lathe_z([(0.0, -0.5), (0.18, -0.5), (0.22, -0.46), (0.2, 0.3), (0.12, 0.3), (0.0, 0.3)], segs=16)
bowl = bmesh.ops.create_uvsphere(bm, u_segments=20, v_segments=10, radius=0.5)
for v in bowl["verts"]:
    v.co = Vector((v.co.x * 0.75, v.co.y * 0.42, v.co.z * 0.32 + 0.42))
ob = obj(bm, "SM_M4_WoodSpoon"); uv(ob); export(ob, "SM_M4_WoodSpoon", slots=[("Butcher", None)])

# ---------------- robot vacuum (unit cylinder frame) and the gem (sphere frame)
clear()
bm = lathe_z([(0.0, -0.5), (0.47, -0.5), (0.5, -0.4), (0.5, 0.3), (0.48, 0.45), (0.42, 0.5), (0.0, 0.5)], segs=64)
for k in range(2):
    c = bmesh.ops.create_cone(bm, cap_ends=True, segments=16, radius1=0.12, radius2=0.12, depth=0.02)
    for v in c["verts"]: v.co = Vector((v.co.x - 0.15 + k * 0.3, v.co.y + 0.15, v.co.z + 0.5))
ob = obj(bm, "SM_M4_RobotVacuum"); uv(ob)
export(ob, "SM_M4_RobotVacuum", slots=[("Chrome", lambda c, p: c.z > 0.49 and Vector((c.x, c.y)).length < 0.3), ("BlackGlass", lambda c, p: c.z > 0.4), ("Plinth", None)],
       ucx=[Vector((math.cos(2 * math.pi * i / 16) * 0.5, math.sin(2 * math.pi * i / 16) * 0.5, z)) for i in range(16) for z in (-0.5, 0.5)])
clear()
bm = bmesh.new()
top = [bm.verts.new((math.cos(2 * math.pi * i / 8) * 0.3, math.sin(2 * math.pi * i / 8) * 0.3, 0.28)) for i in range(8)]
mid = [bm.verts.new((math.cos(2 * math.pi * (i + 0.5) / 8) * 0.5, math.sin(2 * math.pi * (i + 0.5) / 8) * 0.5, 0.12)) for i in range(8)]
tip = bm.verts.new((0, 0, -0.5))
bm.faces.new(top)
for i in range(8):
    bm.faces.new((top[i], mid[i], top[(i + 1) % 8])); bm.faces.new((mid[i], mid[(i + 1) % 8], top[(i + 1) % 8]))
    bm.faces.new((mid[(i + 1) % 8], mid[i], tip))
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
ob = obj(bm, "SM_M4_Gem"); export(ob, "SM_M4_Gem", smooth=False)
print("M4GADGETS DONE")
