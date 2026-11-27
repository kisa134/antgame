# KAN-36, pass 3 (27.11): the raw hero food put right. Exec'd by food_hero.py after food_hero_more.py: the same helpers, and
# only the foods listed in FIX go through the code below - every other food is made exactly as before.
#   Tomato    was a wedge that read as a piece of salmon                -> a whole tomato with its calyx; halves show the seed chambers
#   Sponge    had a separate disc of "crust" lying on top                -> one piece of sponge cake, the baked crust is its own top layer
#   Eggshell, Grape, Butter: blotches, hatching, seams in the textures  -> the causes found on the renders and removed:
#               1. THE cause (seen in the AO channel, not guessed): the pre-cut pieces are made before the bake and lie exactly on
#                  the whole food, so its AO rays hit them - grey noise all over, lines along the cut, chevrons along every row
#                  of triangles where the two triangulations differ -> nothing but the food itself is in the scene while it bakes;
#               2. smart_project laid bevel and rim faces edge-on (zero-area UV triangles: broken tangents, rainbow pixels in the
#                  normal map) and cut every round thing into ragged islands -> charts by direction, unwrapped (angle based);
#               3. 6 px of margin at 2048 does not survive the mips -> 16 px;
#               4. the butter's "knife ridges" were a sub-millimetre wave over the whole block (corduroy) -> a few soft drags.
#   Also found on the way (it is in every food of pass 2; put right here for the foods of FIX only): the inner strip (_InC) was
#   baked on a sheet one metre tall, so every look that reads the position came out smeared on the cut faces - see inner_strip.
#   Ice       was milky: M_FoodV2 is an opaque subsurface material (Opacity = 1 - Jelly is its scatter, not transparency)
#             -> its own translucent master M_FoodV2_Glass (make_food_materials.py), the opacity mask baked into the blue of _R
#   Cookie, Bread, Pea, Crisp, Nut: Catmull-Clark had rounded his shapes into pebbles -> his silhouette kept (simple subdivision,
#             a light relax of the creases only), the detail on top; the same clean unwrap / AO / margin
#   Cake      its pieces were cut by flat planes -> a ragged break (the plane cut done in a warped space), a crumbly inner face;
#             the whole cake and its textures are not touched (only the pieces, the inner strip and food.json are written)
from mathutils import noise as mnoise

FIX = {      # (ragged: when a boolean cut fails and the food is cut by planes instead, the break is ragged, not flat)
    "Tomato": dict(fix=True, ragged=True, align=True), "Sponge": dict(fix=True, ragged=True), "Eggshell": dict(fix=True), "Grape": dict(fix=True, ragged=True),
    "Butter": dict(fix=True, ragged=True), "Ice": dict(fix=True, glass=True, ragged=True),
    "Cookie": dict(fix=True, ragged=True), "Bread": dict(fix=True, ragged=True), "Pea": dict(fix=True, ragged=True),
    "Crisp": dict(fix=True, ragged=True), "Nut": dict(fix=True, ragged=True),
    "Cake": dict(keep_whole=True, ragged=True),
}
FX = {}                                  # the flags of the food being made (empty: the pipeline as it was)
MARGIN_PX = 16

# ================================================================ the unwrap: charts by direction, no edge-on faces
def chart_unwrap(o, margin_px=MARGIN_PX):
    """UV0: the faces grouped by the axis their normal is nearest to (or by the builder's own face attribute 'chart' >= 1),
    a seam between the groups and on every hard crease, an angle based unwrap of each chart, the islands at one texel density."""
    me = o.data
    bm = bmesh.new(); bm.from_mesh(me); bm.faces.ensure_lookup_table()
    axes = [Vector(a) for a in ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1))]
    part = bm.faces.layers.int.get("part"); own = bm.faces.layers.int.get("chart")
    # the direction of a face is read off the shape, not off its crumbs: the normals averaged over a few rings of neighbours
    # (a rough, displaced surface would otherwise fall into a hundred little islands)
    sn = [f.normal.copy() for f in bm.faces]
    for _ in range(5):
        nn = []
        for f in bm.faces:
            a = sn[f.index] * 2.0
            for e in f.edges:
                for g in e.link_faces:
                    if g is not f and f.normal.dot(g.normal) > 0.2: a += sn[g.index]
            nn.append(a.normalized() if a.length > 1e-9 else sn[f.index])
        sn = nn
    lab = []
    for f in bm.faces:
        k = max(range(6), key=lambda i: sn[f.index].dot(axes[i]))
        if part and f[part]: k += 100
        if own and f[own] > 0: k = 1000 + f[own]
        lab.append(k)
    for _ in range(4):                   # a face that sticks into the chart next door goes over to it: straight borders, no slivers
        new = lab[:]
        for f in bm.faces:
            if lab[f.index] >= 100: continue
            cnt = {}
            for e in f.edges:
                for g in e.link_faces:
                    if g is not f and lab[g.index] < 100 and f.normal.dot(g.normal) > 0.5: cnt[lab[g.index]] = cnt.get(lab[g.index], 0) + 1
            if cnt:
                best = max(cnt, key=cnt.get)
                if best != lab[f.index] and cnt[best] >= 2 and f.normal.dot(axes[best]) > 0.38: new[f.index] = best
        lab = new
    # (the separate shells of the mesh - a stem, a sepal, a curl: each keeps at least two charts, or it would be a closed island
    #  with no seam at all, which cannot be unwrapped)
    shell = [-1] * len(bm.faces); shell_n = []
    for f0 in bm.faces:
        if shell[f0.index] >= 0: continue
        k = len(shell_n); stack = [f0]; shell[f0.index] = k; n = 0
        while stack:
            f = stack.pop(); n += 1
            for e in f.edges:
                for g in e.link_faces:
                    if shell[g.index] < 0: shell[g.index] = k; stack.append(g)
        shell_n.append(n)
    for _ in range(4):                   # an island of a few faces (the corner of a bevel, a sliver) joins the chart it touches most
        seen = set(); moved = 0
        labs = [set() for _ in shell_n]
        for f in bm.faces: labs[shell[f.index]].add(lab[f.index])
        for f0 in bm.faces:
            if f0.index in seen or lab[f0.index] >= 100: continue
            comp = [f0]; seen.add(f0.index); k0 = lab[f0.index]; i = 0; nb = {}
            while i < len(comp):
                f = comp[i]; i += 1
                for e in f.edges:
                    for g in e.link_faces:
                        if g is f: continue
                        if lab[g.index] == k0:
                            if g.index not in seen: seen.add(g.index); comp.append(g)
                        elif lab[g.index] < 100: nb[lab[g.index]] = nb.get(lab[g.index], 0) + 1
            sh = shell[f0.index]
            if len(comp) < max(8, shell_n[sh] // 60) and nb and len(labs[sh]) > 2:
                best = max(nb, key=nb.get); moved += 1
                for f in comp: lab[f.index] = best
                labs[sh] = set(lab[i] for i in range(len(lab)) if shell[i] == sh) if shell_n[sh] < 4000 else labs[sh]
        if not moved: break
    for e in bm.edges:
        fs = e.link_faces
        e.seam = len(fs) != 2 or lab[fs[0].index] != lab[fs[1].index] or (lab[fs[0].index] >= 100 and fs[0].normal.dot(fs[1].normal) < 0.25)
    bm.to_mesh(me); bm.free()
    me.uv_layers.active = me.uv_layers["UV0"]
    active(o); bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="SELECT")
    try:
        bpy.ops.uv.select_all(action="SELECT")
    except Exception: pass
    bpy.ops.uv.unwrap(method="ANGLE_BASED", fill_holes=True, correct_aspect=True, margin=0.01)
    try: bpy.ops.uv.average_islands_scale()
    except Exception as e: log("average_islands_scale", e)
    m = margin_px / float(TEX)
    try: bpy.ops.uv.pack_islands(rotate=True, margin_method="FRACTION", margin=m)
    except Exception as e:
        log("pack (fraction)", e)
        try: bpy.ops.uv.pack_islands(rotate=True, margin=m * 2.0)
        except Exception as e2: log("pack", e2)
    bpy.ops.object.mode_set(mode="OBJECT")
    log("chart unwrap:", len(set(lab)), "charts,", tris(o), "tris")

_finish_whole_v4 = finish_whole
def finish_whole(parts, fid):
    whole = _finish_whole_v4(parts, fid)
    if FX.get("fix"): chart_unwrap(whole)
    return whole

