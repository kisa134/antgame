# Муравейник — экран колонии (мета-прогрессия)

Ветка `code/ANT-colony`. Колония между рейдами показана как живой 2D-разрез щели в стене (Fallout Shelter / SimAnt): залы вдоль ходов, муравьи бегают по ходам, Королева на троне, кладовые растут кучами по сортам еды, личинки светятся в яслях, трофеи висят на стенах, имена павших — на Стене памяти, внизу «Летопись колонии». После каждого рейда — «шоу прогресса» (10–20 с, пропускается).

Одна колония на co-op группу: колония хоста (как и раньше), `Saved/Nest/colony.sav`, реплицируется внутри `AAntRaid::Nest`. Клиенты видят её только для чтения.

## Код

| Файл | Что |
|---|---|
| `AntMetaBus.h` | интерфейс с личным слоем (AntProfile / AntRoster): `FAntRaidSummary`, `OnRaidSummary`, `GetFallenVeterans`, `GetPlayerHighlights` |
| `AntColony.h/.cpp` | правила: залы, ходы, сорта еды, трофеи, летопись, павшие, сохранение v2 + миграция v1, рейд «на бумаге», API для туториала |
| `AntColonyView.h/.cpp` | экран (Canvas, как весь HUD): разрез, муравьи, Королева, шоу, карточки залов |
| `AntRaidColony.cpp` | часть рейда: кто что принёс (по сортам), трофеи, грибница, летопись, сводка на шину, dev-команды `colony ...` |
| `AntNest.*`, `AntRaid.*` | новые поля состояния (v2), хуки доставки / смерти / конца рейда, команды `Dig` / `Chamber` |
| `AntHUDRaid.cpp`, `AntHUDNest.cpp` | фаза гнезда показывает Муравейник; старый планшет — «ШТАБ» ([Tab] туда-обратно) |
| `AntAdmin.cpp` | вкладка «Колония» |

## Залы (11) и старая экономика

Шесть старых веток стали залами: купить ярус ветки = отстроить зал (цены прежние: I 120 еды, II 300 + 2 молочка, III 650 + 5 молочка и довольная Королева). Старый экран «ШТАБ» покупает то же самое.

| Зал | Ветка / новое | Ход |
|---|---|---|
| Тронный зал | растёт сам: уровень колонии 8 → трон из коробка + корона-крышечка, 20 → балдахин из крышечки и фольги, корона с бусинами | хаб |
| Кладовые | Носильщики | хаб |
| Ясли | Выводок | хаб |
| Казарма | Солдаты | хаб |
| Арсенал | Арсенал (инструменты) | хаб |
| Грибная ферма | новое: +20 / +40 / +60 еды после рейда, на III ещё +1 молочко | Трубный ход |
| Трофейная | новое: вешает 4 / 8 / все трофеи, каждый висящий +2% к еде; на III Королева +2 настроения после рейда | Трубный ход |
| Мастерская сапёров | Инженеры | Проводной ход |
| Радиоточка | новое: приказы платят вдвое; +1 молочко за рейд с выполненным приказом; Королева +3 настроения после рейда | Проводной ход |
| Плац | Разведчики | Подпольный ход |
| Стена памяти | новое: смерти огорчают Королеву вдвое меньше; при гибели отряда красные уносят треть, а не половину; после рейда с потерями +1 личинка | Подпольный ход |

Новые залы: I 150 еды, II 400 + 3 молочка, III 800 + 6 и довольная Королева (они сверх старой экономики, поэтому чуть дороже).

**Ходы** (выбор, куда расти): к трубе (вода, грибы), к проводке (свет, мастерская, радио), к щели в полу (кошачий лаз, разведка, плац, память). Цена по порядку рытья: 120 → 300 + 1 → 500 + 3 молочка — первый ход дешёвый, поэтому колонии растут по-разному. Ветки Инженеры и Разведчики теперь требуют своего хода (старые сохранения получают его бесплатно, см. миграцию).

**Уровень колонии** = сумма ярусов всех залов + прорытые ходы, 0..33.

