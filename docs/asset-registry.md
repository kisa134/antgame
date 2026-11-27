# Реестр ассетов Ant Kitchen (KAN-26)

Составлен 27.11.2026 по ветке `code/ANT-tails` (от `code/ANT-v14` 6342dd9b). Охват: всё, что лежит в `Content/` под git (4414 файлов, 1345 ассетов), плюс то, что игра берёт вне `Content/` (видео телевизора, ассеты движка).

Источник определён по документам проекта (`docs/AUDIO_CREDITS.md`, `docs/UI_CREDITS.md`, `docs/level/V4_CREDITS.md`, `docs/level/V4_SPEND.md`, `docs/level/LIFE.md`, `docs/*_SPEND.md`), по скриптам в `Tools/` и по истории git (каким коммитом и с каким описанием файл появился и менялся). Файлы внутри `.uasset` не вскрывались: реестр опирается на записи проекта, а не на экспертизу содержимого. Где запись одна и перепроверить её нечем, это сказано в строке.

## 1. Итог по источникам

| Источник | Ассетов | Файлов | Свой по правилу владельца? |
|---|---|---|---|
| Blender (свои скрипты `Tools/blender`, `Tools/food`, `Tools/map4`, `Tools/life`, `Tools/chains`) | 878 | 3578 | да |
| Higgsfield (картинки GPT Image, звуки Mirelo) | 214 | 287 | да |
| Свой код (синтез звука, рисование PIL, материалы UE из скриптов) | 145 | 150 | да |
| Meshy (аккаунт владельца, октябрь) | 34 | 203 | да |
| Meshy через Higgsfield (image → 3D, 22.11) | 22 | 122 | да |
| Blender + картинка Higgsfield | 6 | 24 | да |
| ElevenLabs напрямую | 34 | 38 | **нет в списке «Blender, Meshy, Higgsfield»** |
| Чужое из интернета (шрифты) | 9 | 9 | **нет** |
| Движок Epic (копии шаблонов плагина) | 3 | 3 | **не своё, но часть лицензии UE** |
| **Всего** | **1345** | **4414** | |

## 2. Что не своё — отдельным списком

Здесь всё, что не сделано в Blender, Meshy или Higgsfield. Решение по каждому пункту за владельцем; ничего не удалено и не заменено.

| № | Что | Где лежит | Откуда | Где в игре | Чем заменить |
|---|---|---|---|---|---|
| 1 | **Шрифты, 5 файлов TTF**: Russo One, PT Sans Narrow (Regular, Bold), PT Mono, Neucha, и 4 текста лицензий | `Content/UI/Fonts/` | скачаны с github.com/google/fonts, лицензия SIL OFL 1.1 (коммерческое использование разрешено) | весь интерфейс: меню, HUD, карточки, титры | своего шрифта в проекте нет; варианты — заказать или нарисовать свой, либо оставить (OFL это допускает) |
| 2 | **Голоса Windows (Microsoft Pavel, Irina)** в четырёх каналах телевизора: «Царство муравьёв», футбол, реклама «Белая Погода», «Вкусно за 5 минут» | внутри видео `Content/Movies/Life/*.mp4` (вне git, копируется в сборку) | системный синтезатор речи Windows; право использовать эти голоса в продаваемой игре в проекте не проверялось | телевизор в гостиной, каналы 1, 3, 4, 5 | переозвучить через Higgsfield (это уже записано в `docs/level/LIFE.md` как долг) |
| 3 | **38 звуков ElevenLabs** (`S_*`, 34 названия) | `Content/SoundsReal/` | сгенерированы 19.10 на платном аккаунте ElevenLabs напрямую, не через Higgsfield | **звучат в игре**: в таблице звуков `AntAudio.cpp` названо 31 из них (попадание, крит, убийство, плевок, топот Человека, кран, холодильник, тостер, муха, паук, марш армии, подбор снаряжения и др.) | перегенерировать те же 31 звук в Higgsfield (Mirelo) под теми же именами; код менять не придётся |
| 4 | **3 системы Niagara** `NS_FlipHose`, `NS_FlipPool`, `NS_FlipSplash` | `Content/Fluid/` | копии шаблонов плагина NiagaraFluids из движка (Epic), сделаны скриптом `Tools/make_fluid_assets.py` | прототип воды FLIP | это часть движка, распространяется с игрой по лицензии UE; своим считать нельзя |
| 5 | **Ассеты движка**, на которые ссылается код: `/Engine/BasicShapes/` (куб, сфера, цилиндр, конус, плоскость), `/Engine/EngineMaterials/` (EmissiveMeshMaterial, DefaultMaterial, WorldGridMaterial, DefaultDeferredDecalMaterial), `/Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent`, карта `/Engine/Maps/Entry` | в движке, не в репозитории | Epic, лицензия UE | запасные формы и служебные материалы; 14 разных путей в `Source/` | в карте v4 базовые формы уже заменены своими `SM_M4_Box/Ball/Cyl/Cone/Plane`; остальные ссылки — запасной путь |

Проверено и чужим не является:

- **Скачанные текстуры Poly Haven и ambientCG** (41 набор, стояли с 22.11 по 26.11) заменены своими в задаче KAN-24. Все 132 файла `Content/Map4/Textures` последний раз менялись коммитами KAN-24 от 26.11 (0cd254e1, 81f1c9d8, 107cfacc, 583f7f3e), скрипты скачивания удалены. Старые файлы остаются в истории git.
- **Голоса ElevenLabs и видео Kling в телевизоре** (каналы 0–3, 6) сделаны через Higgsfield, то есть в разрешённом инструменте.
- **Анимаций Mixamo и чужих скелетов нет**: Человек, жильцы и животные собраны из сегментов Blender и двигаются кодом.

Чего реестр не покрывает: содержимое папок `SourceArt/` (исходники, в игру не попадают), концепт-арт `SourceArt/Map4/higgsfield/concept/` (в игру не идёт; на одном кадре модель нарисовала настоящие бренды — в `V4_CREDITS.md` помечено «не использовать»), плагины Steam (код, не ассеты).

## 3. Группы ассетов

| Папка | Что это | Источник | Чем подтверждается | В игре сейчас |
|---|---|---|---|---|
| `Content/Hero` (200 моделей) | новые модели кухни, гостиной и мелких предметов «уровня швабры», с текстурами, LOD, кусками разрушения | Blender | `Tools/blender/hero_*.py`, исходники `SourceArt/Blender/hero*`, коммиты KAN-22/23 | да, по умолчанию (`-AntHeroArt=0` выключает) |
| `Content/Kitchen`, `Content/Living` (253) | прежние модели квартиры | Blender | `Tools/blender/kitchen_*.py`, `living_*.py`, `level_v2_*.py` | только там, где нет модели в `Hero` |
| `Content/LevelV3` (69) | детали уровня v3 | Blender | `Tools/blender/level_v3*.py`, `level_v31/v32_pieces.py` | да, кроме заменённых в `Hero` |
| `Content/FoodHero` (33) | еда, новые модели | Blender | `Tools/food/food_hero*.py` | да, по умолчанию с 27.11 |
| `Content/FoodV2` (35) | еда, прежний набор | Blender | `Tools/food/food_v2*.py` | только с `-AntFoodHero=0` |
| `Content/Ant`, `Content/Human`, `Content/Npc` (112) | тело муравья, Человека, бабушки, малыша, кошки, собаки по сегментам | Blender | `Tools/blender/ant_build.py`, `human_build.py`, `npc_*.py` | да |
| `Content/Map4/Props` (117) | вещи карты v4 | 92 Blender, 22 Meshy через Higgsfield, 3 — наш меш с картинкой Higgsfield | `docs/level/V4_CREDITS.md`, `V4_SPEND.md`, `SourceArt/Map4/higgsfield/glb/` (22 файла) | см. раздел 4 |
| `Content/Map4/Textures` (48 наборов) | поверхности | Blender; у трёх (обои кухни, обои гостиной, ковёр) цвет — картинка Higgsfield | `Tools/blender/make_own_textures.py`, KAN-24 | да |
| `Content/Map4/Decals` (12) | рисунки малыша, фото, картина, календарь, этикетки | Higgsfield | `SourceArt/Map4/higgsfield/decal/` | да |
| `Content/Meshy` (34 модели) | первые модели проекта | Meshy | `ASSETS.md`, `Tools/import_meshy.py`, коммиты 18–19.10 | см. раздел 5 |
| `Content/Textures` (13 текстур) | первые поверхности (плитка, дуб, гранит и т. д.) | Higgsfield | только описание коммита de8fcc53 и строка в `docs/FEATURES.md`; исходных картинок в `SourceArt` нет | код ссылается (старый материал `M_Surface`); видны ли при карте v4, не проверено |
| `Content/UI` кроме шрифтов (177) | фоны, загрузка, портреты, карточки, иконки, логотип, муравейник | Higgsfield; в муравейнике 3 картинки Higgsfield и 40 рендеров Blender; 10 служебных текстур нарисованы кодом | `docs/UI_CREDITS.md`, `docs/COLONY_SPEND.md` | да |
| `Content/Audio/Music` (14) | музыка | свой синтезатор | `Tools/audio/synth.py`, `make_music.py` | да |
| `Content/Audio/Sfx` (74 названия, 152 файла) | звуки | 62 Higgsfield (Mirelo), 12 свой синтез | `docs/AUDIO_CREDITS.md`, `docs/CHAINS_SPEND.md`, `Tools/audio/raw_manifest.txt` | да |
| `Content/Sounds` (23) | самые первые звуки-заглушки | свой синтез | `SOUNDS.md` | нет: код на `/Game/Sounds` не ссылается, папка лежит мёртвым грузом |
| `Content/Life` (56) | «живая квартира»: надписи, записки, чеки, птицы, звуки, материалы | свой код | `Tools/life/make_life_assets.py` | да |
| Видео телевизора, 9 каналов | `Content/Movies/Life/*.mp4`, вне git | 4 клипа Higgsfield (Kling), остальное своя 2D-анимация и рендер Blender; голоса — Higgsfield и Windows (см. раздел 2, пункт 2) | `docs/level/LIFE.md`, `docs/LIFE_SPEND.md` | да |
| `Content/Fluid`, `Content/Mess`, `Content/Swarm` (13) | материалы воды, грязи, роя и 3 системы Niagara | 10 материалов — свой код; 3 системы Niagara — копии шаблонов движка (раздел 2, пункт 4) | `Tools/make_fluid_assets.py`, `make_mess_assets.py`, `make_swarm_material.py` | да |

## 4. Модели Meshy через Higgsfield (22.11) — 22 штуки

Это единственная группа сгенерированных 3D-моделей в карте v4. Десять из них — те самые «подмены» из KAN-25.

| Модель | Что | В игре сейчас |
|---|---|---|
| `SM_M4R_K_Chair`, `SM_M4R_L_Armchair`, `SM_M4R_K_Kettle`, `SM_M4R_L_ToyCar`, `SM_M4R_L_ToyTrain`, `SM_M4R_K_Bin` | стул, кресло, чайник, машинка, паровозик, ведро с педалью | нет: поверх стоит модель из `Content/Hero` (Blender). Вернутся только с `-AntHeroArt=0` |
| `SM_M4R_L_PottedPlant`, `SM_M4R_K_PlantPot` | монстера, герань | **да**: модели в `Hero` для них нет |
| `SM_M4R_K_Mop`, `SM_M4R_V31_Broom` | швабра, веник | нет: код держит свои модели (решение владельца 26.11) |
| `SM_M4_BirthdayCake`, `SM_M4_Sandwich`, `SM_M4_Cutlet`, `SM_M4_Cookie`, `SM_M4_IceCream`, `SM_M4_AppleSlice`, `SM_M4_Popcorn`, `SM_M4_Zefir`, `SM_M4_Caramel` | еда карты v4 | не проверено запуском после включения `FoodHero` по умолчанию |
| `SM_M4_Teddy` | мишка | да (декор) |
| `SM_M4_FlyBody`, `SM_M4_SpiderBody` | тела мухи и паука (ноги и крылья — Blender) | да |

Ещё три «подмены» из KAN-25 — `SM_M4R_K_Calendar`, `SM_M4R_V3_KidDrawing`, `SM_M4R_V3_PictureFrame` — не 3D-генерация: это геометрия наших же мешей с напечатанной картинкой Higgsfield. Календарь и рисунок сейчас перекрыты моделями из `Hero`, рамка картины — нет.

## 5. Модели Meshy (октябрь) — 34 штуки, где на них ссылается код

Таблица показывает, где имя встречается в `Source/`. Часть этих ссылок — запасной путь на случай, когда нет модельной кухни; какие из 34 реально видны в текущей игре, запуском не проверялось.

| Модель | Ссылки в коде |
|---|---|
| `SM_Ant_Body`, `SM_Ant_Coxa`, `SM_Ant_Femur`, `SM_Ant_Tibia`, `SM_Ant_Antenna_Scape`, `SM_Ant_Antenna_Flagellum`, `SM_Ant_AcidGland` | `AntBody.cpp`, `AntCharacter.cpp`, `AntArmy.cpp`, `AntChain.cpp`, `AntRagdoll.cpp` |
| `SM_Human_Dad`, `SM_Slipper`, `SM_BugSpray`, `SM_RolledNewspaper` | `HumanCharacter.cpp`, `HumanBody.cpp` |
| `SM_Food_CakePiece`, `SM_Food_CheeseChunk`, `SM_Food_Chocolate`, `SM_Food_Gummy`, `SM_Food_GummyBearGold`, `SM_Food_HoneyDrop`, `SM_Food_SugarCube` | `AntTypes.cpp`, `AntReact.cpp`, `KitchenInteractables.cpp` |
| `SM_Fridge_Body`, `SM_Fridge_Door`, `SM_Oven_Body`, `SM_Oven_Door`, `SM_Cabinet_Base`, `SM_Cabinet_Door`, `SM_Drawer` | `AntTypes.cpp`, `KitchenBuilder.cpp`, `KitchenInteractables.cpp` |
| `SM_Toaster_Body`, `SM_Toaster_Lever`, `SM_CerealBox`, `SM_HoneyJar`, `SM_JamJar` | `KitchenBuilder.cpp`, `KitchenInteractables.cpp` |
| `SM_LightSwitch`, `SM_WallVent`, `SM_Wire_Exposed`, `SM_MouseTrap` | `KitchenInteractables.cpp` |

