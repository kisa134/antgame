# -*- coding: utf-8 -*-
# Ant Kitchen, level v2: docs/level/level_data.json (the level as it is) + Tools/level/level_v2_layout.py (the redesign)
#   + SourceArt/Blender/level_v2/level_v2_new_pieces.json (boxes of the new meshes, written by Tools/blender/level_v2_pieces.py)
#   -> docs/level/level_v2.json                               the whole new layout in the schema of level_data.json
#   -> SourceArt/Blender/level_v2/level_v2_manifest.json      every placed mesh of v2 (old and new) in the kitchen-manifest schema
#   python Tools/level/build_level_v2.py
# Pure python. Reads Source/AntKitchen/KitchenArtData.inl (never writes there). Nothing here touches the game.
import json, os, sys, math, re, copy, heapq, time
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
try: sys.stdout.reconfigure(encoding='utf-8', errors='replace')
except Exception: pass
import level_v2_layout as V
LV = os.path.join(ROOT, 'docs', 'level')
ART = os.path.join(ROOT, 'SourceArt', 'Blender')
# v2 is in the game now: the v1 level this redesign starts from is frozen in docs/level/v1 (level_data.json as extracted from the
# v1 code, and the v1 placement table); docs/level/level_data.json is re-extracted from the real code and describes v2.
D1 = json.load(open(os.path.join(LV, 'v1', 'level_data.json'), encoding='utf-8'))
NP = json.load(open(os.path.join(ART, 'level_v2', 'level_v2_new_pieces.json'), encoding='utf-8'))
D = copy.deepcopy(D1)
R1 = lambda v: round(float(v), 1)

# ---- transforms: turn by rot degrees (multiple of 90) about pivot, then shift by d -------------------------------------------------
class T:
    def __init__(self, d=(0, 0, 0), rot=0, pivot=None, why=''):
        self.d = (float(d[0]), float(d[1]), float(d[2]) if len(d) > 2 else 0.0); self.rot = int(rot) % 360; self.pivot = pivot; self.why = why
        self.c, self.s = {0: (1, 0), 90: (0, 1), 180: (-1, 0), 270: (0, -1)}[self.rot]
    def xy(self, x, y):
        if self.rot:
            px, py = self.pivot
            x, y = px + self.c * (x - px) - self.s * (y - py), py + self.s * (x - px) + self.c * (y - py)
        return x + self.d[0], y + self.d[1]
    def pt(self, p):
        x, y = self.xy(p[0], p[1])
        return [R1(x), R1(y)] + ([R1(p[2] + self.d[2])] if len(p) > 2 else [])
    def rect(self, r):
        a, b = self.xy(r[0], r[1]), self.xy(r[2], r[3])
        return [R1(min(a[0], b[0])), R1(min(a[1], b[1])), R1(max(a[0], b[0])), R1(max(a[1], b[1]))]
    def box(self, b):
        r = self.rect([b[0], b[1], b[3], b[4]])
        return [r[0], r[1], R1(b[2] + self.d[2]), r[2], r[3], R1(b[5] + self.d[2])]
    def vec(self, v):
        return [self.c * v[0] - self.s * v[1], self.s * v[0] + self.c * v[1], v[2]]

OBJ1 = {o['id']: o for o in D1['objects']}
def in_rect(p, r): return r[0] <= p[0] <= r[2] and r[1] <= p[1] <= r[3]
RULES = []     # (area or None, rect, only-ids or None, T, z range for point data or None)
for g in V.GROUPS:
    RULES.append((g['area'], g['rect'], g.get('only'), T(g['d'], g.get('rot', 0), g.get('pivot'), g['name']), None))
for oid, to in V.MOVES.items():
    o = OBJ1[oid]
    d = (to[0] - o['origin'][0], to[1] - o['origin'][1], (to[2] - o['origin'][2]) if len(to) > 2 else 0.0)
    b = o['bb']
    RULES.append((o['area'], [b[0] - 30, b[1] - 30, b[2] + 30, b[3] + 30], [oid], T(d, 0, None, 'перенос: ' + o['ru']), (o['z'][0] - 60, o['z'][1] + 60)))
def rule_obj(o):
    for area, rect, only, t, zr in RULES:
        if (only is None or o['id'] in only) and o['area'] == area and in_rect(o['c'], rect): return t
    return None
def rule_pt(p, area=None, z=None):
    if z is None and len(p) > 2: z = p[2]
    for a, rect, only, t, zr in RULES:
        if zr and (z is None or not (zr[0] <= z <= zr[1])): continue
        if (area is None or a == area) and in_rect(p, rect): return t
    return None

def area_of(x, y):
    best = None
    for a in V.AREAS:
        for r in a['rects']:
            dx = max(r[0] - x, x - r[2], 0.0); dy = max(r[1] - y, y - r[3], 0.0)
            dd = math.hypot(dx, dy)
            if best is None or dd < best[0]: best = (dd, a['id'])
    return best[1]

# ---- the art table of the game: where every existing mesh instance stands and how it is turned --------------------------------------
def fl(s): return float(s.strip().rstrip('f').rstrip('.') or 0) if s.strip() not in ('', '-') else 0.0
def v3(s): return [fl(v) for v in s.split(',')]
INL = open(os.path.join(LV, 'v1', 'KitchenArtData.inl'), encoding='utf-8-sig').read()
ROWS = [{'name': m.group(1), 'index': int(m.group(2)), 'role': int(m.group(3)), 'parent': int(m.group(4)), 'loc': v3(m.group(5)), 'ax': v3(m.group(6)), 'ay': v3(m.group(7)),
         'az': v3(m.group(8)), 'scale': v3(m.group(9))}
        for m in re.finditer(r'\{ TEXT\("(\w+)"\), (\d+), (\d+), (-?\d+), \{([^}]*)\}, \{([^}]*)\}, \{([^}]*)\}, \{([^}]*)\}, \{([^}]*)\} \},', INL)]
MOV = [{'inst': int(m.group(1)), 'type': int(m.group(2)), 'axis': v3(m.group(3)), 'angle': fl(m.group(4)), 'slide': v3(m.group(5)), 'flags': int(m.group(6)), 'auto': fl(m.group(7)),
        'open': m.group(8), 'close': m.group(9)}
       for m in re.finditer(r'\{ (\d+), (\d+), \{([^}]*)\}, ([^,{}]+), \{([^}]*)\}, (\d+), ([^,{}]+), TEXT\("([^"]*)"\), TEXT\("([^"]*)"\) \},', INL)]
assert len(ROWS) == D1['counts']['art_instances'] and len(MOV) == D1['counts']['movers'], (len(ROWS), len(MOV))

