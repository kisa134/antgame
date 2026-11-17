# Интеграция смоделированной кухни и квартиры (Blender → игра)

Состояние на 17.11.2026. Модели: `SourceArt/Blender/kitchen` (117 мешей `SM_K_*`, 171 размещение) и `SourceArt/Blender/living` (83 меша `SM_L_* / SM_Y_* / SM_S_*`, 231 размещение: гостиная, двор, улица).

## Как это устроено

| Шаг | Где | Что делает |
| --- | --- | --- |
| Импорт | `Tools/import_blender_art.py` (запуск: `Saved/art/import_art.ps1 -Sets kitchen` или `living`) | FBX → `/Game/Kitchen/<Имя>/<Имя>` и `/Game/Living/...`. Один меш на файл, имена слотов материалов сохраняются, материалы и текстуры не импортируются, без Nanite. Коллизия — собственные треугольники меша (complex as simple); у 15 мелких предметов ещё и простой бокс. Отчёт: `Saved/blender_art_report.json`, сверка габаритов и слотов с манифестом — в логе (`ART:`). |
| Таблицы | `Tools/gen_kitchen_art.py` → `Source/AntKitchen/KitchenArtData.inl` | Манифесты превращаются в две таблицы C++: все размещения (меш, точка, оси, масштаб, роль) и все «двигающиеся части» (петля/выдвижение, угол, флаги, подписи). Перегенерировать после любого изменения манифеста. |
| Вид и коллизия | `KitchenArt.h/.cpp` | Загрузка мешей, таблица «слот материала → вид», коллизия, размещение. |
| Статичные куски | `AKitchenBuilder::BuildArt()` | Ставит всё с ролью «статика»; старые коробки тех же предметов пропускаются (`if (!bArt)`). |
| Двери, ящики, крышки | `AKitchenDoor`, `AKitchenKnob`, `AKitchenPedalBin`, `AKitchenToaster`, `AKitchenSwitch`, `AKitchenTrap`, `AKitchenFaucet` + новый `AKitchenMover` | Существующие гаджеты берут свои модели вместо коробок; всё остальное подвижное — универсальный `AKitchenMover`. |
| Упаковка | `Config/DefaultGame.ini` | `/Game/Kitchen` и `/Game/Living` добавлены в `DirectoriesToAlwaysCook`. |

**Переключатель:** `-AntOldKitchen` в командной строке — прежняя кухня из коробок (ничего не удалено, обе версии собираются из одного кода). Отладка: `-AntArtOpen` (всё открыто), `-AntArtProbe` (лог: что лежит под каждой точкой лута), `-AntArtTour` (муравья по очереди ставят на все поверхности, в лог пишется, устоял ли он).

## Материалы

Все модели красятся материалом игры `M_Surface` (трипланар) с текстурами из `/Game/Textures`; на каждый вид — один общий экземпляр материала (кэш закреплён `AddToRoot`, переживает перезагрузку карты из меню).

| Слоты | Вид |
| --- | --- |
| Cabinet, CabinetInset | `T_Cabinet_Sage` (шалфейная краска), вставка чуть темнее |
| Butcher / Walnut, Bark | `T_Wood_Oak` / `T_Wood_Walnut` |
| Steel, Chrome, Brass, Iron | `T_Steel_Brushed` с оттенком и своей шероховатостью/металличностью |
| Enamel, FridgeRed, FridgeLiner, Red, Blue, MugBlue, Tile, Teal, Navy, BinGreen, CarPaint | `T_Enamel_White` с оттенком (чистая эмаль/краска) |
| Fabric, SofaFabric, Cushion, CarpetTrim, SockWool, Yarn | `T_Fabric_Grey` с оттенком |
| ковёр кухни, Carpet | `T_Rug_Red` |
| Soil | `T_Soil` |
| Concrete, Kerb, Asphalt, Stone | каменный режим материала (крапинка) |
| HeatGlow, Screen, Bulb, LampGlass, Headlight | светящийся материал |
| LampShade | свой экземпляр на каждую лампу: яркость ведёт выключатель |
| остальное (Plinth, Grout, Paper, Honey, Jam, Leaf, Cheese…) | режим «гладкая краска» того же материала с цветом и шероховатостью из манифеста |

