# A small numpy synthesizer + sequencer for the Ant Kitchen soundtrack (all music is generated here: owned, no licences).
# Loops render into a circular buffer: a note or a reverb tail that runs past the end wraps to the start, so every loop is seamless.
import numpy as np
from scipy.signal import butter, sosfilt

SR = 48000
NOTE = {"C": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3, "Eb": 3, "E": 4, "F": 5, "F#": 6, "Gb": 6, "G": 7, "G#": 8, "Ab": 8, "A": 9, "A#": 10, "Bb": 10, "B": 11}

def midi(n):
    if isinstance(n, (int, float)):
        return n
    name, octv = (n[:2], n[2:]) if len(n) > 2 and n[1] in "#b" else (n[:1], n[1:])
    return 12 * (int(octv) + 1) + NOTE[name]

def hz(m):
    return 440.0 * 2 ** ((midi(m) - 69) / 12.0)

_sos_cache = {}
def filt(x, kind, fc, order=2):
    fc = float(min(max(fc, 20.0), SR * 0.45))
    key = (kind, round(fc, 1), order)
    if key not in _sos_cache:
        _sos_cache[key] = butter(order, fc, kind, fs=SR, output="sos")
    return sosfilt(_sos_cache[key], x, axis=0)

def bp(x, lo, hi):
    return filt(filt(x, "highpass", lo), "lowpass", hi)

def adsr(n, a, d, s, r, hold):
    """Envelope over n samples: attack a, decay d to level s, held until 'hold' seconds, release r."""
    t = np.arange(n) / SR
    e = np.where(t < a, t / max(a, 1e-4), s + (1 - s) * np.exp(-(t - a) / max(d, 1e-4)))
    rel = t > hold
    if rel.any():
        e_h = e[np.argmax(rel) - 1] if np.argmax(rel) > 0 else e[0]
        e = np.where(rel, e_h * np.exp(-(t - hold) / max(r, 1e-4)), e)
    return e

def expdec(n, tau, att=0.002):
    t = np.arange(n) / SR
    return np.minimum(1.0, t / att) * np.exp(-t / tau)

def saw(ph):
    return 2.0 * (ph / (2 * np.pi) % 1.0) - 1.0

def phase(f, n):
    if np.isscalar(f):
        return 2 * np.pi * f * np.arange(n) / SR
    return 2 * np.pi * np.cumsum(f) / SR

RNG = np.random.default_rng(1234)

# ---------------------------------------------------------------- instruments: each returns a mono note
def rhodes(f, dur, vel=1.0):
    n = int((dur + 1.2) * SR)
    t = np.arange(n) / SR
    idx = 1.8 * vel * np.exp(-t / 0.35) + 0.25
    mod = np.sin(phase(f, n)) * idx
    body = np.sin(phase(f, n) + mod)
    tine = 0.12 * vel * np.sin(phase(f * 14.0, n)) * np.exp(-t / 0.03)
    env = adsr(n, 0.003, 1.2, 0.35, 0.25, dur)
    return (body + tine) * env * 0.5 * vel

def bass(f, dur, vel=1.0, bright=900.0):
    n = int((dur + 0.15) * SR)
    t = np.arange(n) / SR
    ph = phase(f, n)
    s = 0.8 * np.sin(ph) + 0.35 * saw(ph)
    hi = filt(s, "lowpass", bright * (0.6 + vel))
    lo = filt(s, "lowpass", 220)
    e = np.exp(-t / 0.09)
    s = lo + (hi - lo) * e
    env = adsr(n, 0.004, 0.5, 0.75, 0.05, dur)
    return np.tanh(1.6 * s * env) * 0.6 * vel

def sub(f, dur, vel=1.0):
    n = int((dur + 0.1) * SR)
    return np.sin(phase(f, n)) * adsr(n, 0.01, 0.4, 0.8, 0.06, dur) * 0.5 * vel

