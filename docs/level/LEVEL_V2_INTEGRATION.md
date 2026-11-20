# Уровень v2 — как внедрять

Для программиста, который будет ставить v2 в игру. Собрано 2026-11-20 12:58. Сейчас сделано только то, что не трогает игру: модели, данные, таблицы, карта. В `Source`, `Config`, `Content` не изменено ничего. Порядок ниже — порядок работы.

## 0. Что где лежит

| Файл | Что это |
|---|---|
| `docs/level/level_v2.json` | вся новая раскладка в схеме `level_data.json`: объекты, проёмы, точки, лут, опасности, укрытия, районы, граф. Поле `v2` у записи: `new` / `moved` / `copy` / `fixed`. Источник правды для всех чисел ниже |
| `Tools/level/level_v2_layout.py` | сама раскладка как данные (что убрано, сдвинуто, добавлено и почему). Править здесь, потом `Tools/level/run_level_v2.ps1` |
| `SourceArt/Blender/level_v2/*.fbx` | 52 новых мешей (25566 треугольников). Скрипты: `Tools/blender/level_v2_pieces.py` и `level_v2_p1..p3.py` |
| `SourceArt/Blender/level_v2/level_v2_new_pieces.json` | манифест новых мешей в формате `kitchen_manifest.json` (размеры, пивоты, слоты, габариты в игре, петли) |
| `SourceArt/Blender/level_v2/level_v2_manifest.json` | единый манифест v2: каждый размещённый меш уровня (старые на новых местах + новые), формат кухонного манифеста |
| `docs/level/KitchenArtData_v2.inl` | готовая таблица размещений для игры (`GKitchenArt`, `GKitchenMovers`) в формате `KitchenArtData.inl`; пишет `Tools/level/gen_level_v2_art.py` |
| `docs/level/level_audit_v2.json`, `level_v2_check.json` | аудит v2 и проверки (Человек проходит, ничего не висит, двери открываются, граф, укрытия) |
| `docs/level/level_map_v2.html` | карта «было / стало» |
| `SourceArt/Blender/level_v2/preview_*.jpg` | рендеры: вид сверху, изометрии комнат и двора, 11 видов с высоты муравья (`Tools/blender/level_v2_preview.py`) |

## 1. Меши

1. Импортировать FBX из `SourceArt/Blender/level_v2` тем же `Tools/import_blender_art.py` (масштаб 19, коллизия complex-as-simple, слоты сохраняются). Папку выбирает имя: `SM_K_*` → `/Game/Kitchen/<Имя>/<Имя>`, всё остальное → `/Game/Living/...` — так их находит `KitchenArt::Mesh` (`KitchenArt.cpp:221`). В импортёр надо добавить набор `level_v2` с этим правилом; обе папки уже в `DirectoriesToAlwaysCook`.
2. Сверить отчёт импорта с `level_v2_new_pieces.json` (габариты, слоты) — как для кухни.
3. Новых слотов материалов нет: все 37 слотов уже есть в таблице `GLooks` (`KitchenArt.cpp`): BinGreen, Biscuit, Blue, Brass, Butcher, Cabinet, CakePink, CarPaint, Cardboard, Cheese, Chrome, Dust, Enamel, Fabric, Honey, Ice, Jam, Leaf, LeafLight, Leather, MugBlue, Paper, PlasticBlue, Plinth, Red, ShelfGlass, Sisal, SockWool, Soil, Steel, Stone, Sugar, Teal, Terracotta, Toast, Walnut, Yarn.

