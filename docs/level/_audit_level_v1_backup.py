# -*- coding: utf-8 -*-
# Ant Kitchen, level map, step 2: docs/level/level_data.json -> docs/level/level_audit.json + docs/level/LEVEL_AUDIT.md
#   python Tools/level/audit_level.py
# Pure python, a few seconds. Nothing here touches the game.
#
# What is computed (all of it "derived", see the notes written into LEVEL_AUDIT.md):
#   - a 100 uu grid of the whole level: where the Human can stand (capsule radius 340 against bounding boxes),
#     how far every floor cell is from such a place (his reach), travel time from the nest (walls block, furniture does not);
#   - a 1 m (1900 uu) grid per area and tier with what is in every cell, and its class: dead / transit / contested / safe;
#   - dead zones, stretches without cover, loot that is too easy or pointless, ways up per surface, the main routes.
import json, os, sys, math, heapq, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
OUT = None
for _i, _a in enumerate(sys.argv):
    if _a == '--root': ROOT = sys.argv[_i + 1]
    if _a == '--out': OUT = sys.argv[_i + 1]
OUT = OUT or os.path.join(ROOT, 'docs', 'level')
D = json.load(open(os.path.join(OUT, 'level_data.json'), encoding='utf-8'))

UU = 1900.0
G = 100.0
GX0, GY0, GX1, GY1 = -16600.0, -16100.0, 21100.0, 6100.0
NX, NY = int((GX1 - GX0) / G), int((GY1 - GY0) / G)
ANT = D['constants']['ant']
V_SPRINT, V_WALK = ANT['sprint_uu_s'], ANT['walk_uu_s']
WALL_K = ANT['wall_factor']
HUM = D['human']
CAP_R = HUM['capsule']['radius']; STEP_H = HUM['capsule']['step_height']; HUM_H = 2 * HUM['capsule']['half_height']
STOMP_REACH = HUM['attacks']['stomp']['reach_from_centre']
SLAP_REACH = HUM['attacks']['slap']['ai_uses_between_2d'][1]
NEWS_R = HUM['attacks']['newspaper']['range_from_shoulder'] + HUM['attacks']['newspaper']['hit_radius']
SHOULDER_Z = HUM['capsule']['half_height'] + 960.0
EYE_Z = HUM['eye_height_assumed']
AREA_RU = {'kitchen': 'Кухня', 'living': 'Гостиная', 'yard': 'Двор', 'street': 'Улица'}
TIER_RU = {'floor': 'пол', 'work': 'столешницы и столы', 'top': 'верха и полки', 'int': 'внутри', 'vent': 'вентиляция и за стенами'}
CLS_RU = {'dead': 'мёртвая', 'transit': 'транзит', 'contested': 'спорная', 'safe': 'безопасная'}