def clav(f, dur, vel=1.0):
    n = int((dur + 0.08) * SR)
    t = np.arange(n) / SR
    ph = phase(f, n)
    pulse = np.where((ph / (2 * np.pi)) % 1.0 < 0.25, 1.0, -0.33)
    hi = filt(pulse, "lowpass", 4200)
    lo = filt(pulse, "lowpass", 900)
    e = np.exp(-t / 0.05)
    s = filt(lo + (hi - lo) * e, "highpass", 180)
    return s * adsr(n, 0.001, 0.12, 0.3, 0.03, dur) * 0.35 * vel

def brass(f, dur, vel=1.0):
    n = int((dur + 0.2) * SR)
    t = np.arange(n) / SR
    s = sum(saw(phase(f * d, n) + RNG.uniform(0, 6.28)) for d in (0.996, 1.0, 1.005))
    swell = np.minimum(1.0, t / 0.05)
    hi = filt(s, "lowpass", 3800 * vel)
    lo = filt(s, "lowpass", 700)
    s = lo + (hi - lo) * (swell * np.exp(-t / 0.35) * 0.8 + 0.2)
    return s * adsr(n, 0.02, 0.3, 0.6, 0.12, dur) * 0.18 * vel

def pad(f, dur, vel=1.0, cut=1400.0):
    n = int((dur + 1.0) * SR)
    s = sum(saw(phase(f * d, n) + RNG.uniform(0, 6.28)) for d in (0.993, 0.998, 1.0, 1.003, 1.008))
    s = filt(s, "lowpass", cut)
    return s * adsr(n, 0.6, 1.0, 0.85, 0.8, dur) * 0.07 * vel

def glock(f, dur, vel=1.0):
    n = int(1.6 * SR)
    t = np.arange(n) / SR
    s = np.sin(phase(f, n)) * np.exp(-t / 0.6) + 0.4 * np.sin(phase(f * 2.76, n)) * np.exp(-t / 0.15) + 0.2 * np.sin(phase(f * 5.4, n)) * np.exp(-t / 0.05)
    return s * np.minimum(1, t / 0.001) * 0.3 * vel

def vibes(f, dur, vel=1.0):
    n = int((dur + 1.5) * SR)
    t = np.arange(n) / SR
    s = np.sin(phase(f, n)) + 0.25 * np.sin(phase(f * 4.0, n)) * np.exp(-t / 0.2)
    trem = 1 - 0.25 * (0.5 + 0.5 * np.sin(2 * np.pi * 5.5 * t))
    return s * trem * adsr(n, 0.002, 1.5, 0.3, 0.6, dur) * 0.3 * vel

def whistle(f, dur, vel=1.0):
    n = int((dur + 0.15) * SR)
    t = np.arange(n) / SR
    vib = 1 + 0.006 * np.sin(2 * np.pi * 5.8 * t) * np.minimum(1, t / 0.25)
    s = np.sin(phase(f * vib, n)) + 0.08 * np.sin(phase(2 * f * vib, n))
    breath = bp(RNG.normal(0, 1, n), f * 0.9, f * 1.3) * 0.15
    return (s + breath) * adsr(n, 0.03, 0.2, 0.85, 0.08, dur) * 0.28 * vel

def tuba(f, dur, vel=1.0):
    n = int((dur + 0.12) * SR)
    ph = phase(f, n)
    s = filt(saw(ph), "lowpass", 420) + 0.6 * np.sin(ph)
    return np.tanh(1.3 * s) * adsr(n, 0.025, 0.25, 0.7, 0.06, dur) * 0.45 * vel

