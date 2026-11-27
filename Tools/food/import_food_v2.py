# KAN-36: imports the V2 food (Tools/food/food_v2.py -> SourceArt/FoodV2/<Id>/) into /Game/FoodV2/SM_FoodV2_<Id>/:
#   SM_FoodV2_<Id> (+ _P<k> pieces)   static meshes at real size: FBX metres, imported at 1 m = 1900 uu (an ant is ~15 uu);
#                                     collision = the model's own triangles (complex as simple); Nanite off
#   T_FoodV2_<Id>_*                    textures (C sRGB, N normal with the green flipped: Blender is OpenGL, R masks, states sRGB+alpha)
#   MI_FoodV2_<Id>                     M_FoodV2 with the textures (jelly from food.json)
#   MI_FoodV2_<Id>_Inner[_P<k>]        M_FoodV2_Inner: the inner strip, height range in each piece's own space
# The game uses them only with -AntFoodV2 (AntFood.cpp). Needs Tools/food/make_food_materials.py first.
# Run: UnrealEditor-Cmd.exe AntKitchen.uproject -run=pythonscript -script=Tools/food/import_food_v2.py   (ANT_FOOD=Cake,Gummy to limit)
# ANT_FOOD_SET=hero: the hero set instead (Tools/food/food_hero.py -> SourceArt/FoodHero/<Id>/ -> /Game/FoodHero/SM_FoodHero_<Id>/):
#   the same materials (M_FoodV2, M_FoodV2_Inner), plus the UCX_ hulls of the FBX as the simple collision (complex = the triangles,
#   for the ants) and the LOD1 / LOD2 FBX as LODs. Used by the game only with -AntFoodHero[=Cake+Apple] (AntFood.cpp).
import unreal, os, glob, json
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SET = "FoodHero" if os.environ.get("ANT_FOOD_SET", "").lower() == "hero" else "FoodV2"
HERO = SET == "FoodHero"
SRC = os.path.join(PROJ, "SourceArt", SET)
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
UU_PER_M = 1900.0
ONLY = [s.strip() for s in os.environ.get("ANT_FOOD", "").split(",") if s.strip()]

def log(s): unreal.log_warning("FOODIMP " + s)

def task(f, dest, name, opts=None):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f); t.set_editor_property("destination_path", dest); t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True); t.set_editor_property("replace_existing", True); t.set_editor_property("save", False)
    if opts: t.set_editor_property("options", opts)
    AT.import_asset_tasks([t])
    return unreal.load_asset(dest + "/" + name)

def import_tex(f, dest, kind):
    name = os.path.splitext(os.path.basename(f))[0]
    a = task(f, dest, name)
    if not a: log("texture failed " + f); return None
    if kind == "N":
        a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        a.set_editor_property("srgb", False); a.set_editor_property("flip_green_channel", True)
        a.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    elif kind == "R":
        a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        a.set_editor_property("srgb", False); a.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    else:
        a.set_editor_property("srgb", True); a.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    EAL.save_asset(a.get_path_name())
    return a