## 6. Полный список по ассетам

Одна строка — один ассет (папка модели со всеми её файлами, набор текстур, звук со всеми вариантами). Список собран из `git ls-files Content` и правил, перечисленных выше.

### Ant — 9 ассетов (Blender (свой скрипт): 9)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `SM_AntB_FlagA` | 1 | Blender (свой скрипт) |  |
| `SM_AntB_FlagB` | 1 | Blender (свой скрипт) |  |
| `SM_AntB_Gaster` | 1 | Blender (свой скрипт) |  |
| `SM_AntB_Head` | 1 | Blender (свой скрипт) |  |
| `SM_AntB_Jaw` | 1 | Blender (свой скрипт) |  |
| `SM_AntB_Petiole` | 1 | Blender (свой скрипт) |  |
| `SM_AntB_Tarsus` | 1 | Blender (свой скрипт) |  |
| `SM_AntB_Thorax` | 1 | Blender (свой скрипт) |  |
| `SM_AntB_Tibia` | 1 | Blender (свой скрипт) |  |

### Audio/Mix — 7 ассетов (свой код: 7)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `SC_Ambience` | 1 | свой код |  |
| `SC_Master` | 1 | свой код |  |
| `SC_Music` | 1 | свой код |  |
| `SC_Sfx` | 1 | свой код |  |
| `SC_Ui` | 1 | свой код |  |
| `SC_Voice` | 1 | свой код |  |
| `SM_AntDuck` | 1 | свой код |  |

### Audio/Music — 14 ассетов (свой код: 14)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `M_Funk_Action` | 1 | свой код |  |
| `M_Funk_Base` | 1 | свой код |  |
| `M_Funk_Tension` | 1 | свой код |  |
| `M_Funk_Urgency` | 1 | свой код |  |
| `M_Lounge_Action` | 1 | свой код |  |
| `M_Lounge_Base` | 1 | свой код |  |
| `M_Lounge_Tension` | 1 | свой код |  |
| `M_Lounge_Urgency` | 1 | свой код |  |
| `M_Nest` | 1 | свой код |  |
| `M_Sting_Card` | 1 | свой код |  |
| `M_Sting_Fail` | 1 | свой код |  |
| `M_Sting_LastLight` | 1 | свой код |  |
| `M_Sting_RaidStart` | 1 | свой код |  |
| `M_Sting_Win` | 1 | свой код |  |

### Audio/Sfx — 74 ассетов (Higgsfield: 62, свой код: 12)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `A_AcidHit` | 3 | Higgsfield |  |
| `A_AmbClock` | 1 | свой код |  |
| `A_AmbDay` | 1 | Higgsfield |  |
| `A_AmbDrip` | 1 | Higgsfield |  |
| `A_AmbEvening` | 1 | Higgsfield |  |
| `A_AmbFridge` | 1 | Higgsfield |  |
| `A_AmbNight` | 1 | Higgsfield |  |
| `A_AntHurt` | 3 | Higgsfield |  |
| `A_BatImpact` | 2 | Higgsfield |  |
| `A_BatWhoosh` | 2 | Higgsfield |  |
| `A_BiteN` | 5 | Higgsfield |  |
| `A_BreakerClick` | 2 | свой код |  |
| `A_Broom` | 2 | Higgsfield |  |
| `A_CardFlip` | 2 | Higgsfield |  |
| `A_CardPick` | 1 | свой код |  |
| `A_CarryStrain` | 3 | Higgsfield |  |
| `A_CarryThump` | 4 | Higgsfield |  |
| `A_CatHiss` | 2 | Higgsfield |  |
| `A_CatPounce` | 1 | Higgsfield |  |
| `A_CatPurr` | 1 | Higgsfield |  |
| `A_ColaFizz` | 1 | Higgsfield |  |
| `A_CrashCeramic` | 1 | Higgsfield |  |
| `A_CrashGlass` | 1 | Higgsfield |  |
| `A_CrashMetal` | 1 | Higgsfield |  |
| `A_CrashPlastic` | 1 | Higgsfield |  |
| `A_CrashWood` | 1 | Higgsfield |  |
| `A_Crumbs` | 1 | Higgsfield |  |
| `A_CurtainRustle` | 1 | Higgsfield |  |
| `A_DeliverCheer` | 1 | Higgsfield |  |
| `A_DeliverChime` | 1 | свой код |  |
| `A_DogBark` | 3 | Higgsfield |  |
| `A_DogEat` | 2 | Higgsfield |  |
| `A_DogSniff` | 2 | Higgsfield |  |
| `A_DogSnort` | 2 | Higgsfield |  |
| `A_EarSlap` | 2 | Higgsfield |  |
| `A_FlourWhoosh` | 2 | свой код |  |
| `A_GrabN` | 3 | Higgsfield |  |
| `A_Heartbeat` | 1 | Higgsfield |  |
| `A_HingeCreak` | 3 | Higgsfield |  |
| `A_HumanGrumble` | 2 | Higgsfield |  |
| `A_HumanPickup` | 2 | Higgsfield |  |
| `A_HumanRoar` | 2 | Higgsfield |  |
| `A_HumanSlip` | 1 | Higgsfield |  |
| `A_HumanSniff` | 1 | Higgsfield |  |
| `A_HumanStepN` | 4 | Higgsfield |  |
| `A_IceClink` | 2 | свой код |  |
| `A_KidGiggle` | 2 | Higgsfield |  |
| `A_KidShout` | 1 | Higgsfield |  |
| `A_MicroBeep` | 1 | свой код |  |
| `A_PocketRustle` | 2 | Higgsfield |  |
| `A_PowerDown` | 1 | свой код |  |
| `A_PowerUp` | 1 | свой код |  |
| `A_Release` | 3 | Higgsfield |  |
| `A_RipOff` | 1 | Higgsfield |  |
| `A_ScrewDrop` | 3 | Higgsfield |  |
| `A_ScrewTurn` | 3 | Higgsfield |  |
| `A_SlingTwang` | 2 | свой код |  |
| `A_SlipperSlap` | 2 | Higgsfield |  |
| `A_Sneeze` | 2 | Higgsfield |  |
| `A_SpiderSkitter` | 1 | Higgsfield |  |
| `A_SpitN` | 4 | Higgsfield |  |
| `A_SplatCream` | 3 | Higgsfield |  |
| `A_SplatJam` | 3 | Higgsfield |  |
| `A_StaticCrackle` | 2 | свой код |  |
| `A_StepMetalN` | 5 | Higgsfield |  |
| `A_StepRug` | 5 | Higgsfield |  |
| `A_StepSticky` | 5 | Higgsfield |  |
| `A_StepTile` | 5 | Higgsfield |  |
| `A_StepWoodN` | 5 | Higgsfield |  |
| `A_SwatterN` | 1 | Higgsfield |  |
| `A_TrailCut` | 1 | свой код |  |
| `A_TrailLay` | 3 | Higgsfield |  |
| `A_Wobble` | 1 | Higgsfield |  |
| `A_WorkerPatter` | 1 | Higgsfield |  |

### Fluid — 6 ассетов (свой код: 3, движок Epic: 3)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `M_AntDrop` | 1 | свой код |  |
| `M_AntLiquid` | 1 | свой код |  |
| `M_AntStream` | 1 | свой код |  |
| `NS_FlipHose` | 1 | движок Epic | копия шаблона плагина NiagaraFluids (Epic) |
| `NS_FlipPool` | 1 | движок Epic | копия шаблона плагина NiagaraFluids (Epic) |
| `NS_FlipSplash` | 1 | движок Epic | копия шаблона плагина NiagaraFluids (Epic) |