| Меш | Треугольников | Размещений | Слоты | Заметка |
|---|---|---|---|---|
| SM_K_HighChair | 1140 | 1 | Butcher, Plinth, Teal, Enamel | seat top z=1500, tray floor z=1750 (rim 1790); the tray side is the front; four splayed legs, footrest at 600 |
| SM_K_CarTrack | 436 | 1 | Honey, Blue, PlasticBlue | toy car track clamped to the west edge of the dining table: slope 27.9 deg, 148 uu wide between the lips; the ramp heavy loot is dragged down |
| SM_K_Broom | 372 | 1 | Sisal, Red, Butcher | leans on the west end of the worktop: slope 34.0 deg; a way up and down for dragged loot along the west wall |
| SM_K_Dustpan | 264 | 1 | Red | open towards the front: ants walk in; 240 uu high at the handle |
| SM_K_Slipper | 640 | 4 | Leather, SockWool, Yarn | felt slipper, open at the heel: a cave 370 x 480 x 146 inside; 230 high, the Human steps over it |
| SM_K_GroceryBag | 556 | 1 | Cardboard, Sisal, Toast, Leaf | paper bag on its side, mouth at the front (1070 x 590 opening): a cave 1170 deep |
| SM_K_Apple | 352 | 2 | Red, Walnut, Leaf |  |
| SM_K_CardboardBox | 1106 | 1 | Cardboard, Paper | delivery box, flaps open: inside 1664 x 1114 x 1080; holes: front 260 x 182 at the floor, back 260 x 172 at the floor, hand holes 300 x 120 at z 700 on both ends |
| SM_K_Bottle | 384 | 4 | ShelfGlass, Paper | empty bottle on its side, neck towards the front: a tunnel 68 uu wide at the neck, 216 inside |
| SM_K_TinCan | 376 | 2 | Steel, Red | open tin on its side, lid bent up at the front |
| SM_K_BreadBox | 448 | 1 | Butcher | floor top z=30, top z=600; the slanted front is SM_K_BreadBox_Lid |
| SM_K_BreadBox_Lid | 144 | 1 | Butcher, Brass |  |
| SM_K_Bun | 168 | 2 | Toast |  |
| SM_K_KnifeBlock | 352 | 1 | Walnut, Plinth, Brass |  |
| SM_K_DishRack | 888 | 1 | Enamel, Chrome | drip tray + wire rack; three SM_K_Plate stand in it (placed by the layout) |
| SM_K_FruitBowl | 1084 | 1 | MugBlue, Red, Honey, LeafLight, Cheese |  |
| SM_K_FlyPaper | 336 | 1 | Cardboard, Honey, Plinth | sticky fly ribbon from the lamp over the dining table down to 20 uu above the table top; a slow way up to the lamp |
| SM_K_PaperTowel | 244 | 1 | Paper, Chrome | roll under the wall cabinet (holder top at 1250 = cabinet bottom), tail hanging to the worktop |
| SM_K_ChiliString | 1300 | 1 | Sisal, Red, Leaf, Brass | string of dried chilies along the west wall from the wall shelf to the side of the first wall cabinet |
| SM_K_FridgeMagnet | 96 | 6 | Honey, Enamel | origin on the surface it sticks to; front = -Y |
| SM_K_FridgeNote | 56 | 2 | Paper, Red | paper note under a magnet, its lower edge stands 55 uu off the surface: room to hide behind |
| SM_K_Calendar | 180 | 1 | Paper, Red, Enamel, Chrome, Plinth |  |
| SM_K_Sandwich | 348 | 1 | Toast, Leaf, CakePink, Cheese |  |
| SM_K_SugarSpill | 860 | 1 | Sugar | spilled sugar: 6 uu thick, two cubes of 120 |
| SM_K_JamPuddle | 60 | 1 | Jam | sticky: the game needs a sticky zone here (level_v2.json hazards) |
| SM_K_Puddle | 52 | 2 | Ice |  |
| SM_K_FlourSack | 376 | 2 | Paper, Sisal, Blue |  |
| SM_K_PastaBox | 152 | 3 | Blue, Cheese |  |
| SM_K_CookieTin | 532 | 2 | Red, Biscuit | open biscuit tin, lid leaning against it |
| SM_L_ShoeRack | 836 | 1 | Butcher, Walnut | open rack: slatted shelves with tops at 120 / 420 / 780, top board at 1100; ants pass between the slats (45 uu gaps) |
| SM_L_ToyBlock | 156 | 28 | Butcher, Red, Blue, Leaf, Honey | 240 cube: lower than the Human's step, stacks of them are stairs for dropped loot |
| SM_L_ToyArch | 260 | 6 | Blue |  |
| SM_L_ToyCar | 672 | 3 | Plinth, Red, ShelfGlass, Honey, Enamel | toy truck, 240 high, nose at the front; 70 uu of clearance under the chassis |
| SM_L_ToyTrain | 576 | 1 | Plinth, Red, Blue, Leaf, Honey | wooden engine, stands on the track (rail top 40) |
| SM_L_ToyMouse | 276 | 1 | Dust, CakePink, Plinth, Yarn |  |
| SM_L_TrainTrack | 1080 | 1 | Butcher, Walnut | wooden train set, an oval 3600 x 1980 on the carpet: 180 wide, 40 high; a clear lane round the toys |
| SM_L_BookRamp | 108 | 1 | Blue, Paper, Honey, Red | board book leaning on the west edge of the coffee table: slope 29.0 deg, 900 wide; the way down for the main object |
| SM_L_Blanket | 124 | 1 | Yarn, Sisal | throw hanging from the sofa seat to the floor: slope 38.4 deg; a tent underneath |
| SM_L_YarnBridge | 272 | 1 | Yarn | a strand of yarn from the rim of the knitting basket to the coffee table top; ants only: the Human must walk through it |
| SM_L_Garland | 936 | 1 | Sisal, Honey, Blue, Leaf, CakePink, Red, Brass | birthday bunting from the wall above the door to the top of the bookshelf, lowest point about 3780 (the Human is 3400); ants only |
| SM_L_SideTable | 460 | 1 | Walnut | top z=1000, lower shelf z=330 |
| SM_L_CatFlap | 176 | 1 | Enamel | liner of the opening x 7000..7200, y -7600..-7100, z 30..590 in the living room's west wall (the wall needs this hole in the code) |
| SM_L_CatFlap_Leaf | 88 | 1 | ShelfGlass, Enamel |  |
| SM_L_PowerStrip | 644 | 1 | Enamel, Plinth, Red | extension lead by the south wall: one cable climbs the west end of the TV stand to its top (a way up), one runs to the floor lamp |
| SM_Y_AntHill | 840 | 1 | Soil, Stone, Plinth | the red clan's mound: crater on top, three tunnel mouths on the slope; their safe zone has radius 560 |
| SM_Y_FlowerBed | 2124 | 2 | Terracotta, Soil, Leaf, CakePink, Cheese, LeafLight, Honey, Red | brick-edged bed, soil top z=240; cover between the stems |
| SM_Y_Sprinkler | 208 | 1 | PlasticBlue, Brass |  |
| SM_Y_GardenHose | 748 | 1 | Leaf, Brass | garden hose from the wall tap to the sprinkler: 68 uu thick, a dry line across the lawn |
| SM_Y_OutdoorTap | 356 | 1 | Brass, Red | origin on the wall face, on the pipe's axis |
| SM_Y_WateringCan | 328 | 1 | BinGreen | open top: a pool or a hiding place inside; the spout is the front |
| SM_Y_LawnRobot | 568 | 1 | CarPaint, Plinth, Red | mower robot; in the game it should drive like the robot vacuum of event 3 (needs code) |
| SM_S_CoffeeCup | 428 | 1 | Paper, Cardboard, Plinth, Walnut | dropped paper cup on its side with a coffee puddle (sticky zone) and its lid |

