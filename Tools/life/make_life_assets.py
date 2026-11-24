# The living flat's prints and sounds (docs/level/LIFE.md): drawn and synthesized here, nothing licensed, no real brands.
#   python Tools/life/make_life_assets.py   -> SourceArt/Life/tex/T_Life_*.png, SourceArt/Life/audio/A_Life_*.wav
# Imported by Tools/life/import_life_assets.py (/Game/Life/Textures, /Game/Life/Audio).
import os, sys, math, random
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from scipy.io import wavfile
HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(PROJ, "Tools", "audio"))
import synth as S
TEX = os.path.join(PROJ, "SourceArt", "Life", "tex")
AUD = os.path.join(PROJ, "SourceArt", "Life", "audio")
VO = os.path.join(PROJ, "SourceArt", "Life", "tv", "vo")
FONTS = os.path.join(PROJ, "Content", "UI", "Fonts")
for d in (TEX, AUD, VO): os.makedirs(d, exist_ok=True)
F = lambda n, s: ImageFont.truetype(os.path.join(FONTS, n), s)
HAND = lambda s: F("Neucha-Regular.ttf", s)
SANS = lambda s: F("PTSansNarrow-Bold.ttf", s)
RUSSO = lambda s: F("RussoOne-Regular.ttf", s)
MONO = lambda s: F("PTMono-Regular.ttf", s)
rng = random.Random(5)

def save(im, name):
    im.save(os.path.join(TEX, name + ".png")); print("TEX", name, im.size)

def paper(w, h, base=(250, 246, 225), grain=10):
    a = np.array(Image.new("RGB", (w, h), base), np.float32)
    a += np.random.default_rng(w * 7 + h).normal(0, grain, (h, w, 1))
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))

# ---------------------------------------------------------------- notes on the fridge (one set per seed: the code picks 3 of them)
NOTES = [((255, 240, 120), ["Купить:", "молоко", "яйца 10 шт", "свечи!!"]),
         ((255, 190, 200), ["ТОРТ", "НЕ ТРОГАТЬ!", "(до вечера)"]),
         ((180, 230, 255), ["Бадрика", "кормить", "2 раза", "НЕ со стола"]),
         ((200, 255, 190), ["Позвонить", "бабушке", "в 18:00"]),
         ((255, 220, 150), ["Тёме 6 лет!", "шарики ✓", "гирлянда ✓", "торт ?"]),
         ((240, 240, 240), ["Сантехник", "вт, 10-14", "кран капает"]),
         ((255, 240, 120), ["Опять", "муравьи?!", "купить", "ловушки"]),
         ((255, 200, 160), ["Кот —", "НЕ на стол!"])]
for k, (col, lines) in enumerate(NOTES):
    im = paper(256, 256, col, 6); dr = ImageDraw.Draw(im)
    dr.rectangle([0, 0, 255, 28], fill=tuple(int(c * 0.93) for c in col))
    y = 44
    for ln in lines:
        dr.text((18 + rng.randint(-3, 3), y), ln, font=HAND(40 if len(ln) < 10 else 34), fill=(30, 40, 110) if k % 3 else (150, 20, 30))
        y += 48
    if k == 1: dr.line([(20, 120), (236, 112)], fill=(200, 20, 30), width=3)
    save(im.rotate(rng.uniform(-2, 2), fillcolor=col), "T_Life_Note%d" % k)

