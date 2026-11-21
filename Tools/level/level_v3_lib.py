# -*- coding: utf-8 -*-
# Ant Kitchen, level v3: geometry and checks shared by build_level_v3.py, check_level_v3.py and build_level_map_v3.py.
#   apply(B, comp)  -> one level state (objects in the v2 schema, hiding places, connectors, graph, hazards, movers, manifest instances)
#   checks(B, st)   -> the seed rules of level_v3_layout.VALIDATE (Human nav, cover <= 4 s, cake routes, floats, overlaps, door sweeps, scatter)
# Pure python + numpy (no scipy). Reads docs/level/level_v2.json, SourceArt/Blender/level_v2/level_v2_manifest.json and, when it exists,
# SourceArt/Blender/level_v3/level_v3_new_pieces.json (else the boxes of level_v3_layout.PIECES).
import json, os, sys, math, copy, heapq
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE))
LV = os.path.join(ROOT, 'docs', 'level'); ART = os.path.join(ROOT, 'SourceArt', 'Blender')
sys.path.insert(0, HERE)
import level_v3_layout as L
K = 1.0 / 1900.0
N3 = np.diag([-1.0, 1.0, 1.0]); M3 = np.diag([1.0, -1.0, 1.0])

def load_base():
    D2 = json.load(open(os.path.join(LV, 'level_v2.json'), encoding='utf-8'))
    M2 = json.load(open(os.path.join(ART, 'level_v2', 'level_v2_manifest.json'), encoding='utf-8'))
    npj = os.path.join(ART, 'level_v3', 'level_v3_new_pieces.json')
    NP = json.load(open(npj, encoding='utf-8')) if os.path.exists(npj) else None
    B = {'D2': D2, 'M2': M2, 'NP': NP}
    B['piece_v2'] = {p['name']: p for p in M2['pieces']}
    B['piece_v3'] = {p['name']: p for p in NP['pieces']} if NP else {}
    # every placed instance of v2, tagged with its object id
    obj_by_mesh = {}
    for o in D2['objects']:
        if o.get('mesh'): obj_by_mesh.setdefault(o['mesh'], []).append(o)
    insts = []
    for p in M2['pieces']:
        for i in p['instances']:
            oid = i.get('object')
            if not oid:
                bmin = i['game_bbox_min_uu']
                best = None
                for o in obj_by_mesh.get(p['name'], []):
                    d = abs(o['bb'][0] - bmin[0]) + abs(o['bb'][1] - bmin[1]) + abs(o['z'][0] - bmin[2])
                    if d < 6 and (best is None or d < best[0]): best = (d, o['id'])
                oid = best[1] if best else None
            insts.append({'piece': p['name'], 'file': _file_v2(p['file']), 'inst': i, 'object': oid})
    B['insts'] = insts
    # hiding places linked to their object by box
    hides = []
    for h in D2['human']['sees']['never_targets']:
        if 'rect' not in h: continue
        hh = dict(h); hh.setdefault('id', 'h_' + h['ru'][:12]); hh['object'] = None
        for o in D2['objects']:
            if o.get('mesh') and abs(o['bb'][0] - h['rect'][0]) < 5 and abs(o['bb'][1] - h['rect'][1]) < 5 and abs(o['bb'][2] - h['rect'][2]) < 5:
                hh['object'] = o['id']; break
        hides.append(hh)
    B['hides'] = hides
    B['obj'] = {o['id']: o for o in D2['objects']}
    return B

def _file_v2(f):
    f = f.replace('\\', '/')
    return f if f.startswith('../') else '../level_v2/' + f

# ---- rotations ---------------------------------------------------------------------------------------------------------------------------
def Rz(deg):
    a = math.radians(deg); c, s = math.cos(a), math.sin(a)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1.0]])
def Rx(deg):
    a = math.radians(deg); c, s = math.cos(a), math.sin(a)
    return np.array([[1.0, 0, 0], [0, c, -s], [0, s, c]])
def Ry(deg):
    a = math.radians(deg); c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0, s], [0, 1.0, 0], [-s, 0, c]])
def g2s(p): return np.array([-p[0] * K, p[1] * K, p[2] * K])
def s2g(v): return [-v[0] / K, v[1] / K, v[2] / K]
def align_z(d):
    d = d / (np.linalg.norm(d) or 1.0); z = np.array([0, 0, 1.0]); v = np.cross(z, d); c = float(np.dot(z, d))
    if np.linalg.norm(v) < 1e-9: return np.eye(3) if c > 0 else np.diag([1.0, -1.0, -1.0])
    vx = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + vx + vx @ vx * (1.0 / (1.0 + c))

def local_box(B, name):
    """Local box of a mesh in metres (Blender piece frame, relative to the mesh origin)."""
    if name in B['piece_v3']:
        p = B['piece_v3'][name]; return np.array(p['bbox_min_m']), np.array(p['bbox_max_m'])
    if name in L.PIECES:
        b = L.PIECES[name]['box']; return np.array([b[0], b[2], b[4]]) * K, np.array([b[1], b[3], b[5]]) * K
    p = B['piece_v2'][name]; return np.array(p['bbox_min_m']), np.array(p['bbox_max_m'])

def make_inst(B, name, t, R, s):
    mn, mx = local_box(B, name)
    cs = []
    for i in range(8):
        c = np.array([mx[0] if i & 1 else mn[0], mx[1] if i & 2 else mn[1], mx[2] if i & 4 else mn[2]]) * np.array(s)
        cs.append(s2g(t + R @ c))
    cs = np.array(cs)
    Rg = N3 @ R @ M3
    d = {'blender_loc_m': [round(float(v), 5) for v in t], 'blender_rot_3x3': [[round(float(v), 6) for v in row] for row in R], 'scale': [float(v) for v in s],
         'game_loc_uu': [round(v, 1) for v in s2g(t)], 'game_axis_x': [round(float(v), 5) for v in Rg[:, 0]], 'game_axis_y': [round(float(v), 5) for v in Rg[:, 1]],
         'game_axis_z': [round(float(v), 5) for v in Rg[:, 2]], 'game_bbox_min_uu': [round(float(v), 1) for v in cs.min(0)], 'game_bbox_max_uu': [round(float(v), 1) for v in cs.max(0)]}
    if abs(Rg[2][2] - 1) < 1e-6:
        d['game_yaw_deg'] = round(math.degrees(math.atan2(Rg[1][0], Rg[0][0])), 3)
    return d

