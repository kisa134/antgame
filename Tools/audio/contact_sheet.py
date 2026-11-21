# Spectrogram + waveform contact sheet of WAV files (visual QA of generated audio).
# Usage: python contact_sheet.py <out.png> <wav> [<wav> ...]   (or a glob)
import sys, os, glob, wave, numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

def load(p):
    with wave.open(p) as w:
        sr = w.getframerate(); ch = w.getnchannels()
        x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float64) / 32768
    if ch > 1:
        x = x.reshape(-1, ch).mean(axis=1)
    return x, sr

out = sys.argv[1]
files = []
for a in sys.argv[2:]:
    files += sorted(glob.glob(a))
cols = 4
rows = (len(files) + cols - 1) // cols
fig, axs = plt.subplots(rows, cols, figsize=(cols * 4.2, rows * 1.9), squeeze=False)
for i, f in enumerate(files):
    ax = axs[i // cols][i % cols]
    x, sr = load(f)
    ax.specgram(x + 1e-9, NFFT=1024, Fs=sr, noverlap=512, cmap="magma", vmin=-120, vmax=-20)
    ax.set_ylim(0, 12000)
    t = np.arange(len(x)) / sr
    ax2 = ax.twinx()
    ax2.plot(t, x, color="cyan", lw=0.3, alpha=0.6)
    ax2.set_ylim(-1, 1); ax2.set_yticks([])
    pk = 20 * np.log10(np.abs(x).max() + 1e-9)
    ax.set_title("%s  %.2fs pk%.0f" % (os.path.basename(f)[:-4], len(x) / sr, pk), fontsize=7)
    ax.tick_params(labelsize=5)
for j in range(len(files), rows * cols):
    axs[j // cols][j % cols].axis("off")
plt.tight_layout()
plt.savefig(out, dpi=70)
print("wrote", out, len(files))
