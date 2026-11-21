# -*- coding: utf-8 -*-
# Ant Kitchen level v3: the design data -> the C++ tables the game reads (behind -AntLevelV3).
#   Source/AntKitchen/LevelV3Art.inl  - the BASE placement table (GKitchenArtV3, GKitchenMoversV3) from
#                                       SourceArt/Blender/level_v3/level_v3_manifest.json, with each row's object id and flags
#   Source/AntKitchen/LevelV3Data.inl - the gameplay tables of v3 (loot, cover, patrol, zones, districts...: as LevelV2Data.inl),
#                                       the district variants (their ops and the meshes they add or move), the interactive objects
#                                       and their fasteners, and THE SEED TABLE: compose(seed, tier) of level_v3_layout for seeds
#                                       0..N-1 of every tier, each one run through the checks of check_level_v3 (level_v3_lib.run_checks).
#                                       Only seeds that pass go into the table; the game never builds a layout that is not in it.
#                                       Scatter items the rules drop are re-drawn here (up to 16 tries each) and the seed re-checked.
#   python Tools/level/gen_level_v3_code.py [--seeds N]      (default 256 per tier; then compile)
# Also writes docs/level/level_v3_seed_table.json (what went into the table, with the check numbers of every seed).
import json, os, sys, math, time, collections, ast
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE)); sys.path.insert(0, HERE)
try: sys.stdout.reconfigure(encoding='utf-8', errors='replace')
except Exception: pass
import level_v3_layout as L
import level_v3_lib as X
NSEEDS = 256
for i, a in enumerate(sys.argv):
    if a == '--seeds': NSEEDS = int(sys.argv[i + 1])
D = json.load(open(os.path.join(ROOT, 'docs', 'level', 'level_v3.json'), encoding='utf-8'))
D2 = json.load(open(os.path.join(ROOT, 'docs', 'level', 'level_v2.json'), encoding='utf-8'))
M = json.load(open(os.path.join(ROOT, 'SourceArt', 'Blender', 'level_v3', 'level_v3_manifest.json'), encoding='utf-8'))
V3 = D['v3']
OBJ = {o['id']: o for o in D['objects']}
OBJ2 = {o['id']: o for o in D2['objects']}
SRC = os.path.join(ROOT, 'Source', 'AntKitchen')

def f(v, p=3):
    s = ('%.*f' % (p, float(v))).rstrip('0').rstrip('.')
    if s in ('-0', ''): s = '0'
    return s + ('f' if '.' in s else '.f')
def f6(v): return f(v, 6)
def q(s): return str(s).replace('\\', '\\\\').replace('"', '\\"')
def axis(v): return '{%s, %s, %s}' % (f6(v[0]), f6(v[1]), f6(v[2]))
def vec(v): return '{%s, %s, %s}' % (f(v[0]), f(v[1]), f(v[2]))
def inst_row(e): return '{ TEXT("%s"), %d, %d, %d, %s, %s, %s, %s, %s }' % (e['name'], e['index'], e['role'], e['parent'], vec(e['loc']), axis(e['ax']), axis(e['ay']), axis(e['az']), vec(e['scale']))

def ants_only(oid, o=None):
    o = o or OBJ.get(oid) or {}
    return o.get('collision') == 'ants_only' or bool(o.get('cloth')) or o.get('cat') == 'fastener'

# ------------------------------------------------------------------ 1. the base table
insts, movers, v1map = [], [], {}
for p in M['pieces']:
    for k, i in enumerate(p['instances']):
        e = {'name': p['name'], 'index': i.get('index', k), 'role': i.get('role', 0), 'parent': -1, 'loc': i['game_loc_uu'], 'ax': i['game_axis_x'],
             'ay': i['game_axis_y'], 'az': i['game_axis_z'], 'scale': i.get('scale') or [1, 1, 1], 'src': i, 'obj': i.get('object') or ''}
        if e['role'] == 1:
            mv = i['mover']
            movers.append({'inst': len(insts), 'type': 0 if mv['type'] == 'hinge' else 1, 'axis': i.get('hinge_axis_game', [0, 0, 1]), 'angle': float(i.get('open_angle_game_deg', 0.0)),
                           'slide': i.get('slide_game_uu', [0, 0, 0]), 'flags': mv['flags'], 'auto': float(mv['auto_close_s']), 'open': mv['open_text'], 'close': mv['close_text']})
            if 'v1_mover' in mv: v1map[mv['v1_mover']] = len(movers) - 1
        insts.append(e)
