# docs/flat-art-list.md from the manifests of hero_bedroom, hero_bath, hero_hall (KAN-41): asset -> room -> size in game units,
# and for what opens: its parent, the attach point and the motion. Run: python Tools/blender/flat_art_list.py
import json, os
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
S = 1900.0
ROOMS = [("bedroom", "Спальня"), ("bath", "Ванная и туалет"), ("hall", "Прихожая")]

def uu(v): return int(round(v * S))
def ue(p): return "(%d, %d, %d)" % (uu(p[0]), uu(-p[1]), uu(p[2]))        # the game mirrors Y

def motion(a):
    m = a["move"]
    if m == "hinge" and a["axis"] == "Z":
        return "петля, ось Z через начало координат детали; открывается на %d° (в UE yaw %+d°)" % (abs(a["open_deg"]), -a["open_deg"])
    if m == "hinge":
        return "петля, ось X через начало координат детали; свободный (передний) край поднимается вверх на %d°" % abs(a["open_deg"])
    if m == "slide":
        return "выдвигается на %s uu (UE)" % ue(a["slide_m"])
    if m == "lift":
        return "снимается вверх на %d uu" % uu(a["slide_m"][2])
    return "не двигается: лежит / висит в системе координат родителя"

out = ["# Квартира «Двушка»: hero-ассеты спальни, ванной и прихожей (KAN-41)", "",
       "Файл собирается скриптом `Tools/blender/flat_art_list.py` из манифестов `SourceArt/Blender/hero_<room>/hero_<room>_manifest.json`. Руками не править.", "",
       "## Как этим пользоваться", "",
       "- Ассет в игре: `/Game/Hero/<Имя>/<Имя>`, материал `MI_<Имя>` (родитель `M_Hero`, слой состояний AntState), куски разрушения `<Имя>_Brk<i>` в той же папке.",
       "- Меши импортированы в сантиметрах (1 м = 100), как вся кухня и гостиная: ставить через `KitchenArt::Add` с обычным масштабом строки `{1,1,1}`, множитель `KitchenArt::ImportScale = 19` он применяет сам. Все размеры ниже уже в игровых единицах (1 м = 1900 uu).",
       "- Система координат каждого ассета: X вправо, перед предмета смотрит в -Y Blender (в UE это +Y, потому что импорт зеркалит Y), задняя стенка / стена сзади, Z вверх, пол на Z = 0. Точки в таблицах даны уже в координатах UE.",
       "- Настенные вещи (шкафчик, полотенцесушитель, держатель бумаги, вешалка, зеркало) имеют начало координат НА ПОЛУ у стены: высота подвеса уже заложена в модель.",
       "- Всё, что открывается, сделано отдельным мешем с началом координат на оси петли или на переднем крае ящика. Где оно стоит на родителе и как двигается, сказано в таблице «Подвижные части» (то же самое лежит в манифесте в поле `attach`).",
       "- Коллизия: UCX-оболочки по форме для физики, точные треугольники LOD0 для муравьёв (как у кухни).", ""]
