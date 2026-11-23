# Imports SourceArt/Audio/SfxChains/A_*.wav (Tools/chains/make_chain_sfx.py) into /Game/Audio/Sfx with the settings every effect has
# (AntAudio.cpp: PowerDown, PowerUp, BreakerClick, MicroBeep, FlourWhoosh, SlingTwang, StaticCrackle, IceClink).
# Run:  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<path>/import_chain_sfx.py" -unattended -nosplash -nullrhi
import unreal, os, glob
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
files = sorted(glob.glob(os.path.join(PROJ, "SourceArt", "Audio", "SfxChains", "*.wav")))
tasks = []
for f in files:
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f)
    t.set_editor_property("destination_path", "/Game/Audio/Sfx")
    t.set_editor_property("destination_name", os.path.splitext(os.path.basename(f))[0])
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", False)
    tasks.append(t)
AT.import_asset_tasks(tasks)
cls = unreal.load_asset("/Game/Audio/Mix/SC_Sfx")
for f in files:
    path = "/Game/Audio/Sfx/" + os.path.splitext(os.path.basename(f))[0]
    w = unreal.load_asset(path)
    if not w:
        unreal.log("ANTAUDIO FAILED " + path)
        continue
    if cls:
        w.set_editor_property("sound_class_object", cls)
    w.set_editor_property("compression_quality", 70)
    EAL.save_asset(path)
    unreal.log("ANTAUDIO imported " + path)
unreal.log("ANTAUDIO DONE")
