#!/usr/bin/env python3
"""Ant Kitchen project map generator.

Reads the repo (Source, docs, Content, Tools, git, gh releases) + curated YAML in project/map/
and writes project/map/map.json (+ SUMMARY.md). Usage:  python Tools/projectmap/build_map.py [--no-gh] [--out DIR]
Needs: Python 3.11, pyyaml (pip install --user pyyaml), git, optional gh.
"""
import os, re, sys, json, subprocess, datetime, collections, argparse
import yaml

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
MAP = os.path.join(ROOT, 'project', 'map')
SRC = os.path.join(ROOT, 'Source', 'AntKitchen')

def rd(p):
    with open(p, encoding='utf-8', errors='replace') as f: return f.read()
def ly(name, default=None):
    p = os.path.join(MAP, name)
    return yaml.safe_load(rd(p)) if os.path.exists(p) else default
def sh(args, check=False):
    try:
        r = subprocess.run(args, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=120)
        return r.stdout
    except Exception as e:
        return ''
def clip(s, n):
    s = re.sub(r'\s+', ' ', s or '').strip()
    return s if len(s) <= n else s[:n - 1].rstrip() + '…'
def clean_md(s):
    s = re.sub(r'`([^`]*)`', r'\1', s); s = re.sub(r'\*\*([^*]*)\*\*', r'\1', s)
    s = re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', s); return s.strip()
def today(): return datetime.date.today().isoformat()

nodes = collections.OrderedDict(); edges = []; edge_set = set()
def add(id, type, name, area=None, parent=None, **f):
    n = dict(id=id, type=type, name=name, area=area, parent=parent)
    n.update({k: v for k, v in f.items() if v not in (None, [], {}, '')})
    if id in nodes: nodes[id].update(n); return nodes[id]
    nodes[id] = n; return n
def edge(a, b, kind='uses', w=None):
    if a == b or a is None or b is None: return
    k = (a, b, kind)
    if k in edge_set: return
    edge_set.add(k); e = dict(**{'from': a, 'to': b, 'kind': kind}); 
    if w: e['w'] = w
    edges.append(e)

ap = ly('areas.yaml', {}); META = ap.get('meta', {})
REPO = META.get('repo', 'kisa134/antgame'); BRANCH = META.get('blob_branch', 'code/ANT-v14')
BLOB = f'https://github.com/{REPO}/blob/{BRANCH}/'
def blob(p): return BLOB + p.replace('\\', '/')

# ---------------------------------------------------------------- areas
for a in ap.get('areas', []):
    add('area.' + a['id'], 'area', a['name'], a['id'], None, summary=a.get('summary'), color=a.get('color'), status='in-game')
AREA_IDS = {a['id'] for a in ap.get('areas', [])}

# ---------------------------------------------------------------- systems
SYSTEMS = ly('systems.yaml', {}).get('systems', [])
for s in SYSTEMS:
    add(s['id'], 'system', s['name'], s['area'], 'area.' + s['area'], status=s.get('status', 'in-game'), owner=s.get('owner'),
        summary=s.get('summary'), docs=s.get('docs'), tags=['code'] if s.get('modules') else None)
SYS_RULES = [(re.compile(rx), s['id']) for s in SYSTEMS for rx in s.get('modules', [])]
SYS_AREA = {s['id']: s['area'] for s in SYSTEMS}
def sys_of(stem):
    for cand in (stem, re.sub(r'(Data|Art|Look)$', '', stem)):
        for rx, sid in SYS_RULES:
            if rx.search(cand): return sid
    return 'sys.misc'

# ---------------------------------------------------------------- git history (one pass)
git_files = collections.defaultdict(list)   # path -> [(hash,date,subject)]
recent = []
out = sh(['git', 'log', '--name-only', '--no-merges', '--date=short', '--pretty=format:@@%h|%ad|%an|%s', '-n', '1500'])
cur = None
for ln in out.splitlines():
    if ln.startswith('@@'):
        h, d, an, sj = ln[2:].split('|', 3); cur = (h, d, sj); recent.append(dict(hash=h, date=d, author=an, subject=sj, files=0))
    elif ln.strip() and cur:
        git_files[ln.strip().replace('\\', '/')].append(cur); recent[-1]['files'] += 1
HEAD = sh(['git', 'rev-parse', '--short', 'HEAD']).strip(); CUR_BRANCH = sh(['git', 'rev-parse', '--abbrev-ref', 'HEAD']).strip()
def commits_for(paths, n=4):
    seen = {}; 
    for p in paths:
        for c in git_files.get(p.replace('\\', '/'), []): seen[c[0]] = c
    cs = sorted(seen.values(), key=lambda c: c[1], reverse=True)
    return len(cs), [dict(hash=c[0], date=c[1], subject=clip(c[2], 110)) for c in cs[:n]]

# ---------------------------------------------------------------- code modules
stems = collections.defaultdict(list)
for fn in sorted(os.listdir(SRC)):
    if re.search(r'\.(h|cpp|inl|cs)$', fn):
        stem = re.sub(r'\.Build\.cs$|\.(h|cpp|inl|cs)$', '', fn)
        stems[stem].append(fn)
STEM_SYS = {}; inc = collections.defaultdict(set); sys_edges = collections.Counter()
def first_comment(txt):
    m = re.search(r'/\*\*(.*?)\*/', txt, re.S)
    if m:
        c = re.sub(r'^\s*\*\s?', '', m.group(1), flags=re.M).strip()
        return clip(c, 420)
    m = re.match(r'(?:\s*//[^\n]*\n){1,6}', txt)
    if m: return clip(re.sub(r'^\s*//\s?', '', m.group(0), flags=re.M), 300)
    return None
