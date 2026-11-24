#!/usr/bin/env python3
"""PlaytestKit report: one self-contained HTML from a folder / zip(s) / Supabase.

  python ptk_report.py <Saved/Playtest folder | folder of zips | file.zip> --config projects/ant-kitchen.json --out report.html
  python ptk_report.py supabase --url https://xxx.supabase.co --key <service_role> --project ant-kitchen --config ...

Deps: pandas, numpy, matplotlib  (pip install --user pandas numpy matplotlib)
"""
import argparse, base64, html, io, json, os, sys, math
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import font_manager  # noqa
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ptk_loader

BG, FG, GRID = "#1b1d21", "#e6e6e6", "#3a3d44"
C_OK, C_BAD, C_WARN, C_ACC = "#6fcf97", "#eb5757", "#f2c94c", "#56b4e9"
TAGCOL = {"bug": C_BAD, "confusing": C_WARN, "boring": "#9b9b9b", "fun": C_OK}
plt.rcParams.update({"figure.facecolor": BG, "axes.facecolor": "#23262b", "axes.edgecolor": GRID, "axes.labelcolor": FG,
                     "xtick.color": FG, "ytick.color": FG, "text.color": FG, "axes.grid": True, "grid.color": GRID,
                     "grid.linewidth": .5, "font.size": 9, "axes.titlesize": 10, "figure.dpi": 110})


# ----------------------------------------------------------------------------- helpers
class Report:
    def __init__(self, out, title):
        self.out, self.title, self.parts, self.toc, self.charts = out, title, [], [], {}
        self.chart_dir = os.path.splitext(out)[0] + "_charts"
        os.makedirs(self.chart_dir, exist_ok=True)

    def h2(self, id_, text):
        self.toc.append((id_, text)); self.parts.append(f'<h2 id="{id_}">{html.escape(text)}</h2>')

    def p(self, t, cls=""):
        self.parts.append(f'<p class="{cls}">{t}</p>')

    def raw(self, h):
        self.parts.append(h)

    def fig(self, fig, name, alt=""):
        buf = io.BytesIO(); fig.savefig(buf, format="png", facecolor=fig.get_facecolor(), bbox_inches="tight"); plt.close(fig)
        b = buf.getvalue(); open(os.path.join(self.chart_dir, name + ".png"), "wb").write(b)
        self.parts.append(f'<img class="chart" alt="{html.escape(alt)}" src="data:image/png;base64,{base64.b64encode(b).decode()}">')

    def table(self, df, cls="", fmt=None, maxrows=60):
        if df is None or len(df) == 0:
            self.p("нет данных", "muted"); return
        d = df.head(maxrows).copy()
        for c in d.columns:
            if d[c].dtype.kind == "f":
                d[c] = d[c].map(lambda v: "" if pd.isna(v) else (f"{v:.1f}" if abs(v) < 1000 else f"{v:.0f}"))
        head = "".join(f"<th>{html.escape(str(c))}</th>" for c in d.columns)
        rows = "".join("<tr>" + "".join(f"<td>{v if str(v).startswith('<') else html.escape(str(v))}</td>" for v in r) + "</tr>" for r in d.itertuples(index=False))
        extra = f'<p class="muted">показано {maxrows} из {len(df)}</p>' if len(df) > maxrows else ""
        self.parts.append(f'<table class="{cls}"><thead><tr>{head}</tr></thead><tbody>{rows}</tbody></table>{extra}')

    def save(self, meta):
        toc = "".join(f'<a href="#{i}">{html.escape(t)}</a>' for i, t in self.toc)
        doc = f"""<!doctype html><html lang="ru"><head><meta charset="utf-8"><title>{html.escape(self.title)}</title><style>
body{{background:{BG};color:{FG};font:14px/1.5 -apple-system,Segoe UI,Roboto,Arial,sans-serif;margin:0}}
.wrap{{max-width:1180px;margin:0 auto;padding:24px}}h1{{font-size:26px;margin:.2em 0}}h2{{margin-top:2.2em;border-bottom:1px solid {GRID};padding-bottom:.25em}}h3{{margin:1.4em 0 .4em}}
nav{{position:sticky;top:0;background:{BG};padding:8px 0;border-bottom:1px solid {GRID};display:flex;flex-wrap:wrap;gap:6px 14px;font-size:12px;z-index:5}}nav a{{color:{C_ACC};text-decoration:none}}
table{{border-collapse:collapse;margin:.6em 0;font-size:12.5px}}th,td{{border:1px solid {GRID};padding:3px 8px;text-align:left;vertical-align:top}}th{{background:#2a2d33}}
.chart{{max-width:100%;margin:.5em 0;border-radius:6px}}.muted{{color:#8d9199}}.kpis{{display:flex;flex-wrap:wrap;gap:10px}}.kpi{{background:#23262b;border:1px solid {GRID};border-radius:8px;padding:8px 14px;min-width:120px}}
.kpi b{{display:block;font-size:22px}}.ok{{color:{C_OK}}}.bad{{color:{C_BAD}}}.warn{{color:{C_WARN}}}.tag{{padding:1px 7px;border-radius:9px;color:#111;font-weight:600;font-size:11px}}
.mk{{display:flex;gap:12px;margin:10px 0;padding:8px;background:#23262b;border-radius:8px;border:1px solid {GRID}}}.mk img{{width:260px;border-radius:4px;flex:none}}
</style></head><body><div class="wrap"><h1>{html.escape(self.title)}</h1><p class="muted">{html.escape(meta)}</p><nav>{toc}</nav>{''.join(self.parts)}
<p class="muted" style="margin-top:3em">PlaytestKit report · anon data only · {html.escape(meta)}</p></div></body></html>"""
        open(self.out, "w", encoding="utf-8").write(doc)