# ================================================================ the bake: flat-shaded AO, a wide margin, the opacity mask of glass
def bake_emit(o, node_name, size, mg):
    """the value of the node <node_name> of every material, as an image (the opacity mask of the ice)"""
    img = FV.new_img("OP", size); img.colorspace_settings.name = "Non-Color"
    undo = []
    for s in o.material_slots:
        nt = s.material.node_tree
        out = [n for n in nt.nodes if n.type == "OUTPUT_MATERIAL"][0]
        old = out.inputs["Surface"].links[0].from_socket if out.inputs["Surface"].links else None
        em = nt.nodes.new("ShaderNodeEmission"); src = nt.nodes.get(node_name)
        if src: nt.links.new(src.outputs[0], em.inputs["Color"])
        else: em.inputs["Color"].default_value = (1, 1, 1, 1)
        nt.links.new(em.outputs[0], out.inputs["Surface"]); undo.append((nt, out, old, em))
    FV.bake_pass(o, "EMIT", img, samples=4, margin=mg)
    for nt, out, old, em in undo:
        if old: nt.links.new(old, out.inputs["Surface"])
        nt.nodes.remove(em)
    return img

_bake_hero_v4 = bake_hero
def bake_hero(o, fid, d, L):
    p = lambda s: os.path.join(d, "T_FoodHero_%s_%s.png" % (fid, s))
    if FX.get("keep_whole"):
        log(fid, "the whole food is kept: no bake"); return {k: p(k) for k in ("C", "N", "R")}
    if not FX.get("fix"): return _bake_hero_v4(o, fid, d, L)
    t0 = time.time(); mg = MARGIN_PX
    world_for_bake(L)
    # THE cause of the blotches and chevrons in the AO: the pre-cut pieces are made before the bake and lie exactly on the food -
    # the AO rays of the whole food hit them (half of the rays, by chance: grey noise; along the cut and along every triangle row
    # where the two triangulations differ: lines and chevrons). Nothing but the food itself is in the scene while it bakes.
    others = [x for x in S().objects if x is not o and not x.hide_render]
    for x in others: x.hide_render = True
    FV.set_state([o], **{k: 0.0 for k in FV.STATES})
    u = metal_off(o)
    c = FV.new_img("C", TEX); FV.bake_pass(o, "DIFFUSE", c, samples=32, margin=mg, filt={"COLOR"}); FV.save_png(c, p("C"))
    metal_on(u)
    nimg = FV.new_img("N", TEX); nimg.colorspace_settings.name = "Non-Color"; FV.bake_pass(o, "NORMAL", nimg, samples=16, margin=mg); FV.save_png(nimg, p("N"))
    ao = FV.new_img("AO", TEX); ao.colorspace_settings.name = "Non-Color"; FV.bake_pass(o, "AO", ao, samples=128, margin=mg)
    for x in others: x.hide_render = False
    ro = FV.new_img("RO", TEX); ro.colorspace_settings.name = "Non-Color"; FV.bake_pass(o, "ROUGHNESS", ro, samples=4, margin=mg)
    chans = [(FV.pix(ao), 0), (FV.pix(ro), 0), (None, 0)]
    if FX.get("glass"): chans[2] = (FV.pix(bake_emit(o, "OP_MASK", TEX, mg)), 0)
    FV.compose(p("R"), TEX, None, chans=chans)
    for st, vals in FV.GAME_STATES:
        FV.set_state([o], **{k: 0.0 for k in FV.STATES}); FV.set_state([o], **vals)
        u = metal_off(o)
        sc = FV.new_img("SC", STEX); FV.bake_pass(o, "DIFFUSE", sc, samples=8, margin=mg // 2, filt={"COLOR"})
        metal_on(u)
        sr = FV.new_img("SR", STEX); sr.colorspace_settings.name = "Non-Color"; FV.bake_pass(o, "ROUGHNESS", sr, samples=2, margin=mg // 2)
        FV.compose(p(st), STEX, FV.pix(sc), a_from=FV.pix(sr))
    FV.set_state([o], **{k: 0.0 for k in FV.STATES})
    log(fid, "baked (pass 3) %.0f s" % (time.time() - t0))
    return {k: p(k) for k in ("C", "N", "R")}

_export_lod0_v4, _export_at_v4 = export_lod0, export_at
def _uv_moments(ob, layer, off):
    uv = ob.data.uv_layers[layer].data if layer else ob.data.uv_layers.active.data
    s = [0.0] * 6; n = 0
    for i, l in enumerate(ob.data.loops):
        c = ob.data.vertices[l.vertex_index].co - off; u, v = uv[i].uv
        s[0] += u; s[1] += v; s[2] += u * c.x * 100; s[3] += v * c.y * 100; s[4] += u * c.z * 100; s[5] += v * c.x * 100; n += 1
    return n, s

def export_lod0(o, fid, path, hulls):
    if FX.get("keep_whole"):
        # the whole food stays as it is on disk: the pieces made now must sit in the very same atlas - checked, not assumed
        mn, mx = bounds([o]); ctr = (mn + mx) * 0.5
        before = set(bpy.data.objects); bpy.ops.import_scene.fbx(filepath=path)
        new = [x for x in bpy.data.objects if x not in before]; ms = [x for x in new if x.type == "MESH" and not x.name.startswith("UCX")]
        ok = False
        if ms:
            FV.active(ms[0]); bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
            n1, a = _uv_moments(o, "UV0", ctr); n2, b = _uv_moments(ms[0], None, Vector())
            ok = n1 == n2 and all(abs(p - q) <= 2e-4 * max(1.0, abs(p)) for p, q in zip(a, b))
            log(fid, "kept whole: loops", n1, n2, "moments", ["%.4f" % x for x in a], ["%.4f" % x for x in b])
        for x in new: bpy.data.objects.remove(x, do_unlink=True)
        log(fid, "UV of the kept whole:", "MATCH" if ok else "MISMATCH - the pieces would not fit its textures")
        return ctr
    return _export_lod0_v4(o, fid, path, hulls)
def export_at(o, path, ctr):
    if FX.get("keep_whole"): return
    return _export_at_v4(o, path, ctr)

_baked_material_v4 = baked_material
def baked_material(name, paths, spec):
    """the review's stand-in for the game's material; glass: clear where the mask (blue of _R) is dark, as M_FoodV2_Glass"""
    m = _baked_material_v4(name, paths, spec)
    if FX.get("glass"):
        nt = m.node_tree; p = nt.nodes["Principled BSDF"]; t = NT(nt)
        r = [n for n in nt.nodes if n.type == "TEX_IMAGE" and n.image and n.image.filepath.replace("\\", "/").endswith("_R.png")][0]
        sep = t.n("ShaderNodeSeparateColor"); t.set(sep.inputs[0], r.outputs["Color"])
        t.set(p.inputs["Transmission Weight"], t.sub(1.0, sep.outputs[2], True))
        p.inputs["IOR"].default_value = 1.31; p.inputs["Coat Weight"].default_value = 0.3; p.inputs["Subsurface Weight"].default_value = 0.0
    return m

# ================================================================ a ragged break by planes (skins that are not closed: the cake)
def _rag(q, amp, big):
    return amp * (mnoise.noise(q / big) * 1.0 + mnoise.noise(q / (big * 0.31) + Vector((3.1, 0.7, 1.3))) * 0.45
                  + mnoise.noise(q / (big * 0.11) + Vector((7.7, 2.2, 5.1))) * 0.2)

_plane_pieces_v4 = plane_pieces
def plane_pieces(whole, plan, inner):
    """as before, but the cut is not flat: the mesh is warped along the cut's normal by a noise of the two other coordinates,
    cut by the plane, the cap meshed finely, and warped back - the cap comes out as the ragged sheet, the skin as it was"""
    if not FX.get("ragged"): return _plane_pieces_v4(whole, plan, inner)
    mn, mx = bounds([whole]); Lw = max(mx - mn); edge = Lw * 0.035
    pieces = [dup(whole, "pp")]
    if inner.name not in [m.name for m in whole.data.materials if m]: pieces[0].data.materials.append(inner)
    for ci, (co, no, amp, sc) in enumerate(plan):
        co, no = Vector(co), Vector(no).normalized()
        ta = no.orthogonal().normalized(); tb = no.cross(ta)
        w = lambda p: _rag(Vector(((p - co).dot(ta), (p - co).dot(tb), ci * 0.37)), amp * 1.8, Lw * 0.22)
        nxt = []
        for pc in pieces:
            for side in (0, 1):
                o = dup(pc, pc.name + ("a" if side == 0 else "b"))
                ii = [m.name for m in o.data.materials].index(inner.name)
                bm = bmesh.new(); bm.from_mesh(o.data)
                for v in bm.verts: v.co -= no * w(v.co)
                r = bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], dist=1e-7, plane_co=co, plane_no=no,
                                           clear_inner=(side == 0), clear_outer=(side == 1))
                cut = [g for g in r["geom_cut"] if isinstance(g, bmesh.types.BMEdge)]
                if cut:
                    try:
                        f = bmesh.ops.triangle_fill(bm, use_beauty=True, use_dissolve=False, edges=cut, normal=(no if side == 1 else -no))
                        for fc in f["geom"]:
                            if isinstance(fc, bmesh.types.BMFace): fc.material_index = ii; fc.smooth = True
                    except Exception as e:
                        log("plane cut fill failed", e)
                    for _ in range(7):       # the cap to an even mesh: split its long inner edges, re-triangulate, flip
                        cap = [fc for fc in bm.faces if fc.material_index == ii]
                        capset = set(cap)
                        long_ = [e for e in set(e for fc in cap for e in fc.edges)
                                 if e.calc_length() > edge and all(lf in capset for lf in e.link_faces) and len(e.link_faces) == 2]
                        if not long_ or len(cap) > 3000: break
                        bmesh.ops.subdivide_edges(bm, edges=long_, cuts=1)
                        cap = [fc for fc in bm.faces if fc.material_index == ii]
                        bmesh.ops.triangulate(bm, faces=[fc for fc in cap if len(fc.verts) > 3])
                        cap = [fc for fc in bm.faces if fc.material_index == ii]; capset = set(cap)
                        inn = [e for e in set(e for fc in cap for e in fc.edges) if all(lf in capset for lf in e.link_faces)]
                        bmesh.ops.beautify_fill(bm, faces=cap, edges=inn)
                    for fc in bm.faces:
                        if fc.material_index == ii: fc.smooth = True
                for v in bm.verts: v.co += no * w(v.co)
                # the crumb of the break: the cap's own vertices pushed in and out a little more
                for v in bm.verts:
                    if v.link_faces and all(lf.material_index == ii for lf in v.link_faces):
                        v.co += no * (mnoise.noise(v.co / (Lw * 0.085)) * amp * 0.55)
                n = len(bm.faces); bm.to_mesh(o.data); bm.free()
                if n > 30: nxt.append(o)
                else: bpy.data.objects.remove(o, do_unlink=True)
            bpy.data.objects.remove(pc, do_unlink=True)
        pieces = nxt
    pieces.sort(key=lambda o: -sum(p.area for p in o.data.polygons))
    log("ragged plane pieces:", [tris(p) for p in pieces])
    return pieces