for e in insts:
    if e['role'] == 3: e['parent'] = v1map[e['src']['rides_v1_mover']]
# v3.1 (owner's decision): the five v2 ramps that read as a prototype get believable meshes built to the same boxes
# (Tools/blender/level_v31_pieces.py -> SourceArt/Blender/level_v31). The rows stay; only the mesh name changes.
V31 = {'SM_K_Spoon': 'SM_V31_PizzaPeel', 'SM_K_Broom': 'SM_V31_Broom', 'SM_K_CerealBox': 'SM_V31_DeliveryBox', 'SM_L_Blanket': 'SM_V31_Throw', 'SM_L_ToyBlock': 'SM_V31_BoardBooks'}
for e in insts:
    e['v2name'] = e['name']
by_mesh = {(e['name'], e['index']): n for n, e in enumerate(insts)}
# object id of every row: the manifest names v1/v3 rows; v2 rows are found through level_v3.json (mesh + index)
# (by mesh and place: the manifest's own index is the order within the piece, not the v2 index)
for oid, o in OBJ.items():
    if any(e['obj'] == oid for e in insts) or 'origin' not in o: continue
    best, bd = None, 30.0
    for n, e in enumerate(insts):
        if e['name'] != o.get('mesh') or e['obj']: continue
        d = math.dist(e['loc'], o['origin'])
        if d < bd: best, bd = n, d
    if best is not None: insts[best]['obj'] = oid
def base_of(oid):
    for n, e in enumerate(insts):
        if e['obj'] == oid: return n
    return None
FL_ANTS = 1
def flags_of(e): return (FL_ANTS if (e['obj'] and ants_only(e['obj'])) else 0)
fl = [flags_of(e) for e in insts]

for e in insts:
    e['name'] = V31.get(e['name'], e['name'])
A = ['// Level v3 base placement. Generated by Tools/level/gen_level_v3_code.py from SourceArt/Blender/level_v3/level_v3_manifest.json. Do not edit by hand.',
     '// Included by KitchenArt.cpp only (used with -AntLevelV3). %d instances, %d generic movers.' % (len(insts), len(movers)), '',
     'static const FKitchenArtInst GKitchenArtV3[] =', '{']
A += ['\t%s,   // %d %s' % (inst_row(e), n, e['obj']) for n, e in enumerate(insts)]
A += ['};', '', '// The design object id of every row (level_v3.json objects[].id), "" when it has none.', 'static const TCHAR* const GKitchenArtV3Ids[] =', '{']
A += ['\tTEXT("%s"),' % e['obj'] for e in insts]
A += ['};', '', '// Flags of every row: 1 = ants only (the Human, the grandma and the kid walk through it).', 'static const uint8 GKitchenArtV3Flags[] =', '{']
A += ['\t' + ', '.join(str(x) for x in fl[i:i + 40]) + ',' for i in range(0, len(fl), 40)]
A += ['};', '', '// Instance tint of every row (a look name for the first slot: the balloons), "" for none.', 'static const TCHAR* const GKitchenArtV3Tint[] =', '{']
A += ['\tTEXT("%s"),' % (e['src'].get('tint') or '') for e in insts]
A += ['};', '', 'static const FKitchenArtMover GKitchenMoversV3[] =', '{']
A += ['\t{ %d, %d, %s, %s, %s, %d, %s, TEXT("%s"), TEXT("%s") },   // %d %s#%d' % (m['inst'], m['type'], vec(m['axis']), f(m['angle']), vec(m['slide']), m['flags'], f(m['auto']), m['open'], m['close'], n,
          insts[m['inst']]['name'], insts[m['inst']]['index']) for n, m in enumerate(movers)]
A += ['};', '']
with open(os.path.join(SRC, 'LevelV3Art.inl'), 'w', encoding='utf-8-sig', newline='') as fo: fo.write('\n'.join(A) + '\n')

