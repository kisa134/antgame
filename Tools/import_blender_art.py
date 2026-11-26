# Imports the Blender-made level art into the project as static meshes:
#   SourceArt/Blender/kitchen/*.fbx -> /Game/Kitchen/<Name>/<Name>
#   SourceArt/Blender/living/*.fbx  -> /Game/Living/<Name>/<Name>
#   SourceArt/Blender/human/*.fbx   -> /Game/Human/<Name>/<Name>   (run through Tools/import_human.py)
#   SourceArt/Blender/level_v2/*.fbx -> /Game/Kitchen or /Game/Living by the name's prefix (set "level_v2")
# One mesh per FBX (combined), material SLOT NAMES are kept (the game maps slot name -> look in code),
# no materials / textures are imported, no Nanite, no lightmap UVs.
# Collision: furniture and everything ants crawl on or into uses its own triangles
# ("use complex collision as simple"); small loose props also get a simple box so they can be
# simulated or carried. Writes Saved/blender_art_report.json and compares bounds / slots with the
# manifest next to the FBX files. Lines you need to read are logged as warnings ("ART: ...").
#
# Run (see Saved/art/import_art.ps1):
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=%TEMP%\import_blender_art.py -unattended -nosplash -nullrhi
# Optional environment variable ANT_ART_SETS=kitchen,living limits what is imported.
import unreal, os, glob, json

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SETS = [
    ("kitchen", os.path.join(PROJ, "SourceArt", "Blender", "kitchen"), "/Game/Kitchen", "kitchen_manifest.json"),
    ("living", os.path.join(PROJ, "SourceArt", "Blender", "living"), "/Game/Living", "living_manifest.json"),
    ("human", os.path.join(PROJ, "SourceArt", "Blender", "human"), "/Game/Human", "human_manifest.json"),   # the Human's segments (Tools/import_human.py)
    # level v2: the folder is picked by the name, the way KitchenArt::Mesh looks a mesh up (SM_K_* -> /Game/Kitchen, the rest -> /Game/Living)
    ("level_v2", os.path.join(PROJ, "SourceArt", "Blender", "level_v2"), None, "level_v2_new_pieces.json"),
    # level v3: every SM_V3_* piece in its own folder (KitchenArt::Mesh looks SM_V3_* up under /Game/LevelV3)
    ("level_v3", os.path.join(PROJ, "SourceArt", "Blender", "level_v3"), "/Game/LevelV3", "level_v3_new_pieces.json"),
    ("level_v31", os.path.join(PROJ, "SourceArt", "Blender", "level_v31"), "/Game/LevelV3", "level_v31_new_pieces.json"),
    ("level_v32", os.path.join(PROJ, "SourceArt", "Blender", "level_v32"), "/Game/LevelV3", "level_v32_new_pieces.json"),   # the sink zone pilot
]
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()

# Loose props: a simple box as well (the game may simulate or carry them). Everything else: triangles only.
SIMPLE_BOX = (
    "SM_K_Toast", "SM_K_SpiceJar", "SM_K_Shaker", "SM_K_Sponge", "SM_K_Knife", "SM_K_Fork", "SM_K_IceTray",
    "SM_K_EggTray", "SM_K_MilkCarton", "SM_K_CheeseWedge", "SM_K_ButterTub", "SM_K_Bone", "SM_K_Detergent",
    "SM_K_Pot_Lid", "SM_K_TennisBall",)

def log(s):
    unreal.log_warning("ART: " + s)

def import_fbx(filename, dest, name):
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
    sm.set_editor_property("remove_degenerates", False)
    sm.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    for k, v in (("build_nanite", False), ("build_reversed_index_buffer", False)):
        try:
            sm.set_editor_property(k, v)
        except Exception:
            pass
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", filename)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    t.set_editor_property("options", opts)
    AT.import_asset_tasks([t])
    return unreal.load_asset(dest + "/" + name)