def num(df, col):
    return pd.to_numeric(df[col], errors="coerce") if col in df else pd.Series(np.nan, index=df.index)


def col(df, c, default=None):
    return df[c] if c in df else pd.Series(default, index=df.index)


def ev(E, *names):
    return E[E["name"].isin(names)]


def kpi(label, val, cls=""):
    return f'<div class="kpi"><span class="muted">{html.escape(label)}</span><b class="{cls}">{html.escape(str(val))}</b></div>'


def fmt_t(sec):
    if sec is None or (isinstance(sec, float) and math.isnan(sec)): return ""
    sec = int(sec); return f"{sec//3600}:{sec%3600//60:02d}:{sec%60:02d}" if sec >= 3600 else f"{sec//60}:{sec%60:02d}"


class MapT:
    """world (uu) -> image px with optional background image."""
    def __init__(self, cfg, base):
        m = cfg.get("map") or {}
        self.w = m.get("world", {"x0": -7500, "x1": 7500, "y0": -5300, "y1": 5900})
        self.img = None
        ip = os.path.join(base, m.get("image", "")) if m.get("image") else None
        if ip and os.path.exists(ip):
            self.img = plt.imread(ip)
            jp = os.path.splitext(ip)[0] + ".json"
            if os.path.exists(jp):
                t = json.load(open(jp)); self.w = {k: t[k] for k in ("x0", "x1", "y0", "y1")}
        self.cell = m.get("grid_uu", 150)

    def axes(self, figsize=(9, 6.4)):
        fig, ax = plt.subplots(figsize=figsize)
        ax.grid(False)
        if self.img is not None:
            ax.imshow(self.img, extent=[self.w["x0"], self.w["x1"], self.w["y0"], self.w["y1"]], origin="upper", aspect="equal")
        ax.set_xlim(self.w["x0"], self.w["x1"]); ax.set_ylim(self.w["y0"], self.w["y1"]); ax.set_aspect("equal")
        ax.set_xticks([]); ax.set_yticks([])
        return fig, ax

    def heat(self, ax, x, y, cmap="inferno", alpha=.75, bins=None, vmax_q=.995):
        x, y = np.asarray(x, float), np.asarray(y, float)
        ok = ~(np.isnan(x) | np.isnan(y))
        x, y = x[ok], y[ok]
        if len(x) == 0: return False
        bx = bins or int((self.w["x1"] - self.w["x0"]) / self.cell)
        by = bins or int((self.w["y1"] - self.w["y0"]) / self.cell)
        H, xe, ye = np.histogram2d(x, y, bins=[bx, by], range=[[self.w["x0"], self.w["x1"]], [self.w["y0"], self.w["y1"]]])
        H = H.T.astype(float); H[H == 0] = np.nan
        vmax = np.nanquantile(H, vmax_q) if np.isfinite(H).any() else 1
        ax.imshow(H, extent=[self.w["x0"], self.w["x1"], self.w["y0"], self.w["y1"]], origin="lower", cmap=cmap, alpha=alpha, vmin=0, vmax=max(vmax, 1), interpolation="nearest")
        return True


def bar_h(ax, labels, vals, colors=None):
    y = np.arange(len(labels)); ax.barh(y, vals, color=colors or C_ACC); ax.set_yticks(y); ax.set_yticklabels(labels); ax.invert_yaxis()
    for i, v in enumerate(vals): ax.text(v, i, f" {v:g}", va="center", fontsize=8)


# ----------------------------------------------------------------------------- sections
def s_overview(R, D, cfg, M):
    S, E = D.sessions, D.events
    R.h2("overview", "1. Обзор")
    n = len(S); testers = S["anon_id"].nunique()
    tot = S["seconds"].sum()
    crashes = int((E["name"] == "crash").sum()); fatal = int((num(ev(E, "crash"), "p_fatal").fillna(1) > 0).sum()) if crashes else 0
    markers = int((E["name"] == "marker").sum())
    builds = ", ".join(sorted(set(map(str, S["build"].dropna())))) or "-"
    M.update(sessions=n, testers=testers)
    R.raw('<div class="kpis">' + kpi("сессий", n) + kpi("тестеров", testers) + kpi("всего играли", fmt_t(tot)) +
          kpi("медиана сессии", fmt_t(S["seconds"].median())) + kpi("краши", crashes, "bad" if crashes else "ok") +
          kpi("маркеры F7", markers) + kpi("ошибок в логе", int(S.get("cnt_errors", pd.Series(0)).fillna(0).sum())) + kpi("сборки", builds) + '</div>')
    t = S[["session_id", "anon_id", "role", "build", "seconds", "n_events"]].copy()
    t["seconds"] = t["seconds"].map(fmt_t)
    mach = ev(E, "session_start")
    if len(mach) and "p_machine" in mach:
        mm = mach.set_index("sid")["p_machine"].to_dict()
        t["gpu / cpu / ram"] = t["session_id"].map(lambda s: (lambda d: f"{d.get('gpu','')} / {d.get('cpu','')} / {d.get('ram_gb','')}GB" if isinstance(d, dict) else "")(mm.get(s)))
    R.table(t)