# ------------------------------------------------------------------ 2. gameplay tables (as gen_level_v2_code.py, v3 data)
G = ['// Level v3 gameplay tables. Generated by Tools/level/gen_level_v3_code.py from docs/level/level_v3.json and Tools/level/level_v3_layout.py. Do not edit by hand.',
     '// Included by LevelV3.cpp only.', '']
spots = D['loot']['spots']
for s in spots:
    if s['i'] in L.LOOT_FIX: s['p'] = list(L.LOOT_FIX[s['i']][0])
G += ['// Food spots: x, y, the Z of the surface, tier 0..3. %d spots (v2 spots; LOOT_FIX applied).' % len(spots), 'static const FLevelV2Spot GV3Loot[] =', '{']
for s in spots:
    p = s['p']; G.append('\t{ %s, %s, %s, %d },   // %d %s' % (f(p[0], 1), f(p[1], 1), f(p[2], 1), s['tier_loot'], s['i'], (s.get('label') or s.get('group') or '')))
G += ['};', 'static const int32 GV3LootIds[] = { %s };' % ', '.join(str(s['i']) for s in spots), '', 'static const FVector3f GV3Heavy[] =', '{']
for h in D['loot']['heavy']:
    p = h['p']; G.append('\t{ %s, %s, %s },   // H%d %s' % (f(p[0], 1), f(p[1], 1), f(p[2], 1), h['i'], h['label']))
G += ['};', 'static const int32 GV3HeavyIds[] = { %s };' % ', '.join(str(h['i']) for h in D['loot']['heavy']), '',
      '// The cake places M1..M4: x, y, z, base chance in percent (the seed table carries the one picked).', 'static const FLevelV2Main GV3Main[] =', '{']
main = D['loot']['main']
for m in main['spots']:
    p = m['p']; G.append('\t{ %s, %s, %s, %d, TEXT("%s") },   // %s' % (f(p[0], 1), f(p[1], 1), f(p[2], 1), int(round(100 * L.CAKE_SPOTS[m['id']]['w'])), q(L.CAKE_SPOTS[m['id']]['ru']), m['id']))
G += ['};', '']
cov = [c for c in D['human']['sees']['never_targets'] if 'rect' in c and not c.get('src', '').startswith('HumanAIController')]
G += ['// Cover boxes of the base (the seed adds and takes some away: hide / hide_off). %d boxes.' % len(cov), 'static const FLevelV2Box GV3Cover[] =', '{']
for c in cov:
    r, z = c['rect'], c['z']; G.append('\t{ %s, %s, %s, %s, %s, %s, TEXT("%s") },' % (f(r[0], 1), f(r[1], 1), f(z[0], 1), f(r[2], 1), f(r[3], 1), f(z[1], 1), q(c['ru'])))
G += ['};', 'static const TCHAR* const GV3CoverIds[] = { %s };' % ', '.join('TEXT("%s")' % q(c.get('id') or '') for c in cov), '']
G += ['static const FLevelV2Box GV3TargetBounds[] =', '{']
for r in D['human']['targeting_bounds']:
    G.append('\t{ %s, %s, -400.f, %s, %s, 6000.f, TEXT("") },' % (f(r[0], 1), f(r[1], 1), f(r[2], 1), f(r[3], 1)))
pat = D['human']['patrol']
G += ['};', '', '// Patrol stops (v3: stops 2 and 10 moved, PATROL_FIX).', 'static const FVector2f GV3Patrol[] =', '{']
G += ['\t{ %s, %s },' % (f(p[0], 1), f(p[1], 1)) for p in pat['points']]
G += ['};', 'static const float GV3WanderKitchen[4] = { %s };' % ', '.join(f(v, 1) for v in pat['kitchen_box']),
      'static const float GV3WanderLiving[4] = { %s };' % ', '.join(f(v, 1) for v in pat['living_box']), '']
