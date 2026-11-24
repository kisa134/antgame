# Ant Kitchen на Mac: нативная сборка (для друга с MacBook / Mac mini)

Цель: ты собираешь игру на своём Mac и играешь в кооп с друзьями на Windows. Игра ставится как обычное приложение `AntKitchen.app`, друзья на Windows тебя видят через Steam.

> Нативную Mac-сборку можно собрать **только на Mac** (Xcode + Unreal Engine 5.8). На Windows её не собрать. Если собирать нет сил, есть запасной путь без компиляции: Windows-сборка через CrossOver, см. `docs/MAC_CROSSOVER.md` (медленнее и капризнее).

Ничего программировать не нужно. Нужно один раз поставить 3 программы и запустить одну команду.

---

## 0. Коротко (если уже всё стоит)

```bash
git clone https://github.com/kisa134/antgame.git && cd antgame
git checkout code/ANT-mac        # или ту ветку/коммит, что дали друзья на Windows
git lfs install && git lfs pull
./Tools/mac/build_mac.sh         # проверка Mac + сборка + упаковка (40-90 минут в первый раз)
open Build/Mac/Mac/AntKitchen.app   # путь скрипт напечатает в конце
```

Если что-то не стоит, читай по порядку ниже.

---

## 1. Что нужно

| Что | Минимум | Рекомендуется |
|---|---|---|
| Mac | Apple Silicon (M1/M2/M3/M4). Intel не поддерживаем | M2/M3 и новее |
| macOS | 14.5 Sonoma | 15 Sequoia или новее (для Xcode 26.1.1 нужна macOS 15.6+, проверь на странице загрузки Xcode) |
| Оперативка | 16 ГБ | 32 ГБ |
| Диск | ~150 ГБ свободно: движок 40-60 ГБ, Xcode 15-20 ГБ, проект и сборка 30-40 ГБ | SSD |
| Xcode | 26.0 | **26.1.1** (рекомендует Epic для UE 5.8). Xcode 26.4 с движком НЕ совместим |
| Аккаунты | Epic Games (бесплатный), Steam | |
| Интернет | Нужен, всё качается, суммарно 100+ ГБ | |

Источник по версиям: Epic, «macOS Development Requirements for Unreal Engine» (https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine): рекомендуемый Xcode 26.1.1, минимум 26.0, 26.4 несовместим; минимум M1/M2 «в зависимости от функций рендера», рекомендуется 32 ГБ. Сам движок 5.8 тоже прописывает Xcode 26.1.1 как основную версию (`Engine/Config/Apple/Apple_SDK.json`).

---

## 2. Установка (один раз)

### 2.1 Xcode 26.1.1

1. Открой https://developer.apple.com/download/all/ , войди своим Apple ID (бесплатный), найди **Xcode 26.1.1**, скачай `.xip` (около 3 ГБ, распаковка ещё 20+ ГБ).
   Если в App Store предлагается другая версия, не ставь её, нужна именно 26.1.x (26.4 ломает сборку).
2. Распакуй (двойной клик по .xip), перетащи `Xcode.app` в `/Applications`.
3. Запусти Xcode один раз, нажми **Agree**, дай установить дополнительные компоненты.
4. В Terminal (Cmd+Пробел, напиши Terminal):
   ```bash
   sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
   sudo xcodebuild -license accept
   xcodebuild -downloadComponent MetalToolchain
   ```
   Последняя команда качает компилятор шейдеров Metal. Без него игра не соберётся с ошибкой про `metal`.
5. Проверка: `xcodebuild -version` должен показать `Xcode 26.1.1`.

### 2.2 Epic Games Launcher и Unreal Engine 5.8

1. Скачай Epic Games Launcher: https://www.unrealengine.com/download , войди (или создай аккаунт).
2. Вкладка **Unreal Engine** -> **Library** -> **+** (Install Engine) -> версия **5.8.x**.
3. Нажми **Options** и убедись, что стоит галка **Mac** в Target Platforms (остальные можно снять, это экономит место).
4. Дождись конца установки. Обычно движок оказывается в `/Users/Shared/Epic Games/UE_5.8`.

