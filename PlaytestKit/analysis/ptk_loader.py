"""PlaytestKit loaders: folder / zip(s) / Supabase  ->  Data(sessions, events, shots).

Normalised shapes
  sessions : DataFrame, one row per session (session_id, anon_id, role, started, ended, build, platform, machine, flags, counters...)
  events   : DataFrame, columns sid, seq, t (ms since session start), ts, name, cat, p_<prop>..., c_<ctx>..., c_pos_x, c_pos_y
  shots    : {(sid, file_name): callable -> bytes}
"""
import io, json, os, re, zipfile, urllib.request, urllib.parse
from dataclasses import dataclass, field
import pandas as pd


@dataclass
class Data:
    sessions: pd.DataFrame
    events: pd.DataFrame
    shots: dict = field(default_factory=dict)
    source: str = ""


def _jl(text):
    out = []
    for ln in text.splitlines():
        ln = ln.strip()
        if not ln:
            continue
        try:
            out.append(json.loads(ln))
        except Exception:
            pass  # torn last line after a crash is expected
    return out


def _flat(prefix, d, out):
    for k, v in (d or {}).items():
        if k == "pos" and isinstance(v, dict):
            out[prefix + "pos_x"] = v.get("x"); out[prefix + "pos_y"] = v.get("y"); out[prefix + "pos_z"] = v.get("z")
        elif isinstance(v, (dict, list)):
            out[prefix + k] = v
        else:
            out[prefix + k] = v


def _event_rows(sid, lines, session_ctx=None):
    rows, last_ctx = [], {}
    for e in lines:
        r = {"sid": sid, "seq": e.get("seq"), "t": e.get("t"), "ts": e.get("ts"), "name": e.get("name"), "cat": e.get("cat")}
        _flat("p_", e.get("props"), r)
        ctx = e.get("ctx") or {}
        # compact ctx on breadcrumbs inherits what it omitted from the previous full ctx
        merged = dict(last_ctx); merged.update(ctx)
        if e.get("name") != "breadcrumb":
            last_ctx = {k: v for k, v in merged.items() if k not in ("pos", "fps")}
        _flat("c_", merged, r)
        rows.append(r)
    return rows


def _session_row(sid, meta, n_events):
    meta = meta or {}
    b, p = meta.get("build") or {}, meta.get("player") or {}
    m = {}
    st = meta.get("started")
    en = meta.get("ended")
    row = {"session_id": sid, "anon_id": p.get("anon_id") or sid, "role": p.get("role"), "started": st, "ended": en,
           "build": b.get("version"), "git_sha": b.get("git_sha"), "platform": b.get("platform"), "config": b.get("config"),
           "flags": meta.get("flags"), "n_events": n_events}
    for k, v in (meta.get("counters") or {}).items():
        row["cnt_" + k] = v
    return row


def load_folder(path):
    """path: a Saved/Playtest folder, a folder of them, a .zip, or a folder with zips (collect_playtest.ps1 output)."""
    sess, evs, shots = [], [], {}

    def add(sid, meta, ev_text, shot_getters):
        lines = _jl(ev_text)
        if not lines:
            return
        rows = _event_rows(sid, lines)
        evs.extend(rows)
        sess.append(_session_row(sid, meta, len(rows)))
        for nm, g in shot_getters.items():
            shots[(sid, nm)] = g

    def from_zip(zpath):
        with zipfile.ZipFile(zpath) as z:
            names = z.namelist()
            for n in names:
                if n.endswith("events.jsonl"):
                    base = n[: -len("events.jsonl")]
                    sid = base.rstrip("/").split("/")[-1] or os.path.basename(zpath)
                    meta = None
                    if base + "session.json" in names:
                        try: meta = json.loads(z.read(base + "session.json").decode("utf-8-sig"))
                        except Exception: pass
                    getters = {}
                    for s in names:
                        if s.startswith(base + "shots/") and not s.endswith("/"):
                            getters[os.path.basename(s)] = (lambda zp=zpath, nn=s: zipfile.ZipFile(zp).read(nn))
                    add(sid, meta, z.read(n).decode("utf-8-sig", "replace"), getters)

    if os.path.isfile(path) and path.lower().endswith(".zip"):
        from_zip(path)
    else:
        for root, dirs, files in os.walk(path):
            for f in files:
                fp = os.path.join(root, f)
                if f == "events.jsonl":
                    sid = os.path.basename(root)
                    meta = None
                    mp = os.path.join(root, "session.json")
                    if os.path.exists(mp):
                        try: meta = json.load(open(mp, encoding="utf-8-sig"))
                        except Exception: pass
                    getters = {}
                    sd = os.path.join(root, "shots")
                    if os.path.isdir(sd):
                        for s in os.listdir(sd):
                            getters[s] = (lambda q=os.path.join(sd, s): open(q, "rb").read())
                    add(sid, meta, open(fp, encoding="utf-8-sig", errors="replace").read(), getters)
                elif f.lower().endswith(".zip"):
                    try: from_zip(fp)
                    except Exception as ex: print("skip zip", fp, ex)
    return _finish(sess, evs, shots, path)


