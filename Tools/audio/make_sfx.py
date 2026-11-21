# Raw Higgsfield takes (Saved/AudioRaw/*.mp3) -> game-ready WAVs in SourceArt/Audio/Sfx/A_<Name>_<k>.wav.
# Takes with several hits are cut into variations, loops get a crossfaded seam, everything is levelled per category.
# Usage: python make_sfx.py <project dir>
import sys, os, glob, json, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from sfxlib import *

PROJ = sys.argv[1]
RAW = os.path.join(PROJ, "Saved", "AudioRaw")
OUT = os.path.join(PROJ, "SourceArt", "Audio", "Sfx")

# name: (raw take, mode, variations, max seconds, level dB (active RMS), extras)
#   modes: hits = cut the strongest onsets; whole = the trimmed take; loop = seamless loop; chunks = equal slices
SPEC = {
    "StepTile":     ("step_tile", "hits", 5, 0.22, -30, {"min_dist": 0.2, "hp": 300, "decay": 0.04}),
    "StepWoodN":    ("step_wood", "hits", 5, 0.22, -30, {"min_dist": 0.2, "hp": 150, "decay": 0.045}),
    "StepRug":      ("step_rug", "hits", 5, 0.25, -33, {"min_dist": 0.2, "hp": 150, "decay": 0.06}),
    "StepMetalN":   ("step_metal", "hits", 5, 0.22, -30, {"min_dist": 0.2, "hp": 400, "decay": 0.04}),
    "StepSticky":   ("step_sticky", "hits", 5, 0.35, -28, {"min_dist": 0.25, "decay": 0.1}),
    "BiteN":        ("bite", "hits", 5, 0.35, -16, {"min_dist": 0.25}),
    "SpitN":        ("spit", "hits", 4, 0.6, -18, {"min_dist": 0.5}),
    "AcidHit":      ("acid_sizzle", "chunks", 3, 1.3, -20, {}),
    "GrabN":        ("grab", "hits", 4, 0.4, -22, {"min_dist": 0.4}),
    "Release":      ("release", "hits", 4, 0.4, -20, {"min_dist": 0.4}),
    "CarryStrain":  ("carry_strain", "chunks", 3, 2.0, -22, {}),
    "CarryThump":   ("carry_thump", "hits", 4, 0.6, -15, {"min_dist": 0.5, "shelf": (150, 6), "sub": (0.5, 0.15)}),
    "RipOff":       ("rip_tumble", "whole", 1, 4.0, -18, {}),
    "DeliverCheer": ("crowd_cheer", "whole", 1, 4.0, -20, {}),
    "TrailLay":     ("trail_lay", "hits", 3, 0.9, -24, {"min_dist": 0.6}),
    "WorkerPatter": ("worker_patter", "loop", 1, 8.0, -26, {"xf": 1.0}),
    "ScrewTurn":    ("screw_unscrew", "chunks", 3, 1.6, -20, {}),
    "ScrewDrop":    ("screw_drop", "hits", 3, 1.2, -18, {"min_dist": 0.9}),
    "HingeCreak":   ("hinge_creak", "hits", 3, 1.8, -20, {"min_dist": 1.2}),
    "Wobble":       ("wobble_creak", "whole", 1, 4.0, -18, {}),
    "CrashWood":    ("tip_wood", "whole", 1, 4.0, -12, {"shelf": (120, 5), "sub": (0.6, 0.5)}),
    "CrashGlass":   ("crash_glass", "whole", 1, 3.0, -13, {}),
    "CrashCeramic": ("crash_ceramic", "whole", 1, 3.0, -13, {}),
    "CrashMetal":   ("crash_metal", "whole", 1, 3.0, -13, {}),
    "CrashPlastic": ("crash_plastic", "whole", 1, 3.0, -15, {}),
    "SplatJam":     ("splat_jam", "hits", 3, 1.0, -17, {"min_dist": 0.8}),
    "SplatCream":   ("splat_cream", "hits", 3, 1.0, -17, {"min_dist": 0.8}),
    "ColaFizz":     ("cola_fizz", "whole", 1, 4.0, -22, {}),
    "Crumbs":       ("crumbs", "whole", 1, 3.0, -22, {}),
    "HumanStepN":   ("human_step", "hits", 4, 1.0, -12, {"min_dist": 0.8, "shelf": (110, 7), "sub": (0.9, 0.35)}),
    "SlipperSlap":  ("slipper_slap", "hits", 3, 0.6, -13, {"min_dist": 0.9, "shelf": (140, 5)}),
    "HumanGrumble": ("human_grumble", "chunks", 2, 2.5, -18, {}),
    "HumanRoar":    ("human_roar", "hits", 2, 2.0, -14, {"min_dist": 1.4}),
    "HumanSlip":    ("human_slipfall", "whole", 1, 4.0, -14, {"shelf": (120, 4)}),
    "BatWhoosh":    ("bat_whoosh", "hits", 3, 0.9, -16, {"min_dist": 0.9}),
    "BatImpact":    ("bat_impact", "hits", 2, 1.5, -12, {"min_dist": 1.2, "shelf": (130, 4)}),
    "HumanPickup":  ("human_pickup", "hits", 2, 1.2, -20, {"min_dist": 1.0}),
    "HumanSniff":   ("human_sniff", "whole", 1, 4.0, -20, {}),
    "CatPurr":      ("cat_purr", "loop", 1, 8.0, -24, {"xf": 1.0}),
    "CatHiss":      ("cat_hiss", "hits", 2, 1.6, -16, {"min_dist": 1.2}),
    "CatPounce":    ("cat_pounce", "whole", 1, 3.0, -16, {}),
    "DogSnort":     ("dog_snort", "chunks", 2, 2.5, -20, {}),
    "DogBark":      ("dog_bark", "hits", 3, 0.8, -14, {"min_dist": 0.7}),
    "DogSniff":     ("dog_sniff", "chunks", 2, 2.0, -22, {}),
    "DogEat":       ("dog_eat", "chunks", 2, 2.0, -20, {}),
    "Broom":        ("broom", "hits", 3, 1.2, -18, {"min_dist": 0.9}),
    "SwatterN":     ("swatter", "hits", 2, 1.5, -15, {"min_dist": 1.2}),
    "KidGiggle":    ("kid_giggle", "hits", 2, 1.8, -18, {"min_dist": 1.3}),
    "KidShout":     ("kid_shout", "whole", 1, 3.0, -16, {}),
    "SpiderSkitter":("spider_skitter", "whole", 1, 3.0, -22, {}),
    "CardFlip":     ("card_flip", "hits", 2, 1.2, -20, {"min_dist": 1.0}),
    "CurtainRustle":("curtain_rustle", "whole", 1, 3.0, -22, {}),
    "AntHurt":      ("ant_hurt", "hits", 3, 0.6, -20, {"min_dist": 0.5}),
    "AmbFridge":    ("amb_fridge", "loop", 1, 20.0, -30, {"xf": 2.0}),
    "AmbClock":     ("@clock", "loop", 1, 8.0, -34, {}),
    "AmbDrip":      ("amb_drip", "loop", 1, 10.0, -34, {"xf": 1.0}),
    "AmbDay":       ("amb_day", "loop", 1, 30.0, -28, {"xf": 3.0}),
    "AmbEvening":   ("amb_evening", "loop", 1, 30.0, -30, {"xf": 3.0}),
    "AmbNight":     ("amb_night", "loop", 1, 30.0, -32, {"xf": 3.0}),
}

