# KAN-22 (small props): the printed labels of the hero props, drawn here (PIL + the system fonts), our own words and design,
# no real brands. Each label is a PNG the Blender material maps onto the prop (Tools/blender/hero_props.py), then baked.
# Run: python Tools/art/make_labels.py [out_dir]   (default SourceArt/Blender/hero_props/labels)
import os, sys, math, random
from PIL import Image, ImageDraw, ImageFont, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "SourceArt", "Blender", "hero_props", "labels")
FD = r"C:\Windows\Fonts"

def F(name, size):
    return ImageFont.truetype(os.path.join(FD, name), size)

def ctext(d, cx, y, s, font, fill, spacing=0):
    w = d.textlength(s, font=font) + spacing * (len(s) - 1)
    if spacing:
        x = cx - w / 2
        for ch in s:
            d.text((x, y), ch, font=font, fill=fill); x += d.textlength(ch, font=font) + spacing
    else:
        d.text((cx - w / 2, y), s, font=font, fill=fill)

def paper(w, h, col, grain=6, seed=1):
    im = Image.new("RGB", (w, h), col)
    px = im.load(); R = random.Random(seed)
    for _ in range(w * h // 18):
        x, y = R.randrange(w), R.randrange(h); v = R.randint(-grain, grain)
        c = px[x, y]; px[x, y] = tuple(max(0, min(255, k + v)) for k in c)
    return im

def barcode(d, x, y, w, h, seed):
    R = random.Random(seed); cx = x
    d.rectangle([x - 6, y - 6, x + w + 6, y + h + 26], fill=(250, 250, 246))
    while cx < x + w:
        bw = R.choice((2, 2, 3, 4, 6))
        if R.random() < 0.55: d.rectangle([cx, y, cx + bw - 1, y + h], fill=(18, 18, 20))
        cx += bw
    d.text((x, y + h + 2), "4 607" + "".join(str(R.randint(0, 9)) for _ in range(8)), font=F("consola.ttf", 18), fill=(18, 18, 20))

def save(im, name):
    os.makedirs(OUT, exist_ok=True)
    p = os.path.join(OUT, name + ".png"); im.save(p); print("LABEL", p, im.size)

# ------------------------------------------------------------------------------------------------ spice jar: paprika
def spice():
    W, H = 1600, 420
    im = paper(W, H, (238, 230, 205), 7, 3); d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W, 60], fill=(150, 32, 22)); d.rectangle([0, H - 60, W, H], fill=(150, 32, 22))
    for x in range(0, W, 40):   # a little zigzag border under the band
        d.polygon([(x, 60), (x + 20, 80), (x + 40, 60)], fill=(150, 32, 22))
        d.polygon([(x, H - 60), (x + 20, H - 80), (x + 40, H - 60)], fill=(150, 32, 22))
    ctext(d, W * 0.5, 12, "ПРЯНЫЙ ДВОР", F("georgiab.ttf", 34), (245, 225, 180), 6)
    ctext(d, W * 0.5, 120, "Паприка", F("georgiaz.ttf", 120), (120, 24, 16))
    ctext(d, W * 0.5, 260, "сладкая · молотая", F("georgiai.ttf", 44), (60, 40, 30))
    # pods on both sides
    for cx in (W * 0.16, W * 0.84):
        d.ellipse([cx - 70, 150, cx + 70, 250], fill=(178, 30, 18)); d.ellipse([cx - 40, 165, cx + 10, 190], fill=(230, 110, 90))
        d.rectangle([cx - 8, 125, cx + 8, 155], fill=(60, 110, 40))
    d.text((W * 0.04, H - 52), "40 г", font=F("arialbd.ttf", 34), fill=(245, 225, 180))
    d.text((W * 0.80, H - 52), "годен до 03.2028", font=F("arial.ttf", 26), fill=(245, 225, 180))
    save(im, "spice_paprika")

# ------------------------------------------------------------------------------------------------ jam jar
def jam():
    W, H = 1800, 600
    im = paper(W, H, (246, 240, 222), 6, 5); d = ImageDraw.Draw(im)
    d.rounded_rectangle([30, 30, W - 30, H - 30], 40, outline=(170, 30, 40), width=8)
    d.rounded_rectangle([48, 48, W - 48, H - 48], 30, outline=(170, 30, 40), width=2)
    ctext(d, W * 0.5, 70, "БАБУШКИНА ПОЛКА", F("georgiab.ttf", 46), (90, 60, 40), 8)
    ctext(d, W * 0.5, 150, "Варенье", F("Gabriola.ttf", 210), (160, 20, 35))
    ctext(d, W * 0.5, 400, "клубничное · домашнее", F("georgiai.ttf", 56), (90, 60, 40))
    for cx in (W * 0.14, W * 0.86):   # strawberries
        d.polygon([(cx - 80, 230), (cx + 80, 230), (cx, 400)], fill=(200, 28, 40)); d.ellipse([cx - 80, 180, cx + 80, 290], fill=(200, 28, 40))
        R = random.Random(int(cx))
        for _ in range(26):
            x, y = cx + R.uniform(-60, 60), R.uniform(200, 330)
            if abs(x - cx) < (400 - y) * 0.5 + 30: d.ellipse([x - 4, y - 6, x + 4, y + 6], fill=(250, 220, 120))
        d.polygon([(cx - 60, 190), (cx, 160), (cx + 60, 190), (cx, 210)], fill=(60, 130, 50))
    d.text((80, H - 110), "350 г", font=F("arialbd.ttf", 44), fill=(90, 60, 40))
    d.text((W - 430, H - 100), "сварено 14.08", font=F("segoepr.ttf", 46), fill=(40, 50, 110))
    save(im, "jam")

