# The colony screen's textures (docs/COLONY.md): SourceArt/UI/Colony/*.png -> /Game/UI/Colony as UI textures (no mips, clamped).
# Run: UnrealEditor-Cmd.exe AntKitchen.uproject -run=pythonscript -script=Tools/make_colony_assets.py
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "SourceArt", "UI", "Colony")
AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary

tasks = []
for f in sorted(os.listdir(SRC)):
    if not f.lower().endswith(".png"):
        continue
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", os.path.join(SRC, f))
    t.set_editor_property("destination_path", "/Game/UI/Colony")
    t.set_editor_property("destination_name", os.path.splitext(f)[0])
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", False)
    tasks.append(t)
AT.import_asset_tasks(tasks)
done = 0
for t in tasks:
    for path in t.get_editor_property("imported_object_paths"):
        tex = unreal.load_asset(path)
        if not isinstance(tex, unreal.Texture2D):
            continue
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("never_stream", True)
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
        tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
        EAL.save_asset(path.split(".")[0], only_if_is_dirty=False)
        done += 1
unreal.log("make_colony_assets: imported %d textures" % done)
