# The Ant Kitchen soundtrack, generated (Tools/audio/synth.py). Writes SourceArt/Audio/Music/*.wav:
#   M_<Set>_Base / _Tension / _Action / _Urgency : synchronized stems of one seamless loop (raid sets Funk, Lounge)
#   M_Nest : the colony march (nest / menu), one seamless loop
#   M_Sting_* : one-shots (win, fail, card, raid start, last light) and UI sounds (A_DeliverChime, A_CardPick, A_TrailCut)
# Usage: python make_music.py <project dir> [set ...]
import sys, os, json, subprocess, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from synth import *
import sfxlib

PROJ = sys.argv[1]
OUT = os.path.join(PROJ, "SourceArt", "Audio", "Music")
SFX = os.path.join(PROJ, "SourceArt", "Audio", "Sfx")
os.makedirs(OUT, exist_ok=True)

def ch(*names):
    return [midi(n) for n in names]

class Song:
    def __init__(self, bpm, swing, bars, chords, roots, seed):
        self.bpm, self.swing, self.bars = bpm, swing, bars
        self.step = 60.0 / bpm / 4.0
        self.len = bars * 16 * self.step
        self.chords, self.roots = chords, roots   # one per bar
        self.rng = np.random.default_rng(seed)

    def t(self, bar, step, human=0.004):
        s = step
        off = (self.swing - 0.5) * 2 * self.step if (int(s) % 2 == 1) else 0.0
        return (bar * 16 + s) * self.step + off + self.rng.normal(0, human)

    def v(self, base, spread=0.1):
        return float(np.clip(base * (1 + self.rng.normal(0, spread)), 0.05, 1.3))

# ------------------------------------------------------------------ the two raid sets
def funk_song():
    # D dorian funk, 98 BPM: A A B B A A (48 bars, ~118 s)
    A = [("Dm9", "D2"), ("Dm9", "D2"), ("G13", "G2"), ("G13", "G2"), ("Dm9", "D2"), ("Dm9", "D2"), ("Bbmaj7", "Bb1"), ("A7#9", "A1")]
    B = [("Gm9", "G1"), ("C13", "C2"), ("Fmaj9", "F2"), ("Bbmaj7", "Bb1"), ("Em7b5", "E2"), ("A7#9", "A1"), ("Dm9", "D2"), ("A7#9", "A1")]
    V = {"Dm9": ch("F3", "A3", "C4", "E4"), "G13": ch("F3", "A3", "B3", "E4"), "Bbmaj7": ch("Bb3", "D4", "F4", "A4"),
         "A7#9": ch("C#4", "G4", "C5"), "Gm9": ch("Bb3", "D4", "F4", "A4"), "C13": ch("Bb3", "E4", "A4"),
         "Fmaj9": ch("A3", "C4", "E4", "G4"), "Em7b5": ch("G3", "Bb3", "D4", "E4")}
    seq = A + A + B + B + A + A
    s = Song(98, 0.57, len(seq), [V[c] for c, _ in seq], [midi(r) for _, r in seq], 11)
    s.names = [c for c, _ in seq]
    s.section = ["A"] * 16 + ["B"] * 16 + ["A2"] * 16
    s.style = "funk"
    return s

def lounge_song():
    # A minor lounge / lo-fi bossa, 88 BPM: A B A B' (32 bars x ... = 40 bars, ~109 s)
    A = [("Am9", "A1"), ("D9", "D2"), ("Gmaj7", "G1"), ("Cmaj7", "C2"), ("Fmaj7", "F1"), ("Bm7b5", "B1"), ("E7#9", "E2"), ("E7#9", "E2")]
    B = [("Dm9", "D2"), ("G13", "G1"), ("Cmaj9", "C2"), ("Fmaj7", "F1"), ("Bm7b5", "B1"), ("E7#9", "E2"), ("Am9", "A1"), ("Am9", "A1")]
    V = {"Am9": ch("C4", "E4", "G4", "B4"), "D9": ch("C4", "E4", "F#4", "A4"), "Gmaj7": ch("B3", "D4", "F#4", "A4"),
         "Cmaj7": ch("B3", "E4", "G4", "C5"), "Fmaj7": ch("A3", "C4", "E4", "G4"), "Bm7b5": ch("A3", "D4", "F4"),
         "E7#9": ch("G#3", "D4", "G4"), "Dm9": ch("F3", "A3", "C4", "E4"), "G13": ch("F3", "B3", "E4"), "Cmaj9": ch("E4", "G4", "B4", "D5")}
    seq = A + B + A + B + A
    s = Song(88, 0.55, len(seq), [V[c] for c, _ in seq], [midi(r) for _, r in seq], 22)
    s.names = [c for c, _ in seq]
    s.section = ["A"] * 8 + ["B"] * 8 + ["A"] * 8 + ["B2"] * 8 + ["A2"] * 8
    s.style = "lounge"
    return s

