# -*- coding: utf-8 -*-
# Ant Kitchen, level v2: checks of docs/level/level_v2.json against the audit grids, and the before / after numbers.
#   python Tools/level/audit_level.py                              (v1)
#   python Tools/level/audit_level.py --data level_v2.json          (v2, with the hide table)
#   python Tools/level/audit_level.py --data level_v2.json --no-hide   (v2, what the code gives today: no hide table)
#   python Tools/level/check_level_v2.py     -> docs/level/level_v2_check.json + a summary on the console
# Checked: the Human can stand at and walk between his patrol points and through the door; nothing new blocks a door or a drawer;
# nothing new floats; no loot inside a solid; rest points of the creatures do not overlap; the route graph (length, time, seconds
# in the Human's reach); cells further than 4 s from cover; dead cells; ways per surface; the loot gradient.
import json, os, sys, math
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE)); LV = os.path.join(ROOT, 'docs', 'level')
try: sys.stdout.reconfigure(encoding='utf-8', errors='replace')
except Exception: pass
def load(n):
    p = os.path.join(LV, n)
    return json.load(open(p, encoding='utf-8')) if os.path.exists(p) else None
D1, A1, D2, A2, A2N = load('v1/level_data.json'), load('v1/level_audit.json'), load('level_v2.json'), load('level_audit_v2.json'), load('level_audit_v2_nohide.json')   # v1 is frozen in docs/level/v1
UU = 1900.0
ANT = D2['constants']['ant']; VS, VW, WK = ANT['sprint_uu_s'], ANT['walk_uu_s'], ANT['wall_factor']
OUT = {'problems': [], 'notes': []}
def bad(kind, text, **kw): OUT['problems'].append(dict(kind=kind, text=text, **kw))

