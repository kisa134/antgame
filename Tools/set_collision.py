# Meshy kitchen models: use their own triangles as collision (complex as simple),
# so ants crawl on exactly what is drawn. Ant body parts and food are skipped.
import unreal
EAL = unreal.EditorAssetLibrary
SKIP = ("SM_Ant_", "SM_Food_", "SM_Human_", "SM_Slipper", "SM_BugSpray", "SM_RolledNewspaper")
n = 0
for path in EAL.list_assets("/Game/Meshy", recursive=True, include_folder=False):
    a = EAL.load_asset(path)
    if not isinstance(a, unreal.StaticMesh):
        continue
    name = a.get_name()
    if name.startswith(SKIP):
        continue
    bs = a.get_editor_property("body_setup")
    if bs is None:
        continue
    bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    EAL.save_loaded_asset(a)
    n += 1
    unreal.log("COLLISION complex " + name)
unreal.log("COLLISION_DONE %d" % n)
