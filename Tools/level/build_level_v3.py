# -*- coding: utf-8 -*-
# Ant Kitchen, level v3: docs/level/level_v3.json (the v2 schema, extended) + SourceArt/Blender/level_v3/level_v3_manifest.json (every
# placed mesh of the base state + per-variant and «после боя» instances) + render states for Tools/blender/level_v3_preview.py.
#   python Tools/level/build_level_v3.py
# Reads docs/level/level_v2.json, SourceArt/Blender/level_v2/level_v2_manifest.json, SourceArt/Blender/level_v3/level_v3_new_pieces.json.
# v2 files are not changed.
import json, os, sys, copy, math, datetime
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
try: sys.stdout.reconfigure(encoding='utf-8', errors='replace')
except Exception: pass
import level_v3_layout as L
import level_v3_lib as X
B = X.load_base(); D2 = B['D2']
OUT3 = os.path.join(X.ART, 'level_v3'); os.makedirs(OUT3, exist_ok=True)
SHOW = {'palisad': 'A', 'reddvor': 'B', 'plintus': 'B', 'kladovye': 'C', 'moika': 'A', 'plita': 'A', 'lednik': 'A', 'plato': 'A', 'svalka': 'A',
        'zastenye': 'A', 'porog': 'C', 'babushka': 'A', 'play': 'D', 'stenka': 'C'}
SHOW_SEED = 7

def comp_force(force, seed=0, clean=True):
    c = L.compose(seed, 1, force=dict({k: 'A' for k in L.DISTRICT_ORDER}, **force))
    if clean: c['doors'] = {}; c['scatter'] = []
    return c

MAT_V2 = [('SM_K_Plate', 'ceramic'), ('SM_K_Mug', 'ceramic'), ('SM_K_Bowl', 'ceramic'), ('SM_K_SugarBowl', 'ceramic'), ('SM_K_Jar', 'glass'), ('SM_K_SpiceJar', 'glass'),
          ('SM_K_PlantPot', 'terracotta'), ('SM_K_Bottle', 'glass'), ('SM_K_TinCan', 'metal'), ('SM_K_Curtain', 'cloth'), ('SM_K_Towel', 'cloth'), ('SM_L_Blanket', 'cloth'),
          ('SM_L_Book', 'paper'), ('SM_K_CardboardBox', 'cardboard'), ('SM_K_CerealBox', 'cardboard'), ('SM_K_Newspaper', 'paper'), ('SM_L_Sock', 'cloth'), ('SM_K_Slipper', 'cloth'),
          ('SM_L_YarnBall', 'yarn'), ('SM_L_YarnThread', 'yarn'), ('SM_L_Vase', 'ceramic'), ('SM_K_Kettle', 'metal'), ('SM_K_Pot', 'metal'), ('SM_L_SodaCan', 'metal'),
          ('SM_K_FridgeNote', 'paper'), ('SM_K_Calendar', 'paper'), ('SM_L_Garland', 'paper'), ('SM_K_GroceryBag', 'paper'), ('SM_K_FlourSack', 'paper'), ('SM_K_PastaBox', 'cardboard')]
INTER = {}
for it in L.INTERACTIVE:
    INTER.setdefault(it['obj'], []).append(it)
FAST_OF = {}
for it in L.INTERACTIVE:
    for f in it['fasteners']:
        FAST_OF.setdefault(f, []).append(it['id'])

def annotate(o):
    o = dict(o)
    if not o.get('mat'):
        for pre, m in MAT_V2:
            if (o.get('mesh') or '').startswith(pre): o['mat'] = m; break
    if o['id'] in INTER:
        it = INTER[o['id']][0]
        o['interactive'] = it['id']; o['fasteners'] = [{'id': f, 'kind': it.get('kind', 'friction')} for f in it['fasteners']]
        o['break'] = L.BREAK.get(it['mat'], {}).get('result'); o['chain'] = it['chain']
    if o['id'] in FAST_OF: o['fastener_of'] = FAST_OF[o['id']]
    if o.get('mat') and 'break' not in o: o['break'] = L.BREAK.get(o['mat'], {}).get('result')
    if o['id'] in L.CONTENTS: o['contents'] = L.CONTENTS[o['id']]
    if (o.get('mesh') or '') in ('SM_K_Towel', 'SM_L_Blanket') or L.PIECES.get(o.get('mesh') or '', {}).get('cloth'): o['cloth'] = True
    return o

