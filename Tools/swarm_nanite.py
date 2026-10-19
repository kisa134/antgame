# The swarm uses the Nanite ant body directly: Nanite keeps tens of thousands of instances cheap.
import unreal
EAL = unreal.EditorAssetLibrary
for p in ("/Game/Swarm/M_SwarmAnt", "/Game/Swarm/M_SwarmAntArt"):
    m = unreal.load_asset(p)
    if m:
        m.set_editor_property("used_with_nanite", True)
        m.set_editor_property("used_with_instanced_static_meshes", True)
        unreal.MaterialEditingLibrary.recompile_material(m)
        EAL.save_loaded_asset(m)
if EAL.does_asset_exist("/Game/Swarm/SM_SwarmAnt"):
    EAL.delete_asset("/Game/Swarm/SM_SwarmAnt")
unreal.log("SWARM_NANITE_OK")
