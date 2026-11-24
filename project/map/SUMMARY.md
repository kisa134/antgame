# Карта проекта Ant Kitchen: сводка (2026-11-24, e2ff0e3)

Интерактивная карта: https://claude.ai/artifact/8rs9aDgofMHhTbwH2vu8Y5 (артефакт claude.ai, slug `3fa651ed-b3ea-48d3-8daf-7abc4d247e16`). Правки со страницы лежат в базе артефакта, коллекция `edits`.

Файл создаётся автоматически: `python Tools/projectmap/build_map.py`. Не правь руками. Ручные данные: `project/map/*.yaml`.

Узлов: 1246, связей: 635. По типам: asset-part 430, asset 280, file 246, system 172, build 41, doc 38, area 13, decision 11, bug 9, character 6

## Области и системы

### Геймплей
Муравей-игрок, перенос, рой, рейд, оружие, режим Человек против муравьёв, Учебка.
- **Муравей-игрок: движение и лазание** [in-game] (Claude): Ползание по стенам и потолку, прыжок, захват, клиент-авторитетное движение. Код: AntCharacter.cpp, AntCharacter.h, AntMovementComponent.cpp, AntMovementComponent.h
- **Тело муравья из сегментов** [in-game] (Claude): Муравей как шарнирное насекомое: голова, жвалы, грудь, стебелёк, брюшко, 6 ног по 4 сегмента, усики. Нет скелета, UAntBodyComponent позируе… Код: AntBody.cpp, AntBody.h, AntBodyData.inl
- **Перенос еды вместе** [in-game] (Claude): Муравьи физически поднимают предмет лапами, вес и положение распределяются по ногам, до 8 игроков. Код: AntCarry.cpp, AntCarry.h, AntHaulAdapter.cpp, AntHaulAdapter.h
- **Снежный ком, рой и отряд ботов** [in-game] (Claude): Каждый раунд с одного муравья, еда даёт массу и апгрейды, выделение отряда, тропы для рабочих. Код: AntArmy.cpp, AntArmy.h, AntRts.cpp, AntRts.h
- **Рейд, смены и маршруты** [in-game] (Claude): Цикл забега: квота, фазы Человека, общие жизни, итоги, сид уровня. Код: AntGameState.cpp, AntGameState.h, AntKitchenGameMode.cpp, AntKitchenGameMode.h
- **Оружие муравья и кислота** [in-game] (Claude): Кислотный плевок, укус, оружие бойца. Код: AcidProjectile.cpp, AcidProjectile.h, AntWeapon.cpp, AntWeapon.h
- **Человек против муравьёв** [in-game] (Claude): Игрок за Человека громит кухню и защищает еду против игроков-муравьёв. Код: AntVersus.cpp, AntVersus.h, WarMode.cpp, WarMode.h
- **Боты-муравьи** [in-game] (Claude): ИИ ботов, команды AddBot. Код: AntBotController.cpp, AntBotController.h
- **Учебка Сержанта Жвалова и демо** [in-game] (Claude): 16 этапов обучения, песочница, демо-рейд. Живым человеком не проверено. Код: AntTutorial.cpp, AntTutorial.h
- **Внутрь людей, реакции, ткань** [in-game] (Claude): Муравьи лезут в ухо, штанину, карман; настоящая ткань скатерти. Код: AntCloth.cpp, AntCloth.h, AntInside.cpp, AntInside.h

### Мир и уровень
Квартира v4: кухня, гостиная, коридор, детская, двор; живая квартира; интерактивные предметы.
- **Квартира: уровень v4** [in-game] (Claude): Районы, сид рейда, оболочка стен и полов, свет и материалы, без примитивов. Код: AntNest.cpp, AntNest.h, KitchenArt.cpp, KitchenArt.h
- **Живая квартира: ТВ, мир за окном** [in-game] (Claude): Телевизор с каналами, улица за окном, робот-пылесос, посудомойка, мелочи масштаба муравья. Код: AntLife.h, AntLifeChannels.inl, AntLifeTV.cpp, AntLifeWorld.cpp
- **Интерактивные предметы и гаджеты** [in-game] (Claude): Двери, ящики, кран, тостер, щиток, события кухни. Код: KitchenEvents.cpp, KitchenEvents.h, KitchenGadgets.cpp, KitchenGadgets.h

### Персонажи
Человек (папа), бабушка, малыш: тело из сегментов, ИИ, маршруты.
- **Люди: тело, ИИ, маршруты** [in-game] (Claude): Папа, бабушка, малыш как тяжёлые тела из сегментов с инерцией, пинком, полу-рэгдоллом; распорядок дня. Код: FolkCharacters.cpp, FolkCharacters.h, HumanAIController.cpp, HumanAIController.h

### Существа
Кошка Муся, Бадрик, муха, паук и другие жильцы.
- **Животные и насекомые** [in-game] (Claude): Кошка Муся, Бадрик, муха, паук: четвероногое тело и мозг. Код: CreatureCharacters.cpp, CreatureCharacters.h, CritterBrain.cpp, Critters.cpp

### Предметы и еда
Лут и еда по ярусам T0-T3, мебель и посуда, библиотека ассетов Meshy.
- **Лут и еда** [in-game] (Claude): Еда ярусов T0-T3, разрезание на куски, добыча с существ, торт на день рождения. Код: KitchenLoot.cpp, KitchenLoot.h

