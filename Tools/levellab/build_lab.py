# -*- coding: utf-8 -*-
"""KAN-37 «Лаборатория уровней»: grey-box floor plans as data.

One function per variant lays the flat out in metres (origin: the inner south-west corner of the flat, X east, Y north,
Z up, floor at 0). This script turns that into:
  docs/level/lab/lab_<X>.json        the variant (boxes, zones, hatches, nest, food, cake, Human, route graph)
  Saved/levellab/plan_<X>.png         a top-down plan (rooms, furniture, climb paths, tunnels, hatches, food, graph)
  Saved/levellab/check_<X>.txt/.json  the automatic check (nest -> first food per room, routes to the cake, unreachable food,
                                      the corridor blocked by the Human)
  Source/AntKitchen/AntLabData.inl    every docs/level/lab/lab_*.json as bytes (the game parses it at run time)

Run:  python Tools/levellab/build_lab.py [A]      (no argument: every variant that has a layout function)
Only A exists for now (the owner picked «Двушка», 26.11.2026); the loader takes more variants without code changes.
"""
import json, math, os, sys, heapq, itertools

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
UU = 1900.0                 # uu per metre (KitchenBuilder.h)
SPRINT = 456.0 / UU         # m/s: the ant's sprint
CLIMB_MUL = 1.3             # a metre up a curtain costs 1.3 metres of floor
CEIL = 2.6
FIRST_FOOD_LIMIT = 25.0     # s, nest -> the first food of every room

# ---------------------------------------------------------------------------------------------------------- geometry

class Flat:
    def __init__(self, vid, name):
        self.vid, self.name = vid, name
        self.rooms, self.boxes, self.zones, self.hatches = [], [], [], []
        self.nodes, self.edges, self.food, self.cover = {}, [], [], []
        self.nest, self.cake, self.human = None, None, None
        self.notes = []
        self._n = itertools.count()

    # -- rooms
    def room(self, rid, name, rect, hero, physics, floor_col):
        self.rooms.append(dict(id=rid, name=name, rect=list(rect), hero=hero, physics=physics,
                               area=round((rect[2] - rect[0]) * (rect[3] - rect[1]), 2)))
        x0, y0, x1, y1 = rect
        self.box('floor_' + rid, (x0, y0, 0.0), (x1, y1, 0.004), 'decal', floor_col, climb=False, collide=False, room=rid)

    def room_at(self, x, y):
        for r in self.rooms:
            x0, y0, x1, y1 = r['rect']
            if x0 - 1e-6 <= x <= x1 + 1e-6 and y0 - 1e-6 <= y <= y1 + 1e-6:
                return r['id']
        return ''

    # -- boxes
    def box(self, bid, mn, mx, kind, col, climb=False, collide=True, room=None, rot=None, tag=''):
        mn, mx = [float(v) for v in mn], [float(v) for v in mx]
        for i in range(3):
            if mx[i] < mn[i]:
                mn[i], mx[i] = mx[i], mn[i]
        if any(mx[i] - mn[i] < 1e-5 for i in range(3)):
            return
        c = [(mn[i] + mx[i]) / 2 for i in range(3)]
        s = [mx[i] - mn[i] for i in range(3)]
        if room is None:
            room = self.room_at(c[0], c[1])
        b = dict(id='%s#%d' % (bid, next(self._n)), kind=kind, c=[round(v, 4) for v in c], s=[round(v, 4) for v in s],
                 climb=1 if climb else 0, col=col, room=room)
        if not collide:
            b['nocol'] = 1
        if rot:
            b['r'] = [round(v, 3) for v in rot]
        if tag:
            b['tag'] = tag
        self.boxes.append(b)

    def beam(self, bid, a, b, w, t, kind, col, climb=True):
        """A thin board/cable/garland from point a to point b (centre line on its top face), w wide, t thick."""
        ax, ay, az = a; bx, by, bz = b
        dx, dy, dz = bx - ax, by - ay, bz - az
        L = math.sqrt(dx * dx + dy * dy + dz * dz)
        yaw = math.degrees(math.atan2(dy, dx))
        pitch = math.degrees(math.atan2(dz, math.hypot(dx, dy)))
        c = ((ax + bx) / 2, (ay + by) / 2, (az + bz) / 2 - t / 2)
        self.boxes.append(dict(id='%s#%d' % (bid, next(self._n)), kind=kind, c=[round(v, 4) for v in c], s=[round(L, 4), w, t],
                               r=[round(pitch, 3), round(yaw, 3), 0.0], climb=1 if climb else 0, col=col, room=self.room_at(c[0], c[1])))

    def slab(self, bid, mn, mx, holes, kind, col, climb=False, axis=None):
        """A box with rectangular holes through it along its thinnest axis (or `axis`). holes: [(a0, a1, b0, b1)] in the two
        other axes, in order (e.g. a wall along X with thickness in Y: holes are (x0, x1, z0, z1))."""
        s = [mx[i] - mn[i] for i in range(3)]
        n = axis if axis is not None else s.index(min(s))
        u, v = [i for i in range(3) if i != n]
        us = sorted(set([mn[u], mx[u]] + [h[k] for h in holes for k in (0, 1) if mn[u] < h[k] < mx[u]]))
        for i in range(len(us) - 1):
            a0, a1 = us[i], us[i + 1]
            cuts = sorted([(max(h[2], mn[v]), min(h[3], mx[v])) for h in holes if h[0] <= a0 + 1e-9 and h[1] >= a1 - 1e-9])
            z = mn[v]
            for c0, c1 in cuts + [(mx[v], mx[v])]:
                if c0 > z + 1e-6:
                    p0, p1 = list(mn), list(mx)
                    p0[u], p1[u], p0[v], p1[v] = a0, a1, z, c0
                    self.box(bid, p0, p1, kind, col, climb)
                z = max(z, c1)

    def wall(self, bid, mn, mx, holes=(), col='#c8c8c4'):
        self.slab(bid, mn, mx, list(holes), 'wall', col, climb=False)

    def shell(self, bid, mn, mx, t, holes, col_out, col_in='#8f8a80', top=False):
        """A hollow box (a ventilation block, a pipe chase): four sides (and a lid if top), outer half smooth (no climbing),
        inner half rough (ants climb it). holes: {'S'|'N'|'E'|'W': [(a0, a1, z0, z1)]} in world coordinates."""
        x0, y0, z0 = mn; x1, y1, z1 = mx
        h = t / 2
        sides = {
            'S': ((x0, y0, z0), (x1, y0 + h, z1), (x0, y0 + h, z0), (x1, y0 + t, z1)),
            'N': ((x0, y1 - h, z0), (x1, y1, z1), (x0, y1 - t, z0), (x1, y1 - h, z1)),
            'W': ((x0, y0 + t, z0), (x0 + h, y1 - t, z1), (x0 + h, y0 + t, z0), (x0 + t, y1 - t, z1)),
            'E': ((x1 - h, y0 + t, z0), (x1, y1 - t, z1), (x1 - t, y0 + t, z0), (x1 - h, y1 - t, z1)),
        }
        for k, (o0, o1, i0, i1) in sides.items():
            hs = holes.get(k, [])
            self.slab(bid + k + 'o', o0, o1, hs, 'shell', col_out, climb=False, axis=1 if k in 'SN' else 0)
            self.slab(bid + k + 'i', i0, i1, hs, 'shell_in', col_in, climb=True, axis=1 if k in 'SN' else 0)
        if top:
            self.box(bid + 'lid', (x0, y0, z1 - t), (x1, y1, z1), 'shell', col_out)

    # -- the rest
    def zone(self, kind, mn, mx, note, temp=None, rate=None):
        z = dict(kind=kind, min=list(mn), max=list(mx), note=note)
        if temp is not None:
            z['temp'], z['rate'] = temp, rate
        self.zones.append(z)

    def hatch(self, hid, name, mn, mx, cls, use, note):
        c = [(mn[i] + mx[i]) / 2 for i in range(3)]
        s = [mx[i] - mn[i] for i in range(3)]
        self.hatches.append(dict(id=hid, name=name, c=c, s=s, cls=cls, use=[list(p) for p in use], note=note))

    def node(self, nid, at, name, hole=False):
        self.nodes[nid] = dict(id=nid, at=list(at), name=name, room=self.room_at(at[0], at[1]), hole=1 if (hole or at[2] > 0.3) else 0)

    def edge(self, a, b, kind, gate=''):
        A, B = self.nodes[a]['at'], self.nodes[b]['at']
        horiz = math.hypot(A[0] - B[0], A[1] - B[1])
        vert = abs(A[2] - B[2])
        run = (horiz + vert * CLIMB_MUL) / SPRINT
        seen = {'floor': 0.9, 'cover': 0.2, 'loggia': 0.6, 'climb': 0.6, 'high': 0.4, 'wall': 0.0}[kind] * run
        self.edges.append(dict(a=a, b=b, kind=kind, run=round(run, 2), seen=round(seen, 2), gate=gate))

    def chain(self, ids, kind):
        for a, b in zip(ids, ids[1:]):
            self.edge(a, b, kind)

    def add_food(self, at, tier, node, heavy=False, what=''):
        self.food.append(dict(at=list(at), tier=tier, node=node, room=self.room_at(at[0], at[1]), heavy=1 if heavy else 0, what=what))


