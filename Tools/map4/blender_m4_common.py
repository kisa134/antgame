# shared helpers for the map v4 Blender scripts (exec'd by them)
def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)

def obj(bm, name):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(o); return o

def uv(ob, kind="smart"):
    bpy.context.view_layer.objects.active = ob
    for o in bpy.context.scene.objects: o.select_set(o == ob)
    bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="SELECT")
    if kind == "cube": bpy.ops.uv.cube_project(cube_size=1.0)
    elif kind == "cyl": bpy.ops.uv.cylinder_project()
    else: bpy.ops.uv.smart_project(angle_limit=1.15)
    bpy.ops.object.mode_set(mode="OBJECT")

def double_side(bm, faces=None):
    faces = list(faces if faces is not None else bm.faces)
    d = bmesh.ops.duplicate(bm, geom=faces)
    nf = [g for g in d["geom"] if isinstance(g, bmesh.types.BMFace)]
    bmesh.ops.reverse_faces(bm, faces=nf)

def lathe_z(profile, segs=32):
    """profile: (radius, z) from bottom to top; caps at radius 0"""
    bm = bmesh.new()
    rings = []
    for (r, z) in profile:
        if r <= 1e-6:
            rings.append([bm.verts.new((0, 0, z))])
        else:
            rings.append([bm.verts.new((math.cos(2 * math.pi * i / segs) * r, math.sin(2 * math.pi * i / segs) * r, z)) for i in range(segs)])
    for k in range(len(rings) - 1):
        A, B = rings[k], rings[k + 1]
        for i in range(segs):
            if len(A) == 1 and len(B) == 1: continue
            if len(A) == 1: bm.faces.new((A[0], B[(i + 1) % segs], B[i]))
            elif len(B) == 1: bm.faces.new((A[i], A[(i + 1) % segs], B[0]))
            else: bm.faces.new((A[i], A[(i + 1) % segs], B[(i + 1) % segs], B[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return bm

def export(ob, name, slots=None, ucx=None, smooth=True, extra=None):
    d = os.path.join(OUT, name); os.makedirs(d, exist_ok=True)
    me = ob.data
    me.materials.clear()
    names = [s[0] for s in slots] if slots else ["M4Shape"]
    for n in names:
        me.materials.append(bpy.data.materials.get(n) or bpy.data.materials.new(n))
    if slots:
        for p in me.polygons:
            c = sum((me.vertices[i].co for i in p.vertices), Vector()) / len(p.vertices)
            for k, (n, pred) in enumerate(slots):
                if pred is None or pred(c, p):
                    p.material_index = k
                    break
    for p in me.polygons: p.use_smooth = smooth
    objs = [ob]
    if ucx:
        b2 = bmesh.new()
        for q in ucx: b2.verts.new(q)
        bmesh.ops.convex_hull(b2, input=b2.verts)
        objs.append(obj(b2, "UCX_%s_00" % name))
    for o in bpy.context.scene.objects: o.select_set(o in objs)
    bpy.context.view_layer.objects.active = ob
    ob.name = name
    bpy.ops.export_scene.fbx(filepath=os.path.join(d, name + ".fbx"), use_selection=True, object_types={"MESH"}, use_mesh_modifiers=True,
                             mesh_smooth_type="FACE", bake_anim=False, add_leaf_bones=False)
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    info = {"name": name, "tris": tris, "nanite": False, "slots": bool(slots)}
    if extra: info.update(extra)
    json.dump(info, open(os.path.join(d, name + ".json"), "w"))
    print("M4PROP", name, tris)
