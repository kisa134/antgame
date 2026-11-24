# PlaytestKit: контракт данных

`event.schema.json` (JSON Schema 2020-12) описывает всё, что плагин пишет на диск и шлёт на сервер. Это единственный контракт: плагин, бэкенд, анализ и SDK для других движков опираются только на него.

## Файлы на диске (`Saved/Playtest/<session_id>/`)

| Файл | Что |
|---|---|
| `events.jsonl` | по строке на событие: `{"seq":12,"t":48210,"ts":"2026-11-24T12:00:48.210Z","name":"death","cat":"combat","props":{...},"ctx":{...}}` |
| `session.json` | мета сессии (`$defs/session_meta`): проект, сборка, `anon_id`, роль, время, флаги командной строки (только имена), счётчики |
| `shots/*.jpg` | скриншоты маркеров F7 |
| `../anon_id.txt`, `../consent.json`, `../upload.json` | идентификатор, согласие, смещение отправки |

Поля строки: `seq` (монотонный в сессии), `t` (мс от старта сессии, ось для склейки всего), `ts` (UTC, у breadcrumb опущен для экономии), `name` (lower_snake), `cat` (по умолчанию часть имени до первого `_`), `props` (плоские значения), `ctx` (карта, роль, режим, рейд, тир, `pos{x,y,z}`, fps; у `breadcrumb` сжатый: только изменившееся).

## Батч по сети (POST на `Endpoint`)

Тело (gzip, `Content-Encoding: gzip`, `content-type: application/json`): поля сессии вынесены на верхний уровень, события без дублей:

```json
{"project":"ant-kitchen","session_id":"20261124-120000-0a1b","build":{"version":"..","git_sha":".."},"player":{"anon_id":".."},"events":[{"seq":1,"t":0,"ts":"..","name":"session_start","cat":"session","props":{},"ctx":{}}]}
```

Заголовки: `x-ptk-key` (ключ записи), `x-ptk-project`. Повторная отправка безопасна: ключ идемпотентности `(session_id, seq)`. Скриншоты: `POST <Endpoint>/file?session=<id>&name=<file>` (image/jpeg). Ответ: `{"ok":true,"accepted":N}`; 401 неверный ключ, 400 битый батч (клиент не повторяет), 5xx/сеть (клиент повторяет с паузой).

## Стандартные события

Полный список с полями: `x-standard-events` в схеме. Группы: `session`, `level`, `progress`, `combat`, `economy`, `ui`, `perf`, `hitch`, `net`, `breadcrumb`, `error`, `marker`, `survey`, `custom`. Любые другие имена допустимы (`additionalProperties`), но чтобы они попали в отчёт, их нужно перечислить в конфиге проекта (`analysis/projects/*.json`).

## Версионирование

`schema_v` в `session.json`. Добавление полей и событий не ломает ничего; переименование/удаление = новая мажорная версия схемы и новая миграция бэкенда.
