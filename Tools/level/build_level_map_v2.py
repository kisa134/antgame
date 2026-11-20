# -*- coding: utf-8 -*-
# Ant Kitchen, level map v2: docs/level/level_map_v2.html — the same phone-first layered map as level_map.html, for the redesign,
# with a "было / стало" switch. One self-contained file (inline SVG, CSS, JS; no external URLs).
#   python Tools/level/build_level_map_v2.py
# Needs: level_data.json + level_audit.json (v1), level_v2.json + level_audit_v2.json + level_v2_check.json (v2).
# URL hash for screenshots and links: #s=2&v=kitchen&ly=geo,route,cover&c=12  (state, view, layers, card index).
import json, os, sys, html, math, re
import xml.etree.ElementTree as ET
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE)); LV = os.path.join(ROOT, 'docs', 'level')
try: sys.stdout.reconfigure(encoding='utf-8', errors='replace')
except Exception: pass
def load(n): return json.load(open(os.path.join(LV, n), encoding='utf-8'))
D1, A1, D2, A2, CH = load('v1/level_data.json'), load('v1/level_audit.json'), load('level_v2.json'), load('level_audit_v2.json'), load('level_v2_check.json')   # v1 is frozen in docs/level/v1
A2N = load('level_audit_v2_nohide.json')
UU = 1900.0
def X(x): return round(x / 100.0, 1)
def Y(y): return round(-y / 100.0, 1)
def e(s): return html.escape(str(s), quote=True)
def num(v, d=1): return (('%.' + str(d) + 'f') % v).replace('.', ',')
CARDS = []
def card(title, kind, rows):
    CARDS.append([str(title), kind, [[str(k), str(v)] for k, v in rows if v not in (None, '', [])]]); return len(CARDS) - 1
def rect(x0, y0, x1, y1, cls, c=None, extra=''):
    return '<rect x="%s" y="%s" width="%s" height="%s" class="%s"%s%s/>' % (X(x0), Y(y1), round((x1 - x0) / 100.0, 1), round((y1 - y0) / 100.0, 1), cls, '' if c is None else ' data-c="%d"' % c, extra)
def mk(x, y, inner, c=None, cls=''):
    return '<g class="mk %s" transform="translate(%s,%s)"%s><g class="mi">%s</g></g>' % (cls, X(x), Y(y), '' if c is None else ' data-c="%d"' % c, inner)
def label(x, y, s, cls='lb'): return '<text class="%s" x="%s" y="%s">%s</text>' % (cls, X(x), Y(y), e(s))
def line(pts, cls, c=None): return '<polyline class="%s" points="%s"%s/>' % (cls, ' '.join('%s,%s' % (X(p[0]), Y(p[1])) for p in pts), '' if c is None else ' data-c="%d"' % c)
TIER_RU = {'floor': 'пол', 'under': 'под мебелью', 'work': 'столешницы и столы', 'top': 'верха и полки', 'int': 'внутри', 'vent': 'в стенах и вентиляции'}
AREA_RU = {'kitchen': 'Кухня', 'living': 'Гостиная', 'yard': 'Двор', 'street': 'Улица'}
KIND_RU = {'opening': 'проход', 'climb': 'путь наверх', 'ramp': 'наклонный путь', 'stairs': 'лесенка (вниз с грузом)', 'duct': 'воздуховод', 'teleport': 'телепорт', 'gap': 'щель / лаз', 'launch': 'катапульта', 'path': 'дорожка'}
HAUL_RU = {'ramp': 'пандус: тащат волоком вверх и вниз', 'stairs': 'ступени до 600 uu: груз роняют вниз, не бьётся', 'flat': 'ровный лаз: тащат волоком'}

def grid_runs(A, name, pick, step=2):
    g = A['grid']; rows = g[name]; out = []
    for j in range(0, g['ny'] - step + 1, step):
        run = None
        for i in range(0, g['nx'] - step + 1, step):
            v = None
            for dj in range(step):
                for di in range(step):
                    q = pick(rows[j + dj][i + di])
                    if q and (v is None or q > v): v = q
            if run and run[1] == v: run[2] = i + step
            else:
                if run and run[1]: out.append((j, run))
                run = [i, v, i + step]
        if run and run[1]: out.append((j, run))
    s = []
    for j, (i0, v, i1) in out:
        s.append('<rect x="%s" y="%s" width="%s" height="%s" class="%s"/>' % (X(g['x0'] + i0 * g['g']), Y(g['y0'] + (j + step) * g['g']), (i1 - i0) * g['g'] / 100.0, step * g['g'] / 100.0, v))
    return ''.join(s)

