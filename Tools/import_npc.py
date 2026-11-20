# Imports the jointed creatures' segments: SourceArt/Blender/npc_<set>/*.fbx -> /Game/Npc/<Name>/<Name>.
# One static mesh per FBX, material SLOT NAMES kept (the game maps slot name -> look in code: NpcBody.cpp),
# no materials imported, no Nanite, collision = the mesh's own triangles (ants crawl on exactly what is drawn).
# The segments are drawn with /Game/Human/M_HumanSurface (made by Tools/import_human.py).
# Run (see Saved/npc/imp.ps1):  set ANT_NPC_SETS=cat,dog  then
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=%TEMP%\import_npc.py -unattended -nosplash -nullrhi
import unreal, os, glob, json

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
DST = "/Game/Npc"
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()

def log(s):
    unreal.log_warning("NPCART: " + s)

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

def complex_collision(mesh):
    bs = mesh.get_editor_property("body_setup")
    if bs:
        agg = bs.get_editor_property("agg_geom")
        for k in ("box_elems", "sphere_elems", "sphyl_elems", "convex_elems"):
            agg.set_editor_property(k, [])
        bs.set_editor_property("agg_geom", agg)
        bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)

sets = [s.strip().lower() for s in os.environ.get("ANT_NPC_SETS", "").split(",") if s.strip()]
if not sets:
    sets = [os.path.basename(d)[4:] for d in sorted(glob.glob(os.path.join(PROJ, "SourceArt", "Blender", "npc_*"))) if os.path.isdir(d)]
report = {}
for set_name in sets:
    src = os.path.join(PROJ, "SourceArt", "Blender", "npc_" + set_name)
    files = sorted(glob.glob(os.path.join(src, "*.fbx")))
    if not files:
        log("%s: no FBX in %s, skipped" % (set_name, src))
        continue
    manifest = {}
    mpath = os.path.join(src, "manifest.json")
    if os.path.exists(mpath):
        with open(mpath, encoding="utf-8") as f:
            for p in json.load(f).get("pieces", []):
                manifest[p["name"]] = p
    ok, bad, tris_total = 0, [], 0
    for fbx in files:
        name = os.path.splitext(os.path.basename(fbx))[0]
        mesh = import_fbx(fbx, DST + "/" + name, name)
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
        complex_collision(mesh)
        EAL.save_loaded_asset(mesh)
        box = mesh.get_bounding_box()
        mn, mx = box.min, box.max
        slots = [str(m.get_editor_property("material_slot_name")) for m in mesh.get_editor_property("static_materials")]
        tris = mesh.get_num_triangles(0)
        tris_total += tris
        note = ""
        p = manifest.get(name)
        if p:
            a, b = p["bbox_min_m"], p["bbox_max_m"]
            emn = [100.0 * a[0], -100.0 * b[1], 100.0 * a[2]]
            emx = [100.0 * b[0], -100.0 * a[1], 100.0 * b[2]]
            d = max(abs(mn.x - emn[0]), abs(mn.y - emn[1]), abs(mn.z - emn[2]), abs(mx.x - emx[0]), abs(mx.y - emx[1]), abs(mx.z - emx[2]))
            if d > 0.06:
                note += " BOUNDS_DIFF=%.2f" % d
            if sorted(slots) != sorted(p["materials"]):
                note += " SLOTS_DIFF manifest=" + ",".join(p["materials"])
        if note:
            log("%s:%s" % (name, note))
        ok += 1
        report[name] = {"tris": tris, "slots": slots, "min": [mn.x, mn.y, mn.z], "max": [mx.x, mx.y, mx.z]}
    log("%s: imported %d of %d, %d triangles, failed: %s" % (set_name, ok, len(files), tris_total, ",".join(bad) or "none"))
with open(os.path.join(PROJ, "Saved", "npc_art_report.json"), "w", encoding="utf-8") as f:
    json.dump(report, f, indent=1)
log("NPC_IMPORT_DONE")
