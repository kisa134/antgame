# Seam check of looping WAVs: the jump from the last sample to the first, against the signal's usual sample-to-sample step,
# and the level just before / after the seam (a seamless loop shows neither a click nor a level step).
import sys, glob, os, wave, numpy as np
for f in sorted(sum([glob.glob(a) for a in sys.argv[1:]], [])):
    with wave.open(f) as w:
        ch = w.getnchannels(); sr = w.getframerate()
        x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float64).reshape(-1, ch) / 32768
    d = np.abs(np.diff(x, axis=0))
    typical = np.percentile(d, 99.9)
    seam = np.abs(x[0] - x[-1]).max()
    n = int(0.25 * sr)
    a = np.sqrt(np.mean(x[-n:] ** 2)); b = np.sqrt(np.mean(x[:n] ** 2))
    print("%-28s %6.1fs seam=%.4f p99.9step=%.4f  rms end/start %.1f/%.1f dB  %s" % (os.path.basename(f), len(x) / sr, seam, typical,
          20 * np.log10(a + 1e-9), 20 * np.log10(b + 1e-9), "OK" if seam <= typical * 1.5 else "CLICK?"))
