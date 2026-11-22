# Imports the inside-people sound effects (Higgsfield Mirelo SFX takes, trimmed and levelled by ffmpeg into SourceArt/Audio/SfxInside)
# into /Game/Audio/Sfx with the same settings import_audio.py gives every effect (AntAudio.cpp: Sneeze, EarSlap, PocketRustle, Heartbeat).
# Run:  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<path>/import_inside_sfx.py" -unattended -nosplash -nullrhi
import unreal, os, glob

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
VOICE = ("Sneeze",)

files = sorted(glob.glob(os.path.join(PROJ, "SourceArt", "Audio", "SfxInside", "*.wav")))
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
for f in files:
    name = os.path.splitext(os.path.basename(f))[0]
    path = "/Game/Audio/Sfx/" + name
    w = unreal.load_asset(path)
    if not w:
        unreal.log("ANTAUDIO FAILED " + path)
        continue
    core = name[2:].rsplit("_", 1)[0]
    cls = unreal.load_asset("/Game/Audio/Mix/SC_" + ("Voice" if core in VOICE else "Sfx"))
    w.set_editor_property("sound_class_object", cls)
    w.set_editor_property("compression_quality", 70)
    EAL.save_asset(path)
    unreal.log("ANTAUDIO imported " + path)
unreal.log("ANTAUDIO DONE")
