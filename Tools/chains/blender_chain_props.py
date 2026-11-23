# Materials and chain reactions 2.0 (docs/MATERIALS.md): the new props, modelled procedurally (Higgsfield had no credits):
# ice cube, the kid's slingshot and its pouch, a roll of mentos and one sweet, the kitchen power strip, the breaker box and
# its main switch. Frame: 1 Blender unit = 100 uu (map v4 "uu" props); slots are map v4 look keys (Map4Look), the code
# assigns the looks. Out: SourceArt/Map4/props/<Name>/ -> Tools/map4/import_m4_props.py (M4_PROPS=SM_R_...).
#   blender --background --factory-startup --python blender_chain_props.py -- <out dir>
import bpy, bmesh, sys, os, math, json
from mathutils import Vector, Matrix
OUT = sys.argv[sys.argv.index("--") + 1]
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "map4", "blender_m4_common.py")).read())

def bevel_box(bm, sx, sy, sz, off=(0, 0, 0), w=0.05, segs=2):
    r = bmesh.ops.create_cube(bm, size=1.0)
    vs = r["verts"]
    for v in vs:
        v.co = Vector((v.co.x * sx + off[0], v.co.y * sy + off[1], v.co.z * sz + off[2]))
    es = list({e for v in vs for e in v.link_edges})
    bmesh.ops.bevel(bm, geom=vs + es, offset=w, segments=segs, affect="EDGES", profile=0.5)
    return vs

def cyl(bm, a, b, r, segs=16, caps=True):
    a, b = Vector(a), Vector(b)
    d = b - a
    q = d.to_track_quat("Z", "Y")
    c = bmesh.ops.create_cone(bm, cap_ends=caps, segments=segs, radius1=r, radius2=r, depth=d.length)
    for v in c["verts"]:
        v.co = q @ v.co + (a + b) * 0.5
    return c["verts"]

def sphere(bm, c, rx, ry, rz, segs=16, rings=10):
    s = bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=rings, radius=1.0)
    for v in s["verts"]:
        v.co = Vector((v.co.x * rx + c[0], v.co.y * ry + c[1], v.co.z * rz + c[2]))
    return s["verts"]

# ---- ice cube: 0.5 (50 uu), soft edges, a little irregular
clear()
bm = bmesh.new()
bevel_box(bm, 0.5, 0.5, 0.46, (0, 0, 0.23), w=0.07, segs=3)
for v in bm.verts:
    v.co.x += 0.012 * math.sin(v.co.y * 31.0 + v.co.z * 17.0)
    v.co.y += 0.012 * math.sin(v.co.z * 23.0 + v.co.x * 13.0)
ob = obj(bm, "SM_R_IceCube"); uv(ob, "cube")
export(ob, "SM_R_IceCube", slots=[("Ice", None)])

# ---- the kid's slingshot: a wooden Y (handle, crotch, two prongs), tips at (0, +-0.92, 2.8); the band is drawn by the code
clear()
bm = bmesh.new()
cyl(bm, (0, 0, 0.0), (0, 0, 1.45), 0.13, 16)
sphere(bm, (0, 0, 1.45), 0.17, 0.2, 0.17)
for s in (-1, 1):
    cyl(bm, (0, 0, 1.45), (0, s * 0.92, 2.8), 0.11, 14)
    sphere(bm, (0, s * 0.92, 2.8), 0.13, 0.13, 0.13, 12, 8)
# a grip wrap
cyl(bm, (0, 0, 0.35), (0, 0, 1.0), 0.145, 16)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.001)
ob = obj(bm, "SM_R_Slingshot"); uv(ob)
export(ob, "SM_R_Slingshot", slots=[("Leather", lambda c, p: 0.34 < c.z < 1.01 and abs(c.y) < 0.2), ("Oak", None)])

clear()
bm = bmesh.new()
sphere(bm, (0, 0, 0), 0.32, 0.36, 0.14, 16, 10)
ob = obj(bm, "SM_R_SlingPouch"); uv(ob)
export(ob, "SM_R_SlingPouch", slots=[("Leather", None)])

# ---- a roll of mentos lying along X, paper with two blue bands; one sweet (a white lens)
clear()
bm = bmesh.new()
cyl(bm, (-0.85, 0, 0.5), (0.85, 0, 0.5), 0.5, 32)
cyl(bm, (-0.86, 0, 0.5), (-0.55, 0, 0.5), 0.505, 32, caps=False)
cyl(bm, (0.55, 0, 0.5), (0.86, 0, 0.5), 0.505, 32, caps=False)
ob = obj(bm, "SM_R_MentosRoll"); uv(ob, "cyl")
export(ob, "SM_R_MentosRoll", slots=[("Blue", lambda c, p: abs(c.x) > 0.54), ("Paper", None)])
clear()
bm = lathe_z([(0.0, 0.0), (0.12, 0.004), (0.19, 0.03), (0.2, 0.06), (0.19, 0.09), (0.12, 0.116), (0.0, 0.12)], segs=32)
ob = obj(bm, "SM_R_Mentos"); uv(ob)
export(ob, "SM_R_Mentos", slots=[("Sugar", None)])

# ---- the power strip: a white block 5.2 long, three sockets on top, a red switch at the end
clear()
bm = bmesh.new()
bevel_box(bm, 5.2, 1.2, 0.7, (0, 0, 0.35), w=0.12, segs=3)
for i in range(3):
    x = -1.6 + i * 1.25
    cyl(bm, (x, 0, 0.66), (x, 0, 0.72), 0.42, 24)
    for s in (-1, 1):
        cyl(bm, (x + s * 0.16, 0, 0.6), (x + s * 0.16, 0, 0.73), 0.06, 8)
bevel_box(bm, 0.5, 0.6, 0.18, (2.2, 0, 0.75), w=0.05)
ob = obj(bm, "SM_R_PowerStrip"); uv(ob)
export(ob, "SM_R_PowerStrip", slots=[("Red", lambda c, p: c.x > 1.9 and c.z > 0.68), ("BlackGlass", lambda c, p: c.z > 0.6 and c.x < 1.9 and abs(c.y) < 0.43), ("FridgeLiner", None)])

# ---- the breaker box: a white steel cabinet on the wall (its back at +X, the front with the breakers at -X), 6 small breakers
clear()
bm = bmesh.new()
bevel_box(bm, 0.8, 4.2, 6.2, (0, 0, 0), w=0.08, segs=2)          # body (centre at the origin)
bevel_box(bm, 0.08, 3.7, 1.0, (-0.44, 0, 1.2), w=0.02)           # the DIN rail cover
for i in range(6):
    y = -1.5 + i * 0.6
    bevel_box(bm, 0.22, 0.32, 0.5, (-0.56, y, 1.2), w=0.03)        # a breaker
bevel_box(bm, 0.05, 3.9, 2.2, (-0.42, 0, -1.4), w=0.02)          # a label plate
ob = obj(bm, "SM_R_BreakerBox"); uv(ob)
export(ob, "SM_R_BreakerBox", slots=[("BlackGlass", lambda c, p: c.x < -0.44 and c.z > 0.9 and c.z < 1.5), ("Paper", lambda c, p: c.x < -0.43 and c.z < -0.2), ("FridgeLiner", None)])
clear()
bm = bmesh.new()
bevel_box(bm, 0.45, 0.35, 1.5, (0, 0, 0.75), w=0.06)
ob = obj(bm, "SM_R_BreakerLever"); uv(ob)
export(ob, "SM_R_BreakerLever", slots=[("Red", None)])
print("CHAINPROPS DONE")
