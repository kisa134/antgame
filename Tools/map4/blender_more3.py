# Map v4: the residents' hand props as models in the frames of the shapes they replace (HumanBody: unit cube, cylinder
# along Z), so the code swaps the mesh in place: fly swatter head, broom head, dustpan, phone, watering can, crayon,
# folded towel, ladle.   blender --background --factory-startup --python blender_more3.py -- <out dir>
import bpy, bmesh, sys, os, math, random, json
from mathutils import Vector, Matrix, noise
argv = sys.argv[sys.argv.index("--") + 1:]
OUT = argv[0]
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "blender_m4_common.py")).read())

def box(bm, c, s):
    r = bmesh.ops.create_cube(bm, size=1.0)
    for v in r["verts"]:
        v.co = Vector((c[0] + v.co.x * s[0], c[1] + v.co.y * s[1], c[2] + v.co.z * s[2]))
    return r
def cyl(bm, c, r, h, segs=16, axis="Z", r2=None):
    g = bmesh.ops.create_cone(bm, cap_ends=True, segments=segs, radius1=r, radius2=r if r2 is None else r2, depth=h)
    for v in g["verts"]:
        p = v.co.copy()
        if axis == "X": p = Vector((p.z, p.y, -p.x))
        elif axis == "Y": p = Vector((p.x, p.z, -p.y))
        v.co = p + Vector(c)
    return g
def inside(vs, lo, hi):
    return all(lo[i] <= vs[i] <= hi[i] for i in range(3))

# ---- fly swatter head: a lattice in the XZ plane (thin along Y), a rim; cube frame
clear(); bm = bmesh.new()
n = 9
for i in range(n + 1):
    t = -0.5 + i / n
    box(bm, (t, 0, 0), (0.035, 0.6, 1.0))      # bars along Z
    box(bm, (0, 0, t), (1.0, 0.6, 0.035))      # bars along X
for (c, s) in (((0, 0, 0.47), (1, 1, 0.06)), ((0, 0, -0.47), (1, 1, 0.06)), ((0.47, 0, 0), (0.06, 1, 1)), ((-0.47, 0, 0), (0.06, 1, 1))):
    box(bm, c, s)
ob = obj(bm, "SM_M4_SwatterHead"); uv(ob, "cube"); export(ob, "SM_M4_SwatterHead", slots=[("Red", None)], smooth=False)

# ---- broom head: straws along X from the binding (-X, at the handle) to a splayed tip (+X); cube frame
clear(); bm = bmesh.new(); random.seed(5)
for k in range(140):
    z0 = random.uniform(-0.22, 0.22); y0 = random.uniform(-0.25, 0.25)
    z1 = z0 * 2.1 + random.uniform(-0.04, 0.04); y1 = y0 * 1.6 + random.uniform(-0.05, 0.05)
    x1 = 0.5 - abs(z1) * 0.15 + random.uniform(-0.04, 0.0)
    a = Vector((-0.3, y0, z0)); b = Vector((x1, max(-0.5, min(0.5, y1)), max(-0.5, min(0.5, z1))))
    side = (b - a).cross(Vector((0, 1, 0))).normalized() * 0.012
    vs = [bm.verts.new(p) for p in (a - side, a + side, b + side * 0.5, b - side * 0.5)]
    f = bm.faces.new(vs); double_side(bm, [f])
straw_faces = len(bm.faces)
g = cyl(bm, (-0.38, 0, 0), 0.26, 0.24, 20, "X")
for t in (-0.45, -0.3):
    cyl(bm, (t, 0, 0), 0.275, 0.03, 20, "X")
ob = obj(bm, "SM_M4_BroomHead"); uv(ob)
export(ob, "SM_M4_BroomHead", slots=[("Red", lambda c, p: abs(c.x + 0.45) < 0.03 or abs(c.x + 0.3) < 0.03), ("Sisal", None)], extra={"two_sided": True})

# ---- dustpan: a flat pan with walls at the back and sides (+Z up), a short handle at the back; cube frame
clear(); bm = bmesh.new()
box(bm, (0.05, 0, -0.4), (0.9, 1.0, 0.2))              # floor of the pan
box(bm, (-0.38, 0, 0.05), (0.08, 1.0, 0.9))            # back wall
box(bm, (0.05, 0.47, 0.0), (0.9, 0.06, 0.8))           # sides
box(bm, (0.05, -0.47, 0.0), (0.9, 0.06, 0.8))
box(bm, (-0.46, 0, 0.3), (0.12, 0.16, 0.4))            # handle stub
ob = obj(bm, "SM_M4_Dustpan"); uv(ob, "cube"); export(ob, "SM_M4_Dustpan", slots=[("Red", None)], smooth=False)

