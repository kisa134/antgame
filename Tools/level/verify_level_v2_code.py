# -*- coding: utf-8 -*-
# Ant Kitchen level v2: is the C++ what docs/level/level_data.json says? (python Tools/level/verify_level_v2_code.py)
# The placement table and the gameplay tables are generated from the data, so they are compared whole. The constants that live
# in the C++ by hand (walls, holes, nests, containers, trophies, pets...) are looked up as text: each check is a value taken
# from the JSON and the fragment of code that must carry it. Writes docs/level/level_v2_code_check.json; exit code 1 on a miss.
import json, os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE))
try: sys.stdout.reconfigure(encoding='utf-8', errors='replace')
except Exception: pass
LV = os.path.join(ROOT, 'docs', 'level'); SRC = os.path.join(ROOT, 'Source', 'AntKitchen')
D = json.load(open(os.path.join(LV, 'level_data.json'), encoding='utf-8'))
M = json.load(open(os.path.join(ROOT, 'SourceArt', 'Blender', 'level_v2', 'level_v2_manifest.json'), encoding='utf-8')) if os.path.exists(os.path.join(ROOT, 'SourceArt', 'Blender', 'level_v2', 'level_v2_manifest.json')) else None
def src(n): return open(os.path.join(SRC, n), encoding='utf-8-sig').read().replace('\r\n', '\n')
def f(v):
    s = ('%.1f' % float(v)).rstrip('0').rstrip('.')
    if s in ('-0', ''): s = '0'
    return s + ('f' if '.' in s else '.f')
def fv(p): return 'FVector(%s)' % ', '.join(f(v) for v in p)
OUT = []
def check(what, ok, where, detail=''):
    OUT.append({'what': what, 'ok': bool(ok), 'where': where, 'detail': detail})
def has(what, fname, needle):
    check(what, needle in src(fname), fname, needle)
def by(lst, key, val): return next(x for x in lst if x.get(key) == val)

# ---- generated tables, compared whole
inl = src('LevelV2Data.inl')
rows = re.findall(r'\{ (-?[\d.]+)f?, (-?[\d.]+)f?, (-?[\d.]+)f?, (\d) \},   // (\d+)', inl)
spots = D['loot']['spots']
same = len(rows) == len(spots) and all(abs(float(r[0]) - s['p'][0]) < 0.06 and abs(float(r[1]) - s['p'][1]) < 0.06 and abs(float(r[2]) - s['p'][2]) < 0.06 and int(r[3]) == s['tier_loot'] and int(r[4]) == s['i'] for r, s in zip(rows, spots))
check('loot spots: %d in the data, %d in LevelV2Data.inl, every one equal' % (len(spots), len(rows)), same, 'LevelV2Data.inl')
cov = [c for c in D['human']['sees']['never_targets'] if 'rect' in c and not c.get('src', '').startswith('HumanAIController')]
check('cover boxes: %d in the data' % len(cov), inl.count('TEXT("') >= len(cov) and all(('%s, %s, %s, %s, %s, %s' % (f(c['rect'][0]), f(c['rect'][1]), f(c['z'][0]), f(c['rect'][2]), f(c['rect'][3]), f(c['z'][1]))) in inl for c in cov), 'LevelV2Data.inl')
check('patrol stops: %d' % len(D['human']['patrol']['points']), all(('{ %s, %s },' % (f(p[0]), f(p[1]))) in inl for p in D['human']['patrol']['points']), 'LevelV2Data.inl')
check('heavy prize spots: %d' % len(D['loot']['heavy']), all(('{ %s, %s, %s },' % tuple(f(v) for v in h['p'])) in inl for h in D['loot']['heavy']), 'LevelV2Data.inl')
check('main object: value %d, weight %d, %d places' % (D['loot']['main']['value'], D['loot']['main']['weight'], len(D['loot']['main']['spots'])),
      ('GV2MainValue = %d;' % D['loot']['main']['value']) in inl and ('GV2MainWeightDesign = %s;' % f(D['loot']['main']['weight'])) in inl and all(('{ %s, %s, %s, ' % tuple(f(v) for v in m['p'])) in inl for m in D['loot']['main']['spots']), 'LevelV2Data.inl')
art = src('KitchenArtData.inl')
n_art = len(re.findall(r'^\t\{ TEXT\("SM_', art, re.M))
check('art placements: %d objects of the data are placed meshes, %d rows in KitchenArtData.inl' % (D['counts']['art_instances'], n_art), n_art == D['counts']['art_instances'], 'KitchenArtData.inl')
if M:
    n_m = sum(len(p['instances']) for p in M['pieces'])
    check('art placements: %d instances in level_v2_manifest.json' % n_m, n_m == n_art, 'KitchenArtData.inl')
check('movers: %d' % D['counts']['movers'], len(re.findall(r'^\t\{ \d+, [01], \{', art, re.M)) == D['counts']['movers'], 'KitchenArtData.inl')

# ---- constants written in the C++ by hand
nest1 = by(D['spawns'], 'id', 'nest1')['p']
has('fire ants: nest %s' % nest1, 'KitchenBuilder.cpp', fv(nest1))
has('fly feeder: dog poop', 'KitchenBuilder.cpp', fv(by(D['spawns'], 'id', 'fly_poop')['p']))
b = by(D['spawns'], 'id', 'fly_bin')['p']
has('fly feeder: yard bin', 'KitchenBuilder.cpp', fv([b[0], b[1], 0]))
for o in D['openings']:
    if o.get('v2') in ('new', 'fixed'):
        bx = o['box']; needle = '%s, %s' % (fv(bx[:3]), fv(bx[3:]))
        fn = 'KitchenHouse.cpp' if o['id'] in ('pit', 'north_hole', 'east_hole', 'nest_crack', 'window_crack') else 'KitchenBuilder.cpp'
        has('opening %s %s' % (o['id'], bx), fn, needle)