Фартук смоделирован плитка к плитке, поэтому плитка — глянцевая эмаль, швы — отдельный матовый слот (плиточный режим материала дал бы вторую, несовпадающую сетку швов).

## Кухня: что чем заменено

| Модели | Что заменяют в коде | Как сохранён геймплей |
| --- | --- | --- |
| Base1400, BaseDrawers, Base1300, Base1200, BaseSink, WorktopA/B, Sink, Faucet, Splashback | `BuildNorthRun` целиком, фартук из `Build` | Шкафы на 250 не доходят до стены: коридор за ними остался (проверено: муравей стоит в коридоре, в т.ч. за плитой). Шкафы теперь полые, внутрь можно залезть через дверцы и щель под ними. |
| Base*_DoorL/R, BaseSink_DoorL/R, WallCab*_DoorL/R, Drawer2, Drawer3 | декоративные дверцы и фальш-ящики | `AKitchenMover`: [E] открыть/закрыть. Дверца под мойкой справа открыта с начала, как раньше. Угол 110°, у стены и у дверец-соседей — 90°. Нижний ящик выезжает только на 300: дальше лежит скалка. |
| Drawer1 | `AKitchenDoor(Drawer)` | Тот же гаджет (заначка при первом открытии), стоит на X=-4900 — там, где модель. |
| SinkTrap, SinkHose, Detergent, Sponge, MouseTrap(+Bar) | трубы, бутылка, губка, мышеловка | Проход «слив ↔ под мойку» и мышеловка-гаджет на месте; у ловушки модельная скоба. |
| Fridge, Fridge_DoorMain | `BuildFridge`, `AKitchenDoor(Fridge)` | Те же полости (холод/мороз работают по тем же зонам), зазор под холодильником и лаз в морозилку сзади остались. |
| Fridge_DoorFreezer, Fridge_Crisper | неподвижный фасад морозилки; пол камеры | `AKitchenMover`. Морозилка сама закрывается через 15 с; ящик для овощей выдвигается только при открытой двери и задвигается, когда она закрывается. |
| MilkCarton, CheeseWedge, EggTray, ButterTub, JarJam, Plate, IceCreamTub, IceTray | еда в холодильнике | Статика на полках; точки лута сдвинуты с занятых мест (см. ниже). |
| Range, Range_OvenDoor, Range_Rack, Range_Tray, Range_Knob ×5, Range_Burner ×4, Pot | духовка, плита, ручки, кастрюля | Дверца — `AKitchenDoor(Oven)` (рампа). Средняя ручка и передняя правая конфорка принадлежат `AKitchenKnob`; суп, пузыри и жар — прежние. Pot_Lid лежит на столешнице: кастрюля в игре открытая. |
| Hood, WallCab1250 ×4, WallCab1500 | вытяжка, верхние шкафы | Полые, с полками; на верх по-прежнему ведут тостер-катапульта и вентиляция. |
| Dishwasher, Dishwasher_Door, Dishwasher_Rack ×2 (+тарелки, миска) | `BuildAppliances`, `AKitchenDoor(Dishwasher)` | Корзины — `AKitchenMover`, выкатываются при открытой дверце и везут посуду с собой. |
| Microwave, Microwave_Door, Microwave_Plate, Bowl | микроволновка | Дверца — `AKitchenDoor(Microwave)`. |
| Toaster, Toaster_Lever, Toast ×2 | тостер | `AKitchenToaster`: модельный рычаг и тосты; зона запуска углублена в настоящие щели. |
| Bin, Bin_Lid, Bin_Pedal | педальное ведро | `AKitchenPedalBin` с модельной крышкой и педалью; мешок и объедки внутри прежние. |
| Switch, Switch_Toggle | выключатель | `AKitchenSwitch`, клавиша модельная. |
| Faucet_TapHot/Cold | вентили | Поворачиваются, когда идёт их вода (`AKitchenFaucet`). |
| Table, Chair ×2, Rug, все предметы на столе | стол, стулья, ковёр | Те же высоты (стол 1400, сиденье 880). |
| Bucket, Mop, Clock, WallShelf, Window, WindowSill ×2, Curtain ×2, CurtainRod, Skirting ×8, VentGrille ×4, CeilingLamp ×3 | ведро, часы, полка, окно, шторы, плинтусы, решётки, плафоны | Плинтусы с теми же разрывами под щели и решётки; между ламелями решёток можно пролезть. В ведре вода — отдельная поверхность. |
| CerealBox, PetMat, PetBowl ×2, Kibble, Towel, Spoon, Fork, Shoe, Newspaper, TeaBag, Matchbox(+Tray), RollingPin, TennisBall, Bone, UtensilRail, Ladle, Spatula, Whisk, SpiceShelf ×4, SpiceJar ×9, JarHoney, JarSugar, SugarBowl, Mug ×2, Cake, Shaker ×2, CuttingBoard, Knife, Kettle, PlantPot | коробки и «клаттер» | Все пути наверх (полотенце, шторы, ложка, нитка чайного пакетика, рейлинг, полочки) остались. Кружка на столешнице сдвинута: место по манифесту — внутри несмоделированной коробки хлопьев. |

