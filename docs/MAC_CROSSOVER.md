# Ant Kitchen на Mac через CrossOver (запасной путь: Windows-сборка)

Когда использовать: нет времени/места собирать нативную версию (`docs/MAC_BUILD.md`). CrossOver запускает обычную Windows-сборку `AntKitchen.exe` на Mac. Минусы: медленнее нативной, больше памяти, на Unreal Engine 5 бывают вылеты. Нативная сборка предпочтительнее. Платно (есть пробный период у CodeWeavers).

Для кооп-игры оба пути одинаковы: нужен Steam (в CrossOver это **Windows-Steam**, а не Mac-Steam), и версия игры должна совпадать с версией друзей.

---

## 1. Установка

1. Установи CrossOver 26 (https://www.codeweavers.com/crossover). CrossOver 26 основан на Wine 11.0 и содержит D3DMetal 3.0 и DXMT 0.72 (источник: блог CodeWeavers, https://www.codeweavers.com/blog/mjohnson/2026/02/10/crossover-26-cures-artificial-incompatibility-with-windows-games-on-mac ).
2. Создай **новую бутылку** (Bottle) только под Ant Kitchen: **Bottle -> New Bottle**, тип **Windows 10 64-bit**, имя `AntKitchen`. Отдельная бутылка нужна, чтобы настройки графики не влияли на другие игры.
3. Установи в эту же бутылку **Steam для Windows**: в CrossOver **Install** -> найди «Steam» -> выбери бутылку `AntKitchen`. Войди в аккаунт. Когда Steam просит «Microsoft Visual C++ Redistributable», разреши установку, а если не предлагает, поставь `vcredist 2015-2022 x64` через **Install** (или Bottle -> Install Software -> Visual C++ Runtime).
4. Добавь друзей в этом Steam. Друзья видят тебя «в сети» через Windows-Steam.

**Нужны:** Apple Silicon Mac, лучше 16+ ГБ ОЗУ, около 10 ГБ под игру и бутылку.

---

## 2. Настройки бутылки (Bottle -> Advanced Settings)

Названия пунктов по документации CodeWeavers для CrossOver Mac 26 (https://codeweavers.helpjuice.com/en_US/advanced-settings-in-crossover-mac-26 ):

| Настройка | Значение | Зачем |
|---|---|---|
| **Graphics** | **D3DMetal** (а не Auto) | Транслятор DirectX 11 и 12 от Apple Game Porting Toolkit; применяется ко всем приложениям в бутылке. Пользователи CodeWeavers советуют D3DMetal как самый быстрый вариант, DXMT близко, но с редкими подвисаниями; DXVK и Wine-режим медленнее ( https://www.codeweavers.com/compatibility/crossover/tips/atlyss/improved-fps-using-different-syncing-and-graphics-options ). Для другой UE-игры на Auto шейдеры не компилировались, а при принудительном D3DMetal всё заработало ( https://www.codeweavers.com/compatibility/crossover/tips/atomic-heart/getting-around-shader-compilation-failure-due-to-video-memory-error ). |
| **High Resolution Mode** | **Вкл** | Отключает удвоение пикселей и сообщает приложению 192 DPI: картинка чёткая на Retina и игра не рендерит в «двойном» масштабе. Если интерфейс или мышь ведут себя странно (клики мимо), выключи и сравни: у некоторых приложений этот режим рендерится неверно. |
| **MSync / ESync** | Оставь включённым то, что стоит по умолчанию (в CrossOver 26 это MSync). Если видишь рывки, переключи и сравни | Это способ синхронизации потоков. По отзывам, разница небольшая, ESync иногда стабильнее по кадрам ( ссылка на tips выше ), поэтому переключатель имеет смысл как эксперимент. В официальной статье про 26-ю версию упомянут только MSync. |
| **DLSS** | Выкл | Через MetalFX работает только с D3DMetal/DXMT и в нашей игре не используется; у части игр делает хуже. |

Применить: **Apply**, затем полностью закрой Steam/игру (Wine-серверу нужен рестарт: меню бутылки -> **Wine Tools -> Quit All Programs**).

---

## 3. Куда положить игру

Самый простой вариант: скачай Windows-сборку (`AntKitchen_*.zip` из релиза) на Mac, распакуй в папку внутри диска бутылки:

`~/Library/Application Support/CrossOver/Bottles/AntKitchen/drive_c/AntKitchen/`

(в Finder: Go -> Go to Folder и вставь путь; можно и любой другой путь, но сохраняй короткий путь без кириллицы).

Запускать нужно главный `AntKitchen.exe` из папки `Windows` распакованной сборки (не `AntKitchen-Win64-Shipping.exe` из Binaries).

---

## 4. Запуск

1. Сначала запусти **Steam в той же бутылке** (CrossOver -> бутылка AntKitchen -> Steam -> Run), дождись входа.
2. CrossOver -> бутылка `AntKitchen` -> **Run Command** -> выбери `AntKitchen.exe`. Либо **Install** -> «Install an unlisted application» -> выбери exe (появится иконка).
3. Параметры запуска (в поле **Command**/Arguments в Run Command или в свойствах ярлыка):
   - **Первый запуск**: без параметров (D3D12 через D3DMetal).
   - **Если вылетает/чёрный экран/зависает на шейдерах**: добавь `-dx11`
     (UE работает через D3D11; для UE-игр на CrossOver это основной запасной вариант).
   - **Если окно обрезано или в неверном размере**: `-windowed -ResX=1600 -ResY=900`.
   - **Если не хватает памяти**: `-windowed -ResX=1280 -ResY=720` и в настройках игры низкие тени/пыль.
   - Для диагностики: `-log`.

Пример строки: `AntKitchen.exe -dx11 -windowed -ResX=1600 -ResY=900 -log`

---

## 5. Известные проблемы Unreal Engine 5 в CrossOver

- **Подвисания из-за компиляции шейдеров.** Первые минуты любой UE-игры шейдеры компилируются «на лету» (Mac это делает медленнее). Приём из базы CodeWeavers для UE-игры Ready or Not: в `GameUserSettings.ini` бутылки добавить `[/Script/Engine.RendererSettings]` и `r.CreateShadersOnLoad=1`, чтобы шейдеры собирались при загрузке уровня, а не во время игры ( https://codeweavers.com/compatibility/crossover/tips/ready-or-not/fix-stuttering ). Для Ant Kitchen файл лежит в `drive_c/users/crossover/AppData/Local/AntKitchen/Saved/Config/Windows/GameUserSettings.ini` (создаётся после первого запуска; если пути нет, найди через Finder по имени `GameUserSettings.ini` внутри бутылки). После первого прогона кэш шейдеров сохранится и дальше будет плавнее.
- **Вылет DX12 на UE5** (`GGlobalSamplerDescriptorHeapSize <= MaximumSamplerHeapSize`) описан у другой UE5-игры на CrossOver; обходной путь, на котором игроки видели запуск, это `-dx11`, но там же писали о низкой производительности (около 9-12 FPS) и большом расходе памяти 10-12 ГБ ( https://www.codeweavers.com/compatibility/crossover/forum/starrupture?msg=342791 ). Это крупная игра, у Ant Kitchen нагрузка существенно ниже, но ожидай заметного падения FPS относительно нативной сборки.
- **Память.** Wine + D3DMetal + UE дают большой расход. Закрой Chrome/Telegram, не держи вторую игру в другой бутылке. Если Mac уходит в своп, играть нельзя.
- **Steam просит VC++ Redistributable** даже когда он стоит: поставь `vcredist 2015-2022 x64` в бутылку ещё раз (тот же отчёт по StarRupture).
- **Оверлей Steam и «Присоединиться к игре»** могут не срабатывать из CrossOver. Присоединяйся через список лобби в меню игры или проси друга прислать приглашение из Steam-чата.
- **Звук/курсор:** при сворачивании окна курсор может не вернуться: Cmd+Tab назад, клик по окну.
- Это неофициальный путь. Известный надёжный способ: нативная сборка (`docs/MAC_BUILD.md`).

---

## 6. Чек-лист, если не запускается

1. Steam в бутылке запущен и залогинен.
2. Graphics = **D3DMetal**, Apply, Quit All Programs, повторить.
3. Добавь `-dx11`.
4. Удали кэш: `drive_c/users/crossover/AppData/Local/AntKitchen/Saved/` (сохранения колонии лежат тут же, `Nest/` не удаляй без нужды).
5. Версия игры равна версии у друзей.
6. Пришли лог: `.../AntKitchen/Saved/Logs/AntKitchen.log`.

---

## Источники
- CodeWeavers, Advanced Settings in CrossOver Mac 26: https://codeweavers.helpjuice.com/en_US/advanced-settings-in-crossover-mac-26
- CodeWeavers, CrossOver 26 release post: https://www.codeweavers.com/blog/mjohnson/2026/02/10/crossover-26-cures-artificial-incompatibility-with-windows-games-on-mac
- CodeWeavers tips, ATLYSS (синхронизация и графика): https://www.codeweavers.com/compatibility/crossover/tips/atlyss/improved-fps-using-different-syncing-and-graphics-options
- CodeWeavers tips, Atomic Heart (UE, ошибка шейдеров, D3DMetal): https://www.codeweavers.com/compatibility/crossover/tips/atomic-heart/getting-around-shader-compilation-failure-due-to-video-memory-error
- CodeWeavers tips, Ready or Not (UE, подвисания, `r.CreateShadersOnLoad`): https://codeweavers.com/compatibility/crossover/tips/ready-or-not/fix-stuttering
- CodeWeavers forum, StarRupture (UE5, `-dx11`, память): https://www.codeweavers.com/compatibility/crossover/forum/starrupture?msg=342791
