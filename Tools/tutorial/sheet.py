import sys, os, glob
from PIL import Image, ImageDraw
src, out = sys.argv[1], sys.argv[2]
cols = int(sys.argv[3]) if len(sys.argv) > 3 else 4
w = int(sys.argv[4]) if len(sys.argv) > 4 else 480
files = sorted(glob.glob(os.path.join(src, '*.png')))
h = w * 9 // 16
rows = (len(files) + cols - 1) // cols
S = Image.new('RGB', (cols * w, rows * h), (20, 20, 20))
for i, f in enumerate(files):
    im = Image.open(f).convert('RGB').resize((w, h), Image.LANCZOS)
    d = ImageDraw.Draw(im); d.rectangle([0, 0, 170, 16], fill=(0, 0, 0)); d.text((3, 2), os.path.basename(f)[:-4], fill=(255, 255, 0))
    S.paste(im, ((i % cols) * w, (i // cols) * h))
S.save(out, quality=85)
print(out, S.size)
