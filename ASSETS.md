# Ant Kitchen: ассеты для Meshy AI

Список того, что сгенерить. Промпты на английском, вставляй как есть.

## Как генерить, чтобы всё смотрелось одной игрой

1. **Сначала сделай один эталон стиля**, например кубик сахара или кружку. Когда понравится, генерируй остальное через **Image to 3D** с этим скрином как референсом или держи одинаковый хвост промпта (ниже). Иначе каждая модель выйдет в своём стиле.
2. **Хвост стиля**, добавляй к каждому промпту:
   `, stylized realistic, chunky readable shapes, slightly exaggerated proportions, clean PBR materials, soft worn edges, game-ready prop, single object, centered, no base, no background`
3. Настройки: **PBR включён**, Art style **Realistic** (или Sculpture, если хочешь мультяшнее), **Target polycount**: мелкий лут 3–6k, пропы 8–20k, большая мебель 20–40k. Экспорт **FBX** (или GLB), текстуры 2K.
4. **Одна модель — один объект.** Не проси «стол с тортом», генерируй отдельно.
5. Модели, которые открываются (холодильник, духовка, ящик, тостер), генерируй **по частям**: корпус отдельно, дверь отдельно. Иначе дверь не будет открываться.
6. Имена файлов бери из колонки «Файл», я по ним подхвачу модели в коде.

**Масштаб:** человек в игре ростом около 3400 юнитов, муравей длиной около 80. Колонка «Размер в игре» даёт длину × ширину × высоту в юнитах UE (см). Скейл на импорте выставлю сам, просто держи пропорции объекта правильными.

---

## P0 — нужно в первую очередь

### Муравей (игрок)
Тело утверждено (глянцевый тёмный хитин с медными прожилками — это эталон стиля для всех частей муравья).
Лапы и антенны процедурные: код сам сгибает их и ставит на стену/потолок. Поэтому их генерим **отдельными
прямыми сегментами** — один меш на кость. Каждый сегмент: лежит горизонтально, вдоль одной оси, без изгиба.

Хвост для всех частей муравья:
`, glossy dark brown-black chitin with thin copper cracks, same style as the ant body reference, single isolated part, lying horizontally, straight, centered, no base, no background`

| Файл | Промпт | Размер в игре |
|---|---|---|
| `SM_Ant_Body` ✅ | (готово) ant body without legs and antennae, head with mandibles, thorax, petiole, abdomen. **Перегенерь с добавкой:** `three pairs of small round leg sockets on the underside of the thorax` — чтобы лапам было куда крепиться | 85 × 30 × 30 |
| `SM_Ant_Coxa` | `ant hip joint, short thick rounded chitin segment like a small barrel` | 8 × 8 × 8 |
| `SM_Ant_Femur` | `ant upper leg segment, straight thick chitin rod, slightly flattened, thicker in the middle, ball joint knob at one end` | 25 × 7 × 7 |
| `SM_Ant_Tibia` | `ant lower leg segment, straight thin chitin rod tapering toward the end, ending with a tiny segmented foot and two small hook claws, few fine spines along it` | 32 × 5 × 5 |
| `SM_Ant_Antenna_Scape` | `ant antenna base segment, long thin straight chitin rod` | 16 × 3 × 3 |
| `SM_Ant_Antenna_Flagellum` | `ant antenna tip, straight segmented beaded feeler, 10 small bead segments, slightly thicker club at the end` | 22 × 3 × 3 |
| `SM_Ant_AcidGland` | `small glowing translucent acid sac, organic bulb, emissive green liquid inside` | 15 × 15 × 15 |

Касты на потом (то же тело, другой силуэт):
| `SM_Ant_Soldier_Body` | `soldier ant body without legs and antennae, oversized armored head with huge crossing mandibles` |
|---|---|
| `SM_Ant_Queen_Body` | `ant queen body without legs and antennae, huge long abdomen, wing stubs on thorax` |
| `SM_Ant_Scout_Body` | `slim winged ant body without legs and antennae, two pairs of translucent veined wings folded on the back` |

### Человек (враг / босс)
Генерируй в **A-pose** и прогони через **Meshy Auto-Rig** (humanoid). Анимации ходьбы и топота возьмём из Meshy Animation Library или Mixamo.

| Файл | Промпт | Размер в игре |
|---|---|---|
| `SK_Human_Dad` | `tired middle-aged man in stretched t-shirt, sweatpants and fluffy house slippers, messy hair, slight belly, A-pose, full body, character for game` | рост 3400 |
| `SM_Slipper` | `fluffy house slipper, worn, slightly flattened sole` | 520 × 220 × 160 |
| `SM_BugSpray` | `aerosol bug spray can, bright warning label, red nozzle cap` | 120 × 120 × 420 |
| `SM_RolledNewspaper` | `rolled up newspaper used as a weapon, slightly crumpled` | 700 × 90 × 90 |