## Ресурсы

- **Еда по сортам**: Сахар, Белок, Жир, Крошки (`AntColony::KindOf` по имени добычи: торт/сладкое → сахар, корм/семечки/котлета → белок, сыр/чипсы/попкорн/масло → жир, хлеб/печенье/крупа/всё прочее → крошки). Сорта — для глаза (кучи в кладовых, сводка); тратится общая сумма `Food`, как раньше; траты снимаются со всех сортов пропорционально (`AntColony::Sync`).
- **Молочко** (торт, целые трофеи, находки, приказы, грибница III, радио II).
- **Трофеи** (13): Пуговица (первый рейд с добычей), Кремовая розочка (первый торт), Монетка (добыча ≥ 1,5 запроса), Колечко колбасы (150 белка), Клок тапка Великана (торт дома, хотя кто-то погиб), Осколок тарелки (угроза III+), Мармеладный мишка / Фольга / Корочка сыра (донести целым), Напёрсток молочка (особая находка), Серёжка Бабушки (2 приказа), Жвало красного (отбить торт из холма), Свечка именинника (торт в «День рождения»).
- **Личинки / рабочие** — как раньше.

## Баланс

Цель: 1 рейд ≈ 1 видимое изменение, 10–15 рейдов до полной колонии. Всё вместе стоит ≈ 12 700 еды и ≈ 82 молочка (6×1070 + 4×1350 + 920 еды; 42 + 36 + 4 молочка); молочко и есть главный ограничитель темпа. Рейд I угрозы приносит ≈ 600 (торт) + 200–350 еды и 2–4 молочка, на III–IV угрозе ×1,6–2. Проверка — `colony sim 15` (рейды на бумаге: торт в 86% рейдов, бумажная колония всегда берёт самую трудную открытую угрозу и строит самое дешёвое): уровень после рейдов 1/2/3/5/8/10/12 = 6/12/15/21/27/30/33 — полная колония к 12-му рейду при почти идеальной игре, ≈ 2 постройки за рейд. Первый рейд обставляет хаб (ярусы I дешёвые — цены старых веток не трогали).

## Шоу после рейда

Запускается на каждой машине, когда `AAntRaid::ColonyShow` меняется и фаза — гнездо (после экрана «Боевое донесение»). Этапы (пустые пропускаются), камера наезжает на нужный зал:

1. Заставка «РЕЙД N: ТОРТ ДОМА! / ОТРЯД ПОГИБ / БЕЗ ТОРТА» (2,2 с)
2. Добыча: колонны муравьёв от входа через тронный зал в кладовые несут еду по сортам; кучи растут от «до рейда» к «после» (4,6 с)
3. Королева: лицо «до» → реакция (подпрыгивает / трясётся), реплика на бумажке, «КОРОЛЕВА +12» (3,2 с)
4. Ясли: личинки появляются (2,2 с)
5. Трофей опускается сверху и вешается в трофейной (2,8 с)
6. «МОЖНО СТРОИТЬ: …» — подсвечивается первый зал / ход, на который теперь хватает (2,4 с)
7. «Летопись колонии» печатает строку рейда (3,2 с)
8. «Отличились в рейде»: карточки из `AntMetaBus::GetPlayerHighlights()` (звания, медали — от слоя бойцов) и свои (донёс торт, лучший носильщик, спасатель)

ЛКМ / Пробел / Esc — пропустить.

## Анимация (всё в движке, Canvas)

Муравьи — процедурные (три тела, шагающие лапки, усики), бегают по графу ходов, часть несёт крошку своего сорта; число ≈ рабочие/2 (6..46). Королева дышит (squash), реагирует на смену настроения, засыпает после 25 с без мыши (Z-z-z). Личинки светятся пульсом, коконы покачиваются, грибы колышутся, шестерёнки мастерской крутятся, радио «вещает» волнами (сильнее во время шоу), капля с трубы падает (в грибную ферму, если ход прорыт), проводка мерцает, свет из щели в полу дышит, пылинки — ближний параллакс-слой (1,25), виньетка — ещё один. Наведение — подсветка зала и подсказка внизу; клик — карточка зала со всеми ярусами и кнопкой стройки. Колесо — зум 1..2,5 к курсору, перетаскивание — панорама.

