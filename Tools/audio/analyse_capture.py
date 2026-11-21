# Listen-check of a captured session: levels per second (peak, RMS), clipping, the music state and every sound played
# (ANTMUSIC / ANTSFX lines of the game log). Usage: python analyse_capture.py <audio.wav> <game.log> [out.png]
import sys, re, wave, collections, numpy as np

wav, log = sys.argv[1], sys.argv[2]
with wave.open(wav) as w:
    sr, ch, sw = w.getframerate(), w.getnchannels(), w.getsampwidth()
    raw = w.readframes(w.getnframes())
if sw == 2:
    x = np.frombuffer(raw, dtype=np.int16).astype(np.float64) / 32768
elif sw == 4:
    x = np.frombuffer(raw, dtype=np.int32).astype(np.float64) / 2147483648
else:
    x = (np.frombuffer(raw, dtype=np.uint8).astype(np.float64) - 128) / 128
x = x.reshape(-1, ch)
m = x.mean(axis=1)
n = len(m) // sr
print("audio: %.1f s, %d Hz, %d ch, peak %.1f dBFS, clipped samples %d" % (len(m) / sr, sr, ch, 20 * np.log10(np.abs(x).max() + 1e-12), int((np.abs(x) > 0.999).sum())))
rms = [20 * np.log10(np.sqrt(np.mean(m[i * sr:(i + 1) * sr] ** 2)) + 1e-12) for i in range(n)]
pk = [20 * np.log10(np.abs(m[i * sr:(i + 1) * sr]).max() + 1e-12) for i in range(n)]
silent = sum(1 for r in rms if r < -60)
print("seconds: %d, silent (< -60 dB) %d, rms min/median/max %.1f / %.1f / %.1f dB" % (n, silent, min(rms), float(np.median(rms)), max(rms)))
print("per 5 s rms:", " ".join("%.0f" % np.mean(rms[i:i + 5]) for i in range(0, n, 5)))
sfx = collections.Counter()
music = []
for line in open(log, encoding="utf-8", errors="ignore"):
    a = re.search(r"ANTSFX (\w+) ", line)
    if a:
        sfx[a.group(1)] += 1
    b = re.search(r"ANTMUSIC (.*)", line)
    if b:
        music.append(b.group(1).strip())
print("ANTSFX total %d, kinds %d:" % (sum(sfx.values()), len(sfx)))
print("  " + ", ".join("%s %d" % kv for kv in sfx.most_common()))
print("ANTMUSIC lines %d:" % len(music))
for l in music:
    print("  " + l)
if len(sys.argv) > 3:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(2, 1, figsize=(14, 6))
    ax[0].plot(rms, label="rms"); ax[0].plot(pk, label="peak"); ax[0].legend(); ax[0].set_ylabel("dBFS"); ax[0].grid(alpha=0.3)
    ax[1].specgram(m + 1e-9, NFFT=4096, Fs=sr, noverlap=2048, cmap="magma", vmin=-110, vmax=-20); ax[1].set_ylim(20, 12000); ax[1].set_yscale("symlog", linthresh=200)
    plt.tight_layout(); plt.savefig(sys.argv[3], dpi=60)
