#!/usr/bin/env python3
"""Top-down map of the Ant Kitchen level for PlaytestKit heatmaps.

Reads docs/level/level_v3.json (areas / walls / objects bb) and draws a flat
plan view to <out>.png. The world->image transform is written next to it
(<out>.json) and referenced by projects/ant-kitchen.json ("map").

    python make_topdown_ant_kitchen.py --level <repo>/docs/level/level_v3.json --out maps/ant_kitchen_topdown.png
"""
import argparse, json, os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle

CAT_COL = {"furn": "#8a7f72", "appl": "#6f7f8f", "food": "#d9a441", "door": "#a0522d", "decor": "#9a8f80",
           "human": "#c0504d", "pet": "#7a5c9a"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--level", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--px", type=int, default=1600, help="image width in px")
    a = ap.parse_args()
    d = json.load(open(a.level, encoding="utf-8"))

    rects = [r for ar in d["areas"] for r in ar["rects"]]
    xs = [r[0] for r in rects] + [r[2] for r in rects]
    ys = [r[1] for r in rects] + [r[3] for r in rects]
    for w in d["walls"]:
        xs += [w["rect"][0], w["rect"][2]]; ys += [w["rect"][1], w["rect"][3]]
    pad = 300
    x0, x1, y0, y1 = min(xs) - pad, max(xs) + pad, min(ys) - pad, max(ys) + pad
    wpx = a.px
    hpx = int(round(wpx * (y1 - y0) / (x1 - x0)))

    fig = plt.figure(figsize=(wpx / 100, hpx / 100), dpi=100)
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(x0, x1); ax.set_ylim(y0, y1); ax.axis("off")
    fig.patch.set_facecolor("#1d1f23")
    for r in rects:
        ax.add_patch(Rectangle((r[0], r[1]), r[2] - r[0], r[3] - r[1], fc="#2c3038", ec="none"))
    for s in d.get("surfaces", []):
        for r in s.get("rects", []):
            ax.add_patch(Rectangle((r[0], r[1]), r[2] - r[0], r[3] - r[1], fc="#3a3f49", ec="none", alpha=.5))
    for o in d["objects"]:
        bb = o.get("bb")
        if not bb or len(bb) < 4:
            continue
        col = CAT_COL.get(o.get("cat"), "#6b6f78")
        ax.add_patch(Rectangle((bb[0], bb[1]), bb[2] - bb[0], bb[3] - bb[1], fc=col, ec="#14161a", lw=.4, alpha=.8))
    for w in d["walls"]:
        r = w["rect"]
        ax.add_patch(Rectangle((r[0], r[1]), r[2] - r[0], r[3] - r[1], fc="#555b66", ec="#14161a", lw=.5))
    for ar in d["areas"]:
        r = ar["rects"][0]
        ax.text((r[0] + r[2]) / 2, (r[1] + r[3]) / 2, ar.get("ru", ar["id"]), color="#ffffff", alpha=.25, ha="center", va="center", fontsize=22)
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    fig.savefig(a.out, dpi=100, facecolor=fig.get_facecolor())
    tf = {"image": os.path.basename(a.out), "w": wpx, "h": hpx, "x0": x0, "x1": x1, "y0": y0, "y1": y1,
          "note": "px = (x-x0)/(x1-x0)*w ; py = (y1-y)/(y1-y0)*h  (world Y north is up)"}
    json.dump(tf, open(os.path.splitext(a.out)[0] + ".json", "w"), indent=1)
    print("map", a.out, wpx, "x", hpx, "world", x0, x1, y0, y1)


if __name__ == "__main__":
    main()
