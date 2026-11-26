# "Before" pictures for the owner (rule 2: no replacement without a before/after): imports the current FBX of a piece,
# gives its slots plain colours by name and renders it in the hero studio from the same two cameras as the new model.
#   blender --background --factory-startup --python hero_before.py -- <fbx> <out.jpg> [cam: wide|ant]
import bpy, sys, os, math
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hero_lib as H
argv = sys.argv[sys.argv.index("--") + 1:]
FBX, OUT = argv[0], argv[1]
H.reset()
bpy.ops.import_scene.fbx(filepath=FBX)
COL = {"Sisal": (0.8, 0.66, 0.42), "Plinth": (0.08, 0.08, 0.09), "Walnut": (0.35, 0.2, 0.1), "Butcher": (0.62, 0.42, 0.22), "Blue": (0.1, 0.25, 0.75),
       "Paper": (0.97, 0.96, 0.88), "PlasticBlue": (0.12, 0.3, 0.75), "Steel": (0.7, 0.7, 0.72), "Red": (0.75, 0.1, 0.08), "Fabric": (0.6, 0.6, 0.6)}
obs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
for o in obs:
    for s in o.material_slots:
        if s.material:
            n = s.material.name.split(".")[0]
            s.material.use_nodes = True
            p = s.material.node_tree.nodes.get("Principled BSDF")
            if p:
                p.inputs["Base Color"].default_value = (*COL.get(n, (0.6, 0.6, 0.6)), 1)
                p.inputs["Roughness"].default_value = 0.6
mn, mx = H.bounds(obs)
H.stage()
H.frame_camera(mn, mx, argv[2] if len(argv) > 2 else "wide")
H.render(OUT, samples=96)