Не смоделировано и осталось прежним: вентилятор, оголённый провод, коробка хлопьев на столешнице, лампа над столом, мусор и мешок у ведра, нора и комната мыши, шатающаяся плитка, муравейники, паутина, короба вентиляции.

## Гостиная, двор, улица

| Модели | Что заменяют |
| --- | --- |
| Sofa (+подушки), CoffeeTable (+миска, тарелка, пульт, банка, журналы, геймпад), TVStand (+2 сдвижные дверцы), TV, GameConsole, Bookshelf (+93 книги, ваза), Armchair, FloorLamp, PottedPlant, Radiator, Carpet, Skirting, CeilingLamp ×2, Sock, Coin, Charger, YarnBall/Thread, DustBunny ×5 | мебель и мелочи `BuildLivingRoom` / `BuildClutter` |
| DoorFrame + Door_Leaf | рама проёма. Дверь — новая: стоит открытой; если её закрыть, Человек и питомцы сами распахивают её на подходе |
| KnittingBasket, DogBed, CatScratchPost, Controller | новые предметы |
| FencePanel ×22, FencePanel_Half ×4, FencePost, FenceGateFrame + FenceGate_Leaf, Tree, Bin + Bin_Lid + Bin_Trash, TrashBag, AppleCore, SteppingStone ×7, DogBall/Bone/Rope/Bowl, DogPoop, GrassTuft A/B/C | забор, калитка, дерево, уличный бак, камни, игрушки, трава (`BuildYard`, `BuildWestStreet`) |
| Facade, FacadeCorner ×2, Gutter, RoofEdge, Drainpipe, PavementSlab ×9, Kerb ×9, Road ×7, Manhole, StormDrain, Hydrant, StreetLamp, IceCream, Gum, Car + 4 колеса | фасад, желоб, водосток, тротуар, дорога, машина, гидрант, фонарь |

Северный забор двора (y = -5200) и газоны остались коробками: в манифесте их нет.

## Коллизии

- Статика: `WorldStatic`, блокирует всё; коллизия по треугольникам — муравьи ползают по тому, что нарисовано, внутри каждой полости; еда лежит на столешницах и полках; Человек упирается в мебель.
- Подвижные части: канал `AntClimbable` с физикой: муравей, стоящий на дверце или в ящике, едет вместе с ней; еда в ящике остаётся в ящике.
- Открытую дверцу нижнего шкафа или ящик Человек захлопывает, когда упирается в них (не застревает).
- Точки лута, попавшие внутрь новых моделей, сдвинуты (`AKitchenBuilder::LootSpots`, таблица `Moved`).