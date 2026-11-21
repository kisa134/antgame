# -*- coding: utf-8 -*-
# Ant Kitchen, level v3: the seed checks of level_v3_layout.VALIDATE on
#   1. the v2 level (calibration: the same numbers as check_level_v2 / audit_level, by the faster numpy method of level_v3_lib),
#   2. the v3 BASE state, 3. every district variant (others A), 4. every PAIR of variants of two districts (--pairs, ~940 states),
#   5. N random seeds (default 300, threats 1-4 in turn).
#   python Tools/level/check_level_v3.py [--seeds N] [--pairs]      -> docs/level/level_v3_check.json + a summary
# Rules: the cake has 2 edge-disjoint haul routes to the nest (best <= 200 s, second <= 330 s carried); every floor cell of the kitchen and
# the living room is <= 4 s sprint from cover; the Human (capsule 340, step 250) reaches his spawn, door points, patrol and routine spots,
# and so do the grandma and the kid when they are home; nothing new floats, overlaps a solid or stands in a door's sweep; scatter that breaks
# its rules is dropped (counted, not a failure: the game re-draws it).
import json, os, sys, time, collections, itertools
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
try: sys.stdout.reconfigure(encoding='utf-8', errors='replace')
except Exception: pass
import level_v3_layout as L
import level_v3_lib as X
N = 300; PAIRS = '--pairs' in sys.argv
for i, a in enumerate(sys.argv):
    if a == '--seeds': N = int(sys.argv[i + 1])
t0 = time.time()
B = X.load_base(); Gd = X.Grid(B['D2'])
def brief(r):
    return {'ok': r['ok'], 'fails': r['fails'], 'cover': r['cover'], 'cake': r['cake'], 'scatter_dropped': len(r['scatter_dropped']), 'stand_cells': r['stand_cells']}
OUT = {'what': 'level v3 seed validation (Tools/level/check_level_v3.py)', 'rules': L.VALIDATE, 'method': 'numpy grid 100 uu as audit_level.py: capsule clearance 390 from solids painted by box (sloped HUMAN_STEP_RAMPS only above the 250 step, hanging cloth skipped), flood from his spawn; cover = hidden boxes + cells farther than the slap reach (1250) from where he stands + yard/street/walls; 16-direction geodesic to cover; cake: min-cost 2-flow on the route graph, carried at 0.7 walk speed'}
# 1. v2 calibration
st2 = {'objects': B['D2']['objects'], 'hides': B['hides'], 'comp': None, 'graph': X.graph_state(B['D2'], []), 'insts': [], 'conn_off': []}
stand2, _ = X.human_nav(B, st2, Gd); cov2, _a, _b, _c = X.cover(B, st2, Gd, stand2)
OUT['v2_calibration'] = {'cover': cov2, 'audit_v2_said': {'kitchen_worst_s': 4.0, 'living_worst_s': 3.9, 'kitchen_cover_pct': 17.8, 'living_cover_pct': 18.9}}
# 2. the default state (BASE + variant A of every district: what seed-less tools and the renders start from) and the bare skeleton
def comp_force(force, seed=0):
    c = L.compose(seed, 1, force=dict({k: 'A' for k in L.DISTRICT_ORDER}, **force)); return c
rb = X.run_checks(B, X.apply(B, comp_force({})), Gd); OUT['base'] = brief(rb)
rs = X.run_checks(B, dict(X.apply(B, None), comp=None), Gd)
OUT['skeleton'] = dict(brief(rs), note='BASE alone, without any district variant: never played (every seed applies one variant per district); informative only')
# 3. variants
VR = []
for d in L.DISTRICT_ORDER:
    for v in L.VARIANTS[d]:
        r = X.run_checks(B, X.apply(B, comp_force({d: v['id']})), Gd)
        VR.append({'district': d, 'variant': v['id'], 'ru': v['ru'], **brief(r)})
OUT['variants'] = VR
# 4. pairs
PR = []
if PAIRS:
    for d1, d2 in itertools.combinations(L.DISTRICT_ORDER, 2):
        for v1 in L.VARIANTS[d1]:
            for v2 in L.VARIANTS[d2]:
                r = X.run_checks(B, X.apply(B, comp_force({d1: v1['id'], d2: v2['id']})), Gd)
                if not r['ok']: PR.append({'pair': [d1 + v1['id'], d2 + v2['id']], 'fails': r['fails']})
    OUT['pairs'] = {'checked': sum(len(L.VARIANTS[a]) * len(L.VARIANTS[b]) for a, b in itertools.combinations(L.DISTRICT_ORDER, 2)), 'failed': PR}
# 4b. loot points that new or moved v3 solids now cover (hollow pieces are fine: the loot is inside them on purpose)
HOLLOW = ('SM_K_CerealBox', 'SM_K_Slipper', 'SM_V3_GiftBox', 'SM_V3_Backpack', 'SM_V3_ToyBox', 'SM_K_Chair', 'SM_K_FruitBowl', 'SM_V3_DirtyDishStack', 'SM_V3_StepStool', 'SM_L_ToyArch', 'SM_L_ToyCar', 'SM_K_Shoe', 'SM_V3_ShoppingTrolley', 'SM_V3_VeggieCrate')
def loot_pts(D):
    o = [('L%d' % s['i'], (L.LOOT_FIX.get(s['i'], (s['p'],))[0])) for s in D['loot']['spots']] + [('D%d' % s['i'], s['p']) for s in D['loot']['behind_doors']]
    o += [('H%d' % s['i'], s['p'][:2] + [s['spawn_z']]) for s in D['loot']['heavy']] + [('T%d' % s['i'], s['p']) for s in D['loot']['trophies']] + [(m['id'], m['p']) for m in D['loot']['main']['spots']]
    return o