zones = [h for h in D['hazards'] if h['kind'] in ('sticky', 'wet') and h.get('src', '').startswith('level_v2')]
G += ['// Sticky (0) / wet (1) zones of the base.', 'static const FLevelV2Zone GV3Zones[] =', '{']
def zone_row(kind, h):
    z = h.get('z', [0, 300])
    if 'rect' in h: r = h['rect']; box = (r[0], r[1], r[2], r[3]); rad = 0
    else: c = h['circle']; box = (c[0] - c[2], c[1] - c[2], c[0] + c[2], c[1] + c[2]); rad = c[2]
    return '{ %d, %s, %s, %s, %s, %s, %s, %s }' % (kind, f(box[0], 1), f(box[1], 1), f(z[0], 1), f(box[2], 1), f(box[3], 1), f(z[1], 1), f(rad, 1))
for h in zones: G.append('\t%s,   // %s' % (zone_row(0 if h['kind'] == 'sticky' else 1, h), h['ru']))
G += ['};', '', 'static const FLevelV2District GV3Districts[] =', '{']
for d in sorted(D['districts'], key=lambda d: 0 if d['id'] == 'zastenye' else 1):
    for r in d['rects']: G.append('\t{ %s, %s, %s, %s, TEXT("%s") },' % (f(r[0], 1), f(r[1], 1), f(r[2], 1), f(r[3], 1), q(d['ru'])))
G += ['};', '', 'static const FLevelV2Callout GV3Callouts[] =', '{']
for n in D['graph']['nodes']:
    p = n['p']; G.append('\t{ %s, %s, %s, TEXT("%s") },' % (f(p[0], 1), f(p[1], 1), f(p[2] if len(p) > 2 else 0, 1), q(n['ru'])))
G += ['};', '']

# ------------------------------------------------------------------ 3. district variants
DORD = L.DISTRICT_ORDER
DRU = {d['id']: d['ru'] for d in D['districts']}
VI = M['variant_instances']
VART = []          # extra placement rows (adds, restores, the new place of a moved base row)
def vart(e):
    VART.append(e); return len(VART) - 1
def man_entry(d, v, oid=None, piece=None, near=None, op=None):
    best, bd = None, 1e18
    for i in VI[d][v]:
        if oid and i.get('object') == oid: return i
        if piece and i['piece'] == piece and (op is None or i.get('op') == op):
            dd = 0 if near is None else math.hypot(i['game_loc_uu'][0] - near[0], i['game_loc_uu'][1] - near[1])
            if dd < bd: best, bd = i, dd
    return best
def row_from(i, name=None, index=0, obj=''):
    return {'name': name or i['piece'], 'index': index, 'role': 0, 'parent': -1, 'loc': i['game_loc_uu'], 'ax': i['game_axis_x'], 'ay': i['game_axis_y'], 'az': i['game_axis_z'],
            'scale': i.get('scale') or [1, 1, 1], 'obj': obj, 'tint': i.get('tint')}