### 2.3 git и git-lfs

В Terminal:
```bash
xcode-select --install      # даст git; если пишет "already installed", всё ок
brew install git-lfs        # нужен Homebrew (https://brew.sh); без brew скачай установщик с https://git-lfs.com
git lfs install
```

### 2.4 Steam для Mac

Скачай и войди: https://store.steampowered.com/about/ . **Steam должен быть запущен и залогинен, когда ты запускаешь игру.** Друзей из Steam нужно добавить заранее (кооп идёт через них).

---

## 3. Получить код (и версия обязана совпадать с друзьями)

```bash
cd ~/Documents
git clone https://github.com/kisa134/antgame.git
cd antgame
git checkout code/ANT-mac
git lfs pull
git log -1 --oneline            # запомни/сообщи этот коммит друзьям
```

**ВАЖНО: версия игры у всех в кооп-комнате должна быть одинаковой** (один и тот же коммит). Если у друзей на Windows сборка из другого коммита, Mac не сможет подключиться: пакеты реплицируются по-разному и будет «Connection lost» / мгновенный вылет клиента. Спроси у хоста, из какого он коммита (`git log -1 --oneline` или версия в меню), и сделай `git checkout <тот коммит>` перед сборкой.

Размер: `git lfs pull` качает несколько ГБ ассетов. Если видишь ошибку про квоту LFS, напиши владельцу репозитория.

---

## 4. Сборка

### Способ А (рекомендую): одна команда

```bash
cd ~/Documents/antgame
./Tools/mac/build_mac.sh --check     # быстрая проверка Mac (1 минута), ничего не собирает
./Tools/mac/build_mac.sh             # полная сборка и упаковка
```

Скрипт сам проверяет Xcode/Metal/движок/LFS, пишет зелёным OK, жёлтым предупреждения, красным ошибки с подсказкой что делать, собирает игру, снимает карантин и подписывает приложение локально. В конце печатает путь к `AntKitchen.app` и команду запуска.

Если `Permission denied`: `chmod +x Tools/mac/build_mac.sh`.
Полезные ключи: `--clean` (стереть старую сборку, лечит странные ошибки), `--editor` (собрать только редакторские модули, чтобы открыть проект в редакторе).
Переменные: `UE_ROOT="/путь/к/UE_5.8"` если движок лежит не там; `CONFIG=Shipping` для релизной сборки.

Первый раз это 40-90 минут (компиляция C++ и шейдеров Metal). Mac будет горячим, не закрывай крышку и подключи зарядку. Повторные сборки в разы быстрее.

### Способ Б: через редактор Unreal (если хочется кнопками)

1. Выполни один раз `./Tools/mac/build_mac.sh --editor` (собирает C++ модули редактора).
2. Двойной клик по `AntKitchen.uproject`. Если спросит **«Missing modules, rebuild?»**, нажми **Yes** (это Способ Б делает сам; сообщение нормально, когда папки `Binaries` ещё нет).
3. Меню **Platforms** -> **Mac** -> **Package Project**, выбери пустую папку.
   Конфигурацию в **Platforms -> Mac -> Package** ставь **Development**.

### Способ В: ручная команда (то же, что делает скрипт)

```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
  -project="$HOME/Documents/antgame/AntKitchen.uproject" \
  -noP4 -utf8output -platform=Mac -targetplatform=Mac \
  -specifiedarchitecture=arm64 -clientconfig=Development \
  -build -cook -allmaps -stage -pak -compressed \
  -archive -archivedirectory="$HOME/Documents/antgame/Build/Mac" -nocompileeditor
```

Флаг архитектуры: `-specifiedarchitecture=arm64` (M-серия); `arm64+x86_64` даёт универсальную сборку (дольше и больше). Документация Epic: https://dev.epicgames.com/documentation/unreal-engine/supporting-universal-binaries-for-macos-in-unreal-engine

---

## 5. Где игра