# ---------------------------------------------------------------------------------------------------------- palette
WALL = '#d2cdc4'      # plaster: smooth, ants do not climb it
OUTW = '#b9b3a8'      # outer walls
FURN = '#8c9198'      # furniture bodies: lacquer, laminate, enamel (no climbing)
FURN2 = '#a3a7ad'
TOP = '#b5b9be'       # worktops, table tops, shelves
CLIMB = '#e8902a'     # what ants climb: towels, curtains, cables, coats, blocks, boxes (orange on purpose)
SHELL = '#9aa1a8'     # vent block / pipe chase outside
SHELL_IN = '#6f6a62'  # their rough inside (climbable)
GLASS = '#a9d6ee'
NEST = '#6b4a2b'
WATER = '#4f8fd0'
HOT = '#d0503a'
COLD = '#7fb7e0'

# ---------------------------------------------------------------------------------------------------------- variant A

def variant_A():
    F = Flat('A', 'Двушка')
    F.area_note = 'панельная двушка 54 м²: коридор посередине, кухня 10.8, гостиная 19.1, спальня 12.1, ванная и туалет раздельно, прихожая, лоджия вдоль кухни и гостиной'
    T = 0.12          # partitions
    O = 0.3           # outer walls
    DOOR = 2.0
    # ---- rooms (inner rectangles)
    F.room('kitchen', 'Кухня', (0.0, 0.0, 3.0, 3.6), 'плита с духовкой', 'жар и холод: конфорки и батарея жгут, подоконник у окна дует холодом', '#cfc6b4')
    F.room('living', 'Гостиная', (3.12, 0.0, 8.42, 3.6), 'праздничный стол с тортом', 'ткань и провода: шторы, скатерть, гирлянда, кабель ТВ, плед', '#c9c2b8')
    F.room('loggia', 'Лоджия', (0.0, -1.4, 8.42, -0.3), 'сушилка с простынёй', 'ветер и погода: холодно, у форточки лужа (ветер пока только в данных)', '#b8bcb6')
    F.room('corridor', 'Коридор', (0.6, 3.72, 4.48, 5.0), 'кабель-канал под потолком', 'узкое место: Человек здесь перекрывает всё', '#c4bfb3')
    F.room('bedroom', 'Спальня', (4.6, 3.72, 8.42, 6.9), 'кровать с одеялом', 'мягкое и игрушки детей: одеяло, кубики, мишка', '#c7c0c9')
    F.room('bath', 'Ванная', (0.0, 5.12, 1.74, 6.9), 'ванна с водой', 'вода и пар: в ванне мокро, над ней тепло и влажно, лужа у ванны', '#b9c9cf')
    F.room('wc', 'Туалет', (1.86, 5.12, 2.86, 6.9), 'держатель с рулоном', 'бумага как верёвка', '#bcc8cc')
    F.room('hall', 'Прихожая', (2.98, 5.12, 4.48, 6.9), 'вешалка с пальто', 'обувь и пальто как вертикаль', '#c8bfae')

    # ---- floor slab, ceiling, outer shell
    F.box('slab', (-O, -1.5, -0.12), (8.42 + O, 6.9 + O, 0.0), 'floor', '#a7a39b', room='')
    F.box('ceiling', (-O, -1.5, CEIL), (8.42 + O, 6.9 + O, CEIL + 0.1), 'ceiling', '#e9e7e2', room='', tag='ceiling')
    F.wall('outN', (-O, 6.9, 0), (8.42 + O, 6.9 + O, CEIL), [(6.0, 7.2, 0.85, 2.3)], OUTW)          # the bedroom window
    F.box('glassN', (6.0, 7.02, 0.85), (7.2, 7.06, 2.3), 'glass', GLASS)
    F.box('sillN', (6.0, 6.7, 0.82), (7.2, 6.9, 0.85), 'top', TOP)
    F.box('frontdoor', (3.2, 6.88, 0.0), (4.1, 6.9, 2.05), 'furn', '#6d5a48')                          # the front door, shut
    F.wall('outW', (-O, -1.5, 0), (0.0, 6.9, CEIL), [], OUTW)
    F.wall('outE', (8.42, -1.5, 0), (8.42 + O, 6.9, CEIL), [], OUTW)
    # the wall between the rooms and the loggia: two windows, two balcony doors
    F.wall('outS', (0.0, -0.3, 0), (8.42, 0.0, CEIL), [(0.8, 1.8, 0.88, 2.4), (2.0, 2.8, 0.0, 2.1), (4.5, 5.6, 0.88, 2.4), (6.0, 6.8, 0.0, 2.1)], OUTW)
    F.box('glassK', (0.8, -0.17, 0.88), (1.8, -0.13, 2.4), 'glass', GLASS)
    F.box('glassL', (4.5, -0.17, 0.88), (5.6, -0.13, 2.4), 'glass', GLASS)
    F.box('thrK', (2.0, -0.3, 0.0), (2.8, 0.0, 0.02), 'climb', CLIMB, climb=True)
    F.box('thrL', (6.0, -0.3, 0.0), (6.8, 0.0, 0.02), 'climb', CLIMB, climb=True)
    # loggia front: a concrete parapet, glazing above (one pane is an open transom: the wind and the rain come in there)
    F.wall('parapet', (0.0, -1.5, 0), (8.42, -1.4, 1.0), [], OUTW)
    F.box('glassG', (0.0, -1.48, 1.0), (8.42, -1.44, CEIL), 'glass', GLASS)

    # ---- partitions (doors 0.8 wide, 2.0 high)
    F.wall('wKC', (0.6, 3.6, 0), (3.0, 3.6 + T, CEIL), [(2.1, 2.9, 0, DOOR), (0.62, 0.82, 2.28, 2.40)])     # kitchen | corridor (+ the riser duct)
    F.wall('wLC', (3.0, 3.6, 0), (8.42, 3.6 + T, CEIL), [(3.5, 4.3, 0, DOOR), (3.56, 3.64, 2.42, 2.52), (6.0, 6.4, 0.0, 0.05)])   # living | corridor, bedroom (+ cable hole, skirting gap)
    F.wall('wKL', (3.0, 0.0, 0), (3.0 + T, 3.6, CEIL), [])                                                     # kitchen | living: no door (the loop goes round)
    F.wall('wCN', (0.6, 5.0, 0), (4.48, 5.0 + T, CEIL), [(0.9, 1.5, 0, DOOR), (0.62, 0.82, 2.28, 2.40), (2.06, 2.66, 0, DOOR), (3.1, 4.36, 0, 2.2)])   # corridor | bath, wc, hall
    F.wall('wTW', (1.74, 5.12, 0), (1.74 + T, 6.9, CEIL), [])
    F.wall('wWH', (2.86, 5.12, 0), (2.86 + T, 6.9, CEIL), [])
    F.wall('wHB', (4.48, 3.72, 0), (4.48 + T, 6.9, CEIL), [(4.0, 4.8, 0, DOOR)])                           # corridor, hall | bedroom

    # ---- the ventilation block (вентблок) between the kitchen, the corridor and the bath: the nest is at its foot
    F.shell('vent', (0.0, 3.0, 0.0), (0.6, 5.12, CEIL), 0.06, {
        'S': [(0.18, 0.42, 0.0, 0.07)],                                  # the nest's kitchen entrance (a gap under the grille)
        'E': [(4.15, 4.45, 0.0, 0.07), (3.74, 3.92, 2.44, 2.56)],        # the corridor entrance; the hole up to the cable channel
        'N': [(0.2, 0.4, 2.0, 2.14)],                                     # the bathroom grille (hatch «vent»)
    }, SHELL)
    F.box('nestbed', (0.08, 3.08, 0.0), (0.52, 3.6, 0.03), 'decal', NEST, collide=False)
    F.box('nestmound', (0.15, 3.15, 0.0), (0.45, 3.45, 0.06), 'decal', NEST, collide=False)
    F.cover.append(dict(min=[0.06, 3.06, 0.0], max=[0.54, 5.06, CEIL], name='в вентблоке'))
    F.nest = dict(main=[0.3, 2.82, 0.0], second=[0.75, 4.3, 0.0], radius=0.3, spawn=[0.9, 2.5, 0.0], spawn_r=0.22,
                  note='гнездо в основании вентблока; вход из кухни (щель под решёткой) и из коридора')

    # ---- the riser (стояк): bath -> up -> a duct over the corridor -> down a chase in the kitchen corner -> under the sink
    F.shell('chaseK', (0.62, 3.42, 0.0), (0.82, 3.6 + 0.02, 2.28), 0.02, {'E': [(3.44, 3.6, 0.0, 0.12)], 'N': [(0.62, 0.82, 0.0, 2.28)]}, SHELL)
    F.shell('riser', (0.62, 5.14, 0.0), (0.82, 5.34, 2.28), 0.02, {'E': [(5.16, 5.32, 0.0, 0.08)]}, SHELL)
    F.box('ductF', (0.64, 3.6, 2.28), (0.80, 5.14, 2.30), 'shell_in', SHELL_IN, climb=True)
    F.box('ductR', (0.62, 3.42, 2.38), (0.82, 5.34, 2.40), 'shell', SHELL)
    F.box('ductW', (0.62, 3.42, 2.28), (0.64, 5.34, 2.38), 'shell_in', SHELL_IN, climb=True)
    F.box('ductE', (0.80, 3.42, 2.28), (0.82, 5.34, 2.38), 'shell_in', SHELL_IN, climb=True)
    F.cover.append(dict(min=[0.62, 3.42, 0.0], max=[0.82, 5.34, 2.4], name='в стояке'))

    # ---- kitchen
    F.box('kCabW', (0.0, 1.2, 0.0), (0.6, 3.0, 0.81), 'furn', FURN)                     # the west run
    F.box('kTopW', (0.0, 1.2, 0.81), (0.62, 3.0, 0.85), 'top', TOP)
    F.box('kettle', (0.1, 2.1, 0.85), (0.32, 2.32, 1.1), 'furn', FURN2)
    F.box('kCable', (0.62, 2.2, 0.0), (0.64, 2.24, 0.85), 'climb', CLIMB, climb=True)    # the kettle's cable down to the socket
    F.box('fridge', (0.0, 0.45, 0.0), (0.62, 1.15, 1.85), 'furn', '#d9dcdf')            # enamel: not climbable
    F.box('sinkL', (0.84, 3.0, 0.0), (0.86, 3.4, 0.81), 'furn', FURN)                   # the sink cabinet: hollow, the back corner open for the pipes
    F.box('sinkR', (1.42, 3.0, 0.0), (1.44, 3.6, 0.81), 'furn', FURN)
    F.box('sinkD', (0.86, 3.0, 0.03), (1.42, 3.02, 0.81), 'furn', FURN2)                 # its door: a 3 cm gap under it
    F.box('sinkT', (0.84, 3.0, 0.81), (1.44, 3.6, 0.85), 'top', TOP)
    F.cover.append(dict(min=[0.86, 3.02, 0.0], max=[1.42, 3.6, 0.81], name='под мойкой'))
    F.box('stove', (1.44, 3.0, 0.0), (2.04, 3.6, 0.85), 'furn', '#7d8186')
    F.box('towel', (1.6, 2.97, 0.0), (1.88, 3.0, 0.85), 'climb', CLIMB, climb=True)      # the tea towel on the oven handle
    F.box('wcabN', (0.84, 3.28, 1.45), (2.04, 3.6, 2.15), 'furn', FURN)
    F.box('wcabW', (0.0, 1.2, 1.45), (0.33, 3.0, 2.15), 'furn', FURN)
    F.box('hoodCable', (2.04, 3.55, 0.85), (2.06, 3.6, 2.15), 'climb', CLIMB, climb=True) # the hood's cable up to the cabinet tops
    F.box('kTable', (1.4, 1.0, 0.72), (2.4, 1.8, 0.75), 'top', TOP)
    for (lx, ly) in ((1.43, 1.03), (2.33, 1.03), (1.43, 1.73), (2.33, 1.73)):
        F.box('kLeg', (lx, ly, 0.0), (lx + 0.04, ly + 0.04, 0.72), 'furn', FURN)
    F.box('kCloth', (1.6, 0.98, 0.0), (2.2, 1.0, 0.75), 'climb', CLIMB, climb=True)      # the oilcloth hangs down to the floor on one side
    for cy in (0.55, 1.85):
        F.box('kChair', (1.62, cy, 0.42), (2.02, cy + 0.4, 0.45), 'furn', FURN2)
        for (lx, ly) in ((1.63, cy + 0.01), (1.98, cy + 0.01), (1.63, cy + 0.36), (1.98, cy + 0.36)):
            F.box('kChairLeg', (lx, ly, 0.0), (lx + 0.03, ly + 0.03, 0.42), 'furn', FURN)
    F.box('kSill', (0.8, 0.0, 0.85), (1.8, 0.25, 0.88), 'top', TOP)
    F.box('kCurt1', (0.8, 0.25, 0.0), (1.0, 0.27, 0.88), 'climb', CLIMB, climb=True)      # the curtain: floor -> sill
    F.box('kCurt2', (0.8, 0.10, 0.88), (1.0, 0.12, 2.43), 'climb', CLIMB, climb=True)     # its upper part: sill -> rod
    F.box('kRod', (0.7, 0.12, 2.40), (1.9, 0.16, 2.43), 'climb', CLIMB, climb=True)
    F.box('radiator', (0.9, 0.04, 0.12), (1.7, 0.12, 0.7), 'climb', '#c9c9c9', climb=True)
    F.zone('hot', (1.48, 3.04, 0.85), (2.0, 3.56, 0.95), 'конфорки (включены): горячо', temp=75, rate=14)
    F.zone('hot', (0.88, 0.0, 0.1), (1.72, 0.2, 0.75), 'батарея', temp=48, rate=6)
    F.zone('cold', (0.8, 0.0, 0.88), (1.8, 0.25, 1.0), 'подоконник: из окна дует', temp=6, rate=5)
    F.box('hotplate', (1.52, 3.08, 0.85), (1.96, 3.52, 0.852), 'decal', HOT, collide=False)

    # ---- living room
    F.box('tvStand', (3.12, 1.0, 0.0), (3.6, 2.6, 0.5), 'furn', FURN)
    F.box('tv', (3.15, 1.3, 0.5), (3.25, 2.3, 1.2), 'furn', '#2a2c30')
    F.box('tvCable', (3.6, 1.75, 0.0), (3.62, 1.79, 0.5), 'climb', CLIMB, climb=True)
    F.box('shelf', (5.0, 3.25, 0.0), (6.6, 3.6, 2.0), 'furn', FURN)
    F.box('fTable', (5.0, 1.2, 0.72), (6.6, 2.4, 0.75), 'top', TOP)                     # the birthday table
    for (lx, ly) in ((5.03, 1.23), (6.53, 1.23), (5.03, 2.33), (6.53, 2.33)):
        F.box('fLeg', (lx, ly, 0.0), (lx + 0.04, ly + 0.04, 0.72), 'furn', FURN)
    F.box('fCloth', (5.0, 1.18, 0.0), (6.6, 1.2, 0.75), 'climb', CLIMB, climb=True)      # the festive tablecloth, down to the floor
    F.box('lSill', (4.5, 0.0, 0.85), (5.6, 0.25, 0.88), 'top', TOP)
    F.box('lCurt1', (4.5, 0.25, 0.0), (4.7, 0.27, 0.88), 'climb', CLIMB, climb=True)
    F.box('lCurt2', (4.5, 0.10, 0.88), (4.7, 0.12, 2.48), 'climb', CLIMB, climb=True)
    F.box('lRod', (4.3, 0.12, 2.45), (7.0, 0.16, 2.48), 'climb', CLIMB, climb=True)
    F.beam('garland1', (6.4, 3.27, 2.0), (6.2, 2.39, 0.75), 0.03, 0.01, 'climb', CLIMB)  # the garland: shelf top -> table
    F.beam('garland2', (5.8, 0.15, 2.48), (5.8, 1.21, 0.75), 0.03, 0.01, 'climb', CLIMB) # and rod -> table
    F.box('sofaBase', (7.5, 0.8, 0.12), (8.42, 2.9, 0.45), 'furn', FURN2)
    F.box('sofaBack', (8.2, 0.8, 0.45), (8.42, 2.9, 0.85), 'furn', FURN2)
    for (lx, ly) in ((7.52, 0.82), (8.36, 0.82), (7.52, 2.84), (8.36, 2.84)):
        F.box('sofaLeg', (lx, ly, 0.0), (lx + 0.04, ly + 0.04, 0.12), 'furn', FURN)
    F.box('blanket', (7.48, 1.5, 0.0), (7.5, 2.0, 0.45), 'climb', CLIMB, climb=True)
    F.cover.append(dict(min=[7.5, 0.8, 0.0], max=[8.42, 2.9, 0.12], name='под диваном'))
    # the cable channel: from the vent block's top hole along the corridor, through the wall, along the living room to the shelf
    F.box('cabC', (0.6, 3.72, 2.42), (3.64, 3.76, 2.44), 'climb', CLIMB, climb=True)
    F.box('cabThru', (3.56, 3.56, 2.42), (3.64, 3.76, 2.44), 'climb', CLIMB, climb=True)
    F.box('cabL', (3.56, 3.56, 2.42), (5.04, 3.6, 2.44), 'climb', CLIMB, climb=True)
    F.box('cabDrop', (5.0, 3.56, 2.0), (5.04, 3.6, 2.44), 'climb', CLIMB, climb=True)
    F.box('cabFloor', (2.0, 3.72, 0.0), (2.02, 3.76, 2.44), 'climb', CLIMB, climb=True)  # the router cable down to the corridor floor
    F.box('ductS', (0.62, 3.42, 2.28), (0.82, 3.44, 2.38), 'shell_in', SHELL_IN, climb=True)
    F.box('ductN', (0.62, 5.32, 2.28), (0.82, 5.34, 2.38), 'shell_in', SHELL_IN, climb=True)

    # ---- bedroom
    F.box('bed', (6.4, 4.2, 0.25), (8.42, 5.8, 0.55), 'furn', FURN2)
    for (lx, ly) in ((6.42, 4.22), (8.36, 4.22), (6.42, 5.74), (8.36, 5.74)):
        F.box('bedLeg', (lx, ly, 0.0), (lx + 0.04, ly + 0.04, 0.25), 'furn', FURN)
    F.box('duvet', (6.38, 4.3, 0.0), (6.4, 5.7, 0.55), 'climb', CLIMB, climb=True)       # the duvet slipped down to the floor
    F.box('headboard', (8.3, 4.2, 0.55), (8.42, 5.8, 1.1), 'climb', CLIMB, climb=True)   # padded headboard
    F.cover.append(dict(min=[6.4, 4.2, 0.0], max=[8.42, 5.8, 0.25], name='под кроватью'))
    F.box('wardrobe', (4.6, 6.3, 0.0), (5.8, 6.9, 2.2), 'furn', FURN)
    F.box('toybox', (5.0, 3.9, 0.0), (5.6, 4.35, 0.4), 'furn', '#9a8fb0')
    for i, h in enumerate((0.13, 0.26, 0.4)):                                            # the kid's blocks, a staircase up to the toy box
        F.box('block', (5.6, 3.95 + 0.1 * i, 0.0), (5.7, 4.05 + 0.1 * i, h), 'climb', CLIMB, climb=True)
    F.box('bear', (7.0, 6.3, 0.0), (7.3, 6.6, 0.3), 'climb', CLIMB, climb=True)          # a plush bear
    F.zone('soft', (6.4, 4.2, 0.55), (8.3, 5.8, 0.7), 'мягкое: на одеяле не слышно шагов (пока только в данных)')
    F.box('skirtB', (4.6, 3.72, 0.05), (8.42, 3.74, 0.08), 'furn', '#7b6a58')            # the bedroom's skirting board

    # ---- bath
    F.box('washer', (0.0, 5.5, 0.0), (0.6, 6.1, 0.85), 'furn', '#dcdfe2')
    F.box('hose', (0.6, 5.7, 0.0), (0.62, 5.74, 0.85), 'climb', CLIMB, climb=True)
    F.box('bSink', (1.1, 5.12, 0.0), (1.6, 5.45, 0.85), 'furn', '#dcdfe2')
    F.box('bTowel', (0.15, 5.12, 0.4), (0.45, 5.15, 2.0), 'climb', CLIMB, climb=True)    # a towel on a hook under the grille
    F.box('bTowelLow', (0.15, 5.15, 0.0), (0.45, 5.17, 0.42), 'climb', CLIMB, climb=True) # its end on the floor
    F.box('tubFloor', (0.0, 6.2, 0.0), (1.74, 6.9, 0.1), 'furn', '#e3e6e8')
    F.box('tubS', (0.0, 6.2, 0.1), (1.74, 6.26, 0.55), 'furn', '#e3e6e8')
    F.box('tubW', (0.0, 6.26, 0.1), (0.06, 6.9, 0.55), 'furn', '#e3e6e8')
    F.box('tubE', (1.68, 6.26, 0.1), (1.74, 6.9, 0.55), 'furn', '#e3e6e8')
    F.box('water', (0.06, 6.26, 0.38), (1.68, 6.9, 0.384), 'decal', WATER, collide=False)
    F.box('mat', (0.6, 6.18, 0.0), (1.0, 6.2, 0.55), 'climb', CLIMB, climb=True)        # the bath mat hung over the side
    F.zone('wet', (0.06, 6.26, 0.1), (1.68, 6.9, 0.42), 'вода в ванне')
    F.zone('steam', (0.0, 6.2, 0.55), (1.74, 6.9, 1.6), 'пар над ванной: тепло и влажно', temp=36, rate=3)
    F.zone('wet', (0.9, 5.7, 0.0), (1.6, 6.18, 0.03), 'лужа у ванны')
    F.box('puddle', (0.9, 5.7, 0.0), (1.6, 6.18, 0.005), 'decal', WATER, collide=False)

    # ---- wc
    F.box('toilet', (2.1, 6.3, 0.0), (2.6, 6.9, 0.42), 'furn', '#e3e6e8')
    F.box('tank', (2.1, 6.72, 0.42), (2.6, 6.9, 0.8), 'furn', '#e3e6e8')
    F.box('paperShelf', (1.86, 5.95, 0.70), (1.98, 6.15, 0.72), 'top', TOP)
    F.box('paper', (1.98, 6.0, 0.0), (2.0, 6.1, 0.72), 'climb', CLIMB, climb=True)       # the unrolled paper down to the floor

    # ---- hall
    F.box('shoeRack', (3.0, 6.45, 0.0), (3.9, 6.88, 0.45), 'furn', FURN)
    F.box('boot', (3.3, 6.15, 0.0), (3.42, 6.45, 0.45), 'climb', CLIMB, climb=True)       # a tall boot leans on the rack
    F.box('hatShelf', (4.2, 5.3, 1.75), (4.48, 6.4, 1.78), 'top', TOP)
    F.box('coat', (4.17, 5.45, 0.05), (4.2, 5.75, 1.78), 'climb', CLIMB, climb=True)     # a long coat on the hook
    F.box('coat2', (4.17, 5.9, 0.6), (4.2, 6.2, 1.78), 'climb', CLIMB, climb=True)       # a short jacket (does not reach the floor)

    # ---- loggia
    F.box('box1', (7.6, -1.4, 0.0), (8.1, -0.95, 0.4), 'climb', CLIMB, climb=True)       # cardboard boxes, a staircase
    F.box('box2', (7.7, -1.4, 0.4), (8.1, -1.0, 0.8), 'climb', CLIMB, climb=True)
    F.box('box3', (7.8, -1.4, 0.8), (8.2, -1.05, 1.2), 'climb', CLIMB, climb=True)
    F.box('gShelf', (8.0, -1.4, 1.18), (8.42, -0.9, 1.2), 'top', TOP)
    F.box('rackRail', (3.6, -0.62, 1.38), (5.2, -0.58, 1.42), 'climb', CLIMB, climb=True)
    for lx in (3.6, 5.16):
        F.box('rackLeg', (lx, -0.62, 0.0), (lx + 0.04, -0.58, 1.38), 'furn', FURN)
    F.box('sheet', (4.2, -0.6, 0.0), (4.8, -0.58, 1.42), 'climb', CLIMB, climb=True)      # the sheet drying on the rack, down to the floor
    F.box('sack', (6.7, -1.2, 0.0), (7.2, -0.7, 0.35), 'furn', '#8a7650')
    F.zone('cold', (0.0, -1.4, 0.0), (8.42, -0.3, CEIL), 'лоджия: холодно', temp=5, rate=2)
    F.zone('wet', (3.0, -1.4, 0.0), (3.6, -1.0, 0.03), 'лужа под форточкой')
    F.box('gPuddle', (3.0, -1.4, 0.0), (3.6, -1.0, 0.005), 'decal', WATER, collide=False)
    F.zone('wind', (3.0, -1.4, 1.0), (3.6, -0.8, CEIL), 'форточка: сквозняк (пока только в данных)')

    # ---- hatches: three shortcuts in the walls, shut at the start of a raid (pull with [E])
    F.hatch('vent', 'Решётка вентиляции в ванной', (0.19, 5.12, 1.99), (0.41, 5.14, 2.15), 2,
            [(0.3, 5.2, 2.0), (0.3, 5.05, 2.0)], 'вентблок -> ванная (высоко, под потолком)')
    F.hatch('riser', 'Люк стояка в ванной', (0.82, 5.15, 0.0), (0.84, 5.33, 0.09), 2,
            [(0.9, 5.24, 0.0), (0.72, 5.24, 0.0)], 'стояк: ванная -> короб над коридором -> под мойку на кухне')
    F.hatch('skirt', 'Плинтус между гостиной и спальней', (5.98, 3.58, 0.0), (6.42, 3.6, 0.07), 1,
            [(6.2, 3.5, 0.0), (6.2, 3.8, 0.0)], 'щель под плинтусом: гостиная <-> спальня')

    # ---- the Human: comes in at the front door, walks his round (door to door, both loops)
    F.human = dict(spawn=[3.7, 6.2], patrol=[
        [3.7, 6.0], [3.7, 4.36], [2.5, 4.36], [2.5, 3.2], [1.2, 2.2], [2.4, 0.6], [2.4, -0.85], [4.2, -0.85], [6.4, -0.85],
        [6.4, 0.6], [4.4, 1.8], [7.0, 3.0], [3.9, 3.1], [3.9, 4.36], [5.6, 4.4], [5.6, 5.9], [3.9, 4.36], [1.2, 4.36], [1.2, 5.9], [1.2, 4.36]])

    # ---- the route graph (metres; z of the surface)
    N = F.node
    N('nest', (0.3, 2.82, 0), 'Гнездо (кухня)')
    N('nest2', (0.75, 4.3, 0), 'Гнездо (коридор)')
    N('nestIn', (0.3, 3.4, 0), 'Вентблок, низ', hole=True)
    N('shaftUp', (0.3, 4.4, 2.0), 'Вентблок, верх', hole=True)
    N('ventTop', (0.5, 3.83, 2.44), 'Выход к кабель-каналу', hole=True)
    N('grille', (0.3, 5.14, 2.0), 'Решётка в ванной', hole=True)
    # kitchen
    N('kMid', (1.5, 2.3, 0), 'Кухня, середина')
    N('kTowel', (1.74, 2.9, 0), 'У плиты')
    N('kWork', (1.74, 3.25, 0.85), 'Столешница')
    N('kHood', (2.05, 3.5, 1.5), 'Шнур вытяжки')
    N('kCabTop', (1.5, 3.45, 2.15), 'Верх шкафчиков')
    N('kSink', (1.13, 2.9, 0), 'Перед мойкой')
    N('kSinkIn', (1.1, 3.35, 0), 'Под мойкой')
    N('kChaseBot', (0.72, 3.52, 0.02), 'Короб, низ', hole=True)
    N('kChaseTop', (0.72, 3.52, 2.3), 'Короб, верх', hole=True)
    N('kCableF', (0.7, 2.22, 0), 'Шнур чайника')
    N('kWorkW', (0.3, 2.0, 0.85), 'Столешница у окна')
    N('kTableF', (1.9, 0.9, 0), 'Под клеёнкой')
    N('kTable', (1.9, 1.4, 0.75), 'Обеденный стол')
    N('kCurtF', (0.9, 0.35, 0), 'Штора на кухне')
    N('kSill', (1.3, 0.15, 0.88), 'Подоконник кухни')
    N('kRod', (1.3, 0.14, 2.43), 'Карниз кухни')
    N('kDoorC', (2.5, 3.66, 0), 'Дверь кухни')
    N('kBal', (2.4, 0.3, 0), 'Балконная дверь кухни')
    # loggia
    N('gK', (2.4, -0.7, 0), 'Лоджия у кухни')
    N('gMid', (4.0, -1.0, 0), 'Лоджия, середина')
    N('gSheet', (4.5, -0.7, 0), 'Простыня')
    N('gRail', (4.5, -0.6, 1.42), 'Сушилка')
    N('gL', (6.4, -0.7, 0), 'Лоджия у гостиной')
    N('gBoxes', (7.5, -1.1, 0), 'Коробки')
    N('gShelf', (8.1, -1.2, 1.2), 'Полка на лоджии')
    # corridor
    N('cW', (1.2, 4.36, 0), 'Коридор, запад')
    N('cMid', (2.5, 4.36, 0), 'Коридор, середина')
    N('cCable', (2.01, 3.85, 0), 'Кабель роутера')
    N('cE', (3.9, 4.36, 0), 'Коридор, восток')
    N('cabMid', (2.0, 3.74, 2.44), 'Кабель-канал, коридор')
    N('cabHole', (3.6, 3.66, 2.44), 'Кабель-канал, сквозь стену', hole=True)
    N('cabL', (4.6, 3.58, 2.44), 'Кабель-канал, гостиная')
    # living
    N('lDoor', (3.9, 3.2, 0), 'Дверь гостиной')
    N('lMid', (4.6, 2.0, 0), 'Гостиная, середина')
    N('lTvF', (3.7, 1.77, 0), 'Кабель ТВ')
    N('lTv', (3.4, 1.8, 0.5), 'Тумба ТВ')
    N('lClothF', (5.8, 1.05, 0), 'Под скатертью')
    N('cake', (5.8, 1.8, 0.75), 'Праздничный стол')
    N('lShelfTop', (5.8, 3.42, 2.0), 'Верх стеллажа')
    N('lCurtF', (4.6, 0.4, 0), 'Штора в гостиной')
    N('lSill', (5.1, 0.15, 0.88), 'Подоконник гостиной')
    N('lRod', (5.8, 0.14, 2.48), 'Карниз гостиной')
    N('lBal', (6.4, 0.3, 0), 'Балконная дверь гостиной')
    N('lSofaF', (7.3, 1.75, 0), 'У дивана')
    N('lSofa', (7.9, 1.6, 0.45), 'Диван')
    N('lUnderSofa', (7.95, 2.3, 0), 'Под диваном')
    N('lSkirt', (6.2, 3.45, 0), 'Плинтус (гостиная)')
    # bedroom
    N('bDoor', (4.8, 4.4, 0), 'Дверь спальни')
    N('bMid', (5.8, 5.0, 0), 'Спальня, середина')
    N('bSkirt', (6.2, 3.85, 0), 'Плинтус (спальня)')
    N('bBlocks', (5.75, 4.0, 0), 'Кубики')
    N('bToy', (5.3, 4.1, 0.4), 'Ящик с игрушками')
    N('bDuvet', (6.3, 5.0, 0), 'Одеяло')
    N('bBed', (7.4, 5.0, 0.55), 'Кровать')
    N('bHead', (8.36, 5.0, 1.1), 'Изголовье')
    N('bUnder', (7.3, 4.6, 0), 'Под кроватью')
    N('bBear', (6.9, 6.45, 0), 'Мишка')
    # bath, wc, hall
    N('tDoor', (1.2, 5.3, 0), 'Дверь ванной')
    N('tMid', (1.2, 5.9, 0), 'Ванная, середина')
    N('tTowel', (0.3, 5.3, 0), 'Полотенце')
    N('tRiser', (0.92, 5.24, 0), 'Люк стояка')
    N('riserIn', (0.72, 5.24, 0), 'Стояк, низ', hole=True)
    N('riserTop', (0.72, 5.24, 2.3), 'Стояк, верх', hole=True)
    N('duct', (0.72, 4.4, 2.3), 'Короб над коридором', hole=True)
    N('tHose', (0.7, 5.72, 0), 'Шланг стиралки')
    N('tWasher', (0.3, 5.8, 0.85), 'Стиральная машина')
    N('tMat', (0.8, 6.1, 0), 'Коврик')
    N('tRim', (1.0, 6.23, 0.55), 'Край ванны')
    N('wDoor', (2.36, 5.3, 0), 'Дверь туалета')
    N('wMid', (2.36, 5.9, 0), 'Туалет')
    N('wPaper', (2.05, 6.05, 0), 'Рулон')
    N('wShelf', (1.92, 6.05, 0.72), 'Полочка с бумагой')
    N('hMid', (3.7, 5.7, 0), 'Прихожая')
    N('hBoot', (3.36, 6.1, 0), 'Сапог')
    N('hRack', (3.5, 6.65, 0.45), 'Обувница')
    N('hCoat', (4.1, 5.6, 0), 'Пальто')
    N('hShelf', (4.34, 5.6, 1.78), 'Полка для шапок')

    E = F.edge
    # the nest
    E('nest', 'nestIn', 'cover'); E('nest2', 'nestIn', 'cover'); E('nest', 'kMid', 'floor'); E('nest2', 'cW', 'floor')
    E('nestIn', 'shaftUp', 'wall'); E('shaftUp', 'ventTop', 'wall'); E('shaftUp', 'grille', 'wall', gate='vent')
    E('ventTop', 'cabMid', 'high')
    # kitchen floor and heights
    E('kMid', 'kTowel', 'floor'); F.chain(['kTowel', 'kWork', 'kHood', 'kCabTop'], 'climb')
    E('kMid', 'kSink', 'floor'); E('kSink', 'kSinkIn', 'cover'); E('kSinkIn', 'kChaseBot', 'cover')
    E('kChaseBot', 'kChaseTop', 'wall'); E('kChaseTop', 'duct', 'wall'); E('duct', 'riserTop', 'wall'); E('riserTop', 'riserIn', 'wall')
    E('riserIn', 'tRiser', 'wall', gate='riser')
    E('nest', 'kCableF', 'floor'); E('kCableF', 'kWorkW', 'climb'); E('kMid', 'kCableF', 'floor')
    E('kMid', 'kTableF', 'floor'); E('kTableF', 'kTable', 'climb')
    E('kMid', 'kCurtF', 'floor'); E('kCurtF', 'kSill', 'climb'); E('kSill', 'kRod', 'climb')
    E('kMid', 'kDoorC', 'floor'); E('kDoorC', 'cMid', 'floor'); E('kMid', 'kBal', 'floor'); E('kBal', 'gK', 'loggia')
    # loggia
    F.chain(['gK', 'gMid', 'gL'], 'loggia'); E('gMid', 'gSheet', 'loggia'); E('gSheet', 'gRail', 'climb')
    E('gL', 'gBoxes', 'loggia'); E('gBoxes', 'gShelf', 'climb'); E('gL', 'lBal', 'loggia')
    # corridor
    F.chain(['cW', 'cMid', 'cE'], 'floor'); E('cMid', 'cCable', 'floor'); E('cCable', 'cabMid', 'climb')
    E('cabMid', 'cabHole', 'high'); E('cabHole', 'cabL', 'high'); E('cabL', 'lShelfTop', 'high')
    E('cE', 'lDoor', 'floor'); E('cE', 'bDoor', 'floor'); E('cW', 'tDoor', 'floor'); E('cMid', 'wDoor', 'floor'); E('cE', 'hMid', 'floor')
    # living
    E('lDoor', 'lMid', 'floor'); E('lMid', 'lTvF', 'floor'); E('lTvF', 'lTv', 'climb')
    E('lMid', 'lClothF', 'floor'); E('lClothF', 'cake', 'climb')
    E('lShelfTop', 'cake', 'high')                                    # garland 1
    E('lMid', 'lCurtF', 'floor'); E('lCurtF', 'lSill', 'climb'); E('lSill', 'lRod', 'climb'); E('lRod', 'cake', 'high')   # garland 2
    E('lMid', 'lBal', 'floor'); E('lMid', 'lSofaF', 'floor'); E('lSofaF', 'lSofa', 'climb'); E('lSofaF', 'lUnderSofa', 'cover')
    E('lMid', 'lSkirt', 'floor'); E('lSkirt', 'bSkirt', 'wall', gate='skirt')
    # bedroom
    E('bDoor', 'bMid', 'floor'); E('bMid', 'bSkirt', 'floor'); E('bDoor', 'bBlocks', 'floor'); E('bBlocks', 'bToy', 'climb')
    E('bMid', 'bDuvet', 'floor'); E('bDuvet', 'bBed', 'climb'); E('bBed', 'bHead', 'climb'); E('bMid', 'bUnder', 'cover'); E('bMid', 'bBear', 'floor')
    # bath
    E('tDoor', 'tMid', 'floor'); E('tDoor', 'tRiser', 'floor'); E('tDoor', 'tTowel', 'floor'); E('tTowel', 'grille', 'climb')
    E('tMid', 'tHose', 'floor'); E('tHose', 'tWasher', 'climb'); E('tMid', 'tMat', 'floor'); E('tMat', 'tRim', 'climb')
    # wc, hall
    E('wDoor', 'wMid', 'floor'); E('wMid', 'wPaper', 'floor'); E('wPaper', 'wShelf', 'climb')
    E('hMid', 'hBoot', 'floor'); E('hBoot', 'hRack', 'climb'); E('hMid', 'hCoat', 'floor'); E('hCoat', 'hShelf', 'climb')

    # ---- food (surface points; tiers 0..3 by the gradient rule: the dear things deep or high). The same count and the same
    #      heavy pieces in every variant, so that the comparison is fair: 38 spots + 3 heavy + the cake.
    A = F.add_food
    A((0.9, 2.45, 0), 0, 'kMid', what='крошки у гнезда'); A((1.6, 2.6, 0), 0, 'kMid', what='крошки')
    A((1.9, 1.4, 0), 1, 'kTableF', what='под столом'); A((1.1, 3.35, 0), 1, 'kSinkIn', what='под мойкой')
    A((1.2, 3.3, 0.85), 1, 'kWork', what='у мойки'); A((1.95, 3.2, 0.85), 2, 'kWork', what='у плиты')
    A((1.65, 1.3, 0.75), 2, 'kTable', what='на столе'); A((1.05, 0.13, 0.88), 2, 'kSill', what='на подоконнике')
    A((1.2, 3.45, 2.15), 3, 'kCabTop', what='на шкафчиках'); A((0.3, 1.75, 0.85), 1, 'kWorkW', what='у окна на столешнице')
    A((1.7, 4.45, 0), 0, 'cMid', what='в коридоре'); A((3.3, 4.6, 0), 1, 'cE', what='в коридоре у прихожей')
    A((3.7, 5.55, 0), 1, 'hMid', what='в прихожей'); A((3.5, 6.65, 0.45), 2, 'hRack', what='на обувнице'); A((4.34, 5.9, 1.78), 3, 'hShelf', what='на полке для шапок')
    A((1.3, 5.95, 0), 1, 'tMid', what='в ванной'); A((0.3, 5.8, 0.85), 2, 'tWasher', what='на стиралке'); A((1.2, 6.23, 0.55), 2, 'tRim', what='на краю ванны')
    A((2.4, 5.8, 0), 1, 'wMid', what='в туалете'); A((1.92, 6.05, 0.72), 2, 'wShelf', what='на полочке')
    A((3.6, 3.15, 0), 1, 'lDoor', what='у двери гостиной'); A((4.7, 2.6, 0), 1, 'lMid', what='на ковре')
    A((7.95, 2.3, 0), 2, 'lUnderSofa', what='под диваном'); A((3.35, 2.2, 0.5), 2, 'lTv', what='на тумбе')
    A((5.1, 0.13, 0.88), 2, 'lSill', what='на подоконнике'); A((7.9, 1.3, 0.45), 2, 'lSofa', what='на диване')
    A((6.0, 3.42, 2.0), 3, 'lShelfTop', what='на стеллаже'); A((5.3, 1.5, 0.75), 2, 'cake', what='на праздничном столе')
    A((6.3, 2.1, 0.75), 3, 'cake', what='на праздничном столе')
    A((4.85, 4.55, 0), 1, 'bDoor', what='в спальне у двери'); A((5.3, 4.15, 0.4), 2, 'bToy', what='на ящике с игрушками')
    A((7.4, 5.3, 0.55), 2, 'bBed', what='на кровати'); A((8.36, 4.6, 1.1), 3, 'bHead', what='на изголовье')
    A((7.2, 5.0, 0), 2, 'bUnder', what='под кроватью'); A((7.15, 6.45, 0.3), 2, 'bBear', what='на мишке')
    A((2.9, -0.8, 0), 1, 'gK', what='на лоджии'); A((4.5, -0.6, 1.42), 2, 'gRail', what='на сушилке'); A((8.1, -1.2, 1.2), 3, 'gShelf', what='банки на полке')
    A((6.95, -0.95, 0.35), 3, 'gL', heavy=True, what='мешок картошки (тяжёлое)')
    A((1.9, 1.5, 0.75), 3, 'kTable', heavy=True, what='батон на столе (тяжёлое)')
    A((7.6, 4.9, 0.55), 3, 'bBed', heavy=True, what='бутерброд ребёнка (тяжёлое)')
    F.cake = dict(at=[5.8, 1.8, 0.75], node='cake', where='на праздничном столе в гостиной')
    return F

