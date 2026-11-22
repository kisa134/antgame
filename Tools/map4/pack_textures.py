# Map v4: packs the CC0 / generated texture sets into the game's triplanar PBR format.
#   T_M4_<Name>_C.jpg  base colour (sRGB)
#   T_M4_<Name>_N.png  normal, DirectX (green flipped from the OpenGL sources)
#   T_M4_<Name>_R.jpg  R = ambient occlusion, G = roughness, B = metalness
# Sources: SourceArt/Map4/textures/<polyhaven id>/ (Poly Haven, CC0), .../acg_<id>/ (ambientCG, CC0),
#          SourceArt/Map4/higgsfield/tex/ (generated for this project). Credits: docs/level/V4_CREDITS.md
# Run (any machine with Python + Pillow + numpy): python Tools/map4/pack_textures.py
import os, sys, glob
import numpy as np
from PIL import Image
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SRC = os.path.join(ROOT, 'SourceArt', 'Map4', 'textures')
GEN = os.path.join(ROOT, 'SourceArt', 'Map4', 'higgsfield', 'tex')
OUT = os.path.join(ROOT, 'SourceArt', 'Map4', 'packed')
os.makedirs(OUT, exist_ok=True)

def ph(id_, c=None):
    d = os.path.join(SRC, id_)
    return dict(c=os.path.join(d, c or 'Diffuse.jpg'), n=os.path.join(d, 'nor_gl.jpg'), r=os.path.join(d, 'Rough.jpg'), ao=os.path.join(d, 'AO.jpg'), m=None)

def acg(id_):
    d = os.path.join(SRC, 'acg_' + id_)
    f = lambda s: (glob.glob(os.path.join(d, '*_%s.jpg' % s)) or [None])[0]
    return dict(c=f('Color'), n=f('NormalGL'), r=f('Roughness'), ao=f('AmbientOcclusion'), m=f('Metalness'))

def with_color(base, color):
    b = dict(base); b['c'] = color; return b

SETS = {
    'FloorTile': (ph('floor_tiles_06'), 2048),
    'TileCream': (ph('interior_tiles'), 2048),
    'Parquet': (ph('herringbone_parquet'), 2048),
    'Oak': (ph('wood_table_001'), 2048),
    'TableWood': (ph('wood_table_worn'), 2048),
    'Walnut': (ph('walnut_veneer'), 1024),
    'PaintWood': (ph('white_oak_veneer'), 2048),
    'Plaster': (ph('plastered_wall'), 2048),
    'Stucco': (ph('white_stucco'), 1024),
    'WallpaperK': (with_color(ph('plastered_wall'), os.path.join(GEN, 'wallpaper_kitchen.png')), 2048),
    'WallpaperL': (with_color(ph('plastered_wall'), os.path.join(GEN, 'wallpaper_living.png')), 2048),
    'RugRose': (with_color(ph('dirty_carpet'), os.path.join(GEN, 'rug_rose.png')), 2048),
    'Linen': (ph('rough_linen'), 1024),
    'Gingham': (ph('fabric_pattern_07', 'col_1.jpg'), 1024),
    'SofaWool': (ph('poly_wool_herringbone'), 1024),
    'Boucle': (ph('wool_boucle'), 1024),
    'Knit': (ph('knitted_fleece'), 1024),
    'Terry': (ph('terry_cloth'), 1024),
    'Jersey': (ph('cotton_jersey'), 1024),
    'Melange': (ph('jersey_melange'), 1024),
    'Carpet': (ph('dirty_carpet'), 1024),
    'Jute': (ph('hessian_230'), 1024),
    'Leather': (ph('brown_leather'), 1024),
    'Grass': (ph('leafy_grass'), 2048),
    'Soil': (ph('forest_ground_04'), 2048),
    'Asphalt': (ph('asphalt_02'), 1024),
    'Pavement': (ph('concrete_pavement'), 2048),
    'Bark': (ph('bark_brown_02'), 1024),
    'Picket': (ph('white_planks_clean'), 1024),
    'Siding': (ph('weathered_plank_siding'), 1024),
    'Concrete': (ph('smooth_concrete_floor'), 1024),
    'Marble': (ph('marble_01'), 1024),
    'MetalPlate': (ph('metal_plate_02'), 1024),
    'Brushed': (acg('Metal009'), 1024),
    'Brushed2': (acg('Metal012'), 1024),
    'Cardboard': (acg('Cardboard001'), 1024),
    'Paper': (acg('Paper001'), 1024),
    'Plastic': (acg('Plastic010'), 1024),
    'Rubber': (acg('Rubber004'), 1024),
}

