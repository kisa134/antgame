# KAN-24: imports our own surface sets (SourceArt/Blender/textures_own/T_M4_*, made by Tools/blender/make_own_textures.py)
# over /Game/Map4/Textures/T_M4_<Set>_{C,N,R} IN PLACE: same asset names, so every material and instance keeps its links.
# Only the textures (make_m4_materials.py also re-imports the decals and rebuilds the materials; not needed for this).
#   M4_TEX=Grass,Soil  only these sets
# Run: UnrealEditor-Cmd.exe AntKitchen.uproject -run=pythonscript -script=Tools/map4/import_own_textures.py
import unreal, os, glob
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OWN = os.path.join(PROJ, "SourceArt", "Blender", "textures_own")
TEX = "/Game/Map4/Textures"


def import_tex(f, name, kind):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f)
    t.set_editor_property("destination_path", TEX)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", False)
    AT.import_asset_tasks([t])
    a = unreal.load_asset(TEX + "/" + name)
    if not a:
        unreal.log_warning("OWNTEX import failed " + f)
        return False
    if kind == "N":
        a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        a.set_editor_property("srgb", False)
    elif kind == "R":
        a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        a.set_editor_property("srgb", False)
    else:
        a.set_editor_property("srgb", True)
    a.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD if kind != "N" else unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    EAL.save_asset(TEX + "/" + name)
    return True


only = [s for s in os.environ.get("M4_TEX", "").split(",") if s]
n = 0
for f in sorted(glob.glob(os.path.join(OWN, "T_M4_*"))):
    name = os.path.splitext(os.path.basename(f))[0]
    if only and not any(name.startswith("T_M4_%s_" % s) for s in only):
        continue
    if import_tex(f, name, name[-1]):
        n += 1
        unreal.log("OWNTEX " + name)
unreal.log("OWNTEX_DONE %d" % n)