MOVER_NAMES = {'door_fridge': 'FRIDGE', 'door_oven': 'OVEN', 'door_dishwasher': 'DISHWASHER', 'door_drawer': 'DRAWER', 'breadbox_lid': 30, 'cat_flap': 31}
OPK = {'add': 0, 'remove': 1, 'move': 2, 'mover': 3, 'hide': 4, 'hide_off': 5, 'hazard': 6, 'hazard_off': 7, 'dog_spot': 8, 'loot_z': 9, 'conn_off': 10, 'edge': 11, 'cake': 12, 'gadget': 13}
OPS = []
VARS = []
TIMES = ['day', 'evening', 'night']
def parse_dict(s): return s if isinstance(s, dict) else ast.literal_eval(s)
for di, d in enumerate(DORD):
    for v in V3['variants'][d]:
        first = len(OPS)
        for op in v['ops']:
            k = op[0]; row = {'k': OPK.get(k, 0), 'a': -1, 'b': -1, 'f': [0.0] * 6, 't': ''}
            if k == 'add':
                i = man_entry(d, v['id'], oid=op[1]); assert i, (d, v['id'], op)
                o = next((x for x in v['objects'] if x.get('id') == op[1]), {})
                row['a'] = vart(dict(row_from(i, obj=op[1]), flags=FL_ANTS if ants_only(op[1], o) else 0)); row['t'] = op[1]
            elif k == 'restore':
                o2 = OBJ2[op[1]]; i = man_entry(d, v['id'], piece=o2['mesh'], near=o2['origin']); assert i, (d, v['id'], op)
                row['k'] = OPK['add']; row['a'] = vart(dict(row_from(i, index=o2.get('index', 0), obj=op[1]), flags=0)); row['t'] = op[1]
            elif k == 'remove':
                row['a'] = base_of(op[1]); row['t'] = op[1]; assert row['a'] is not None, op
            elif k == 'move':
                n = base_of(op[1]); assert n is not None, op
                i = man_entry(d, v['id'], oid=op[1]) or man_entry(d, v['id'], piece=insts[n]['v2name'], near=op[2], op='move'); assert i, (d, v['id'], op)
                row['a'] = n; row['b'] = vart(dict(row_from(i, insts[n]['name'], insts[n]['index'], op[1]), flags=fl[n])); row['t'] = op[1]
            elif k == 'mover':
                name, st = op[1], op[2]
                mi = int(name[5:]) if name.startswith('mover') else MOVER_NAMES[name]
                if isinstance(mi, str): row['k'] = OPK['gadget']; row['t'] = mi
                else: row['a'] = mi
                row['b'] = 0 if st == 'closed' else (1 if st == 'open' else 2)
                row['f'][0] = float(st.split(':')[1]) if st.startswith('ajar') else 0.0
            elif k in ('hide', 'hazard'):
                h = parse_dict(op[1]); row['t'] = h.get('id', '')
                if 'rect' in h: r = h['rect']; row['f'] = [r[0], r[1], r[2], r[3], 0.0, 0.0]
                else: c = h['circle']; row['f'] = [c[0] - c[2], c[1] - c[2], c[0] + c[2], c[1] + c[2], c[2], 0.0]
                z = h.get('z', [0, 300]); row['f'][5] = z[1]
                row['b'] = {'wet': 1, 'sticky': 0, 'heat': 2, 'noise': 3}.get(h.get('kind'), -1)
                row['ru'] = h.get('ru', '')
            elif k in ('hide_off', 'hazard_off', 'conn_off'):
                row['t'] = op[1]
            elif k == 'dog_spot':
                row['t'] = op[1]; row['f'][0], row['f'][1] = op[2][0], op[2][1]
            elif k == 'loot_z':
                row['t'] = ','.join(op[1]); row['f'][0] = op[2]
            elif k == 'edge':
                row['t'] = '%s-%s' % (op[1], op[2]); row['ru'] = '%s: %s' % (op[3], op[4] if len(op) > 4 else '')
            elif k == 'cake':
                row['t'] = op[1]; row['f'][0] = op[2]
            OPS.append(row)
        when = 0
        for t in (v.get('when') or TIMES): when |= 1 << TIMES.index(t)
        VARS.append((di, v['id'], v['ru'], v['plays'], first, len(OPS) - first, when))
G += ['// Extra placements of the district variants: what they add, what they bring back from v2, the new place of what they move.',
      'static const FKitchenArtInst GV3VarArt[] =', '{']
G += ['\t%s,   // %d %s' % (inst_row(e), n, e['obj']) for n, e in enumerate(VART)]
G += ['};', '// Flags as GKitchenArtV3Flags (1 ants only); Tint: a look name for the piece\'s first slot, or "".',
      'static const uint8 GV3VarArtFlags[] = { %s };' % ', '.join(str(e.get('flags', 0)) for e in VART),
      'static const TCHAR* const GV3VarArtTint[] = { %s };' % ', '.join('TEXT("%s")' % (e.get('tint') or '') for e in VART),
      'static const TCHAR* const GV3VarArtIds[] = { %s };' % ', '.join('TEXT("%s")' % e['obj'] for e in VART), '']
G += ['// Variant ops: kind (LevelV3::EOp), A, B, six numbers, a text (object / mover / zone id), a Russian note.', 'static const FLevelV3Op GV3Ops[] =', '{']
for n, o in enumerate(OPS):
    G.append('\t{ %d, %d, %d, { %s }, TEXT("%s"), TEXT("%s") },   // %d' % (o['k'], o['a'] if o['a'] is not None else -1, o['b'], ', '.join(f(x, 1) for x in o['f']), q(o['t']), q(o.get('ru', '')), n))
