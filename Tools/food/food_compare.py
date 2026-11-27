# KAN-36: "was | now" sheets of the hero food, from what the game gets (the exported FBX and the baked textures), not from the
# Blender materials. Left: a kept copy of SourceArt/FoodHero/<Id> from before the change; right: the folder as it is now.
# Run: blender --background --factory-startup --python Tools/food/food_compare.py -- <project> <before root> <out dir> <Id,Id> [samples]
import bpy, sys, os, math, json
from mathutils import Vector, Matrix
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
argv = sys.argv[sys.argv.index("--") + 1:]
PROJ, BEFORE, OUT, IDS = argv[0], argv[1], argv[2], [x for x in argv[3].replace("+", ",").split(",") if x]
SAMPLES = int(argv[4]) if len(argv) > 4 else 96
sys.argv = sys.argv[:sys.argv.index("--") + 1] + [PROJ, "none", "none"]
import food_v2 as FV
from food_v2 import NT
AFTER = os.path.join(PROJ, "SourceArt", "FoodHero")
WORK = os.path.join(PROJ, "Saved", "food_fix_work")
os.makedirs(OUT, exist_ok=True)
def log(*a): print("FOODCMP", *a, flush=True)

def game_mat(d, fid, man):
    """M_FoodV2 (or M_FoodV2_Glass) with the food's textures, as near as Cycles gets"""
    m = bpy.data.materials.new("Game_" + fid + ("_" + os.path.basename(os.path.dirname(d)))); m.use_nodes = True
    p = m.node_tree.nodes["Principled BSDF"]; t = NT(m.node_tree)
    def tex(k, data):
        n = t.n("ShaderNodeTexImage"); n.image = bpy.data.images.load(os.path.join(d, "T_FoodHero_%s_%s.png" % (fid, k)), check_existing=False)
        if data: n.image.colorspace_settings.name = "Non-Color"
        return n
    c = tex("C", False); r = tex("R", True); n = tex("N", True)
    sep = t.n("ShaderNodeSeparateColor"); t.set(sep.inputs[0], r.outputs["Color"])
    t.set(p.inputs["Base Color"], t.mixc(0.45, c.outputs["Color"], t.mixc(1.0, c.outputs["Color"], sep.outputs[0], "MULTIPLY")))
    t.set(p.inputs["Roughness"], sep.outputs[1])
    nm = t.n("ShaderNodeNormalMap"); t.set(nm.inputs["Color"], n.outputs["Color"]); t.set(p.inputs["Normal"], nm.outputs["Normal"])
    j = float(man.get("jelly", 0.0))
    if man.get("glass"):
        t.set(p.inputs["Transmission Weight"], t.sub(1.0, sep.outputs[2], True)); p.inputs["IOR"].default_value = 1.31
        p.inputs["Coat Weight"].default_value = 0.3
    elif j > 0:
        p.inputs["Subsurface Weight"].default_value = j; p.inputs["Subsurface Scale"].default_value = 0.004; p.inputs["Coat Weight"].default_value = 0.4
    return m

def load(root, fid):
    d = os.path.join(root, fid)
    with open(os.path.join(d, "food.json"), encoding="utf-8") as f: man = json.load(f)
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=os.path.join(d, "SM_FoodHero_%s.fbx" % fid))
    new = [o for o in bpy.data.objects if o not in before]
    keep = [o for o in new if o.type == "MESH" and not o.name.startswith("UCX")]
    for o in new:
        if o not in keep: bpy.data.objects.remove(o, do_unlink=True)
    o = keep[0]
    FV.active(o); bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    mn, mx = FV.bounds([o]); k = max(man["size_m"]) / max(1e-9, max(mx - mn))
    if abs(k - 1.0) > 0.02: o.data.transform(Matrix.Diagonal((k, k, k, 1.0)))
    mn, mx = FV.bounds([o]); c = (mn + mx) * 0.5
    o.data.transform(Matrix.Translation((-c.x, -c.y, -mn.z))); o.data.update()
    for p in o.data.polygons: p.use_smooth = True
    FV.assign(o, game_mat(d, fid, man))
    return o, man

