# -*- coding: utf-8 -*-
# Ant Kitchen, level map v3: docs/level/level_map_v3.html — the phone-first layered map of v2 (build_level_map_v2.py) for v3, with
# a seed switcher (the seed is drawn in the page by the same mulberry32 compose() as level_v3_layout.py), a variant switcher per district,
# layers for fasteners, breakables, cloth, hiding places, NPC routine spots and paths, cake spots and the route graph, and the checker's
# verdict for every validated seed. One self-contained file (inline SVG + JS, no external URLs), < 2 MB.
#   python Tools/level/build_level_map_v3.py        (after build_level_v3.py and check_level_v3.py)
# URL hash: #seed=12&threat=2&v=kitchen&ly=geo,fast,cover
import json, os, sys, html
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
try: sys.stdout.reconfigure(encoding='utf-8', errors='replace')
except Exception: pass
import level_v3_layout as L
LV = os.path.join(os.path.dirname(os.path.dirname(HERE)), 'docs', 'level')
D = json.load(open(os.path.join(LV, 'level_v3.json'), encoding='utf-8'))
CH = json.load(open(os.path.join(LV, 'level_v3_check.json'), encoding='utf-8')) if os.path.exists(os.path.join(LV, 'level_v3_check.json')) else None
def r0(v): return int(round(v))
def cobj(o):
    d = {'i': o['id'], 'n': o['ru'], 'm': o.get('mesh') or '', 'c': o['cat'], 'b': [r0(v) for v in o['bb']], 'z': [r0(o['z'][0]), r0(o['z'][1])]}
    for k_in, k_out in (('mat', 'mt'), ('interactive', 'it'), ('v3', 'v'), ('district', 'd'), ('cloth', 'cl'), ('fastener_of', 'fo'), ('state', 'st'), ('collision', 'co')):
        if o.get(k_in): d[k_out] = o[k_in]
    if o.get('fasteners'): d['f'] = [f['id'] for f in o['fasteners']]
    if o.get('contents'): d['ct'] = [p[0] for p in o['contents']['pool']]
    if o.get('break'): d['bk'] = o['break']
    return d
