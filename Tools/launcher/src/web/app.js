'use strict';
const $ = (s, r = document) => r.querySelector(s), $$ = (s, r = document) => [...r.querySelectorAll(s)];
let S = null, MOCK = false, lastHash = '', newsHash = '', setHash = '', dismissedUpd = false;
const Q = new URLSearchParams(location.search);

// ---------- bridge (+ mock for browser preview)
function mockState() {
  const m = Q.get('mock') || 'installed';
  const rel = (tag, name, body, d) => ({ tag, name, body, published: d, url: 'https://github.com/kisa134/antgame/releases/tag/' + tag, size: 1094456866, has_zip: true });
  const st = {
    version: '1.0.0', auto_platform: 'windows', platform: m === 'mac' ? 'mac' : 'auto', root: 'C:\\Users\\Юра\\AppData\\Local\\AntKitchen',
    installed: m === 'notinstalled' ? '' : 'playtest-2026-11-24-v14', previous: '', installed_tags: [], rel_status: 'ok', rel_error: '',
    target: { tag: m === 'update' ? 'playtest-2026-11-24-v14.1' : 'playtest-2026-11-24-v14', name: 'Playtest v14', size: 1094456866, published: '2026-11-24T11:57:12Z' },
    update_available: m === 'update', channel: 'latest', pinned_tag: '', steam: m === 'nosteam' ? 'not_running' : 'ok', prereq: 'ok', running: false, launching: false,
    free: 214e9, task: null, opts: { tutorial: false, admin: false, windowed: false, args: '' }, selfupdate: { available: m === 'selfupd', version: '1.1.0' }, frozen: true, notice: '', chat_url: '',
    releases: [
      rel('playtest-2026-11-24-v14.1', 'Playtest v14.1: машины ездят снаружи', '## Что нового\n- Уличные машины едут вдоль стены дома, а не сквозь квартиру\n- Исправлен лог дороги\n\n**Важно:** у всех должна быть одна версия.', '2026-11-24T16:40:00Z'),
      rel('playtest-2026-11-24-v14', 'Playtest v14: карта v4, муравейник, боец, цепочки 2.0', '## Главное\n- Карта v4 и муравейник\n- Боец и цепочки 2.0\n- Живая квартира, Учебка', '2026-11-24T11:57:12Z')],
  };
  if (m === 'download') st.task = { tag: 'playtest-2026-11-24-v14', phase: 'download', pct: 37.4, bytes: 409e6, total: 1094e6, speed: 14.2e6, eta: 48, msg: 'Скачиваем', error: '' }, st.installed = '';
  if (m === 'running') st.running = true;
  return st;
}
async function call(name, ...a) {
  if (MOCK) { if (name === 'get_state') return mockState(); return true; }
  return await window.pywebview.api[name](...a);
}
function boot() {
  if (window.pywebview && window.pywebview.api) return start();
  window.addEventListener('pywebviewready', start, { once: true });
  setTimeout(() => { if (!window.pywebview) { MOCK = true; start(); } }, 900);
}
let started = false;
function start() { if (started) return; started = true; init(); tick(); }

