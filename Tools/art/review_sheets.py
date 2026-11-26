# KAN-22 pass 2: review sheets from the packaged game's shots (Saved/Screenshots/film of DistArt):
#   etalon_vs_game.jpg   the approved Blender reference | the game (wide, ant view, wet, char, tipped, broken)
#   <shot>.jpg           in the game: old models (-AntHeroArt=0) | hero art, the same camera
# Run: python Tools/art/review_sheets.py   -> Saved/art_review/v3/ingame/
import os, sys
from PIL import Image, ImageDraw, ImageFont
W = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FILM = os.path.join(W, "DistArt", "Windows", "AntKitchen", "Saved", "Screenshots", "film")
REF = r"C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen\Saved\claude\shots"
OUT = os.path.join(W, "Saved", "art_review", "v3", "ingame")
os.makedirs(OUT, exist_ok=True)
try: FONT = ImageFont.truetype("arial.ttf", 30)
except Exception: FONT = ImageFont.load_default()

def tile(paths, labels, out, w=960, h=540, cols=2):
    ims = []
    for p in paths:
        if p and os.path.exists(p):
            im = Image.open(p).convert("RGB"); im = im.resize((w, int(im.height * w / im.width)))
            c = Image.new("RGB", (w, h), (20, 20, 20)); c.paste(im.crop((0, max(0, (im.height - h) // 2), w, max(0, (im.height - h) // 2) + h)), (0, 0)); ims.append(c)
        else:
            ims.append(Image.new("RGB", (w, h), (60, 20, 20)))
    rows = (len(ims) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * w, rows * h), (0, 0, 0))
    d = ImageDraw.Draw(sheet)
    for k, (im, lb) in enumerate(zip(ims, labels)):
        x, y = (k % cols) * w, (k // cols) * h
        sheet.paste(im, (x, y))
        d.rectangle((x, y, x + 12 + 17 * len(lb), y + 44), fill=(0, 0, 0))
        d.text((x + 8, y + 6), lb, fill=(255, 255, 255), font=FONT)
    sheet.save(out, quality=90)
    print("SHEET", out)

def f(name): return os.path.join(FILM, name + ".png")

# the approved reference | the game
tile([os.path.join(REF, "hero_mop_bucket.jpg"), f("new_mop_bucket_wide"),
      os.path.join(REF, "hero_mop_bucket_ant.jpg"), f("new_mop_bucket_ant"),
      os.path.join(REF, "hero_mop_states.jpg"), f("new_wet"),
      os.path.join(REF, "hero_mop_states.jpg"), f("new_char"),
      None, f("new_tipped"),
      None, f("new_bucket_broken")],
     ["эталон (Blender)", "в игре", "эталон: вид муравья", "в игре: вид муравья", "эталон: мокрая", "в игре: мокрое",
      "эталон: горит", "в игре: уголь + кромка огня", "эталон: -", "в игре: опрокинуто, катится, вода", "эталон: -", "в игре: расколото"],
     os.path.join(OUT, "etalon_vs_game.jpg"))
# old | new for every shot
for s in ("mop_bucket_wide", "mop_bucket_ant", "run", "fridge", "range", "sink", "micro", "toaster", "table", "bin",
          "cab_ant", "wallcab", "wet", "table_wet", "char", "cab_char", "tip1", "tipped", "bucket_broken", "table_broken", "mop_broken"):
    tile([f("old_" + s), f("new_" + s)], ["было (-AntHeroArt=0)", "стало"], os.path.join(OUT, s + ".jpg"))