def build(D, A, v2):
    L = {k: [] for k in ('base', 'cells', 'geo', 'dist', 'cover', 'human', 'haz', 'rival', 'conn', 'haul', 'route', 'loot', 'main', 'label')}
    for a in D['areas']:
        for r in a['rects']: L['base'].append(rect(r[0], r[1], r[2], r[3], 'ar-' + a['id']))
    for w in D['walls']:
        r = w['rect']; L['base'].append(rect(r[0], r[1], r[2], r[3], 'wall' if not w.get('kind') else ('fence' if w['kind'] == 'fence' else 'blocker')))
    for a in D['areas']:
        r = a['rects'][0]; L['label'].append(label((r[0] + r[2]) / 2, r[3] - 420, a['ru'], 'lb room'))
    # objects
    for o in sorted(D['objects'], key=lambda o: (o['z'][1], o['z'][0])):
        b = o['bb']
        if o.get('mesh') == 'SM_Y_Tree':      # the box of a tree is its crown: draw the trunk and the crown's outline instead
            L['geo'].append('<circle cx="%s" cy="%s" r="5.2" class="o h3"/><circle cx="%s" cy="%s" r="%s" class="crown"/>' % (X(o['origin'][0]), Y(o['origin'][1]), X(o['origin'][0]), Y(o['origin'][1]), round((b[2] - b[0]) / 200.0, 1)))
            continue
        if (b[2] - b[0]) * (b[3] - b[1]) < 3000: continue
        h = o['z'][1]; cls = 'o h0' if h < 300 else ('o h1' if h < 1000 else ('o h2' if h < 2500 else 'o h3'))
        if o['cat'] == 'part': cls = 'o part'
        if not o.get('solid'): cls += ' flat'
        st = o.get('v2')
        c = None
        if st in ('new', 'moved', 'copy') or o['cat'] == 'furn' or (b[2] - b[0]) * (b[3] - b[1]) > 600000:
            if st: cls += ' ' + st
            c = card(o['ru'], {'new': 'новый предмет', 'moved': 'передвинут', 'copy': 'ещё один экземпляр'}.get(st, 'предмет'),
                     [('меш', o.get('mesh') or 'коробка из кода'), ('где', '%s, x %d..%d, y %d..%d' % (AREA_RU[o['area']], b[0], b[2], b[1], b[3])), ('высота', '%d..%d uu (%s м)' % (o['z'][0], o['z'][1], num(o['z'][1] / UU, 2))),
                      ('зачем передвинут', o.get('v2_why')), ('укрытие внутри', 'да' if o.get('interior') and st else None), ('коллизия', 'только муравьи' if o.get('collision') else None)])
        L['geo'].append(rect(b[0], b[1], b[2], b[3], cls, c))
    # cover: seconds of sprint to the nearest cover
    L['cover'].append(grid_runs(A, 'tocover', lambda ch: {'0': 'cv0', 'b': 'cv3', 'c': 'cv4', 'd': 'cv6', 'e': 'cv9', 'f': 'cv9'}.get(ch)))
    for h in D['human']['sees']['never_targets']:
        if 'rect' in h:
            r = h['rect']; L['cover'].append(rect(r[0], r[1], r[2], r[3], 'hide' + (' need' if h.get('needs_code') else ''), card(h['ru'], 'укрытие: слепая зона ИИ', [('высота', '%d..%d' % tuple(h['z'])), ('в коде', 'таблица укрытий v2 (LevelV2::CoverAt)' if h.get('needs_code') else 'уже есть (%s)' % h.get('src', ''))])))
    # human
    L['human'].append(grid_runs(A, 'stand', lambda ch: 'hs' if ch == '1' else None))
    P = D['human']['patrol']
    for k in ('kitchen_box', 'living_box'):
        r = P[k]; L['human'].append(rect(r[0], r[1], r[2], r[3], 'hbox'))
    for p in P.get('points', []): L['human'].append(mk(p[0], p[1], '<circle r="4" class="hp"/>'))
    for p in P['door_waypoints']: L['human'].append(mk(p[0], p[1], '<rect x="-4" y="-4" width="8" height="8" class="hp"/>'))
    sp = D['human']['spawn']; L['human'].append(mk(sp[0], sp[1], '<circle r="8" class="hp"/><text class="mt" y="3.5">Ч</text>', card('Появление Человека', 'Человек', [('точка', '(%d, %d)' % (sp[0], sp[1])), ('ходит', P['kind'])])))
    # hazards and rival ground
    for h in D['hazards']:
        cls = 'hz rv' if h['kind'] == 'rival' else ('hz pet' if h['kind'] in ('pet', 'npc', 'robot') else 'hz')
        c = card(h['ru'], 'опасность', [('что делает', h['effect']), ('высота', '%d..%d' % tuple(h['z'])), ('в коде', 'сделано в v2' if h.get('needs_code') else None)])
        tgt = 'rival' if h['kind'] == 'rival' else 'haz'
        if 'circle' in h: L[tgt].append('<circle cx="%s" cy="%s" r="%s" class="%s" data-c="%d"/>' % (X(h['circle'][0]), Y(h['circle'][1]), round(h['circle'][2] / 100.0, 1), cls, c))
        else: L[tgt].append(rect(h['rect'][0], h['rect'][1], h['rect'][2], h['rect'][3], cls, c))
    for s in D['spawns']:
        if s['kind'] in ('nest', 'nest_rival'):
            c = card(s['ru'], 'гнездо', [('точка', '(%d, %d)' % (s['p'][0], s['p'][1])), ('радиус', s.get('radius')), ('заметка', s.get('note'))])
            L['main' if s['kind'] == 'nest' else 'rival'].append('<circle cx="%s" cy="%s" r="%s" class="%s" data-c="%d"/>' % (X(s['p'][0]), Y(s['p'][1]), s['radius'] / 100.0, 'nest' if s['kind'] == 'nest' else 'nest rv', c))
            L['main' if s['kind'] == 'nest' else 'rival'].append(mk(s['p'][0], s['p'][1], '<circle r="9" class="%s"/><text class="mt" y="3.5">%s</text>' % ('nm' if s['kind'] == 'nest' else 'nm rvm', 'Г' if s['kind'] == 'nest' else 'О'), c))
        if s['kind'] == 'extract':
            c = card(s['ru'], 'вынос', [('точка', '(%d, %d)' % (s['p'][0], s['p'][1])), ('в коде', 'второй круг сдачи (v2, KitchenLoot.cpp)' if s.get('needs_code') else 'как в v1')])
            L['main'].append(mk(s['p'][0], s['p'][1], '<path d="M0,-11 L10,7 L-10,7 Z" class="ex"/><text class="mt" y="5">В</text>', c))
    # connectors and hauling paths
    for k in D['connectors']:
        haul = k.get('haul')
        rows = [('вид', KIND_RU.get(k['kind'], k['kind'])), ('откуда - куда', '%s -> %s' % (TIER_RU.get(k['from_tier'], k['from_tier']), TIER_RU.get(k['to_tier'], k['to_tier']))), ('перепад', '%d uu' % k.get('height_uu', 0)),
                ('с грузом', HAUL_RU.get(haul)), ('заметка', k.get('note'))]
        c = card(k['ru'], 'новый путь' if k.get('v2') == 'new' else 'путь', rows)
        p0 = k['pts'][0]
        if haul in ('ramp', 'stairs', 'flat'):
            L['haul'].append(line(k['pts'], 'hl ' + haul, c)); L['haul'].append(mk(p0[0], p0[1], '<path d="M-7,6 L7,6 L7,-7 Z" class="hm"/>', c))
        else:
            if len(k['pts']) > 1 and math.hypot(k['pts'][-1][0] - p0[0], k['pts'][-1][1] - p0[1]) > 300: L['conn'].append(line(k['pts'], 'cl', c))
            L['conn'].append(mk(p0[0], p0[1], '<path d="M0,-7 L6,5 L-6,5 Z" class="cm%s"/>' % (' new' if k.get('v2') == 'new' else ''), c))
    # loot
    lv = {l['id']: l for l in A['loot']}
    for l in A['loot']:
        x, y, z = l['p']
        rows = [('ценность', num(l['ev'], 0)), ('где', '%s; %s; %s' % (AREA_RU[l['area']], TIER_RU.get(l['tier'], l['tier']), l.get('place', ''))), ('высота', '%d' % z), ('от гнезда', '%s с бегом' % num(l['t'], 0)),
                ('Человек', 'не достаёт' if l.get('cover') == 2 else 'достаёт'), ('за дверью', 'да' if l.get('door') else None)]
        if l['kind'] == 'main':
            c = card(l['name'], 'главный объект рейда', rows + [('место', l['label'])])
            L['main'].append(mk(x, y, '<path d="M0,-15 L4.4,-4.6 L15,-4.6 L6.5,2.4 L9.5,13 L0,6.6 L-9.5,13 L-6.5,2.4 L-15,-4.6 L-4.4,-4.6 Z" class="mo"/><text class="mt" y="4">%s</text>' % l['id'][1], c)); continue
        c = card(l['name'], 'еда', rows + [('подпись', l.get('label'))])
        t = l['lvl']
        if l['kind'] == 'heavy': inner = '<rect x="-6" y="-6" width="12" height="12" class="lt3"/>'
        elif l['kind'] == 'trophy': inner = '<path d="M0,-9 L2.6,-2.8 L9,-2.8 L3.9,1.4 L5.7,8 L0,4 L-5.7,8 L-3.9,1.4 L-9,-2.8 L-2.6,-2.8 Z" class="lt3"/>'
        else: inner = '<circle r="%s" class="lt%d"/>' % ((2.2, 3.4, 4.6, 6)[t], t)
        L['loot'].append(mk(x, y, inner, c, 'door' if l.get('door') else ''))
    # the 1 m cells of the audit (floor)
    for cc in A['cells']:
        if cc['tier'] != 'floor': continue
        r = cc['rect']; L['cells'].append(rect(r[0], r[1], r[2], r[3], 'ce ' + cc['cls']))
    return L