# ---- objects ----------------------------------------------------------------------------------------------------------------------------
objects, changes = [], {'removed': [], 'moved': [], 'copied': [], 'new': []}
ROW_T = {}          # art row -> transform
for o in D['objects']:
    if o['id'] in V.REMOVE:
        changes['removed'].append({'id': o['id'], 'ru': o['ru'], 'mesh': o.get('mesh'), 'why': V.REMOVE_WHY.get(o['id'], V.REMOVE_WHY['*'])}); continue
    t = rule_obj(o)
    if t:
        if o.get('origin'): o['origin'] = t.pt(o['origin'])
        o['bb'] = t.rect(o['bb']); o['c'] = t.pt(o['c'])
        o['s'] = [R1(o['bb'][2] - o['bb'][0]), R1(o['bb'][3] - o['bb'][1])]
        o['z'] = [R1(o['z'][0] + t.d[2]), R1(o['z'][1] + t.d[2])]
        o['yaw'] = R1(((o['yaw'] + t.rot + 180) % 360) - 180)
        if 'tops' in o: o['tops'] = [R1(z + t.d[2]) for z in o['tops']]
        if 'bb_open' in o: o['bb_open'] = t.rect(o['bb_open'])
        if 'c_open' in o: o['c_open'] = t.pt(o['c_open'])
        if 'yaw_open' in o: o['yaw_open'] = R1(((o['yaw_open'] + t.rot + 180) % 360) - 180)
        o['area'] = area_of(o['c'][0], o['c'][1]); o['v2'] = 'moved'; o['v2_why'] = t.why
        changes['moved'].append({'id': o['id'], 'ru': o['ru'], 'why': t.why, 'to': o['c']})
        if o['id'].startswith('art'): ROW_T[int(o['id'][3:])] = t
    objects.append(o)
NEXT = {}
for o in objects:
    if o.get('mesh'): NEXT[o['mesh']] = max(NEXT.get(o['mesh'], 0), o['index'] + 1)
COPIES = []         # (source art row, transform, scale, new index)
COPY_INT = []
for k, c in enumerate(V.COPY):
    src = OBJ1[c['src']]
    so = src['origin']
    d = c['d'] if 'd' in c else (c['to'][0] - so[0], c['to'][1] - so[1], c['to'][2] - so[2])
    t = T(d, c.get('rot', 0), (so[0], so[1]), 'копия')
    o = {k2: copy.deepcopy(v) for k2, v in src.items() if k2 not in ('mover', 'bb_open', 'c_open', 'yaw_open', 'pose', 'gadget', 'rides_mover', 'note', 'art_note')}
    bb, z = list(src['bb']), list(src['z'])
    sc = c.get('scale')
    if sc:      # scale about the origin (only used on pieces that stand square to the axes)
        ax = (sc[0], sc[1]) if abs(math.sin(math.radians(src['yaw']))) < 0.1 else (sc[1], sc[0])
        bb = [so[0] + (bb[0] - so[0]) * ax[0], so[1] + (bb[1] - so[1]) * ax[1], so[0] + (bb[2] - so[0]) * ax[0], so[1] + (bb[3] - so[1]) * ax[1]]
    o.update({'id': 'copy%d' % k, 'index': NEXT.get(src['mesh'], 0), 'ru': c.get('ru', src['ru']), 'role': 'static', 'origin': t.pt(so), 'bb': t.rect(bb), 'z': [R1(z[0] + t.d[2]), R1(z[1] + t.d[2])],
              'yaw': R1(((src['yaw'] + t.rot + 180) % 360) - 180), 'src': 'level_v2_layout.COPY (mesh of %s)' % c['src'], 'v2': 'copy'})
    if 'tops' in o: o['tops'] = [R1(v + t.d[2]) for v in o['tops']]
    o['c'] = [R1((o['bb'][0] + o['bb'][2]) / 2), R1((o['bb'][1] + o['bb'][3]) / 2)]; o['s'] = [R1(o['bb'][2] - o['bb'][0]), R1(o['bb'][3] - o['bb'][1])]
    o['area'] = area_of(o['c'][0], o['c'][1])
    NEXT[src['mesh']] = o['index'] + 1
    objects.append(o); changes['copied'].append({'id': o['id'], 'mesh': o['mesh'], 'ru': o['ru'], 'at': o['origin']})
    if c.get('hide'):
        COPY_INT.append({'id': 'copy%d' % k, 'ru': c['hide'], 'box': [o['bb'][0], o['bb'][1], o['z'][0], o['bb'][2], o['bb'][3], o['z'][1]], 'entry': 'как у оригинала', 'src': 'level_v2', 'object': o['id'], 'area': o['area']})
    COPIES.append((int(c['src'][3:]), t, sc, o['index'], o['id']))

def rodrigues(v, axis, deg):
    a = math.radians(deg); n = math.sqrt(sum(c * c for c in axis)) or 1.0
    k = [c / n for c in axis]; c, s = math.cos(a), math.sin(a)
    cr = [k[1] * v[2] - k[2] * v[1], k[2] * v[0] - k[0] * v[2], k[0] * v[1] - k[1] * v[0]]
    dt = sum(k[i] * v[i] for i in range(3))
    return [v[i] * c + cr[i] * s + k[i] * dt * (1 - c) for i in range(3)]
NEW_MOVERS = {'SM_K_BreadBox_Lid': {'open': 'Открыть хлебницу', 'close': 'Закрыть хлебницу', 'flags': 8, 'auto': 0, 'door_class': 'light'},
              'SM_L_CatFlap_Leaf': {'open': 'Открыть кошачью дверцу', 'close': 'Закрыть кошачью дверцу', 'flags': 0, 'auto': 15, 'door_class': 'heavy'}}
IN_RU = {'SM_K_Slipper': 'в тапке', 'SM_K_Dustpan': 'в совке', 'SM_K_GroceryBag': 'в пакете с продуктами', 'SM_K_CardboardBox': 'в картонной коробке', 'SM_K_Bottle': 'в бутылке',
         'SM_K_TinCan': 'в консервной банке', 'SM_K_BreadBox': 'в хлебнице', 'SM_K_CookieTin': 'в жестянке с печеньем', 'SM_L_ShoeRack': 'в обувнице', 'SM_L_ToyArch': 'под аркой из кубика',
         'SM_L_ToyCar': 'под грузовичком', 'SM_L_ToyTrain': 'в паровозике', 'SM_Y_FlowerBed': 'в грядке', 'SM_Y_WateringCan': 'в лейке', 'SM_S_CoffeeCup': 'в стаканчике'}