# ------------------------------------------------------------------------------------------------ pasta box: front, back, side, top
def pasta():
    W, H = 600, 1500       # the front (300 x 750 uu)
    def base(seed):
        im = paper(W, H, (236, 196, 52), 5, seed); d = ImageDraw.Draw(im)
        d.rectangle([0, 0, W, 220], fill=(24, 60, 140)); d.rectangle([0, H - 170, W, H], fill=(24, 60, 140))
        return im, d
    im, d = base(7)
    ctext(d, W * 0.5, 40, "ЗОЛОТОЕ", F("impact.ttf", 70), (250, 236, 170), 4)
    ctext(d, W * 0.5, 120, "ПОЛЕ", F("impact.ttf", 70), (250, 236, 170), 10)
    ctext(d, W * 0.5, 270, "Перья", F("georgiaz.ttf", 120), (24, 60, 140))
    ctext(d, W * 0.5, 410, "из твёрдых сортов пшеницы", F("arial.ttf", 30), (60, 50, 20))
    # the window onto the pasta (clear film): drawn penne
    d.rounded_rectangle([70, 500, W - 70, 1120], 50, fill=(250, 238, 190), outline=(24, 60, 140), width=6)
    R = random.Random(11)
    for _ in range(46):
        x, y, a = R.uniform(120, W - 120), R.uniform(560, 1060), R.uniform(0, math.pi)
        dx, dy = math.cos(a) * 55, math.sin(a) * 55; px, py = -math.sin(a) * 16, math.cos(a) * 16
        pts = [(x - dx + px, y - dy + py), (x + dx + px + 10, y + dy + py), (x + dx - px + 10, y + dy - py), (x - dx - px, y - dy - py)]
        d.polygon(pts, fill=(232, 186, 80), outline=(190, 140, 40))
        for k in range(-3, 4):
            d.line([(x + dx * k / 4 + px * 0.9, y + dy * k / 4 + py * 0.9), (x + dx * k / 4 - px * 0.9, y + dy * k / 4 - py * 0.9)], fill=(205, 158, 60), width=2)
    d.text((40, H - 130), "500 г", font=F("impact.ttf", 80), fill=(250, 236, 170))
    d.text((W - 250, H - 110), "варить 11 мин", font=F("arial.ttf", 30), fill=(250, 236, 170))
    save(im, "pasta_front")
    im, d = base(8)
    ctext(d, W * 0.5, 80, "ЗОЛОТОЕ ПОЛЕ", F("impact.ttf", 60), (250, 236, 170), 2)
    ctext(d, W * 0.5, 270, "Как варить", F("georgiab.ttf", 54), (24, 60, 140))
    txt = ["1. Вскипятите 2 л воды,", "    посолите по вкусу.", "2. Всыпьте 250 г макарон,", "    помешайте.", "3. Варите 11 минут.", "4. Откиньте на дуршлаг.",
           "", "Пищевая ценность", "на 100 г:", "белки 12 г", "жиры 1,5 г", "углеводы 70 г", "345 ккал"]
    for i, s in enumerate(txt): d.text((50, 360 + i * 46), s, font=F("arial.ttf", 34), fill=(40, 40, 30))
    barcode(d, 330, H - 400, 220, 140, 3)
    save(im, "pasta_back")
    im = paper(400, H, (236, 196, 52), 5, 9); d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 400, 220], fill=(24, 60, 140)); d.rectangle([0, H - 170, 400, H], fill=(24, 60, 140))
    sb = Image.new("RGBA", (1100, 140), (0, 0, 0, 0)); ds = ImageDraw.Draw(sb)
    ds.text((10, 10), "ЗОЛОТОЕ ПОЛЕ · ПЕРЬЯ", font=F("impact.ttf", 100), fill=(24, 60, 140, 255))
    im.paste(sb.rotate(90, expand=True), (130, 250), sb.rotate(90, expand=True))
    save(im, "pasta_side")
    im = paper(600, 400, (24, 60, 140), 5, 10); d = ImageDraw.Draw(im)
    ctext(d, 300, 120, "ОТКРЫВАТЬ ЗДЕСЬ", F("arialbd.ttf", 40), (250, 236, 170)); d.line([(40, 200), (560, 200)], fill=(250, 236, 170), width=4)
    save(im, "pasta_top")