for stem, fl in stems.items():
    sid = sys_of(stem); STEM_SYS[stem] = sid
    loc = 0; comment = None; incs = set()
    for fn in fl:
        t = rd(os.path.join(SRC, fn)); loc += t.count('\n')
        for m in re.finditer(r'#include\s+"([A-Za-z0-9_]+)\.(?:h|inl)"', t): incs.add(m.group(1))
        if comment is None and fn.endswith('.h'): comment = first_comment(t)
        if comment is None and fn.endswith('.inl'): comment = first_comment(t)
    if comment is None: comment = first_comment(rd(os.path.join(SRC, fl[0])))
    paths = ['Source/AntKitchen/' + f for f in fl]
    cnt, cs = commits_for(paths)
    add('file.' + stem, 'file', stem, SYS_AREA.get(sid, 'tech'), sid, status='in-game', summary=comment, files=paths, loc=loc,
        commits=cs, commit_count=cnt, last_change=cs[0]['date'] if cs else None, includes=sorted(i for i in incs if i in stems and i != stem))
    inc[stem] = incs
for stem, incs in inc.items():
    for i in incs:
        if i in STEM_SYS and STEM_SYS[i] != STEM_SYS[stem]: sys_edges[(STEM_SYS[stem], STEM_SYS[i])] += 1
for (a, b), w in sys_edges.items(): edge(a, b, 'uses', w)
# aggregate system stats
for sid in [s['id'] for s in SYSTEMS]:
    fl = [n for n in nodes.values() if n['type'] == 'file' and n['parent'] == sid]
    if fl:
        allc = []; 
        for n in fl: allc += [(c['date'], c['hash'], c['subject']) for c in n.get('commits', [])]
        allc = sorted(set(allc), reverse=True)[:5]
        nodes[sid].update(loc=sum(n['loc'] for n in fl), module_count=len(fl), last_change=max((n.get('last_change') or '') for n in fl) or None,
                          commits=[dict(date=d, hash=h, subject=s) for d, h, s in allc], commit_count=sum(n.get('commit_count', 0) for n in fl),
                          files=[p for n in fl for p in n['files']][:12])

# ---------------------------------------------------------------- docs
DOC_AREA = {'FIGHTER': 'meta', 'COLONY': 'meta', 'MATERIALS': 'physics', 'CHAINS': 'physics', 'AUDIO': 'audio', 'SOUNDS': 'audio', 'UI_': 'ui', 'LIFE': 'world', 'LEVEL': 'world', 'V4_': 'world',
            'FILMING': 'marketing', 'ADMIN': 'tech', 'STATUS': 'team', 'FEATURES': 'team', 'GAME': 'gameplay', 'DEMO': 'gameplay', 'ONBOARDING': 'gameplay', 'PLAYTEST': 'team',
            'ASSETS': 'items', 'ART_INTEGRATION': 'items', 'GLOSSARY': 'team', 'CLAUDE': 'team', 'README': 'team', 'PROJECT_MAP': 'tools'}
DOCS = {}
def doc_paths():
    for rel in ['CLAUDE.md', 'README.md', 'ASSETS.md', 'SOUNDS.md']:
        if os.path.exists(os.path.join(ROOT, rel)): yield rel
    for base in ['docs', 'docs/level']:
        d = os.path.join(ROOT, base)
        if os.path.isdir(d):
            for fn in sorted(os.listdir(d)):
                if fn.lower().endswith('.md'): yield base + '/' + fn
for rel in doc_paths():
    t = rd(os.path.join(ROOT, rel)); m = re.search(r'^#\s+(.+)$', t, re.M)
    title = clean_md(m.group(1)) if m else rel
    stemname = os.path.basename(rel).upper()
    area = next((a for k, a in DOC_AREA.items() if stemname.startswith(k)), 'team')
    cnt, cs = commits_for([rel], 2)
    nid = 'doc.' + rel.replace('/', '.').replace('.md', '')
    DOCS[rel] = nid
    add(nid, 'doc', title, area, 'area.' + area, status='in-game', files=[rel], summary=clip(re.sub(r'^#.*$', '', t, flags=re.M), 220), last_change=cs[0]['date'] if cs else None,
        commits=cs, loc=t.count('\n'), links=[dict(title=rel, url=blob(rel))])
for s in SYSTEMS:
    for d in s.get('docs') or []:
        if d in DOCS: edge(s['id'], DOCS[d], 'documents')

# ---------------------------------------------------------------- features (docs/FEATURES.md)
def fm(sec):
    mods = set(m.group(1) for m in re.finditer(r'\b([A-Z][A-Za-z0-9]{2,})\.(?:h|cpp|inl)\b', sec)) | set(m.group(1) for m in re.finditer(r'`([A-Z][A-Za-z0-9]{3,})`', sec))
    return [m for m in mods if m in stems]
feat_ids = {}
ft = rd(os.path.join(ROOT, 'docs', 'FEATURES.md')) if os.path.exists(os.path.join(ROOT, 'docs', 'FEATURES.md')) else ''
parts = re.split(r'^## (\d+)\.\s+(.+)$', ft, flags=re.M)
KW_AREA = [('вод', 'physics'), ('цеп', 'physics'), ('материал', 'physics'), ('разруш', 'physics'), ('грязь', 'physics'), ('муравейник', 'meta'), ('боец', 'meta'), ('интерфейс', 'ui'), ('звук', 'audio'),
           ('человек', 'characters'), ('кошк', 'creatures'), ('карта', 'world'), ('уровень', 'world'), ('квартир', 'world'), ('лут', 'items'), ('еда', 'items'), ('админ', 'tech'), ('сеть', 'tech'), ('съёмк', 'marketing'), ('учебк', 'gameplay')]