new_interiors = []
n_new = 0
for e in NP['pieces']:
    ru, cat, fx = V.MESH[e['name']]
    for i, ins in enumerate(e['instances']):
        mn, mx = ins['game_bbox_min_uu'], ins['game_bbox_max_uu']
        o = {'id': 'new%d' % n_new, 'mesh': e['name'], 'index': i, 'ru': ru, 'cat': cat, 'role': 'mover' if e['name'] in NEW_MOVERS else 'static', 'solid': not fx.get('flat', False), 'climbable': True,
             'src': 'SourceArt/Blender/level_v2/level_v2_new_pieces.json', 'origin': [R1(v) for v in ins['game_loc_uu']], 'bb': [R1(mn[0]), R1(mn[1]), R1(mx[0]), R1(mx[1])], 'z': [R1(mn[2]), R1(mx[2])],
             'yaw': ins.get('game_yaw_deg', 0.0), 'v2': 'new', 'tris': e['triangles']}
        n_new += 1
        o['c'] = [R1((mn[0] + mx[0]) / 2), R1((mn[1] + mx[1]) / 2)]; o['s'] = [R1(mx[0] - mn[0]), R1(mx[1] - mn[1])]
        o['area'] = area_of(o['c'][0], o['c'][1])
        if 'game_yaw_deg' not in ins: o['tilted'] = True
        if e.get('note'): o['art_note'] = e['note']
        if 'gap_under' in fx: o['gap_under'] = float(fx['gap_under'])
        if 'tops' in fx: o['tops'] = [R1(mn[2] + v) for v in fx['tops']]
        if fx.get('collision'): o['collision'] = fx['collision']
        if fx.get('interior'):
            o['interior'] = True
            if e['name'] in IN_RU:
                new_interiors.append({'id': '%s_%d' % (e['name'][5:].lower(), i), 'ru': IN_RU[e['name']], 'box': [R1(mn[0]), R1(mn[1]), R1(mn[2]), R1(mx[0]), R1(mx[1]), R1(mx[2])],
                                      'entry': e.get('note', ''), 'src': 'level_v2', 'object': o['id'], 'area': o['area']})
        if e['name'] in NEW_MOVERS:
            nm = NEW_MOVERS[e['name']]
            ax, ang, org = ins['hinge_axis_game'], ins['open_angle_game_deg'], ins['game_loc_uu']
            cs = [[org[j] + rodrigues([(mx[0] if k & 1 else mn[0]) - org[0], (mx[1] if k & 2 else mn[1]) - org[1], (mx[2] if k & 4 else mn[2]) - org[2]], ax, ang)[j] for j in range(3)] for k in range(8)]
            o['mover'] = {'part': len(MOV) + sum(1 for x in objects if x.get('v2') == 'new' and x['role'] == 'mover'), 'type': 'hinge', 'axis': ax, 'angle': ang, 'slide': [0.0, 0.0, 0.0],
                          'flags': (['human_bumps_shut'] if nm['flags'] & 8 else []), 'auto_close_s': float(nm['auto']), 'open_text': nm['open'], 'close_text': nm['close'], 'door_class': nm['door_class'],
                          'note': e['motion'].get('note', ''), 'src': 'level_v2'}
            o['bb_open'] = [R1(min(c[0] for c in cs)), R1(min(c[1] for c in cs)), R1(max(c[0] for c in cs)), R1(max(c[1] for c in cs))]
            o['z_open'] = [R1(min(c[2] for c in cs)), R1(max(c[2] for c in cs))]; o['pose'] = 'closed'
        objects.append(o); changes['new'].append({'id': o['id'], 'mesh': o['mesh'], 'ru': ru, 'at': o['origin'], 'area': o['area']})
D['objects'] = objects
# ---- everything that is a point or a box: moved with its group --------------------------------------------------------------------------
def mv_pt(p, area=None):
    t = rule_pt(p, area)
    return (t.pt(p) if t else list(p)), t
def mid(b): return ((b[0] + b[3]) / 2.0, (b[1] + b[4]) / 2.0)
for c in D['containers']:
    t = rule_pt(mid(c['box']), None, (c['box'][2] + c['box'][5]) / 2.0)
    if t: c['box'] = t.box(c['box']); c['v2'] = 'moved'
D['containers'] += copy.deepcopy(V.CONTAINERS_ADD)
ints = []
for it in D['interiors']:
    t = rule_pt(mid(it['box']), None, (it['box'][2] + it['box'][5]) / 2.0)
    if t: it['box'] = t.box(it['box']); it['v2'] = 'moved'
    it['area'] = area_of(*mid(it['box'])); ints.append(it)
D['interiors'] = ints + COPY_INT + new_interiors
for s in D['surfaces']:
    r0 = s['rects'][0]
    t = rule_pt(((r0[0] + r0[2]) / 2.0, (r0[1] + r0[3]) / 2.0), s['area'], s['z'])
    if t:
        s['rects'] = [t.rect(r) for r in s['rects']]; s['z'] = s['z'] + t.d[2]; s['v2'] = 'moved'
        s['area'] = area_of((s['rects'][0][0] + s['rects'][0][2]) / 2.0, (s['rects'][0][1] + s['rects'][0][3]) / 2.0)
    s['aids'] = list(s['aids']) + [a for a in V.SURFACE_AIDS.get(s['id'], []) if a not in s['aids']]
for s in V.SURFACES_ADD:
    s = dict(s); s.update({'src': 'level_v2_layout.py', 'note': '', 'v2': 'new'}); D['surfaces'].append(s)
conns = []
for c in D['connectors']:
    if c['id'] in V.CONNECTORS_DROP: continue
    moved = False
    for k, p in enumerate(c['pts']):
        q, t = mv_pt(p)
        if t: c['pts'][k] = q; moved = True
    if moved: c['v2'] = 'moved'; c['area'] = area_of(c['pts'][0][0], c['pts'][0][1])
    if c['id'] in V.CONNECTORS_NOTE: c['note'] = (c.get('note', '') + ' ' + V.CONNECTORS_NOTE[c['id']]).strip()
    c.setdefault('haul', {'oven_door': 'stairs', 'dishwasher_door': 'stairs'}.get(c['id']))
    conns.append(c)
for c in V.CONNECTORS_ADD:
    c = dict(c); zs = [p[2] for p in c['pts']]
    c.update({'height_uu': R1(max(zs) - min(zs)), 'src': 'level_v2_layout.py', 'exact': 'design', 'v2': 'new'}); conns.append(c)
D['connectors'] = conns
haz = []
for h in D['hazards']:
    if any(h['id'].startswith(p) for p in V.HAZARDS_DROP_PREFIX): continue
    key = 'circle' if 'circle' in h else 'rect'
    ctr = h[key][:2] if key == 'circle' else ((h[key][0] + h[key][2]) / 2.0, (h[key][1] + h[key][3]) / 2.0)
    t = rule_pt(ctr, h['area'], (h['z'][0] + h['z'][1]) / 2.0)
    if t:
        h[key] = (t.pt(h[key][:2]) + [h[key][2]]) if key == 'circle' else t.rect(h[key]); h['v2'] = 'moved'
    if h['id'] == 'fire_nest':
        h['circle'] = [V.SPAWN_FIX['nest1']['p'][0], V.SPAWN_FIX['nest1']['p'][1], 560]; h['area'] = 'yard'; h['ru'] = 'Красный холм: гнездо огненных'; h['v2'] = 'moved'
    haz.append(h)