L1, L2 = build(D1, A1, False), build(D2, A2, True)
# v1 extras: the audit's routes
for r in A1['routes']:
    c = card('Маршрут %s: %s' % (r['id'][1:], r['ru']), 'маршрут (как есть)', [('длина', '%s м' % num(r['len_m'])), ('бегом', '%s с' % num(r['t_sprint'], 0)), ('на виду', '%s с' % num(r['exposed_s'], 0)), ('назад с грузом', '%s-%s с' % (num(r['t_back_loaded'][0], 0), num(r['t_back_loaded'][1], 0)))])
    L1['route'].append(line(r['pts'], 'rt open', c))
    L1['route'].append(mk(r['pts'][-1][0], r['pts'][-1][1], '<circle r="7" class="nd"/><text class="mt" y="3.5">%s</text>' % r['id'][1:], c))
# v2 extras: districts, the route graph
for d in D2['districts']:
    c = card(d['ru'], 'район', [('роль в рейде', d['role']), ('вид', d['look']), ('добыча', d['loot']), ('опасность', d['danger']), ('правило', d['rule']), ('входы и выходы', d['ways']), ('ориентир', d['landmark'])])
    for r in d['rects']: L2['dist'].append('<rect x="%s" y="%s" width="%s" height="%s" class="di" style="fill:%s;stroke:%s" data-c="%d"/>' % (X(r[0]), Y(r[3]), (r[2] - r[0]) / 100.0, (r[3] - r[1]) / 100.0, d['color'], d['color'], c))
    r = max(d['rects'], key=lambda r: (r[2] - r[0]) * (r[3] - r[1]))
    L2['dist'].append(label((r[0] + r[2]) / 2, (r[1] + r[3]) / 2, d['ru'], 'lb dn'))
ROUTE = {r['id']: r for r in CH['graph']['routes']}
EK = {'open': 'быстро, на виду', 'cover': 'вдоль укрытий', 'wall': 'внутри стены', 'out': 'снаружи, без Человека'}
for ed in CH['graph']['edges']:
    c = card('%s — %s' % (next(n['ru'] for n in D2['graph']['nodes'] if n['id'] == ed['a']), next(n['ru'] for n in D2['graph']['nodes'] if n['id'] == ed['b'])), 'ребро графа',
             [('вид', EK[ed['kind']]), ('длина', '%s м' % num(ed['len_m'])), ('бегом', '%s с' % num(ed['t_sprint'])), ('с грузом', '%s-%s с' % (num(ed['t_loaded'][0], 0), num(ed['t_loaded'][1], 0))), ('на виду', '%s с (%d %%)' % (num(ed['exposed_s']), ed['exposed_pct'])), ('заметка', ed['note'])])
    L2['route'].append(line(ed['pts'], 'rt ' + ed['kind'], c))
