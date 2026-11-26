# KAN-22 (small props): the review sheets "было | стало" - Saved/art_review/props/<Name>.jpg.
# Rows: the packaged game (old models with -AntHeroArt=0 | the hero ones), shot by shot from the ant's camera and wide; then the
# Blender review of the hero piece (overall + the ant's view; wet, charred, broken).
# Run: python Tools/art/props_review.py <film shots dir> <Saved/art_review/props> name=shot1,shot2 [name=...]
import os, sys
from PIL import Image, ImageDraw, ImageFont

SHOTS, REV = sys.argv[1], sys.argv[2]
W = 1600
F = ImageFont.truetype(r"C:\Windows\Fonts\arialbd.ttf", 34)

def fit(p, w):
    im = Image.open(p).convert("RGB"); return im.resize((w, int(im.height * w / im.width)))

def tag(im, s):
    d = ImageDraw.Draw(im); d.rectangle([0, 0, 22 + d.textlength(s, font=F), 52], fill=(0, 0, 0)); d.text((11, 8), s, font=F, fill=(255, 255, 255)); return im

for arg in sys.argv[3:]:
    name, shots = arg.split("=")
    rows = []
    for s in shots.split(","):
        old, new = os.path.join(SHOTS, "old_%s.png" % s), os.path.join(SHOTS, "new_%s.png" % s)
        if not (os.path.exists(old) and os.path.exists(new)):
            print("MISSING", s); continue
        a, b = tag(fit(old, W // 2), "было (-AntHeroArt=0)"), tag(fit(new, W // 2), "стало")
        r = Image.new("RGB", (W, max(a.height, b.height))); r.paste(a, (0, 0)); r.paste(b, (W // 2, 0)); rows.append(r)
    for extra, label in (("_blender.jpg", "Blender: общий вид | глазами муравья"), ("_states.jpg", "мокрый | обугленный | разбит | муравей")):
        p = os.path.join(REV, name + extra)
        if os.path.exists(p): rows.append(tag(fit(p, W), label))
    if not rows:
        continue
    out = Image.new("RGB", (W, sum(r.height for r in rows)), (20, 20, 20)); y = 0
    for r in rows: out.paste(r, (0, y)); y += r.height
    out.save(os.path.join(REV, name + ".jpg"), quality=88); print("SHEET", name, out.size)
