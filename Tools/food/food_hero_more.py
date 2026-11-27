# The rest of the hero food (exec'd by food_hero.py: the same helpers). First his other models, then the new foods.

# ---------------------------------------------------------------- CHEESE: his Meshy chunk with holes
@own_food("Cheese", ("meshy", "SM_Food_CheeseChunk"), "Soft", 7.0, 2, label="Meshy SM_Food_CheeseChunk")
def cheese(o, imgs, tint, L):
    paste = own_colour_at(o, imgs, lambda co, h: True, (0.9, 0.6, 0.15))
    pv = max(paste); log("cheese paste", paste)
    def detail(t, c):
        v = c.v
        hole = t.mapr(c.val, pv * 0.78, pv * 0.55)                                 # his holes are painted darker: their inside
        pin = specks(t, v, 420.0, 0.22)                                            # pinholes
        cry = specks(t, v, 2200.0, 0.2, (3.1, 0.7, 1.9))                          # tyrosine crystals: tiny white grains
        rind = t.mul(t.mapr(c.ny, 0.55, 0.85), t.sub(1.0, hole))                   # the wax rind on the wheel's outside (+Y face)
        grit = t.noise(v, 1800.0, 4.0, 0.7)
        col = t.mixc(t.mul(hole, 0.55), c.col, tone(t, c.col, t.mapr(grit, 0.3, 0.7, 0.62, 0.95)))   # rough inside of a hole: shaded pits
        col = t.mixc(t.mul(pin, 0.75), col, tone(t, c.col, 0.4))
        col = t.mixc(t.mul(cry, t.sub(1.0, rind)), col, (1.0, 0.96, 0.82))
        col = t.mixc(t.mul(rind, 0.85), col, t.mixc(1.0, c.col, (0.95, 0.55, 0.22), "MULTIPLY"))
        sweat = t.noise(v, 90.0, 3.0)
        rough = t.mapr(sweat, 0.35, 0.7, 0.34, 0.6)                                # a waxy paste, sweaty in patches
        rough = t.mixf(hole, rough, t.mapr(grit, 0.3, 0.7, 0.5, 0.8))
        rough = t.mixf(rind, rough, 0.28)
        h = t.add(t.mul(hole, t.mul(grit, 1.4)), t.mul(pin, -0.9))
        h = t.add(h, t.mul(cry, 0.4)); h = t.mixf(rind, h, t.mul(t.noise(v, 400.0, 2.0), 0.15))
        return col, rough, h, {"Coat Weight": t.mul(rind, 0.5)}
    def inner(t, v, h):
        vo = t.voronoi(t.vmath("MULTIPLY", v, (1.0, 1.0, 1.25)), 70.0, "F1"); rr = t.sep(t.voronoi(v, 70.0, "F1", out="Color"))[0]
        hole = t.mul(t.mapr(vo, 0.16, 0.12, 0.0, 1.0), t.mapr(rr, 0.55, 0.6))      # round eyes, some cells only
        grit = t.noise(v, 1800.0, 4.0, 0.7); pin = specks(t, v, 520.0, 0.10)
        col = t.mixc(t.mul(hole, 0.6), paste, tuple(x * 0.62 for x in paste)); col = t.mixc(t.mul(pin, 0.6), col, tuple(x * 0.55 for x in paste))
        return col, t.mixf(hole, 0.5, 0.75), t.add(t.mul(hole, t.add(-1.0, t.mul(grit, 0.6))), t.mul(pin, -0.8))
    return dict(detail=detail, mat=dict(sss=0.25, sss_rad=(1.0, 0.6, 0.2), bump_str=0.8), inner=inner, cut_ru="сыр с дырками",
                cuts=[((0.05, 0, 0), (1.0, 0.1, 0.05), 0.012)])

# ---------------------------------------------------------------- CHOCOLATE: his Meshy two-square piece
@own_food("Chocolate", ("meshy", "SM_Food_Chocolate"), "Melting", 5.0, 4, label="Meshy SM_Food_Chocolate")
def chocolate(o, imgs, tint, L):
    cc = own_colour_at(o, imgs, lambda co, h: True, (0.1, 0.04, 0.02))
    def detail(t, c):
        v = c.v
        mot = t.noise(v, 260.0, 4.0)
        bloom = t.mul(t.mapr(t.noise(v, 45.0, 3.0), 0.62, 0.74), 0.22)            # a faint sugar bloom in a few patches
        scr = t.noise(t.vmath("MULTIPLY", v, (0.08, 1.0, 1.0)), 1400.0, 2.0)       # fine moulding streaks
        col = t.mixc(t.mul(t.sub(mot, 0.5), 0.35), c.col, tone(t, c.col, 1.35))
        col = t.mixc(bloom, col, (0.42, 0.33, 0.27))
        up = t.mapr(c.nz, 0.5, 0.9)
        rough = t.mixf(up, t.mapr(mot, 0.3, 0.7, 0.32, 0.45), t.mapr(scr, 0.3, 0.7, 0.10, 0.2))   # tempered: glossy flat tops
        rough = t.add(rough, t.mul(bloom, 1.5))
        h = t.add(t.mul(scr, 0.12), t.mul(t.noise(v, 3000.0, 2.0), 0.06))
        return col, rough, h, {"Coat Weight": t.mul(up, 0.35)}
    def inner(t, v, h):
        cr = t.noise(v, 900.0, 5.0, 0.7); ch = t.voronoi(v, 500.0, "DISTANCE_TO_EDGE")
        col = t.mixc(t.mapr(cr, 0.3, 0.7), tuple(x * 0.8 for x in cc), tuple(min(1, x * 1.25) for x in cc))
        return col, t.mapr(cr, 0.3, 0.7, 0.55, 0.8), t.add(t.mul(cr, 0.8), t.mapr(ch, 0.0, 0.05, -0.5, 0.0))
    return dict(detail=detail, mat=dict(sss=0.05, bump_str=0.6), inner=inner, cut_ru="излом шоколада",
                cuts=[((0, 0, 0), (1.0, 0.04, 0.0), 0.01), ((0, 0, 0), (0.03, 1.0, 0.0), 0.01)])