JN = {j['node']: j['id'] for j in D2['graph']['junctions']}
for n in D2['graph']['nodes']:
    inr = [r['id'] for r in D2['graph']['routes'] if n['id'] in r['fast'] or n['id'] in r['covered']]
    c = card(n['ru'] + ((' (' + JN[n['id']] + ')') if n['id'] in JN else ''), 'узел: позывной для HUD', [('район', next(d['ru'] for d in D2['districts'] if d['id'] == n['district'])), ('роль', {'home': 'дом', 'entry': 'вход / лаз', 'junction': 'перекрёсток: место встреч и засад', 'goal': 'цель', 'pass': 'проход', 'up': 'путь наверх', 'enemy': 'гнездо соперников'}[n['role']]),
                                                          ('рёбер', CH['graph']['degree'].get(n['id'])), ('пути вверх и вниз', ', '.join(n['ups'])), ('маршруты', ', '.join(inr))])
    L2['route'].append(mk(n['p'][0], n['p'][1], '<circle r="%s" class="nd%s"/>' % (9 if n['id'] in JN else 6, ' jn' if n['id'] in JN else ''), c))
    L2['label'].append(label(n['p'][0], n['p'][1] + 330, n['ru'], 'lb nn'))

LAYERS = [('geo', 'Стены и мебель', 1, 'Чем темнее, тем выше. В «стало»: бирюзовая рамка = новый предмет, пунктир = передвинут.'),
          ('dist', 'Районы (итог)', 1, '14 районов: у каждого свой вид, добыча, опасность и правило. Нажми на район.'),
          ('route', 'Маршруты и узлы', 1, 'Красный = быстро и на виду, зелёный = вдоль укрытий, фиолетовый пунктир = в стене, синий = снаружи. Большие кружки = перекрёстки J1-J4. В «было» — девять маршрутов аудита.'),
          ('main', 'Гнездо, главный объект, вынос', 1, 'Звезда с цифрой = место главного объекта (одно из трёх за рейд). Треугольник В = точка выноса. Г = гнездо.'),
          ('haul', 'Пути с грузом', 1, 'Оранжевые линии: сплошная = пандус, штрих = лесенка (только вниз), точки = ровный лаз.'),
          ('loot', 'Еда', 0, 'Круг = точка еды, размер и цвет = уровень 0-3; квадрат = тяжёлый приз; звёздочка = трофей; пунктирное кольцо = за дверью.'),
          ('cover', 'Укрытия и правило 3-4 с', 0, 'Зелёное = укрытие. Жёлтое = до укрытия 3-4 с бегом. Оранжевое = 4-6 с. Красное = дольше. Зелёный контур = укрытие-предмет (пунктир: нужна таблица укрытий в коде).'),
          ('rival', 'Земля огненных', 1, 'Красная штриховка = территория соперников, О = их гнездо.'),
          ('haz', 'Опасности', 0, 'Жар, холод, вода, ток, липкое, ноги людей, Бадрик, поливалка, робот.'),
          ('conn', 'Пути наверх и лазы', 0, 'Треугольник = путь наверх или лаз (бирюзовый = новый).'),
          ('human', 'Человек', 0, 'Оранжевая заливка = где он может встать. Рамки = коробки блуждания. Точки = остановки патруля.'),
          ('cells', 'Аудит: клетки 1 x 1 м', 0, 'Серое = мёртвая, без заливки = транзит, оранжевое = спорная, зелёное = безопасная.'),
          ('label', 'Подписи', 1, 'Комнаты и позывные узлов.')]
ORDER = ['base', 'cells', 'dist', 'geo', 'cover', 'human', 'haz', 'rival', 'conn', 'haul', 'route', 'loot', 'main', 'label']
def state(L, n): return '<g id="st%d">' % n + ''.join('<g class="ly" data-ly="%s">%s</g>' % (k, ''.join(L[k])) for k in ORDER) + '</g>'
SVG = state(L1, 1) + state(L2, 2)

# ---- the summary: before / after -------------------------------------------------------------------------------------------------
T1, T2 = CH['totals']['v1'], CH['totals']['v2']
C1, C2, C2N = CH['cover']['v1'], CH['cover']['v2'], CH['cover']['v2_without_hide_table']
def tr(*c): return '<tr>' + ''.join('<td>%s</td>' % x for x in c) + '</tr>'
rows = []
for k, ru in (('kitchen/floor', 'Кухня, пол'), ('kitchen/work', 'Кухня, столы'), ('kitchen/top', 'Кухня, верха'), ('living/floor', 'Гостиная, пол'), ('living/work', 'Гостиная, столы'), ('living/top', 'Гостиная, верха'), ('yard/floor', 'Двор'), ('street/floor', 'Улица')):
    rows.append(tr(ru, '%d %%' % T1[k]['dead_pct'], '<b>%d %%</b>' % T2[k]['dead_pct'], '%d %%' % T1[k]['contested_pct'], '<b>%d %%</b>' % T2[k]['contested_pct']))
SUM = '<h3>Мёртвые и спорные клетки</h3><table><tr><th></th><th>мёртвых было</th><th>стало</th><th>спорных было</th><th>стало</th></tr>' + ''.join(rows) + '</table>'
SUM += '<h3>Укрытия</h3><table><tr><th></th><th>было</th><th>стало</th><th>стало без таблицы укрытий в коде</th></tr>'
for a, ru in (('kitchen', 'Кухня'), ('living', 'Гостиная')):
    SUM += tr(ru + ': укрытие, % пола', num(C1[a]['cover_pct_x10']), '<b>%s</b>' % num(C2[a]['cover_pct_x10']), num(C2N[a]['cover_pct_x10']))
    SUM += tr(ru + ': худший бег до укрытия, с', num(C1[a]['worst_s']), '<b>%s</b>' % num(C2[a]['worst_s']), num(C2N[a]['worst_s']))
    SUM += tr(ru + ': пол дальше 4 с от укрытия, %', num(C1[a]['over4s_pct']), '<b>%s</b>' % num(C2[a]['over4s_pct']), num(C2N[a]['over4s_pct']))
