# PlaytestKit backend (Supabase) — деплой за 10 минут

Бэкенд **необязателен**. Без него плагин пишет всё локально в `Saved/Playtest/`, а `tools/collect_playtest.ps1` собирает папки тестеров в zip (см. ниже «Без бэкенда»).

Что здесь:

| Путь | Что |
|---|---|
| `supabase/migrations/20261124000000_playtestkit_v1.sql` | схема `ptk` (projects, builds, sessions, events, markers, survey_answers, files), RLS, роль `ptk_ingest` (только INSERT), bucket `ptk-files` (приватный), вьюхи `v_sessions`, `v_deaths` |
| `supabase/functions/ingest/index.ts` | Edge Function: принимает батчи (gzip), идемпотентно по `(session_id, seq)`, `/file` для скриншотов; ключ `x-ptk-key`, allow-list проектов, rate limit |

## Что нужно решить (ты)

1. **В какой Supabase-проект ставим.** Свободных нет (большинство на паузе). Варианты: (а) разбудить существующий проект и положить схему `ptk` рядом (схема изолирована, на чужие таблицы не влияет); (б) завести новый проект `playtest` (может стоить денег на платном плане; Free хватает для 5–20 тестеров). Я ничего не создавал и не трогал.
2. **Значение секрета** `PTK_INGEST_KEY` (любая длинная случайная строка). Он попадёт в сборку для тестеров, это «пароль на запись», а не секрет уровня service_role. Чтобы ротировать, поменяй секрет и пересобери сборку.

## Деплой (supabase CLI)

```powershell
npm i -g supabase            # или scoop install supabase
cd PlaytestKit\backend
supabase login
supabase link --project-ref <PROJECT_REF>       # ref из URL дашборда
supabase db push                                 # применит migrations/*.sql
supabase secrets set PTK_INGEST_KEY=<длинная_случайная_строка> PTK_ALLOWED_PROJECTS=ant-kitchen
supabase functions deploy ingest --no-verify-jwt # --no-verify-jwt обязателен: игра не умеет JWT, защита через x-ptk-key
```

Эндпоинт: `https://<PROJECT_REF>.supabase.co/functions/v1/ingest`

Проверка (должно вернуть `{"ok":true,...}`; 401 = неверный ключ):

```powershell
curl.exe -s -X POST "https://<REF>.supabase.co/functions/v1/ingest" -H "x-ptk-key: <KEY>" -H "x-ptk-project: ant-kitchen" -H "content-type: application/json" -d "{\"project\":\"ant-kitchen\",\"session_id\":\"test-0001\",\"events\":[]}"
```

### Подключить к игре

В `Config/DefaultPlaytestKit.ini` (в сборку для тестеров):

```ini
[/Script/PlaytestKit.PlaytestKitSettings]
Endpoint=https://<REF>.supabase.co/functions/v1/ingest
IngestKey=<KEY>
```

или без правки конфига: `-PTKEndpoint=https://.../ingest -PTKKey=<KEY>`. Пустой Endpoint = загрузка выключена.

### Усиленный режим (по желанию)

По умолчанию функция пишет через service_role. Чтобы функция могла только INSERT: создай JWT для роли `ptk_ingest` (секрет JWT проекта: Settings, API, JWT Secret), положи его в секрет `PTK_INGEST_JWT`. Тогда даже при утечке функции никто не прочитает данные других.

## Анализ

```powershell
$env:PTK_SUPABASE_URL = "https://<REF>.supabase.co"
$env:PTK_SERVICE_KEY  = "<service_role key>"   # только у тебя, тестерам НЕ давать
python PlaytestKit\analysis\ptk_report.py supabase --project ant-kitchen --out report.html
```

SQL для быстрых вопросов: `select * from ptk.v_sessions order by started_at desc;`, `select cause, count(*) from ptk.v_deaths group by 1;`

## Приватность и хранение

- Таблицы доступны только `service_role`; anon/authenticated ничего не видят (RLS без политик).
- Идентификатор игрока: случайный `anon_id`, не связан ни с Steam, ни с машиной. Steam ID, если включён `bHashSteamId`, хранится только как соленый SHA-1.
- Скриншоты: приватный bucket `ptk-files`, доступ через service_role.
- Удалить данные тестера: `delete from ptk.sessions where anon_id = '<id>';` (каскадом уйдут события, маркеры, ответы).
- Срок хранения: после плейтеста `delete from ptk.sessions where started_at < now() - interval '90 days';`

## Без бэкенда (zero-backend)

1. Тестер играет, данные лежат в `<Игра>\Saved\Playtest\<session>\` (`events.jsonl`, `session.json`, `shots/`).
2. Тестер запускает `PlaytestKit\tools\collect_playtest.ps1` (или кладёт рядом с игрой) и шлёт получившийся `playtest_<дата>.zip` в чат.
3. Ты кладёшь все zip в одну папку и запускаешь `python ptk_report.py <папка_с_zip>`.