def trans_off(o):
    """a linked transmission (the ice: clear where its mask is dark) bakes its colour dark: unlinked while the colour bakes"""
    undo = []
    for s in o.material_slots:
        nt = s.material.node_tree; p = nt.nodes.get("Principled BSDF")
        if not p: continue
        for l in [l for l in nt.links if l.to_socket == p.inputs["Transmission Weight"]]:
            undo.append((nt, l.from_socket, l.to_socket)); nt.links.remove(l)
    return undo

_metal_off_v4, _metal_on_v4 = metal_off, metal_on
def metal_off(o):
    return (_metal_off_v4(o), trans_off(o))
def metal_on(u):
    _metal_on_v4(u[0])
    for nt, a, b in u[1]: nt.links.new(a, b)

# ================================================================ the runs: the flags of the food on while it is made
_run_new_v4, _run_own_v4 = run_new, run_own
def _with_fx(fn, fid):
    global FX
    FX = dict(FIX.get(fid, {}))
    if FX: log(fid, "pass 3 flags", FX)
    try:
        man = fn(fid)
    finally:
        fx, FX = FX, {}
    jf = os.path.join(OUT_ART, fid, "food.json")
    if (fx.get("glass") or fx.get("align")) and MODE in ("model", "all") and os.path.exists(jf):
        with open(jf, encoding="utf-8") as f: j = json.load(f)
        if fx.get("glass"): j["glass"] = True; j["jelly"] = 0.0            # (import_food_v2.py: the translucent master)
        if fx.get("align"): j["inner_align"] = True                        # (import_food_v2.py: the inner strip centred on the whole food)
        with open(jf, "w", encoding="utf-8") as f: json.dump(j, f, indent=1, ensure_ascii=False)
    return man
def run_new(fid): return _with_fx(_run_new_v4, fid)
def run_own(fid): return _with_fx(_run_own_v4, fid)

def refood(fid, kind, cm, pieces, axis="z"):
    """a new builder for a food of pass 1 (same id, kind, size, pieces); its look is complete: no overlay on top"""
    def deco(fn):
        FV.food(fid, kind, cm, pieces, axis)(fn)
        NEW[fid]["over"] = lambda L: {}
        return fn
    return deco

def mesh_noise(o, amp, scale, weight=None, amp2=0.0, scale2=1.0):
    """the vertices pushed along their normals by a noise of their position (two octaves); weight(co, normal) -> 0..1"""
    me = o.data
    for v in me.vertices:
        w = weight(v.co, v.normal) if weight else 1.0
        if w <= 0.0: continue
        d = mnoise.noise(v.co / scale) * amp + (mnoise.noise(v.co / scale2 + Vector((5.3, 1.1, 2.9))) * amp2 if amp2 else 0.0)
        v.co += v.normal * d * w
    me.update()

# ================================================================ GRAPE: the same berry, clean; a real pedicel
@refood("Grape", "Rolling", 2.2, 2, "r")
def build_grape_fix():
    R = 0.0095; XP = R * 1.12; ZC = 0.0092
    def skin(t, v, h=None):
        x, y, z = t.sep(v)
        big = t.noise(v, 55.0, 3.0)
        col = t.mixc(t.mapr(big, 0.3, 0.7), (0.30, 0.46, 0.06), (0.50, 0.64, 0.16))
        blush = t.mapr(t.add(y, t.mul(t.sub(big, 0.5), 0.006)), 0.002, 0.009)                       # the sunny side: a little amber
        col = t.mixc(t.mul(blush, 0.5), col, (0.62, 0.58, 0.14))
        vein = t.noise(t.vmath("MULTIPLY", v, (0.14, 1.0, 1.0)), 520.0, 3.0)                        # the strands of the flesh, from the stem
        col = t.mixc(t.mul(t.mapr(vein, 0.5, 0.75), 0.16), col, (0.70, 0.78, 0.36))
        dots = specks(t, v, 1300.0, 0.07)
        col = t.mixc(t.mul(dots, 0.55), col, (0.30, 0.22, 0.06))
        bloom = t.mul(t.mapr(t.noise(v, 70.0, 4.0, 0.6), 0.42, 0.72), t.mapr(t.noise(v, 380.0, 2.0), 0.25, 0.6, 0.55, 1.0))   # the waxy film, wiped off in places
        col = t.mixc(t.mul(bloom, 0.20), col, (0.80, 0.86, 0.80))
        ring = t.mapr(t.vmath("DISTANCE", v, (XP, 0.0, ZC), out=1), 0.0016, 0.0027, 1.0, 0.0)          # the corky ring round the stem
        col = t.mixc(t.mul(ring, 0.8), col, (0.36, 0.26, 0.10))
        tip = t.mapr(t.vmath("DISTANCE", v, (-XP, 0.0, ZC), out=1), 0.0004, 0.0010, 1.0, 0.0)          # the dark point where the flower was
        col = t.mixc(t.mul(tip, 0.85), col, (0.16, 0.11, 0.04))
        rough = t.mixf(ring, t.mapr(bloom, 0.0, 1.0, 0.10, 0.40), 0.7)
        hh = t.add(t.mul(dots, 0.15), t.add(t.mul(ring, t.mul(t.noise(v, 2500.0, 2.0), 0.6)), t.mul(tip, 0.5)))
        return col, rough, hh
    def wood(t, v, h=None):
        f = t.noise(t.vmath("MULTIPLY", v, (0.2, 1.0, 1.0)), 1800.0, 3.0); n = t.noise(v, 500.0, 3.0)
        col = t.mixc(t.mapr(n, 0.3, 0.7), (0.17, 0.14, 0.04), (0.10, 0.065, 0.025))
        return t.mixc(t.mul(f, 0.3), col, (0.26, 0.20, 0.08)), t.mapr(n, 0.3, 0.7, 0.6, 0.8), t.mul(f, 0.6)
    gm = FV.food_mat("Grape", skin, sss=0.8, sss_rad=(0.7, 1.0, 0.35), sss_scale=0.006, coat=0.35, coat_rough=0.06, bump=0.00012)
    stem = FV.food_mat("GStem", wood, bump=0.00012, bump_str=0.8)
    g = FV.uvsphere("Grape", R, (0, 0, ZC), 64, 32, (1.12, 1.0, 0.98))
    mesh_noise(g, 0.00016, 0.006)
    for p in g.data.polygons: p.use_smooth = True
    FV.assign(g, gm)
    s = FV.tube("GStem", [(XP - 0.0006, 0, ZC), (XP + 0.0008, 0.0002, ZC + 0.0002), (XP + 0.0022, 0.0006, ZC + 0.0008), (XP + 0.0034, 0.0010, ZC + 0.0017)],
                0.00065, 12, True, [1.9, 1.1, 1.0, 1.4])
    for p in s.data.polygons: p.use_smooth = True
    FV.assign(s, stem)
    def inner(t, v, h):
        seeds = t.mapr(t.voronoi(v, 260.0, "F1"), 0.0, 0.12, 1.0, 0.0)
        c = t.mixc(t.mapr(h, 0.85, 0.95), (0.78, 0.86, 0.48), (0.50, 0.68, 0.15))
        c = t.mixc(t.mul(seeds, t.mapr(h, 0.25, 0.4, 1.0, 0.0)), c, (0.55, 0.40, 0.15))
        return c, 0.12, t.mul(t.noise(v, 800.0, 2.0), 0.3)
    return dict(parts=[g, s], inner=inner, cuts=[((0, 0, 0), (1, 0, 0.1), 0.0005, 0.002)], jelly=0.6)