def inner_mat(d, fid, man, pc):
    """M_FoodV2_Inner on a piece: the inner strip read as the game reads it (make_food_materials.py INNER_UV; the piece's origin is
    its centre) - U along X (+ 0.7 Y, or centred on the whole food with "inner_align"), V the height (or the radius)"""
    m = bpy.data.materials.new("Inner_%s_%s" % (fid, pc["name"])); m.use_nodes = True
    p = m.node_tree.nodes["Principled BSDF"]; t = NT(m.node_tree)
    f = os.path.join(d, "T_FoodHero_%s_InC.png" % fid)
    if not os.path.exists(f):
        p.inputs["Base Color"].default_value = (0.8, 0.8, 0.8, 1); return m
    x, y, z = t.sep(t.n("ShaderNodeTexCoord").outputs["Object"])
    hr = man.get("h_range_m", [-0.02, 0.02]); dz, dx = pc["centre_m"][2], pc["centre_m"][0]
    if man.get("inner_align"):
        u = t.add(t.math("DIVIDE", x, max(1e-6, man["size_m"][0])), 0.5 + dx / max(1e-6, man["size_m"][0]))
    else:
        u = t.math("DIVIDE", t.add(x, t.mul(y, 0.7)), max(1e-6, max(man["size_m"][0], man["size_m"][1])))
    if man.get("inner_axis") == "r":
        hh = t.math("DIVIDE", t.math("SQRT", t.add(t.mul(x, x), t.mul(y, y))), max(1e-6, hr[1]))
    else:
        hh = t.math("DIVIDE", t.sub(z, hr[0] - dz), max(1e-6, hr[1] - hr[0]))
    uv = t.n("ShaderNodeCombineXYZ"); t.set(uv.inputs[0], u); t.set(uv.inputs[1], t.math("MINIMUM", t.math("MAXIMUM", hh, 0.0), 1.0))
    tx = t.n("ShaderNodeTexImage"); tx.image = bpy.data.images.load(f, check_existing=False); t.set(tx.inputs["Vector"], uv.outputs[0])
    t.set(p.inputs["Base Color"], tx.outputs["Color"]); p.inputs["Roughness"].default_value = 0.8
    return m

def load_pieces(root, fid, outer, man, view):
    """the pre-cut pieces as the game shows them, each turned so that its cut faces the camera"""
    d = os.path.join(root, fid); out = []
    for pc in man.get("pieces", []):
        f = os.path.join(d, pc["name"] + ".fbx")
        if not os.path.exists(f): continue
        before = set(bpy.data.objects); bpy.ops.import_scene.fbx(filepath=f)
        new = [o for o in bpy.data.objects if o not in before]; keep = [o for o in new if o.type == "MESH"]
        for o in new:
            if o not in keep: bpy.data.objects.remove(o, do_unlink=True)
        if not keep: continue
        o = keep[0]; FV.active(o); bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        mn, mx = FV.bounds([o]); k = max(pc["size_m"]) / max(1e-9, max(mx - mn))
        if abs(k - 1.0) > 0.02: o.data.transform(Matrix.Diagonal((k, k, k, 1.0)))
        for p in o.data.polygons: p.use_smooth = True
        im = inner_mat(d, fid, man, pc); inner_ids = []
        for i, s in enumerate(o.material_slots):
            if s.material and s.material.name.startswith("Inner"): s.material = im; inner_ids.append(i)
            else: s.material = outer
        nsum = Vector()
        for p in o.data.polygons:
            if p.material_index in inner_ids: nsum += p.normal * p.area
        if nsum.length > 1e-9:
            o.rotation_mode = "QUATERNION"; o.rotation_quaternion = nsum.normalized().rotation_difference(view)
        out.append(o)
    bpy.context.view_layer.update()
    return out

def row(objs, cx, gap):
    """the objects side by side on the floor, the row centred on cx"""
    bpy.context.view_layer.update(); x = 0.0
    for o in objs:
        mn, mx = FV.bounds([o]); o.location += Vector((x - mn.x, -(mn.y + mx.y) * 0.5, -mn.z + 0.0002)); x += (mx.x - mn.x) + gap
        bpy.context.view_layer.update()
    for o in objs: o.location.x += cx - (x - gap) * 0.5
    bpy.context.view_layer.update()