VARIANTS = {'A': variant_A}

# ---------------------------------------------------------------------------------------------------------- export

def export(F):
    xs = [r['rect'][0] for r in F.rooms] + [r['rect'][2] for r in F.rooms]
    ys = [r['rect'][1] for r in F.rooms] + [r['rect'][3] for r in F.rooms]
    return dict(id=F.vid, name=F.name, note=F.area_note, version=1, uu_per_m=UU, sprint_mps=round(SPRINT, 4),
                origin_uu=[60000.0, 0.0, 0.0], bounds=[min(xs), min(ys), max(xs), max(ys)], ceiling=CEIL,
                rooms=F.rooms, boxes=F.boxes, zones=F.zones, hatches=F.hatches, cover=F.cover, nest=F.nest,
                food=F.food, cake=F.cake, human=F.human, nodes=list(F.nodes.values()), edges=F.edges)

# ---------------------------------------------------------------------------------------------------------- the check

def dijkstra(D, sources, open_gates, banned_rooms=(), kinds=None):
    adj = {n['id']: [] for n in D['nodes']}
    room = {n['id']: n['room'] for n in D['nodes']}
    for e in D['edges']:
        if e['gate'] and e['gate'] not in open_gates:
            continue
        if kinds is not None and e['kind'] not in kinds:
            continue
        if room[e['a']] in banned_rooms or room[e['b']] in banned_rooms:
            continue
        adj[e['a']].append((e['b'], e['run'], e))
        adj[e['b']].append((e['a'], e['run'], e))
    dist, prev = {}, {}
    pq = [(0.0, s) for s in sources]
    for s in sources:
        dist[s] = 0.0
    while pq:
        d, u = heapq.heappop(pq)
        if d > dist.get(u, 1e18):
            continue
        for v, w, e in adj[u]:
            if d + w < dist.get(v, 1e18):
                dist[v], prev[v] = d + w, (u, e)
                heapq.heappush(pq, (d + w, v))
    return dist, prev