base = X.apply(B, None)
D3 = copy.deepcopy(D2)
D3['meta'] = {'title': 'Ant Kitchen — level v3 (design data, not in the game yet)', 'generated': datetime.datetime.now().strftime('%Y-%m-%d %H:%M'), 'units': D2['meta'].get('units'),
              'tool': 'Tools/level/build_level_v3.py from level_v2.json + Tools/level/level_v3_layout.py', 'version': L.VERSION,
              'schema': 'the schema of level_v2.json; objects/hides/patrol/surfaces = the BASE state (every seed); everything that depends on the seed is under "v3"',
              'extensions': {'objects[].mat': 'material / break class (v3.break_classes)', 'objects[].fasteners': 'what holds it (ids of fastener pieces or kinds)', 'objects[].fastener_of': 'the interactive objects this fastener holds',
                             'objects[].contents': 'container pool', 'objects[].cloth': 'simulation-ready cloth', 'objects[].district/variant': 'only in v3.variants', 'objects[].v3': 'new | moved | scaled',
                             'containers[].contents': 'pool for the seed', 'v3.routines': 'NPC routine spots', 'v3.inside_people': 'spots on the bodies', 'v3.variants': 'per district 3-4 authored states'}}
D3['objects'] = [annotate(o) for o in base['objects']]
D3['human']['sees']['never_targets'] = [h for h in D2['human']['sees']['never_targets'] if 'rect' not in h] + [{k: v for k, v in h.items() if k != 'object'} for h in base['hides']]
D3['human']['patrol']['points'] = [list(L.PATROL_FIX[k][0]) if k in L.PATROL_FIX else p for k, p in enumerate(D2['human']['patrol']['points'])]
D3['human']['patrol']['v3_fix'] = {str(k): v[1] for k, v in L.PATROL_FIX.items()}
D3['hazards'] = [h for h in D2['hazards'] if h['id'] not in base['hazards_off']] + base['hazards_add']
for c in D3['containers']:
    if c['id'] in L.CONTENTS: c['contents'] = L.CONTENTS[c['id']]
D3['surfaces'] = D2['surfaces'] + [
    {'id': 'toybox_top', 'ru': 'Крышка ящика для игрушек', 'tier': 'work', 'rects': [[8360, 505, 9800, 1425]], 'z': 840, 'area': 'living', 'aids': ['стена', 'диван (подлокотник)'], 'src': 'level_v3'},
    {'id': 'crate_top', 'ru': 'Картошка в ящике', 'tier': 'floor', 'rects': [[4250, -4200, 5150, -3600]], 'z': 450, 'area': 'kitchen', 'aids': ['рейки'], 'src': 'level_v3'},
    {'id': 'gift_bookcase', 'ru': 'Подарок на книжном шкафу', 'tier': 'top', 'rects': [[19230, -3670, 20170, -2730]], 'z': 5330, 'area': 'living', 'aids': ['книжный шкаф'], 'src': 'level_v3'},
    {'id': 'backpack_top', 'ru': 'Рюкзак папы', 'tier': 'floor', 'rects': [[-3200, -2250, -2600, -1750]], 'z': 820, 'area': 'kitchen', 'aids': ['лямки'], 'src': 'level_v3'}]
D3['pets']['dog']['spots'] = base['pets']['dog']; D3['pets']['cat']['spots'] = base['pets']['cat']
D3['loot']['main']['spots'] = D2['loot']['main']['spots'] + [{'id': 'M4', 'p': list(L.CAKE_SPOTS['M4']['p']), 'ru': L.CAKE_SPOTS['M4']['ru'], 'district': 'stenka', 'area': 'living', 'only_with': 'stenka C', 'container': 'v3_gift2', 'behind_door': False}]
for sp in D3['loot']['spots']:
    if sp['i'] in L.LOOT_FIX:
        sp['p'] = list(L.LOOT_FIX[sp['i']][0]); sp['v3'] = L.LOOT_FIX[sp['i']][1]