# ---------------------------------------------------------------- JELLY: his gummy bear / gummy worm / honey drop
def jelly_detail(sugar=0.22, rough=0.14, bubbles=0.06):
    def detail(t, c):
        v = c.v
        g, r = crystals(t, v, 1700.0)
        sug = t.mul(specks(t, v, 1500.0, sugar, (1.3, 2.1, 0.4)), 1.0)             # sugar sanding: raised white grains
        bub = specks(t, v, 520.0, bubbles, (4.0, 1.0, 2.0))                        # little air bubbles caught in it
        depth = t.mapr(t.noise(v, 70.0, 3.0), 0.3, 0.7, 0.82, 1.15)                # thicker / thinner: the colour deepens
        col = tone(t, c.col, depth)
        col = t.mixc(t.mul(bub, 0.45), col, tone(t, c.col, 1.6))
        col = t.mixc(t.mul(sug, 0.75), col, (0.98, 0.95, 0.88))
        rgh = t.mixf(sug, rough, t.mapr(r, 0.0, 1.0, 0.25, 0.55))
        h = t.add(t.mul(sug, 0.9), t.mul(bub, -0.25))
        return col, rgh, h
    return detail

def jelly_inner(col):
    def inner(t, v, h):
        bub = specks(t, v, 520.0, 0.08, (4.0, 1.0, 2.0)); tear = t.noise(v, 300.0, 4.0)
        c = t.mixc(t.mul(bub, 0.4), tuple(min(1, x * 1.15) for x in col), tuple(min(1, x * 1.5) for x in col))
        return c, t.mapr(tear, 0.3, 0.7, 0.12, 0.3), t.mul(tear, 0.6)
    return inner

@own_food("GummyBear", ("meshy", "SM_Food_GummyBearGold"), "Elastic", 5.0, 2, label="Meshy SM_Food_GummyBearGold")
def gummy_bear(o, imgs, tint, L):
    jc = own_colour_at(o, imgs, lambda co, h: True, (0.9, 0.5, 0.05))
    return dict(detail=jelly_detail(0.20), mat=dict(sss=1.0, sss_rad=(1.0, 0.6, 0.15), sss_scale=L * 0.08, coat=0.7, coat_rough=0.06, bump_str=0.5),
                inner=jelly_inner(jc), jelly=0.85, coat=0.6, hulls=3, cut_ru="разорванный мармелад",
                cuts=[((0, 0, 0.05), (0.15, 0.1, 1.0), 0.05)])

@own_food("Gummy", ("meshy", "SM_Food_Gummy"), "Elastic", 5.0, 2, label="Meshy SM_Food_Gummy")
def gummy(o, imgs, tint, L):
    jc = own_colour_at(o, imgs, lambda co, h: True, (0.9, 0.3, 0.1))
    return dict(detail=jelly_detail(0.26), mat=dict(sss=1.0, sss_rad=(1.0, 0.5, 0.2), sss_scale=L * 0.06, coat=0.7, coat_rough=0.06, bump_str=0.5),
                inner=jelly_inner(jc), jelly=0.85, coat=0.6, hulls=4, cut_ru="разорванный мармелад",
                cuts=[((0, 0, 0), (1.0, 0.15, 0.1), 0.05)])

@own_food("Honey", ("meshy", "SM_Food_HoneyDrop"), "Sticky", 2.4, 0, label="Meshy SM_Food_HoneyDrop")
def honey(o, imgs, tint, L):
    def detail(t, c):
        v = c.v
        bub = specks(t, v, 900.0, 0.07, (2.0, 5.0, 1.0)); cry = specks(t, v, 2400.0, 0.04)
        depth = t.mapr(c.h, 0.0, 1.0, 0.8, 1.15)                                   # thick at the foot: deeper amber
        col = tone(t, c.col, depth)
        col = t.mixc(t.mul(bub, 0.6), col, (1.0, 0.85, 0.45)); col = t.mixc(t.mul(cry, 0.4), col, (1.0, 0.9, 0.6))
        return col, t.mixf(cry, 0.04, 0.3), t.add(t.mul(t.noise(v, 200.0, 2.0), 0.08), t.mul(bub, -0.2))
    return dict(detail=detail, mat=dict(sss=1.0, sss_rad=(1.0, 0.45, 0.08), sss_scale=L * 0.15, coat=1.0, coat_rough=0.03, bump_str=0.4),
                inner=lambda t, v, h: ((0.9, 0.45, 0.04), 0.05, 0.0), jelly=0.6, coat=1.0, cuts=[])

# ---------------------------------------------------------------- SUGAR: his Meshy cube
@own_food("Sugar", ("meshy", "SM_Food_SugarCube"), "Hard", 1.6, 3, label="Meshy SM_Food_SugarCube")
def sugar(o, imgs, tint, L):
    def detail(t, c):
        v = c.v
        e, r = crystals(t, v, 2200.0)                                              # 0.45 mm grains: every one a facet
        e2, r2 = crystals(t, v, 5200.0)
        col = t.mixc(t.mul(e, 0.14), tone(t, c.col, 1.1), tone(t, c.col, 0.8))     # faint grain edges: it stays white
        col = t.mixc(t.mul(t.mapr(r2, 0.8, 0.95), 0.45), col, (1.0, 1.0, 1.0))
        rough = t.mapr(r, 0.0, 1.0, 0.12, 0.62)                                    # some grains face the light: sparkle
        h = t.add(t.mul(e, -0.8), t.add(t.mul(r, 0.35), t.mul(e2, -0.25)))
        return col, rough, h
    def inner(t, v, h):
        e, r = crystals(t, v, 2200.0)
        return t.mixc(t.mul(e, 0.3), (0.92, 0.92, 0.9), (0.68, 0.68, 0.66)), t.mapr(r, 0.0, 1.0, 0.15, 0.6), t.add(t.mul(e, -0.8), t.mul(r, 0.35))
    return dict(detail=detail, mat=dict(sss=0.35, sss_rad=(1.0, 1.0, 1.0), bump_str=0.9), inner=inner, cut_ru="сахарные кристаллы",
                cuts=[((0, 0, 0), (1.0, 0.2, 0.15), 0.06), ((0, 0.1, 0), (0.1, 1.0, -0.2), 0.06)])