def edge_disjoint_paths(D, sources, target, open_gates):
    """Max number of edge-disjoint routes nest -> cake (unit capacities, undirected): how many squads can go at once
    without sharing a single stretch."""
    cap = {}
    def add(a, b):
        cap[(a, b)] = cap.get((a, b), 0) + 1
        cap[(b, a)] = cap.get((b, a), 0) + 1
    for e in D['edges']:
        if not e['gate'] or e['gate'] in open_gates:
            add(e['a'], e['b'])
    S = '__src'
    for s in sources:
        cap[(S, s)] = 99
        cap.setdefault((s, S), 0)
    nbr = {}
    for (a, b) in cap:
        nbr.setdefault(a, set()).add(b)
    flow = 0
    while True:
        par = {S: None}
        q = [S]
        while q and target not in par:
            u = q.pop(0)
            for v in nbr.get(u, ()):
                if v not in par and cap.get((u, v), 0) > 0:
                    par[v] = u
                    q.append(v)
        if target not in par:
            return flow
        v = target
        while par[v] is not None:
            u = par[v]
            cap[(u, v)] -= 1
            cap[(v, u)] = cap.get((v, u), 0) + 1
            v = u
        flow += 1

def path_of(prev, t):
    out = []
    while t in prev:
        u, e = prev[t]
        out.append(e)
        t = u
    return list(reversed(out))