## 2. Таблица размещений

1. `python Tools/level/gen_level_v2_art.py` → `docs/level/KitchenArtData_v2.inl`: 528 размещений, из них 345 строк совпадают с нынешней таблицей, остальные — сдвинутые, повторные и новые. Скрипт сам проверяет формат: нынешний `KitchenArtData.inl` он воспроизводит строка в строку.
2. Заменить им `Source/AntKitchen/KitchenArtData.inl`. После этого **не запускать** старый `Tools/gen_kitchen_art.py`: он пересоберёт v1 из двух старых манифестов. Правильнее — научить его читать `level_v2_manifest.json` (там уже есть роль, петля, сдвиг, флаги и подписи каждой подвижной части; словари `MOVERS`, `L_MOVERS`, `tiled()` и `MOVE` становятся не нужны).
3. Индексы экземпляров старых мешей сохранены (гаджеты ищут свои части по имени и индексу: `SM_K_Range_Knob` 2, `SM_K_Range_Burner` 1 и т. д.). У `SM_Y_FencePanel` остались индексы 19–21 и новые с 22: старые 0–18 убраны.
4. Новые подвижные части (роль 1, `AKitchenMover`): `SM_K_BreadBox_Lid` (петля по верхнему краю, −95°, флаг 8 «Человек захлопывает», лёгкая) и `SM_L_CatFlap_Leaf` (петля по верхнему краю, 80° наружу, автозакрытие 15 с, тяжёлая). Класс двери (`AntRun.h:114`) для них надо задать там же, где он задаётся остальным.
5. `SM_K_Plate` в сушилке (три повторных размещения) — статика, не «ездоки».