### FoodHero — 33 ассетов (Blender (свой скрипт): 33)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `SM_FoodHero_Apple` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Bone` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Bread` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Butter` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Cake` | 19 | Blender (свой скрипт) |  |
| `SM_FoodHero_Candy` | 17 | Blender (свой скрипт) |  |
| `SM_FoodHero_Cheese` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Chocolate` | 19 | Blender (свой скрипт) |  |
| `SM_FoodHero_Condensed` | 11 | Blender (свой скрипт) |  |
| `SM_FoodHero_Cookie` | 17 | Blender (свой скрипт) |  |
| `SM_FoodHero_Cracker` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Crisp` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Eggshell` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Grape` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Gum` | 11 | Blender (свой скрипт) |  |
| `SM_FoodHero_Gummy` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_GummyBear` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Honey` | 11 | Blender (свой скрипт) |  |
| `SM_FoodHero_Ice` | 17 | Blender (свой скрипт) |  |
| `SM_FoodHero_IceCream` | 17 | Blender (свой скрипт) |  |
| `SM_FoodHero_Jam` | 11 | Blender (свой скрипт) |  |
| `SM_FoodHero_Marshmallow` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Noodle` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Nut` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Olive` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Pancake` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Pea` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Sausage` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Sponge` | 17 | Blender (свой скрипт) |  |
| `SM_FoodHero_Sugar` | 17 | Blender (свой скрипт) |  |
| `SM_FoodHero_Tomato` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Wafer` | 15 | Blender (свой скрипт) |  |
| `SM_FoodHero_Watermelon` | 15 | Blender (свой скрипт) |  |

### FoodV2 — 35 ассетов (Blender (свой скрипт): 35)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `M_FoodV2` | 1 | Blender (свой скрипт) |  |
| `M_FoodV2_Inner` | 1 | Blender (свой скрипт) |  |
| `SM_FoodV2_Apple` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_Bone` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_Bread` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_Butter` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_Cake` | 19 | Blender (свой скрипт) |  |
| `SM_FoodV2_Candy` | 17 | Blender (свой скрипт) |  |
| `SM_FoodV2_Cheese` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_Chocolate` | 19 | Blender (свой скрипт) |  |
| `SM_FoodV2_Condensed` | 11 | Blender (свой скрипт) |  |
| `SM_FoodV2_Cookie` | 17 | Blender (свой скрипт) |  |
| `SM_FoodV2_Cracker` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_Crisp` | 13 | Blender (свой скрипт) |  |
| `SM_FoodV2_Eggshell` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_Grape` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_Gum` | 11 | Blender (свой скрипт) |  |
| `SM_FoodV2_Gummy` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_GummyBear` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_Honey` | 11 | Blender (свой скрипт) |  |
| `SM_FoodV2_Ice` | 17 | Blender (свой скрипт) |  |
| `SM_FoodV2_IceCream` | 17 | Blender (свой скрипт) |  |
| `SM_FoodV2_Jam` | 11 | Blender (свой скрипт) |  |
| `SM_FoodV2_Marshmallow` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_Noodle` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_Nut` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_Olive` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_Pancake` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_Pea` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_Sausage` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_Sponge` | 17 | Blender (свой скрипт) |  |
| `SM_FoodV2_Sugar` | 17 | Blender (свой скрипт) |  |
| `SM_FoodV2_Tomato` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_Wafer` | 15 | Blender (свой скрипт) |  |
| `SM_FoodV2_Watermelon` | 15 | Blender (свой скрипт) |  |

### Hero — 200 ассетов (Blender (свой скрипт): 200)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `SM_K_Base1200` | 12 | Blender (свой скрипт) |  |
| `SM_K_Base1200_DoorL` | 12 | Blender (свой скрипт) |  |
| `SM_K_Base1200_DoorR` | 12 | Blender (свой скрипт) |  |
| `SM_K_Base1300` | 12 | Blender (свой скрипт) |  |
| `SM_K_Base1300_DoorL` | 12 | Blender (свой скрипт) |  |
| `SM_K_Base1300_DoorR` | 12 | Blender (свой скрипт) |  |
| `SM_K_Base1400` | 12 | Blender (свой скрипт) |  |
| `SM_K_Base1400_DoorL` | 12 | Blender (свой скрипт) |  |
| `SM_K_Base1400_DoorR` | 12 | Blender (свой скрипт) |  |
| `SM_K_BaseDrawers` | 12 | Blender (свой скрипт) |  |
| `SM_K_BaseDrawers_Drawer1` | 12 | Blender (свой скрипт) |  |
| `SM_K_BaseDrawers_Drawer2` | 12 | Blender (свой скрипт) |  |
| `SM_K_BaseDrawers_Drawer3` | 12 | Blender (свой скрипт) |  |
| `SM_K_BaseSink` | 12 | Blender (свой скрипт) |  |
| `SM_K_BaseSink_DoorL` | 11 | Blender (свой скрипт) |  |
| `SM_K_BaseSink_DoorR` | 11 | Blender (свой скрипт) |  |
| `SM_K_Bin` | 9 | Blender (свой скрипт) |  |
| `SM_K_Bin_Lid` | 9 | Blender (свой скрипт) |  |
| `SM_K_Bin_Pedal` | 9 | Blender (свой скрипт) |  |
| `SM_K_Bottle` | 27 | Blender (свой скрипт) |  |
| `SM_K_Bowl` | 15 | Blender (свой скрипт) |  |
| `SM_K_Bucket` | 10 | Blender (свой скрипт) |  |
| `SM_K_ButterTub` | 7 | Blender (свой скрипт) |  |
| `SM_K_Calendar` | 5 | Blender (свой скрипт) |  |
| `SM_K_CarTrack` | 5 | Blender (свой скрипт) |  |
| `SM_K_CardboardBox` | 5 | Blender (свой скрипт) |  |
| `SM_K_CeilingLamp` | 6 | Blender (свой скрипт) |  |
| `SM_K_Chair` | 13 | Blender (свой скрипт) |  |
| `SM_K_CookieTin` | 8 | Blender (свой скрипт) |  |
| `SM_K_CuttingBoard` | 7 | Blender (свой скрипт) |  |
| `SM_K_Detergent` | 7 | Blender (свой скрипт) |  |
| `SM_K_DishRack` | 5 | Blender (свой скрипт) |  |
| `SM_K_Dishwasher` | 9 | Blender (свой скрипт) |  |
| `SM_K_Dishwasher_Door` | 9 | Blender (свой скрипт) |  |
| `SM_K_Dishwasher_Rack` | 9 | Blender (свой скрипт) |  |
| `SM_K_Dustpan` | 5 | Blender (свой скрипт) |  |
| `SM_K_EggTray` | 8 | Blender (свой скрипт) |  |
| `SM_K_Faucet` | 9 | Blender (свой скрипт) |  |
| `SM_K_Faucet_TapCold` | 9 | Blender (свой скрипт) |  |
| `SM_K_Faucet_TapHot` | 9 | Blender (свой скрипт) |  |
| `SM_K_FlourSack` | 8 | Blender (свой скрипт) |  |
| `SM_K_FlyPaper` | 5 | Blender (свой скрипт) |  |
| `SM_K_Fork` | 5 | Blender (свой скрипт) |  |
| `SM_K_Fridge` | 9 | Blender (свой скрипт) |  |
| `SM_K_FridgeNote` | 5 | Blender (свой скрипт) |  |
| `SM_K_Fridge_Crisper` | 9 | Blender (свой скрипт) |  |
| `SM_K_Fridge_DoorFreezer` | 9 | Blender (свой скрипт) |  |
| `SM_K_Fridge_DoorMain` | 9 | Blender (свой скрипт) |  |
| `SM_K_FruitBowl` | 15 | Blender (свой скрипт) |  |
| `SM_K_GroceryBag` | 8 | Blender (свой скрипт) |  |
| `SM_K_HighChair` | 13 | Blender (свой скрипт) |  |
| `SM_K_Hood` | 9 | Blender (свой скрипт) |  |
| `SM_K_IceCreamTub` | 7 | Blender (свой скрипт) |  |
| `SM_K_IceTray` | 8 | Blender (свой скрипт) |  |
| `SM_K_JarJam` | 25 | Blender (свой скрипт) |  |
| `SM_K_JarSugar` | 25 | Blender (свой скрипт) |  |
| `SM_K_Kettle` | 11 | Blender (свой скрипт) |  |
| `SM_K_Knife` | 5 | Blender (свой скрипт) |  |
| `SM_K_KnifeBlock` | 7 | Blender (свой скрипт) |  |
| `SM_K_Ladle` | 5 | Blender (свой скрипт) |  |
| `SM_K_Matchbox` | 5 | Blender (свой скрипт) |  |
| `SM_K_Matchbox_Tray` | 5 | Blender (свой скрипт) |  |
| `SM_K_Microwave` | 9 | Blender (свой скрипт) |  |
| `SM_K_Microwave_Door` | 9 | Blender (свой скрипт) |  |
| `SM_K_Microwave_Plate` | 9 | Blender (свой скрипт) |  |
| `SM_K_MilkCarton` | 8 | Blender (свой скрипт) |  |
| `SM_K_Mop` | 7 | Blender (свой скрипт) |  |
| `SM_K_MouseTrap` | 7 | Blender (свой скрипт) |  |
| `SM_K_MouseTrap_Bar` | 5 | Blender (свой скрипт) |  |
| `SM_K_Mug` | 14 | Blender (свой скрипт) |  |
| `SM_K_Newspaper` | 5 | Blender (свой скрипт) |  |
| `SM_K_PaperTowel` | 5 | Blender (свой скрипт) |  |
| `SM_K_PastaBox` | 7 | Blender (свой скрипт) |  |
| `SM_K_PetBowl` | 5 | Blender (свой скрипт) |  |
| `SM_K_Plate` | 14 | Blender (свой скрипт) |  |
| `SM_K_Pot` | 5 | Blender (свой скрипт) |  |
| `SM_K_Pot_Lid` | 7 | Blender (свой скрипт) |  |
| `SM_K_Range` | 9 | Blender (свой скрипт) |  |
| `SM_K_Range_Burner` | 9 | Blender (свой скрипт) |  |
| `SM_K_Range_Knob` | 9 | Blender (свой скрипт) |  |
| `SM_K_Range_OvenDoor` | 9 | Blender (свой скрипт) |  |
| `SM_K_Range_Rack` | 9 | Blender (свой скрипт) |  |
| `SM_K_Range_Tray` | 9 | Blender (свой скрипт) |  |
| `SM_K_RollingPin` | 5 | Blender (свой скрипт) |  |
| `SM_K_Rug` | 8 | Blender (свой скрипт) |  |
| `SM_K_Shaker` | 16 | Blender (свой скрипт) |  |
| `SM_K_Sink` | 9 | Blender (свой скрипт) |  |
| `SM_K_SinkTrap` | 9 | Blender (свой скрипт) |  |
| `SM_K_Skirting` | 7 | Blender (свой скрипт) |  |
| `SM_K_Spatula` | 5 | Blender (свой скрипт) |  |
| `SM_K_SpiceJar` | 22 | Blender (свой скрипт) |  |
| `SM_K_Sponge` | 7 | Blender (свой скрипт) |  |
| `SM_K_SugarBowl` | 18 | Blender (свой скрипт) |  |
| `SM_K_Switch` | 6 | Blender (свой скрипт) |  |
| `SM_K_Switch_Toggle` | 6 | Blender (свой скрипт) |  |
| `SM_K_Table` | 12 | Blender (свой скрипт) |  |
| `SM_K_TeaBag` | 5 | Blender (свой скрипт) |  |
| `SM_K_TennisBall` | 5 | Blender (свой скрипт) |  |
| `SM_K_TinCan` | 7 | Blender (свой скрипт) |  |
| `SM_K_Toaster` | 9 | Blender (свой скрипт) |  |
| `SM_K_Toaster_Lever` | 9 | Blender (свой скрипт) |  |
| `SM_K_Towel` | 5 | Blender (свой скрипт) |  |
| `SM_K_WallCab1250` | 12 | Blender (свой скрипт) |  |
| `SM_K_WallCab1250_DoorL` | 12 | Blender (свой скрипт) |  |
| `SM_K_WallCab1250_DoorR` | 12 | Blender (свой скрипт) |  |
| `SM_K_WallCab1500` | 12 | Blender (свой скрипт) |  |
| `SM_K_WallCab1500_DoorL` | 12 | Blender (свой скрипт) |  |
| `SM_K_WallCab1500_DoorR` | 12 | Blender (свой скрипт) |  |
| `SM_K_Whisk` | 5 | Blender (свой скрипт) |  |
| `SM_K_Window` | 8 | Blender (свой скрипт) |  |
| `SM_K_WindowSill` | 7 | Blender (свой скрипт) |  |
| `SM_K_WorktopA` | 12 | Blender (свой скрипт) |  |
| `SM_K_WorktopB` | 12 | Blender (свой скрипт) |  |
| `SM_L_Armchair` | 11 | Blender (свой скрипт) |  |
| `SM_L_BiscuitPlate` | 14 | Blender (свой скрипт) |  |
| `SM_L_Book_A` | 7 | Blender (свой скрипт) |  |
| `SM_L_Book_B` | 7 | Blender (свой скрипт) |  |
| `SM_L_Book_C` | 7 | Blender (свой скрипт) |  |
| `SM_L_Book_D` | 7 | Blender (свой скрипт) |  |
| `SM_L_Book_E` | 7 | Blender (свой скрипт) |  |
| `SM_L_Book_F` | 7 | Blender (свой скрипт) |  |
| `SM_L_Book_G` | 7 | Blender (свой скрипт) |  |
| `SM_L_Book_H` | 7 | Blender (свой скрипт) |  |
| `SM_L_Bookshelf` | 16 | Blender (свой скрипт) |  |
| `SM_L_Carpet` | 8 | Blender (свой скрипт) |  |
| `SM_L_CatFlap` | 6 | Blender (свой скрипт) |  |
| `SM_L_CatFlap_Leaf` | 7 | Blender (свой скрипт) |  |
| `SM_L_CatScratchPost` | 8 | Blender (свой скрипт) |  |
| `SM_L_CeilingLamp` | 6 | Blender (свой скрипт) |  |
| `SM_L_Charger` | 5 | Blender (свой скрипт) |  |
| `SM_L_CoffeeTable` | 11 | Blender (свой скрипт) |  |
| `SM_L_Coin` | 5 | Blender (свой скрипт) |  |
| `SM_L_Controller` | 5 | Blender (свой скрипт) |  |
| `SM_L_DogBed` | 7 | Blender (свой скрипт) |  |
| `SM_L_DoorFrame` | 9 | Blender (свой скрипт) |  |
| `SM_L_Door_Leaf` | 7 | Blender (свой скрипт) |  |
| `SM_L_FloorLamp` | 8 | Blender (свой скрипт) |  |
| `SM_L_GameConsole` | 5 | Blender (свой скрипт) |  |
| `SM_L_Magazines` | 5 | Blender (свой скрипт) |  |
| `SM_L_Pillow_A` | 6 | Blender (свой скрипт) |  |
| `SM_L_Pillow_B` | 6 | Blender (свой скрипт) |  |
| `SM_L_PowerStrip` | 5 | Blender (свой скрипт) |  |
| `SM_L_Radiator` | 7 | Blender (свой скрипт) |  |
| `SM_L_Remote` | 7 | Blender (свой скрипт) |  |
| `SM_L_ShoeRack` | 11 | Blender (свой скрипт) |  |
| `SM_L_SideTable` | 11 | Blender (свой скрипт) |  |
| `SM_L_Skirting` | 7 | Blender (свой скрипт) |  |
| `SM_L_SnackBowl` | 15 | Blender (свой скрипт) |  |
| `SM_L_Sock` | 5 | Blender (свой скрипт) |  |
| `SM_L_SodaCan` | 5 | Blender (свой скрипт) |  |
| `SM_L_Sofa` | 9 | Blender (свой скрипт) |  |
| `SM_L_Sofa_BackCushion` | 8 | Blender (свой скрипт) |  |
| `SM_L_Sofa_SeatCushion` | 8 | Blender (свой скрипт) |  |
| `SM_L_TV` | 10 | Blender (свой скрипт) |  |
| `SM_L_TVStand` | 12 | Blender (свой скрипт) |  |
| `SM_L_TVStand_Door` | 8 | Blender (свой скрипт) |  |
| `SM_L_ToyArch` | 5 | Blender (свой скрипт) |  |
| `SM_L_ToyCar` | 6 | Blender (свой скрипт) |  |
| `SM_L_ToyMouse` | 5 | Blender (свой скрипт) |  |
| `SM_L_ToyTrain` | 6 | Blender (свой скрипт) |  |
| `SM_L_TrainTrack` | 5 | Blender (свой скрипт) |  |
| `SM_L_YarnBall` | 5 | Blender (свой скрипт) |  |
| `SM_V31_BoardBooks` | 7 | Blender (свой скрипт) |  |
| `SM_V31_Broom` | 7 | Blender (свой скрипт) |  |
| `SM_V31_Throw` | 5 | Blender (свой скрипт) |  |
| `SM_V3_Balloon` | 5 | Blender (свой скрипт) |  |
| `SM_V3_BalloonWeight` | 5 | Blender (свой скрипт) |  |
| `SM_V3_CerealBox` | 7 | Blender (свой скрипт) |  |
| `SM_V3_ColaBottle` | 8 | Blender (свой скрипт) |  |
| `SM_V3_ColaCap` | 5 | Blender (свой скрипт) |  |
| `SM_V3_Cornice` | 7 | Blender (свой скрипт) |  |
| `SM_V3_CurtainPanel` | 5 | Blender (свой скрипт) |  |
| `SM_V3_CurtainRing` | 6 | Blender (свой скрипт) |  |
| `SM_V3_CurtainRod` | 6 | Blender (свой скрипт) |  |
| `SM_V3_DishSoap` | 7 | Blender (свой скрипт) |  |
| `SM_V3_DogFoodBag` | 7 | Blender (свой скрипт) |  |
| `SM_V3_FloorCushion` | 7 | Blender (свой скрипт) |  |
| `SM_V3_GiftBox` | 7 | Blender (свой скрипт) |  |
| `SM_V3_GiftBox_Lid` | 5 | Blender (свой скрипт) |  |
| `SM_V3_Glasses` | 8 | Blender (свой скрипт) |  |
| `SM_V3_Hinge` | 6 | Blender (свой скрипт) |  |
| `SM_V3_JamJar` | 22 | Blender (свой скрипт) |  |
| `SM_V3_JamJar_Lid` | 5 | Blender (свой скрипт) |  |
| `SM_V3_KidDrawing` | 5 | Blender (свой скрипт) |  |
| `SM_V3_KnitNeedle` | 5 | Blender (свой скрипт) |  |
| `SM_V3_Outlet` | 6 | Blender (свой скрипт) |  |
| `SM_V3_Pail` | 9 | Blender (свой скрипт) |  |
| `SM_V3_PendantLamp` | 6 | Blender (свой скрипт) |  |
| `SM_V3_Phone` | 5 | Blender (свой скрипт) |  |
| `SM_V3_PickleJar` | 24 | Blender (свой скрипт) |  |
| `SM_V3_Plunger` | 7 | Blender (свой скрипт) |  |
| `SM_V3_RagPile` | 5 | Blender (свой скрипт) |  |
| `SM_V3_RodBracket` | 6 | Blender (свой скрипт) |  |
| `SM_V3_ShelfBracket` | 6 | Blender (свой скрипт) |  |
| `SM_V3_SoakBoard` | 7 | Blender (свой скрипт) |  |
| `SM_V3_Sprayer` | 7 | Blender (свой скрипт) |  |
| `SM_V3_ToyBox` | 10 | Blender (свой скрипт) |  |
| `SM_V3_ToyBox_Lid` | 7 | Blender (свой скрипт) |  |
| `SM_V3_WallShelf` | 7 | Blender (свой скрипт) |  |
| `SM_V3_WireRack` | 5 | Blender (свой скрипт) |  |

### Human — 20 ассетов (Blender (свой скрипт): 19, свой код: 1)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `M_HumanSurface` | 1 | свой код | материал, Tools/import_human.py |
| `SM_H_Belly` | 1 | Blender (свой скрипт) |  |
| `SM_H_Chest` | 1 | Blender (свой скрипт) |  |
| `SM_H_Fingers_L` | 1 | Blender (свой скрипт) |  |
| `SM_H_Fingers_R` | 1 | Blender (свой скрипт) |  |
| `SM_H_Foot_L` | 1 | Blender (свой скрипт) |  |
| `SM_H_Foot_R` | 1 | Blender (свой скрипт) |  |
| `SM_H_Forearm_L` | 1 | Blender (свой скрипт) |  |
| `SM_H_Forearm_R` | 1 | Blender (свой скрипт) |  |
| `SM_H_Hand_L` | 1 | Blender (свой скрипт) |  |
| `SM_H_Hand_R` | 1 | Blender (свой скрипт) |  |
| `SM_H_Head` | 1 | Blender (свой скрипт) |  |
| `SM_H_Neck` | 1 | Blender (свой скрипт) |  |
| `SM_H_Pelvis` | 1 | Blender (свой скрипт) |  |
| `SM_H_Shin_L` | 1 | Blender (свой скрипт) |  |
| `SM_H_Shin_R` | 1 | Blender (свой скрипт) |  |
| `SM_H_Thigh_L` | 1 | Blender (свой скрипт) |  |
| `SM_H_Thigh_R` | 1 | Blender (свой скрипт) |  |
| `SM_H_UpperArm_L` | 1 | Blender (свой скрипт) |  |
| `SM_H_UpperArm_R` | 1 | Blender (свой скрипт) |  |

### Kitchen — 147 ассетов (Blender (свой скрипт): 147)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `Hero` | 12 | Blender (свой скрипт) |  |
| `SM_K_Apple` | 1 | Blender (свой скрипт) |  |
| `SM_K_Base1200` | 1 | Blender (свой скрипт) |  |
| `SM_K_Base1200_DoorL` | 1 | Blender (свой скрипт) |  |
| `SM_K_Base1200_DoorR` | 1 | Blender (свой скрипт) |  |
| `SM_K_Base1300` | 1 | Blender (свой скрипт) |  |
| `SM_K_Base1300_DoorL` | 1 | Blender (свой скрипт) |  |
| `SM_K_Base1300_DoorR` | 1 | Blender (свой скрипт) |  |
| `SM_K_Base1400` | 1 | Blender (свой скрипт) |  |
| `SM_K_Base1400_DoorL` | 1 | Blender (свой скрипт) |  |
| `SM_K_Base1400_DoorR` | 1 | Blender (свой скрипт) |  |
| `SM_K_BaseDrawers` | 1 | Blender (свой скрипт) |  |
| `SM_K_BaseDrawers_Drawer1` | 1 | Blender (свой скрипт) |  |
| `SM_K_BaseDrawers_Drawer2` | 1 | Blender (свой скрипт) |  |
| `SM_K_BaseDrawers_Drawer3` | 1 | Blender (свой скрипт) |  |
| `SM_K_BaseSink` | 1 | Blender (свой скрипт) |  |
| `SM_K_BaseSink_DoorL` | 1 | Blender (свой скрипт) |  |
| `SM_K_BaseSink_DoorR` | 1 | Blender (свой скрипт) |  |
| `SM_K_Bin` | 1 | Blender (свой скрипт) |  |
| `SM_K_Bin_Lid` | 1 | Blender (свой скрипт) |  |
| `SM_K_Bin_Pedal` | 1 | Blender (свой скрипт) |  |
| `SM_K_Bone` | 1 | Blender (свой скрипт) |  |
| `SM_K_Bottle` | 1 | Blender (свой скрипт) |  |
| `SM_K_Bowl` | 1 | Blender (свой скрипт) |  |
| `SM_K_BreadBox` | 1 | Blender (свой скрипт) |  |
| `SM_K_BreadBox_Lid` | 1 | Blender (свой скрипт) |  |
| `SM_K_Broom` | 1 | Blender (свой скрипт) |  |
| `SM_K_Bucket` | 10 | Blender (свой скрипт) |  |
| `SM_K_Bun` | 1 | Blender (свой скрипт) |  |
| `SM_K_ButterTub` | 1 | Blender (свой скрипт) |  |
| `SM_K_Cake` | 1 | Blender (свой скрипт) |  |
| `SM_K_Calendar` | 1 | Blender (свой скрипт) |  |
| `SM_K_CarTrack` | 1 | Blender (свой скрипт) |  |
| `SM_K_CardboardBox` | 1 | Blender (свой скрипт) |  |
| `SM_K_CeilingLamp` | 1 | Blender (свой скрипт) |  |
| `SM_K_CerealBox` | 1 | Blender (свой скрипт) |  |
| `SM_K_Chair` | 1 | Blender (свой скрипт) |  |
| `SM_K_CheeseWedge` | 1 | Blender (свой скрипт) |  |
| `SM_K_ChiliString` | 1 | Blender (свой скрипт) |  |
| `SM_K_Clock` | 1 | Blender (свой скрипт) |  |
| `SM_K_CookieTin` | 1 | Blender (свой скрипт) |  |
| `SM_K_Curtain` | 1 | Blender (свой скрипт) |  |
| `SM_K_CurtainRod` | 1 | Blender (свой скрипт) |  |
| `SM_K_CuttingBoard` | 1 | Blender (свой скрипт) |  |
| `SM_K_Detergent` | 1 | Blender (свой скрипт) |  |
| `SM_K_DishRack` | 1 | Blender (свой скрипт) |  |
| `SM_K_Dishwasher` | 1 | Blender (свой скрипт) |  |
| `SM_K_Dishwasher_Door` | 1 | Blender (свой скрипт) |  |
| `SM_K_Dishwasher_Rack` | 1 | Blender (свой скрипт) |  |
| `SM_K_Dustpan` | 1 | Blender (свой скрипт) |  |
| `SM_K_EggTray` | 1 | Blender (свой скрипт) |  |
| `SM_K_Faucet` | 1 | Blender (свой скрипт) |  |
| `SM_K_Faucet_TapCold` | 1 | Blender (свой скрипт) |  |
| `SM_K_Faucet_TapHot` | 1 | Blender (свой скрипт) |  |
| `SM_K_FlourSack` | 1 | Blender (свой скрипт) |  |
| `SM_K_FlyPaper` | 1 | Blender (свой скрипт) |  |
| `SM_K_Fork` | 1 | Blender (свой скрипт) |  |
| `SM_K_Fridge` | 1 | Blender (свой скрипт) |  |
| `SM_K_FridgeMagnet` | 1 | Blender (свой скрипт) |  |
| `SM_K_FridgeNote` | 1 | Blender (свой скрипт) |  |
| `SM_K_Fridge_Crisper` | 1 | Blender (свой скрипт) |  |
| `SM_K_Fridge_DoorFreezer` | 1 | Blender (свой скрипт) |  |
| `SM_K_Fridge_DoorMain` | 1 | Blender (свой скрипт) |  |
| `SM_K_FruitBowl` | 1 | Blender (свой скрипт) |  |
| `SM_K_GroceryBag` | 1 | Blender (свой скрипт) |  |
| `SM_K_HighChair` | 1 | Blender (свой скрипт) |  |
| `SM_K_Hood` | 1 | Blender (свой скрипт) |  |
| `SM_K_IceCreamTub` | 1 | Blender (свой скрипт) |  |
| `SM_K_IceTray` | 1 | Blender (свой скрипт) |  |
| `SM_K_JamPuddle` | 1 | Blender (свой скрипт) |  |
| `SM_K_JarHoney` | 1 | Blender (свой скрипт) |  |
| `SM_K_JarJam` | 1 | Blender (свой скрипт) |  |
| `SM_K_JarSugar` | 1 | Blender (свой скрипт) |  |
| `SM_K_Kettle` | 1 | Blender (свой скрипт) |  |
| `SM_K_Kibble` | 1 | Blender (свой скрипт) |  |
| `SM_K_Knife` | 1 | Blender (свой скрипт) |  |
| `SM_K_KnifeBlock` | 1 | Blender (свой скрипт) |  |
| `SM_K_Ladle` | 1 | Blender (свой скрипт) |  |
| `SM_K_Matchbox` | 1 | Blender (свой скрипт) |  |
| `SM_K_Matchbox_Tray` | 1 | Blender (свой скрипт) |  |
| `SM_K_Microwave` | 1 | Blender (свой скрипт) |  |
| `SM_K_Microwave_Door` | 1 | Blender (свой скрипт) |  |
| `SM_K_Microwave_Plate` | 1 | Blender (свой скрипт) |  |
| `SM_K_MilkCarton` | 1 | Blender (свой скрипт) |  |
| `SM_K_Mop` | 7 | Blender (свой скрипт) |  |
| `SM_K_MouseTrap` | 1 | Blender (свой скрипт) |  |
| `SM_K_MouseTrap_Bar` | 1 | Blender (свой скрипт) |  |
| `SM_K_Mug` | 1 | Blender (свой скрипт) |  |
| `SM_K_Newspaper` | 1 | Blender (свой скрипт) |  |
| `SM_K_PaperTowel` | 1 | Blender (свой скрипт) |  |
| `SM_K_PastaBox` | 1 | Blender (свой скрипт) |  |
| `SM_K_PetBowl` | 1 | Blender (свой скрипт) |  |
| `SM_K_PetMat` | 1 | Blender (свой скрипт) |  |
| `SM_K_PlantPot` | 1 | Blender (свой скрипт) |  |
| `SM_K_Plate` | 1 | Blender (свой скрипт) |  |
| `SM_K_Pot` | 1 | Blender (свой скрипт) |  |
| `SM_K_Pot_Lid` | 1 | Blender (свой скрипт) |  |
| `SM_K_Puddle` | 1 | Blender (свой скрипт) |  |
| `SM_K_Range` | 1 | Blender (свой скрипт) |  |
| `SM_K_Range_Burner` | 1 | Blender (свой скрипт) |  |
| `SM_K_Range_Knob` | 1 | Blender (свой скрипт) |  |
| `SM_K_Range_OvenDoor` | 1 | Blender (свой скрипт) |  |
| `SM_K_Range_Rack` | 1 | Blender (свой скрипт) |  |
| `SM_K_Range_Tray` | 1 | Blender (свой скрипт) |  |
| `SM_K_RollingPin` | 1 | Blender (свой скрипт) |  |
| `SM_K_Rug` | 1 | Blender (свой скрипт) |  |
| `SM_K_Sandwich` | 1 | Blender (свой скрипт) |  |
| `SM_K_Shaker` | 1 | Blender (свой скрипт) |  |
| `SM_K_Shoe` | 1 | Blender (свой скрипт) |  |
| `SM_K_Sink` | 1 | Blender (свой скрипт) |  |
| `SM_K_SinkHose` | 1 | Blender (свой скрипт) |  |
| `SM_K_SinkTrap` | 1 | Blender (свой скрипт) |  |
| `SM_K_Skirting` | 1 | Blender (свой скрипт) |  |
| `SM_K_Slipper` | 1 | Blender (свой скрипт) |  |
| `SM_K_Spatula` | 1 | Blender (свой скрипт) |  |
| `SM_K_SpiceJar` | 1 | Blender (свой скрипт) |  |
| `SM_K_SpiceShelf` | 1 | Blender (свой скрипт) |  |
| `SM_K_Splashback` | 1 | Blender (свой скрипт) |  |
| `SM_K_Sponge` | 1 | Blender (свой скрипт) |  |
| `SM_K_Spoon` | 1 | Blender (свой скрипт) |  |
| `SM_K_SugarBowl` | 1 | Blender (свой скрипт) |  |
| `SM_K_SugarSpill` | 1 | Blender (свой скрипт) |  |
| `SM_K_Switch` | 1 | Blender (свой скрипт) |  |
| `SM_K_Switch_Toggle` | 1 | Blender (свой скрипт) |  |
| `SM_K_Table` | 1 | Blender (свой скрипт) |  |
| `SM_K_TeaBag` | 1 | Blender (свой скрипт) |  |
| `SM_K_TennisBall` | 1 | Blender (свой скрипт) |  |
| `SM_K_TinCan` | 1 | Blender (свой скрипт) |  |
| `SM_K_Toast` | 1 | Blender (свой скрипт) |  |
| `SM_K_Toaster` | 1 | Blender (свой скрипт) |  |
| `SM_K_Toaster_Lever` | 1 | Blender (свой скрипт) |  |
| `SM_K_Towel` | 1 | Blender (свой скрипт) |  |
| `SM_K_UtensilRail` | 1 | Blender (свой скрипт) |  |
| `SM_K_VentGrille` | 1 | Blender (свой скрипт) |  |
| `SM_K_VentGrilleSmall` | 1 | Blender (свой скрипт) |  |
| `SM_K_WallCab1250` | 1 | Blender (свой скрипт) |  |
| `SM_K_WallCab1250_DoorL` | 1 | Blender (свой скрипт) |  |
| `SM_K_WallCab1250_DoorR` | 1 | Blender (свой скрипт) |  |
| `SM_K_WallCab1500` | 1 | Blender (свой скрипт) |  |
| `SM_K_WallCab1500_DoorL` | 1 | Blender (свой скрипт) |  |
| `SM_K_WallCab1500_DoorR` | 1 | Blender (свой скрипт) |  |
| `SM_K_WallShelf` | 1 | Blender (свой скрипт) |  |
| `SM_K_Whisk` | 1 | Blender (свой скрипт) |  |
| `SM_K_Window` | 1 | Blender (свой скрипт) |  |
| `SM_K_WindowSill` | 1 | Blender (свой скрипт) |  |
| `SM_K_WorktopA` | 1 | Blender (свой скрипт) |  |
| `SM_K_WorktopB` | 1 | Blender (свой скрипт) |  |

### LevelV3 — 69 ассетов (Blender (свой скрипт): 69)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `SM_V31_BoardBooks` | 1 | Blender (свой скрипт) |  |
| `SM_V31_Broom` | 7 | Blender (свой скрипт) |  |
| `SM_V31_DeliveryBox` | 1 | Blender (свой скрипт) |  |
| `SM_V31_PizzaPeel` | 1 | Blender (свой скрипт) |  |
| `SM_V31_Throw` | 1 | Blender (свой скрипт) |  |
| `SM_V3_Backpack` | 1 | Blender (свой скрипт) |  |
| `SM_V3_Balloon` | 1 | Blender (свой скрипт) |  |
| `SM_V3_BalloonString` | 1 | Blender (свой скрипт) |  |
| `SM_V3_BalloonWeight` | 1 | Blender (свой скрипт) |  |
| `SM_V3_BlanketFort` | 1 | Blender (свой скрипт) |  |
| `SM_V3_CerealBox` | 1 | Blender (свой скрипт) |  |
| `SM_V3_CerealSpill` | 1 | Blender (свой скрипт) |  |
| `SM_V3_ColaBottle` | 1 | Blender (свой скрипт) |  |
| `SM_V3_ColaCap` | 1 | Blender (свой скрипт) |  |
| `SM_V3_ColaPuddle` | 1 | Blender (свой скрипт) |  |
| `SM_V3_Cornice` | 1 | Blender (свой скрипт) |  |
| `SM_V3_CurtainPanel` | 1 | Blender (свой скрипт) |  |
| `SM_V3_CurtainRing` | 1 | Blender (свой скрипт) |  |
| `SM_V3_CurtainRod` | 1 | Blender (свой скрипт) |  |
| `SM_V3_CurtainTorn` | 1 | Blender (свой скрипт) |  |
| `SM_V3_DirtyDishStack` | 1 | Blender (свой скрипт) |  |
| `SM_V3_DishSoap` | 7 | Blender (свой скрипт) |  |
| `SM_V3_DogFoodBag` | 1 | Blender (свой скрипт) |  |
| `SM_V3_DoorMat` | 1 | Blender (свой скрипт) |  |
| `SM_V3_FloorCushion` | 1 | Blender (свой скрипт) |  |
| `SM_V3_GiftBox` | 1 | Blender (свой скрипт) |  |
| `SM_V3_GiftBox_Lid` | 1 | Blender (свой скрипт) |  |
| `SM_V3_GlassShards` | 1 | Blender (свой скрипт) |  |
| `SM_V3_Glasses` | 1 | Blender (свой скрипт) |  |
| `SM_V3_Hinge` | 1 | Blender (свой скрипт) |  |
| `SM_V3_JamJar` | 1 | Blender (свой скрипт) |  |
| `SM_V3_JamJar_Lid` | 1 | Blender (свой скрипт) |  |
| `SM_V3_JamSplat` | 1 | Blender (свой скрипт) |  |
| `SM_V3_KidDrawing` | 1 | Blender (свой скрипт) |  |
| `SM_V3_KnitNeedle` | 1 | Blender (свой скрипт) |  |
| `SM_V3_KnitScarf` | 1 | Blender (свой скрипт) |  |
| `SM_V3_LaundryLine` | 1 | Blender (свой скрипт) |  |
| `SM_V3_LaundryPole` | 1 | Blender (свой скрипт) |  |
| `SM_V3_Nail` | 1 | Blender (свой скрипт) |  |
| `SM_V3_NewspaperStack` | 1 | Blender (свой скрипт) |  |
| `SM_V3_Outlet` | 1 | Blender (свой скрипт) |  |
| `SM_V3_Pail` | 9 | Blender (свой скрипт) |  |
| `SM_V3_PendantLamp` | 1 | Blender (свой скрипт) |  |
| `SM_V3_Phone` | 1 | Blender (свой скрипт) |  |
| `SM_V3_PickleJar` | 1 | Blender (свой скрипт) |  |
| `SM_V3_PictureFrame` | 1 | Blender (свой скрипт) |  |
| `SM_V3_Plunger` | 7 | Blender (свой скрипт) |  |
| `SM_V3_RagPile` | 5 | Blender (свой скрипт) |  |
| `SM_V3_RodBracket` | 1 | Blender (свой скрипт) |  |
| `SM_V3_Screw` | 1 | Blender (свой скрипт) |  |
| `SM_V3_ShelfBracket` | 1 | Blender (свой скрипт) |  |
| `SM_V3_ShoppingTrolley` | 1 | Blender (свой скрипт) |  |
| `SM_V3_SoakBoard` | 7 | Blender (свой скрипт) |  |
| `SM_V3_SoilHeap` | 1 | Blender (свой скрипт) |  |
| `SM_V3_Sprayer` | 7 | Blender (свой скрипт) |  |
| `SM_V3_StepStool` | 1 | Blender (свой скрипт) |  |
| `SM_V3_Tablecloth` | 1 | Blender (свой скрипт) |  |
| `SM_V3_TableclothLong` | 1 | Blender (свой скрипт) |  |
| `SM_V3_TableclothPulled` | 1 | Blender (свой скрипт) |  |
| `SM_V3_TapeStrip` | 1 | Blender (свой скрипт) |  |
| `SM_V3_TeaCup` | 1 | Blender (свой скрипт) |  |
| `SM_V3_Teapot` | 1 | Blender (свой скрипт) |  |
| `SM_V3_Teapot_Lid` | 1 | Blender (свой скрипт) |  |
| `SM_V3_ToyBox` | 1 | Blender (свой скрипт) |  |
| `SM_V3_ToyBox_Lid` | 1 | Blender (свой скрипт) |  |
| `SM_V3_TrashBag` | 1 | Blender (свой скрипт) |  |
| `SM_V3_VeggieCrate` | 1 | Blender (свой скрипт) |  |
| `SM_V3_WallShelf` | 1 | Blender (свой скрипт) |  |
| `SM_V3_WireRack` | 5 | Blender (свой скрипт) |  |

### Life/Audio — 14 ассетов (свой код: 14)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `A_Life_CarPass` | 1 | свой код |  |
| `A_Life_Click` | 1 | свой код |  |
| `A_Life_Drip` | 1 | свой код |  |
| `A_Life_Flap` | 1 | свой код |  |
| `A_Life_FlyBuzz` | 1 | свой код |  |
| `A_Life_KettleBoil` | 1 | свой код |  |
| `A_Life_MothTap` | 1 | свой код |  |
| `A_Life_MusicBox` | 1 | свой код |  |
| `A_Life_PhoneBuzz` | 1 | свой код |  |
| `A_Life_Pigeon` | 1 | свой код |  |
| `A_Life_Radio` | 1 | свой код |  |
| `A_Life_Rain` | 1 | свой код |  |
| `A_Life_TvStatic` | 1 | свой код |  |
| `A_Life_Wash` | 1 | свой код |  |

### Life/Materials — 7 ассетов (свой код: 7)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `M_Life_Clouds` | 1 | свой код |  |
| `M_Life_Decal` | 1 | свой код |  |
| `M_Life_Display` | 1 | свой код |  |
| `M_Life_Facade` | 1 | свой код |  |
| `M_Life_Rain` | 1 | свой код |  |
| `M_Life_Screen` | 1 | свой код |  |
| `M_Life_Stars` | 1 | свой код |  |

### Life/Media — 1 ассетов (свой код: 1)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `MT_Life_Default` | 1 | свой код |  |

### Life/Textures — 34 ассетов (свой код: 34)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `T_Life_Calendar` | 1 | свой код |  |
| `T_Life_CoffeeRing` | 1 | свой код |  |
| `T_Life_Graffiti` | 1 | свой код |  |
| `T_Life_Letter00` | 1 | свой код |  |
| `T_Life_Letter01` | 1 | свой код |  |
| `T_Life_Letter02` | 1 | свой код |  |
| `T_Life_Letter03` | 1 | свой код |  |
| `T_Life_Letter04` | 1 | свой код |  |
| `T_Life_Letter05` | 1 | свой код |  |
| `T_Life_Letter06` | 1 | свой код |  |
| `T_Life_Letter07` | 1 | свой код |  |
| `T_Life_Letter08` | 1 | свой код |  |
| `T_Life_Letter09` | 1 | свой код |  |
| `T_Life_Letter10` | 1 | свой код |  |
| `T_Life_Letter11` | 1 | свой код |  |
| `T_Life_Letter12` | 1 | свой код |  |
| `T_Life_Note0` | 1 | свой код |  |
| `T_Life_Note1` | 1 | свой код |  |
| `T_Life_Note2` | 1 | свой код |  |
| `T_Life_Note3` | 1 | свой код |  |
| `T_Life_Note4` | 1 | свой код |  |
| `T_Life_Note5` | 1 | свой код |  |
| `T_Life_Note6` | 1 | свой код |  |
| `T_Life_Note7` | 1 | свой код |  |
| `T_Life_PhoneFeed` | 1 | свой код |  |
| `T_Life_PhoneLock` | 1 | свой код |  |
| `T_Life_Pigeon` | 1 | свой код |  |
| `T_Life_Price0` | 1 | свой код |  |
| `T_Life_Price1` | 1 | свой код |  |
| `T_Life_Price2` | 1 | свой код |  |
| `T_Life_Receipt` | 1 | свой код |  |
| `T_Life_Scratches` | 1 | свой код |  |
| `T_Life_Sparrow` | 1 | свой код |  |
| `T_Life_Tit` | 1 | свой код |  |

### Living — 106 ассетов (Blender (свой скрипт): 106)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `SM_L_Armchair` | 1 | Blender (свой скрипт) |  |
| `SM_L_BiscuitPlate` | 1 | Blender (свой скрипт) |  |
| `SM_L_Blanket` | 1 | Blender (свой скрипт) |  |
| `SM_L_BookRamp` | 1 | Blender (свой скрипт) |  |
| `SM_L_Book_A` | 1 | Blender (свой скрипт) |  |
| `SM_L_Book_B` | 1 | Blender (свой скрипт) |  |
| `SM_L_Book_C` | 1 | Blender (свой скрипт) |  |
| `SM_L_Book_D` | 1 | Blender (свой скрипт) |  |
| `SM_L_Book_E` | 1 | Blender (свой скрипт) |  |
| `SM_L_Book_F` | 1 | Blender (свой скрипт) |  |
| `SM_L_Book_G` | 1 | Blender (свой скрипт) |  |
| `SM_L_Book_H` | 1 | Blender (свой скрипт) |  |
| `SM_L_Bookshelf` | 1 | Blender (свой скрипт) |  |
| `SM_L_Carpet` | 1 | Blender (свой скрипт) |  |
| `SM_L_CatFlap` | 1 | Blender (свой скрипт) |  |
| `SM_L_CatFlap_Leaf` | 1 | Blender (свой скрипт) |  |
| `SM_L_CatScratchPost` | 1 | Blender (свой скрипт) |  |
| `SM_L_CeilingLamp` | 1 | Blender (свой скрипт) |  |
| `SM_L_Charger` | 1 | Blender (свой скрипт) |  |
| `SM_L_CoffeeTable` | 1 | Blender (свой скрипт) |  |
| `SM_L_Coin` | 1 | Blender (свой скрипт) |  |
| `SM_L_Controller` | 1 | Blender (свой скрипт) |  |
| `SM_L_DogBed` | 1 | Blender (свой скрипт) |  |
| `SM_L_DoorFrame` | 1 | Blender (свой скрипт) |  |
| `SM_L_Door_Leaf` | 1 | Blender (свой скрипт) |  |
| `SM_L_DustBunny` | 1 | Blender (свой скрипт) |  |
| `SM_L_FloorLamp` | 1 | Blender (свой скрипт) |  |
| `SM_L_GameConsole` | 1 | Blender (свой скрипт) |  |
| `SM_L_Garland` | 1 | Blender (свой скрипт) |  |
| `SM_L_KnittingBasket` | 1 | Blender (свой скрипт) |  |
| `SM_L_Magazines` | 1 | Blender (свой скрипт) |  |
| `SM_L_Pillow_A` | 1 | Blender (свой скрипт) |  |
| `SM_L_Pillow_B` | 1 | Blender (свой скрипт) |  |
| `SM_L_PottedPlant` | 1 | Blender (свой скрипт) |  |
| `SM_L_PowerStrip` | 1 | Blender (свой скрипт) |  |
| `SM_L_Radiator` | 1 | Blender (свой скрипт) |  |
| `SM_L_Remote` | 1 | Blender (свой скрипт) |  |
| `SM_L_ShoeRack` | 1 | Blender (свой скрипт) |  |
| `SM_L_SideTable` | 1 | Blender (свой скрипт) |  |
| `SM_L_Skirting` | 1 | Blender (свой скрипт) |  |
| `SM_L_SnackBowl` | 1 | Blender (свой скрипт) |  |
| `SM_L_Sock` | 1 | Blender (свой скрипт) |  |
| `SM_L_SodaCan` | 1 | Blender (свой скрипт) |  |
| `SM_L_Sofa` | 1 | Blender (свой скрипт) |  |
| `SM_L_Sofa_BackCushion` | 1 | Blender (свой скрипт) |  |
| `SM_L_Sofa_SeatCushion` | 1 | Blender (свой скрипт) |  |
| `SM_L_TV` | 1 | Blender (свой скрипт) |  |
| `SM_L_TVStand` | 1 | Blender (свой скрипт) |  |
| `SM_L_TVStand_Door` | 1 | Blender (свой скрипт) |  |
| `SM_L_ToyArch` | 1 | Blender (свой скрипт) |  |
| `SM_L_ToyBlock` | 1 | Blender (свой скрипт) |  |
| `SM_L_ToyCar` | 1 | Blender (свой скрипт) |  |
| `SM_L_ToyMouse` | 1 | Blender (свой скрипт) |  |
| `SM_L_ToyTrain` | 1 | Blender (свой скрипт) |  |
| `SM_L_TrainTrack` | 1 | Blender (свой скрипт) |  |
| `SM_L_Vase` | 1 | Blender (свой скрипт) |  |
| `SM_L_YarnBall` | 1 | Blender (свой скрипт) |  |
| `SM_L_YarnBridge` | 1 | Blender (свой скрипт) |  |
| `SM_L_YarnThread` | 1 | Blender (свой скрипт) |  |
| `SM_S_Car` | 1 | Blender (свой скрипт) |  |
| `SM_S_Car_Wheel` | 1 | Blender (свой скрипт) |  |
| `SM_S_CoffeeCup` | 1 | Blender (свой скрипт) |  |
| `SM_S_Drainpipe` | 1 | Blender (свой скрипт) |  |
| `SM_S_Facade` | 1 | Blender (свой скрипт) |  |
| `SM_S_FacadeCorner` | 1 | Blender (свой скрипт) |  |
| `SM_S_Gum` | 1 | Blender (свой скрипт) |  |
| `SM_S_Gutter` | 1 | Blender (свой скрипт) |  |
| `SM_S_Hydrant` | 1 | Blender (свой скрипт) |  |
| `SM_S_IceCream` | 1 | Blender (свой скрипт) |  |
| `SM_S_Kerb` | 1 | Blender (свой скрипт) |  |
| `SM_S_Manhole` | 1 | Blender (свой скрипт) |  |
| `SM_S_PavementSlab_A` | 1 | Blender (свой скрипт) |  |
| `SM_S_PavementSlab_B` | 1 | Blender (свой скрипт) |  |
| `SM_S_Road` | 1 | Blender (свой скрипт) |  |
| `SM_S_RoofEdge` | 1 | Blender (свой скрипт) |  |
| `SM_S_StormDrain` | 1 | Blender (свой скрипт) |  |
| `SM_S_StreetLamp` | 1 | Blender (свой скрипт) |  |
| `SM_Y_AntHill` | 1 | Blender (свой скрипт) |  |
| `SM_Y_AppleCore` | 1 | Blender (свой скрипт) |  |
| `SM_Y_Bin` | 1 | Blender (свой скрипт) |  |
| `SM_Y_Bin_Lid` | 1 | Blender (свой скрипт) |  |
| `SM_Y_Bin_Trash` | 1 | Blender (свой скрипт) |  |
| `SM_Y_DogBall` | 1 | Blender (свой скрипт) |  |
| `SM_Y_DogBone` | 1 | Blender (свой скрипт) |  |
| `SM_Y_DogBowl` | 1 | Blender (свой скрипт) |  |
| `SM_Y_DogPoop` | 1 | Blender (свой скрипт) |  |
| `SM_Y_DogRope` | 1 | Blender (свой скрипт) |  |
| `SM_Y_FenceGateFrame` | 1 | Blender (свой скрипт) |  |
| `SM_Y_FenceGate_Leaf` | 1 | Blender (свой скрипт) |  |
| `SM_Y_FencePanel` | 1 | Blender (свой скрипт) |  |
| `SM_Y_FencePanel_Half` | 1 | Blender (свой скрипт) |  |
| `SM_Y_FencePost` | 1 | Blender (свой скрипт) |  |
| `SM_Y_FlowerBed` | 1 | Blender (свой скрипт) |  |
| `SM_Y_GardenHose` | 1 | Blender (свой скрипт) |  |
| `SM_Y_GrassTuft_A` | 1 | Blender (свой скрипт) |  |
| `SM_Y_GrassTuft_B` | 1 | Blender (свой скрипт) |  |
| `SM_Y_GrassTuft_C` | 1 | Blender (свой скрипт) |  |
| `SM_Y_LawnRobot` | 1 | Blender (свой скрипт) |  |
| `SM_Y_OutdoorTap` | 1 | Blender (свой скрипт) |  |
| `SM_Y_Sprinkler` | 1 | Blender (свой скрипт) |  |
| `SM_Y_SteppingStone_A` | 1 | Blender (свой скрипт) |  |
| `SM_Y_SteppingStone_B` | 1 | Blender (свой скрипт) |  |
| `SM_Y_TrashBag` | 1 | Blender (свой скрипт) |  |
| `SM_Y_Tree` | 1 | Blender (свой скрипт) |  |
| `SM_Y_WateringCan` | 1 | Blender (свой скрипт) |  |
| `SM_Y_WindowSill` | 1 | Blender (свой скрипт) |  |

### Map4/Decals — 12 ассетов (Higgsfield: 12)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `T_M4D_calendar` | 1 | Higgsfield |  |
| `T_M4D_drawing_ants` | 1 | Higgsfield |  |
| `T_M4D_drawing_cake` | 1 | Higgsfield |  |
| `T_M4D_drawing_catdog` | 1 | Higgsfield |  |
| `T_M4D_drawing_family` | 1 | Higgsfield |  |
| `T_M4D_label_biscuits` | 1 | Higgsfield |  |
| `T_M4D_label_cereal` | 1 | Higgsfield |  |
| `T_M4D_label_cola` | 1 | Higgsfield |  |
| `T_M4D_label_condmilk` | 1 | Higgsfield |  |
| `T_M4D_label_lollipop` | 1 | Higgsfield |  |
| `T_M4D_painting_birch` | 1 | Higgsfield |  |
| `T_M4D_photo_family` | 1 | Higgsfield |  |

### Map4/Materials — 9 ассетов (свой код: 9)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `MI_M4_Paint` | 1 | свой код |  |
| `M_M4` | 1 | свой код |  |
| `M_M4_2S` | 1 | свой код |  |
| `M_M4_Food` | 1 | свой код |  |
| `M_M4_FxFlame` | 1 | свой код |  |
| `M_M4_FxSmoke` | 1 | свой код |  |
| `M_M4_UV` | 1 | свой код |  |
| `M_M4_UV2` | 1 | свой код |  |
| `M_M4_UV_Food` | 1 | свой код |  |

### Map4/Props — 117 ассетов (Blender (свой скрипт): 92, Meshy через Higgsfield: 22, Blender + картинка Higgsfield: 3)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `SM_Life_Bird` | 2 | Blender (свой скрипт) |  |
| `SM_Life_BirdWing` | 2 | Blender (свой скрипт) |  |
| `SM_Life_Brick` | 2 | Blender (свой скрипт) |  |
| `SM_Life_CableTie` | 2 | Blender (свой скрипт) |  |
| `SM_Life_ClockHand` | 2 | Blender (свой скрипт) |  |
| `SM_Life_DeadSpider` | 2 | Blender (свой скрипт) |  |
| `SM_Life_Earring` | 2 | Blender (свой скрипт) |  |
| `SM_Life_Gum` | 2 | Blender (свой скрипт) |  |
| `SM_Life_HairPin` | 2 | Blender (свой скрипт) |  |
| `SM_Life_HairTuft` | 2 | Blender (свой скрипт) |  |
| `SM_Life_MusicBox` | 2 | Blender (свой скрипт) |  |
| `SM_Life_MusicStar` | 2 | Blender (свой скрипт) |  |
| `SM_Life_NailClip` | 2 | Blender (свой скрипт) |  |
| `SM_Life_Note` | 2 | Blender (свой скрипт) |  |
| `SM_Life_Pennant` | 2 | Blender (свой скрипт) |  |
| `SM_Life_Quad` | 2 | Blender (свой скрипт) |  |
| `SM_Life_Radio` | 2 | Blender (свой скрипт) |  |
| `SM_Life_Receipt` | 2 | Blender (свой скрипт) |  |
| `SM_Life_Shaving` | 2 | Blender (свой скрипт) |  |
| `SM_Life_Sticker` | 2 | Blender (свой скрипт) |  |
| `SM_Life_Tablet` | 2 | Blender (свой скрипт) |  |
| `SM_Life_Web` | 2 | Blender (свой скрипт) |  |
| `SM_M4R_K_Bin` | 6 | Meshy через Higgsfield |  |
| `SM_M4R_K_Calendar` | 5 | Blender + картинка Higgsfield | геометрия нашего меша + картинка Higgsfield |
| `SM_M4R_K_Chair` | 6 | Meshy через Higgsfield |  |
| `SM_M4R_K_Kettle` | 6 | Meshy через Higgsfield |  |
| `SM_M4R_K_Mop` | 6 | Meshy через Higgsfield |  |
| `SM_M4R_K_PlantPot` | 6 | Meshy через Higgsfield |  |
| `SM_M4R_L_Armchair` | 6 | Meshy через Higgsfield |  |
| `SM_M4R_L_PottedPlant` | 6 | Meshy через Higgsfield |  |
| `SM_M4R_L_ToyCar` | 6 | Meshy через Higgsfield |  |
| `SM_M4R_L_ToyTrain` | 6 | Meshy через Higgsfield |  |
| `SM_M4R_V31_Broom` | 6 | Meshy через Higgsfield |  |
| `SM_M4R_V3_KidDrawing` | 5 | Blender + картинка Higgsfield | геометрия нашего меша + картинка Higgsfield |
| `SM_M4R_V3_PictureFrame` | 5 | Blender + картинка Higgsfield | геометрия нашего меша + картинка Higgsfield |
| `SM_M4_AppleSlice` | 5 | Meshy через Higgsfield |  |
| `SM_M4_Ball` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Bat` | 2 | Blender (свой скрипт) |  |
| `SM_M4_BayLeaf` | 2 | Blender (свой скрипт) |  |
| `SM_M4_BirthdayCake` | 6 | Meshy через Higgsfield |  |
| `SM_M4_BiscuitPack` | 5 | Blender (свой скрипт) |  |
| `SM_M4_BottleCap` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Box` | 2 | Blender (свой скрипт) |  |
| `SM_M4_BoxSharp` | 2 | Blender (свой скрипт) |  |
| `SM_M4_BroomHead` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Button` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Candle` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Caramel` | 5 | Meshy через Higgsfield |  |
| `SM_M4_CarrotSlice` | 2 | Blender (свой скрипт) |  |
| `SM_M4_CerealBox` | 5 | Blender (свой скрипт) |  |
| `SM_M4_Coin` | 2 | Blender (свой скрипт) |  |
| `SM_M4_CompoundEye` | 2 | Blender (свой скрипт) |  |
| `SM_M4_CondensedMilk` | 5 | Blender (свой скрипт) |  |
| `SM_M4_Cone` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Cookie` | 5 | Meshy через Higgsfield |  |
| `SM_M4_Crayon` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Crumb` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Cutlet` | 5 | Meshy через Higgsfield |  |
| `SM_M4_Cyl` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Drain` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Droplet` | 2 | Blender (свой скрипт) |  |
| `SM_M4_DustBunny` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Dustpan` | 2 | Blender (свой скрипт) |  |
| `SM_M4_FanBase` | 2 | Blender (свой скрипт) |  |
| `SM_M4_FanBlades` | 2 | Blender (свой скрипт) |  |
| `SM_M4_FanCage` | 2 | Blender (свой скрипт) |  |
| `SM_M4_FanMotor` | 2 | Blender (свой скрипт) |  |
| `SM_M4_FanPole` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Fang` | 2 | Blender (свой скрипт) |  |
| `SM_M4_FlyBody` | 5 | Meshy через Higgsfield |  |
| `SM_M4_Gem` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Grape` | 2 | Blender (свой скрипт) |  |
| `SM_M4_IceCream` | 5 | Meshy через Higgsfield |  |
| `SM_M4_InsectAbdomen` | 2 | Blender (свой скрипт) |  |
| `SM_M4_InsectHead` | 2 | Blender (свой скрипт) |  |
| `SM_M4_InsectThorax` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Ladle` | 2 | Blender (свой скрипт) |  |
| `SM_M4_LegPair` | 2 | Blender (свой скрипт) |  |
| `SM_M4_LegSeg` | 2 | Blender (свой скрипт) |  |
| `SM_M4_LiquidDisc` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Lollipop` | 5 | Blender (свой скрипт) |  |
| `SM_M4_Macaroni` | 5 | Blender (свой скрипт) |  |
| `SM_M4_Pan` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Pea` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Pebble` | 2 | Blender (свой скрипт) |  |
| `SM_M4_PetBottle` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Pheromone` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Phone` | 2 | Blender (свой скрипт) |  |
| `SM_M4_PipeEnd` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Plane` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Popcorn` | 5 | Meshy через Higgsfield |  |
| `SM_M4_Puff` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Rice` | 2 | Blender (свой скрипт) |  |
| `SM_M4_RobotVacuum` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Sandwich` | 5 | Meshy через Higgsfield |  |
| `SM_M4_Sausage` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Socket` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Speck` | 2 | Blender (свой скрипт) |  |
| `SM_M4_SpiderBody` | 5 | Meshy через Higgsfield |  |
| `SM_M4_SwatterHead` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Teddy` | 6 | Meshy через Higgsfield |  |
| `SM_M4_Thimble` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Thread` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Towel` | 2 | Blender (свой скрипт) |  |
| `SM_M4_TrashBag` | 3 | Blender (свой скрипт) |  |
| `SM_M4_WateringCan` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Wing` | 2 | Blender (свой скрипт) |  |
| `SM_M4_WoodSpoon` | 2 | Blender (свой скрипт) |  |
| `SM_M4_Zefir` | 5 | Meshy через Higgsfield |  |
| `SM_R_BreakerBox` | 2 | Blender (свой скрипт) |  |
| `SM_R_BreakerLever` | 2 | Blender (свой скрипт) |  |
| `SM_R_IceCube` | 2 | Blender (свой скрипт) |  |
| `SM_R_Mentos` | 2 | Blender (свой скрипт) |  |
| `SM_R_MentosRoll` | 2 | Blender (свой скрипт) |  |
| `SM_R_PowerStrip` | 2 | Blender (свой скрипт) |  |
| `SM_R_SlingPouch` | 2 | Blender (свой скрипт) |  |
| `SM_R_Slingshot` | 2 | Blender (свой скрипт) |  |