for i in range(1, len(parts) - 2, 3):
    num, title, body = parts[i], parts[i + 1].strip(), parts[i + 2]
    date = (re.search(r'\((\d{1,2}\.\d{1,2}\.\d{4})', title) or [None, None])[1]
    br = (re.search(r'ветк[аи]\s+`([^`]+)`', title) or [None, None])[1]
    clean_title = clean_md(re.sub(r'\s*\([^()]*\d{2}\.\d{2}\.\d{4}[^()]*\)\s*$', '', title))
    mods = fm(body)
    cnt = collections.Counter(STEM_SYS[m] for m in mods)
    sid = cnt.most_common(1)[0][0] if cnt else None
    if sid is None:
        sid = next((s['id'] for k, a in KW_AREA for s in SYSTEMS if k in title.lower() and s['area'] == a), None)
    area = SYS_AREA.get(sid, next((a for k, a in KW_AREA if k in title.lower()), 'team'))
    first = next((clean_md(l.lstrip('-* ').strip()) for l in body.split('\n') if l.strip() and not l.startswith('#') and not l.startswith('|')), '')
    issues = [clip(clean_md(l.lstrip('-* ')), 200) for l in body.split('\n') if ('⚠' in l or 'не проверял' in l.lower() or 'не сделано' in l.lower() or 'не влита' in l.lower())][:5]
    nid = f'feat.{int(num):02d}'; feat_ids[int(num)] = nid
    status = 'in-game'
    if re.search(r'не влита', title): status = 'in-progress'
    add(nid, 'system', f'§{num}. {clean_title}', area, sid or 'area.' + area, status=status, summary=clip(first, 300), owner='Claude', date=date, branch=br, files=['docs/FEATURES.md'],
        known_issues=issues, tags=['feature'], links=[dict(title='docs/FEATURES.md', url=blob('docs/FEATURES.md'))])
    for m in mods[:12]: edge(nid, 'file.' + m, 'uses')
    for d in set(re.findall(r'docs/[A-Za-z0-9_/]+\.md', body)):
        if d in DOCS: edge(nid, DOCS[d], 'documents')

# ---------------------------------------------------------------- STATUS.md -> builds + known issues
st = rd(os.path.join(ROOT, 'docs', 'STATUS.md')) if os.path.exists(os.path.join(ROOT, 'docs', 'STATUS.md')) else ''
sp = re.split(r'^## (.+)$', st, flags=re.M)
nb = 0
for i in range(1, len(sp) - 1, 2):
    title, body = sp[i].strip(), sp[i + 1]
    if title.startswith('Одной строкой'): continue
    nb += 1; nid = f'status.{nb:02d}'
    date = (re.search(r'(\d{1,2}\.\d{1,2}\.\d{4})', title) or [None, None])[1]
    br = (re.search(r'ветк[аи]\s+`([^`]+)`', title) or [None, None])[1]
    mods = fm(body); cnt = collections.Counter(STEM_SYS[m] for m in mods); sid = cnt.most_common(1)[0][0] if cnt else None
    add(nid, 'build', clean_md(re.sub(r'\s*\(.*$', '', title)), 'tech', 'area.tech', status='in-game' if 'не влита' not in title else 'in-progress', date=date, branch=br,
        summary=clip(clean_md(' '.join(l for l in body.split('\n') if l.startswith('**Проверено'))), 260), files=['docs/STATUS.md'])
    if sid: edge(nid, sid, 'documents')
    k = 0
    for l in body.split('\n'):
        m = re.match(r'\*\*(Не проверено|Не сделано(?: / известное)?|Известное)[^*]*\*\*:?\s*(.*)', l)
        if m and m.group(2).strip():
            k += 1; bid = f'bug.{nb:02d}.{k}'
            add(bid, 'bug', f'{m.group(1)}: {clip(clean_md(m.group(2)), 70)}', SYS_AREA.get(sid, 'tech'), sid or nid, status='open', summary=clip(clean_md(m.group(2)), 600), date=date, branch=br, files=['docs/STATUS.md'])
            edge(bid, nid, 'part-of')

# ---------------------------------------------------------------- GitHub releases
if '--no-gh' not in sys.argv:
    j = sh(['gh', 'release', 'list', '-R', REPO, '-L', '30', '--json', 'name,tagName,publishedAt,isPrerelease'])
    try:
        for r in json.loads(j):
            add('rel.' + r['tagName'], 'build', r['name'], 'tech', 'area.tech', status='in-game', date=r['publishedAt'][:10], tag=r['tagName'],
                links=[dict(title='release', url=f'https://github.com/{REPO}/releases/tag/{r["tagName"]}')], tags=['release'])
    except Exception: pass

# ---------------------------------------------------------------- level districts + chains
lv = os.path.join(ROOT, 'docs', 'level', 'LEVEL_V3.md')
if os.path.exists(lv):
    t = rd(lv); m = re.search(r'^## 4\..*?$(.*?)^## 5\.', t, re.S | re.M)
    if m:
        for h, body in re.findall(r'^### (.+?)\n(.*?)(?=^### |\Z)', m.group(1), re.S | re.M):
            first = next((clean_md(l) for l in body.split('\n') if l.strip() and not l.startswith('|')), '')
            add('district.' + re.sub(r'\W+', '_', h.strip().lower()), 'system', f'Район: {h.strip()}', 'world', 'sys.level', status='in-game', summary=clip(first, 300), tags=['district'], files=['docs/level/LEVEL_V3.md'])
    m5 = re.search(r'^## 5\..*?$(.*?)^## 6\.', t, re.S | re.M)
    if m5:
        for h, body in re.findall(r'^### (.+?)\n(.*?)(?=^### |\Z)', m5.group(1), re.S | re.M):
            first = next((clean_md(l) for l in body.split('\n') if l.strip() and not l.startswith('|')), '')
            add('inter.' + re.sub(r'\W+', '_', h.strip().lower()), 'system', f'Интерактив: {h.strip()}', 'world', 'sys.interactive', status='in-game', summary=clip(first, 300), tags=['interactive'], files=['docs/level/LEVEL_V3.md'])