# ---------------------------------------------------------------- ICE CREAM: his plombir brick in its torn wrapper (Map4, Higgsfield icecream.glb)
@own_food("IceCream", ("m4", "SM_M4_IceCream"), "Melting", 7.0, 3, label="SM_M4_IceCream (Higgsfield)")
def icecream(o, imgs, tint, L):
    cream = own_colour_at(o, imgs, lambda co, h: True, (0.9, 0.85, 0.7))
    def detail(t, c):
        v = c.v
        ic = t.mul(smoothband(t, c.hue, 0.06, 0.2, 0.02), t.mapr(c.sat, 0.12, 0.25))     # the ice cream itself: warm cream, the wrapper is white / blue
        wr = t.sub(1.0, ic)
        frost, fr = crystals(t, v, 3200.0)
        melt = t.noise(v, 60.0, 3.0)
        crin = t.voronoi(t.vmath("ADD", v, t.vmath("MULTIPLY", t.noise(v, 90.0, 2.0, out="Color"), (0.004, 0.004, 0.004))), 130.0, "DISTANCE_TO_EDGE")
        col = t.mixc(t.mul(ic, t.mul(frost, 0.25)), c.col, (1.0, 1.0, 1.0))
        col = t.mixc(t.mul(wr, t.mapr(crin, 0.0, 0.05, 0.06, 0.0)), col, tone(t, c.col, 0.9))   # paper creases (faint: his print stays clean)
        rough = t.mixf(ic, c.rough, t.mapr(melt, 0.4, 0.65, 0.55, 0.18))                 # frosty, glossy where it melts
        rough = t.mixf(wr, rough, t.mapr(crin, 0.0, 0.06, 0.6, 0.38))
        h = t.add(t.mul(ic, t.add(t.mul(frost, 0.4), t.mul(melt, 0.3))), t.mul(wr, t.mapr(crin, 0.0, 0.06, -1.0, 0.0)))
        return col, rough, h
    def inner(t, v, h):
        frost, fr = crystals(t, v, 3200.0); n = t.noise(v, 300.0, 3.0)
        return t.mixc(t.mul(frost, 0.25), cream, (1.0, 1.0, 1.0)), t.mapr(n, 0.3, 0.7, 0.35, 0.6), t.add(t.mul(frost, 0.4), t.mul(n, 0.3))
    return dict(detail=detail, mat=dict(sss=0.3, sss_rad=(1.0, 0.9, 0.8), bump_str=0.7), inner=inner, cut_ru="пломбир",
                cuts=[((0, 0.15, 0), (0.1, 1.0, 0.05), 0.02), ((0, -0.2, 0), (0.05, 1.0, -0.1), 0.02)])

# ---------------------------------------------------------------- CANDY: his wrapped caramel (Map4, Higgsfield caramel.glb)
@own_food("Candy", ("m4", "SM_M4_Caramel"), "Hard", 2.5, 3, label="SM_M4_Caramel (Higgsfield)")
def candy(o, imgs, tint, L):
    red = own_colour_at(o, imgs, lambda co, h: abs(co.x) < 0.004, (0.5, 0.02, 0.02))
    def detail(t, c):
        v = c.v
        gold = t.mul(smoothband(t, c.hue, 0.06, 0.18, 0.02), t.mapr(c.sat, 0.3, 0.5))
        crin = t.voronoi(t.vmath("ADD", v, t.vmath("MULTIPLY", t.noise(v, 300.0, 2.0, out="Color"), (0.0015, 0.0015, 0.0015))), 900.0, "DISTANCE_TO_EDGE")
        fold = t.wave(t.vmath("MULTIPLY", v, (0.15, 1.0, 1.0)), 1400.0, "BANDS", 3.0, "Y")   # the twisted ends: folds along the candy
        cr = t.mapr(crin, 0.0, 0.05, 1.0, 0.0)
        col = t.mixc(t.mul(cr, 0.25), c.col, tone(t, c.col, 1.5))                          # crinkled foil catches the light at every crease
        rough = t.mapr(crin, 0.0, 0.1, 0.12, 0.3)
        rough = t.mixf(gold, rough, 0.2)
        h = t.add(t.mul(cr, -0.6), t.mul(t.mul(gold, fold), 0.5))
        return col, rough, h, {"Metallic": t.mul(gold, 0.7)}
    def inner(t, v, h):
        n = t.noise(v, 400.0, 3.0); b = specks(t, v, 1200.0, 0.1)
        return t.mixc(t.mul(b, 0.4), tuple(x * 0.7 for x in red), (0.9, 0.35, 0.2)), t.mapr(n, 0.3, 0.7, 0.06, 0.18), t.mul(n, 0.2)
    return dict(detail=detail, mat=dict(coat=0.5, coat_rough=0.05, bump_str=0.6), inner=inner, cut_ru="карамель", hulls=3,
                cuts=[((-0.12, 0, 0), (1.0, 0.1, 0.1), 0.05), ((0.12, 0, 0), (1.0, -0.1, 0.1), 0.05)])

# ---------------------------------------------------------------- MARSHMALLOW: his zefir (Map4, Higgsfield zefir.glb)
@own_food("Marshmallow", ("m4", "SM_M4_Zefir"), "Elastic", 3.5, 2, label="SM_M4_Zefir (Higgsfield)")
def zefir(o, imgs, tint, L):
    inside = own_colour_at(o, imgs, lambda co, h: h < 0.35, (0.9, 0.88, 0.85))
    def detail(t, c):
        v = c.v
        dust = specks(t, v, 2600.0, 0.35)                                          # powdered sugar
        po = pores(t, v, 2600.0)                                                   # the airy foam just under the skin
        col = t.mixc(t.mul(dust, 0.45), c.col, (1.0, 0.98, 0.98))
        col = t.mixc(t.mul(po, 0.12), col, tone(t, c.col, 0.82))
        return col, t.mapr(dust, 0.0, 1.0, 0.62, 0.85), t.add(t.mul(dust, 0.4), t.mul(po, -0.35))
    def inner(t, v, h):
        po = pores(t, v, 2000.0)
        return t.mixc(t.mul(po, 0.25), inside, tuple(x * 0.75 for x in inside)), 0.8, t.mul(po, -0.8)
    return dict(detail=detail, mat=dict(sss=0.4, sss_rad=(1.0, 0.85, 0.85), sheen=0.4, bump_str=0.7), inner=inner, cut_ru="воздушный зефир",
                cuts=[((0, 0, 0), (1.0, 0.2, 0.1), 0.05)])