IR_ROOM = None
IR_HALL = None

def irs():
    global IR_ROOM, IR_HALL
    if IR_ROOM is None:
        IR_ROOM = make_ir(1.1, 6000, 3)
        IR_HALL = make_ir(2.4, 4500, 5)
    return IR_ROOM, IR_HALL

# ------------------------------------------------------------------ harmony helpers
def quality(name):
    third = 3 if ("m" in name and "maj" not in name) else 4
    fifth = 6 if "b5" in name else 7
    seventh = 11 if "maj" in name else 10
    return {"R": 0, "O": 12, "3": third, "5": fifth, "7": seventh, "4": 5, "9": 14, "L5": fifth - 12}

def bass_line(song, bar, pattern):
    q = quality(song.names[bar])
    root = song.roots[bar]
    nxt = song.roots[(bar + 1) % song.bars]
    out = []
    for step, sym, ln, vel in pattern:
        if sym == "A":            # chromatic approach into the next bar's root
            m = nxt - 1 if nxt > root else nxt + 1
        else:
            m = root + q[sym]
        out.append((step, m, ln, vel))
    return out

FUNK_BASS = [
    [(0, "R", 3, 1.0), (3, "O", 1, 0.65), (6, "R", 2, 0.85), (8, "5", 1, 0.6), (10, "7", 2, 0.8), (13, "O", 1, 0.6), (14, "R", 1, 0.7)],
    [(0, "R", 2, 1.0), (2, "R", 1, 0.45), (4, "O", 1, 0.7), (6, "5", 2, 0.8), (9, "R", 1, 0.6), (10, "3", 2, 0.75), (12, "4", 1, 0.6), (14, "5", 1, 0.7), (15, "A", 1, 0.65)],
]
LOUNGE_BASS = [
    [(0, "R", 5, 0.9), (6, "5", 2, 0.7), (8, "5", 5, 0.75), (14, "R", 2, 0.6)],
    [(0, "R", 5, 0.9), (6, "5", 2, 0.7), (8, "O", 4, 0.7), (12, "5", 2, 0.6), (15, "A", 1, 0.55)],
]

def strum(tr, song, t, notes, inst, dur, vel, gain, send, spread=0.008, pan=0.0):
    for k, m in enumerate(notes):
        tr.add(t + k * spread, inst(hz(m), dur, song.v(vel)), gain, pan + (k - len(notes) / 2) * 0.12, send)

