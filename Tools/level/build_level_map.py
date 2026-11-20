# -*- coding: utf-8 -*-
# Ant Kitchen, level map, step 3: docs/level/level_data.json + level_audit.json -> docs/level/level_map.html
#   python Tools/level/build_level_map.py
# One self-contained HTML file (inline CSS / JS / SVG, no network). Phone first: drag, pinch, tap an object for its card.
# The script checks its own output: the JSON parses, every object lies inside its area (or is listed as suspicious),
# the SVG is well-formed XML, the HTML has no external URLs.
import json, os, re, sys, math, html
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
OUT = None
for _i, _a in enumerate(sys.argv):
    if _a == '--root': ROOT = sys.argv[_i + 1]
    if _a == '--out': OUT = sys.argv[_i + 1]
OUT = OUT or os.path.join(ROOT, 'docs', 'level')
D = json.load(open(os.path.join(OUT, 'level_data.json'), encoding='utf-8'))
A = json.load(open(os.path.join(OUT, 'level_audit.json'), encoding='utf-8'))
UU = 1900.0
S = 100.0                     # 1 SVG unit = 100 uu; +Y (north) is up, so SVG y = -Y

def sx(x): return round(x / S, 2)
def sy(y): return round(-y / S, 2)
def e(s): return html.escape(str(s), quote=True)
def mtr(v, d=2): return ('%.*f' % (d, v / UU)).replace('.', ',')
def num(v): return ('%g' % round(v, 1)).replace('.', ',')

CARDS = []
def card(title, kind, rows):
    CARDS.append({'t': title, 'k': kind, 'r': [[a, str(b)] for (a, b) in rows if b not in (None, '', [])]})
    return len(CARDS) - 1

AREA_RU = {'kitchen': 'Кухня', 'living': 'Гостиная', 'yard': 'Двор', 'street': 'Улица'}
TIER_RU = {'floor': 'пол', 'under': 'под мебелью', 'work': 'столешницы и столы', 'top': 'верха и полки', 'int': 'внутри', 'vent': 'вентиляция и за стенами'}
TIER_CODE = {'floor': 'f', 'under': 'u f', 'work': 'w', 'top': 't', 'int': 'i', 'vent': 'v'}
CAT_RU = {'furn': 'мебель / техника', 'part': 'дверца, ящик, крышка (двигается)', 'prop': 'предмет', 'food': 'еда-декор', 'deco': 'декор на стене / потолке', 'struct': 'оболочка: плинтус, забор, покрытие', 'conn': 'по этому лезут'}
CLS_RU = {'dead': 'мёртвая', 'transit': 'транзит', 'contested': 'спорная', 'safe': 'безопасная'}
CLS_LET = {'dead': 'М', 'transit': 'Т', 'contested': 'С', 'safe': 'Б'}
V_SPRINT = A['meta']['speeds_uu_s']['sprint']; V_WALK = A['meta']['speeds_uu_s']['walk']; WALL_K = A['meta']['speeds_uu_s']['wall_factor']

def tier_by_z(z): return 'floor' if z < 300 else ('work' if z < 2100 else 'top')
INSIDE = [c['box'] for c in D['containers']] + [i['box'] for i in D['interiors']]
def obj_tiers(o):
    t = set()
    cx_, cy_ = o['c']; zc = (o['z'][0] + o['z'][1]) / 2.0
    inside = any(b[0] < cx_ < b[3] and b[1] < cy_ < b[4] and b[2] - 5 <= zc <= b[5] + 5 for b in INSIDE) and not o.get('interior') and o['cat'] != 'furn'
    if inside or o.get('rides_mover') is not None: return 'i'
    if o['z'][0] < 300: t.add('f')
    if 'gap_under' in o: t.add('u'); t.add('f')
    for z in o.get('tops', []): t.add(TIER_CODE[tier_by_z(z)])
    if o['z'][0] >= 300: t.add(TIER_CODE[tier_by_z(o['z'][0])])
    if o['z'][1] >= 2100 and o['z'][0] < 2100 and o['cat'] in ('furn', 'conn', 'struct', 'deco'): t.add('t')
    if o.get('interior'): t.add('i')
    if 'Vent' in str(o.get('mesh')): t.add('v')
    return ' '.join(sorted(t)) or 'f'

G = {k: [] for k in ('base', 'iso', 'heat', 'geo', 'cover', 'reach', 'human', 'dist', 'haz', 'conn', 'route', 'inter', 'spawn', 'event', 'loot', 'label')}

# ---- the ground and the walls ----------------------------------------------------------------------------------------
def rect(layer, x0, y0, x1, y1, cls, extra=''):
    G[layer].append('<rect x="%s" y="%s" width="%s" height="%s" class="%s" %s/>' % (sx(x0), sy(y1), round((x1 - x0) / S, 2), round((y1 - y0) / S, 2), cls, extra))
for a in D['areas']:
    for r in a['rects']: rect('base', r[0], r[1], r[2], r[3], 'ar ar-' + a['id'])
    for p in a.get('parts', []):
        r = p['rect']; rect('base', r[0], r[1], r[2], min(r[3], 5600), 'ar ar-' + {'газон с гнездом': 'yard', 'тротуар': 'pave', 'бордюр': 'kerb', 'дорога': 'road'}[p['ru']])
for sp in D['spaces']:
    if 'box' in sp and sp['id'] in ('mouse_room',):
        b = sp['box']; rect('base', b[0], b[1], b[3], b[4], 'ar ar-kitchen')
def wall_piece(x0, y0, x1, y1): rect('base', x0, y0, x1, y1, 'wall')
# walls with their real holes (crack, window crack, doorway, mouse room)
wall_piece(-7200, 5000, 3500, 5600); wall_piece(4300, 5000, 7200, 5600); wall_piece(3500, 5550, 4300, 5600); wall_piece(3500, 5000, 3800, 5050); wall_piece(3960, 5000, 4300, 5050)
wall_piece(-7200, -5200, -4600, -5000); wall_piece(-4400, -5200, 7200, -5000)
wall_piece(-7200, -5000, -7000, -2750); wall_piece(-7200, -2250, -7000, 5000)
wall_piece(7000, -5000, 7200, -4800); wall_piece(7000, -3200, 7200, 5000)
wall_piece(7200, 1500, 20400, 1700); wall_piece(7000, -9200, 20400, -9000); wall_piece(20200, -9000, 20400, 1500); wall_piece(7000, -9000, 7200, -5000)
G['base'].append('<line x1="%s" y1="%s" x2="%s" y2="%s" class="window"/>' % (sx(-1500), sy(-5100), sx(1500), sy(-5100)))

# ---- objects ---------------------------------------------------------------------------------------------------------
def obj_card(o):
    rows = [('Что', CAT_RU.get(o['cat'], o['cat'])), ('Зона', AREA_RU[o['area']]),
            ('Высота', 'низ %s, верх %s uu (%s–%s м)' % (num(o['z'][0]), num(o['z'][1]), mtr(o['z'][0]), mtr(o['z'][1]))),
            ('Габарит', '%s × %s uu (%s × %s м)' % (num(o['s'][0]), num(o['s'][1]), mtr(o['s'][0]), mtr(o['s'][1]))),
            ('Просвет под ним', ('%s uu (%s м)' % (num(o['gap_under']), mtr(o['gap_under']))) if 'gap_under' in o else None),
            ('Поверхности', ', '.join('z=%s' % num(z) for z in o['tops']) if o.get('tops') else None),
            ('Внутрь можно залезть', 'да' if o.get('interior') else None),
            ('Твёрдое, по нему ползают', 'да' if o.get('solid') else 'нет (без коллизии)')]
    mv = o.get('mover')
    if mv:
        rows += [('Открывается', '%s; %s' % (mv['open_text'], {'light': 'лёгкая: один муравей', 'medium': 'средняя: сила 24, около двух', 'heavy': 'тяжёлая: сила 40, около трёх'}[mv['door_class']])),
                 ('Движение', ('петля, %s°' % num(abs(mv['angle']))) if mv['type'] == 'hinge' else ('выдвигается на %s uu' % num(math.hypot(mv['slide'][0], mv['slide'][1])))),
                 ('Особенности', ', '.join({'starts_open': 'открыта с начала', 'needs_fridge_open': 'только при открытом холодильнике', 'needs_dishwasher_open': 'только при открытой посудомойке',
                                            'human_bumps_shut': 'Человек захлопывает, задев', 'human_opens': 'Человек иногда сам открывает на 20 с', 'human_and_pets_push_open': 'Человек и питомцы распахивают'}[f] for f in mv['flags'])),
                 ('Человек идёт закрывать', 'да' if mv['human_walks_over_to_shut'] else 'нет'), ('Сама закрывается', ('через %s с' % num(mv['auto_close_s'])) if mv['auto_close_s'] else None)]
    if o.get('gadget'): rows.append(('Управляется', o['gadget']))
    rows += [('Заметка', o.get('note')), ('Модель', ('%s #%d' % (o['mesh'], o['index'])) if o.get('mesh') else 'коробка из кода'), ('Источник', o['src'])]
    return card(o['ru'], 'объект', rows)

def hshade(z1): return 'h0' if z1 < 300 else ('h1' if z1 < 1000 else ('h2' if z1 < 2500 else 'h3'))
objs = sorted(D['objects'], key=lambda o: (o['z'][0] >= 300, o['z'][1]))
for o in objs:
    if o.get('mesh') in ('SM_S_Road', 'SM_S_PavementSlab_A', 'SM_S_Kerb'): continue      # drawn as the ground
    ci_ = obj_card(o)
    tiers = obj_tiers(o)
    opened = o.get('pose') == 'open' and 'bb_open' in o
    if o.get('mesh') == 'SM_Y_Tree':
        G['geo'].append('<circle cx="%s" cy="%s" r="%s" class="ob c-conn h3" data-t="f t" data-c="%d"/>' % (sx(o['origin'][0]), sy(o['origin'][1]), 5.2, ci_))
        G['geo'].append('<circle cx="%s" cy="%s" r="%s" class="canopy" data-t="t"/>' % (sx(o['origin'][0]), sy(o['origin'][1]), 41))
        continue
    cls = 'ob c-%s %s%s' % (o['cat'], hshade(o['z'][1]), '' if o.get('solid') else ' nosolid')
    if opened and 'c_open' in o: c, yaw = o['c_open'], o['yaw_open']
    elif opened: c, yaw = [(o['bb_open'][0] + o['bb_open'][2]) / 2, (o['bb_open'][1] + o['bb_open'][3]) / 2], o['yaw']
    else: c, yaw = o['c'], o['yaw']
    w, h = max(o['s'][0], 12) / S, max(o['s'][1], 12) / S
    tr = ' transform="rotate(%s %s %s)"' % (round(-yaw, 1), sx(c[0]), sy(c[1])) if abs(yaw) % 180 > 0.01 else ''
    if abs(abs(yaw) - 180) < 0.01 or abs(yaw) < 0.01: tr = ''
    G['geo'].append('<rect x="%s" y="%s" width="%s" height="%s" class="%s" data-t="%s" data-c="%d"%s/>' % (round(sx(c[0]) - w / 2, 2), round(sy(c[1]) - h / 2, 2), round(w, 2), round(h, 2), cls, tiers, ci_, tr))
    big = o['s'][0] * o['s'][1]
    if o['cat'] == 'furn' or (big >= 500000 and o['cat'] in ('prop', 'conn', 'food')):
        lvl = 2 if (o['cat'] == 'furn' and big >= 1500000) else 3
        if str(o.get('mesh')).startswith(('SM_L_Sofa_', 'SM_K_Plate', 'SM_K_WallCab', 'SM_K_Base1')) and o['index'] > 0: continue
        G['label'].append('<text x="%s" y="%s" class="lb l%d" data-t="%s">%s</text>' % (sx(c[0]), sy(c[1]), lvl, tiers, e(o['ru'].split(' (')[0])))
