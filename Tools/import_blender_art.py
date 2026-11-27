# Imports the Blender-made level art into the project as static meshes:
#   SourceArt/Blender/kitchen/*.fbx -> /Game/Kitchen/<Name>/<Name>
#   SourceArt/Blender/living/*.fbx  -> /Game/Living/<Name>/<Name>
#   SourceArt/Blender/human/*.fbx   -> /Game/Human/<Name>/<Name>   (run through Tools/import_human.py)
#   SourceArt/Blender/level_v2/*.fbx -> /Game/Kitchen or /Game/Living by the name's prefix (set "level_v2")
# One mesh per FBX (combined), material SLOT NAMES are kept (the game maps slot name -> look in code),
# no materials / textures are imported, no Nanite, no lightmap UVs.
# Collision: furniture and everything ants crawl on or into uses its own triangles
# ("use complex collision as simple"); small loose props also get a simple box so they can be
# simulated or carried. Writes Saved/blender_art_report.json and compares bounds / slots with the
# manifest next to the FBX files. Lines you need to read are logged as warnings ("ART: ...").
#
# Run (see Saved/art/import_art.ps1):
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=%TEMP%\import_blender_art.py -unattended -nosplash -nullrhi
# Optional environment variable ANT_ART_SETS=kitchen,living limits what is imported.
import unreal, os, glob, json

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SETS = [
    ("kitchen", os.path.join(PROJ, "SourceArt", "Blender", "kitchen"), "/Game/Kitchen", "kitchen_manifest.json"),
    ("living", os.path.join(PROJ, "SourceArt", "Blender", "living"), "/Game/Living", "living_manifest.json"),
    ("human", os.path.join(PROJ, "SourceArt", "Blender", "human"), "/Game/Human", "human_manifest.json"),   # the Human's segments (Tools/import_human.py)
    # level v2: the folder is picked by the name, the way KitchenArt::Mesh looks a mesh up (SM_K_* -> /Game/Kitchen, the rest -> /Game/Living)
    ("level_v2", os.path.join(PROJ, "SourceArt", "Blender", "level_v2"), None, "level_v2_new_pieces.json"),
    # level v3: every SM_V3_* piece in its own folder (KitchenArt::Mesh looks SM_V3_* up under /Game/LevelV3)
    ("level_v3", os.path.join(PROJ, "SourceArt", "Blender", "level_v3"), "/Game/LevelV3", "level_v3_new_pieces.json"),
    ("level_v31", os.path.join(PROJ, "SourceArt", "Blender", "level_v31"), "/Game/LevelV3", "level_v31_new_pieces.json"),
    ("level_v32", os.path.join(PROJ, "SourceArt", "Blender", "level_v32"), "/Game/LevelV3", "level_v32_new_pieces.json"),   # the sink zone pilot
]
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()

# Loose props: a simple box as well (the game may simulate or carry them). Everything else: triangles only.
SIMPLE_BOX = (
    "SM_K_Toast", "SM_K_SpiceJar", "SM_K_Shaker", "SM_K_Sponge", "SM_K_Knife", "SM_K_Fork", "SM_K_IceTray",
    "SM_K_EggTray", "SM_K_MilkCarton", "SM_K_CheeseWedge", "SM_K_ButterTub", "SM_K_Bone", "SM_K_Detergent",
    "SM_K_Pot_Lid", "SM_K_TennisBall",)

def log(s):
    unreal.log_warning("ART: " + s)

def import_fbx(filename, dest, name):
    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", True)
    opts.set_editor_property("import_as_skeletal", False)
    opts.set_editor_property("import_materials", False)
    opts.set_editor_property("import_textures", False)
    opts.set_editor_property("import_animations", False)
    opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = opts.get_editor_property("static_mesh_import_data")
    sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("auto_generate_collision", False)
    sm.set_editor_property("generate_lightmap_u_vs", False)
    sm.set_editor_property("remove_degenerates", False)
    sm.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    for k, v in (("build_nanite", False), ("build_reversed_index_buffer", False)):
        try:
            sm.set_editor_property(k, v)
        except Exception:
            pass
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", filename)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    t.set_editor_property("options", opts)
    AT.import_asset_tasks([t])
    return unreal.load_asset(dest + "/" + name)