### Map4/Textures — 48 ассетов (Blender (свой скрипт): 45, Blender + картинка Higgsfield: 3)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `T_M4_Asphalt` | 3 | Blender (свой скрипт) |  |
| `T_M4_Bark` | 3 | Blender (свой скрипт) |  |
| `T_M4_Boucle` | 3 | Blender (свой скрипт) |  |
| `T_M4_BoucleG` | 1 | Blender (свой скрипт) |  |
| `T_M4_Brushed` | 3 | Blender (свой скрипт) |  |
| `T_M4_Brushed2` | 3 | Blender (свой скрипт) |  |
| `T_M4_Cardboard` | 3 | Blender (свой скрипт) |  |
| `T_M4_Carpet` | 3 | Blender (свой скрипт) |  |
| `T_M4_Concrete` | 3 | Blender (свой скрипт) |  |
| `T_M4_Detail` | 1 | Blender (свой скрипт) |  |
| `T_M4_Enamel` | 3 | Blender (свой скрипт) |  |
| `T_M4_FloorTile` | 3 | Blender (свой скрипт) |  |
| `T_M4_Gingham` | 3 | Blender (свой скрипт) |  |
| `T_M4_GinghamBlue` | 3 | Blender (свой скрипт) |  |
| `T_M4_GinghamGreen` | 3 | Blender (свой скрипт) |  |
| `T_M4_Grass` | 3 | Blender (свой скрипт) |  |
| `T_M4_Jersey` | 3 | Blender (свой скрипт) |  |
| `T_M4_JerseyG` | 1 | Blender (свой скрипт) |  |
| `T_M4_Jute` | 3 | Blender (свой скрипт) |  |
| `T_M4_Knit` | 3 | Blender (свой скрипт) |  |
| `T_M4_KnitG` | 1 | Blender (свой скрипт) |  |
| `T_M4_Leather` | 3 | Blender (свой скрипт) |  |
| `T_M4_Linen` | 3 | Blender (свой скрипт) |  |
| `T_M4_LinenG` | 1 | Blender (свой скрипт) |  |
| `T_M4_Marble` | 3 | Blender (свой скрипт) |  |
| `T_M4_Melange` | 3 | Blender (свой скрипт) |  |
| `T_M4_MetalPlate` | 3 | Blender (свой скрипт) |  |
| `T_M4_Oak` | 3 | Blender (свой скрипт) |  |
| `T_M4_PaintWood` | 3 | Blender (свой скрипт) |  |
| `T_M4_Paper` | 3 | Blender (свой скрипт) |  |
| `T_M4_Parquet` | 3 | Blender (свой скрипт) |  |
| `T_M4_Pavement` | 3 | Blender (свой скрипт) |  |
| `T_M4_Picket` | 3 | Blender (свой скрипт) |  |
| `T_M4_Plaster` | 3 | Blender (свой скрипт) |  |
| `T_M4_Plastic` | 3 | Blender (свой скрипт) |  |
| `T_M4_Rubber` | 3 | Blender (свой скрипт) |  |
| `T_M4_RugRose` | 3 | Blender + картинка Higgsfield | цвет — картинка Higgsfield, рельеф и шероховатость — Blender |
| `T_M4_Siding` | 3 | Blender (свой скрипт) |  |
| `T_M4_SofaWool` | 3 | Blender (свой скрипт) |  |
| `T_M4_Soil` | 3 | Blender (свой скрипт) |  |
| `T_M4_Stucco` | 3 | Blender (свой скрипт) |  |
| `T_M4_TableWood` | 3 | Blender (свой скрипт) |  |
| `T_M4_Terry` | 3 | Blender (свой скрипт) |  |
| `T_M4_TileCream` | 3 | Blender (свой скрипт) |  |
| `T_M4_WallpaperK` | 3 | Blender + картинка Higgsfield | цвет — картинка Higgsfield, рельеф и шероховатость — Blender |
| `T_M4_WallpaperL` | 3 | Blender + картинка Higgsfield | цвет — картинка Higgsfield, рельеф и шероховатость — Blender |
| `T_M4_Walnut` | 3 | Blender (свой скрипт) |  |
| `T_M4_Weave` | 1 | Blender (свой скрипт) |  |

