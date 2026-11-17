# Headless Blender check: build a tiny cabinet from code, render a preview, export an FBX.
import bpy, sys, os
out = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)

def box(name, size, loc, color):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    o = bpy.context.object
    o.name = name
    o.scale = size
    bpy.ops.object.transform_apply(scale=True)
    b = o.modifiers.new("Bevel", "BEVEL")
    b.width = 0.012
    b.segments = 3
    m = bpy.data.materials.new(name + "_mat")
    m.diffuse_color = color
    o.data.materials.append(m)
    return o

box("Body", (0.6, 0.58, 0.82), (0, 0, 0.45), (0.85, 0.78, 0.62, 1))
box("Top", (0.64, 0.62, 0.04), (0, 0, 0.88), (0.25, 0.25, 0.27, 1))
box("Door", (0.56, 0.02, 0.74), (0, -0.30, 0.45), (0.92, 0.86, 0.70, 1))
box("Handle", (0.14, 0.02, 0.02), (0, -0.33, 0.72), (0.7, 0.7, 0.72, 1))
box("Plinth", (0.6, 0.5, 0.08), (0, 0.03, 0.04), (0.2, 0.2, 0.2, 1))

bpy.ops.object.light_add(type="SUN", location=(2, -3, 4))
bpy.context.object.data.energy = 3
bpy.context.object.rotation_euler = (0.9, 0.1, 0.5)
bpy.ops.object.camera_add(location=(1.6, -2.0, 1.3), rotation=(1.25, 0, 0.67))
scene = bpy.context.scene
scene.camera = bpy.context.object
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "MATERIAL"
scene.render.resolution_x = 800
scene.render.resolution_y = 600
scene.render.filepath = os.path.join(out, "blender_check.png")
bpy.ops.render.render(write_still=True)
bpy.ops.export_scene.fbx(filepath=os.path.join(out, "blender_check.fbx"), use_selection=False)
print("BLENDER_CHECK_DONE", bpy.app.version_string)