D3['changes'] = {'v2': D2.get('changes'), 'v3': {'removed': L.REMOVE, 'moved': {k: {'to': v[0], 'yaw': v[1], 'z0': v[2], 'why': v[3]} for k, v in L.MOVE.items()},
                 'scaled': L.SCALE, 'added': [{'id': a['id'], 'piece': a['piece'], 'ru': a['ru'], 'p': a['p'], 'yaw': a['yaw']} for a in L.ADD]}}

# variants: the objects and ops of each (others A)
VAR = {}
for d in L.DISTRICT_ORDER:
    VAR[d] = []
    for v in L.VARIANTS[d]:
        st = X.apply(B, comp_force({d: v['id']}))
        objs = [annotate(o) for o in st['objects'] if o.get('district') == d and o.get('v3') in ('new', 'restored')]
        removed = [op[1] for op in v['ops'] if op[0] == 'remove']
        moved = {op[1]: [o for o in st['objects'] if o['id'] == op[1]][0]['bb'] for op in v['ops'] if op[0] == 'move' and any(o['id'] == op[1] for o in st['objects'])}
        ops = [[str(x) if not isinstance(x, (int, float, str, list, tuple, type(None))) else x for x in op] if op[0] != 'add' else ['add', op[1]['id']] for op in v['ops']]
        VAR[d].append({'id': v['id'], 'ru': v['ru'], 'plays': v['plays'], 'weight': v['weight'], 'when': v['when'], 'ops': ops, 'objects': objs, 'removed': removed, 'moved_bb': moved})
# routine paths on the showcase state
show = X.apply(B, comp_force(SHOW, SHOW_SEED))
Gd = X.Grid(D2); stand, _s = X.human_nav(B, show, Gd)
PATHS = {}
for who in ('human', 'grandma', 'kid'):
    sp = [r for r in L.ROUTINES[who] if r.get('p')]
    PATHS[who] = []
    for a, b in zip(sp, sp[1:]):
        p = X.path_on_stand(Gd, stand, a['p'][:2], b['p'][:2])
        PATHS[who].append({'from': a['id'], 'to': b['id'], 'pts': p})
