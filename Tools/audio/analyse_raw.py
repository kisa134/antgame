import sys, os, glob, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from sfxlib import *
raw = sys.argv[1]
for f in sorted(glob.glob(os.path.join(raw, "*.mp3"))):
    x = decode(f)
    s = segments(x)
    lens = ",".join("%.2f" % ((b - a) / SR) for a, b in s[:10])
    print("%-16s dur=%5.2f peak=%6.1f act=%6.1f segs=%d [%s]" % (os.path.basename(f)[:-4], len(x) / SR, db(np.abs(x).max()), active_rms_db(x), len(s), lens))
