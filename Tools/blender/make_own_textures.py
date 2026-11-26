# KAN-24: makes every map v4 surface set (/Game/Map4/Textures/T_M4_<Set>_{C,N,R}) ourselves, in Blender, replacing the
# downloaded sets of 22.11. Nothing is downloaded or read from outside the project.
#   The shader:  SourceArt/Blender/textures_own/own_textures.osl  (one procedural OSL shader, Mat = set index, seamless)
#   This script: bakes it on a unit plane (Cycles, CPU/OSL, emission bakes), then per set
#       C  base colour (sRGB)                                        T_M4_<Set>_C.jpg
#       N  normal, DirectX (green down), from the baked height      T_M4_<Set>_N.png
#       R  R = AO (cavity of the baked height), G = roughness, B = metalness   T_M4_<Set>_R.jpg
#     all with wrap-around maths, so they tile without a seam.
#   Calibration: the colour / AO / roughness means and the normal strength of each set are set to CAL below, the values the
#   look in Source/AntKitchen/Map4Look.cpp (tints, roughness factors) was tuned against; the pattern is ours.
#   WallpaperK / WallpaperL / RugRose keep their generated print (Higgsfield, SourceArt/Map4/higgsfield/tex) as the colour.
#   Output: SourceArt/Blender/textures_own/T_M4_*  (Tools/map4/make_m4_materials.py imports them from there)
# Run: "C:\Program Files\Blender Foundation\Blender 5.3\blender.exe" -b --factory-startup -P Tools/blender/make_own_textures.py -- [Set ...]
import bpy, os, sys, time
import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OWN = os.path.join(ROOT, "SourceArt", "Blender", "textures_own")
OSL = os.path.join(OWN, "own_textures.osl")
GEN = os.path.join(ROOT, "SourceArt", "Map4", "higgsfield", "tex")

# name: (Mat index in own_textures.osl, size, colour match mode)
#   gain = per channel to CAL, lum = brightness only, lum:k = brightness k x CAL (lum+ = 1.15: the white of a check),
#   print = the generated print
SETS = {
    "Plaster": (0, 2048, "gain"), "Stucco": (1, 1024, "gain"), "WallpaperK": (2, 2048, "print"), "WallpaperL": (2, 2048, "print"),
    "Concrete": (3, 1024, "gain"), "Marble": (4, 1024, "gain"), "Asphalt": (5, 1024, "gain"), "Pavement": (6, 2048, "gain"),
    "Soil": (7, 2048, "gain"), "Grass": (8, 2048, "lum:0.8"), "Bark": (9, 1024, "gain"), "TileCream": (10, 2048, "gain"),
    "FloorTile": (11, 2048, "gain"), "Oak": (12, 2048, "gain"), "TableWood": (13, 2048, "gain"), "Walnut": (14, 1024, "gain"),
    "PaintWood": (15, 2048, "gain"), "Parquet": (16, 2048, "gain"), "Picket": (17, 1024, "gain"), "Siding": (18, 1024, "gain"),
    "Brushed": (19, 1024, "gain"), "Brushed2": (20, 1024, "gain"), "MetalPlate": (21, 1024, "gain"), "Plastic": (22, 1024, "gain"),
    "Rubber": (23, 1024, "gain"), "Leather": (24, 1024, "gain"), "Paper": (25, 1024, "gain"), "Cardboard": (26, 1024, "gain"),
    "Linen": (27, 1024, "gain"), "Gingham": (28, 1024, "lum+"), "GinghamBlue": (29, 1024, "lum+"), "GinghamGreen": (30, 1024, "lum+"),
    "SofaWool": (31, 1024, "gain"), "Boucle": (32, 1024, "gain"), "Knit": (33, 1024, "gain"), "Terry": (34, 1024, "gain"),
    "Jersey": (35, 1024, "gain"), "Melange": (36, 1024, "gain"), "Carpet": (37, 1024, "gain"), "Jute": (38, 1024, "gain"),
    "RugRose": (39, 2048, "print"), "Enamel": (40, 1024, "flat"), "Detail": (41, 1024, "nonly"), "Weave": (42, 512, "nonly"),
}
GREY = {"BoucleG": "Boucle", "JerseyG": "Jersey", "KnitG": "Knit", "LinenG": "Linen"}   # grey copies for the residents' cloth (NpcBody.cpp)
PRINT = {"WallpaperK": "wallpaper_kitchen.png", "WallpaperL": "wallpaper_living.png", "RugRose": "rug_rose.png"}