# ================================================================ BUTTER: a block with a knife-cut end; smooth fat, a few soft drags
@refood("Butter", "Melting", 3.0, 2)
def build_butter_fix():
    def butter(t, v, h=None):
        n = t.noise(v, 90.0, 3.0)
        col = t.mixc(t.mapr(n, 0.3, 0.7, 0.0, 0.35), (1.0, 0.78, 0.30), (1.0, 0.86, 0.44))
        sweat = t.noise(v, 140.0, 3.0)
        drops = specks(t, v, 900.0, 0.05, (1.7, 0.4, 2.2)); pit = specks(t, v, 1500.0, 0.06)
        patch = t.mapr(t.noise(v, 38.0, 2.0), 0.50, 0.68)                                          # the knife dragged here and there, not everywhere
        drag = t.mul(t.wave(t.vmath("MULTIPLY", v, (0.12, 1.0, 1.0)), 110.0, "BANDS", 2.5, "Y"), patch)
        smear = t.noise(t.vmath("MULTIPLY", v, (0.2, 1.0, 1.0)), 420.0, 3.0)
        col = t.mixc(t.mul(drag, 0.14), col, (1.0, 0.95, 0.70))
        col = t.mixc(t.mul(pit, 0.25), col, (0.86, 0.66, 0.26))
        rough = t.mixf(drops, t.mapr(sweat, 0.35, 0.65, 0.22, 0.44), 0.08)
        hh = t.add(t.add(t.mul(drag, 0.40), t.mul(t.sub(smear, 0.5), 0.16)), t.add(t.mul(drops, 0.5), t.mul(pit, -0.35)))
        return col, rough, hh
    m = FV.food_mat("Butter", butter, sss=0.7, sss_rad=(1, 0.85, 0.45), sss_scale=0.004, bump=0.00025)
    b = FV.cube("Butter", (0.03, 0.022, 0.016), (0, 0, 0.008))
    sm = b.modifiers.new("sub", "SUBSURF"); sm.subdivision_type = "SIMPLE"; sm.levels = 4; FV.apply_mods(b)
    for v in b.data.vertices:
        x, y, z = v.co
        k = max(0.0, min(1.0, (x - 0.004) / 0.011))                                                # the +X end: sliced off with a knife, slanting
        v.co.x -= 0.13 * (z - 0.008) * k * k
        f = 1.0 - z / 0.016                                                                        # softened: slumped a little, wider at the foot
        v.co.x *= 1.0 + 0.03 * f * f; v.co.y *= 1.0 + 0.045 * f * f
        if z > 0.0155:                                                                             # a knife was drawn along the top: a groove ...
            a = max(0.0, min(1.0, (x + 0.0125) / 0.004)) * max(0.0, min(1.0, (0.0035 - x) / 0.003))
            v.co.z -= 0.0019 * a * math.exp(-((y - 0.0025) / 0.0034) ** 2)
            v.co.z += 0.0004 * a * (math.exp(-((y - 0.0075) / 0.0012) ** 2) + math.exp(-((y + 0.0025) / 0.0012) ** 2))   # (its raised lips)
    FV.smooth(b, 0, 0.0016, 4); FV.apply_mods(b)
    mesh_noise(b, 0.00028, 0.007, lambda co, n: 0.25 if n.z < -0.7 else 1.0)
    FV.assign(b, m)
    # ... and the curl it scraped off lies at the groove's end: a rolled ribbon of butter
    poly, n_ = [], 44
    sp_ = lambda i, th: ((0.0007 + 0.0021 * i / n_ + th) * math.cos(0.5 + i / n_ * 3.3 * math.pi), (0.0007 + 0.0021 * i / n_ + th) * math.sin(0.5 + i / n_ * 3.3 * math.pi))
    for i in range(n_ + 1): poly.append(sp_(i, 0.0))
    for i in range(n_, -1, -1): poly.append(sp_(i, 0.00055))
    curl = FV.prism("Curl", poly, -0.0034, 0.0034)
    for v in curl.data.vertices:                                                                   # (narrower at one end, as a curl is)
        k = 1.0 + 0.10 * (v.co.z / 0.0034); v.co.x *= k; v.co.y *= k
    curl.rotation_euler = (math.pi / 2, 0, math.radians(8)); curl.location = (0.0060, 0.0025, 0.016 + 0.0029)
    FV.active(curl); bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    FV.smooth(curl, 0, 0.00012, 2); FV.apply_mods(curl); FV.assign(curl, m)
    return dict(parts=[b, curl], inner=lambda t, v, h: butter(t, v), cuts=[((-0.004, 0, 0), (1, 0.2, 0.2), 0.0007, 0.003)], jelly=0.3)

# ================================================================ ICE: a tray cube - clear, a white heart of trapped air, frost, cracks
@refood("Ice", "Melting", 2.6, 3)
def build_ice_fix():
    HX, HY, HZ = 0.013, 0.012, 0.011
    def ice(t, v, h=None):
        x, y, z = t.sep(v)
        ax = t.math("ABSOLUTE", t.math("DIVIDE", x, HX)); ay = t.math("ABSOLUTE", t.math("DIVIDE", y, HY))
        az = t.math("ABSOLUTE", t.math("DIVIDE", t.sub(z, HZ), HZ))
        hi = t.math("MAXIMUM", ax, t.math("MAXIMUM", ay, az)); lo = t.math("MINIMUM", ax, t.math("MINIMUM", ay, az))
        mid = t.sub(t.sub(t.add(t.add(ax, ay), az), hi), lo)                                       # on a face: 0 at its centre, 1 at its edge
        cloudn = t.noise(v, 110.0, 4.0, 0.6)
        core = t.mapr(t.add(mid, t.mul(t.sub(cloudn, 0.5), 0.55)), 0.34, 0.80, 1.0, 0.0, smooth=True)   # the white heart, seen through every face
        needles = t.mapr(t.voronoi(t.vmath("MULTIPLY", v, (1.0, 1.0, 0.22)), 560.0, "F1"), 0.0, 0.3, 1.0, 0.0)   # the air froze in as threads
        core = t.mul(core, t.mapr(needles, 0.0, 1.0, 0.68, 1.0))
        frost = t.mapr(t.noise(v, 150.0, 4.0), 0.57, 0.75)                                         # rime where a wet hand did not melt it
        e, r = crystals(t, v, 1500.0)
        vd = t.vmath("ADD", v, t.vmath("MULTIPLY", t.noise(v, 70.0, 2.0, out="Color"), (0.004, 0.004, 0.004)))
        crack = t.mul(t.mapr(t.voronoi(vd, 95.0, "DISTANCE_TO_EDGE"), 0.0, 0.012, 1.0, 0.0), t.mapr(t.noise(v, 45.0, 2.0), 0.52, 0.62))
        bub = specks(t, v, 700.0, 0.10, (0.5, 1.5, 2.5))
        col = t.mixc(core, (0.80, 0.90, 0.97), (0.95, 0.98, 1.0))
        col = t.mixc(t.mul(frost, 0.6), col, (0.93, 0.97, 1.0)); col = t.mixc(crack, col, (1.0, 1.0, 1.0)); col = t.mixc(t.mul(bub, 0.7), col, (1.0, 1.0, 1.0))
        rough = t.mixf(frost, 0.03, t.mapr(r, 0.0, 1.0, 0.22, 0.5)); rough = t.mixf(t.mul(crack, 0.6), rough, 0.3)
        melt = t.noise(v, 60.0, 2.0)
        hh = t.add(t.add(t.mul(melt, 0.5), t.mul(frost, t.mul(e, 0.5))), t.add(t.mul(crack, -0.5), t.mul(bub, -0.3)))
        # what is not clear (the game: the blue of _R, the opacity of M_FoodV2_Glass)
        mask = t.math("MAXIMUM", t.math("MAXIMUM", t.mul(core, 0.95), t.mul(frost, 0.6)), t.math("MAXIMUM", t.mul(crack, 0.95), t.mul(bub, 0.8)))
        nd = t.n("ShaderNodeMath", operation="ADD"); nd.name = "OP_MASK"; t.set(nd.inputs[0], mask); nd.inputs[1].default_value = 0.0
        p = t.t.nodes["Principled BSDF"]
        t.set(p.inputs["Transmission Weight"], t.mul(t.sub(1.0, nd.outputs[0], True), 0.96))
        return col, rough, hh
    m = FV.food_mat("Ice", ice, trans=0.92, ior=1.31, coat=0.5, coat_rough=0.02, bump=0.00012)
    c = FV.cube("Ice", (0.026, 0.024, 0.022), (0, 0, 0.011))
    sm = c.modifiers.new("sub", "SUBSURF"); sm.subdivision_type = "SIMPLE"; sm.levels = 4; FV.apply_mods(c)
    for v in c.data.vertices:
        x, y, z = v.co
        k = 0.94 + 0.06 * (z / 0.022)                                                              # the draft of the tray: narrower at the foot
        v.co.x = x * k; v.co.y = y * k
        if z > 0.0215: v.co.z -= 0.0016 * max(0.0, 1.0 - (x * x + y * y) / 0.00013)                # the dimple of a tray cube
    FV.smooth(c, 0, 0.0028, 5); FV.apply_mods(c)
    mesh_noise(c, 0.0004, 0.009, lambda co, n: 0.3 if n.z < -0.7 else 1.0)                         # it has begun to melt: soft, uneven faces
    FV.assign(c, m)
    def inner(t, v, h):                                                                            # a break in ice is white: a rough, frosty face
        e, r = crystals(t, v, 800.0); n = t.noise(v, 200.0, 3.0)
        return t.mixc(t.mul(e, 0.3), (0.88, 0.94, 0.99), (1.0, 1.0, 1.0)), t.mapr(r, 0.0, 1.0, 0.12, 0.4), t.add(t.mul(e, -0.5), t.mul(n, 0.4))
    # three pieces, as the food table says: one crack through the middle, a second that takes a corner off one half only
    # (two cuts that both cross the whole cube make four pieces, and the fourth was simply dropped: a quarter of the cube vanished)
    return dict(parts=[c], inner=inner, cuts=[((0, 0, 0), (1, -0.3, 0.2), 0.0008, 0.002), ((0.008, 0.008, 0), (1, 1, 0.2), 0.0008, 0.002)], jelly=0.0)