### Meshy — 36 ассетов (Meshy: 34, свой код: 2)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `M_MeshyPBR` | 1 | свой код | материал, Tools/import_meshy.py |
| `M_MeshyPBR_Food` | 1 | свой код | материал, Tools/import_meshy.py |
| `SM_Ant_AcidGland` | 6 | Meshy |  |
| `SM_Ant_Antenna_Flagellum` | 6 | Meshy |  |
| `SM_Ant_Antenna_Scape` | 6 | Meshy |  |
| `SM_Ant_Body` | 6 | Meshy |  |
| `SM_Ant_Coxa` | 6 | Meshy |  |
| `SM_Ant_Femur` | 6 | Meshy |  |
| `SM_Ant_Tibia` | 6 | Meshy |  |
| `SM_BugSpray` | 6 | Meshy |  |
| `SM_Cabinet_Base` | 6 | Meshy |  |
| `SM_Cabinet_Door` | 6 | Meshy |  |
| `SM_CerealBox` | 6 | Meshy |  |
| `SM_Drawer` | 6 | Meshy |  |
| `SM_Food_CakePiece` | 6 | Meshy |  |
| `SM_Food_CheeseChunk` | 6 | Meshy |  |
| `SM_Food_Chocolate` | 6 | Meshy |  |
| `SM_Food_Gummy` | 6 | Meshy |  |
| `SM_Food_GummyBearGold` | 6 | Meshy |  |
| `SM_Food_HoneyDrop` | 6 | Meshy |  |
| `SM_Food_SugarCube` | 6 | Meshy |  |
| `SM_Fridge_Body` | 6 | Meshy |  |
| `SM_Fridge_Door` | 6 | Meshy |  |
| `SM_HoneyJar` | 6 | Meshy |  |
| `SM_Human_Dad` | 5 | Meshy |  |
| `SM_JamJar` | 6 | Meshy |  |
| `SM_LightSwitch` | 6 | Meshy |  |
| `SM_MouseTrap` | 6 | Meshy |  |
| `SM_Oven_Body` | 6 | Meshy |  |
| `SM_Oven_Door` | 6 | Meshy |  |
| `SM_RolledNewspaper` | 6 | Meshy |  |
| `SM_Slipper` | 6 | Meshy |  |
| `SM_Toaster_Body` | 6 | Meshy |  |
| `SM_Toaster_Lever` | 6 | Meshy |  |
| `SM_WallVent` | 6 | Meshy |  |
| `SM_Wire_Exposed` | 6 | Meshy |  |