def trim_body(x, start_db=-30.0, end_db=-42.0):
    """The main body of a take: from just before it first gets loud to just after it last is."""
    e = env(x, SR, 0.01)
    pk = e.max()
    on = np.where(e > pk * 10 ** (start_db / 20))[0]
    on2 = np.where(e > pk * 10 ** (end_db / 20))[0]
    if len(on) == 0:
        return x
    s = max(0, on[0] - int(0.03 * SR))
    t = min(len(x), on2[-1] + int(0.15 * SR))
    return x[s:t]

def decay(x, tau):
    t = np.arange(len(x)) / SR
    return x * np.exp(-t / tau)

def sub_boom(n, f0=70.0, f1=38.0, tau=0.3, gain=0.5):
    """A falling sine 'boom' under an impact: the macro weight of something huge."""
    t = np.arange(n) / SR
    f = f1 + (f0 - f1) * np.exp(-t / 0.08)
    ph = 2 * np.pi * np.cumsum(f) / SR
    a = np.minimum(1.0, t / 0.004) * np.exp(-t / tau)
    return gain * np.sin(ph) * a

def synth_clock():
    """A wall clock: tick, tock (8 s loop, 1 Hz), a woody click with a faint metal ring."""
    n = 8 * SR
    y = np.zeros(n)
    rng = np.random.default_rng(7)
    for k in range(8):
        i = k * SR
        L = int(0.06 * SR)
        t = np.arange(L) / SR
        click = rng.normal(0, 1, L) * np.exp(-t / 0.004)
        f = 2900 if k % 2 == 0 else 2350
        ring = 0.25 * np.sin(2 * np.pi * f * t) * np.exp(-t / 0.015) + 0.2 * np.sin(2 * np.pi * 900 * t) * np.exp(-t / 0.01)
        y[i:i + L] += (bp_click(click) + ring) * (1.0 if k % 2 == 0 else 0.8)
    room = lp(rng.normal(0, 1, n), 300) * 0.003
    return y + room

