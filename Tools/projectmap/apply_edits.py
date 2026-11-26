#!/usr/bin/env python3
"""Merge edits made in the interactive map page into project/map/overrides.yaml.
Usage: python Tools/projectmap/apply_edits.py edits.json
edits.json = {"<node id>": {"status":..., "owner":..., "note":..., "tasks":[{"text":..,"done":..}], "updated":...}, ...}
(from the page's "Копировать правки" button, or Claude reading the artifact db collection `edits`).
"""
import sys, os, json, yaml
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
p = os.path.join(ROOT, 'project', 'map', 'overrides.yaml')
ov = yaml.safe_load(open(p, encoding='utf-8')) or {}
ov.setdefault('nodes', {}); ov['nodes'] = ov['nodes'] or {}
src = json.load(open(sys.argv[1], encoding='utf-8'))
if isinstance(src, list): src = {e['id']: e for e in src}
n = 0
for nid, e in src.items():
    e = {k: v for k, v in e.items() if k in ('status', 'owner', 'note', 'tasks', 'summary') and v not in (None, '', [])}
    if e: ov['nodes'].setdefault(nid, {}).update(e); n += 1
hdr = ''.join(l for l in open(p, encoding='utf-8') if l.startswith('#'))
with open(p, 'w', encoding='utf-8') as f:
    f.write(hdr); yaml.safe_dump(ov, f, allow_unicode=True, sort_keys=False, width=140)
print('merged', n, 'nodes; now run build_map.py')