# set: (colour mean sRGB, AO mean, roughness mean, normal xy std (encoded))
CAL = {
    'Asphalt': ((0.3546, 0.352, 0.3317), 0.72, 0.769, 0.0781), 'Bark': ((0.372, 0.3373, 0.2503), 0.588, 0.901, 0.1447),
    'Boucle': ((0.4618, 0.4271, 0.3783), 0.876, 0.657, 0.0624), 'BoucleG': ((0.4415, 0.4415, 0.4415), None, None, None),
    'Brushed': ((0.5765, 0.5809, 0.5965), 0.996, 0.47, 0.0089), 'Brushed2': ((0.8356, 0.8676, 0.8937), 0.999, 0.088, 0.0032),
    'Cardboard': ((0.6894, 0.5541, 0.3443), 0.998, 0.641, 0.0594), 'Carpet': ((0.2079, 0.1848, 0.1445), 0.478, 0.811, 0.0848),
    'Concrete': ((0.3334, 0.2371, 0.1599), 0.896, 0.681, 0.0112), 'Detail': (None, None, None, 0.1734),
    'Enamel': ((0.9177, 0.9216, 0.898), 0.999, 0.233, 0.0317), 'FloorTile': ((0.4788, 0.4369, 0.4218), 0.97, 0.503, 0.0109),
    'Gingham': ((0.6051, 0.3501, 0.3332), 0.861, 0.64, 0.0695), 'GinghamBlue': ((0.3645, 0.4421, 0.4504), 0.861, 0.64, 0.0695),
    'GinghamGreen': ((0.442, 0.5347, 0.3804), 0.861, 0.64, 0.0695), 'Grass': ((0.591, 0.5152, 0.35), 0.737, 0.7, 0.1477),
    'Jersey': ((0.7566, 0.6725, 0.6172), 0.919, 0.801, 0.0203), 'JerseyG': ((0.5745, 0.5745, 0.5745), None, None, None),
    'Jute': ((0.5498, 0.4707, 0.3496), 0.901, 0.584, 0.0787), 'Knit': ((0.3338, 0.2949, 0.2477), 0.9, 0.623, 0.0644),
    'KnitG': ((0.5525, 0.5525, 0.5525), None, None, None), 'Leather': ((0.3177, 0.1786, 0.0628), 0.878, 0.661, 0.061),
    'Linen': ((0.5702, 0.6704, 0.8055), 0.771, 0.718, 0.0426), 'LinenG': ((0.5744, 0.5744, 0.5744), None, None, None),
    'Marble': ((0.6939, 0.6146, 0.4772), 0.953, 0.506, 0.0054), 'Melange': ((0.4286, 0.52, 0.7007), 0.873, 0.559, 0.0482),
    'MetalPlate': ((0.3305, 0.2855, 0.2425), 0.957, 0.658, 0.0211), 'Oak': ((0.2428, 0.1003, 0.0431), 0.967, 0.235, 0.012),
    'PaintWood': ((0.5574, 0.4794, 0.3847), 0.986, 0.745, 0.0235), 'Paper': ((0.9527, 0.9488, 0.9427), 0.991, 0.631, 0.0667),
    'Parquet': ((0.5721, 0.4386, 0.3023), 0.993, 0.099, 0.0097), 'Pavement': ((0.4975, 0.4594, 0.3999), 0.802, 0.839, 0.0852),
    'Picket': ((0.7038, 0.6515, 0.6172), 0.951, 0.525, 0.0311), 'Plaster': ((0.7108, 0.6625, 0.612), 0.977, 0.941, 0.0644),
    'Plastic': ((0.6339, 0.6483, 0.6725), 0.999, 0.364, 0.0037), 'Rubber': ((0.1666, 0.1703, 0.1951), 0.991, 0.59, 0.078),
    'RugRose': ((0.6632, 0.4448, 0.397), 0.478, 0.79, 0.0887), 'Siding': ((0.2972, 0.2394, 0.1961), 0.918, 0.895, 0.0505),
    'SofaWool': ((0.4744, 0.4605, 0.442), 0.85, 0.86, 0.0543), 'Soil': ((0.4165, 0.3522, 0.2657), 0.867, 0.916, 0.1246),
    'Stucco': ((0.841, 0.8217, 0.8128), 0.988, 0.876, 0.0364), 'TableWood': ((0.3273, 0.1859, 0.0838), 0.978, 0.735, 0.0266),
    'Terry': ((0.0246, 0.2849, 0.5884), 0.847, 0.687, 0.0669), 'TileCream': ((0.6803, 0.5918, 0.514), 0.995, 0.71, 0.0234),
    'WallpaperK': ((0.9589, 0.8641, 0.6736), 0.977, 0.941, 0.0644), 'WallpaperL': ((0.5965, 0.6244, 0.4541), 0.977, 0.941, 0.0644),
    'Walnut': ((0.5426, 0.4134, 0.3171), 0.985, 0.576, 0.0123), 'Weave': (None, None, None, 0.1364),
}
METAL = {"Brushed", "Brushed2"}