## Арт

Текстуры `/Game/UI/Colony/T_Colony_*` (исходники `SourceArt/UI/Colony`, импорт `Tools/make_colony_assets.py`). Без текстуры экран рисует заглушки (полосы грунта, точки, процедурную Королеву) — работает всегда.

- **Higgsfield** (gpt_image_2_6, high): фон-разрез `Bg`, оболочки залов `Shell_A` (тронный, с земляным креслом) и `Shell_B` (с ямками). Сырьё — `Art/Colony/raw`. Аккаунт кончился на 4-й картинке (см. `docs/COLONY_SPEND.md`).
- **Blender** (`Tools/colony_blender_art.py`, Cycles, прозрачный фон, тёплый ключ): Королева ×5 настроений + короны `Crown_2/3` (рендер с holdout тела, ложится точно поверх), накладки ярусов `Frame` (II: спички, зубочистка, нитки) и `Deco` (III: крышечки-лампы, гирлянда с пуговицами, фольга), троны `Seat_2/3`, `Larva`, `Cocoon`, кучи `Pile_*` ×4, `Jelly`, `CakeSlice`, `Bunk`, `Crate`, `Mushroom`, `Radio`, `Target`, `Plaque`, трофеи `Trophy_*` ×13. Подготовка — `Tools/colony_art_prep.py`.
- Иконки UI-агента (`/Game/UI/Icons`) — для сортов еды, инструментов арсенала, карточек.

### Промпты Higgsfield (замена Blender-арта, когда будут кредиты)

Общая преамбула (всегда первой): *Game sprite for a cozy military-parody game about 8 mm ants. Style: stylized 3D animated-feature look (Pixar-like), macro miniature diorama, chunky readable shapes, warm amber candle light, chocolate and ochre soil browns, soft ambient occlusion. Strict side view cross-section (cutaway, like an ant farm or Fallout Shelter). … No ants, no text, no letters, no flags, no emblems, no crosses.* Залы — `aspect 3:2`, `background: transparent`, `quality: high`; ярус II и III — правкой картинки яруса I (`medias` = job id яруса I, «same chamber, now …»), чтобы зал не менялся.

| Картинка | Промпт (после преамбулы) |
|---|---|
| Bg (16:9, сделано) | inside of a crack in a kitchen wall … layered soil and plaster strata, floor tiles and baseboard on top with a bright entrance gap, roots, a rusty copper pipe with a drop on the left, old wires on the right, a gap under the floorboards with light at the bottom right, a bottle cap, an earring and a button half-buried; no chambers, no tunnels |
| Throne I (сделано) | ONE rounded chamber, the queen's throne room, stage 1 of 3 (bare earth): an empty seat of packed earth, a dry leaf as a carpet, sugar crumbs, one glowing crumb lamp, tunnel openings left and right |
| Stores I (сделано) | ONE chamber, the food storeroom, stage 1 of 3: four empty hollows for food piles, a seed husk, a crumb |
| <зал> II | same chamber, now reinforced: matchstick posts and a toothpick beam lashed with thread, a matchwood plank floor, the same layout |
| <зал> III | same chamber, now decorated with trophies: bottle-cap lamps glowing on threads, a garland of thread with buttons, foil rugs, a polished look |
| Nursery I | ONE chamber, the nursery: soft cotton-fluff beds on the floor, glowing pale larvae, silk cocoons along the back wall |
| Barracks I | ONE chamber, the barracks: bunks made of matchboxes, a sharpening stone, a drill-sergeant's corner |
| Arsenal I | ONE chamber, the tool store: crumb baits, scent vials, a bottle-cap shield rack |
| Fungus I | ONE chamber by a dripping pipe: a damp fungus garden with small mushrooms in rows |
| Trophies I | ONE chamber, the trophy hall: empty pins on the back wall waiting for trophies |
| Workshop I | ONE chamber, the sappers' workshop under old wiring: a bare bulb, pliers made of pins, gears from a watch |
| Radio I | ONE chamber, «Nest radio»: a thimble loudspeaker on a red button, a wire to the wiring |
| Parade I | ONE chamber, the parade ground: a flat trodden floor, a bottle-cap target, sugar-cube hurdles, light from a floor gap |
| Memorial I | ONE chamber, the wall of memory: a slab of dark bark on the back wall (blank, the names are drawn in game), a matchstick eternal flame |
| Queen (1:1) | Her Sweetness the Queen Mother: huge plump glossy amber abdomen with soft stripes, small regal head, big expressive eyes with lashes, tiny wing stubs, a pearl-bead necklace; side view facing right, reclining as if on a throne; mood: content / delighted (open-mouth laugh, blush) / grumpy (half-shut eyes, frown) / furious (red face, bared teeth, angry brows) / asleep (eyes closed) — генерировать «content», остальные — правкой с ней в `medias` |
| Crown (1:1) | the same queen wearing a crown made of a crimped golden bottle cap (II) / with bead jewels and a foil cape (III) — потом `remove_background` и вычесть тело |
| Trophies (1:1 каждый) | game icon of <a sewing button / a cream rose from a cake / a coin / a slice of sausage / a piece of a fluffy pink slipper / a china plate shard with a blue rim / a golden gummy bear / a crumpled chocolate foil / a cheese rind / a thimble of royal jelly / a pearl earring / a red ant's mandible / a striped birthday candle>, isolated, transparent background |