# ---------------------------------------------------------------- CONDENSED MILK: his tin can (Map4)
@own_food("Condensed", ("m4", "SM_M4_CondensedMilk"), "Sticky", 8.0, 0, label="SM_M4_CondensedMilk")
def condensed(o, imgs, tint, L):
    mn, mx = bounds([o]); R = max(mx.x - mn.x, mx.y - mn.y) * 0.5; H = mx.z - mn.z
    tin = bpy.data.materials.new("Tin"); tin.use_nodes = True; tp = tin.node_tree.nodes["Principled BSDF"]; tt = NT(tin.node_tree)
    tv = tt.n("ShaderNodeTexCoord").outputs["Object"]
    rings = tt.wave(tv, 2600.0, "RINGS", 1.0)
    tt.set(tp.inputs["Base Color"], tt.mixc(tt.mapr(rings, 0.3, 0.7, 0.0, 0.2), (0.78, 0.78, 0.8), (0.55, 0.55, 0.58)))
    tt.set(tp.inputs["Roughness"], tt.mapr(rings, 0.0, 1.0, 0.18, 0.32)); tp.inputs["Metallic"].default_value = 1.0
    def detail(t, c):
        v = c.v
        cap = t.mapr(t.math("ABSOLUTE", c.nz), 0.6, 0.85)                          # lid and bottom: tin with pressed rings
        rings_ = t.wave(v, 2600.0, "RINGS", 1.0)
        fib = t.noise(t.vmath("MULTIPLY", v, (1.0, 1.0, 3.0)), 1800.0, 3.0)        # the label: paper fibre
        wr = t.noise(t.vmath("MULTIPLY", v, (0.6, 0.6, 2.5)), 120.0, 2.0)          # and a few soft wrinkles
        col = t.mixc(t.mul(t.sub(1.0, cap), t.mul(t.sub(fib, 0.5), 0.12)), c.col, (1.0, 1.0, 1.0))
        col = t.mixc(t.mul(cap, t.mapr(rings_, 0.3, 0.7, 0.0, 0.25)), col, tone(t, c.col, 0.7))
        rough = t.mixf(cap, t.mapr(c.lum, 0.1, 0.6, 0.38, 0.55), t.mapr(rings_, 0.0, 1.0, 0.18, 0.3))
        h = t.mixf(cap, t.add(t.mul(fib, 0.1), t.mul(wr, 0.4)), t.mul(rings_, 0.4))
        return col, rough, h, {"Metallic": t.mul(cap, 0.9)}
    def extras(o):
        return [ring("RimTop", R * 0.985, R * 0.035, mx.z - R * 0.03, tin), ring("RimBottom", R * 0.985, R * 0.035, mn.z + R * 0.03, tin),
                ring("LidBead", R * 0.8, R * 0.012, mx.z - R * 0.005, tin)]
    return dict(detail=detail, mat=dict(bump_str=0.5), extras=extras, inner=lambda t, v, h: ((0.95, 0.9, 0.75), 0.2, 0.0), cuts=[])

# ---------------------------------------------------------------- his generic Map4 shapes in a food's colour (the game: SM_M4_Crumb / Pea / Speck / Pebble
# painted with AntFood's colour). Same shape, same colour: subdivided round, and the food's surface on it.
@own_food("Cookie", ("m4look", "SM_M4_Crumb", (0.55, 0.38, 0.22)), "Crumbly", 3.2, 3, label="SM_M4_Crumb, цвет печенья")
def cookie(o, imgs, tint, L):
    base = (0.55, 0.38, 0.22)
    def prep(o): sub_smooth(o, 2, 0.05, 0.25, 1)
    def detail(t, c):
        v = c.v
        n = t.noise(v, 900.0, 6.0, 0.6); bake_ = t.noise(v, 70.0, 3.0)
        sug = specks(t, v, 1300.0, 0.2)                                            # sugar grains on top
        crumb = pores(t, v, 1600.0)                                                # a short, sandy dough
        crack = t.mul(t.mapr(t.voronoi(v, 260.0, "DISTANCE_TO_EDGE"), 0.0, 0.015, 1.0, 0.0), t.mapr(t.noise(v, 50.0, 2.0), 0.55, 0.65))
        col = tone(t, c.col, 1.25)                                                 # (his paint reads lighter than the raw tint)
        col = t.mixc(t.mul(t.mapr(bake_, 0.45, 0.75), 0.6), col, tone(t, c.col, 0.85))   # browner where it baked more
        col = t.mixc(t.mul(crumb, 0.35), col, tone(t, c.col, 0.8))
        col = t.mixc(t.mul(sug, 0.7), col, (0.95, 0.9, 0.8)); col = t.mixc(t.mul(crack, 0.4), col, tone(t, c.col, 0.6))
        return col, t.mixf(sug, t.mapr(n, 0.3, 0.7, 0.68, 0.85), 0.3), t.add(t.add(t.mul(n, 0.4), t.mul(sug, 0.5)), t.add(t.mul(crack, -0.8), t.mul(crumb, -0.5)))
    def inner(t, v, h):
        n = t.noise(v, 260.0, 6.0, 0.6); po = pores(t, v, 1800.0)
        col = t.mixc(t.mul(po, 0.4), tuple(min(1, x * 1.18) for x in base), tuple(x * 0.6 for x in base))
        return col, 0.85, t.add(t.mul(n, 0.6), t.mul(po, -0.8))
    return dict(prep=prep, detail=detail, mat=dict(bump_str=0.9), inner=inner, cut_ru="рассыпчатое тесто",
                cuts=[((0, 0, 0), (1.0, 0.2, 0.1), 0.06), ((0.1, 0, 0), (0.2, 1.0, 0.1), 0.06)])

@own_food("Bread", ("m4look", "SM_M4_Crumb", (0.80, 0.60, 0.35)), "Crumbly", 2.2, 2, label="SM_M4_Crumb, цвет хлеба")
def bread(o, imgs, tint, L):
    base = (0.80, 0.60, 0.35)
    def prep(o): sub_smooth(o, 2, 0.06, 0.2, 2)
    def detail(t, c):
        v = c.v
        po = pores(t, v, 1100.0, wobble=0.4)
        crust = t.mapr(t.add(c.x, t.mul(t.noise(v, 30.0, 2.0), 0.004)), L * 0.12, L * 0.3)   # one side is the crust
        col = t.mixc(t.mul(po, 0.55), c.col, tone(t, c.col, 0.5))
        col = t.mixc(t.mul(t.sub(1.0, po), 0.25), col, (0.98, 0.92, 0.78))
        col = t.mixc(crust, col, t.mixc(t.mapr(t.noise(v, 150.0, 4.0), 0.3, 0.7), (0.52, 0.27, 0.08), (0.38, 0.18, 0.05)))
        rough = t.mixf(crust, t.mapr(po, 0.0, 1.0, 0.8, 0.95), 0.55)
        h = t.mixf(crust, t.mul(po, -1.2), t.mul(t.noise(v, 400.0, 3.0), 0.4))
        return col, rough, h
    def inner(t, v, h):
        po = pores(t, v, 1100.0, wobble=0.4)
        return t.mixc(t.mul(po, 0.55), (0.88, 0.75, 0.5), (0.45, 0.33, 0.18)), 0.9, t.mul(po, -1.2)
    return dict(prep=prep, detail=detail, mat=dict(bump_str=1.0), inner=inner, cut_ru="мякиш", cuts=[((0, 0, 0), (1.0, 0.25, 0.1), 0.08)])