LP = loot_pts(B['D2']); LB = []
for tag, comp in [('default', comp_force({}))] + [(d + v['id'], comp_force({d: v['id']})) for d in L.DISTRICT_ORDER for v in L.VARIANTS[d]]:
    st = X.apply(B, comp)
    lz = {}
    for d in L.DISTRICT_ORDER:
        for op in [x for x in L.VARIANTS[d] if x['id'] == comp['variants'][d]][0]['ops']:
            if op[0] == 'loot_z':
                for i in op[1]: lz[i] = op[2]
    for o in st['objects']:
        if o.get('v3') not in ('new', 'moved', 'scaled', 'restored') or not o.get('solid') or o.get('cloth') or o.get('collision') == 'ants_only' or o['cat'] in ('fastener', 'part', 'struct'): continue
        b = o['bb']
        for lid, p in LP:
            z = lz.get(lid, p[2])
            if b[0] + 20 < p[0] < b[2] - 20 and b[1] + 20 < p[1] < b[3] - 20 and o['z'][0] - 5 <= z < o['z'][1] - 20:
                LB.append({'state': tag, 'loot': lid, 'p': p, 'in': o['id'], 'mesh': o.get('mesh'), 'ok': (o.get('mesh') or '').startswith(HOLLOW)})
seenl = set(); LB = [x for x in LB if not ((x['loot'], x['in']) in seenl or seenl.add((x['loot'], x['in'])))]
OUT['loot_covered'] = {'hollow_ok': [x for x in LB if x['ok']], 'problems': [x for x in LB if not x['ok']]}
# 5. seeds
SR = []; reasons = collections.Counter(); cover_worst = []; cake_t = []; dropped = 0
for s in range(N):
    comp = L.compose(s, 1 + s % 4)
    r = X.run_checks(B, X.apply(B, comp), Gd)
    for f in r['fails']: reasons[f.split(' at ')[0].split(' (')[0][:80]] += 1
    cover_worst.append(max(r['cover']['kitchen']['worst_s'], r['cover']['living']['worst_s']))
    if len(r['cake']['times_s']) >= 2: cake_t.append(r['cake']['times_s'])
    dropped += len(r['scatter_dropped'])
    SR.append({'seed': s, 'threat': comp['threat'], 'time': comp['time'], 'home': [k for k, v in comp['home'].items() if v], 'variants': ''.join(comp['variants'][d] for d in L.DISTRICT_ORDER),
               'cake': comp['cake'], 'ok': r['ok'], 'fails': r['fails'], 'cover_worst_s': cover_worst[-1], 'cake_s': r['cake']['times_s']})
ok = sum(1 for x in SR if x['ok'])
seen = collections.defaultdict(set)
for x in SR:
    for d, v in zip(L.DISTRICT_ORDER, x['variants']): seen[d].add(v)
OUT['seeds'] = {'n': N, 'passed': ok, 'pass_rate': round(100.0 * ok / max(1, N), 1), 'fail_reasons': dict(reasons), 'cover_worst_s_max': max(cover_worst) if cover_worst else None,
                'cake_best_s_max': max(t[0] for t in cake_t) if cake_t else None, 'cake_second_s_max': max(t[1] for t in cake_t) if cake_t else None, 'scatter_dropped_total': dropped,
                'variants_seen': {d: ''.join(sorted(v)) for d, v in seen.items()}, 'district_order': L.DISTRICT_ORDER, 'list': SR}
OUT['fix_log'] = [{'rule': a, 'found': b, 'fixed': c} for a, b, c in L.FIX_LOG]
OUT['seconds'] = round(time.time() - t0, 1)
with open(os.path.join(X.LV, 'level_v3_check.json'), 'w', encoding='utf-8') as f: json.dump(OUT, f, ensure_ascii=False, indent=1)
print('v2 calibration: kitchen %.2f s (audit 4,0), living %.2f s (audit 3,9)' % (cov2['kitchen']['worst_s'], cov2['living']['worst_s']))
print('default state (all A): %s %s cover %s/%s s; skeleton without variants: cover %s/%s s (never played)' % ('OK' if rb['ok'] else 'FAIL', rb['fails'][:3], rb['cover']['kitchen']['worst_s'], rb['cover']['living']['worst_s'], rs['cover']['kitchen']['worst_s'], rs['cover']['living']['worst_s']))
print('variants: %d of %d pass' % (sum(1 for v in VR if v['ok']), len(VR)))
print('loot covered by new v3 solids: %d inside hollow pieces (fine), %d problems %s' % (len(OUT['loot_covered']['hollow_ok']), len(OUT['loot_covered']['problems']), [(x['state'], x['loot'], x['in']) for x in OUT['loot_covered']['problems']][:8]))
for v in VR:
    if not v['ok']: print('   ', v['district'], v['variant'], v['fails'][:3])
if PAIRS: print('pairs: %d checked, %d failed' % (OUT['pairs']['checked'], len(PR)))
for p in PR[:20]: print('   ', p['pair'], p['fails'][:2])
print('seeds: %d of %d pass (%.1f %%), worst cover %.2f s, cake best route max %s s, second max %s s, scatter dropped %d' % (ok, N, OUT['seeds']['pass_rate'], OUT['seeds']['cover_worst_s_max'], OUT['seeds']['cake_best_s_max'], OUT['seeds']['cake_second_s_max'], dropped))
for k, n in reasons.most_common(12): print('   %4d  %s' % (n, k))
print('wrote docs/level/level_v3_check.json in %.0f s' % (time.time() - t0))