# ------------------------------------------------------------------ stems
def stem_base(song):
    tr = Track(song.len)
    st = song.step
    for b in range(song.bars):
        sec = song.section[b]
        last = (b % 8 == 7)
        if song.style == "funk":
            kicks = [0, 6, 10, 14] if last else ([0, 10] if b % 2 == 0 else [0, 7, 10])
            for s in kicks:
                tr.add(song.t(b, s), kick(song.v(0.95 if s == 0 else 0.75)), 0.9, 0, 0.02)
            for s in (4, 12):
                tr.add(song.t(b, s), snare(song.v(0.9)), 0.75, 0.05, 0.15)
            for s in (7, 9, 15):
                if song.rng.random() < 0.6:
                    tr.add(song.t(b, s), snare(song.v(0.18, 0.3), tight=0.6), 0.6, 0.05, 0.1)
            for s in range(16):
                if s == 14 and b % 2 == 1:
                    tr.add(song.t(b, s), hat(song.v(0.5), True), 0.6, -0.3, 0.05)
                elif not (s == 15 and b % 2 == 1):
                    tr.add(song.t(b, s, 0.003), hat(song.v(0.55 if s % 2 == 0 else 0.28)), 0.6, -0.3, 0.03)
            for step, m, ln, vel in bass_line(song, b, FUNK_BASS[b % 2]):
                tr.add(song.t(b, step), bass(hz(m), ln * st * 0.9, song.v(vel)), 0.85, 0, 0.0)
            comp = [(0, 5, 0.7), (6, 1, 0.5), (10, 2, 0.55), (14, 2, 0.45)] if b % 4 != 3 else [(0, 3, 0.65), (4, 1, 0.45), (8, 6, 0.6)]
            if sec == "A2":
                comp = comp + [(3, 1, 0.35)]
            for s, ln, vel in comp:
                strum(tr, song, song.t(b, s), song.chords[b], rhodes, ln * st, vel, 0.55, 0.35, 0.006, 0.1)
            if sec == "B" and b % 2 == 0:
                tones = sorted(song.chords[b]) + [m + 12 for m in sorted(song.chords[b])]
                motif = [(0, 6, 2), (3, 5, 1), (6, 4, 2), (10, 5, 4)] if b % 4 == 0 else [(0, 7, 2), (2, 6, 1), (4, 5, 2), (8, 3, 6)]
                for s, k, ln in motif:
                    tr.add(song.t(b, s), vibes(hz(tones[min(k, len(tones) - 1)] + 12), ln * st, song.v(0.7)), 0.5, 0.35, 0.4)
        else:   # lounge
            for s in ([0, 8] if b % 2 == 0 else [0, 6, 8]):
                tr.add(song.t(b, s), kick(song.v(0.6), 0.6), 0.8, 0, 0.03)
            clave = [0, 3, 6, 10, 12] if b % 2 == 0 else [2, 4, 8, 11, 14]
            for s in clave:
                tr.add(song.t(b, s), rim(song.v(0.6)), 0.55, 0.25, 0.15)
            for s in (4, 12):
                tr.add(song.t(b, s), brush(song.v(0.8)), 0.8, -0.1, 0.2)
            for s in range(0, 16, 2):
                tr.add(song.t(b, s), shaker(song.v(0.5 if s % 4 == 0 else 0.35)), 0.6, -0.35, 0.1)
            for step, m, ln, vel in bass_line(song, b, LOUNGE_BASS[b % 2]):
                tr.add(song.t(b, step), bass(hz(m), ln * st * 0.95, song.v(vel), bright=500), 0.95, 0, 0.0)
            strum(tr, song, song.t(b, 0), song.chords[b], rhodes, 10 * st, 0.6, 0.6, 0.45, 0.02, 0.0)
            if b % 2 == 1:
                strum(tr, song, song.t(b, 10), song.chords[b], rhodes, 5 * st, 0.4, 0.5, 0.45, 0.015, 0.0)
            if sec in ("B", "B2"):
                tones = sorted(song.chords[b]) + [m + 12 for m in sorted(song.chords[b])]
                motif = [(2, 4, 3), (6, 5, 2), (8, 6, 6)] if b % 2 == 0 else [(0, 5, 2), (3, 4, 2), (6, 3, 8)]
                for s, k, ln in motif:
                    tr.add(song.t(b, s), vibes(hz(tones[min(k, len(tones) - 1)] + 12), ln * st, song.v(0.6)), 0.55, -0.3, 0.5)
    room, hall = irs()
    out = tr.render(hall)
    if song.style == "lounge":
        out = lofi(out)
    return out

