import unreal
EAL = unreal.EditorAssetLibrary
m = unreal.load_asset("/Game/Swarm/SM_SwarmAnt")
opts = unreal.EditorScriptingMeshReductionOptions()
rs = []
for pct, screen in ((1.0, 1.0), (0.12, 0.08), (0.03, 0.02)):
    r = unreal.EditorScriptingMeshReductionSettings()
    r.set_editor_property("percent_triangles", pct)
    r.set_editor_property("screen_size", screen)
    rs.append(r)
opts.set_editor_property("reduction_settings", rs)
opts.set_editor_property("auto_compute_lod_screen_size", False)
n = unreal.EditorStaticMeshLibrary.set_lods(m, opts)
EAL.save_loaded_asset(m)
unreal.log("SWARM_LODS n=%d " % n + " ".join(str(m.get_num_triangles(i)) for i in range(m.get_num_lods())))