# ================================================================ SPONGE: one piece of sponge cake; the crust is its baked top, not a lid
@refood("Sponge", "Crumbly", 5.5, 3)
def build_sponge_fix():
    SX, SY, SZ, DOME = 0.055, 0.04, 0.034, 0.0032
    def look(t, v, h=None):
        x, y, z = t.sep(v)
        fx = t.sub(1.0, t.math("POWER", t.math("DIVIDE", x, SX * 0.5), 2.0)); fy = t.sub(1.0, t.math("POWER", t.math("DIVIDE", y, SY * 0.5), 2.0))
        top = t.add(SZ, t.mul(DOME, t.mul(fx, fy)))                                                # the height of the baked top over this point
        wob = t.mul(t.sub(t.noise(v, 160.0, 3.0), 0.5), 0.0012)
        depth = t.add(t.sub(top, z), wob)
        crust = t.mapr(depth, 0.0011, 0.0022, 1.0, 0.0, smooth=True)                               # 1.5 mm of brown skin, seen as a line on the cut sides
        under = t.mul(t.mapr(depth, 0.0022, 0.0060, 1.0, 0.0), t.sub(1.0, crust))                  # the crumb just under it: a shade darker
        foot = t.mapr(t.add(z, wob), 0.0006, 0.0016, 1.0, 0.0)                                     # the baked bottom, thin
        po = pores(t, v, 1300.0)
        cb, cr_, ch = FV.sponge_look((0.98, 0.80, 0.42))(t, v)
        col = t.mixc(t.mul(po, 0.45), cb, tone(t, cb, 0.5)); col = t.mixc(t.mul(t.sub(1.0, po), 0.15), col, (1.0, 0.95, 0.8))
        col = t.mixc(t.mul(under, 0.35), col, (0.80, 0.56, 0.26))
        n = t.noise(v, 190.0, 5.0); fine = t.noise(v, 1400.0, 3.0)
        vd = t.vmath("ADD", v, t.vmath("MULTIPLY", t.noise(v, 120.0, 2.0, out="Color"), (0.003, 0.003, 0.003)))
        wrk = t.mul(t.mapr(t.voronoi(vd, 210.0, "DISTANCE_TO_EDGE"), 0.0, 0.016, 1.0, 0.0), t.mapr(t.noise(v, 55.0, 2.0), 0.45, 0.62, 0.0, 0.32))   # the skin of the top: a few fine wrinkles, not a net
        ccol = t.mixc(t.mapr(n, 0.3, 0.72), (0.64, 0.36, 0.12), (0.44, 0.22, 0.07)); ccol = t.mixc(wrk, ccol, (0.34, 0.16, 0.05))
        ccol = t.mixc(t.mul(specks(t, v, 900.0, 0.08), 0.6), ccol, (0.85, 0.62, 0.32))             # crumbs stuck to it
        col = t.mixc(crust, col, ccol); col = t.mixc(t.mul(foot, 0.8), col, (0.66, 0.42, 0.17))
        rough = t.mixf(crust, t.mapr(po, 0.0, 1.0, 0.75, 0.95), t.mapr(n, 0.3, 0.7, 0.36, 0.55))
        hh = t.mixf(crust, t.add(ch, t.mul(po, -0.9)), t.add(t.mul(n, 0.35), t.add(t.mul(wrk, -1.2), t.mul(fine, 0.12))))
        return col, rough, hh
    sp = FV.food_mat("Sponge", look, bump=0.0005, bump_str=0.8)
    body = FV.cube("Body", (SX, SY, SZ), (0, 0, SZ * 0.5))
    sm = body.modifiers.new("sub", "SUBSURF"); sm.subdivision_type = "SIMPLE"; sm.levels = 4; FV.apply_mods(body)
    for v in body.data.vertices:
        x, y, z = v.co
        if z > SZ * 0.99:                                                                          # the top rose in the oven: domed, rounded over at the rim
            ex, ey = abs(x) / (SX * 0.5), abs(y) / (SY * 0.5); e = max(ex, ey)
            s = max(0.0, min(1.0, (e - 0.72) / 0.28)); s = s * s * (3 - 2 * s)
            v.co.z = SZ + DOME * (1 - ex * ex) * (1 - ey * ey) - 0.0016 * s
    FV.smooth(body, 0, 0.0014, 3); FV.apply_mods(body)
    # the cut sides are crumbly, the top smoother, the foot flat on the floor
    mesh_noise(body, 0.0008, 0.0045, lambda co, n: 0.15 if n.z < -0.7 else (0.4 if n.z > 0.7 else 1.0), 0.00035, 0.0016)
    FV.assign(body, sp)
    return dict(parts=[body], inner=lambda t, v, h: look(t, v),
                # (three pieces, as the food table says: torn across twice - not once across and once along, which makes four)
                cuts=[((-0.009, 0, 0), (1, 0.2, 0.1), 0.0012, 0.008), ((0.010, 0, 0), (1, -0.15, 0.05), 0.0012, 0.008)], hmin=(0, 0, 0.0), hmax=(0, 0, 0.038))