def setup_collision(mesh, name):
    simple = name in SIMPLE_BOX
    bs = mesh.get_editor_property("body_setup")
    if bs:
        # (the editor subsystems do not exist in a commandlet: the aggregate geometry is written directly)
        agg = bs.get_editor_property("agg_geom")
        boxes = []
        if simple:
            b = mesh.get_bounding_box()
            e = unreal.KBoxElem()
            e.set_editor_property("center", unreal.Vector((b.min.x + b.max.x) * 0.5, (b.min.y + b.max.y) * 0.5, (b.min.z + b.max.z) * 0.5))
            e.set_editor_property("x", max(0.1, b.max.x - b.min.x))
            e.set_editor_property("y", max(0.1, b.max.y - b.min.y))
            e.set_editor_property("z", max(0.1, b.max.z - b.min.z))
            boxes = [e]
        agg.set_editor_property("box_elems", boxes)
        agg.set_editor_property("sphere_elems", [])
        agg.set_editor_property("sphyl_elems", [])
        agg.set_editor_property("convex_elems", [])
        bs.set_editor_property("agg_geom", agg)
        flag = unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX if simple else unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
        bs.set_editor_property("collision_trace_flag", flag)
    return "box" if simple else "complex"

# ------------------------------------------------------------------------------------------------ hero props (KAN-22)
# SourceArt/Blender/hero/<Name>/ (Tools/blender/hero_lib.py; hero_manifest.json): <Name>.fbx = LOD0 + UCX_ hulls, _LOD1/_LOD2,
# movers (<Name>_Handle...), pre-cut break pieces <Name>_Brk<i>, tex/T_<Name>_{C,N,R}.png. Into the folder KitchenArt::Mesh reads
# (SM_K_ -> /Game/Kitchen/<Name>, SM_V3* -> /Game/LevelV3/<Name>, else /Game/Living/<Name>), with MI_<Name> (parent
# /Game/Kitchen/Hero/M_Hero: Tools/art/make_hero_material.py) on its Hero_<Name> slot. Collision: the UCX hulls stay in the
# body (AntWreck makes its physics boxes of them) and the triangles of LOD <collision_lod> are "complex as simple" (the ants
# walk on exactly what is drawn). Break pieces are CPU-readable (AntWreckGeo reads them in the packaged game).
HERO_SRC = os.path.join(PROJ, "SourceArt", "Blender", "hero")
HERO_MAT = "/Game/Kitchen/Hero"
ESML = unreal.EditorStaticMeshLibrary

def hero_dir(name):
    return "/Game/LevelV3/" + name if name.startswith("SM_V3") else "/Game/Kitchen/" + name if name.startswith("SM_K_") else "/Game/Living/" + name

def hero_tex(f, dest, name, kind):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f); t.set_editor_property("destination_path", dest); t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True); t.set_editor_property("replace_existing", True); t.set_editor_property("save", False)
    AT.import_asset_tasks([t])
    a = unreal.load_asset(dest + "/" + name)
    if not a:
        log("FAILED texture " + f); return None
    if kind == "N":
        a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        a.set_editor_property("srgb", False); a.set_editor_property("flip_green_channel", True)   # Blender bakes OpenGL normals
        a.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    elif kind == "R":
        a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        a.set_editor_property("srgb", False); a.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    else:
        a.set_editor_property("srgb", True); a.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    EAL.save_asset(dest + "/" + name)
    return a