class Grid:
    def __init__(self, A):
        g = A['grid']; self.x0, self.y0, self.g, self.nx, self.ny = g['x0'], g['y0'], g['g'], g['nx'], g['ny']; self.A = g
    def ij(self, x, y): return max(0, min(self.nx - 1, int((x - self.x0) // self.g))), max(0, min(self.ny - 1, int((y - self.y0) // self.g)))
    def at(self, name, x, y):
        i, j = self.ij(x, y); return self.A[name][j][i]
G2 = Grid(A2)

# ---- 1. the Human ---------------------------------------------------------------------------------------------------------------
H = D2['human']
def stand_near(x, y, r=150):
    for dx in range(-r, r + 1, 50):
        for dy in range(-r, r + 1, 50):
            if G2.at('stand', x + dx, y + dy) == '1': return True
    return False
pts = [('spawn', H['spawn'][:2]), ('flee', H['flee_point'][:2])] + [('door%d' % k, p[:2]) for k, p in enumerate(H['patrol']['door_waypoints'])] + [('patrol%d' % k, p) for k, p in enumerate(H['patrol']['points'])]
hum = []
for name, p in pts:
    ok = stand_near(p[0], p[1])
    hum.append({'point': name, 'p': p, 'can_stand': ok})
    if not ok: bad('human', 'he cannot stand at %s %s' % (name, p))
def box_share(b):
    n = k = 0
    for x in range(int(b[0]) + 50, int(b[2]), 100):
        for y in range(int(b[1]) + 50, int(b[3]), 100):
            n += 1; k += G2.at('stand', x, y) == '1'
    return round(100.0 * k / max(1, n))
st = A2['grid']['stand']
OUT['human'] = {'points': hum, 'stand_cells': {'kitchen': sum(r.count('1') for r in st) if False else None},
                'wander_box_standable_pct': {'kitchen': box_share(H['patrol']['kitchen_box']), 'living': box_share(H['patrol']['living_box'])},
                'note': 'the audit keeps only the places connected to his spawn point at capsule radius 340 (+50 for the grid), so "can stand" also means "can walk there"'}
def area_stand(A, rect):
    g = Grid(A); n = 0
    for j, row in enumerate(A['grid']['stand']):
        y = g.y0 + (j + .5) * g.g
        if rect[1] < y < rect[3]:
            for i, c in enumerate(row):
                if c == '1' and rect[0] < g.x0 + (i + .5) * g.g < rect[2]: n += 1
    return round(n * g.g * g.g / (UU * UU), 1)
OUT['human']['stand_m2'] = {'v1': {'kitchen': area_stand(A1, [-7000, -5000, 7000, 5000]), 'living': area_stand(A1, [7200, -9000, 20200, 1500])},
                            'v2': {'kitchen': area_stand(A2, [-7000, -5000, 7000, 5000]), 'living': area_stand(A2, [7200, -9000, 20200, 1500])}}

# ---- 2. creatures: rest points must not overlap ------------------------------------------------------------------------------------
rest = [(s['id'], s['p']) for s in D2['spawns'] if s['kind'] in ('dog_spot', 'cat_spot', 'npc')]
mn = (1e9, None, None)
for a in range(len(rest)):
    for b in range(a + 1, len(rest)):
        d = math.dist(rest[a][1], rest[b][1])
        if d < mn[0]: mn = (d, rest[a][0], rest[b][0])
        if d < 400: bad('pets', 'rest points %s and %s are %d uu apart' % (rest[a][0], rest[b][0], d))
OUT['pets'] = {'closest_pair': [mn[1], mn[2]], 'distance_uu': round(mn[0])}
for sid, p in rest:
    for o in D2['objects']:
        if o.get('mesh') in ('SM_K_TennisBall', 'SM_Y_DogBall') and o['bb'][0] - 100 < p[0] < o['bb'][2] + 100 and o['bb'][1] - 100 < p[1] < o['bb'][3] + 100 and abs(p[2] - o['z'][0]) < 300:
            bad('pets', '%s rests on %s' % (sid, o['id']))

# ---- 3. nothing floats: every new, copied or moved thing stands on the floor, on something, or hangs on a wall or a neighbour -------
OBJ = D2['objects']
WALLS = [w['rect'] for w in D2['walls']]
SURF = [(s['z'], r) for s in D2['surfaces'] for r in s['rects']]
def ov(a, b): return max(0.0, min(a[2], b[2]) - max(a[0], b[0])) * max(0.0, min(a[3], b[3]) - max(a[1], b[1]))
def gap(a, b): return max(a[0] - b[2], b[0] - a[2], a[1] - b[3], b[1] - a[3], 0.0)
GROUND = {'kitchen': 0, 'living': 0, 'yard': 0, 'street': 0}
floats = []
for o in OBJ:
    if o.get('v2') not in ('new', 'copy', 'moved') or o['cat'] == 'part': continue
    z0, bb = o['z'][0], o['bb']
    if z0 <= 30 or (o['area'] == 'street' and z0 <= 30): continue
    how = None
    for z, r in SURF:
        if abs(z - z0) <= 45 and ov(bb, r) > 0: how = 'surface'; break
    if not how:
        for q in OBJ:
            if q is o or not q.get('solid'): continue
            if ov(bb, q['bb']) > 0 and (abs(q['z'][1] - z0) <= 45 or any(abs(t - z0) <= 45 for t in q.get('tops', [])) or (q.get('interior') and q['z'][0] <= z0 <= q['z'][1])): how = 'on ' + q['id']; break
    if not how:
        for w in WALLS:
            if gap(bb, w) <= 45: how = 'wall'; break
    if not how:
        for q in OBJ:
            if q is o or not q.get('solid') or q['cat'] in ('prop', 'food'): continue
            if gap(bb, q['bb']) <= 45 and q['z'][0] <= o['z'][1] and q['z'][1] >= z0: how = 'hangs on ' + q['id']; break
    if not how:
        floats.append({'id': o['id'], 'mesh': o.get('mesh'), 'z0': z0, 'bb': bb}); bad('float', '%s %s stands on nothing at z=%s' % (o['id'], o.get('mesh'), z0))
OUT['floats'] = floats

# ---- 4. doors and drawers: nothing new in their sweep -----------------------------------------------------------------------------------
sweeps = []
for m in OBJ:
    if 'bb_open' not in m: continue
    zr = m.get('z_open') or m['z']
    for o in OBJ:
        if o is m or o.get('v2') not in ('new', 'copy', 'moved') or not o.get('solid') or o['cat'] == 'part' or o.get('collision') == 'ants_only' or o.get('mesh') == 'SM_Y_Tree': continue
        if o['z'][0] < zr[1] - 10 and o['z'][1] > zr[0] + 10 and ov(m['bb_open'], o['bb']) > 400:
            if o.get('mesh', '').startswith(m.get('mesh', 'x')[:12]) or m.get('mesh', '').startswith((o.get('mesh') or 'x')): continue
            sweeps.append({'door': m['id'], 'door_ru': m['ru'], 'blocked_by': o['id'], 'mesh': o.get('mesh')}); bad('sweep', '%s (%s) hits %s %s when it opens' % (m['id'], m.get('mesh'), o['id'], o.get('mesh')))
OUT['door_sweeps'] = sweeps

# ---- 5. loot inside solids --------------------------------------------------------------------------------------------------------------
def loot_points(D):
    L = [('L%d' % s['i'], s['p'], s['tier_loot'], s['area']) for s in D['loot']['spots']] + [('D%d' % s['i'], s['p'], s['tier_loot'], s['area']) for s in D['loot']['behind_doors']]
    L += [('H%d' % s['i'], s['p'][:2] + [s['spawn_z']], 3, s['area']) for s in D['loot']['heavy']] + [('T%d' % s['i'], s['p'], 3, s['area']) for s in D['loot']['trophies']]
    L += [(m['id'], m['p'], 3, m['area']) for m in D['loot'].get('main', {}).get('spots', [])]
    return L
def buried(D):
    out = []
    for lid, p, tier, area in loot_points(D):
        for o in D['objects']:
            if not o.get('solid') or o.get('interior') or o['cat'] in ('struct', 'part', 'deco', 'conn') or o.get('tilted') or 'gap_under' in o or o.get('collision'): continue
            b = o['bb']
            if b[0] + 25 < p[0] < b[2] - 25 and b[1] + 25 < p[1] < b[3] - 25 and o['z'][0] + 25 < p[2] < o['z'][1] - 25:
                out.append({'loot': lid, 'p': p, 'inside': o['id'], 'mesh': o.get('mesh'), 'ru': o['ru']})
    return out
OUT['loot_in_boxes'] = {'v1': buried(D1), 'v2': buried(D2), 'note': 'boxes are bounding boxes: a hit means "look at it", not "broken" (v1 shows what the method catches)'}

# ---- 6. the route graph -----------------------------------------------------------------------------------------------------------------
NODE = {n['id']: n for n in D2['graph']['nodes']}
EDGE = {}
for e in D2['graph']['edges']:
    open_len = slap = cover = 0.0
    P = e['pts']
    for k in range(len(P) - 1):
        ln = math.hypot(P[k + 1][0] - P[k][0], P[k + 1][1] - P[k][1]); n = max(1, int(ln // 50))
        for q in range(n):
            t = (q + 0.5) / n
            c = G2.at('cover', P[k][0] + (P[k + 1][0] - P[k][0]) * t, P[k][1] + (P[k + 1][1] - P[k][1]) * t)
            if c == '0': open_len += ln / n
            elif c == '1': slap += ln / n
            else: cover += ln / n
    e['exposed_s'] = round(open_len / VS, 1); e['slap_only_s'] = round(slap / VS, 1); e['exposed_pct'] = round(100.0 * open_len / max(1.0, e['len_uu']))
    EDGE[(e['a'], e['b'])] = e; EDGE[(e['b'], e['a'])] = e
routes = []
for r in D2['graph']['routes']:
    row = {'id': r['id'], 'ru': r['ru']}
    for kind in ('fast', 'covered'):
        seq = r[kind]; ln = ex = 0.0; miss = []
        for a, b in zip(seq, seq[1:]):
            e = EDGE.get((a, b))
            if not e: miss.append(a + '-' + b); continue
            ln += e['len_uu']; ex += e['exposed_s']
        if miss: bad('graph', 'route %s %s uses edges that do not exist: %s' % (r['id'], kind, miss))
        row[kind] = {'nodes': seq, 'len_m': round(ln / UU, 1), 't_sprint': round(ln / VS, 1), 't_walk': round(ln / VW, 1), 't_loaded': [round(ln / (VW * 0.9), 1), round(ln / (VW * 0.5), 1)],
                     'exposed_s': round(ex, 1), 'exposed_pct': round(100.0 * ex / max(0.1, ln / VS))}
    routes.append(row)
deg = {}
for e in D2['graph']['edges']:
    deg[e['a']] = deg.get(e['a'], 0) + 1; deg[e['b']] = deg.get(e['b'], 0) + 1
OUT['graph'] = {'routes': routes, 'edges': D2['graph']['edges'], 'degree': deg, 'junctions': [{'node': j['node'], 'id': j['id'], 'edges': deg.get(j['node'], 0)} for j in D2['graph']['junctions']]}
for n in D2['graph']['nodes']:
    x, y = n['p'][0], n['p'][1]
    if n['id'] not in deg: bad('graph', 'node %s has no edges' % n['id'])
goals = [n['id'] for n in D2['graph']['nodes'] if n['role'] in ('goal', 'junction')]
OUT['graph']['goal_nodes_with_under_2_edges'] = [g for g in goals if deg.get(g, 0) < 2]

# ---- 7. cover: the 3-4 s rule -------------------------------------------------------------------------------------------------------------
def cover_stats(A):
    return None if A is None else {r['area']: {k: r.get(k) for k in ('cover_pct', 'cover_pct_x10', 'open_pct', 'slap_only_pct', 'over3s_pct', 'over4s_pct', 'over4s_cells', 'mean_s')} | {'worst_s': r['worst'][0]['s_sprint'] if r['worst'] else 0, 'worst_at': r['worst'][0]['at'] if r['worst'] else None} for r in A['nocover']}
OUT['cover'] = {'v1': cover_stats(A1), 'v2': cover_stats(A2), 'v2_without_hide_table': cover_stats(A2N)}
viol = []
tc = A2['grid'].get('tocover')
if tc:
    for j, row in enumerate(tc):
        for i, c in enumerate(row):
            if c in 'def': viol.append([round(G2.x0 + (i + .5) * G2.g), round(G2.y0 + (j + .5) * G2.g), c])
OUT['cover']['v2_cells_over_4s'] = len(viol); OUT['cover']['v2_cells_over_4s_sample'] = viol[::max(1, len(viol) // 40)][:40]

# ---- 8. dead cells, totals, ways per surface, loot gradient ---------------------------------------------------------------------------------
def totals(A): return {t['area'] + '/' + t['tier']: {k: t[k] for k in ('m2', 'dead_pct', 'transit_pct', 'contested_pct', 'safe_pct', 'ev')} for t in A['totals']}
OUT['totals'] = {'v1': totals(A1), 'v2': totals(A2)}
OUT['dead_cells_v2_indoor'] = [{'area': c['area'], 'tier': c['tier'], 'rect': c['rect'], 'm2': c['m2'], 'surfaces': c.get('surfaces', [])} for c in A2['cells'] if c['cls'] == 'dead' and c['area'] in ('kitchen', 'living')]
def ways(A):
    hi = [s for s in A['surfaces'] if s['z'] >= 400]
    return {'surfaces': len(hi), 'no_way': len([s for s in hi if s['ways'] == 0]), 'one_way': len([s for s in hi if s['ways'] == 1]), 'two_or_more': len([s for s in hi if s['ways'] >= 2]),
            'empty': len([s for s in A['surfaces'] if s['empty']]), 'list_under_2': [[s['id'], s['ways']] for s in hi if s['ways'] < 2]}
OUT['ways'] = {'v1': ways(A1), 'v2': ways(A2)}
OUT['hauling'] = {'v1': {'ramps': ['spoon (65 deg: too steep to drag)'], 'door_shelves': ['oven_door', 'dishwasher_door']},
                  'v2': {'ramps': [c['id'] for c in D2['connectors'] if c.get('haul') == 'ramp'], 'stairs': [c['id'] for c in D2['connectors'] if c.get('haul') == 'stairs'], 'flat_holes': [c['id'] for c in D2['connectors'] if c.get('haul') == 'flat']}}
def gradient(A):
    bands = [(0, 15), (15, 25), (25, 40), (40, 1e9)]; out = []
    for a, b in bands:
        ls = [l for l in A['loot'] if a <= l['t'] < b and l['area'] in ('kitchen', 'living')]
        out.append({'band_s': [a, b if b < 1e8 else None], 'points': len(ls), 'ev_sum': round(sum(l['ev'] for l in ls)), 'ev_mean': round(sum(l['ev'] for l in ls) / max(1, len(ls)), 1), 'tier2plus': len([l for l in ls if l['lvl'] >= 2]), 'tier3': len([l for l in ls if l['lvl'] >= 3])})
    return out
OUT['gradient'] = {'v1': gradient(A1), 'v2': gradient(A2)}
OUT['easy_loot'] = {'v1': len(A1['easy_loot']), 'v2': len(A2['easy_loot']), 'v2_list': [[l['id'], l['ev'], l['t'], l['p']] for l in A2['easy_loot']]}
OUT['audit_routes'] = {'v1': [[r['id'], r['len_m'], r['t_sprint'], r['exposed_s']] for r in A1['routes']], 'v2': [[r['id'], r['len_m'], r['t_sprint'], r['exposed_s']] for r in A2['routes']]}
def area_m2(D): return {a['id']: round(sum((r[2] - r[0]) * (r[3] - r[1]) for r in a['rects']) / (UU * UU), 1) for a in D['areas']}
OUT['areas_m2'] = {'v1': area_m2(D1), 'v2': area_m2(D2)}
# ---- 9. landmarks: how many big recognisable things an ant on the floor sees (straight line from 30 uu above the floor to the top of each)
LM2 = {'kitchen': [('красный холодильник', (3700, 3900, 3700)), ('вытяжка', (-750, 4600, 5000)), ('окно кухни', (0, -5000, 3300)), ('лампа над столом', (500, -500, 3650)), ('дверь в гостиную', (7050, -4000, 3700)), ('верхние шкафы', (-4500, 4300, 4350))],
       'living': [('книжный шкаф', (19650, -4500, 4600)), ('телевизор', (13150, -8650, 3200)), ('диван', (13500, 1150, 2050)), ('торшер', (9900, -7450, 4150)), ('окно гостиной', (18285, -8990, 3300)), ('дверь в кухню', (7250, -4000, 3700)), ('гирлянда', (13200, -4250, 3800))]}
LM1 = {'kitchen': LM2['kitchen'], 'living': [('книжный шкаф', (19650, -4500, 4600)), ('телевизор', (13750, -8650, 3200)), ('диван', (13500, 1150, 2050)), ('торшер', (17800, 700, 4150)), ('дверь в кухню', (7250, -4000, 3700))]}
def seg_hits(p, q, bx):
    t0, t1 = 0.0, 1.0
    for k in range(3):
        d = q[k] - p[k]; lo, hi = bx[k], bx[k + 3]
        if abs(d) < 1e-9:
            if p[k] < lo or p[k] > hi: return False
        else:
            a, b = (lo - p[k]) / d, (hi - p[k]) / d
            if a > b: a, b = b, a
            t0, t1 = max(t0, a), min(t1, b)
            if t0 > t1: return False
    return t1 - t0 > 1e-6
def landmarks(D, A, LM):
    out = {}
    for area, lms in LM.items():
        occ = []
        for o in D['objects']:
            if not o.get('solid') or o['area'] != area or o['cat'] in ('struct', 'deco') or o.get('collision') or o.get('role') == 'rider': continue
            b = o['bb']; ar = (b[2] - b[0]) * (b[3] - b[1]); z0 = o.get('gap_under', o['z'][0]); h = o['z'][1] - z0
            if h < 60 or not (ar >= 250000 or (h >= 800 and ar >= 20000)): continue
            occ.append((b[0], b[1], z0, b[2], b[3], o['z'][1]))
        seen = []
        for c in A['cells']:
            if c['area'] != area or c['tier'] != 'floor': continue
            p = (c['pt'][0], c['pt'][1], 30.0); n = 0
            for ru, q in lms:
                boxes = [bx for bx in occ if not (bx[0] - 150 <= q[0] <= bx[3] + 150 and bx[1] - 150 <= q[1] <= bx[4] + 150 and bx[2] - 150 <= q[2] <= bx[5] + 150) and not (bx[0] <= p[0] <= bx[3] and bx[1] <= p[1] <= bx[4] and bx[2] <= 30 <= bx[5])]
                if not any(seg_hits(p, q, bx) for bx in boxes): n += 1
            seen.append(n)
        out[area] = {'landmarks': [l[0] for l in lms], 'cells': len(seen), 'min_seen': min(seen), 'mean_seen': round(sum(seen) / len(seen), 1), 'cells_seeing_2_plus_pct': round(100.0 * sum(1 for v in seen if v >= 2) / len(seen))}
    return out
OUT['landmarks'] = {'v1': landmarks(D1, A1, LM1), 'v2': landmarks(D2, A2, LM2), 'note': 'geometry only: bounding boxes as occluders, the eye 30 uu above the middle of every 1 m floor cell'}
OUT['summary'] = {'v1': A1['summary'], 'v2': A2['summary']}
OUT['counts'] = {'v1': D1['counts'], 'v2': D2['counts']}
with open(os.path.join(LV, 'level_v2_check.json'), 'w', encoding='utf-8', newline='\n') as fo:
    json.dump(OUT, fo, ensure_ascii=False, indent=1)

print('PROBLEMS %d' % len(OUT['problems']))
for p in OUT['problems'][:40]: print('  ', p['kind'], '|', p['text'])
print('human wander boxes standable %:', OUT['human']['wander_box_standable_pct'], 'stand m2:', OUT['human']['stand_m2'])
print('pets closest', OUT['pets'])
print('cover v1', OUT['cover']['v1']); print('cover v2', OUT['cover']['v2']); print('cover v2 no hide', OUT['cover']['v2_without_hide_table'])
print('cells over 4 s:', len(viol), viol[::max(1, len(viol) // 12)][:12])
for k in OUT['totals']['v2']: print('%-16s v1 %s' % (k, OUT['totals']['v1'].get(k)), '\n%-16s v2 %s' % ('', OUT['totals']['v2'][k]))
for c in OUT['dead_cells_v2_indoor']: print('DEAD', c['area'], c['tier'], c['rect'], c['m2'], len(c['surfaces']))
print('ways', OUT['ways']['v1'], OUT['ways']['v2'])
print('gradient v1', OUT['gradient']['v1']); print('gradient v2', OUT['gradient']['v2'])
print('easy', OUT['easy_loot'])
for r in routes: print(r['id'], 'fast %5.1f m %5.1f s exp %5.1f | cover %5.1f m %5.1f s exp %5.1f' % (r['fast']['len_m'], r['fast']['t_sprint'], r['fast']['exposed_s'], r['covered']['len_m'], r['covered']['t_sprint'], r['covered']['exposed_s']))
print('landmarks v1', {k: (v['min_seen'], v['mean_seen'], v['cells_seeing_2_plus_pct']) for k, v in OUT['landmarks']['v1'].items()}, 'v2', {k: (v['min_seen'], v['mean_seen'], v['cells_seeing_2_plus_pct']) for k, v in OUT['landmarks']['v2'].items()})
print('loot in boxes v1 %d, v2 %d' % (len(OUT['loot_in_boxes']['v1']), len(OUT['loot_in_boxes']['v2'])))
v1b = set(b['loot'] for b in OUT['loot_in_boxes']['v1'])
for b in OUT['loot_in_boxes']['v2']:
    if b['loot'] not in v1b: print('  BOX', b['loot'], [round(v) for v in b['p']], b['inside'], b['mesh'])