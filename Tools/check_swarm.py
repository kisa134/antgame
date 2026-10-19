import unreal
m = unreal.load_asset("/Game/Swarm/SM_SwarmAnt")
unreal.log("SWARM_CHECK tris=%d nanite=%s" % (m.get_num_triangles(0), m.get_editor_property("nanite_settings").get_editor_property("enabled")))