### Лут — еда (то, что муравьи тащат)
| Файл | Промпт | Размер в игре |
|---|---|---|
| `SM_Food_BreadCrumb` | `small bread crumb, porous crust and soft inside` | 30 |
| `SM_Food_CookieCrumb` | `broken chocolate chip cookie crumb` | 30 |
| `SM_Food_RiceGrain` | `single grain of white rice` | 40 × 15 × 15 |
| `SM_Food_SugarCube` | `sugar cube, crystalline sparkling grains, slightly chipped corner` | 60 |
| `SM_Food_CheeseChunk` | `chunk of yellow swiss cheese with holes` | 80 × 60 × 40 |
| `SM_Food_CatKibble` | `single piece of dry cat food kibble, fish shape` | 50 |
| `SM_Food_Cereal` | `single corn flake cereal, golden, curled` | 60 × 50 × 10 |
| `SM_Food_Gummy` | `glossy translucent gummy candy worm` | 70 |
| `SM_Food_HoneyDrop` | `drop of thick golden honey, glossy, translucent` | 60 |
| `SM_Food_CakePiece` | `small piece of pink layered birthday cake with cream` | 100 |
| `SM_Food_Chip` | `single potato chip, wavy, salted` | 100 × 80 × 10 |
| `SM_Food_GummyBearGold` | `golden gummy bear, shiny, legendary treasure glow` | 110 |
| `SM_Food_Caramel` | `wrapped caramel candy in red shiny wrapper` | 90 |
| `SM_Food_Chocolate` | `piece of dark chocolate bar, two squares, glossy` | 120 × 80 × 25 |
| `SM_Food_Jam` | `blob of strawberry jam, glossy, with seeds` | 70 |
| `SM_Food_Pea` | `single green pea` | 40 |

### Лут — снаряжение (кислотные железы, они же «оружие»)
Пять типов оружия. Один меш на тип, а редкость я покрашу цветом.

| Файл | Промпт |
|---|---|
| `SM_Gear_Spit` | `organic insect acid gland weapon, small bulb with nozzle, bio-tech` |
| `SM_Gear_Shotgun` | `organic insect gland weapon with six short nozzles in a cluster, bio-tech` |
| `SM_Gear_Minigun` | `organic insect gland weapon with rotating ring of small nozzles, bio-tech` |
| `SM_Gear_Mortar` | `big bulbous organic insect gland weapon, wide mortar nozzle, glowing poison` |
| `SM_Gear_Bouncer` | `organic insect gland weapon with springy rubbery sac, bio-tech` |
| `SM_Loot_Pouch` | `small sack made of a leaf tied with a thread, full of crumbs` (выпавший при смерти груз) |

---

## P1 — кухня и интерактив

### Мебель и техника (интерактивная — по частям!)
| Файл | Промпт | Размер в игре |
|---|---|---|
| `SM_Fridge_Body` | `retro white fridge body without doors, open front showing empty interior cavity, rounded corners` | 2000 × 1700 × 3700 |
| `SM_Fridge_Door` | `retro white fridge door, inner side with shelves for bottles and eggs, chrome handle` | 2000 × 150 × 2500 |
| `SM_Fridge_Shelf` | `wire fridge shelf rack, chrome` | 1900 × 1500 × 40 |
| `SM_Oven_Body` | `built-in kitchen oven cavity without door, dark enamel interior, heating elements` | 1500 × 1700 × 1500 |
| `SM_Oven_Door` | `oven door with glass window and steel handle` | 1500 × 80 × 1300 |
| `SM_Stove_Top` | `four burner electric stove top, black glass, coil burners` | 1500 × 2000 × 40 |
| `SM_Cabinet_Base` | `kitchen base cabinet, mint green painted wood, with countertop, simple door` | 1300 × 1750 × 1700 |
| `SM_Cabinet_Door` | `mint green kitchen cabinet door with chrome handle` | 650 × 20 × 1450 |
| `SM_Drawer` | `kitchen drawer pulled out, mint green front, empty inside` | 1250 × 1500 × 300 |
| `SM_Sink_Basin` | `stainless steel kitchen sink basin with drain hole` | 1400 × 1400 × 550 |
| `SM_Faucet` | `chrome kitchen faucet, tall spout` | 150 × 500 × 530 |
| `SM_Faucet_Valve` | `chrome faucet valve knob, cross handle, red dot` | 100 × 100 × 120 |
| `SM_Toaster_Body` | `retro red toaster with two slots` | 700 × 600 × 440 |
| `SM_Toaster_Lever` | `toaster lever knob, chrome` | 80 × 40 × 40 |
| `SM_Microwave` | `old microwave oven, steel, dark glass door` | 1100 × 900 × 640 |
| `SM_Table` | `wooden dining table, rustic` | 4400 × 2600 × 1400 |
| `SM_Chair` | `wooden kitchen chair` | 900 × 900 × 2100 |
| `SM_TrashCan` | `tall steel pedal trash can, slightly dented` | Ø1300 × 1500 |
| `SM_TrashCan_Lid` | `steel trash can lid` | Ø1350 × 50 |
| `SM_LightSwitch` | `wall light switch plate, old plastic, single toggle` | 150 × 30 × 220 |
| `SM_WallVent` | `metal wall air vent grille, slightly rusty` | 400 × 40 × 250 |
| `SM_Wire_Exposed` | `frayed electrical cable with exposed copper wires, sparking end` | 800 × 30 × 30 |
| `SM_Outlet` | `wall power outlet, old, slightly burnt` | 120 × 30 × 180 |
| `SM_MouseTrap` | `wooden spring mouse trap, cocked, with cheese bait` | 380 × 180 × 80 |
| `SM_DrainPipe` | `p-trap sink drain pipe, white pvc, under sink` | 500 × 200 × 600 |
| `SM_Baseboard_Hole` | `small arched mouse hole in a wooden baseboard` | 200 × 30 × 150 |