## 3. Стены, проёмы, края

Стены строит код (`KitchenHouse.cpp`, `KitchenBuilder.cpp`). Размеры комнат не менялись. Нужно:

| Что | Коробка x0, y0, z0 — x1, y1, z1 | Где в коде сейчас |
|---|---|---|
| изменить: Яма под шатающейся плиткой | (4700, 1700, -300) — (5300, 2300, 1) | KitchenHouse.cpp:97 |
| новый проём: Северный лаз: палисадник <-> коридор за шкафами | (-7250, 4770, 0) — (-6950, 4980, 150) | рядом с остальными проёмами: KitchenHouse.cpp:79–143, стены гостиной KitchenBuilder.cpp:1262–1265 |
| новый проём: Мусорный лаз: кухня (за коробкой) <-> гостиная (за лежанкой) | (6950, 880, 0) — (7250, 1080, 160) | рядом с остальными проёмами: KitchenHouse.cpp:79–143, стены гостиной KitchenBuilder.cpp:1262–1265 |
| новый проём: Кошачья дверца: гостиная <-> двор A (засов изнутри) | (6950, -7600, 30) — (7250, -7100, 590) | рядом с остальными проёмами: KitchenHouse.cpp:79–143, стены гостиной KitchenBuilder.cpp:1262–1265 |
| новый проём: Красная щель под окном гостиной: гостиная <-> двор B | (19400, -9250, 0) — (19600, -8950, 120) | рядом с остальными проёмами: KitchenHouse.cpp:79–143, стены гостиной KitchenBuilder.cpp:1262–1265 |
| новый проём: Окно гостиной (проём) | (16785, -9200, 2200) — (19785, -9000, 4200) | рядом с остальными проёмами: KitchenHouse.cpp:79–143, стены гостиной KitchenBuilder.cpp:1262–1265 |
| изменить: Яма под плиткой у мусорки | (4700, 1700, -300) — (5300, 2300, 0) | KitchenHouse.cpp:154. v2: пол ямы опустить до z=-300 (коробка пола -400..-300), стенки -300..0; еда появляется на z=-290 |
| невидимый блок высотой 2600: край карты: тротуар и дорога, север | (-15300, 5600, 0) — (-9400, 5720, 2600) | как коробки забора в KitchenHouse.cpp:308 |
| невидимый блок высотой 2600: край карты: тротуар и дорога, юг | (-15300, -5320, 0) — (-9400, -5200, 2600) | как коробки забора в KitchenHouse.cpp:308 |
| невидимый блок высотой 2600: край карты: за машиной | (-15420, -5200, 0) — (-15300, 5600, 2600) | как коробки забора в KitchenHouse.cpp:308 |

- Заборы двора — меши `SM_Y_FencePanel` / `_Half` / `SM_Y_FencePost` из новой таблицы; две коробки северного забора старого двора (`KitchenHouse.cpp:308`) убрать.
- Газон двора: новые прямоугольники (-9400, -8800)–(7000, -5200); (5200, -12200)–(20400, -9200); (5200, -9200)–(7000, -8800) (было KitchenBuilder.cpp:1806). Улица: (-15300, -5200)–(-7200, 5600).
- Холмик огненных в кухне (коробка 6200..7000 × 650..1850) убрать: вместо него меш `SM_Y_AntHill` во дворе.
- **Плинтусы не раздвинуты.** Секции `SM_K_Skirting` / `SM_L_Skirting` стоят как в v1 и закрывают мусорный лаз, кошачью дверцу, красную щель и северный лаз. Их надо раздвинуть так же, как раздвинуты под старые щели.

## 4. Точки и коробки в коде

Всё, что в `level_v2.json` отличается от `level_data.json`. Колонка «где» — откуда значение взято в v1 (поле `src`).

