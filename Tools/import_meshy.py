# Imports every Meshy FBX from SourceArt/Meshy/<Name>/... into /Game/Meshy/<Name>/
# (static mesh with Nanite + its PBR textures + a material instance of M_MeshyPBR),
# then writes Saved/meshy_report.json with bounds so the code can scale/orient each part.
import unreal, os, glob, json

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJ, "SourceArt", "Meshy")
DST = "/Game/Meshy"
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()

def import_file(filename, dest, name, options=None):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", filename)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    if options:
        t.set_editor_property("options", options)
    AT.import_asset_tasks([t])
    paths = t.get_editor_property("imported_object_paths")
    return paths[0] if paths else None

report = {}
imported = []   # (name, dest, mesh, textures)
for folder in sorted(glob.glob(os.path.join(SRC, "*"))):
    if not os.path.isdir(folder):
        continue
    name = os.path.basename(folder)
    fbx = glob.glob(os.path.join(folder, "**", "*.fbx"), recursive=True)
    if not fbx:
        continue
    fbx = fbx[0]
    base = fbx[:-4]
    dest = DST + "/" + name

    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", True)
    opts.set_editor_property("import_as_skeletal", False)
    opts.set_editor_property("import_materials", False)
    opts.set_editor_property("import_textures", False)
    opts.set_editor_property("import_animations", False)
    sm = opts.get_editor_property("static_mesh_import_data")
    sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("auto_generate_collision", False)
    try:
        sm.set_editor_property("build_nanite", True)
    except Exception:
        pass
    mesh_path = import_file(fbx, dest, name, opts)
    mesh = unreal.load_asset(dest + "/" + name)
    if not mesh:
        unreal.log_warning("MESHY: failed mesh " + name)
        continue

    textures = {}
    for key, suffix, srgb, comp in [
        ("BaseColor", ".png", True, unreal.TextureCompressionSettings.TC_DEFAULT),
        ("Normal", "_normal.png", False, unreal.TextureCompressionSettings.TC_NORMALMAP),
        ("Metallic", "_metallic.png", False, unreal.TextureCompressionSettings.TC_GRAYSCALE),
        ("Roughness", "_roughness.png", False, unreal.TextureCompressionSettings.TC_GRAYSCALE),
    ]:
        f = base + suffix
        if not os.path.exists(f):
            continue
        tname = "T_" + name[3:] + "_" + key
        import_file(f, dest, tname)
        tx = unreal.load_asset(dest + "/" + tname)
        if tx:
            tx.set_editor_property("srgb", srgb)
            tx.set_editor_property("compression_settings", comp)
            if key == "Normal":
                tx.set_editor_property("flip_green_channel", False)
            EAL.save_asset(dest + "/" + tname)
            textures[key] = tx

    imported.append((name, dest, mesh, textures))
    unreal.log("MESHY: imported " + name)


# ---------------------------------------------------------------- master material
# Texture parameters need valid defaults or the material fails to compile: use the ant body's textures.
def tex_default(key):
    return unreal.load_asset(DST + "/SM_Ant_Body/T_Ant_Body_" + key)

for name, dest, mesh, textures in imported:
    p = dest + "/MI_" + name[3:]
    if EAL.does_asset_exist(p):
        EAL.delete_asset(p)
MASTER = DST + "/M_MeshyPBR"
if EAL.does_asset_exist(MASTER):
    EAL.delete_asset(MASTER)
m = AT.create_asset("M_MeshyPBR", DST, unreal.Material, unreal.MaterialFactoryNew())
def tex(name, y, sampler):
    t = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, y)
    t.set_editor_property("parameter_name", name)
    t.set_editor_property("sampler_type", sampler)
    d = tex_default(name)
    if d:
        t.set_editor_property("texture", d)
    return t
bc = tex("BaseColor", -300, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
nm = tex("Normal", 0, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
mt = tex("Metallic", 300, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
rg = tex("Roughness", 600, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
tint = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -700, -500)
tint.set_editor_property("parameter_name", "Tint")
tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
mul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -350, -350)
MEL.connect_material_expressions(bc, "RGB", mul, "A")
MEL.connect_material_expressions(tint, "", mul, "B")
MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(nm, "RGB", unreal.MaterialProperty.MP_NORMAL)
MEL.connect_material_property(mt, "R", unreal.MaterialProperty.MP_METALLIC)
MEL.connect_material_property(rg, "R", unreal.MaterialProperty.MP_ROUGHNESS)
glow = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, 900)
glow.set_editor_property("parameter_name", "Glow")
gmul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -350, 850)
MEL.connect_material_expressions(mul, "", gmul, "A")
MEL.connect_material_expressions(glow, "", gmul, "B")
MEL.connect_material_property(gmul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.recompile_material(m)
EAL.save_asset(MASTER)
master = m

for name, dest, mesh, textures in imported:
    mi_path = dest + "/MI_" + name[3:]
    if EAL.does_asset_exist(mi_path):
        EAL.delete_asset(mi_path)
    mi = AT.create_asset("MI_" + name[3:], dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, master)
    for key, tx in textures.items():
        MEL.set_material_instance_texture_parameter_value(mi, key, tx)
    EAL.save_asset(mi_path)

    mesh.set_material(0, mi)
    EAL.save_asset(dest + "/" + name)

    box = mesh.get_bounding_box()
    mn, mx = box.min, box.max
    report[name] = {
        "mesh": dest + "/" + name + "." + name,
        "material": mi_path + ".MI_" + name[3:],
        "min": [mn.x, mn.y, mn.z],
        "max": [mx.x, mx.y, mx.z],
        "tris": mesh.get_num_triangles(0) if hasattr(mesh, "get_num_triangles") else -1,
        "slots": len(mesh.get_editor_property("static_materials")),
    }

with open(os.path.join(PROJ, "Saved", "meshy_report.json"), "w") as f:
    json.dump(report, f, indent=1)
unreal.log("MESHY_DONE %d" % len(report))
