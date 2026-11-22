# Карта v4 — источники ассетов и лицензии

Все сторонние ассеты — **CC0** (общественное достояние, коммерческое использование разрешено, без атрибуции; список — для порядка).
Сгенерированные — Higgsfield (подписка владельца проекта), права у владельца проекта.

## Poly Haven (https://polyhaven.com, CC0)

Текстуры 1–2K (цвет, нормаль GL, шероховатость, AO), скачивание скриптом `Tools/map4/download_polyhaven.ps1` (список — `Tools/map4/polyhaven_ids.txt`), упаковка — `Tools/map4/pack_textures.py`:

| Набор игры | Poly Haven | Где |
|---|---|---|
| FloorTile | https://polyhaven.com/a/floor_tiles_06 | пол кухни |
| TileCream | https://polyhaven.com/a/interior_tiles | плитка |
| Parquet | https://polyhaven.com/a/herringbone_parquet | паркет гостиной |
| Oak | https://polyhaven.com/a/wood_table_001 | столешница, дерево |
| TableWood | https://polyhaven.com/a/wood_table_worn | стол |
| Walnut | https://polyhaven.com/a/walnut_veneer | орех |
| PaintWood | https://polyhaven.com/a/white_oak_veneer | крашеные фасады, плинтус |
| Plaster | https://polyhaven.com/a/plastered_wall | штукатурка, основа обоев |
| Stucco | https://polyhaven.com/a/white_stucco | потолок |
| Linen | https://polyhaven.com/a/rough_linen | шторы, абажуры |
| Gingham | https://polyhaven.com/a/fabric_pattern_07 | скатерть |
| SofaWool | https://polyhaven.com/a/poly_wool_herringbone | диван |
| Boucle | https://polyhaven.com/a/wool_boucle | плед, теннисный мяч |
| Knit | https://polyhaven.com/a/knitted_fleece | пряжа, носки |
| Terry | https://polyhaven.com/a/terry_cloth | полотенце |
| Jersey | https://polyhaven.com/a/cotton_jersey | подушки |
| Melange | https://polyhaven.com/a/jersey_melange | трикотаж |
| Carpet | https://polyhaven.com/a/dirty_carpet | ковёр, основа ковра гостиной |
| Jute | https://polyhaven.com/a/hessian_230 | коврики, плетёное |
| Leather | https://polyhaven.com/a/brown_leather | кожа |
| Grass | https://polyhaven.com/a/leafy_grass | газон |
| Soil | https://polyhaven.com/a/forest_ground_04 | земля |
| Asphalt | https://polyhaven.com/a/asphalt_02 | дорога |
| Pavement | https://polyhaven.com/a/concrete_pavement | тротуар |
| Bark | https://polyhaven.com/a/bark_brown_02 | кора |
| Picket | https://polyhaven.com/a/white_planks_clean | забор |
| Siding | https://polyhaven.com/a/weathered_plank_siding | фасад, сарай |
| Concrete | https://polyhaven.com/a/smooth_concrete_floor | бетон, бордюр |
| Marble | https://polyhaven.com/a/marble_01 | камень |
| MetalPlate | https://polyhaven.com/a/metal_plate_02 | чугун, жесть |

## ambientCG (https://ambientcg.com, CC0)

`Tools/map4/download_ambientcg.ps1`: Metal009 (Brushed — нержавейка), Metal012 (Brushed2 — хром, латунь), Cardboard001, Paper001, Plastic010, Rubber004.
Ссылки: https://ambientcg.com/view?id=Metal009 , …?id=Metal012 , …?id=Cardboard001 , …?id=Paper001 , …?id=Plastic010 , …?id=Rubber004

## Сгенерировано (Higgsfield) — `SourceArt/Map4/higgsfield/`

- `tex/`: обои кухни, обои гостиной, ковёр (бесшовные; швы сглажены в `pack_textures.py`).
- `decal/`: 4 рисунка малыша, фото семьи, картина, календарь, этикетки (кола, хлопья, сгущёнка, печенье, леденец). Бренды выдуманные.
- `shots/`: продуктовые снимки для image→3D; `glb/`: исходные 3D-модели Meshy v7 (через Higgsfield) — торт, бутерброд, котлета, печенье, пломбир, долька яблока, попкорн, зефир, карамелька.
- `concept/`: концепт-арт районов (ориентир, в игру не идёт; на одном кадре модель нарисовала реальные бренды — не использовать).

Процедурные (наши): эмаль/краска (нормаль «апельсиновая корка»), микро-нормаль масштаба муравья, базовые формы и FX-меши (`Tools/map4/blender_shapes.py`), упаковка еды (`Tools/map4/blender_food_simple.py`).

**Ревью художника (Данила):** всё в `SourceArt/Map4/` и `Content/Map4/` сделано по прямому распоряжению владельца без художника — просьба просмотреть.