| Что | Было | Стало | Где в коде |
|---|---|---|---|
| Гнездо огненных муравьёв (соперники, боты) | (6300, 1250, 0) | (12800, -10700, 0) | KitchenBuilder.cpp:69, KitchenBuilder.h:39 |
| Появление огненных | (6020, 1250, 60) | (12500, -10700, 60) | AntKitchenGameMode.cpp:697 |
| Кошка: спит на диване | (13500, 400, 950) | (15500, 400, 950) | AntKitchenGameMode.cpp:301 |
| Кормушка мух: собачья кучка | (-3000, -9500, 0) | (1300, -8300, 0) | KitchenBuilder.h:59 |
| Кормушка мух: уличный бак | (4200, -13000, 2650) | (18600, -11000, 2650) | KitchenBuilder.h:60 |
| новое: Вынос 1: щель под плинтусом (гнездо) | — | (-7250, -2500, 0) | level_v2_layout.py |
| новое: Вынос 2: южный вход гнезда у угла дома (со стороны двора) | — | (-8150, -5750, 0) (нужен код) | level_v2_layout.py |
| Человек: коробка блуждания, кухня | [-6000, -4300, 6200, 2550] | [-6000, -4300, 5000, 2550] | KitchenBuilder.cpp:84 |
| Человек: коробка блуждания, гостиная | [8000, -8300, 19000, -500] | [8000, -7000, 19000, -600] | KitchenBuilder.cpp:95 |
| Человек: путевые точки двери | ['(6500, -4000, 0)', '(7700, -4000, 0)'] | без изменений (ботинок с них убран) | KitchenBuilder.cpp:232 |
| Человек: границы выбора цели | любая точка в 8000 uu, в том числе двор и улица | только [[-7000, -5000, 7000, 5600], [7200, -9000, 20200, 1500]] | HumanAIController.cpp:40 (PickTarget); пути к щелям KitchenBuilder.cpp:156, 162 больше не нужны |
| Человек: остановки патруля (новое, по желанию) | — | (3800, -3200) (-3600, -2400) (-4700, 1900) (-2200, 1900) (3300, 2300) (4200, -2000) (6400, -3900) (7800, -4000) (9600, -2600) (11200, -5800) (13000, -2100) (17800, -5200) (13400, -6900) | HumanAIController.cpp:420 |
| Бадрик: точка «the shoe by the door» | (6300, -4000, 0) | (8500, -500, 0) — нюхает обувь у обувницы | Critters.cpp:79 |
| Бадрик: точка «asleep on the living-room rug» | (14000, -5500, 0) | (18150, -7900, 0) — спит на своей лежанке у батареи | Critters.cpp:80 |
| Бадрик: точка «sitting by the sofa, begging» | (12000, -1200, 0) | (16700, -1500, 0) — сидит у дивана, клянчит | Critters.cpp:81 |
| Кошка: точка «asleep on the sofa» | (13500, 400, 950) | (15500, 400, 950) — спит на восточной подушке дивана | Critters.cpp:88 |
| Кошка: точка «on grandma's lap... the armchair» | (9250, -5800, 800) | (8000, -8300, 1311) — на когтеточке | Critters.cpp:89 |
| Кошка: точка «the living-room floor» | (11000, -3000, 0) | (12600, -1700, 0) — на полу гостиной у кубиков | Critters.cpp:91 |
| зона: Батарея | коробка (7700, 1150, 9600, 1500) | коробка (17345, -9000, 19245, -8650) | KitchenBuilder.cpp:1396 |
| зона: Гнездо огненных | круг (6300, 1250, 560) | круг (12800, -10700, 560) | KitchenBuilder.h:39 |
| новая зона: Липко: лужа варенья у детского стульчика (sticky) | — | коробка (3100, -1800, 3800, -1200), z (-10, 40) | таблицы зон: липкое KitchenBuilder.cpp:1405, мокрое :1422 |
| новая зона: Липко: лента от мух над столом (sticky) | — | круг (500, -500, 90), z (1420, 3500) | таблицы зон: липкое KitchenBuilder.cpp:1405, мокрое :1422 |
| новая зона: Мокро: лужа под сифоном (wet) | — | коробка (-3650, 3400, -3050, 3900), z (130, 180) | таблицы зон: липкое KitchenBuilder.cpp:1405, мокрое :1422 |
| новая зона: Мокро: лужица у мойки (wet) | — | круг (-4330, 3500, 260), z (1700, 1740) | таблицы зон: липкое KitchenBuilder.cpp:1405, мокрое :1422 |
| новая зона: Липко: пролитый кофе (sticky) | — | коробка (-10650, 450, -9950, 950), z (0, 40) | таблицы зон: липкое KitchenBuilder.cpp:1405, мокрое :1422 |
| новая зона: Поливалка: вода по кругу (water) | — | круг (-1500, -7300, 1400), z (0, 400); нужен код | таблицы зон: липкое KitchenBuilder.cpp:1405, мокрое :1422 |
| новая зона: Робот-газонокосилка (robot) | — | коробка (-9200, -8700, 4700, -7800), z (0, 330); нужен код | таблицы зон: липкое KitchenBuilder.cpp:1405, мокрое :1422 |
| новая зона: Земля огненных (rival) | — | коробка (5200, -12200, 20400, -9200), z (0, 400) | таблицы зон: липкое KitchenBuilder.cpp:1405, мокрое :1422 |
| контейнер: в тумбе под ТВ | (11740, -8950, 100) — (14345, -7750, 1000) | (11140, -8950, 100) — (13745, -7750, 1000) | AntRun.cpp:156 |
| новый контейнер: в хлебнице | — | (-6840, 3040, 1740) — (-5700, 3660, 2280), сила 6 | таблица контейнеров AntRun.cpp:141 |
| Приставка на тумбе: включает телевизор | (14600, -8400, 1160) | (14000, -8400, 1160) | AntRun.cpp:627 |
| Кружка (столкнуть) | (-5600, 3170, 1820) | (-4700, 3170, 1820) | AntRun.cpp:666 |
| новое: Кошачья дверца | — | (7100, -7350, 590); тяжёлая (40, около трёх фуражиров); сама закрывается через 15 с. Засов: тянуть можно только из гостиной — нужен код; без него просто тяжёлая с обеих сторон | — |
| новое: Крышка хлебницы | — | (-6300, 3020, 2180); лёгкая (один муравей): откидывается вверх; Человек её закрывает | — |
| Событие «кошка проснулась»: откуда прыгает | (13500, 400, 800) | (15500, 400, 800) | Critters.cpp:252 |