Видео-петли (Королева дышит, праздник в тронном зале) — `generate_video` по картинке Королевы, 4–5 с loop; импорт как flipbook. Пока Королева анимируется спрайтом (дыхание, подпрыгивание, дрожь, сон).

## Сохранение

`colony.sav` v2 = строки v1 + `stock=`, `dug=`, `own=`, `trophies=`, `grown=`, `chron0..13=`, `fallen0..23=`, та же контрольная сумма. Старое сохранение v1 читается и мигрирует (`AntColony::Migrate`): еда делится на сорта 30/20/15/35 (сахар/белок/жир/крошки), ходы к уже построенным Инженерам / Разведчикам считаются прорытыми, в летописи — «колония переехала в новый муравейник». Прогресс не теряется; при первой записи v1-файл уходит в `colony.bak`. Новые сохранения старая сборка не прочтёт (версия выше) — у каждой сборки своя папка Saved.

## Сеть

Всё состояние — в `AAntRaid::Nest` (реплицируется), шоу — счётчик `AAntRaid::ColonyShow`. Стройка и рытьё — `ServerRaid(Dig / Chamber)`, только хост; у клиента кнопки серые с подсказкой «Колония хоста». Подсветки игроков (звания/медали) каждая машина берёт у своего `AntMetaBus` провайдера.

## API для других систем

```cpp
#include "AntColony.h"      // сервер
FAntColonyGift G;  G.Food[AntFoodKind::Sugar] = 150;  G.Jelly = 2;  G.Chronicle = TEXT("Подарок Королевы");
AntColony::GiveGift(WorldContext, G);   // в кладовые хоста, сохраняет, реплицирует
AntColony::ReplayShow(WorldContext);    // всем ещё раз шоу последнего рейда
#include "AntColonyView.h"  // эта машина
AntColonyView::Open(AntColony::Stores); // экран колонии (+ карточка зала)
```

Конец рейда ничего не требует: фаза гнезда сама открывает Муравейник и играет шоу.

## Dev / админ

Вкладка админки «Колония». Команды `-AntDo` / консоль админки: `colony info | view | sheet | show | showat T | open <зал> | close | zoom Z X Y | sleep | shot NAME | give food|sugar|protein|fat|crumbs|jelly|larvae|workers N | give trophy all|none|random|I | dig all|none|D|buy D | set <зал> T | setall T | up <зал> | mood N | sim N [nobuild] | reset | v1save | reload`.