mt = os.path.join(ROOT, 'docs', 'MATERIALS.md')
if os.path.exists(mt):
    t = rd(mt); chains = {}
    for m in re.finditer(r'^\|\s*\**(C\d+)\**\s*\|(.*)\|\s*$', t, re.M):
        cid = m.group(1); cells = [clean_md(c) for c in m.group(2).split('|')]
        c = chains.setdefault(cid, dict(cells=None, checks=[]))
        if len(cells) >= 4 and c['cells'] is None: c['cells'] = cells
        elif len(cells) <= 3 and c['cells'] is not None: c['checks'].append(' / '.join(cells[1:]))
        elif c['cells'] is None: c['cells'] = cells
    for cid, c in chains.items():
        n = int(cid[1:]); built = n <= 22; cl = c['cells'] or ['']
        summ = cl[0] + ('. Действие: ' + cl[1] if len(cl) > 1 and built else '') + ('. Шаги: ' + cl[2] if len(cl) > 2 and built else '') + ('. Развязка: ' + cl[3] if len(cl) > 3 and built else '')
        if not built and len(cl) > 1: summ += '. Нужно: ' + cl[1]
        add('chain.' + cid, 'system', f'{cid}: {clip(cl[0], 80)}', 'physics', 'sys.chains', status='in-game' if built else 'designed', summary=clip(summ, 520),
            tags=['chain'], files=['docs/MATERIALS.md'], note=clip('Проверено: ' + '; '.join(c['checks']), 300) if c['checks'] else None)
    
# ---------------------------------------------------------------- Content scan + source refs
CONTENT = os.path.join(ROOT, 'Content')
content_dirs = {}; mesh_files = {}   # name -> relpath
for top in sorted(os.listdir(CONTENT)) if os.path.isdir(CONTENT) else []:
    p = os.path.join(CONTENT, top)
    if not os.path.isdir(p): continue
    cnt = 0; kinds = collections.Counter()
    for dp, dn, fn in os.walk(p):
        for f in fn:
            if f.endswith('.uasset') or f.endswith('.umap'):
                cnt += 1; base = f.rsplit('.', 1)[0]; pre = base.split('_')[0]
                kinds[pre] += 1
                if pre in ('SM', 'SK'): mesh_files[base] = os.path.relpath(os.path.join(dp, f), ROOT).replace('\\', '/')
    content_dirs[top] = (cnt, kinds)
ALLSRC = {}
for fn in os.listdir(SRC):
    if fn.endswith(('.cpp', '.h', '.inl')): ALLSRC[fn] = rd(os.path.join(SRC, fn))
def src_refs(name):
    return sorted(fn for fn, t in ALLSRC.items() if name in t)
CONTENT_AREA = {'Ant': 'characters', 'Audio': 'audio', 'Sounds': 'audio', 'SoundsReal': 'audio', 'Fluid': 'physics', 'Human': 'characters', 'Npc': 'characters', 'Kitchen': 'world', 'LevelV3': 'world',
                'Life': 'world', 'Living': 'world', 'Map4': 'world', 'Meshy': 'items', 'Mess': 'physics', 'Swarm': 'gameplay', 'Textures': 'world', 'UI': 'ui'}
for top, (cnt, kinds) in content_dirs.items():
    if cnt == 0: continue
    ar = CONTENT_AREA.get(top, 'tech')
    add('content.' + top, 'asset', f'Content/{top}', ar, 'area.' + ar, status='in-game', summary=f'{cnt} ассетов Unreal: ' + ', '.join(f'{k} {v}' for k, v in kinds.most_common(6)), files=[f'Content/{top}'], tags=['content-folder'], count=cnt, source='repo')

# ---------------------------------------------------------------- ASSETS.md (prompts)
assets_md = {}
am = os.path.join(ROOT, 'ASSETS.md')
if os.path.exists(am):
    for ln in rd(am).split('\n'):
        m = re.match(r'^\|\s*`((?:SM|SK)_[A-Za-z0-9_]+)`\s*(✅)?\s*\|\s*(.+?)\s*\|\s*(.*?)\s*\|\s*$', ln)
        if m: assets_md[m.group(1)] = dict(done=bool(m.group(2)), prompt=m.group(3), size=m.group(4))

# ---------------------------------------------------------------- Meshy-imported meshes (Content/Meshy)
meshy_names = [d for d in os.listdir(os.path.join(CONTENT, 'Meshy'))] if os.path.isdir(os.path.join(CONTENT, 'Meshy')) else []
def mesh_asset(name, area, parent, prompt_from=None, **extra):
    refs = src_refs(name)
    st = 'in-game' if refs else 'imported'
    am_ = assets_md.get(name, {})
    prompt = (prompt_from or am_.get('prompt'))
    f = dict(status=st, source=extra.pop('source', 'Meshy'), prompt=prompt, size=am_.get('size'), files=([mesh_files[name]] if name in mesh_files else []) + ['Source/AntKitchen/' + r for r in refs[:4]],
             tags=['mesh'])
    f.update(extra)
    return add('mesh.' + name, 'asset', name, area, parent, **f)
for nm in meshy_names:
    if nm.startswith(('SM_', 'SK_')) and not nm.startswith('SM_Ant_'):
        mesh_asset(nm, 'items', 'content.Meshy')