def frame(objs, w, h, lens, d, margin, fstop, look_z):
    mn, mx = FV.bounds(objs); c = (mn + mx) * 0.5; ext = mx - mn
    dv = Vector(d).normalized(); right = Vector((0, 0, 1)).cross(-dv).normalized(); up = (-dv).cross(right).normalized()
    pts = [Vector((x, y, z)) for x in (mn.x, mx.x) for y in (mn.y, mx.y) for z in (mn.z, mx.z)]
    hw = max(abs((p - c).dot(right)) for p in pts) * margin; hh = max(abs((p - c).dot(up)) for p in pts) * margin
    tx = 18.0 / lens; ty = 18.0 * h / w / lens
    dist = max(hw / tx, hh / ty) + max(abs((p - c).dot(dv)) for p in pts)
    look = Vector((c.x, c.y, mn.z + ext.z * look_z))
    return FV.camera(look + dv * dist, look, lens, fstop)

def sheet(fid):
    FV.reset()
    was, mw = load(BEFORE, fid); now, mn_ = load(AFTER, fid)
    L = max(max(mw["size_m"]), max(mn_["size_m"]))
    FV.stage(span=L * 2.6); bpy.context.scene.render.resolution_percentage = 100
    fs = 11.0 * max(1.0, 0.06 / L); shots = []
    gap = L * 0.78
    for k, (yaw, d, lens, lz, hgt) in enumerate(((-28, (0.0, -1.0, 0.42), 40, 0.42, 900), (118, (0.25, -1.0, 0.16), 34, 0.45, 800))):
        made = []
        FV.place(was, -gap, 0.0, math.radians(yaw)); FV.place(now, gap, 0.0, math.radians(yaw))
        ty = min(FV.bounds([was])[0].y, FV.bounds([now])[0].y) - 0.14 * L
        if k == 0:
            made.append(FV.text("было", (-gap, ty, 0.0003), 0.10 * L)); made.append(FV.text("стало", (gap, ty, 0.0003), 0.10 * L))
        made.append(frame([was, now] + made, 1800, hgt, lens, d, 1.10, fs * (1.0 if k == 0 else 0.7), lz))
        p = os.path.join(WORK, "cmp_%s_%d.jpg" % (fid, k)); FV.render(p, 1800, hgt, SAMPLES); shots.append(p)
        for o in made: bpy.data.objects.remove(o, do_unlink=True)
    # the pre-cut pieces (what the food breaks into), their cut faces to the camera: was | now
    view = Vector((0.0, -1.0, 0.55)).normalized()
    pw = load_pieces(BEFORE, fid, was.material_slots[0].material, mw, view); pn = load_pieces(AFTER, fid, now.material_slots[0].material, mn_, view)
    was.hide_render = True; now.hide_render = True
    if pw or pn:
        wide = max([0.0] + [sum(max(o.dimensions) for o in g) + 0.1 * L * len(g) for g in (pw, pn)])
        half = max(L * 0.9, wide * 0.62)
        row(pw, -half, 0.1 * L); row(pn, half, 0.1 * L)
        # (two specks at the far ends: both halves are framed alike, whatever the pieces are)
        marks = [FV.cube("mark", (1e-5, 1e-5, 1e-5), (sx * half * 2.0, 0, 1e-5)) for sx in (-1, 1)]
        bpy.context.view_layer.update()
        cam_ = frame(pw + pn + marks, 1800, 800, 40, (0.0, -1.0, 0.55), 1.02, fs, 0.3)
        p = os.path.join(WORK, "cmp_%s_p.jpg" % fid); FV.render(p, 1800, 800, SAMPLES); shots.append(p)
        for o in pw + pn + marks + [cam_]: bpy.data.objects.remove(o, do_unlink=True)
    import numpy as np
    ims = []
    for p in shots:
        im = bpy.data.images.load(p); w, h = im.size; a = np.empty(w * h * 4, np.float32); im.pixels.foreach_get(a); ims.append(a.reshape(h, w, 4))
    H = sum(i.shape[0] for i in ims); big = np.ones((H, 1800, 4), np.float32); y = 0
    for i in reversed(ims):                 # (bottom-up: the last picture first)
        big[y:y + i.shape[0]] = i; y += i.shape[0]
    big[:, 898:902, :3] = 0.25
    o = bpy.data.images.new("cmp_" + fid, 1800, H); o.pixels.foreach_set(big.ravel())
    out = os.path.join(OUT, "%s_was_now.jpg" % fid); o.filepath_raw = out; o.file_format = "JPEG"; o.save()
    log("SHEET", out)

for fid in IDS:
    try: sheet(fid)
    except Exception as e:
        import traceback; traceback.print_exc(); log("FAILED", fid, e)
log("ALL_DONE")