# ---------------------------------------------------------------- the calendar page: November 2026, the 23rd circled (the birthday)
im = paper(512, 400, (252, 252, 248), 4); dr = ImageDraw.Draw(im)
dr.rectangle([0, 0, 511, 64], fill=(200, 40, 50))
dr.text((256, 32), "НОЯБРЬ  2026", font=RUSSO(40), fill=(255, 255, 255), anchor="mm")
days = ["Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"]
cw, ch = 70, 50
for i, dname in enumerate(days):
    dr.text((22 + i * cw + cw // 2, 86), dname, font=SANS(24), fill=(200, 40, 50) if i >= 5 else (60, 60, 60), anchor="mm")
first = 6   # 1 November 2026 is a Sunday
for dday in range(1, 31):
    idx = first + dday - 1
    cx = 22 + (idx % 7) * cw + cw // 2; cy = 122 + (idx // 7) * ch
    dr.text((cx, cy), str(dday), font=SANS(30), fill=(200, 40, 50) if idx % 7 >= 5 else (40, 40, 40), anchor="mm")
    if dday < 23:
        dr.line([(cx - 18, cy + 14), (cx + 18, cy - 14)], fill=(80, 80, 160), width=2)
    if dday == 23:
        for r in range(3):
            dr.ellipse([cx - 28 - r, cy - 22 - r, cx + 28 + r, cy + 22 + r], outline=(220, 20, 40), width=2)
        dr.text((cx + 6, cy + 34), "ДР Тёмы!", font=HAND(26), fill=(220, 20, 40), anchor="mm")
    if dday == 26:
        dr.text((cx, cy + 18), "врач", font=HAND(18), fill=(30, 60, 140), anchor="mm")
save(im, "T_Life_Calendar")

# ---------------------------------------------------------------- the receipt (a strip; the code stretches it on the curled mesh)
im = paper(256, 640, (250, 250, 246), 5); dr = ImageDraw.Draw(im)
rows = [("МАГАЗИН «У ДОМА»", None), ("кассир: 3", None), ("-" * 22, None), ("Молоко 3,2%", "89,90"), ("Яйца С1 10шт", "119,00"), ("Мука 1кг", "64,50"),
        ("Сахар 1кг", "79,90"), ("Свечи для торта", "49,00"), ("Шарики 10шт", "199,00"), ("Ловушка д/насек.", "159,00"), ("Корм собачий", "349,00"),
        ("-" * 22, None), ("ИТОГО", "1110,20"), ("НАЛИЧНЫМИ", "1200,00"), ("СДАЧА", "89,80"), ("", None), ("СПАСИБО ЗА ПОКУПКУ!", None)]
y = 24
for a, b in rows:
    if b is None: dr.text((128, y), a, font=MONO(20), fill=(60, 60, 70), anchor="mm")
    else:
        dr.text((14, y), a, font=MONO(18), fill=(60, 60, 70), anchor="lm"); dr.text((242, y), b, font=MONO(18), fill=(60, 60, 70), anchor="rm")
    y += 30
for x in range(20, 236, 6): dr.line([(x, 590), (x, 620)], fill=(40, 40, 40), width=rng.choice([1, 2, 3]))
save(im, "T_Life_Receipt")

# ---------------------------------------------------------------- price stickers
for k, (price, col) in enumerate([("39,90", (255, 230, 60)), ("129,00", (255, 255, 255)), ("−30%", (255, 90, 80))]):
    im = Image.new("RGB", (192, 112), col); dr = ImageDraw.Draw(im)
    dr.rounded_rectangle([3, 3, 188, 108], 14, outline=(150, 150, 150), width=3)
    dr.text((96, 60), price, font=RUSSO(48 if k < 2 else 52), fill=(30, 30, 30) if k < 2 else (255, 255, 255), anchor="mm")
    save(im, "T_Life_Price%d" % k)

# ---------------------------------------------------------------- the rival colony's graffiti (RGBA: a decal-like print on the skirting)
im = Image.new("RGBA", (512, 256), (0, 0, 0, 0)); dr = ImageDraw.Draw(im)
red = (170, 25, 20, 235)
def scr(x, y, t, s):
    for dx, dy in ((0, 0), (1, 1), (-1, 0)):
        dr.text((x + dx, y + dy), t, font=HAND(s), fill=red)
scr(18, 8, "ЩЕЛЬ — НАША!", 64)
scr(40, 92, "торт возьмём", 44)
scr(120, 150, "сами", 40)
ax, ay = 400, 190
for k in (-1, 0, 1):
    dr.line([(ax + k * 18, ay), (ax + k * 24 - 14, ay + 30)], fill=red, width=5); dr.line([(ax + k * 18, ay), (ax + k * 24 + 14, ay + 30)], fill=red, width=5)
dr.ellipse([ax - 52, ay - 18, ax - 14, ay + 18], fill=red); dr.ellipse([ax - 16, ay - 12, ax + 16, ay + 12], fill=red); dr.ellipse([ax + 12, ay - 16, ax + 44, ay + 16], fill=red)
dr.line([(ax + 40, ay - 12), (ax + 62, ay - 40)], fill=red, width=4); dr.line([(ax + 34, ay - 14), (ax + 48, ay - 46)], fill=red, width=4)
for k in range(60):
    x, y = rng.randint(0, 511), rng.randint(0, 255)
    dr.ellipse([x, y, x + 3, y + 3], fill=(170, 25, 20, rng.randint(60, 160)))
save(im, "T_Life_Graffiti")

# ---------------------------------------------------------------- coffee ring / cat scratches / slobber (RGBA, the code puts them on surfaces)
im = Image.new("RGBA", (256, 256), (0, 0, 0, 0)); a = np.zeros((256, 256), np.float32)
yy, xx = np.mgrid[0:256, 0:256]; r = np.hypot(xx - 128, yy - 128)
a = np.exp(-((r - 100) / 6.0) ** 2) * 0.85 + np.clip(1 - r / 100, 0, 1) * 0.12
a *= 0.75 + 0.25 * np.random.default_rng(3).random((256, 256))
rgb = np.dstack([np.full((256, 256), 90), np.full((256, 256), 55), np.full((256, 256), 30), np.clip(a * 255, 0, 255)]).astype(np.uint8)
save(Image.fromarray(rgb, "RGBA"), "T_Life_CoffeeRing")
im = Image.new("RGBA", (256, 256), (0, 0, 0, 0)); dr = ImageDraw.Draw(im)
for k in range(4):
    x0 = 50 + k * 40
    pts = [(x0 + 6 * math.sin(t / 30.0 + k), 20 + t) for t in range(0, 220, 6)]
    dr.line(pts, fill=(235, 225, 205, 230), width=6); dr.line([(p[0] + 2, p[1]) for p in pts], fill=(120, 90, 60, 160), width=2)
save(im, "T_Life_Scratches")

# ---------------------------------------------------------------- the birthday banner: one letter per pennant
LETTERS = "С ДНЁМ РОЖДЕНИЯ"
COLS = [(230, 70, 80), (250, 170, 50), (90, 180, 230), (130, 200, 90), (190, 120, 220)]
k = 0
for ch in LETTERS:
    if ch == " ": continue
    col = COLS[k % len(COLS)]
    im = Image.new("RGB", (256, 342), col); dr = ImageDraw.Draw(im)
    for s in range(0, 342, 24): dr.line([(0, s), (256, s + 30)], fill=tuple(min(255, c + 18) for c in col), width=8)
    dr.text((128, 120), ch, font=RUSSO(140), fill=(255, 255, 255), anchor="mm", stroke_width=6, stroke_fill=tuple(int(c * 0.6) for c in col))
    save(im, "T_Life_Letter%02d" % k); k += 1

# ---------------------------------------------------------------- the Dad's phone: a scrolling feed (tall) and the lock screen
im = Image.new("RGB", (256, 1024), (245, 245, 248)); dr = ImageDraw.Draw(im)
posts = [((255, 200, 120), "Котики дня", "мяу x 1000"), ((120, 180, 255), "ЖУКИ — ПЧЁЛЫ 0:0", "Лига Дворов"), ((200, 255, 200), "Рецепт торта", "за 5 минут"),
         ((255, 160, 180), "Смешное видео", "собака и пылесос"), ((230, 230, 120), "Погода", "вечером дождь"), ((180, 160, 255), "Мемы", "понедельник опять")]
y = 8
for col, t1, t2 in posts:
    dr.rounded_rectangle([8, y, 248, y + 160], 10, fill=(255, 255, 255), outline=(220, 220, 225))
    dr.rectangle([16, y + 8, 240, y + 104], fill=col)
    for j in range(5):
        cx = 40 + j * 44; dr.ellipse([cx - 14, y + 40, cx + 14, y + 72], fill=tuple(int(c * 0.8) for c in col))
    dr.text((18, y + 118), t1, font=SANS(24), fill=(30, 30, 40), anchor="lm"); dr.text((18, y + 142), t2, font=SANS(18), fill=(110, 110, 120), anchor="lm")
    y += 170
save(im, "T_Life_PhoneFeed")
im = Image.new("RGB", (256, 512), (20, 30, 60)); dr = ImageDraw.Draw(im)
for k2 in range(512):
    dr.line([(0, k2), (255, k2)], fill=(20 + k2 // 12, 30 + k2 // 10, 70 + k2 // 8))
dr.text((128, 110), "19:42", font=SANS(72), fill=(255, 255, 255), anchor="mm")
dr.rounded_rectangle([14, 200, 242, 290], 14, fill=(240, 240, 245))
dr.text((26, 228), "Мама", font=SANS(26), fill=(20, 20, 30), anchor="lm")
dr.text((26, 262), "Торт забрал?? 🎂", font=SANS(22), fill=(60, 60, 70), anchor="lm")
dr.rounded_rectangle([14, 300, 242, 370], 14, fill=(240, 240, 245))
dr.text((26, 334), "Лига Дворов: гол!", font=SANS(22), fill=(60, 60, 70), anchor="lm")
save(im, "T_Life_PhoneLock")

# ---------------------------------------------------------------- birds' plumage (UV: the code's cube/smart unwrap, so soft speckled colour fields)
def plumage(name, base, spot, belly):
    a = np.zeros((256, 256, 3), np.float32); a[:] = base
    nz = np.random.default_rng(len(name)).random((64, 64)).repeat(4, 0).repeat(4, 1)
    a = a * (0.85 + 0.3 * nz[..., None])
    yy = np.mgrid[0:256, 0:256][0]
    a = np.where((yy > 150)[..., None], a * 0.4 + np.array(belly) * 0.6, a)
    sp = np.random.default_rng(len(name) + 3).random((256, 256)) > 0.985
    a[sp] = spot
    save(Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2)), name)
plumage("T_Life_Sparrow", (140, 105, 70), (60, 40, 25), (215, 200, 175))
plumage("T_Life_Pigeon", (120, 125, 140), (90, 140, 120), (150, 150, 165))
plumage("T_Life_Tit", (70, 120, 200), (250, 220, 60), (245, 215, 60))

# ================================================================ sounds
SR = S.SR
def wav(name, x, peak=0.8):
    x = np.asarray(x, np.float32)
    if x.ndim == 1: x = np.stack([x, x], 1)
    m = np.abs(x).max()
    if m > 0: x = x * (peak / m)
    wavfile.write(os.path.join(AUD, name + ".wav"), SR, (np.clip(x, -1, 1) * 32767).astype(np.int16)); print("AUD", name, len(x) / SR)

def loopfade(x, sec=0.25):
    """Seamless loop: the tail crossfaded into the head."""
    k = int(sec * SR); h = x[:k].copy(); t = x[-k:].copy(); r = np.linspace(0, 1, k)
    if x.ndim > 1: r = r[:, None]
    out = x[:-k].copy(); out[:k] = h * r + t * (1 - r)
    return out

n = int(6.0 * SR); t = np.arange(n) / SR
f = 205 + 12 * np.sin(2 * np.pi * 3.1 * t) + 8 * np.sin(2 * np.pi * 7.7 * t)
buzz = np.sign(np.sin(S.phase(f, n))) * 0.3 + np.sin(S.phase(f * 2, n)) * 0.2
buzz = S.bp(buzz, 150, 3000) * (0.6 + 0.4 * np.sin(2 * np.pi * 0.9 * t) ** 2)
wav("A_Life_FlyBuzz", loopfade(buzz), 0.5)
n = int(10 * SR)
rain = S.bp(S.RNG.normal(0, 1, (n, 2)), 400, 7000) * 0.4
for k in range(1400):
    i = S.RNG.integers(0, n - 2000); L = S.RNG.integers(200, 1600)
    rain[i:i + L] += S.bp(S.RNG.normal(0, 1, (L, 2)), 2000, 9000) * np.exp(-np.linspace(0, 6, L))[:, None] * S.RNG.uniform(0.3, 1.2)
wav("A_Life_Rain", loopfade(rain, 0.5), 0.6)
# the dishwasher: a hum, a water swish that comes and goes, a click now and then
n = int(12 * SR); t = np.arange(n) / SR
hum = np.sin(2 * np.pi * 100 * t) * 0.15 + np.sin(2 * np.pi * 200 * t) * 0.05
sw = S.bp(S.RNG.normal(0, 1, n), 200, 1500) * (0.5 + 0.5 * np.sin(2 * np.pi * t / 3.0)) * 0.5
wav("A_Life_Wash", loopfade(stack := np.stack([hum + sw, hum + sw * 0.9], 1), 0.4), 0.5)
# the kettle: a rising rumble (a loop of the boil)
n = int(8 * SR); t = np.arange(n) / SR
boil = S.bp(S.RNG.normal(0, 1, n), 80, 900) * 0.6
for k in range(300):
    i = S.RNG.integers(0, n - 3000)
    L = 2400; boil[i:i + L] += np.sin(2 * np.pi * S.RNG.uniform(300, 900) * np.arange(L) / SR) * np.exp(-np.linspace(0, 8, L)) * 0.3
wav("A_Life_KettleBoil", loopfade(boil), 0.5)
# one-shots
n = int(1.5 * SR); t = np.arange(n) / SR
pb = np.sin(2 * np.pi * 160 * t) * (np.sin(2 * np.pi * 9 * t) > 0) * ((t % 0.75) < 0.5)
wav("A_Life_PhoneBuzz", S.bp(pb + 0.3 * np.sign(np.sin(2 * np.pi * 160 * t)) * ((t % 0.75) < 0.5), 100, 2000) * np.minimum(1, (1.5 - t) * 8), 0.6)
n = int(0.5 * SR); t = np.arange(n) / SR
drip = np.sin(2 * np.pi * (1400 - 900 * t) * t) * np.exp(-t * 30) + np.sin(2 * np.pi * 2600 * t) * np.exp(-t * 60) * 0.3
wav("A_Life_Drip", drip, 0.5)
n = int(0.12 * SR); t = np.arange(n) / SR
wav("A_Life_Click", S.bp(S.RNG.normal(0, 1, n), 1500, 8000) * np.exp(-t * 90) + np.sin(2 * np.pi * 2200 * t) * np.exp(-t * 120) * 0.4, 0.6)
n = int(0.6 * SR); t = np.arange(n) / SR
wav("A_Life_TvStatic", S.bp(S.RNG.normal(0, 1, n), 600, 10000) * np.minimum(1, t * 40) * np.exp(-t * 3), 0.5)
# a pigeon's coo: a low hooted burble (two formants, a pitch wobble)
n = int(1.4 * SR); t = np.arange(n) / SR
f0 = 240 + 40 * np.sin(2 * np.pi * 2.5 * t) - 30 * t
coo = np.sin(S.phase(f0, n)) + 0.4 * np.sin(S.phase(f0 * 2, n))
env = np.clip(np.sin(np.pi * t / 1.4), 0, 1) * (0.6 + 0.4 * np.sin(2 * np.pi * 6 * t))
wav("A_Life_Pigeon", S.filt(coo * env, "lowpass", 900), 0.5)
wav("A_Life_Flap", S.bp(S.RNG.normal(0, 1, int(0.8 * SR)), 300, 4000) * (np.abs(np.sin(2 * np.pi * 9 * np.arange(int(0.8 * SR)) / SR)) ** 3) * np.exp(-np.linspace(0, 3, int(0.8 * SR))), 0.5)
# a car going by: a rising and falling engine with tyre hiss, panned by the code
n = int(6 * SR); t = np.arange(n) / SR
pk = np.exp(-((t - 3.0) / 1.1) ** 2)
eng = np.sin(S.phase(70 + 25 * pk - 10 * (t > 3.0) * pk, n)) * 0.5 + S.bp(S.RNG.normal(0, 1, n), 300, 5000) * 0.6
wav("A_Life_CarPass", eng * pk, 0.6)
n = int(0.25 * SR); t = np.arange(n) / SR
wav("A_Life_MothTap", S.bp(S.RNG.normal(0, 1, n), 1000, 6000) * np.exp(-t * 40), 0.4)
# music: the kid's music box (a lullaby on glock-like tines, slightly detuned, a loop)
T = S.Track(16.0)
mel = ["E5", "G5", "C6", "G5", "E5", "G5", "C6", "B5", "A5", "F5", "A5", "C6", "B5", "G5", "D5", "G5"] * 2
for i, nn in enumerate(mel):
    T.add(i * 0.5, S.glock(S.hz(nn) * (1 + 0.003 * math.sin(i)), 0.9, 0.8), 0.5, 0.0, 0.3)
    if i % 4 == 0: T.add(i * 0.5, S.glock(S.hz(nn) / 2, 1.2, 0.6), 0.25, 0.0, 0.3)
wav("A_Life_MusicBox", T.render(S.make_ir(1.6, 6000)), 0.6)
# the kitchen radio in the morning: the host (Irina) + a light song, band-limited like a small speaker
import subprocess
def tts(name, text, voice="Irina", rate=1.0, pitch=1.0):
    out = os.path.join(VO, name + ".wav")
    if not os.path.exists(out):
        tf = os.path.join(VO, name + ".txt"); open(tf, "w", encoding="utf-8").write(text)
        subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", os.path.join(HERE, "tts.ps1"), "-Voice", voice, "-TextFile", tf, "-Out", out,
                        "-Rate", str(rate), "-Pitch", str(pitch)], check=True, capture_output=True)
    sr, x = wavfile.read(out); x = x.astype(np.float32) / 32768.0
    if x.ndim > 1: x = x.mean(1)
    if sr != SR:
        idx = np.arange(0, len(x), sr / SR); x = np.interp(idx, np.arange(len(x)), x)
    return x
