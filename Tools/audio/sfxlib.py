# Shared helpers for the Ant Kitchen audio pipeline (decode, segment, loop, loudness, write).
import subprocess, numpy as np, os, wave

SR = 48000

def decode(path, sr=SR, mono=True):
    cmd = ["ffmpeg", "-v", "error", "-i", path, "-f", "f32le", "-ac", "1" if mono else "2", "-ar", str(sr), "-"]
    raw = subprocess.run(cmd, stdout=subprocess.PIPE, check=True).stdout
    x = np.frombuffer(raw, dtype=np.float32).astype(np.float64)
    if not mono:
        x = x.reshape(-1, 2)
    return x

def write_wav(path, x, sr=SR):
    x = np.asarray(x, dtype=np.float64)
    x = np.clip(x, -1.0, 1.0)
    pcm = (x * 32767.0).astype(np.int16)
    ch = 1 if pcm.ndim == 1 else pcm.shape[1]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(ch)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(pcm.tobytes())

def env(x, sr=SR, win=0.01):
    n = max(1, int(sr * win))
    m = x if x.ndim == 1 else x.mean(axis=1)
    p = np.convolve(m * m, np.ones(n) / n, mode="same")
    return np.sqrt(p + 1e-20)

def db(v):
    return 20 * np.log10(max(v, 1e-12))

def segments(x, sr=SR, rel_db=-30.0, min_gap=0.06, min_len=0.05, pad=0.012):
    """Splits a take with several hits separated by quiet into (start, end) sample ranges."""
    e = env(x, sr, 0.006)
    thr = e.max() * 10 ** (rel_db / 20.0)
    on = e > thr
    segs = []
    i, n = 0, len(on)
    gap = int(min_gap * sr)
    while i < n:
        if on[i]:
            j = i
            last = i
            while j < n and (j - last) < gap:
                if on[j]:
                    last = j
                j += 1
            s = max(0, i - int(pad * sr))
            t = min(n, last + int(pad * 4 * sr))
            if (t - s) >= min_len * sr:
                segs.append((s, t))
            i = j
        else:
            i += 1
    return segs

def trim(x, sr=SR, rel_db=-45.0):
    e = env(x, sr, 0.005)
    thr = e.max() * 10 ** (rel_db / 20.0)
    idx = np.where(e > thr)[0]
    if len(idx) == 0:
        return x
    s = max(0, idx[0] - int(0.004 * sr))
    t = min(len(x), idx[-1] + int(0.02 * sr))
    return x[s:t]

def fade(x, sr=SR, fin=0.003, fout=0.03):
    x = x.copy()
    a, b = int(fin * sr), int(fout * sr)
    a = min(a, len(x) // 4)
    b = min(b, len(x) // 2)
    if a > 0:
        x[:a] *= np.linspace(0, 1, a) if x.ndim == 1 else np.linspace(0, 1, a)[:, None]
    if b > 0:
        x[-b:] *= np.linspace(1, 0, b) if x.ndim == 1 else np.linspace(1, 0, b)[:, None]
    return x

def active_rms_db(x, sr=SR):
    """Loudness proxy: RMS of the loudest 50% of 20 ms frames (ignores the silent tail)."""
    m = x if x.ndim == 1 else x.mean(axis=1)
    n = int(0.02 * sr)
    k = len(m) // n
    if k < 1:
        return db(np.sqrt(np.mean(m * m)))
    fr = np.sqrt(np.mean(m[:k * n].reshape(k, n) ** 2, axis=1))
    fr = np.sort(fr)[::-1][:max(1, k // 2)]
    return db(np.sqrt(np.mean(fr ** 2)))

def level(x, target_db, peak_db=-1.0, sr=SR):
    g = 10 ** ((target_db - active_rms_db(x, sr)) / 20.0)
    y = x * g
    pk = np.abs(y).max()
    lim = 10 ** (peak_db / 20.0)
    if pk > lim:
        y *= lim / pk
    return y

def loop_xfade(x, sr=SR, xf=1.0):
    """Seamless loop: the last xf seconds are crossfaded (equal power) into the start; returns the shortened loop."""
    n = int(xf * sr)
    n = min(n, len(x) // 3)
    head = x[:n]
    tail = x[-n:]
    t = np.linspace(0, np.pi / 2, n)
    fi, fo = np.sin(t), np.cos(t)
    if x.ndim > 1:
        fi, fo = fi[:, None], fo[:, None]
    y = x[:-n].copy()
    y[:n] = head * fi + tail * fo
    return y

def hp(x, fc, sr=SR):
    from scipy.signal import butter, sosfilt
    sos = butter(2, fc, "highpass", fs=sr, output="sos")
    return sosfilt(sos, x, axis=0)

def lp(x, fc, sr=SR):
    from scipy.signal import butter, sosfilt
    sos = butter(2, fc, "lowpass", fs=sr, output="sos")
    return sosfilt(sos, x, axis=0)

def lowshelf(x, fc, gain_db, sr=SR):
    """Adds gain_db of the band below fc (a parallel low band, the 'macro' weight)."""
    g = 10 ** (gain_db / 20.0) - 1.0
    return x + g * lp(x, fc, sr)

def onset_peaks(x, n, min_dist=0.18, sr=SR, smooth=0.004):
    """The n strongest onsets (sample indices of the attack), at least min_dist apart, in time order."""
    e = env(x, sr, smooth)
    le = np.log(e + 1e-9)
    d = np.diff(le, prepend=le[0])
    d[d < 0] = 0
    # onset strength weighted by how loud the hit gets right after it
    look = int(0.03 * sr)
    after = np.array([e[i:i + look].max() if i + look < len(e) else e[i:].max() for i in range(0, len(e), 64)])
    strength = np.convolve(d, np.ones(int(0.004 * sr)), mode="same")
    cand = []
    hop = 64
    for k in range(0, len(e), hop):
        cand.append((strength[k] * after[k // hop], k))
    cand.sort(reverse=True)
    picked = []
    md = int(min_dist * sr)
    for s, k in cand:
        if s <= 0:
            break
        if all(abs(k - p) >= md for p in picked):
            picked.append(k)
        if len(picked) >= n:
            break
    return sorted(picked)

def cut_hits(x, n, max_len, min_dist=0.18, pre=0.006, sr=SR):
    """Cuts up to n hits: each runs from its onset to the next onset (or max_len), trimmed and faded."""
    pk = onset_peaks(x, n, min_dist, sr)
    out = []
    for i, p in enumerate(pk):
        s = max(0, p - int(pre * sr))
        nxt = pk[i + 1] - int(0.01 * sr) if i + 1 < len(pk) else len(x)
        t = min(nxt, s + int(max_len * sr))
        y = trim(x[s:t], sr, -50.0)
        if len(y) > 0.03 * sr:
            out.append(fade(y, sr, 0.002, min(0.05, len(y) / sr * 0.3)))
    return out