После сборки: `~/Documents/antgame/Build/Mac/Mac/AntKitchen.app` (точный путь печатает скрипт; если нужно найти: `find ~/Documents/antgame/Build -name "*.app"`). Размер несколько ГБ.

Можно перетащить в `/Applications` или оставить как есть. Не удаляй папку `Build` пока играешь.

---

## 6. Запуск (Gatekeeper)

Скрипт уже снял карантин и подписал приложение. Если ты копировал `.app` на другой Mac/по сети/через AirDrop, macOS скажет «приложение повреждено» или «нельзя проверить разработчика». Лечится двумя командами:

```bash
xattr -dr com.apple.quarantine /путь/к/AntKitchen.app
codesign --force --deep --sign - /путь/к/AntKitchen.app
```

Либо: правый клик по приложению -> **Открыть** -> **Открыть**. Либо **Системные настройки -> Конфиденциальность и безопасность -> Всё равно открыть** (появляется после первой неудачной попытки).

Сертификат Apple Developer ($99) для игры у себя и друзей НЕ нужен. Rosetta тоже не нужна: сборка нативная arm64.

Запуск с логом (если что-то непонятно): `./AntKitchen.app/Contents/MacOS/AntKitchen -log` (имя файла внутри MacOS может отличаться, `ls` покажет).

---

## 7. Кооп с друзьями на Windows

1. Запусти **Steam**, залогинься, он должен висеть открытым. (Игра сейчас идёт под тестовым AppID 480 «Spacewar», в Steam у вас обоих статус «играет в Spacewar», это нормально.)
2. Запусти `AntKitchen.app`. В главном меню должна быть строка «Steam: <твой ник>». Если вместо неё красная надпись про Steam, Steam не запущен или не залогинен: закрой игру, запусти Steam, запусти игру снова.
3. Друг на Windows создаёт лобби (хостит из меню игры). Ты находишь его лобби в списке в меню или заходишь через Steam: список друзей -> ПКМ по другу -> **Присоединиться к игре**.
4. Если лобби не находится: ваши версии игры различаются (см. раздел 3), или у друга закрыт Steam, или вы не друзья в Steam.

Mac-хост тоже работает (SteamSockets и Steam для Mac поддерживаются движком), но сначала проверьте, что твой Mac как клиент нормально подключается.

### Особенности управления на Mac
- **Ctrl** используется в игре (рывок/присесть). macOS перехватывает Ctrl+стрелки (Mission Control). Отключи в Системные настройки -> Клавиатура -> Сочетания клавиш -> Mission Control.
- **F-клавиши** (F2-F7 съёмка, F4 тренировка, F10 фпс) на MacBook требуют **fn** (или включи «Использовать клавиши F1, F2 и т. д. как стандартные функциональные» в настройках клавиатуры).
- Курсор не залипает: в меню нажми Esc; если мышь «убегает» в оконном режиме, переключи в настройках игры полный экран.
- Звук: если пропал после смены наушников, перезапусти игру (CoreAudio).

---

## 8. Если что-то сломалось

Сначала запусти `./Tools/mac/build_mac.sh --check`, он диагностирует большинство проблем. Лог последней сборки: `/tmp/antkitchen_mac_build.log` (ищи `error:`).

