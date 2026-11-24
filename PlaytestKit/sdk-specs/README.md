# PlaytestKit для не-Unreal проектов (Unity, Godot, Web)

Плагин UE5 писать не нужно: достаточно реализовать контракт из `../schema/README.md`. Минимум для совместимости с бэкендом и отчётом:

1. **Сессия**: `session_id = yyyyMMdd-HHmmss-xxxx`, случайный `anon_id` (один раз на установку, в файле, не из железа), согласие до любой записи.
2. **События** в формате `{seq,t,ts,name,cat,props,ctx}`; `seq` с 1 без дыр, `t` в мс от старта.
3. **Обязательные**: `session_start`, `session_end`, `level_start`, `perf_sample` (раз в 5 с: `fps_avg, fps_p1, frame_ms, mem_mb`), `marker`, `survey_answer`, `death` (если есть смерти), `crash`.
4. **Хранение**: `events.jsonl` + `session.json` в `<persistent>/Playtest/<session_id>/`, дописывать построчно, flush раз в 1-2 с и при выходе.
5. **Отправка** (необязательно): `POST` батчем gzip на `Endpoint` с заголовками `x-ptk-key`, `x-ptk-project`; повторы с паузой 5/15/60/300 с; оффлайн = очередь на диске.
6. **Маркер**: горячая клавиша (F7), 4 тега, скриншот в `shots/`, последние 30 с перфа в `props.perf30`.

Отчёт `analysis/ptk_report.py` читает такую папку как есть, движок значения не имеет.

## Unity (набросок)

`Application.persistentDataPath + "/Playtest/" + sessionId`; `Time.unscaledDeltaTime` ring-buffer для fps; `Application.logMessageReceivedThreaded` для счётчиков ошибок; `ScreenCapture.CaptureScreenshotAsTexture` для маркера; `UnityWebRequest` + `System.IO.Compression.GZipStream` для батчей.

## Web / JS (набросок)

`localStorage` для `anon_id`, события в массив, `fetch(endpoint, {method:'POST', headers:{'x-ptk-key':KEY,'x-ptk-project':P,'content-type':'application/json'}, body})`; `navigator.sendBeacon` при `pagehide` (без gzip, батч до 64 КБ); `performance.now()` для `t`.

## Godot (набросок)

`user://Playtest/<sid>/events.jsonl` через `FileAccess.open(..., WRITE_READ)` и `store_line`; fps из `Engine.get_frames_per_second()`; `HTTPRequest` для отправки.
