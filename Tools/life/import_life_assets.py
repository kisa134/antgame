# Imports the living flat's prints and sounds (Tools/life/make_life_assets.py) into /Game/Life/Textures and /Game/Life/Audio.
# Run: UnrealEditor-Cmd.exe AntKitchen.uproject -run=pythonscript -script=Tools/life/import_life_assets.py
import unreal, os, glob
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary

def imp(f, dest):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("destination_name", os.path.splitext(os.path.basename(f))[0])
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", False)
    AT.import_asset_tasks([t])
    return unreal.load_asset(dest + "/" + os.path.splitext(os.path.basename(f))[0])

n = 0
for f in sorted(glob.glob(os.path.join(PROJ, "SourceArt", "Life", "tex", "T_Life_*.png"))):
    a = imp(f, "/Game/Life/Textures")
    if not a: unreal.log_warning("LIFEIMP failed " + f); continue
    a.set_editor_property("srgb", True)
    a.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    EAL.save_asset(a.get_path_name()); n += 1
unreal.log("LIFEIMP textures %d" % n)

amb = unreal.load_asset("/Game/Audio/Mix/SC_Ambience")
sfx = unreal.load_asset("/Game/Audio/Mix/SC_Sfx")
LOOPS = ("A_Life_FlyBuzz", "A_Life_Rain", "A_Life_Wash", "A_Life_KettleBoil", "A_Life_MusicBox", "A_Life_Radio")
m = 0
for f in sorted(glob.glob(os.path.join(PROJ, "SourceArt", "Life", "audio", "A_Life_*.wav"))):
    w = imp(f, "/Game/Life/Audio")
    if not w: unreal.log_warning("LIFEIMP failed " + f); continue
    name = os.path.splitext(os.path.basename(f))[0]
    loop = name in LOOPS
    try:
        w.set_editor_property("sound_class_object", amb if loop else sfx)
        w.set_editor_property("looping", loop)
        w.set_editor_property("compression_quality", 60)
        if loop: w.set_editor_property("virtualization_mode", unreal.VirtualizationMode.PLAY_WHEN_SILENT)
    except Exception as e:
        unreal.log_warning("LIFEIMP %s: %s" % (name, e))
    EAL.save_asset(w.get_path_name()); m += 1
unreal.log("LIFEIMP sounds %d" % m)
unreal.log("LIFEIMP_DONE")