# ---------------------------------------------------------------- Characters: Ant (flagship) + rigs from *Data.inl
def ant_model():
    ant = add('character.ant', 'character', 'Муравей-боец (рабочий)', 'characters', 'area.characters', status='in-game', owner='Юра', source='Meshy + Blender',
              summary='Игрок и боты. Тело из жёстких кусков, нарезанных из утверждённой модели Meshy (Tools/blender/ant_build.py), без скелета: UAntBodyComponent позирует каждый кадр. 5 кусков тела, 6 ног по 4 сегмента, усики ×2, кислотная железа.',
              files=['Source/AntKitchen/AntBody.cpp', 'Source/AntKitchen/AntBody.h', 'Source/AntKitchen/AntBodyData.inl', 'Content/Ant', 'Content/Meshy'], docs=['ASSETS.md', 'docs/ART_INTEGRATION.md'], tags=['flagship'])
    edge('character.ant', 'sys.ant-body', 'uses'); edge('sys.ant-body', 'character.ant', 'renders')
    prompts = {}
    bib = json.load(open(os.path.join(MAP, 'assets_bible.json'), encoding='utf-8')) if os.path.exists(os.path.join(MAP, 'assets_bible.json')) else []
    worker = next((c for c in bib if c['id'] == 'AK_CHR_ANT_Worker'), None)
    bp = {}
    if worker:
        key = ['head', 'jaw', 'thorax', 'petiole', 'gaster', 'coxa', 'femur', 'tibia', 'tarsus', 'scape', 'flagellum']
        for k, p in zip(key, worker['parts']): bp[k] = p['prompt']
        edge('character.ant', 'asset.AK_CHR_ANT_Worker', 'documents')
    def mesh_state(n):
        refs = src_refs(n); exists = n in mesh_files or n in meshy_names
        return ('in-game' if refs and exists else 'imported' if exists else 'prompt-ready')
    # shared meshes
    shared = [('SM_Ant_Body', 'Тело целиком (эталон стиля, утверждено)', None, 'Meshy', 'approved'),
              ('SM_AntB_Head', 'Голова', 'head', 'Blender (нарезка SM_Ant_Body)', None), ('SM_AntB_Jaw', 'Нижняя челюсть (жвалы)', 'jaw', 'Blender (нарезка)', None),
              ('SM_AntB_Thorax', 'Грудь', 'thorax', 'Blender (нарезка)', None), ('SM_AntB_Petiole', 'Стебелёк', 'petiole', 'Blender (нарезка)', None),
              ('SM_AntB_Gaster', 'Брюшко', 'gaster', 'Blender (нарезка)', None),
              ('SM_Ant_Coxa', 'Тазик (кокса)', 'coxa', 'Meshy', None), ('SM_Ant_Femur', 'Бедро', 'femur', 'Meshy', None),
              ('SM_AntB_Tibia', 'Голень', 'tibia', 'Blender (нарезка)', None), ('SM_AntB_Tarsus', 'Лапка', 'tarsus', 'Blender (нарезка)', None),
              ('SM_Ant_Antenna_Scape', 'Усик: рукоять', 'scape', 'Meshy', None), ('SM_AntB_FlagA', 'Усик: жгутик, половина A', 'flagellum', 'Blender (нарезка)', None),
              ('SM_AntB_FlagB', 'Усик: жгутик, половина B', 'flagellum', 'Blender (нарезка)', None), ('SM_Ant_AcidGland', 'Кислотная железа', None, 'Meshy', None)]
    mid = {}
    for nm, title, key, src, forced in shared:
        am_ = assets_md.get(nm, {})
        prompt = am_.get('prompt') or (bp.get(key) if key else None)
        st = forced or mesh_state(nm)
        if nm == 'SM_Ant_Body': st = 'approved'
        n = add('mesh.' + nm, 'asset', nm, 'characters', 'character.ant', status=st, source=src, prompt=prompt, size=am_.get('size'), summary=title,
                files=([mesh_files[nm]] if nm in mesh_files else []) + ['Source/AntKitchen/AntBody.cpp'], tags=['mesh', 'ant'])
        mid[nm] = n['id']
    # body pieces as parts under body mesh node references
    for nm in ['SM_AntB_Head', 'SM_AntB_Jaw', 'SM_AntB_Thorax', 'SM_AntB_Petiole', 'SM_AntB_Gaster']: edge('mesh.' + nm, 'mesh.SM_Ant_Body', 'part-of')
    # legs
    for side in ('L', 'R'):
        for i in (1, 2, 3):
            lid = f'part.ant.leg.{side}{i}'
            add(lid, 'asset-part', f'Лапа {side}{i} ({["передняя","средняя","задняя"][i-1]}, {"левая" if side=="L" else "правая"})', 'characters', 'character.ant', status='in-game', source='Meshy + Blender', summary='4 сегмента: кокса, бедро, голень, лапка. Код сгибает и ставит ногу на стену или потолок.', files=['Source/AntKitchen/AntBody.cpp'])
            for seg, nm in (('coxa', 'SM_Ant_Coxa'), ('femur', 'SM_Ant_Femur'), ('tibia', 'SM_AntB_Tibia'), ('tarsus', 'SM_AntB_Tarsus')):
                pid = f'part.ant.leg.{side}{i}.{seg}'
                src_node = nodes[mid[nm]]
                add(pid, 'asset-part', {'coxa': 'Кокса', 'femur': 'Бедро', 'tibia': 'Голень', 'tarsus': 'Лапка'}[seg] + f' {side}{i}', 'characters', lid, status=src_node['status'], source=src_node.get('source'),
                    prompt=src_node.get('prompt'), mesh=nm, files=src_node.get('files'), tags=['instance'])
                edge(pid, mid[nm], 'uses')
    for side in ('L', 'R'):
        aid = f'part.ant.antenna.{side}'
        add(aid, 'asset-part', f'Усик {"левый" if side=="L" else "правый"}', 'characters', 'character.ant', status='in-game', source='Meshy + Blender', summary='Рукоять + два куска жгутика.', files=['Source/AntKitchen/AntBody.cpp'])
        for seg, nm, tt in (('scape', 'SM_Ant_Antenna_Scape', 'Рукоять'), ('flagA', 'SM_AntB_FlagA', 'Жгутик A'), ('flagB', 'SM_AntB_FlagB', 'Жгутик B')):
            pid = f'{aid}.{seg}'; src_node = nodes[mid[nm]]
            add(pid, 'asset-part', f'{tt} {side}', 'characters', aid, status=src_node['status'], source=src_node.get('source'), prompt=src_node.get('prompt'), mesh=nm, tags=['instance'])
            edge(pid, mid[nm], 'uses')
    for nm, key in (('Head', 'SM_AntB_Head'), ('Jaw', 'SM_AntB_Jaw'), ('Thorax', 'SM_AntB_Thorax'), ('Petiole', 'SM_AntB_Petiole'), ('Gaster', 'SM_AntB_Gaster')):
        pass
    # castes (not yet made)
    for nm, title in (('SM_Ant_Soldier_Body', 'Каста: солдат (огромная голова, жвалы)'), ('SM_Ant_Queen_Body', 'Каста: королева (длинное брюшко, крылья-огрызки)'), ('SM_Ant_Scout_Body', 'Каста: разведчик (крылья)')):
        am_ = assets_md.get(nm, {})
        add('mesh.' + nm, 'asset', nm, 'characters', 'character.ant', status='prompt-ready' if am_.get('prompt') else 'idea', source='Meshy', prompt=am_.get('prompt'), size=am_.get('size'), summary=title, tags=['caste'])
    if 'AK_CHR_ANT_Castes' in {c['id'] for c in bib}: edge('mesh.SM_Ant_Soldier_Body', 'asset.AK_CHR_ANT_Castes', 'documents')
    add('feat.ant-cosmetics', 'system', 'Облик муравья: цвета панциря, узоры, эмоции, погоны', 'meta', 'sys.fighter', status='in-game', summary='AntCosmetics: цвета и узоры панциря, цвет тропы, знаки отличия звания, эмоции. Выбранное едет по сети.', files=['Source/AntKitchen/AntCosmetics.cpp'], tags=['feature'])
    edge('feat.ant-cosmetics', 'character.ant', 'renders'); edge('feat.ant-cosmetics', 'file.AntCosmetics', 'uses')