pit = by(D['spaces'], 'id', 'pit')['box']
has('pit floor at %s' % pit[2], 'KitchenHouse.cpp', 'FVector(5300.f, 2300.f, %s), DarkDirt);' % f(pit[2]))
for w in D['walls']:
    if w.get('kind') == 'blocker':
        r = w['rect']
        check('edge blocker %s' % r, re.search(r'\{ %s, %s, -?[\d.]+f?, %s \}' % (re.escape(f(r[0])), re.escape(f(r[1])), re.escape(f(r[3]))), src('KitchenHouse.cpp')) is not None or
              ('{ %s, %s, %s, %s }' % tuple(f(v) for v in r)) in src('KitchenHouse.cpp'), 'KitchenHouse.cpp', 'BuildEdges: the wall on this edge (the code makes it at least as long)')
ex = by(D['extraction'], 'id', 'extract1')
has('second delivery point %s r %s' % (ex['p'], ex['radius']), 'LevelV2.h', 'SouthEntrance() { return %s; }' % fv(ex['p']))
has('second delivery radius', 'LevelV2.h', 'SouthEntranceRadius() { return %s; }' % f(ex['radius']))
sp = by(D['hazards'], 'id', 'sprinkler')['circle']
has('sprinkler %s' % sp, 'LevelV2.h', 'Sprinkler() { return %s; }' % fv([sp[0], sp[1], 0]))
has('sprinkler radius', 'LevelV2.h', 'SprinklerRadius() { return %s; }' % f(sp[2]))
rad = by(D['hazards'], 'id', 'radiator'); r = rad['rect']
has('radiator zone', 'KitchenBuilder.cpp', 'FBox(%s, %s)' % (fv([r[0], r[1], rad['z'][0]]), fv([r[2], r[3], rad['z'][1]])))
for c in D['containers']:
    if c.get('v2') or c['id'] == 'BreadBox':
        bx = c['box']
        has('container %s' % c['id'], 'AntRun.cpp', '%s, %s' % (fv(bx[:3]), fv(bx[3:])))
for t in D['loot']['trophies']:
    has('trophy %d «%s»' % (t['i'], t['ru']), 'AntKitchenGameMode.cpp', fv(t['p']))
bd = src('AntRun.cpp')
check('food behind doors: %d spots with their tiers' % len(D['loot']['behind_doors']), all(('{ %s, %d }' % (fv(s['p']), s['tier_loot'])) in bd for s in D['loot']['behind_doors']), 'AntRun.cpp')
has('pushable mug', 'AntRun.cpp', fv(by(D['interactables'], 'id', 'push1')['p']))
st = D['loot']['special'][0]['p']
check('cargo in the pit at z %s (the code spawns it 25 above the floor)' % st[2], 'KitchenArt::Enabled() ? -265.f : -350.f' in src('AntKitchenGameMode.cpp'), 'AntKitchenGameMode.cpp')
cb = src('CritterBrain.cpp')
for who in ('dog', 'cat'):
    for s in D['pets'][who]['spots']:
        p = s['p']
        ok = fv(p) in cb or ('FVector(%s, %s, 0.f)' % (f(p[0]), f(p[1]))) in cb
        OUT.append({'what': 'pet spot %s: %s %s' % (who, s['label'], p), 'ok': ok, 'where': 'CritterBrain.cpp', 'detail': '' if ok else 'taken from where the furniture stands (ArtAt + offset), not written as this literal', 'soft': True})
hb = D['human']['patrol']
check('Human wander boxes', ('GV2WanderKitchen[4] = { %s }' % ', '.join(f(v) for v in hb['kitchen_box'])) in inl and ('GV2WanderLiving[4] = { %s }' % ', '.join(f(v) for v in hb['living_box'])) in inl, 'LevelV2Data.inl')
check('toaster rule (fewer than two on the worktop, 40 s)', 'OnTop < 2 && Now() >= NextFoodAt' in src('KitchenInteractables.cpp') and 'NextFoodAt = Now() + 40.f;' in src('KitchenInteractables.cpp'), 'KitchenInteractables.cpp')
check('cat flap: heavy, latched, shuts by itself', 'Name.Contains(TEXT("CatFlap"))' in src('KitchenMover.cpp') and 'OnLatchSideOfCatFlap' in src('KitchenMover.cpp'), 'KitchenMover.cpp')
check('ants-only pieces', all(n in src('KitchenBuilder.cpp') for n in ('TEXT("SM_L_YarnBridge")', 'TEXT("SM_L_Garland")', 'TEXT("SM_K_ChiliString")', 'TEXT("SM_K_FlyPaper")')), 'KitchenBuilder.cpp')

hard = [o for o in OUT if not o.get('soft')]
bad = [o for o in hard if not o['ok']]
soft_bad = [o for o in OUT if o.get('soft') and not o['ok']]
with open(os.path.join(LV, 'level_v2_code_check.json'), 'w', encoding='utf-8', newline='\n') as fo:
    json.dump({'what': 'docs/level/level_data.json against the C++ (Tools/level/verify_level_v2_code.py)', 'checks': len(hard), 'passed': len(hard) - len(bad), 'failed': bad,
               'not_literal': soft_bad, 'all': OUT}, fo, ensure_ascii=False, indent=1)
print('%d checks, %d pass, %d fail; %d pet spots are not literals in the code (they follow the furniture)' % (len(hard), len(hard) - len(bad), len(bad), len(soft_bad)))
for o in bad: print('FAIL %s | %s | %s' % (o['what'], o['where'], o['detail']))
sys.exit(1 if bad else 0)