### Mess — 5 ассетов (свой код: 5)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `M_MessBlob` | 1 | свой код |  |
| `M_MessDrop` | 1 | свой код |  |
| `M_MessDust` | 1 | свой код |  |
| `M_MessDustUnlit` | 1 | свой код |  |
| `M_MessStain` | 1 | свой код |  |

### Npc — 83 ассетов (Blender (свой скрипт): 83)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `SM_C_Belly` | 1 | Blender (свой скрипт) |  |
| `SM_C_Chest` | 1 | Blender (свой скрипт) |  |
| `SM_C_Fingers_L` | 1 | Blender (свой скрипт) |  |
| `SM_C_Fingers_R` | 1 | Blender (свой скрипт) |  |
| `SM_C_Foot_L` | 1 | Blender (свой скрипт) |  |
| `SM_C_Foot_R` | 1 | Blender (свой скрипт) |  |
| `SM_C_Forearm_L` | 1 | Blender (свой скрипт) |  |
| `SM_C_Forearm_R` | 1 | Blender (свой скрипт) |  |
| `SM_C_Hand_L` | 1 | Blender (свой скрипт) |  |
| `SM_C_Hand_R` | 1 | Blender (свой скрипт) |  |
| `SM_C_Head` | 1 | Blender (свой скрипт) |  |
| `SM_C_Neck` | 1 | Blender (свой скрипт) |  |
| `SM_C_Pelvis` | 1 | Blender (свой скрипт) |  |
| `SM_C_Shin_L` | 1 | Blender (свой скрипт) |  |
| `SM_C_Shin_R` | 1 | Blender (свой скрипт) |  |
| `SM_C_Thigh_L` | 1 | Blender (свой скрипт) |  |
| `SM_C_Thigh_R` | 1 | Blender (свой скрипт) |  |
| `SM_C_UpperArm_L` | 1 | Blender (свой скрипт) |  |
| `SM_C_UpperArm_R` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_Belly` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_Chest` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_EarL` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_EarR` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_FLL` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_FLP` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_FLU` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_FRL` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_FRP` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_FRU` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_HLL` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_HLP` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_HLU` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_HRL` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_HRP` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_HRU` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_Head` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_Hips` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_Jaw` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_Neck` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_Tail1` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_Tail2` | 1 | Blender (свой скрипт) |  |
| `SM_Cat_Tail3` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_Belly` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_Chest` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_EarL` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_EarR` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_FLL` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_FLP` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_FLU` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_FRL` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_FRP` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_FRU` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_HLL` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_HLP` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_HLU` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_HRL` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_HRP` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_HRU` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_Head` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_Hips` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_Jaw` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_Neck` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_Tail1` | 1 | Blender (свой скрипт) |  |
| `SM_Dog_Tongue` | 1 | Blender (свой скрипт) |  |
| `SM_G_Belly` | 1 | Blender (свой скрипт) |  |
| `SM_G_Chest` | 1 | Blender (свой скрипт) |  |
| `SM_G_Fingers_L` | 1 | Blender (свой скрипт) |  |
| `SM_G_Fingers_R` | 1 | Blender (свой скрипт) |  |
| `SM_G_Foot_L` | 1 | Blender (свой скрипт) |  |
| `SM_G_Foot_R` | 1 | Blender (свой скрипт) |  |
| `SM_G_Forearm_L` | 1 | Blender (свой скрипт) |  |
| `SM_G_Forearm_R` | 1 | Blender (свой скрипт) |  |
| `SM_G_Hand_L` | 1 | Blender (свой скрипт) |  |
| `SM_G_Hand_R` | 1 | Blender (свой скрипт) |  |
| `SM_G_Head` | 1 | Blender (свой скрипт) |  |
| `SM_G_Neck` | 1 | Blender (свой скрипт) |  |
| `SM_G_Pelvis` | 1 | Blender (свой скрипт) |  |
| `SM_G_Shin_L` | 1 | Blender (свой скрипт) |  |
| `SM_G_Shin_R` | 1 | Blender (свой скрипт) |  |
| `SM_G_Thigh_L` | 1 | Blender (свой скрипт) |  |
| `SM_G_Thigh_R` | 1 | Blender (свой скрипт) |  |
| `SM_G_UpperArm_L` | 1 | Blender (свой скрипт) |  |
| `SM_G_UpperArm_R` | 1 | Blender (свой скрипт) |  |

### Sounds — 23 ассетов (свой код: 23)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `S_Ambience` | 1 | свой код | заглушка, синтез кодом |
| `S_Bite` | 1 | свой код | заглушка, синтез кодом |
| `S_Buzz` | 1 | свой код | заглушка, синтез кодом |
| `S_Crit` | 1 | свой код | заглушка, синтез кодом |
| `S_Crunch` | 1 | свой код | заглушка, синтез кодом |
| `S_Deliver` | 1 | свой код | заглушка, синтез кодом |
| `S_Drop` | 1 | свой код | заглушка, синтез кодом |
| `S_Grab` | 1 | свой код | заглушка, синтез кодом |
| `S_Hiss` | 1 | свой код | заглушка, синтез кодом |
| `S_Hit` | 1 | свой код | заглушка, синтез кодом |
| `S_Jump` | 1 | свой код | заглушка, синтез кодом |
| `S_Kill` | 1 | свой код | заглушка, синтез кодом |
| `S_Rage` | 1 | свой код | заглушка, синтез кодом |
| `S_Sizzle` | 1 | свой код | заглушка, синтез кодом |
| `S_Spit` | 1 | свой код | заглушка, синтез кодом |
| `S_Splash` | 1 | свой код | заглушка, синтез кодом |
| `S_Splat` | 1 | свой код | заглушка, синтез кодом |
| `S_Spray` | 1 | свой код | заглушка, синтез кодом |
| `S_Squish` | 1 | свой код | заглушка, синтез кодом |
| `S_Step` | 1 | свой код | заглушка, синтез кодом |
| `S_Stomp` | 1 | свой код | заглушка, синтез кодом |
| `S_Web` | 1 | свой код | заглушка, синтез кодом |
| `S_Zap` | 1 | свой код | заглушка, синтез кодом |

### SoundsReal — 34 ассетов (ElevenLabs: 34)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `S_Ambience` | 1 | ElevenLabs |  |
| `S_ArmyCharge` | 1 | ElevenLabs |  |
| `S_ArmyMarch` | 1 | ElevenLabs |  |
| `S_Bite` | 1 | ElevenLabs |  |
| `S_Buzz` | 1 | ElevenLabs |  |
| `S_Crit` | 1 | ElevenLabs |  |
| `S_Crunch` | 1 | ElevenLabs |  |
| `S_Deliver` | 1 | ElevenLabs |  |
| `S_Drop` | 1 | ElevenLabs |  |
| `S_Faucet` | 1 | ElevenLabs |  |
| `S_FridgeDoor` | 1 | ElevenLabs |  |
| `S_Grab` | 1 | ElevenLabs |  |
| `S_Hiss` | 1 | ElevenLabs |  |
| `S_Hit` | 1 | ElevenLabs |  |
| `S_HumanPanic` | 1 | ElevenLabs |  |
| `S_HumanStep` | 2 | ElevenLabs |  |
| `S_Jump` | 1 | ElevenLabs |  |
| `S_Kill` | 1 | ElevenLabs |  |
| `S_Legendary` | 1 | ElevenLabs |  |
| `S_Pickup_Gear` | 1 | ElevenLabs |  |
| `S_Rage` | 1 | ElevenLabs |  |
| `S_Sizzle` | 1 | ElevenLabs |  |
| `S_SpiderStep` | 1 | ElevenLabs |  |
| `S_Spit` | 1 | ElevenLabs |  |
| `S_Splash` | 1 | ElevenLabs |  |
| `S_Splat` | 1 | ElevenLabs |  |
| `S_Spray` | 1 | ElevenLabs |  |
| `S_Squish` | 1 | ElevenLabs |  |
| `S_Step` | 4 | ElevenLabs |  |
| `S_Stomp` | 1 | ElevenLabs |  |
| `S_Swat` | 1 | ElevenLabs |  |
| `S_Toaster` | 1 | ElevenLabs |  |
| `S_Web` | 1 | ElevenLabs |  |
| `S_Zap` | 1 | ElevenLabs |  |

### Swarm — 2 ассетов (свой код: 2)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `M_SwarmAnt` | 1 | свой код |  |
| `M_SwarmAntArt` | 1 | свой код |  |

### Textures — 14 ассетов (Higgsfield: 13, свой код: 1)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `M_Surface` | 1 | свой код |  |
| `T_Cabinet_Sage` | 1 | Higgsfield |  |
| `T_Ceiling` | 1 | Higgsfield |  |
| `T_Enamel_White` | 1 | Higgsfield |  |
| `T_Fabric_Grey` | 1 | Higgsfield |  |
| `T_Floor_Tile` | 1 | Higgsfield |  |
| `T_Granite` | 1 | Higgsfield |  |
| `T_Parquet_Oak` | 1 | Higgsfield |  |
| `T_Rug_Red` | 1 | Higgsfield |  |
| `T_Soil` | 1 | Higgsfield |  |
| `T_Steel_Brushed` | 1 | Higgsfield |  |
| `T_Subway_Tile` | 1 | Higgsfield |  |
| `T_Wood_Oak` | 1 | Higgsfield |  |
| `T_Wood_Walnut` | 1 | Higgsfield |  |

### UI/Backgrounds — 8 ассетов (Higgsfield: 8)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `T_UI_BgCake` | 1 | Higgsfield |  |
| `T_UI_BgDawn` | 1 | Higgsfield |  |
| `T_UI_BgKitchen` | 1 | Higgsfield |  |
| `T_UI_BgNest` | 1 | Higgsfield |  |
| `T_UI_BgSlipper` | 1 | Higgsfield |  |
| `T_UI_BgWarRoom` | 1 | Higgsfield |  |
| `T_UI_NewsLose` | 1 | Higgsfield |  |
| `T_UI_NewsWin` | 1 | Higgsfield |  |

### UI/Cards — 18 ассетов (Higgsfield: 18)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `T_UI_CardAcid` | 1 | Higgsfield |  |
| `T_UI_CardAlarm` | 1 | Higgsfield |  |
| `T_UI_CardArmor` | 1 | Higgsfield |  |
| `T_UI_CardBait` | 1 | Higgsfield |  |
| `T_UI_CardBite` | 1 | Higgsfield |  |
| `T_UI_CardBrood` | 1 | Higgsfield |  |
| `T_UI_CardCarry` | 1 | Higgsfield |  |
| `T_UI_CardDecoy` | 1 | Higgsfield |  |
| `T_UI_CardGreed` | 1 | Higgsfield |  |
| `T_UI_CardGrip` | 1 | Higgsfield |  |
| `T_UI_CardLever` | 1 | Higgsfield |  |
| `T_UI_CardQuiet` | 1 | Higgsfield |  |
| `T_UI_CardRecruit` | 1 | Higgsfield |  |
| `T_UI_CardSpeed` | 1 | Higgsfield |  |
| `T_UI_CardSquad` | 1 | Higgsfield |  |
| `T_UI_CardTrail` | 1 | Higgsfield |  |
| `T_UI_CardVeteran` | 1 | Higgsfield |  |
| `T_UI_CardWorkers` | 1 | Higgsfield |  |

### UI/Colony — 43 ассетов (Blender (свой скрипт): 40, Higgsfield: 3)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `T_Colony_Bg` | 1 | Higgsfield |  |
| `T_Colony_Bunk` | 1 | Blender (свой скрипт) |  |
| `T_Colony_CakeSlice` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Cocoon` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Crate` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Crown_2` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Crown_3` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Deco` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Dot` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Frame` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Glow` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Jelly` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Larva` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Mushroom` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Pile_Crumbs` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Pile_Fat` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Pile_Protein` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Pile_Sugar` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Plaque` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Queen_Delighted` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Queen_Furious` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Queen_Grumpy` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Queen_Happy` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Queen_Sleeping` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Radio` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Seat_2` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Seat_3` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Shell_A` | 1 | Higgsfield |  |
| `T_Colony_Shell_B` | 1 | Higgsfield |  |
| `T_Colony_Target` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Trophy_Bear` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Trophy_Button` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Trophy_Candle` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Trophy_Cheese` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Trophy_Coin` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Trophy_Earring` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Trophy_Foil` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Trophy_Jaw` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Trophy_Plate` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Trophy_Rose` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Trophy_Sausage` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Trophy_Slipper` | 1 | Blender (свой скрипт) |  |
| `T_Colony_Trophy_Thimble` | 1 | Blender (свой скрипт) |  |