tot = 0
for room, ru in ROOMS:
    mp = os.path.join(ROOT, "SourceArt", "Blender", "hero_" + room, "hero_%s_manifest.json" % room)
    if not os.path.exists(mp):
        out += ["## %s" % ru, "", "Манифеста нет.", ""]; continue
    ps = {p["name"]: p for p in json.load(open(mp, encoding="utf-8"))["pieces"]}
    order = []
    for n in sorted(ps):
        if not ps[n].get("attach"):
            order.append(n)
            order += sorted(k for k in ps if ps[k].get("attach", {}).get("parent") == n)
    order += [n for n in sorted(ps) if n not in order]
    out += ["## %s (набор `hero_%s`, %d ассетов)" % (ru, room, len(order)), "",
            "| Ассет | Что это | Размер X × Y × Z, uu | Границы в UE, uu: min → max | Треугольники LOD0 / 1 / 2 | UCX | Куски | Класс |", "|---|---|---|---|---|---|---|---|"]
    for n in order:
        p = ps[n]; a, b = p["bbox_min_m"], p["bbox_max_m"]; tot += 1
        mn = (uu(a[0]), uu(-b[1]), uu(a[2])); mx = (uu(b[0]), uu(-a[1]), uu(b[2]))
        out.append("| `%s`%s | %s | %d × %d × %d | (%d, %d, %d) → (%d, %d, %d) | %s | %d | %d | %s |" % (
            n, " ↳" if p.get("attach") else "", p.get("ru", ""), uu(b[0] - a[0]), uu(b[1] - a[1]), uu(b[2] - a[2]), mn[0], mn[1], mn[2], mx[0], mx[1], mx[2],
            " / ".join(str(t) for t in p["lod_triangles"]), p["ucx"], len(p["breaks"]), p.get("break_class", "")))
    mv = [n for n in order if ps[n].get("attach")]
    if mv:
        out += ["", "Подвижные и приложенные части (↳):", "", "| Часть | Родитель | Точки крепления на родителе, uu (UE) | Движение |", "|---|---|---|---|"]
        for n in mv:
            a = ps[n]["attach"]
            out.append("| `%s` | `%s` | %s | %s |" % (n, a["parent"], "; ".join(ue(q) for q in a["at_m"]), motion(a)))
    out.append("")
out += ["## Соответствие кубам лаборатории (`Tools/levellab/build_lab.py`, вариант A)", "",
        "| Куб в лаборатории | Ассет |", "|---|---|",
        "| `bed`, `bedLeg`, `headboard` | `SM_B_Bed` + 2 × `SM_B_Pillow` |", "| `duvet` | `SM_B_Duvet` (в системе координат кровати) |",
        "| `wardrobe` | `SM_B_Wardrobe` + `SM_B_Wardrobe_DoorL`, `SM_B_Wardrobe_DoorR` |", "| `toybox` | уже есть: `SM_V3_ToyBox` + `SM_V3_ToyBox_Lid` (набор hero_living) |",
        "| нет куба: тумбы | 2 × `SM_B_Nightstand` + `SM_B_Nightstand_Drawer` по сторонам кровати |", "| нет куба: комод | `SM_B_Dresser` + 4 × `SM_B_Dresser_Drawer` |",
        "| `washer`, `hose` | `SM_Bath_Washer` + `SM_Bath_Washer_Door` (шланги и провод на задней стенке машины, отдельного меша шланга нет) |",
        "| `bSink` | `SM_Bath_Sink` + `SM_Bath_Sink_Door` |", "| нет куба: шкафчик | `SM_Bath_Cabinet` + `SM_Bath_Cabinet_Door` над раковиной |",
        "| `bTowel`, `bTowelLow` | `SM_Bath_TowelRail` + `SM_Bath_Towel` (полотенце в системе координат сушителя) |",
        "| `tubFloor`, `tubS`, `tubW`, `tubE` | `SM_Bath_Tub` + `SM_Bath_Tub_Panel` |", "| `mat` | `SM_Bath_Mat` (в системе координат ванны) |",
        "| `toilet`, `tank` | `SM_Bath_Toilet` + `SM_Bath_Toilet_Seat`, `SM_Bath_Toilet_Lid`, `SM_Bath_Toilet_TankLid` |", "| `paperShelf`, `paper` | `SM_Bath_PaperHolder` |",
        "| `shoeRack` | уже есть: `SM_L_ShoeRack` (набор hero_living) |", "| `hatShelf` | `SM_Hall_CoatRack` |",
        "| `coat`, `coat2` | `SM_Hall_Coat`, `SM_Hall_Jacket` (в системе координат вешалки) |",
        "| нет куба: зеркало, входная дверь, коврик | `SM_Hall_Mirror`; `SM_Hall_EntryDoor_Frame` + `SM_Hall_EntryDoor_Leaf`; `SM_Hall_DoorMat` |",
        "| `block`, `bear`, `boot` | не делались: это мелкие предметы, а не мебель |", "",
        "Всего ассетов в трёх наборах: %d." % tot, ""]
dst = os.path.join(ROOT, "docs", "flat-art-list.md")
open(dst, "w", encoding="utf-8", newline="\n").write("\n".join(out))
print("written", dst, tot)