G += ['};', '', '// The variants: district (index into GV3DistrictIds), letter, name, what it plays, first op, op count, times of day (1 day, 2 evening, 4 night).',
      'static const FLevelV3Variant GV3Variants[] =', '{']
for di, vid, ru, plays, first, cnt, when in VARS:
    G.append('\t{ %d, TEXT(\'%s\'), TEXT("%s"), TEXT("%s"), %d, %d, %d },' % (di, vid, q(ru), q(plays), first, cnt, when))
G += ['};', 'static const TCHAR* const GV3DistrictIds[] = { %s };' % ', '.join('TEXT("%s")' % d for d in DORD),
      'static const TCHAR* const GV3DistrictRu[] = { %s };' % ', '.join('TEXT("%s")' % q(DRU[d]) for d in DORD), '']

# ------------------------------------------------------------------ 4. interactive objects and fasteners
KIND = {'screw': 0, 'hinge': 1, 'nail': 2, 'magnet': 3, 'tape': 4, 'ring': 5, 'knot': 6, 'cap': 7, 'needle': 8, 'friction': 9, 'zip': 10, 'code': 11}
MAT = {'glass': 0, 'ceramic': 1, 'terracotta': 2, 'plastic_bottle': 3, 'plastic': 4, 'cardboard': 5, 'paper': 6, 'cloth': 7, 'latex': 8, 'wood': 9, 'metal': 10, 'yarn': 11, 'food': 12, 'tin': 13}
FIX = []; FAST = []
for it in V3['interactive']:
    n = base_of(it['obj'])
    parts = [x['id'] for x in D['objects'] if x.get('part_of') == it['obj'] and x.get('cat') != 'fastener']
    fs = []
    for fid in it['fasteners']:
        if fid in OBJ and OBJ[fid].get('cat') == 'fastener':
            fk = OBJ[fid]['mesh'].replace('SM_V3_', '')
            kind = {'Screw': 'screw', 'Hinge': 'hinge', 'Nail': 'nail', 'TapeStrip': 'tape', 'CurtainRing': 'ring', 'ColaCap': 'cap', 'KnitNeedle': 'needle', 'BalloonString': 'knot'}.get(fk, it.get('kind', 'screw'))
            fs.append((fid, base_of(fid), kind))
        elif fid in OBJ:
            fs.append((fid, base_of(fid), 'magnet' if 'Magnet' in OBJ[fid].get('mesh', '') else 'knot'))
        else:
            fs.append((fid, None, 'friction' if fid == 'friction' else 'code'))
    FIX.append({'id': it['id'], 'obj': it['obj'], 'inst': -1 if n is None else n, 'ru': it['ru'], 'mat': it.get('mat', 'wood'), 'first': len(FAST), 'n': len(fs),
                'district': it.get('district') or '', 'undo': it.get('undo', ''), 'result': it.get('result', ''),
                'parts': [base_of(p) for p in parts if base_of(p) is not None]})
    for fid, fn, kind in fs: FAST.append({'id': fid, 'inst': -1 if fn is None else fn, 'kind': kind, 'fix': len(FIX) - 1})
G += ['// The interactive objects of the design (v3.interactive): id, design object, its base row (-1: none), name, material class (LevelV3::EMat),',
      '// first fastener, fastener count, district, what the ants do, what happens.', 'static const FLevelV3Fixture GV3Fixtures[] =', '{']
for x in FIX:
    G.append('\t{ TEXT("%s"), TEXT("%s"), %d, TEXT("%s"), %d, %d, %d, TEXT("%s"), TEXT("%s"), TEXT("%s") },' % (x['id'], x['obj'], x['inst'], q(x['ru']), MAT.get(x['mat'], 9), x['first'], x['n'], x['district'], q(x['undo']), q(x['result'])))