def lofi(x):
    """Gentle lo-fi: a soft top end and a bed of vinyl crackle."""
    pre = SR   # filter circularly: the loop's own end warms the filter up, so the seam stays clean
    y = filt(np.concatenate([x[-pre:], x]), "lowpass", 9000)[pre:]
    n = len(y)
    crack = np.zeros(n)
    rng = np.random.default_rng(9)
    idx = rng.integers(0, n, n // 2400)
    crack[idx] = rng.normal(0, 0.25, len(idx))
    crack = filt(crack, "highpass", 1500)
    hiss = filt(rng.normal(0, 1, n), "lowpass", 5000) * 0.0025
    return y + (crack * 0.5 + hiss)[:, None]

def stem_tension(song):
    tr = Track(song.len)
    st = song.step
    for b in range(song.bars):
        root = song.roots[b]
        for s in range(16):
            tr.add(song.t(b, s, 0.002), shaker(song.v(0.55 if s % 4 == 2 else 0.32)), 0.7, 0.4, 0.05)
        for s in range(0, 16, 2):
            tr.add(song.t(b, s, 0.001), bass(hz(root + 12), st * 1.2, song.v(0.5, 0.05), bright=380), 0.55, -0.15, 0.1)
        if b % 2 == 0:
            low = sorted(song.chords[b])
            for m in low[:3]:
                tr.add(song.t(b, 0, 0), pad(hz(m - 12), 32 * st * 0.95, 0.9, 900), 0.9, 0, 0.6)
        for s in (2, 6, 10, 14):
            tr.add(song.t(b, s), rim(song.v(0.35)), 0.4, -0.4, 0.2)
        if b % 2 == 1:
            for s, f in ((12, 110), (14, 82)):
                tr.add(song.t(b, s), tom(f, song.v(0.6)), 0.6, 0.2, 0.25)
        if b % 4 == 3:
            tr.add(song.t(b, 0, 0), noise_riser(16 * st, 0.8), 0.8, 0, 0.4)
    room, hall = irs()
    return tr.render(hall)

def stem_action(song):
    tr = Track(song.len)
    st = song.step
    kicks = []
    for b in range(song.bars):
        for s in (0, 4, 8, 12):
            t = song.t(b, s, 0.001)
            kicks.append(t)
            tr.add(t, kick(song.v(1.0, 0.05), 1.25), 0.85, 0, 0.03)
        for s in (4, 12):
            tr.add(song.t(b, s), clap(song.v(0.9)), 0.7, 0.1, 0.25)
        if b % 4 == 0:
            tr.add(song.t(b, 0, 0), crash(song.v(0.9)), 0.7, -0.4, 0.2)
        for s in (0, 3, 4, 7, 8, 10, 11, 14):
            tones = sorted(song.chords[b])
            tr.add(song.t(b, s), clav(hz(tones[s % len(tones)] + 12), st * 0.8, song.v(0.7 if s % 4 == 0 else 0.5)), 0.6, 0.45, 0.08)
        if b % 2 == 0:
            for s, ln in ((0, 2), (3, 1), (10, 3)):
                strum(tr, song, song.t(b, s), [m + 12 for m in song.chords[b]], brass, ln * st, 0.95, 0.75, 0.3, 0.004, -0.2)
        pat = FUNK_BASS[b % 2] if song.style == "funk" else LOUNGE_BASS[b % 2]
        for step, m, ln, vel in bass_line(song, b, pat):
            tr.add(song.t(b, step), bass(hz(m + 12), ln * st * 0.85, song.v(vel), bright=2600), 0.4, 0.3, 0.05)
        if b % 8 == 7:
            for k, s in enumerate((12, 13, 14, 15)):
                tr.add(song.t(b, s), tom((196, 165, 130, 98)[k], song.v(0.85)), 0.7, 0.3 - k * 0.2, 0.2)
    room, hall = irs()
    out = tr.render(room)
    return out * duck(len(out), kicks, 0.2, 0.1)[:, None]

def stem_urgency(song):
    tr = Track(song.len)
    st = song.step
    for b in range(song.bars):
        for s in range(0, 16, 2):
            tr.add(song.t(b, s, 0.0005), woodblock(song.v(0.8 if s % 4 == 0 else 0.55), 1250 if s % 4 == 0 else 980), 0.6, 0.5 if s % 4 else -0.5, 0.15)
        for s in (0, 8):
            tr.add(song.t(b, s), timpani(hz(song.roots[b] + 12), song.v(0.8)), 0.75, 0, 0.3)
        top = sorted(song.chords[b])[-2:]
        for m in top:
            n = int(16 * st * SR)
            tt = np.arange(n) / SR
            note = pad(hz(m + 12), 16 * st, 0.9, 3000)[:n] * (0.55 + 0.45 * np.sin(2 * np.pi * (song.bpm / 60 * 2) * tt))
            tr.add(song.t(b, 0, 0), note, 0.8, 0, 0.5)
        if b % 2 == 1:
            tr.add(song.t(b - 1, 0, 0) if b > 0 else 0.0, noise_riser(32 * st, 1.0), 0.8, 0, 0.3)
    room, hall = irs()
    return tr.render(hall)

# ------------------------------------------------------------------ the nest: «Марш Великой Колонии Щели» (a funky colony march, Bb, 100 BPM)
NEST_MEL = [  # (bar, step, note, steps)
    (0, 0, "F4", 4), (0, 4, "Bb4", 4), (0, 8, "D5", 6), (0, 14, "C5", 2),
    (1, 0, "Bb4", 4), (1, 4, "A4", 2), (1, 6, "G4", 2), (1, 8, "F4", 8),
    (2, 0, "G4", 4), (2, 4, "A4", 4), (2, 8, "Bb4", 4), (2, 12, "D5", 4),
    (3, 0, "C5", 12),
    (4, 0, "F4", 4), (4, 4, "Bb4", 4), (4, 8, "D5", 6), (4, 14, "F5", 2),
    (5, 0, "Eb5", 4), (5, 4, "D5", 2), (5, 6, "C5", 2), (5, 8, "Bb4", 4), (5, 12, "G4", 4),
    (6, 0, "F4", 6), (6, 6, "A4", 2), (6, 8, "C5", 4), (6, 12, "Eb5", 4),
    (7, 0, "Bb4", 12),
]
NEST_CH = [("Bb", "Bb1", ch("D3", "F3", "Bb3")), ("F7", "F1", ch("Eb3", "A3", "C4")), ("Eb", "Eb2", ch("Eb3", "G3", "Bb3")), ("F", "F1", ch("F3", "A3", "C4")),
           ("Bb", "Bb1", ch("D3", "F3", "Bb3")), ("Eb", "Eb2", ch("Eb3", "G3", "Bb3")), ("F7", "F1", ch("Eb3", "A3", "C4")), ("Bb", "Bb1", ch("D3", "F3", "Bb3"))]

def nest_song():
    seq = NEST_CH * 4
    s = Song(100, 0.54, len(seq), [c for _, _, c in seq], [midi(r) for _, r, _ in seq], 33)
    s.names = [n for n, _, _ in seq]
    s.style = "nest"
    return s

def stem_nest(song):
    tr = Track(song.len)
    st = song.step
    for b in range(song.bars):
        part = b // 8          # 0 whistle, 1 brass, 2 groove (no melody), 3 whistle + glock
        # march snare: the drill pattern with rolls into every 4th bar
        for s in (0, 4, 6, 8, 12, 14) if b % 2 == 0 else (0, 3, 4, 8, 11, 12):
            tr.add(song.t(b, s), snare(song.v(0.55 if s % 4 else 0.8), 230, 0.55), 0.55, 0.1, 0.12)
        if b % 4 == 3:
            for s in range(8, 16):
                tr.add(song.t(b, s), snare(song.v(0.3 + 0.04 * s), 230, 0.4), 0.5, 0.1, 0.12)
        for s in (0, 8):
            tr.add(song.t(b, s), kick(song.v(0.8), 0.8), 0.8, 0, 0.03)
        for s in range(0, 16, 2):
            tr.add(song.t(b, s, 0.002), hat(song.v(0.4 if s % 4 == 0 else 0.25)), 0.45, -0.3, 0.03)
        # tuba oom-pah (with a funky pickup) and brass 'pah' on the off beats
        q = quality(song.names[b])
        r = song.roots[b]
        for s, sym in ((0, "R"), (8, "5"), (14, "O") if b % 2 else (12, "5")):
            tr.add(song.t(b, s), tuba(hz(r + q[sym] + 12), 2.5 * st, song.v(0.9)), 0.9, 0, 0.05)
        for s in (4, 12):
            strum(tr, song, song.t(b, s), song.chords[b], brass, 1.5 * st, 0.6, 0.55, 0.2, 0.003, 0.15)
        # the tune
        for mb, ms, nn, ln in NEST_MEL:
            if mb == b % 8:
                if part in (0, 3):
                    tr.add(song.t(b, ms), whistle(hz(nn), ln * st * 0.92, song.v(0.85)), 0.6, -0.15, 0.35)
                if part == 3:
                    tr.add(song.t(b, ms), glock(hz(midi(nn) + 12), ln * st, song.v(0.6)), 0.35, 0.3, 0.35)
                if part == 1:
                    strum(tr, song, song.t(b, ms), [midi(nn) - 12, midi(nn)], brass, ln * st * 0.9, 0.85, 0.6, 0.3, 0.002, -0.1)
        if part == 2 and b % 2 == 0:
            strum(tr, song, song.t(b, 0), [m + 12 for m in song.chords[b]], rhodes, 6 * st, 0.6, 0.5, 0.35, 0.01, 0.0)
            strum(tr, song, song.t(b, 10), [m + 12 for m in song.chords[b]], rhodes, 3 * st, 0.5, 0.5, 0.35, 0.01, 0.0)
    room, hall = irs()
    return tr.render(hall)

# ------------------------------------------------------------------ stings and UI one-shots (not loops: rendered with a tail)
def one_shot(seconds, fn):
    tr = Track(seconds + 3.0)
    fn(tr)
    room, hall = irs()
    out = tr.render(hall)
    n = int(seconds * SR)
    out = out[: n + int(2.5 * SR)]
    k = int(0.5 * SR)
    out[-k:] *= np.linspace(1, 0, k)[:, None]
    return out

def sting_win(tr):
    for k, (t, chord) in enumerate(((0.0, ("Bb3", "D4", "F4")), (0.28, ("Eb4", "G4", "Bb4")), (0.56, ("F4", "A4", "C5")), (0.84, ("Bb4", "D5", "F5", "Bb5")))):
        ln = 0.22 if k < 3 else 1.6
        for m in chord:
            tr.add(t, brass(hz(m), ln, 1.0), 0.8, 0, 0.35)
        tr.add(t, timpani(hz("Bb2") if k != 2 else hz("F2"), 0.9), 0.6, 0, 0.3)
    for i in range(10):
        tr.add(0.84 - 0.25 + i * 0.025, snare(0.3 + 0.05 * i, 230, 0.4), 0.4, 0.1, 0.2)
    tr.add(0.84, crash(1.0, 2.5), 0.8, -0.3, 0.3)
    for k, m in enumerate(("Bb5", "D6", "F6", "Bb6")):
        tr.add(0.84 + k * 0.07, glock(hz(m), 0.5, 0.8), 0.4, 0.3, 0.4)

def sting_fail(tr):
    notes = (("B3", "B3", 0.35), ("Bb3", "Bb3", 0.35), ("A3", "A3", 0.35), ("Ab3", "G3", 1.4))
    t = 0.0
    for a, b_, d in notes:
        tr.add(t, trombone(hz(a), hz(b_), d, 1.0), 0.9, 0, 0.3)
        t += d + 0.08
    tr.add(t - 1.45, timpani(hz("G2"), 0.7), 0.5, 0, 0.3)

def sting_card(tr):
    for k, m in enumerate(("D5", "F5", "A5", "C6", "E6")):
        tr.add(k * 0.045, glock(hz(m), 0.4, 0.8 - k * 0.08), 0.6, -0.4 + k * 0.2, 0.5)
    tr.add(0.0, noise_riser(0.25, 0.6)[::-1].copy(), 0.6, 0, 0.3)

def sting_raid(tr):
    # "Вперёд!": a snare fill, then a brass hit on the downbeat
    for k in range(8):
        tr.add(k * 0.076, snare(0.4 + 0.07 * k, 200, 0.5), 0.6, 0.1, 0.2)
        if k % 2 == 0:
            tr.add(k * 0.076, tom((196, 165, 130, 98)[k // 2], 0.8), 0.5, 0.2 - k * 0.1, 0.2)
    for m in ("D3", "A3", "D4", "F4", "C5"):
        tr.add(0.61, brass(hz(m), 0.5, 1.0), 0.8, 0, 0.3)
    tr.add(0.61, kick(1.0, 1.3), 0.9, 0, 0.0)
    tr.add(0.61, crash(0.9, 2.0), 0.7, -0.3, 0.3)

def sting_last(tr):
    tr.add(0.0, noise_riser(1.2, 1.2), 1.0, 0, 0.3)
    for m in ("D2", "D3", "Ab3", "C4", "Eb4"):
        tr.add(1.2, brass(hz(m), 0.9, 1.0), 0.85, 0, 0.35)
    tr.add(1.2, timpani(hz("D2"), 1.0), 0.8, 0, 0.3)
    tr.add(1.2, crash(1.0, 2.5), 0.6, 0.3, 0.3)
    for k in range(4):
        tr.add(1.2 + 0.18 * k, woodblock(0.9, 1250), 0.6, 0.4, 0.2)

def ui_deliver(tr):
    for k, m in enumerate(("G5", "B5", "D6", "G6")):
        tr.add(k * 0.06, glock(hz(m), 0.3, 0.9), 0.7, -0.3 + k * 0.2, 0.3)

def ui_cardpick(tr):
    for k, m in enumerate(("C6", "G6")):
        tr.add(k * 0.05, glock(hz(m), 0.2, 0.9), 0.7, 0, 0.3)
    tr.add(0.0, clav(hz("C5"), 0.05, 1.0), 0.5, 0, 0.1)

def ui_trailcut(tr):
    n = int(0.25 * SR)
    t = np.arange(n) / SR
    f = 1800 * np.exp(-t / 0.05) + 300
    s = np.sin(phase(f, n)) * np.exp(-t / 0.06) * 0.5
    snip = filt(RNG.normal(0, 1, n), "highpass", 5000) * np.exp(-t / 0.01) * 0.4
    tr.add(0.0, s + snip, 0.8, 0, 0.15)

# ------------------------------------------------------------------ loudness and output
def lufs(x):
    tmp = os.path.join(PROJ, "Saved", "_lufs_tmp.wav")
    sfxlib.write_wav(tmp, np.clip(x, -1, 1))
    r = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", tmp, "-af", "ebur128=peak=true", "-f", "null", "-"], stderr=subprocess.PIPE, text=True).stderr
    os.remove(tmp)
    i = r.rfind("I:")
    return float(r[i + 2:].split("LUFS")[0])

def write_set(name, stems, target=-16.0, ref=("Base", "Tension")):
    refmix = sum(stems[k] for k in ref)
    full = sum(stems.values())
    g = 10 ** ((target - lufs(refmix)) / 20)
    pk = np.abs(full).max() * g
    if pk > 0.94:
        g *= 0.94 / pk
    info = {}
    for k, x in stems.items():
        y = soft_clip(x * g, 0.98)
        fn = os.path.join(OUT, "M_%s_%s.wav" % (name, k) if k else "M_%s.wav" % name)
        sfxlib.write_wav(fn, y)
        info[k or name] = round(len(y) / SR, 3)
    info["lufs_ref"] = round(lufs(refmix * g), 1)
    info["lufs_full"] = round(lufs(full * g), 1)
    info["peak_full"] = round(float(20 * np.log10(np.abs(full).max() * g + 1e-9)), 1)
    print(name, info)
    return info

def write_one(path, x, target):
    g = 10 ** ((target - lufs(x)) / 20)
    pk = np.abs(x).max() * g
    if pk > 0.89:
        g *= 0.89 / pk
    sfxlib.write_wav(path, x * g)
    print(os.path.basename(path), round(len(x) / SR, 2), "s")

if __name__ == "__main__":
    want = set(sys.argv[2:]) or {"funk", "lounge", "nest", "stings"}
    report = {}
    for key, mk in (("funk", funk_song), ("lounge", lounge_song)):
        if key in want:
            s = mk()
            stems = {"Base": stem_base(s), "Tension": stem_tension(s), "Action": stem_action(s), "Urgency": stem_urgency(s)}
            report[key] = write_set(key.capitalize(), stems)
            report[key]["bpm"] = s.bpm
            report[key]["bars"] = s.bars
    if "nest" in want:
        s = nest_song()
        report["nest"] = write_set("Nest", {"": stem_nest(s)}, -17.0, ("",))
    if "stings" in want:
        for nm, fn, secs, tgt in (("Win", sting_win, 2.6, -14), ("Fail", sting_fail, 2.4, -15), ("Card", sting_card, 0.6, -18),
                                  ("RaidStart", sting_raid, 1.3, -14), ("LastLight", sting_last, 2.2, -14)):
            write_one(os.path.join(OUT, "M_Sting_%s.wav" % nm), one_shot(secs, fn), tgt)
        for nm, fn, secs, tgt in (("DeliverChime", ui_deliver, 0.4, -20), ("CardPick", ui_cardpick, 0.2, -22), ("TrailCut", ui_trailcut, 0.25, -22)):
            x = one_shot(secs, fn).mean(axis=1)
            x = x[: int((secs + 0.8) * SR)]
            x[-int(0.2 * SR):] *= np.linspace(1, 0, int(0.2 * SR))
            write_one(os.path.join(SFX, "A_%s_1.wav" % nm), x, tgt)
    with open(os.path.join(PROJ, "Saved", "music_report.json"), "w") as f:
        json.dump(report, f, indent=1)
