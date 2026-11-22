# Map v4: the M4 base materials are used on instanced meshes (grass tufts, trail dots, swarm legs) and on Nanite props;
# a material without the usage flag draws as the grey default on those. Sets the flags in place (no rebuild: the props'
# instances keep their parents) and saves.  Run like the other Tools/map4 editor scripts.
import unreal
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
for n in ("M_M4", "M_M4_2S", "M_M4_UV", "M_M4_UV2"):
    m = unreal.load_asset("/Game/Map4/Materials/" + n)
    if not m:
        unreal.log_warning("M4USAGE missing " + n); continue
    for flag in ("used_with_instanced_static_meshes", "used_with_nanite", "used_with_skeletal_mesh", "used_with_morph_targets", "used_with_spline_meshes"):
        try:
            m.set_editor_property(flag, True)
        except Exception as e:
            unreal.log_warning("M4USAGE %s %s: %s" % (n, flag, e))
    MEL.recompile_material(m)
    EAL.save_asset("/Game/Map4/Materials/" + n)
    unreal.log("M4USAGE " + n)