def s_hypotheses(R, D, cfg, M):
    hs = cfg.get("hypotheses") or []
    if not hs: return
    R.h2("hyp", "2. Гипотезы плейтеста")
    rows = []
    for h in hs:
        v = M.get(h.get("metric"))
        ok = None
        if v is not None and not (isinstance(v, float) and math.isnan(v)):
            ok = True
            if "target_max" in h: ok &= v <= h["target_max"]
            if "target_min" in h: ok &= v >= h["target_min"]
        st = "нет данных" if ok is None else ("<span class='ok'>подтверждена</span>" if ok else "<span class='bad'>не подтверждена</span>")
        tg = " / ".join(f"{k.split('_')[1]} {h[k]}" for k in ("target_min", "target_max") if k in h)
        rows.append([h["id"], h["text"], h.get("metric"), "" if v is None else round(float(v), 2), tg, st])
    R.table(pd.DataFrame(rows, columns=["id", "гипотеза", "метрика", "значение", "цель", "итог"]))


def s_funnel(R, D, cfg, M):
    E = D.events
    R.h2("funnel", "3. Онбординг и туториал: воронка и время на шаг")
    st, dn = ev(E, "tutorial_step_start"), ev(E, "tutorial_step_complete")
    if st.empty:
        st, dn = ev(E, "objective_start"), ev(E, "objective_complete")
    if st.empty:
        R.p("Нет событий туториала/целей.", "muted"); return
    order = st.assign(no=num(st, "p_no")).groupby("p_id")["no"].min().sort_values().index.tolist() if "p_no" in st else list(dict.fromkeys(st["p_id"]))
    rows = []
    sk, fl = ev(E, "tutorial_step_skip"), ev(E, "tutorial_step_fail")
    for sid_ in order:
        a = st[st["p_id"] == sid_]; b = dn[dn["p_id"] == sid_]
        sec = num(b, "p_seconds"); par = num(a, "p_par").median() if "p_par" in a else np.nan
        rows.append({"шаг": sid_, "начали": a["sid"].nunique(), "прошли": b["sid"].nunique(),
                     "пропуск": sk[sk["p_id"] == sid_]["sid"].nunique() if len(sk) else 0, "фейлов": len(fl[fl["p_id"] == sid_]) if len(fl) else 0,
                     "медиана, с": sec.median(), "p90, с": sec.quantile(.9) if len(sec) else np.nan, "par, с": par,
                     "x par": sec.median() / par if par and par > 0 else np.nan,
                     "ретраи": num(b, "p_retries").sum() if "p_retries" in b else 0, "помощь": num(b, "p_assists").sum() if "p_assists" in b else 0})
    F = pd.DataFrame(rows)
    F["отвал, %"] = (1 - F["прошли"] / F["начали"].replace(0, np.nan)) * 100
    R.table(F)
    fig, axs = plt.subplots(1, 2, figsize=(11, max(3, .32 * len(F) + 1.2)))
    y = np.arange(len(F))
    axs[0].barh(y - .2, F["начали"], .4, color=C_ACC, label="начали"); axs[0].barh(y + .2, F["прошли"], .4, color=C_OK, label="прошли")
    axs[0].set_yticks(y); axs[0].set_yticklabels(F["шаг"]); axs[0].invert_yaxis(); axs[0].legend(); axs[0].set_title("воронка (игроки)")
    axs[1].barh(y - .2, F["медиана, с"].fillna(0), .4, color=C_WARN, label="медиана"); axs[1].barh(y + .2, F["par, с"].fillna(0), .4, color="#8d9199", label="par")
    axs[1].set_yticks(y); axs[1].set_yticklabels([]); axs[1].invert_yaxis(); axs[1].legend(); axs[1].set_title("время на шаг, с")
    R.fig(fig, "funnel", "воронка")
    worst = F.sort_values("x par", ascending=False).head(3)
    if len(worst) and worst["x par"].notna().any():
        R.p("Дольше всего относительно par: " + ", ".join(f"<b>{r['шаг']}</b> (x{r['x par']:.1f})" for _, r in worst.iterrows() if pd.notna(r["x par"])))
    fin = ev(E, "tutorial_finish")
    if len(fin):
        tt = num(fin, "p_seconds"); M["tutorial_total_s"] = float(tt.median()) if tt.notna().any() else np.nan
        R.p(f"Туториал завершили: {fin['sid'].nunique()} из {D.sessions['session_id'].nunique()} сессий.")


