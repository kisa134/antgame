#!/usr/bin/env python3
"""PlaytestKit self-measured overhead per perf window.  ptk_overhead.py <Saved/Playtest folder>
ptk_ms_avg = mean cost of the plugin's per-frame tick (breadcrumb/perf/net sampling, UI tick, file flush) in that 5 s window."""
import glob, json, os, statistics as st, sys
rows = []
for d in glob.glob(os.path.join(sys.argv[1], "*", "")):
    for x in open(d + "events.jsonl", encoding="utf-8-sig"):
        if '"perf_sample"' in x:
            e = json.loads(x)
            p = e["props"]
            rows.append((os.path.basename(d.rstrip("\\/"))[-4:], e["t"] / 1000, p.get("ptk_ms_avg", 0), p.get("ptk_ms_max", 0), p.get("fps_avg", 0), p.get("ptk_track_us", 0)))
for r in rows:
    print("%s t=%6.1f avg=%.4f ms  max=%8.3f ms  fps=%5.1f  track=%.1f us" % r)
a = [r[2] for r in rows]
if a:
    print("windows %d | mean %.4f | median %.4f | p90 %.4f | max-window %.4f ms/frame" % (len(a), st.mean(a), st.median(a), sorted(a)[int(len(a) * .9) - 1], max(a)))
