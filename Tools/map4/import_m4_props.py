# Map v4: imports every prop in SourceArt/Map4/props/<Name>/ (made by Tools/map4/blender_glb_prop.py or the Blender
# prop scripts) into /Game/Map4/Props/<Name>/: the mesh (Nanite, a simple box + its own triangles for collision),
# T_<Name>_{C,N,R} and MI_<Short> (an instance of /Game/Map4/Materials/M_M4_UV).
# Run: UnrealEditor-Cmd.exe AntKitchen.uproject -run=pythonscript -script=Tools/map4/import_m4_props.py
# Optional: M4_PROPS=SM_M4_A,SM_M4_B limits the set.
import unreal, os, glob, json
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJ, "SourceArt", "Map4", "props")
DST = "/Game/Map4/Props"
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
only = [s.strip() for s in os.environ.get("M4_PROPS", "").split(",") if s.strip()]

def task(f, dest, name, opts=None):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    if opts:
        t.set_editor_property("options", opts)
    AT.import_asset_tasks([t])
    return unreal.load_asset(dest + "/" + name)

report = {}
for folder in sorted(glob.glob(os.path.join(SRC, "SM_*"))):
    name = os.path.basename(folder)
    if only and name not in only:
        continue
    fbx = os.path.join(folder, name + ".fbx")
    if not os.path.exists(fbx):
        continue
    dest = DST + "/" + name
    info = {}
    jp = os.path.join(folder, name + ".json")
    if os.path.exists(jp):
        info = json.load(open(jp))
    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", True)
    opts.set_editor_property("import_as_skeletal", False)
    opts.set_editor_property("import_materials", False)
    opts.set_editor_property("import_textures", False)
    opts.set_editor_property("import_animations", False)
    opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = opts.get_editor_property("static_mesh_import_data")
    sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("auto_generate_collision", False)
    sm.set_editor_property("generate_lightmap_u_vs", False)
    sm.set_editor_property("one_convex_hull_per_ucx", True)
    try:
        sm.set_editor_property("build_nanite", info.get("nanite", True))
    except Exception:
        pass
    mesh = task(fbx, dest, name, opts)
    if not mesh:
        unreal.log_warning("M4PROP failed " + name)
        continue
    tex = {}
    for k in ("C", "N", "R"):
        f = os.path.join(folder, "T_%s_%s.png" % (name, k))
        if not os.path.exists(f):
            continue
        tn = "T_%s_%s" % (name[3:], k)
        tx = task(f, dest, tn)
        if tx:
            if k == "N":
                tx.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                tx.set_editor_property("srgb", False)
            elif k == "R":
                tx.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
                tx.set_editor_property("srgb", False)
            EAL.save_asset(dest + "/" + tn)
            tex[k] = tx
    mi_name = "MI_" + name[3:]
    if EAL.does_asset_exist(dest + "/" + mi_name):
        EAL.delete_asset(dest + "/" + mi_name)
    mi = AT.create_asset(mi_name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    parent = unreal.load_asset("/Game/Map4/Materials/" + ("M_M4_UV2" if info.get("two_sided") else "M_M4_UV"))
    MEL.set_material_instance_parent(mi, parent)
    for k, tx in tex.items():
        MEL.set_material_instance_texture_parameter_value(mi, k, tx)
    if info.get("rough_add") is not None:
        MEL.set_material_instance_scalar_parameter_value(mi, "RoughAdd", info["rough_add"])
    EAL.save_asset(dest + "/" + mi_name)
    # collision: the drawn triangles for queries (ants walk on what is drawn), plus a box unless UCX hulls came with the FBX
    bs = mesh.get_editor_property("body_setup")
    if bs:
        agg = bs.get_editor_property("agg_geom")
        if not agg.get_editor_property("convex_elems"):
            b = mesh.get_bounding_box()
            e = unreal.KBoxElem()
            e.set_editor_property("center", unreal.Vector((b.min.x + b.max.x) * 0.5, (b.min.y + b.max.y) * 0.5, (b.min.z + b.max.z) * 0.5))
            e.set_editor_property("x", max(0.1, b.max.x - b.min.x))
            e.set_editor_property("y", max(0.1, b.max.y - b.min.y))
            e.set_editor_property("z", max(0.1, b.max.z - b.min.z))
            agg.set_editor_property("box_elems", [e])
            bs.set_editor_property("agg_geom", agg)
        # static props the ants climb: their own triangles; physics props and weapons: the simple body
        PHYS = ("SM_M4_BottleCap", "SM_M4_Coin", "SM_M4_Thimble", "SM_M4_Grape", "SM_M4_Gem", "SM_M4_CarrotSlice", "SM_M4_BayLeaf", "SM_M4_Bat", "SM_M4_Pan", "SM_M4_Sausage")
        cas = info.get("complex_simple", bool(info.get("slots")) and name not in PHYS)
        bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE if cas else unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
    if info.get("slots"):
        pass   # material slots named after the game's looks (Map4Look / KitchenArt::ApplyLook): the code assigns them
    elif info.get("generic") or not tex:
        paint = unreal.load_asset("/Game/Map4/Materials/MI_M4_Paint")
        mesh.set_material(0, paint if paint else mi)
    else:
        mesh.set_material(0, mi)
    EAL.save_loaded_asset(mesh)
    b = mesh.get_bounding_box()
    report[name] = {"min": [b.min.x, b.min.y, b.min.z], "max": [b.max.x, b.max.y, b.max.z], "tex": list(tex.keys())}
    unreal.log("M4PROP imported %s %s" % (name, report[name]))
out = os.path.join(PROJ, "Saved", "map4")
os.makedirs(out, exist_ok=True)
json.dump(report, open(os.path.join(out, "props_report.json"), "w"), indent=1)
unreal.log("M4PROPS_DONE %d" % len(report))