# ================================================================ EGGSHELL: the end of an egg, broken off - built as a shell (no booleans)
@refood("Eggshell", "Brittle", 3.0, 2)
def build_eggshell_fix():
    AR, AZ, TH = 0.0172, 0.0220, 0.00045
    NS, NR = 112, 14
    Rg = random.Random(3)
    key = sorted(Rg.sample(range(NS), 26))
    kv = {k: Rg.uniform(-0.075, 0.075) for k in key}
    chips = Rg.sample(key, 4)
    for k in chips: kv[k] -= Rg.uniform(0.15, 0.25)                                                 # where a chip went deep
    def rim(i):
        """the polar angle of the broken edge on meridian i: straight runs between the corners, as a shell breaks"""
        i %= NS
        prv = max([k for k in key if k <= i] or [key[-1] - NS]); nxt = min([k for k in key if k >= i] or [key[0] + NS])
        f = 0.0 if nxt == prv else (i - prv) / float(nxt - prv)
        th = 2 * math.pi * i / NS
        return math.radians(61) + 0.085 * math.sin(th * 2 + 0.7) + 0.045 * math.sin(th * 3 + 2.0) + kv[prv % NS] * (1 - f) + kv[nxt % NS] * f
    bm = bmesh.new(); lay_ = bm.faces.layers.int.new("chart")
    def shell(ar, az, chart, mi):
        pole = bm.verts.new((0, 0, az))
        rings = [[bm.verts.new((ar * math.sin(rim(i) * j / NR) * math.cos(2 * math.pi * i / NS), ar * math.sin(rim(i) * j / NR) * math.sin(2 * math.pi * i / NS),
                                az * math.cos(rim(i) * j / NR))) for i in range(NS)] for j in range(1, NR + 1)]
        for i in range(NS):
            i2 = (i + 1) % NS
            fs = [bm.faces.new((pole, rings[0][i], rings[0][i2]))]
            for j in range(NR - 1): fs.append(bm.faces.new((rings[j][i], rings[j + 1][i], rings[j + 1][i2], rings[j][i2])))
            for f in fs: f[lay_] = chart; f.material_index = mi; f.smooth = True
        return rings[-1]
    ro = shell(AR, AZ, 1, 0); ri = shell(AR - TH, AZ - TH, 2, 1)
    for i in range(NS):
        i2 = (i + 1) % NS
        f = bm.faces.new((ro[i], ro[i2], ri[i2], ri[i])); f[lay_] = 3; f.material_index = 2; f.smooth = False
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    zmin = min(v.co.z for v in bm.verts)
    for v in bm.verts: v.co.z -= zmin
    ex = max(v.co.x for v in bm.verts) - min(v.co.x for v in bm.verts); ey = max(v.co.y for v in bm.verts) - min(v.co.y for v in bm.verts)
    rot = 0.0
    if ey > ex:                                                                                    # (the long side along X, as the pipeline wants)
        for v in bm.verts: v.co.x, v.co.y = -v.co.y, v.co.x
        rot = math.pi / 2
    H = max(v.co.z for v in bm.verts)
    outer = FV.mesh_obj("Shell", bm)
    cracks = [(2 * math.pi * chips[0] / NS + rot, 0.80), (2 * math.pi * chips[1] / NS + rot, 0.55)]   # a hairline runs up from the two deepest chips
    def shell_look(t, v, h=None):
        x, y, z = t.sep(v)
        n = t.noise(v, 80.0, 4.0)
        col = t.mixc(t.mapr(n, 0.3, 0.7), (0.52, 0.27, 0.14), (0.66, 0.40, 0.23))                     # a brown egg
        fr = specks(t, v, 260.0, 0.16); col = t.mixc(t.mul(fr, 0.55), col, (0.42, 0.22, 0.12))       # its freckles
        po = specks(t, v, 2600.0, 0.32); col = t.mixc(t.mul(po, 0.12), col, (0.62, 0.42, 0.28))       # the pores of the shell
        ca = specks(t, v, 900.0, 0.035, (2.2, 0.9, 1.4)); col = t.mixc(t.mul(ca, 0.6), col, (0.95, 0.88, 0.78))   # grains of lime
        ang = t.math("ARCTAN2", y, x); wob = t.mul(t.sub(t.noise(v, 260.0, 3.0), 0.5), 0.16); ck = 0.0
        for a0, top in cracks:
            d = t.math("ABSOLUTE", t.math("WRAP", t.sub(t.add(ang, wob), a0), math.pi, -math.pi))
            line = t.mul(t.mapr(d, 0.006, 0.016, 1.0, 0.0), t.mapr(z, H * top * 0.8, H * top, 1.0, 0.0))
            ck = line if isinstance(ck, float) else t.math("MAXIMUM", ck, line)
        col = t.mixc(t.mul(ck, 0.8), col, (0.30, 0.19, 0.11))
        rough = t.mapr(n, 0.3, 0.7, 0.48, 0.64)
        return col, rough, t.add(t.add(t.mul(po, -0.45), t.mul(ca, 0.5)), t.add(t.mul(fr, 0.06), t.mul(ck, -1.5)))
    def mem_look(t, v, h=None):                                                                    # inside: the white skin of the egg, wrinkled, a little wet
        wr = t.noise(v, 420.0, 3.0, 0.5, 3.0); dry = t.noise(v, 70.0, 2.0)
        col = t.mixc(t.mul(wr, 0.25), (0.92, 0.90, 0.84), (1.0, 0.99, 0.95)); col = t.mixc(t.mapr(dry, 0.55, 0.75, 0.0, 0.3), col, (0.86, 0.80, 0.66))
        return col, t.mapr(wr, 0.3, 0.7, 0.24, 0.46), t.mul(wr, 0.5)
    outer.data.materials.append(FV.food_mat("Shell", shell_look, bump=0.0002))
    outer.data.materials.append(FV.food_mat("Membrane", mem_look, bump=0.00015, sheen=0.3))
    outer.data.materials.append(FV.food_mat("ShellEdge", lambda t, v, h=None: ((0.96, 0.94, 0.90), 0.8, t.mul(t.noise(v, 3000.0, 2.0), 0.4)), bump=0.0001))
    return dict(parts=[outer], inner=lambda t, v, h: ((0.95, 0.93, 0.88), 0.7, t.mul(t.noise(v, 3000.0, 2.0), 0.4)), cuts=[((0, 0, 0), (0.2, 1, 0), 0.0006, 0.003)])