G += ['};', '', '// Their fasteners: design id, base row (-1: held by friction / by code), kind (LevelV3::EFastener), fixture.', 'static const FLevelV3FastenerDef GV3Fasteners[] =', '{']
for x in FAST: G.append('\t{ TEXT("%s"), %d, %d, %d },' % (q(x['id']), x['inst'], KIND[x['kind']], x['fix']))
G += ['};', '', '// Rows that belong to a fixture (its parts: the shelf\'s brackets, the lid of a jar...): fixture, base row.', 'static const int32 GV3FixtureParts[][2] =', '{']
for k, x in enumerate(FIX):
    for p in x['parts']: G.append('\t{ %d, %d },   // %s' % (k, p, insts[p]['obj']))
G += ['\t{ -1, -1 }', '};', '']

# ------------------------------------------------------------------ 5. the seed table
t0 = time.time()
B = X.load_base(); Gd = X.Grid(B['D2'])
ZONES = L.SEED['scatter']
NAMES = []
def name_id(s):
    if s not in NAMES: NAMES.append(s)
    return NAMES.index(s)
def same(a, b): return a.get('p') == b.get('p') and a['item'] == b['item'] and a['zone'] == b['zone']
def redraw(comp, st, res):
    """Scatter items the rules dropped are drawn again in their zone (their own dice, from the seed and the tier), up to 16 tries each."""
    dropped = res['scatter_dropped']
    if not dropped: return comp, res, 0
    rr = L.Rng(comp['seed'] * 7919 + 17 * comp['threat'] + 3)
    zr = {z['id']: z for z in ZONES}
    keep = [s for s in comp['scatter'] if not any(same(s, d) for d in dropped)]
    fixed = 0
    for dd in dropped:
        z = zr[dd['zone']]
        for _ in range(16):
            x = z['rect'][0] + rr.next() * (z['rect'][2] - z['rect'][0]); y = z['rect'][1] + rr.next() * (z['rect'][3] - z['rect'][1])
            cand = {'zone': dd['zone'], 'item': dd['item'], 'p': [round(x), round(y), z.get('z', 0)], 'yaw': round(rr.next() * 360 - 180)}
            k2, d2 = X.scatter_check(B, dict(st, comp=dict(comp, scatter=keep + [cand])), Gd, None)
            if not d2: keep.append(cand); fixed += 1; break
    comp = dict(comp, scatter=keep)
    res2 = X.run_checks(B, X.apply(B, comp), Gd)
    return comp, res2, fixed
SEEDS = []; SC = []; stats = collections.Counter(); fails = collections.Counter()
CAKE = {'M1': 0, 'M2': 1, 'M3': 2, 'M4': 3}
CACHE = os.path.join(ROOT, 'docs', 'level', 'level_v3_seed_table.json')
REUSE = '--reuse' in sys.argv   # the validated table of the last full run (art-only changes: the seeds and their checks do not change)
if REUSE:
    C = json.load(open(CACHE, encoding='utf-8'))
    assert C.get('n_per_tier') == NSEEDS and 'scatter' in C, 'no reusable seed table: run without --reuse'
    SEEDS, SC, NAMES[:], stats = C['seeds'], [tuple(x) for x in C['scatter']], C['scatter_names'], collections.Counter(C['stats'])
    for x in SEEDS: x['loose'] = [base_of(i) for i in x['loose_ids']]
for tier in ((1, 2, 3, 4) if not REUSE else ()):
    for s in range(NSEEDS):
        comp = L.compose(s, tier)
        st = X.apply(B, comp); res = X.run_checks(B, st, Gd)
        if not res['ok']:
            stats['failed'] += 1
            for x in res['fails']: fails[x.split(' at ')[0][:70]] += 1
            continue
        comp, res, fixed = redraw(comp, st, res)
        stats['redrawn'] += fixed; stats['still_dropped'] += len(res['scatter_dropped'])
        if not res['ok']:
            stats['failed_after_redraw'] += 1; continue
        dropped = res['scatter_dropped']
        first = len(SC)
        for sc in comp['scatter']:
            if any(same(sc, d) for d in dropped): continue
            SC.append((name_id(sc['zone']), name_id(sc['item']), sc.get('p'), sc.get('yaw', 0)))
        home = sum(1 << k for k, w in enumerate(('grandma', 'kid', 'dog', 'cat')) if comp['home'][w])
        doors = sorted(int(m[5:]) for m in comp['doors'])
        loose = [base_of(x) for x in comp['loose']]
        assert all(x is not None for x in loose), comp['loose']
        SEEDS.append({'seed': s, 'tier': tier, 'time': TIMES.index(comp['time']), 'home': home, 'var': ''.join(comp['variants'][d] for d in DORD), 'cake': CAKE[comp['cake']],
                      'doors': doors, 'loose': loose, 'loose_ids': comp['loose'], 'sc': (first, len(SC) - first),
                      'cover': max(res['cover']['kitchen']['worst_s'], res['cover']['living']['worst_s']), 'cake_s': res['cake']['times_s'][:2]})
        stats['ok'] += 1
