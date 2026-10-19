# Imports SourceArt/Sounds/*.wav into /Game/Sounds as SoundWaves.
import unreal, os, glob
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
AT = unreal.AssetToolsHelpers.get_asset_tools()
for f in sorted(glob.glob(os.path.join(PROJ, "SourceArt", "Sounds", "*.wav"))):
    name = os.path.splitext(os.path.basename(f))[0]
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f)
    t.set_editor_property("destination_path", "/Game/Sounds")
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    AT.import_asset_tasks([t])
    unreal.log("SOUND imported " + name)
for loop in ("S_Spray", "S_Buzz", "S_Ambience"):
    p = "/Game/Sounds/" + loop
    if unreal.EditorAssetLibrary.does_asset_exist(p):
        s = unreal.load_asset(p)
        s.set_editor_property("looping", True)
        unreal.EditorAssetLibrary.save_asset(p)
unreal.log("SOUNDS_DONE")