def s_deaths(R, D, cfg, mt, M):
    E = D.events
    R.h2("deaths", "4. Смерти и сложность")
    d = ev(E, "death")
    if d.empty:
        R.p("Смертей не зафиксировано.", "muted"); return
    names = cfg.get("death_causes_ru", {})
    d = d.copy(); d["cause"] = col(d, "p_cause", "?").fillna("?").map(lambda c: names.get(c, c))
    cnt = d.groupby("cause").size().sort_values(ascending=False)
    fig, axs = plt.subplots(1, 2, figsize=(11, max(3, .3 * len(cnt) + 1)))
    bar_h(axs[0], list(cnt.index), list(cnt.values), C_BAD); axs[0].set_title("смерти по причинам")
    kl = col(d, "p_killer").dropna().value_counts().head(10)
    if len(kl): bar_h(axs[1], list(kl.index), list(kl.values), C_WARN); axs[1].set_title("убийцы (класс)")
    R.fig(fig, "deaths_cause", "причины")
    fig, ax = mt.axes()
    bc = ev(E, "breadcrumb")
    mt.heat(ax, col(bc, "c_pos_x"), col(bc, "c_pos_y"), cmap="Blues", alpha=.55)
    px, py = num(d, "c_pos_x"), num(d, "c_pos_y")
    ax.scatter(px, py, s=36, c=C_BAD, edgecolors="white", linewidths=.6, alpha=.9, label=f"смерти ({int(px.notna().sum())})")
    ax.set_title("карта: синий = где ходят (breadcrumbs), красное = смерти"); ax.legend(loc="lower right")
    R.fig(fig, "deaths_map", "карта смертей")
    top = d.assign(cx=(px // 400 * 400), cy=(py // 400 * 400)).dropna(subset=["cx"]).groupby(["cx", "cy"]).size().sort_values(ascending=False).head(8)
    if len(top):
        R.p("Горячие клетки смертей (клетка 400uu, левый нижний угол): " + "; ".join(f"({int(a)}, {int(b)}) x{n}" for (a, b), n in top.items()))
    rn = col(d, "c_raid_no")
    raids = ev(E, "raid_start")
    if len(raids):
        deaths_per = d.groupby("sid").size().sum() / max(1, len(raids)); M["deaths_per_raid"] = deaths_per
        re_ = ev(E, "raid_end").copy(); re_["raid_no"] = num(re_, "p_raid_no")
        dd = d.assign(raid_no=num(d, "c_raid_no")).groupby("raid_no").size()
        res = re_.groupby("raid_no")["p_end_name"].apply(lambda s: s.value_counts().to_dict()) if "p_end_name" in re_ else None
        rr = re_.groupby("raid_no").size().rename("рейдов")
        T = pd.concat([rr, dd.rename("смертей")], axis=1).fillna(0)
        T["смертей/рейд"] = T["смертей"] / T["рейдов"].replace(0, np.nan)
        if "p_end_name" in re_:
            for k in ("left", "timeout", "wiped"):
                T[k] = re_[re_["p_end_name"] == k].groupby("raid_no").size()
            T = T.fillna(0); T["вайп, %"] = T["wiped"] / T["рейдов"].replace(0, np.nan) * 100
        T = T.reset_index().rename(columns={"index": "raid_no"})
        R.p("Кривая сложности по номеру рейда:")
        R.table(T)
        fig, ax = plt.subplots(figsize=(8, 3.2))
        ax.plot(T["raid_no"], T["смертей/рейд"], marker="o", color=C_BAD, label="смертей/рейд")
        if "вайп, %" in T:
            ax2 = ax.twinx(); ax2.plot(T["raid_no"], T["вайп, %"], marker="s", color=C_WARN, label="вайп %"); ax2.set_ylabel("вайп, %"); ax2.grid(False)
        ax.set_xlabel("raid_no"); ax.set_ylabel("смертей/рейд"); ax.set_title("кривая сложности")
        R.fig(fig, "difficulty", "кривая сложности")
    ls = num(d, "p_life_seconds").dropna()
    if len(ls):
        R.p(f"Время жизни до смерти: медиана {ls.median():.0f} с, p10 {ls.quantile(.1):.0f} с.")


def s_economy(R, D, cfg, M):
    E = D.events
    R.h2("economy", "5. Экономика: источники и стоки")
    so, sk = ev(E, "economy_source"), ev(E, "economy_sink")
    if so.empty and sk.empty:
        R.p("Нет событий экономики.", "muted"); return
    fig, axs = plt.subplots(1, 2, figsize=(11, 3.6))
    for ax, df, key, ttl, c in ((axs[0], so, "p_src", "источники (сумма)", C_OK), (axs[1], sk, "p_sink", "стоки (сумма)", C_BAD)):
        if len(df):
            g = df.assign(a=num(df, "p_amount")).groupby(col(df, key, "?").fillna("?"))["a"].sum().sort_values(ascending=False).head(12)
            bar_h(ax, list(g.index), [round(v, 1) for v in g.values], c)
        ax.set_title(ttl)
    R.fig(fig, "economy", "экономика")
    if len(so) and "p_item" in so:
        g = so.assign(a=num(so, "p_amount")).groupby("p_item")["a"].agg(["sum", "count"]).sort_values("sum", ascending=False).head(15)
        R.table(g.reset_index().rename(columns={"p_item": "предмет", "sum": "сумма", "count": "событий"}))
    if not sk.empty: return
    R.p("Стоков (economy_sink) игра пока не шлёт: баланс прихода без расхода не показывает инфляцию.", "muted")


def s_markers(R, D, cfg, mt, M):
    E = D.events
    R.h2("markers", "6. Маркеры F7: таймлайн, скриншоты, радость и боль")
    m = ev(E, "marker").copy()
    if m.empty:
        R.p("Маркеров нет.", "muted"); return
    m["tag"] = col(m, "p_tag", "?").fillna("?")
    c = m["tag"].value_counts()
    R.raw('<div class="kpis">' + "".join(kpi(k, v, "") for k, v in c.items()) + '</div>')
    fig, ax = mt.axes()
    for tg, g in m.groupby("tag"):
        ax.scatter(num(g, "c_pos_x"), num(g, "c_pos_y"), s=55, c=TAGCOL.get(tg, C_ACC), edgecolors="white", linewidths=.6, label=f"{tg} ({len(g)})")
    ax.legend(loc="lower right"); ax.set_title("где игроки ставили маркеры")
    R.fig(fig, "markers_map", "карта маркеров")
    # timeline: sessions on Y, time on X
    S = D.sessions.set_index("session_id")
    fig, ax = plt.subplots(figsize=(11, max(2.5, .45 * m["sid"].nunique() + 1)))
    ys = {s: i for i, s in enumerate(sorted(m["sid"].unique()))}
    for s, i in ys.items():
        L = (S["seconds"].get(s, 0) or 0) / 60.0
        ax.hlines(i, 0, L, color=GRID, lw=6)
    for tg, g in m.groupby("tag"):
        ax.scatter(g["t"] / 60000.0, g["sid"].map(ys), c=TAGCOL.get(tg, C_ACC), s=45, label=tg, zorder=3)
    ax.set_yticks(list(ys.values())); ax.set_yticklabels([s[-9:] for s in ys]); ax.invert_yaxis(); ax.set_xlabel("минуты от старта сессии"); ax.legend(ncol=4)
    ax.set_title("таймлайн маркеров по сессиям")
    R.fig(fig, "markers_timeline", "таймлайн")
    # per raid_no tag histogram + per-minute fun vs bad
    g = m.assign(raid=num(m, "c_raid_no")).dropna(subset=["raid"]).groupby(["raid", "tag"]).size().unstack(fill_value=0)
    if len(g):
        fig, ax = plt.subplots(figsize=(8, 3)); g.plot(kind="bar", stacked=True, ax=ax, color=[TAGCOL.get(k, C_ACC) for k in g.columns]); ax.set_xlabel("raid_no"); ax.set_title("маркеры по номеру рейда")
        R.fig(fig, "markers_raid", "маркеры по рейдам")
    shots = {(r.sid, os.path.basename(str(r.p_shot))): r for r in m.itertuples() if hasattr(r, "p_shot") and isinstance(r.p_shot, str)}
    order = {"bug": 0, "confusing": 1, "boring": 2, "fun": 3}
    for r in sorted(m.itertuples(), key=lambda r: (order.get(r.tag, 9), r.sid, r.t))[:80]:
        img = ""
        key = (r.sid, os.path.basename(str(getattr(r, "p_shot", "") or "")))
        g_ = D.shots.get(key)
        if g_:
            try: img = f'<img src="data:image/jpeg;base64,{base64.b64encode(g_()).decode()}">'
            except Exception: pass
        pf = getattr(r, "p_perf30", None)
        pfs = ""
        if isinstance(pf, list) and pf:
            fps = [x.get("fps") for x in pf if isinstance(x, dict) and x.get("fps") is not None]
            if fps: pfs = f" · fps за 30 с: мин {min(fps):.0f} / сред {np.mean(fps):.0f}"
        rec = getattr(r, "p_recent", None)
        recs = ("<div class='muted'>перед этим: " + html.escape(" → ".join(map(str, rec[-6:]))) + "</div>") if isinstance(rec, list) and rec else ""
        loc = f"{getattr(r, 'c_map', '') or ''} рейд {getattr(r, 'c_raid_no', '')}" if str(getattr(r, 'c_raid_no', '')) not in ("", "nan", "None") else str(getattr(r, 'c_map', '') or '')
        R.raw(f'<div class="mk">{img}<div><span class="tag" style="background:{TAGCOL.get(r.tag, C_ACC)}">{html.escape(r.tag)}</span> '
              f'<b>{html.escape(str(getattr(r, "p_text", "") or ""))}</b><div class="muted">{html.escape(r.sid[-14:])} · {fmt_t(r.t/1000)} · {html.escape(loc)}{pfs}</div>{recs}</div></div>')


def s_surveys(R, D, cfg, M):
    E = D.events
    R.h2("surveys", "7. Опросы")
    a = ev(E, "survey_answer").copy()
    if a.empty:
        R.p("Ответов на опросы нет.", "muted"); return
    a["typ"] = col(a, "p_type", "")
    a["val"] = col(a, "p_value").where(a["typ"] != "text", col(a, "p_text"))   # text answers live in props.text
    a["qid"] = col(a, "p_qid", "?")
    a["sv"] = col(a, "p_survey_id", "?")
    shown = ev(E, "survey_show"); asked = ev(E, "survey_ask")
    R.p(f"Показано опросов: {len(shown)} · микро-вопросов: {len(asked)} · ответов: {len(a)} · skipped/timeout не считаются ответами.")
    sc = a[a["typ"] == "scale"].copy(); sc["v"] = pd.to_numeric(sc["val"], errors="coerce")
    if len(sc):
        g = sc.groupby(["sv", "qid"])["v"].agg(n="count", mean="mean", median="median", mn="min", mx="max").reset_index()
        R.p("Шкалы:"); R.table(g)
        qs = list(g.itertuples()); k = min(len(qs), 8)
        fig, axs = plt.subplots(int(math.ceil(k / 4)), 4 if k >= 4 else k, figsize=(11, 2.4 * math.ceil(k / 4)), squeeze=False)
        for ax in axs.flat: ax.axis("off")
        for ax, q in zip(axs.flat, qs[:k]):
            ax.axis("on"); v = sc[(sc["sv"] == q.sv) & (sc["qid"] == q.qid)]["v"].dropna()
            ax.hist(v, bins=np.arange(v.min() - .5, max(v.max(), v.min() + 1) + 1.5, 1) if len(v) else 5, color=C_ACC); ax.set_title(f"{q.qid} (n={q.n})", fontsize=8)
        R.fig(fig, "survey_scales", "шкалы")
        ov = sc[sc["qid"] == "overall"]["v"].dropna()
        if len(ov): M["overall"] = float(ov.mean())
    ch = a[a["typ"] == "choice"]
    if len(ch):
        g = ch.groupby(["qid", ch["val"].astype(str)]).size().rename("n").reset_index()
        R.p("Выбор:"); R.table(g)
        pa = ch[ch["qid"] == "play_again"]
        if len(pa):
            yes = pa["val"].astype(str).str.lower().str.startswith(("y", "да", "скорее да", "1", "true")).mean()
            R.p(f"Сыграли бы ещё: <b>{yes*100:.0f}%</b>"); M["play_again_pct"] = yes * 100
    tx = a[(a["typ"] == "text") & a["val"].astype(str).str.strip().ne("") & a["val"].notna()]
    if len(tx):
        R.p("Свободные ответы:")
        for qid, g in tx.groupby("qid"):
            R.raw(f"<h3>{html.escape(str(qid))}</h3><ul>" + "".join(f"<li>{html.escape(str(v))} <span class='muted'>({html.escape(s[-9:])})</span></li>" for s, v in zip(g["sid"], g["val"])) + "</ul>")


def s_features(R, D, cfg, M):
    E = D.events
    R.h2("features", "8. Использование фич и экранов")
    names = cfg.get("feature_events") or []
    nS = max(1, D.sessions["session_id"].nunique())
    rows = []
    for n in names:
        g = E[E["name"] == n]
        rows.append({"событие": n, "всего": len(g), "сессий": g["sid"].nunique(), "% сессий": g["sid"].nunique() / nS * 100})
    # any event name starting with feature prefixes
    top = E[~E["cat"].isin(["perf", "net", "breadcrumb", "hitch", "session", "level", "error", "survey", "marker"])]["name"].value_counts().head(40)
    T = pd.DataFrame(rows)
    if len(T): R.table(T.sort_values("всего", ascending=False))
    R.p("Все игровые события по частоте:")
    t2 = pd.DataFrame({"событие": top.index, "всего": top.values,
                       "сессий": [E[E["name"] == n]["sid"].nunique() for n in top.index]})
    t2["% сессий"] = t2["сессий"] / nS * 100
    fig, ax = plt.subplots(figsize=(8, max(3, .24 * len(top) + 1)))
    bar_h(ax, list(top.index), list(top.values)); ax.set_title("частота игровых событий")
    R.fig(fig, "features", "события")
    R.table(t2, maxrows=40)
    ui = ev(E, "ui_screen")
    if len(ui):
        g = ui.assign(s=num(ui, "p_seconds_prev")).groupby("p_from")["s"].agg(["count", "sum", "median"]).sort_values("sum", ascending=False)
        R.p("Время на экранах UI (по from: сколько провели на экране перед уходом):")
        R.table(g.reset_index().rename(columns={"p_from": "экран", "count": "заходов", "sum": "сумма, с", "median": "медиана, с"}))
    sc = ev(E, "setting_change")
    if len(sc):
        R.p("Что меняли в настройках:"); R.table(sc.groupby(["p_key", "p_to"]).size().rename("раз").reset_index().sort_values("раз", ascending=False))
    gl = ev(E, "game_log_counts")
    if len(gl) and "p_tags" in gl:
        tot = {}
        for d in gl["p_tags"]:
            if isinstance(d, dict):
                for k, v in d.items(): tot[k] = tot.get(k, 0) + (v or 0)
        if tot:
            R.p("Игровые лог-теги (ANTxxx) за все сессии:"); R.table(pd.DataFrame(sorted(tot.items(), key=lambda x: -x[1]), columns=["тег", "кол-во"]))


def s_perf(R, D, cfg, mt, M):
    E, S = D.events, D.sessions
    R.h2("perf", "9. Производительность")
    p = ev(E, "perf_sample").copy()
    if p.empty:
        R.p("Нет perf_sample.", "muted"); return
    for c in ("p_fps_avg", "p_fps_min", "p_fps_p1", "p_frame_ms", "p_gpu_ms", "p_game_ms", "p_render_ms", "p_mem_mb", "p_hitches"):
        p[c] = num(p, c)
    g = p.groupby("sid").agg(fps_p50=("p_fps_avg", "median"), fps_p1=("p_fps_p1", lambda s: s.quantile(.01) if s.notna().any() else np.nan),
                             fps_min=("p_fps_min", "min"), game_ms=("p_game_ms", "median"), render_ms=("p_render_ms", "median"),
                             gpu_ms=("p_gpu_ms", "median"), mem_mb=("p_mem_mb", "max"), hitches=("p_hitches", "sum"), role=("c_role", "first")).reset_index()
    M["fps_p50"] = float(p["p_fps_avg"].median()); M["fps_p1"] = float(p["p_fps_p1"].quantile(.01)) if p["p_fps_p1"].notna().any() else np.nan
    pf = cfg.get("perf") or {}
    R.raw('<div class="kpis">' + kpi("fps медиана", f"{M['fps_p50']:.0f}", "ok" if M["fps_p50"] >= pf.get("fps_good", 60) else "warn") +
          kpi("fps p1", f"{M['fps_p1']:.0f}" if not math.isnan(M["fps_p1"]) else "-") + kpi("пиков >%dмс" % pf.get("hitch_ms", 100), int((E["name"] == "hitch").sum())) +
          kpi("память max, МБ", f"{p['p_mem_mb'].max():.0f}") + '</div>')
    R.table(g)
    if "p_ptk_ms_avg" in p:
        oa, om = num(p, "p_ptk_ms_avg"), num(p, "p_ptk_ms_max")
        tu = num(p, "p_ptk_track_us")
        M["ptk_overhead_ms"] = float(oa.mean())
        R.p(f"Накладные расходы самого PlaytestKit (измеряет плагин): среднее <b>{oa.mean():.4f} мс/кадр</b>, худший кадр {om.max():.3f} мс, "
            f"Track() ~{tu.mean():.1f} мкс на событие. Бюджет 0.1 мс/кадр: " + ("<span class='ok'>в норме</span>" if oa.mean() < 0.1 else "<span class='bad'>превышен</span>"))
    fig, axs = plt.subplots(1, 3, figsize=(11, 3.2))
    axs[0].hist(p["p_fps_avg"].dropna(), bins=30, color=C_ACC); axs[0].set_title("fps (avg по 5 с окнам)")
    axs[1].hist(p["p_frame_ms"].dropna().clip(upper=60), bins=30, color=C_WARN); axs[1].set_title("frame ms (clip 60)")
    parts = [p["p_game_ms"].dropna(), p["p_render_ms"].dropna(), p["p_gpu_ms"].dropna()]
    axs[2].boxplot([x for x in parts if len(x)], showfliers=False); axs[2].set_xticks(range(1, 1 + sum(1 for x in parts if len(x))))
    axs[2].set_xticklabels([n for n, x in zip(["game", "render", "gpu"], parts) if len(x)]); axs[2].set_title("ms по потокам (без выбросов)")
    R.fig(fig, "perf_dist", "perf")
    # fps over time by session
    fig, ax = plt.subplots(figsize=(11, 3.2))
    for s, d in p.groupby("sid"):
        ax.plot(d["t"] / 60000.0, d["p_fps_avg"], lw=1, label=s[-9:])
    ax.set_xlabel("минуты"); ax.set_ylabel("fps"); ax.set_title("fps во времени"); ax.legend(fontsize=6, ncol=4)
    R.fig(fig, "perf_time", "fps во времени")
    h = ev(E, "hitch").copy()
    if len(h):
        h["ms"] = num(h, "p_ms")
        fig, ax = mt.axes()
        hx, hy = num(h, "c_pos_x"), num(h, "c_pos_y")
        okm = hx.notna()
        ax.scatter(hx[okm], hy[okm], s=h["ms"][okm].clip(30, 800) / 8, c=C_BAD, alpha=.55, edgecolors="white", linewidths=.4)
        ax.set_title(f"где фризы (>{pf.get('hitch_ms', 100)} мс, размер = длительность), n={len(h)}")
        R.fig(fig, "hitch_map", "фризы")
        cell = cfg.get("areas_for_hitches", 600)
        h["cx"] = (hx // cell * cell); h["cy"] = (hy // cell * cell)
        top = h.dropna(subset=["cx"]).groupby(["cx", "cy"])["ms"].agg(["count", "max"]).sort_values("count", ascending=False).head(10).reset_index()
        R.p("Топ областей по фризам:"); R.table(top.rename(columns={"cx": "x0", "cy": "y0", "count": "фризов", "max": "макс, мс"}))
        if "p_loading" in h:
            R.p(f"Из них при загрузке: {int(h['p_loading'].fillna(False).astype(bool).sum())}", "muted")
    lv = ev(E, "level_start")
    if len(lv) and "p_load_ms" in lv:
        R.p("Загрузка карт, мс: " + ", ".join(f"{k}: {v:.0f}" for k, v in lv.assign(l=num(lv, "p_load_ms")).groupby("p_map")["l"].median().items()))


def s_coop(R, D, cfg, M):
    E, S = D.events, D.sessions
    R.h2("coop", "10. Кооп: здоровье соединения")
    n = ev(E, "net_sample").copy()
    if n.empty:
        R.p("Нет net_sample (одиночные сессии или клиент без соединения).", "muted"); return
    for c in ("p_ping_avg", "p_ping_max", "p_loss_in_pct", "p_loss_out_pct", "p_in_kbps", "p_out_kbps", "p_conns"):
        n[c] = num(n, c)
    g = n.groupby("sid").agg(роль=("p_role", "first"), игроков_макс=("p_conns", "max"), ping_p50=("p_ping_avg", "median"),
                             ping_p95=("p_ping_avg", lambda s: s.quantile(.95)), ping_max=("p_ping_max", "max"),
                             loss_in_p95=("p_loss_in_pct", lambda s: s.quantile(.95)), loss_out_p95=("p_loss_out_pct", lambda s: s.quantile(.95)),
                             in_kbps=("p_in_kbps", "median"), out_kbps=("p_out_kbps", "median")).reset_index()
    R.table(g)
    fig, axs = plt.subplots(1, 3, figsize=(11, 3))
    for s, d in n.groupby("sid"): axs[0].plot(d["t"] / 60000.0, d["p_ping_avg"], lw=1)
    axs[0].set_title("ping, мс"); axs[0].set_xlabel("мин")
    axs[1].hist(n["p_loss_in_pct"].dropna().clip(upper=20), bins=25, color=C_WARN); axs[1].set_title("loss in, % (clip 20)")
    axs[2].hist(n["p_out_kbps"].dropna(), bins=25, color=C_ACC); axs[2].set_title("out kbps")
    R.fig(fig, "coop", "кооп")
    drops = ev(E, "net_failure", "net_disconnect")
    if len(drops): R.p(f"Обрывов/сбоев сети: {len(drops)} в {drops['sid'].nunique()} сессиях.")


def s_bugs(R, D, cfg, M):
    E = D.events
    R.h2("bugs", "11. Топ багов (severity x frequency)")
    nS = max(1, D.sessions["session_id"].nunique())
    rows = []

    def norm(s): 
        import re
        return re.sub(r"0x[0-9a-fA-F]+|\d+", "#", str(s))[:120]
    cr = ev(E, "crash")
    for k, g in cr.groupby(col(cr, "p_msg", "?").map(norm)):
        rows.append(("crash", k, 4, g["sid"].nunique(), len(g), ""))
    mk = ev(E, "marker"); mk = mk[col(mk, "p_tag") == "bug"]
    for k, g in mk.groupby(col(mk, "p_text", "").fillna("").str.lower().str.strip().str[:70]):
        rows.append(("marker:bug", k or "(без текста)", 3, g["sid"].nunique(), len(g), ""))
    er = ev(E, "error")
    for k, g in er.groupby(col(er, "p_msg", "?").map(norm)):
        rows.append(("log error", k, 2, g["sid"].nunique(), len(g), col(g, "p_log_cat", "").iloc[0]))
    ass = ev(E, "ensure")
    for k, g in ass.groupby(col(ass, "p_msg", "?").map(norm)):
        rows.append(("ensure", k, 3, g["sid"].nunique(), len(g), ""))
    if not rows:
        R.p("Багов, крашей и ошибок лога не зафиксировано.", "ok"); return
    T = pd.DataFrame(rows, columns=["тип", "что", "severity(1-4)", "сессий", "раз", "категория"])
    T["freq %"] = T["сессий"] / nS * 100
    T["score"] = T["severity(1-4)"] * (1 + np.log1p(T["сессий"])) * (1 + T["freq %"] / 100)
    T = T.sort_values("score", ascending=False)
    R.p("severity: crash=4, ensure/bug-маркер=3, ошибка лога=2. score = severity x частота; ручной триаж см. docs/PLAYTEST_SYSTEM.md.")
    R.table(T, maxrows=40)
    g = ev(E, "game_log_counts")
    if len(g): pass


# ----------------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(description="PlaytestKit HTML report")
    ap.add_argument("source", help="folder | .zip | 'supabase'")
    ap.add_argument("--config", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "projects", "ant-kitchen.json"))
    ap.add_argument("--out", default="ptk_report.html")
    ap.add_argument("--url"); ap.add_argument("--key"); ap.add_argument("--project"); ap.add_argument("--since")
    ap.add_argument("--build", help="only this build version")
    a = ap.parse_args()
    cfg = json.load(open(a.config, encoding="utf-8")) if os.path.exists(a.config) else {}
    base = os.path.dirname(os.path.abspath(a.config)) if os.path.exists(a.config) else "."
    base = os.path.dirname(base) if os.path.basename(base) == "projects" else base
    if a.source == "supabase":
        key = a.key or os.environ.get("PTK_SERVICE_KEY"); url = a.url or os.environ.get("PTK_SUPABASE_URL")
        D = ptk_loader.load_supabase(url, key, a.project or cfg.get("project"), a.since)
    else:
        D = ptk_loader.load_folder(a.source)
    if a.build:
        keep = D.sessions[D.sessions["build"].astype(str) == a.build]["session_id"]
        D.sessions = D.sessions[D.sessions["session_id"].isin(keep)]; D.events = D.events[D.events["sid"].isin(keep)]
    mt = MapT(cfg, base)
    R = Report(a.out, cfg.get("title", "PlaytestKit"))
    M = {}
    for fn in (s_overview, s_funnel, s_deaths, s_economy, s_markers, s_surveys, s_features, s_perf, s_coop, s_bugs):
        try:
            if fn in (s_deaths, s_markers, s_perf):
                fn(R, D, cfg, mt, M)
            else:
                fn(R, D, cfg, M)
        except Exception as ex:
            import traceback; traceback.print_exc()
            R.raw(f'<p class="bad">секция {fn.__name__} упала: {html.escape(repr(ex))}</p>')
    # hypotheses need metrics from the other sections: build it, then move it to the top
    n_before = len(R.parts)
    s_hypotheses(R, D, cfg, M)
    hy = R.parts[n_before:]; del R.parts[n_before:]
    if hy:
        at = next((i for i, x in enumerate(R.parts) if 'id="funnel"' in x), 1)
        R.parts[at:at] = hy
        R.toc.insert(1, R.toc.pop())
    R.save(f"{D.source} · сессий {len(D.sessions)} · {pd.Timestamp.now().strftime('%Y-%m-%d %H:%M')}")
    print("report:", a.out, "| charts:", R.chart_dir)


if __name__ == "__main__":
    main()