NEED = [
    {'id': 'seed', 'ru': 'Сид рейда: compose(seed) из level_v3_layout (mulberry32, тот же код в карте) — выбрать вариант каждого района, время суток, кто дома, дверцы, «шатающийся» крепёж, место торта, россыпь; на старте рейда спавнить/убирать предметы вариантов (v3.variants[].objects) и менять данные укрытий, патруля, графа', 'size': 'большой', 'value': 1},
    {'id': 'fasteners', 'ru': 'Крепёж: шуруп/петля/кольцо/скотч/крышка/спица/узел — точка [E] на крепеже, сила как у дверей (суммируется), прогресс, состояние «шатается»; объект на крепеже — физический (отпустить, повернуть на втором уголке, упасть)', 'size': 'большой', 'value': 2},
    {'id': 'break', 'ru': 'Классы разрушения: стекло/керамика/терракота бьются при падении (порог из v3.break_classes), спавн «после»-мешей (осколки, клякса, земля), содержимое высыпается, шум по классу', 'size': 'средний', 'value': 3},
    {'id': 'liquid', 'ru': 'Жидкости: открученная и лежащая бутылка 20 с растит лужу (меш SM_V3_ColaPuddle масштабом), лужа — существующий статус «липко»/«мокро»', 'size': 'средний', 'value': 4},
    {'id': 'slip', 'ru': 'Человек поскальзывается в луже (делает другой агент сейчас — «скользкая лужа»): роняет то, что в руках, 3 с лежит — завязать на лужи из цепочек', 'size': 'малый (если лужа готова)', 'value': 5},
    {'id': 'cloth', 'ru': 'Ткань: скатерть/шторы/плед/шалаш — Chaos Cloth или свой спринг-грид на 24x90; тянуть за кромку отрядом (как дверь), рвать по линии укусов, падать кучей (куча = новое укрытие в данных)', 'size': 'большой', 'value': 6},
    {'id': 'routines', 'ru': 'Распорядок NPC: v3.routines — точки, время суток, предметы; Человек уходит в коридор на 45 с (выносит мусор), бабушка поливает цветы и сматывает пряжу, малыш вываливает игрушки; реакция «чинит/убирает» на сломанное', 'size': 'большой', 'value': 7},
    {'id': 'who_home', 'ru': 'Кто дома: не спавнить бабушку/малыша/Бадрика/кошку по сиду', 'size': 'малый', 'value': 8},
    {'id': 'inside', 'ru': 'Внутрь людей: v3.inside_people — состояние «внутри» на сегменте (ухо, нос, штанина, карман, пучок, тапок), эффекты на Человека/бабушку/малыша', 'size': 'большой', 'value': 9},
    {'id': 'time_of_day', 'ru': 'Время суток: свет по v3.time_of_day (солнце, небо, лампы), темнота ночью как «выбило пробки»', 'size': 'средний', 'value': 10},
    {'id': 'slots', 'ru': 'Новые слоты материалов (ClothCheck, Curtain, Cola, PetClear, GiftWrap, Photo, Bulb, Brine, Latex, Plaster, Potato) + tint экземпляра (шарики, простыни) + двусторонний материал ткани', 'size': 'малый', 'value': 11},
]
D3['needs_code'] = D2.get('needs_code', [])
D3['v3'] = {'version': L.VERSION, 'story': L.STORY, 'look': L.ROOM_LOOK, 'time_of_day': L.TIME_OF_DAY, 'lights': L.LIGHTS, 'density': L.DENSITY,
            'pieces': {k: {kk: vv for kk, vv in v.items()} for k, v in L.PIECES.items()}, 'break_classes': L.BREAK, 'fastener_kinds': L.FASTENER,
            'interactive': L.INTERACTIVE, 'chains': L.CHAINS, 'contents': L.CONTENTS, 'cloth': L.CLOTH, 'toybox_contents': L.TOYBOX_CONTENTS,
            'inside_people': L.INSIDE_PEOPLE, 'routines': L.ROUTINES, 'routine_paths_showcase': PATHS, 'variants': VAR, 'district_order': L.DISTRICT_ORDER,
            'cake_spots': L.CAKE_SPOTS, 'seed': L.SEED, 'validate': L.VALIDATE, 'patrol_fix': {str(k): v for k, v in L.PATROL_FIX.items()}, 'human_step_ramps': L.HUMAN_STEP_RAMPS,
            'chaos': [[str(x) if isinstance(x, dict) is False else x.get('id') for x in op] for op in L.CHAOS], 'showcase': {'force': SHOW, 'seed': SHOW_SEED},
            'needs_code': NEED, 'fix_log': L.FIX_LOG}
D3['counts'] = dict(D2.get('counts', {}), objects=len(D3['objects']), v3_new=sum(1 for o in D3['objects'] if o.get('v3') == 'new'), v3_moved=sum(1 for o in D3['objects'] if o.get('v3') == 'moved'),
                    v3_scaled=sum(1 for o in D3['objects'] if o.get('v3') == 'scaled'), variants=sum(len(v) for v in VAR.values()), hides=len(base['hides']))
with open(os.path.join(X.LV, 'level_v3.json'), 'w', encoding='utf-8') as f: json.dump(D3, f, ensure_ascii=False, indent=1)

# ---- manifest: base instances grouped by piece, plus variant and chaos instances
def piece_entry(name, file):
    if name in B['piece_v3']:
        p = B['piece_v3'][name]; return {'name': name, 'file': name + '.fbx', 'set': 'level_v3', 'triangles': p['triangles'], 'materials': p['materials'], 'origin': p['origin'],
                                         'bbox_min_m': p['bbox_min_m'], 'bbox_max_m': p['bbox_max_m'], 'note': p.get('note', '')}
    p = B['piece_v2'][name]
    return {'name': name, 'file': file, 'set': p.get('set', 'level_v2'), 'triangles': p['triangles'], 'materials': p['materials'], 'origin': p.get('origin'), 'bbox_min_m': p['bbox_min_m'], 'bbox_max_m': p['bbox_max_m']}