### Физика и системность
Вода, грязь, разрушения, материалы и цепные реакции.
- **Настоящая вода и жидкости** [in-game] (Claude): Симуляция воды, течение несёт крошки, лужи, скользкая лужа. Код: AntFluidSim.cpp, AntFluidSim.h, AntFluidWorld.cpp, AntFluidWorld.h
- **Грязь, следы, пыль** [in-game] (Claude): Размазывание еды, следы, пылинки в свете. Код: AntMess.cpp, AntMess.h, AntMessDust.cpp
- **Разрушение, осколки, рэгдолл** [in-game] (Claude): Мебель падает и бьётся, осколки режут, муравьи разлетаются на части. Код: AntGibs.cpp, AntGibs.h, AntRagdoll.cpp, AntRagdoll.h
- **Материалы и цепные реакции 2.0** [in-game] (Claude): 39 материалов, тепло, порошки, электричество, цепочки C7-C22. Код: AntChain.cpp, AntChain.h, AntMaterials.cpp

### Мета-прогрессия
Муравейник, боец и отряд, ветераны, облик.
- **Муравейник: экран колонии** [in-game] (Claude): 2D-разрез муравейника, постройки по ярусам, королева, меняется после каждого рейда. Код: AntColony.cpp, AntColony.h, AntColonyView.cpp, AntColonyView.h
- **Боец и отряд: звания, перки, ветераны** [in-game] (Claude): Сайдгрейд-перки, медали, облик, именные ветераны с постоянной смертью. Код: AntClan.cpp, AntClan.h, AntCosmetics.cpp, AntCosmetics.h

### Интерфейс
Кухонный штаб: меню, лобби, HUD, донесение, настройки.
- **Интерфейс «кухонный штаб»** [in-game] (Claude): Меню, лобби, настройки, загрузка, HUD, карты, донесение. Код: AntFighterUi.cpp, AntFighterUi.h, AntHUD.cpp, AntHUD.h

### Звук
Адаптивный саундтрек, звуки, озвучка RU/EN.
- **Звук и музыка** [in-game] (Claude): Адаптивный саундтрек, 130 новых звуков, микс. Код: AntAudio.cpp, AntAudio.h

### Техника
Ядро, сеть (listen server), админ-режим, сборки Dist, релизы playtest.
- **Ядро, типы, эффекты** [in-game] (Claude): Модуль игры, общие типы, масштаб (AntShrink), эффекты. Код: AntFx.cpp, AntFx.h, AntKitchen.Build.cs, AntKitchen.cpp
- **Админ-режим** [in-game] (Claude): Панель и команды для тестов и съёмки. Код: AntAdmin.cpp, AntAdmin.h
- **Сеть: listen server** [in-game] (Claude): Listen server, UDP 7777, клиент-авторитетное движение, реплицируется состояние.

### Инструменты и пайплайн
Скрипты Tools/, Blender, импорт Meshy, карта проекта.
- **Скрипты Tools/** [in-game] (Claude): Импорт Meshy, Blender-скрипты, сборка, тесты.
- **Пайплайн ассетов Meshy в Unreal** [in-progress] (Юра): Юра генерирует в Meshy по карточкам библии, кладёт в SourceArt/Meshy/<Комната>/<ID>/, Claude импортирует (Tools/import_meshy.py), ставит ма…
- **Карта проекта (эта система)** [in-game] (Claude): project/map/*.yaml + Tools/projectmap/build_map.py -> map.json -> интерактивная страница.

### Маркетинг и медиа
Съёмка, трейлеры, Higgsfield, ролики, страница Steam.
- **Режим съёмки** [in-game] (Claude): Камеры и сценарии для трейлеров и скриншотов. Код: AntFilm.cpp, AntFilm.h
- **Маркетинговый комплект и трейлеры** [in-progress] (Денис): Скриншоты, гифки, ролики, стиль: мультфильм 3D и жирный комикс. Ролик под «Полёт валькирий».

### Команда и процесс
Люди, правила работы, Jira, решения по дизайну.
- **Процесс работы и правила Claude** [in-game] (Юра): Jira ANT для агентов, ветки code/ANT-*, LFS-замки, PR в main, FPF.

## Ассеты Meshy (библия, 219 карточек)

prompt-ready: 206, in-game: 13

Карточки: `project/map/assets_bible.json`; узлы `asset.AK_*`; части `part.AK_*`.

## Где что лежит

- Код: `Source/AntKitchen/*`; документы: `docs/*.md`, `docs/FEATURES.md` (полный список), `docs/STATUS.md` (где мы сейчас); ассеты: `Content/*`, `SourceArt/*`; скрипты: `Tools/*`.
- Карта: `project/map/` (areas/systems/manual/overrides.yaml, assets_bible.json, map.json), генератор `Tools/projectmap/`.

## Как обновлять

1. Поменял системы или ассеты: `python Tools/projectmap/build_map.py` (нужен pyyaml).
2. Ручная правка статуса, владельца, заметки: `project/map/overrides.yaml` (или `python Tools/projectmap/apply_edits.py edits.json` из страницы).
3. Перепубликовать страницу: `publish_files` (slug выше, mode patch): `data/map.json` = `project/map/map.json`; вид страницы лежит в `Tools/projectmap/viewer/index.html` (as `index.html`).
4. Правки Юры со страницы: прочитать коллекцию `edits` артефакта (db_list), сохранить как JSON, `python Tools/projectmap/apply_edits.py edits.json`, затем build_map.py и публикация.

## Типы узлов и статусы

Типы: area, system (системы и фичи, теги person/feature/district/chain), character, asset, asset-part, file, doc, build (релизы и срезы STATUS), bug, decision.
Статусы систем: idea, designed, in-progress, in-game, polished, blocked, deprecated. Статусы ассетов: idea, prompt-ready, generated, imported, in-game, approved.