def _rest(url, key, table, params, schema="ptk"):
    out, off, page = [], 0, 1000
    while True:
        q = dict(params); q["offset"] = off; q["limit"] = page
        req = urllib.request.Request(f"{url}/rest/v1/{table}?" + urllib.parse.urlencode(q),
                                     headers={"apikey": key, "Authorization": "Bearer " + key, "Accept-Profile": schema})
        with urllib.request.urlopen(req, timeout=60) as r:
            chunk = json.loads(r.read().decode("utf-8"))
        out += chunk
        if len(chunk) < page:
            return out
        off += page


def load_supabase(url, key, project, since=None):
    """Service-role key required (reads ptk.* which anon/ingest roles cannot). Never ship this key to testers."""
    url = url.rstrip("/")
    flt = {"project": "eq." + project}
    if since:
        flt["started_at"] = "gte." + since
    srows = _rest(url, key, "sessions", dict(flt, select="*", order="started_at.asc"))
    sess, evs = [], []
    for s in srows:
        sid = s["session_id"]
        e = _rest(url, key, "events", {"session_id": "eq." + sid, "select": "seq,t_ms,ts,name,cat,props,ctx", "order": "seq.asc"})
        lines = [{"seq": x["seq"], "t": x["t_ms"], "ts": x["ts"], "name": x["name"], "cat": x["cat"], "props": x["props"], "ctx": x["ctx"]} for x in e]
        evs.extend(_event_rows(sid, lines))
        meta = s.get("meta") or {}
        sess.append(_session_row(sid, meta, len(lines)))
    shots = {}
    try:
        for f in _rest(url, key, "files", {"select": "session_id,name,path", "name": "like.shots*"}):
            def g(p=f["path"]):
                rq = urllib.request.Request(f"{url}/storage/v1/object/ptk-files/{p}", headers={"apikey": key, "Authorization": "Bearer " + key})
                return urllib.request.urlopen(rq, timeout=60).read()
            shots[(f["session_id"], os.path.basename(f["name"]))] = g
    except Exception as ex:
        print("files not loaded:", ex)
    return _finish(sess, evs, shots, "supabase:" + url)


def _finish(sess, evs, shots, src):
    S = pd.DataFrame(sess)
    E = pd.DataFrame(evs)
    if E.empty:
        raise SystemExit("no events found in " + src)
    E["t"] = pd.to_numeric(E["t"], errors="coerce")
    E["tsd"] = pd.to_datetime(E["ts"], errors="coerce", utc=True)
    E = E.sort_values(["sid", "seq"]).reset_index(drop=True)
    for c in ("started", "ended"):
        if c in S:
            S[c] = pd.to_datetime(S[c], errors="coerce", utc=True)
    # session length: from the last event when session.json lacks `ended` (crash)
    last = E.groupby("sid")["t"].max() / 1000.0
    S["seconds"] = S["session_id"].map(last)
    return Data(S, E, shots, src)