ant_model()

# rigs from *Data.inl
RIGS = [('HumanBodyData.inl', 'character.dad', 'Папа (Человек)', 'sys.human', 'characters', 'AK_CHR_HUM'), ('HumanGranData.inl', 'character.gran', 'Бабушка', 'sys.human', 'characters', 'AK_CHR_GRAN'),
        ('HumanKidData.inl', 'character.kid', 'Малыш (4 года)', 'sys.human', 'characters', 'AK_CHR_KID'), ('QuadCatData.inl', 'character.cat', 'Кошка Муся', 'sys.critters', 'creatures', 'AK_CHR_CAT'),
        ('QuadDogData.inl', 'character.dog', 'Бадрик (французский бульдог)', 'sys.critters', 'creatures', 'AK_CHR_DOG')]
for fn, cid, cname, sid, area, bprefix in RIGS:
    p = os.path.join(SRC, fn)
    if not os.path.exists(p): continue
    t = rd(p); segs = re.findall(r'\{\s*TEXT\("([^"]+)"\)\s*,\s*(-?\d+)\s*,', t)
    seen = []; [seen.append(s) for s in segs if s[0] not in [x[0] for x in seen]]
    c = add(cid, 'character', cname, area, sid, status='in-game', owner='Claude', source='Meshy / Blender', summary=f'Тело из {len(seen)} сегментов, сочленённое, без скелета (риг в {fn}).', files=['Source/AntKitchen/' + fn], tags=['rig'])
    edge(cid, 'file.' + fn.rsplit('.', 1)[0], 'uses')
    for nm, par in seen:
        mesh_ok = nm in mesh_files; refs = src_refs(nm)
        parent = cid
        pid = 'part.' + cid.split('.')[1] + '.' + nm
        add(pid, 'asset-part', nm, area, cid, status=('in-game' if mesh_ok else 'prompt-ready'), source='Blender/Meshy', mesh=nm, files=[mesh_files[nm]] if mesh_ok else None, tags=['segment'])

# ---------------------------------------------------------------- Bible (Meshy) cards
ROOM_AREA = {'KIT': 'world', 'LIV': 'world', 'HAL': 'world', 'KID': 'world', 'GRN': 'world', 'OUT': 'world', 'FOOD': 'items', 'TOOL': 'items', 'MIC': 'items', 'CHR': 'characters'}
ROOM_RU = {}
bible = json.load(open(os.path.join(MAP, 'assets_bible.json'), encoding='utf-8')) if os.path.exists(os.path.join(MAP, 'assets_bible.json')) else []
# existing mesh lookup by lowercase tokens
def norm(s): return re.sub(r'[^a-z0-9]', '', s.lower())
mesh_by_norm = collections.defaultdict(list)
for nm in set(list(mesh_files) + meshy_names):
    if nm.startswith(('SM_', 'SK_')):
        mesh_by_norm[norm(nm.split('_', 1)[1].split('_')[0])].append(nm)
ALIAS = {'fridge': 'fridge', 'stove': 'oven', 'toaster': 'toaster', 'bugspray': 'bugspray', 'slipper': 'slipper', 'newspaper': 'rolledNewspaper', 'sugar': 'sugarcube', 'cheese': 'cheesechunk', 'honey': 'honeydrop',
         'gummy': 'gummy', 'goldbear': 'gummybeargold', 'chocolate': 'chocolate', 'switch': 'lightswitch', 'mousetrap': 'mousetrap', 'vent': 'wallvent', 'drawer': 'drawer'}