def group(insts):
    P = {}
    for x in insts:
        e = P.setdefault(x['piece'], dict(piece_entry(x['piece'], x['file']), instances=[]))
        e['instances'].append(x['inst'])
    return list(P.values())
mats = dict(json.load(open(os.path.join(X.ART, 'level_v2', 'level_v2_manifest.json'), encoding='utf-8'))['materials'])
if B['NP']: mats.update(B['NP']['materials'])
MAN = {'what': 'Ant Kitchen level v3: every placed mesh of the BASE state (all seeds) in the kitchen-manifest schema; variant_instances / chaos_instances are spawned by the seed. Written by Tools/level/build_level_v3.py.',
       'conventions': json.load(open(os.path.join(X.ART, 'level_v2', 'level_v2_manifest.json'), encoding='utf-8'))['conventions'],
       'roles': '0 static, 1 generic mover (instance.mover), 2 built by a gadget actor, 3 rides a mover', 'materials': mats, 'pieces': group(base['insts'])}
MAN['totals'] = {'pieces': len(MAN['pieces']), 'placed_instances': sum(len(p['instances']) for p in MAN['pieces']),
                 'new_pieces_v3': len(B['piece_v3']), 'new_triangles_v3': sum(p['triangles'] for p in B['piece_v3'].values())}
base_ids = {x['object'] for x in base['insts']}
VI = {}
for d in L.DISTRICT_ORDER:
    VI[d] = {}
    for v in L.VARIANTS[d]:
        st = X.apply(B, comp_force({d: v['id']}))
        mv = {op[1] for op in v['ops'] if op[0] in ('move', 'restore')}
        VI[d][v['id']] = [dict(x['inst'], piece=x['piece'], file=x['file'], op='move' if x['object'] in mv else 'add') for x in st['insts'] if x['object'] not in base_ids or x['object'] in mv]
MAN['variant_instances'] = VI
ch = X.apply(B, comp_force(SHOW, SHOW_SEED), chaos=True)
MAN['chaos_instances'] = [dict(x['inst'], piece=x['piece'], file=x['file']) for x in ch['insts'] if any(o['id'] == x['object'] and o.get('district') == 'chaos' for o in ch['objects'])]
with open(os.path.join(OUT3, 'level_v3_manifest.json'), 'w', encoding='utf-8') as f: json.dump(MAN, f, ensure_ascii=False, indent=1)

# ---- render states (Tools/blender/level_v3_preview.py)
def render_state(st, name):
    poses = {}
    for o in st['objects']:
        if o.get('pose_v3'): poses[o['id']] = o['pose_v3']
    insts = []
    for x in st['insts']:
        i = dict(x['inst']); i['piece'] = x['piece']; i['file'] = x['file']
        if x['object'] in poses: i['pose'] = poses[x['object']]
        insts.append(i)
    doc = {'name': name, 'comp': st['comp'], 'instances': insts, 'materials': mats}
    with open(os.path.join(OUT3, 'state_%s.json' % name), 'w', encoding='utf-8') as f: json.dump(doc, f, ensure_ascii=False)
render_state(show, 'showcase'); render_state(ch, 'chaos')
render_state(X.apply(B, None), 'base')
print('level_v3.json: %d objects (new %d, moved %d, scaled %d), hides %d, variants %d' % (len(D3['objects']), D3['counts']['v3_new'], D3['counts']['v3_moved'], D3['counts']['v3_scaled'], len(base['hides']), D3['counts']['variants']))
print('manifest: %d pieces, %d base instances; v3 new pieces %d, %d triangles' % (MAN['totals']['pieces'], MAN['totals']['placed_instances'], MAN['totals']['new_pieces_v3'], MAN['totals']['new_triangles_v3']))