# ------------------------------------------------------------------------------------------------ mug print (dad's mug)
def mug():
    W, H = 2048, 512
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    # u 0.5 = the side facing -Y, the handle at u 0.75 (+X): the hearts on the side away from the handle, u 0.25
    ctext(d, W * 0.57, 120, "ПАПА", F("georgiab.ttf", 200), (250, 248, 240, 255), 10)
    ctext(d, W * 0.57, 360, "лучший по утрам", F("georgiai.ttf", 64), (250, 248, 240, 255))
    save(im, "mug_print")

# ------------------------------------------------------------------------------------------------ tin of peas (full wrap)
def peas():
    W, H = 2400, 640
    im = paper(W, H, (40, 110, 52), 6, 21); d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W, 70], fill=(236, 222, 170)); d.rectangle([0, H - 70, W, H], fill=(236, 222, 170))
    for half in (0, W // 2):
        cx = half + W * 0.25
        ctext(d, cx, 90, "ДАРЫ ГРЯДКИ", F("georgiab.ttf", 50), (236, 222, 170), 8)
        ctext(d, cx, 170, "Горошек", F("georgiaz.ttf", 150), (250, 240, 200))
        ctext(d, cx, 360, "зелёный · нежный", F("georgiai.ttf", 56), (236, 222, 170))
        R = random.Random(half + 3)
        for k in range(9):   # peas in a pod
            x = cx - 260 + k * 62; y = 500 + R.uniform(-6, 6)
            d.ellipse([x - 28, y - 28, x + 28, y + 28], fill=(120, 190, 70), outline=(70, 130, 40), width=3)
            d.ellipse([x - 14, y - 18, x - 2, y - 6], fill=(190, 230, 140))
    d.text((40, H - 62), "400 г", font=F("arialbd.ttf", 46), fill=(40, 110, 52))
    barcode(d, W // 2 - 140, 120, 260, 120, 7)
    d.text((W // 2 - 150, 300), "Состав: горошек, вода,\nсоль, сахар.\nХранить при 0...+25", font=F("arial.ttf", 30), fill=(236, 222, 170))
    save(im, "peas")

# ------------------------------------------------------------------------------------------------ lemonade bottle label (half peeled in the model)
def lemonade():
    W, H = 1600, 700
    im = paper(W, H, (250, 246, 232), 6, 31); d = ImageDraw.Draw(im)
    for k in range(0, W, 60): d.polygon([(k, 0), (k + 30, 40), (k + 60, 0)], fill=(30, 70, 150))
    for k in range(0, W, 60): d.polygon([(k, H), (k + 30, H - 40), (k + 60, H)], fill=(30, 70, 150))
    ctext(d, W * 0.5, 70, "ЛИМОНАД", F("impact.ttf", 120), (200, 30, 40), 12)
    ctext(d, W * 0.5, 230, "«Звёздный»", F("georgiaz.ttf", 150), (30, 70, 150))
    for k in range(5):   # a row of stars
        cx, cy, r = W * 0.5 + (k - 2) * 120, 520, 40
        pts = [(cx + (r if i % 2 == 0 else r * 0.45) * math.sin(i * math.pi / 5), cy - (r if i % 2 == 0 else r * 0.45) * math.cos(i * math.pi / 5)) for i in range(10)]
        d.polygon(pts, fill=(240, 180, 30))
    d.text((60, H - 120), "0,5 л", font=F("arialbd.ttf", 50), fill=(30, 70, 150))
    d.text((W - 380, H - 110), "газированный", font=F("arial.ttf", 40), fill=(30, 70, 150))
    save(im, "lemonade")

# ------------------------------------------------------------------------------------------------ mouse trap: the stamp burnt into the board
def trap():
    W, H = 1520, 720
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    ink = (120, 30, 20, 210)
    ctext(d, W * 0.62, 120, "МЫШЕЛОВ", F("impact.ttf", 120), ink, 10)
    ctext(d, W * 0.62, 280, "№ 3 · фабрика «Капкан»", F("georgiab.ttf", 56), ink)
    d.line([(W * 0.3, 420), (W * 0.92, 420)], fill=ink, width=6)
    d.polygon([(W * 0.92, 400), (W * 0.97, 420), (W * 0.92, 440)], fill=ink)
    ctext(d, W * 0.62, 460, "приманку класть сюда", F("georgiai.ttf", 48), ink)
    save(im, "trap_stamp")

# ------------------------------------------------------------------------------------------------ floor cleaner bottle
def detergent():
    W, H = 1100, 1300
    im = paper(W, H, (250, 250, 246), 4, 41); d = ImageDraw.Draw(im)
    d.rounded_rectangle([20, 20, W - 20, H - 20], 60, fill=(250, 250, 246), outline=(20, 110, 60), width=10)
    ctext(d, W * 0.5, 70, "ЧИСТО", F("impact.ttf", 170), (20, 110, 60), 10)
    ctext(d, W * 0.5, 250, "ДОМА", F("impact.ttf", 170), (240, 190, 20), 10)
    ctext(d, W * 0.5, 470, "средство для мытья полов", F("arialbd.ttf", 52), (40, 40, 40))
    d.ellipse([W * 0.5 - 200, 580, W * 0.5 + 200, 880], fill=(250, 215, 40), outline=(200, 160, 20), width=8)   # a lemon
    d.ellipse([W * 0.5 - 120, 630, W * 0.5 - 20, 690], fill=(255, 240, 150))
    d.polygon([(W * 0.5 + 180, 640), (W * 0.5 + 300, 560), (W * 0.5 + 220, 680)], fill=(60, 140, 50))
    ctext(d, W * 0.5, 930, "Лимон и мята", F("georgiaz.ttf", 90), (20, 110, 60))
    d.text((70, H - 230), "1 л", font=F("impact.ttf", 110), fill=(20, 110, 60))
    d.text((W - 520, H - 200), "Беречь от детей!", font=F("arialbd.ttf", 40), fill=(200, 40, 30))
    save(im, "detergent")

# ------------------------------------------------------------------------------------------------ the family's books (SM_L_Book_A..H)
# key: thickness, depth, height (m, the old boxes), cloth colour, ink, title, author, series mark
BOOKS = {
    "A": (0.0566, 0.2648, 0.30, (150, 30, 30), (232, 200, 120), "Сад у моря", "Вера Лунина", "роман"),
    "B": (0.0766, 0.3048, 0.36, (30, 100, 100), (240, 230, 200), "Кухня на каждый день", "Н. Сытина", "500 рецептов"),
    "C": (0.1016, 0.3248, 0.40, (90, 55, 30), (232, 200, 120), "Жуки и муравьи", "проф. А. Усачёв", "энциклопедия"),
    "D": (0.0516, 0.2248, 0.25, (200, 170, 120), (120, 40, 30), "Сказки на ночь", "собрала бабушка Тоня", "для малышей"),
    "E": (0.0666, 0.2848, 0.33, (70, 80, 90), (240, 236, 226), "Вязание крючком", "Майя Петлина", "уроки"),
    "F": (0.0866, 0.3048, 0.38, (230, 226, 214), (150, 30, 30), "История одного дома", "Пётр Кирпичёв", "хроника"),
    "G": (0.0496, 0.2448, 0.27, (24, 40, 90), (232, 200, 120), "Стихи о дожде", "Илья Тучкин", "поэзия"),
    "H": (0.1116, 0.3048, 0.29, (140, 30, 40), (240, 236, 226), "Атлас мира", "картография", "большой атлас"),
}
PX = 2600   # pixels per metre of cover

def book(k):
    t, dd, h, cloth, ink, title, author, mark = BOOKS[k]
    W, H = int(dd * PX), int(h * PX)
    im = paper(W, H, cloth, 9, ord(k)); d = ImageDraw.Draw(im)
    m = int(0.012 * PX)
    d.rectangle([m, m, W - m, H - m], outline=ink, width=4)
    d.rectangle([m + 10, m + 10, W - m - 10, H - m - 10], outline=ink, width=1)
    fs = max(30, int(W / max(8, len(title)) * 1.25))
    words = title.split(); lines = [" ".join(words[:(len(words) + 1) // 2]), " ".join(words[(len(words) + 1) // 2:])] if len(title) > 12 else [title]
    for i, s in enumerate(lines):
        ctext(d, W * 0.5, H * 0.22 + i * fs * 1.15, s, F("georgiab.ttf", fs), ink)
    ctext(d, W * 0.5, H * 0.66, author, F("georgiai.ttf", max(24, fs // 2)), ink)
    cx, cy = W * 0.5, H * 0.52   # a small emblem
    d.ellipse([cx - fs * 0.5, cy - fs * 0.5, cx + fs * 0.5, cy + fs * 0.5], outline=ink, width=4)
    d.line([(cx - fs * 0.3, cy), (cx + fs * 0.3, cy)], fill=ink, width=4); d.line([(cx, cy - fs * 0.3), (cx, cy + fs * 0.3)], fill=ink, width=4)
    ctext(d, W * 0.5, H * 0.86, mark, F("georgia.ttf", max(20, fs // 3)), ink, 4)
    save(im, "book_%s_front" % k)
    back = paper(W, H, cloth, 9, ord(k) + 50); db = ImageDraw.Draw(back)
    db.rectangle([m, m, W - m, H - m], outline=ink, width=4)
    for i in range(6): db.line([(W * 0.18, H * 0.3 + i * H * 0.05), (W * 0.82, H * 0.3 + i * H * 0.05)], fill=tuple(int(c * 0.7 + cl * 0.3) for c, cl in zip(ink, cloth)), width=3)
    barcode(db, int(W * 0.55), int(H * 0.78), int(W * 0.3), int(H * 0.08), ord(k))
    save(back, "book_%s_back" % k)
    S, T = int(t * PX), H   # the spine: title running down it (as on Russian books, bottom to top)
    sp = paper(S, T, cloth, 9, ord(k) + 90); ds = ImageDraw.Draw(sp)
    for yy in (int(T * 0.06), int(T * 0.94)): ds.rectangle([0, yy - 6, S, yy + 6], fill=ink)
    strip = Image.new("RGBA", (int(T * 0.8), S), (0, 0, 0, 0)); dz = ImageDraw.Draw(strip)
    f2 = F("georgiab.ttf", max(18, int(S * 0.42)))
    txt = author.split()[-1] + "  ·  " + title
    while dz.textlength(txt, font=f2) > strip.width * 0.95 and f2.size > 14: f2 = F("georgiab.ttf", f2.size - 2)
    dz.text(((strip.width - dz.textlength(txt, font=f2)) / 2, (S - f2.size) / 2 - 2), txt, font=f2, fill=(*ink, 255))
    r = strip.rotate(90, expand=True); sp.paste(r, (0, int(T * 0.1)), r)
    save(sp, "book_%s_spine" % k)

def plain(name, col):
    save(paper(256, 256, col, 6, 3), name)

# ------------------------------------------------------------------------------------------------ dad's phone: the lock screen
def phone():
    W, H = 640, 1240   # 160 x 310 uu
    im = Image.new("RGB", (W, H)); d = ImageDraw.Draw(im)
    for y in range(H):   # an evening sky wallpaper
        t = y / H; d.line([(0, y), (W, y)], fill=(int(30 + 200 * t ** 2), int(40 + 90 * t), int(90 + 40 * (1 - t))))
    d.ellipse([W * 0.55, H * 0.55, W * 0.85, H * 0.70], fill=(250, 210, 150))
    d.polygon([(0, H * 0.8), (W * 0.3, H * 0.68), (W * 0.6, H * 0.78), (W, H * 0.7), (W, H), (0, H)], fill=(30, 30, 50))
    m = 30; d.rectangle([0, 0, W - 1, H - 1], outline=(5, 5, 6), width=m)   # the black border round the screen
    ctext(d, W * 0.5, 150, "07:42", F("segoeuil.ttf", 170), (250, 250, 250))
    ctext(d, W * 0.5, 350, "четверг, 26 ноября", F("segoeui.ttf", 40), (235, 235, 240))
    for i, (who, msg) in enumerate((("Мама", "купи хлеб и молоко"), ("Работа", "созвон перенесли на 10:30"))):
        y = 470 + i * 140
        d.rounded_rectangle([60, y, W - 60, y + 115], 26, fill=(245, 245, 248))
        d.text((90, y + 14), who, font=F("segoeuib.ttf", 34), fill=(30, 30, 30)); d.text((90, y + 60), msg, font=F("segoeui.ttf", 30), fill=(70, 70, 70))
    d.rounded_rectangle([W * 0.35, H - 80, W * 0.65, H - 68], 6, fill=(240, 240, 240))
    d.ellipse([W * 0.5 - 16, 50, W * 0.5 + 16, 82], fill=(2, 2, 3))   # front camera
    save(im, "phone_screen")

# ------------------------------------------------------------------------------------------------ the TV remote: the print round its buttons
REMOTE_BTNS = [(x, y) for y in (-55, 0, 55) for x in (-200, -140, -80)] + [(-20, 0)]   # (uu: the number pad, then 0)
def remote():
    W, H = 1200, 400   # 600 x 200 uu: 2 px / uu, x right, y up
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    def px(x, y): return (W / 2 + 2 * x, H / 2 - 2 * y)
    f = F("arialbd.ttf", 22)
    for i, (x, y) in enumerate(REMOTE_BTNS):
        cx, cy = px(x, y + 25); s = str((i + 1) % 10); d.text((cx - d.textlength(s, font=f) / 2, cy - 12), s, font=f, fill=(230, 230, 230, 255))
    cx, cy = px(110, 0)
    for a, s in ((0, "OK"),):
        d.text((cx - d.textlength(s, font=f) / 2, cy - 12), s, font=f, fill=(230, 230, 230, 255))
    for s, (x, y) in (("VOL", (200, 60)), ("CH", (200, -60)), ("МЕНЮ", (40, 70))):
        cx, cy = px(x, y); d.text((cx - d.textlength(s, font=F("arial.ttf", 18)) / 2, cy - 10), s, font=F("arial.ttf", 18), fill=(200, 200, 200, 255))
    cx, cy = px(255, 0); d.text((cx - 50, cy + 30), "ЭКРАН-3", font=F("arialbd.ttf", 20), fill=(170, 170, 170, 255))
    save(im, "remote_print")

# ------------------------------------------------------------------------------------------------ the matchbox label
def matches():
    W, H = 832, 1000   # 416 x 500 uu
    im = paper(W, H, (232, 214, 170), 6, 61); d = ImageDraw.Draw(im)
    d.rectangle([40, 40, W - 40, H - 40], fill=(30, 60, 120))
    d.rectangle([60, 60, W - 60, H - 60], outline=(232, 214, 170), width=4)
    ctext(d, W * 0.5, 120, "СПИЧКИ", F("impact.ttf", 130), (232, 214, 170), 10)
    cx, cy = W * 0.5, H * 0.55   # a flame
    d.polygon([(cx, cy - 190), (cx + 90, cy), (cx + 60, cy + 110), (cx, cy + 140), (cx - 60, cy + 110), (cx - 90, cy)], fill=(240, 140, 30))
    d.polygon([(cx, cy - 90), (cx + 45, cy + 30), (cx, cy + 110), (cx - 45, cy + 30)], fill=(250, 220, 90))
    ctext(d, W * 0.5, H - 200, "фабрика «Огонёк»", F("georgiai.ttf", 50), (232, 214, 170))
    ctext(d, W * 0.5, H - 130, "60 шт.", F("arialbd.ttf", 44), (232, 214, 170))
    save(im, "matchbox_top")

# ------------------------------------------------------------------------------------------------ the kid's board books (a stack of three)
KIDBOOKS = [("Кто как говорит?", (240, 180, 40), (200, 60, 40)), ("Цвета", (60, 160, 210), (250, 250, 250)), ("Раз, два, три!", (120, 190, 80), (250, 240, 200))]
def kidbooks():
    for i, (title, bg, ink) in enumerate(KIDBOOKS):
        W = 960; im = Image.new("RGB", (W, W), bg); d = ImageDraw.Draw(im)
        R = random.Random(i)
        for k in range(9):   # dots, balls, a sun
            x, y, r = R.uniform(100, 860), R.uniform(380, 860), R.uniform(40, 90)
            d.ellipse([x - r, y - r, x + r, y + r], fill=tuple(R.randint(60, 250) for _ in range(3)), outline=(40, 40, 40), width=5)
        d.rounded_rectangle([40, 60, W - 40, 300], 60, fill=ink)
        fs = 110 if len(title) < 10 else 80
        ctext(d, W * 0.5, 180 - fs * 0.6, title, F("comicbd.ttf", fs), bg)
        save(im, "kidbook_%d" % i)
        ed = Image.new("RGB", (W, 240), (245, 240, 228)); de = ImageDraw.Draw(ed)
        for k in range(0, 240, 24): de.line([(0, k), (W, k)], fill=(190, 180, 160), width=3)   # thick board pages
        save(ed, "kidbook_edge")
        sp = Image.new("RGB", (W, 240), ink); ds = ImageDraw.Draw(sp); ctext(ds, W * 0.5, 70, title, F("comicbd.ttf", 80), bg)
        save(sp, "kidbook_spine_%d" % i)

# ------------------------------------------------------------------------------------------------ the TV remote, v2: symbols on the keys
# (uu, x along the remote, the IR end at +x; y across). The text's "up" is +x (as held, pointing at the TV).
REMOTE_KEYS = ([("num", -80 - 60 * c, 50 - 50 * r, str(1 + c + 3 * r)) for r in range(3) for c in range(3)] + [("num", -255, 0, "0")] +
               [("power", 250, 55, ""), ("mute", 250, -55, ""), ("ok", 140, 0, "OK")] +
               [("col", -20, y, c) for y, c in ((-66, "r"), (-22, "g"), (22, "y"), (66, "b"))])
def remote2():
    W, H = 1200, 400
    def P(x, y): return ((x + 300) * 2, (100 - y) * 2)
    for layer in ("keys", "body"):
        im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
        def sym(x, y, s, size=30, col=(240, 240, 240, 255), font="arialbd.ttf"):
            t = Image.new("RGBA", (120, 120), (0, 0, 0, 0)); dt = ImageDraw.Draw(t); f = F(font, size)
            dt.text((60 - dt.textlength(s, font=f) / 2, 60 - size * 0.62), s, font=f, fill=col)
            r = t.rotate(-90); cx, cy = P(x, y); im.paste(r, (int(cx - 60), int(cy - 60)), r)
        if layer == "keys":
            for kind, x, y, s in REMOTE_KEYS:
                if kind == "num": sym(x, y, s, 34)
                elif kind == "ok": sym(x, y, s, 26)
                elif kind == "power":   # the power sign
                    cx, cy = P(x, y); d.arc([cx - 14, cy - 14, cx + 14, cy + 14], -60, 240, fill=(250, 250, 250, 255), width=4); d.line([(cx, cy), (cx + 18, cy)], fill=(250, 250, 250, 255), width=4)
                elif kind == "mute":   # a speaker and a cross
                    cx, cy = P(x, y); d.polygon([(cx - 4, cy - 12), (cx + 2, cy - 12), (cx + 12, cy - 20), (cx + 12, cy + 20), (cx + 2, cy + 12), (cx - 4, cy + 12)], fill=(235, 235, 235, 255))
                    d.line([(cx - 18, cy - 10), (cx - 6, cy + 10)], fill=(235, 235, 235, 255), width=3); d.line([(cx - 18, cy + 10), (cx - 6, cy - 10)], fill=(235, 235, 235, 255), width=3)
            for (x, y, s) in ((55, 60, "+"), (25, 60, "−"), (55, -60, "▲"), (25, -60, "▼")): sym(x, y, s, 30, font="seguisym.ttf" if s in "▲▼" else "arialbd.ttf")
            for a, (dx, dy) in enumerate(((44, 0), (-44, 0), (0, 44), (0, -44))):   # the d-pad arrows on the ring
                cx, cy = P(140 + dx, dy); ang = math.atan2(-dy, dx); pts = [(cx + 10 * math.cos(ang + k * 2.4), cy + 10 * math.sin(ang + k * 2.4)) for k in range(3)]
                d.polygon(pts, fill=(235, 235, 235, 255))
        else:
            for (x, y, s) in ((40, 92, "VOL"), (40, -92, "CH"), (250, 85, ""), (-20, 90, "")): sym(x, y, s, 18, (190, 190, 190, 255), "arial.ttf")
            sym(-150, -88, "ЭКРАН · 3", 20, (170, 170, 170, 255), "arialbd.ttf")
        save(im, "remote_%s" % layer)

# ------------------------------------------------------------------------------------------------ batch 5: the sugar jar (a hand-written kraft label), the salt shaker's letter
def sugar():
    W, H = 1500, 700
    im = paper(W, H, (196, 160, 112), 10, 9); d = ImageDraw.Draw(im)
    d.rounded_rectangle([26, 26, W - 26, H - 26], 30, outline=(92, 60, 34), width=5)
    for x in range(60, W - 60, 28): d.line([(x, 52), (x + 12, 52)], fill=(92, 60, 34), width=3)
    ctext(d, W * 0.5, 110, "Сахар", F("Gabriola.ttf", 300), (64, 40, 24))
    ctext(d, W * 0.5, 470, "песок · не трогать до праздника", F("segoepr.ttf", 52), (40, 50, 110))
    d.text((80, H - 120), "1 кг", font=F("georgiab.ttf", 56), fill=(64, 40, 24))
    for cx, cy in ((W * 0.12, 220), (W * 0.88, 220)):   # sugar cubes, drawn in ink
        d.polygon([(cx - 50, cy), (cx, cy - 28), (cx + 50, cy), (cx, cy + 28)], outline=(64, 40, 24), width=4)
        d.line([(cx - 50, cy), (cx - 50, cy + 60), (cx, cy + 88), (cx, cy + 28)], fill=(64, 40, 24), width=4)
        d.line([(cx, cy + 88), (cx + 50, cy + 60), (cx + 50, cy)], fill=(64, 40, 24), width=4)
    save(im, "sugar")

def salt():
    W, H = 1200, 600
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    ctext(d, W * 0.5, 40, "С", F("georgiab.ttf", 420), (20, 45, 130, 255))
    for k in range(3): d.line([(W * 0.2, 520 + k * 22), (W * 0.8, 520 + k * 22)], fill=(20, 45, 130, 255), width=6)
    save(im, "salt")

# ------------------------------------------------------------------------------------------------ batch 8: the soda can, the milk carton, the cereal box
def soda():
    W, H = 2400, 900
    im = Image.new("RGB", (W, H), (176, 18, 24)); d = ImageDraw.Draw(im); R = random.Random(8)
    for _ in range(140):   # bubbles
        x, y, r = R.uniform(0, W), R.uniform(0, H), R.uniform(6, 34)
        d.ellipse([x - r, y - r, x + r, y + r], outline=(240, 120, 110), width=3)
    d.rectangle([0, 600, W, 660], fill=(250, 214, 60))
    for cx in (W * 0.5, W * 0.0, W * 1.0):
        ctext(d, cx, 200, "ИСКРА", F("impact.ttf", 300), (255, 250, 240), 10)
        ctext(d, cx, 520, "лимонад газированный", F("georgiai.ttf", 64), (255, 236, 200))
    ctext(d, W * 0.25, 700, "0,33 л", F("arialbd.ttf", 60), (255, 250, 240)); ctext(d, W * 0.75, 700, "охладить перед вскрытием", F("arial.ttf", 46), (255, 236, 200))
    save(im, "soda")

def milk():
    W, H = 1000, 1000
    def face(title):
        im = paper(W, H, (246, 248, 250), 4, 11); d = ImageDraw.Draw(im)
        d.rectangle([0, 620, W, H], fill=(40, 110, 200))
        d.polygon([(0, 640), (220, 560), (480, 620), (760, 540), (W, 600), (W, 700), (0, 700)], fill=(90, 170, 70))
        for x in (150, 420, 760):   # little daisies on the meadow
            for a in range(6): d.ellipse([x + 22 * math.cos(a) - 10, 640 + 22 * math.sin(a) - 10, x + 22 * math.cos(a) + 10, 640 + 22 * math.sin(a) + 10], fill=(255, 255, 255))
            d.ellipse([x - 9, 631, x + 9, 649], fill=(250, 200, 40))
        ctext(d, W * 0.5, 80, "ЛУГОВОЕ", F("georgiab.ttf", 70), (40, 110, 200), 8)
        ctext(d, W * 0.5, 230, title, F("Gabriola.ttf", 240), (20, 60, 140))
        ctext(d, W * 0.5, 760, "3,2% · 1 л", F("arialbd.ttf", 90), (255, 255, 255))
        return im
    save(face("Молоко"), "milk_front")
    im = paper(W, H, (246, 248, 250), 4, 12); d = ImageDraw.Draw(im)
    ctext(d, W * 0.5, 60, "Пищевая ценность на 100 г", F("arialbd.ttf", 44), (20, 60, 140))
    for k, (a, b) in enumerate((("белки", "2,9 г"), ("жиры", "3,2 г"), ("углеводы", "4,7 г"), ("энергия", "58 ккал"))):
        d.text((120, 170 + k * 80), a, font=F("arial.ttf", 44), fill=(30, 30, 40)); d.text((640, 170 + k * 80), b, font=F("arial.ttf", 44), fill=(30, 30, 40))
        d.line([(110, 230 + k * 80), (890, 230 + k * 80)], fill=(150, 160, 180), width=2)
    ctext(d, W * 0.5, 560, "хранить при +2...+6 °С", F("arial.ttf", 44), (30, 30, 40)); ctext(d, W * 0.5, 640, "годен до: 29.11", F("consola.ttf", 56), (30, 30, 40))
    barcode(d, 330, 760, 340, 110, 12); save(im, "milk_side")

def cereal():
    W, H = 1200, 2000
    im = Image.new("RGB", (W, H), (236, 150, 30)); d = ImageDraw.Draw(im)
    for k in range(0, H, 80): d.line([(0, k), (W, k + 400)], fill=(244, 170, 50), width=30)
    ctext(d, W * 0.5, 120, "ХРУСТИКИ", F("impact.ttf", 220), (120, 30, 20), 6)
    ctext(d, W * 0.5, 380, "медовые колечки", F("georgiai.ttf", 80), (255, 248, 230))
    d.ellipse([200, 900, 1000, 1500], fill=(245, 245, 250)); d.ellipse([240, 920, 960, 1240], fill=(250, 250, 255))
    R = random.Random(14)
    for _ in range(60):   # the rings in the bowl
        x, y = R.uniform(300, 900), R.uniform(960, 1200); r = R.uniform(28, 40)
        d.ellipse([x - r, y - r * 0.7, x + r, y + r * 0.7], fill=(214, 140, 40), outline=(160, 90, 20), width=4); d.ellipse([x - r * 0.35, y - r * 0.25, x + r * 0.35, y + r * 0.25], fill=(250, 250, 255))
    d.ellipse([820, 560, 1120, 860], fill=(220, 30, 40)); ctext(d, 970, 640, "+ мёд", F("arialbd.ttf", 70), (255, 255, 255))
    ctext(d, W * 0.5, 1650, "375 г", F("arialbd.ttf", 90), (120, 30, 20))
    save(im, "cereal_front")
    im = Image.new("RGB", (W, H), (240, 200, 120)); d = ImageDraw.Draw(im)
    ctext(d, W * 0.5, 80, "Помоги муравью дойти до колечка!", F("arialbd.ttf", 54), (120, 30, 20)); R = random.Random(15)
    for i in range(10):   # a maze grid
        for j in range(14):
            x, y = 150 + i * 90, 250 + j * 90
            if R.random() < 0.55: d.line([(x, y), (x + 90, y)], fill=(80, 40, 20), width=10)
            if R.random() < 0.45: d.line([(x, y), (x, y + 90)], fill=(80, 40, 20), width=10)
    d.rectangle([150, 250, 1050, 1510], outline=(80, 40, 20), width=12)
    barcode(d, 420, 1700, 360, 120, 15); save(im, "cereal_back")
    im = Image.new("RGB", (600, H), (236, 150, 30)); d = ImageDraw.Draw(im)
    ctext(d, 300, 120, "ХРУСТИКИ", F("impact.ttf", 110), (120, 30, 20))
    d.rectangle([40, 400, 560, 1300], fill=(255, 248, 230))
    for k, (a, b) in enumerate((("белки", "8 г"), ("жиры", "3 г"), ("углеводы", "79 г"), ("энергия", "370 ккал"))):
        d.text((70, 440 + k * 90), a, font=F("arial.ttf", 44), fill=(40, 30, 20)); d.text((380, 440 + k * 90), b, font=F("arial.ttf", 44), fill=(40, 30, 20))
    save(im, "cereal_side")
    save(Image.new("RGB", (600, 400), (236, 150, 30)), "cereal_top")

if __name__ == "__main__":
    spice(); jam(); pasta(); mug(); peas(); lemonade(); trap(); detergent()
    for k in BOOKS: book(k)
    phone(); remote(); matches(); kidbooks(); remote2(); sugar(); salt(); soda(); milk(); cereal()