def place(B, a):
    """An ADD entry -> (manifest instance, object in the v2 schema)."""
    name = a['piece']; s = a.get('scale') or [1.0, 1.0, 1.0]
    t = g2s(a['p'])
    R = Rz(180.0 - (a.get('yaw') or 0.0))
    if a.get('tilt'): R = R @ Rx(a['tilt'][0]) @ Ry(a['tilt'][1])
    if a.get('aim'):
        dg = np.array(a['aim'], float) - np.array(a['p'], float)
        R = align_z(np.array([-dg[0], dg[1], dg[2]]))
        ln = L.PIECES[name]['box'][5] if name in L.PIECES else 4600.0
        s = [1.0, 1.0, float(np.linalg.norm(dg)) / ln]
    inst = make_inst(B, name, t, R, s)
    pc = L.PIECES.get(name, {})
    cat = a.get('cat') or pc.get('cat') or ('furn' if a.get('copy') else 'prop')
    mn, mx = inst['game_bbox_min_uu'], inst['game_bbox_max_uu']
    o = {'id': a['id'], 'mesh': name, 'index': 0, 'ru': a.get('ru') or pc.get('ru', name), 'cat': cat, 'role': 'static', 'solid': a.get('solid', True), 'climbable': True,
         'origin': list(a['p']), 'bb': [mn[0], mn[1], mx[0], mx[1]], 'z': [mn[2], mx[2]], 'c': [(mn[0] + mx[0]) / 2, (mn[1] + mx[1]) / 2], 's': [mx[0] - mn[0], mx[1] - mn[1]],
         'yaw': a.get('yaw'), 'area': a.get('area'), 'v3': 'new', 'mount': a.get('mount'), 'district': a.get('district'), 'mat': pc.get('mat'), 'src': 'level_v3_layout'}
    for k in ('part_of', 'replaces', 'tops', 'cloth', 'tint', 'hazard', 'state'):
        if a.get(k) is not None: o[k] = a[k]
    if pc.get('state'): o['state'] = pc['state']
    if cat in ('fastener',) or name in ('SM_V3_BalloonString', 'SM_V3_CurtainRing', 'SM_V3_TapeStrip', 'SM_V3_KidDrawing', 'SM_V3_Cornice', 'SM_V3_Outlet', 'SM_V3_PictureFrame', 'SM_V3_Nail', 'SM_V3_Balloon', 'SM_V3_PendantLamp', 'SM_V3_LaundryLine'):
        o['collision'] = 'ants_only'
    if pc.get('cloth') or a.get('cloth'): o['cloth'] = True
    if name == 'SM_V3_ToyBox_Lid':
        inst['role'] = 1
        inst['mover'] = {'type': 'hinge', 'flags': 0, 'auto_close_s': 0.0, 'open_text': 'Открыть ящик с игрушками', 'close_text': 'Закрыть ящик', 'door_class': 'medium'}
        Rg = N3 @ R @ M3; ax = Rg @ (M3 @ np.array([1.0, 0, 0]))
        inst['hinge_axis_game'] = [round(float(v), 5) for v in ax]; inst['open_angle_game_deg'] = -100.0
        o['role'] = 'mover'; o['cat'] = 'part'
    inst['object'] = a['id']; inst['role'] = inst.get('role', 0); inst['index'] = 0
    if a.get('tint'): inst['tint'] = a['tint']
    return inst, o

def _upd_obj(o, inst):
    mn, mx = inst['game_bbox_min_uu'], inst['game_bbox_max_uu']
    o['bb'] = [mn[0], mn[1], mx[0], mx[1]]; o['z'] = [mn[2], mx[2]]; o['c'] = [(mn[0] + mx[0]) / 2, (mn[1] + mx[1]) / 2]; o['s'] = [mx[0] - mn[0], mx[1] - mn[1]]
    o['origin'] = inst['game_loc_uu']
    if 'game_yaw_deg' in inst: o['yaw'] = inst['game_yaw_deg']

def mover_key(o):
    if o.get('mover') and o['mover'].get('part') is not None: return 'mover%d' % o['mover']['part']
    g = {'art43': 'door_fridge', 'art53': 'door_oven', 'art67': 'door_dishwasher', 'art71': 'door_microwave', 'art4': 'door_drawer'}
    return g.get(o['id'])
CODE_DOOR_OBJ = {'door_fridge': 'art43', 'door_oven': 'art53', 'door_dishwasher': 'art67', 'door_microwave': 'art71', 'door_drawer': 'art4', 'cat_flap': 'new96', 'breadbox_lid': 'new19'}

