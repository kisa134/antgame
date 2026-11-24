-- PlaytestKit backend schema v1 (Supabase / plain Postgres 15+).
-- Apply with:  supabase db push   (or paste into the SQL editor of a project YOU chose).
-- Nothing here creates a project; see PlaytestKit/backend/README.md.

create schema if not exists ptk;

-- ------------------------------------------------------------------ roles
-- `ptk_ingest` is the role the ingest function acts as. It can INSERT and nothing else (no select, update, delete).
-- The function runs with the service role by default (RLS is bypassed); for the hardened mode see README: it signs a JWT with role=ptk_ingest.
do $$ begin
  if not exists (select 1 from pg_roles where rolname = 'ptk_ingest') then
    create role ptk_ingest nologin;
  end if;
end $$;
grant usage on schema ptk to ptk_ingest;
grant ptk_ingest to authenticator;   -- lets PostgREST switch to it for JWTs that carry role=ptk_ingest

-- ------------------------------------------------------------------ tables
create table if not exists ptk.projects (
  id          text primary key check (id ~ '^[a-z0-9][a-z0-9-]{1,39}$'),
  name        text,
  created_at  timestamptz not null default now()
);

create table if not exists ptk.builds (
  id          bigint generated always as identity primary key,
  project     text not null references ptk.projects(id),
  version     text not null,
  git_sha     text not null default '',
  platform    text not null default '',
  config      text not null default '',
  first_seen  timestamptz not null default now(),
  unique (project, version, git_sha, platform, config)
);

create table if not exists ptk.sessions (
  session_id  text primary key,
  project     text not null references ptk.projects(id),
  build_id    bigint references ptk.builds(id),
  anon_id     text not null,
  role        text,
  steam       text,
  started_at  timestamptz,
  ended_at    timestamptz,
  platform    text,
  meta        jsonb not null default '{}'::jsonb,   -- the whole session.json
  received_at timestamptz not null default now()
);
create index if not exists sessions_project_started on ptk.sessions (project, started_at desc);
create index if not exists sessions_anon on ptk.sessions (anon_id);