Точки отдыха питомцев целиком — `pets` в `level_v2.json` (в v1: `Critters.cpp:76–93`). Ноги малыша и тапки бабушки не двигались.

## 5. Лут

| Таблица | Было | Стало | Где в коде |
|---|---|---|---|
| Точки еды (`loot.spots`) | 178 | 327: убраны сломанные и весь старый двор, добавлено 165 новых, уровни пересчитаны по правилу градиента (поле `tier_v1` — что было) | KitchenBuilder.cpp LootSpots (+ таблица Moved), россыпь крошек :295 |
| Тяжёлые призы (`loot.heavy`) | 6 | 6, все новые: (3850, -600, 0); (6000, 2950, 30); (6250, 1250, 30); (11540, -2040, 0); (15300, -5600, 0); (16300, -11300, 0) | KitchenBuilder.cpp:74 |
| Трофеи (`loot.trophies`) | 13 | 13: №6 с полки над гнездом на книжный шкаф (19700, -4500, 4740); №11 сдвинут с тумбой ТВ | AntKitchenGameMode.cpp:1681 |
| Еда за дверями (`loot.behind_doors`) | 19 | 19, уровни понижены по градиенту у 3 | AntRun.cpp:688, 698 |
| Особое (`loot.special`) | 5 | груз в яме на z=-290; заначка в ящике ур. 1, 2, 1; правило тостера | AntKitchenGameMode.cpp:306; KitchenInteractables.cpp:1089 |
| Главный объект (`loot.main`) | — | новое: 3 места, вес 70, ценность 600 | нужен код режима вылазки |

Нижняя корзина посудомойки: точки (300, 3600, 420) и (900, 4200, 420) по-прежнему над проволокой — поднять на z=540 (на тарелки) или дать корзине дно.

## 6. Таблица укрытий (обязательно)