def setup_collision(mesh, name):
    simple = name in SIMPLE_BOX
    bs = mesh.get_editor_property("body_setup")
    if bs:
        # (the editor subsystems do not exist in a commandlet: the aggregate geometry is written directly)
        agg = bs.get_editor_property("agg_geom")
        boxes = []
        if simple:
            b = mesh.get_bounding_box()
            e = unreal.KBoxElem()
            e.set_editor_property("center", unreal.Vector((b.min.x + b.max.x) * 0.5, (b.min.y + b.max.y) * 0.5, (b.min.z + b.max.z) * 0.5))
            e.set_editor_property("x", max(0.1, b.max.x - b.min.x))
            e.set_editor_property("y", max(0.1, b.max.y - b.min.y))
            e.set_editor_property("z", max(0.1, b.max.z - b.min.z))
            boxes = [e]
        agg.set_editor_property("box_elems", boxes)
        agg.set_editor_property("sphere_elems", [])
        agg.set_editor_property("sphyl_elems", [])
        agg.set_editor_property("convex_elems", [])
        bs.set_editor_property("agg_geom", agg)
        flag = unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX if simple else unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
        bs.set_editor_property("collision_trace_flag", flag)
    return "box" if simple else "complex"

only = [s.strip().lower() for s in os.environ.get("ANT_ART_SETS", "").split(",") if s.strip()]
report = {}
for set_name, src, dst, manifest_name in SETS:
    if only and set_name not in only:
        continue
    files = sorted(glob.glob(os.path.join(src, "*.fbx")))
    if not files:
        log("%s: no FBX in %s, skipped" % (set_name, src))
        continue
    manifest = {}
    mpath = os.path.join(src, manifest_name)
    if os.path.exists(mpath):
        with open(mpath, encoding="utf-8") as f:
            for p in json.load(f).get("pieces", []):
                manifest[p["name"]] = p
    ok = 0
    bad = []
    for fbx in files:
        name = os.path.splitext(os.path.basename(fbx))[0]
        dest = (dst if dst else ("/Game/Kitchen" if name.startswith("SM_K_") else "/Game/Living")) + "/" + name
        mesh = import_fbx(fbx, dest, name)
        if not mesh or not isinstance(mesh, unreal.StaticMesh):
            log("FAILED " + name)
            bad.append(name)
            continue
        try:
            ns = mesh.get_editor_property("nanite_settings")
            if ns.get_editor_property("enabled"):
                ns.set_editor_property("enabled", False)
                mesh.set_editor_property("nanite_settings", ns)
        except Exception:
            pass
        coll = setup_collision(mesh, name)
        EAL.save_loaded_asset(mesh)
        box = mesh.get_bounding_box()
        mn, mx = box.min, box.max
        slots = [str(m.get_editor_property("material_slot_name")) for m in mesh.get_editor_property("static_materials")]
        tris = mesh.get_num_triangles(0)
        note = ""
        p = manifest.get(name)
        if p:
            a, b = p["bbox_min_m"], p["bbox_max_m"]
            emn = [100.0 * a[0], -100.0 * b[1], 100.0 * a[2]]
            emx = [100.0 * b[0], -100.0 * a[1], 100.0 * b[2]]
            d = max(abs(mn.x - emn[0]), abs(mn.y - emn[1]), abs(mn.z - emn[2]), abs(mx.x - emx[0]), abs(mx.y - emx[1]), abs(mx.z - emx[2]))
            if d > 0.06:
                note += " BOUNDS_DIFF=%.2f expected(%.1f,%.1f,%.1f)..(%.1f,%.1f,%.1f)" % (d, emn[0], emn[1], emn[2], emx[0], emx[1], emx[2])
            if sorted(slots) != sorted(p["materials"]):
                note += " SLOTS_DIFF manifest=" + ",".join(p["materials"])
            if tris != p.get("triangles", tris):
                note += " TRIS manifest=%d" % p["triangles"]
        elif manifest:
            note += " NOT_IN_MANIFEST"
        log("%s min(%.1f,%.1f,%.1f) max(%.1f,%.1f,%.1f) tris %d coll %s slots %s%s" % (
            name, mn.x, mn.y, mn.z, mx.x, mx.y, mx.z, tris, coll, ",".join(slots), note))
        report[name] = {"mesh": dest + "/" + name + "." + name, "min": [mn.x, mn.y, mn.z], "max": [mx.x, mx.y, mx.z],
                        "tris": tris, "slots": slots, "collision": coll, "set": set_name, "note": note.strip()}
        ok += 1
    missing = [n for n in manifest if n not in report]
    log("SET %s: imported %d of %d FBX, failed %d, manifest pieces without a mesh: %d %s" % (
        set_name, ok, len(files), len(bad), len(missing), ",".join(missing[:20])))

with open(os.path.join(PROJ, "Saved", os.environ.get("ANT_ART_REPORT", "blender_art_report.json")), "w") as f:
    json.dump(report, f, indent=1)
log("DONE %d meshes" % len(report))