for a in D['areas']:
    r = a['rects'][0]
    G['label'].append('<text x="%s" y="%s" class="lb l1">%s</text>' % (sx((r[0] + r[2]) / 2), sy((r[1] + r[3]) / 2), e(a['ru'].upper())))

# ---- markers: constant size on screen (the inner group is scaled by the page) ----------------------------------------
def mk(layer, x, y, inner, tiers, ci_, cls=''):
    G[layer].append('<g transform="translate(%s %s)" data-t="%s"><g class="mk %s" data-c="%d">%s</g></g>' % (sx(x), sy(y), tiers, cls, ci_, inner))
def letter(ch, size=9): return '<text class="mt" font-size="%d" y="%s">%s</text>' % (size, round(size * 0.36, 1), e(ch))
def poly(pts): return ' '.join('%s,%s' % (round(p[0], 1), round(p[1], 1)) for p in pts)
def star(r):
    return poly([(math.sin(k * math.pi / 5) * (r if k % 2 == 0 else r * 0.45), -math.cos(k * math.pi / 5) * (r if k % 2 == 0 else r * 0.45)) for k in range(10)])
def hexa(r): return poly([(math.cos(k * math.pi / 3) * r, math.sin(k * math.pi / 3) * r) for k in range(6)])
def tsec(t): return '%s с бегом, %s с шагом' % (num(round(t)), num(round(t * V_SPRINT / V_WALK)))

# ---- loot -------------------------------------------------------------------------------------------------------------
SRC = {}
for s in D['loot']['spots']: SRC['L%d' % s['i']] = (s['src'], s.get('how', ''), s)
for s in D['loot']['behind_doors']: SRC['D%d' % s['i']] = (s['src'], 'literal in the code', s)
for s in D['loot']['heavy']: SRC['H%d' % s['i']] = (s['src'], 'literal in the code', s)
for s in D['loot']['trophies']: SRC['T%d' % s['i']] = (s['src'], 'literal in the code', s)
for n_, s in enumerate(D['loot']['special']): SRC['S%d' % n_] = (s['src'], 'literal in the code', s)
NOGO = {x['id']: x for x in A['nogo_loot']}
RULES = D['loot']['rules']
for l in sorted(A['loot'], key=lambda l: l['ev']):
    src, how, raw = SRC[l['id']]
    val = l.get('nominal', l['ev'])
    r = max(3.5, min(12.0, 3.0 + math.sqrt(val) * 0.5))
    kind = {'spot': 'точка лута', 'door': 'еда за дверью', 'heavy': 'тяжёлый приз', 'trophy': 'трофей', 'special': 'особое'}[l['kind']]
    resp = {'spot': 'да: случайное дозаполнение каждые 10 с, пока еды меньше 60 % точек', 'door': 'нет: один раз в начале смены', 'heavy': 'да: если тяжёлых меньше трёх',
            'trophy': 'да: через 40 с после сдачи или поломки, на случайной из свободных точек', 'special': 'см. описание'}[l['kind']]
    risk = {2: 'не достаёт (укрытие или слепая зона)', 1: 'достаёт только ладонью', 0: 'достаёт'}[l['cover']]
    rows = [('Что', kind), ('Где', l['place']), ('Ценность', ('%s (в расчётах %s: трофеев одновременно три)' % (num(val), num(l['ev']))) if 'nominal' in l else num(l['ev'])),
            ('Уровень еды', l['lvl'] if l['kind'] in ('spot', 'door') else None), ('Ярус', TIER_RU[l['tier']]), ('Координаты', '(%s, %s, z=%s)' % (num(l['p'][0]), num(l['p'][1]), num(l['p'][2]))),
            ('За дверью', ('да' + (', нужна сила %s' % num(raw['need_strength']) if raw.get('need_strength') else '')) if l['door'] else 'нет'),
            ('От гнезда', tsec(l['t'])), ('Человек', risk), ('Появляется снова', resp),
            ('Бьётся', 'да, при падении примерно с 0,4 м и выше' + ('; трофей теряет x2' if l['kind'] == 'trophy' else '; куски в сумме +10 %') if l['kind'] != 'special' else None),
            ('Проблема', '; '.join(NOGO[l['id']]['why']) if l['id'] in NOGO else None),
            ('Лог игры (ARTPROBE)', ('под точкой %s на z=%s' % (raw['probe']['below'][0], raw['probe']['below'][1])) if raw.get('probe') else None),
            ('Комментарий в коде', l.get('label')), ('Точность', {'literal in the code': 'точно: числа из кода'}.get(how, 'точно: воспроизведён генератор случайных чисел, сверено с логом игры')), ('Источник', src)]
    ci_ = card(l['name'], 'еда', rows)
    if l['kind'] == 'heavy': shape = '<rect x="%s" y="%s" width="%s" height="%s" class="lo lo-heavy"/>' % (-r, -r, 2 * r, 2 * r)
    elif l['kind'] == 'trophy': shape = '<polygon points="%s" class="lo lo-trophy"/>' % star(r + 2)
    elif l['kind'] == 'special': shape = '<polygon points="%s" class="lo lo-special"/>' % poly([(0, -r - 1), (r + 1, 0), (0, r + 1), (-r - 1, 0)])
    else: shape = '<circle r="%s" class="lo lo%d"/>' % (round(r, 1), l['lvl'])
    if l['door']: shape += '<circle r="%s" class="lo-lock"/>' % round(r + 3.2, 1)
    if l['kind'] in ('spot', 'door') and l['lvl'] >= 2: shape += letter(str(l['lvl']), 8)
    if l['id'] in NOGO and any('внутри' in w or 'под основанием' in w for w in NOGO[l['id']]['why']): shape += '<path d="M-5,-5 L5,5 M-5,5 L5,-5" class="lo-x"/>'
    mk('loot', l['p'][0], l['p'][1], shape, TIER_CODE[l['tier']], ci_, 'big' if val >= 60 else 'small')

# ---- connectors -------------------------------------------------------------------------------------------------------
KIND_RU = {'opening': 'проход между зонами', 'climb': 'путь наверх', 'ramp': 'наклонный путь', 'duct': 'воздуховод', 'teleport': 'телепорт (без груза)', 'launch': 'катапульта', 'gap': 'щель / лаз', 'path': 'тропинка'}
for c in D['connectors']:
    rows = [('Что', KIND_RU[c['kind']]), ('Ведёт', '%s → %s' % (TIER_RU[c['from_tier']], TIER_RU[c['to_tier']])), ('Перепад высоты', ('%s uu (%s м), %s с бегом' % (num(c['height_uu']), mtr(c['height_uu']), num(round(c['height_uu'] / (V_SPRINT * WALL_K), 1)))) if c['height_uu'] else None),
            ('Заметка', c['note']), ('Точность', {'code': 'точно: числа из кода', 'art table': 'точно: таблица размещения моделей', 'derived': 'оценка, выведено из констант'}[c['exact']]), ('Источник', c['src'])]
    ci_ = card(c['ru'], 'переход', rows)
    tiers = ' '.join(sorted(set((TIER_CODE[c['from_tier']] + ' ' + TIER_CODE[c['to_tier']]).split())))
    pts = c['pts']
    if len(set((p[0], p[1]) for p in pts)) > 1:
        G['conn'].append('<polyline points="%s" class="cn cn-%s" data-t="%s" data-c="%d"/>' % (' '.join('%s,%s' % (sx(p[0]), sy(p[1])) for p in pts), c['kind'], tiers, ci_))
    k = c['kind']
    if k == 'opening': shape = '<path d="M-9,0 L-3,-5 L-3,5 Z M9,0 L3,-5 L3,5 Z" class="co"/>'
    elif k in ('climb', 'launch'): shape = '<path d="M0,-7 L6.5,5 L-6.5,5 Z" class="co"/>' + ('<circle r="9.5" class="co-ring"/>' if k == 'launch' else '')
    elif k == 'ramp': shape = '<path d="M-7,5 L7,5 L7,-6 Z" class="co"/>'
    elif k == 'duct': shape = '<rect x="-7" y="-7" width="14" height="14" class="co-box"/><path d="M0,-4.5 L4,3 L-4,3 Z" class="co"/>'
    elif k == 'teleport': shape = '<circle r="6.5" class="co-box"/><circle r="2.5" class="co"/>'
    elif k == 'gap': shape = '<path d="M-6,5 L-6,-1 A6,6 0 0 1 6,-1 L6,5" class="co-box"/>'
    else: shape = '<circle r="3" class="co"/>'
    mk('conn', pts[0][0], pts[0][1], shape, tiers, ci_)
    if k in ('duct', 'teleport', 'opening', 'launch') and (pts[-1][0], pts[-1][1]) != (pts[0][0], pts[0][1]):
        mk('conn', pts[-1][0], pts[-1][1], '<circle r="3.5" class="co"/>', tiers, ci_)