### Пропы на кухне
| Файл | Промпт | Размер в игре |
|---|---|---|
| `SM_CerealBox` | `cereal box lying on its side, open end, colorful generic cartoon design, no real brand` | 1200 × 800 × 420 |
| `SM_HoneyJar` | `glass honey jar tipped over, dripping honey` | Ø340 × 380 |
| `SM_JamJar` | `glass jar of strawberry jam, tipped over, spilled` | Ø300 × 350 |
| `SM_SugarJar` | `ceramic sugar jar with lid` | Ø500 × 500 |
| `SM_Mug` | `orange ceramic coffee mug` | Ø360 × 300 |
| `SM_Cake` | `round pink birthday cake with white cream and a cherry` | Ø1000 × 500 |
| `SM_Plate` | `white ceramic dinner plate` | Ø900 × 30 |
| `SM_CuttingBoard` | `wooden cutting board with knife marks` | 1100 × 700 × 50 |
| `SM_Knife` | `kitchen chef knife` | 650 × 80 × 20 |
| `SM_SaltShaker` / `SM_PepperShaker` | `glass salt shaker with steel cap` / `black pepper shaker` | Ø160 × 260 |
| `SM_CatBowl_Food` / `SM_CatBowl_Water` | `orange plastic cat food bowl full of kibble` / `blue cat water bowl` | Ø960 × 170 |
| `SM_MopBucket` | `blue plastic mop bucket with water` | Ø840 × 650 |
| `SM_Mop` | `wooden mop` | 60 × 60 × 4300 |
| `SM_Sponge` | `yellow green kitchen sponge, worn` | 300 × 200 × 120 |
| `SM_BottleCap` | `metal bottle cap, crumpled` | Ø80 |
| `SM_Coin` | `old dull coin` | Ø60 |
| `SM_DustBunny` | `ball of dust and hair, dust bunny` | 150 |
| `SM_BananaPeel` | `banana peel, browning` | 400 × 200 × 80 |
| `SM_AppleCore` | `apple core, bitten` | 140 × 140 × 220 |
| `SM_PizzaCrust` | `piece of pizza crust` | 400 × 200 × 40 |
| `SM_TrashBag` | `black trash bag, tied, bulging` | 1000 × 800 × 600 |
| `SM_Grape` | `single green grape` | 80 |
| `SM_Thimble` | `metal sewing thimble` | Ø60 × 70 |

---

## P2 — потом (новые карты и боссы)

| Файл | Промпт |
|---|---|
| `SK_Cat` | `chubby orange tabby cat, sitting, game character` (босс/событие «пришёл кот») |
| `SK_Human_Grandma` | `small old grandma in a robe with a fly swatter, A-pose` (босс другой карты) |
| `SM_RobotVacuum` | `round robot vacuum cleaner` (патруль на карте «гостиная») |
| `SK_Spider` | `big hairy house spider` (мини-босс в вентиляции) |

## Текстуры / декали (Meshy Texture или любой генератор)
- `T_Tiles_Checker` — бело-красная кухонная плитка, швы, лёгкая грязь
- `T_Wall_Paint` — кремовая краска, потёртости
- `T_Decal_Honey`, `T_Decal_Jam`, `T_Decal_Water` — лужи для пола
- `T_Decal_Crumbs` — россыпь крошек

---
Как сгенеришь пачку, кидай FBX в `Content/Meshy/` в проекте и пиши мне, я подключу их вместо кубов.