def log(*a):
    print("OWNTEX", *a, flush=True)


# ------------------------------------------------------------------------------------------ the bake scene
def scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.shading_system = True          # OSL
    sc.cycles.samples = 4                    # a little anti-aliasing of the procedural detail
    sc.render.threads_mode = "FIXED"
    sc.render.threads = 12                   # the PC is shared
    sc.render.bake.margin = 0
    bpy.ops.mesh.primitive_plane_add(size=1.0, location=(0, 0, 0))
    ob = bpy.context.active_object
    mat = bpy.data.materials.new("OwnTextures")
    mat.use_nodes = True
    nt = mat.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sn = nt.nodes.new("ShaderNodeScript")
    sn.mode = "INTERNAL"
    sn.script = bpy.data.texts.load(OSL)
    try:
        from cycles import osl as cosl
        cosl.update_script_node(sn, lambda lvl, msg: log("OSL", lvl, msg))
    except Exception as e:
        log("OSL update failed", e)
        raise
    em = nt.nodes.new("ShaderNodeEmission")
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    tex = nt.nodes.new("ShaderNodeTexImage")
    nt.links.new(tc.outputs["UV"], sn.inputs["UV"])
    nt.links.new(em.outputs[0], out.inputs["Surface"])
    nt.nodes.active = tex
    ob.data.materials.append(mat)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    return dict(ob=ob, nt=nt, sn=sn, em=em, tex=tex)


def bake(S, mat_index, socket, size):
    S["sn"].inputs["Mat"].default_value = mat_index
    nt = S["nt"]
    for l in list(S["em"].inputs[0].links):
        nt.links.remove(l)
    nt.links.new(S["sn"].outputs[socket], S["em"].inputs[0])
    img = bpy.data.images.new("bake", size, size, alpha=False, float_buffer=True)
    img.colorspace_settings.name = "Non-Color"
    S["tex"].image = img
    nt.nodes.active = S["tex"]
    bpy.ops.object.bake(type="EMIT", margin=0, use_clear=True)
    a = np.empty(size * size * 4, dtype=np.float32)
    img.pixels.foreach_get(a)
    bpy.data.images.remove(img)
    return a.reshape(size, size, 4)[..., :3].copy()      # rows bottom-up (v up), as Blender keeps them


# ------------------------------------------------------------------------------------------ maths (all wrap-around)
def blur(a, sigma):
    n = a.shape[0]
    k = np.fft.fftfreq(n)
    g = np.exp(-2.0 * (np.pi * sigma) ** 2 * (k[:, None] ** 2 + k[None, :] ** 2))
    return np.real(np.fft.ifft2(np.fft.fft2(a) * g))