groups = {}
for c in bible:
    area = ROOM_AREA.get(c['room_code'], 'items')
    gid = f"grp.{c['room_code']}.{c['group_code']}"
    if gid not in groups:
        groups[gid] = add(gid, 'system', f"{c['room']} · {c['group']}", area, 'area.' + area, status='in-progress', summary='Группа карточек библии ассетов Meshy.', tags=['asset-group'], owner='Юра'); edge(gid, 'sys.asset-pipeline', 'documents')
    tail = c['id'].split('_', 3)[3]
    stand = None
    for cand in (ALIAS.get(norm(tail)), tail):
        if cand and mesh_by_norm.get(norm(cand)): stand = mesh_by_norm[norm(cand)]; break
    if not stand:
        for k, v in mesh_by_norm.items():
            if k and (k == norm(tail) or (len(k) > 4 and k in norm(tail))): stand = v; break
    parts = c['parts']; st = 'prompt-ready' if (parts or c['variants']) else 'idea'
    if stand: st = 'imported' if not any(src_refs(n) for n in stand) else 'in-game'
    sf = [mesh_files[n] for n in (stand or []) if n in mesh_files]
    # status override if a file exists in SourceArt/Meshy/<room>/<ID>
    srcdir = os.path.join(ROOT, 'SourceArt', 'Meshy')
    for dp, dn, fn in os.walk(srcdir) if os.path.isdir(srcdir) else []:
        if os.path.basename(dp) == c['id'] and fn: st = 'generated' if st in ('prompt-ready', 'idea') else st; sf.append(os.path.relpath(dp, ROOT).replace('\\', '/'))
    a = add('asset.' + c['id'], 'asset', c['id'], area, gid, status=st, source='Meshy', summary=c['name'], title=c['name'], room=c['room'], room_code=c['room_code'], group_code=c['group_code'], group=c['group'],
            size=c['size'], tris=c['tris'], tex=c['tex'], kind=c['kind'], info=c['info'], variants=c['variants'], stand_in=stand, files=sf, notes=c['notes'], part_count=len(parts), owner='Юра', tags=['bible'])
    for i, p in enumerate(parts):
        add(f"part.{c['id']}.{i+1}", 'asset-part', p['name'], area, a['id'], status=st if st in ('prompt-ready', 'idea', 'generated') else st, source='Meshy', prompt=p['prompt'], tags=['bible'])
    for n in stand or []:
        edge(a['id'], 'mesh.' + n, 'uses') if ('mesh.' + n) in nodes else None
for fn, cid, cname, sid, area, bprefix in RIGS:
    for b in sorted(k for k in list(nodes) if k.startswith('asset.' + bprefix)): edge(cid, b, 'documents')

# ---------------------------------------------------------------- Tools scan
tdir = os.path.join(ROOT, 'Tools')
for dp, dn, fn in os.walk(tdir):
    dn[:] = [d for d in dn if d not in ('__pycache__', 'node_modules', '.git', 'projectmap')]
    for f in fn:
        if f.endswith(('.py', '.ps1', '.bat', '.cmd', '.sh')):
            rel = os.path.relpath(os.path.join(dp, f), ROOT).replace('\\', '/'); t = rd(os.path.join(dp, f))
            m = re.match(r'\s*(?:#!.*\n)?(?:"""(.*?)"""|((?:\s*#[^\n]*\n){1,5}))', t, re.S)
            desc = clip(re.sub(r'^\s*#\s?', '', (m.group(1) or m.group(2) or ''), flags=re.M), 260) if m else None
            cnt, cs = commits_for([rel], 2)
            add('tool.' + rel.replace('/', '.'), 'file', rel[len('Tools/'):], 'tools', 'sys.tools', status='in-game', summary=desc, files=[rel], commits=cs, last_change=cs[0]['date'] if cs else None, loc=t.count('\n'), tags=['tool'])
add('tool.projectmap', 'file', 'projectmap/build_map.py', 'tools', 'sys.projectmap', status='in-game', summary='Генератор карты проекта.', files=['Tools/projectmap/build_map.py'], tags=['tool'])

# ---------------------------------------------------------------- manual people / decisions / marketing
man = ly('manual.yaml', {})
for p in man.get('people', []):
    add(p['id'], 'system', p['name'], p['area'], 'area.' + p['area'], status='in-game', owner=p.get('owner'), summary=p['summary'], tags=['person'])
for d in man.get('decisions', []):
    add(d['id'], 'decision', d['name'], d['area'], 'area.' + d['area'], status='in-game', summary=d['summary'], date=d.get('date'))
for m_ in man.get('marketing', []):
    add(m_['id'], 'system', m_['name'], m_['area'], 'sys.marketing' if m_['area'] == 'marketing' else 'area.' + m_['area'], status=m_.get('status', 'idea'), owner=m_.get('owner'), summary=m_['summary'], tags=['marketing'])
edge('person.denis', 'sys.marketing', 'uses'); edge('person.danila', 'sys.asset-pipeline', 'uses'); edge('person.yura', 'sys.asset-pipeline', 'uses')
edge('dec.meshy-bible', 'sys.asset-pipeline', 'documents'); edge('dec.core-loop', 'sys.carry', 'documents'); edge('dec.progression-3-layers', 'sys.fighter', 'documents'); edge('dec.progression-3-layers', 'sys.colony', 'documents')
edge('dec.real-scale', 'sys.ant-body', 'documents'); edge('dec.destruction-ants-only', 'sys.destruction', 'documents'); edge('dec.run-of-shifts', 'sys.raid', 'documents'); edge('dec.tone', 'sys.audio', 'documents')

# link systems with links[] field
for s in SYSTEMS:
    for l in s.get('links', []) or []: edge(s['id'], l, 'renders')
# systems with no modules but net
edge('sys.net', 'sys.raid', 'uses')