def import_mesh(f, dest, name):
    o = unreal.FbxImportUI()
    o.set_editor_property("import_mesh", True); o.set_editor_property("import_as_skeletal", False)
    o.set_editor_property("import_materials", False); o.set_editor_property("import_textures", False); o.set_editor_property("import_animations", False)
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = o.get_editor_property("static_mesh_import_data")
    sm.set_editor_property("combine_meshes", True); sm.set_editor_property("auto_generate_collision", False)
    sm.set_editor_property("generate_lightmap_u_vs", False); sm.set_editor_property("remove_degenerates", False)
    sm.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS if HERO else unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    if HERO:
        try: sm.set_editor_property("one_convex_hull_per_ucx", True)
        except Exception: pass
    sm.set_editor_property("import_uniform_scale", UU_PER_M / 100.0)      # FBX metres come in as cm: x19 -> 1 m = 1900 uu
    for k, v in (("build_nanite", False), ("build_reversed_index_buffer", False)):
        try: sm.set_editor_property(k, v)
        except Exception: pass
    if HERO and EAL.does_asset_exist(dest + "/" + name):
        EAL.delete_asset(dest + "/" + name)   # (a reimport over an asset made by the other importer keeps its unit and old hulls)
    mesh = task(f, dest, name, o)
    if not mesh: log("mesh failed " + f); return None
    try:
        ns = mesh.get_editor_property("nanite_settings")
        if ns.get_editor_property("enabled"): ns.set_editor_property("enabled", False); mesh.set_editor_property("nanite_settings", ns)
    except Exception: pass
    bs = mesh.get_editor_property("body_setup")
    if bs:
        agg = bs.get_editor_property("agg_geom")
        n_ucx = len(agg.get_editor_property("convex_elems"))
        if HERO and n_ucx > 0:
            # the hero set brings its own UCX_ hulls: simple = the hulls (physics), complex = the triangles (the ants walk on the shape)
            for k in ("box_elems", "sphere_elems", "sphyl_elems"): agg.set_editor_property(k, [])
            bs.set_editor_property("agg_geom", agg)
            bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
            log("%s: %d UCX hulls kept" % (name, n_ucx))
        else:
            for k in ("box_elems", "sphere_elems", "sphyl_elems", "convex_elems"): agg.set_editor_property(k, [])
            bs.set_editor_property("agg_geom", agg)
            bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)   # collision by shape
    EAL.save_loaded_asset(mesh)
    return mesh

