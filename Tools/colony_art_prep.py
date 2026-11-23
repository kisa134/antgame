# The colony screen's pictures -> SourceArt/UI/Colony/T_Colony_*.png (imported by Tools/make_colony_assets.py into /Game/UI/Colony).
# Sources: Art/Colony/raw (Higgsfield: the background plate, the two chamber shells) and Art/Colony/blender (Tools/colony_blender_art.py).
# Props are cut to what they show and padded to the shape the screen draws them at; overlays keep their framing.
import os, sys
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
RAW = os.path.join(ROOT, "Art", "Colony", "raw")
BL = os.path.join(ROOT, "Art", "Colony", "blender")
OUT = os.path.join(ROOT, "SourceArt", "UI", "Colony")
os.makedirs(OUT, exist_ok=True)

def save(img, name):
    img.save(os.path.join(OUT, "T_Colony_%s.png" % name), optimize=True)
    print("prep", name, img.size)

def fit(img, aspect, max_side=512, valign="center"):
    """Crop to the picture's content, pad to aspect (w/h), at most max_side on the long side."""
    bb = img.getchannel("A").point(lambda a: 255 if a > 10 else 0).getbbox()
    if bb:
        img = img.crop(bb)
    w, h = img.size
    pad = 6
    if w / h > aspect:
        W, H = w + pad * 2, int((w + pad * 2) / aspect)
    else:
        H = h + pad * 2
        W = int(H * aspect)
    canvas = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    x = (W - w) // 2
    y = (H - h) // 2 if valign == "center" else H - h - pad
    canvas.paste(img, (x, y), img)
    k = min(1.0, max_side / max(W, H))
    if k < 1.0:
        canvas = canvas.resize((max(1, int(W * k)), max(1, int(H * k))), Image.LANCZOS)
    return canvas

# Higgsfield plates
if os.path.exists(os.path.join(RAW, "bg_plate.png")):
    save(Image.open(os.path.join(RAW, "bg_plate.png")).convert("RGB").resize((1920, 1080), Image.LANCZOS), "Bg")
for src, name in (("throne_1.png", "Shell_A"), ("stores_1.png", "Shell_B")):
    p = os.path.join(RAW, src)
    if os.path.exists(p):
        save(Image.open(p).convert("RGBA"), name)

# The soft dot every procedural shape is drawn with.
d = Image.new("L", (128, 128), 0)
ImageDraw.Draw(d).ellipse((14, 14, 114, 114), fill=255)
d = d.filter(ImageFilter.GaussianBlur(6))
dot = Image.new("RGBA", (128, 128), (255, 255, 255, 0))
dot.putalpha(d)
save(dot, "Dot")
# The glow for additive light (black outside, so additive blending leaves no square).
g = Image.new("L", (128, 128), 0)
ImageDraw.Draw(g).ellipse((26, 26, 102, 102), fill=255)
g = g.filter(ImageFilter.GaussianBlur(14))
glow = Image.merge("RGBA", (g, g, g, g))
save(glow, "Glow")

# Blender renders
keep = {"Frame", "Deco", "Seat_2", "Seat_3", "Crown_2", "Crown_3"} | {"Queen_" + f for f in ("Furious", "Grumpy", "Happy", "Delighted", "Sleeping")}
aspect = {"Larva": 34 / 22, "Cocoon": 22 / 34, "CakeSlice": 64 / 52, "Bunk": 58 / 30, "Crate": 60 / 48, "Plaque": 1.63, "Jelly": 1.0, "Mushroom": 1.0, "Radio": 1.0, "Target": 1.0}
if os.path.isdir(BL):
    for f in sorted(os.listdir(BL)):
        if not f.endswith(".png"):
            continue
        n = f[:-4]
        img = Image.open(os.path.join(BL, f)).convert("RGBA")
        if n in keep:
            save(img, n)
        elif n.startswith("Pile_"):
            save(fit(img, 1.6, 512, "bottom"), n)
        elif n.startswith("Trophy_"):
            save(fit(img, 1.0, 256), n)
        elif n in aspect:
            save(fit(img, aspect[n], 384, "bottom" if n in ("Mushroom", "Radio", "Target", "Jelly") else "center"), n)
        else:
            save(img, n)
