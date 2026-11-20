# Ant Kitchen: look at the Meshy ant parts before cutting them (stats + orthographic previews).
# Run: blender --background --factory-startup --python ant_look.py -- <SourceArt/Meshy dir> <out dir>
import bpy, sys, os, glob, math, json
from mathutils import Vector
import bmesh

argv = sys.argv[sys.argv.index("--") + 1:]
SRC, OUT = argv[0], argv[1]
os.makedirs(OUT, exist_ok=True)
PARTS = ["SM_Ant_Body", "SM_Ant_Femur", "SM_Ant_Tibia", "SM_Ant_Coxa", "SM_Ant_Antenna_Scape", "SM_Ant_Antenna_Flagellum", "SM_Ant_AcidGland"]
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene

def load(part):
    d = glob.glob(os.path.join(SRC, part, "*"))
    fbx = glob.glob(os.path.join(SRC, part, "*", "*.fbx"))[0]
    before = set(bpy.data.objects)
    try:
        bpy.ops.wm.fbx_import(filepath=fbx)
    except Exception:
        bpy.ops.import_scene.fbx(filepath=fbx)
    obs = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in obs if o.type == "MESH"]
    return fbx, obs, meshes

world = bpy.data.worlds.new("World")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (1, 1, 1, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 1.2
scene.world = world
camd = bpy.data.cameras.new("Cam")
camd.type = "ORTHO"
cam = bpy.data.objects.new("Cam", camd)
scene.collection.objects.link(cam)
scene.camera = cam
for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
    try:
        scene.render.engine = eng
        break
    except Exception:
        pass
try:
    scene.view_settings.view_transform = "Standard"
except Exception:
    pass
sun = bpy.data.lights.new("Sun", "SUN")
sun.energy = 2.5
so = bpy.data.objects.new("Sun", sun)
so.rotation_euler = (math.radians(40), 0, math.radians(30))
scene.collection.objects.link(so)
scene.render.resolution_x = 900
scene.render.resolution_y = 600
scene.render.film_transparent = False

def shot(name, center, size, direction, up_axis="Z"):
    d = Vector(direction).normalized()
    cam.location = center + d * size * 4.0
    q = (-d).to_track_quat("-Z", "Y")
    cam.rotation_euler = q.to_euler()
    camd.ortho_scale = size * 1.15
    camd.clip_start = 0.001
    camd.clip_end = size * 20
    scene.render.filepath = os.path.join(OUT, name + ".png")
    bpy.ops.render.render(write_still=True)
    print("RENDERED", name)

info = {}
for part in PARTS:
    fbx, obs, meshes = load(part)
    rec = {"fbx": os.path.basename(fbx), "objects": [(o.name, o.type) for o in obs]}
    for o in meshes:
        me = o.data
        mw = o.matrix_world
        vs = [mw @ v.co for v in me.vertices]
        mn = Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs)))
        mx = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
        bm = bmesh.new()
        bm.from_mesh(me)
        # islands
        seen = set()
        isl = []
        for v in bm.verts:
            if v.index in seen:
                continue
            stack = [v]
            seen.add(v.index)
            n = 0
            while stack:
                a = stack.pop()
                n += 1
                for e in a.link_edges:
                    b = e.other_vert(a)
                    if b.index not in seen:
                        seen.add(b.index)
                        stack.append(b)
            isl.append(n)
        open_edges = sum(1 for e in bm.edges if len(e.link_faces) != 2)
        bm.free()
        rec.update({"verts": len(me.vertices), "polys": len(me.polygons), "min": list(mn), "max": list(mx),
                    "scale": list(o.scale), "rot": list(o.rotation_euler), "islands": sorted(isl, reverse=True)[:8], "n_islands": len(isl),
                    "open_edges": open_edges, "uv_layers": [u.name for u in me.uv_layers],
                    "materials": [s.material.name if s.material else None for s in o.material_slots]})
        c = (mn + mx) * 0.5
        size = max(mx - mn)
        print("PART", part, json.dumps(rec))
        if part == "SM_Ant_Body":
            # profile: cross-section extent along the long axis (40 slices)
            ext = mx - mn
            ax = 0 if ext.x >= ext.y and ext.x >= ext.z else (1 if ext.y >= ext.z else 2)
            N = 60
            prof = []
            for i in range(N):
                a = mn[ax] + ext[ax] * i / N
                b = mn[ax] + ext[ax] * (i + 1) / N
                sl = [v for v in vs if a <= v[ax] < b]
                if sl:
                    o1, o2 = [k for k in range(3) if k != ax]
                    prof.append((round((a + b) / 2, 4), round(min(v[o1] for v in sl), 4), round(max(v[o1] for v in sl), 4), round(min(v[o2] for v in sl), 4), round(max(v[o2] for v in sl), 4), len(sl)))
                else:
                    prof.append((round((a + b) / 2, 4), 0, 0, 0, 0, 0))
            print("PROFILE axis", ax, json.dumps(prof))
        for nm, d in (("side", (0, -1, 0)), ("side2", (1, 0, 0)), ("top", (0, 0, 1)), ("q", (1, -1, 0.6))):
            shot(part + "_" + nm, c, size, d)
    info[part] = rec
    for o in obs:
        bpy.data.objects.remove(o)
json.dump(info, open(os.path.join(OUT, "look.json"), "w"), indent=1)
print("LOOK_DONE")