def hero_mesh(fbx, dest, name, ucx=False):
    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", True); opts.set_editor_property("import_as_skeletal", False)
    opts.set_editor_property("import_materials", False); opts.set_editor_property("import_textures", False)
    opts.set_editor_property("import_animations", False)
    opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = opts.get_editor_property("static_mesh_import_data")
    sm.set_editor_property("combine_meshes", True); sm.set_editor_property("auto_generate_collision", False)
    sm.set_editor_property("generate_lightmap_u_vs", False); sm.set_editor_property("remove_degenerates", False)
    sm.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    sm.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    try: sm.set_editor_property("one_convex_hull_per_ucx", True)
    except Exception: pass
    for k, v in (("build_nanite", False), ("build_reversed_index_buffer", False)):
        try: sm.set_editor_property(k, v)
        except Exception: pass
    if EAL.does_asset_exist(dest + "/" + name):
        EAL.delete_asset(dest + "/" + name)   # (a reimport over the old model keeps its old slots: the replacement is approved, KAN-22)
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", fbx); t.set_editor_property("destination_path", dest); t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True); t.set_editor_property("replace_existing", True); t.set_editor_property("save", True)
    t.set_editor_property("options", opts)
    AT.import_asset_tasks([t])
    return unreal.load_asset(dest + "/" + name + "." + name)

def hero_slots(mesh, mats, by_name=None):
    """slot i gets mats[i] (keeping the slot names: Hero_<Name>, Break); by_name {prefix: material} wins for slots named so
    (hero props: Hero_Glass -> the glass instance, Break -> the fracture of the piece's class)"""
    # (set_material, not a copied static_materials list: the copy came back as WorldGridMaterial in the cook)
    sl = mesh.get_editor_property("static_materials")
    for i in range(len(sl)):
        sn = str(sl[i].get_editor_property("material_slot_name"))
        pick = None
        for pre, m in (by_name or {}).items():
            if sn.startswith(pre) and m: pick = m; break
        if pick is None and i < len(mats): pick = mats[i]
        if pick:
            mesh.set_material(i, pick)
    got = [mesh.get_material(i) for i in range(len(mesh.get_editor_property("static_materials")))]
    log("hero slots %s: %s" % (mesh.get_name(), [g.get_name() if g else None for g in got]))

def hero_collision(mesh, complex_as_simple=True, keep_convex=True):
    bs = mesh.get_editor_property("body_setup")
    if not bs:
        return 0
    agg = bs.get_editor_property("agg_geom")
    n = len(agg.get_editor_property("convex_elems"))
    if not keep_convex:
        agg.set_editor_property("convex_elems", [])
    agg.set_editor_property("box_elems", []); agg.set_editor_property("sphere_elems", []); agg.set_editor_property("sphyl_elems", [])
    bs.set_editor_property("agg_geom", agg)
    bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE if complex_as_simple else unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    return n

def hero_dir_new(name):
    """the hero rebuilds that keep the old model: /Game/Hero/<Name> (KitchenArt::Mesh looks there first; -AntHeroArt=0 skips it)"""
    return "/Game/Hero/" + name