create table if not exists ptk.events (
  session_id  text not null references ptk.sessions(session_id) on delete cascade,
  seq         integer not null,
  t_ms        bigint not null,
  ts          timestamptz not null,
  name        text not null,
  cat         text not null,
  props       jsonb not null default '{}'::jsonb,
  ctx         jsonb not null default '{}'::jsonb,
  -- hot context columns for fast filters / heatmaps
  map         text generated always as (ctx ->> 'map') stored,
  raid_no     integer generated always as (nullif(ctx ->> 'raid_no', '')::integer) stored,
  pos_x       real generated always as (nullif(ctx #>> '{pos,x}', '')::real) stored,
  pos_y       real generated always as (nullif(ctx #>> '{pos,y}', '')::real) stored,
  primary key (session_id, seq)               -- idempotency: a retried batch is a no-op
);
create index if not exists events_name_ts on ptk.events (name, ts);
create index if not exists events_cat_ts on ptk.events (cat, ts);
create index if not exists events_session_t on ptk.events (session_id, t_ms);
create index if not exists events_props_gin on ptk.events using gin (props jsonb_path_ops);

-- markers and survey answers are copied out of the event stream by the trigger below (easier to query / review)
create table if not exists ptk.markers (
  session_id  text not null references ptk.sessions(session_id) on delete cascade,
  seq         integer not null,
  ts          timestamptz not null,
  tag         text not null,
  note        text,
  shot        text,
  map         text,
  raid_no     integer,
  pos_x       real,
  pos_y       real,
  props       jsonb not null default '{}'::jsonb,
  triage      jsonb not null default '{}'::jsonb,   -- filled by humans: {severity, frequency, owner, status}
  primary key (session_id, seq)
);
create index if not exists markers_tag on ptk.markers (tag, ts);

create table if not exists ptk.survey_answers (
  session_id  text not null references ptk.sessions(session_id) on delete cascade,
  seq         integer not null,
  ts          timestamptz not null,
  survey_id   text not null,
  qid         text not null,
  type        text,
  value_text  text,
  value_num   double precision,
  primary key (session_id, seq)
);
create index if not exists survey_qid on ptk.survey_answers (survey_id, qid);

create table if not exists ptk.files (
  session_id  text not null references ptk.sessions(session_id) on delete cascade,
  name        text not null,
  bucket      text not null default 'ptk-files',
  path        text not null,
  bytes       integer,
  created_at  timestamptz not null default now(),
  primary key (session_id, name)
);

-- ------------------------------------------------------------------ copy markers / answers out of events
create or replace function ptk.fan_out() returns trigger language plpgsql security definer set search_path = ptk, public as $$
begin
  if new.name = 'marker' then
    insert into ptk.markers (session_id, seq, ts, tag, note, shot, map, raid_no, pos_x, pos_y, props)
    values (new.session_id, new.seq, new.ts, coalesce(new.props ->> 'tag', 'bug'), new.props ->> 'text', new.props ->> 'shot', new.map, new.raid_no, new.pos_x, new.pos_y, new.props)
    on conflict do nothing;
  elsif new.name = 'survey_answer' then
    insert into ptk.survey_answers (session_id, seq, ts, survey_id, qid, type, value_text, value_num)
    values (new.session_id, new.seq, new.ts, coalesce(new.props ->> 'survey_id', ''), coalesce(new.props ->> 'qid', ''), new.props ->> 'type',
            coalesce(new.props ->> 'text', new.props ->> 'value'), nullif(new.props ->> 'num', '')::double precision)
    on conflict do nothing;
  end if;
  return new;
end $$;
drop trigger if exists events_fan_out on ptk.events;
create trigger events_fan_out after insert on ptk.events for each row execute function ptk.fan_out();

-- ------------------------------------------------------------------ RLS: insert-only for the ingest role, nothing for anon / authenticated
alter table ptk.projects        enable row level security;
alter table ptk.builds          enable row level security;
alter table ptk.sessions        enable row level security;
alter table ptk.events          enable row level security;
alter table ptk.markers         enable row level security;
alter table ptk.survey_answers  enable row level security;
alter table ptk.files           enable row level security;

revoke all on all tables in schema ptk from anon, authenticated, public;
grant insert on ptk.projects, ptk.builds, ptk.sessions, ptk.events, ptk.files to ptk_ingest;
grant usage, select on all sequences in schema ptk to ptk_ingest;
-- (no select/update/delete grants: ON CONFLICT DO NOTHING needs none; the builds lookup goes through the function below)

drop policy if exists ptk_ingest_insert on ptk.projects;       create policy ptk_ingest_insert on ptk.projects        for insert to ptk_ingest with check (true);
drop policy if exists ptk_ingest_insert on ptk.builds;         create policy ptk_ingest_insert on ptk.builds          for insert to ptk_ingest with check (true);
drop policy if exists ptk_ingest_insert on ptk.sessions;       create policy ptk_ingest_insert on ptk.sessions        for insert to ptk_ingest with check (true);
drop policy if exists ptk_ingest_insert on ptk.events;         create policy ptk_ingest_insert on ptk.events          for insert to ptk_ingest with check (true);
drop policy if exists ptk_ingest_insert on ptk.files;          create policy ptk_ingest_insert on ptk.files           for insert to ptk_ingest with check (true);
-- markers / survey_answers are filled by the SECURITY DEFINER trigger only (no direct insert policy on purpose).

-- the build id lookup the function needs (security definer: the ingest role has no SELECT)
create or replace function ptk.upsert_build(p_project text, p_version text, p_sha text, p_platform text, p_config text)
returns bigint language plpgsql security definer set search_path = ptk, public as $$
declare v_id bigint;
begin
  insert into ptk.projects (id, name) values (p_project, p_project) on conflict do nothing;
  insert into ptk.builds (project, version, git_sha, platform, config) values (p_project, p_version, p_sha, p_platform, p_config) on conflict do nothing;
  select id into v_id from ptk.builds where project = p_project and version = p_version and git_sha = p_sha and platform = p_platform and config = p_config;
  return v_id;
end $$;
revoke all on function ptk.upsert_build(text, text, text, text, text) from public;
grant execute on function ptk.upsert_build(text, text, text, text, text) to ptk_ingest, service_role;

-- ------------------------------------------------------------------ analysis helpers (service role / dashboard only)
create or replace view ptk.v_sessions as
select s.*, b.version, b.git_sha, (extract(epoch from (coalesce(s.ended_at, s.received_at) - s.started_at)))::int as seconds
from ptk.sessions s left join ptk.builds b on b.id = s.build_id;

create or replace view ptk.v_deaths as
select e.session_id, e.ts, e.map, e.raid_no, e.pos_x, e.pos_y, e.props ->> 'cause' as cause, e.props ->> 'killer' as killer
from ptk.events e where e.name = 'death';

grant select on ptk.v_sessions, ptk.v_deaths to service_role;
grant all on all tables in schema ptk to service_role;
grant usage on schema ptk to service_role;

-- ------------------------------------------------------------------ storage bucket for screenshots (private)
insert into storage.buckets (id, name, public) values ('ptk-files', 'ptk-files', false) on conflict do nothing;
-- the function uploads with the service role / ingest role; nobody else gets a policy => no public access.
do $$ begin
  if not exists (select 1 from pg_policies where schemaname = 'storage' and tablename = 'objects' and policyname = 'ptk_ingest_upload') then
    create policy ptk_ingest_upload on storage.objects for insert to ptk_ingest with check (bucket_id = 'ptk-files');
  end if;
end $$;

-- Expose the schema to PostgREST only if you query from the dashboard/REST: Dashboard > Settings > API > Exposed schemas: add `ptk`.