def food_time(D, dist, f):
    n = next(x for x in D['nodes'] if x['id'] == f['node'])
    if f['node'] not in dist:
        return None
    A, B = n['at'], f['at']
    return dist[f['node']] + (math.hypot(A[0] - B[0], A[1] - B[1]) + abs(A[2] - B[2]) * CLIMB_MUL) / SPRINT

def check(D):
    allg = {h['id'] for h in D['hatches']}
    src = ['nest', 'nest2']
    lines, res = [], dict(variant=D['id'], rooms={}, problems=[])
    lines.append('KAN-37 level lab check: variant %s «%s»' % (D['id'], D['name']))
    lines.append('rooms %d, boxes %d (climbable %d), zones %d, hatches %d, food %d (+cake), graph %d nodes / %d edges' % (
        len(D['rooms']), len(D['boxes']), sum(b['climb'] for b in D['boxes']), len(D['zones']), len(D['hatches']), len(D['food']), len(D['nodes']), len(D['edges'])))
    for label, gates in (('hatches shut', set()), ('hatches open', allg)):
        dist, prev = dijkstra(D, src, gates)
        lines.append('')
        lines.append('-- nest -> first food per room (%s), limit %.0f s' % (label, FIRST_FOOD_LIMIT))
        for r in D['rooms']:
            ts = [(food_time(D, dist, f), f) for f in D['food'] if f['room'] == r['id']]
            ts = [(t, f) for t, f in ts if t is not None]
            if not ts:
                lines.append('  %-9s no reachable food' % r['id'])
                res['problems'].append('%s: no reachable food (%s)' % (r['id'], label))
                continue
            t, f = min(ts, key=lambda x: x[0])
            ok = t <= FIRST_FOOD_LIMIT
            lines.append('  %-9s %5.1f s  %s  (%s, tier %d)' % (r['id'], t, 'ok' if ok else 'TOO FAR', f['what'], f['tier']))
            res['rooms'].setdefault(r['id'], {})[label] = round(t, 1)
            if not ok and label == 'hatches shut':
                res['problems'].append('%s: first food %.1f s' % (r['id'], t))
        bad = [f for f in D['food'] if food_time(D, dist, f) is None]
        lines.append('  unreachable food: %d %s' % (len(bad), ', '.join('%s (%s)' % (f['what'], f['room']) for f in bad)))
        res['unreachable_' + label.replace(' ', '_')] = [f['what'] for f in bad]
        if bad and label == 'hatches open':
            res['problems'].append('unreachable food with every hatch open: %d' % len(bad))
        # routes to the cake
        n_disj = edge_disjoint_paths(D, src, D['cake']['node'], gates)
        best = dist.get(D['cake']['node'])
        lines.append('-- the cake (%s): fastest %.1f s, edge-disjoint routes %d' % (D['cake']['where'], best if best is not None else -1, n_disj))
        res['cake_routes_' + label.replace(' ', '_')] = n_disj
        # floor: every edge floor/cover/loggia except the final climb onto the table
        cake = D['cake']['node']
        floor_ok = any(dijkstra(D, src, gates, kinds={'floor', 'cover', 'loggia'})[0].get(n) is not None for n in ('lClothF',))
        high_ok = False; wall_ok = False
        d2, p2 = dijkstra(D, src, gates, kinds={'floor', 'cover', 'loggia', 'climb', 'high', 'wall'})
        # high: a route that arrives at the cake by a 'high' edge (garland)
        for e in D['edges']:
            if e['kind'] == 'high' and cake in (e['a'], e['b']):
                other = e['b'] if e['a'] == cake else e['a']
                if other in d2:
                    high_ok = True
        # walls: a route that uses a wall edge on the way (the skirting gap from the bedroom, or the riser...)
        dw, pw = dijkstra(D, src, gates, banned_rooms=('corridor',))
        lines.append('   floor route: %s; high route: %s' % ('yes' if floor_ok else 'NO', 'yes' if high_ok else 'NO'))
        # every distinct way in: which first edge into the living room the fastest path uses, banning the others in turn
        ways = []
        entries = [('дверь из коридора', ('cE', 'lDoor')), ('лоджия', ('gL', 'lBal')), ('кабель-канал', ('cabHole', 'cabL')), ('плинтус из спальни', ('lSkirt', 'bSkirt'))]
        for name, (a, b) in entries:
            DD = dict(D); DD['edges'] = [e for e in D['edges'] if not any(set((e['a'], e['b'])) == set(pr) for nm, pr in entries if nm != name)]
            dd, _ = dijkstra(DD, src, gates)
            t = dd.get(cake)
            ways.append((name, t))
            lines.append('   via %-20s %s' % (name, ('%.1f s' % t) if t is not None else 'no way'))
        res['cake_ways_' + label.replace(' ', '_')] = {n: (round(t, 1) if t is not None else None) for n, t in ways}
        n_ways = sum(1 for n, t in ways if t is not None)
        if n_ways < 3:
            res['problems'].append('fewer than 3 ways to the cake (%s): %d' % (label, n_ways))
        # the Human stands in the corridor
        lines.append('-- the Human blocks the corridor (%s):' % label)
        reach = []
        for r in D['rooms']:
            if r['id'] == 'corridor':
                continue
            ok = any(food_time(D, dw, f) is not None for f in D['food'] if f['room'] == r['id'])
            reach.append((r['id'], ok))
        lines.append('   rooms still reachable: %s' % ', '.join('%s %s' % (r, 'yes' if ok else 'NO') for r, ok in reach))
        lines.append('   the cake: %s' % (('%.1f s' % dw[cake]) if cake in dw else 'NO WAY'))
        res['corridor_blocked_' + label.replace(' ', '_')] = dict(rooms={r: ok for r, ok in reach}, cake=round(dw[cake], 1) if cake in dw else None)
    lines.append('')
    lines.append('problems: %d %s' % (len(res['problems']), '; '.join(res['problems'])))
    return '\n'.join(lines), res