G += ['// THE SEED TABLE: compose(seed, tier) of Tools/level/level_v3_layout.py for seeds 0..%d of every tier, each run through the seed checks' % (NSEEDS - 1),
      '// (level_v3_lib.run_checks: cake routes, cover <= 4 s, the Human\'s reach, floats, overlaps, door sweeps; dropped scatter re-drawn). %d of %d passed.' % (stats['ok'], 4 * NSEEDS),
      '// seed, tier, time (0 day 1 evening 2 night), home bits (1 grandma 2 kid 4 dog 8 cat; the Human always), variant letters per district,',
      '// cake (0 M1 .. 3 M4), ajar doors (generic mover indices), loose fasteners (base rows), first scatter row, scatter count.',
      'static const FLevelV3Seed GV3Seeds[] =', '{']
for x in SEEDS:
    d3 = (x['doors'] + [-1, -1, -1])[:3]; l3 = (x['loose'] + [-1, -1, -1])[:3]
    G.append('\t{ %d, %d, %d, %d, TEXT("%s"), %d, { %d, %d, %d }, { %d, %d, %d }, %d, %d },' % (x['seed'], x['tier'], x['time'], x['home'], x['var'], x['cake'], *d3, *l3, x['sc'][0], x['sc'][1]))
G += ['};', '', '// Scatter rows: zone, item (indices into GV3ScatterNames), has a place (0 = a v2 object brought back: new38 sugar, new39 jam), x, y, z, yaw.',
      'static const FLevelV3Scatter GV3Scatter[] =', '{']
for z, it, p, yaw in SC:
    if p: G.append('\t{ %d, %d, 1, %s, %s, %s, %s },' % (z, it, f(p[0], 1), f(p[1], 1), f(p[2], 1), f(yaw, 1)))
    else: G.append('\t{ %d, %d, 0, 0.f, 0.f, 0.f, 0.f },' % (z, it))
G += ['\t{ 0, 0, 0, 0.f, 0.f, 0.f, 0.f }', '};', 'static const TCHAR* const GV3ScatterNames[] = { %s };' % ', '.join('TEXT("%s")' % q(s) for s in NAMES), '']
with open(os.path.join(SRC, 'LevelV3Data.inl'), 'w', encoding='utf-8-sig', newline='') as fo: fo.write('\n'.join(G) + '\n')
json.dump({'what': 'the validated seed table that went into Source/AntKitchen/LevelV3Data.inl (Tools/level/gen_level_v3_code.py)', 'n_per_tier': NSEEDS,
           'stats': dict(stats), 'fails': dict(fails), 'seeds': SEEDS, 'scatter': SC, 'scatter_names': NAMES},
          open(os.path.join(ROOT, 'docs', 'level', 'level_v3_seed_table.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
print('base %d rows, %d movers; variants %d, ops %d, variant rows %d; fixtures %d, fasteners %d' % (len(insts), len(movers), len(VARS), len(OPS), len(VART), len(FIX), len(FAST)))
print('seeds: %s in %.0f s; failures %s' % (dict(stats), time.time() - t0, fails.most_common(6)))
for t in (1, 2, 3, 4):
    xs = [x for x in SEEDS if x['tier'] == t]
    if xs: print('tier %d: %d seeds, cover worst %.2f s, cake best max %.0f s; time %s; cake %s' % (t, len(xs), max(x['cover'] for x in xs), max(x['cake_s'][0] for x in xs),
                 dict(collections.Counter(x['time'] for x in xs)), dict(collections.Counter(x['cake'] for x in xs))))
