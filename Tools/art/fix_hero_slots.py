# KAN-22: put MI_<Name> (and MI_HeroBreak_<cls> on the fracture slot) back on every hero mesh, its loose parts and break pieces.
# Run: Saved/art/ue_py.cmd <copy of this file> <log>
import unreal, json, os
EAL = unreal.EditorAssetLibrary
proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
doc = json.load(open(os.path.join(proj, "SourceArt/Blender/hero/hero_manifest.json"), encoding="utf-8"))
H = "/Game/Kitchen/Hero"
brk = {"plastic": unreal.load_asset(H + "/MI_HeroBreak_Plastic"), "wood": unreal.load_asset(H + "/MI_HeroBreak_Wood")}

def hero_dir(name):
    return "/Game/LevelV3/" + name if name.startswith("SM_V3") else "/Game/Kitchen/" + name if name.startswith("SM_K_") else "/Game/Living/" + name

def put(path, mats):
    m = unreal.load_asset(path)
    if not m:
        unreal.log_warning("ART: FIXSLOT missing " + path); return
    n = len(m.get_editor_property("static_materials"))
    for i in range(n):
        if i < len(mats) and mats[i]: m.set_material(i, mats[i])
    EAL.save_asset(path)
    unreal.log_warning("ART: FIXSLOT %s %s" % (m.get_name(), [m.get_material(i).get_name() if m.get_material(i) else None for i in range(n)]))

for p in doc["pieces"]:
    name = p["name"]; d = hero_dir(name)
    mi = unreal.load_asset(d + "/MI_" + name)
    if not mi:
        unreal.log_warning("ART: FIXSLOT no MI " + d); continue
    unreal.MaterialEditingLibrary.set_material_instance_parent(mi, unreal.load_asset(H + "/M_Hero"))
    EAL.save_asset(d + "/MI_" + name)
    cls = "plastic" if any(k in name for k in ("Bucket", "Pail", "Sprayer", "DishSoap")) else "wood"
    put(d + "/" + name, [mi])
    for mv in p.get("movers", []):
        put(d + "/" + mv["name"], [mi])
    for bp in p.get("breaks", []):
        put(d + "/" + bp["name"], [mi, brk[cls]])