def import_hero(only_names=None, HERO_SRC=HERO_SRC, manifest="hero_manifest.json", hero_dir=hero_dir):
    mpath = os.path.join(HERO_SRC, manifest)
    if not os.path.exists(mpath):
        log("hero: no manifest"); return
    doc = json.load(open(mpath, encoding="utf-8"))
    # the legacy FBX importer: it reads the UCX_ hulls and keeps every material slot of the file (Interchange skips both here)
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")
    brk = {"plastic": unreal.load_asset(HERO_MAT + "/MI_HeroBreak_Plastic"), "wood": unreal.load_asset(HERO_MAT + "/MI_HeroBreak_Wood")}
    parent = unreal.load_asset(HERO_MAT + "/M_Hero")
    if not parent:
        log("hero: /Game/Kitchen/Hero/M_Hero missing - run Tools/art/make_hero_material.py first"); return
    for p in doc["pieces"]:
        name = p["name"]
        if only_names and name not in only_names:
            continue
        src = os.path.join(HERO_SRC, name); dest = hero_dir(name)
        texs = {k: hero_tex(os.path.join(HERO_SRC, v), dest, "T_%s_%s" % (name, k), k) for k, v in p["textures"].items()}
        mi_name = "MI_" + name
        if not EAL.does_asset_exist(dest + "/" + mi_name):
            AT.create_asset(mi_name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi = unreal.load_asset(dest + "/" + mi_name)
        MEL = unreal.MaterialEditingLibrary
        MEL.set_material_instance_parent(mi, parent)
        for k, t in texs.items():
            if t: MEL.set_material_instance_texture_parameter_value(mi, k, t)
        EAL.save_asset(dest + "/" + mi_name)
        # the owner's palette (hero_kitchen_block.py pass 5): the same model baked in his own map v4 colours -> MI_<Name>_Owner,
        # which the mesh and its break pieces wear by default (rule 2); KitchenArt swaps MI_<Name> in with -AntHeroPalette=new
        if p.get("textures_owner"):
            otex = {k: hero_tex(os.path.join(HERO_SRC, v), dest, "T_%s_%s_Owner" % (name, k), k) for k, v in p["textures_owner"].items()}
            if not EAL.does_asset_exist(dest + "/" + mi_name + "_Owner"):
                AT.create_asset(mi_name + "_Owner", dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            mio = unreal.load_asset(dest + "/" + mi_name + "_Owner")
            MEL.set_material_instance_parent(mio, parent)
            for k, t in otex.items():
                if t: MEL.set_material_instance_texture_parameter_value(mio, k, t)
            EAL.save_asset(dest + "/" + mi_name + "_Owner")
            mi = mio
        # glass (hero props): slot Hero_Glass -> MI_<Name>_Glass, parent M_HeroGlass (translucent; Tools/art/make_hero_glass.py)
        gmi = None
        gpar = unreal.load_asset(HERO_MAT + "/M_HeroGlass") if p.get("glass") else None
        if gpar:
            gname = "MI_%s_Glass" % name
            if not EAL.does_asset_exist(dest + "/" + gname):
                AT.create_asset(gname, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            gmi = unreal.load_asset(dest + "/" + gname)
            MEL.set_material_instance_parent(gmi, gpar)
            for k, t in texs.items():
                if t: MEL.set_material_instance_texture_parameter_value(gmi, k, t)
            MEL.set_material_instance_scalar_parameter_value(gmi, "Opacity", float(p["glass"].get("opacity", 0.16)))
            EAL.save_asset(dest + "/" + gname)
        elif p.get("glass"):
            log("hero %s: M_HeroGlass missing - run Tools/art/make_hero_glass.py" % name)
        mesh = hero_mesh(os.path.join(src, p["file"]), dest, name, ucx=True)
        if not mesh:
            log("FAILED hero " + name); continue
        # LOD1 / LOD2: their own Blender-decimated meshes, copied in by AntArtTools (no editor subsystems in a commandlet)
        for i, lod_file, screen in ((1, name + "_LOD1.fbx", 0.35), (2, name + "_LOD2.fbx", 0.12)):
            f = os.path.join(src, lod_file)
            if os.path.exists(f):
                tmp = hero_mesh(f, HERO_MAT + "/Tmp", "%s_LOD%d" % (name, i))
                ok = tmp and unreal.AntArtTools.set_lod_from_mesh(mesh, i, tmp, screen)
                if not ok: log("hero %s: LOD%d FAILED" % (name, i))
                if tmp: EAL.delete_asset(HERO_MAT + "/Tmp/%s_LOD%d" % (name, i))
        hero_slots(mesh, [mi], {"Hero_Glass": gmi})
        hull = hero_collision(mesh, True, True)
        mesh.set_editor_property("lod_for_collision", int(p.get("collision_lod", 0)))
        EAL.save_loaded_asset(mesh)
        b = mesh.get_bounding_box()
        lods = unreal.AntArtTools.lod_count(mesh)
        hull = unreal.AntArtTools.convex_count(mesh)
        tris = [mesh.get_num_triangles(i) for i in range(lods)]
        cls = p.get("break_class") or ("plastic" if any(k in name for k in ("Bucket", "Pail", "Sprayer", "DishSoap")) else "wood")
        if cls not in brk:   # (glass, ceramic, paper, metal...: MI_HeroBreak_<Class> if Tools/art/make_hero_material.py made it)
            brk[cls] = unreal.load_asset(HERO_MAT + "/MI_HeroBreak_" + cls.capitalize()) or brk["plastic"]
        for mv in p.get("movers", []):
            m2 = hero_mesh(os.path.join(src, mv["file"]), dest, mv["name"])
            if m2:
                hero_slots(m2, [mi], {"Hero_Glass": gmi}); hero_collision(m2, True, False); EAL.save_loaded_asset(m2)
                log("hero %s mover %s tris %d" % (name, mv["name"], m2.get_num_triangles(0)))
        for bp in p.get("breaks", []):
            m3 = hero_mesh(os.path.join(src, bp["file"]), dest, bp["name"])
            if m3:
                hero_slots(m3, [mi, brk.get(cls)], {"Hero_Glass": gmi, "Break": brk.get(cls), "Hero_": mi})
                hero_collision(m3, False, False)
                m3.set_editor_property("allow_cpu_access", True)
                EAL.save_loaded_asset(m3)
                log("hero %s break %s tris %d slots %s" % (name, bp["name"], m3.get_num_triangles(0), [str(s.get_editor_property("material_slot_name")) for s in m3.get_editor_property("static_materials")]))
        log("hero %s -> %s min(%.1f,%.1f,%.1f) max(%.1f,%.1f,%.1f) LOD tris %s, %d hulls, collision LOD %d, slots %s" % (
            name, dest, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z, tris, hull, p.get("collision_lod", 0),
            [str(s.get_editor_property("material_slot_name")) for s in mesh.get_editor_property("static_materials")]))
        report[name] = {"mesh": dest + "/" + name, "lod_tris": tris, "hulls": hull, "set": "hero"}

only = [s.strip().lower() for s in os.environ.get("ANT_ART_SETS", "").split(",") if s.strip()]
report = {}
if "hero" in only:
    import_hero([n for n in os.environ.get("ANT_HERO_NAMES", "").replace("+", ",").split(",") if n and n != "all"] or None)   # (names joined by + : cmd splits arguments at commas)
if "hero_props" in only:
    # the small props of the flat (hero_props.py): SourceArt/Blender/hero_props -> /Game/Hero/<Name>, the old model stays
    import_hero([n for n in os.environ.get("ANT_HERO_NAMES", "").replace("+", ",").split(",") if n and n != "all"] or None,
                os.path.join(PROJ, "SourceArt", "Blender", "hero_props"), "hero_props_manifest.json", hero_dir_new)
if "hero_living" in only:
    # the living room and the furniture of the flat (hero_living.py): SourceArt/Blender/hero_living -> /Game/Hero/<Name>
    import_hero([n for n in os.environ.get("ANT_HERO_NAMES", "").replace("+", ",").split(",") if n and n != "all"] or None,
                os.path.join(PROJ, "SourceArt", "Blender", "hero_living"), "hero_living_manifest.json", hero_dir_new)
if "hero_kitchen" in only:
    # the kitchen block, the mop, the bucket and the sink set (hero_kitchen_a.py, hero_kitchen_sink.py, hero_kitchen_block.py):
    # SourceArt/Blender/hero_kitchen -> /Game/Hero/<Name>, the old model stays (-AntHeroArt=0)
    import_hero([n for n in os.environ.get("ANT_HERO_NAMES", "").replace("+", ",").split(",") if n and n != "all"] or None,
                os.path.join(PROJ, "SourceArt", "Blender", "hero_kitchen"), "hero_kitchen_manifest.json", hero_dir_new)
for flat_set in ("hero_bedroom", "hero_bath", "hero_hall"):
    if flat_set in only:
        # the rooms of the new flat (hero_flat.py, KAN-41): SourceArt/Blender/hero_<room> -> /Game/Hero/<Name>; what opens
        # (doors, drawers, lids) is a piece of its own, its place on the parent is in the manifest ("attach")
        import_hero([n for n in os.environ.get("ANT_HERO_NAMES", "").replace("+", ",").split(",") if n and n != "all"] or None,
                    os.path.join(PROJ, "SourceArt", "Blender", flat_set), flat_set + "_manifest.json", hero_dir_new)
for set_name, src, dst, manifest_name in SETS:
    if only and set_name not in only:
        continue
    files = sorted(glob.glob(os.path.join(src, "*.fbx")))
    if not files:
        log("%s: no FBX in %s, skipped" % (set_name, src))
        continue
    manifest = {}
    mpath = os.path.join(src, manifest_name)
    if os.path.exists(mpath):
        with open(mpath, encoding="utf-8") as f:
            for p in json.load(f).get("pieces", []):
                manifest[p["name"]] = p
    ok = 0
    bad = []
    for fbx in files:
        name = os.path.splitext(os.path.basename(fbx))[0]
        dest = (dst if dst else ("/Game/Kitchen" if name.startswith("SM_K_") else "/Game/Living")) + "/" + name
        mesh = import_fbx(fbx, dest, name)
        if not mesh or not isinstance(mesh, unreal.StaticMesh):
            log("FAILED " + name)
            bad.append(name)
            continue
        try:
            ns = mesh.get_editor_property("nanite_settings")
            if ns.get_editor_property("enabled"):
                ns.set_editor_property("enabled", False)
                mesh.set_editor_property("nanite_settings", ns)
        except Exception:
            pass
        coll = setup_collision(mesh, name)
        EAL.save_loaded_asset(mesh)
        box = mesh.get_bounding_box()
        mn, mx = box.min, box.max
        slots = [str(m.get_editor_property("material_slot_name")) for m in mesh.get_editor_property("static_materials")]
        tris = mesh.get_num_triangles(0)
        note = ""
        p = manifest.get(name)
        if p:
            a, b = p["bbox_min_m"], p["bbox_max_m"]
            emn = [100.0 * a[0], -100.0 * b[1], 100.0 * a[2]]
            emx = [100.0 * b[0], -100.0 * a[1], 100.0 * b[2]]
            d = max(abs(mn.x - emn[0]), abs(mn.y - emn[1]), abs(mn.z - emn[2]), abs(mx.x - emx[0]), abs(mx.y - emx[1]), abs(mx.z - emx[2]))
            if d > 0.06:
                note += " BOUNDS_DIFF=%.2f expected(%.1f,%.1f,%.1f)..(%.1f,%.1f,%.1f)" % (d, emn[0], emn[1], emn[2], emx[0], emx[1], emx[2])
            if sorted(slots) != sorted(p["materials"]):
                note += " SLOTS_DIFF manifest=" + ",".join(p["materials"])
            if tris != p.get("triangles", tris):
                note += " TRIS manifest=%d" % p["triangles"]
        elif manifest:
            note += " NOT_IN_MANIFEST"
        log("%s min(%.1f,%.1f,%.1f) max(%.1f,%.1f,%.1f) tris %d coll %s slots %s%s" % (
            name, mn.x, mn.y, mn.z, mx.x, mx.y, mx.z, tris, coll, ",".join(slots), note))
        report[name] = {"mesh": dest + "/" + name + "." + name, "min": [mn.x, mn.y, mn.z], "max": [mx.x, mx.y, mx.z],
                        "tris": tris, "slots": slots, "collision": coll, "set": set_name, "note": note.strip()}
        ok += 1
    missing = [n for n in manifest if n not in report]
    log("SET %s: imported %d of %d FBX, failed %d, manifest pieces without a mesh: %d %s" % (
        set_name, ok, len(files), len(bad), len(missing), ",".join(missing[:20])))

with open(os.path.join(PROJ, "Saved", os.environ.get("ANT_ART_REPORT", "blender_art_report.json")), "w") as f:
    json.dump(report, f, indent=1)
log("DONE %d meshes" % len(report))