SUM += '</table><h3>Пути и добыча</h3><table><tr><th></th><th>было</th><th>стало</th></tr>'
W1, W2 = CH['ways']['v1'], CH['ways']['v2']
g1, g2 = CH['gradient']['v1'], CH['gradient']['v2']
SUM += tr('Входов в дом со стороны гнезда', 1, '<b>2</b> (+ щель под окном)') + tr('Перекрёстков, где маршруты сходятся', 0, '<b>4</b>') + tr('Связей кухня - гостиная', 1, '<b>2</b> (+ кошачья дверца через двор)')
SUM += tr('Пандусов, по которым тащат волоком', '0 (ложка круче 55°)', '<b>%d</b>' % len(CH['hauling']['v2']['ramps'])) + tr('Лесенок для спуска груза', '2 (дверцы духовки и посудомойки)', '<b>%d</b>' % len(CH['hauling']['v2']['stairs']))
SUM += tr('Поверхностей без единого задуманного пути', '%d из %d' % (W1['no_way'], W1['surfaces']), '<b>%d из %d</b>' % (W2['no_way'], W2['surfaces'])) + tr('Поверхностей с двумя путями и больше', W1['two_or_more'], '<b>%d</b>' % W2['two_or_more'])
SUM += tr('Пустых поверхностей', W1['empty'], '<b>%d</b>' % W2['empty']) + tr('Еда ур. 2-3 ближе 15 с от гнезда', g1[0]['tier2plus'], '<b>%d</b>' % g2[0]['tier2plus'])
SUM += tr('Средняя ценность точки: до 15 с / 15-25 / 25-40 / дальше', ' / '.join(num(b['ev_mean'], 0) for b in g1), '<b>' + ' / '.join(num(b['ev_mean'], 0) for b in g2) + '</b>')
SUM += tr('Двор и улица, м²', num(CH['areas_m2']['v1']['yard'] + CH['areas_m2']['v1']['street'], 0), '<b>%s</b>' % num(CH['areas_m2']['v2']['yard'] + CH['areas_m2']['v2']['street'], 0))
SUM += tr('Точек еды', CH['counts']['v1']['loot_spots'], '<b>%d</b> + 3 места главного объекта' % CH['counts']['v2']['loot_spots']) + '</table>'
SUM += '<h3>Маршруты «стало»: быстрый и в укрытии</h3><table><tr><th>куда</th><th>быстрый: м / с / на виду, с</th><th>в укрытии: м / с / на виду, с</th></tr>'
for r in CH['graph']['routes']:
    SUM += tr(e(r['ru']), '%s / %s / %s' % (num(r['fast']['len_m']), num(r['fast']['t_sprint'], 0), num(r['fast']['exposed_s'], 0)), '%s / %s / %s' % (num(r['covered']['len_m']), num(r['covered']['t_sprint'], 0), num(r['covered']['exposed_s'], 0)))
SUM += '</table><p class="mut">Всё посчитано скриптами Tools/level по данным, игра не запускалась. Времена — бег без груза (456 uu/с). «На виду» — секунды в клетках, куда достаёт топот. Полный текст: docs/level/LEVEL_V2.md. Собрано %s.</p>' % e(D2['meta']['generated'])
NEED = '<h3>Код уровня v2 (сделан 20.11.2026)</h3><ul>' + ''.join('<li>%s<br><b>сделано:</b> %s</li>' % (e(n['ru']), e(n.get('done', '—'))) for n in D2['needs_code']) + '</ul>'
VIEWS = {'1': {'all': [-16700, -16300, 21300, 6000], 'kitchen': [-7500, -5500, 7500, 5900], 'living': [6800, -9400, 20600, 1900], 'yard': [-12300, -16300, 21300, -4900], 'street': [-16700, -5600, -6700, 6000]},
         '2': {'all': [-15600, -12500, 20700, 5900], 'kitchen': [-7500, -5500, 7500, 5900], 'living': [6800, -9400, 20600, 1900], 'yard': [-9700, -12500, 20700, -4900], 'street': [-15600, -5600, -6700, 6000]}}