@own_food("Pea", ("m4look", "SM_M4_Pea", (0.35, 0.62, 0.22)), "Rolling", 0.9, 2, axis="r", label="SM_M4_Pea, зелёная")
def pea(o, imgs, tint, L):
    def prep(o): sub_smooth(o, 3, 0.03, 0.4, 3)
    def detail(t, c):
        v = c.v
        wr = t.noise(v, 520.0, 3.0, 0.5, 2.5)                                      # a pea's skin: soft wrinkles
        vary = t.noise(v, 120.0, 3.0)
        hil = t.mapr(t.vmath("DISTANCE", v, (L * 0.5, 0.0, L * 0.5), out=1), L * 0.09, L * 0.13, 1.0, 0.0)   # the scar where it hung in the pod
        col = t.mixc(t.mul(t.sub(vary, 0.5), 0.5), c.col, (0.62, 0.78, 0.3))
        col = t.mixc(hil, col, (0.78, 0.82, 0.55))
        return col, t.mapr(wr, 0.3, 0.7, 0.28, 0.42), t.add(t.mul(wr, 0.6), t.mul(hil, -0.3))
    def inner(t, v, h):
        n = t.noise(v, 900.0, 3.0)
        c = t.mixc(t.mapr(h, 0.88, 0.95), (0.62, 0.8, 0.38), (0.3, 0.55, 0.15))
        return c, t.mapr(n, 0.3, 0.7, 0.5, 0.75), t.mul(n, 0.4)
    return dict(prep=prep, detail=detail, mat=dict(sss=0.45, sss_rad=(0.6, 1.0, 0.4), coat=0.3, bump_str=0.5), inner=inner, cut_ru="половинки горошины",
                cuts=[((0, 0, 0), (1.0, 0.05, 0.3), 0.02)])

@own_food("Crisp", ("m4look", "SM_M4_Speck", (0.95, 0.80, 0.40)), "Brittle", 4.5, 2, label="SM_M4_Speck, цвет чипса")
def crisp(o, imgs, tint, L):
    def prep(o):
        sub_smooth(o, 3, 0.0)
        mn, mx = bounds([o])
        for v in o.data.vertices:                                                  # a crisp curls: a gentle saddle and a ripple
            x, y = (v.co.x - (mn.x + mx.x) * 0.5) / (mx.x - mn.x), (v.co.y - (mn.y + mx.y) * 0.5) / (mx.y - mn.y)
            v.co.z += (x * x - y * y) * L * 0.18 + math.sin(x * 9.0 + y * 4.0) * L * 0.025
        o.data.update()
    def detail(t, c):
        v = c.v
        bl = specks(t, v, 160.0, 0.35)                                             # blisters
        salt = specks(t, v, 1600.0, 0.15, (2.0, 0.3, 1.1))
        brown = t.mapr(t.noise(v, 55.0, 4.0), 0.58, 0.72)                          # over-fried spots
        oil = t.noise(v, 35.0, 3.0)
        col = t.mixc(t.mul(brown, 0.7), c.col, t.mixc(1.0, c.col, (0.78, 0.5, 0.2), "MULTIPLY"))
        col = t.mixc(t.mul(bl, 0.25), col, (1.0, 0.95, 0.7)); col = t.mixc(t.mul(salt, 0.8), col, (1.0, 1.0, 0.98))
        return col, t.mixf(salt, t.mapr(oil, 0.35, 0.65, 0.22, 0.55), 0.35), t.add(t.mul(bl, 0.8), t.mul(salt, 0.4))
    def inner(t, v, h):
        n = t.noise(v, 900.0, 3.0)
        return (0.98, 0.9, 0.62), t.mapr(n, 0.3, 0.7, 0.5, 0.75), t.mul(n, 0.6)
    return dict(prep=prep, detail=detail, mat=dict(sss=0.25, sss_rad=(1.0, 0.8, 0.4), bump_str=0.6), inner=inner, cut_ru="излом чипса",
                cuts=[((0, 0, 0), (1.0, 0.3, 0.0), 0.04)])

@own_food("Nut", ("m4look", "SM_M4_Pebble", (0.30, 0.17, 0.08)), "Hard", 2.2, 2, axis="r", label="SM_M4_Pebble, орех в шоколаде")
def nut(o, imgs, tint, L):
    def prep(o): sub_smooth(o, 2, 0.04, 0.35, 4)
    def detail(t, c):
        v = c.v
        lump = t.noise(v, 140.0, 3.0); fine = t.noise(v, 1500.0, 2.0)
        cocoa = specks(t, v, 1800.0, 0.06)
        col = t.mixc(t.mul(t.sub(lump, 0.5), 0.4), c.col, tone(t, c.col, 1.4)); col = t.mixc(t.mul(cocoa, 0.5), col, (0.18, 0.1, 0.05))
        return col, t.mixf(cocoa, t.mapr(fine, 0.3, 0.7, 0.14, 0.26), 0.7), t.add(t.mul(lump, 0.8), t.mul(fine, 0.1))
    def inner(t, v, h):
        f = t.noise(t.vmath("MULTIPLY", v, (1.0, 1.0, 0.3)), 900.0, 4.0)
        shell = t.mapr(h, 0.72, 0.78)
        c = t.mixc(shell, t.mixc(t.mapr(f, 0.3, 0.7), (0.62, 0.42, 0.2), (0.78, 0.6, 0.35)), (0.2, 0.09, 0.04))
        return c, t.mixf(shell, 0.7, 0.4), t.mul(f, t.sub(1.0, shell))
    return dict(prep=prep, detail=detail, mat=dict(coat=0.4, coat_rough=0.08, bump_str=0.6), inner=inner, cut_ru="шоколад и орех",
                cuts=[((0, 0, 0), (1.0, 0.1, 0.2), 0.02)])