def bp_click(x):
    return hp(lp(x, 6000), 1200)

def process(name, spec):
    src, mode, n, maxlen, target, ex = spec
    if src == "@clock":
        y = synth_clock()
        y = level(y, target, -1.0)
        fn = "A_%s_1.wav" % name
        write_wav(os.path.join(OUT, fn), y)
        return [(fn, len(y) / SR)]
    x = decode(os.path.join(RAW, src + ".mp3"))
    if "hp" in ex:
        x = hp(x, ex["hp"])
    else:
        x = hp(x, 25)
    if "shelf" in ex:
        x = lowshelf(x, *ex["shelf"])
    if mode == "hits":
        parts = cut_hits(x, n, maxlen, ex.get("min_dist", 0.2))
    elif mode == "whole":
        parts = [fade(trim_body(x)[: int(maxlen * SR)], SR, 0.003, 0.08)]
    elif mode == "chunks":
        y = trim(x, SR, -50.0)
        L = min(int(maxlen * SR), len(y) // n)
        parts = [fade(y[i * L:(i + 1) * L], SR, 0.02, 0.15) for i in range(n)]
    elif mode == "loop":
        y = x[: int(maxlen * SR)]
        if ex.get("xf", 1.0) > 0:
            y = loop_xfade(y, SR, ex["xf"])
        else:
            y = y[: len(y) // SR * SR]
        parts = [y]
    if len(parts) > 1:
        longest = max(len(p) for p in parts)
        parts = [p for p in parts if len(p) >= 0.4 * longest]
    if "decay" in ex:
        parts = [fade(decay(p, ex["decay"]), SR, 0.001, 0.02) for p in parts]
    if "sub" in ex:
        g, tau = ex["sub"]
        parts = [p + sub_boom(len(p), tau=tau, gain=g * np.abs(p).max()) for p in parts]
    out = []
    for k, p in enumerate(parts):
        p = level(p, target, -1.0)
        fn = "A_%s_%d.wav" % (name, k + 1)
        write_wav(os.path.join(OUT, fn), p)
        out.append((fn, len(p) / SR))
    return out

if __name__ == "__main__":
    only = sys.argv[2:] if len(sys.argv) > 2 else None
    report = {}
    for name, spec in SPEC.items():
        if only and name not in only:
            continue
        for f in glob.glob(os.path.join(OUT, "A_%s_*.wav" % name)):
            os.remove(f)
        r = process(name, spec)
        report[name] = r
        print("%-14s %d  %s" % (name, len(r), " ".join("%.2f" % d for _, d in r)))
    with open(os.path.join(PROJ, "Saved", "sfx_report.json"), "w") as f:
        json.dump(report, f, indent=1)
