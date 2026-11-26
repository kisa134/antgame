# The owner's palette for hero pieces outside the kitchen block (fix14: the kitchen chairs and the high chair, hero_living.py).
# Rule 2: the colours are the owner's - the map v4 looks (Source/AntKitchen/Map4Look.cpp: per old slot, his own textures
# SourceArt/Blender/textures_own/T_M4_<Set>_{C,R} x tint, tile in uu), the same as hero_kitchen_block.py pass 5 bakes for the
# kitchen block. The hero model is baked a second time with its materials swapped for those looks -> <Name>/tex_owner/ (the
# manifest's textures_owner -> MI_<Name>_Owner, the game's default; -AntHeroPalette=new shows the painted set, MI_<Name>).
# (load_m4looks / owner_look are hero_kitchen_block.py's, which runs its main() on import and cannot be imported.)
import bpy, os, re
import hero_lib as H

HERE = os.path.dirname(os.path.abspath(__file__))
TEXOWN = os.path.join(HERE, "..", "..", "SourceArt", "Blender", "textures_own")
M4LOOKS = {}

def load_m4looks():
    if M4LOOKS: return
    src = open(os.path.join(HERE, "..", "..", "Source", "AntKitchen", "Map4Look.cpp"), encoding="utf-8", errors="ignore").read()
    num = r"\s*(-?[\d.]+)f"
    pat = re.compile(r'\{\s*TEXT\("([^"]+)"\),\s*TEXT\("([^"]+)"\),' + ",".join([num] * 11))
    for m in pat.finditer(src):
        k = m.group(1)
        if k in M4LOOKS: continue
        v = [float(m.group(i)) for i in range(3, 14)]
        M4LOOKS[k] = {"set": m.group(2), "tile": v[0], "tint": v[1:4], "rmul": v[4], "radd": v[5], "metal": v[6], "nstr": v[7]}
    print("HERO owner palette: %d looks from Map4Look.cpp" % len(M4LOOKS))

def _img_tex(m, path, data, x, y, vec):
    t = H.N(m, "ShaderNodeTexImage", x, y); t.image = bpy.data.images.load(path, check_existing=True)
    if data: t.image.colorspace_settings.name = "Non-Color"
    t.projection = "BOX"; t.projection_blend = 0.25; t.interpolation = "Cubic"
    H.L(m, vec, t.inputs["Vector"]); return t

def owner_look(name, slot, hero_mat):
    """the map v4 look of an old slot as a Blender material (None: no look - the hero material stays)"""
    lk = M4LOOKS.get("%s/%s" % (name, slot)) or M4LOOKS.get(slot)
    if not lk: return None
    key = "Owner_%s_%s" % (lk["set"], "_".join("%.2f" % c for c in lk["tint"] + [lk["rmul"], lk["radd"], lk["metal"]]))
    m = bpy.data.materials.get(key)
    if m: return m
    cpath = [p for p in (os.path.join(TEXOWN, "T_M4_%s_C.%s" % (lk["set"], e)) for e in ("jpg", "png")) if os.path.exists(p)]
    if not cpath: return None
    rpath = [p for p in (os.path.join(TEXOWN, "T_M4_%s_R.%s" % (lk["set"], e)) for e in ("jpg", "png")) if os.path.exists(p)]
    m, p = H.mat(key)
    tc = H.N(m, "ShaderNodeTexCoord", -1300); mp = H.N(m, "ShaderNodeMapping", -1100)
    s = 1.0 / max(0.05, lk["tile"] / 1900.0)                        # (one repeat per tile uu, as M_M4 lays it)
    mp.inputs["Scale"].default_value = (s, s, s); H.L(m, tc.outputs["Object"], mp.inputs["Vector"])
    c = _img_tex(m, cpath[0], False, -800, 300, mp.outputs["Vector"])
    mul = H.N(m, "ShaderNodeMix", -450, 300); mul.data_type = "RGBA"; mul.blend_type = "MULTIPLY"; mul.inputs[0].default_value = 1.0
    mul.inputs[7].default_value = (*[min(1.0, t) for t in lk["tint"]], 1)
    H.L(m, c.outputs["Color"], mul.inputs[6])
    if max(lk["tint"]) > 1.0:                                        # (tints over 1 brighten: M_M4 multiplies unclamped)
        br = H.N(m, "ShaderNodeMix", -300, 300); br.data_type = "RGBA"; br.blend_type = "MULTIPLY"; br.inputs[0].default_value = 1.0
        br.inputs[7].default_value = (*[max(1.0, t) for t in lk["tint"]], 1); H.L(m, mul.outputs[2], br.inputs[6]); mul = br
    H.L(m, mul.outputs[2], p.inputs["Base Color"])
    if rpath:
        r = _img_tex(m, rpath[0], True, -800, -50, mp.outputs["Vector"]); sep = H.N(m, "ShaderNodeSeparateColor", -600, -50)
        H.L(m, r.outputs["Color"], sep.inputs["Color"])
        ma = H.N(m, "ShaderNodeMath", -450, -50); ma.operation = "MULTIPLY_ADD"; ma.use_clamp = True
        ma.inputs[1].default_value = lk["rmul"]; ma.inputs[2].default_value = lk["radd"]
        H.L(m, sep.outputs["Green"], ma.inputs[0]); H.L(m, ma.outputs[0], p.inputs["Roughness"])
    else:
        p.inputs["Roughness"].default_value = min(1.0, 0.5 * lk["rmul"] + lk["radd"])
    p.inputs["Metallic"].default_value = min(1.0, lk["metal"])
    bw = H.N(m, "ShaderNodeRGBToBW", -450, -300); H.L(m, c.outputs["Color"], bw.inputs["Color"])
    bp = H.N(m, "ShaderNodeBump", -250, -300); bp.inputs["Strength"].default_value = 0.12 * lk["nstr"]; bp.inputs["Distance"].default_value = 0.0004
    H.L(m, bw.outputs["Val"], bp.inputs["Height"]); H.L(m, bp.outputs["Normal"], p.inputs["Normal"])
    H.tag(m, min(1.0, lk["metal"]), float(hero_mat.get("hero_porosity", 0.2)) if hero_mat else 0.2)
    return m

def bake_owner(o, name, out_dir, size, slot_map, **bake_kw):
    """o (its hero materials still on) baked in the owner's looks: slot_map {hero material name: old slot}; returns the
    paths of tex_owner/T_<name>_{C,N,R}.png or None when no material has a look. o's materials are put back."""
    load_m4looks()
    undo = []
    for i, s in enumerate(o.material_slots):
        hm = s.material
        if not hm: continue
        old = slot_map.get(hm.name.split(".")[0])
        om = owner_look(name, old, hm) if old else None
        if om:
            undo.append((i, hm)); s.material = om
    if not undo:
        print("HERO %s: owner palette - no map v4 look for %s" % (name, [s.material.name for s in o.material_slots if s.material])); return None
    print("HERO %s: owner palette for %s" % (name, [(o.material_slots[i].material.name, hm.name) for i, hm in undo]))
    paths = H.bake_set(o, name, os.path.join(out_dir, "tex_owner"), size, **bake_kw)
    for i, hm in undo: o.material_slots[i].material = hm
    return paths
