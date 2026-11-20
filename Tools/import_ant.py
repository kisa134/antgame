# Imports the cut pieces of the ant: SourceArt/Blender/ant/SM_AntB_*.fbx -> /Game/Ant/<Name>/<Name>.
# No materials or textures are imported: every piece keeps the UVs of the Meshy mesh it was cut from and the game draws
# it with that mesh's material instance (/Game/Meshy/SM_Ant_Body/MI_Ant_Body and so on). Nanite like the Meshy parts,
# no collision. Run (see Saved/ant/imp.ps1):
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=%TEMP%\import_ant.py -unattended -nosplash -nullrhi
import unreal, os, glob, json

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJ, "SourceArt", "Blender", "ant")
DST = "/Game/Ant"
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()

def log(s):
    unreal.log_warning("ANTART: " + s)

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
    for k, v in (("build_nanite", True), ("build_reversed_index_buffer", False)):
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

manifest = {}
mpath = os.path.join(SRC, "ant_manifest.json")
if os.path.exists(mpath):
    with open(mpath, encoding="utf-8") as f:
        for p in json.load(f).get("pieces", []):
            manifest[p["name"]] = p
ok, bad = 0, []
report = {}
for fbx in sorted(glob.glob(os.path.join(SRC, "SM_AntB_*.fbx"))):
    name = os.path.splitext(os.path.basename(fbx))[0]
    mesh = import_fbx(fbx, DST + "/" + name, name)
    if not mesh or not isinstance(mesh, unreal.StaticMesh):
        log("FAILED " + name)
        bad.append(name)
        continue
    p = manifest.get(name)
    src_mat = None
    if p:
        short = p["from"][3:]
        src_mat = unreal.load_asset("/Game/Meshy/%s/MI_%s" % (p["from"], short))
        if src_mat:
            mesh.set_material(0, src_mat)
    bs = mesh.get_editor_property("body_setup")
    if bs:
        bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    EAL.save_loaded_asset(mesh)
    box = mesh.get_bounding_box()
    mn, mx = box.min, box.max
    note = ""
    if p:
        d = max(abs(mn.x - p["min"][0]), abs(mn.y - p["min"][1]), abs(mn.z - p["min"][2]), abs(mx.x - p["max"][0]), abs(mx.y - p["max"][1]), abs(mx.z - p["max"][2]))
        if d > 0.05:
            note = " BOUNDS_DIFF=%.3f ue=(%.2f %.2f %.2f)..(%.2f %.2f %.2f)" % (d, mn.x, mn.y, mn.z, mx.x, mx.y, mx.z)
    log("%s: %d tris, material %s%s" % (name, mesh.get_num_triangles(0), src_mat.get_name() if src_mat else "NONE", note))
    report[name] = {"min": [mn.x, mn.y, mn.z], "max": [mx.x, mx.y, mx.z]}
    ok += 1
with open(os.path.join(PROJ, "Saved", "ant_art_report.json"), "w", encoding="utf-8") as f:
    json.dump(report, f, indent=1)
log("imported %d, failed: %s" % (ok, ",".join(bad) or "none"))
log("ANT_IMPORT_DONE")
