# The TV of the living room (docs/level/LIFE.md, "ТВ"): nine invented channels, each a seamless loop (picture and sound end where they
# begin), 960x540 H.264 + AAC, ready for the WMF media player of the packaged game.
#   python Tools/life/make_tv_channels.py [only ids]   -> SourceArt/Life/tv/out/*.mp4, copied to Content/Movies/Life/
# Sources: four Higgsfield clips (SourceArt/Life/tv/hf: cartoon, ant nest, news studio, football-like match) and two voice lines;
# the rest is drawn here (PIL frames), the music is ours (Tools/audio/synth.py) and the Russian voices are Windows' Pavel / Irina
# (Tools/life/tts.ps1). Nothing here shows a real brand, person, flag or logo.
import os, sys, math, subprocess, shutil, random, json
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from scipy.io import wavfile

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(PROJ, "Tools", "audio"))
import synth as S

SRC = os.path.join(PROJ, "SourceArt", "Life", "tv")
HF = os.path.join(SRC, "hf")
VO = os.path.join(SRC, "vo")
TMP = os.path.join(SRC, "tmp")
OUT = os.path.join(SRC, "out")
MOV = os.path.join(PROJ, "Content", "Movies", "Life")
FONTS = os.path.join(PROJ, "Content", "UI", "Fonts")
for d in (VO, TMP, OUT, MOV):
    os.makedirs(d, exist_ok=True)
W, H, FPS = 960, 540, 24
SR = S.SR
ONLY = sys.argv[1:]

def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), size)
RUSSO = lambda s: font("RussoOne-Regular.ttf", s)
SANS = lambda s: font("PTSansNarrow-Bold.ttf", s)
HAND = lambda s: font("Neucha-Regular.ttf", s)

def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        print("FAILED", " ".join(cmd)[:400]); print(r.stderr[-2000:]); raise SystemExit(1)
    return r.stdout

def dur(f):
    return float(run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", f]).strip())

# ---------------------------------------------------------------- audio helpers
def tts(name, text, voice="Pavel", rate=1.0, pitch=1.0):
    out = os.path.join(VO, name + ".wav")
    if not os.path.exists(out):
        tf = os.path.join(VO, name + ".txt")
        open(tf, "w", encoding="utf-8").write(text)
        run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", os.path.join(HERE, "tts.ps1"), "-Voice", voice, "-TextFile", tf, "-Out", out,
             "-Rate", str(rate), "-Pitch", str(pitch)])
    return out

def load(f, gain=1.0, speed=1.0):
    """Any audio file -> float stereo at SR (ffmpeg decodes; speed changes tempo and pitch together)."""
    tmp = os.path.join(TMP, "dec_%d.wav" % abs(hash((f, speed))))
    af = "aresample=%d" % SR if speed == 1.0 else "asetrate=%d,aresample=%d" % (int(SR * speed), SR)
    run(["ffmpeg", "-v", "error", "-y", "-i", f, "-vn", "-af", af, "-ac", "2", "-ar", str(SR), tmp])
    sr, x = wavfile.read(tmp)
    x = x.astype(np.float32) / 32768.0
    return x * gain

def mix_into(buf, x, t, gain=1.0):
    i = int(t * SR)
    n = min(len(x), len(buf) - i)
    if n > 0:
        buf[i:i + n] += x[:n] * gain

def save_wav(path, x):
    x = np.clip(x, -1, 1)
    wavfile.write(path, SR, (x * 32767).astype(np.int16))

def norm(x, peak=0.85):
    m = np.abs(x).max()
    return x * (peak / m) if m > 1e-6 else x

def mono(x):
    return x if x.ndim == 1 else x.mean(axis=1)

def stereo(m):
    return np.stack([m, m], axis=1)

# ---------------------------------------------------------------- video helpers
def encode(frames_dir, wav, out, seconds):
    run(["ffmpeg", "-v", "error", "-y", "-framerate", str(FPS), "-i", os.path.join(frames_dir, "f%05d.jpg"), "-i", wav, "-t", "%.3f" % seconds,
         "-c:v", "libx264", "-preset", "slow", "-crf", "22", "-pix_fmt", "yuv420p", "-profile:v", "main", "-g", str(FPS), "-r", str(FPS),
         "-c:a", "aac", "-b:a", "128k", "-ar", "48000", "-ac", "2", "-movflags", "+faststart", out])

def frames(name):
    d = os.path.join(TMP, name)
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(d)
    return d

def put(d, i, im):
    im.convert("RGB").save(os.path.join(d, "f%05d.jpg" % i), quality=92)

def clip_frames(src, d, start_index, seconds=None, vf="", t0=0.0):
    """Decodes a clip into d as f%05d.jpg from start_index on (scaled to W x H plus an optional filter). Returns the count."""
    tmpd = os.path.join(TMP, "_clip")
    if os.path.isdir(tmpd): shutil.rmtree(tmpd)
    os.makedirs(tmpd)
    chain = "scale=%d:%d:flags=lanczos,fps=%d" % (W, H, FPS) + ("," + vf if vf else "")
    cmd = ["ffmpeg", "-v", "error", "-y", "-ss", "%.3f" % t0, "-i", src]
    if seconds: cmd += ["-t", "%.3f" % seconds]
    run(cmd + ["-vf", chain, "-q:v", "2", os.path.join(tmpd, "c%05d.jpg")])
    names = sorted(os.listdir(tmpd))
    for k, n in enumerate(names):
        shutil.move(os.path.join(tmpd, n), os.path.join(d, "f%05d.jpg" % (start_index + k)))
    return len(names)

def outline_text(dr, xy, text, fnt, fill, stroke=(0, 0, 0), sw=3, anchor="mm"):
    dr.text(xy, text, font=fnt, fill=fill, stroke_width=sw, stroke_fill=stroke, anchor=anchor)

def vgrad(w, h, top, bottom):
    a = np.linspace(0, 1, h)[:, None, None]
    img = (np.array(top)[None, None, :] * (1 - a) + np.array(bottom)[None, None, :] * a)
    return Image.fromarray(np.repeat(img, w, axis=1).astype(np.uint8))

