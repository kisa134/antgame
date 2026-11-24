// PlaytestKit ingest (Supabase Edge Function, Deno).
//   POST /ingest            body: batch JSON (optionally gzip: Content-Encoding: gzip) -> {ok, accepted, ...}
//   POST /ingest/file?session=ID&name=m12.jpg   body: image/jpeg -> storage bucket ptk-files
// Auth: header x-ptk-key must equal the secret PTK_INGEST_KEY (set it with `supabase secrets set`). Deploy with --no-verify-jwt.
// DB role: service role by default; if PTK_INGEST_JWT is set (JWT with role=ptk_ingest) the function acts as that INSERT-only role.
// Limits: body <= 1 MB (after gunzip), <= 600 events, per anon_id rate limit (best effort, per isolate), project allow-list.
import { createClient } from "https://esm.sh/@supabase/supabase-js@2.45.4";

const SB_URL = Deno.env.get("SUPABASE_URL")!;
const KEY = Deno.env.get("PTK_INGEST_JWT") ?? Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!;
const INGEST_KEY = Deno.env.get("PTK_INGEST_KEY") ?? "";
const ALLOWED = (Deno.env.get("PTK_ALLOWED_PROJECTS") ?? "").split(",").map((s) => s.trim()).filter(Boolean);
const RATE_PER_MIN = Number(Deno.env.get("PTK_RATE_PER_MIN") ?? "6000"); // events per anon id per minute
const MAX_BODY = 1_000_000;
const MAX_EVENTS = 600;

const sb = createClient(SB_URL, KEY, { auth: { persistSession: false }, db: { schema: "ptk" } });
const sbStorage = createClient(SB_URL, KEY, { auth: { persistSession: false } });

const rate = new Map<string, { n: number; t: number }>();
function rateOk(anon: string, n: number): boolean {
  const now = Date.now();
  const r = rate.get(anon);
  if (!r || now - r.t > 60_000) { rate.set(anon, { n, t: now }); return n <= RATE_PER_MIN; }
  r.n += n;
  return r.n <= RATE_PER_MIN;
}

const J = (status: number, body: unknown) =>
  new Response(JSON.stringify(body), { status, headers: { "content-type": "application/json" } });

async function readBody(req: Request): Promise<Uint8Array | null> {
  let stream = req.body;
  if (!stream) return new Uint8Array();
  if ((req.headers.get("content-encoding") ?? "").toLowerCase() === "gzip") {
    stream = stream.pipeThrough(new DecompressionStream("gzip"));
  }
  const reader = stream.getReader();
  const chunks: Uint8Array[] = [];
  let total = 0;
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    total += value.length;
    if (total > MAX_BODY) return null;
    chunks.push(value);
  }
  const out = new Uint8Array(total);
  let o = 0;
  for (const c of chunks) { out.set(c, o); o += c.length; }
  return out;
}

const RE_PROJECT = /^[a-z0-9][a-z0-9-]{1,39}$/;
const RE_SESSION = /^[A-Za-z0-9_-]{8,64}$/;
const RE_NAME = /^[a-z][a-z0-9_]{1,47}$/;
const RE_CAT = /^[a-z][a-z0-9_]{1,23}$/;

// ts of a line without "ts" (breadcrumbs): session start + t
function lineTs(l: any, startedMs: number): string {
  if (typeof l.ts === "string" && !isNaN(Date.parse(l.ts))) return l.ts;
  return new Date(startedMs + Number(l.t ?? 0)).toISOString();
}