# ---- nests and spawns ---------------------------------------------------------------------------------------------------
SP_LET = {'nest': 'Г', 'spawn_ant': 'м', 'nest_rival': 'О', 'spawn_rival': 'о', 'spawn_human': 'Ч', 'human_flee': 'ч', 'spawn_dog': 'Б', 'spawn_cat': 'К', 'npc': 'Н', 'spawn_spider': 'П', 'fly_food': 'ж', 'dog_spot': 'б', 'cat_spot': 'к'}
for s in D['spawns']:
    ci_ = card(s['ru'], 'гнёзда и появление', [('Зона', AREA_RU[s['area']]), ('Координаты', '(%s, %s, z=%s)' % (num(s['p'][0]), num(s['p'][1]), num(s['p'][2]))), ('Радиус', ('%s uu' % num(s['radius'])) if s.get('radius') else None), ('Заметка', s['note']), ('Источник', s['src'])])
    t = TIER_CODE[tier_by_z(s['p'][2])]
    if s.get('radius'):
        G['spawn'].append('<circle cx="%s" cy="%s" r="%s" class="nestr" data-t="f"/>' % (sx(s['p'][0]), sy(s['p'][1]), round(s['radius'] / S, 2)))
    big = s['kind'] in ('nest', 'nest_rival', 'spawn_human', 'spawn_dog', 'spawn_cat', 'spawn_spider')
    shape = ('<polygon points="%s" class="sp"/>' % hexa(10 if big else 7)) + letter(SP_LET[s['kind']], 10 if big else 8)
    mk('human' if s['kind'] in ('spawn_human', 'human_flee') else 'spawn', s['p'][0], s['p'][1], shape, t, ci_, 'sp-' + s['kind'])

# ---- things to press, noise makers -----------------------------------------------------------------------------------
IK = {'door': 'Д', 'mover': 'д', 'gadget': 'E', 'noise': 'Ш', 'push': '→'}
IK_RU = {'door': 'тяжёлая / особая дверь', 'mover': 'дверца, ящик, крышка', 'gadget': 'кнопка / гаджет', 'noise': 'источник шума', 'push': 'предмет, который толкают'}
for i in D['interactables']:
    z = i['p'][2]
    inside = any(b[0] < i['p'][0] < b[3] and b[1] < i['p'][1] < b[4] and b[2] < z < b[5] for b in INSIDE)
    rows = [('Что', IK_RU[i['kind']]), ('Действие', i['does']), ('Громкость шума', i.get('loud')), ('Человек идёт закрывать', ('да' if i.get('human_shuts') else 'нет') if i['kind'] in ('door', 'mover') else None),
            ('Масса', i.get('mass')), ('Координаты', '(%s, %s, z=%s)' % (num(i['p'][0]), num(i['p'][1]), num(z))), ('Ярус', TIER_RU[tier_by_z(z)]), ('Источник', i['src'])]
    ci_ = card(i['ru'], 'нажать / шум', rows)
    small = i['kind'] in ('mover', 'push')
    r = 5 if small else 7.5
    shape = ('<circle r="%s" class="ia ia-%s"/>' % (r, i['kind']) if i['kind'] == 'push' else '<rect x="%s" y="%s" width="%s" height="%s" rx="2" class="ia ia-%s"/>' % (-r, -r, 2 * r, 2 * r, i['kind'])) + letter(IK[i['kind']], 7 if small else 9)
    mk('inter', i['p'][0], i['p'][1], shape, TIER_CODE[tier_by_z(z)] + (' i' if inside else ''), ci_, 'small' if small else 'big')
    if i.get('human_shuts'):
        mk('human', i['p'][0], i['p'][1], '<path d="M-6,-6 L6,6 M-6,6 L6,-6" class="hx"/><circle r="9" class="hx"/>', TIER_CODE[tier_by_z(z)], card('Человек закрывает: ' + i['ru'], 'Человек', [('Правило', D['human']['shuts_doors']['ru']), ('Источник', ', '.join(D['human']['shuts_doors']['src']))]), 'small')

# ---- hazards ----------------------------------------------------------------------------------------------------------
HK_RU = {'heat': 'жар', 'cold': 'холод', 'wet': 'мокро', 'water': 'вода', 'shock': 'ток', 'trap': 'ловушка', 'sticky': 'липко', 'wind': 'ветер', 'npc': 'ноги людей', 'pet': 'питомец', 'rival': 'соперники'}
def shape_svg(h, cls, extra):
    if 'circle' in h: return '<circle cx="%s" cy="%s" r="%s" class="%s" %s/>' % (sx(h['circle'][0]), sy(h['circle'][1]), round(h['circle'][2] / S, 2), cls, extra)
    r = h['rect']; return '<rect x="%s" y="%s" width="%s" height="%s" class="%s" %s/>' % (sx(r[0]), sy(r[3]), round((r[2] - r[0]) / S, 2), round((r[3] - r[1]) / S, 2), cls, extra)
def haz_tiers(h):
    t = set()
    if h['z'][0] < 300: t.add('f'); t.add('u')
    if h['z'][0] < 2100 and h['z'][1] > 300: t.add('w')
    if h['z'][1] > 2100: t.add('t')
    if h['id'] in ('oven', 'fridge', 'freezer', 'dishwasher', 'microwave', 'bin_lid', 'trap'): t = {'i'}
    return ' '.join(sorted(t))
for h in D['hazards']:
    ci_ = card(h['ru'], 'опасность', [('Вид', HK_RU[h['kind']]), ('Что делает', h['effect']), ('Высота', 'z %s..%s' % (num(h['z'][0]), num(h['z'][1]))), ('Точность', 'оценка, выведено из констант' if h['exact'] == 'derived' else 'точно: числа из кода'), ('Источник', h['src'])])
    G['haz'].append(shape_svg(h, 'hz hz-' + h['kind'], 'data-t="%s" data-c="%d"' % (haz_tiers(h), ci_)))
    c = h.get('circle') or [(h['rect'][0] + h['rect'][2]) / 2, (h['rect'][1] + h['rect'][3]) / 2]
    mk('haz', c[0], c[1], '<path d="M0,-7 L7,5.5 L-7,5.5 Z" class="hzm"/>' + letter('!', 8), haz_tiers(h), ci_, 'small')
# the dog's straight lines between its spots (it walks through furniture; its paws crush on the way)
dog = D['pets']['dog']['spots']
pc = card('Тропы Бадрика', 'питомец', [('Как ходит', D['pets']['dog']['note']), ('Скорость', '650 uu/с'), ('Лапы', 'давят в радиусе 110 uu, пока он идёт'), ('Следующая точка', 'случайная из семи'), ('Источник', D['pets']['dog']['src'])])
door = [(6500, -4000), (7700, -4000)]
segs = []
for a in range(len(dog)):
    for b in range(a + 1, len(dog)):
        pa, pb = dog[a]['p'], dog[b]['p']
        if (pa[0] < 7100) == (pb[0] < 7100): segs.append([pa, pb])
        else:
            k, l_ = (pa, pb) if pa[0] < 7100 else (pb, pa)
            segs.append([k, door[0], door[1], l_])
for sg in segs:
    G['haz'].append('<polyline points="%s" class="dogpath" data-t="f u" data-c="%d"/>' % (' '.join('%s,%s' % (sx(p[0]), sy(p[1])) for p in sg), pc))

# ---- grids from the audit: runs of equal cells at 200 uu -----------------------------------------------------------------
GR = A['grid']
def runs(rows, pick):
    """rows: strings, one char per 100 uu cell. Yields (x0, y0, x1, y1, class) for runs of 200 uu cells."""
    out = []
    for j in range(0, GR['ny'] - 1, 2):
        row = rows[j]; cur = None; start = 0
        for i in range(0, GR['nx'] + 2, 2):
            ch = pick(row[i]) if i < GR['nx'] else None
            if ch != cur:
                if cur: out.append((GR['x0'] + start * GR['g'], GR['y0'] + j * GR['g'], GR['x0'] + i * GR['g'], GR['y0'] + (j + 2) * GR['g'], cur))
                cur, start = ch, i
    return out
for (x0, y0, x1, y1, c) in runs(GR['iso'], lambda ch: ch if ch in 'abcde' else None):
    rect('iso', x0, y0, x1, y1, 'iso iso-' + c)
for (x0, y0, x1, y1, c) in runs(GR['cover'], lambda ch: '2' if ch == '2' else None):
    rect('cover', x0, y0, x1, y1, 'cv', 'data-t="f u"')
for (x0, y0, x1, y1, c) in runs(GR['cover'], lambda ch: ch if ch in '01' else None):
    rect('reach', x0, y0, x1, y1, 'rc rc' + c, 'data-t="f u"')
for (x0, y0, x1, y1, c) in runs(GR['stand'], lambda ch: '1' if ch == '1' else None):
    rect('human', x0, y0, x1, y1, 'stand', 'data-t="f"')
HM = D['human']
at = HM['attacks']
G['reach'].append('<!-- floor: where the stomp (rc0) and only the hand slap (rc1) reach -->')
rcard = card('Досягаемость Человека на полу', 'Человек', [('Сплошная штриховка', 'достаёт топот: до %s uu от места, где он может встать; убивает только до 220 над полом' % at['stomp']['reach_from_centre']),
    ('Редкая штриховка', 'достаёт только шлепок ладонью: до %s uu (ИИ), не чаще раза в 6 с, урон 150' % at['slap']['ai_uses_between_2d'][1]),
    ('Газета', 'на столах и верхах: %s' % at['newspaper']['reach_formula']), ('Дихлофос', at['spray']['range'] + '; конус 20°; нужен прямой луч; высота примерно до ' + at['spray']['max_height_estimate']),
    ('Как посчитано', 'капсула радиусом 340 против габаритов мебели; предметы ниже 450 препятствием не считались'), ('Источник', ', '.join(at['stomp']['src'][:2] + at['slap']['src'][:2]))])
ccard = card('Укрытие: сюда Человек не достаёт', 'укрытие', [('На полу', 'дальше 1250 uu от любого места, где он может встать, либо слепая зона ИИ'), ('Слепые зоны ИИ', '; '.join(h['ru'] for h in HM['sees']['never_targets'])),
    ('Снаружи', 'во двор и на улицу он не выходит'), ('Источник', HM['sees']['never_targets'][0]['src'])])
G['cover'].append('<rect x="-166" y="-61" width="1" height="1" class="cv" data-c="%d" style="display:none"/>' % ccard)
for h in HM['sees']['never_targets']:
    if 'rect' not in h: continue
    r = h['rect']; ts = set()
    if h['z'][0] < 300: ts.update('fu')
    if h['z'][0] < 2100 and h['z'][1] > 300: ts.add('w')
    if h['z'][1] > 2100: ts.add('t')
    ci_ = card('Слепая зона ИИ: ' + h['ru'], 'укрытие', [('Правило', 'ИИ Человека никогда не выбирает целью муравья в этом объёме'), ('Высота', 'z %s..%s' % (h['z'][0], h['z'][1])), ('Источник', h['src'])])
    rect('cover', r[0], r[1], r[2], min(r[3], 5000), 'blind', 'data-t="%s" data-c="%d"' % (' '.join(sorted(ts)), ci_))
