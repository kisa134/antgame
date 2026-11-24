#!/usr/bin/env python3
"""Quick look at one session folder: event counts + selected events.  ptk_peek.py <session_dir> [name,name,...]"""
import collections, json, os, sys
d = sys.argv[1]
want = set((sys.argv[2] if len(sys.argv) > 2 else "marker,survey_show,survey_answer,survey_ask,session_start,session_end,level_start,tutorial_step_start,tutorial_step_complete,ui_screen,consent,crash,ensure").split(","))
L = []
for x in open(os.path.join(d, "events.jsonl"), encoding="utf-8-sig"):
    x = x.strip()
    if x:
        try: L.append(json.loads(x))
        except Exception: print("BAD LINE:", x[:80])
print("events", len(L), "last t", L[-1]["t"] / 1000 if L else 0, "s")
print(dict(collections.Counter(e["name"] for e in L).most_common()))
for e in L:
    if e["name"] in want:
        print(json.dumps(e, ensure_ascii=False)[:360])
shots = os.path.join(d, "shots")
if os.path.isdir(shots):
    print("shots:", [(f, os.path.getsize(os.path.join(shots, f))) for f in os.listdir(shots)])
print(open(os.path.join(d, "session.json"), encoding="utf-8-sig").read()[:700])
