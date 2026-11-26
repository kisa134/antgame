# fix14: before / after pairs from the packaged game (Tools/art/play_sink_chairs.cmd) -> Saved/review/<shot>.jpg
# python fix14_review.py <shots dir> <before prefix> <after prefix> [<extra prefix> <extra label>]
import os, sys
from PIL import Image, ImageDraw, ImageFont

src, a, b = sys.argv[1], sys.argv[2], sys.argv[3]
extra = (sys.argv[4], sys.argv[5]) if len(sys.argv) > 5 else None
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Saved", "review")
os.makedirs(out, exist_ok=True)
font = ImageFont.truetype(r"C:\Windows\Fonts\arial.ttf", 30)
W, H = 960, 540
shots = ["sink_0s", "sink_20s", "sink_45s", "sink_65s", "sink_wide", "chair1", "highchair", "chair2"]
for s in shots:
    cols = [(a, "было: v14.13"), (b, "стало: fix14")] + ([extra] if extra else [])
    files = [os.path.join(src, "%s_%s.png" % (p, s)) for p, _ in cols]
    if not all(os.path.exists(f) for f in files):
        print("skip", s); continue
    sheet = Image.new("RGB", (W * len(cols), H + 50), (24, 24, 24))
    d = ImageDraw.Draw(sheet)
    for i, (f, (p, label)) in enumerate(zip(files, cols)):
        sheet.paste(Image.open(f).convert("RGB").resize((W, H)), (i * W, 50))
        d.text((i * W + 16, 8), "%s  (%s)" % (label, s), fill=(240, 240, 240), font=font)
    sheet.save(os.path.join(out, s + ".jpg"), quality=90)
    print("saved", s)