# ================================================================ TOMATO: a whole small tomato, the green calyx and stem on; halves show the chambers
@refood("Tomato", "Juicy", 4.0, 2)
def build_tomato_fix():
    R, HZ = 0.020, 0.0172                                                                         # 4 cm across, 3.4 cm tall
    body = FV.uvsphere("Tomato", R, (0, 0, 0), 80, 40)
    for v in body.data.vertices:
        x, y, z = v.co; r = math.hypot(x, y); th = math.atan2(y, x); u = z / R                    # u: -1 at the foot, 1 at the top
        k = 1.0 + 0.07 * u * (1.0 - u * u)                                                         # fullest above the middle
        s = max(0.0, min(1.0, (u - 0.15) / 0.8)); s = s * s * (3 - 2 * s)
        k *= 1.0 + s * (0.024 * math.cos(5 * th + 0.4) + 0.012 * math.cos(3 * th + 1.3))           # the shoulders: five soft lobes
        k *= 1.0 + 0.012 * math.cos(2 * th + 0.9)                                                  # (not a perfect circle from above)
        z2 = z * (HZ / R)
        if u > 0: z2 -= 0.0034 * math.exp(-(r / 0.0052) ** 2)                                      # the hollow the stem sits in
        else: z2 += 0.0007 * math.exp(-(r / 0.0030) ** 2)                                          # the flat little scar under it
        v.co = Vector((x * k, y * k, z2 + HZ))
    zf = min(v.co.z for v in body.data.vertices)
    for v in body.data.vertices: v.co.z -= zf
    mesh_noise(body, 0.00035, 0.011)
    for p in body.data.polygons: p.use_smooth = True
    body.data.update(); bpy.context.view_layer.update()
    def top_z(x, y):
        ok, loc, nrm, idx = body.ray_cast(Vector((x, y, 0.1)), Vector((0, 0, -1)))
        return loc.z if ok else 2 * HZ
    z0 = top_z(0.0, 0.0)
    def skin(t, v, h=None):
        x, y, z = t.sep(v)
        rxy = t.math("SQRT", t.add(t.mul(x, x), t.mul(y, y))); u = t.mapr(z, 0.0, 2 * HZ)
        n = t.noise(v, 45.0, 3.0); fine = t.noise(v, 900.0, 2.0)
        col = t.mixc(t.mapr(n, 0.3, 0.7), (0.46, 0.012, 0.007), (0.62, 0.030, 0.010))                  # ripe red, not one flat colour
        sh = t.mul(t.mapr(u, 0.60, 0.95), t.mapr(t.add(rxy, t.mul(t.sub(n, 0.5), 0.005)), 0.0150, 0.0040))   # round the stem: not quite ripe, orange
        col = t.mixc(t.mul(sh, 0.5), col, (0.74, 0.17, 0.02))
        ang = t.math("ARCTAN2", y, x); av = t.n("ShaderNodeCombineXYZ"); t.set(av.inputs[0], ang); t.set(av.inputs[1], t.mul(z, 12.0))
        streak = t.mul(t.mapr(t.noise(av.outputs[0], 5.0, 3.0), 0.5, 0.72), t.mapr(u, 0.35, 0.9))      # pale streaks running down from the stem
        col = t.mixc(t.mul(streak, 0.14), col, (0.86, 0.22, 0.05))
        dots = specks(t, v, 1200.0, 0.10); col = t.mixc(t.mul(dots, 0.22), col, (0.95, 0.55, 0.30))    # pin-point pores
        scar = t.mapr(t.vmath("DISTANCE", v, (0.0, 0.0, 0.0), out=1), 0.0010, 0.0022, 1.0, 0.0)        # the dry dot where the flower was
        col = t.mixc(t.mul(scar, 0.8), col, (0.42, 0.26, 0.10))
        rough = t.mixf(scar, t.mapr(fine, 0.3, 0.7, 0.07, 0.15), 0.6)
        return col, rough, t.add(t.mul(dots, 0.12), t.add(t.mul(fine, 0.04), t.mul(scar, -0.4)))
    def green(t, v, h=None):
        x, y, z = t.sep(v)
        rxy = t.math("SQRT", t.add(t.mul(x, x), t.mul(y, y))); n = t.noise(v, 260.0, 4.0)
        col = t.mixc(t.mapr(n, 0.3, 0.7), (0.085, 0.22, 0.03), (0.17, 0.34, 0.06))
        col = t.mixc(t.mul(t.mapr(rxy, 0.0065, 0.0105), 0.55), col, (0.34, 0.30, 0.10))                # the tips of the sepals dry first
        hairs = specks(t, v, 3000.0, 0.3); col = t.mixc(t.mul(hairs, 0.3), col, (0.50, 0.62, 0.35))    # the fine down of a tomato stem
        vein = t.noise(t.vmath("MULTIPLY", v, (1.0, 1.0, 0.2)), 1400.0, 2.0)
        return col, t.mapr(n, 0.3, 0.7, 0.48, 0.66), t.add(t.mul(vein, 0.4), t.mul(hairs, 0.3))
    tm = FV.food_mat("Tomato", skin, sss=0.35, sss_rad=(1, 0.25, 0.15), sss_scale=0.004, coat=0.6, coat_rough=0.04, bump=0.00015)
    gm = FV.food_mat("Calyx", green, bump=0.00012, bump_str=0.8, sheen=0.4)
    FV.assign(body, tm)
    parts = [body]; Rg = random.Random(21)
    for k in range(5):                                                                             # five sepals: on the shoulder, the tips curling up
        a0 = 2 * math.pi * k / 5 + Rg.uniform(-0.16, 0.16); ln = Rg.uniform(0.0085, 0.0105); wd = Rg.uniform(0.0015, 0.0019)
        lift = Rg.uniform(0.0025, 0.0050); bend = Rg.uniform(-0.35, 0.35); NU, NV = 10, 4
        bm = bmesh.new(); grid = []
        for i in range(NU + 1):
            f = i / float(NU); row = []
            w = wd * (0.55 + 1.6 * f) * (1.0 - f) ** 0.8 if f < 0.999 else 0.0                     # widest a third out, a point at the tip
            for j in range(NV + 1):
                c = (j / float(NV) - 0.5) * 2.0
                rr = 0.0012 + ln * f; aa = a0 + bend * f * f
                px = rr * math.cos(aa) - c * w * math.sin(aa); py = rr * math.sin(aa) + c * w * math.cos(aa)
                pz = top_z(px, py) + 0.00028 + lift * max(0.0, (f - 0.5) / 0.5) ** 2 + 0.00035 * c * c - 0.0002 * (1 - c * c) * f
                row.append(bm.verts.new((px, py, pz)))
            grid.append(row)
        for i in range(NU):
            for j in range(NV): bm.faces.new((grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]))
        bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-6)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        sp = FV.mesh_obj("Sepal%d" % k, bm)
        so = sp.modifiers.new("sol", "SOLIDIFY"); so.thickness = 0.00032; so.offset = 0.0; FV.apply_mods(sp)
        for p in sp.data.polygons: p.use_smooth = True
        FV.assign(sp, gm); parts.append(sp)
    hub = FV.uvsphere("Hub", 0.0023, (0, 0, z0 + 0.0002), 20, 10, (1.0, 1.0, 0.36))
    for p in hub.data.polygons: p.use_smooth = True
    FV.assign(hub, gm); parts.append(hub)
    st = FV.tube("Stem", [(0, 0, z0 - 0.0006), (0.0002, 0.0, z0 + 0.0022), (0.0009, 0.0003, z0 + 0.0046), (0.0024, 0.0008, z0 + 0.0066)], 0.00095, 12, True, [1.5, 1.0, 1.22, 1.05])
    for p in st.data.polygons: p.use_smooth = True
    FV.assign(st, gm); parts.append(st)
    def inner(t, v, h):
        """the cut through the stem: the wall of flesh, the pale core, two chambers of gel with their seeds"""
        x, y, z = t.sep(v)
        ex = t.math("DIVIDE", x, R * 1.03); ez = t.math("DIVIDE", t.sub(z, HZ), HZ)
        rho = t.math("SQRT", t.add(t.mul(ex, ex), t.mul(ez, ez)))
        wob = t.mul(t.sub(t.noise(v, 140.0, 3.0), 0.5), 0.10)
        wall = t.mapr(t.add(rho, wob), 0.70, 0.80, smooth=True)
        skin_ = t.mapr(rho, 0.955, 0.985)
        # the core: thick under the stem, swelling into the chambers half way down (where the seeds hang), ending in a round point
        ezc = t.math("DIVIDE", t.add(ez, 0.02), 0.80)
        bulge = t.add(0.13, t.mul(0.10, t.mapr(t.math("ABSOLUTE", t.add(ez, 0.05)), 0.0, 0.5, 1.0, 0.0, smooth=True)))
        exc = t.math("DIVIDE", t.add(t.math("ABSOLUTE", ex), t.mul(wob, 0.6)), t.add(bulge, t.mul(0.07, t.mapr(ez, 0.3, 0.8))))
        core = t.mapr(t.math("SQRT", t.add(t.mul(exc, exc), t.math("POWER", t.math("ABSOLUTE", ezc), 4.0))), 0.80, 1.15, 1.0, 0.0, smooth=True)
        gel = t.mul(t.sub(1.0, wall), t.sub(1.0, core))
        sv = t.voronoi(t.vmath("MULTIPLY", v, (1.0, 1.0, 0.62)), 215.0, "F1")
        seed = t.mul(t.mapr(sv, 0.15, 0.20, 1.0, 0.0, smooth=True), gel); rim_ = t.mul(t.mapr(sv, 0.20, 0.25, 1.0, 0.0), gel)
        halo = t.mul(t.mapr(sv, 0.24, 0.46, 1.0, 0.0, smooth=True), gel)
        fn = t.noise(v, 300.0, 3.0)
        ang = t.math("ARCTAN2", ez, ex); av = t.n("ShaderNodeCombineXYZ"); t.set(av.inputs[0], ang); t.set(av.inputs[1], t.mul(rho, 0.6))
        fibre = t.mapr(t.noise(av.outputs[0], 14.0, 2.0), 0.45, 0.75)                               # the wall's flesh runs from the skin inwards
        flesh = t.mixc(t.mapr(fn, 0.3, 0.7), (0.70, 0.060, 0.030), (0.84, 0.15, 0.07)); flesh = t.mixc(t.mul(fibre, 0.3), flesh, (0.92, 0.36, 0.22))
        flesh = t.mixc(t.mul(specks(t, v, 700.0, 0.12), 0.5), flesh, (0.96, 0.66, 0.48))             # its little white vessels, cut across
        gelc = t.mixc(halo, (0.66, 0.20, 0.035), (0.80, 0.60, 0.16))                                  # the gel: clear red, a green-gold sac round every seed
        corec = t.mixc(t.mapr(fn, 0.3, 0.7), (0.90, 0.46, 0.34), (0.95, 0.66, 0.54))
        col = t.mixc(gel, flesh, gelc); col = t.mixc(core, col, corec)
        col = t.mixc(rim_, col, (0.62, 0.46, 0.16)); col = t.mixc(seed, col, (0.93, 0.85, 0.58))
        col = t.mixc(skin_, col, (0.50, 0.020, 0.010))
        rough = t.mixf(gel, t.mapr(fn, 0.3, 0.7, 0.22, 0.36), 0.06)
        return col, rough, t.add(t.add(t.mul(seed, 0.8), t.mul(gel, -0.4)), t.add(t.mul(fibre, t.mul(wall, 0.25)), t.mul(t.noise(v, 1500.0, 2.0), 0.12)))
    return dict(parts=parts, inner=inner, cuts=[((0.0, 0.0030, 0.0), (0.04, 1.0, 0.02), 0.00022, 0.007)], inner_sss=0.12)

# ================================================================ SECOND PASS: his generic shapes keep their silhouette
def keep_shape(o, relax=2, factor=0.5, budget=9000):
    """his low-poly shape made dense WITHOUT rounding it: simple subdivision (every face stays where it is), then the creases
    alone softened by a couple of smoothing steps one small edge wide. (Catmull-Clark pulled every corner in: a pebble.)"""
    n0 = max(1, tris(o)); levels = 1
    while n0 * 4 ** (levels + 1) <= budget and levels < 4: levels += 1
    s = o.modifiers.new("sub", "SUBSURF"); s.subdivision_type = "SIMPLE"; s.levels = levels; s.render_levels = levels; FV.apply_mods(o)
    bm = bmesh.new(); bm.from_mesh(o.data)
    for _ in range(relax): bmesh.ops.smooth_vert(bm, verts=bm.verts[:], factor=factor, use_axis_x=True, use_axis_y=True, use_axis_z=True)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(o.data); bm.free()
    for p in o.data.polygons: p.use_smooth = True
    o.data.update()
    log(o.name, "shape kept:", n0, "->", tris(o), "tris, simple x%d, relax %d" % (levels, relax))