Deno.serve(async (req: Request) => {
  if (req.method !== "POST") return J(405, { ok: false, error: "POST only" });
  if (INGEST_KEY && req.headers.get("x-ptk-key") !== INGEST_KEY) return J(401, { ok: false, error: "bad key" });
  const url = new URL(req.url);

  // ---------------------------------------------------------------- files
  if (url.pathname.endsWith("/file")) {
    const session = url.searchParams.get("session") ?? "";
    const name = url.searchParams.get("name") ?? "";
    if (!RE_SESSION.test(session) || !/^[A-Za-z0-9_.-]{1,64}\.jpg$/.test(name)) return J(400, { ok: false, error: "bad session/name" });
    const buf = await readBody(req);
    if (!buf || buf.length === 0) return J(413, { ok: false, error: "empty or too large" });
    const path = `${session}/${name}`;
    const up = await sbStorage.storage.from("ptk-files").upload(path, buf, { contentType: "image/jpeg", upsert: true });
    if (up.error) return J(500, { ok: false, error: up.error.message });
    await sb.from("files").upsert({ session_id: session, name, path, bytes: buf.length }, { onConflict: "session_id,name", ignoreDuplicates: true });
    return J(200, { ok: true });
  }

  // ---------------------------------------------------------------- batch
  const raw = await readBody(req);
  if (!raw) return J(413, { ok: false, error: "body too large" });
  let b: any;
  try { b = JSON.parse(new TextDecoder().decode(raw)); } catch { return J(400, { ok: false, error: "bad json" }); }
  if (b?.schema_v !== 1) return J(400, { ok: false, error: "schema_v must be 1" });
  if (!RE_PROJECT.test(b.project ?? "")) return J(400, { ok: false, error: "bad project" });
  if (ALLOWED.length && !ALLOWED.includes(b.project)) return J(403, { ok: false, error: "project not allowed" });
  if (!RE_SESSION.test(b.session_id ?? "")) return J(400, { ok: false, error: "bad session_id" });
  if (!Array.isArray(b.events) || b.events.length > MAX_EVENTS) return J(400, { ok: false, error: "events missing or > " + MAX_EVENTS });

  const meta = b.session ?? null;
  const anon: string = meta?.player?.anon_id ?? b.player?.anon_id ?? "";
  if (anon && !rateOk(anon, b.events.length)) return J(429, { ok: false, error: "rate limit" });

  // the first batch of a session carries session.json: create build + session row (insert-only, duplicates ignored)
  if (meta) {
    if (!RE_PROJECT.test(meta.project ?? "") || meta.project !== b.project || meta.session_id !== b.session_id) return J(400, { ok: false, error: "session meta mismatch" });
    if (typeof meta.player?.anon_id !== "string" || meta.player.anon_id.length < 8) return J(400, { ok: false, error: "bad anon_id" });
    const bd = meta.build ?? {};
    const bid = await sb.rpc("upsert_build", { p_project: b.project, p_version: String(bd.version ?? ""), p_sha: String(bd.git_sha ?? ""), p_platform: String(bd.platform ?? ""), p_config: String(bd.config ?? "") });
    if (bid.error) return J(500, { ok: false, error: "build: " + bid.error.message });
    const ins = await sb.from("sessions").insert({
      session_id: b.session_id, project: b.project, build_id: bid.data, anon_id: meta.player.anon_id, role: meta.player.role ?? null, steam: meta.player.steam ?? null,
      started_at: meta.started ?? null, ended_at: meta.ended ?? null, platform: bd.platform ?? null, meta,
    }, { count: undefined }).select().maybeSingle();
    // duplicate session = fine (retry); any other error is real
    if (ins.error && !/duplicate key|already exists|23505/.test(ins.error.message + (ins.error.code ?? ""))) {
      // `select()` needs SELECT in hardened mode: retry without returning
      const ins2 = await sb.from("sessions").insert({
        session_id: b.session_id, project: b.project, build_id: bid.data, anon_id: meta.player.anon_id, role: meta.player.role ?? null, steam: meta.player.steam ?? null,
        started_at: meta.started ?? null, ended_at: meta.ended ?? null, platform: bd.platform ?? null, meta,
      });
      if (ins2.error && ins2.error.code !== "23505") return J(500, { ok: false, error: "session: " + ins2.error.message });
    }
  }

  const startedMs = meta?.started ? Date.parse(meta.started) : Date.now() - 3600_000;
  const rows: any[] = [];
  let bad = 0;
  for (const l of b.events) {
    if (!Number.isInteger(l?.seq) || l.seq < 1 || !RE_NAME.test(l?.name ?? "") || !RE_CAT.test(l?.cat ?? "") || typeof l?.t !== "number") { bad++; continue; }
    const props = l.props && typeof l.props === "object" ? l.props : {};
    if (JSON.stringify(props).length > 4000) { bad++; continue; }
    rows.push({ session_id: b.session_id, seq: l.seq, t_ms: Math.max(0, Math.round(l.t)), ts: lineTs(l, startedMs), name: l.name, cat: l.cat, props, ctx: l.ctx ?? {} });
  }
  if (rows.length) {
    // ON CONFLICT DO NOTHING via ignoreDuplicates: a retried batch is idempotent
    const r = await sb.from("events").upsert(rows, { onConflict: "session_id,seq", ignoreDuplicates: true });
    if (r.error) {
      // the session row may be missing (meta was lost): ask the SDK to resend with meta? we return 409 so it is retried by the SDK later.
      if (r.error.code === "23503") return J(409, { ok: false, error: "unknown session; send session meta first" });
      return J(500, { ok: false, error: r.error.message });
    }
  }
  return J(200, { ok: true, accepted: rows.length, rejected: bad });
});