# ---- mobile phone: a rounded slab, the screen on +Z; cube frame
clear(); bm = bmesh.new()
r = box(bm, (0, 0, 0), (1, 1, 1))
bmesh.ops.bevel(bm, geom=[e for e in bm.edges], offset=0.08, segments=3, affect="EDGES")
box(bm, (0.0, 0, 0.5), (0.86, 0.84, 0.04))
ob = obj(bm, "SM_M4_Phone"); uv(ob, "cube")
export(ob, "SM_M4_Phone", slots=[("Screen", lambda c, p: c.z > 0.49 and p.normal.z > 0.5), ("Plinth", None)], smooth=False)

# ---- watering can: body along Z at the back, spout to +X rising, a handle arc on the back top; cube frame
clear(); bm = bmesh.new()
cyl(bm, (-0.18, 0, -0.12), 0.3, 0.72, 28)
cyl(bm, (-0.18, 0, 0.26), 0.12, 0.08, 20)                       # filler neck
sp = cyl(bm, (0.0, 0, 0.0), 0.06, 0.62, 12, r2=0.035)          # spout, then tilted up towards +X
for v in sp["verts"]:
    v.co = Matrix.Rotation(math.radians(-58), 3, "Y") @ v.co + Vector((0.2, 0, -0.02))
cyl(bm, (0.44, 0, 0.27), 0.07, 0.06, 14, "X")                  # the rose
for k in range(9):
    a = math.pi * k / 8
    cyl(bm, (-0.18 - 0.28 * math.cos(a), 0, 0.26 + 0.2 * math.sin(a)), 0.035, 0.12, 8, "Y")
ob = obj(bm, "SM_M4_WateringCan"); uv(ob)
export(ob, "SM_M4_WateringCan", slots=[("Steel", lambda c, p: c.x > 0.38), ("BinGreen", None)])

# ---- wax crayon: cylinder frame along Z, a cone tip at +Z, a paper wrapper
clear(); bm = bmesh.new()
cyl(bm, (0, 0, -0.1), 0.45, 0.8, 16)
cyl(bm, (0, 0, 0.4), 0.45, 0.2, 16, r2=0.12)
cyl(bm, (0, 0, -0.12), 0.5, 0.6, 16)
ob = obj(bm, "SM_M4_Crayon"); uv(ob, "cyl")
export(ob, "SM_M4_Crayon", slots=[("Paper", lambda c, p: abs(c.z + 0.12) < 0.31 and math.hypot(c.x, c.y) > 0.47), ("Red", None)])

# ---- folded towel: a soft slab with a rolled fold along one edge; cube frame
clear(); bm = bmesh.new()
bmesh.ops.create_cube(bm, size=1.0)
bmesh.ops.bevel(bm, geom=list(bm.edges), offset=0.2, segments=4, affect="EDGES")
bmesh.ops.subdivide_edges(bm, edges=list(bm.edges), cuts=2, use_grid_fill=True)
for v in bm.verts:
    v.co.z *= 0.85 + 0.15 * math.cos(v.co.x * 9.0) * (v.co.z > 0)
    v.co += v.normal * noise.noise(v.co * 6.0) * 0.02
ob = obj(bm, "SM_M4_Towel"); uv(ob, "cube"); export(ob, "SM_M4_Towel", slots=[("SockWool", None)])

# ---- ladle: cylinder frame along Z (hangs from the fist: grip at +Z, the bowl at -Z); the code gives it a wider frame
clear(); bm = bmesh.new()
cyl(bm, (0, 0, 0.12), 0.06, 0.76, 10)
h = bmesh.ops.create_uvsphere(bm, u_segments=20, v_segments=10, radius=0.5)
for v in h["verts"]:
    v.co.z = min(v.co.z, 0.0) * 0.55 - 0.22
    v.co.x -= 0.0
cyl(bm, (0, 0, 0.47), 0.09, 0.06, 10)
ob = obj(bm, "SM_M4_Ladle"); uv(ob); export(ob, "SM_M4_Ladle", slots=[("Steel", None)])
print("M4MORE3 DONE")
