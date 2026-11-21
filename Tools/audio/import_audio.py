# Imports the generated audio (SourceArt/Audio/Sfx, SourceArt/Audio/Music) into /Game/Audio and builds the mix assets.
# Run:  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<path>/import_audio.py" -unattended -nosplash -nullrhi
#   SoundClasses /Game/Audio/Mix/SC_Master > SC_Music, SC_Sfx, SC_Voice, SC_Ui, SC_Ambience (every new wave gets its class)
#   SoundMix /Game/Audio/Mix/SM_AntDuck: music -55%, ambience -40% while pushed (AntAudio::Duck, stings)
#   Loops (music stems, room tones, the fridge, the swarm, the purr) are set looping; music stems are force-inline and play when silent,
#   so the four stems of a set start together and never drift.
import unreal, os, glob

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MIX = "/Game/Audio/Mix"

def log(s):
    unreal.log("ANTAUDIO " + s)

def try_set(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as e:
        log("could not set %s.%s: %s" % (obj.get_name(), prop, e))
        return False

def make_class(name):
    path = MIX + "/" + name
    if EAL.does_asset_exist(path):
        return unreal.load_asset(path)
    c = AT.create_asset(name, MIX, unreal.SoundClass, unreal.SoundClassFactory())
    EAL.save_asset(path)
    return c

master = make_class("SC_Master")
classes = {k: make_class("SC_" + k) for k in ("Music", "Sfx", "Voice", "Ui", "Ambience")}
try_set(master, "child_classes", list(classes.values()))
EAL.save_asset(MIX + "/SC_Master")
for k in classes:
    EAL.save_asset(MIX + "/SC_" + k)

duck_path = MIX + "/SM_AntDuck"
duck = unreal.load_asset(duck_path) if EAL.does_asset_exist(duck_path) else AT.create_asset("SM_AntDuck", MIX, unreal.SoundMix, unreal.SoundMixFactory())
adj = []
for k, v in (("Music", 0.45), ("Ambience", 0.6)):
    a = unreal.SoundClassAdjuster()
    try_set(a, "sound_class_object", classes[k])
    try_set(a, "volume_adjuster", v)
    try_set(a, "apply_to_children", True)
    adj.append(a)
try_set(duck, "sound_class_effects", adj)
try_set(duck, "fade_in_time", 0.15)
try_set(duck, "fade_out_time", 0.8)
EAL.save_asset(duck_path)

VOICE = ("HumanGrumble", "HumanRoar", "HumanSniff", "CatPurr", "CatHiss", "DogSnort", "DogBark", "DogSniff", "KidGiggle", "KidShout", "DeliverCheer", "AntHurt")
UI = ("CardPick", "CardFlip", "DeliverChime")
LOOPS = ("A_AmbFridge_1", "A_AmbClock_1", "A_AmbDrip_1", "A_AmbDay_1", "A_AmbEvening_1", "A_AmbNight_1", "A_WorkerPatter_1", "A_CatPurr_1")

def import_dir(src, dst):
    files = sorted(glob.glob(os.path.join(PROJ, "SourceArt", "Audio", src, "*.wav")))
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", f)
        t.set_editor_property("destination_path", dst)
        t.set_editor_property("destination_name", os.path.splitext(os.path.basename(f))[0])
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", False)
        tasks.append(t)
    AT.import_asset_tasks(tasks)
    return [os.path.splitext(os.path.basename(f))[0] for f in files]

n = 0
for name in import_dir("Sfx", "/Game/Audio/Sfx"):
    path = "/Game/Audio/Sfx/" + name
    w = unreal.load_asset(path)
    if not w:
        log("FAILED " + path)
        continue
    core = name[2:].rsplit("_", 1)[0]
    cls = "Ambience" if core.startswith("Amb") else "Voice" if core in VOICE else "Ui" if core in UI else "Sfx"
    try_set(w, "sound_class_object", classes[cls])
    try_set(w, "looping", name in LOOPS)
    try_set(w, "compression_quality", 70)
    if name in LOOPS:
        try_set(w, "virtualization_mode", unreal.VirtualizationMode.PLAY_WHEN_SILENT)
    EAL.save_asset(path)
    n += 1
log("sfx imported %d" % n)

m = 0
for name in import_dir("Music", "/Game/Audio/Music"):
    path = "/Game/Audio/Music/" + name
    w = unreal.load_asset(path)
    if not w:
        log("FAILED " + path)
        continue
    loop = not name.startswith("M_Sting_")
    try_set(w, "sound_class_object", classes["Music"])
    try_set(w, "looping", loop)
    try_set(w, "compression_quality", 60)
    if loop:
        try_set(w, "virtualization_mode", unreal.VirtualizationMode.PLAY_WHEN_SILENT)
        try_set(w, "loading_behavior", unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
    EAL.save_asset(path)
    m += 1
log("music imported %d" % m)
log("DONE")