PAGE = r'''<!DOCTYPE html>
<html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Ant Kitchen: карта уровня v2 (было / стало)</title>
<style>
:root{--bg:#f4f1ea;--fg:#1f2428;--mut:#6b7178;--pan:#fffdf8;--line:#cfc9bd;--acc:#0e7c86;--k:1}
@media (prefers-color-scheme:dark){:root{--bg:#15181b;--fg:#e8e6e1;--mut:#9aa0a6;--pan:#1e2327;--line:#3a4046;--acc:#4fc3cc}}
*{box-sizing:border-box}html,body{margin:0;height:100%;background:var(--bg);color:var(--fg);font:14px/1.35 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;overflow:hidden}
#top{position:fixed;left:0;right:0;top:0;z-index:5;background:var(--pan);border-bottom:1px solid var(--line);padding:6px 8px;display:flex;flex-wrap:wrap;gap:6px;align-items:center}
#top h1{font-size:14px;margin:0 6px 0 0;white-space:nowrap}
.sw{display:flex;border:1px solid var(--acc);border-radius:8px;overflow:hidden}
.sw button{border:0;background:transparent;color:var(--fg);padding:7px 12px;font:inherit;font-weight:600}
.sw button.on{background:var(--acc);color:#fff}
.chips{display:flex;gap:4px;overflow-x:auto}
.chip,.btn{border:1px solid var(--line);background:transparent;color:var(--fg);border-radius:14px;padding:6px 10px;font:inherit;white-space:nowrap}
.chip.on{border-color:var(--acc);color:var(--acc)}
svg{position:fixed;left:0;top:0;width:100%;height:100%;touch-action:none;background:var(--bg)}
.sheet{position:fixed;left:0;right:0;bottom:0;z-index:6;background:var(--pan);border-top:1px solid var(--line);max-height:62%;overflow:auto;padding:10px 12px 16px;transform:translateY(105%);transition:transform .18s}
.sheet.open{transform:none}
@media (min-width:760px){.sheet{left:auto;right:10px;bottom:10px;width:420px;max-height:80%;border:1px solid var(--line);border-radius:10px}}
.sheet h2{font-size:16px;margin:0 30px 2px 0}.sheet h3{font-size:14px;margin:12px 0 4px}.kind{color:var(--mut);font-size:12px;margin-bottom:6px}
.x{position:absolute;right:8px;top:6px;border:0;background:transparent;color:var(--fg);font-size:22px;padding:4px 8px}
.row{display:flex;gap:8px;padding:3px 0;border-top:1px solid var(--line)}.row b{flex:0 0 34%;font-weight:600;color:var(--mut)}
.lr{display:block;padding:7px 0;border-top:1px solid var(--line)}.lr input{margin-right:8px;transform:scale(1.25)}.ln{font-weight:600}.ld{display:block;color:var(--mut);font-size:12px;margin-left:26px}
table{border-collapse:collapse;width:100%;font-size:12.5px}td,th{border-top:1px solid var(--line);padding:3px 4px;text-align:left;vertical-align:top}th{color:var(--mut);font-weight:600}
.mut{color:var(--mut);font-size:12px}
.ar-kitchen{fill:#efe6cf}.ar-living{fill:#e6d3bd}.ar-yard{fill:#cfe3b9}.ar-street{fill:#d9dccf}
.wall{fill:#5a5148}.fence{fill:#b8a58a}.blocker{fill:none;stroke:#b8a58a;stroke-width:.6;stroke-dasharray:1.5 1.5}
.o{stroke:#00000030;stroke-width:.25}.h0{fill:#00000012}.h1{fill:#0000002a}.h2{fill:#00000048}.h3{fill:#0000006a}.part{fill:none;stroke:#3b6fb0;stroke-width:.3}.flat{fill:#ffffff40;stroke-dasharray:1 1}
.crown{fill:#3f8f3f18;stroke:#3f8f3f;stroke-width:.5;stroke-dasharray:2 2}.o.new{stroke:#0e9aa7;stroke-width:.9}.o.copy{stroke:#0e9aa7;stroke-width:.6;stroke-dasharray:1.2 .8}.o.moved{stroke:#b05f00;stroke-width:.8;stroke-dasharray:2 1.2}
.di{fill-opacity:.22;stroke-width:.7;stroke-opacity:.9}
.cv0{fill:#2e9c4b66}.cv3{fill:#e6c40055}.cv4{fill:#e6c40099}.cv6{fill:#f07a1acc}.cv9{fill:#d81e1ee6}
.hide{fill:#2e9c4b40;stroke:#1d7a36;stroke-width:.5}.hide.need{stroke-dasharray:1.6 1}
.hs{fill:#f08a2440}.hbox{fill:none;stroke:#d06a00;stroke-width:.5;stroke-dasharray:3 2}.hp{fill:#d06a00}
.hz{fill:#d81e1e2e;stroke:#b01212;stroke-width:.4}.hz.pet{fill:#d81e1e14;stroke-dasharray:1.5 1.5}.hz.rv{fill:#c8553d33;stroke:#c8553d;stroke-width:.6;stroke-dasharray:3 1.5}
.nest{fill:#0e7c8622;stroke:#0e7c86;stroke-width:.6;stroke-dasharray:2 1.5}.nest.rv{fill:#c8553d30;stroke:#c8553d}
.nm{fill:#0e7c86;stroke:#fff;stroke-width:1}.nm.rvm{fill:#c8553d}.ex{fill:#0e7c86;stroke:#fff;stroke-width:1}.mo{fill:#f2c200;stroke:#5a4300;stroke-width:1.2}
.mt{font:700 10px system-ui,sans-serif;fill:#fff;text-anchor:middle;pointer-events:none}.mo+.mt{fill:#3a2b00}
.cl{fill:none;stroke:#5b6b7a;stroke-width:.5;stroke-dasharray:1.5 1.5}.cm{fill:#5b6b7a;stroke:#fff;stroke-width:.8}.cm.new{fill:#0e9aa7}
.hl{fill:none;stroke:#f07a1a;stroke-width:1.3;stroke-linecap:round}.hl.stairs{stroke-dasharray:2.5 1.6}.hl.flat{stroke-dasharray:.4 2}.hm{fill:#f07a1a;stroke:#fff;stroke-width:.8}
.rt{fill:none;stroke-width:1.1;stroke-linecap:round;stroke-linejoin:round;opacity:.9}.rt.open{stroke:#d81e1e}.rt.cover{stroke:#1d8a3d}.rt.wall{stroke:#7a4fb5;stroke-dasharray:2.4 1.6}.rt.out{stroke:#2b6fd1}
.nd{fill:#fff;stroke:#1f2428;stroke-width:1.6}.nd.jn{fill:#f2c200;stroke-width:2.2}
.lt0{fill:#9aa0a6}.lt1{fill:#5aa9e6}.lt2{fill:#f08a24}.lt3{fill:#d81e1e}.mk.door .mi>*{stroke:#1f2428;stroke-width:1.2;stroke-dasharray:2 1.6}
.ce{stroke:#00000018;stroke-width:.15;fill:none}.ce.dead{fill:#50505077}.ce.contested{fill:#f08a2455}.ce.safe{fill:#2e9c4b4d}
.mi{transform:scale(var(--k))}
.lb{font:600 calc(11px*var(--k)) system-ui,sans-serif;fill:var(--fg);text-anchor:middle;pointer-events:none;paint-order:stroke;stroke:var(--bg);stroke-width:calc(3px*var(--k));stroke-linejoin:round}
.lb.room{font-size:calc(15px*var(--k));fill:var(--mut)}.lb.dn{font-size:calc(12px*var(--k))}.lb.nn{font-weight:500;font-size:calc(10px*var(--k))}
[data-c]{cursor:pointer}.sel{filter:drop-shadow(0 0 1.2px #0e7c86) drop-shadow(0 0 1.2px #0e7c86)}
</style></head><body>
<div id="top"><h1>Ant Kitchen · уровень</h1>
<div class="sw" id="sw"><button data-s="1">Было</button><button data-s="2" class="on">Стало</button></div>
<div class="chips" id="views"><button class="chip on" data-v="all">Всё</button><button class="chip" data-v="kitchen">Кухня</button><button class="chip" data-v="living">Гостиная</button><button class="chip" data-v="yard">Двор</button><button class="chip" data-v="street">Улица</button></div>
<button class="btn" id="bl">Слои</button><button class="btn" id="bs">Итоги</button><button class="btn" id="bn">Код v2</button></div>
<svg id="map" xmlns="http://www.w3.org/2000/svg">@@SVG@@</svg>
<div class="sheet" id="card"><button class="x" data-x="card">×</button><h2 id="ct"></h2><div class="kind" id="ck"></div><div id="cr"></div></div>
<div class="sheet" id="layers"><button class="x" data-x="layers">×</button><h2>Слои</h2>@@LAYERS@@</div>
<div class="sheet" id="summary"><button class="x" data-x="summary">×</button><h2>Было / стало</h2>@@SUMMARY@@</div>
<div class="sheet" id="need"><button class="x" data-x="need">×</button><h2>Что не заработает без кода</h2>@@NEED@@</div>
<script id="cards" type="application/json">@@CARDS@@</script>
<script>
(function(){
var svg=document.getElementById('map'),CARDS=JSON.parse(document.getElementById('cards').textContent),VIEWS=@@VIEWS@@,vb=[0,0,100,100],st=2,sel=null,P={},moved=0,pd=0;
function size(){var r=svg.getBoundingClientRect();return [Math.max(60,r.width),Math.max(60,r.height),r.left,r.top];}
function apply(){svg.setAttribute('viewBox',vb.join(' '));var s=size();document.documentElement.style.setProperty('--k',(vb[2]/s[0]).toFixed(4));}
function fit(v){var s=size(),top=document.getElementById('top').offsetHeight,x0=v[0]/100,x1=v[2]/100,y0=-v[3]/100,y1=-v[1]/100,w=x1-x0,h=y1-y0,ah=s[1]-top,a=s[0]/ah;
 if(w/h>a){h=w/a;}else{w=h*a;}var cx=(x0+x1)/2,cy=(y0+y1)/2,k=w/s[0];vb=[cx-w/2,cy-h/2-top*k/2-top*k/2,w,h+top*k];vb[1]=cy-h/2-top*k;vb[3]=h+top*k;apply();}
function zoomAt(px,py,f){var s=size(),k=vb[2]/s[0],ux=vb[0]+(px-s[2])*k,uy=vb[1]+(py-s[3])*k,nw=Math.max(8,Math.min(900,vb[2]*f));f=nw/vb[2];vb=[ux-(ux-vb[0])*f,uy-(uy-vb[1])*f,nw,vb[3]*f];apply();}
function n(){return Object.keys(P).length;}
svg.addEventListener('pointerdown',function(ev){svg.setPointerCapture(ev.pointerId);P[ev.pointerId]=[ev.clientX,ev.clientY];moved=0;if(n()===2){var a=Object.keys(P);pd=Math.hypot(P[a[0]][0]-P[a[1]][0],P[a[0]][1]-P[a[1]][1]);}});
svg.addEventListener('pointermove',function(ev){var p=P[ev.pointerId];if(!p)return;var s=size(),k=vb[2]/s[0],dx=ev.clientX-p[0],dy=ev.clientY-p[1];
 if(n()===1){moved+=Math.abs(dx)+Math.abs(dy);vb[0]-=dx*k;vb[1]-=dy*k;apply();}P[ev.pointerId]=[ev.clientX,ev.clientY];
 if(n()===2){var a=Object.keys(P),d=Math.hypot(P[a[0]][0]-P[a[1]][0],P[a[0]][1]-P[a[1]][1]);if(pd>0&&d>0){zoomAt((P[a[0]][0]+P[a[1]][0])/2,(P[a[0]][1]+P[a[1]][1])/2,pd/d);}pd=d;moved=99;}});
function up(ev){if(!P[ev.pointerId])return;delete P[ev.pointerId];if(n()===0&&moved<9&&ev.type==='pointerup'){var el=document.elementFromPoint(ev.clientX,ev.clientY);el=el&&el.closest?el.closest('[data-c]'):null;if(el)show(el);else close('card');}}
svg.addEventListener('pointerup',up);svg.addEventListener('pointercancel',up);
svg.addEventListener('wheel',function(ev){ev.preventDefault();zoomAt(ev.clientX,ev.clientY,ev.deltaY>0?1.2:1/1.2);},{passive:false});
function show(el){if(sel)sel.classList.remove('sel');sel=el;el.classList.add('sel');var c=CARDS[+el.getAttribute('data-c')];if(!c)return;
 document.getElementById('ct').textContent=c[0];document.getElementById('ck').textContent=c[1];var h='';for(var i=0;i<c[2].length;i++){h+='<div class="row"><b></b><span></span></div>';}
 var r=document.getElementById('cr');r.innerHTML=h;var rows=r.querySelectorAll('.row');for(i=0;i<rows.length;i++){rows[i].firstChild.textContent=c[2][i][0];rows[i].lastChild.textContent=c[2][i][1];}open('card');}
function open(id){['card','layers','summary','need'].forEach(function(x){document.getElementById(x).classList.toggle('open',x===id);});}
function close(id){document.getElementById(id).classList.remove('open');if(id==='card'&&sel){sel.classList.remove('sel');sel=null;}}
document.addEventListener('click',function(ev){var b=ev.target.closest('.x');if(b)close(b.getAttribute('data-x'));});
var cur='all';function setState(s){st=s;document.getElementById('st1').style.display=s===1?'':'none';document.getElementById('st2').style.display=s===2?'':'none';
 Array.prototype.forEach.call(document.querySelectorAll('#sw button'),function(b){b.classList.toggle('on',+b.getAttribute('data-s')===s);});close('card');fit(VIEWS[st][cur]);}
document.getElementById('sw').addEventListener('click',function(ev){var b=ev.target.closest('button');if(b)setState(+b.getAttribute('data-s'));});
function setView(v){cur=v;Array.prototype.forEach.call(document.querySelectorAll('#views .chip'),function(b){b.classList.toggle('on',b.getAttribute('data-v')===v);});fit(VIEWS[st][v]);}
document.getElementById('views').addEventListener('click',function(ev){var b=ev.target.closest('.chip');if(b)setView(b.getAttribute('data-v'));});
function layer(id,on){Array.prototype.forEach.call(svg.querySelectorAll('g.ly[data-ly="'+id+'"]'),function(g){g.style.display=on?'':'none';});var cb=document.querySelector('#layers input[data-ly="'+id+'"]');if(cb)cb.checked=on;}
document.getElementById('layers').addEventListener('change',function(ev){var c=ev.target;if(c.getAttribute('data-ly'))layer(c.getAttribute('data-ly'),c.checked);});
document.getElementById('bl').addEventListener('click',function(){open('layers');});document.getElementById('bs').addEventListener('click',function(){open('summary');});document.getElementById('bn').addEventListener('click',function(){open('need');});
Array.prototype.forEach.call(document.querySelectorAll('#layers input'),function(c){layer(c.getAttribute('data-ly'),c.checked);});
var h={};location.hash.replace(/^#/,'').split('&').forEach(function(kv){var p=kv.split('=');if(p[0])h[p[0]]=decodeURIComponent(p[1]||'');});
cur=VIEWS[2][h.v]?h.v:'all';setState(h.s==='1'?1:2);
if(h.ly){var want=h.ly.split(',');Array.prototype.forEach.call(document.querySelectorAll('#layers input'),function(c){layer(c.getAttribute('data-ly'),want.indexOf(c.getAttribute('data-ly'))>=0);});layer('base',true);}
setView(cur);
if(h.p){open(h.p);}
if(h.c){var el=document.querySelector('#st'+st+' [data-c="'+h.c+'"]');if(el)show(el);}
window.addEventListener('resize',function(){fit(VIEWS[st][cur]);});
})();
</script></body></html>
'''
layers_html = ''.join('<label class="lr"><input type="checkbox" data-ly="%s"%s><span class="ln">%s</span><span class="ld">%s</span></label>' % (k, ' checked' if on else '', e(nm), e(desc)) for (k, nm, on, desc) in LAYERS)
cards_json = json.dumps(CARDS, ensure_ascii=False, separators=(',', ':')).replace('</', '<\\/')
page = PAGE.replace('@@SVG@@', SVG).replace('@@LAYERS@@', layers_html).replace('@@SUMMARY@@', SUM).replace('@@NEED@@', NEED).replace('@@VIEWS@@', json.dumps(VIEWS)).replace('@@CARDS@@', cards_json)
problems = []
try: json.loads(cards_json.replace('<\\/', '</'))
except Exception as ex: problems.append('cards JSON does not parse: %s' % ex)
try:
    root = ET.fromstring('<svg>' + SVG + '</svg>'); n_el = sum(1 for _ in root.iter())
    mx = max([int(el.get('data-c')) for el in root.iter() if el.get('data-c') is not None] + [0])
    if mx >= len(CARDS): problems.append('a marker points at card %d of %d' % (mx, len(CARDS)))
except Exception as ex:
    problems.append('SVG is not well-formed XML: %s' % ex); n_el = 0
low = page.lower()
for bad in ('http:', 'https:', '<link', '@import', 'src="', "src='", 'url('):
    if bad in low.replace('xmlns="http://www.w3.org/2000/svg"', ''): problems.append('external reference in the HTML: %s' % bad)
size_kb = len(page.encode('utf-8')) / 1024.0
if size_kb > 1500: problems.append('the file is %.0f KB (limit about 1500)' % size_kb)
viol = CH['cover']['v2_cells_over_4s']
if viol: problems.append('%d indoor cells of v2 are further than 4 s from cover' % viol)
with open(os.path.join(LV, 'level_map_v2.html'), 'w', encoding='utf-8', newline='\n') as fo: fo.write(page)
print('wrote level_map_v2.html: %.0f KB, %d SVG elements, %d cards' % (size_kb, n_el, len(CARDS)))
print('CHECKS OK' if not problems else 'CHECKS FAILED:\n  ' + '\n  '.join(problems))
sys.exit(1 if problems else 0)