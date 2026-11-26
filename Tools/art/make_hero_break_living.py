# Hero art, living room (hero_living.py): the fracture-face materials the living set needs on top of make_hero_material.py
# and make_hero_glass.py: MI_HeroBreak_Cloth (torn upholstery: the foam and the cut weave) and MI_HeroBreak_Plaster
# (the white core of a broken cornice). Children of /Game/Kitchen/Hero/M_Hero, no texture (UseTex 0), like the others.
# Run: UnrealEditor-Cmd <uproject> -run=pythonscript -script=<copy of this file>
import unreal
DIR = "/Game/Kitchen/Hero"
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()

def break_mi(name, tint, rough):
    mi = unreal.load_asset(DIR + "/" + name) or AT.create_asset(name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, unreal.load_asset(DIR + "/M_Hero"))
    MEL.set_material_instance_scalar_parameter_value(mi, "UseTex", 0.0)
    MEL.set_material_instance_scalar_parameter_value(mi, "BreakRough", rough)
    MEL.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*tint))
    EAL.save_asset(DIR + "/" + name)
    unreal.log_warning("ART: HERO " + name)

if not unreal.load_asset(DIR + "/M_Hero"):
    unreal.log_error("ART: M_Hero missing - run Tools/art/make_hero_material.py first")
else:
    break_mi("MI_HeroBreak_Cloth", (0.88, 0.80, 0.52, 1), 0.95)     # yellow upholstery foam in the torn cover
    break_mi("MI_HeroBreak_Plaster", (0.93, 0.92, 0.88, 1), 0.92)   # chalk-white gypsum core
