# Materials and chain reactions 2.0: the new sound effects, synthesised in-house (no samples, no credits): breaker click,
# power down / up, microwave beep, flour whoosh, slingshot twang, static crackle, ice clink. -> SourceArt/Audio/SfxChains/A_*.wav
#   python make_chain_sfx.py <project dir>
import sys, os, numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "audio"))
import sfxlib as L
SR = L.SR
OUT = os.path.join(sys.argv[1], "SourceArt", "Audio", "SfxChains")
rng = np.random.default_rng(7)
t = lambda d: np.arange(int(SR * d)) / SR

def noise(d): return rng.standard_normal(int(SR * d))
def envexp(d, k): return np.exp(-t(d) * k)
def save(name, x, db=-16.0):
    x = L.fade(x, SR, 0.002, 0.02)
    x = L.level(x, db)
    L.write_wav(os.path.join(OUT, name + ".wav"), x)
    print("SFX", name, len(x) / SR)

# breaker click: a hard snap + a short metal ring + the box's thud
for k in range(2):
    d = 0.35
    snap = noise(d) * envexp(d, 260 + 60 * k)
    ring = np.sin(2 * np.pi * (2300 + 180 * k) * t(d)) * envexp(d, 40) * 0.25
    thud = np.sin(2 * np.pi * 95 * t(d)) * envexp(d, 30) * 0.6
    save("A_BreakerClick_%d" % (k + 1), L.hp(snap, 600) + ring + thud, -14)

# power down: the hum of the flat sinking (fridge, lamps) and a click
d = 1.6
f = 100 * np.exp(-t(d) * 1.4) + 30
ph = 2 * np.pi * np.cumsum(f) / SR
hum = (np.sin(ph) + 0.4 * np.sin(2 * ph) + 0.2 * np.sin(3 * ph)) * np.clip(1 - t(d) / d, 0, 1) ** 1.5
click = np.zeros_like(hum); c = noise(0.05) * envexp(0.05, 120); click[:len(c)] = c
save("A_PowerDown_1", hum * 0.8 + L.hp(click, 800), -15)
# power up: a click, then the hum rising, a fluorescent flicker
d = 1.2
f = 40 + 60 * (1 - np.exp(-t(d) * 4))
ph = 2 * np.pi * np.cumsum(f) / SR
hum = (np.sin(ph) + 0.35 * np.sin(2 * ph)) * np.clip(t(d) / 0.3, 0, 1) * np.clip((d - t(d)) / 0.4, 0, 1)
flick = L.hp(noise(d), 3000) * (np.sin(2 * np.pi * 9 * t(d)) > 0.6) * 0.15 * np.clip(1 - t(d) / 0.8, 0, 1)
c = noise(0.05) * envexp(0.05, 120); hum[:len(c)] += L.hp(c, 800)
save("A_PowerUp_1", hum + flick, -17)
# the microwave's three beeps
d = 1.5
x = np.zeros(int(SR * d))
for i in range(3):
    s = int(SR * i * 0.45); n = int(SR * 0.22)
    tt = np.arange(n) / SR
    x[s:s + n] = np.sin(2 * np.pi * 2000 * tt) * (np.minimum(1, tt / 0.004)) * (np.minimum(1, (0.22 - tt) / 0.01)) + 0.15 * np.sin(2 * np.pi * 4000 * tt)
save("A_MicroBeep_1", x, -18)
# flour whoosh: a soft filtered noise burst that settles
for k in range(2):
    d = 1.4 + 0.3 * k
    n = noise(d)
    e = np.clip(t(d) / 0.12, 0, 1) * np.exp(-np.maximum(0, t(d) - 0.12) * 2.4)
    w = L.lp(n, 1800 - 300 * k) * e + L.lp(noise(d), 300) * e * 0.6
    save("A_FlourWhoosh_%d" % (k + 1), w, -15)
# slingshot twang: a plucked rubber band (Karplus-Strong), falling pitch
for k in range(2):
    d = 0.9
    f0 = 140 + 30 * k
    N = int(SR / f0)
    buf = rng.uniform(-1, 1, N)
    out = np.zeros(int(SR * d))
    for i in range(len(out)):
        out[i] = buf[i % N]
        buf[i % N] = 0.5 * (buf[i % N] + buf[(i + 1) % N]) * 0.996
    whoosh = L.hp(noise(d), 1500) * envexp(d, 9) * 0.25
    save("A_SlingTwang_%d" % (k + 1), out * envexp(d, 3.5) + whoosh, -14)
# static crackle: random clicks
for k in range(2):
    d = 0.7
    x = np.zeros(int(SR * d))
    for i in range(40 + 20 * k):
        p = rng.integers(0, len(x) - 400)
        c = rng.standard_normal(300) * np.exp(-np.arange(300) / 30) * rng.uniform(0.2, 1)
        x[p:p + 300] += c
    save("A_StaticCrackle_%d" % (k + 1), L.hp(x, 2000), -18)
# ice clink: glassy high partials, two hits
for k in range(2):
    d = 0.6
    x = np.zeros(int(SR * d))
    for hit, a in ((0.0, 1.0), (0.09 + 0.03 * k, 0.6)):
        s = int(SR * hit); n = len(x) - s
        tt = np.arange(n) / SR
        part = sum(np.sin(2 * np.pi * f * tt) * np.exp(-tt * dk) * g for f, dk, g in ((3100 + 200 * k, 22, 1), (5300, 30, 0.6), (7900, 45, 0.35), (1700, 18, 0.3)))
        x[s:] += part * a
    save("A_IceClink_%d" % (k + 1), x, -17)
print("SFX DONE")