def mi(dest, name, parent, scalars, textures):
    full = dest + "/" + name
    if EAL.does_asset_exist(full): EAL.delete_asset(full)
    m = AT.create_asset(name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(m, unreal.load_asset(parent))
    for k, v in scalars.items(): MEL.set_material_instance_scalar_parameter_value(m, k, v)
    for k, t in textures.items():
        if t: MEL.set_material_instance_texture_parameter_value(m, k, t)
    EAL.save_asset(full)
    return m

def set_slots(mesh, outer, inner):
    mats = mesh.get_editor_property("static_materials")
    out = []
    for s in mats:
        n = str(s.get_editor_property("material_slot_name"))
        s.set_editor_property("material_interface", inner if n.startswith("Inner") else outer)
        out.append(s)
    mesh.set_editor_property("static_materials", out)
    EAL.save_loaded_asset(mesh)
    return [str(s.get_editor_property("material_slot_name")) for s in out]

n_ok = 0
if HERO:
    # the legacy FBX importer: it reads the UCX_ hulls and the tangents the normal map was baked against (Interchange skips the hulls here)
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")
for jf in sorted(glob.glob(os.path.join(SRC, "*", "food.json"))):
    with open(jf, encoding="utf-8") as f: man = json.load(f)
    fid = man["id"]
    if ONLY and fid not in ONLY: continue
    d = os.path.dirname(jf)
    dest = "/Game/%s/SM_%s_%s" % (SET, SET, fid)
    tx = {}
    for k in ("C", "N", "R", "Toast", "Burnt", "Spoil", "Frozen", "InC"):
        f = os.path.join(d, "T_%s_%s_%s.png" % (SET, fid, k))
        if os.path.exists(f): tx[k] = import_tex(f, dest, "N" if k == "N" else ("R" if k == "R" else "C"))
    # ("glass" in food.json - the ice: the translucent master, the opacity mask in the blue of _R; make_food_materials.py)
    glass = bool(man.get("glass")) and EAL.does_asset_exist("/Game/FoodV2/M_FoodV2_Glass")
    if man.get("glass") and not glass: log("%s: M_FoodV2_Glass is missing (FOODMAT_ONLY=glass make_food_materials.py) - opaque for now" % fid)
    outer = mi(dest, "MI_%s_%s" % (SET, fid), "/Game/FoodV2/M_FoodV2_Glass" if glass else "/Game/FoodV2/M_FoodV2", {} if glass else {"Jelly": float(man.get("jelly", 0.0))},
               {"C": tx.get("C"), "N": tx.get("N"), "R": tx.get("R"), "TToast": tx.get("Toast"), "TBurnt": tx.get("Burnt"), "TSpoil": tx.get("Spoil"), "TFrozen": tx.get("Frozen")})
    hr = man.get("h_range_m", [-0.02, 0.02])
    axis = 1.0 if man.get("inner_axis") == "r" else 0.0
    whole = import_mesh(os.path.join(d, "SM_%s_%s.fbx" % (SET, fid)), dest, "SM_%s_%s" % (SET, fid))
    if not whole: continue
    upm = UU_PER_M
    if HERO:
        # (the legacy importer keeps its own unit: the inner strip's numbers are in the mesh's units, measured off its box;
        #  the game sizes the hero set by its box too - AntFood::ScaleV2)
        bb = whole.get_bounding_box(); ext = max(bb.max.x - bb.min.x, bb.max.y - bb.min.y, bb.max.z - bb.min.z)
        upm = ext / max(1e-6, max(man["size_m"])); log("%s: %.1f units per metre in the mesh" % (fid, upm))
    usize = max(man["size_m"][0], man["size_m"][1]) * upm
    # ("inner_align" in food.json - the tomato: its inner strip is a real cross-section, so U runs along X alone and is 0.5 at the
    #  whole food's centre whatever piece the face belongs to: the core lands in the middle of the cut)
    align = bool(man.get("inner_align"))
    def inner_mi(name, dz, dx=0.0):
        sc = {"HMin": (hr[0] - dz) * upm, "HMax": (hr[1] - dz) * upm if axis < 0.5 else hr[1] * upm, "Axis": axis, "USize": usize}
        if align:
            sc.update({"USize": man["size_m"][0] * upm, "UYWeight": 0.0, "UOffset": 0.5 + dx / max(1e-6, man["size_m"][0])})
        return mi(dest, name, "/Game/FoodV2/M_FoodV2_Inner", sc, {"InC": tx.get("InC")})
    inner = inner_mi("MI_%s_%s_Inner" % (SET, fid), 0.0)
    slots = set_slots(whole, outer, inner)
    if HERO:
        # LOD1 / LOD2 (the Human's distance) from their own FBX, the same atlas and slot
        # (no editor subsystems in a commandlet: each LOD is imported as a mesh of its own and copied in by AntArtTools, as
        #  Tools/import_blender_art.py does for the hero props)
        for li, screen in ((1, 0.35), (2, 0.12)):
            lf = os.path.join(d, "SM_%s_%s_LOD%d.fbx" % (SET, fid, li))
            if not os.path.exists(lf): continue
            tmpd = "/Game/%s/Tmp" % SET
            tmp = import_mesh(lf, tmpd, "%s_LOD%d" % (fid, li))
            ok = bool(tmp) and unreal.AntArtTools.set_lod_from_mesh(whole, li, tmp, screen)
            log("%s LOD%d %s" % (fid, li, "ok" if ok else "FAILED"))
            if tmp: EAL.delete_asset(tmpd + "/%s_LOD%d" % (fid, li))
        try: log("%s LODs: %d" % (fid, unreal.AntArtTools.lod_count(whole)))
        except Exception: pass
        EAL.save_loaded_asset(whole)
    b = whole.get_bounding_box()
    log("%s whole %.0f x %.0f x %.0f uu (%.1f cm long), tris %d, slots %s" % (fid, b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z,
        (b.max.x - b.min.x) / 19.0, whole.get_num_triangles(0), ",".join(slots)))
    for k, pc in enumerate(man.get("pieces", [])):
        f = os.path.join(d, pc["name"] + ".fbx")
        pm = import_mesh(f, dest, pc["name"])
        if not pm: continue
        # (a piece's own origin is its centre: its height range moves by its offset from the whole)
        pin = inner_mi("MI_%s_%s_Inner_P%d" % (SET, fid, k), pc["centre_m"][2], pc["centre_m"][0])
        slots = set_slots(pm, outer, pin)
        log("%s piece %d tris %d slots %s" % (fid, k, pm.get_num_triangles(0), ",".join(slots)))
    n_ok += 1
log("DONE %d foods" % n_ok)