haz += copy.deepcopy(V.HAZARDS_ADD)
for k, (p, does, sec, label) in enumerate(V.PETS['dog']):
    if does == 'sniff_or_eat':
        haz.append({'id': 'dog_eat%d' % k, 'ru': 'Бадрик ест еду рядом: ' + label, 'kind': 'pet', 'z': [0, 600], 'circle': [p[0], p[1], 1150], 'area': area_of(p[0], p[1]), 'exact': 'design',
                    'effect': 'пока стоит здесь (%d с), раз в 1,6 с съедает еду в 450 uu от пасти (пасть в 700 перед ним)' % sec, 'src': 'level_v2_layout.PETS (Critters.cpp:330)'})
D['hazards'] = haz
for it in D['interactables']:
    q, t = mv_pt(it['p'], it['area'])
    if t: it['p'] = q; it['v2'] = 'moved'; it['area'] = area_of(q[0], q[1])
    if it['id'] in V.INTERACT_MOVE: it['p'] = [float(v) for v in V.INTERACT_MOVE[it['id']]]; it['v2'] = 'moved'
D['interactables'] += copy.deepcopy(V.INTERACT_ADD)
sp = []
for s in D['spawns']:
    if s['kind'] in ('dog_spot', 'cat_spot'): continue
    q, t = mv_pt(s['p'], s['area'])
    if t: s['p'] = q; s['v2'] = 'moved'; s['area'] = area_of(q[0], q[1])
    if s['id'] in V.SPAWN_FIX: s.update(copy.deepcopy(V.SPAWN_FIX[s['id']])); s['v2'] = 'moved'
    sp.append(s)
for who, kind in (('dog', 'dog_spot'), ('cat', 'cat_spot')):
    for k, (p, does, sec, label) in enumerate(V.PETS[who]):
        sp.append({'id': '%s%d' % (who, k), 'ru': 'Точка %s: %s' % ('Бадрика' if who == 'dog' else 'кошки', label), 'kind': kind, 'p': [float(v) for v in p], 'area': area_of(p[0], p[1]),
                   'src': 'level_v2_layout.PETS', 'note': '%s, %d с' % (does, sec)})
    D['pets'][who]['spots'] = [{'p': [float(v) for v in p], 'does': does, 'seconds': float(sec), 'label': label, 'src': 'level_v2_layout.PETS'} for (p, does, sec, label) in V.PETS[who]]
for e in V.EXTRACT:
    sp.append({'id': e['id'], 'ru': e['ru'], 'kind': 'extract', 'p': [float(v) for v in e['p']], 'area': area_of(e['p'][0], e['p'][1]), 'src': 'level_v2_layout.py', 'note': 'нужен код' if e['needs_code'] else '',
               'radius': e.get('radius', 0), 'needs_code': e['needs_code']})
D['spawns'] = sp
for s in D['secrets']:
    q, t = mv_pt(s['p'], s['area'])
    if t: s['p'] = q
D['secrets'] = [s for s in D['secrets'] if s['area'] != 'yard'] + [
    {'ru': 'Северный лаз', 'p': [-7100.0, 4875.0, 0.0], 'what': 'второй вход в дом: из палисадника прямо в коридор за шкафами', 'area': 'kitchen', 'src': 'level_v2'},
    {'ru': 'Мусорный лаз', 'p': [7100.0, 980.0, 0.0], 'what': 'ход в гостиную за картонной коробкой, в обход двери', 'area': 'kitchen', 'src': 'level_v2'},
    {'ru': 'Кошачья дверца', 'p': [7100.0, -7350.0, 30.0], 'what': 'короткая дорога из гостиной домой через двор; открывается изнутри', 'area': 'living', 'src': 'level_v2'},
    {'ru': 'Красная щель', 'p': [19500.0, -9100.0, 0.0], 'what': 'лаз огненных под окном гостиной', 'area': 'living', 'src': 'level_v2'},
    {'ru': 'Тент под пледом', 'p': [11400.0, -800.0, 0.0], 'what': 'укрытие и точка ур. 2 под пледом у дивана', 'area': 'living', 'src': 'level_v2'},
    {'ru': 'Картонная коробка', 'p': [6250.0, 1150.0, 30.0], 'what': 'форт с четырьмя входами; за ним решётка вентиляции 2 и мусорный лаз', 'area': 'kitchen', 'src': 'level_v2'}]
for ev in D['events']:
    if ev['id'] == 2: ev['circle'] = [V.PETS['cat'][0][0][0], V.PETS['cat'][0][0][1], 800]

# ---- openings, spaces, walls, areas ------------------------------------------------------------------------------------------------------
for o in D['openings']:
    if o['id'] in V.OPENINGS_FIX: o.update(copy.deepcopy(V.OPENINGS_FIX[o['id']])); o['v2'] = 'fixed'
D['openings'] += [dict(o, src='level_v2_layout.py', v2='new') for o in V.OPENINGS_ADD]
for s in D['spaces']:
    if s['id'] in V.SPACES_FIX: s.update(copy.deepcopy(V.SPACES_FIX[s['id']])); s['v2'] = 'fixed'
D['spaces'] += [dict(s, src='level_v2_layout.py', v2='new') for s in V.SPACES_ADD]
D['walls'] += [{'ru': e['ru'], 'rect': e['rect'], 'z': e.get('z', [0, 2600]), 'kind': e['kind'], 'src': 'level_v2_layout.EDGES', 'v2': 'new'} for e in V.EDGES]
D['areas'] = [dict(a, src='level_v2_layout.AREAS') for a in V.AREAS]