# ---------------------------------------------------------------- drop empty misc, overrides
if not any(n['parent'] == 'sys.misc' for n in nodes.values()): nodes.pop('sys.misc', None)
ov = ly('overrides.yaml', {}) or {}
for nid, f in (ov.get('nodes') or {}).items():
    if nid in nodes: nodes[nid].update({k: v for k, v in f.items() if v is not None}); nodes[nid]['edited'] = True
for n in ov.get('add_nodes') or []:
    add(n['id'], n.get('type', 'system'), n['name'], n.get('area'), n.get('parent') or ('area.' + n['area'] if n.get('area') else None), **{k: v for k, v in n.items() if k not in ('id', 'type', 'name', 'area', 'parent')}); nodes[n['id']]['manual'] = True
for e in ov.get('add_edges') or []: edge(e['from'], e['to'], e.get('kind', 'uses'))
for rid in ov.get('remove') or []: nodes.pop(rid, None)
for n in nodes.values():
    n.setdefault('updated', n.get('last_change') or n.get('date') or None)
    if n.get('files') and n['type'] in ('file', 'doc', 'asset', 'system', 'character', 'asset-part', 'build', 'bug'):
        n['links'] = n.get('links') or [dict(title=f, url=blob(f)) for f in n['files'][:6] if re.search(r'\.\w{1,5}$', f) and not f.startswith('Content/')]
edges[:] = [e for e in edges if e['from'] in nodes and e['to'] in nodes]

# ---------------------------------------------------------------- output
cnt_type = collections.Counter(n['type'] for n in nodes.values()); cnt_area = collections.Counter(n['area'] for n in nodes.values())
meta = dict(generated=datetime.datetime.now().isoformat(timespec='seconds'), head=HEAD, branch=CUR_BRANCH, repo=REPO, blob_base=BLOB, node_count=len(nodes), edge_count=len(edges), types=dict(cnt_type), by_area=dict(cnt_area))
data = dict(meta=meta, nodes=list(nodes.values()), edges=edges, recent=recent[:40], areas=[dict(id=a['id'], name=a['name'], color=a['color']) for a in ap['areas']])
os.makedirs(MAP, exist_ok=True)
with open(os.path.join(MAP, 'map.json'), 'w', encoding='utf-8') as f: json.dump(data, f, ensure_ascii=False, separators=(',', ':'))
# SUMMARY.md
VURL = META.get('viewer_url', ''); VSLUG = META.get('viewer_slug', '')
L = [f'# Карта проекта Ant Kitchen: сводка ({today()}, {HEAD})', '', f'Интерактивная карта: {VURL} (артефакт claude.ai, slug `{VSLUG}`). Правки со страницы лежат в базе артефакта, коллекция `edits`.', '', 'Файл создаётся автоматически: `python Tools/projectmap/build_map.py`. Не правь руками. Ручные данные: `project/map/*.yaml`.', '',
     f'Узлов: {len(nodes)}, связей: {len(edges)}. По типам: ' + ', '.join(f'{k} {v}' for k, v in cnt_type.most_common()), '', '## Области и системы', '']
for a in ap['areas']:
    L.append(f'### {a["name"]}'); L.append(a.get('summary', ''))
    for s in SYSTEMS:
        if s['area'] == a['id'] and s['id'] in nodes:
            n = nodes[s['id']]; L.append(f"- **{n['name']}** [{n.get('status')}] ({n.get('owner') or '-'}): {clip(n.get('summary'), 140)}" + (f" Код: {', '.join(sorted(set(x.split('/')[-1] for x in (n.get('files') or [])[:4])))}" if n.get('files') else ''))
    L.append('')
ac = collections.Counter(n['status'] for n in nodes.values() if n['type'] == 'asset' and 'bible' in (n.get('tags') or []))
L += ['## Ассеты Meshy (библия, 219 карточек)', '', ', '.join(f'{k}: {v}' for k, v in ac.most_common()), '', 'Карточки: `project/map/assets_bible.json`; узлы `asset.AK_*`; части `part.AK_*`.', '',
      '## Где что лежит', '', '- Код: `Source/AntKitchen/*`; документы: `docs/*.md`, `docs/FEATURES.md` (полный список), `docs/STATUS.md` (где мы сейчас); ассеты: `Content/*`, `SourceArt/*`; скрипты: `Tools/*`.',
      '- Карта: `project/map/` (areas/systems/manual/overrides.yaml, assets_bible.json, map.json), генератор `Tools/projectmap/`.', '',
      '## Как обновлять', '', '1. Поменял системы или ассеты: `python Tools/projectmap/build_map.py` (нужен pyyaml).', '2. Ручная правка статуса, владельца, заметки: `project/map/overrides.yaml` (или `python Tools/projectmap/apply_edits.py edits.json` из страницы).',
      '3. Перепубликовать страницу: `publish_files` (slug выше, mode patch): `data/map.json` = `project/map/map.json`; вид страницы лежит в `Tools/projectmap/viewer/index.html` (as `index.html`).',
      '4. Правки Юры со страницы: прочитать коллекцию `edits` артефакта (db_list), сохранить как JSON, `python Tools/projectmap/apply_edits.py edits.json`, затем build_map.py и публикация.', '',
      '## Типы узлов и статусы', '', 'Типы: area, system (системы и фичи, теги person/feature/district/chain), character, asset, asset-part, file, doc, build (релизы и срезы STATUS), bug, decision.',
      'Статусы систем: idea, designed, in-progress, in-game, polished, blocked, deprecated. Статусы ассетов: idea, prompt-ready, generated, imported, in-game, approved.']
with open(os.path.join(MAP, 'SUMMARY.md'), 'w', encoding='utf-8') as f: f.write('\n'.join(L) + '\n')
print(json.dumps(meta, ensure_ascii=False))