| Симптом | Что делать |
|---|---|
| `Xcode not detected` / `Unable to find a valid installed Xcode` | `sudo xcode-select -s /Applications/Xcode.app/Contents/Developer`, запусти Xcode один раз руками, `sudo xcodebuild -license accept` |
| `xcrun: error: unable to find utility "metal"` / ошибки шейдеров | `xcodebuild -downloadComponent MetalToolchain`, затем `./Tools/mac/build_mac.sh --clean` |
| Ошибки компилятора внутри кода движка, в логе Xcode 26.4+ | Поставь Xcode 26.1.1 (раздел 2.1) и переключи `xcode-select -s` на него |
| `Failed to finalize the .app with Xcode` / `Finalize` / ошибки codesign при упаковке | Для локальной игры подпись не нужна. Пересобери с `--clean`; если повторяется, открой `Saved/Logs/` и посмотри причину; обычно это «No signing certificate»: в Xcode -> Settings -> Accounts войди Apple ID (бесплатный) или пропусти и подпиши потом вручную `codesign --force --deep --sign - AntKitchen.app` |
| Окно чёрное / ассеты розовые / «пустая кухня» | Не скачались LFS-файлы: `git lfs pull`, потом `--clean` и сборка |
| «App is damaged and can't be opened» | `xattr -dr com.apple.quarantine AntKitchen.app` + `codesign --force --deep --sign - AntKitchen.app` |
| Игра сразу закрывается | Запусти из Terminal с `-log`, смотри последние строки; логи лежат в `~/Library/Application Support/Epic/AntKitchen/Saved/Logs/` (или рядом с приложением в `Saved/Logs`) |
| `GenerateProjectFiles` падает | Не нужно для упаковки, игнорируй |
| Нет места на диске | Нужно ~40 ГБ свободно на время упаковки; очисть `Build/`, `Intermediate/`, `Saved/` |
| Сборка падает на 99% «Mac не установлен в движке» | В Launcher: UE 5.8 -> Options -> Target Platforms -> Mac, доустановить |
| Друзья не видят лобби / не коннектится | Разные версии (раздел 3); Steam закрыт; нет дружбы в Steam |
| Телевизор на кухне (видео) не играет | Должны быть файлы `Content/Movies/Life/*.mp4` (LFS). На Mac видео идёт через AVFoundation, H.264/AAC mp4 поддерживает нормально |

Железо слабое (M1 8 ГБ, лагает): в `Config/Mac/MacEngine.ini` уже отключён Lumen GI. Дополнительно можно снизить разрешение в настройках игры, закрыть браузер.

---

## 9. Что мы поменяли в проекте ради Mac (для разработчика)

- `Config/Mac/MacEngine.ini`: только для Mac отключены Lumen GI и VSM (иначе M1/M2 идёт в слайд-шоу).
- `Config/DefaultEngine.ini`: в конце секция `[/Script/MacTargetPlatform.MacTargetSettings]` убирает `SF_METAL_SM6`: собирается только Metal SM5 (быстрее cook, меньше рисков). Для SM6 (M2+, macOS 14+, Nanite/VSM) удалить эти 2 строки.
- `Tools/mac/build_mac.sh`: сборка одной командой; `.gitattributes`: `*.sh` всегда с LF.
- Код C++ менять не потребовалось: Windows-специфичного кода в `Source/AntKitchen` нет (проверено, см. отчёт в коммите). Известные риски: компилятор Clang строже MSVC (возможны мелкие ошибки при первой Mac-сборке, их надо присылать), плагин NiagaraFluids на Mac не проверялся.

---

## 10. Лаунчер игры на Mac (по желанию)

В `Tools/launcher/` (в сборке `AntKitchen_v14`) лежит лаунчер на Python (pywebview), у которого есть страница «Ant Kitchen на Mac» с этой инструкцией. Запустить из исходников на Mac:

```bash
python3 -m pip install pywebview requests
cd Tools/launcher/src      # путь может отличаться; файл launcher.py
python3 launcher.py
```

Если `pip` ругается на externally-managed-environment: `python3 -m venv ~/ak-venv && source ~/ak-venv/bin/activate` и повторить установку. Лаунчер уже знает про macOS (пути `~/Library/Application Support/AntKitchen`, проверка Steam через `ps` и `/Applications/Steam.app`, открытие папок через `open`). Он не собирает игру, а только показывает инструкцию и (когда будет сборка) скачивает готовую.

---

## Источники
- Epic: macOS Development Requirements for Unreal Engine: https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine
- Epic: Supporting Universal Binaries for macOS: https://dev.epicgames.com/documentation/unreal-engine/supporting-universal-binaries-for-macos-in-unreal-engine
- Версии Xcode/macOS, которые ждёт движок: `Engine/Config/Apple/Apple_SDK.json`, `Engine/Config/Mac/Mac_SDK.json` внутри установки UE 5.8 (Xcode 26.1.1, минимум macOS 14).
