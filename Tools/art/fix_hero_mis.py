# KAN-22: re-point every hero MI_<Name> at M_Hero and its own baked set (T_<Name>_C/N/R).
# Needed after M_Hero is rebuilt in place: the overrides of the instances must follow the new parameter expressions.
# Run: Saved/art/ue_py.cmd <copy of this file> <log>
import unreal, json, os
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
doc = json.load(open(os.path.join(proj, "SourceArt/Blender/hero/hero_manifest.json"), encoding="utf-8"))
parent = unreal.load_asset("/Game/Kitchen/Hero/M_Hero")
for p in doc["pieces"]:
    name = p["name"]; d = "/Game/Kitchen/" + name
    mi = unreal.load_asset(d + "/MI_" + name)
    if not mi:
        unreal.log_warning("ART: FIXMI %s: no MI" % name); continue
    MEL.set_material_instance_parent(mi, parent)
    got = []
    for k in ("C", "N", "R"):
        t = unreal.load_asset("%s/T_%s_%s" % (d, name, k))
        if t:
            MEL.set_material_instance_texture_parameter_value(mi, k, t)
        v = MEL.get_material_instance_texture_parameter_value(mi, k)
        got.append("%s=%s" % (k, v.get_name() if v else None))
    MEL.update_material_instance(mi)
    EAL.save_asset(d + "/MI_" + name)
    unreal.log_warning("ART: FIXMI %s %s" % (name, " ".join(got)))