def cx(i): return GX0 + (i + 0.5) * G
def cy(j): return GY0 + (j + 0.5) * G
def ci(x): return max(0, min(NX - 1, int((x - GX0) // G)))
def cj(y): return max(0, min(NY - 1, int((y - GY0) // G)))

# ---- regions: 0 solid / void, 1 kitchen, 2 living room, 3 yard, 4 street side, 5 mouse room -------------------------
def region_cell(x, y):
    if -7000 < x < 7000 and -5000 < y < 5000: return 1
    if 7200 < x < 20200 and -9000 < y < 1500: return 2
    if 7000 <= x <= 7200 and -4800 < y < -3200: return 1 if x < 7100 else 2
    if -7250 < x <= -7000 and -2750 < y < -2250: return 4
    if -4600 < x < -4400 and -5250 < y <= -5000: return 3
    if 3500 < x < 4300 and 5050 < y < 5550: return 5
    if 3800 < x < 3960 and 5000 <= y <= 5100: return 5
    if (-12000 < x < 7000 and -16000 < y < -5200) or (7000 <= x < 21000 and -16000 < y < -9200): return 3
    if -16500 < x < -7200 and -5200 <= y < 5600: return 4
    return 0
REG = [[region_cell(cx(i), cy(j)) for i in range(NX)] for j in range(NY)]
AREA_OF_REG = {1: 'kitchen', 2: 'living', 3: 'yard', 4: 'street', 5: 'kitchen'}

# fences an ant has to climb over (2600 up, 2600 down) unless it uses the gap
FENCE_S = 2 * 2600.0 / (V_SPRINT * WALL_K)
EXTRA = {}
for j in range(NY):
    y = cy(j)
    if -5200 < y < 5600 and not (3340 < y < 4640):
        EXTRA[(ci(-9450), j)] = FENCE_S            # the street fence along x = -9400, the gate stands open
for i in range(NX):
    x = cx(i)
    if -12000 < x < -9400 and not (-10800 < x < -10000):
        EXTRA[(i, cj(-5250))] = FENCE_S            # the yard's north fence, with its gap

def dijkstra(src_cells, speed):
    INF = 1e18
    T = [[INF] * NX for _ in range(NY)]
    PAR = [[None] * NX for _ in range(NY)]
    pq = []
    for (i, j) in src_cells:
        T[j][i] = 0.0; heapq.heappush(pq, (0.0, i, j))
    step = G / speed
    # 16 directions (the 8 neighbours and the knight moves): paths are within ~3 % of the straight-line length
    MOVES = [(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1), (2, 1), (2, -1), (-2, 1), (-2, -1), (1, 2), (1, -2), (-1, 2), (-1, -2)]
    while pq:
        t, i, j = heapq.heappop(pq)
        if t > T[j][i]: continue
        for di, dj in MOVES:
            a, b = i + di, j + dj
            if a < 0 or b < 0 or a >= NX or b >= NY or REG[b][a] == 0: continue
            if abs(di) == 2:
                s1 = di // 2
                if REG[j][i + s1] == 0 or REG[b][i + s1] == 0 or (i + s1, j) in EXTRA or (i + s1, b) in EXTRA: continue
            elif abs(dj) == 2:
                s1 = dj // 2
                if REG[j + s1][i] == 0 or REG[j + s1][a] == 0 or (i, j + s1) in EXTRA or (a, j + s1) in EXTRA: continue
            elif di and dj and (REG[j][a] == 0 or REG[b][i] == 0): continue
            nt = t + step * math.hypot(di, dj) + EXTRA.get((a, b), 0.0)
            if nt < T[b][a]:
                T[b][a] = nt; PAR[b][a] = (i, j); heapq.heappush(pq, (nt, a, b))
    return T, PAR

def chamfer(seed):
    """Distance (uu) from every cell to the nearest seed cell (two-pass 3-4 chamfer, about 3 % off the true distance)."""
    INF = 1e9
    d = [[0.0 if seed[j][i] else INF for i in range(NX)] for j in range(NY)]
    a, b = G, G * 1.41421
    for j in range(NY):
        row = d[j]; up = d[j - 1] if j > 0 else None
        for i in range(NX):
            v = row[i]
            if v == 0.0: continue
            if i > 0 and row[i - 1] + a < v: v = row[i - 1] + a
            if up:
                if up[i] + a < v: v = up[i] + a
                if i > 0 and up[i - 1] + b < v: v = up[i - 1] + b
                if i < NX - 1 and up[i + 1] + b < v: v = up[i + 1] + b
            row[i] = v
    for j in range(NY - 1, -1, -1):
        row = d[j]; dn = d[j + 1] if j < NY - 1 else None
        for i in range(NX - 1, -1, -1):
            v = row[i]
            if v == 0.0: continue
            if i < NX - 1 and row[i + 1] + a < v: v = row[i + 1] + a
            if dn:
                if dn[i] + a < v: v = dn[i] + a
                if i > 0 and dn[i - 1] + b < v: v = dn[i - 1] + b
                if i < NX - 1 and dn[i + 1] + b < v: v = dn[i + 1] + b
            row[i] = v
    return d

def paint(mask, bb, val=True):
    for j in range(cj(bb[1] + 1), cj(bb[3] - 1) + 1):
        y = cy(j)
        if not (bb[1] <= y <= bb[3]): continue
        row = mask[j]
        for i in range(ci(bb[0] + 1), ci(bb[2] - 1) + 1):
            if bb[0] <= cx(i) <= bb[2]: row[i] = val

def pose_bb(o):
    return o['bb_open'] if (o.get('pose') == 'open' and 'bb_open' in o) else o['bb']

OBJ = D['objects']
# ---- where the Human can stand ---------------------------------------------------------------------------------------
# His step height is 250. Whether he gets over things 250..450 high (the shoe, the newspaper, the tennis ball, the rolling pin...)
# depends on their triangles, which are not known here: they are NOT counted as obstacles, so his reach is never under-estimated.
LOW_CLUTTER = 450.0
LOW = []
solid = [[REG[j][i] not in (1, 2) for i in range(NX)] for j in range(NY)]
for o in OBJ:
    if not o.get('solid') or o.get('role') == 'rider' or o.get('mesh') in ('SM_L_DoorFrame', 'SM_K_Window'): continue
    if o['z'][0] < HUM_H and o['z'][1] > LOW_CLUTTER:
        paint(solid, pose_bb(o))
    elif o['z'][0] < 60 and o['z'][1] > STEP_H and o['area'] in ('kitchen', 'living') and o['cat'] != 'struct':
        LOW.append(o['ru'])
d_solid = chamfer(solid)
can = [[(REG[j][i] in (1, 2)) and d_solid[j][i] >= CAP_R + G * 0.5 for i in range(NX)] for j in range(NY)]
# keep what is connected to his spawn point
stand = [[False] * NX for _ in range(NY)]
st = [(ci(HUM['spawn'][0]), cj(HUM['spawn'][1]))]
if can[st[0][1]][st[0][0]]: stand[st[0][1]][st[0][0]] = True
while st:
    i, j = st.pop()
    for a, b in ((i + 1, j), (i - 1, j), (i, j + 1), (i, j - 1)):
        if 0 <= a < NX and 0 <= b < NY and can[b][a] and not stand[b][a]:
            stand[b][a] = True; st.append((a, b))
DH = chamfer(stand)     # how far a cell is from the nearest place he can stand

# ---- what the AI never targets, and the cover level of every floor cell ---------------------------------------------
HIDDEN = [h for h in HUM['sees']['never_targets'] if 'rect' in h]
def hidden_at(x, y, z):
    for h in HIDDEN:
        r = h['rect']
        if r[0] < x < r[2] and r[1] < y < r[3] and h['z'][0] <= z < h['z'][1]: return True
    return False
NEST = [s for s in D['spawns'] if s['id'] == 'nest0'][0]
def in_nest(x, y): return math.hypot(x - NEST['p'][0], y - NEST['p'][1]) < NEST['radius']

# cover level of a floor cell: 2 = nothing of his reaches (or he never looks there), 1 = only the hand slap reaches, 0 = open
COVER = [[-1] * NX for _ in range(NY)]
for j in range(NY):
    for i in range(NX):
        r = REG[j][i]
        if r == 0: continue
        x, y = cx(i), cy(j)
        if r in (3, 4, 5) or hidden_at(x, y, 5.0) or in_nest(x, y): COVER[j][i] = 2
        elif DH[j][i] > SLAP_REACH: COVER[j][i] = 2
        elif DH[j][i] > STOMP_REACH: COVER[j][i] = 1
        else: COVER[j][i] = 0

def news_reach(x, y, z):
    """Can the rolled newspaper land here (AI rules: no line of sight, range from the shoulder)?"""
    d = DH[cj(y)][ci(x)]
    return d * d + (z - SHOULDER_Z) ** 2 <= NEWS_R * NEWS_R

# ---- travel time from where the colony's ants appear ---------------------------------------------------------------
SPAWN = [s for s in D['spawns'] if s['id'] == 'spawn0'][0]['p']
TF, PAR = dijkstra([(ci(SPAWN[0]), cj(SPAWN[1]))], V_SPRINT)
def t_at(x, y, z=0.0):
    t = TF[cj(y)][ci(x)]
    if t > 1e17:   # inside a wall (a duct, a shelf on a wall...): take the nearest open cell
        best = 1e18
        for dj in range(-4, 5):
            for di in range(-4, 5):
                a, b = ci(x) + di, cj(y) + dj
                if 0 <= a < NX and 0 <= b < NY and TF[b][a] < 1e17:
                    best = min(best, TF[b][a] + math.hypot(di, dj) * G / V_SPRINT)
        t = best
    ground = -225.0 if x < -11100 else 0.0
    return t + abs(z - ground) / (V_SPRINT * WALL_K)

# ---- everything that can be picked up, in one list ------------------------------------------------------------------
LOOT = []
for s in D['loot']['spots']:
    LOOT.append({'id': 'L%d' % s['i'], 'kind': 'spot', 'p': s['p'], 'tier': s['tier'], 'area': s['area'], 'ev': s['ev'], 'lvl': s['tier_loot'], 'door': bool(s.get('behind_door')),
                 'where': s.get('where_ru') or s.get('under') or '', 'label': s['label'] or s['group'], 'dog': bool(s.get('dog_eats')), 'cont': s.get('container'), 'space': s.get('space')})
for s in D['loot']['behind_doors']:
    LOOT.append({'id': 'D%d' % s['i'], 'kind': 'door', 'p': s['p'], 'tier': 'int', 'area': s['area'], 'ev': s['ev'], 'lvl': s['tier_loot'], 'door': True, 'where': s.get('where_ru', ''), 'label': s['label'], 'cont': s.get('container')})
for s in D['loot']['heavy']:
    LOOT.append({'id': 'H%d' % s['i'], 'kind': 'heavy', 'p': s['p'], 'tier': s['tier'] if s['tier'] != 'under' else 'floor', 'area': s['area'], 'ev': s['ev'], 'lvl': 3, 'door': False,
                 'where': s.get('under', ''), 'name': 'Тяжёлый приз: %s (%d, вес %d)' % (s['first_item'].get('ru', ''), s['first_item'].get('value', 0), s['first_item'].get('weight', 0)), 'label': s['label']})
# Three trophies exist at a time: two of the spots inside containers, one of the spots in the open. A spot's value is weighted by that.
_tin = sum(1 for s in D['loot']['trophies'] if s.get('container')); _tout = len(D['loot']['trophies']) - _tin
for s in D['loot']['trophies']:
    pres = (2.0 / _tin) if s.get('container') else (1.0 / _tout)
    LOOT.append({'id': 'T%d' % s['i'], 'kind': 'trophy', 'p': s['p'], 'tier': s['tier'], 'area': s['area'], 'ev': round(s['ev'] * pres, 1), 'nominal': s['ev'], 'present': round(pres, 2), 'lvl': 3,
                 'door': bool(s.get('behind_door')) or s.get('space') == 'pedal_bin', 'where': s.get('where_ru', ''), 'name': 'Трофей: %s (%d, целым x2 = %d, вес %d)' % (s['ru'], s['value'], s['ev'], s['weight']),
                 'label': s['label'], 'cont': s.get('container'), 'space': s.get('space')})
for n, s in enumerate(D['loot']['special']):
    z = s['p'][2]
    LOOT.append({'id': 'S%d' % n, 'kind': 'special', 'p': s['p'], 'tier': 'vent' if z < 0 else ('int' if n == 1 else ('floor' if z < 300 else 'work')), 'area': 'living' if s['p'][0] > 7100 else 'kitchen',
                 'ev': s['value'], 'lvl': 2, 'door': n == 1, 'where': {0: 'в яме под плиткой', 1: 'тумба с тремя ящиками'}.get(n, ''), 'name': s['ru'], 'label': '',
                 'cont': 'TopDrawer' if n == 1 else None, 'space': 'pit' if n == 0 else None})
for l in LOOT:
    l['t'] = round(t_at(l['p'][0], l['p'][1], l['p'][2]), 1)
    x, y, z = l['p']
    if l['tier'] in ('floor', 'under'):
        l['cover'] = COVER[cj(y)][ci(x)] if REG[cj(y)][ci(x)] else 2
    elif l['tier'] in ('work', 'top'):
        l['cover'] = 2 if (l['area'] in ('yard', 'street') or hidden_at(x, y, z + 20) or not news_reach(x, y, z)) else 0
    else:
        l['cover'] = 2

# ---- the 1 m grid ----------------------------------------------------------------------------------------------------
M = UU
def overlap(a, b):
    w = min(a[2], b[2]) - max(a[0], b[0]); h = min(a[3], b[3]) - max(a[1], b[1])
    return w * h if w > 0 and h > 0 else 0.0
def circ_rect(c, r):
    px = min(max(c[0], r[0]), r[2]); py = min(max(c[1], r[1]), r[3])
    return math.hypot(c[0] - px, c[1] - py) <= c[2]
def shape_hits(h, r):
    return circ_rect(h['circle'], r) if 'circle' in h else overlap(h['rect'], r) > 0

GAPS = [o for o in OBJ if 'gap_under' in o]
FLOORSTAND = [o for o in OBJ if o.get('solid') and o['z'][0] < 60 and o['z'][1] > 300 and 'gap_under' not in o and o['cat'] in ('furn', 'prop', 'conn', 'food') and o.get('mesh') != 'SM_Y_Tree' and not o.get('tilted')]
under = [[False] * NX for _ in range(NY)]
for o in GAPS: paint(under, o['bb'])
occ = [[False] * NX for _ in range(NY)]
for o in FLOORSTAND: paint(occ, o['bb'])
LAND = [o for o in OBJ if o['cat'] == 'furn' or (o['cat'] in ('prop', 'conn', 'food') and (o['bb'][2] - o['bb'][0]) * (o['bb'][3] - o['bb'][1]) >= 250000 and o.get('mesh') != 'SM_Y_Tree')]
LAND = [o for o in LAND if not str(o.get('mesh') or '').startswith(('SM_L_Book_', 'SM_K_Plate', 'SM_L_Sofa_'))]
TREE = [o for o in OBJ if o.get('mesh') == 'SM_Y_Tree']
INTER = [i for i in D['interactables'] if not (i['kind'] == 'push' and i['id'].startswith('junk'))]
HAZ = D['hazards']
CONN = D['connectors']
SURF = D['surfaces']

def tier_of_z(z): return 'floor' if z < 300 else ('work' if z < 2100 else 'top')

# a Russian "where" for every piece of loot: its container / hideout / the surface it lies on / the nearest big thing
def place_ru(l):
    if l.get('where'): return l['where']
    x, y, z = l['p']
    if l['tier'] == 'vent': return {'corridor': 'в коридоре за шкафами', 'mouse_room': 'в комнате мыши', 'pit': 'в яме под плиткой', 'duct1': 'в вентиляции 1', 'duct2': 'в вентиляции 2'}.get(l.get('space'), 'за стеной')
    if l['tier'] in ('work', 'top'):
        for s in SURF:
            if abs(z - s['z']) <= 320 and any(r[0] <= x <= r[2] and r[1] <= y <= r[3] for r in s['rects']): return 'на: ' + s['ru']
        best = min(((math.hypot(x - o['c'][0], y - o['c'][1]), o['ru']) for o in OBJ if o['z'][0] - 60 <= z <= o['z'][1] + 60 and o['bb'][0] - 60 <= x <= o['bb'][2] + 60 and o['bb'][1] - 60 <= y <= o['bb'][3] + 60 and o['cat'] != 'struct'), default=None)
        if best: return 'на: ' + best[1]
    near = min(((max(o['bb'][0] - x, x - o['bb'][2], 0.0) + max(o['bb'][1] - y, y - o['bb'][3], 0.0), o['ru']) for o in LAND if o['area'] == l['area'] and (o['z'][0] < 300 or 'gap_under' in o)), default=None)
    if near and near[0] < 150: return ('под: ' if l['tier'] == 'under' else ('на полу у: ' if z < 300 else 'у: ')) + near[1]
    return ('%s%s м от: %s' % ('на полу, ' if z < 300 else '', ('%.1f' % (near[0] / UU)).replace('.', ','), near[1])) if near else ('на полу' if z < 300 else 'на высоте %d' % z)
LVL_RU = {0: 'крошка', 1: 'еда ур. 1', 2: 'еда ур. 2', 3: 'еда ур. 3'}
for l in LOOT:
    l['place'] = place_ru(l)
    if 'name' not in l: l['name'] = LVL_RU[l['lvl']] + (' за дверью' if l['kind'] == 'door' else '')

CELLS = []
def make_cells():
    for area in D['areas']:
        aid = area['id']
        xs = [r[0] for r in area['rects']] + [r[2] for r in area['rects']]; ys = [r[1] for r in area['rects']] + [r[3] for r in area['rects']]
        ax0, ay0, ax1, ay1 = min(xs), min(ys), max(xs), max(ys)
        nx, ny = int(math.ceil((ax1 - ax0) / M)), int(math.ceil((ay1 - ay0) / M))
        regs = {'kitchen': (1,), 'living': (2,), 'yard': (3,), 'street': (4,)}[aid]
        for cjj in range(ny):
            for cii in range(nx):
                r = [ax0 + cii * M, ay0 + cjj * M, min(ax1, ax0 + (cii + 1) * M), min(ay1, ay0 + (cjj + 1) * M)]
                fine = [(i, j) for j in range(cj(r[1] + 1), cj(r[3] - 1) + 1) for i in range(ci(r[0] + 1), ci(r[2] - 1) + 1) if REG[j][i] in regs and r[0] <= cx(i) < r[2] and r[1] <= cy(j) < r[3]]
                if fine:
                    walk = [(i, j) for (i, j) in fine if not occ[j][i]]
                    c = {'area': aid, 'tier': 'floor', 'ci': cii, 'cj': cjj, 'rect': r, 'm2': round(len(walk) * G * G / (M * M), 2), 'm2_all': round(len(fine) * G * G / (M * M), 2)}
                    if walk:
                        c['open'] = round(sum(1 for (i, j) in walk if COVER[j][i] == 0) / len(walk), 2)
                        c['slap_only'] = round(sum(1 for (i, j) in walk if COVER[j][i] == 1) / len(walk), 2)
                        c['cover'] = round(sum(1 for (i, j) in walk if COVER[j][i] == 2) / len(walk), 2)
                        c['under'] = round(sum(1 for (i, j) in walk if under[j][i]) / len(walk), 2)
                        c['t'] = round(sum(TF[j][i] for (i, j) in walk) / len(walk), 1)
                        mx = sum(cx(i) for (i, j) in walk) / len(walk); my = sum(cy(j) for (i, j) in walk) / len(walk)
                        c['pt'] = [round(mx), round(my), 10]
                        CELLS.append(c)
                for tier in ('work', 'top'):
                    parts = []
                    for s in SURF:
                        if s['tier'] != tier or s['area'] != aid: continue
                        for sr in s['rects']:
                            a = overlap(sr, r)
                            if a > 0: parts.append((s, sr, a))
                    if not parts: continue
                    c = {'area': aid, 'tier': tier, 'ci': cii, 'cj': cjj, 'rect': r, 'surfaces': sorted(set(p[0]['ru'] for p in parts)), 'zs': sorted(set(p[0]['z'] for p in parts))}
                    best = max(parts, key=lambda p: p[2])
                    c['m2'] = round(sum(min(p[2], M * M) for p in parts if p[0]['z'] == best[0]['z']) / (M * M), 2)
                    br = [max(best[1][0], r[0]), max(best[1][1], r[1]), min(best[1][2], r[2]), min(best[1][3], r[3])]
                    px, py, pz = (br[0] + br[2]) / 2, (br[1] + br[3]) / 2, best[0]['z']
                    c['pt'] = [round(px), round(py), pz + 10]
                    n = k = h = 0
                    for a in range(5):
                        for b in range(5):
                            sx = br[0] + (a + 0.5) * (br[2] - br[0]) / 5; sy = br[1] + (b + 0.5) * (br[3] - br[1]) / 5
                            n += 1
                            if aid in ('yard', 'street') or hidden_at(sx, sy, pz + 20): h += 1
                            elif news_reach(sx, sy, pz): k += 1
                    c['open'] = round(k / n, 2); c['cover'] = round(1 - k / n, 2); c['hidden'] = round(h / n, 2); c['slap_only'] = 0.0; c['under'] = 0.0
                    c['t'] = round(t_at(px, py, pz), 1)
                    CELLS.append(c)
make_cells()

def in_rect(p, r): return r[0] <= p[0] < r[2] and r[1] <= p[1] < r[3]
for c in CELLS:
    r, tier = c['rect'], c['tier']
    floor = tier == 'floor'
    c['loot'] = [l['id'] for l in LOOT if in_rect(l['p'], r) and l['area'] == c['area'] and ((l['tier'] in ('floor', 'under')) if floor else (l['tier'] == tier))]
    ls = [l for l in LOOT if l['id'] in c['loot']]
    c['ev'] = round(sum(l['ev'] for l in ls))
    c['crumbs'] = sum(1 for l in ls if l['lvl'] == 0)
    c['has_loot'] = any(l['lvl'] >= 1 for l in ls) or c['crumbs'] >= 2
    c['inter'] = [i['id'] for i in INTER if in_rect(i['p'], r) and tier_of_z(i['p'][2]) == tier]
    c['conn'] = sorted(set(k['id'] for k in CONN for p in (k['pts'][0], k['pts'][-1]) if in_rect(p, r) and tier_of_z(p[2]) == tier and k['area'] == c['area']))
    c['haz'] = [h['id'] for h in HAZ if h['area'] == c['area'] and shape_hits(h, r) and ((h['z'][0] < 300) if floor else (tier_of_z(max(h['z'][0], 300)) == tier or (h['z'][0] < 2100 <= h['z'][1] and tier == 'top'))) and h['kind'] != 'rival']
    if floor:
        c['land'] = sorted(set(o['ru'] for o in LAND if o['area'] == c['area'] and (o['z'][0] < 300 or 'gap_under' in o) and overlap(o['bb'], r) >= min(0.05 * M * M, 0.5 * (o['bb'][2] - o['bb'][0]) * (o['bb'][3] - o['bb'][1]))))
        for o in TREE:
            if circ_rect([o['origin'][0], o['origin'][1], 520], r): c['land'].append(o['ru'])
        for op in D['openings']:
            b = op['box']
            if b[2] < 300 and overlap([b[0] - 60, b[1] - 60, b[3] + 60, b[4] + 60], r) > 0: c['land'].append(op['ru'])
        for sp in D['spawns']:
            if sp['kind'] in ('nest', 'nest_rival') and circ_rect([sp['p'][0], sp['p'][1], sp['radius']], r): c['land'].append(sp['ru'])
    else:
        c['land'] = list(c['surfaces'])

# ---- routes a player would actually run ------------------------------------------------------------------------------
ROUTE_DEFS = [
    ('r1', 'Полка с мёдом над гнездом', (-6780, -650, 2680)),
    ('r2', 'Обеденный стол: торт, тарелка', (200, -600, 1400)),
    ('r3', 'Западная столешница: тостер, ящики, шкафы', (-5700, 3300, 1710)),
    ('r4', 'Плита, духовка, микроволновка', (-750, 3300, 1740)),
    ('r5', 'Холодильник (дверь; верх на 3700)', (3700, 2900, 0)),
    ('r6', 'Мусорный угол: педаль ведра', (6000, -2300, 0)),
    ('r7', 'Гостиная: под диван', (13600, 700, 0)),
    ('r8', 'Гостиная: тумба ТВ', (13300, -7650, 0)),
    ('r9', 'Двор: уличный бак (кратчайший путь — через кухню и щель под окном)', (4200, -11950, 2790)),
]
def rdp(pts, eps):
    if len(pts) < 3: return pts
    (x0, y0), (x1, y1) = pts[0], pts[-1]
    dx, dy = x1 - x0, y1 - y0; n = math.hypot(dx, dy) or 1.0
    k, best = 0, -1.0
    for i in range(1, len(pts) - 1):
        d = abs((pts[i][0] - x0) * dy - (pts[i][1] - y0) * dx) / n
        if d > best: k, best = i, d
    if best <= eps: return [pts[0], pts[-1]]
    return rdp(pts[:k + 1], eps)[:-1] + rdp(pts[k:], eps)

ROUTES = []
USE = {}
for rid, ru, tgt in ROUTE_DEFS:
    i, j = ci(tgt[0]), cj(tgt[1])
    cells = []
    while (i, j) is not None and PAR[j][i] is not None:
        cells.append((i, j)); i, j = PAR[j][i]
    cells.append((i, j)); cells.reverse()
    length = 0.0; exposed = 0.0; slap = 0.0
    for a in range(1, len(cells)):
        (i0, j0), (i1, j1) = cells[a - 1], cells[a]
        d = math.hypot(i1 - i0, j1 - j0) * G; length += d
        cv = COVER[j1][i1]
        if cv == 0: exposed += d / V_SPRINT
        elif cv == 1: slap += d / V_SPRINT
    for c in set(cells): USE[c] = USE.get(c, 0) + 1
    climb = abs(tgt[2])
    t_floor = TF[cells[-1][1]][cells[-1][0]]
    t_climb = climb / (V_SPRINT * WALL_K)
    tx, ty, tz = tgt
    climb_open = bool(climb) and REG[cj(ty)][ci(tx)] in (1, 2) and news_reach(tx, ty, tz / 2.0) and not hidden_at(tx, ty, tz / 2.0)
    k = V_SPRINT / V_WALK
    ROUTES.append({'id': rid, 'ru': ru, 'target': list(tgt), 'pts': [[round(x), round(y)] for (x, y) in rdp([(cx(i), cy(j)) for (i, j) in cells], 70.0)], 'cells': cells,
                   'len_m': round((length + climb) / UU, 1), 'floor_m': round(length / UU, 1), 'climb_m': round(climb / UU, 2),
                   't_sprint': round(t_floor + t_climb, 1), 't_walk': round((t_floor + t_climb) * k, 1),
                   't_back_loaded': [round((t_floor + t_climb) * k / 0.9, 1), round((t_floor + t_climb) * k / 0.5, 1)],
                   'exposed_s': round(exposed + (t_climb if climb_open else 0.0), 1), 'slap_only_s': round(slap, 1), 'climb_exposed': climb_open})
ROUTE_CELLS = set()
for r in ROUTES:
    for (i, j) in r['cells']: ROUTE_CELLS.add((i, j))
# where the routes part: length of the common start of every pair
SPLITS = []
for a in range(len(ROUTES)):
    for b in range(a + 1, len(ROUTES)):
        ca, cb = ROUTES[a]['cells'], ROUTES[b]['cells']
        near_b = set((i + di, j + dj) for (i, j) in cb for di in range(-4, 5) for dj in range(-4, 5))
        n = 0; dist = 0.0
        while n < len(ca) and ca[n] in near_b:      # together = closer than ~0.2 m to each other
            if n: dist += math.hypot(ca[n][0] - ca[n - 1][0], ca[n][1] - ca[n - 1][1]) * G
            n += 1
        last = ca[max(0, n - 1)]
        SPLITS.append({'a': ROUTES[a]['id'], 'b': ROUTES[b]['id'], 'shared_m': round(dist / UU, 1), 'at': [round(cx(last[0])), round(cy(last[1]))]})
for c in CELLS:
    r = c['rect']
    c['route'] = c['tier'] == 'floor' and any(r[0] <= cx(i) < r[2] and r[1] <= cy(j) < r[3] for (i, j) in ROUTE_CELLS)
for r in ROUTES:
    tx, ty, tz = r['target']
    if tz >= 300:
        for c in CELLS:
            if c['area'] in ('kitchen', 'living', 'yard') and c['tier'] == tier_of_z(tz) and in_rect((tx, ty), c['rect']): c['route'] = True

# ---- line of sight from the Human's eye height (geometry only: the AI does not use it) -------------------------------
OCC = []
for o in OBJ:
    if not o.get('solid') or o.get('role') == 'rider' or o['cat'] in ('struct', 'deco') or o.get('mesh') in ('SM_Y_Tree',): continue
    b = pose_bb(o); a = (b[2] - b[0]) * (b[3] - b[1]); z0 = o.get('gap_under', o['z'][0]); h = o['z'][1] - z0
    if h < 60 or not (a >= 250000 or (h >= 800 and a >= 20000)): continue
    OCC.append((b[0], b[1], z0, b[2], b[3], o['z'][1], o['area']))
def seg_hits(p, q, bx):
    t0, t1 = 0.0, 1.0
    for k in range(3):
        d = q[k] - p[k]; lo, hi = bx[k], bx[k + 3]
        if abs(d) < 1e-9:
            if p[k] < lo or p[k] > hi: return False
        else:
            a, b = (lo - p[k]) / d, (hi - p[k]) / d
            if a > b: a, b = b, a
            if a > t0: t0 = a
            if b < t1: t1 = b
            if t0 > t1: return False
    return t1 - t0 > 1e-6 and t0 < 0.985     # a hit only at the very end is the surface the point lies on
EYES = {'kitchen': [], 'living': []}
for j in range(0, NY, 12):
    for i in range(0, NX, 12):
        if stand[j][i]: EYES[AREA_OF_REG[REG[j][i]]].append((cx(i), cy(j), EYE_Z))
for c in CELLS:
    if c['area'] not in EYES or not EYES[c['area']]: continue
    occ_a = [b for b in OCC if b[6] == c['area']]
    p = c['pt']; n = v = 0
    for e in EYES[c['area']]:
        n += 1
        if not any(seg_hits(e, p, b) for b in occ_a): v += 1
    c['vis'] = round(v / n, 2)

# ---- classes ---------------------------------------------------------------------------------------------------------
for c in CELLS:
    floor = c['tier'] == 'floor'
    has_inter = bool(c['inter'])
    strong = c['ev'] >= 50 or has_inter          # worth a trip: a tier-2 piece, two tier-1 pieces, or something to press
    weak = c['ev'] >= 14                         # small change: one tier-1 piece or two crumbs
    has_do = strong or weak or bool(c['conn'])
    has_fear = bool(c['haz'])
    has_see = bool(c['land']) if floor else False
    human = c['open'] >= 0.4
    cover = c['cover'] >= 0.6
    if not has_do and not has_fear and not has_see and not c['route']: cls = 'dead'
    elif strong and (human or has_fear): cls = 'contested'
    elif cover and not has_fear: cls = 'safe'
    else: cls = 'transit'
    c['cls'] = cls
    c['why'] = {'loot': strong, 'small_loot': weak and not strong, 'use': has_inter, 'way': bool(c['conn']), 'fear': has_fear, 'see': has_see, 'route': c['route'], 'human': human, 'cover': cover}

# ---- closed spaces: containers, hideouts, ducts ----------------------------------------------------------------------
UNITS = []
def add_unit(uid, ru, tier, box, area, extra):
    ls = [l for l in LOOT if (l.get('cont') == uid or l.get('space') == uid)]
    hz = [h['id'] for h in HAZ if h['area'] == area and shape_hits(h, [box[0], box[1], box[3], box[4]]) and h['z'][0] < box[5] and h['z'][1] > box[2] and h['kind'] not in ('rival', 'pet', 'wind')]
    u = {'id': uid, 'ru': ru, 'tier': tier, 'area': area, 'box': box, 'm2': round((box[3] - box[0]) * (box[4] - box[1]) / (M * M), 2), 'loot': [l['id'] for l in ls], 'ev': round(sum(l['ev'] for l in ls)),
         'haz': hz, 't': round(t_at((box[0] + box[3]) / 2, (box[1] + box[4]) / 2, max(0, box[2])), 1)}
    u.update(extra)
    fear = bool(hz) or bool(extra.get('trap'))
    u['cls'] = 'dead' if not ls else ('contested' if fear else 'safe')
    UNITS.append(u)
for c in D['containers']:
    add_unit(c['id'], 'Контейнер: ' + c['where_ru'], 'int', c['box'], 'living' if c['box'][0] > 7100 else 'kitchen', {'need_strength': c['need_strength'], 'trap': c['trap'], 'doors': [d.get('ru') or d['actor'] for d in c['doors']]})
for it in D['interiors']:
    add_unit(it['id'], 'Укрытие: ' + it['ru'], 'int', it['box'], 'yard' if it['box'][1] < -9200 else ('living' if it['box'][0] > 7100 else 'kitchen'), {'entry': it['entry']})
for sp in D['spaces']:
    bx = sp.get('box')
    if not bx:
        bs = list(sp['boxes'].values()); bx = [min(b[0] for b in bs), min(b[1] for b in bs), min(b[2] for b in bs), max(b[3] for b in bs), max(b[4] for b in bs), max(b[5] for b in bs)]
    add_unit(sp['id'], sp['ru'], 'vent', bx, 'kitchen', {})

# ---- totals ----------------------------------------------------------------------------------------------------------
TOT = {}
for c in CELLS:
    k = c['area'] + '/' + c['tier']
    t = TOT.setdefault(k, {'area': c['area'], 'tier': c['tier'], 'm2': 0.0, 'dead': 0.0, 'transit': 0.0, 'contested': 0.0, 'safe': 0.0, 'cells': 0, 'ev': 0})
    t['m2'] += c['m2']; t[c['cls']] += c['m2']; t['cells'] += 1; t['ev'] += c['ev']
for t in TOT.values():
    for k in ('dead', 'transit', 'contested', 'safe'): t[k + '_pct'] = round(100.0 * t[k] / t['m2']) if t['m2'] else 0
    for k in ('m2', 'dead', 'transit', 'contested', 'safe'): t[k] = round(t[k], 1)

# ---- dead zones: joined dead cells -----------------------------------------------------------------------------------
def rect_dist(a, b):
    dx = max(a[0] - b[2], b[0] - a[2], 0.0); dy = max(a[1] - b[3], b[1] - a[3], 0.0)
    return math.hypot(dx, dy)
ZONES = []
seen = set()
idx = {(c['area'], c['tier'], c['ci'], c['cj']): c for c in CELLS}
for c in CELLS:
    key = (c['area'], c['tier'], c['ci'], c['cj'])
    if c['cls'] != 'dead' or key in seen: continue
    comp = []; st = [key]; seen.add(key)
    while st:
        a, t, i, j = st.pop(); comp.append(idx[(a, t, i, j)])
        for n in ((a, t, i + 1, j), (a, t, i - 1, j), (a, t, i, j + 1), (a, t, i, j - 1)):
            if n in idx and idx[n]['cls'] == 'dead' and n not in seen: seen.add(n); st.append(n)
    bb = [min(k['rect'][0] for k in comp), min(k['rect'][1] for k in comp), max(k['rect'][2] for k in comp), max(k['rect'][3] for k in comp)]
    near = sorted(((rect_dist(bb, o['bb']), o['ru']) for o in LAND + TREE if o['area'] == c['area'] and (o['z'][0] < 300 or 'gap_under' in o)), key=lambda v: v[0])
    names = []
    for d, n in near:
        if n not in names: names.append(n)
        if len(names) == 2: break
    ar = [a for a in D['areas'] if a['id'] == c['area']][0]
    xs = [r[0] for r in ar['rects']] + [r[2] for r in ar['rects']]; ys = [r[1] for r in ar['rects']] + [r[3] for r in ar['rects']]
    walls = [w for w, ok in (('западной', bb[0] <= min(xs) + 1), ('восточной', bb[2] >= max(xs) - 1), ('южной', bb[1] <= min(ys) + 1), ('северной', bb[3] >= max(ys) - 1)) if ok]
    ZONES.append({'area': c['area'], 'tier': c['tier'], 'cells': len(comp), 'm2': round(sum(k['m2'] for k in comp), 1), 'bb': [round(v) for v in bb], 'w_m': round((bb[2] - bb[0]) / M, 1), 'h_m': round((bb[3] - bb[1]) / M, 1),
                  'near': names, 'walls': walls, 't': round(min(k['t'] for k in comp), 1), 'surfaces': sorted(set(s for k in comp for s in k.get('surfaces', [])))})
ZONES.sort(key=lambda z: -z['m2'])

# ---- longest runs without cover (distance along the floor to the nearest cover, walls respected) --------------------
seeds = [(i, j) for j in range(NY) for i in range(NX) if COVER[j][i] == 2 and not occ[j][i] and (REG[j][i] in (1, 2, 5) or (REG[j][i] == 4 and cx(i) > -7300) or (REG[j][i] == 3 and cy(j) > -5300 and -4700 < cx(i) < -4300))]
DC, _p = dijkstra(seeds, 1.0)
NOCOVER = []
for reg in (1, 2):
    cand = sorted(((DC[j][i], i, j) for j in range(NY) for i in range(NX) if REG[j][i] == reg and COVER[j][i] < 2 and not occ[j][i] and DC[j][i] < 1e17), reverse=True)
    picked = []
    for d, i, j in cand:
        if all(math.hypot(cx(i) - cx(a), cy(j) - cy(b)) > 2500 for (_d, a, b) in picked):
            picked.append((d, i, j))
        if len(picked) == 3: break
    tot = sum(1 for j in range(NY) for i in range(NX) if REG[j][i] == reg and not occ[j][i])
    opn = sum(1 for j in range(NY) for i in range(NX) if REG[j][i] == reg and not occ[j][i] and COVER[j][i] == 0)
    slp = sum(1 for j in range(NY) for i in range(NX) if REG[j][i] == reg and not occ[j][i] and COVER[j][i] == 1)
    NOCOVER.append({'area': AREA_OF_REG[reg], 'open_pct': round(100.0 * opn / tot), 'slap_only_pct': round(100.0 * slp / tot), 'cover_pct': round(100.0 * (tot - opn - slp) / tot),
                    'worst': [{'at': [round(cx(i)), round(cy(j))], 'to_cover_m': round(d / UU, 2), 's_sprint': round(d / V_SPRINT, 1), 's_walk': round(d / V_WALK, 1), 's_loaded': [round(d / (V_WALK * 0.9), 1), round(d / (V_WALK * 0.5), 1)]} for (d, i, j) in picked]})

# ---- loot that is too easy / loot nobody would go for ----------------------------------------------------------------
BROKEN = {'L165': 'стоит внутри пола ямы', 'L166': 'стоит внутри пола ямы', 'S0': 'стоит внутри пола ямы', 'L106': 'внутри кровати мыши', 'L45': 'под основанием вентилятора'}
EASY = sorted([l for l in LOOT if l['ev'] >= 60 and l['t'] <= 25 and l['id'] not in BROKEN], key=lambda l: -l['ev'] / max(1.0, l['t']))
NOGO = []
for l in LOOT:
    why = []
    if l['id'] in BROKEN: why.append(BROKEN[l['id']])
    if l.get('dog'): why.append('Бадрик съедает еду в этой зоне')
    if l['ev'] < 30 and l['t'] >= 45: why.append('дёшево (%.0f) и далеко (%.0f с бегом)' % (l['ev'], l['t']))
    if why: NOGO.append({'id': l['id'], 'p': l['p'], 'ev': l['ev'], 't': l['t'], 'area': l['area'], 'tier': l['tier'], 'label': l['label'], 'why': why})
NOGO.sort(key=lambda n: n['ev'] / max(1.0, n['t']))

def loot_on(s):
    return [l for l in LOOT if l['tier'] in ('work', 'top') and abs(l['p'][2] - s['z']) <= 320 and any(in_rect(l['p'], r) for r in s['rects'])]
def inter_on(s):
    return [i for i in INTER if -50 <= i['p'][2] - s['z'] <= 500 and any(in_rect(i['p'], r) for r in s['rects'])]
conn_ids = {c['id']: c for c in CONN}
SURFACES = []
for s in SURF:
    ls, ins = loot_on(s), inter_on(s)
    designed = [a for a in s['aids'] if a in conn_ids]
    SURFACES.append({'id': s['id'], 'ru': s['ru'], 'area': s['area'], 'tier': s['tier'], 'z': s['z'], 'm2': round(sum((r[2] - r[0]) * (r[3] - r[1]) for r in s['rects']) / (M * M), 2), 'ev': round(sum(l['ev'] for l in ls)),
                     'loot': len(ls), 'inter': [i['ru'] for i in ins], 'designed_ways': [conn_ids[a]['ru'] for a in designed], 'other_ways': [a for a in s['aids'] if a not in conn_ids],
                     'empty': not ls and not ins, 'ways': len(designed)})
EMPTY_TIERS = [t for t in TOT.values() if t['ev'] == 0]

# ---- files -----------------------------------------------------------------------------------------------------------
def band(t): return 'a' if t <= 10 else ('b' if t <= 20 else ('c' if t <= 40 else ('d' if t <= 60 else ('e' if t <= 90 else 'f'))))
grid = {'x0': GX0, 'y0': GY0, 'g': G, 'nx': NX, 'ny': NY,
        'iso': [''.join('.' if REG[j][i] == 0 else band(TF[j][i]) for i in range(NX)) for j in range(NY)],
        'cover': [''.join('.' if REG[j][i] == 0 else ('o' if REG[j][i] in (3, 4) else str(COVER[j][i])) for i in range(NX)) for j in range(NY)],
        'stand': [''.join('1' if stand[j][i] else '0' for i in range(NX)) for j in range(NY)],
        'legend': {'iso': 'a <=10 s, b <=20, c <=40, d <=60, e <=90, f more (sprint, unladen, floor level)', 'cover': '0 open (stomp reaches), 1 only the hand slap reaches, 2 cover, o outdoors (no Human), . solid'}}
for r in ROUTES: del r['cells']
for c in CELLS: c['rect'] = [round(v) for v in c['rect']]
audit = {
    'meta': {'generated': time.strftime('%Y-%m-%d %H:%M'), 'tool': 'Tools/level/audit_level.py', 'from': 'docs/level/level_data.json (' + D['meta']['generated'] + ')',
             'speeds_uu_s': {'walk': V_WALK, 'sprint': V_SPRINT, 'wall_factor': WALL_K}, 'origin': SPAWN, 'cell_m': 1.0, 'fine_cell_uu': G,
             'method': 'time = shortest floor path at sprint speed (walls block, furniture does not) + vertical climb at wall speed; Human standing places from bounding boxes; all values derived'},
    'totals': sorted(TOT.values(), key=lambda t: (['kitchen', 'living', 'yard', 'street'].index(t['area']), ['floor', 'work', 'top'].index(t['tier']))),
    'cells': CELLS, 'units': UNITS, 'dead_zones': ZONES, 'nocover': NOCOVER, 'easy_loot': EASY[:12], 'nogo_loot': NOGO, 'surfaces': SURFACES,
    'routes': ROUTES, 'splits': SPLITS, 'loot': LOOT, 'grid': grid}
with open(os.path.join(OUT, 'level_audit.json'), 'w', encoding='utf-8', newline='\n') as fo:
    json.dump(audit, fo, ensure_ascii=False, separators=(',', ':'))

def n(v, d=1): return (('%.' + str(d) + 'f') % v).replace('.', ',')
def xy(p): return '(%d, %d)' % (p[0], p[1])
L = []
W = L.append
A = {a['id']: a for a in D['areas']}
def area_m2(a): return sum((r[2] - r[0]) * (r[3] - r[1]) for r in A[a]['rects']) / (M * M)
TOTALS = audit['totals']
W('# Аудит уровня Ant Kitchen — как есть сейчас')
W('')
W('Собрано %s скриптом `Tools/level/audit_level.py` из `docs/level/level_data.json`. Карта: `docs/level/level_map.html`. Игра не запускалась: всё посчитано по коду и таблицам, в игре не проверено.' % audit['meta']['generated'])
W('')
W('## Как считалось')
W('')
W('- Масштаб 1 м = 1900 uu. Кухня %s × %s м (%s м²), гостиная %s × %s м (%s м²), двор %s м², улица %s м².' % (n(14000 / M), n(10000 / M), n(area_m2('kitchen'), 0), n(13000 / M), n(10500 / M), n(area_m2('living'), 0), n(area_m2('yard'), 0), n(area_m2('street'), 0)))
W('- Муравей: шаг %d uu/с, бег (Shift, без груза) %d uu/с, по стене ×%s. С грузом 0,5–0,9 от шага, без бега и прыжка. Все времена — бегом без груза от точки появления %s; шагом ×%s, назад с грузом ×%s–%s.' % (V_WALK, V_SPRINT, n(WALL_K, 2), xy(SPAWN), n(V_SPRINT / V_WALK, 2), n(V_SPRINT / V_WALK / 0.9, 1), n(V_SPRINT / V_WALK / 0.5, 1)))
W('- Время = кратчайший путь по полу (стены мешают, мебель нет) + подъём по вертикали со скоростью стены. Это оценка снизу.')
W('- Человек: капсула радиусом %d; где он может встать — по габаритам мебели. Топот достаёт на %d uu от его центра и на 220 над полом; ладонь — до %d; газета — %d от плеча (плечо на %d над полом), видимость не проверяется; дихлофос 2300–4025, нужен прямой луч.' % (CAP_R, STOMP_REACH, SLAP_REACH, NEWS_R, SHOULDER_Z))
W('- Укрытие на полу: не достаёт ни топот, ни ладонь, либо ИИ туда не смотрит (севернее y=4750, под холодильником, под мойкой, закрытые контейнеры, гнездо). Во дворе и на улице Человека нет.')
W('- Предметы высотой 250–450 (шаг Человека 250) препятствием для него не считались — неизвестно, перешагивает ли он их: %s.' % ', '.join(sorted(set(LOW))))
W('- Ценность точки = средняя ценность её уровня (крошка %s, ур. 1 — %s, ур. 2 — %s, ур. 3 — %s); трофей — с учётом того, что их одновременно три.' % tuple(n(t['expected_value'], 0) for t in D['loot']['food']['tiers']))
W('- Клетка 1 × 1 м. **Мёртвая**: еды меньше чем на 14 (одна крошка), нет кнопки, нет пути наверх, нет опасности, нет предмета, не на маршруте. **Спорная**: еды на 50 и больше или есть кнопка — и достаёт Человек либо есть местная опасность. **Безопасная**: укрытие на 60 %% клетки и без опасностей. **Транзит**: остальное, в том числе мелочь дешевле 50 на открытом месте.' % ())
W('')
W('## Сколько мёртвого')
W('')
W('| Зона | Ярус | м² | мёртвая | транзит | спорная | безопасная |')
W('|---|---|---|---|---|---|---|')
for t in TOTALS:
    W('| %s | %s | %s | **%d %%** | %d %% | %d %% | %d %% |' % (AREA_RU[t['area']], TIER_RU[t['tier']], n(t['m2']), t['dead_pct'], t['transit_pct'], t['contested_pct'], t['safe_pct']))
W('')
dead_units = [u for u in UNITS if u['cls'] == 'dead']
W('Закрытых мест (контейнеры, укрытия, вентиляция) %d; без еды: %s.' % (len(UNITS), ', '.join(u['ru'].split(': ', 1)[-1] for u in dead_units) or 'нет'))
for a in ('kitchen', 'living'):
    fc = [c for c in CELLS if c['area'] == a and c['tier'] == 'floor' and 'vis' in c]
    if fc: W('%s: с высоты глаз Человека (%d) плохо видно (меньше чем с четверти мест, где он может стоять) %d %% клеток пола. ИИ этим не пользуется: он «видит» сквозь мебель.' % (AREA_RU[a], EYE_Z, round(100.0 * sum(c['m2'] for c in fc if c['vis'] < 0.25) / sum(c['m2'] for c in fc))))
W('')
W('## 10 худших мёртвых зон')
W('')
EDGE = {'kitchen': 'стены', 'living': 'стены', 'yard': 'забора', 'street': 'края'}
GROUND = {'kitchen': 'пол', 'living': 'пол', 'yard': 'газон', 'street': 'земля'}
def zone_place(z):
    tot = [t for t in TOTALS if t['area'] == z['area'] and t['tier'] == z['tier']][0]
    if z['tier'] != 'floor': return ', '.join(z['surfaces'])
    if z['m2'] > 0.5 * tot['m2']: return 'почти весь %s, от края до края' % GROUND[z['area']]
    s = GROUND[z['area']]
    if z['near']: s += ' между «%s»' % '» и «'.join(z['near']) if len(z['near']) > 1 else ' у «%s»' % z['near'][0]
    if z['walls']:
        ws = z['walls'] if z['area'] in ('kitchen', 'living') else [w[:-2] + 'ого' if w.endswith('ой') else w for w in z['walls']]
        s += ', у %s %s' % (' и '.join(ws), EDGE[z['area']])
    return s
for k, z in enumerate(ZONES[:10], 1):
    z['text'] = zone_place(z)
    W('%d. **%s, %s** — %s: %s × %s м, %s м² (x %d..%d, y %d..%d). Ничего нет. От гнезда %s с.' % (k, AREA_RU[z['area']], TIER_RU[z['tier']], z['text'], n(z['w_m']), n(z['h_m']), n(z['m2']), z['bb'][0], z['bb'][2], z['bb'][1], z['bb'][3], n(z['t'], 0)))
for z in ZONES[10:]: z['text'] = zone_place(z)
W('')
W('## Где негде спрятаться')
W('')
for r in NOCOVER:
    W('- **%s**: открыто %d %% пола, ещё %d %% достаёт только ладонь, укрытие %d %%.' % (AREA_RU[r['area']], r['open_pct'], r['slap_only_pct'], r['cover_pct']))
    for w in r['worst']:
        W('  - от %s до ближайшего укрытия %s м: %s с бегом, %s с шагом, %s–%s с с грузом.' % (xy(w['at']), n(w['to_cover_m']), n(w['s_sprint']), n(w['s_walk']), n(w['s_loaded'][0]), n(w['s_loaded'][1])))
W('')
W('## Еда: слишком легко и никому не нужно')
W('')
W('Слишком легко (дорого и близко):')
W('')
for l in EASY[:8]:
    risk = 'Человек не достаёт' if l['cover'] == 2 else ('достаёт только ладонь' if l['cover'] == 1 else 'Человек достаёт')
    W('- %s, %s — %s с от гнезда, %s%s. %s.' % (l['name'], l['place'], n(l['t'], 0), xy(l['p']), (', z=%d' % l['p'][2]) if l['p'][2] else '', risk + ('; за дверью' if l['door'] else '')))
W('')
bro = [x for x in NOGO if x['id'] in BROKEN]; dog = [x for x in NOGO if any('Бадрик' in w for w in x['why'])]; far = [x for x in NOGO if any('далеко' in w for w in x['why'])]
W('Никто не пойдёт — %d точек из %d:' % (len(NOGO), len(LOOT)))
W('')
W('- сломаны %d: %s;' % (len(bro), '; '.join('%s %s' % (xy(x['p']), x['why'][0]) for x in bro)))
W('- в зоне, где еду съедает Бадрик (миска, мусорка, ботинок, мука): %d;' % len(dog))
W('- дешевле 30 и дальше 45 с бегом: %d (кухня %d, гостиная %d, двор и улица %d).' % (len(far), sum(1 for x in far if x['area'] == 'kitchen'), sum(1 for x in far if x['area'] == 'living'), sum(1 for x in far if x['area'] in ('yard', 'street'))))
W('')
W('## Куда незачем лезть')
W('')
if EMPTY_TIERS: W('Ярусы без еды вообще: ' + '; '.join('%s — %s (%s м²)' % (AREA_RU[t['area']], TIER_RU[t['tier']], n(t['m2'])) for t in EMPTY_TIERS) + '.')
emp = [s for s in SURFACES if s['empty']]
W('Поверхности без еды и без кнопок, %d из %d: %s.' % (len(emp), len(SURFACES), '; '.join('%s (z=%d, %s м²)' % (s['ru'], s['z'], n(s['m2'])) for s in emp)))
W('')
W('## Пути наверх')
W('')
W('Муравей лезет по любой стене и фасаду, поэтому тупиков нет. Вопрос в задуманных путях (полотенце, ложка, нитка, вентиляция):')
W('')
hi = [s for s in SURFACES if s['z'] >= 400]
W('- **нет ни одного** (только голый фасад или стена): ' + '; '.join('%s (z=%d)' % (s['ru'], s['z']) for s in hi if s['ways'] == 0) + '.')
W('- **один**: ' + '; '.join('%s — %s' % (s['ru'], s['designed_ways'][0].split(':')[0]) for s in hi if s['ways'] == 1) + '.')
W('- **два и больше**: ' + '; '.join('%s (%d)' % (s['ru'], s['ways']) for s in hi if s['ways'] > 1) + '.')
W('- Наклонный путь (по нему можно тащить волоком) один на всю карту: ложка к обеденному столу. Открытые дверцы духовки и посудомойки дают полки у пола, но это тяжёлые двери.')
W('')
W('## Маршруты')
W('')
W('| № | Куда | Длина, м | Бегом, с | Шагом, с | Назад с грузом, с | На виду, с |')
W('|---|---|---|---|---|---|---|')
for r in ROUTES:
    W('| %s | %s | %s | %s | %s | %s–%s | %s |' % (r['id'][1:], r['ru'], n(r['len_m']), n(r['t_sprint'], 0), n(r['t_walk'], 0), n(r['t_back_loaded'][0], 0), n(r['t_back_loaded'][1], 0), n(r['exposed_s'], 0)))
W('')
W('«На виду» — секунды бегом по клеткам, где достаёт топот. Назад с грузом это время в 1,8–3,2 раза больше.')
W('')
W('Все маршруты выходят через одну щель в западной стене (%s м от точки появления) и сразу за ней расходятся веером. Дольше всего идут вместе:' % n(math.hypot(SPAWN[0] + 6950, SPAWN[1] + 2500) / UU))
W('')
for s in sorted(SPLITS, key=lambda s: -s['shared_m'])[:5]:
    W('- %s и %s: %s м, до %s.' % (s['a'][1:], s['b'][1:], n(s['shared_m']), xy(s['at'])))
W('')
W('## Похоже на баги')
W('')
for s in D['suspicious']:
    if s['kind'] in ('loot inside solid', 'loot falls', 'open edge', 'ai', 'balance', 'logic', 'npc overlap', 'loot inside a box', 'docs'): W('- %s' % s['ru'])
W('')
W('## Что бросается в глаза')
W('')
fl = {t['area']: t for t in TOTALS if t['tier'] == 'floor'}
ev_in = sum(l['ev'] for l in LOOT if l['area'] in ('kitchen', 'living')); ev_out = sum(l['ev'] for l in LOOT if l['area'] in ('yard', 'street'))
all_m2 = sum(area_m2(a) for a in A)
W('1. Кухня: мёртвого пола %d %%, но %d %% — транзит: по нему только бегут. Спорных клеток %d %%.' % (fl['kitchen']['dead_pct'], fl['kitchen']['transit_pct'], fl['kitchen']['contested_pct']))
W('2. Гостиная: мёртвого пола %d %%, спорного %d %%. Туда %s с бегом в одну сторону, а еды на полу и столах меньше, чем на кухне.' % (fl['living']['dead_pct'], fl['living']['contested_pct'], n(ROUTES[6]['t_sprint'], 0)))
W('3. Двор и улица — %s м² из %s (%d %% карты), еды там %d %% от всей. Человека там нет: ни риска, ни награды. Двор мёртв на %d %%.' % (n(area_m2('yard') + area_m2('street'), 0), n(all_m2, 0), round(100 * (area_m2('yard') + area_m2('street')) / all_m2), round(100.0 * ev_out / max(1.0, ev_in + ev_out)), fl['yard']['dead_pct']))
if EASY: W('4. Самая лёгкая дорогая еда: %s, %s — %s с бегом от гнезда.' % (EASY[0]['name'], EASY[0]['place'], n(EASY[0]['t'], 0)))
W('5. Спрятаться негде: укрытие — %d %% пола кухни и %d %% гостиной. До ближайшего укрытия бывает %s с бегом, с грузом до %s с.' % (NOCOVER[0]['cover_pct'], NOCOVER[1]['cover_pct'], n(max(NOCOVER[0]['worst'][0]['s_sprint'], NOCOVER[1]['worst'][0]['s_sprint'])), n(max(NOCOVER[0]['worst'][0]['s_loaded'][1], NOCOVER[1]['worst'][0]['s_loaded'][1]), 0)))
W('6. В дом один вход — щель в западной стене; за ней все маршруты расходятся, общих узлов внутри почти нет. Кратчайший путь во двор — тоже через кухню.')
W('7. Северная стена кухни (севернее y=4750) — слепая зона ИИ на любой высоте; блуждая, Человек не подходит к столешницам ближе 400 uu.')
W('8. Пустых поверхностей %d из %d; наклонный путь наверх один (ложка); тостер даёт еду бесконечно; яма под плиткой сломана.' % (len(emp), len(SURFACES)))
W('')
with open(os.path.join(OUT, 'LEVEL_AUDIT.md'), 'w', encoding='utf-8', newline='\n') as fo:
    fo.write('\n'.join(L))
audit['summary'] = {'floor_dead_pct': {a: fl[a]['dead_pct'] for a in fl}, 'floor_transit_pct': {a: fl[a]['transit_pct'] for a in fl}, 'floor_contested_pct': {a: fl[a]['contested_pct'] for a in fl},
                    'cover_pct': {r['area']: r['cover_pct'] for r in NOCOVER}, 'worst_run_s': {r['area']: r['worst'][0]['s_sprint'] for r in NOCOVER}, 'loot_points': len(LOOT), 'nogo_points': len(NOGO),
                    'empty_surfaces': len(emp), 'surfaces': len(SURFACES), 'outdoor_area_pct': round(100 * (area_m2('yard') + area_m2('street')) / all_m2), 'outdoor_loot_pct': round(100.0 * ev_out / max(1.0, ev_in + ev_out)),
                    'observations': [l[3:] for l in L[-9:-1]]}
audit['low_clutter'] = sorted(set(LOW))
with open(os.path.join(OUT, 'level_audit.json'), 'w', encoding='utf-8', newline='\n') as fo:
    json.dump(audit, fo, ensure_ascii=False, separators=(',', ':'))
print('wrote level_audit.json and LEVEL_AUDIT.md: cells %d, units %d, dead zones %d, routes %d' % (len(CELLS), len(UNITS), len(ZONES), len(ROUTES)))
for t in TOTALS: print('%-8s %-6s %6.1f m2  dead %3d%% transit %3d%% contested %3d%% safe %3d%%  ev %d' % (t['area'], t['tier'], t['m2'], t['dead_pct'], t['transit_pct'], t['contested_pct'], t['safe_pct'], t['ev']))
for r in ROUTES: print(r['id'], r['len_m'], 'm', r['t_sprint'], 's sprint, exposed', r['exposed_s'], 'slap', r['slap_only_s'])