# ---- apply BASE + variants of a composition ----------------------------------------------------------------------------------------------
def apply(B, comp=None, base_only=False, chaos=False):
    D2 = B['D2']
    objs = {o['id']: copy.deepcopy(o) for o in D2['objects']}
    order = [o['id'] for o in D2['objects']]
    insts = [dict(x, inst=copy.deepcopy(x['inst'])) for x in B['insts']]
    hides = [dict(h) for h in B['hides']]
    conn_off, edge_ops, hazards_add, hazards_off, movers, notes, cake_w = set(), [], [], set(), {}, [], {}
    pets = {'dog': [dict(s) for s in D2['pets']['dog']['spots']], 'cat': [dict(s) for s in D2['pets']['cat']['spots']]}
    def inst_of(oid): return [x for x in insts if x['object'] == oid]
    def remove(oid, why=None):
        if oid not in objs: return
        objs.pop(oid)
        for x in inst_of(oid): insts.remove(x)
        for h in [h for h in hides if h.get('object') == oid]: hides.remove(h)
    def add(a):
        inst, o = place(B, a)
        if a['id'] in objs: remove(a['id'])
        objs[a['id']] = o; order.append(a['id'])
        pf = 'SM_V3_' in a['piece']
        insts.append({'piece': a['piece'], 'file': (a['piece'] + '.fbx') if pf else _file_v2(B['piece_v2'][a['piece']]['file']), 'inst': inst, 'object': a['id']})
        if a.get('hide'):
            h = dict(a['hide']); h.setdefault('id', 'hide_' + a['id']); h['object'] = a['id']; h['src'] = 'level_v3'; hides.append(h)
        if a.get('hazard') == 'radiator':
            hazards_add.append({'id': 'heat_' + a['id'], 'ru': 'Батарея (горячая)', 'kind': 'heat', 'rect': o['bb'], 'z': o['z']})
    def move(oid, cxy, yaw=None, z0=None):
        o = objs.get(oid)
        if not o: return
        for x in inst_of(oid):
            i = x['inst']; t = np.array(i['blender_loc_m']); R = np.array(i['blender_rot_3x3']); s = i.get('scale') or [1, 1, 1]
            if yaw is not None:
                old = i.get('game_yaw_deg', o.get('yaw') or 0.0)
                R = Rz(-(yaw - old)) @ R
            tmp = make_inst(B, x['piece'], t, R, s)
            c = ((tmp['game_bbox_min_uu'][0] + tmp['game_bbox_max_uu'][0]) / 2, (tmp['game_bbox_min_uu'][1] + tmp['game_bbox_max_uu'][1]) / 2)
            dz = 0.0 if z0 is None else z0 - tmp['game_bbox_min_uu'][2]
            g = s2g(t); g = [g[0] + cxy[0] - c[0], g[1] + cxy[1] - c[1], g[2] + dz]
            ni = make_inst(B, x['piece'], g2s(g), R, s)
            for k in ('object', 'role', 'index', 'mover', 'hinge_axis_game', 'open_angle_game_deg', 'slide_game_uu'):
                if k in i: ni[k] = i[k]
            x['inst'] = ni
            old_bb = list(o['bb']); _upd_obj(o, ni)
            for h in hides:
                if h.get('object') == oid:
                    h['rect'] = [o['bb'][0], o['bb'][1], o['bb'][2], o['bb'][3]]
        o['v3'] = 'moved'
    def rotate_world(oid, axis, deg, pivot):
        if not deg: return
        ax = {'x': np.array([-1.0, 0, 0]), 'y': np.array([0, 1.0, 0]), 'z': np.array([0, 0, 1.0])}[axis]
        c, sn = math.cos(math.radians(deg)), math.sin(math.radians(deg))
        vx = np.array([[0, -ax[2], ax[1]], [ax[2], 0, -ax[0]], [-ax[1], ax[0], 0]])
        Rw = np.eye(3) * c + sn * vx + (1 - c) * np.outer(ax, ax)
        P = g2s(pivot)
        for x in inst_of(oid):
            i = x['inst']; t = np.array(i['blender_loc_m']); R = np.array(i['blender_rot_3x3'])
            ni = make_inst(B, x['piece'], P + Rw @ (t - P), Rw @ R, i.get('scale') or [1, 1, 1])
            for kk in ('object', 'role', 'index', 'tint'):
                if kk in i: ni[kk] = i[kk]
            x['inst'] = ni
            if oid in objs: _upd_obj(objs[oid], ni); objs[oid]['v3'] = 'moved'; objs[oid]['tilted'] = True
    def scale(oid, k):
        for x in inst_of(oid):
            i = x['inst']; s = [v * k for v in (i.get('scale') or [1, 1, 1])]
            ni = make_inst(B, x['piece'], np.array(i['blender_loc_m']), np.array(i['blender_rot_3x3']), s)
            for kk in ('object', 'role', 'index', 'mover', 'hinge_axis_game', 'open_angle_game_deg', 'slide_game_uu'):
                if kk in i: ni[kk] = i[kk]
            x['inst'] = ni; _upd_obj(objs[oid], ni); objs[oid]['v3'] = 'scaled'
            for h in hides:
                if h.get('object') == oid:
                    o = objs[oid]; h['rect'] = [o['bb'][0], o['bb'][1], o['bb'][2], o['bb'][3]]; h['z'] = [o['z'][0], min(h['z'][1], o['z'][1])]
    # BASE
    for oid in L.REMOVE: remove(oid)
    hazards_off.add('jam_floor')                     # the jam puddle by the high chair is now seed mess / a chain result
    for oid, (cxy, yaw, z0, why) in L.MOVE.items(): move(oid, cxy, yaw, z0)
    for oid, k in L.SCALE.items():
        if oid in objs: scale(oid, k)
    for a in L.ADD: add(a)
    if comp is None or base_only:
        st = _finish(B, objs, order, insts, hides, conn_off, edge_ops, hazards_add, hazards_off, movers, pets, cake_w, comp)
        return st
    # variants
    for d in L.DISTRICT_ORDER:
        vid = comp['variants'][d]
        v = [x for x in L.VARIANTS[d] if x['id'] == vid][0]
        for op in v['ops']:
            k = op[0]
            if k == 'add': add(dict(op[1], district=d, variant=vid))
            elif k == 'remove': remove(op[1])
            elif k == 'restore':
                o = copy.deepcopy(B['obj'][op[1]]); o['district'] = d; o['variant'] = vid; o['v3'] = 'restored'; objs[o['id']] = o; order.append(o['id'])
                for x in B['insts']:
                    if x['object'] == o['id']: insts.append(dict(x, inst=copy.deepcopy(x['inst'])))
                for h in B['hides']:
                    if h.get('object') == o['id']: hides.append(dict(h))
            elif k == 'move':
                move(op[1], op[2], op[3], op[4])
                if len(op) > 5 and op[1] in objs: objs[op[1]]['mount'] = op[5]
            elif k == 'mover': movers[op[1]] = op[2]
            elif k == 'edge': edge_ops.append((op[1], op[2], op[3], op[4]))
            elif k == 'conn_off': conn_off.add(op[1])
            elif k == 'hide': h = dict(op[1]); h['object'] = None; h['src'] = 'v3 variant'; hides.append(h)
            elif k == 'hide_off': hides[:] = [h for h in hides if h.get('id') != op[1]]
            elif k == 'hazard': hazards_add.append(dict(op[1], district=d))
            elif k == 'hazard_off': hazards_off.add(op[1])
            elif k == 'cake': cake_w[op[1]] = op[2]
            elif k == 'dog_spot':
                for sp in pets['dog']:
                    if sp['label'] == op[1]: sp['p'] = [op[2][0], op[2][1], 0.0]
    if chaos:
        for op in L.CHAOS:
            k = op[0]
            if k == 'add': add(dict(op[1], district='chaos', variant='chaos'))
            elif k == 'remove': remove(op[1])
            elif k == 'move': move(op[1], op[2], op[3], op[4])
            elif k == 'mover': movers[op[1]] = op[2]
            elif k == 'rot': rotate_world(op[1], op[2], op[3], op[4])
    for m, sv in comp.get('doors', {}).items(): movers.setdefault(m, sv)
    for sc in comp.get('scatter', []):
        if sc.get('v2_object'):
            o = copy.deepcopy(B['obj'][sc['v2_object']]); objs[o['id']] = o
            for x in B['insts']:
                if x['object'] == o['id']: insts.append(dict(x, inst=copy.deepcopy(x['inst'])))
            if o['id'] == 'new39': hazards_off.discard('jam_floor')
    return _finish(B, objs, order, insts, hides, conn_off, edge_ops, hazards_add, hazards_off, movers, pets, cake_w, comp)

