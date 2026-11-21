# UI art prep (branch code/ANT-ui): cuts the Higgsfield icon sheets into icons (flood-fill cut-out of the white page), sizes the
# plates, portraits and cards, writes SourceArt/UI/<Folder>/T_UI_*.jpg|png. Then Tools/make_ui_assets.py imports them.
# Usage: UI_RAW=<downloaded pngs>/ UI_OUT=SourceArt/UI/ python Tools/ui_art_prep.py bg por card mat icons logo
import os, sys
import numpy as np
from PIL import Image, ImageFilter, ImageDraw
from collections import deque
R=os.environ.get("UI_RAW", "Saved/ui/raw/")
O=os.environ.get("UI_OUT", "SourceArt/UI/")
def save_jpg(im, sub, name, q=90):
    os.makedirs(O+sub, exist_ok=True); im.convert('RGB').save(O+sub+'/T_UI_'+name+'.jpg', quality=q)
def save_png(im, sub, name):
    os.makedirs(O+sub, exist_ok=True); im.save(O+sub+'/T_UI_'+name+'.png', optimize=True)
def fit(im, w, h):
    sw, sh = im.size; s = max(w/sw, h/sh)
    im = im.resize((round(sw*s), round(sh*s)), Image.LANCZOS)
    x = (im.size[0]-w)//2; y = (im.size[1]-h)//2
    return im.crop((x, y, x+w, y+h))
def cutout(cell, thr=232):
    a = np.asarray(cell.convert('RGB')).astype(np.int16)
    h, w, _ = a.shape
    bright = (a.min(axis=2) >= thr) & ((a.max(axis=2)-a.min(axis=2)) < 22)
    bg = np.zeros((h, w), bool)
    q = deque()
    for x in range(w):
        for y in (0, h-1):
            if bright[y, x] and not bg[y, x]: bg[y, x] = True; q.append((y, x))
    for y in range(h):
        for x in (0, w-1):
            if bright[y, x] and not bg[y, x]: bg[y, x] = True; q.append((y, x))
    while q:
        y, x = q.popleft()
        for dy, dx in ((1,0),(-1,0),(0,1),(0,-1)):
            ny, nx = y+dy, x+dx
            if 0 <= ny < h and 0 <= nx < w and bright[ny, nx] and not bg[ny, nx]:
                bg[ny, nx] = True; q.append((ny, nx))
    alpha = Image.fromarray(np.where(bg, 0, 255).astype(np.uint8))
    alpha = alpha.filter(ImageFilter.MinFilter(3)).filter(ImageFilter.GaussianBlur(0.8))
    out = cell.convert('RGBA'); out.putalpha(alpha)
    bb = alpha.point(lambda v: 255 if v > 20 else 0).getbbox()
    return out, bb
def icons(sheet, names, size=256):
    im = Image.open(R+sheet+'.png').convert('RGB')
    W, H = im.size; cw, ch = W//4, H//4
    for i, n in enumerate(names):
        cx, cy = (i%4)*cw, (i//4)*ch
        cell = im.crop((cx, cy, cx+cw, cy+ch))
        out, bb = cutout(cell)
        if not bb: print('empty', n); continue
        icon = out.crop(bb)
        s = max(icon.size); pad = int(s*0.06)
        canvas = Image.new('RGBA', (s+2*pad, s+2*pad), (0,0,0,0))
        canvas.paste(icon, ((s+2*pad-icon.size[0])//2, (s+2*pad-icon.size[1])//2), icon)
        save_png(canvas.resize((size, size), Image.LANCZOS), 'Icons', 'Icon_'+n)
ICON_SHEETS = {
 'icons_food': 'Sugar Cookie Cheese Bread Candy Grape Cake Jam Cracker Chocolate Popcorn Seed Nut Cereal Apple Jelly',
 'icons_threats': 'Human Slipper Spray Cat Dog GranSlipper Swatter Broom KidHand FireAnt Spider Fly Newspaper RedHill Ambush Phone',
 'icons_hud': 'Medic Acid Carry Mass Timer Compass NestEntrance Larva Worker Squad Tag Ready Gear Radio Controls Medal',
 'icons_commands': 'Pin Lens Wrench Trail Emblem Bomb Decree Clipboard Spawn Hourglass Bolt Dice Shield Eye Strength Speed',
 'icons_extra': 'Forager Soldier Scout Barrel Brood Guard Carriers Engineers Scouts Arsenal Stamp Gamepad Friends Language Subtitles Scale',
}
def run(which):
    if 'bg' in which:
        for n, tag in (('bg_kitchen_wide','BgKitchen'),('bg_warroom','BgWarRoom'),('bg_nest','BgNest')):
            save_jpg(Image.open(R+n+'.png').resize((2560, 1097), Image.LANCZOS), 'Backgrounds', tag)
        for n, tag in (('bg_counter_cake','BgCake'),('bg_fridge_dawn','BgDawn'),('bg_slipper','BgSlipper'),('news_win','NewsWin'),('news_lose','NewsLose')):
            save_jpg(fit(Image.open(R+n+'.png'), 1920, 1080), 'Backgrounds', tag)
        for n in ('chain','gruzd','cat','spray','dog','medic'):
            save_jpg(fit(Image.open(R+'load_'+n+'.png'), 1920, 1080), 'Loading', 'Load'+n.capitalize())
    if 'por' in which:
        for n in ('queen','zhvalov','gruzd','usik','tikhon','kapelka','vintik'):
            save_jpg(Image.open(R+'por_'+n+'.png').resize((512,512), Image.LANCZOS), 'Portraits', 'Por'+n.capitalize())
    if 'card' in which:
        for f in sorted(os.listdir(R)):
            if f.startswith('card_'):
                n = f[5:-4]; save_jpg(fit(Image.open(R+f), 600, 800), 'Cards', 'Card'+n.capitalize())
    if 'mat' in which:
        save_jpg(Image.open(R+'tex_cardboard.png').resize((512,512), Image.LANCZOS), 'Materials', 'Cardboard')
        save_jpg(Image.open(R+'tex_receipt.png').resize((512,512), Image.LANCZOS), 'Materials', 'Paper')
        save_jpg(fit(Image.open(R+'studio.png'), 1024, 1024), 'Logo', 'Studio')
        em, bb = cutout(Image.open(R+'emblem.png').convert('RGB'), 238)
        em = em.crop(bb); s = max(em.size); c = Image.new('RGBA', (s, s)); c.paste(em, ((s-em.size[0])//2, (s-em.size[1])//2), em)
        save_png(c.resize((512,512), Image.LANCZOS), 'Logo', 'Emblem')
    if 'icons' in which:
        for sheet, names in ICON_SHEETS.items():
            if os.path.exists(R+sheet+'.png'): icons(sheet, names.split())
    if 'logo' in which:
        lg, bb = cutout(Image.open(R+'logo_final.png').convert('RGB'), 236)
        lg = lg.crop(bb); w = 1600; lg = lg.resize((w, round(lg.size[1]*w/lg.size[0])), Image.LANCZOS)
        save_png(lg, 'Logo', 'Logo')
if __name__ == '__main__':
    run(sys.argv[1:])