`HumanAIController::PickTarget` (строки 52–58) пропускает муравьёв в четырёх зашитых коробках и в закрытых контейнерах. В v2 этот список — данные: `human.sees.never_targets`, записей: 53. Первые 5 — то, что в коде уже есть (четыре коробки и правило контейнеров); остальные, в таблице ниже, новые. Желательно тем же списком глушить топот, ладонь и газету (сейчас они бьют сквозь всё): тогда укрытие честное и против Человека-игрока.

| Укрытие | x0, y0 — x1, y1 | z |
|---|---|---|
| в коробке хлопьев | (-5800, 376) — (-4420, 1424) | 0..410 |
| в ботинке | (7830, 200) — (8270, 1200) | 0..388 |
| в носке | (8373, -2560) — (9627, -1440) | 0..180 |
| в спичечном коробке | (-2200, 2392) — (-1700, 2808) | 0..168 |
| под газетой | (654, -4951) — (1796, -3699) | 0..352 |
| в носке на кухне | (-3827, -2760) — (-2573, -1640) | 0..180 |
| в ботинке в углу кухни | (-6670, -4950) — (-6230, -3950) | 0..388 |
| в совке | (-6611, -1090) — (-5896, -291) | 0..236 |
| в тапке | (4950, -5065) — (5650, -4035) | 0..245 |
| в тапке | (1785, -3250) — (2815, -2550) | 0..245 |
| в тапке | (8189, -261) — (9011, 761) | 0..245 |
| в тапке | (10321, -1152) — (10879, -148) | 0..245 |
| в пакете с продуктами | (5369, 2350) — (6707, 3450) | 0..650 |
| в картонной коробке | (5559, 190) — (6941, 2116) | 0..1609 |
| в бутылке | (3921, 114) — (4679, 1186) | -2..242 |
| в бутылке | (4238, 736) — (5362, 1164) | -2..242 |
| в бутылке | (5692, -588) — (6808, -252) | -2..242 |
| в бутылке | (2080, 1250) — (2320, 2350) | -2..242 |
| в консервной банке | (4114, -1415) — (4685, -944) | -3..244 |
| в консервной банке | (4002, -432) — (4523, 122) | -3..244 |
| в хлебнице | (-6870, 3000) — (-5670, 3700) | 1710..2310 |
| в жестянке с печеньем | (-6600, 3094) — (-5699, 3806) | 44..735 |
| в жестянке с печеньем | (1800, 3994) — (2701, 4706) | 44..735 |
| в обувнице | (7200, -1550) — (7800, 150) | 0..1100 |
| под аркой из кубика | (11240, -2420) — (11840, -2180) | 240..480 |
| под аркой из кубика | (-73, 1285) — (573, 1715) | 0..240 |
| под аркой из кубика | (-3320, 696) — (-2680, 1204) | 0..240 |
| под аркой из кубика | (17179, -6394) — (17821, -6006) | 0..240 |
| под аркой из кубика | (17346, -2620) — (17854, -1980) | 0..240 |
| под аркой из кубика | (14177, -6515) — (14823, -6085) | 0..240 |
| под грузовичком | (11325, -6294) — (12076, -5507) | 0..240 |
| под грузовичком | (-5694, -501) — (-5005, -99) | 0..240 |
| под грузовичком | (17453, -4599) — (18146, -3802) | 0..240 |
| в паровозике | (15050, -4827) — (15550, -4573) | 44..370 |
| за западной шторой кухни | (-2030, -5000) — (-1470, -4660) | 0..2100 |
| за восточной шторой кухни | (1470, -5000) — (2030, -4660) | 0..2100 |
| за западной шторой гостиной | (16420, -9000) — (16980, -8660) | 0..2100 |
| за восточной шторой гостиной | (19590, -9000) — (20150, -8660) | 0..2100 |
| в корзине с вязанием | (9890, -4770) — (10610, -4130) | 0..760 |
| под книжкой-картонкой | (15000, -3850) — (15900, -2950) | 0..470 |
| под пледом у дивана | (14700, -1200) — (16100, -300) | 0..520 |
| под креслом бабушки (щель 114 uu) | (8300, -6900) — (10000, -4700) | 0..114 |
| в крепости из кубиков | (11180, -2420) — (11900, -1920) | 0..240 |
| под открытой дверью в гостиную (щель 23 uu) | (7163, -3270) — (7841, -1654) | 0..30 |
| за батареей гостиной | (17400, -9000) — (19170, -8800) | 0..300 |
| за книгами на нижней полке книжного шкафа | (19230, -7080) — (20190, -1920) | 0..900 |
| за горшком с растением | (18400, 1300) — (19400, 1500) | 0..300 |
| под нижней полкой журнального стола | (12876, -3916) — (13923, -3083) | 0..330 |