def resize(a, n):
    """box / bilinear resample of a periodic map to n x n (only used for the small Enamel maps)"""
    m = a.shape[0]
    if m == n:
        return a
    if m % n == 0:
        f = m // n
        return a.reshape(n, f, n, f, *a.shape[2:]).mean(axis=(1, 3))
    raise ValueError("resize %d -> %d" % (m, n))


def normal_dx(h, k):
    gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
    gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5          # +v (rows go up)
    nx, ny, nz = -gx * k, -gy * k, np.ones_like(h)            # OpenGL (+Y)
    l = np.sqrt(nx * nx + ny * ny + nz * nz)
    return np.stack([nx / l, -ny / l, nz / l], -1) * 0.5 + 0.5  # DirectX: green flipped for Unreal


def fit_normal(h, target):
    """the slope factor that gives the encoded normal the target spread (mean std of its x and y)"""
    lo, hi = 1e-4, 1e4
    for _ in range(40):
        k = np.sqrt(lo * hi)
        n = normal_dx(h, k)
        s = 0.5 * (n[..., 0].std() + n[..., 1].std())
        if s < target:
            lo = k
        else:
            hi = k
    return normal_dx(h, np.sqrt(lo * hi))


def cavity_ao(h, target, size):
    c = np.clip(blur(h, size / 160.0) - h, 0.0, None) + 0.5 * np.clip(blur(h, size / 40.0) - h, 0.0, None)
    if target is None or target >= 0.995 or c.max() <= 0:
        return np.clip(1.0 - c * 0.02, 0.0, 1.0)
    lo, hi = 0.0, 50.0
    for _ in range(40):
        a = 0.5 * (lo + hi)
        if np.clip(1.0 - a * c, 0.0, 1.0).mean() > target:
            lo = a
        else:
            hi = a
    return np.clip(1.0 - 0.5 * (lo + hi) * c, 0.0, 1.0)


def to_srgb(c):
    c = np.clip(c, 0.0, None)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1.0 / 2.4) - 0.055)


def match_colour(s, target, mode):
    if target is None or mode in ("flat", "keep"):
        return np.clip(s, 0.0, 1.0)
    m = s.reshape(-1, 3).mean(0)
    if mode.startswith("lum"):
        w = np.array([0.2126, 0.7152, 0.0722])
        k = 1.15 if mode == "lum+" else float(mode[4:]) if mode.startswith("lum:") else 1.0
        target = tuple(k * x for x in target)
        g = float(np.dot(target, w)) / max(float(np.dot(m, w)), 1e-4)
        for _ in range(4):                                     # re-fit after the clip at 1
            o = np.clip(s * g, 0.0, 1.0)
            g *= float(np.dot(target, w)) / max(float(np.dot(o.reshape(-1, 3).mean(0), w)), 1e-4)
        return np.clip(s * g, 0.0, 1.0)
    g = np.array(target) / np.maximum(m, 1e-4)
    for _ in range(4):
        o = np.clip(s * g, 0.0, 1.0)
        g *= np.array(target) / np.maximum(o.reshape(-1, 3).mean(0), 1e-4)
    return np.clip(s * g, 0.0, 1.0)