host = tts("radio", "Доброе утро! Вы слушаете «Радио Кухня». В городе плюс восемнадцать, ветер слабый, вечером возможен дождь. "
           "Не забудьте зонтик — и закрыть сахарницу. А теперь — музыка!", "Irina", 1.05, 1.0)
secs = 44.0
T = S.Track(secs); beat = 60 / 104.0
prog = [["C3", "E3", "G3"], ["A2", "C3", "E3"], ["F2", "A2", "C3"], ["G2", "B2", "D3"]]
mel = ["E5", "G5", "C6", "B5", "A5", "G5", "E5", "D5", "C5", "D5", "E5", "G5", "A5", "G5", "E5", "-"]
for i in range(int(secs / beat)):
    tt = i * beat; chd = prog[(i // 4) % 4]
    for nn in chd: T.add(tt, S.rhodes(S.hz(nn) * 2, beat * 0.9, 0.5), 0.08, 0.2)
    T.add(tt, S.bass(S.hz(chd[0]), beat * 0.7, 0.8), 0.2)
    T.add(tt, S.kick(0.6) if i % 2 == 0 else S.snare(0.5), 0.2)
    T.add(tt + beat / 2, S.hat(0.4), 0.07)
    nn = mel[i % len(mel)]
    if nn != "-": T.add(tt, S.whistle(S.hz(nn), beat * 0.8, 0.7), 0.12, -0.2, 0.3)
song = T.render(S.make_ir(1.2)).mean(1)
mix = song * 0.5
hlen = min(len(host), int(secs * SR) - SR)
duck = np.ones(len(mix)); duck[:hlen + SR] = 0.25
mix = mix * duck
mix[int(0.3 * SR):int(0.3 * SR) + hlen] += host[:hlen] * 1.2
mix = S.bp(mix, 300, 4200) + S.RNG.normal(0, 0.004, len(mix))
wav("A_Life_Radio", mix, 0.6)
print("LIFE_ASSETS_DONE")