# ================================================================ THE NEW FOOD (not in the game before): the pass-1 model (food_v2.py), its looks raised
# by an overlay on each of its materials (more detail on top of the pass-1 look, between the look and the states), then the
# same pipeline as his food: LOD0 <= 12k, 2048 bake, UCX, LODs, pieces, the review sheet with the ant.
def overlay(m, fn, **bsdf):
    """fn(t, c) -> (colour, roughness, height) on top of c.col / c.rough / c.h0 (the pass-1 look's sockets)"""
    nt = m.node_tree; t = NT(nt)
    g = [n for n in nt.nodes if n.type == "GROUP" and n.node_tree and n.node_tree.name.startswith("FoodStates")][0]
    def src(name):
        inp = g.inputs[name]
        return inp.links[0].from_socket if inp.links else (tuple(inp.default_value) if hasattr(inp.default_value, "__len__") else inp.default_value)
    c = Ctx(); c.t = t
    c.col, c.rough, c.h0, c.v = src("Color"), src("Rough"), src("Height"), src("Vector")
    c.x, c.y, c.z = t.sep(c.v)
    geo = t.n("ShaderNodeNewGeometry")
    nob = t.n("ShaderNodeVectorTransform"); nob.vector_type = "NORMAL"; nob.convert_from = "WORLD"; nob.convert_to = "OBJECT"
    t.set(nob.inputs[0], geo.outputs["Normal"]); c.nx, c.ny, c.nz = t.sep(nob.outputs[0])
    col, rough, h = fn(t, c)
    for k, val in (("Color", col), ("Rough", rough), ("Height", h)):
        for l in list(g.inputs[k].links): nt.links.remove(l)
        t.set(g.inputs[k], val)
    p = nt.nodes.get("Principled BSDF")
    for k, val in bsdf.items(): p.inputs[k].default_value = val
    log("overlay on", m.name)

def new_food(fid, label_ru=""):
    def deco(fn):
        NEW[fid] = dict(over=fn, label=label_ru); return fn
    return deco

def run_new(fid):
    FV.reset()
    Fd = FV.FOODS[fid]; L = Fd["size"]
    spec = Fd["build"]()
    bmn, bmx = bounds(spec["parts"])
    if (bmx.y - bmn.y) > (bmx.x - bmn.x) * 1.05:                     # (as pass 1: the long side along X)
        R90 = Matrix.Rotation(math.pi / 2, 4, "Z")
        for o in spec["parts"]:
            o.matrix_world = R90 @ o.matrix_world
            active(o); bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        spec["cuts"] = [(tuple(R90.to_3x3() @ Vector(o)), tuple(R90.to_3x3() @ Vector(n)), a, sc) for (o, n, a, sc) in spec.get("cuts", [])]
    overs = NEW[fid]["over"](L) or {}
    done = set()
    for o in spec["parts"]:
        for s in o.material_slots:
            m = s.material
            if not m or m.name in done: continue
            key = m.name.split(".")[0]
            if key in overs:
                ov = overs[key]; overlay(m, ov[0], **(ov[1] if len(ov) > 1 else {})); done.add(m.name)
    parts = FV.prepare_parts(spec["parts"], LOD0_MAX - 500)
    whole = FV.join_copies(parts, "SM_FoodHero_" + fid)
    for o in parts: bpy.data.objects.remove(o, do_unlink=True)
    whole = finish_whole([whole], fid)
    F = dict(kind=Fd["kind"], size=L, pieces=Fd["pieces"], axis=Fd["inner_axis"], label="", src=None)
    sp = dict(inner=spec["inner"], cuts_abs=spec.get("cuts", []), jelly=spec.get("jelly", 0.0), coat=spec.get("coat", 0.0),
              cut_ru=NEW[fid]["label"] or "внутренность", hulls=overs.get("_hulls", 1))
    if "hmin" in spec: sp["hmin"] = spec["hmin"]
    if "hmax" in spec: sp["hmax"] = spec["hmax"]
    return produce(fid, F, sp, whole, L)

# ---------------------------------------------------------------- the overlays
def _grain(t, c, scale, amt, col, k=0.5):
    s = specks(t, c.v, scale, amt); return t.mixc(t.mul(s, k), c.col, col), s

@new_food("Sponge", "бисквит с порами")
def up_sponge(L):
    def sponge(t, c):
        po = pores(t, c.v, 1300.0)
        col = t.mixc(t.mul(po, 0.45), c.col, tone(t, c.col, 0.5)); col = t.mixc(t.mul(t.sub(1.0, po), 0.15), col, (1.0, 0.95, 0.8))
        return col, t.mapr(po, 0.0, 1.0, 0.75, 0.95), t.add(c.h0, t.mul(po, -0.9))
    def crust(t, c):
        n = t.noise(c.v, 200.0, 5.0); cr = t.mul(t.mapr(t.voronoi(c.v, 140.0, "DISTANCE_TO_EDGE"), 0.0, 0.02, 1.0, 0.0), 0.6)
        col = t.mixc(t.mapr(n, 0.35, 0.7), c.col, tone(t, c.col, 0.7)); col = t.mixc(cr, col, tone(t, c.col, 0.5))
        return col, t.mapr(n, 0.3, 0.7, 0.35, 0.55), t.add(c.h0, t.add(t.mul(n, 0.4), t.mul(cr, -0.8)))
    return {"Sponge": (sponge,), "Crust": (crust, {"Coat Weight": 0.2})}

@new_food("Gum", "")
def up_gum(L):
    def gum(t, c):
        wr = t.noise(c.v, 700.0, 3.0, 0.5, 3.0); dust = specks(t, c.v, 2200.0, 0.3)
        col = t.mixc(t.mul(dust, 0.35), c.col, (1.0, 0.95, 0.97))
        return col, t.mapr(dust, 0.0, 1.0, 0.45, 0.7), t.add(c.h0, t.add(t.mul(wr, 0.7), t.mul(dust, 0.3)))
    return {"Gum": (gum, {"Sheen Weight": 0.3})}

@new_food("Sausage", "мясной фарш")
def up_sausage(L):
    def skin(t, c):
        wr = t.noise(t.vmath("MULTIPLY", c.v, (1.0, 0.22, 0.22)), 700.0, 3.0)      # the casing: fine wrinkles round it, not along
        fat = specks(t, c.v, 600.0, 0.25); sh = t.noise(c.v, 60.0, 2.0)
        col = t.mixc(t.mul(fat, 0.3), c.col, (1.0, 0.85, 0.8)); col = t.mixc(t.mul(t.sub(sh, 0.5), 0.3), col, tone(t, c.col, 0.8))
        return col, t.mapr(sh, 0.3, 0.7, 0.18, 0.35), t.add(c.h0, t.add(t.mul(wr, 0.5), t.mul(fat, 0.2)))
    def meat(t, c):
        fat = specks(t, c.v, 500.0, 0.4); n = t.noise(c.v, 1200.0, 3.0)
        col = t.mixc(t.mul(fat, 0.7), c.col, (0.95, 0.82, 0.78))
        return col, t.mapr(n, 0.3, 0.7, 0.4, 0.6), t.add(c.h0, t.mul(n, 0.5))
    return {"Skin": (skin, {"Coat Weight": 0.3}), "Meat": (meat,)}