// ---------- helpers
const fmtB = b => b >= 1e9 ? (b / 1e9).toFixed(2) + ' ГБ' : (b / 1e6).toFixed(b > 1e8 ? 0 : 1) + ' МБ';
const fmtT = s => { s = Math.round(s); if (s < 1) return '—'; const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60), x = s % 60; return h ? `${h} ч ${m} мин` : m ? `${m}:${String(x).padStart(2, '0')}` : `${x} с`; };
const shortTag = t => (t || '').replace('playtest-', '');
const verOf = t => { const m = /-(v[\d.]+)$/.exec(t || ''); return m ? m[1] : shortTag(t); };
const fmtD = d => d ? new Date(d).toLocaleDateString('ru-RU', { day: 'numeric', month: 'long', year: 'numeric' }) : '—';
const esc = s => String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
function md(src) {
  const inl = t => esc(t).replace(/`([^`]+)`/g, '<code>$1</code>').replace(/\*\*([^*]+)\*\*/g, '<b>$1</b>').replace(/\[([^\]]+)\]\((https?:[^)]+)\)/g, '<a data-url="$2">$1</a>');
  let out = '', list = false;
  for (const raw of (src || '').split(/\r?\n/)) {
    const l = raw.trimEnd(); let m;
    if ((m = /^\s*[-*]\s+(.*)/.exec(l))) { if (!list) { out += '<ul>'; list = true; } out += '<li>' + inl(m[1]) + '</li>'; continue; }
    if (list) { out += '</ul>'; list = false; }
    if ((m = /^#{1,6}\s+(.*)/.exec(l))) out += '<h3>' + inl(m[1]) + '</h3>';
    else if (l.trim()) out += '<p>' + inl(l) + '</p>';
  }
  return out + (list ? '</ul>' : '');
}
let toastT; function toast(t) { const e = $('#toast'); e.textContent = t; e.classList.add('on'); clearTimeout(toastT); toastT = setTimeout(() => e.classList.remove('on'), 3200); }
function ask(title, text, ok = 'ОК', danger = false) {
  return new Promise(res => {
    $('#m-title').textContent = title; $('#m-text').innerHTML = text; const y = $('#m-yes'); y.textContent = ok; y.className = 'btn ' + (danger ? 'danger' : 'amber');
    $('#modal').hidden = false; const done = v => { $('#modal').hidden = true; res(v); };
    y.onclick = () => done(true); $('#m-no').onclick = () => done(false);
  });
}
const plat = () => !S ? 'windows' : (S.platform === 'auto' ? (S.auto_platform === 'mac' ? 'mac' : S.auto_platform === 'windows' ? 'windows' : 'mac') : S.platform);

// ---------- init
function init() {
  $$('#nav button').forEach(b => b.onclick = () => go(b.dataset.tab));
  $$('#plat-seg button').forEach(b => b.onclick = async () => { await call('set_platform', b.dataset.plat); S.platform = b.dataset.plat; render(true); });
  $('#btn-refresh').onclick = async () => { toast('Проверяем новые сборки…'); await call('refresh'); };
  document.addEventListener('click', e => {
    const a = e.target.closest('[data-url]'); if (a) { e.preventDefault(); call('open_url', a.dataset.url); return; }
    const h = e.target.closest('a[href^="http"]'); if (h) { e.preventDefault(); call('open_url', h.href); }
    const acc = e.target.closest('.acc > button'); if (acc) acc.parentElement.classList.toggle('open');
  });
  $('#tab-guide').innerHTML = CONTENT.guide; $('#tab-coop').innerHTML = CONTENT.coop;
  $('#tab-bug').innerHTML = `
  <div class="page paper tape"><h2>Отчёт о баге</h2>
    <p>Нашёл баг? Три шага: <b>собери логи</b>, <b>заполни шаблон</b>, скинь всё в чат плейтеста вместе со скриншотом.</p>
    <div class="row-btn"><button class="btn amber" id="b-collect">Собрать логи на рабочий стол</button><button class="btn" id="b-open">Открыть папку логов</button></div>
    <div class="hint">Архив включает <code>Saved\\Logs</code>, <code>Saved\\Crashes</code> и <code>Saved\\Playtest</code> (если есть) и ложится на рабочий стол с датой в имени.</div>
    <h3>Шаблон сообщения</h3><textarea class="tpl" id="b-tpl" readonly>${CONTENT.bugTemplate}</textarea>
    <div class="row-btn"><button class="btn" id="b-copy">Скопировать шаблон</button></div></div>`;
  $('#b-collect').onclick = async () => { const r = await call('collect_logs'); toast(r.ok ? `Готово: ${r.count} файлов → рабочий стол` : r.error); };
  $('#b-open').onclick = async () => { if (!await call('open_path', 'logs')) toast('Папки логов ещё нет — игру не запускали'); };
  $('#b-copy').onclick = async () => { let ok = false; try { await navigator.clipboard.writeText(CONTENT.bugTemplate); ok = true; } catch (e) { ok = await call('copy_text', CONTENT.bugTemplate); } toast(ok ? 'Скопировано' : 'Не удалось скопировать'); };
  ['o-tutorial', 'o-admin', 'o-windowed', 'o-args'].forEach(id => $('#' + id).addEventListener('change', saveOpts));
  $('#o-args').addEventListener('input', saveOpts);
  $('#btn-main').onclick = onMain; $('#btn-sec').onclick = onSec;
  $('#pr-pause').onclick = () => { const t = S.task; if (t && (t.phase === 'paused')) call('install', t.tag); else call('pause'); };
  $('#pr-cancel').onclick = async () => { await call('cancel'); await call('dismiss'); };
  const hash = location.hash.slice(1); if (hash) go(hash);
}
function saveOpts() { call('set_opts', { tutorial: $('#o-tutorial').checked, admin: $('#o-admin').checked, windowed: $('#o-windowed').checked, args: $('#o-args').value }); }
window.go = function (name) { $$('#nav button').forEach(b => b.classList.toggle('on', b.dataset.tab === name)); $$('.tab').forEach(t => t.classList.toggle('on', t.id === 'tab-' + name)); $('.content').scrollTop = 0; };

async function tick() {
  try { S = await call('get_state'); render(); } catch (e) { console.error(e); }
  setTimeout(tick, S && (S.task || S.launching) ? 450 : 1500);
}

// ---------- actions
async function onMain() {
  const a = $('#btn-main').dataset.act;
  if (a === 'play') { const r = await call('play', optsNow()); if (!r.ok) toast(r.error); }
  else if (a === 'install') { const r = await call('install'); if (r && !r.ok) toast(r.error); }
  else if (a === 'prereq') { const r = await call('install_prereqs'); if (!r.ok) toast(r.error); }
  else if (a === 'retry') { await call('refresh'); }
}
async function onSec() {
  const a = $('#btn-sec').dataset.act;
  if (a === 'update') { const r = await call('install'); if (r && !r.ok) toast(r.error); }
  else if (a === 'steam') { await call('start_steam'); toast('Запускаем Steam…'); }
}
const optsNow = () => ({ tutorial: $('#o-tutorial').checked, admin: $('#o-admin').checked, windowed: $('#o-windowed').checked, args: $('#o-args').value });

// ---------- render
function setPill(cls, txt) { return `<span class="pill ${cls}">${txt}</span>`; }
function render(force) {
  if (!S) return;
  const P = plat();
  $$('#plat-seg button').forEach(b => b.classList.toggle('on', b.dataset.plat === P));
  $('#plat-seg').title = S.platform === 'auto' ? 'Платформа определена автоматически' : 'Платформа выбрана вручную';
  $('#ver-line').textContent = 'лаунчер v' + S.version;
  $('#play-win').hidden = P !== 'windows'; $('#play-mac').hidden = P === 'windows';
  if (P !== 'windows' && !$('#play-mac').dataset.done) {
    $('#play-mac').innerHTML = `<div class="page paper tape mac-hero"><h2>Ant Kitchen на Mac</h2><div class="hint">Нативная Mac-версия <b>в работе</b>. Пока — только через CrossOver; запасной путь — облачный Windows-ПК.</div>${CONTENT.macBody}</div>`; $('#play-mac').dataset.done = 1;
  }
  // chips
  const tgt = S.target, chips = [];
  chips.push(S.installed ? setPill(S.update_available ? 'warn' : 'ok', 'установлена ' + verOf(S.installed)) : setPill('warn', 'не установлена'));
  if (tgt) chips.push(setPill('', 'последняя ' + verOf(tgt.tag)));
  chips.push(S.rel_status === 'ok' ? setPill('ok', 'онлайн') : S.rel_status === 'loading' ? setPill('', 'проверка…') : setPill('bad', 'нет связи с GitHub'));
  $('#top-chips').innerHTML = chips.join('');
  // banners
  let bn = '';
  if (S.selfupdate && S.selfupdate.available && !dismissedUpd) bn += `<div class="banner"><span>Вышла новая версия лаунчера <b>v${esc(S.selfupdate.version)}</b></span><span class="grow"></span><button class="btn amber small" id="bn-self">Обновить лаунчер</button><button class="btn ghost small" id="bn-x">Позже</button></div>`;
  if (S.rel_status === 'error' && !S.releases.length) bn += `<div class="banner bad"><span>Не удалось получить список сборок: ${esc(S.rel_error)}</span><span class="grow"></span><button class="btn small" id="bn-retry">Повторить</button></div>`;
  if (bn !== $('#banners').dataset.h) { $('#banners').innerHTML = bn; $('#banners').dataset.h = bn; const b = $('#bn-self'); if (b) b.onclick = async () => { toast('Скачиваем обновление…'); const r = await call('apply_self_update'); if (r && !r.ok) toast(r.error); }; const x = $('#bn-x'); if (x) x.onclick = () => { dismissedUpd = true; render(); }; const r2 = $('#bn-retry'); if (r2) r2.onclick = () => call('refresh'); }
  $('#nav-news-dot').hidden = !S.update_available;
  if (P === 'windows') renderPlay();
  renderNews(); renderLinks(); renderSettings();
}

function renderPlay() {
  const t = S.task, tgt = S.target, inst = S.installed, busy = t && !['done'].includes(t.phase);
  const main = $('#btn-main'), sec = $('#btn-sec'), title = $('#p-title'), sub = $('#p-sub'), kick = $('#p-kicker');
  // sync option inputs once
  if (!$('#p-opts').dataset.init && S.opts) { $('#o-tutorial').checked = !!S.opts.tutorial; $('#o-admin').checked = !!S.opts.admin; $('#o-windowed').checked = !!S.opts.windowed; $('#o-args').value = S.opts.args || ''; $('#p-opts').dataset.init = 1; }
  $('#p-opts').hidden = !inst;
  let act = '', label = '', dis = false, go = false;
  kick.textContent = 'СТАТУС СБОРКИ';
  if (busy) {
    const names = { download: 'Скачиваем сборку', verify: 'Проверяем файлы', extract: 'Распаковываем', migrate: 'Переносим сохранения', prereq: 'Ставим библиотеки', paused: 'Пауза', error: 'Ошибка' };
    title.textContent = t.phase === 'error' ? 'Что-то пошло не так' : t.phase === 'paused' ? 'Загрузка на паузе' : 'Устанавливаем ' + verOf(t.tag || (tgt && tgt.tag));
    sub.textContent = t.phase === 'error' ? t.error : 'Можно закрыть лаунчер — докачка продолжится с того же места. ' + (t.phase === 'prereq' ? 'Windows спросит разрешение — нажми «Да».' : '');
    $('#p-progress').hidden = false; main.hidden = true; sec.hidden = true;
    const pct = Math.max(0, Math.min(100, t.pct || 0));
    $('#pr-fill').style.width = pct + '%'; $('#pr-pct').textContent = pct.toFixed(pct < 10 ? 1 : 0) + '%';
    $('#pr-phase').textContent = (t.phase === 'download' && t.msg !== 'Скачиваем') ? t.msg : names[t.phase] || t.msg;
    $('.bar', $('#p-progress')).className = 'bar' + (t.phase === 'paused' ? ' paused' : t.phase === 'error' ? ' err' : '');
    $('#pr-info').textContent = t.phase === 'download' ? `${fmtB(t.bytes)} из ${fmtB(t.total)} · ${t.speed ? (t.speed / 1e6).toFixed(1) + ' МБ/с · осталось ' + fmtT(t.eta) : 'ждём данные…'}` : (t.phase === 'error' ? '' : t.msg);
    const pb = $('#pr-pause'); pb.hidden = !['download', 'paused', 'error'].includes(t.phase);
    pb.textContent = t.phase === 'paused' ? 'Продолжить' : t.phase === 'error' ? 'Повторить' : 'Пауза';
    pb.onclick = () => (t.phase === 'download' ? call('pause') : call('install', t.tag));
    $('#pr-cancel').hidden = t.phase === 'prereq'; $('#pr-cancel').onclick = async () => { await call('cancel'); await call('dismiss'); };
  } else {
    if (t && t.phase === 'done') call('dismiss');
    $('#p-progress').hidden = true; main.hidden = false;
    if (!inst) {
      if (!tgt) { title.textContent = S.rel_status === 'loading' ? 'Ищем последнюю сборку…' : 'Нет связи с GitHub'; sub.textContent = 'Проверь интернет и нажми «Повторить».'; act = S.rel_status === 'loading' ? '' : 'retry'; label = S.rel_status === 'loading' ? '…' : 'ПОВТОРИТЬ'; dis = !act; }
      else { title.textContent = 'Давай установим игру'; sub.textContent = `Последняя сборка ${verOf(tgt.tag)} от ${fmtD(tgt.published)}. Скачаем ${fmtB(tgt.size)}, поставим в ${S.root}. Всё остальное лаунчер сделает сам.`; act = 'install'; label = 'УСТАНОВИТЬ'; }
      sec.hidden = true;
    } else if (S.prereq === 'missing') {
      title.textContent = 'Шаг 1: библиотеки Windows'; sub.textContent = 'Один раз поставим системные библиотеки для Unreal Engine (VC++). Windows попросит разрешение — нажми «Да».'; act = 'prereq'; label = 'ПОСТАВИТЬ БИБЛИОТЕКИ'; sec.hidden = true;
    } else {
      go = true;
      if (S.running) { title.textContent = 'Ты в игре'; sub.textContent = 'Игра запущена. Приятной вылазки, муравей!'; act = 'play'; label = 'В ИГРЕ'; dis = true; }
      else if (S.launching) { title.textContent = 'Запускаем…'; sub.textContent = 'Первый запуск может занять 1–2 минуты: игра собирает шейдеры.'; act = 'play'; label = 'ЗАПУСК…'; dis = true; }
      else if (S.update_available) { title.textContent = 'Вышла новая сборка ' + verOf(tgt.tag); sub.textContent = 'В кооп нужна одна и та же версия у всех. Обновление сохранит твой прогресс.'; act = 'play'; label = 'ИГРАТЬ'; }
      else { title.textContent = 'Всё готово. На вылазку!'; sub.textContent = 'У тебя последняя сборка ' + verOf(inst) + '. Steam и библиотеки проверены.'; act = 'play'; label = 'ИГРАТЬ'; }
      if (S.update_available && !S.running) { sec.hidden = false; sec.dataset.act = 'update'; sec.className = 'btn amber'; sec.textContent = 'Обновить до ' + verOf(tgt.tag) + ' · ' + fmtB(tgt.size); }
      else if (S.steam !== 'ok' && S.steam !== undefined && !S.running) { sec.hidden = false; sec.dataset.act = 'steam'; sec.className = 'btn'; sec.textContent = S.steam === 'not_installed' ? 'Steam не найден' : S.steam === 'not_logged_in' ? 'Войди в Steam' : 'Запустить Steam'; if (S.steam === 'not_installed') sec.dataset.act = 'dl-steam'; }
      else sec.hidden = true;
    }
    main.dataset.act = act; main.textContent = label; main.disabled = dis || !act; main.classList.toggle('go', go && !dis);
  }
  if (sec.dataset.act === 'dl-steam') sec.onclick = () => call('open_url', 'https://store.steampowered.com/about/');
  else sec.onclick = onSec;
  renderReceipt(); renderChecks(); renderLatest();
}
function renderReceipt() {
  const t = S.target, st = { ok: ['ok', 'OK, запущен'], not_running: ['warn', 'не запущен'], not_installed: ['bad', 'не найден'], not_logged_in: ['warn', 'нет входа'] }[S.steam] || ['', '?'];
  const row = (a, b, c = '') => `<div class="row"><span>${a}</span><b class="${c}">${b}</b></div>`;
  const ready = S.installed && !S.update_available && S.prereq === 'ok';
  $('#p-receipt').innerHTML = `<h4>СВОДКА ЩЕЛИ</h4><div class="sep"></div>` +
    row('Установлена', S.installed ? verOf(S.installed) : '—', S.installed ? 'ok' : 'warn') +
    row('Последняя', t ? verOf(t.tag) : '—') + row('Выпущена', t ? fmtD(t.published) : '—') +
    `<div class="sep"></div>` + row('Steam', st[1], st[0]) + row('Библиотеки', S.prereq === 'ok' ? 'OK' : 'нужны', S.prereq === 'ok' ? 'ok' : 'warn') +
    row('Свободно', fmtB(S.free)) + `<div class="sep"></div><div style="text-align:center"><span class="stamp ${ready ? 'ok' : 'warn'}">${S.running ? 'В ИГРЕ' : ready ? 'ГОТОВО' : S.installed ? 'ЕСТЬ ДЕЛА' : 'ЖДЁМ'}</span></div>`;
}
function renderChecks() {
  const sm = { ok: ['ok', 'Steam: OK', 'Запущен, вход выполнен. Кооп через друзей Steam работает.'], not_running: ['warn', 'Steam не запущен', 'Запусти Steam и войди <b>до</b> игры, иначе кооп только по IP.'], not_logged_in: ['warn', 'Steam: нет входа', 'Steam запущен, но вход не выполнен.'], not_installed: ['bad', 'Steam не найден', 'Для коопа нужен клиент Steam (бесплатно).'] }[S.steam] || ['', 'Steam', ''];
  const free = S.free, low = free < 4e9;
  $('#p-checks').innerHTML = `
    <div class="card check"><h3>СТИМ</h3><div class="st"><span class="dot ${sm[0]}"></span>${sm[1]}</div><p>${sm[2]}</p><div class="row-btn">${S.steam === 'not_running' || S.steam === 'not_logged_in' ? '<button class="btn small" data-do="steam">Запустить Steam</button>' : S.steam === 'not_installed' ? '<button class="btn small" data-url="https://store.steampowered.com/about/">Скачать Steam</button>' : ''}</div></div>
    <div class="card check"><h3>БИБЛИОТЕКИ</h3><div class="st"><span class="dot ${S.prereq === 'ok' ? 'ok' : 'warn'}"></span>${S.prereq === 'ok' ? 'Библиотеки: OK' : 'Нужно поставить'}</div><p>Системные библиотеки Unreal Engine. Ставятся один раз на компьютер.</p></div>
    <div class="card check"><h3>МЕСТО</h3><div class="st"><span class="dot ${low ? 'bad' : 'ok'}"></span>Свободно ${fmtB(free)}</div><p>Для установки нужно ~3 ГБ. Папка: <a data-do="root" style="cursor:pointer">${esc(S.root)}</a></p></div>`;
  $$('#p-checks [data-do]').forEach(e => e.onclick = () => e.dataset.do === 'steam' ? (call('start_steam'), toast('Запускаем Steam…')) : call('open_path', 'root'));
}
function renderLatest() {
  const r = S.releases[0]; const el = $('#p-latest');
  el.hidden = !r; if (!r) return;
  el.innerHTML = `<h3>Что нового в ${esc(verOf(r.tag))}</h3><div class="md" style="max-height:170px;overflow:auto;color:#d9ccb0">${md(r.body) || '<p>Заметок к релизу нет.</p>'}</div>`;
}
function renderNews() {
  const h = JSON.stringify([S.releases.map(r => r.tag + r.body.length), S.installed, S.target && S.target.tag]); if (h === newsHash) return; newsHash = h;
  $('#news-list').innerHTML = S.releases.length ? S.releases.map((r, i) => `<div class="news paper tape md"><header><h2>${esc(r.name)}</h2>${r.tag === S.installed ? '<span class="badge cur">установлена</span>' : ''}${i === 0 ? '<span class="badge new">последняя</span>' : ''}<span class="badge">${fmtD(r.published)}</span></header>
    ${md(r.body) || '<p>Заметок нет.</p>'}<div class="row-btn" style="margin-top:12px"><a class="btn small ghost" data-url="${r.url}">Открыть на GitHub</a>${r.has_zip && r.tag !== S.installed ? `<button class="btn small" data-inst="${r.tag}">Установить эту версию</button>` : ''}</div></div>`).join('') : '<div class="page paper tape"><h2>Новости</h2><p>Список релизов пока не загрузился. Проверь интернет и нажми «Обновить список».</p></div>';
  $$('#news-list [data-inst]').forEach(b => b.onclick = async () => { if (await ask('Установить ' + verOf(b.dataset.inst) + '?', 'Сохранения будут перенесены в эту версию. Предыдущая останется для отката.', 'Установить')) { go('play'); const r = await call('install', b.dataset.inst); if (r && !r.ok) toast(r.error); } });
}
function renderLinks() {
  const chat = S.chat_url || ''; const h = 'l' + chat; if ($('#tab-links').dataset.h === h) return; $('#tab-links').dataset.h = h;
  $('#tab-links').innerHTML = `<div class="page"><div class="links">${CONTENT.links(chat).map(([t, d, u]) => `<a class="link ${u ? '' : 'off'}" ${u ? `data-url="${u}"` : ''}><b>${t}</b><span>${d}</span></a>`).join('')}</div></div>`;
}
function renderSettings() {
  const focus = document.activeElement && document.activeElement.closest('#tab-settings') && ['INPUT', 'SELECT'].includes(document.activeElement.tagName);
  const h = JSON.stringify([S.root, S.installed, S.previous, S.channel, S.pinned_tag, S.releases.length, S.version, S.prereq, S.chat_url, !!S.task]); if (h === setHash || focus) return; setHash = h;
  const opts = S.releases.filter(r => r.has_zip).map(r => `<option value="${r.tag}" ${r.tag === S.pinned_tag ? 'selected' : ''}>${esc(verOf(r.tag))} — ${esc(r.name)}</option>`).join('');
  $('#tab-settings').innerHTML = `<div class="page paper tape"><h2>Настройки</h2>
   <div class="set"><label>Папка установки<small>Игра и сохранения лежат здесь</small></label><div class="val"><code>${esc(S.root)}</code><button class="btn small" id="s-root">Изменить…</button><button class="btn small ghost" id="s-open">Открыть</button></div></div>
   <div class="set"><label>Канал обновлений</label><div class="val"><select class="field" id="s-chan"><option value="latest" ${S.channel !== 'pinned' ? 'selected' : ''}>Всегда последняя сборка</option>${opts ? `<optgroup label="Закрепить версию">${opts}</optgroup>` : ''}</select></div></div>
   <div class="set"><label>Сохранения<small>Переносятся при обновлении</small></label><div class="val"><button class="btn small" id="s-saved">Открыть папку Saved</button></div></div>
   <div class="set"><label>Целостность файлов</label><div class="val"><button class="btn small" id="s-verify">Проверить файлы</button><button class="btn small" id="s-re">Переустановить</button></div></div>
   <div class="set"><label>Откат<small>${S.previous ? 'Предыдущая сборка: ' + esc(verOf(S.previous)) : 'Предыдущей сборки нет'}</small></label><div class="val"><button class="btn small" id="s-back" ${S.previous ? '' : 'disabled'}>Вернуть предыдущую</button></div></div>
   <div class="set"><label>Библиотеки Windows<small>${S.prereq === 'ok' ? 'Установлены' : 'Не установлены'}</small></label><div class="val"><button class="btn small" id="s-pre" ${S.installed ? '' : 'disabled'}>Поставить заново</button></div></div>
   <div class="set"><label>Ссылка на чат команды<small>Discord / Telegram — появится на вкладке «Ссылки»</small></label><div class="val"><input type="text" class="field" id="s-chat" placeholder="https://discord.gg/…" value="${esc(S.chat_url || '')}"><button class="btn small" id="s-chat-ok">Сохранить</button></div></div>
   <div class="set"><label>Язык</label><div class="val"><select class="field"><option>Русский</option><option disabled>English — скоро</option></select></div></div>
   <div class="set"><label>Удаление<small>Сохранения копируются в SavedBackup</small></label><div class="val"><button class="btn small danger" id="s-del">Удалить игру</button></div></div>
   <div class="set"><label>О лаунчере</label><div class="val"><span>v${esc(S.version)}</span><button class="btn small ghost" id="s-ldata">Папка данных лаунчера</button></div></div></div>`;
  const $s = id => $('#' + id);
  $s('s-root').onclick = async () => { const r = await call('choose_root'); if (r.ok) { toast('Папка изменена'); setHash = ''; } else if (r.error) toast(r.error); };
  $s('s-open').onclick = () => call('open_path', 'root'); $s('s-saved').onclick = async () => { if (!await call('open_path', 'saved')) toast('Игра не установлена'); };
  $s('s-ldata').onclick = () => call('open_path', 'launcher');
  $s('s-chan').onchange = async e => { const v = e.target.value; await call('set_channel', v === 'latest' ? 'latest' : 'pinned', v === 'latest' ? '' : v); S.channel = v === 'latest' ? 'latest' : 'pinned'; toast(v === 'latest' ? 'Будет ставиться последняя сборка' : 'Версия закреплена: ' + verOf(v)); };
  $s('s-verify').onclick = async () => { toast('Проверяем…'); const r = await call('verify'); toast((r.ok ? '✓ ' : '✗ ') + r.msg); };
  $s('s-re').onclick = async () => { if (await ask('Переустановить игру?', 'Сборка скачается заново (≈1 ГБ). Сохранения будут сохранены.', 'Переустановить')) { go('play'); const r = await call('reinstall'); if (r && !r.ok) toast(r.error); } };
  $s('s-back').onclick = async () => { if (await ask('Откатить версию?', 'Вернём предыдущую сборку и перенесём в неё сохранения.', 'Откатить')) { const r = await call('rollback'); toast(r.ok ? 'Готово' : r.error); } };
  $s('s-pre').onclick = async () => { go('play'); const r = await call('install_prereqs'); if (!r.ok) toast(r.error); };
  $s('s-chat-ok').onclick = async () => { await call('set_chat_url', $s('s-chat').value.trim()); S.chat_url = $s('s-chat').value.trim(); toast('Сохранено'); };
  $s('s-del').onclick = async () => { if (await ask('Удалить игру?', 'Все сборки будут удалены с диска. Сохранения скопируются в папку <code>SavedBackup</code>. Это нельзя отменить.', 'Удалить', true)) { const r = await call('uninstall'); toast(r.ok ? 'Удалено' : r.error); } };
}
boot();
