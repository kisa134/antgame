# Prints the shape of the v2 "ramp" pieces level v3.1 replaces: bbox and the top profile along their long axis (uu, 1 m = 1900).
import bpy, sys, os
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE))
F = {'SM_K_Spoon': 'kitchen', 'SM_K_Broom': 'level_v2', 'SM_K_CerealBox': 'kitchen', 'SM_L_Blanket': 'level_v2', 'SM_L_ToyBlock': 'level_v2'}
for n, d in F.items():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=os.path.join(ROOT, 'SourceArt', 'Blender', d, n + '.fbx'))
    vs = []
    for ob in bpy.context.scene.objects:
        if ob.type == 'MESH':
            for v in ob.data.vertices:
                w = ob.matrix_world @ v.co; vs.append((w.x * 1900, w.y * 1900, w.z * 1900))
    xs, ys, zs = zip(*vs)
    print('PROBE', n, 'x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f verts %d' % (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs), len(vs)))
    ax = 0 if max(xs) - min(xs) >= max(ys) - min(ys) else 1
    lo, hi = min(p[ax] for p in vs), max(p[ax] for p in vs)
    prof = []
    for k in range(12):
        a, b = lo + (hi - lo) * k / 12, lo + (hi - lo) * (k + 1) / 12
        zz = [p[2] for p in vs if a <= p[ax] <= b]
        oth = [p[1 - ax] for p in vs if a <= p[ax] <= b]
        prof.append('%.0f:z%.0f-%.0f w%.0f' % ((a + b) / 2, min(zz) if zz else -1, max(zz) if zz else -1, (max(oth) - min(oth)) if oth else 0))
    print('PROFILE', n, 'axis', 'xy'[ax], ' | '.join(prof))