@new_food("Noodle", "")
def up_noodle(L):
    def noodle(t, c):
        st = specks(t, c.v, 2500.0, 0.2); n = t.noise(c.v, 400.0, 3.0)
        col = t.mixc(t.mul(st, 0.3), c.col, (1.0, 0.98, 0.9))
        return col, t.mapr(n, 0.3, 0.7, 0.3, 0.5), t.add(c.h0, t.add(t.mul(n, 0.3), t.mul(st, 0.2)))
    return {"Noodle": (noodle, {"Subsurface Weight": 0.4})}

@new_food("Pancake", "блинное тесто")
def up_pancake(L):
    def pan(t, c):
        fry = t.mapr(t.noise(c.v, 140.0, 6.0, 0.65, 1.5), 0.42, 0.68)               # fried: golden-brown in soft lacy patches
        frk = specks(t, c.v, 700.0, 0.3)                                            # darker freckles
        bub = specks(t, c.v, 1100.0, 0.25, (0.7, 1.3, 0.2))                         # the little holes of the batter
        col = t.mixc(t.mul(fry, 0.85), c.col, (0.62, 0.36, 0.12)); col = t.mixc(t.mul(frk, 0.5), col, (0.42, 0.22, 0.07))
        col = t.mixc(t.mul(bub, 0.45), col, tone(t, c.col, 0.6))
        return col, t.mapr(fry, 0.0, 1.0, 0.55, 0.42), t.add(c.h0, t.add(t.mul(fry, 0.25), t.mul(bub, -0.7)))
    return {"Pancake": (pan,)}

@new_food("Bone", "кость: губчатое нутро")
def up_bone(L):
    def bone(t, c):
        ends = t.mapr(t.math("ABSOLUTE", c.x), L * 0.33, L * 0.46)                 # the knuckles: cartilage, porous
        po = pores(t, c.v, 2200.0)
        grease = t.noise(c.v, 45.0, 3.0); fib = t.noise(t.vmath("MULTIPLY", c.v, (0.1, 1.0, 1.0)), 900.0, 3.0)   # fibres along the shaft
        meat = t.mul(t.mapr(t.noise(c.v, 90.0, 4.0), 0.62, 0.7), t.sub(1.0, ends))   # a few scraps of meat left on it
        col = t.mixc(t.mul(ends, 0.6), c.col, (0.95, 0.88, 0.7)); col = t.mixc(t.mul(t.mul(ends, po), 0.5), col, tone(t, c.col, 0.6))
        col = t.mixc(t.mul(t.sub(fib, 0.5), 0.3), col, tone(t, c.col, 0.85)); col = t.mixc(t.mul(meat, 0.85), col, (0.45, 0.2, 0.12))
        rough = t.mixf(ends, t.mapr(grease, 0.35, 0.65, 0.25, 0.55), 0.35); rough = t.mixf(meat, rough, 0.5)
        h = t.add(c.h0, t.add(t.mul(t.mul(ends, po), -0.8), t.add(t.mul(fib, 0.25), t.mul(meat, 0.6))))
        return col, rough, h
    return {"Bone": (bone, {"Subsurface Weight": 0.25, "Coat Weight": 0.2})}

@new_food("Grape", "мякоть винограда")
def up_grape(L):
    def grape(t, c):
        bloom = t.mapr(t.noise(c.v, 60.0, 3.0), 0.45, 0.75)                        # the waxy bloom: a dusty film, wiped off in places
        dots = specks(t, c.v, 1400.0, 0.1)
        deep = t.mixc(0.55, c.col, (0.22, 0.42, 0.05))                              # a ripe green grape: deeper, clearer than pass 1
        col = t.mixc(t.mul(bloom, 0.22), deep, (0.75, 0.8, 0.7)); col = t.mixc(t.mul(dots, 0.4), col, tone(t, deep, 0.6))
        return col, t.mapr(bloom, 0.0, 1.0, 0.12, 0.45), t.add(c.h0, t.mul(dots, 0.2))
    return {"Grape": (grape, {"Subsurface Weight": 0.8})}

@new_food("Olive", "мякоть маслины")
def up_olive(L):
    def olive(t, c):
        dots = specks(t, c.v, 900.0, 0.22); oil = t.noise(c.v, 80.0, 3.0); wr = t.noise(c.v, 400.0, 3.0, 0.5, 2.0)
        col = t.mixc(t.mul(dots, 0.35), c.col, tone(t, c.col, 1.8))
        return col, t.mapr(oil, 0.3, 0.7, 0.06, 0.22), t.add(c.h0, t.add(t.mul(wr, 0.25), t.mul(dots, 0.15)))
    return {"Olive": (olive, {"Coat Weight": 0.6})}

@new_food("Jam", "")
def up_jam(L):
    def jam(t, c):
        bits = t.mapr(t.voronoi(c.v, 300.0, "F1"), 0.0, 0.18, 1.0, 0.0); bub = specks(t, c.v, 800.0, 0.06)
        col = t.mixc(t.mul(bits, 0.45), c.col, tone(t, c.col, 0.55)); col = t.mixc(t.mul(bub, 0.5), col, tone(t, c.col, 1.8))
        return col, t.mixf(bits, 0.05, 0.15), t.add(c.h0, t.add(t.mul(bits, 0.5), t.mul(bub, -0.2)))
    return {"Jam": (jam, {"Coat Weight": 0.8})}

@new_food("Butter", "сливочное масло")
def up_butter(L):
    def butter(t, c):
        ridge = t.wave(t.vmath("MULTIPLY", c.v, (1.0, 0.2, 1.0)), 700.0, "BANDS", 4.0, "X")   # knife ridges
        sweat = t.noise(c.v, 120.0, 3.0); pit = specks(t, c.v, 1600.0, 0.08)
        col = t.mixc(t.mul(t.sub(ridge, 0.5), 0.15), c.col, tone(t, c.col, 0.88))
        return col, t.mapr(sweat, 0.35, 0.65, 0.18, 0.4), t.add(c.h0, t.add(t.mul(ridge, 0.35), t.mul(pit, -0.4)))
    return {"Butter": (butter, {"Subsurface Weight": 0.3})}