def reshape(fid, prep, cuts=None):
    """the same spec (his colours, the detail layers, the cuts), a new prep"""
    old = OWN[fid]["spec"]
    def spec(o, imgs, tint, L):
        sp = old(o, imgs, tint, L); sp["prep"] = lambda ob: prep(ob, L)
        if cuts: sp["cuts"] = cuts
        return sp
    OWN[fid]["spec"] = spec

# (the relief added here is surface grain only - a fraction of a millimetre: his faces, ridges and corners stay where he put them)
def _cookie_prep(o, L):
    keep_shape(o, 1, 0.35)
    mesh_noise(o, L * 0.0045, L * 0.035, None, L * 0.002, L * 0.012)                               # a broken biscuit: a sandy surface
def _bread_prep(o, L):
    keep_shape(o, 1, 0.35)
    mesh_noise(o, L * 0.006, L * 0.045, None, L * 0.003, L * 0.015)                                # torn crumb: open, uneven
def _pea_prep(o, L):
    keep_shape(o, 2, 0.5)                                                                          # a pea is smooth: no lumps
def _crisp_prep(o, L):
    keep_shape(o, 1, 0.35)
    mesh_noise(o, L * 0.003, L * 0.03, None, L * 0.0012, L * 0.012)                                # blistered by the oil
def _nut_prep(o, L):
    keep_shape(o, 1, 0.4)
    mesh_noise(o, L * 0.003, L * 0.08)                                                             # the chocolate took the nut's lumps
for _fid, _prep in (("Bread", _bread_prep), ("Pea", _pea_prep), ("Crisp", _crisp_prep), ("Nut", _nut_prep)):
    reshape(_fid, _prep)
# (the cookie breaks into three, as the food table says: two breaks side by side - its old two crossed, made four, and one was dropped)
reshape("Cookie", _cookie_prep, [((-0.14, 0, 0), (1.0, 0.25, 0.1), 0.06), ((0.16, 0, 0), (1.0, -0.3, 0.1), 0.06)])

# ================================================================ CAKE: the pieces only - a ragged break, a crumbly face
_cake_spec_v4 = OWN["Cake"]["spec"]
def _cake_spec(o, imgs, tint, L):
    sp = _cake_spec_v4(o, imgs, tint, L)
    B = dict(cr=(0.27, 0.43), gl=(0.66, 0.76))
    sponge = own_colour_at(o, imgs, lambda co, h: 0.05 < h < 0.24 or 0.47 < h < 0.62, (0.55, 0.16, 0.2))
    cream = own_colour_at(o, imgs, lambda co, h: 0.31 < h < 0.40, (0.85, 0.75, 0.72))
    glaze = own_colour_at(o, imgs, lambda co, h: 0.68 < h < 0.74, (0.75, 0.3, 0.38))
    top = own_colour_at(o, imgs, lambda co, h: h > 0.85, (0.9, 0.85, 0.85))
    def inner(t, v, h):
        wob = t.mul(t.sub(t.noise(v, 90.0, 3.0), 0.5), 0.035); hw = t.add(h, wob)                    # the layers do not break along a ruler
        po = pores(t, v, 1500.0); big = t.mapr(t.voronoi(v, 420.0, "F1"), 0.05, 0.12, 1.0, 0.0)       # the crumb's cells and a few torn holes
        hole = t.mul(big, t.mapr(t.noise(v, 160.0, 2.0), 0.5, 0.62))
        spc = t.mixc(t.mul(po, 0.6), tuple(min(1.0, x * 1.12) for x in sponge), tuple(x * 0.36 for x in sponge))
        spc = t.mixc(t.mul(t.sub(1.0, po), 0.22), spc, (1.0, 0.86, 0.82))                            # the walls of the cells catch the light
        spc = t.mixc(hole, spc, tuple(x * 0.22 for x in sponge))
        crm = t.mixc(t.mapr(t.voronoi(v, 400.0, "F1"), 0.0, 0.12, 0.8, 0.0), cream, (0.55, 0.05, 0.12))   # the cream with its berry bits
        crm = t.mixc(t.mul(t.sub(t.noise(v, 600.0, 3.0), 0.5), 0.3), crm, (1.0, 1.0, 1.0))
        cr = smoothband(t, hw, *B["cr"], 0.012); gl = smoothband(t, hw, *B["gl"], 0.01); tp = t.mapr(hw, 0.75, 0.78)
        col = t.mixc(cr, spc, crm); col = t.mixc(gl, col, glaze); col = t.mixc(tp, col, top)
        soft = t.add(t.add(cr, gl), tp, True)
        rough = t.mixf(soft, t.mapr(po, 0.0, 1.0, 0.78, 0.95), 0.34)
        hh = t.mixf(soft, t.add(t.mul(po, -1.0), t.mul(hole, -1.6)), t.mul(t.noise(v, 500.0, 3.0), 0.3))
        return col, rough, hh
    sp["inner"] = inner
    return sp
OWN["Cake"]["spec"] = _cake_spec

# ================================================================ the inner strip: baked on a sheet of the food's own size
_inner_strip_v4 = inner_strip
def inner_strip(fid, d, inner_fn, mn, mx, axis):
    """T_FoodHero_<fid>_InC. The sheet of pass 2 was size.x wide and ONE METRE tall (the plane's scale was set on the wrong axis:
    seen on the tomato's strip - its whole cross-section a hair-line across the middle), and had no width at all for a food that
    gives its own height range. Every look that reads the position (pores, seeds, a core) came out smeared. Here the sheet is the
    food's own cross-section: as wide as the food, from the foot to the top of its height range (or one radius deep)."""
    if not FX: return _inner_strip_v4(fid, d, inner_fn, mn, mx, axis)
    w = bpy.data.objects.get("SM_FoodHero_" + fid)
    wmn, wmx = bounds([w]) if w else (mn, mx)
    cx, cy, sx = (wmn.x + wmx.x) * 0.5, (wmn.y + wmx.y) * 0.5, wmx.x - wmn.x
    bpy.ops.mesh.primitive_plane_add(size=1.0); pl = bpy.context.object
    if axis == "z":
        pl.scale = (sx, mx.z - mn.z, 1.0); pl.rotation_euler = (math.pi / 2, 0, 0); pl.location = (cx, cy, (mn.z + mx.z) * 0.5)
    else:
        R = max(sx, wmx.y - wmn.y) * 0.5
        pl.scale = (sx, R, 1.0); pl.location = (cx, cy + R * 0.5, (wmn.z + wmx.z) * 0.5)
    active(pl); bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    def strip(t, v):
        uv = t.n("ShaderNodeTexCoord").outputs["UV"]
        return inner_fn(t, v, t.sep(uv)[1])
    FV.assign(pl, FV.food_mat("InnerStrip_" + fid, strip, bump=0.0003))
    others = [x for x in S().objects if x is not pl and not x.hide_render]
    for x in others: x.hide_render = True
    ic = FV.new_img("IC", STEX); FV.bake_pass(pl, "DIFFUSE", ic, samples=16, filt={"COLOR"}); FV.save_png(ic, os.path.join(d, "T_FoodHero_%s_InC.png" % fid))
    for x in others: x.hide_render = False
    bpy.data.objects.remove(pl, do_unlink=True)
    log(fid, "inner strip: %.1f x %.1f cm" % (sx * 100, ((mx.z - mn.z) if axis == "z" else max(sx, wmx.y - wmn.y) * 0.5) * 100))

# ================================================================ the boolean pieces: checked for their cut faces
_pieces_from_parts_v2 = FV.pieces_from_parts
def _pieces_from_parts(parts, plan, inner_mat, seed):
    """When Blender's exact solver gives up on a mesh, FV.boolean falls back to a solver that does not hand the cutter's material
    to the faces it makes: the cut comes out in the Outer slot, with no place in the atlas - black in the game (seen on the
    sponge and the ice of this pass; the check below found the same in pass 2's Chocolate and Watermelon, which are not touched
    here). A piece without an inner face means the cut failed: none is returned, and the food is cut by planes instead."""
    ps = _pieces_from_parts_v2(parts, plan, inner_mat, seed)
    if not FX: return ps
    def share(p):
        ii = [i for i, s in enumerate(p.material_slots) if s.material and s.material.name == inner_mat.name]
        tot = sum(pl.area for pl in p.data.polygons)
        return sum(pl.area for pl in p.data.polygons if pl.material_index in ii) / max(1e-12, tot)
    sh = [share(p) for p in ps]
    if any(s < 0.01 for s in sh):
        log("boolean pieces without a cut face (inner share %s): cut by planes instead" % ["%.0f%%" % (s * 100) for s in sh])
        for p in ps: bpy.data.objects.remove(p, do_unlink=True)
        return []
    return ps
FV.pieces_from_parts = _pieces_from_parts