def seamless_print(a):
    """the generated print: blend a half-offset copy across the tile's borders (as the old packer did)"""
    h, w = a.shape[:2]
    s = np.roll(np.roll(a, h // 2, 0), w // 2, 1)
    y = np.abs(np.linspace(-1, 1, h))[:, None]
    x = np.abs(np.linspace(-1, 1, w))[None, :]
    m = np.clip((np.maximum(x, y) - 0.72) / 0.28, 0, 1)[..., None]
    return a * (1 - m) + s * m


def seam_ratio(a):
    """how much bigger the step across the tile's border is than inside it (1 = seamless)"""
    a = a.astype(np.float64)
    inner = np.abs(np.diff(a, axis=1)).mean()
    edge = np.abs(a[:, 0] - a[:, -1]).mean()
    inner2 = np.abs(np.diff(a, axis=0)).mean()
    edge2 = np.abs(a[0, :] - a[-1, :]).mean()
    return max(edge / max(inner, 1e-9), edge2 / max(inner2, 1e-9))


def save(a, path):
    """a: rows bottom-up, values 0..1, 1 or 3 channels. Written as is (no colour transform)."""
    if a.ndim == 2:
        a = np.stack([a, a, a], -1)
    n = a.shape[0]
    img = bpy.data.images.new("out", n, n, alpha=False, float_buffer=False)
    img.colorspace_settings.name = "Non-Color"
    px = np.ones((n, n, 4), dtype=np.float32)
    px[..., :3] = np.clip(a, 0.0, 1.0)
    img.pixels.foreach_set(px.ravel())
    img.filepath_raw = path
    img.file_format = "PNG" if path.endswith(".png") else "JPEG"
    try:
        img.save(filepath=path, quality=95)
    except TypeError:
        img.save()
    bpy.data.images.remove(img)


def load_print(fname, size):
    img = bpy.data.images.load(os.path.join(GEN, fname))
    img.colorspace_settings.name = "Non-Color"                  # raw sRGB values
    if img.size[0] != size or img.size[1] != size:
        img.scale(size, size)
    a = np.empty(size * size * 4, dtype=np.float32)
    img.pixels.foreach_get(a)
    bpy.data.images.remove(img)
    return seamless_print(a.reshape(size, size, 4)[..., :3])


# ------------------------------------------------------------------------------------------ one set
def make(S, name):
    idx, size, mode = SETS[name]
    cal = CAL.get(name, (None, None, None, 0.05))
    t0 = time.time()
    base = os.path.join(OWN, "T_M4_" + name)
    dat = bake(S, idx, "Dat", size)
    h = dat[..., 0].astype(np.float64)
    h = (h - h.mean()) / max(h.std(), 1e-6)
    save(fit_normal(h, cal[3]), base + "_N.png")
    if mode == "nonly":
        log(name, "N", size, "seam h %.2f" % seam_ratio(h), "%.1fs" % (time.time() - t0))
        return None
    if mode == "print":
        c = load_print(PRINT[name], size)
    elif mode == "flat":
        c = match_colour(to_srgb(bake(S, idx, "Col", 64)), cal[0], "gain")
    else:
        c = match_colour(to_srgb(bake(S, idx, "Col", size)), cal[0], mode)
    save(c, base + "_C.jpg")
    ao = cavity_ao(h, cal[1], size)
    rough = dat[..., 1]
    rough = np.clip(rough - rough.mean() + cal[2], 0.02, 1.0)
    metal = np.ones_like(rough) if name in METAL else np.clip(dat[..., 2], 0.0, 1.0)
    orm = np.stack([ao, rough, metal], -1)
    if mode == "flat":
        orm = resize(orm, 512)
    save(orm, base + "_R.jpg")
    log(name, size, "C mean", np.round(c.reshape(-1, 3).mean(0), 3), "target", cal[0], "AO %.3f R %.3f" % (ao.mean(), rough.mean()),
        "seam C %.2f h %.2f" % (seam_ratio(c.mean(-1)), seam_ratio(h)), "%.1fs" % (time.time() - t0))
    return c


def grey(name, c):
    w = np.array([0.2126, 0.7152, 0.0722])
    g = np.repeat((c * w).sum(-1)[..., None], 3, -1)
    g = match_colour(g, CAL[name][0], "gain")
    save(g, os.path.join(OWN, "T_M4_%s_C.jpg" % name))
    log(name, "grey of", GREY[name], "mean %.3f" % g.mean())


def main():
    want = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    os.makedirs(OWN, exist_ok=True)
    S = scene()
    done = {}
    for name in SETS:
        need_for_grey = any(src == name and (not want or g in want) for g, src in GREY.items())
        if want and name not in want and not need_for_grey:
            continue
        done[name] = make(S, name)
    for g, src in GREY.items():
        if (not want or g in want or src in want) and done.get(src) is not None:
            grey(g, done[src])
    if not want:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OWN, "textures_own.blend"))
    log("DONE", len(done))


main()