# ---------------------------------------------------------------------------------------------------------- the plan image

def hexc(s, a=255):
    s = s.lstrip('#')
    return (int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16), a)

def render(D, path, check_text):
    from PIL import Image, ImageDraw, ImageFont
    S = 150                                   # px per metre
    x0, y0, x1, y1 = D['bounds']
    pad = 0.6
    W = int((x1 - x0 + 2 * pad) * S) + 560
    H = int((y1 - y0 + 2 * pad) * S) + 120
    img = Image.new('RGBA', (W, H), (250, 249, 246, 255))
    dr = ImageDraw.Draw(img, 'RGBA')
    def font(sz):
        for f in ('C:/Windows/Fonts/segoeui.ttf', 'C:/Windows/Fonts/arial.ttf', 'DejaVuSans.ttf'):
            try:
                return ImageFont.truetype(f, sz)
            except Exception:
                pass
        return ImageFont.load_default()
    f10, f12, f14, f18, f24 = font(13), font(15), font(17), font(22), font(30)
    def P(x, y):
        return ((x - x0 + pad) * S, (y1 + pad - y) * S + 60)
    def rect(mn, mx, fill, outline=None, w=1):
        a, b = P(mn[0], mx[1]), P(mx[0], mn[1])
        dr.rectangle([a, b], fill=fill, outline=outline, width=w)
    dr.text((20, 14), 'Вариант %s «%s» — %s' % (D['id'], D['name'], D['note']), fill=(30, 30, 30), font=f18)
    # rooms
    for r in D['rooms']:
        rx0, ry0, rx1, ry1 = r['rect']
        col = next((b['col'] for b in D['boxes'] if b['id'].startswith('floor_' + r['id'] + '#')), '#dddddd')
        rect((rx0, ry0), (rx1, ry1), hexc(col, 255))
    # boxes, low to high (what is higher is drawn later); walls dark
    def zmax(b):
        return b['c'][2] + b['s'][2] / 2
    order = sorted([b for b in D['boxes'] if b['kind'] not in ('decal', 'floor', 'ceiling')], key=lambda b: (b['kind'] == 'climb', zmax(b)))
    for b in order:
        c, s = b['c'], b['s']
        if 'r' in b:
            yaw = math.radians(b['r'][1]); pitch = math.radians(b['r'][0])
            L = s[0] * math.cos(pitch) / 2
            ax, ay = c[0] - math.cos(yaw) * L, c[1] - math.sin(yaw) * L
            bx, by = c[0] + math.cos(yaw) * L, c[1] + math.sin(yaw) * L
            dr.line([P(ax, ay), P(bx, by)], fill=hexc(b['col']), width=4)
            continue
        mn = (c[0] - s[0] / 2, c[1] - s[1] / 2); mx = (c[0] + s[0] / 2, c[1] + s[1] / 2)
        if b['kind'] == 'wall':
            rect(mn, mx, (70, 68, 64, 255))
        elif b['kind'] in ('shell',):
            rect(mn, mx, (90, 100, 110, 200))
        elif b['kind'] == 'shell_in':
            rect(mn, mx, (60, 55, 50, 160))
        elif b['kind'] == 'glass':
            rect(mn, mx, (120, 190, 230, 255))
        elif b['kind'] == 'climb':
            rect(mn, mx, hexc(b['col'], 235), outline=(150, 80, 10, 255))
        else:
            shade = int(max(60, 200 - zmax(b) * 55))
            rect(mn, mx, (shade, shade + 4, shade + 10, 220), outline=(50, 50, 55, 255))
    for b in D['boxes']:
        if b['kind'] == 'decal' and not b['id'].startswith('floor_'):
            c, s = b['c'], b['s']
            rect((c[0] - s[0] / 2, c[1] - s[1] / 2), (c[0] + s[0] / 2, c[1] + s[1] / 2), hexc(b['col'], 200))
    # zones
    zc = {'hot': (220, 60, 40), 'cold': (60, 140, 230), 'wet': (40, 110, 220), 'steam': (200, 120, 200), 'wind': (120, 200, 200), 'soft': (230, 160, 200), 'sticky': (180, 120, 30)}
    for z in D['zones']:
        c = zc.get(z['kind'], (128, 128, 128))
        a, bb = P(z['min'][0], z['max'][1]), P(z['max'][0], z['min'][1])
        dr.rectangle([a, bb], outline=c + (255,), width=2)
        if z['kind'] != 'cold' or (z['max'][0] - z['min'][0]) < 2:
            dr.text((a[0] + 3, a[1] + 2), z['kind'], fill=c + (255,), font=f10)
    # hatches
    for h in D['hatches']:
        mn = (h['c'][0] - h['s'][0] / 2 - 0.03, h['c'][1] - h['s'][1] / 2 - 0.03); mx = (h['c'][0] + h['s'][0] / 2 + 0.03, h['c'][1] + h['s'][1] / 2 + 0.03)
        rect(mn, mx, (220, 30, 30, 255))
        p = P(h['c'][0], h['c'][1])
        dr.text((p[0] + 8, p[1] - 22), 'люк «%s» (%d)' % (h['id'], h['cls']), fill=(200, 20, 20), font=f12)
    # the graph
    nodes = {n['id']: n for n in D['nodes']}
    ec = {'floor': (90, 90, 90, 150), 'cover': (60, 140, 60, 200), 'loggia': (60, 120, 160, 200), 'climb': (230, 130, 20, 230), 'high': (210, 40, 160, 230), 'wall': (40, 40, 200, 230)}
    for e in D['edges']:
        a, b = nodes[e['a']]['at'], nodes[e['b']]['at']
        dr.line([P(a[0], a[1]), P(b[0], b[1])], fill=ec[e['kind']], width=3 if e['kind'] in ('wall', 'high') else 2)
    for n in D['nodes']:
        p = P(n['at'][0], n['at'][1])
        r = 4
        dr.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=(30, 30, 30, 255) if not n['hole'] else (40, 40, 200, 255))
    # food, cake, nest, the Human
    tc = [(250, 230, 120), (240, 180, 60), (230, 110, 40), (200, 40, 40)]
    for f in D['food']:
        p = P(f['at'][0], f['at'][1])
        r = 9 if f['heavy'] else 6
        dr.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=tc[f['tier']] + (255,), outline=(60, 30, 0, 255), width=2 if f['heavy'] else 1)
    p = P(D['cake']['at'][0], D['cake']['at'][1])
    dr.rectangle([p[0] - 16, p[1] - 11, p[0] + 16, p[1] + 11], fill=(255, 150, 200, 255), outline=(150, 30, 90, 255), width=3)
    dr.text((p[0] - 18, p[1] + 13), 'ТОРТ', fill=(150, 30, 90), font=f14)
    for k, lab in (('main', 'ГНЕЗДО'), ('second', 'гнездо-2')):
        q = D['nest'][k]
        p = P(q[0], q[1])
        rr = D['nest']['radius'] * S
        dr.ellipse([p[0] - rr, p[1] - rr, p[0] + rr, p[1] + rr], outline=(107, 74, 43, 255), width=3)
        dr.text((p[0] + rr + 2, p[1] - 9), lab, fill=(107, 74, 43), font=f14)
    pts = [P(x, y) for x, y in D['human']['patrol']]
    dr.line(pts + [pts[0]], fill=(200, 30, 30, 110), width=2)
    for q in pts:
        dr.rectangle([q[0] - 3, q[1] - 3, q[0] + 3, q[1] + 3], fill=(200, 30, 30, 200))
    p = P(*D['human']['spawn'])
    dr.text((p[0] - 30, p[1] + 6), 'Человек', fill=(200, 30, 30), font=f14)
    # room labels
    for r in D['rooms']:
        rx0, ry0, rx1, ry1 = r['rect']
        p = P(rx0, ry1)
        dr.text((p[0] + 6, p[1] + 4), '%s %.1f м²' % (r['name'], r['area']), fill=(20, 20, 20), font=f18)
    # scale bar
    a = P(x0, y0 - 0.45); b = P(x0 + 1.0, y0 - 0.45)
    dr.line([a, b], fill=(0, 0, 0), width=4)
    dr.text((a[0], a[1] + 4), '1 м = 1900 uu (муравей 8 мм)', fill=(0, 0, 0), font=f12)
    # legend and the check
    LX = int((x1 - x0 + 2 * pad) * S) + 20
    y = 70
    leg = [('стена (не лезут)', (70, 68, 64)), ('мебель (не лезут), темнее = выше', (120, 124, 130)), ('по чему лезут: ткань, провода, кубики', (232, 144, 42)),
           ('вентблок / стояк / короб', (90, 100, 110)), ('люк в стене (закрыт в начале)', (220, 30, 30)), ('стекло', (120, 190, 230)),
           ('граф: пол', (90, 90, 90)), ('граф: укрытие', (60, 140, 60)), ('граф: лоджия', (60, 120, 160)), ('граф: подъём', (230, 130, 20)),
           ('граф: верх (кабели, гирлянды)', (210, 40, 160)), ('граф: внутри стен', (40, 40, 200)), ('маршрут Человека', (200, 30, 30)),
           ('еда: ярус 0..3 (круг побольше — тяжёлая)', (230, 110, 40))]
    for t, c in leg:
        dr.rectangle([LX, y, LX + 22, y + 14], fill=c + (255,))
        dr.text((LX + 30, y - 3), t, fill=(20, 20, 20), font=f12)
        y += 22
    y += 10
    for ln in check_text.split('\n'):
        dr.text((LX, y), ln[:78], fill=(20, 20, 20), font=f10)
        y += 16
        if y > H - 20:
            break
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.convert('RGB').save(path)