V3 = D['v3']
DATA = {
    'walls': [[r0(v) for v in w['rect']] for w in D['walls'] if not w.get('kind')],
    'fences': [[r0(v) for v in w['rect']] for w in D['walls'] if w.get('kind') == 'fence'],
    'areas': [{'id': a['id'], 'ru': a['ru'], 'rects': a['rects']} for a in D['areas']],
    'districts': [{'id': d['id'], 'ru': d['ru'], 'rects': d['rects']} for d in D['districts']],
    'objs': [cobj(o) for o in D['objects'] if o['area'] in ('kitchen', 'living', 'yard', 'street') and o['cat'] != 'part' or o.get('v3')],
    'hides': [{'r': [r0(v) for v in h['rect']], 'n': h['ru'], 'z': h['z']} for h in D['human']['sees']['never_targets'] if 'rect' in h],
    'variants': {d: [{'id': v['id'], 'ru': v['ru'], 'plays': v['plays'], 'when': v['when'], 'ops': v['ops'], 'objs': [cobj(o) for o in v['objects']], 'rm': v['removed'],
                      'mv': {k: [r0(x) for x in bb] for k, bb in v['moved_bb'].items()}} for v in V3['variants'][d]] for d in V3['district_order']},
    'order': V3['district_order'],
    'nodes': [{'id': n['id'], 'ru': n['ru'], 'p': n['p'][:2], 'role': n['role']} for n in D['graph']['nodes']],
    'edges': [{'a': e['a'], 'b': e['b'], 'k': e['kind'], 'len': e['len_uu']} for e in D['graph']['edges']],
    'routines': {w: [{'id': r['id'], 'ru': r['ru'], 'p': r.get('p'), 'when': r.get('when', []), 's': r.get('seconds')} for r in V3['routines'][w]] for w in V3['routines']},
    'paths': {w: [p['pts'] for p in V3['routine_paths_showcase'][w] if p['pts']] for w in V3['routine_paths_showcase']},
    'cake': {k: {'node': v['node'], 'ru': v['ru'], 'p': (list(v['p']) if v.get('p') else [m['p'] for m in D['loot']['main']['spots'] if m['id'] == k][0][:2])} for k, v in V3['cake_spots'].items()},
    'inside': [{'id': x['id'], 'ru': x['ru'], 'who': x['who'], 'seg': x['segment'], 'when': x['when'], 'eff': x['effect']} for x in V3['inside_people']],
    'chains': [{'id': c['id'], 'ru': c['ru'], 'steps': c['steps']} for c in V3['chains']],
    'interactive': [{'id': i['id'], 'ru': i['ru'], 'obj': i['obj'], 'mat': i['mat'], 'undo': i['undo'], 'res': i['result'], 'f': i['fasteners']} for i in V3['interactive']],
    'seedcfg': {'time': L.SEED['time'], 'tbt': {str(k): v for k, v in L.SEED['time_by_threat'].items()}, 'bt': {str(k): v for k, v in L.SEED.get('by_threat', {}).items()}, 'home': L.SEED['home'], 'doors': L.SEED['doors_random'], 'loose': L.SEED['loose'],
                'scatter': [{'id': z['id'], 'rect': z['rect'], 'z': z.get('z', 0), 'items': z['items'], 'n': list(z['n']), 'needs': z.get('needs'), 'tmax': z.get('threat_max')} for z in L.SEED['scatter']],
                'vw': {d: [{'id': v['id'], 'w': v['weight'], 'when': v['when'], 'cake': [[op[1], op[2]] for op in v['ops'] if op[0] == 'cake']} for v in L.VARIANTS[d]] for d in L.DISTRICT_ORDER},
                'cw': {k: v['w'] for k, v in L.CAKE_SPOTS.items()}},
    'check': {str(x['seed']) + '_' + str(x['threat']): {'ok': x['ok'], 'f': x['fails'], 'cw': x['cover_worst_s'], 'cs': x['cake_s']} for x in (CH['seeds']['list'] if CH else [])},
    'checkv': {x['district'] + x['variant']: {'ok': x['ok'], 'f': x['fails']} for x in (CH['variants'] if CH else [])},
    'summary': ({'n': CH['seeds']['n'], 'passed': CH['seeds']['passed'], 'rate': CH['seeds']['pass_rate'], 'pairs': CH.get('pairs', {}).get('checked'), 'pairs_failed': len(CH.get('pairs', {}).get('failed', [])),
                 'cover': CH['seeds']['cover_worst_s_max'], 'v2': CH['v2_calibration']['cover']} if CH else None),
    'tod': {k: v['ru'] for k, v in V3['time_of_day'].items()},
    'story': V3['story']['ru'],
}
J = json.dumps(DATA, ensure_ascii=False, separators=(',', ':'))
PAGE = r'''<!DOCTYPE html><html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ant Kitchen — карта уровня v3</title><style>
:root{--bg:#f6f3ec;--ink:#222;--mut:#777;--k:#c9b48a;--l:#8fa58a}
*{box-sizing:border-box}body{margin:0;font:14px/1.35 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--ink)}
header{position:sticky;top:0;z-index:5;background:#fff;border-bottom:1px solid #ddd;padding:6px 8px;display:flex;flex-wrap:wrap;gap:6px;align-items:center}
header b{font-size:15px}input,select,button{font:inherit;padding:4px 6px;border:1px solid #bbb;border-radius:6px;background:#fff}
button.on{background:#222;color:#fff}#verdict{font-weight:600}#verdict.ok{color:#18794e}#verdict.bad{color:#c0392b}
#wrap{position:relative}svg{width:100%;height:auto;max-height:78vh;display:block;background:#e9e6dc;touch-action:manipulation}
#panel{padding:8px;display:grid;gap:8px}details{background:#fff;border:1px solid #ddd;border-radius:8px;padding:6px 8px}summary{cursor:pointer;font-weight:600}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:6px}.grid label{display:flex;gap:4px;align-items:center}
#card{position:fixed;left:8px;right:8px;bottom:8px;max-height:40vh;overflow:auto;background:#fff;border:1px solid #bbb;border-radius:10px;padding:8px 10px;box-shadow:0 4px 18px #0003;display:none;z-index:9}
#card h3{margin:0 0 4px;font-size:15px}#card .x{float:right;border:0;font-size:18px}.mut{color:var(--mut)}.tag{display:inline-block;padding:0 6px;border-radius:9px;background:#eee;margin:1px}
.w{fill:#5b5048}.fn{fill:#bbb}.ar-kitchen{fill:#f4eedf}.ar-living{fill:#ece3d3}.ar-yard{fill:#cfe0bb}.ar-street{fill:#dcdcd4}
.o{fill:#b6aa95;stroke:#6d6355;stroke-width:.06}.o.v3{fill:#7fc1b8;stroke:#1f6f66}.o.mv{stroke-dasharray:.4 .2;stroke:#1f6f66;stroke-width:.15}.o.hi{fill:#d8cfbf}.o.var{fill:#f0b45c;stroke:#9a5d00;stroke-width:.12}
.hide{fill:#2e86de33;stroke:#2e86de;stroke-width:.06}.fast{fill:#d35400}.brk{fill:none;stroke-width:.25}.cl{fill:url(#hatch);stroke:#8e44ad;stroke-width:.1}
.ed{stroke:#555;stroke-width:.12;fill:none}.ed.open{stroke:#c0392b}.ed.cover{stroke:#2e86de}.ed.wall{stroke:#7f8c8d;stroke-dasharray:.4 .2}.ed.out{stroke:#27ae60}.ed.closed{stroke:#000;stroke-dasharray:.2 .2;opacity:.6}.ed.route{stroke:#f1c40f;stroke-width:.5;opacity:.85}
.nd{fill:#fff;stroke:#333;stroke-width:.08}.rt{stroke-width:.18;fill:none;opacity:.85}.rt.human{stroke:#c0392b}.rt.grandma{stroke:#8e44ad}.rt.kid{stroke:#e67e22}
.sp{stroke:#fff;stroke-width:.08}.sp.human{fill:#c0392b}.sp.grandma{fill:#8e44ad}.sp.kid{fill:#e67e22}.sp.dog{fill:#a0522d}.sp.cat{fill:#7f8c8d}
.cake{fill:#ff6fa9;stroke:#900;stroke-width:.1}.cake.act{stroke-width:.35;fill:#ff2d7a}.sc{fill:#333}.lb{font-size:1.1px;fill:#333;paint-order:stroke;stroke:#fff;stroke-width:.25}.dl{font-size:1.6px;fill:#555;font-weight:700;opacity:.55}
</style></head><body>
<header><b>Ant Kitchen · уровень v3</b>
<label>сид <input id="seed" type="number" value="0" min="0" style="width:6em"></label>
<label>угроза <select id="threat"><option>1</option><option>2</option><option>3</option><option>4</option></select></label>
<button id="rnd">случайный</button><span id="tod" class="tag"></span><span id="home" class="mut"></span><span id="verdict"></span></header>
<div id="wrap"><svg id="map" viewBox="-160 -64 370 190" xmlns="http://www.w3.org/2000/svg"><defs><pattern id="hatch" width=".6" height=".6" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><rect width=".6" height=".6" fill="#8e44ad22"/><line x1="0" y1="0" x2="0" y2=".6" stroke="#8e44ad" stroke-width=".15"/></pattern></defs><g id="g"></g></svg></div>
<div id="panel">
<div><button data-v="all">всё</button> <button data-v="kitchen">кухня</button> <button data-v="living">гостиная</button> <button data-v="yard">двор</button> <span class="mut">Тап по предмету — карточка.</span></div>
<details open><summary>Слои</summary><div class="grid" id="layers"></div></details>
<details><summary>Варианты районов (сид задаёт; можно переключить руками)</summary><div class="grid" id="vars"></div><p id="plays" class="mut"></p></details>
<details><summary>Цепочки, внутрь людей, крепёж</summary><div id="lists"></div></details>
<details><summary>Проверка сидов</summary><div id="sum"></div></details>
<details><summary>История квартиры</summary><p id="story"></p></details>
</div><div id="card"><button class="x" onclick="this.parentNode.style.display='none'">×</button><div id="cardb"></div></div>
<script>
const D=__DATA__;
const X=x=>x/100,Y=y=>-y/100,E=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const LAYERS=[['geo','Стены и мебель',1],['var','Предметы вариантов',1],['fast','Крепёж (шурупы, петли, кольца)',1],['brk','Бьётся / разрушается',0],['cloth','Ткань',1],['hide','Укрытия (Человек не видит)',0],
['graph','Граф маршрутов',0],['cake','Места торта и маршруты',1],['rout','Распорядок: точки',1],['path','Распорядок: пути (витрина)',0],['scatter','Россыпь сида',1],['lbl','Подписи',0],['dist','Районы',0]];
const ON={};LAYERS.forEach(l=>ON[l[0]]=!!l[2]);
const VIEWS={all:[-160,-64,370,190],kitchen:[-76,-55,154,108],living:[67,-19,140,112],yard:[-98,49,306,76]};
// ---- mulberry32 compose(), the same as level_v3_layout.compose
function Rng(seed){this.a=(Math.imul(seed,2654435761|0)+0x9E3779B9)>>>0}
Rng.prototype.next=function(){this.a=(this.a+0x6D2B79F5)>>>0;let t=this.a;t=Math.imul(t^(t>>>15),t|1)>>>0;t=(t^((t+(Math.imul(t^(t>>>7),t|61)>>>0))>>>0))>>>0;return ((t^(t>>>14))>>>0)/4294967296};
Rng.prototype.pick=function(it,w){let tot=w.reduce((a,b)=>a+b,0);if(tot<=0)return it[0];let r=this.next()*tot;for(let i=0;i<it.length;i++){r-=w[i];if(r<0)return it[i]}return it[it.length-1]};
Rng.prototype.int=function(a,b){return a+Math.floor(this.next()*(b-a+1))};
function compose(seed,threat){const C=D.seedcfg,r=new Rng(seed);const tw=C.tbt[String(threat)]||C.time;const ts=Object.keys(tw);let time=r.pick(ts,ts.map(k=>tw[k]));
 const home={human:true};['grandma','kid','dog','cat'].forEach(k=>home[k]=r.next()<C.home[k]);if(threat>=4)home.kid=true;
 if(['grandma','kid','dog','cat'].filter(k=>home[k]).length<2){home.dog=true;home.kid=true}
 const bt=(C.bt||{})[String(threat)];if(bt){time=bt.time||time;Object.keys(home).forEach(k=>delete home[k]);home.human=true;Object.assign(home,bt.home)}
 const variants={};D.order.forEach(d=>{const vs=C.vw[d];const ws=vs.map(v=>{let w=v.w;if(v.when&&v.when.indexOf(time)<0)w=0;if(d=='babushka'&&v.id=='B'&&!home.grandma)w=0;if(d=='play'&&!home.kid)w=(v.id=='C'?w*3:(v.id=='B'?0:w));return w});variants[d]=r.pick(vs.map(v=>v.id),ws)});
 const pool=C.doors.pool,doors={};const nd=r.int(C.doors.n[0],C.doors.n[1]);for(let i=0;i<nd;i++)doors[pool[Math.floor(r.next()*pool.length)]]=C.doors.state;
 const lp=C.loose.pool,ls=new Set();const nl=r.int(C.loose.n[0],C.loose.n[1]);for(let i=0;i<nl;i++)ls.add(lp[Math.floor(r.next()*lp.length)]);
 const spots=Object.assign({},C.cw);D.order.forEach(d=>{const v=C.vw[d].find(v=>v.id==variants[d]);v.cake.forEach(c=>{spots[c[0]]=spots[c[0]]?((spots[c[0]]||0.25)*c[1]):c[1]*0.25})});
 const ks=Object.keys(spots).sort();const cake=r.pick(ks,ks.map(k=>spots[k]));
 const scatter=[];C.scatter.forEach(z=>{if(z.needs&&!home[z.needs])return;if(z.tmax&&threat>z.tmax)return;const n=r.int(z.n[0],z.n[1]);for(let i=0;i<n;i++){const item=z.items[Math.floor(r.next()*z.items.length)];
  if(!z.rect){scatter.push({zone:z.id,item:item});continue}const x=z.rect[0]+r.next()*(z.rect[2]-z.rect[0]),y=z.rect[1]+r.next()*(z.rect[3]-z.rect[1]);scatter.push({zone:z.id,item:item,p:[Math.round(x),Math.round(y)],yaw:Math.round(r.next()*360-180)})}});
 return {seed,threat,time,home,variants,doors,loose:[...ls].sort(),cake,scatter}}
// ---- state
let S={comp:null,vars:{}};
function rect(b,cls,id,extra){return `<rect class="${cls}" x="${X(b[0])}" y="${Y(b[3])}" width="${(b[2]-b[0])/100}" height="${(b[3]-b[1])/100}"${id?` data-i="${id}"`:''}${extra||''}/>`}
const OB={};D.objs.forEach(o=>OB[o.i]=o);Object.values(D.variants).forEach(vs=>vs.forEach(v=>v.objs.forEach(o=>OB[o.i]=o)));
function draw(){const L={geo:[],var:[],fast:[],brk:[],cloth:[],hide:[],graph:[],cake:[],rout:[],path:[],scatter:[],lbl:[],dist:[]};
 D.areas.forEach(a=>a.rects.forEach(r=>L.geo.push(rect(r,'ar-'+a.id))));
 const rm=new Set(),mv={},act=[];D.order.forEach(d=>{const v=D.variants[d].find(v=>v.id==S.vars[d]);v.rm.forEach(i=>rm.add(i));Object.assign(mv,v.mv);v.objs.forEach(o=>act.push(o))});
 const BRK={glass:'#16a085',ceramic:'#2980b9',terracotta:'#d35400',pet:'#8e44ad',latex:'#e84393',cardboard:'#a67c52',paper:'#95a5a6',cloth:'#8e44ad',yarn:'#c0392b'};
 const all=D.objs.filter(o=>!rm.has(o.i)).map(o=>mv[o.i]?Object.assign({},o,{b:mv[o.i],v:'moved'}):o).concat(act.map(o=>Object.assign({},o,{var:1})));
 all.sort((a,b)=>a.z[1]-b.z[1]);
 all.forEach(o=>{if(o.m=='SM_Y_Tree'){L.geo.push(rect(o.b,'',o.i,' fill="none" stroke="#6a8f4e" stroke-width=".12" stroke-dasharray=".6 .3"'));return}
  if(o.c=='fastener'){L.fast.push(`<circle class="fast" cx="${X((o.b[0]+o.b[2])/2)}" cy="${Y((o.b[1]+o.b[3])/2)}" r=".45" data-i="${o.i}"/>`);return}
  if(o.cl){L.cloth.push(rect(o.b,'cl',o.i));return}if(o.co=='ants_only'&&o.c!='conn'&&!o.var){return}
  const cls='o'+(o.var?' var':(o.v=='new'?' v3':''))+(o.v=='moved'?' mv':'')+(o.z[0]>1500?' hi':'');(o.var?L.var:L.geo).push(rect(o.b,cls,o.i));
  if(o.mt&&BRK[o.mt])L.brk.push(rect(o.b,'brk',o.i,` stroke="${BRK[o.mt]}"`));
  if(o.z[1]>900&&(o.b[2]-o.b[0])*(o.b[3]-o.b[1])>600000&&o.c=='furn')L.lbl.push(`<text class="lb" x="${X(o.b[0])+.2}" y="${Y((o.b[1]+o.b[3])/2)}">${E(o.n.split(' (')[0].slice(0,22))}</text>`)});
 D.walls.forEach(w=>L.geo.push(rect(w,'w')));D.fences.forEach(w=>L.geo.push(rect(w,'fn')));
 D.hides.forEach(h=>L.hide.push(rect(h.r,'hide',null,` data-h="${E(h.n)}"`)));D.order.forEach(d=>{const v=D.variants[d].find(v=>v.id==S.vars[d]);v.ops.forEach(op=>{if(op[0]=='hide'&&op[1].rect)L.hide.push(rect(op[1].rect,'hide',null,` data-h="${E(op[1].ru)}"`))})});
 D.districts.forEach(d=>d.rects.forEach((r,k)=>{if(k==0)L.dist.push(`<text class="dl" x="${X(r[0])+.5}" y="${Y(r[3])+2}">${E(d.ru)} · ${S.vars[d.id]||''}</text>`);L.dist.push(rect(r,'',null,' fill="none" stroke="#999" stroke-width=".08" stroke-dasharray=".5 .3"'))}));
 const N={};D.nodes.forEach(n=>N[n.id]=n);const est={};D.order.forEach(d=>{const v=D.variants[d].find(v=>v.id==S.vars[d]);v.ops.forEach(op=>{if(op[0]=='edge')est[[op[1],op[2]].sort().join('|')]=op[3]})});
 D.edges.forEach(e=>{const a=N[e.a].p,b=N[e.b].p;const s=est[[e.a,e.b].sort().join('|')];const k=s=='closed'?'closed':(s=='cover'||s=='open'?s:e.k);L.graph.push(`<line class="ed ${k}" x1="${X(a[0])}" y1="${Y(a[1])}" x2="${X(b[0])}" y2="${Y(b[1])}"><title>${E(N[e.a].ru)} — ${E(N[e.b].ru)} ${s||''}</title></line>`)});
 D.nodes.forEach(n=>L.graph.push(`<circle class="nd" cx="${X(n.p[0])}" cy="${Y(n.p[1])}" r=".55"><title>${E(n.ru)}</title></circle>`));
 const c=S.comp,key=c?c.seed+'_'+c.threat:'',ck=D.check[key];
 Object.entries(D.cake).forEach(([k,v])=>{const actv=c&&c.cake==k;L.cake.push(`<circle class="cake${actv?' act':''}" cx="${X(v.p[0])}" cy="${Y(v.p[1])}" r="${actv?1.2:.8}"><title>${k}: ${E(v.ru)}</title></circle>`);
  if(actv&&ck&&ck.cs)L.cake.push(`<text class="lb" x="${X(v.p[0])+1.4}" y="${Y(v.p[1])}">торт ${k}: ${ck.cs.join(' с / ')} с</text>`)});
 ['human','grandma','kid','dog','cat'].forEach(w=>{if(c&&!c.home[w])return;(D.routines[w]||[]).forEach(r=>{if(!r.p)return;if(c&&r.when.length&&r.when.indexOf(c.time)<0)return;
  L.rout.push(`<circle class="sp ${w}" cx="${X(r.p[0])}" cy="${Y(r.p[1])}" r=".7" data-r="${E(w+': '+r.ru+' · '+(r.s||'')+' с')}"/>`)})});
 Object.entries(D.paths).forEach(([w,ps])=>{if(c&&!c.home[w])return;ps.forEach(p=>L.path.push(`<polyline class="rt ${w}" points="${p.map(q=>X(q[0])+','+Y(q[1])).join(' ')}"/>`))});
 if(c)c.scatter.forEach(s=>{if(s.p)L.scatter.push(`<circle class="sc" cx="${X(s.p[0])}" cy="${Y(s.p[1])}" r=".22"><title>${E(s.item)}</title></circle>`)});
 const order=['geo','dist','hide','cloth','var','brk','graph','path','rout','fast','cake','scatter','lbl'];
 document.getElementById('g').innerHTML=order.map(k=>ON[k]?`<g>${L[k].join('')}</g>`:'').join('');
 const vd=document.getElementById('verdict');if(ck){vd.className=ck.ok?'ok':'bad';vd.textContent=ck.ok?'✓ сид прошёл проверку':'✗ '+ck.f.slice(0,2).join('; ')}
 else{const allA=D.order.every(d=>c&&S.vars[d]==c.variants[d]);vd.className='mut';vd.textContent=allA?'сид не входит в проверенные (0–'+((D.summary||{}).n-1)+' по угрозам 1–4 по кругу)':'варианты изменены вручную'}}
function setSeed(){const s=parseInt(document.getElementById('seed').value)||0,t=parseInt(document.getElementById('threat').value);S.comp=compose(s,t);S.vars=Object.assign({},S.comp.variants);
 document.getElementById('tod').textContent=D.tod[S.comp.time];document.getElementById('home').textContent='дома: '+Object.keys(S.comp.home).filter(k=>S.comp.home[k]).map(k=>({human:'Человек',grandma:'бабушка',kid:'малыш',dog:'Бадрик',cat:'кошка'})[k]).join(', ')+' · торт '+S.comp.cake+' · шатается: '+S.comp.loose.join(', ');
 buildVars();draw();location.hash='seed='+s+'&threat='+t}
function buildVars(){const box=document.getElementById('vars');box.innerHTML=D.order.map(d=>{const vs=D.variants[d];const nm=D.districts.find(x=>x.id==d).ru;
 return `<label>${E(nm)} <select data-d="${d}">${vs.map(v=>`<option value="${v.id}"${S.vars[d]==v.id?' selected':''}>${v.id} · ${E(v.ru)}</option>`).join('')}</select></label>`}).join('');
 box.querySelectorAll('select').forEach(s=>s.onchange=()=>{S.vars[s.dataset.d]=s.value;const v=D.variants[s.dataset.d].find(v=>v.id==s.value);const cv=D.checkv[s.dataset.d+s.value];
 document.getElementById('plays').innerHTML='<b>'+E(v.ru)+'</b>: '+E(v.plays)+(cv?(cv.ok?' <span style="color:#18794e">(вариант проверен: ок)</span>':' ✗ '+E(cv.f.join('; '))):'');draw()})}
document.getElementById('layers').innerHTML=LAYERS.map(l=>`<label><input type="checkbox" data-l="${l[0]}"${ON[l[0]]?' checked':''}>${l[1]}</label>`).join('');
document.querySelectorAll('#layers input').forEach(i=>i.onchange=()=>{ON[i.dataset.l]=i.checked;draw()});
document.querySelectorAll('[data-v]').forEach(b=>b.onclick=()=>{document.getElementById('map').setAttribute('viewBox',VIEWS[b.dataset.v].join(' '))});
document.getElementById('seed').onchange=setSeed;document.getElementById('threat').onchange=setSeed;
document.getElementById('rnd').onclick=()=>{document.getElementById('seed').value=Math.floor(Math.random()*100000);setSeed()};
document.getElementById('map').addEventListener('click',ev=>{const t=ev.target;let h='';
 if(t.dataset.i){const o=OB[t.dataset.i];if(!o)return;const it=D.interactive.find(x=>x.obj==o.i);
  h=`<h3>${E(o.n)}</h3><div class="mut">${E(o.i)} · ${E(o.m)} · z ${o.z[0]}–${o.z[1]}</div>`+(o.mt?`<div>материал: <span class="tag">${E(o.mt)}</span> ${o.bk?E(o.bk):''}</div>`:'')+(o.f?`<div>крепёж: ${o.f.map(E).join(', ')}</div>`:'')+(o.fo?`<div>держит: ${o.fo.map(E).join(', ')}</div>`:'')+(o.ct?`<div>внутри: ${o.ct.map(E).join(', ')}</div>`:'')+(o.cl?'<div>ткань (симуляция)</div>':'')+(o.d?`<div>вариант района: ${E(o.d)}</div>`:'')+(it?`<div><b>муравьи:</b> ${E(it.undo)}<br><b>итог:</b> ${E(it.res)}</div>`:'')}
 else if(t.dataset.h)h='<h3>Укрытие</h3>'+E(t.dataset.h);else if(t.dataset.r)h='<h3>Распорядок</h3>'+E(t.dataset.r);
 if(h){document.getElementById('cardb').innerHTML=h;document.getElementById('card').style.display='block'}});
document.getElementById('lists').innerHTML='<h4>Цепочки</h4>'+D.chains.map(c=>`<p><b>${c.id}. ${E(c.ru)}</b><br>${c.steps.map(E).join(' → ')}</p>`).join('')+'<h4>Внутрь людей</h4>'+D.inside.map(x=>`<p><b>${E(x.ru)}</b> (${x.who.join(', ')}; ${E(x.seg)}) — ${E(x.when)}. ${E(x.eff)}</p>`).join('')+'<h4>Разрушаемое и крепёж</h4>'+D.interactive.map(i=>`<p><b>${E(i.ru)}</b> [${E(i.mat)}]: ${E(i.undo)} → ${E(i.res)}</p>`).join('');
const sm=D.summary;document.getElementById('sum').innerHTML=sm?`<p>Сидов проверено: ${sm.n}, прошли ${sm.passed} (${sm.rate} %). Пар вариантов проверено: ${sm.pairs||'—'}, не прошли ${sm.pairs_failed}. Худшее «до укрытия» по всем сидам: ${sm.cover} с (правило 4 с). Калибровка на v2: кухня ${sm.v2.kitchen.worst_s} с, гостиная ${sm.v2.living.worst_s} с (аудит v2: 4,0 и 3,9).</p>`:'нет level_v3_check.json';
document.getElementById('story').textContent=D.story;
const hp=new URLSearchParams(location.hash.slice(1));if(hp.get('seed'))document.getElementById('seed').value=hp.get('seed');if(hp.get('threat'))document.getElementById('threat').value=hp.get('threat');
if(hp.get('ly')){LAYERS.forEach(l=>ON[l[0]]=false);hp.get('ly').split(',').forEach(k=>ON[k]=true);document.querySelectorAll('#layers input').forEach(i=>i.checked=ON[i.dataset.l])}
setSeed();if(hp.get('v')&&VIEWS[hp.get('v')])document.getElementById('map').setAttribute('viewBox',VIEWS[hp.get('v')].join(' '));
</script></body></html>'''
html_text = PAGE.replace('__DATA__', J)
with open(os.path.join(LV, 'level_map_v3.html'), 'w', encoding='utf-8') as f: f.write(html_text)
print('wrote docs/level/level_map_v3.html: %.0f KB' % (len(html_text.encode('utf-8')) / 1024.0))