# ---- the travel-time field (the audit's method: walls block, furniture does not) ----------------------------------------------------------
G = 100.0
xs = [r[0][0] for r in V.REGIONS] + [r[0][2] for r in V.REGIONS]; ys = [r[0][1] for r in V.REGIONS] + [r[0][3] for r in V.REGIONS]
GX0, GY0, GX1, GY1 = min(xs) - 100.0, min(ys) - 100.0, max(xs) + 100.0, max(ys) + 400.0
NX, NY = int((GX1 - GX0) / G), int((GY1 - GY0) / G)
ANT = D['constants']['ant']; VS, WK = ANT['sprint_uu_s'], ANT['wall_factor']
def cx(i): return GX0 + (i + 0.5) * G
def cy(j): return GY0 + (j + 0.5) * G
def ci(x): return max(0, min(NX - 1, int((x - GX0) // G)))
def cj(y): return max(0, min(NY - 1, int((y - GY0) // G)))
def reg(x, y):
    for r, k in V.REGIONS:
        if r[0] < x < r[2] and r[1] < y < r[3]: return k
    return 0
REG = [[reg(cx(i), cy(j)) for i in range(NX)] for j in range(NY)]
FENCE_S = 2 * 2600.0 / (VS * WK)
EXTRA = {}
for f in V.FENCE_LINES:
    r = f['rect']
    for j in range(cj(r[1] + 1), cj(r[3] - 1) + 1):
        if f['gap'] and f['gap'][0] < cy(j) < f['gap'][1]: continue
        EXTRA[(ci((r[0] + r[2]) / 2.0), j)] = FENCE_S
SPAWN = [s for s in D['spawns'] if s['id'] == 'spawn0'][0]['p']
TF = [[1e18] * NX for _ in range(NY)]
pq = [(0.0, ci(SPAWN[0]), cj(SPAWN[1]))]; TF[pq[0][2]][pq[0][1]] = 0.0
MOVES8 = [(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)]
while pq:
    t, i, j = heapq.heappop(pq)
    if t > TF[j][i]: continue
    for di, dj in MOVES8:
        a, b = i + di, j + dj
        if a < 0 or b < 0 or a >= NX or b >= NY or REG[b][a] == 0: continue
        if di and dj and (REG[j][a] == 0 or REG[b][i] == 0): continue
        nt = t + G / VS * math.hypot(di, dj) + EXTRA.get((a, b), 0.0)
        if nt < TF[b][a]: TF[b][a] = nt; heapq.heappush(pq, (nt, a, b))
def t_at(p):
    t = TF[cj(p[1])][ci(p[0])]
    if t > 1e17:
        t = min([TF[b][a] + math.hypot(a - ci(p[0]), b - cj(p[1])) * G / VS for b in range(max(0, cj(p[1]) - 5), min(NY, cj(p[1]) + 6)) for a in range(max(0, ci(p[0]) - 5), min(NX, ci(p[0]) + 6))] + [1e18])
    return t * 0.97 + abs(p[2] - (-225.0 if p[0] < -11100 else 0.0)) / (VS * WK)      # 8-direction paths run about 3 % long

# ---- loot --------------------------------------------------------------------------------------------------------------------------------
EV = {t['tier']: t['expected_value'] for t in D['loot']['food']['tiers']}
def inside(p, b, m=0.0): return b[0] - m <= p[0] <= b[3] + m and b[1] - m <= p[1] <= b[4] + m and b[2] - m <= p[2] <= b[5] + m
def classify(p):
    for c in D['containers']:
        if inside(p, c['box'], 5): return {'tier': 'int', 'container': c['id'], 'behind_door': True, 'where_ru': c['where_ru'], 'need_strength': c['need_strength']}
    for s in D['spaces']:
        for b in ([s['box']] if 'box' in s else list(s['boxes'].values())):
            if inside(p, b, 5): return {'tier': 'vent', 'space': s['id']}
    for it in D['interiors']:
        if inside(p, it['box'], 5): return {'tier': 'int', 'space': it['id'], 'where_ru': it['ru']}
    return {'tier': 'floor' if p[2] < 300 else ('work' if p[2] < 2100 else 'top')}
def in_hazard(p):
    for h in D['hazards']:
        if h['kind'] in ('rival', 'pet', 'wind', 'robot') or not (h['z'][0] - 20 <= p[2] <= h['z'][1] + 20): continue
        if 'circle' in h:
            if math.hypot(p[0] - h['circle'][0], p[1] - h['circle'][1]) <= h['circle'][2]: return h['id']
        elif h['rect'][0] <= p[0] <= h['rect'][2] and h['rect'][1] <= p[1] <= h['rect'][3]: return h['id']
    return None
def cap(p, door):
    t = t_at(p)
    c = [b[1] for b in V.TIER_RULE['bands'] if t < b[0]][0]
    if door or p[2] >= 2100 or in_hazard(p): c = min(3, c + 1)
    if t < 15: c = min(c, 1)
    return c, t
clamped = []
spots = []
for s in D['loot']['spots']:
    if s['i'] in V.LOOT_DROP or s['area'] in V.LOOT_DROP_AREAS: continue
    if s['i'] in getattr(V, 'LOOT_FIX', {}):      # a v1 spot put where the food really comes to rest
        fx = V.LOOT_FIX[s['i']]; s['p'] = [float(v) for v in fx['p']]; s['label'] = fx.get('label', s.get('label', '')); s['v2'] = 'fixed'; s.pop('probe', None)
    q, t = mv_pt(s['p'], s['area'])
    if t: s['p'] = q; s['area'] = area_of(q[0], q[1]); s['v2'] = 'moved'
    spots.append(s)
ni = max(s['i'] for s in D1['loot']['spots']) + 1
for (x, y, z, tier, label) in V.LOOT_ADD:
    p = [float(x), float(y), float(z)]
    s = {'i': ni, 'p': p, 'tier_loot': tier, 'group': 'level v2', 'label': label, 'src': 'level_v2_layout.LOOT_ADD', 'how': 'design', 'area': area_of(x, y), 'v2': 'new'}
    s.update(classify(p)); ni += 1; spots.append(s)
# Spots put where the food really comes to rest (the in-game check "lootall / lootrest", 20.11.2026): by their final place.
REST = {tuple(int(round(v)) for v in a): (b, lab) for (a, b, lab) in getattr(V, 'LOOT_REST', [])}
n_rest = 0
for s in spots:
    k = tuple(int(round(v)) for v in s['p'])
    if k in REST:
        b, lab = REST[k]; s['p'] = [float(v) for v in b]; s['was'] = list(k); s.pop('probe', None); n_rest += 1
        if lab: s['label'] = lab
        if s.get('how') == 'design': s.update(classify(s['p']))
        s['area'] = area_of(s['p'][0], s['p'][1]); s['v2'] = 'new, put to rest' if s.get('v2') == 'new' else 'fixed'
assert n_rest == len(REST), (n_rest, len(REST))
for lst in (spots, D['loot']['behind_doors']):
    for s in lst:
        if lst is not spots:
            q, t = mv_pt(s['p'], s['area'])
            if t: s['p'] = q; s['v2'] = 'moved'
        c, tt = cap(s['p'], bool(s.get('behind_door')))
        s['t_nest'] = round(tt, 1)
        if s['tier_loot'] > c:
            clamped.append({'p': s['p'], 'label': s.get('label') or s.get('group'), 'from': s['tier_loot'], 'to': c, 't': round(tt, 1)})
            s['tier_v1' if s.get('v2') != 'new' else 'tier_wanted'] = s['tier_loot']; s['tier_loot'] = c
        s['ev'] = EV[s['tier_loot']]
D['loot']['spots'] = spots
cyc = D['loot']['food']['heavy_cycle']
D['loot']['heavy'] = []
for k, (x, y, z, label) in enumerate(V.HEAVY):
    p = [float(x), float(y), float(z)]
    h = {'i': k, 'p': p, 'spawn_z': z + 40.0, 'label': label, 'first_item': cyc[k % len(cyc)], 'src': 'level_v2_layout.HEAVY', 'tier_loot': 3, 'area': area_of(x, y), 'ev': cyc[k % len(cyc)]['value'], 't_nest': round(t_at(p), 1), 'v2': 'new'}
    c = classify(p); h['tier'] = 'floor' if c['tier'] in ('floor', 'int') else c['tier']
    if c.get('where_ru'): h['under'] = c['where_ru']
    D['loot']['heavy'].append(h)
for s in D['loot']['trophies']:
    q, t = mv_pt(s['p'], s['area'])
    if t: s['p'] = q; s['v2'] = 'moved'
    if s['i'] in V.TROPHY_MOVE:
        s['p'] = [float(v) for v in V.TROPHY_MOVE[s['i']][0]]; s['label'] = V.TROPHY_MOVE[s['i']][1]; s['area'] = area_of(s['p'][0], s['p'][1]); s['v2'] = 'moved'
        s['tier'] = 'top'; s.pop('container', None); s.pop('behind_door', None)
    s['t_nest'] = round(t_at(s['p']), 1)
for k, fx in V.SPECIAL_FIX.items():
    s = D['loot']['special'][k]
    if 'p' in fx: s['p'] = [float(v) for v in fx['p']]
    if 'value' in fx: s['value_v1'] = s['value']; s['value'] = fx['value']
    s['v2_note'] = fx['note']
main = copy.deepcopy(V.MAIN)
for m in main['spots']:
    m['p'] = [float(v) for v in m['p']]; m['area'] = area_of(m['p'][0], m['p'][1]); m['t_nest'] = round(t_at(m['p']), 1); m.update({k: v for k, v in classify(m['p']).items() if k != 'tier'})
D['loot']['main'] = main
D['loot']['rules']['gradient_ru'] = V.TIER_RULE['ru']
D['loot']['rules']['stairs_ru'] = 'перепад до 600 uu еду не бьёт (бьётся от 1250 uu/с, это падение с ~800): тяжёлое спускают по ступеням — выдвинутые ящики, кубики, дверцы духовки и посудомойки'
D['loot']['rules']['main_ru'] = 'главный объект рейда: один, в одном из трёх мест глубокой зоны (%s); вес %d, ценность %d' % ('; '.join(m['ru'] for m in main['spots']), main['weight'], main['value'])
# ---- the Human: where he walks, what he never looks at (the hide table) -------------------------------------------------------------------
H = D['human']
H['patrol']['kitchen_box'] = V.HUMAN['kitchen_box']; H['patrol']['living_box'] = V.HUMAN['living_box']
H['patrol']['door_waypoints'] = [[float(v) for v in p] for p in V.HUMAN['door_waypoints']]
H['patrol']['points'] = V.HUMAN['patrol_points']
H['patrol']['kind'] += ' — v2: коробки блуждания подогнаны под новую мебель; points = рекомендованные остановки патруля (проверены скриптом)'
H['targeting_bounds'] = V.HUMAN['targeting_bounds']; H['targeting_note'] = V.HUMAN['targeting_note']
hide = [h for h in H['sees']['never_targets']]
for it in D['interiors']:
    if it['id'] in V.HIDE_SKIP or it.get('area') in ('yard', 'street'): continue
    b = it['box']
    hide.append({'id': it['id'], 'ru': it['ru'], 'rect': [b[0], b[1], b[3], b[4]], 'z': [b[2], b[5]], 'src': 'level_v2 hide table', 'needs_code': True})
for (hid, ru, rect, z0, z1) in V.HIDE_ADD:
    hide.append({'id': hid, 'ru': ru, 'rect': rect, 'z': [z0, z1], 'src': 'level_v2 hide table', 'needs_code': True})
H['sees']['never_targets'] = hide

# ---- districts, the route graph, extraction ------------------------------------------------------------------------------------------------
D['districts'] = copy.deepcopy(V.DISTRICTS)
def poly_len(pts): return sum(math.hypot(pts[k + 1][0] - pts[k][0], pts[k + 1][1] - pts[k][1]) for k in range(len(pts) - 1))
NODE = {n[0]: n for n in V.NODES}
edges = []
for (a, b, kind, mids, note) in V.EDGES_G:
    pts = [list(NODE[a][2][:2])] + [list(m) for m in mids] + [list(NODE[b][2][:2])]
    ln = poly_len(pts)
    edges.append({'a': a, 'b': b, 'kind': kind, 'pts': pts, 'len_uu': round(ln), 'len_m': round(ln / V.UU, 2), 't_sprint': round(ln / VS, 1), 't_walk': round(ln / ANT['walk_uu_s'], 1),
                  't_loaded': [round(ln / (ANT['walk_uu_s'] * 0.9), 1), round(ln / (ANT['walk_uu_s'] * 0.5), 1)], 'note': note})
D['graph'] = {'nodes': [{'id': n[0], 'ru': n[1], 'p': [float(v) for v in n[2]], 'district': n[3], 'role': n[4], 'ups': V.NODE_UPS.get(n[0], [])} for n in V.NODES], 'edges': edges,
              'routes': [{'id': r[0], 'ru': r[1], 'fast': r[2], 'covered': r[3]} for r in V.ROUTES], 'junctions': [{'node': j[0], 'id': j[1]} for j in V.JUNCTIONS],
              'note': 'длины и время здесь — по ломаным графа; сколько секунд на виду — docs/level/level_v2_check.json (Tools/level/check_level_v2.py)'}
D['extraction'] = copy.deepcopy(V.EXTRACT)
D['audit'] = {'grid': [GX0, GY0, GX1, GY1], 'regions': [[r, k] for r, k in V.REGIONS], 'region_area': {str(k): v for k, v in V.REGION_AREA.items()}, 'fence_lines': V.FENCE_LINES,
              'seed_out': V.SEED_OUT, 'routes': [[r[0], r[1], list(r[2])] for r in V.AUDIT_ROUTES], 'broken': {}, 'indoor': [1, 2], 'in_wall': [5], 'outdoor': [3, 4]}
D['needs_code'] = [
    {'id': 'hide_table', 'ru': 'Таблица укрытий для ИИ Человека: PickTarget читает human.sees.never_targets (сейчас четыре коробки зашиты в код). Без неё новые укрытия-предметы не работают.', 'size': 'малый', 'must': True},
    {'id': 'target_bounds', 'ru': 'Человек берёт цель только внутри дома (human.targeting_bounds).', 'size': 'малый', 'must': True},
    {'id': 'main_object', 'ru': 'Главный объект рейда: один на рейд, место — случайное из loot.main.spots; за ним идут и огненные.', 'size': 'средний (это сам режим вылазки)', 'must': True},
    {'id': 'ants_only', 'ru': 'Режим коллизии «только муравьи» для нитки, гирлянды, вязанки и липкой ленты (objects[].collision = ants_only): Человек проходит сквозь.', 'size': 'малый', 'must': True},
    {'id': 'toaster_rule', 'ru': 'Тостер: тост выпадает, только если на столешнице меньше двух; после выстрела 40 с без еды.', 'size': 'малый', 'must': False},
    {'id': 'latch', 'ru': 'Кошачья дверца открывается только из гостиной (флаг «засов изнутри» у AKitchenMover). Без него — просто тяжёлая дверь с обеих сторон.', 'size': 'малый', 'must': False},
    {'id': 'extract1', 'ru': 'Второй круг сдачи у угла дома (extraction[1]). Без него груз из двора несут до щели гнезда: +3000 uu.', 'size': 'малый', 'must': False},
    {'id': 'sprinkler', 'ru': 'Поливалка: зона воды по таймеру (эффект струи крана).', 'size': 'малый', 'must': False},
    {'id': 'lawn_robot', 'ru': 'Робот-газонокосилка: робот-пылесос события 3, постоянно, в коробке двора.', 'size': 'малый', 'must': False},
]
DONE = {'hide_table': 'LevelV2::CoverAt + AHumanCharacter::InCover: цель ИИ, топот (и его волна), шлепок и газета; то же для Человека под управлением игрока',
        'target_bounds': 'LevelV2::InHouse в AHumanAIController::PickTarget',
        'main_object': 'AAntKitchenGameMode::SpawnMainObject: один кусок торта на смену в обычном забеге (режим рейда не делался, огненные за ним не охотятся)',
        'ants_only': 'тег AntsOnly на четырёх мешах + AKitchenBuilder::LetThrough (Человек, бабушка, малыш, кошка и собака проходят сквозь)',
        'toaster_rule': 'KitchenInteractables.cpp: меньше двух тостов на столешнице и 40 с тишины',
        'latch': 'AKitchenMover::CanPull: со двора не тянется; закрывается сама через 15 с',
        'extract1': 'KitchenLoot.cpp: сдача в радиусе 500 от (-8150, -5750)',
        'sprinkler': 'LevelV2::SprinklerLevel по серверному времени + мокрая зона в EnvironmentFor: 9 с из каждых 26',
        'lawn_robot': 'стоит неподвижным предметом (решение владельца), не ездит'}
for it in D['needs_code']: it['done'] = DONE[it['id']]
D['changes'] = changes
D['changes']['loot_clamped'] = clamped
D['changes']['groups'] = [{'name': g['name'], 'rect': g['rect'], 'd': g['d'], 'rot': g.get('rot', 0)} for g in V.GROUPS]
D['tiers'][4]['z'] = 'контейнеры за дверями (v1 + хлебница) и укрытия-предметы: тапки, пакет, коробка, бутылки, банки, грузовички, обувница и др. (interiors)'
D['suspicious'] = []
D['v1_bugs'] = [
    {'bug': 'еда внутри пола ямы', 'fix': 'яма углублена до -300, точки на -290 (spaces.pit, loot.special[0]); две сломанные точки убраны'},
    {'bug': 'точка в кровати мыши', 'fix': 'убрана, вместо неё точка на полу комнаты (3900, 5350, 5)'},
    {'bug': 'точка под основанием вентилятора', 'fix': 'убрана'},
    {'bug': 'точка внутри мешка мусора', 'fix': 'перенесена к мешку (5900, -300, 5)'},
    {'bug': 'еда проваливается сквозь корзину посудомойки', 'fix': 'не лечится раскладкой: точки оставлены, нужен пол корзины в модели или точки на z=540 (см. LEVEL_V2_INTEGRATION.md)'},
    {'bug': 'кошка внутри малыша, кошка внутри бабушки, Бадрик на мяче', 'fix': 'новые точки отдыха (pets, spawns): ни одна не совпадает'},
    {'bug': 'нет краёв карты', 'fix': 'заборы и невидимые блоки по всем краям (walls, kind fence / blocker)'},
    {'bug': 'Человек упирается в стену, целясь в муравья снаружи', 'fix': 'цель только внутри дома (human.targeting_bounds, нужен код)'},
    {'bug': 'ботинок на путевой точке двери', 'fix': 'ботинок перенесён к обувнице в гостиной'},
    {'bug': 'тостер — бесконечная еда', 'fix': 'правило: не больше двух тостов на столешнице и 40 с тишины (нужен код)'},
    {'bug': 'в щели гнезда и в щели под окном не было пола (плита кончалась у стены, газон начинался за стеной): боты падали под мир и гибли', 'fix': 'в толще стены положен пол (KitchenHouse.cpp); так же в северном лазе'},
    {'bug': 'ложка — «наклонный путь», но уклон 65°', 'fix': 'помечена как путь только наверх; волоком — трасса, метла, книжка, плед, лесенки'},
]
by_tier = {}
for s in D['loot']['spots']: by_tier[str(s['tier_loot'])] = by_tier.get(str(s['tier_loot']), 0) + 1
D['counts'] = {'art_instances': len([o for o in objects if o.get('mesh')]), 'code_boxes': len([o for o in objects if not o.get('mesh')]), 'movers': len([o for o in objects if o['role'] == 'mover']),
               'containers': len(D['containers']), 'connectors': len(D['connectors']), 'surfaces': len(D['surfaces']), 'spawns': len(D['spawns']), 'loot_spots': len(D['loot']['spots']),
               'loot_spots_by_tier': by_tier, 'heavy_spots': len(D['loot']['heavy']), 'trophy_spots': len(D['loot']['trophies']), 'behind_door_spots': len(D['loot']['behind_doors']),
               'main_spots': len(main['spots']), 'interactables': len(D['interactables']), 'hazards': len(D['hazards']), 'events': len(D['events']), 'secrets': len(D['secrets']),
               'districts': len(D['districts']), 'graph_nodes': len(V.NODES), 'graph_edges': len(edges), 'hide_boxes': len(hide), 'new_meshes': len(NP['pieces']), 'new_instances': n_new,
               'new_triangles': NP['totals']['triangles'], 'copies': len(COPIES), 'removed': len(changes['removed']), 'moved': len(changes['moved']), 'loot_clamped': len(clamped)}
D['meta'].update({'title': 'Ant Kitchen: level v2 (in the game since 2026-11-20)', 'generated': time.strftime('%Y-%m-%d %H:%M'), 'tool': 'Tools/level/build_level_v2.py',
                  'from': 'docs/level/v1/level_data.json (%s, the v1 code as extracted) + Tools/level/level_v2_layout.py + SourceArt/Blender/level_v2/level_v2_new_pieces.json' % D1['meta']['generated'],
                  'mode': 'the level as it is in the game: the placement table (KitchenArtData.inl, gen_level_v2_art.py --install) and the gameplay tables (LevelV2Data.inl, '
                          'gen_level_v2_code.py) are generated from this file; the constants that live in the C++ are checked against it by verify_level_v2_code.py'})
D['meta']['exactness']['v2 (design)'] = 'everything marked v2 = new / moved / fixed comes from level_v2_layout.py; boxes of new meshes are exact (Blender manifest), boxes of moved meshes are the v1 boxes shifted'
with open(os.path.join(LV, 'level_v2.json'), 'w', encoding='utf-8', newline='\n') as fo:
    json.dump(D, fo, ensure_ascii=False, separators=(',', ':'))
# The same data is "the level as it is" now: level_data.json (extract_level.py transcribed the v1 tables by hand and does not
# read the v2 code; its last output is frozen in docs/level/v1). audit_level.py and build_level_map.py read this file.
with open(os.path.join(LV, 'level_data.json'), 'w', encoding='utf-8', newline='\n') as fo:
    json.dump(D, fo, ensure_ascii=False, indent=1)

# ---- the unified art manifest of v2 (kitchen-manifest schema): old pieces where they stand now + the new ones -----------------------------
KM = json.load(open(os.path.join(ART, 'kitchen', 'kitchen_manifest.json'), encoding='utf-8'))
LM = json.load(open(os.path.join(ART, 'living', 'living_manifest.json'), encoding='utf-8'))
PM = {p['name']: (p, 'kitchen') for p in KM['pieces']}
PM.update({p['name']: (p, 'living') for p in LM['pieces']})
K = 1.0 / 1900.0
OBJV2 = {o['id']: o for o in objects}
def inst_json(loc, ax, ay, az, scale, o):
    d = {'game_loc_uu': [R1(v) for v in loc], 'game_axis_x': [round(v, 6) for v in ax], 'game_axis_y': [round(v, 6) for v in ay], 'game_axis_z': [round(v, 6) for v in az], 'scale': scale,
         'blender_loc_m': [round(-loc[0] * K, 5), round(loc[1] * K, 5), round(loc[2] * K, 5)],
         'blender_rot_3x3': [[round(-ax[0], 6), round(ay[0], 6), round(-az[0], 6)], [round(ax[1], 6), round(-ay[1], 6), round(az[1], 6)], [round(ax[2], 6), round(-ay[2], 6), round(az[2], 6)]]}
    if abs(az[2] - 1) < 1e-6: d['game_yaw_deg'] = round(math.degrees(math.atan2(ax[1], ax[0])), 3)
    if o: d['game_bbox_min_uu'] = [o['bb'][0], o['bb'][1], o['z'][0]]; d['game_bbox_max_uu'] = [o['bb'][2], o['bb'][3], o['z'][1]]; d['object'] = o['id']
    return d
MOV_BY_INST = {m['inst']: (k, m) for k, m in enumerate(MOV)}
pieces, order = {}, []
def piece_of(name):
    if name not in pieces:
        src, folder = PM.get(name, ({}, 'living'))
        e = {'name': name, 'file': '../%s/%s.fbx' % (folder, name), 'set': folder, 'triangles': src.get('triangles'), 'materials': src.get('materials') or src.get('slots'), 'instances': []}
        for k in ('origin', 'bbox_size_m', 'bbox_min_m', 'bbox_max_m', 'bbox_size_game_uu', 'size_m', 'note'):
            if k in src: e[k] = src[k]
        pieces[name] = e; order.append(name)
    return pieces[name]
movers_out = []
def add_row(n, row, t, scale=None, index=None, obj=None, role=None):
    loc = (t.pt(row['loc']) if t else list(row['loc'])); ax, ay, az = ((t.vec(row['ax']), t.vec(row['ay']), t.vec(row['az'])) if t else (row['ax'], row['ay'], row['az']))
    sc = [row['scale'][k] * (scale[k] if scale else 1.0) for k in range(3)]
    e = piece_of(row['name'])
    d = inst_json(loc, ax, ay, az, sc, obj)
    d['role'] = row['role'] if role is None else role
    d['index'] = row['index'] if index is None else index
    if n is not None and n in MOV_BY_INST and role is None:
        k, m = MOV_BY_INST[n]
        d['mover'] = {'type': 'hinge' if m['type'] == 0 else 'slide', 'flags': m['flags'], 'auto_close_s': m['auto'], 'open_text': m['open'], 'close_text': m['close'], 'v1_mover': k}
        if m['type'] == 0: d['hinge_axis_game'] = [round(v, 5) for v in (t.vec(m['axis']) if t else m['axis'])]; d['open_angle_game_deg'] = m['angle']
        else: d['slide_game_uu'] = [R1(v) for v in (t.vec(m['slide']) if t else m['slide'])]
    if row['role'] == 3 and role is None: d['rides_v1_mover'] = row['parent']
    e['instances'].append(d)
removed_rows = set(int(i[3:]) for i in V.REMOVE if i.startswith('art'))
for n, row in enumerate(ROWS):
    if n in removed_rows: continue
    add_row(n, row, ROW_T.get(n), obj=OBJV2.get('art%d' % n))
for (n, t, sc, idx, oid) in COPIES:
    add_row(None, ROWS[n], t, sc, idx, OBJV2[oid], role=0)
for e in NP['pieces']:
    q = dict(e); q['set'] = 'level_v2'
    for i, ins in enumerate(q['instances']):
        ins['role'] = 1 if e['name'] in NEW_MOVERS else 0
        if e['name'] in NEW_MOVERS:
            nm = NEW_MOVERS[e['name']]
            ins['mover'] = {'type': 'hinge', 'flags': nm['flags'], 'auto_close_s': nm['auto'], 'open_text': nm['open'], 'close_text': nm['close']}
    pieces[e['name']] = q; order.append(e['name'])
doc = {'what': 'Ant Kitchen level v2: every placed mesh of the redesigned level in the kitchen-manifest schema (instances with game_loc_uu and game axes). Old pieces keep their FBX '
               '(file points into ../kitchen or ../living); new pieces are in this folder. Written by Tools/level/build_level_v2.py.',
       'conventions': NP['conventions'], 'materials': NP['materials'],
       'roles': '0 static, 1 generic mover (instance.mover has flags / prompts; hinge_axis_game + open_angle_game_deg or slide_game_uu), 2 built by a gadget actor, 3 rides a mover',
       'totals': {'pieces': len(order), 'placed_instances': sum(len(pieces[n]['instances']) for n in order), 'new_pieces': len(NP['pieces']), 'new_triangles': NP['totals']['triangles']},
       'pieces': [pieces[n] for n in order]}
with open(os.path.join(ART, 'level_v2', 'level_v2_manifest.json'), 'w', encoding='utf-8', newline='\n') as fo:
    json.dump(doc, fo, ensure_ascii=False, indent=1)
print('wrote level_v2.json: objects %d (removed %d, moved %d, copies %d, new %d), loot %d (by tier %s, clamped %d), hide boxes %d' % (
    len(objects), len(changes['removed']), len(changes['moved']), len(COPIES), n_new, len(D['loot']['spots']), by_tier, len(clamped), len(hide)))
print('wrote level_v2_manifest.json: %d pieces, %d instances' % (doc['totals']['pieces'], doc['totals']['placed_instances']))
for h in D['loot']['heavy']: print('heavy  %5.1f s  %s' % (h['t_nest'], h['p']))
for m in main['spots']: print('main   %5.1f s  %s %s' % (m['t_nest'], m['id'], m['p']))
print('trophies under 25 s:', [(s['i'], s['t_nest']) for s in D['loot']['trophies'] if s['t_nest'] < 25])
print('clamped:', [(c['from'], c['to'], c['t'], [round(v) for v in c['p']]) for c in clamped][:40])