for cn in D['containers']:
    b = cn['box']
    ci_ = card('Контейнер: ' + cn['where_ru'], 'внутри', [('Двери', '; '.join('%s (%s)' % (d.get('ru') or d['actor'], d['door_class']) for d in cn['doors'])), ('Нужная сила', cn['need_strength']),
        ('Запирает', 'да: закрытая дверь с муравьём внутри — беда (холод / жар), команде приходит сообщение' if cn['trap'] else 'нет'), ('Закрытый', 'Человек не видит того, кто внутри'), ('Высота', 'z %s..%s' % (num(b[2]), num(b[5]))), ('Источник', cn['src'])])
    rect('cover', b[0], b[1], b[3], b[4], 'cont', 'data-t="i" data-c="%d"' % ci_)
# the Human: where he wanders, where he ends up when his target is outside
pt = HM['patrol']
for nm, bx in (('кухня', pt['kitchen_box']), ('гостиная', pt['living_box'])):
    ci_ = card('Коробка блуждания Человека: ' + nm, 'Человек', [('Правило', 'патрульных точек нет: идёт в случайную точку этой коробки, новую каждые 6 с или по приходе; 60 % кухня, 40 % гостиная'), ('Границы', str(bx)), ('Источник', ', '.join(pt['src'][:3]))])
    rect('human', bx[0], bx[1], bx[2], bx[3], 'wander', 'data-t="f" data-c="%d"' % ci_)
oc = card('Сюда Человек упирается, когда его цель снаружи', 'Человек', [('Почему', HM['outside_targets']['note']), ('Источник', ', '.join(HM['outside_targets']['src']))])
for p in (HM['outside_targets']['crack_path'][0], HM['outside_targets']['window_path'][0]):
    mk('human', p[0], p[1], '<path d="M0,-8 L8,6 L-8,6 Z" class="hwarn"/>' + letter('!', 8), 'f', oc)
dc = card('Путевые точки двери', 'Человек', [('Правило', 'между комнатами он идёт через эти две точки'), ('Источник', pt['src'][3])])
for p in pt['door_waypoints']: mk('human', p[0], p[1], '<circle r="4" class="hwp"/>', 'f', dc, 'small')
G['human'].append('<rect x="-166" y="-61" width="1" height="1" data-c="%d" style="display:none"/>' % rcard)

# ---- events and secrets -----------------------------------------------------------------------------------------------
for ev in D['events']:
    ci_ = card('Событие %d: %s' % (ev['id'], ev['ru'].capitalize()), 'событие', [('В игре', ev['text_ru']), ('Длится', '%s с' % num(ev['seconds'])), ('Где', ev['where_ru']), ('Когда', D['event_timing']['ru']), ('Источник', ev['where_src'])])
    G['event'].append(shape_svg(ev, 'evz', 'data-t="f u" data-c="%d"' % ci_))
    c = ev.get('circle') or [(ev['rect'][0] + ev['rect'][2]) / 2, (ev['rect'][1] + ev['rect'][3]) / 2]
    if ev['id'] == 3: c = [5300, -3600]
    mk('event', c[0], c[1], '<polygon points="%s" class="evm"/>' % hexa(10) + letter(str(ev['id']), 10), 'f', ci_)
for s in D['secrets']:
    t = 'v' if s['p'][2] < 0 else TIER_CODE[tier_by_z(s['p'][2])]
    ci_ = card('Секрет: ' + s['ru'], 'секрет', [('Что там', s['what']), ('Зона', AREA_RU[s['area']]), ('Источник', s['src'])])
    mk('event', s['p'][0], s['p'][1], '<polygon points="%s" class="sec"/>' % poly([(0, -9), (9, 0), (0, 9), (-9, 0)]) + letter('?', 9), t + ' i v', ci_)

# ---- audit: the 1 m cells -----------------------------------------------------------------------------------------------
LOOTBY = {l['id']: l for l in A['loot']}
NAME = {}
for k_ in ('connectors', 'interactables', 'hazards'):
    for it in D[k_]: NAME[it['id']] = it['ru']
for c in A['cells']:
    r = c['rect']; w = c['why']
    reasons = [t for t, ok in (('еда на 50+', w['loot']), ('мелочь', w.get('small_loot')), ('кнопка / дверь', w['use']), ('путь наверх / проход', w['way']), ('местная опасность', w['fear']), ('есть предмет', w['see']), ('на маршруте', w['route']),
                               ('Человек достаёт', w['human']), ('укрытие', w['cover'])) if ok]
    rows = [('Класс', CLS_RU[c['cls']]), ('Почему', ', '.join(reasons) or 'ничего нет'), ('Зона и ярус', '%s, %s' % (AREA_RU[c['area']], TIER_RU[c['tier']])), ('Поверхности', ', '.join(c.get('surfaces', []))),
            ('Площадь', '%s м²' % num(c['m2'])), ('Еда', ('%s (точек %d)' % (num(c['ev']), len(c['loot']))) if c['loot'] else 'нет'), ('Открыто Человеку', '%d %%' % round(c['open'] * 100)), ('Только ладонь', ('%d %%' % round(c['slap_only'] * 100)) if c['slap_only'] else None),
            ('Укрытие', '%d %%' % round(c['cover'] * 100)), ('Под мебелью', ('%d %%' % round(c['under'] * 100)) if c.get('under') else None), ('Видно с высоты глаз', ('%d %% мест, где он стоит' % round(c['vis'] * 100)) if 'vis' in c else None),
            ('От гнезда', tsec(c['t'])), ('Переходы', ', '.join(NAME.get(i, i) for i in c['conn'])), ('Кнопки', ', '.join(NAME.get(i, i) for i in c['inter'])), ('Опасности', ', '.join(NAME.get(i, i) for i in c['haz'])),
            ('Предметы', ', '.join(c['land']) if c['tier'] == 'floor' else None), ('Клетка', 'x %s..%s, y %s..%s' % (r[0], r[2], r[1], r[3]))]
    ci_ = card('Клетка аудита: ' + CLS_RU[c['cls']], 'аудит', rows)
    t = {'floor': 'f u', 'work': 'w', 'top': 't'}[c['tier']]
    rect('heat', r[0], r[1], r[2], r[3], 'hc hc-' + c['cls'], 'data-t="%s" data-c="%d"' % (t, ci_))
    G['heat'].append('<text x="%s" y="%s" class="hl" data-t="%s">%s</text>' % (sx((r[0] + r[2]) / 2), sy((r[1] + r[3]) / 2), t, CLS_LET[c['cls']]))
for u in A['units']:
    b = u['box']
    rows = [('Класс', CLS_RU[u['cls']]), ('Ярус', TIER_RU[u['tier']]), ('Площадь', '%s м²' % num(u['m2'])), ('Еда', ('%s (точек %d)' % (num(u['ev']), len(u['loot']))) if u['loot'] else 'нет'), ('Нужная сила на дверь', u.get('need_strength')),
            ('Запирает', 'да' if u.get('trap') else None), ('Вход', u.get('entry')), ('Опасности', ', '.join(NAME.get(i, i) for i in u['haz'])), ('От гнезда', tsec(u['t']))]
    ci_ = card(u['ru'], 'аудит: закрытое место', rows)
    mk('heat', (b[0] + b[3]) / 2, (b[1] + b[4]) / 2, '<rect x="-8" y="-8" width="16" height="16" class="hu hc-%s"/>' % u['cls'] + letter(CLS_LET[u['cls']], 9), 'i' if u['tier'] == 'int' else 'v', ci_)