def trombone(f0, f1, dur, vel=1.0):
    """A sliding brass note f0 -> f1 with a wobbly vibrato (the sad 'wah-wah')."""
    n = int((dur + 0.15) * SR)
    t = np.arange(n) / SR
    f = f0 * (f1 / f0) ** np.minimum(1, t / max(dur, 1e-3))
    f = f * (1 + 0.02 * np.sin(2 * np.pi * 6.0 * t) * np.minimum(1, t / 0.3))
    s = sum(saw(phase(f * d, n)) for d in (0.997, 1.003))
    s = filt(s, "lowpass", 1400)
    wah = 0.5 + 0.5 * np.sin(2 * np.pi * min(1.0 / max(dur, 0.1), 3.0) * t - np.pi / 2)
    s = filt(s, "lowpass", 600) + (s - filt(s, "lowpass", 600)) * wah
    return s * adsr(n, 0.04, 0.3, 0.8, 0.1, dur) * 0.25 * vel

# ---------------------------------------------------------------- drums
def kick(vel=1.0, punch=1.0):
    n = int(0.5 * SR)
    t = np.arange(n) / SR
    f = 46 + 110 * punch * np.exp(-t / 0.035)
    s = np.sin(phase(f, n)) * np.exp(-t / 0.28)
    click = filt(RNG.normal(0, 1, n), "highpass", 2000) * np.exp(-t / 0.003) * 0.25
    return np.tanh(1.5 * (s + click)) * 0.9 * vel

def snare(vel=1.0, tone=190.0, tight=1.0):
    n = int(0.35 * SR)
    t = np.arange(n) / SR
    nz = bp(RNG.normal(0, 1, n), 1200, 8000) * np.exp(-t / (0.11 * tight))
    body = np.sin(phase(tone * (1 + 0.3 * np.exp(-t / 0.01)), n)) * np.exp(-t / 0.06)
    return (0.55 * nz + 0.6 * body) * 0.7 * vel

def hat(vel=1.0, open_=False):
    n = int((0.35 if open_ else 0.08) * SR)
    t = np.arange(n) / SR
    s = filt(RNG.normal(0, 1, n), "highpass", 7000) * np.exp(-t / (0.12 if open_ else 0.018))
    return s * 0.35 * vel

def shaker(vel=1.0):
    n = int(0.09 * SR)
    t = np.arange(n) / SR
    s = bp(RNG.normal(0, 1, n), 4500, 11000) * np.minimum(1, t / 0.012) * np.exp(-t / 0.03)
    return s * 0.3 * vel

def clap(vel=1.0):
    n = int(0.3 * SR)
    t = np.arange(n) / SR
    e = np.zeros(n)
    for k, o in enumerate((0.0, 0.01, 0.02)):
        i = int(o * SR)
        e[i:] += np.exp(-(t[: n - i]) / (0.008 if k < 2 else 0.09))
    return bp(RNG.normal(0, 1, n), 900, 6000) * e * 0.45 * vel

def rim(vel=1.0):
    n = int(0.06 * SR)
    t = np.arange(n) / SR
    s = bp(RNG.normal(0, 1, n), 1500, 4000) * np.exp(-t / 0.006) + np.sin(phase(1700, n)) * np.exp(-t / 0.012) * 0.6
    return s * 0.4 * vel

def tom(f, vel=1.0):
    n = int(0.5 * SR)
    t = np.arange(n) / SR
    s = np.sin(phase(f * (1 + 0.5 * np.exp(-t / 0.03)), n)) * np.exp(-t / 0.22)
    return np.tanh(1.4 * s) * 0.6 * vel

def crash(vel=1.0, length=1.8):
    n = int(length * SR)
    t = np.arange(n) / SR
    s = filt(RNG.normal(0, 1, n), "highpass", 4500) * np.exp(-t / (length * 0.35))
    metal = sum(np.sin(phase(f, n)) for f in (3120, 4370, 5650, 7210)) * np.exp(-t / 0.4) * 0.08
    return (s + metal) * 0.3 * vel

