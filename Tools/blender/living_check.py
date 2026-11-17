# Ant Kitchen: re-imports every FBX listed in living_manifest.json and compares it with the manifest
# (bounding box, triangles, material slots, closed mesh).
# Run:  blender --background --factory-startup --python living_check.py -- <living_dir>
import bpy, bmesh, sys, os, json
from mathutils import Vector

d = sys.argv[sys.argv.index("--") + 1]
man = json.load(open(os.path.join(d, "living_manifest.json"), encoding="utf-8"))
bad = 0
total = 0
for p in man["pieces"]:
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o)
    path = os.path.join(d, p["file"])
    if not os.path.exists(path):
        print("MISSING FILE", p["file"])
        bad += 1
        continue
    try:
        bpy.ops.wm.fbx_import(filepath=path)
    except Exception:
        bpy.ops.import_scene.fbx(filepath=path)
    obs = [o for o in bpy.data.objects if o.type == "MESH"]
    msgs = []
    if len(obs) != 1:
        msgs.append("objects=%d" % len(obs))
    if obs:
        o = obs[0]
        bpy.context.view_layer.update()
        cs = [o.matrix_world @ v.co for v in o.data.vertices]
        mn = Vector((min(c.x for c in cs), min(c.y for c in cs), min(c.z for c in cs)))
        mx = Vector((max(c.x for c in cs), max(c.y for c in cs), max(c.z for c in cs)))
        for i in range(3):
            if abs(mn[i] - p["bbox_min_m"][i]) > 0.002 or abs(mx[i] - p["bbox_max_m"][i]) > 0.002:
                msgs.append("bbox axis %d: %.4f..%.4f, manifest %.4f..%.4f" % (i, mn[i], mx[i], p["bbox_min_m"][i], p["bbox_max_m"][i]))
        tris = sum(len(f.vertices) - 2 for f in o.data.polygons)
        total += tris
        if tris != p["triangles"]:
            msgs.append("tris %d, manifest %d" % (tris, p["triangles"]))
        names = [m.name.split(".")[0] if m else "?" for m in o.data.materials]
        if names != p["materials"]:
            msgs.append("materials %s, manifest %s" % (names, p["materials"]))
        bm = bmesh.new()
        bm.from_mesh(o.data)
        op = sum(1 for e in bm.edges if len(e.link_faces) != 2)
        vol = bm.calc_volume(signed=True)
        bm.free()
        if op:
            msgs.append("open_edges=%d" % op)
        if vol <= 0:
            msgs.append("volume=%.6f" % vol)
        if o.name.split(".")[0] != p["name"]:
            msgs.append("object name %s" % o.name)
    if msgs:
        bad += 1
        print("CHECK FAIL", p["name"], "; ".join(msgs))
print("LIVING_CHECK pieces=%d bad=%d triangles=%d" % (len(man["pieces"]), bad, total))