def _finish(B, objs, order, insts, hides, conn_off, edge_ops, hazards_add, hazards_off, movers, pets, cake_w, comp):
    D2 = B['D2']
    olist = [objs[i] for i in dict.fromkeys(order) if i in objs]
    # mover poses
    for o in olist:
        mk = mover_key(o) or {v: k for k, v in CODE_DOOR_OBJ.items()}.get(o['id'])
        if mk and mk in movers: o['pose_v3'] = movers[mk]
    st = {'objects': olist, 'insts': insts, 'hides': hides, 'conn_off': sorted(conn_off), 'edge_ops': edge_ops, 'hazards_add': hazards_add, 'hazards_off': sorted(hazards_off),
          'movers': movers, 'pets': pets, 'cake_w': cake_w, 'comp': comp}
    st['connectors'] = [c for c in D2['connectors'] if c['id'] not in conn_off]
    st['graph'] = graph_state(D2, edge_ops)
    return st

# ---- the route graph of one state --------------------------------------------------------------------------------------------------------
def graph_state(D2, edge_ops):
    E = []
    for e in D2['graph']['edges']:
        e2 = {'a': e['a'], 'b': e['b'], 'kind': e['kind'], 'len_uu': e['len_uu'], 'k': 1.0, 'open': True, 'note': e.get('note', '')}
        for a, b, sv, note in edge_ops:
            if {a, b} == {e['a'], e['b']}:
                if sv == 'closed': e2['open'] = False
                elif sv.startswith('slow:'): e2['k'] = float(sv[5:])
                elif sv in ('cover', 'open'): e2['kind'] = sv
                e2['note'] = note
        E.append(e2)
    return {'nodes': D2['graph']['nodes'], 'edges': E}

CARRY = 288.0 * 0.7          # uu/s: carried load at 0.7 of walk speed (FEATURES: 0.5..0.9)
def cake_routes(G, src, dst='nest'):
    """Edge-disjoint haul routes src -> dst (min-cost flow, 2 units), each with its carry time."""
    nodes = [n['id'] for n in G['nodes']]; ix = {n: i for i, n in enumerate(nodes)}
    arcs = []                       # [u, v, cap, cost, rev]
    adj = [[] for _ in nodes]
    def arc(u, v, cost):
        adj[u].append(len(arcs)); arcs.append([u, v, 1, cost, len(arcs) + 1])
        adj[v].append(len(arcs)); arcs.append([v, u, 0, -cost, len(arcs) - 1])
    for e in G['edges']:
        if not e['open']: continue
        c = e['len_uu'] * e['k'] / CARRY
        arc(ix[e['a']], ix[e['b']], c); arc(ix[e['b']], ix[e['a']], c)
    s, t = ix[src], ix[dst]
    flows = []
    for _ in range(2):
        dist = [1e18] * len(nodes); prev = [None] * len(nodes); dist[s] = 0.0
        for _it in range(len(nodes)):
            ch = False
            for ai, a in enumerate(arcs):
                if a[2] > 0 and dist[a[0]] + a[3] < dist[a[1]] - 1e-9:
                    dist[a[1]] = dist[a[0]] + a[3]; prev[a[1]] = ai; ch = True
            if not ch: break
        if dist[t] >= 1e17: break
        v = t
        while v != s:
            ai = prev[v]; arcs[ai][2] -= 1; arcs[arcs[ai][4]][2] += 1; v = arcs[ai][0]
        flows.append(dist[t])
    # decompose into paths
    used = {}
    for a in arcs:
        if a[3] >= 0 and a[2] == 0 and arcs[a[4]][2] == 1:
            used.setdefault(a[0], []).append(a[1])
    paths = []
    for _ in range(len(flows)):
        p = [s]; v = s; guard = 0
        while v != t and used.get(v) and guard < 100:
            w = used[v].pop(); p.append(w); v = w; guard += 1
        paths.append([nodes[i] for i in p])
    times = []
    for p in paths:
        tt = 0.0
        for a, b in zip(p, p[1:]):
            e = [e for e in G['edges'] if {e['a'], e['b']} == {a, b} and e['open']][0]
            tt += e['len_uu'] * e['k'] / CARRY
        times.append(round(tt, 1))
    order = sorted(range(len(paths)), key=lambda i: times[i])
    return [paths[i] for i in order], [times[i] for i in order]