def timpani(f, vel=1.0):
    n = int(1.2 * SR)
    t = np.arange(n) / SR
    s = np.sin(phase(f * (1 + 0.1 * np.exp(-t / 0.05)), n)) * np.exp(-t / 0.5) + 0.3 * np.sin(phase(f * 1.5, n)) * np.exp(-t / 0.3)
    s += filt(RNG.normal(0, 1, n), "lowpass", 700) * np.exp(-t / 0.03) * 0.4
    return s * 0.55 * vel

def woodblock(vel=1.0, f=1100.0):
    n = int(0.08 * SR)
    t = np.arange(n) / SR
    s = np.sin(phase(f, n)) * np.exp(-t / 0.02) + 0.5 * np.sin(phase(f * 2.3, n)) * np.exp(-t / 0.008)
    return s * 0.35 * vel

def noise_riser(dur, vel=1.0):
    n = int(dur * SR)
    t = np.arange(n) / SR
    x = RNG.normal(0, 1, n)
    lo = filt(x, "lowpass", 600)
    hi = filt(x, "highpass", 3000)
    k = (t / dur) ** 2
    return (lo * (1 - k) + hi * k) * k * 0.12 * vel

def brush(vel=1.0):
    n = int(0.25 * SR)
    t = np.arange(n) / SR
    s = bp(RNG.normal(0, 1, n), 2000, 9000) * np.minimum(1, t / 0.03) * np.exp(-t / 0.08)
    return s * 0.25 * vel

# ---------------------------------------------------------------- the circular track
class Track:
    def __init__(self, seconds):
        self.n = int(round(seconds * SR))
        self.buf = np.zeros((self.n, 2))
        self.sends = np.zeros((self.n, 2))

    def add(self, t, mono, gain=1.0, pan=0.0, send=0.0):
        """Adds a mono note at time t (s), wrapping past the end of the loop."""
        i = int(round(t * SR)) % self.n
        L = len(mono)
        l = np.cos((pan + 1) * np.pi / 4) * gain
        r = np.sin((pan + 1) * np.pi / 4) * gain
        pos = 0
        while pos < L:
            take = min(L - pos, self.n - i)
            seg = mono[pos:pos + take]
            self.buf[i:i + take, 0] += seg * l
            self.buf[i:i + take, 1] += seg * r
            if send > 0:
                self.sends[i:i + take, 0] += seg * l * send
                self.sends[i:i + take, 1] += seg * r * send
            pos += take
            i = 0

    def render(self, ir=None):
        out = self.buf.copy()
        if ir is not None and np.abs(self.sends).max() > 0:
            out += circ_conv(self.sends, ir)
        return out

def make_ir(seconds=1.6, tone=5000.0, seed=3):
    rng = np.random.default_rng(seed)
    n = int(seconds * SR)
    t = np.arange(n) / SR
    ir = rng.normal(0, 1, (n, 2)) * np.exp(-t / (seconds * 0.28))[:, None]
    ir = filt(ir, "lowpass", tone)
    ir[: int(0.012 * SR)] = 0      # pre-delay
    return ir / np.sqrt((ir ** 2).sum(axis=0)).max() * 0.9

def circ_conv(x, ir):
    n = len(x)
    out = np.zeros_like(x)
    for c in range(2):
        h = np.zeros(n)
        m = min(n, len(ir))
        h[:m] = ir[:m, c]
        out[:, c] = np.fft.irfft(np.fft.rfft(x[:, c]) * np.fft.rfft(h), n)
    return out

def duck(n, times, depth=0.35, tau=0.12, loop=None):
    """Sidechain-style gain curve that dips at each time (kick)."""
    g = np.ones(n)
    L = int(0.6 * SR)
    t = np.arange(L) / SR
    dip = 1 - depth * np.exp(-t / tau)
    for tt in times:
        i = int(tt * SR) % n
        take = min(L, n - i)
        g[i:i + take] = np.minimum(g[i:i + take], dip[:take])
        if take < L:
            g[: L - take] = np.minimum(g[: L - take], dip[take:])
    return g

def soft_clip(x, ceiling=0.95):
    return np.tanh(x / ceiling) * ceiling