@new_food("Ice", "лёд")
def up_ice(L):
    def ice(t, c):
        frost = t.mapr(t.noise(c.v, 140.0, 4.0), 0.5, 0.72); e, r = crystals(t, c.v, 1600.0)
        crack = t.mul(t.mapr(t.voronoi(c.v, 90.0, "DISTANCE_TO_EDGE"), 0.0, 0.01, 1.0, 0.0), t.mapr(t.noise(c.v, 40.0, 2.0), 0.5, 0.6))
        col = t.mixc(t.mul(frost, 0.5), c.col, (0.92, 0.96, 1.0)); col = t.mixc(t.mul(crack, 0.5), col, (1.0, 1.0, 1.0))
        return col, t.mixf(frost, 0.03, t.mapr(r, 0.0, 1.0, 0.2, 0.5)), t.add(c.h0, t.add(t.mul(frost, t.mul(e, 0.6)), t.mul(crack, 0.3)))
    return {"Ice": (ice,)}

@new_food("Cracker", "излом крекера")
def up_cracker(L):
    def cracker(t, c):
        dock = t.mapr(t.vmath("DISTANCE", t.vmath("FRACTION", t.vmath("MULTIPLY", c.v, (1.0 / (L * 0.2), 1.0 / (L * 0.2), 0.0))), (0.5, 0.5, 0.0), out=1), 0.06, 0.09, 1.0, 0.0)   # docking holes
        dock = t.mul(dock, t.mapr(t.math("ABSOLUTE", c.nz), 0.7, 0.9))             # (on the flat faces only, not down the sides)
        salt = specks(t, c.v, 900.0, 0.12, (1.0, 2.0, 0.5)); bake_ = t.noise(c.v, 60.0, 4.0); n = t.noise(c.v, 600.0, 5.0)
        col = t.mixc(t.mapr(bake_, 0.4, 0.75, 0.0, 0.6), c.col, (0.62, 0.38, 0.15)); col = t.mixc(t.mul(dock, 0.55), col, tone(t, c.col, 0.6))
        col = t.mixc(t.mul(salt, 0.8), col, (1.0, 1.0, 0.98))
        return col, t.mixf(salt, t.mapr(n, 0.3, 0.7, 0.6, 0.8), 0.3), t.add(c.h0, t.add(t.mul(dock, -1.2), t.add(t.mul(salt, 0.5), t.mul(n, 0.3))))
    return {"Cracker": (cracker,)}

@new_food("Wafer", "вафля и крем")
def up_wafer(L):
    def wafer(t, c):
        g = t.vmath("FRACTION", t.vmath("MULTIPLY", c.v, (1.0 / (L * 0.09), 1.0 / (L * 0.09), 0.0)))   # the waffle grid
        gx, gy, _ = t.sep(g)
        cell = t.mul(t.mapr(t.math("ABSOLUTE", t.sub(gx, 0.5)), 0.32, 0.4, 1.0, 0.0), t.mapr(t.math("ABSOLUTE", t.sub(gy, 0.5)), 0.32, 0.4, 1.0, 0.0))
        top = t.mapr(t.math("ABSOLUTE", c.nz), 0.6, 0.85)
        n = t.noise(c.v, 500.0, 4.0)
        col = t.mixc(t.mul(t.mul(cell, top), 0.25), c.col, tone(t, c.col, 0.78))
        return col, t.mapr(n, 0.3, 0.7, 0.55, 0.75), t.add(c.h0, t.add(t.mul(t.mul(cell, top), -1.0), t.mul(n, 0.3)))
    return {"Wafer": (wafer,)}

@new_food("Eggshell", "скорлупа и плёнка")
def up_egg(L):
    def shell(t, c):
        sp = specks(t, c.v, 260.0, 0.15); po = specks(t, c.v, 2600.0, 0.35); n = t.noise(c.v, 80.0, 3.0)
        col = t.mixc(t.mul(sp, 0.35), c.col, tone(t, c.col, 0.7)); col = t.mixc(t.mul(t.sub(n, 0.5), 0.2), col, tone(t, c.col, 1.08))
        return col, t.mapr(n, 0.3, 0.7, 0.5, 0.7), t.add(c.h0, t.add(t.mul(po, -0.5), t.mul(sp, 0.1)))
    def mem(t, c):
        wr = t.noise(c.v, 500.0, 3.0, 0.5, 3.0)
        return t.mixc(t.mul(wr, 0.2), c.col, (1.0, 1.0, 0.97)), t.mapr(wr, 0.3, 0.7, 0.25, 0.45), t.add(c.h0, t.mul(wr, 0.5))
    return {"Shell": (shell,), "Membrane": (mem, {"Sheen Weight": 0.3})}

@new_food("Watermelon", "сочная мякоть")
def up_watermelon(L):
    def wm(t, c):
        e, r = crystals(t, c.v, 900.0)                                              # the juicy granular flesh
        stripe = t.wave(t.vmath("MULTIPLY", c.v, (1.0, 0.2, 1.0)), 120.0, "BANDS", 6.0, "X")
        col = t.mixc(t.mul(e, 0.25), c.col, tone(t, c.col, 0.8)); col = t.mixc(t.mul(t.mapr(r, 0.8, 0.95), 0.3), col, tone(t, c.col, 1.25))
        return col, t.mapr(r, 0.0, 1.0, 0.08, 0.35), t.add(c.h0, t.add(t.mul(e, -0.5), t.mul(stripe, 0.05)))
    return {"Watermelon": (wm, {"Subsurface Weight": 0.3})}

@new_food("Tomato", "мякоть и семечки")
def up_tomato(L):
    def tom(t, c):
        dots = specks(t, c.v, 1100.0, 0.12); e, r = crystals(t, c.v, 700.0)
        col = t.mixc(t.mul(dots, 0.3), c.col, (1.0, 0.75, 0.4)); col = t.mixc(t.mul(e, 0.2), col, tone(t, c.col, 0.8))
        return col, t.mapr(r, 0.0, 1.0, 0.06, 0.25), t.add(c.h0, t.add(t.mul(dots, 0.15), t.mul(e, -0.3)))
    return {"Tomato": (tom, {"Subsurface Weight": 0.4, "Coat Weight": 0.4})}