# ---- routes -------------------------------------------------------------------------------------------------------------
for r in A['routes']:
    ci_ = card('Маршрут %s: %s' % (r['id'][1:], r['ru']), 'маршрут', [('Длина', '%s м (по полу %s + подъём %s)' % (num(r['len_m']), num(r['floor_m']), num(r['climb_m']))), ('Туда бегом', '%s с' % num(r['t_sprint'])), ('Туда шагом', '%s с' % num(r['t_walk'])),
        ('Назад с грузом', '%s–%s с' % (num(r['t_back_loaded'][0]), num(r['t_back_loaded'][1]))), ('На виду (достаёт топот)', '%s с бегом' % num(r['exposed_s'])), ('Достаёт только ладонь', '%s с' % num(r['slap_only_s'])),
        ('Как посчитано', 'кратчайший путь по полу, мебель не мешает; оценка снизу')])
    G['route'].append('<polyline points="%s" class="rt" data-c="%d"/>' % (' '.join('%s,%s' % (sx(p[0]), sy(p[1])) for p in r['pts']), ci_))
    for p in (r['pts'][-1], r['pts'][len(r['pts']) * 2 // 3]):
        mk('route', p[0], p[1], '<circle r="8.5" class="rtm"/>' + letter(r['id'][1:], 10), 'f u w t i v', ci_)

# ---- districts (a draft) ------------------------------------------------------------------------------------------------
DISTRICTS = [
    ('Нора и запад', 'kitchen', [[-7000, -3200, -3000, 2500]], 'щель в гнездо, полка с мёдом над ней, выключатель, коробка хлопьев-тоннель, мука, скалка'),
    ('Западная столешница', 'kitchen', [[-7000, 2500, -4200, 5000]], 'тостер-катапульта, доска с ножом, три ящика, нижний шкаф, верхние шкафы, выход вентиляции 1'),
    ('Мойка', 'kitchen', [[-4200, 2500, -2800, 5000]], 'раковина с краном, под мойкой открытая дверца, труба, мышеловка, слив-телепорт'),
    ('Плита и техника', 'kitchen', [[-2800, 2500, 2600, 5000]], 'плита с духовкой, вытяжка, рейлинг, полочки специй, полотенце, спичечный коробок, посудомойка, микроволновка, вентилятор, чайник'),
    ('Холодильник', 'kitchen', [[2600, 2500, 5200, 5000]], 'холодильник с морозилкой, щель под ним, лужа и провод сзади, мышиная нора, выход вентиляции 2'),
    ('Стол', 'kitchen', [[-3000, -3200, 4000, 2500]], 'обеденный стол с тортом, два стула, ковёр, ложка-трап, лампа, Бадрик спит под столом'),
    ('Мусорный угол и дверь', 'kitchen', [[4000, -5000, 7000, 2500], [5200, 2500, 7000, 5000]], 'педальное ведро, гнездо огненных, плитка с ямой, ведро со шваброй, паутина, вентиляция 2, дверь в гостиную, ботинок'),
    ('Миски и окно', 'kitchen', [[-7000, -5000, 4000, -3200]], 'миски Бадрика, решётка вентиляции 1 и розетка, щель во двор, окно с подоконником и шторами, газета, стул'),
    ('Диван', 'living', [[7200, -1300, 20200, 1500]], 'диван с малышом и кошкой, тайник под ним, батарея, торшер, растение, зарядка'),
    ('Ковёр и телевизор', 'living', [[10300, -9000, 18900, -1300]], 'ковёр, журнальный стол со снеками, тумба ТВ с приставкой, лежанка собаки'),
    ('Кресло бабушки', 'living', [[7200, -9000, 10300, -1300]], 'кресло с бабушкой, клубок с ниткой, корзина, носок, когтеточка'),
    ('Книжный шкаф', 'living', [[18900, -9000, 20200, -1300]], 'шкаф до потолка: пять полок с книгами, ваза наверху'),
    ('Двор', 'yard', [[-12000, -16000, 7000, -5200], [7000, -16000, 21000, -9200]], 'газон, дерево, уличный бак, дорожка из камней, собачьи игрушки и кучка, внешний подоконник'),
    ('Улица', 'street', [[-16500, -5200, -7200, 5600]], 'гнездо колонии на газоне, забор с калиткой, тротуар с мороженым и жвачкой, дорога, машина, водосток до желоба'),
]
def in_rects(p, rs): return any(r[0] <= p[0] < r[2] and r[1] <= p[1] < r[3] for r in rs)
DIST_OUT = []
for n_, (nm, ar, rs, what) in enumerate(DISTRICTS, 1):
    ls = [l for l in A['loot'] if l['area'] == ar and in_rects(l['p'], rs)]
    m2 = sum((r[2] - r[0]) * (r[3] - r[1]) for r in rs) / (UU * UU)
    cells = [c for c in A['cells'] if c['area'] == ar and c['tier'] == 'floor' and in_rects(((c['rect'][0] + c['rect'][2]) / 2, (c['rect'][1] + c['rect'][3]) / 2), rs)]
    tot = sum(c['m2'] for c in cells) or 1.0
    stat = {k: round(100 * sum(c['m2'] for c in cells if c['cls'] == k) / tot) for k in CLS_RU}
    cnt = lambda key: sum(1 for it in D[key] if it['area'] == ar and in_rects(it.get('p') or it['pts'][0], rs))
    hz = sum(1 for h in D['hazards'] if h['area'] == ar and in_rects(h.get('circle') or [(h['rect'][0] + h['rect'][2]) / 2, (h['rect'][1] + h['rect'][3]) / 2], rs))
    info = {'n': n_, 'name': nm, 'area': ar, 'rects': rs, 'what': what, 'm2': round(m2, 1), 'loot_points': len(ls), 'loot_value': round(sum(l['ev'] for l in ls)), 'connectors': cnt('connectors'), 'interactables': cnt('interactables'), 'hazards': hz, 'floor_classes_pct': stat}
    DIST_OUT.append(info)
    ci_ = card('Район %d (черновик): %s' % (n_, nm), 'район — ЧЕРНОВИК', [('Сейчас здесь', what), ('Площадь', '%s м²' % num(m2)), ('Еда', '%d точек на %s' % (len(ls), num(info['loot_value']))), ('Переходов', info['connectors']), ('Кнопок и дверей', info['interactables']),
        ('Опасностей', hz), ('Пол', ', '.join('%s %d %%' % (CLS_RU[k], v) for k, v in stat.items() if v)), ('Статус', 'черновая нарезка по тому, что физически стоит; границы и названия обсуждаются')])
    for r in rs: rect('dist', r[0], r[1], r[2], r[3], 'ds', 'data-c="%d"' % ci_)
    r = max(rs, key=lambda r: (r[2] - r[0]) * (r[3] - r[1]))
    G['dist'].append('<text x="%s" y="%s" class="dl" data-c="%d">%d<tspan class="dx">. %s</tspan></text>' % (sx((r[0] + r[2]) / 2), sy((r[1] + r[3]) / 2), ci_, n_, e(nm)))

# ---- isochrone labels: where 10 / 20 / 40 / 60 s are crossed along three scan lines ---------------------------------------
def scan(points):
    prev = None
    for (x, y) in points:
        i = int((x - GR['x0']) // GR['g']); j = int((y - GR['y0']) // GR['g'])
        if not (0 <= i < GR['nx'] and 0 <= j < GR['ny']): continue
        ch = GR['iso'][j][i]
        if ch == '.': continue
        if prev and ch != prev and prev in 'abcd' and ch > prev:
            G['iso'].append('<text x="%s" y="%s" class="il">%s с</text>' % (sx(x), sy(y), {'a': 10, 'b': 20, 'c': 40, 'd': 60}[prev]))
        prev = ch
scan([(x, -2450) for x in range(-6900, 6900, 100)])
scan([(x, -4050) for x in range(-6900, 20000, 100)])
scan([(-4450, y) for y in range(-5000, -15900, -100)])
scan([(x, 2050) for x in range(-6900, 6900, 100)])
scan([(-8250, y) for y in range(-2500, 5500, 100)])

# ---- the page -------------------------------------------------------------------------------------------------------------
LAYERS = [  # id, name, on by default, what the marks mean
    ('geo', 'Стены и мебель', 1, 'Прямоугольник = габарит предмета. Чем темнее, тем выше. Пунктирный контур = без коллизии. Синие тонкие = дверцы и ящики.'),
    ('label', 'Подписи', 1, 'Названия комнат и мебели; мелкие появляются при приближении.'),
    ('conn', 'Пути наверх, проходы, трубы', 1, '▲ путь наверх, ◢ наклонный, ◄► проход между зонами, ▲ в рамке и пунктир — воздуховод, ◎ и точки — телепорт, ∩ щель или лаз.'),
    ('loot', 'Еда', 1, 'Круг = точка лута, размер = ценность, цифра = уровень. Пунктирное кольцо = за дверью. Квадрат = тяжёлый приз. Звезда = трофей. Ромб = особое. Красный крест = сломано.'),
    ('spawn', 'Гнёзда и появление', 1, 'Шестиугольник с буквой: Г гнездо, м муравьи, О огненные, Б Бадрик, К кошка, П паук, Н люди, ж еда мух; маленькие б и к — точки питомцев. Пунктирный круг = радиус гнезда.'),
    ('inter', 'Кнопки, двери, шум', 0, 'Квадрат: Д тяжёлая дверь, д дверца или ящик, E кнопка, Ш источник шума. Кружок со стрелкой = предмет, который сталкивают.'),
    ('haz', 'Опасности и питомцы', 0, 'Красная штриховка и ▲! = зона опасности (жар, холод, вода, ток, липко, ноги людей, пёс ест еду). Красные точки = прямые, по которым ходит Бадрик.'),
    ('reach', 'Куда достаёт Человек', 0, 'Частая оранжевая штриховка = достаёт топот. Редкая = только шлепок ладонью. На столах и верхах смотри слой аудита и карточки.'),
    ('cover', 'Укрытия', 0, 'Зелёная сетка = Человек не достаёт. Зелёный пунктир = слепая зона ИИ. Зелёная рамка = контейнер за дверью.'),
    ('human', 'Человек: где ходит, что закрывает', 0, 'Оранжевая заливка = где он может встать. Пунктирные рамки = коробки блуждания. Ч появление, ч куда убегает. ⊗ = дверь, которую он идёт закрывать. ▲! = сюда упирается, когда цель снаружи.'),
    ('event', 'События и секреты', 0, 'Шестиугольник с цифрой = событие смены (1–6) и его зона. Ромб с ? = секрет.'),
    ('iso', 'Время от гнезда', 0, 'Полосы 10 / 20 / 40 / 60 / 90 с бегом без груза по полу: чем светлее, тем дальше. На высоте прибавь z / 374 с: столешница +4,6 с, верх шкафов +11,8 с.'),
    ('heat', 'Аудит: клетки 1 × 1 м', 0, 'М мёртвая (серая штриховка), Т транзит (точки), С спорная (оранжевая), Б безопасная (зелёная). Квадратики — закрытые места.'),
    ('route', 'Маршруты', 0, 'Пунктирные линии с номерами 1–9: куда реально побегут от гнезда.'),
    ('dist', 'Районы — ЧЕРНОВИК', 0, 'Черновая нарезка на 14 районов по тому, что физически стоит. Границы и названия обсуждаются.'),
]
ORDER = ['base', 'iso', 'heat', 'geo', 'cover', 'reach', 'human', 'dist', 'haz', 'conn', 'route', 'inter', 'spawn', 'event', 'loot', 'label']
DEFS = '''<defs>
<pattern id="p-dead" width="3" height="3" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><rect width="3" height="3" class="pbg-dead"/><line x1="0" y1="0" x2="0" y2="3" class="pl-dead"/></pattern>
<pattern id="p-transit" width="3" height="3" patternUnits="userSpaceOnUse"><circle cx="1.5" cy="1.5" r=".45" class="pd-transit"/></pattern>
<pattern id="p-cover" width="2.4" height="2.4" patternUnits="userSpaceOnUse"><path d="M0,0 L0,2.4 M0,0 L2.4,0" class="pl-cover"/></pattern>
<pattern id="p-reach" width="1.6" height="1.6" patternUnits="userSpaceOnUse" patternTransform="rotate(-45)"><line x1="0" y1="0" x2="0" y2="1.6" class="pl-reach"/></pattern>
<pattern id="p-reach2" width="4" height="4" patternUnits="userSpaceOnUse" patternTransform="rotate(-45)"><line x1="0" y1="0" x2="0" y2="4" class="pl-reach"/></pattern>
<pattern id="p-haz" width="2.2" height="2.2" patternUnits="userSpaceOnUse" patternTransform="rotate(60)"><line x1="0" y1="0" x2="0" y2="2.2" class="pl-haz"/></pattern>
<pattern id="p-pet" width="5" height="5" patternUnits="userSpaceOnUse"><circle cx="2.5" cy="2.5" r=".6" class="pd-haz"/></pattern>
</defs>'''
svg_inner = DEFS + '<g id="world">' + ''.join('<g class="ly" data-ly="%s">%s</g>' % (k, ''.join(G[k])) for k in ORDER) + '</g>'
VIEWS = {'all': [-16700, -16800, 21300, 6300], 'kitchen': [-7500, -5500, 7500, 5900], 'living': [6800, -9400, 20600, 1900], 'yard': [-12300, -16800, 21300, -4800], 'street': [-16700, -5600, -6700, 6300]}
SM = A['summary']
fl = SM['floor_dead_pct']
summary_html = ('<p><b>Мёртвый пол:</b> кухня %d %%, гостиная %d %%, двор %d %%, улица %d %%.<br><b>Транзит:</b> кухня %d %%, гостиная %d %%.<br><b>Спорный пол:</b> кухня %d %%, гостиная %d %%.<br>'
                '<b>Укрытие:</b> %d %% пола кухни, %d %% гостиной; до укрытия бывает %s с бегом.<br><b>Еда:</b> %d точек, из них %d никому не нужны.<br><b>Пустых поверхностей:</b> %d из %d.<br>'
                '<b>Двор и улица:</b> %d %% площади, %d %% еды.</p>') % (fl['kitchen'], fl['living'], fl['yard'], fl['street'], SM['floor_transit_pct']['kitchen'], SM['floor_transit_pct']['living'], SM['floor_contested_pct']['kitchen'], SM['floor_contested_pct']['living'],
                SM['cover_pct']['kitchen'], SM['cover_pct']['living'], num(max(SM['worst_run_s'].values())), SM['loot_points'], SM['nogo_points'], SM['empty_surfaces'], SM['surfaces'], SM['outdoor_area_pct'], SM['outdoor_loot_pct'])
summary_html += '<ol>' + ''.join('<li>%s</li>' % e(o) for o in SM['observations']) + '</ol>'
ant = D['constants']['ant']
summary_html += '<p class="mut">Масштаб: 1 м = 1900 uu = %s длин муравья (муравей %d мм = %s uu). Человек ростом 3400 uu. Бег муравья %d uu/с = %s м/с. Полный текст: docs/level/LEVEL_AUDIT.md. Собрано %s из кода, игра не запускалась.</p>' % (
    num(ant['body_lengths_per_m']), ant['real_length_mm'], num(ant['length_uu']), V_SPRINT, num(round(V_SPRINT / UU, 2)), e(A['meta']['generated']))
layers_html = ''.join('<label class="lr"><input type="checkbox" data-ly="%s"%s><span class="ln">%s</span><span class="ld">%s</span></label>' % (k, ' checked' if on else '', e(nm), e(desc)) for (k, nm, on, desc) in LAYERS)

PAGE = r'''<!DOCTYPE html>
<html lang="ru"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<title>Ant Kitchen — карта уровня</title>
<style>
:root{--bg:#f3efe6;--panel:#fffdf8;--ink:#1b1b1f;--mut:#6c6a66;--line:#d6d0c3;--wall:#35343a;--fk:#eee3cc;--fl:#e8d2ab;--fy:#cfe0b9;--fp:#d9d6cf;--fr:#b8b8be;--ob:#6f5f47;--door:#2f6fb0;--food:#b8742a;
--cn:#1668c4;--gold:#e5a50a;--gold2:#f4c430;--pur:#7a3fb5;--teal:#0f8b8d;--mag:#c2337a;--red:#cc2e2e;--grn:#2c8a3e;--org:#e0720b;--iso:#1b5fb8;--rt:#8b1a1a;--acc:#0a84ff}
@media (prefers-color-scheme:dark){:root{--bg:#121214;--panel:#1d1d21;--ink:#eeeef2;--mut:#a09ea6;--line:#34343b;--wall:#d0cfd6;--fk:#2a2722;--fl:#30291d;--fy:#1e2a1b;--fp:#2b2b2f;--fr:#202024;--ob:#c3b190;--door:#6fb1f2;--food:#e0a15a;
--cn:#5aa9ff;--gold:#f2b632;--gold2:#ffd866;--pur:#b488ee;--teal:#3fc7c9;--mag:#f06aa8;--red:#ff6b6b;--grn:#5fd07a;--org:#ff9a3d;--iso:#6aa8ff;--rt:#ff8a80;--acc:#4aa3ff}}
*{box-sizing:border-box}html,body{margin:0;height:100%;background:var(--bg);color:var(--ink);font:15px/1.35 system-ui,-apple-system,"Segoe UI",Roboto,Arial,sans-serif;overflow:hidden}
body{display:flex;flex-direction:column;height:100vh;height:100dvh}
#top{flex:none;background:var(--panel);border-bottom:1px solid var(--line);padding:4px 0 2px}
.row{display:flex;gap:6px;overflow-x:auto;padding:3px 8px;scrollbar-width:none}.row::-webkit-scrollbar{display:none}
.chip{flex:none;min-height:38px;padding:0 13px;border:1.5px solid var(--line);border-radius:19px;background:transparent;color:var(--ink);font:inherit;font-size:14px;white-space:nowrap}
.chip.on{background:var(--ink);color:var(--bg);border-color:var(--ink);font-weight:600}
#wrap{flex:1;position:relative;min-height:0}
#map{display:block;width:100%;height:100%;touch-action:none;user-select:none;-webkit-user-select:none;--k:1;--m:1}
#north{position:absolute;right:10px;top:10px;width:38px;text-align:center;font-weight:700;font-size:12px;line-height:1.05;background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:4px 0;pointer-events:none}
#scale{position:absolute;left:10px;bottom:12px;background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:5px 8px;font-size:12px;pointer-events:none;max-width:70%}
#bar{height:6px;border:2px solid var(--ink);border-top:none;margin-bottom:3px;min-width:8px}
#btnL{position:absolute;right:10px;bottom:12px;min-height:46px;padding:0 16px;border-radius:23px;border:none;background:var(--ink);color:var(--bg);font:600 15px system-ui;box-shadow:0 2px 8px rgba(0,0,0,.3)}
.sheet{position:fixed;left:0;right:0;bottom:0;max-height:66%;background:var(--panel);border-top:1px solid var(--line);border-radius:16px 16px 0 0;box-shadow:0 -4px 18px rgba(0,0,0,.25);transform:translateY(calc(100% + 60px));visibility:hidden;transition:transform .18s,visibility 0s .18s;display:flex;flex-direction:column;z-index:5}
.sheet.open{transform:none;visibility:visible;transition:transform .18s}
.lhide{visibility:hidden}svg[data-z="1"] .dx{display:none}svg[data-z="1"] .dl{font-size:calc(15px*var(--k))}
.sh{flex:none;display:flex;align-items:center;gap:8px;padding:8px 10px 6px 14px;border-bottom:1px solid var(--line)}
.sh h2{flex:1;margin:0;font-size:16px;line-height:1.2}.sh small{display:block;color:var(--mut);font-weight:400;font-size:12px}
.x{flex:none;width:44px;height:44px;border:none;border-radius:22px;background:transparent;color:var(--ink);font-size:24px}
.sb{overflow:auto;padding:6px 14px 18px;-webkit-overflow-scrolling:touch}
.tabs{display:flex;gap:6px;flex:1}.tabs .chip{min-height:36px}
.lr{display:grid;grid-template-columns:34px 1fr;align-items:start;padding:9px 0;border-bottom:1px solid var(--line)}
.lr input{width:24px;height:24px;margin:1px 0 0;accent-color:var(--acc)}.ln{font-weight:600}.ld{grid-column:2;color:var(--mut);font-size:12.5px}
table.cr{border-collapse:collapse;width:100%}table.cr td{vertical-align:top;padding:5px 0;border-bottom:1px solid var(--line);font-size:14px}table.cr td:first-child{color:var(--mut);width:36%;padding-right:8px}
.more{margin-top:10px;display:flex;flex-direction:column;gap:6px}.more button{min-height:42px;text-align:left;padding:6px 12px;border:1.5px solid var(--line);border-radius:10px;background:transparent;color:var(--ink);font:inherit}
.mut{color:var(--mut);font-size:12.5px}ol{padding-left:20px}li{margin:4px 0}
@media (min-width:900px){.sheet{left:auto;right:14px;bottom:14px;width:420px;max-height:82%;border-radius:16px;border:1px solid var(--line)}#btnL{right:14px}}
.ar-kitchen{fill:var(--fk)}.ar-living{fill:var(--fl)}.ar-yard{fill:var(--fy)}.ar-street{fill:var(--fy)}.ar-pave,.ar-kerb{fill:var(--fp)}.ar-road{fill:var(--fr)}
.wall{fill:var(--wall)}.window{stroke:var(--acc);stroke-width:1.4;stroke-dasharray:2 1.3}
.ob{fill:var(--ob);stroke:var(--ink);stroke-width:1;vector-effect:non-scaling-stroke}.h0{fill-opacity:.1}.h1{fill-opacity:.3}.h2{fill-opacity:.5}.h3{fill-opacity:.74}
.c-part{fill:var(--door);stroke:var(--door);fill-opacity:.9}.c-food{fill:var(--food)}.c-deco{fill-opacity:.05;stroke-dasharray:3 2}.c-struct{fill-opacity:.06;stroke-opacity:.35}.c-conn{fill:var(--cn);stroke:var(--cn);fill-opacity:.4}
.nosolid{stroke-dasharray:1.5 2.5}.canopy{fill:var(--grn);fill-opacity:.08;stroke:var(--mut);stroke-dasharray:5 4;vector-effect:non-scaling-stroke;pointer-events:none}
.ghost{opacity:.1;pointer-events:none}.hid{display:none}
.lb,.dl,.hl,.il{text-anchor:middle;dominant-baseline:middle;pointer-events:none;font-family:system-ui,-apple-system,"Segoe UI",Roboto,Arial,sans-serif;paint-order:stroke;stroke:var(--bg);stroke-linejoin:round}
.lb{font-weight:600;font-size:calc(10.5px*var(--k));fill:var(--ink);stroke-width:calc(2.6px*var(--k))}.l1{font-size:calc(15px*var(--k));letter-spacing:.14em;opacity:.55}
svg[data-z="1"] .l2,svg[data-z="1"] .l3,svg[data-z="2"] .l3,svg[data-z="3"] .l1{display:none}
.mk{transform:scale(var(--m));cursor:pointer}.mt{font:700 9px system-ui,Arial,sans-serif;text-anchor:middle;fill:var(--ink);pointer-events:none}
svg[data-z="1"] .mk.small{display:none}
.lo{fill:var(--gold);stroke:var(--ink);stroke-width:1.1}.lo0{fill-opacity:.4}.lo1{fill-opacity:.7}.lo-heavy{stroke-width:2.2}.lo-trophy{fill:var(--gold2);stroke-width:1.4}.lo-special{fill:var(--panel);stroke:var(--gold);stroke-width:2.4}
.lo-lock{fill:none;stroke:var(--ink);stroke-width:1.5;stroke-dasharray:3 2.2}.lo-x{fill:none;stroke:var(--red);stroke-width:2.4}
.cn{fill:none;stroke:var(--cn);stroke-width:2.2;vector-effect:non-scaling-stroke}.cn-duct{stroke-dasharray:7 4}.cn-teleport{stroke-dasharray:1.5 5;stroke-linecap:round}.cn-launch{stroke-dasharray:9 3 2 3}.cn-path{stroke-dasharray:2 4}.cn-gap{stroke-dasharray:4 2}
.co{fill:var(--cn);stroke:var(--bg);stroke-width:1}.co-box{fill:var(--panel);stroke:var(--cn);stroke-width:1.8}.co-ring{fill:none;stroke:var(--cn);stroke-width:1.5}
.sp{fill:var(--pur);stroke:var(--bg);stroke-width:1.2}.nestr{fill:var(--pur);fill-opacity:.07;stroke:var(--pur);stroke-width:1.6;stroke-dasharray:5 4;vector-effect:non-scaling-stroke;pointer-events:none}
.sp-spawn_human .sp,.sp-human_flee .sp{fill:var(--org)}
g[data-ly="spawn"] .mt,g[data-ly="human"] .mt,g[data-ly="event"] .mt,g[data-ly="route"] .mt,g[data-ly="inter"] .big .mt,g[data-ly="haz"] .mt{fill:#fff}
.ia{fill:var(--teal);stroke:var(--bg);stroke-width:1}.ia-noise{fill:var(--mag)}.ia-mover,.ia-push{fill:var(--panel);stroke:var(--teal);stroke-width:1.6}
.hz{fill:url(#p-haz);stroke:var(--red);stroke-width:1.6;vector-effect:non-scaling-stroke}.hz-pet{fill:url(#p-pet);stroke-dasharray:6 4}.hz-wind{stroke-dasharray:2 3}.hz-rival{fill:url(#p-pet)}
.hzm{fill:var(--red);stroke:var(--bg);stroke-width:1}.dogpath{fill:none;stroke:var(--red);stroke-width:1.6;stroke-dasharray:1 6;stroke-linecap:round;vector-effect:non-scaling-stroke}
.rc{pointer-events:none}.rc0{fill:url(#p-reach)}.rc1{fill:url(#p-reach2)}.cv{fill:url(#p-cover);pointer-events:none}
.blind{fill:url(#p-cover);stroke:var(--grn);stroke-width:1.8;stroke-dasharray:7 4;vector-effect:non-scaling-stroke}.cont{fill:var(--grn);fill-opacity:.08;stroke:var(--grn);stroke-width:2.2;vector-effect:non-scaling-stroke}
.stand{fill:var(--org);fill-opacity:.14;pointer-events:none}.wander{fill:none;stroke:var(--org);stroke-width:2;stroke-dasharray:11 6;vector-effect:non-scaling-stroke}
.hx{fill:none;stroke:var(--org);stroke-width:2}.hwarn{fill:var(--org);stroke:var(--bg);stroke-width:1}.hwp{fill:var(--org);stroke:var(--bg)}
.evz{fill:var(--mag);fill-opacity:.06;stroke:var(--mag);stroke-width:1.8;stroke-dasharray:2 5;stroke-linecap:round;vector-effect:non-scaling-stroke}.evm{fill:var(--mag);stroke:var(--bg);stroke-width:1.2}
.sec{fill:var(--ink);stroke:var(--bg);stroke-width:1.2}g[data-ly="event"] .sec ~ .mt{fill:var(--bg)}
.iso{fill:var(--iso);pointer-events:none}.iso-a{fill-opacity:.6}.iso-b{fill-opacity:.44}.iso-c{fill-opacity:.3}.iso-d{fill-opacity:.18}.iso-e{fill-opacity:.08}
.il{font-weight:700;font-size:calc(11px*var(--k));fill:var(--iso);stroke-width:calc(3px*var(--k))}
.hc{stroke:var(--ink);stroke-opacity:.3;stroke-width:1;vector-effect:non-scaling-stroke}.hc-dead{fill:url(#p-dead)}.hc-transit{fill:url(#p-transit)}.hc-contested{fill:var(--org);fill-opacity:.42}.hc-safe{fill:var(--grn);fill-opacity:.3}
.hl{font-weight:700;font-size:calc(12px*var(--k));fill:var(--ink);stroke-width:calc(2.5px*var(--k))}.hu{stroke:var(--ink);stroke-width:1.2}rect.hu.hc-dead{fill:var(--mut)}rect.hu.hc-contested{fill:var(--org);fill-opacity:1}rect.hu.hc-safe{fill:var(--grn);fill-opacity:1}
.rt{fill:none;stroke:var(--rt);stroke-width:3;stroke-dasharray:10 5;stroke-linejoin:round;vector-effect:non-scaling-stroke}.rtm{fill:var(--rt);stroke:var(--bg);stroke-width:1.5}
.ds{fill:var(--ink);fill-opacity:.03;stroke:var(--ink);stroke-width:2.2;stroke-dasharray:3 7;vector-effect:non-scaling-stroke}.dl{font-weight:800;font-size:calc(12.5px*var(--k));fill:var(--ink);stroke-width:calc(4px*var(--k));pointer-events:auto}
.pbg-dead{fill:var(--mut);fill-opacity:.18}.pl-dead{stroke:var(--mut);stroke-width:1.1}.pd-transit{fill:var(--mut)}.pl-cover{stroke:var(--grn);stroke-width:.5;fill:none}.pl-reach{stroke:var(--org);stroke-width:.45}.pl-haz{stroke:var(--red);stroke-width:.5}.pd-haz{fill:var(--red)}
.sel{stroke:var(--acc)!important;stroke-width:3.5px!important;stroke-dasharray:none!important}.mk.sel>:first-child{stroke:var(--acc);stroke-width:3.5px}
</style></head><body>
<div id="top"><div class="row" id="areas">
<button class="chip on" data-a="all">Вся карта</button><button class="chip" data-a="kitchen">Кухня</button><button class="chip" data-a="living">Гостиная</button><button class="chip" data-a="yard">Двор</button><button class="chip" data-a="street">Улица</button>
</div><div class="row" id="tiers">
<button class="chip on" data-tier="all">Все ярусы</button><button class="chip" data-tier="f">Пол</button><button class="chip" data-tier="u">Под мебелью</button><button class="chip" data-tier="w">Столешницы и столы</button><button class="chip" data-tier="t">Верха и полки</button><button class="chip" data-tier="i">Внутри</button><button class="chip" data-tier="v">Вентиляция</button>
</div></div>
<div id="wrap"><svg id="map" viewBox="-167 -63 380 231" preserveAspectRatio="xMidYMid meet" data-z="1">@@SVG@@</svg>
<div id="north">С<br>▲</div><div id="scale"><div id="bar"></div><span id="barl"></span></div>
<button id="btnL">Слои · легенда · итоги</button></div>
<div class="sheet" id="sheet"><div class="sh"><div class="tabs"><button class="chip on" data-tab="tl">Слои и легенда</button><button class="chip" data-tab="ts">Итоги аудита</button></div><button class="x" data-close="sheet" aria-label="Закрыть">×</button></div>
<div class="sb"><div id="tl">@@LAYERS@@<p class="mut">Тащи карту пальцем, своди и разводи два пальца для масштаба, нажми на предмет — откроется карточка. Север сверху. Все размеры в uu (1 м = 1900 uu).</p></div><div id="ts" hidden>@@SUMMARY@@</div></div></div>
<div class="sheet" id="card"><div class="sh"><h2 id="ct"></h2><button class="x" data-close="card" aria-label="Закрыть">×</button></div><div class="sb"><table class="cr" id="cb"></table><div class="more" id="cm"></div></div></div>
<script id="cards" type="application/json">@@CARDS@@</script>
<script>
(function(){'use strict';
var svg=document.getElementById('map'),CARDS=JSON.parse(document.getElementById('cards').textContent),VIEWS=@@VIEWS@@;
var bar=document.getElementById('bar'),barl=document.getElementById('barl'),vb=[-167,-63,380,231],sel=null;
function size(){var r=svg.getBoundingClientRect();return [Math.max(60,r.width),Math.max(60,r.height),r.left,r.top];}
function apply(){
  svg.setAttribute('viewBox',vb.map(function(v){return Math.round(v*100)/100;}).join(' '));
  var s=size(),k=vb[2]/s[0],ppm=19/k;
  svg.style.setProperty('--k',k);svg.style.setProperty('--m',k*Math.max(.55,Math.min(1.15,ppm/45)));
  svg.setAttribute('data-z',ppm<24?1:(ppm<70?2:3));
  var st=[.05,.1,.2,.5,1,2,5,10],m=st[0];for(var i=0;i<st.length;i++){if(st[i]*ppm<=130)m=st[i];}
  if(!lastK||Math.abs(k/lastK-1)>.03){lastK=k;sched();}
  bar.style.width=Math.round(m*ppm)+'px';barl.textContent=(m<1?Math.round(m*100)+' см':m+' м')+' = '+Math.round(m*125)+' муравьёв в длину';
}
var lastK=0,pend=0;
function sched(){if(!pend){pend=1;(window.requestAnimationFrame||setTimeout)(declutter);}}
function lv(t,z){return t.classList.contains('dl')?-1:t.classList.contains('l2')?1:(t.classList.contains('l3')?2:(z===1?0:3));}
function declutter(){pend=0;var z=+svg.getAttribute('data-z'),ls=Array.prototype.slice.call(svg.querySelectorAll('.dl')).concat(Array.prototype.slice.call(svg.querySelectorAll('g.ly[data-ly="label"] .lb'))),kept=[],i,j;
  ls.forEach(function(t){t.classList.remove('lhide');});ls.sort(function(a,b){return lv(a,z)-lv(b,z);});
  for(i=0;i<ls.length;i++){var t=ls[i];if(!t.getClientRects().length)continue;var r=t.getBoundingClientRect(),hit=false;
    for(j=0;j<kept.length;j++){var q=kept[j];if(r.left<q.right+4&&r.right>q.left-4&&r.top<q.bottom+2&&r.bottom>q.top-2){hit=true;break;}}
    if(hit)t.classList.add('lhide');else kept.push(r);}}
function fit(v){var s=size(),x0=v[0]/100,x1=v[2]/100,y0=-v[3]/100,y1=-v[1]/100,w=x1-x0,h=y1-y0,a=s[0]/s[1];
  if(w/h<a){var nw=h*a;x0-=(nw-w)/2;w=nw;}else{var nh=w/a;y0-=(nh-h)/2;h=nh;}vb=[x0,y0,w,h];apply();}
function zoomAt(px,py,f){var s=size(),k=vb[2]/s[0],ux=vb[0]+(px-s[2])*k,uy=vb[1]+(py-s[3])*k,nw=Math.max(6,Math.min(900,vb[2]*f));f=nw/vb[2];
  vb=[ux-(ux-vb[0])*f,uy-(uy-vb[1])*f,vb[2]*f,vb[3]*f];}
var P={},moved=0;
function n(){return Object.keys(P).length;}
svg.addEventListener('pointerdown',function(ev){try{svg.setPointerCapture(ev.pointerId);}catch(x){}P[ev.pointerId]=[ev.clientX,ev.clientY];if(n()===1)moved=0;});
svg.addEventListener('pointermove',function(ev){var p=P[ev.pointerId];if(!p)return;var c=[ev.clientX,ev.clientY],k=vb[2]/size()[0];
  if(n()===1){vb[0]-=(c[0]-p[0])*k;vb[1]-=(c[1]-p[1])*k;moved+=Math.abs(c[0]-p[0])+Math.abs(c[1]-p[1]);}
  else{var o=null;for(var id in P){if(String(id)!==String(ev.pointerId))o=P[id];}
    if(o){var d0=Math.hypot(p[0]-o[0],p[1]-o[1]),d1=Math.hypot(c[0]-o[0],c[1]-o[1]);vb[0]-=(c[0]-p[0])*k/2;vb[1]-=(c[1]-p[1])*k/2;if(d0>4&&d1>4)zoomAt((c[0]+o[0])/2,(c[1]+o[1])/2,d0/d1);}moved+=20;}
  P[ev.pointerId]=c;apply();});
function up(ev){if(!P[ev.pointerId])return;delete P[ev.pointerId];if(n()===0&&moved<9&&ev.type==='pointerup')tap(ev.clientX,ev.clientY);}
svg.addEventListener('pointerup',up);svg.addEventListener('pointercancel',up);
svg.addEventListener('wheel',function(ev){ev.preventDefault();zoomAt(ev.clientX,ev.clientY,ev.deltaY>0?1.18:1/1.18);apply();},{passive:false});
svg.addEventListener('dblclick',function(ev){zoomAt(ev.clientX,ev.clientY,.5);apply();});
window.addEventListener('resize',function(){var s=size();vb[3]=vb[2]*s[1]/s[0];apply();});
function tap(x,y){var out=[],best=null,bd=30*30,list=svg.querySelectorAll('.mk[data-c]'),i,el;
  for(i=0;i<list.length;i++){el=list[i];if(!el.getClientRects().length)continue;var r=el.getBoundingClientRect(),dx=(r.left+r.right)/2-x,dy=(r.top+r.bottom)/2-y,d=dx*dx+dy*dy;if(d<bd){out.push([d,el]);}}
  out.sort(function(a,b){return a[0]-b[0];});out=out.map(function(a){return a[1];});
  var st=document.elementsFromPoint?document.elementsFromPoint(x,y):[document.elementFromPoint(x,y)];
  for(i=0;i<st.length;i++){el=st[i]&&st[i].closest?st[i].closest('[data-c]'):null;if(el&&svg.contains(el)&&out.indexOf(el)<0)out.push(el);}
  var seen={},u=[];for(i=0;i<out.length;i++){var c=out[i].getAttribute('data-c');if(!seen[c]){seen[c]=1;u.push(out[i]);}}
  if(u.length)show(u[0],u.slice(1,7));else close('card');}
function show(el,more){if(sel)sel.classList.remove('sel');sel=el;el.classList.add('sel');var c=CARDS[+el.getAttribute('data-c')];
  document.getElementById('ct').innerHTML='';var h=document.getElementById('ct');h.textContent=c.t;var sm=document.createElement('small');sm.textContent=c.k;h.appendChild(sm);
  var tb=document.getElementById('cb');tb.innerHTML='';c.r.forEach(function(r){var tr=document.createElement('tr'),a=document.createElement('td'),b=document.createElement('td');a.textContent=r[0];b.textContent=r[1];tr.appendChild(a);tr.appendChild(b);tb.appendChild(tr);});
  var cm=document.getElementById('cm');cm.innerHTML='';if(more&&more.length){var p=document.createElement('div');p.className='mut';p.textContent='Ещё в этой точке:';cm.appendChild(p);
    more.forEach(function(m){var b=document.createElement('button');var k=CARDS[+m.getAttribute('data-c')];b.textContent=k.t+' · '+k.k;b.addEventListener('click',function(){show(m,[el].concat(more.filter(function(z){return z!==m;})));});cm.appendChild(b);});}
  close('sheet');document.getElementById('card').classList.add('open');}
function close(id){document.getElementById(id).classList.remove('open');if(id==='card'&&sel){sel.classList.remove('sel');sel=null;}}
Array.prototype.forEach.call(document.querySelectorAll('[data-close]'),function(b){b.addEventListener('click',function(){close(b.getAttribute('data-close'));});});
document.getElementById('btnL').addEventListener('click',function(){close('card');document.getElementById('sheet').classList.toggle('open');});
function chips(id,attr,fn){var row=document.getElementById(id);row.addEventListener('click',function(ev){var b=ev.target.closest('.chip');if(!b)return;Array.prototype.forEach.call(row.querySelectorAll('.chip'),function(c){c.classList.toggle('on',c===b);});fn(b.getAttribute(attr));});}
chips('areas','data-a',function(a){fit(VIEWS[a]);});
chips('tiers','data-tier',function(t){var els=svg.querySelectorAll('[data-t]');for(var i=0;i<els.length;i++){var el=els[i],ok=t==='all'||(' '+el.getAttribute('data-t')+' ').indexOf(' '+t+' ')>=0,geo=el.classList.contains('ob');
  el.classList.toggle('ghost',!ok&&geo);el.classList.toggle('hid',!ok&&!geo);}sched();});
Array.prototype.forEach.call(document.querySelectorAll('.tabs .chip'),function(b){b.addEventListener('click',function(){Array.prototype.forEach.call(document.querySelectorAll('.tabs .chip'),function(c){c.classList.toggle('on',c===b);});
  document.getElementById('tl').hidden=b.getAttribute('data-tab')!=='tl';document.getElementById('ts').hidden=b.getAttribute('data-tab')!=='ts';});});
function layer(id,on){var g=svg.querySelector('g.ly[data-ly="'+id+'"]');if(g)g.style.display=on?'':'none';sched();}
Array.prototype.forEach.call(document.querySelectorAll('#tl input[data-ly]'),function(c){layer(c.getAttribute('data-ly'),c.checked);c.addEventListener('change',function(){layer(c.getAttribute('data-ly'),c.checked);});});
var q=(location.hash||'').replace('#','').split('&');q.forEach(function(kv){var a=kv.split('=');if(a[0]==='layers'){Array.prototype.forEach.call(document.querySelectorAll('#tl input[data-ly]'),function(c){c.checked=a[1].split(',').indexOf(c.getAttribute('data-ly'))>=0;layer(c.getAttribute('data-ly'),c.checked);});}});
var a0='all';q.forEach(function(kv){var a=kv.split('=');if(a[0]==='area'&&VIEWS[a[1]])a0=a[1];});
fit(VIEWS[a0]);Array.prototype.forEach.call(document.querySelectorAll('#areas .chip'),function(c){c.classList.toggle('on',c.getAttribute('data-a')===a0);});
q.forEach(function(kv){var a=kv.split('=');if(a[0]==='sheet'){document.getElementById('sheet').classList.add('open');if(a[1]==='ts')document.querySelector('.tabs .chip[data-tab="ts"]').click();}if(a[0]==='tier'){var b=document.querySelector('#tiers .chip[data-tier="'+a[1]+'"]');if(b)b.click();}
  if(a[0]==='card'){var ce=svg.querySelector('[data-c="'+(+a[1])+'"]');if(ce)show(ce,[]);}});
})();
</script></body></html>
'''
cards_json = json.dumps(CARDS, ensure_ascii=False, separators=(',', ':')).replace('</', '<\\/')
page = PAGE.replace('@@SVG@@', svg_inner).replace('@@LAYERS@@', layers_html).replace('@@SUMMARY@@', summary_html).replace('@@VIEWS@@', json.dumps(VIEWS)).replace('@@CARDS@@', cards_json)

# ---- checks ---------------------------------------------------------------------------------------------------------------
problems = []
try: json.loads(cards_json.replace('<\\/', '</'))
except Exception as ex: problems.append('cards JSON does not parse: %s' % ex)
try:
    root = ET.fromstring('<svg>' + svg_inner + '</svg>')
    n_el = sum(1 for _ in root.iter())
except Exception as ex:
    problems.append('SVG is not well-formed XML: %s' % ex); n_el = 0
low = page.lower()
for bad in ('http:', 'https:', '<link', '@import', 'src="', "src='", 'url(//', 'url("', "url('"):
    if bad in low: problems.append('external reference in the HTML: %s' % bad)
for u in re.findall(r'url\(([^)]*)\)', page):
    if not u.startswith('#'): problems.append('url() that is not a local reference: %s' % u)
BOUNDS = {'kitchen': [-7250, -5250, 7250, 5650], 'living': [6950, -9250, 20450, 1750], 'yard': [-12100, -16100, 21100, -4950], 'street': [-16550, -5450, -6950, 6050]}
listed = set(s.get('object') for s in D['suspicious'] if s.get('object'))
outside = [o for o in D['objects'] if (o['bb'][0] < BOUNDS[o['area']][0] or o['bb'][1] < BOUNDS[o['area']][1] or o['bb'][2] > BOUNDS[o['area']][2] or o['bb'][3] > BOUNDS[o['area']][3])]
for o in outside:
    if o['id'] not in listed: problems.append('object outside its area and not listed as suspicious: %s %s' % (o['id'], o['bb']))
size_kb = len(page.encode('utf-8')) / 1024.0
if size_kb > 1500: problems.append('the file is %.0f KB (limit about 1500)' % size_kb)
with open(os.path.join(OUT, 'level_map.html'), 'w', encoding='utf-8', newline='\n') as fo:
    fo.write(page)
with open(os.path.join(OUT, 'level_districts_draft.json'), 'w', encoding='utf-8', newline='\n') as fo:
    json.dump({'note': 'ЧЕРНОВИК: районы нарезаны по тому, что физически стоит; границы и названия обсуждаются', 'districts': DIST_OUT}, fo, ensure_ascii=False, indent=1)
print('wrote level_map.html: %.0f KB, %d SVG elements, %d cards' % (size_kb, n_el, len(CARDS)))
print('layers: ' + ', '.join('%s %d' % (k, len(G[k])) for k in ORDER))
print('objects outside their area: %d (all listed as suspicious: %s)' % (len(outside), all(o['id'] in listed for o in outside)))
print('CHECKS OK' if not problems else 'CHECKS FAILED:\n  ' + '\n  '.join(problems))
sys.exit(1 if problems else 0)
