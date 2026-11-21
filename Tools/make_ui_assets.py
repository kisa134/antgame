# UI art (branch code/ANT-ui): imports every picture under SourceArt/UI/<Folder>/ into /Game/UI/<Folder>/ as a UI texture.
# The pictures were generated with Higgsfield (docs/UI_CREDITS.md) and cut/sized by Tools/ui_art_prep.py.
# Run: UnrealEditor-Cmd.exe AntKitchen.uproject -run=pythonscript -script=Tools/make_ui_assets.py
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "SourceArt", "UI")
AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary

tasks = []
for folder in sorted(os.listdir(SRC)):
    full = os.path.join(SRC, folder)
    if not os.path.isdir(full):
        continue
    for f in sorted(os.listdir(full)):
        if not f.lower().endswith((".png", ".jpg")):
            continue
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", os.path.join(full, f))
        t.set_editor_property("destination_path", "/Game/UI/" + folder)
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
        # Tiled materials (paper, cardboard, grain) repeat; everything else is clamped so edges never bleed.
        name = path.split("/")[-1]
        tiled = any(k in name for k in ("Paper", "Cardboard", "Grain", "Scratches", "TornEdge"))
        addr = unreal.TextureAddress.TA_WRAP if tiled else unreal.TextureAddress.TA_CLAMP
        tex.set_editor_property("address_x", addr)
        tex.set_editor_property("address_y", addr)
        EAL.save_asset(path.split(".")[0], only_if_is_dirty=False)
        done += 1
unreal.log("make_ui_assets: imported %d textures" % done)