## 7. Остальной код из списка «нужен код»

- **Обязательно.** Таблица укрытий для ИИ Человека: PickTarget читает human.sees.never_targets (сейчас четыре коробки зашиты в код). Без неё новые укрытия-предметы не работают. (малый)
- **Обязательно.** Человек берёт цель только внутри дома (human.targeting_bounds). (малый)
- **Обязательно.** Главный объект рейда: один на рейд, место — случайное из loot.main.spots; за ним идут и огненные. (средний (это сам режим вылазки))
- **Обязательно.** Режим коллизии «только муравьи» для нитки, гирлянды, вязанки и липкой ленты (objects[].collision = ants_only): Человек проходит сквозь. (малый)
- Тостер: тост выпадает, только если на столешнице меньше двух; после выстрела 40 с без еды. (малый)
- Кошачья дверца открывается только из гостиной (флаг «засов изнутри» у AKitchenMover). Без него — просто тяжёлая дверь с обеих сторон. (малый)
- Второй круг сдачи у угла дома (extraction[1]). Без него груз из двора несут до щели гнезда: +3000 uu. (малый)
- Поливалка: зона воды по таймеру (эффект струи крана). (малый)
- Робот-газонокосилка: робот-пылесос события 3, постоянно, в коробке двора. (малый)

Коллизия «только муравьи» нужна мешам: SM_K_ChiliString, SM_K_FlyPaper, SM_L_Garland, SM_L_YarnBridge. Из них Человеку на пути стоит только `SM_L_YarnBridge` (нитка от корзины к журнальному столу, высота 760–990); остальные над столом, у стены или выше его роста.

## 8. Что проверить в игре

- [ ] Человек проходит дверь, обходит журнальный стол с севера (коридор 930 uu между пледом и столом) и доходит до тумбы ТВ; не застревает на трассе у стола, метле, книжке, пледе.
- [ ] Предметы ниже его шага (тапки 245, кубики 240, грузовик 240, бутылки 244, совок 237) он перешагивает, а не упирается.
- [ ] Муравей пролезает: северный лаз, мусорный лаз за коробкой, кошачья дверца, красная щель, дыры коробки (260 × 180), горловина бутылки (68 uu), под открытой дверью (23 uu).
- [ ] Груз, который тащат втроём, идёт по метле (35°), трассе (28°), книжке (29°), пледу (38°) и не срывается; по лесенкам из кубиков и ящиков падает со ступени на ступень и не бьётся (ступени 240 и до 580).
- [ ] Главный объект проходит все три пути: стол → кубики → дверь → щель; тумба → кошачья дверца → двор → угол дома; ящик холодильника → пол → щель.
- [ ] Ничего не висит и не пересекается: хлебница, сушилка с тарелками, магниты, вязанка перцев, гирлянда (низ ~3780 при росте Человека 3400), липкая лента над тортом.
- [ ] Дверцы и ящики открываются: шкаф у тостера (рядом метла), ящики (лесенка), холодильник (рядом пакет и яблоко), тумба ТВ (кубики у восточного торца), дверь в гостиную (за ней обувница).
- [ ] Питомцы: Бадрик спит на лежанке у батареи и не лежит на мяче; кошка не сидит в малыше и в бабушке.
- [ ] Края: с улицы и из двора нельзя упасть за карту.
- [ ] Огненные выходят из красного холма, заходят в гостиную через красную щель и в кухню через щель под окном.
- [ ] Еда не появляется в предметах: `-AntArtProbe` на новой таблице точек.

## 9. Чего в данных нет

- Плинтусы под новые лазы (см. раздел 3).
- Северный забор палисадника и заборы двора поставлены секциями по 2200 и 1100: на стыках у столбов возможны щели до 300 uu, закрыть столбом или блоком.
- Поливалка и робот-газонокосилка — только меши и зоны в `hazards`; поведения нет.
- Проверки в движке: всё посчитано по габаритам.