def load(p, size, mode):
    im = Image.open(p).convert(mode)
    if im.size != (size, size):
        im = im.resize((size, size), Image.LANCZOS)
    return im

def make_seamless(im):
    # generated colour maps: blend a half-offset copy across the seams (cheap but effective)
    a = np.asarray(im).astype(np.float32)
    h, w = a.shape[:2]
    s = np.roll(np.roll(a, h // 2, 0), w // 2, 1)
    y = np.abs(np.linspace(-1, 1, h))[:, None]
    x = np.abs(np.linspace(-1, 1, w))[None, :]
    m = np.clip((np.maximum(x, y) - 0.72) / 0.28, 0, 1)[..., None]
    return Image.fromarray((a * (1 - m) + s * m).astype(np.uint8))

def pack(name, spec, size):
    c = load(spec['c'], size, 'RGB')
    if GEN in spec['c']:
        c = make_seamless(c)
    c.save(os.path.join(OUT, 'T_M4_%s_C.jpg' % name), quality=93)
    n = np.asarray(load(spec['n'], size, 'RGB')).copy()
    n[..., 1] = 255 - n[..., 1]
    Image.fromarray(n).save(os.path.join(OUT, 'T_M4_%s_N.png' % name), optimize=True)
    r = np.asarray(load(spec['r'], size, 'L'))
    ao = np.asarray(load(spec['ao'], size, 'L')) if spec.get('ao') and os.path.exists(spec['ao']) else np.full_like(r, 255)
    m = np.asarray(load(spec['m'], size, 'L')) if spec.get('m') and os.path.exists(spec['m']) else np.zeros_like(r)
    Image.fromarray(np.stack([ao, r, m], -1)).save(os.path.join(OUT, 'T_M4_%s_R.jpg' % name), quality=95)
    print('packed', name, size)

def procedural():
    # enamel / plain paint: orange-peel normal, flat colour; the micro-detail normal for every surface (ant scale)
    rng = np.random.default_rng(7)
    def noise(size, scale):
        f = np.fft.fft2(rng.standard_normal((size, size)))
        ky = np.fft.fftfreq(size)[:, None]; kx = np.fft.fftfreq(size)[None, :]
        k = np.sqrt(kx * kx + ky * ky) + 1e-6
        g = np.real(np.fft.ifft2(f * np.exp(-(k * scale) ** 2)))
        return (g - g.mean()) / (g.std() + 1e-6)
    def normal_from(h, strength):
        gy, gx = np.gradient(h)
        nx, ny, nz = -gx * strength, gy * strength, np.ones_like(h)
        l = np.sqrt(nx * nx + ny * ny + nz * nz)
        n = np.stack([nx / l, ny / l, nz / l], -1)
        return Image.fromarray(((n * 0.5 + 0.5) * 255).astype(np.uint8))
    h = noise(1024, 60.0) * 0.6 + noise(1024, 12.0) * 0.4
    normal_from(h, 0.6).save(os.path.join(OUT, 'T_M4_Enamel_N.png'))
    Image.new('RGB', (64, 64), (235, 235, 230)).save(os.path.join(OUT, 'T_M4_Enamel_C.jpg'), quality=95)
    r = np.clip(60 + noise(512, 40.0) * 8, 0, 255).astype(np.uint8)
    Image.fromarray(np.stack([np.full_like(r, 255), r, np.zeros_like(r)], -1)).save(os.path.join(OUT, 'T_M4_Enamel_R.jpg'), quality=95)
    d = noise(1024, 3.0) * 0.5 + noise(1024, 9.0) * 0.5
    normal_from(d, 1.2).save(os.path.join(OUT, 'T_M4_Detail_N.png'))
    print('packed procedural Enamel, Detail')

only = sys.argv[1:]
for name, (spec, size) in SETS.items():
    if only and name not in only:
        continue
    try:
        pack(name, spec, size)
    except Exception as e:
        print('FAIL', name, e)
if not only or 'proc' in only:
    procedural()