# ---------------------------------------------------------------------------------------------------------- the .inl

def write_inl():
    lab = os.path.join(ROOT, 'docs', 'level', 'lab')
    files = sorted(f for f in os.listdir(lab) if f.startswith('lab_') and f.endswith('.json'))
    out = ['// Generated by Tools/levellab/build_lab.py from docs/level/lab/lab_*.json. Do not edit by hand.',
           '// Each variant: its letter and its JSON (UTF-8 bytes); AntLab.cpp parses it at run time.', '']
    entries = []
    for f in files:
        vid = f[4:-5]
        data = open(os.path.join(lab, f), 'rb').read()
        name = 'GAntLabJson_%s' % vid
        out.append('static const uint8 %s[] = {' % name)
        for i in range(0, len(data), 40):
            out.append('\t' + ','.join(str(b) for b in data[i:i + 40]) + ',')
        out.append('};')
        entries.append('\t{ TEXT("%s"), %s, sizeof(%s) },' % (vid, name, name))
    out.append('struct FAntLabEmbedded { const TCHAR* Id; const uint8* Data; int32 Size; };')
    out.append('static const FAntLabEmbedded GAntLabEmbedded[] = {')
    out += entries
    out.append('};')
    path = os.path.join(ROOT, 'Source', 'AntKitchen', 'AntLabData.inl')
    with open(path, 'w', encoding='utf-8-sig', newline='\r\n') as fh:
        fh.write('\n'.join(out) + '\n')
    return path, files

def main():
    want = [a.upper() for a in sys.argv[1:]] or sorted(VARIANTS)
    for vid in want:
        F = VARIANTS[vid]()
        D = export(F)
        jpath = os.path.join(ROOT, 'docs', 'level', 'lab', 'lab_%s.json' % vid)
        with open(jpath, 'w', encoding='utf-8', newline='\n') as fh:
            json.dump(D, fh, ensure_ascii=False, separators=(',', ':'))
        text, res = check(D)
        sdir = os.path.join(ROOT, 'Saved', 'levellab')
        os.makedirs(sdir, exist_ok=True)
        open(os.path.join(sdir, 'check_%s.txt' % vid), 'w', encoding='utf-8').write(text + '\n')
        json.dump(res, open(os.path.join(sdir, 'check_%s.json' % vid), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
        render(D, os.path.join(sdir, 'plan_%s.png' % vid), text)
        print(text)
        print('-> %s (%d bytes), plan_%s.png' % (jpath, os.path.getsize(jpath), vid))
    path, files = write_inl()
    print('-> %s: %s' % (path, ', '.join(files)))

if __name__ == '__main__':
    main()