def card(title, sub, top, bottom, title_font, color=(255, 255, 255), sub_color=(255, 230, 160)):
    im = vgrad(W, H, top, bottom)
    dr = ImageDraw.Draw(im)
    outline_text(dr, (W // 2, H // 2 - 30), title, title_font, color, sw=4)
    if sub:
        outline_text(dr, (W // 2, H // 2 + 50), sub, SANS(34), sub_color, sw=2)
    return im

def overlay_on(d, first, count, layer_fn):
    """Composites layer_fn(frame_index_in_channel) (RGBA W x H or None) over frames first..first+count-1."""
    for i in range(first, first + count):
        p = os.path.join(d, "f%05d.jpg" % i)
        lay = layer_fn(i)
        if lay is None: continue
        im = Image.open(p).convert("RGBA")
        im.alpha_composite(lay)
        im.convert("RGB").save(p, quality=92)

def finish(cid, d, nframes, audio):
    seconds = nframes / FPS
    n = int(round(seconds * SR))
    if len(audio) < n:
        audio = np.concatenate([audio, np.zeros((n - len(audio), 2))])
    audio = audio[:n]
    # loop seam: a 30 ms fade at both ends (the picture cuts there too)
    k = int(0.03 * SR); ramp = np.linspace(0, 1, k)[:, None]
    audio[:k] *= ramp; audio[-k:] *= ramp[::-1]
    wav = os.path.join(TMP, cid + ".wav")
    save_wav(wav, audio)
    out = os.path.join(OUT, cid + ".mp4")
    encode(d, wav, out, seconds)
    shutil.copy(out, os.path.join(MOV, cid + ".mp4"))
    print("TVCH %s %.1f s %d KB" % (cid, seconds, os.path.getsize(out) // 1024))
    return seconds

# ---------------------------------------------------------------- music beds (seamless, synth.py)
def bed_cartoon(seconds):
    T = S.Track(seconds); beat = 60 / 150.0
    melody = ["C5", "E5", "G5", "E5", "F5", "A5", "G5", "E5", "D5", "F5", "E5", "C5", "D5", "B4", "C5", "G4"]
    n = int(seconds / beat)
    for i in range(n):
        t = i * beat
        T.add(t, S.tuba(S.hz(["C2", "G2", "F2", "G2"][(i // 4) % 4]), beat * 0.8, 0.9), 0.55, -0.2)
        T.add(t, S.glock(S.hz(melody[i % len(melody)]), beat * 0.7, 0.9), 0.35, 0.3, 0.2)
        if i % 2 == 0: T.add(t, S.woodblock(0.8, 1300 if i % 4 == 0 else 1000), 0.3, 0.1)
        if i % 8 == 7: T.add(t + beat * 0.5, S.whistle(S.hz("C6"), beat * 0.4, 0.8), 0.2, 0.4)
    return T.render(S.make_ir(1.0))

def bed_pad(seconds, chords, cut=1200.0, gain=0.4):
    T = S.Track(seconds); step = seconds / len(chords)
    for k, ch in enumerate(chords):
        for nn in ch:
            T.add(k * step, S.pad(S.hz(nn), step * 1.05, 0.8, cut), gain, random.uniform(-0.4, 0.4), 0.4)
    return T.render(S.make_ir(2.2))

def bed_news(seconds):
    T = S.Track(seconds); beat = 60 / 112.0
    for i in range(int(seconds / beat)):
        t = i * beat
        T.add(t, S.kick(0.7), 0.25)
        if i % 2 == 1: T.add(t, S.rim(0.6), 0.15, 0.2)
        T.add(t, S.hat(0.5), 0.08, -0.3); T.add(t + beat / 2, S.hat(0.35), 0.06, -0.3)
        T.add(t, S.pad(S.hz(["D3", "D3", "Bb2", "C3"][(i // 8) % 4]), beat * 1.05, 0.6, 900), 0.12, 0.0, 0.2)
    return T.render(S.make_ir(1.2))

def sting_news():
    T = S.Track(2.5)
    for k, nn in enumerate(["D4", "A4", "D5", "F5"]):
        T.add(k * 0.12, S.brass(S.hz(nn), 0.9, 1.0), 0.35, (k - 1.5) * 0.3, 0.3)
    T.add(0.0, S.timpani(S.hz("D2"), 1.0), 0.6); T.add(0.48, S.crash(0.8, 1.5), 0.25)
    return T.render(S.make_ir(1.6))

def crowd(seconds, swells=()):
    n = int(seconds * SR)
    x = S.bp(S.RNG.normal(0, 1, (n, 2)), 300, 2600) * 0.08
    env = np.ones(n) * 0.6
    for t0, a in swells:
        i = int(t0 * SR); m = int(2.5 * SR)
        e = np.exp(-np.linspace(0, 4, m)) * a
        env[i:i + m] += e[:max(0, min(m, n - i))]
    return x * env[:, None]

def birdsong(seconds, seed=4):
    rng = np.random.default_rng(seed); n = int(seconds * SR); out = np.zeros((n, 2))
    t = 0.2
    while t < seconds - 0.6:
        f0 = rng.uniform(2600, 5200); cnt = rng.integers(2, 7); pan = rng.uniform(-0.7, 0.7)
        for c in range(cnt):
            L = int(rng.uniform(0.04, 0.11) * SR); tt = np.arange(L) / SR
            sweep = f0 * (1 + rng.uniform(-0.35, 0.35) * tt / tt[-1])
            s = np.sin(S.phase(sweep, L)) * np.sin(np.pi * tt / tt[-1]) ** 2 * 0.25
            i = int((t + c * rng.uniform(0.07, 0.14)) * SR)
            if i + L < n:
                out[i:i + L, 0] += s * math.cos((pan + 1) * math.pi / 4); out[i:i + L, 1] += s * math.sin((pan + 1) * math.pi / 4)
        t += rng.uniform(0.5, 1.6)
    return out

def bed_ominous(seconds):
    T = S.Track(seconds)
    chords = [["A2", "E3", "A3", "C4"], ["F2", "C3", "F3", "A3"], ["D2", "A2", "D3", "F3"], ["E2", "B2", "E3", "G#3"]]
    step = seconds / 4
    for k, ch in enumerate(chords):
        for nn in ch:
            T.add(k * step, S.pad(S.hz(nn), step * 1.05, 0.8, 800), 0.28, random.uniform(-0.5, 0.5), 0.5)
    mel = ["A5", "C6", "B5", "E5", "A5", "C6", "E6", "D6", "C6", "B5", "A5", "G#5"]
    for i, nn in enumerate(mel):
        T.add(0.4 + i * (seconds - 1.0) / len(mel), S.glock(S.hz(nn), 0.6, 0.7), 0.22, 0.35, 0.6)
    return T.render(S.make_ir(2.6, 3500))

def bed_kids(seconds):
    T = S.Track(seconds); beat = 60 / 120.0
    mel = ["C5", "C5", "G5", "G5", "A5", "A5", "G5", "-", "F5", "F5", "E5", "E5", "D5", "D5", "C5", "-"]
    for i in range(int(seconds / beat)):
        t = i * beat; nn = mel[i % len(mel)]
        if nn != "-": T.add(t, S.glock(S.hz(nn), beat * 0.8, 0.9), 0.3, 0.2, 0.2)
        T.add(t, S.bass(S.hz(["C3", "F3", "G3", "C3"][(i // 4) % 4]), beat * 0.6, 0.8), 0.25, -0.2)
        if i % 2 == 0: T.add(t, S.shaker(0.6), 0.12, 0.4)
        else: T.add(t, S.clap(0.5), 0.12)
    return T.render(S.make_ir(1.0))

def bed_jazz(seconds):
    T = S.Track(seconds); beat = 60 / 96.0
    prog = [["D3", "F3", "A3", "C4"], ["G2", "F3", "B3", "E4"], ["C3", "E3", "G3", "B3"], ["A2", "E3", "G3", "C#4"]]
    for i in range(int(seconds / beat)):
        t = i * beat; ch = prog[(i // 4) % 4]
        if i % 4 in (0, 2):
            for nn in ch: T.add(t, S.rhodes(S.hz(nn), beat * 1.5, 0.6), 0.12, random.uniform(-0.3, 0.3), 0.3)
        T.add(t, S.bass(S.hz(ch[0]) * (1.0 if i % 2 == 0 else 1.5), beat * 0.8, 0.7), 0.22)
        T.add(t, S.brush(0.8), 0.15, 0.3)
    return T.render(S.make_ir(1.4))

def bed_radio(seconds):
    T = S.Track(seconds); beat = 60 / 104.0
    prog = [["C3", "E3", "G3"], ["A2", "C3", "E3"], ["F2", "A2", "C3"], ["G2", "B2", "D3"]]
    mel = ["E5", "G5", "C6", "B5", "A5", "G5", "E5", "D5", "C5", "D5", "E5", "G5", "A5", "G5", "E5", "-"]
    for i in range(int(seconds / beat)):
        t = i * beat; ch = prog[(i // 4) % 4]
        for nn in ch: T.add(t, S.rhodes(S.hz(nn) * 2, beat * 0.9, 0.5), 0.08, 0.2)
        T.add(t, S.bass(S.hz(ch[0]), beat * 0.7, 0.8), 0.2)
        T.add(t, S.kick(0.6) if i % 2 == 0 else S.snare(0.5), 0.2)
        T.add(t + beat / 2, S.hat(0.4), 0.07)
        nn = mel[i % len(mel)]
        if nn != "-": T.add(t, S.whistle(S.hz(nn), beat * 0.8, 0.7), 0.12, -0.2, 0.3)
    return T.render(S.make_ir(1.2))

# ================================================================ the channels
CHANNELS = []

def channel(cid):
    def deco(f):
        CHANNELS.append((cid, f)); return f
    return deco

@channel("ch0_cartoon")
def ch_cartoon(cid):
    d = frames(cid)
    title = card("КОТ ПИРОЖОК", "и мышь Кнопка · серия 12", (255, 200, 60), (230, 90, 40), RUSSO(78))
    dr = ImageDraw.Draw(title)
    for k in range(14):
        a = k / 14 * 2 * math.pi
        dr.ellipse([W // 2 + 380 * math.cos(a) - 10, H // 2 + 220 * math.sin(a) - 10, W // 2 + 380 * math.cos(a) + 10, H // 2 + 220 * math.sin(a) + 10], fill=(255, 245, 200))
    i = 0
    for k in range(int(2.0 * FPS)):
        z = 1.0 + 0.04 * math.sin(k / FPS * 6)
        im = title.resize((int(W * z), int(H * z))).crop((int((W * z - W) / 2), int((H * z - H) / 2), int((W * z - W) / 2) + W, int((H * z - H) / 2) + H))
        put(d, i, im); i += 1
    src = os.path.join(HF, "cartoon.mp4")
    i += clip_frames(src, d, i)
    i += clip_frames(src, d, i, vf="hflip,setpts=1.1*PTS,crop=iw*0.86:ih*0.86:iw*0.07:ih*0.1,scale=%d:%d" % (W, H))
    end = card("ПРОДОЛЖЕНИЕ СЛЕДУЕТ…", "", (90, 60, 160), (40, 20, 90), RUSSO(54))
    for k in range(int(1.5 * FPS)):
        put(d, i, end); i += 1
    seconds = i / FPS
    audio = bed_cartoon(seconds) * 0.9
    clip = load(src, 0.8)
    mix_into(audio, clip, 2.0)
    mix_into(audio, load(src, 0.7, 1.0 / 1.1), 2.0 + 10.04)
    return finish(cid, d, i, norm(audio))

@channel("ch1_nature")
def ch_nature(cid):
    d = frames(cid)
    title = card("ЦАРСТВО МУРАВЬЁВ", "документальный цикл · фильм третий", (20, 30, 20), (5, 10, 5), SANS(76), (240, 235, 210), (200, 190, 150))
    i = 0
    for k in range(int(2.5 * FPS)):
        a = min(1.0, k / 12.0, (int(2.5 * FPS) - k) / 10.0)
        put(d, i, Image.blend(Image.new("RGB", (W, H)), title, max(0.0, a))); i += 1
    src = os.path.join(HF, "antnest.mp4")
    i += clip_frames(src, d, i)
    i += clip_frames(src, d, i, vf="setpts=1.25*PTS,crop=iw*0.7:ih*0.7:iw*0.15+iw*0.1*t/12:ih*0.15,scale=%d:%d" % (W, H))
    seconds = i / FPS
    vo = tts("nature", "Муравьи... Удивительные создания. В темноте подземных галерей тысячи рабочих трудятся без отдыха. "
             "Каждая крошка — богатство. Каждый сантиметр — подвиг. А где-то наверху, на кухне, уже остывает праздничный торт. И колония это знает.",
             "Pavel", 0.92, 0.9)
    audio = bed_pad(seconds, [["A2", "E3", "A3", "C4"], ["F2", "C3", "F3", "A3"], ["C3", "G3", "C4", "E4"], ["G2", "D3", "G3", "B3"]], 1100, 0.35)
    mix_into(audio, load(os.path.join(HF, "antnest.mp4"), 0.25), 2.5)
    mix_into(audio, load(vo, 1.6), 2.0)
    return finish(cid, d, i, norm(audio))

@channel("ch2_news")
def ch_news(cid):
    d = frames(cid)
    i = 0
    for k in range(int(2.5 * FPS)):
        t = k / FPS
        im = vgrad(W, H, (20, 40, 110), (5, 10, 40)); dr = ImageDraw.Draw(im)
        r = int(60 + 900 * min(1, t / 1.2))
        dr.ellipse([W // 2 - r, H // 2 - r, W // 2 + r, H // 2 + r], outline=(90, 140, 255), width=6)
        if t > 0.5: outline_text(dr, (W // 2, H // 2), "НОВОСТИ ЧАСА", RUSSO(80), (255, 255, 255), (10, 20, 60), 4)
        if t > 0.9: outline_text(dr, (W // 2, H // 2 + 70), "вечерний выпуск", SANS(32), (180, 210, 255), (10, 20, 60), 2)
        put(d, i, im); i += 1
    first = i
    src = os.path.join(HF, "news.mp4")
    i += clip_frames(src, d, i)
    i += clip_frames(src, d, i, vf="setpts=1.0*PTS,crop=iw*0.62:ih*0.62:iw*0.19:ih*0.12,scale=%d:%d" % (W, H))
    # lower third and the ticker
    lt = Image.new("RGBA", (W, H), (0, 0, 0, 0)); dr = ImageDraw.Draw(lt)
    dr.rectangle([0, H - 128, W, H - 64], fill=(200, 20, 30, 235))
    dr.rectangle([0, H - 128, 150, H - 64], fill=(255, 255, 255, 255))
    dr.text((75, H - 96), "СРОЧНО", font=RUSSO(30), fill=(200, 20, 30), anchor="mm")
    dr.text((170, H - 96), "Нашествие муравьёв: пропал праздничный торт", font=SANS(36), fill=(255, 255, 255), anchor="lm")
    dr.rectangle([0, H - 64, W, H - 26], fill=(12, 20, 50, 235))
    tick = "  •  Учёные: муравьи научились открывать холодильник  •  Синоптики обещают крошки к вечеру  •  Робот-пылесос признан героем недели  •  Кот снова спал на работе  •  Сахар подорожал на одну ложку  "
    tw = int(SANS(28).getlength(tick)) + 40
    strip = Image.new("RGBA", (tw, 38), (0, 0, 0, 0)); ImageDraw.Draw(strip).text((0, 19), tick, font=SANS(28), fill=(255, 230, 150), anchor="lm")
    def layer(k):
        L = lt.copy()
        x = W - int(((k - first) / FPS * 110) % (tw + W))
        L.alpha_composite(strip, (x, H - 64)) if x > -tw else None
        if x + tw < W: L.alpha_composite(strip, (x + tw, H - 64))
        return L
    overlay_on(d, first, i - first, layer)
    seconds = i / FPS
    audio = np.zeros((int(seconds * SR) + 10, 2))
    mix_into(audio, sting_news(), 0.0, 0.9)
    bed = bed_news(seconds - 2.5)
    mix_into(audio, bed * 0.35, 2.5)
    mix_into(audio, load(os.path.join(HF, "news_vo.mp3"), 1.4), 2.8)
    return finish(cid, d, i, norm(audio))

@channel("ch3_sport")
def ch_sport(cid):
    d = frames(cid)
    src = os.path.join(HF, "sport.mp4")
    i = clip_frames(src, d, 0)
    first_rep = i
    i += clip_frames(src, d, i, vf="setpts=1.7*PTS,crop=iw*0.6:ih*0.6:iw*0.38:ih*0.2,scale=%d:%d" % (W, H), t0=6.0)
    board = Image.new("RGBA", (W, H), (0, 0, 0, 0)); dr = ImageDraw.Draw(board)
    dr.rounded_rectangle([24, 20, 360, 66], 8, fill=(10, 15, 30, 225))
    dr.rectangle([24, 20, 70, 66], fill=(240, 180, 20, 255))
    dr.text((47, 43), "67'", font=SANS(26), fill=(10, 10, 10), anchor="mm")
    dr.text((80, 43), "ЖУКИ", font=SANS(30), fill=(255, 255, 255), anchor="lm")
    dr.text((215, 43), "0 : 0", font=RUSSO(28), fill=(255, 230, 120), anchor="mm")
    dr.text((262, 43), "ПЧЁЛЫ", font=SANS(30), fill=(255, 255, 255), anchor="lm")
    dr.text((W - 30, 40), "ЛИГА ДВОРОВ", font=RUSSO(24), fill=(255, 255, 255, 200), anchor="rm", stroke_width=2, stroke_fill=(0, 0, 0))
    rep = board.copy(); dr2 = ImageDraw.Draw(rep)
    dr2.rounded_rectangle([W - 210, H - 80, W - 30, H - 34], 8, fill=(200, 20, 30, 230))
    dr2.text((W - 120, H - 57), "ПОВТОР", font=RUSSO(28), fill=(255, 255, 255), anchor="mm")
    overlay_on(d, 0, i, lambda k: rep if k >= first_rep else board)
    seconds = i / FPS
    vo = tts("sport", "И мяч у «Жуков»! Проход по флангу, прострел... Удар! Вратарь в прыжке — нет, мимо! Какой момент, друзья! Лига Дворов, счёт по-прежнему ноль-ноль!",
             "Pavel", 1.25, 1.1)
    audio = crowd(seconds, [(7.0, 1.2), (10.5, 0.6), (15.0, 0.8)])
    mix_into(audio, load(src, 0.7), 0.0)
    mix_into(audio, load(src, 0.5, 1 / 1.7)[int(6.0 / 1.7 * SR):], first_rep / FPS)
    mix_into(audio, load(vo, 1.5), 0.6)
    return finish(cid, d, i, norm(audio))

def draw_can(dr, cx, cy, s, tilt=0.0, frost=0.0):
    """The «Белая Погода» spray can: a white body with a pale blue snowflake band, a silver cap, a nozzle."""
    w, h = 110 * s, 300 * s
    x0, y0 = cx - w / 2, cy - h / 2
    for k in range(int(w)):
        u = k / w
        shade = 0.55 + 0.45 * math.sin(math.pi * u) ** 0.6 + (0.35 if 0.25 < u < 0.33 else 0.0)
        c = tuple(int(min(255, v * shade)) for v in (235, 240, 248))
        dr.line([(x0 + k, y0 + 40 * s), (x0 + k, y0 + h)], fill=c)
    dr.rectangle([x0, y0 + 120 * s, x0 + w, y0 + 200 * s], fill=(150, 200, 240))
    for k in range(6):
        a = k / 6 * math.pi
        dr.line([(cx - 28 * s * math.cos(a), y0 + 160 * s - 28 * s * math.sin(a)), (cx + 28 * s * math.cos(a), y0 + 160 * s + 28 * s * math.sin(a))], fill=(255, 255, 255), width=max(2, int(5 * s)))
    dr.ellipse([x0 + 6 * s, y0 + 10 * s, x0 + w - 6 * s, y0 + 70 * s], fill=(200, 205, 215))
    dr.rectangle([cx - 14 * s, y0 - 4 * s, cx + 14 * s, y0 + 30 * s], fill=(180, 185, 195))
    dr.rectangle([cx + 10 * s, y0 + 2 * s, cx + 26 * s, y0 + 14 * s], fill=(120, 125, 135))

def draw_ant(dr, x, y, s, col=(30, 20, 15)):
    for k in (-1, 0, 1):
        dr.line([(x + k * 9 * s, y), (x + k * 12 * s - 6 * s, y + 12 * s)], fill=col, width=max(1, int(2 * s)))
        dr.line([(x + k * 9 * s, y), (x + k * 12 * s + 6 * s, y + 12 * s)], fill=col, width=max(1, int(2 * s)))
    dr.ellipse([x - 22 * s, y - 7 * s, x - 6 * s, y + 7 * s], fill=col)
    dr.ellipse([x - 7 * s, y - 5 * s, x + 7 * s, y + 5 * s], fill=col)
    dr.ellipse([x + 6 * s, y - 6 * s, x + 18 * s, y + 6 * s], fill=col)
    dr.line([(x + 16 * s, y - 4 * s), (x + 26 * s, y - 14 * s)], fill=col, width=max(1, int(2 * s)))

@channel("ch4_ad")
def ch_ad(cid):
    d = frames(cid)
    seconds = 15.0; n = int(seconds * FPS)
    rng = random.Random(3)
    mist = []
    flakes = [(rng.uniform(0, W), rng.uniform(-H, H), rng.uniform(1.5, 5), rng.uniform(20, 60)) for _ in range(140)]
    for k in range(n):
        t = k / FPS
        if t < 9.0:
            im = vgrad(W, H, (215, 235, 245), (170, 200, 220)); dr = ImageDraw.Draw(im)
            dr.rectangle([0, 380, W, H], fill=(245, 245, 248)); dr.line([(0, 380), (W, 380)], fill=(255, 255, 255), width=4)
            dr.rectangle([0, 392, W, 400], fill=(220, 225, 232))
            for j in range(5):
                ax = (t * 70 + j * 170) % (W + 200) - 100
                frozen = max(0.0, min(1.0, (t - 5.2 - j * 0.15) * 1.5)) if ax > 420 else 0.0
                if frozen > 0: ax = min(ax, 420 + j * 60)
                col = tuple(int(a * (1 - frozen) + b * frozen) for a, b in zip((40, 25, 15), (190, 215, 240)))
                draw_ant(dr, ax if j != 2 or t < 5.5 else ax - (t - 5.5) * 260, 430 + (j % 2) * 30, 1.4, col)
            draw_can(dr, 300, 260, 1.0)
            if 1.0 < t < 5.0 and int(t * 3) % 2 == 0:
                outline_text(dr, (W - 230, 90), "НОВИНКА!", RUSSO(56), (255, 255, 255), (40, 120, 200), 4)
            if 5.0 <= t < 9.0:
                if len(mist) < 400:
                    for _ in range(14): mist.append([320, 120, rng.uniform(120, 420), rng.uniform(-60, 60), rng.uniform(8, 20), 0.0])
                lay = Image.new("RGBA", (W, H), (0, 0, 0, 0)); dl = ImageDraw.Draw(lay)
                for m in mist:
                    m[0] += m[2] / FPS; m[1] += m[3] / FPS; m[2] *= 0.97; m[4] += 18 / FPS; m[5] += 1 / FPS
                    a = int(max(0, 170 - m[5] * 45))
                    dl.ellipse([m[0] - m[4], m[1] - m[4], m[0] + m[4], m[1] + m[4]], fill=(255, 255, 255, a))
                lay = lay.filter(ImageFilter.GaussianBlur(6))
                fr = min(1.0, (t - 5.0) / 3.5)
                fm = np.zeros((H, W), np.float32)
                yy, xx = np.mgrid[0:H, 0:W]
                edge = np.minimum(np.minimum(xx, W - xx), np.minimum(yy, H - yy)) / 260.0
                noise = np.random.default_rng(k // 3).random((H // 8 + 1, W // 8 + 1)).repeat(8, 0).repeat(8, 1)[:H, :W]
                fm = np.clip((fr * 1.3 - edge) * 3 + noise * 0.3 - 0.2, 0, 0.85)
                frost = Image.fromarray(np.dstack([np.full((H, W), 235), np.full((H, W), 245), np.full((H, W), 255), (fm * 255)]).astype(np.uint8), "RGBA")
                im = im.convert("RGBA"); im.alpha_composite(lay); im.alpha_composite(frost)
                outline_text(ImageDraw.Draw(im), (W // 2, 470 if t < 8 else 470), "П-Ш-Ш-Ш-Ш...", HAND(44), (60, 90, 140), (255, 255, 255), 2)
        else:
            u = t - 9.0
            im = vgrad(W, H, (225, 235, 250), (150, 175, 215)); dr = ImageDraw.Draw(im)
            for (fx, fy, fs, fv) in flakes:
                y = (fy + u * fv) % (H + 20)
                dr.ellipse([fx - fs, y - fs, fx + fs, y + fs], fill=(255, 255, 255))
            a = min(1.0, u / 0.6)
            sz = int(84 + 8 * (1 - a))
            outline_text(dr, (W // 2 + 60, 200), "БЕЛАЯ ПОГОДА", RUSSO(sz), (255, 255, 255), (60, 110, 170), 5)
            draw_can(dr, 150, 300, 0.85)
            if u > 1.2: outline_text(dr, (W // 2 + 60, 290), "Тихая. Белая. Вечная.", SANS(44), (40, 70, 120), (255, 255, 255), 2)
            if u > 2.6: outline_text(dr, (W // 2 + 60, 350), "И никто не придёт.", SANS(40), (120, 20, 30), (255, 255, 255), 2)
            dr.text((W // 2, H - 22), "Не распылять на членов семьи, кота и праздничный торт. Хранить в недоступном для муравьёв месте.", font=SANS(17), fill=(40, 50, 70), anchor="mm")
        put(d, k, im)
    vo1 = tts("ad1", "Устали от незваных гостей?", "Pavel", 0.85, 0.75)
    vo2 = tts("ad2", "Белая Погода. Тихая. Белая. Вечная. И никто не придёт.", "Pavel", 0.8, 0.7)
    audio = bed_ominous(seconds) * 0.8
    mix_into(audio, load(vo1, 1.5), 0.8)
    sp = S.bp(S.RNG.normal(0, 1, (int(3.2 * SR), 2)), 1500, 9000) * 0.25 * np.minimum(1, np.linspace(0, 6, int(3.2 * SR)))[:, None] * np.exp(-np.linspace(0, 2, int(3.2 * SR)))[:, None]
    mix_into(audio, sp, 5.0)
    T = S.Track(2.0); T.add(0, S.timpani(S.hz("A1"), 1.0), 0.7); T.add(0, S.crash(0.9, 1.8), 0.3)
    mix_into(audio, T.render(S.make_ir(2.0)), 9.0)
    mix_into(audio, load(vo2, 1.5), 9.6)
    return finish(cid, d, n, norm(audio))

@channel("ch5_cooking")
def ch_cooking(cid):
    d = frames(cid)
    cake_dir = os.path.join(SRC, "cake_render")
    files = sorted(f for f in os.listdir(cake_dir) if f.endswith(".png")) if os.path.isdir(cake_dir) else []
    i = 0
    title = card("ВКУСНО ЗА 5 МИНУТ", "сегодня: торт «Муравейник»", (250, 230, 200), (220, 160, 110), RUSSO(64), (120, 50, 30), (90, 40, 20))
    for k in range(int(2.0 * FPS)):
        put(d, i, title); i += 1
    ov = Image.new("RGBA", (W, H), (0, 0, 0, 0)); dr = ImageDraw.Draw(ov)
    dr.rounded_rectangle([20, H - 92, 470, H - 30], 12, fill=(255, 250, 240, 230))
    dr.text((40, H - 61), "Шаг 3: украшаем клубникой", font=SANS(34), fill=(120, 50, 30), anchor="lm")
    dr.text((W - 24, 30), "ВКУСНО ЗА 5 МИНУТ", font=RUSSO(22), fill=(255, 255, 255), anchor="rm", stroke_width=2, stroke_fill=(120, 50, 30))
    first = i
    if files:
        for f in files:
            im = Image.open(os.path.join(cake_dir, f)).convert("RGB").resize((W, H), Image.LANCZOS)
            put(d, i, im); i += 1
    else:
        for k in range(10 * FPS):
            im = vgrad(W, H, (250, 235, 215), (215, 180, 140)); put(d, i, im); i += 1
    overlay_on(d, first, i - first, lambda k: ov)
    seconds = i / FPS
    vo = tts("cook", "Итак, наш торт почти готов. Последний штрих — клубника. Аккуратно, по кругу. Видите, какая красота? Только не оставляйте торт на столе на ночь... мало ли кто придёт.",
             "Irina", 0.95, 1.05)
    audio = bed_jazz(seconds) * 0.6
    mix_into(audio, load(vo, 1.5), 2.3)
    return finish(cid, d, i, norm(audio))

def draw_snail(dr, x, y, s, t):
    dr.ellipse([x - 70 * s, y - 10 * s, x + 90 * s, y + 30 * s], fill=(140, 210, 90), outline=(60, 120, 40), width=3)
    dr.ellipse([x + 55 * s, y - 70 * s, x + 105 * s, y + 10 * s], fill=(150, 220, 100), outline=(60, 120, 40), width=3)
    for k, dx in enumerate((68, 92)):
        ex, ey = x + dx * s, y - 100 * s + 6 * s * math.sin(t * 6 + k)
        dr.line([(x + (dx - 5) * s, y - 60 * s), (ex, ey)], fill=(60, 120, 40), width=4)
        dr.ellipse([ex - 12 * s, ey - 12 * s, ex + 12 * s, ey + 12 * s], fill=(255, 255, 255), outline=(30, 30, 30), width=2)
        dr.ellipse([ex - 5 * s, ey - 4 * s, ex + 5 * s, ey + 6 * s], fill=(20, 20, 20))
    dr.arc([x + 66 * s, y - 40 * s, x + 96 * s, y - 15 * s], 20, 160, fill=(40, 80, 30), width=3)
    cx, cy = x - 5 * s, y - 45 * s
    dr.ellipse([cx - 62 * s, cy - 62 * s, cx + 62 * s, cy + 62 * s], fill=(240, 150, 60), outline=(150, 70, 20), width=4)
    pts = [(cx + 3 * a * s * math.cos(a + t), cy + 3 * a * s * math.sin(a + t)) for a in np.linspace(0, 18, 120)]
    dr.line(pts, fill=(150, 70, 20), width=5)

def draw_bee(dr, x, y, s, t):
    wing = 20 * s * (0.6 + 0.4 * math.sin(t * 40))
    dr.ellipse([x - 30 * s, y - 40 * s - wing, x + 5 * s, y - 5 * s], fill=(220, 240, 255), outline=(120, 150, 200), width=2)
    dr.ellipse([x - 5 * s, y - 42 * s - wing, x + 30 * s, y - 7 * s], fill=(220, 240, 255), outline=(120, 150, 200), width=2)
    dr.ellipse([x - 45 * s, y - 25 * s, x + 45 * s, y + 25 * s], fill=(255, 210, 40), outline=(60, 40, 10), width=3)
    for k in (-18, 2, 22):
        dr.line([(x + k * s, y - 24 * s), (x + k * s, y + 24 * s)], fill=(40, 30, 10), width=int(9 * s))
    dr.ellipse([x + 30 * s, y - 20 * s, x + 66 * s, y + 16 * s], fill=(255, 220, 60), outline=(60, 40, 10), width=3)
    dr.ellipse([x + 46 * s, y - 10 * s, x + 58 * s, y + 2 * s], fill=(20, 20, 20))
    dr.arc([x + 42 * s, y - 2 * s, x + 62 * s, y + 10 * s], 10, 170, fill=(40, 30, 10), width=2)

@channel("ch6_kids")
def ch_kids(cid):
    d = frames(cid)
    vo = os.path.join(HF, "kids_vo.mp3")
    seconds = 16.0; n = int(seconds * FPS)
    bg = vgrad(W, H, (130, 210, 255), (200, 240, 255)); db = ImageDraw.Draw(bg)
    for k, c in enumerate([(240, 70, 70), (250, 150, 50), (250, 230, 70), (90, 200, 90), (70, 150, 240), (150, 90, 220)]):
        r = 520 - k * 34
        db.arc([W // 2 - r, 470 - r, W // 2 + r, 470 + r], 180, 360, fill=c, width=34)
    db.rectangle([0, 440, W, H], fill=(110, 200, 90))
    for k in range(9):
        fx = 60 + k * 110; db.line([(fx, 440), (fx, 410)], fill=(60, 140, 50), width=4)
        for p in range(5):
            a = p / 5 * 2 * math.pi
            db.ellipse([fx + 14 * math.cos(a) - 9, 400 + 14 * math.sin(a) - 9, fx + 14 * math.cos(a) + 9, 400 + 14 * math.sin(a) + 9], fill=[(255, 120, 160), (255, 255, 255), (255, 200, 60)][k % 3])
        db.ellipse([fx - 7, 393, fx + 7, 407], fill=(250, 220, 60))
    for k in range(n):
        t = k / FPS
        im = bg.copy(); dr = ImageDraw.Draw(im)
        sx, sy = 820, 90
        dr.ellipse([sx - 55, sy - 55, sx + 55, sy + 55], fill=(255, 230, 70))
        for r in range(12):
            a = r / 12 * 2 * math.pi + t * 0.5
            dr.line([(sx + 65 * math.cos(a), sy + 65 * math.sin(a)), (sx + 90 * math.cos(a), sy + 90 * math.sin(a))], fill=(255, 210, 60), width=5)
        dr.arc([sx - 25, sy - 5, sx + 25, sy + 30], 20, 160, fill=(150, 80, 20), width=4)
        dr.ellipse([sx - 22, sy - 20, sx - 10, sy - 8], fill=(150, 80, 20)); dr.ellipse([sx + 10, sy - 20, sx + 22, sy - 8], fill=(150, 80, 20))
        hop = abs(math.sin(t * math.pi * 2)) * 30
        draw_snail(dr, 330 + 40 * math.sin(t * 0.8), 420 - hop * 0.4, 1.3, t)
        draw_bee(dr, 600 + 120 * math.sin(t * 1.3), 220 + 60 * math.sin(t * 2.6), 1.2, t)
        if int(t * 2) % 4 in (1, 2):
            dr.rounded_rectangle([470, 300, 700, 360], 20, fill=(255, 255, 255), outline=(80, 80, 80), width=3)
            dr.text((585, 330), "Ля-ля-ля!", font=HAND(40), fill=(220, 60, 120), anchor="mm")
        dr.text((20, 20), "КАНАЛ «КАРАПУЗ»", font=RUSSO(26), fill=(255, 255, 255), stroke_width=3, stroke_fill=(220, 80, 140))
        put(d, k, im)
    audio = bed_kids(seconds) * 0.6
    mix_into(audio, load(vo, 1.4), 1.0)
    return finish(cid, d, n, norm(audio))

def draw_bird(dr, x, y, s, t, body, belly, flap=None, face=1):
    if flap is not None:
        wy = 26 * s * math.sin(flap)
        dr.polygon([(x - 6 * s * face, y - 2 * s), (x - 30 * s * face, y - 6 * s - wy), (x + 4 * s * face, y - 4 * s)], fill=tuple(max(0, c - 40) for c in body))
    dr.ellipse([x - 18 * s, y - 12 * s, x + 18 * s, y + 12 * s], fill=body)
    dr.ellipse([x - 10 * s, y - 2 * s, x + 14 * s, y + 12 * s], fill=belly)
    hx = x + 16 * s * face
    dr.ellipse([hx - 10 * s, y - 20 * s, hx + 10 * s, y], fill=body)
    dr.polygon([(hx + 9 * s * face, y - 12 * s), (hx + 18 * s * face, y - 9 * s), (hx + 9 * s * face, y - 6 * s)], fill=(60, 50, 40))
    dr.ellipse([hx + 2 * s * face - 2.5 * s, y - 15 * s, hx + 2 * s * face + 2.5 * s, y - 10 * s], fill=(10, 10, 10))
    dr.polygon([(x - 16 * s * face, y - 4 * s), (x - 34 * s * face, y - 10 * s), (x - 32 * s * face, y + 2 * s)], fill=tuple(max(0, c - 30) for c in body))

@channel("ch7_birds")
def ch_birds(cid):
    d = frames(cid)
    seconds = 18.0; n = int(seconds * FPS)
    bg = vgrad(W, H, (150, 195, 235), (220, 235, 225)); db = ImageDraw.Draw(bg)
    for k in range(6):
        cx = k * 190 + 40; db.ellipse([cx - 160, 300 - 90, cx + 160, 300 + 140], fill=(110 + k * 6, 160, 100))
    db.rectangle([0, 380, W, H], fill=(90, 150, 70))
    bg = bg.filter(ImageFilter.GaussianBlur(8)); db = ImageDraw.Draw(bg)
    db.rectangle([470, 250, 500, H], fill=(120, 85, 50))
    db.rectangle([330, 236, 640, 258], fill=(150, 105, 60)); db.rectangle([330, 230, 345, 258], fill=(130, 90, 50)); db.rectangle([625, 230, 640, 258], fill=(130, 90, 50))
    seeds = [(random.uniform(350, 620), random.uniform(228, 236)) for _ in range(40)]
    for sx, sy in seeds: db.ellipse([sx - 3, sy - 2, sx + 3, sy + 2], fill=(90, 70, 40))
    tits = [(400, 0.0), (520, 1.7), (580, 3.1)]
    for k in range(n):
        t = k / FPS
        im = bg.copy(); dr = ImageDraw.Draw(im)
        for j, (bx, ph) in enumerate(tits):
            hop = max(0.0, math.sin(t * 3 + ph)) ** 8 * 18
            peck = 6 if math.sin(t * 7 + ph * 3) > 0.6 else 0
            face = 1 if math.sin(t * 0.5 + ph) > 0 else -1
            body, belly = ((70, 120, 200), (250, 220, 60)) if j != 1 else ((150, 110, 80), (220, 200, 170))
            draw_bird(dr, bx + 10 * math.sin(t * 0.7 + ph), 214 - hop + peck, 1.2, t, body, belly, flap=(t * 30) if hop > 4 else None, face=face)
        for j in range(3):
            u = (t + j * 6.0) % 18.0
            fx = -60 + u * 70; fy = 120 + 40 * math.sin(u * 0.8 + j)
            draw_bird(dr, fx, fy, 0.9, t, (120, 90, 70), (200, 180, 150), flap=t * 22 + j, face=1)
        dr.text((W - 24, 28), "ПРИРОДА HD · ПТИЦЫ", font=SANS(26), fill=(255, 255, 255), anchor="rm", stroke_width=2, stroke_fill=(40, 60, 40))
        put(d, k, im)
    audio = bed_pad(seconds, [["C4", "G4", "E5"], ["A3", "E4", "C5"], ["F3", "C4", "A4"], ["G3", "D4", "B4"]], 2000, 0.15) + birdsong(seconds)
    return finish(cid, d, n, norm(audio, 0.7))

@channel("ch8_nosignal")
def ch_nosignal(cid):
    d = frames(cid)
    seconds = 8.0; n = int(seconds * FPS)
    test = Image.new("RGB", (W, H), (20, 20, 20)); dr = ImageDraw.Draw(test)
    cols = [(200, 200, 200), (200, 200, 30), (30, 200, 200), (30, 200, 30), (200, 30, 200), (200, 30, 30), (30, 30, 200)]
    for k, c in enumerate(cols):
        dr.rectangle([k * W // 7, 0, (k + 1) * W // 7, int(H * 0.7)], fill=c)
    for k in range(10):
        g = int(255 * k / 9); dr.rectangle([k * W // 10, int(H * 0.7), (k + 1) * W // 10, int(H * 0.82)], fill=(g, g, g))
    dr.ellipse([W // 2 - 170, H // 2 - 170, W // 2 + 170, H // 2 + 170], outline=(255, 255, 255), width=5)
    dr.rectangle([W // 2 - 220, H // 2 - 40, W // 2 + 220, H // 2 + 40], fill=(10, 10, 10))
    dr.text((W // 2, H // 2), "НЕТ СИГНАЛА", font=RUSSO(52), fill=(255, 255, 255), anchor="mm")
    dr.text((W // 2, H - 40), "ПРОВЕРЬТЕ АНТЕННУ · КАНАЛ 9", font=SANS(30), fill=(220, 220, 220), anchor="mm")
    rng = np.random.default_rng(7)
    for k in range(n):
        t = k / FPS
        if 3.0 <= t < 6.5:
            im = test
        else:
            nz = rng.integers(0, 255, (H // 6, W // 6), dtype=np.uint8).repeat(6, 0).repeat(6, 1)
            im = Image.fromarray(np.dstack([nz, nz, nz]))
        put(d, k, im)
    n_s = int(seconds * SR)
    hiss = S.bp(S.RNG.normal(0, 1, (n_s, 2)), 800, 12000) * 0.3
    tt = np.arange(n_s) / SR
    gate = ((tt < 3.0) | (tt >= 6.5)).astype(np.float32)[:, None]
    tone = stereo(np.sin(2 * np.pi * 1000 * tt) * 0.15) * (1 - gate)
    return finish(cid, d, n, hiss * gate + tone)

if __name__ == "__main__":
    report = {}
    for cid, f in CHANNELS:
        if ONLY and not any(cid.startswith(o) for o in ONLY):
            continue
        report[cid] = round(f(cid), 2)
    jp = os.path.join(OUT, "channels.json")
    old = json.load(open(jp)) if os.path.exists(jp) else {}
    old.update(report)
    json.dump(old, open(jp, "w"), indent=1)
    print("TV DONE", report)
