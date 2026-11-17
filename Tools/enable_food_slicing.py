# Makes the food models sliceable at runtime: the fracture code reads their triangles on the CPU and cuts
# the lowest LOD, so every food mesh gets CPU access and a light last LOD.
import unreal

NAMES = ["SM_Food_GummyBearGold", "SM_Food_Gummy", "SM_Food_CheeseChunk", "SM_Food_HoneyDrop", "SM_Food_CakePiece",
         "SM_Food_Chocolate", "SM_Food_SugarCube", "SM_Food_DeadFly", "SM_Food_SpiderLeg"]
TARGET_TRIS = 2500.0

def log(s):
    unreal.log_warning("FOODSLICE: " + s)

sub = None
try:
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
except Exception as e:
    log("no StaticMeshEditorSubsystem: %s" % e)

for n in NAMES:
    path = "/Game/Meshy/%s/%s" % (n, n)
    mesh = unreal.load_asset(path)
    if not mesh:
        log("%s MISSING" % n)
        continue
    tris = mesh.get_num_triangles(0)
    lods = mesh.get_num_lods()
    nanite = False
    try:
        nanite = mesh.get_editor_property("nanite_settings").enabled
    except Exception:
        pass
    log("%s: tris=%d lods=%d nanite=%s cpu=%s" % (n, tris, lods, nanite, mesh.get_editor_property("allow_cpu_access")))
    if lods == 1 and tris > TARGET_TRIS * 1.5 and sub:
        try:
            opts = unreal.StaticMeshReductionOptions()
            opts.auto_compute_lod_screen_size = False
            a = unreal.StaticMeshReductionSettings()
            a.percent_triangles = 1.0
            a.screen_size = 1.0
            b = unreal.StaticMeshReductionSettings()
            b.percent_triangles = max(0.01, TARGET_TRIS / float(tris))
            b.screen_size = 0.04
            opts.reduction_settings = [a, b]
            sub.set_lods_with_notification(mesh, opts, True)
            log("%s: added LOD1, now lods=%d lastTris=%d" % (n, mesh.get_num_lods(), mesh.get_num_triangles(mesh.get_num_lods() - 1)))
        except Exception as e:
            log("%s: LOD failed: %s" % (n, e))
    mesh.set_editor_property("allow_cpu_access", True)
    ok = unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    log("%s: saved=%s" % (n, ok))
log("DONE")