# ---- grids (100 uu, the audit grid of v2) -----------------------------------------------------------------------------------------------
class Grid:
    def __init__(self, D2):
        A = D2['audit']; self.X0, self.Y0, self.X1, self.Y1 = A['grid']; self.G = 100.0
        self.NX, self.NY = int((self.X1 - self.X0) / self.G), int((self.Y1 - self.Y0) / self.G)
        self.cx = self.X0 + (np.arange(self.NX) + 0.5) * self.G; self.cy = self.Y0 + (np.arange(self.NY) + 0.5) * self.G
        XX, YY = np.meshgrid(self.cx, self.cy)
        reg = np.zeros((self.NY, self.NX), np.int8)
        for r, k in reversed(A['regions']):
            reg[(XX > r[0]) & (XX < r[2]) & (YY > r[1]) & (YY < r[3])] = k
        self.REG = reg; self.XX, self.YY = XX, YY
        self.seed_out = A.get('seed_out', [])
    def span(self, a, b, c0, n):
        i0 = int(math.ceil((a - c0) / self.G - 0.5)); i1 = int(math.floor((b - c0) / self.G - 0.5))
        return max(0, i0), min(n - 1, i1)
    def paint(self, m, bb, val=True):
        i0, i1 = self.span(bb[0], bb[2], self.X0, self.NX); j0, j1 = self.span(bb[1], bb[3], self.Y0, self.NY)
        if i1 >= i0 and j1 >= j0: m[j0:j1 + 1, i0:i1 + 1] = val
    def paint_poly(self, m, pts):
        P = np.array(pts); a, b = P.min(0), P.max(0)
        i0, i1 = self.span(a[0], b[0], self.X0, self.NX); j0, j1 = self.span(a[1], b[1], self.Y0, self.NY)
        for j in range(j0, j1 + 1):
            for i in range(i0, i1 + 1):
                if _in_poly(self.cx[i], self.cy[j], pts): m[j, i] = True
    def ij(self, x, y): return (max(0, min(self.NX - 1, int((x - self.X0) // self.G))), max(0, min(self.NY - 1, int((y - self.Y0) // self.G))))

def _in_poly(x, y, P):
    c = False; n = len(P)
    for k in range(n):
        x1, y1 = P[k]; x2, y2 = P[(k + 1) % n]
        if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1): c = not c
    return c

def shift(m, di, dj, fill=False):
    out = np.full_like(m, fill)
    H, W = m.shape
    ys, yd = (slice(max(0, -dj), H - max(0, dj)), slice(max(0, dj), H - max(0, -dj)))
    xs, xd = (slice(max(0, -di), W - max(0, di)), slice(max(0, di), W - max(0, -di)))
    out[yd, xd] = m[ys, xs]
    return out
def dilate(m, r_cells, strict=True):
    out = m.copy(); R = int(math.ceil(r_cells))
    for dj in range(-R, R + 1):
        for di in range(-R, R + 1):
            d = math.hypot(di, dj)
            if (d < r_cells if strict else d <= r_cells) and (di or dj): out |= shift(m, di, dj)
    return out
def flood(start, can):
    st = np.zeros_like(can); st[start[1], start[0]] = can[start[1], start[0]]
    for _ in range(4000):
        g = st | ((shift(st, 1, 0) | shift(st, -1, 0) | shift(st, 0, 1) | shift(st, 0, -1)) & can)
        if (g == st).all(): break
        st = g
    return st
MOVES16 = [(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1), (2, 1), (2, -1), (-2, 1), (-2, -1), (1, 2), (1, -2), (-1, 2), (-1, -2)]
def geodesic(seeds, passable, iters=45, G=100.0):
    INF = 1e9
    d = np.where(seeds & passable, 0.0, INF)
    P = passable
    for _ in range(iters):
        nd = d.copy()
        for di, dj in MOVES16:
            ok = P & shift(P, di, dj)          # target and source passable
            if abs(di) == 2:
                s1 = di // 2; ok &= shift(P, s1, 0) & shift(P, s1, dj)
            elif abs(dj) == 2:
                s1 = dj // 2; ok &= shift(P, 0, s1) & shift(P, di, s1)
            elif di and dj:
                ok &= shift(P, di, 0) & shift(P, 0, dj)
            cand = shift(d, di, dj, INF) + G * math.hypot(di, dj)
            nd = np.where(ok & (cand < nd), cand, nd)
        if (nd == d).all(): break
        d = nd
    return d

def leaf_poly(o, frac_deg):
    """Footprint (xy polygon) of a hinged leaf opened by frac_deg from closed, rotating toward its bb_open."""
    bb = o['bb']; hx, hy = (o.get('origin') or [bb[0], bb[1]])[:2]
    C = [(bb[0], bb[1]), (bb[2], bb[1]), (bb[2], bb[3]), (bb[0], bb[3])]
    full = abs((o.get('mover') or {}).get('angle') or 90.0)
    def rot(pts, a):
        c, s = math.cos(math.radians(a)), math.sin(math.radians(a))
        return [(hx + (x - hx) * c - (y - hy) * s, hy + (x - hx) * s + (y - hy) * c) for x, y in pts]
    sign = 1.0
    if o.get('bb_open'):
        bo = o['bb_open']; tgt = ((bo[0] + bo[2]) / 2, (bo[1] + bo[3]) / 2)
        def cen(p): return (sum(x for x, _ in p) / 4, sum(y for _, y in p) / 4)
        a1, a2 = cen(rot(C, full)), cen(rot(C, -full))
        sign = 1.0 if math.hypot(a1[0] - tgt[0], a1[1] - tgt[1]) <= math.hypot(a2[0] - tgt[0], a2[1] - tgt[1]) else -1.0
    return rot(C, sign * frac_deg)

def pose_footprint(o):
    """('bb', box) or ('poly', pts) of an object in its v3 pose."""
    pose = o.get('pose_v3')
    if pose is None:
        if o.get('pose') == 'open' and o.get('bb_open'): return ('bb', o['bb_open'])
        mv = o.get('mover') or {}
        if 'starts_open' in (mv.get('flags') or []) and o.get('bb_open'): return ('bb', o['bb_open'])
        return ('bb', o['bb'])
    if pose == 'closed': return ('bb', o['bb'])
    if pose == 'open': return ('bb', o.get('bb_open') or o['bb'])
    if pose.startswith('ajar:'):
        if (o.get('mover') or {}).get('type', 'hinge') == 'hinge': return ('poly', leaf_poly(o, float(pose[5:])))
        return ('bb', o['bb'])
    return ('bb', o['bb'])

# ---- the checks of one state ------------------------------------------------------------------------------------------------------------
HUM_H, LOW_CLUTTER = 3400.0, 450.0
def human_nav(B, st, Gd):
    D2 = B['D2']; H = D2['human']; R, STEP = L.VALIDATE['human_capsule']
    solid = ~np.isin(Gd.REG, (1, 2))
    for o in st['objects']:
        if not o.get('solid') or o.get('role') == 'rider' or o.get('mesh') in ('SM_L_DoorFrame', 'SM_K_Window') or o.get('collision') == 'ants_only': continue
        if o.get('state') == 'after': continue
        if o.get('cloth') and o.get('mesh') != 'SM_V3_BlanketFort': continue      # hanging cloth: he walks through it / the table under it is painted
        if o['z'][0] < HUM_H and o['z'][1] > LOW_CLUTTER:
            if o['id'] in L.HUMAN_STEP_RAMPS:
                ax, sg = L.HUMAN_STEP_RAMPS[o['id']]; bb = list(o['bb']); k = 0 if ax == 'x' else 1
                lo, hi = bb[k], bb[k + 2]; f = (STEP - o['z'][0]) / max(1.0, o['z'][1] - o['z'][0])
                if sg > 0: bb[k] = lo + f * (hi - lo)
                else: bb[k + 2] = hi - f * (hi - lo)
                Gd.paint(solid, bb); continue
            kind, fp = pose_footprint(o)
            if kind == 'bb': Gd.paint(solid, fp)
            else: Gd.paint_poly(solid, fp)
    can = np.isin(Gd.REG, (1, 2)) & ~dilate(solid, (R + 50.0) / Gd.G)
    sp = Gd.ij(*H['spawn'][:2])
    stand = flood(sp, can)
    return stand, solid

def stand_near(Gd, stand, p, r):
    i, j = Gd.ij(p[0], p[1]); R = int(math.ceil(r / Gd.G))
    j0, j1, i0, i1 = max(0, j - R), min(Gd.NY, j + R + 1), max(0, i - R), min(Gd.NX, i + R + 1)
    sub = stand[j0:j1, i0:i1]
    if not sub.any(): return None
    jj, ii = np.nonzero(sub)
    d = np.hypot(Gd.cx[i0 + ii] - p[0], Gd.cy[j0 + jj] - p[1])
    return float(d.min()) if d.min() <= r else None

def floorstand(o):
    return o.get('solid') and o['z'][0] < 60 and o['z'][1] > 300 and 'gap_under' not in o and o['cat'] in ('furn', 'prop', 'conn', 'food') and o.get('mesh') != 'SM_Y_Tree' and not o.get('tilted') and o.get('state') != 'after'

def cover(B, st, Gd, stand):
    D2 = B['D2']; H = D2['human']
    slap = H['attacks']['slap']['ai_uses_between_2d'][1]
    near = dilate(stand, slap / Gd.G, strict=False)
    hidden = np.zeros_like(stand)
    for h in st['hides']:
        if h['z'][0] <= 5.0 < h['z'][1]: Gd.paint(hidden, h['rect'])
    nest = [s for s in D2['spawns'] if s['id'] == 'nest0'][0]
    in_nest = np.hypot(Gd.XX - nest['p'][0], Gd.YY - nest['p'][1]) < nest['radius']
    REG = Gd.REG
    cov2 = np.isin(REG, (3, 4, 5)) | hidden | in_nest | ~near
    occ = np.zeros_like(stand)
    for o in st['objects']:
        if floorstand(o): Gd.paint(occ, o['bb'])
    so = np.zeros_like(stand)
    for r in Gd.seed_out: so |= (Gd.XX >= r[0]) & (Gd.XX <= r[2]) & (Gd.YY >= r[1]) & (Gd.YY <= r[3])
    seeds = cov2 & ((np.isin(REG, (1, 2, 5)) & (~occ | hidden)) | (np.isin(REG, (3, 4)) & so))
    DC = geodesic(seeds, REG != 0)
    out = {}
    VS = D2['constants']['ant']['sprint_uu_s']
    for reg, name in ((1, 'kitchen'), (2, 'living')):
        m = (REG == reg) & ~occ
        ev = m & ~cov2
        tot = int(m.sum())
        t = DC[ev] / VS
        worst = float(t.max()) if t.size else 0.0
        if t.size:
            jj, ii = np.nonzero(ev); k = int(np.argmax(DC[ev]))
            at = [round(float(Gd.cx[ii[k]])), round(float(Gd.cy[jj[k]]))]
        else: at = None
        out[name] = {'cover_pct': round(100.0 * (tot - int(ev.sum())) / max(1, tot), 1), 'worst_s': round(worst, 2), 'worst_at': at,
                     'over4s_cells': int((t > L.VALIDATE['cover_max_s'] + L.VALIDATE['cover_tolerance_s']).sum()), 'mean_s': round(float(t.mean()), 2) if t.size else 0.0}
    return out, DC, cov2, occ

SURF_KEYS = None
def _ov(a, b): return max(0.0, min(a[2], b[2]) - max(a[0], b[0])) * max(0.0, min(a[3], b[3]) - max(a[1], b[1]))
def _gap(a, b): return max(a[0] - b[2], b[0] - a[2], a[1] - b[3], b[1] - a[3], 0.0)

def floats(B, st):
    D2 = B['D2']; OBJ = {o['id']: o for o in st['objects']}
    SURF = [(s['z'], r) for s in D2['surfaces'] for r in s['rects']]
    WALLS = [w['rect'] for w in D2['walls']]
    bad = []
    for o in st['objects']:
        if o.get('v3') not in ('new', 'moved') or o.get('cat') == 'fastener': continue
        z0, bb, mount = o['z'][0], o['bb'], o.get('mount') or ''
        if z0 <= 30: continue
        how = None
        if mount.startswith('on:'):
            q = OBJ.get(mount[3:])
            if q and _ov(bb, q['bb']) > 0 and (q['z'][0] - 30 <= z0 <= q['z'][1] + 45 or any(abs(t - z0) <= 45 for t in q.get('tops', []))): how = 'on ' + q['id']
        if not how and mount in ('ceiling',) and o['z'][1] >= 5350: how = 'ceiling'
        if not how and mount in ('string',): how = 'string'
        if not how and mount == 'hang':
            for q in st['objects']:
                if q is not o and (q.get('mesh') in ('SM_V3_CurtainRing', 'SM_V3_CurtainRod', 'SM_V3_LaundryLine', 'SM_V3_RodBracket')) and _gap(bb, q['bb']) <= 60 and abs(q['z'][0] - o['z'][1]) <= 120: how = 'hangs on ' + q['id']; break
            if not how:
                for q in st['objects']:
                    if q is not o and q.get('mesh') in ('SM_V3_RodBracket', 'SM_V3_LaundryPole') and _gap(bb, q['bb']) <= 80: how = 'hangs on ' + q['id']; break
        if not how:
            for z, r in SURF:
                if abs(z - z0) <= 45 and _ov(bb, r) > 0: how = 'surface'; break
        if not how:
            for q in st['objects']:
                if q is o or not q.get('solid'): continue
                if _ov(bb, q['bb']) > 0 and (abs(q['z'][1] - z0) <= 45 or any(abs(t - z0) <= 45 for t in q.get('tops', [])) or (q.get('interior') and q['z'][0] <= z0 <= q['z'][1])): how = 'on ' + q['id']; break
        if not how and mount in ('wall', 'hang', 'ceiling', ''):
            for w in WALLS:
                if _gap(bb, w) <= 45: how = 'wall'; break
        if not how: bad.append({'id': o['id'], 'mesh': o.get('mesh'), 'z0': round(z0), 'mount': mount})
    return bad

def _solid_for_overlap(o):
    if not o.get('solid') or o.get('collision') == 'ants_only' or o.get('cloth') or o.get('state') == 'after' or o.get('cat') in ('fastener', 'part', 'deco'): return False
    if o['cat'] == 'struct' and (o['z'][1] - o['z'][0]) < 60: return False
    if (o.get('mesh') or '').startswith(('SM_K_Rug', 'SM_L_Carpet', 'SM_K_PetMat', 'SM_V3_DoorMat', 'SM_L_TrainTrack')): return False
    return True
def overlaps(B, st):
    O = [o for o in st['objects'] if _solid_for_overlap(o)]
    new = [o for o in O if o.get('v3') in ('new', 'moved')]
    out = []
    for o in new:
        for q in O:
            if q.get('mesh') == 'SM_Y_Tree': continue
            if q is o or q['id'] == o.get('replaces') or o['id'] == q.get('replaces') or q.get('part_of') == o['id'] or o.get('part_of') == q['id']: continue
            if (o.get('mount') or '') == 'on:' + q['id'] or (q.get('mount') or '') == 'on:' + o['id']: continue
            if q.get('interior') or o.get('interior'): continue
            a = _ov(o['bb'], q['bb'])
            zo = min(o['z'][1], q['z'][1]) - max(o['z'][0], q['z'][0])
            if a > 400 and zo > 20:
                if 'gap_under' in q and o['z'][1] <= q['gap_under'] + 5: continue
                if 'gap_under' in o and q['z'][1] <= o['gap_under'] + 5: continue
                if q.get('v3') in ('new', 'moved') and q['id'] < o['id']: continue
                out.append({'a': o['id'], 'b': q['id'], 'mesh_a': o.get('mesh'), 'mesh_b': q.get('mesh'), 'area': round(a), 'z_overlap': round(zo)})
    return out

def door_sweeps(B, st):
    O = [o for o in st['objects'] if _solid_for_overlap(o) and o.get('v3') in ('new', 'moved')]
    out = []
    for m in st['objects']:
        if not m.get('bb_open'): continue
        zr = m.get('z_open') or m['z']
        for o in O:
            if o['z'][0] < zr[1] - 10 and o['z'][1] > zr[0] + 10 and _ov(m['bb_open'], o['bb']) > 400:
                out.append({'door': m['id'], 'door_ru': m['ru'], 'blocked_by': o['id'], 'mesh': o.get('mesh')})
    return out

def scatter_check(B, st, Gd, stand):
    comp = st['comp'] or {}
    items = [s for s in comp.get('scatter', []) if s.get('p')]
    solids = [o for o in st['objects'] if o.get('solid') and o['cat'] not in ('struct',) and o.get('collision') != 'ants_only' and not o.get('cloth')]
    spots = [r['p'] for r in L.ROUTINES['human'] if r.get('p')]
    kept, dropped = [], []
    for s in items:
        x, y, z = s['p']; why = None
        for o in solids:
            b = o['bb']
            if b[0] < x < b[2] and b[1] < y < b[3] and o['z'][0] <= z + 10 <= o['z'][1] and 'gap_under' not in o: why = 'inside ' + o['id']; break
        if not why:
            for m in st['objects']:
                if m.get('bb_open') and m['bb_open'][0] < x < m['bb_open'][2] and m['bb_open'][1] < y < m['bb_open'][3] and m['z'][0] < 300: why = 'door sweep ' + m['id']; break
        if not why and any(math.hypot(x - p[0], y - p[1]) < 300 for p in spots): why = 'on a routine spot'
        if not why and any(math.hypot(x - k['p'][0], y - k['p'][1]) < L.VALIDATE['scatter_spacing_uu'] for k in kept): why = 'too close to another'
        (dropped if why else kept).append(dict(s, why=why) if why else s)
    return kept, dropped

def routines_check(B, st, Gd, stand):
    D2 = B['D2']; H = D2['human']; comp = st['comp'] or {'home': {'human': True, 'grandma': True, 'kid': True}}
    r = L.VALIDATE['routine_reach_uu']; out = []
    pts = [('human', 'spawn', H['spawn'][:2], [])] + [('human', 'door%d' % k, p[:2], []) for k, p in enumerate(H['patrol']['door_waypoints'])]
    pts += [('human', 'patrol%d' % k, L.PATROL_FIX.get(k, (p,))[0], []) for k, p in enumerate(H['patrol']['points'])]
    for who in ('human', 'grandma', 'kid'):
        if not comp['home'].get(who, True): continue
        for sp in L.ROUTINES[who]:
            if sp.get('p'): pts.append((who, sp['id'], sp['p'], sp.get('alt', [])))
    for who, name, p, alts in pts:
        d = None
        for q in [p] + list(alts):
            d = stand_near(Gd, stand, q, r)
            if d is not None: p = q; break
        out.append({'who': who, 'spot': name, 'p': [round(p[0]), round(p[1])], 'ok': d is not None, 'd': None if d is None else round(d)})
    return out

def run_checks(B, st, Gd=None, want_grids=False):
    Gd = Gd or Grid(B['D2'])
    stand, solid = human_nav(B, st, Gd)
    cov, DC, cov2, occ = cover(B, st, Gd, stand)
    comp = st['comp']
    cake = comp['cake'] if comp else 'M2'
    node = L.CAKE_SPOTS[cake]['node']
    paths, times = cake_routes(st['graph'], node)
    rt = routines_check(B, st, Gd, stand)
    fl = floats(B, st); ov = overlaps(B, st); sw = door_sweeps(B, st)
    kept, dropped = scatter_check(B, st, Gd, stand)
    V = L.VALIDATE; fails = []
    if len(paths) < V['cake_routes']: fails.append('cake: %d haul route(s) from %s (%s), need %d' % (len(paths), cake, node, V['cake_routes']))
    else:
        if times[0] > V['cake_route_max_s'][0]: fails.append('cake: best route %.0f s > %.0f' % (times[0], V['cake_route_max_s'][0]))
        if times[1] > V['cake_route_max_s'][1]: fails.append('cake: second route %.0f s > %.0f' % (times[1], V['cake_route_max_s'][1]))
    for a, c in cov.items():
        if c['worst_s'] > V['cover_max_s'] + V['cover_tolerance_s']: fails.append('cover: %s %.2f s at %s (%d cells over)' % (a, c['worst_s'], c['worst_at'], c['over4s_cells']))
    for x in rt:
        if not x['ok']: fails.append('nav: %s cannot reach %s %s' % (x['who'], x['spot'], x['p']))
    for f in fl: fails.append('float: %s %s z0=%s' % (f['id'], f['mesh'], f['z0']))
    for o in ov: fails.append('overlap: %s (%s) x %s (%s) %d uu2' % (o['a'], o['mesh_a'], o['b'], o['mesh_b'], o['area']))
    for s in sw: fails.append('door: %s blocked by %s' % (s['door'], s['blocked_by']))
    res = {'ok': not fails, 'fails': fails, 'cover': cov, 'cake': {'spot': cake, 'node': node, 'routes': paths, 'times_s': times},
           'routines': rt, 'floats': fl, 'overlaps': ov, 'door_sweeps': sw, 'scatter_kept': len(kept), 'scatter_dropped': dropped,
           'stand_cells': int(stand.sum())}
    if want_grids: res['_grids'] = {'stand': stand, 'cov2': cov2, 'occ': occ, 'DC': DC, 'Gd': Gd}
    return res

def path_on_stand(Gd, stand, a, b):
    """A* on the Human's stand grid between two points (for the routine paths on the map). Returns game points."""
    s = Gd.ij(*a); t = Gd.ij(*b)
    def nearest(c):
        if stand[c[1], c[0]]: return c
        jj, ii = np.nonzero(stand); k = int(np.argmin((ii - c[0]) ** 2 + (jj - c[1]) ** 2)); return (int(ii[k]), int(jj[k]))
    s, t = nearest(s), nearest(t)
    pq = [(0.0, s)]; g = {s: 0.0}; prev = {}
    while pq:
        f, c = heapq.heappop(pq)
        if c == t: break
        for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)):
            n = (c[0] + di, c[1] + dj)
            if not (0 <= n[0] < Gd.NX and 0 <= n[1] < Gd.NY) or not stand[n[1], n[0]]: continue
            ng = g[c] + math.hypot(di, dj)
            if ng < g.get(n, 1e18):
                g[n] = ng; prev[n] = c; heapq.heappush(pq, (ng + math.hypot(n[0] - t[0], n[1] - t[1]), n))
    if t not in g: return None
    out = [t]
    while out[-1] != s: out.append(prev[out[-1]])
    pts = [(round(float(Gd.cx[i])), round(float(Gd.cy[j]))) for i, j in reversed(out)]
    keep = [pts[0]]                                    # thin straight runs
    for k in range(1, len(pts) - 1):
        a0, b0, c0 = keep[-1], pts[k], pts[k + 1]
        if (b0[0] - a0[0]) * (c0[1] - a0[1]) - (b0[1] - a0[1]) * (c0[0] - a0[0]) != 0: keep.append(b0)
    keep.append(pts[-1])
    return keep