### UI/Fonts — 9 ассетов (ЧУЖОЕ: 9)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `Neucha-Regular.ttf` | 1 | ЧУЖОЕ | OFL 1.1, github.com/google/fonts |
| `OFL-Neucha.txt` | 1 | ЧУЖОЕ | OFL 1.1, github.com/google/fonts |
| `OFL-PTMono.txt` | 1 | ЧУЖОЕ | OFL 1.1, github.com/google/fonts |
| `OFL-PTSansNarrow.txt` | 1 | ЧУЖОЕ | OFL 1.1, github.com/google/fonts |
| `OFL-RussoOne.txt` | 1 | ЧУЖОЕ | OFL 1.1, github.com/google/fonts |
| `PTMono-Regular.ttf` | 1 | ЧУЖОЕ | OFL 1.1, github.com/google/fonts |
| `PTSansNarrow-Bold.ttf` | 1 | ЧУЖОЕ | OFL 1.1, github.com/google/fonts |
| `PTSansNarrow-Regular.ttf` | 1 | ЧУЖОЕ | OFL 1.1, github.com/google/fonts |
| `RussoOne-Regular.ttf` | 1 | ЧУЖОЕ | OFL 1.1, github.com/google/fonts |

### UI/Icons — 80 ассетов (Higgsfield: 80)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `T_UI_Icon_Acid` | 1 | Higgsfield |  |
| `T_UI_Icon_Ambush` | 1 | Higgsfield |  |
| `T_UI_Icon_Apple` | 1 | Higgsfield |  |
| `T_UI_Icon_Arsenal` | 1 | Higgsfield |  |
| `T_UI_Icon_Barrel` | 1 | Higgsfield |  |
| `T_UI_Icon_Bolt` | 1 | Higgsfield |  |
| `T_UI_Icon_Bomb` | 1 | Higgsfield |  |
| `T_UI_Icon_Bread` | 1 | Higgsfield |  |
| `T_UI_Icon_Brood` | 1 | Higgsfield |  |
| `T_UI_Icon_Broom` | 1 | Higgsfield |  |
| `T_UI_Icon_Cake` | 1 | Higgsfield |  |
| `T_UI_Icon_Candy` | 1 | Higgsfield |  |
| `T_UI_Icon_Carriers` | 1 | Higgsfield |  |
| `T_UI_Icon_Carry` | 1 | Higgsfield |  |
| `T_UI_Icon_Cat` | 1 | Higgsfield |  |
| `T_UI_Icon_Cereal` | 1 | Higgsfield |  |
| `T_UI_Icon_Cheese` | 1 | Higgsfield |  |
| `T_UI_Icon_Chocolate` | 1 | Higgsfield |  |
| `T_UI_Icon_Clipboard` | 1 | Higgsfield |  |
| `T_UI_Icon_Compass` | 1 | Higgsfield |  |
| `T_UI_Icon_Controls` | 1 | Higgsfield |  |
| `T_UI_Icon_Cookie` | 1 | Higgsfield |  |
| `T_UI_Icon_Cracker` | 1 | Higgsfield |  |
| `T_UI_Icon_Decree` | 1 | Higgsfield |  |
| `T_UI_Icon_Dice` | 1 | Higgsfield |  |
| `T_UI_Icon_Dog` | 1 | Higgsfield |  |
| `T_UI_Icon_Emblem` | 1 | Higgsfield |  |
| `T_UI_Icon_Engineers` | 1 | Higgsfield |  |
| `T_UI_Icon_Eye` | 1 | Higgsfield |  |
| `T_UI_Icon_FireAnt` | 1 | Higgsfield |  |
| `T_UI_Icon_Fly` | 1 | Higgsfield |  |
| `T_UI_Icon_Forager` | 1 | Higgsfield |  |
| `T_UI_Icon_Friends` | 1 | Higgsfield |  |
| `T_UI_Icon_Gamepad` | 1 | Higgsfield |  |
| `T_UI_Icon_Gear` | 1 | Higgsfield |  |
| `T_UI_Icon_GranSlipper` | 1 | Higgsfield |  |
| `T_UI_Icon_Grape` | 1 | Higgsfield |  |
| `T_UI_Icon_Guard` | 1 | Higgsfield |  |
| `T_UI_Icon_Hourglass` | 1 | Higgsfield |  |
| `T_UI_Icon_Human` | 1 | Higgsfield |  |
| `T_UI_Icon_Jam` | 1 | Higgsfield |  |
| `T_UI_Icon_Jelly` | 1 | Higgsfield |  |
| `T_UI_Icon_KidHand` | 1 | Higgsfield |  |
| `T_UI_Icon_Language` | 1 | Higgsfield |  |
| `T_UI_Icon_Larva` | 1 | Higgsfield |  |
| `T_UI_Icon_Lens` | 1 | Higgsfield |  |
| `T_UI_Icon_Mass` | 1 | Higgsfield |  |
| `T_UI_Icon_Medal` | 1 | Higgsfield |  |
| `T_UI_Icon_Medic` | 1 | Higgsfield |  |
| `T_UI_Icon_NestEntrance` | 1 | Higgsfield |  |
| `T_UI_Icon_Newspaper` | 1 | Higgsfield |  |
| `T_UI_Icon_Nut` | 1 | Higgsfield |  |
| `T_UI_Icon_Phone` | 1 | Higgsfield |  |
| `T_UI_Icon_Pin` | 1 | Higgsfield |  |
| `T_UI_Icon_Popcorn` | 1 | Higgsfield |  |
| `T_UI_Icon_Radio` | 1 | Higgsfield |  |
| `T_UI_Icon_Ready` | 1 | Higgsfield |  |
| `T_UI_Icon_RedHill` | 1 | Higgsfield |  |
| `T_UI_Icon_Scale` | 1 | Higgsfield |  |
| `T_UI_Icon_Scout` | 1 | Higgsfield |  |
| `T_UI_Icon_Scouts` | 1 | Higgsfield |  |
| `T_UI_Icon_Seed` | 1 | Higgsfield |  |
| `T_UI_Icon_Shield` | 1 | Higgsfield |  |
| `T_UI_Icon_Slipper` | 1 | Higgsfield |  |
| `T_UI_Icon_Soldier` | 1 | Higgsfield |  |
| `T_UI_Icon_Spawn` | 1 | Higgsfield |  |
| `T_UI_Icon_Speed` | 1 | Higgsfield |  |
| `T_UI_Icon_Spider` | 1 | Higgsfield |  |
| `T_UI_Icon_Spray` | 1 | Higgsfield |  |
| `T_UI_Icon_Squad` | 1 | Higgsfield |  |
| `T_UI_Icon_Stamp` | 1 | Higgsfield |  |
| `T_UI_Icon_Strength` | 1 | Higgsfield |  |
| `T_UI_Icon_Subtitles` | 1 | Higgsfield |  |
| `T_UI_Icon_Sugar` | 1 | Higgsfield |  |
| `T_UI_Icon_Swatter` | 1 | Higgsfield |  |
| `T_UI_Icon_Tag` | 1 | Higgsfield |  |
| `T_UI_Icon_Timer` | 1 | Higgsfield |  |
| `T_UI_Icon_Trail` | 1 | Higgsfield |  |
| `T_UI_Icon_Worker` | 1 | Higgsfield |  |
| `T_UI_Icon_Wrench` | 1 | Higgsfield |  |

### UI/Loading — 6 ассетов (Higgsfield: 6)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `T_UI_LoadCat` | 1 | Higgsfield |  |
| `T_UI_LoadChain` | 1 | Higgsfield |  |
| `T_UI_LoadDog` | 1 | Higgsfield |  |
| `T_UI_LoadGruzd` | 1 | Higgsfield |  |
| `T_UI_LoadMedic` | 1 | Higgsfield |  |
| `T_UI_LoadSpray` | 1 | Higgsfield |  |

### UI/Logo — 3 ассетов (Higgsfield: 3)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `T_UI_Emblem` | 1 | Higgsfield |  |
| `T_UI_Logo` | 1 | Higgsfield |  |
| `T_UI_Studio` | 1 | Higgsfield |  |

### UI/Materials — 12 ассетов (свой код: 10, Higgsfield: 2)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `T_UI_Cardboard` | 1 | Higgsfield |  |
| `T_UI_Dot` | 1 | свой код |  |
| `T_UI_FadeH` | 1 | свой код |  |
| `T_UI_FadeV` | 1 | свой код |  |
| `T_UI_Grain` | 1 | свой код |  |
| `T_UI_Paper` | 1 | Higgsfield |  |
| `T_UI_Scratches` | 1 | свой код |  |
| `T_UI_StampBox` | 1 | свой код |  |
| `T_UI_StampRing` | 1 | свой код |  |
| `T_UI_Tape` | 1 | свой код |  |
| `T_UI_TornEdge` | 1 | свой код |  |
| `T_UI_Vignette` | 1 | свой код |  |

### UI/Portraits — 7 ассетов (Higgsfield: 7)

| Ассет | Файлов | Источник | Примечание |
|---|---|---|---|
| `T_UI_PorGruzd` | 1 | Higgsfield |  |
| `T_UI_PorKapelka` | 1 | Higgsfield |  |
| `T_UI_PorQueen` | 1 | Higgsfield |  |
| `T_UI_PorTikhon` | 1 | Higgsfield |  |
| `T_UI_PorUsik` | 1 | Higgsfield |  |
| `T_UI_PorVintik` | 1 | Higgsfield |  |
| `T_UI_PorZhvalov` | 1 | Higgsfield |  |
