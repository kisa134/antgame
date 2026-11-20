# -*- coding: utf-8 -*-
# Ant Kitchen, level map, step 1: sources -> docs/level/level_data.json
#   python Tools/level/extract_level.py
# Read-only towards the game. It parses Source/AntKitchen (KitchenArtData.inl, KitchenBuilder.cpp, AntRun.cpp, ...) and the
# Blender manifests. Things that cannot be parsed are transcribed by hand below; each of them carries a "needle":
# a literal piece of the C++ that must still be there. The script looks every needle up (file:line goes into the JSON)
# and lists the ones it cannot find under "unresolved" (= the transcription is stale, check it).
import json, os, re, math, struct, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
OUT = None
for _i, _a in enumerate(sys.argv):
    if _a == '--root': ROOT = sys.argv[_i + 1]
    if _a == '--out': OUT = sys.argv[_i + 1]
OUT = OUT or os.path.join(ROOT, 'docs', 'level')
SRC = os.path.join(ROOT, 'Source', 'AntKitchen')
UU = 1900.0          # Unreal units per metre (KitchenBuilder.h: "1 m of real kitchen ~ 1900 units")
_cache = {}
STALE = []

def text(name):
    if name not in _cache:
        p = name if os.path.isabs(name) else os.path.join(SRC, name)
        _cache[name] = open(p, encoding='utf-8-sig').read()
    return _cache[name]

def ref(name, needle):
    """file:line of the first line with the needle; a needle that is gone is recorded as stale."""
    t = text(name)
    k = t.find(needle)
    if k < 0:
        STALE.append({'file': name, 'needle': needle})
        return name + ':?'
    return '%s:%d' % (name, t.count('\n', 0, k) + 1)

def r1(v): return round(float(v), 1)
def mm(v): return round(float(v) / UU, 3)
def P(x, y, z=0.0): return [r1(x), r1(y), r1(z)]

def region(x, y):
    """AKitchenBuilder::Region: 0 kitchen, 1 living room, 2 back yard, 3 west lawn / sidewalk / street."""
    if y < -5150.0 and (x < 7000.0 or y < -9150.0): return 2
    if x < -7050.0: return 3
    return 0 if x < 7100.0 else 1
AREAS = ['kitchen', 'living', 'yard', 'street']
def area_of(x, y): return AREAS[region(x, y)]

# ---------------------------------------------------------------------------------------------
# The art table (KitchenArtData.inl) + mesh sizes from the Blender manifests
# ---------------------------------------------------------------------------------------------
NUM = r'(-?[\d.]+)f'
V3 = r'\{' + NUM + r',\s*' + NUM + r',\s*' + NUM + r'\}'

def load_art():
    t = text('KitchenArtData.inl')
    rx = re.compile(r'\{ TEXT\("(\w+)"\), (\d+), (\d+), (-?\d+), ' + ', '.join([V3] * 5) + r' \},\s*// (\d+)')
    mrx = re.compile(r'\{ (\d+), (\d+), ' + V3 + ', ' + NUM + ', ' + V3 + r', (\d+), ' + NUM + r', TEXT\("([^"]*)"\), TEXT\("([^"]*)"\) \},\s*// (\d+) (\w+)#(\d+)(.*)')
    inst, mov = [], []
    for ln, line in enumerate(t.splitlines(), 1):
        g = rx.search(line)
        if g:
            g = g.groups(); f = [float(v) for v in g[4:19]]
            inst.append({'name': g[0], 'index': int(g[1]), 'role': int(g[2]), 'parent': int(g[3]), 'loc': f[0:3], 'ax': f[3:6],
                         'ay': f[6:9], 'az': f[9:12], 'scale': f[12:15], 'n': int(g[19]), 'line': ln})
            continue
        g = mrx.search(line)
        if g:
            g = g.groups()
            mov.append({'inst': int(g[0]), 'type': int(g[1]), 'axis': [float(v) for v in g[2:5]], 'angle': float(g[5]),
                        'slide': [float(v) for v in g[6:9]], 'flags': int(g[9]), 'auto': float(g[10]), 'open': g[11], 'close': g[12],
                        'n': int(g[13]), 'note': g[16].strip(), 'line': ln})
    return inst, mov

def load_manifests():
    out = {}
    for sub in ('kitchen', 'living'):
        p = os.path.join(ROOT, 'SourceArt', 'Blender', sub, sub + '_manifest.json')
        if not os.path.exists(p): continue
        for piece in json.load(open(p, encoding='utf-8-sig'))['pieces']:
            out[piece['name']] = piece
    return out

def world_pts(i, piece, pts_m):
    """Blender point (m) -> game: UE mesh-local is (100x, -100y, 100z), the component scale is 19 * Scale."""
    res = []
    for (x, y, z) in pts_m:
        l = (UU * x * i['scale'][0], -UU * y * i['scale'][1], UU * z * i['scale'][2])
        res.append([i['loc'][k] + i['ax'][k] * l[0] + i['ay'][k] * l[1] + i['az'][k] * l[2] for k in range(3)])
    return res

def corners(mn, mx):
    return [(x, y, z) for x in (mn[0], mx[0]) for y in (mn[1], mx[1]) for z in (mn[2], mx[2])]

# name | Russian name | category | flags.  Categories: furn (furniture / appliance), part (door, drawer, lid: moves),
# prop (small thing), food (set dressing that looks like food), deco (on a wall / the ceiling), struct (the shell: skirting,
# fence, pavement...), conn (something an ant climbs).  Flags: int = has an inside, gap=N = N uu of air under it,
# top=Z[,Z] = surfaces an ant stands on.
META_TXT = '''
K_Base1400|Нижний шкаф у тостера|furn|int;top=1710
K_Base1400_DoorL|Дверца нижнего шкафа|part|
K_Base1400_DoorR|Дверца нижнего шкафа|part|
K_BaseDrawers|Тумба с тремя ящиками|furn|int;top=1710
K_BaseDrawers_Drawer1|Верхний ящик (заначка)|part|int
K_BaseDrawers_Drawer2|Средний ящик|part|int
K_BaseDrawers_Drawer3|Нижний ящик (упирается в скалку)|part|int
K_Base1300|Нижний шкаф слева от плиты|furn|int;top=1710
K_Base1300_DoorL|Дверца нижнего шкафа|part|
K_Base1300_DoorR|Дверца нижнего шкафа|part|
K_Base1200|Нижний шкаф под микроволновкой|furn|int;top=1710
K_Base1200_DoorL|Дверца нижнего шкафа|part|
K_Base1200_DoorR|Дверца нижнего шкафа|part|
K_BaseSink|Шкаф под мойкой|furn|int;top=1710
K_BaseSink_DoorL|Дверца под мойкой (открыта с начала)|part|
K_BaseSink_DoorR|Дверца под мойкой|part|
K_SinkTrap|Сифон|prop|
K_SinkHose|Шланг в стену|prop|
K_Detergent|Бутылка моющего|prop|
K_Sponge|Губка|prop|
K_WorktopA|Столешница A (запад, с вырезом под мойку)|furn|top=1710
K_WorktopB|Столешница B (восток)|furn|top=1710
K_Sink|Мойка (чаша)|furn|int;top=1150
K_Faucet|Кран|prop|
K_Faucet_TapHot|Вентиль горячей воды|part|
K_Faucet_TapCold|Вентиль холодной воды|part|
K_Splashback|Фартук из плитки|deco|
K_WallCab1250|Верхний шкаф|furn|int;top=4400
K_WallCab1250_DoorL|Дверца верхнего шкафа|part|
K_WallCab1250_DoorR|Дверца верхнего шкафа|part|
K_WallCab1500|Верхний шкаф над микроволновкой|furn|int;top=4400
K_WallCab1500_DoorL|Дверца верхнего шкафа|part|
K_WallCab1500_DoorR|Дверца верхнего шкафа|part|
K_Fridge|Холодильник|furn|int;gap=120;top=3700
K_Fridge_DoorMain|Дверь холодильника|part|
K_Fridge_DoorFreezer|Дверь морозилки|part|
K_Fridge_Crisper|Ящик для овощей|part|int
K_MilkCarton|Пакет молока|food|
K_CheeseWedge|Кусок сыра (декор)|food|
K_EggTray|Лоток яиц|food|
K_ButterTub|Масло|food|
K_IceCreamTub|Ведёрко мороженого|food|
K_IceTray|Форма для льда|food|
K_Range|Плита с духовкой|furn|int;gap=28;top=1740
K_Range_OvenDoor|Дверца духовки (рампа)|part|
K_Range_Knob|Ручка плиты|part|
K_Range_Burner|Конфорка|prop|
K_Range_Rack|Решётка духовки|prop|
K_Range_Tray|Противень|prop|
K_Hood|Вытяжка|furn|top=5400
K_Dishwasher|Посудомойка|furn|int;top=1710
K_Dishwasher_Door|Дверца посудомойки (рампа)|part|
K_Dishwasher_Rack|Корзина посудомойки|part|
K_Microwave|Микроволновка|furn|int;top=2350
K_Microwave_Door|Дверца микроволновки|part|
K_Microwave_Plate|Тарелка микроволновки|prop|
K_Toaster|Тостер (катапульта)|furn|int;top=2150
K_Toaster_Lever|Рычаг тостера|part|
K_Toast|Тост|prop|
K_Table|Обеденный стол|furn|gap=1320;top=1400
K_Chair|Стул|furn|gap=800;top=880
K_Rug|Ковёр кухни|struct|
K_Bin|Педальное ведро|furn|int;top=1540
K_Bin_Lid|Крышка ведра|part|
K_Bin_Pedal|Педаль ведра|part|
K_Bucket|Ведро с водой|furn|int;top=650
K_Mop|Швабра|conn|
K_Clock|Часы|deco|
K_Switch|Выключатель света|deco|
K_Switch_Toggle|Клавиша выключателя|part|
K_Window|Окно (без стекла)|struct|
K_WindowSill|Подоконник|furn|top=2200
K_PlantPot|Цветок на подоконнике|prop|
K_Curtain|Штора до пола|conn|
K_CurtainRod|Карниз|conn|
K_WallShelf|Настенная полка|furn|top=2680
K_Skirting|Плинтус|struct|
K_VentGrille|Решётка вентиляции у пола|struct|
K_VentGrilleSmall|Решётка вентиляции наверху|struct|
K_CerealBox|Коробка хлопьев (тоннель)|furn|int;top=531
K_PetMat|Коврик под миски|struct|
K_PetBowl|Миска|prop|int
K_Kibble|Горка корма|food|
K_CeilingLamp|Плафон|deco|
K_UtensilRail|Рейлинг|conn|
K_Ladle|Половник|conn|
K_Spatula|Лопатка|conn|
K_Whisk|Венчик|conn|
K_SpiceShelf|Полочка специй|conn|
K_SpiceJar|Баночка специй|prop|
K_JarHoney|Банка мёда (опрокинута)|prop|
K_JarJam|Банка варенья|prop|
K_JarSugar|Банка сахара|prop|
K_SugarBowl|Сахарница|prop|
K_Mug|Кружка|prop|
K_Plate|Тарелка|prop|
K_Bowl|Миска с объедками|prop|
K_Pot|Кастрюля с супом|prop|
K_Pot_Lid|Крышка кастрюли|prop|
K_Kettle|Чайник|prop|
K_CuttingBoard|Разделочная доска|prop|
K_Knife|Нож|prop|
K_TeaBag|Чайный пакетик с ниткой|conn|
K_Towel|Полотенце с духовки до пола|conn|
K_Cake|Торт|food|
K_Shaker|Солонка / перечница|prop|
K_RollingPin|Скалка|prop|
K_Matchbox|Спичечный коробок|prop|int
K_Matchbox_Tray|Лоток коробка|part|int
K_Newspaper|Газета-шалаш|prop|int
K_Spoon|Ложка-трап к столу|conn|
K_Fork|Вилка|prop|
K_Bone|Кость|prop|
K_TennisBall|Теннисный мяч|prop|
K_Shoe|Ботинок|prop|int
K_MouseTrap|Мышеловка|prop|
K_MouseTrap_Bar|Скоба мышеловки|part|
L_Sofa|Диван|furn|gap=260;top=950,1350,2100
L_Sofa_SeatCushion|Подушка сиденья|prop|
L_Sofa_BackCushion|Подушка спинки|prop|
L_Pillow_A|Подушка|prop|
L_Pillow_B|Подушка|prop|
L_CoffeeTable|Журнальный стол|furn|gap=300;top=340,980
L_SnackBowl|Миска со снеками|food|
L_BiscuitPlate|Тарелка с печеньем|food|
L_Remote|Пульт|prop|
L_SodaCan|Банка газировки|prop|
L_Magazines|Журналы|prop|
L_TVStand|Тумба под ТВ (полая)|furn|int;gap=38;top=1100
L_TVStand_Door|Сдвижная дверца тумбы|part|
L_TV|Телевизор|furn|top=3300
L_GameConsole|Приставка (включает ТВ)|prop|
L_Controller|Геймпад|prop|
L_Bookshelf|Книжный шкаф|furn|int;top=960,1860,2760,3660,4660
L_Book_|Книга|prop|
L_Vase|Ваза|prop|int
L_Armchair|Кресло бабушки|furn|gap=114;top=800,1200,1900
L_FloorLamp|Торшер|conn|
L_PottedPlant|Растение в горшке|furn|top=730
L_Radiator|Батарея (горячая)|furn|top=1400
L_Carpet|Ковёр гостиной|struct|
L_Sock|Носок (тоннель)|prop|int
L_Coin|Монетка|prop|
L_Charger|Зарядка с кабелем на диван|conn|
L_YarnBall|Клубок|prop|
L_YarnThread|Нитка от клубка к креслу|conn|
L_DustBunny|Комок пыли|prop|
L_KnittingBasket|Корзина с вязанием|prop|int
L_DoorFrame|Дверная коробка|struct|
L_Door_Leaf|Дверь в гостиную|part|
L_Skirting|Плинтус|struct|
L_CeilingLamp|Плафон|deco|
L_DogBed|Лежанка собаки|furn|top=323
L_CatScratchPost|Когтеточка|conn|
Y_FencePanel|Забор|struct|
Y_FencePanel_Half|Забор (полсекции)|struct|
Y_FencePost|Столб забора|struct|
Y_FenceGateFrame|Рама калитки|struct|
Y_FenceGate_Leaf|Калитка|part|
Y_Tree|Дерево|conn|
Y_Bin|Уличный бак|furn|int;top=2621
Y_Bin_Lid|Крышка бака|part|
Y_Bin_Trash|Мешки в баке|prop|
Y_TrashBag|Мешок мусора|prop|
Y_AppleCore|Огрызок|food|
Y_SteppingStone_A|Камень дорожки|struct|
Y_SteppingStone_B|Камень дорожки|struct|
Y_DogBall|Мяч собаки|prop|
Y_DogBone|Кость собаки|prop|
Y_DogRope|Канат собаки|prop|
Y_DogBowl|Миска собаки во дворе|prop|int
Y_DogPoop|Собачья кучка (еда мух)|prop|
S_Facade|Фасад дома|struct|
S_FacadeCorner|Угол фасада|struct|
S_Gutter|Желоб на крыше|furn|int;top=8960
S_RoofEdge|Край крыши|struct|
S_Drainpipe|Водосточная труба (полая)|conn|int
S_PavementSlab_A|Плита тротуара|struct|
S_PavementSlab_B|Плита тротуара с трещиной|struct|
S_Kerb|Бордюр|struct|
S_Road|Дорога|struct|
S_Manhole|Люк|struct|
S_StormDrain|Ливнёвка|struct|int
S_Hydrant|Гидрант|conn|
S_StreetLamp|Фонарь|conn|
S_IceCream|Упавшее мороженое (липко)|food|
S_Gum|Жвачка (липко)|prop|
S_Car|Машина|furn|gap=600;top=2620
S_Car_Wheel|Колесо|conn|
'''
META = {}
for _l in META_TXT.strip().splitlines():
    _n, _ru, _cat, _fl = _l.split('|')
    _d = {'ru': _ru, 'cat': _cat}
    for _f in filter(None, _fl.split(';')):
        _k, _, _v = _f.partition('=')
        _d[_k] = True if _v == '' else ([float(v) for v in _v.split(',')] if _k == 'top' else float(_v))
    META['SM_' + _n] = _d

def meta_for(name):
    if name in META: return META[name]
    for k in META:
        if k.endswith('_') and name.startswith(k): return META[k]
    return {'ru': name, 'cat': 'prop'}

FLAG_NAMES = {1: 'starts_open', 2: 'needs_fridge_open', 4: 'needs_dishwasher_open', 8: 'human_bumps_shut', 16: 'human_opens', 32: 'human_and_pets_push_open'}
GADGET_OF = {   # role 2: built by an existing gadget actor (Tools/gen_kitchen_art.py, GADGET)
    'SM_K_Fridge_DoorMain': 'AKitchenDoor(Fridge)', 'SM_K_Range_OvenDoor': 'AKitchenDoor(Oven)', 'SM_K_BaseDrawers_Drawer1': 'AKitchenDoor(Drawer)',
    'SM_K_Dishwasher_Door': 'AKitchenDoor(Dishwasher)', 'SM_K_Microwave_Door': 'AKitchenDoor(Microwave)', 'SM_K_Range_Knob': 'AKitchenKnob',
    'SM_K_Range_Burner': 'AKitchenKnob', 'SM_K_Toaster_Lever': 'AKitchenToaster', 'SM_K_Toast': 'AKitchenToaster', 'SM_K_Bin_Lid': 'AKitchenPedalBin',
    'SM_K_Bin_Pedal': 'AKitchenPedalBin', 'SM_K_Switch_Toggle': 'AKitchenSwitch', 'SM_K_MouseTrap_Bar': 'AKitchenTrap',
    'SM_K_Faucet_TapHot': 'AKitchenFaucet', 'SM_K_Faucet_TapCold': 'AKitchenFaucet'}

def build_objects():
    inst, mov = load_art()
    man = load_manifests()
    mover_of = {mv['inst']: mv for mv in mov}
    objs, missing = [], []
    for i in inst:
        piece = man.get(i['name'])
        me = meta_for(i['name'])
        o = {'id': 'art%d' % i['n'], 'mesh': i['name'], 'index': i['index'], 'ru': me['ru'], 'cat': me['cat'],
             'role': ['static', 'mover', 'gadget', 'rider'][i['role']], 'solid': True, 'climbable': True,
             'src': 'KitchenArtData.inl:%d' % i['line'], 'origin': P(*i['loc'])}
        if not piece:
            missing.append(i['name']); continue
        mn, mx = piece['bbox_min_m'], piece['bbox_max_m']
        pts = world_pts(i, piece, corners(mn, mx))
        lo = [min(p[k] for p in pts) for k in range(3)]; hi = [max(p[k] for p in pts) for k in range(3)]
        upright = abs(i['az'][2]) > 0.999
        o['bb'] = [r1(lo[0]), r1(lo[1]), r1(hi[0]), r1(hi[1])]
        o['z'] = [r1(lo[2]), r1(hi[2])]
        if upright:
            c = world_pts(i, piece, [((mn[0] + mx[0]) / 2, (mn[1] + mx[1]) / 2, mn[2])])[0]
            o['c'] = [r1(c[0]), r1(c[1])]
            o['s'] = [r1((mx[0] - mn[0]) * UU * i['scale'][0]), r1((mx[1] - mn[1]) * UU * i['scale'][1])]
            o['yaw'] = round(math.degrees(math.atan2(i['ax'][1], i['ax'][0])), 1)
        else:
            o['c'] = [r1((lo[0] + hi[0]) / 2), r1((lo[1] + hi[1]) / 2)]
            o['s'] = [r1(hi[0] - lo[0]), r1(hi[1] - lo[1])]
            o['yaw'] = 0.0
            o['tilted'] = True
        o['area'] = area_of(o['c'][0], o['c'][1])
        for k in ('int', 'gap', 'top'):
            if k in me: o[{'int': 'interior', 'gap': 'gap_under', 'top': 'tops'}[k]] = me[k]
        note = piece.get('note') or piece.get('notes')
        if note: o['art_note'] = note
        if i['name'] in GADGET_OF and i['role'] == 2: o['gadget'] = GADGET_OF[i['name']]
        if i['role'] == 3: o['rides_mover'] = i['parent']
        mv = mover_of.get(i['n'])
        if mv:
            fl = [v for b, v in FLAG_NAMES.items() if mv['flags'] & b]
            cls = 'heavy' if 'DoorFreezer' in i['name'] else ('light' if (mv['type'] == 1 or 'Lid' in i['name'] or 'Gate' in i['name'] or 'Door_Leaf' in i['name']) else 'medium')
            human_shuts = not (mv['flags'] & (1 | 32)) and not i['name'].startswith('SM_Y_')
            o['mover'] = {'part': mv['n'], 'type': 'hinge' if mv['type'] == 0 else 'slide', 'axis': mv['axis'], 'angle': mv['angle'], 'slide': mv['slide'],
                          'flags': fl, 'auto_close_s': mv['auto'], 'open_text': mv['open'], 'close_text': mv['close'], 'door_class': cls,
                          'human_walks_over_to_shut': human_shuts, 'note': mv['note'], 'src': 'KitchenArtData.inl:%d' % mv['line']}
            # the open pose, for parts that turn about a vertical axis or slide
            if mv['type'] == 1:
                o['bb_open'] = [r1(o['bb'][0] + mv['slide'][0]), r1(o['bb'][1] + mv['slide'][1]), r1(o['bb'][2] + mv['slide'][0]), r1(o['bb'][3] + mv['slide'][1])]
            elif abs(mv['axis'][2]) > 0.99:
                a = math.radians(mv['angle']) * (1 if mv['axis'][2] > 0 else -1)
                ca, sa = math.cos(a), math.sin(a)
                hx, hy = i['loc'][0], i['loc'][1]
                q = [(hx + (p[0] - hx) * ca - (p[1] - hy) * sa, hy + (p[0] - hx) * sa + (p[1] - hy) * ca) for p in pts]
                o['bb_open'] = [r1(min(p[0] for p in q)), r1(min(p[1] for p in q)), r1(max(p[0] for p in q)), r1(max(p[1] for p in q))]
                cx, cy = o['c']
                o['c_open'] = [r1(hx + (cx - hx) * ca - (cy - hy) * sa), r1(hy + (cx - hx) * sa + (cy - hy) * ca)]
                o['yaw_open'] = round(o['yaw'] + math.degrees(a), 1)
            o['pose'] = 'open' if (mv['flags'] & 1) else 'closed'
        objs.append(o)
    return objs, inst, mov, sorted(set(missing))

# ---------------------------------------------------------------------------------------------
# The shell: rooms, walls, holes (KitchenHouse.cpp BuildShell, KitchenBuilder.cpp BuildLivingRoom / BuildYard)
# ---------------------------------------------------------------------------------------------
H, B, GM, RUN, IA = 'KitchenHouse.cpp', 'KitchenBuilder.cpp', 'AntKitchenGameMode.cpp', 'AntRun.cpp', 'KitchenInteractables.cpp'

def build_areas():
    areas = [
        {'id': 'kitchen', 'ru': 'Кухня', 'rects': [[-7000, -5000, 7000, 5000]], 'floor_z': 0, 'ceiling_z': 5400,
         'src': ref(H, 'Solid(FBox(FVector(-7000.f, -5000.f, -100.f), FVector(7000.f, 5000.f, 0.f)), { Pit }')},
        {'id': 'living', 'ru': 'Гостиная', 'rects': [[7200, -9000, 20200, 1500]], 'floor_z': 0, 'ceiling_z': 5400,
         'src': ref(B, 'Box(FVector(7000.f, -9200.f, -100.f), FVector(20400.f, 1700.f, 0.f), Hex(196, 152, 104));')},
        {'id': 'yard', 'ru': 'Двор', 'rects': [[-12000, -16000, 7000, -5200], [7000, -16000, 21000, -9200]], 'floor_z': 0,
         'src': ref(B, 'Box(FVector(-12000.f, -16000.f, -200.f), FVector(7000.f, -5200.f, 0.f), Grass);')},
        {'id': 'street', 'ru': 'Улица', 'rects': [[-16500, -5200, -7200, 5600]], 'floor_z': 0,
         'parts': [{'ru': 'газон с гнездом', 'rect': [-9400, -5200, -7200, 5600], 'z': 0, 'src': ref(H, 'Box(FVector(-9400.f, -5200.f, -200.f), FVector(-7200.f, 5600.f, 0.f), Grass);')},
                   {'ru': 'тротуар', 'rect': [-11000, -5200, -9400, 5600], 'z': 0, 'src': 'KitchenArtData.inl (SM_S_PavementSlab_A/B)'},
                   {'ru': 'бордюр', 'rect': [-11100, -5200, -11000, 5600], 'z': 0, 'src': 'KitchenArtData.inl (SM_S_Kerb)'},
                   {'ru': 'дорога', 'rect': [-16500, -5200, -11100, 6000], 'z': -225, 'src': 'KitchenArtData.inl (SM_S_Road)'}],
         'src': ref(H, 'void AKitchenBuilder::BuildWestStreet()')},
    ]
    walls = [
        {'ru': 'северная стена кухни (толстая, в ней комната мыши)', 'rect': [-7200, 5000, 7200, 5600], 'z': [0, 5400], 'src': ref(H, 'Solid(FBox(FVector(-7200.f, 5000.f, 0.f), FVector(3500.f, 5600.f, 5400.f)), { N1, R1, G2 }, WallC);')},
        {'ru': 'южная стена кухни (окно)', 'rect': [-7200, -5200, 7200, -5000], 'z': [0, 5400], 'src': ref(H, 'Solid(FBox(FVector(-7200.f, -5200.f, 0.f), FVector(-1500.f, -5000.f, 5400.f)), { S1, G1, Socket, WindowCrack }, WallC);')},
        {'ru': 'западная стена кухни (щель в гнездо)', 'rect': [-7200, -5000, -7000, 5000], 'z': [0, 5400], 'src': ref(H, 'Solid(FBox(FVector(-7200.f, -5000.f, 0.f), FVector(-7000.f, 5000.f, 5400.f)), { NestCrack }, WallC);')},
        {'ru': 'восточная стена кухни (дверь в гостиную)', 'rect': [7000, -5000, 7200, 5000], 'z': [0, 5400], 'src': ref(H, 'Solid(FBox(FVector(7000.f, -3200.f, 0.f), FVector(7200.f, 5000.f, 5400.f)), { S2, G3 }, WallC);')},
        {'ru': 'северная стена гостиной', 'rect': [7200, 1500, 20400, 1700], 'z': [0, 5400], 'src': ref(B, 'Box(FVector(7200.f, 1500.f, 0.f), FVector(20400.f, 1700.f, 5400.f), WallC);')},
        {'ru': 'южная стена гостиной', 'rect': [7000, -9200, 20400, -9000], 'z': [0, 5400], 'src': ref(B, 'Box(FVector(7000.f, -9200.f, 0.f), FVector(20400.f, -9000.f, 5400.f), WallC);')},
        {'ru': 'восточная стена гостиной', 'rect': [20200, -9000, 20400, 1500], 'z': [0, 5400], 'src': ref(B, 'Box(FVector(20200.f, -9000.f, 0.f), FVector(20400.f, 1500.f, 5400.f), WallC);')},
        {'ru': 'западная стена гостиной (ниже кухни)', 'rect': [7000, -9000, 7200, -5000], 'z': [0, 5400], 'src': ref(B, 'Box(FVector(7000.f, -9000.f, 0.f), FVector(7200.f, -5000.f, 5400.f), WallC);')},
    ]
    openings = [
        {'id': 'nest_crack', 'ru': 'Щель под плинтусом в гнездо', 'box': [-7250, -2750, 0, -6950, -2250, 250], 'links': ['kitchen', 'street'], 'src': ref(H, 'const FBox NestCrack(FVector(-7250.f, -2750.f, 0.f), FVector(-6950.f, -2250.f, 250.f));')},
        {'id': 'window_crack', 'ru': 'Щель под окном во двор', 'box': [-4600, -5250, 0, -4400, -4950, 120], 'links': ['kitchen', 'yard'], 'src': ref(H, 'const FBox WindowCrack(FVector(-4600.f, -5250.f, 0.f), FVector(-4400.f, -4950.f, 120.f));')},
        {'id': 'window', 'ru': 'Окно без стекла (проём)', 'box': [-1500, -5200, 2200, 1500, -5000, 4200], 'links': ['kitchen', 'yard'], 'src': ref(H, 'Box(FVector(-1500.f, -5200.f, 0.f), FVector(1500.f, -5000.f, 2200.f), WallC);')},
        {'id': 'doorway', 'ru': 'Дверной проём в гостиную', 'box': [7000, -4800, 0, 7200, -3200, 3800], 'links': ['kitchen', 'living'], 'src': ref(H, 'Box(FVector(7000.f, -4800.f, 3800.f), FVector(7200.f, -3200.f, 5400.f), WallC);')},
        {'id': 'mouse_hole', 'ru': 'Мышиная нора за холодильником', 'box': [3800, 5000, 0, 3960, 5050, 140], 'links': ['kitchen', 'mouse_room'], 'src': ref(H, 'Box(FVector(3800.f, 5000.f, 140.f), FVector(3960.f, 5050.f, 450.f), WallC);')},
        {'id': 'pit', 'ru': 'Яма под шатающейся плиткой', 'box': [4700, 1700, -100, 5300, 2300, 1], 'links': ['kitchen', 'pit'], 'src': ref(H, 'const FBox Pit(FVector(4700.f, 1700.f, -100.f), FVector(5300.f, 2300.f, 1.f));')},
    ]
    spaces = [   # hidden rooms inside the shell
        {'id': 'mouse_room', 'ru': 'Комната мыши в стене', 'box': [3500, 5050, 0, 4300, 5550, 450], 'tier': 'vent', 'src': ref(H, 'Box(FVector(3500.f, 5000.f, -100.f), FVector(4300.f, 5600.f, 0.f), Hex(120, 95, 70));')},
        {'id': 'pit', 'ru': 'Яма под плиткой у мусорки', 'box': [4700, 1700, -100, 5300, 2300, 0], 'tier': 'vent', 'src': ref(H, 'Box(FVector(4700.f, 1700.f, -400.f), FVector(5300.f, 2300.f, -100.f), DarkDirt);'),
         'note': 'пол ямы — верх коробки z=-100 (коробка занимает -400..-100): глубина ямы 100 uu, а не 400'},
        {'id': 'corridor', 'ru': 'Коридор за нижними шкафами', 'box': [-7000, 4750, 0, 2500, 5000, 1650], 'tier': 'vent', 'src': ref(B, 'const float Back = 4750.f;   // back face of the cabinets')},
        {'id': 'duct1', 'ru': 'Вентиляция 1: юг, пол -> над верхними шкафами', 'tier': 'vent', 'src': ref(H, 'const FBox S1(FVector(-3300.f, -5180.f, 80.f), FVector(-2900.f, -5020.f, 5580.f));'),
         'boxes': {'S1': [-3300, -5180, 80, -2900, -5020, 5580], 'G1': [-3300, -5020, 80, -2900, -4990, 380], 'Socket': [-3200, -5020, 560, -3000, -4990, 660],
                   'D1': [-3300, -5180, 5420, -2900, 5180, 5580], 'N1': [-3300, 5020, 4480, -2900, 5180, 5580], 'R1': [-4700, 5020, 4480, -2900, 5180, 4640], 'G2': [-4700, 4990, 4480, -4300, 5020, 4640]}},
        {'id': 'duct2', 'ru': 'Вентиляция 2: восток, пол -> над холодильником', 'tier': 'vent', 'src': ref(H, 'const FBox S2(FVector(7020.f, 200.f, 80.f), FVector(7180.f, 600.f, 5580.f));'),
         'boxes': {'S2': [7020, 200, 80, 7180, 600, 5580], 'G3': [6990, 200, 80, 7020, 600, 380], 'D2a': [3500, 200, 5420, 7180, 600, 5580], 'D2b': [3500, 200, 5420, 3900, 5180, 5580],
                   'N2': [3500, 5020, 3700, 3900, 5180, 5580], 'G4': [3500, 4990, 3700, 3900, 5020, 3860]}},
    ]
    return areas, walls, openings, spaces

# ---------------------------------------------------------------------------------------------
# Loot spots: AKitchenBuilder::LootSpots, replayed (the Moved table, the two seeded scatters)
# ---------------------------------------------------------------------------------------------
def f32(v): return struct.unpack('<f', struct.pack('<f', v))[0]

class RandomStream:
    """FRandomStream: Seed = Seed * 196314165 + 907633515; fraction from the top 23 bits."""
    def __init__(self, seed): self.s = seed & 0xffffffff
    def frand(self):
        self.s = (self.s * 196314165 + 907633515) & 0xffffffff
        return struct.unpack('<f', struct.pack('<I', 0x3F800000 | (self.s >> 9)))[0] - 1.0
    def frange(self, a, b): return f32(a + (b - a) * self.frand())

def parse_loot_spots():
    t = text(B)
    a = t.find('void AKitchenBuilder::LootSpots'); b = t.find('// Geometry helpers', a)
    body = t[a:b]; base = t.count('\n', 0, a) + 1
    moved = []
    for g in re.finditer(r'\{\s*' + r',\s*'.join([NUM] * 6) + r'\s*\},?[ \t]*(?://[ \t]*(.*))?', body[:body.find('for (const auto& M : Moved)')]):
        f = [float(v) for v in g.groups()[:6]]
        moved.append((f[:3], f[3:], (g.group(7) or '').strip()))
    spots = []
    def add(x, y, z, tier, label, group, line, how):
        was = None
        for (p0, p1, why) in moved:
            if [x, y, z] == p0:
                was = p0; x, y, z = p1
                label = (label + '; ' if label else '') + 'сдвинута таблицей Moved' + (': ' + why if why else '')
                break
        s = {'i': len(spots), 'p': P(x, y, z), 'tier_loot': tier, 'group': group, 'label': label, 'src': '%s:%d' % (B, line), 'how': how}
        if was: s['before_move'] = P(*was)
        spots.append(s)
    def d2(p, q): return math.hypot(p[0] - q[0], p[1] - q[1])
    for nd in ('FRandomStream R(1337);', 'Placed < 36 && Guard < 500', 'R.FRandRange(-6000.f, 6500.f), R.FRandRange(-4600.f, 2700.f)', 'NestLocation(0)) < 1100.f',
               'NestLocation(1)) < 1000.f', 'FVector(6000.f, -1500.f, 0.f)) < 800.f', 'FVector(6000.f, -2700.f, 0.f)) < 750.f', 'FVector(6200.f, 3900.f, 0.f)) < 550.f',
               'FVector(-3800.f, -3800.f, 0.f)) < 560.f', 'FVector(-2500.f, -3900.f, 0.f)) < 500.f', 'P.X > -5850.f && P.X < -4550.f && P.Y > 450.f && P.Y < 1350.f',
               'FRandomStream R2(4242);', 'for (int32 i = 0; i < 16; ++i)   // crumbs on the wooden floor and the rug', 'i % 5 == 0 ? 1 : 0'):
        ref(B, nd)
    group = ''
    RNG = 'seeded scatter: FRandomStream re-implemented, arguments drawn Y first then X (MSVC evaluates call arguments right to left); checked against the ARTPROBE log'
    for k, ln in enumerate(body.split('\n')):
        s = ln.strip(); line = base + k
        if s.startswith('//'):
            group = s.strip('/ -').strip(); continue
        if 'Add(P.X, P.Y, 0.f, 0);' in s:
            R = RandomStream(1337); placed = 0; guard = 0
            while placed < 36 and guard < 500:
                guard += 1
                y = R.frange(-4600.0, 2700.0); x = R.frange(-6000.0, 6500.0)
                p = (x, y)
                if d2(p, (-7900, -2500)) < 1100 or d2(p, (6300, 1250)) < 1000 or d2(p, (6000, -1500)) < 800 or d2(p, (6000, -2700)) < 750: continue
                if d2(p, (6200, 3900)) < 550 or d2(p, (-3800, -3800)) < 560 or d2(p, (-2500, -3900)) < 500: continue
                if -5850 < x < -4550 and 450 < y < 1350: continue
                add(x, y, 0.0, 0, 'крошка на полу кухни (сид 1337)', 'Floor crumbs', line, RNG); placed += 1
            continue
        if 'R2.FRandRange(8000.f, 19000.f)' in s:
            R = RandomStream(4242)
            for i in range(16):
                y = R.frange(-8300.0, -700.0); x = R.frange(8000.0, 19000.0)
                add(x, y, 0.0, 1 if i % 5 == 0 else 0, 'крошка на полу гостиной (сид 4242)', 'Living room', line, RNG)
            continue
        cm = s.split('//', 1)[1].strip() if '//' in s else ''
        for g in re.finditer(r'Add\(' + NUM + r',\s*' + NUM + r',\s*' + NUM + r',\s*(\d)\)', s):
            add(float(g.group(1)), float(g.group(2)), float(g.group(3)), int(g.group(4)), cm, group, line, 'literal in the code')
    return spots, moved

def read_probe():
    """Saved/art/run_last.txt: the game's own -AntArtProbe log (what lies above / below every loot spot). Optional."""
    p = os.path.join(ROOT, 'Saved', 'art', 'run_last.txt')
    if not os.path.exists(p): return None, None
    t = open(p, encoding='utf-8-sig', errors='replace').read()
    rx = re.compile(r'ARTPROBE (\d+) \((-?\d+),(-?\d+),(-?\d+)\) surface (-?\d+) \| from above: (\S+) (-?\d+) \| from the spot: (\S+) (-?\d+) \| ceiling: (\S+) (-?\d+)')
    rows = {}
    for g in rx.finditer(t):
        g = g.groups()
        rows[int(g[0])] = {'p': [int(g[1]), int(g[2]), int(g[3])], 'above': [g[5], int(g[6])], 'below': [g[7], int(g[8])], 'ceil': [g[9], int(g[10])]}
    stamp = re.search(r'\[(\d{4}\.\d\d\.\d\d-\d\d\.\d\d\.\d\d)', t)
    return rows, (stamp.group(1) if stamp else '?')

def parse_table(name, start, end, rx):
    t = text(name)
    a = t.find(start); b = t.find(end, a) if end else len(t)
    if a < 0 or b < 0:
        STALE.append({'file': name, 'needle': start}); return []
    base = t.count('\n', 0, a) + 1
    out = []
    for k, ln in enumerate(t[a:b].split('\n')):
        for g in re.finditer(rx, ln):
            cm = ln.split('//', 1)[1].strip() if '//' in ln else ''
            out.append((g.groups(), cm, '%s:%d' % (name, base + k)))
    return out

FV = r'FVector\(' + NUM + r',\s*' + NUM + r',\s*' + NUM + r'\)'
CONTAINER_IDS = ['Fridge', 'Freezer', 'Oven', 'Microwave', 'Dishwasher', 'TopDrawer', 'Drawer2', 'CabA', 'CabB', 'CabC', 'Wall0', 'Wall1', 'Wall2', 'Wall3', 'WallE', 'TVStand']

def parse_containers(objs):
    rows = parse_table(RUN, 'static const FDef Defs[Count] =\n\t{\n\t\t{ TEXT("в холодильнике")', 'const FDef& Get(int32 Index)',
                       r'\{ TEXT\("([^"]+)"\),\s*' + FV + r',\s*' + FV + r',\s*(\(int32\)EKitchenDoorKind::(\w+)|-1),\s*\{ (-?\d+), (-?\d+) \},\s*(true|false) \}')
    by_part = {o['mover']['part']: o for o in objs if 'mover' in o}
    out = []
    for n, (g, cm, src) in enumerate(rows):
        f = [float(v) for v in g[1:7]]
        c = {'id': CONTAINER_IDS[n] if n < len(CONTAINER_IDS) else 'C%d' % n, 'n': n, 'where_ru': g[0], 'box': [r1(v) for v in f], 'trap': g[11] == 'true', 'src': src, 'doors': []}
        if g[8]:
            kind = g[8]
            c['doors'].append({'actor': 'AKitchenDoor(%s)' % kind, 'door_class': 'light' if kind == 'Drawer' else ('medium' if kind == 'Microwave' else 'heavy')})
        for part in (int(g[9]), int(g[10])):
            if part >= 0 and part in by_part:
                o = by_part[part]
                c['doors'].append({'actor': 'AKitchenMover #%d' % part, 'object': o['id'], 'ru': o['ru'], 'door_class': o['mover']['door_class']})
        c['need_strength'] = min([{'light': 6, 'medium': 24, 'heavy': 40}[d['door_class']] for d in c['doors']] or [0])
        out.append(c)
    return out

# ---------------------------------------------------------------------------------------------
# Food tables, trophies, heavy prizes, loot behind doors, props
# ---------------------------------------------------------------------------------------------
def parse_food():
    T = 'AntTypes.cpp'
    t = text(T)
    a = t.find('FAntFoodDef RollFood(int32 Tier, FRandomStream& R)'); b = t.find('FAntFoodDef HeavyFood(int32 Index)', a)
    blocks = t[a:b].split('switch (R.RandRange(')[1:]
    tiers = []
    for n, blk in enumerate(blocks):
        items = [{'ru': g[0], 'value': int(g[1]), 'weight': int(g[2])} for g in re.findall(r'MakeFood\(TEXT\("([^"]+)"\), (\d+), (\d+),', blk)]
        tiers.append({'tier': n, 'items': items, 'mean_value': round(sum(i['value'] for i in items) / max(1, len(items)), 1),
                      'mean_weight': round(sum(i['weight'] for i in items) / max(1, len(items)), 1)})
    for n, tr in enumerate(tiers):   # RollTier: 10 % of the rolls are one tier better
        up = tiers[min(n + 1, len(tiers) - 1)]
        tr['expected_value'] = round(0.9 * tr['mean_value'] + 0.1 * up['mean_value'], 1)
    hv = t[b:t.find('float WeaponDps', b)]
    heavy = [{'ru': g[0], 'value': int(g[1]), 'weight': int(g[2])} for g in re.findall(r'MakeFood\(TEXT\("([^"]+)"\), (\d+), (\d+),', hv)]
    return {'tiers': tiers, 'heavy_cycle': heavy, 'src': ref(T, 'FAntFoodDef RollFood(int32 Tier, FRandomStream& R)'),
            'roll_tier_src': ref(GM, 'return FMath::Clamp(Tier + (Rng.FRand() < 0.1f ? 1 : 0), 0, 3);')}

def parse_loot_extras(food):
    out = {}
    hv = parse_table(B, 'void AKitchenBuilder::HeavySpots', 'FVector AKitchenBuilder::HumanSpawn()', r'Out\.Add\(' + FV + r'\);')
    out['heavy'] = []
    for n, (g, cm, src) in enumerate(hv):
        it = food['heavy_cycle'][n % 4] if food['heavy_cycle'] else {}
        out['heavy'].append({'i': n, 'p': P(float(g[0]), float(g[1]), float(g[2])), 'spawn_z': float(g[2]) + 40, 'label': cm, 'first_item': it, 'src': src})
    tr = parse_table(GM, 'static const FTrophy Defs[] =', 'constexpr int32 NumDefs',
                     r'\{ TEXT\("([^"]+)"\), (\d+), (\d+), FLinearColor\([^)]*\), EAntShape::\w+, FVector\([^)]*\), ' + FV + r' \}')
    out['trophies'] = [{'i': n, 'ru': g[0], 'value': int(g[1]), 'value_whole_x2': int(g[1]) * 2, 'weight': int(g[2]), 'p': P(float(g[3]), float(g[4]), float(g[5])),
                        'label': cm, 'src': src} for n, (g, cm, src) in enumerate(tr)]
    cl = parse_table(RUN, 'void AAntKitchenGameMode::SpawnContainerLoot()', 'int32 N = 0;', r'\{ ' + FV + r', (\d) \}')
    out['behind_doors'] = [{'i': n, 'p': P(float(g[0]), float(g[1]), float(g[2])), 'tier_loot': int(g[3]), 'label': cm, 'src': src} for n, (g, cm, src) in enumerate(cl)]
    hp = parse_table(RUN, 'void AAntKitchenGameMode::SpawnHighProps()', 'for (const FDef& D : Defs)',
                     r'\{ TEXT\("([^"]+)"\),\s*EAntShape::(\w+),\s*' + FV + r',\s*' + FV + r',\s*FLinearColor\([^)]*\),\s*' + NUM + r' \}')
    out['high_props'] = [{'ru': g[0], 'shape': g[1], 'p': P(float(g[2]), float(g[3]), float(g[4])), 'size': [float(g[5]), float(g[6]), float(g[7])], 'mass': float(g[8]), 'src': src}
                         for (g, cm, src) in hp]
    jp = parse_table(GM, 'const FPropDef Props[] =', 'for (const FPropDef& D : Props)', r'\{ EAntShape::(\w+), ' + FV + r', FVector\(([^)]*)\), FLinearColor\([^)]*\), ' + NUM + r' \}')
    out['junk_props'] = [{'shape': g[0], 'p': P(float(g[1]), float(g[2]), float(g[3])), 'size': g[4], 'mass': float(g[5]), 'label': cm, 'src': src} for (g, cm, src) in jp]
    return out

def parse_pets():
    rows = parse_table('Critters.cpp', 'void ACritter::BeginPlay()', 'void ACritter::BuildVisual()', r'\{ ' + FV + r', (\d), ' + NUM + r' \}')
    t = text('Critters.cpp'); a = t.find('void ACritter::BeginPlay()'); e = t.find('\t\telse\n', a); else_line = t.count('\n', 0, e) + 1
    dog, cat = [], []
    for (g, cm, src) in rows:
        s = {'p': P(float(g[0]), float(g[1]), float(g[2])), 'does': ['sleep', 'sniff_or_eat', 'sit'][int(g[3])], 'seconds': float(g[4]), 'label': cm, 'src': src}
        (dog if int(src.split(':')[1]) < else_line else cat).append(s)
    return dog, cat

def parse_events():
    rows = parse_table(RUN, 'namespace AntEvent', 'namespace AntPing', r'\{ TEXT\("([^"]*)"\),\s*TEXT\("([^"]*)"\), ' + NUM + r' \}')
    return [{'id': n, 'ru': g[0], 'text_ru': g[1], 'seconds': float(g[2]), 'src': src} for n, (g, cm, src) in enumerate(rows) if n > 0]

def inside(p, box):
    return box[0] < p[0] < box[3] and box[1] < p[1] < box[4] and box[2] < p[2] < box[5]

def classify_loot(spots, objs, containers, spaces, interiors, food):
    gaps = [o for o in objs if 'gap_under' in o]
    ev = {t['tier']: t['expected_value'] for t in food['tiers']}
    for s in spots:
        x, y, z = s['p']; pc = (x, y, z + 35.0)
        s['area'] = area_of(x, y)
        s['ev'] = ev.get(s['tier_loot'], 0)
        tier = None
        for sp in spaces:
            for bx in ([sp['box']] if 'box' in sp else list(sp['boxes'].values())):
                if inside(pc, bx) or (sp['id'] == 'pit' and bx[0] < x < bx[3] and bx[1] < y < bx[4] and z < 0):
                    tier = 'vent'; s['space'] = sp['id']
        if not tier:
            for c in containers:
                if inside(pc, c['box']):
                    tier = 'int'; s['container'] = c['id']; s['behind_door'] = True; s['where_ru'] = c['where_ru']; s['need_strength'] = c['need_strength']
                    break
        if not tier:
            for it in interiors:
                if inside(pc, it['box']):
                    tier = 'int'; s['space'] = it['id']; s['where_ru'] = it['ru']; break
        if not tier:
            if z >= 2100: tier = 'top'
            elif z >= 300: tier = 'work'
            else:
                tier = 'floor'
                for o in gaps:
                    if o['bb'][0] < x < o['bb'][2] and o['bb'][1] < y < o['bb'][3] and z < o['gap_under']:
                        tier = 'under'; s['under'] = o['ru']; s['gap_uu'] = o['gap_under']; break
        s['tier'] = tier
    return spots

# ---------------------------------------------------------------------------------------------
# Hand-transcribed parts (each with a needle)
# ---------------------------------------------------------------------------------------------
BH, AC, AM, AT, HC, HA, CR, KG, KE, KL = 'KitchenBuilder.h', 'AntCharacter.cpp', 'AntMovementComponent.cpp', 'AntTypes.h', 'HumanCharacter.cpp', 'HumanAIController.cpp', 'Critters.cpp', 'KitchenGadgets.cpp', 'KitchenEvents.cpp', 'KitchenLoot.cpp'
HH, IH, ACH, RH, KEH = 'HumanCharacter.h', 'KitchenInteractables.h', 'AntCharacter.h', 'AntRun.h', 'KitchenEvents.h'

def build_constants(food):
    walk, sprint, speedk = 480.0, 760.0, 0.6
    c = {
        'uu_per_m': UU, 'scale_src': 'KitchenBuilder.h: "the Human is ~3400 units tall, so 1 m of real kitchen ~ 1900 units"',
        'ant': {
            'real_length_mm': 8, 'length_uu': round(0.008 * UU, 1), 'body_lengths_per_m': round(1.0 / 0.008, 1),
            'length_src': ref(AT, 'The ant is ~8 mm'), 'collision_sphere_radius_uu': round(26 * 0.48 * 0.33, 2), 'collision_src': ref(ACH, 'static constexpr float BodyRadius = 26.f * AntScale;'),
            'walk_uu_s': walk * speedk, 'sprint_uu_s': sprint * speedk, 'wall_factor': 0.82, 'ceiling_factor': 0.7,
            'carry_factor': '0.5..0.9 of the walk speed when the load is carried (strength >= weight), no sprint, no jump; dragging (strength < weight): 0.05..0.43 and no walls',
            'speed_src': ref(AC, 'M->MaxWalkSpeed = (bSprinting && !Carrying ? 760.f : 480.f) * SpeedK * Load * ClassSpeed;'),
            'speedk_src': ref(AT, 'constexpr float SpeedK = 0.6f;'), 'wall_src': ref(AM, 'Base *= (Up < -0.5f) ? 0.7f : 0.82f;'),
            'sprint_key': 'LeftShift (Config/DefaultInput.ini, ActionName="Sprint")',
            'classes': [{'ru': 'Фуражир', 'strength': 14, 'health': 100, 'speed': 1.0}, {'ru': 'Солдат', 'strength': 20, 'health': 170, 'speed': 0.85},
                        {'ru': 'Разведчик', 'strength': 7, 'health': 70, 'speed': 1.35}, {'ru': 'Бочка', 'strength': 10, 'health': 110, 'speed': 0.95}],
            'classes_src': ref('AntTypes.cpp', 'Defs[0] = { TEXT("Фуражир"), 14.f, 100.f, 1.0f, 620.f, 1.0f, 1.0f, 0.f, 0.f };'),
            'nest_safe_src': ref(AC, 'return FVector::Dist2D(L, AKitchenBuilder::NestLocation(Team)) < AKitchenBuilder::NestRadius(Team) && L.Z < 400.f;'),
        },
        'door_strength': {'light': 6, 'medium': 24, 'heavy': 40, 'src': ref(RH, 'inline float Need(int32 Class) { return Class >= Heavy ? 40.f : Class == Medium ? 24.f : Class == Light ? 6.f : 0.f; }')},
        'food_breaks_above_impact_uu_s': 1250, 'food_break_src': ref(KL, 'const float BreakSpeed = (bTrophy && AntCard::Has(this, AntCard::Padding)) ? 1900.f : 1250.f;'),
        'shift': {'seconds': 480, 'human_wakes_at_s': 75, 'quota': '300 + 120 per extra player, x1.3 every shift', 'ally_bots': 1, 'rival_bots': 2,
                  'src': [ref(GM, 'MatchLength = 480.f;'), ref(GM, 'float Quiet = 75.f + (HasCard(AntCard::Phone) ? 30.f : 0.f);'),
                          ref(GM, 'int32 Quota = FMath::RoundToInt((300.f + 120.f * (Players - 1)) * FMath::Pow(1.3f, (float)(ShiftNumber - 1)) / 10.f) * 10;'), ref(GM, 'AllyBots = 1;'), ref(GM, 'RivalBots = 2;')]},
    }
    return c

def build_human():
    return {
        'spawn': P(3800, -3200, 1720), 'spawn_src': ref(B, 'FVector AKitchenBuilder::HumanSpawn() { return FVector(3800.f, -3200.f, 1720.f); }'),
        'flee_point': P(5600, -4200, 1720), 'flee_src': ref(B, 'FVector AKitchenBuilder::HumanFleePoint() { return FVector(5600.f, -4200.f, 1720.f); }'),
        'capsule': {'radius': 340, 'half_height': 1700, 'step_height': 250, 'src': [ref(HC, 'GetCapsuleComponent()->InitCapsuleSize(340.f, HalfHeight);'), ref(HC, 'const float HalfHeight = 1700.f;'), ref(HC, 'CM->MaxStepHeight = 250.f;')]},
        'walk_uu_s': '720 * (1 + 0.12 * rage)', 'walk_src': ref(HH, 'float GetWalkSpeed() const { return bStunned ? 1.f : bKnockedOut ? 260.f : 720.f * (1.f + 0.12f * Rage); }'),
        'eye_height_assumed': 3150,
        'patrol': {'kind': 'no patrol points: he walks to random floor points (60 % kitchen box, 40 % living-room box), a new one every 6 s or on arrival',
                   'kitchen_box': [-6000, -4300, 6200, 2550], 'living_box': [8000, -8300, 19000, -500], 'door_waypoints': [P(6500, -4000), P(7700, -4000)],
                   'src': [ref(HA, 'WanderPoint = AKitchenBuilder::RandomFloorPoint() + FVector(0.f, 0.f, MyLoc.Z);'), ref(B, 'FBox AKitchenBuilder::PlayArea() { return FBox(FVector(-6000.f, -4300.f, 0.f), FVector(6200.f, 2550.f, 0.f)); }'),
                           ref(B, 'return FVector(R(8000.f, 19000.f), R(-8300.f, -500.f), 0.f);'), ref(B, 'return FVector(bFromKitchen ? 6500.f : 7700.f, D.Y, Z);')]},
        'outside_targets': {'note': 'PickTarget takes any ant within 8000 uu (2D) that is not in its nest, also in the yard and the street. Route() then sends him to the crack waypoints he cannot pass: he ends up pressed to the wall there.',
                            'crack_path': [P(-6700, -2500), P(-7500, -2500)], 'window_path': [P(-4500, -4800), P(-4500, -5400), P(-4500, -5800)],
                            'src': [ref(B, 'static const FVector Out[] = { FVector(-6700.f, -2500.f, 0.f), FVector(-7500.f, -2500.f, 0.f) };'),
                                    ref(B, 'static const FVector Out[] = { FVector(-4500.f, -4800.f, 0.f), FVector(-4500.f, -5400.f, 0.f), FVector(-4500.f, -5800.f, 0.f) };'), ref(HA, 'float BestScore = 8000.f * Notice;')]},
        'sees': {'notice_radius_2d': 8000, 'carrying_bonus': -2500, 'above_300_penalty': 800, 'dark_radius': 2200, 'dark_radius_carrying': 3200, 'line_of_sight': 'none (PickTarget does no trace)',
                 'never_targets': [
                     {'ru': 'всё севернее y=4750 на любой высоте (коридор за шкафами, полочки специй, рейлинг, вентили крана, задняя полоса верха шкафов)', 'rect': [-7000, 4750, 7000, 5600], 'z': [0, 5400], 'src': ref(HA, 'if (AL.Y > 4750.f || FBox(FVector(2700.f, 3000.f, 0.f), FVector(4700.f, 4750.f, 3700.f)).IsInside(AL)')},
                     {'ru': 'объём холодильника вместе с щелью под ним', 'rect': [2700, 3000, 4700, 4750], 'z': [0, 3700], 'src': ref(HA, 'FBox(FVector(2700.f, 3000.f, 0.f), FVector(4700.f, 4750.f, 3700.f)).IsInside(AL)')},
                     {'ru': 'духовка', 'rect': [-1500, 3000, 0, 4750], 'z': [120, 1650], 'src': ref(HA, '|| FBox(FVector(-1500.f, 3000.f, 120.f), FVector(0.f, 4750.f, 1650.f)).IsInside(AL)')},
                     {'ru': 'шкаф под мойкой', 'rect': [-4200, 3000, -2800, 4750], 'z': [0, 1100], 'src': ref(HA, '|| FBox(FVector(-4200.f, 3000.f, 0.f), FVector(-2800.f, 4750.f, 1100.f)).IsInside(AL))')},
                     {'ru': 'любой закрытый контейнер (KitchenContainers::SealedAt) и своё гнездо', 'src': ref(HA, 'if (KitchenContainers::SealedAt(GetWorld(), AL) >= 0)')}],
                 'src': [ref(HA, 'float BestScore = 8000.f * Notice;'), ref(HA, 'if (bDark && Score > (Ant->IsCarrying() ? 3200.f : 2200.f) * Notice)')]},
        'attacks': {
            'stomp': {'ru': 'топот тапком', 'point': '480 uu ahead of the capsule centre, 180 to the side, on his floor', 'kill_radius': 420, 'kill_height_above_floor': 220, 'shock_ring_to': 1092,
                      'ai_uses_within_2d': 760, 'reach_from_centre': 900, 'wind_up_s': 0.55, 'cooldown_s': '1.05 * (1 - 0.18 * rage)', 'damage': 200,
                      'src': [ref(HH, 'static constexpr float StompRadius = 420.f;'), ref(HC, 'return GetActorLocation() + GetActorForwardVector() * 480.f + GetActorRightVector() * (Leg == 0 ? -180.f : 180.f) - FVector(0.f, 0.f, HalfHeight);'),
                              ref(HC, 'if (D2 < StompRadius && A.Z - P.Z < 220.f)'), ref(HC, 'else if (D2 < StompRadius * 2.6f && FMath::Abs(A.Z - P.Z) < 260.f && !Ant->IsAttached())'), ref(HA, 'if (Dist < 760.f && YawErr < 30.f)')]},
            'slap': {'ru': 'шлепок ладонью по полу', 'targets_below_z': 260, 'ai_uses_between_2d': [760, 1250], 'max_from_centre': 1350, 'hit_radius': 300, 'damage': 150, 'not_more_often_than_s': 6, 'line_of_sight': 'none',
                     'src': [ref(HC, 'if (Flat.Size() > 1350.f)'), ref(HA, 'Dist >= 760.f && Dist < 1250.f'), ref(HH, 'static constexpr float SwatRadius = 300.f;')]},
            'newspaper': {'ru': 'газета (стены, столы, верх)', 'from': 'shoulder = capsule centre + 960 (2660 above his floor)', 'range_from_shoulder': 2500, 'hit_radius': 300, 'damage': 150, 'cooldown_s': '2.6 * (1 - 0.15 * rage)',
                          'ai_uses_within_3d_of_chest': 3200, 'line_of_sight': 'none', 'reach_formula': 'point (d, z) is hit when sqrt(d^2 + (z - 2660)^2) <= 2800, d = horizontal distance to where he can stand',
                          'src': [ref(HC, 'const FVector Shoulder = GetActorLocation() + FVector(0.f, 0.f, 960.f);'), ref(HC, 'if (FVector::Dist(Point, Shoulder) > 2500.f)'), ref(HH, 'static constexpr float SwatRange = 3200.f;'),
                                  ref(HA, 'FVector::Dist(TL, MyLoc + FVector(0.f, 0.f, 600.f)) < AHumanCharacter::SwatRange')]},
            'spray': {'ru': 'дихлофос', 'range': '2300 * (1 + 0.25 * rage): 2300 / 2875 / 3450 / 4025', 'cone_half_angle_deg': 20, 'pitch_deg': [-70, 15], 'needs_line_of_sight': True, 'damage_per_s': '60 * (1 + 0.3 * rage)',
                      'nozzle_height_assumed': 2510, 'max_height_estimate': 'about 2510 + range * sin(35 deg): 3830 (rage 0) .. 4820 (rage 3)',
                      'src': [ref(HH, 'static constexpr float SprayRange = 2300.f;'), ref(HH, 'float GetSprayRange() const { return SprayRange * (1.f + 0.25f * Rage); }'), ref(HC, 'const float CosCone = FMath::Cos(FMath::DegreesToRadians(20.f));'),
                              ref(HC, 'R.Pitch = FMath::ClampAngle((float)R.Pitch, -70.f, 15.f);')]},
            'grab': {'ru': 'отбирает груз', 'value_at_least': 50, 'within_2d': 1500, 'below_z': 2000, 'src': ref(HA, 'L->Value >= 50 && FVector::Dist2D(L->GetActorLocation(), MyLoc) < (H->HasJointedBody() ? 1500.f : 1800.f) && L->GetActorLocation().Z < 2000.f')}},
        'investigates': {'ru': 'идёт на самый громкий шум не старше 14 с, 5 с осматривается, выключает источник; тот же источник не работает 40 с; не отвлекается от муравья с грузом ближе 2600',
                         'stops_at_2d': 1150, 'src': [ref(HA, 'if (FVector::Dist2D(Noise.Loc, MyLoc) > 1150.f && TimeNow < NoiseGiveUp)'), ref(HA, 'LookUntil = TimeNow + 5.f;'), ref(HA, 'const bool bHotChase = T && T->IsCarrying() && TargetDist < 2600.f;')],
                         'loudness': {'микроволновка': 1.0, 'тостер': 0.9, 'телевизор': 0.8, 'мышеловка': 0.8, 'кран (если открыл муравей)': 0.65, 'грохот упавшей тяжёлой еды': '0.4..0.9'},
                         'loud_src': [ref(KE, 'if (GM) { GM->MakeNoise(GetActorLocation(), 1.f, AntNoise::Microwave, this); }'), ref(IA, 'NoiseGM->MakeNoise(FVector(-6000.f, 4200.f, 1900.f), 0.9f, AntNoise::Toaster, this);'),
                                      ref(KE, 'if (GM) { GM->MakeNoise(GetActorLocation(), 0.8f, AntNoise::TV, this); }'), ref(IA, 'NoiseGM->MakeNoise(GetActorLocation(), 0.8f, AntNoise::Trap, this, Caught[0]);'), ref(IA, 'GM->MakeNoise(Spout(), 0.65f, AntNoise::Faucet, this);')]},
        'shuts_doors': {'ru': 'закрывает любую дверь / ящик, открытые дольше 4 с, в своей комнате, не дальше 7000 uu; только когда до цели дальше 1500', 'src': [ref(HA, 'float BestD = 7000.f;   // most of the room he is in'), ref(HA, 'TimeNow - It->OpenedAt > 4.f')]},
        'kitchen_life': {'ru': 'каждые 38 с (первый раз на 25-й): сам собой открывается холодильник на 9 с, или кран на 14 с, или дверца нижнего шкафа / средний ящик на 20 с. Срабатывает и пока Человек "в телефоне".',
                         'src': ref(GM, 'GetWorldTimerManager().SetTimer(LifeTimer, this, &AAntKitchenGameMode::KitchenLifeEvent, 38.f, true, 25.f);')},
    }

def build_connectors(ob):
    C = []
    def add(cid, ru, kind, pts, ft, tt, src, note='', exact='code'):
        C.append({'id': cid, 'ru': ru, 'kind': kind, 'pts': [P(*p) for p in pts], 'from_tier': ft, 'to_tier': tt, 'area': area_of(pts[0][0], pts[0][1]),
                  'height_uu': r1(max(p[2] for p in pts) - min(p[2] for p in pts)), 'note': note, 'src': src, 'exact': exact})
    def up(cid, ru, mesh, idx, ft, tt, note='', kind='climb', at=None):
        o = ob(mesh, idx)
        if not o: return
        x, y = at or o['c']
        add(cid, ru, kind, [(x, y, max(0.0, o['z'][0])), (x, y, o['z'][1])], ft, tt, o['src'], note, 'art table')
    # between the areas
    add('nest_crack', 'Щель под плинтусом: кухня <-> газон с гнездом', 'opening', [(-6950, -2500, 0), (-7250, -2500, 0)], 'floor', 'floor', ref(H, 'const FBox NestCrack('),
        '500 x 250 uu. Радиус гнезда 950 от (-7900,-2500) кончается ровно на внутреннем крае щели: вошёл в щель = дома, груз сдан')
    add('window_crack', 'Щель под окном: кухня <-> двор', 'opening', [(-4500, -4950, 0), (-4500, -5250, 0)], 'floor', 'floor', ref(H, 'const FBox WindowCrack('), '200 x 120 uu')
    add('window', 'Окно без стекла: подоконник <-> внешний подоконник <-> газон', 'opening', [(0, -4910, 2200), (0, -5290, 2200), (0, -5390, 0)], 'top', 'floor',
        ref(H, 'Box(FVector(-1500.f, -5200.f, 0.f), FVector(1500.f, -5000.f, 2200.f), WallC);'), 'проём x -1500..1500, z 2200..4200; вниз по стене 2200')
    add('doorway', 'Дверной проём: кухня <-> гостиная', 'opening', [(6900, -4000, 0), (7300, -4000, 0)], 'floor', 'floor', ref(H, 'Box(FVector(7000.f, -4800.f, 3800.f), FVector(7200.f, -3200.f, 5400.f), WallC);'),
        'проём 1600 x 3800; дверь стоит открытой, под закрытой щель 23 uu (12 мм по манифесту); Человек и питомцы распахивают её сами')
    add('corner', 'Угол дома: газон с гнездом <-> двор', 'opening', [(-8300, -4700, 0), (-8300, -5700, 0)], 'floor', 'floor', ref(B, 'static const FVector Out[] = { FVector(-8300.f, -4700.f, 0.f), FVector(-8300.f, -5700.f, 0.f) };'))
    add('gate', 'Калитка: газон <-> тротуар (открыта с начала)', 'opening', [(-9250, 3990, 0), (-9550, 3990, 0)], 'floor', 'floor', (ob('SM_Y_FenceGate_Leaf') or {}).get('src', '?'), 'проём y 3340..4640 (манифест)', 'art table')
    add('yard_fence_gap', 'Разрыв в заборе двора: двор <-> тротуар', 'opening', [(-10400, -5050, 0), (-10400, -5350, 0)], 'floor', 'floor', ref(H, 'if (X > -10800.f && X < -10000.f) { continue; }'), 'x -10800..-10000')
    # ducts, pipes, holes
    add('duct1', 'Вентиляция 1: решётка у пола (юг) -> шахта -> потолок -> решётка над верхними шкафами', 'duct',
        [(-3100, -4990, 230), (-3100, -5100, 230), (-3100, -5100, 5500), (-3100, 5100, 5500), (-3100, 5100, 4560), (-4500, 5100, 4560), (-4500, 4990, 4560)], 'floor', 'top',
        ref(H, 'DuctWind.Add({ S1, FVector(0.f, 0.f, 1.f) });'), 'сквозняк несёт от пола вверх и на выход над шкафами; между ламелями ~50 uu')
    add('duct1_socket', 'Треснувшая розетка: второй вход в шахту вентиляции 1', 'duct', [(-3100, -4990, 610), (-3100, -5100, 610)], 'work', 'vent', ref(H, 'const FBox Socket('))
    add('duct2', 'Вентиляция 2: решётка у пола (восток) -> шахта -> потолок -> решётка над холодильником', 'duct',
        [(6990, 400, 230), (7100, 400, 230), (7100, 400, 5500), (3700, 400, 5500), (3700, 5100, 5500), (3700, 5100, 3780), (3700, 4990, 3780)], 'floor', 'top',
        ref(H, 'DuctWind.Add({ S2, FVector(0.f, 0.f, 1.f) });'), 'выход на стену в 230 uu за холодильником, на высоте его верха')
    add('drain', 'Слив раковины -> под мойку (телепорт, без груза)', 'teleport', [(-3500, 4000, 1150), (-3500, 3600, 200)], 'work', 'int', ref(GM, 'Passage(EPassageLook::Drain, FVector(-3500.f, 4000.f, 1150.f), 0.f, FVector(-3500.f, 3600.f, 200.f)'))
    add('pipe', 'Труба под мойкой -> в раковину (телепорт, без груза)', 'teleport', [(-3300, 4300, 140), (-3700, 3800, 1210)], 'int', 'work', ref(GM, 'Passage(EPassageLook::Pipe, FVector(-3300.f, 4300.f, 140.f), 0.f, FVector(-3700.f, 3800.f, 1210.f)'))
    add('mouse_hole', 'Мышиная нора -> комната мыши', 'gap', [(3880, 4990, 70), (3880, 5150, 70)], 'floor', 'vent', ref(H, 'Box(FVector(3800.f, 5000.f, 140.f), FVector(3960.f, 5050.f, 450.f), WallC);'), 'отверстие 160 x 140 uu в стене за холодильником')
    add('corridor', 'Коридор за нижними шкафами (y 4750..5000, под столешницей)', 'gap', [(2600, 3040, 0), (2600, 4875, 0), (-6900, 4875, 0)], 'floor', 'vent', ref(B, 'const float Back = 4750.f;   // back face of the cabinets'),
        'входы: щель 200 uu между шкафом (x=2500) и холодильником (x=2700); из-за холодильника; дыра в задней стенке духовки (x -800..-600, z 300..430); дыра в задней стенке шкафа под мойкой (x -3600..-3400, z 140..330)')
    add('freezer_gap', 'Лаз из холодильника в морозилку (сзади перегородки)', 'gap', [(3650, 4560, 2480), (3650, 4560, 2580)], 'int', 'int', (ob('SM_K_Fridge') or {}).get('src', '?'), 'x 3500..3800, y 4450..4670 (манифест SM_K_Fridge)', 'art table')
    add('pit', 'Под поднятым краем плитки -> яма', 'gap', [(5000, 2000, 20), (5000, 2000, -100)], 'floor', 'vent', ref(H, 'Part(EAntShape::Cube, FVector(5000.f, 2000.f, 110.f), FVector(1000.f, 1000.f, 20.f), FRotator(0.f, 0.f, 12.f), Hex(150, 60, 55), true);'))
    for k, (nm, ru) in enumerate([('SM_K_Base1400', 'у тостера'), ('SM_K_Base1300', 'слева от плиты'), ('SM_K_Base1200', 'под микроволновкой')]):
        o = ob(nm)
        if o: add('slot%d' % k, 'Щель 25 uu под дверцами шкафа ' + ru + ': внутрь без открывания', 'gap', [(o['c'][0], 3036, 0), (o['c'][0], 3120, 140)], 'floor', 'int', o['src'],
                  'по заметке манифеста: "open front, 25 uu slot under the doors"; еду через щель не вытащить', 'art table')
    # ways up
    up('towel', 'Полотенце: пол -> ручка духовки', 'SM_K_Towel', 0, 'floor', 'work', 'верх на 1436; до плиты (1740) ещё 300 по фасаду')
    up('curtain_w', 'Штора западная: пол -> подоконник -> карниз', 'SM_K_Curtain', 0, 'floor', 'top', 'подоконник на 2200')
    up('curtain_e', 'Штора восточная: пол -> подоконник -> карниз', 'SM_K_Curtain', 1, 'floor', 'top', 'подоконник на 2200')
    o = ob('SM_K_CurtainRod')
    if o: add('curtain_rod', 'Карниз между шторами (наверху ничего нет)', 'climb', [(o['bb'][0], o['c'][1], o['z'][0]), (o['bb'][2], o['c'][1], o['z'][0])], 'top', 'top', o['src'], '', 'art table')
    o = ob('SM_K_Spoon')
    if o: add('spoon', 'Ложка-трап: пол -> край стола', 'ramp', [(o['bb'][0], o['c'][1], o['z'][0]), (o['bb'][2], o['c'][1], o['z'][1])], 'floor', 'work', o['src'], 'единственный наклонный путь на стол', 'art table')
    o = ob('SM_K_TeaBag')
    if o: add('teabag', 'Нитка чайного пакетика: пол -> столешница B', 'climb', [(o['c'][0], o['bb'][1] + 16, 0), (o['c'][0], o['bb'][1] + 16, 1710), (o['c'][0], o['bb'][3] - 100, 1760)], 'floor', 'work', o['src'], '', 'art table')
    up('spatula', 'Лопатка: плита -> рейлинг', 'SM_K_Spatula', 0, 'work', 'top', 'нижний конец лежит на плите (1750)')
    up('ladle', 'Половник: висит на рейлинге (до плиты не достаёт 260)', 'SM_K_Ladle', 0, 'work', 'top')
    up('whisk', 'Венчик: висит на рейлинге', 'SM_K_Whisk', 0, 'work', 'top')
    a, b = ob('SM_K_SpiceShelf', 0), ob('SM_K_SpiceShelf', 3)
    if a and b: add('spice', 'Полочки специй: 4 ступени вверх по фартуку', 'climb', [(a['c'][0], a['c'][1], a['z'][0]), (b['c'][0], b['c'][1], b['z'][1])], 'work', 'top', a['src'], 'от верхней полочки до низа верхнего шкафа 154 uu', 'art table')
    o = ob('SM_K_Mop')
    if o: add('mop', 'Швабра: ведро -> северная стена на 4240 (наверху ничего нет)', 'climb', [(o['c'][0], o['bb'][1], o['z'][0]), (o['c'][0], o['bb'][3], o['z'][1])], 'floor', 'top', o['src'], '', 'art table')
    legs = ref(B, 'const FVector Legs[4] = { FVector(-1500.f, -1600.f, 0.f), FVector(2500.f, -1600.f, 0.f), FVector(-1500.f, 600.f, 0.f), FVector(2500.f, 600.f, 0.f) };')
    for k, (x, y) in enumerate([(-1500, -1600), (2500, -1600), (-1500, 600), (2500, 600)]):
        add('table_leg%d' % k, 'Ножка стола', 'climb', [(x, y, 0), (x, y, 1320)], 'floor', 'work', legs, 'координаты ножек из коробочной версии стола (те же габариты); в манифесте модели ножки не описаны', 'derived')
    add('honey', 'Мёд по западной стене: пол -> полка (липко, скорость x0.35)', 'climb', [(-7000, 275, 0), (-7000, 275, 2680)], 'floor', 'top', ref(B, 'Box(FVector(-7000.f, 190.f, 0.f), FVector(-6994.f, 360.f, 2680.f), Honey, false);'))
    add('toaster', 'Тостер-катапульта: щель тостера -> верх верхних шкафов', 'launch', [(-6250, 4200, 1950), (-6250, 4750, 4700), (-6250, 4950, 4400)], 'work', 'top',
        ref(IA, 'Ant->ApplyLaunch(FVector(FMath::FRandRange(-120.f, 120.f), 320.f, 2750.f));'),
        'оценка: скорость (±120, 320, 2750), гравитация муравья x1.4 -> апогей ~2750 над щелью, снос на север ~850: прилетает на верх шкафов у стены. Точка приземления зависит от физики', 'derived')
    o = ob('SM_L_Charger')
    if o: add('charger', 'Кабель зарядки: пол -> подлокотник дивана', 'climb', [(o['origin'][0], o['origin'][1], 0), (o['c'][0], o['bb'][3], o['z'][1])], 'floor', 'work', o['src'], '', 'art table')
    add('yarn', 'Нитка от клубка: пол -> сиденье кресла', 'climb', [(10600, -5000, 440), (9670, -5500, 1220)], 'floor', 'work', ref(H, 'const FVector A(10600.f, -5000.f, 440.f), B(9670.f, -5500.f, 1220.f);'), 'концы из коробочной версии; модель SM_L_YarnThread подогнана под них')
    o = ob('SM_L_Bookshelf')
    if o: add('bookshelf', 'Книжный шкаф: полки 960 / 1860 / 2760 / 3660, верх 4660', 'climb', [(o['bb'][0], o['c'][1], 0), (o['bb'][0], o['c'][1], 4660)], 'floor', 'top', o['src'], '', 'art table')
    up('floor_lamp', 'Торшер: стойка до абажура (наверху ничего нет)', 'SM_L_FloorLamp', 0, 'floor', 'top')
    up('cat_post', 'Когтеточка (наверху ничего нет)', 'SM_L_CatScratchPost', 0, 'floor', 'work')
    o = ob('SM_S_Drainpipe')
    if o: add('drainpipe', 'Водосточная труба (полая): земля -> желоб на крыше', 'climb', [(o['origin'][0], o['origin'][1], 0), (o['origin'][0], o['origin'][1], 8900), (-7350, -5250, 8960)], 'floor', 'top', o['src'], 'единственный задуманный путь к желобу', 'art table')
    o = ob('SM_Y_Tree')
    if o: add('tree', 'Дерево: ствол -> ветка', 'climb', [(o['origin'][0], o['origin'][1], 0), (o['origin'][0], o['origin'][1], 5700), (-7100, -12500, 5460)], 'floor', 'top', o['src'], 'ветка уходит от ствола на 3,0 м, по манифесту', 'art table')
    up('yard_bin', 'Уличный бак: стенка -> мешки наверху', 'SM_Y_Bin', 0, 'floor', 'top', 'крышка открыта с начала')
    up('street_lamp', 'Фонарь (наверху ничего нет)', 'SM_S_StreetLamp', 0, 'floor', 'top')
    up('hydrant', 'Гидрант (наверху ничего нет)', 'SM_S_Hydrant', 0, 'floor', 'work')
    st = [ob('SM_Y_SteppingStone_A', 0), ob('SM_Y_SteppingStone_B', 0), ob('SM_Y_SteppingStone_A', 1), ob('SM_Y_SteppingStone_B', 1), ob('SM_Y_SteppingStone_A', 2), ob('SM_Y_SteppingStone_B', 2), ob('SM_Y_SteppingStone_A', 3)]
    if all(st): add('stones', 'Дорожка из камней: стена дома -> уличный бак', 'path', [(s['c'][0], s['c'][1], 0) for s in st], 'floor', 'floor', st[0]['src'], '', 'art table')
    add('oven_door', 'Открытая дверца духовки: полка на высоте 300 перед духовкой (тяжёлая дверь)', 'ramp', [(-750, 1800, 300), (-750, 3000, 312)], 'floor', 'int', ref(GM, 'Door(EKitchenDoorKind::Oven, FVector(-750.f, 3000.f, 300.f));'), 'длина дверцы 1215', 'derived')
    add('dishwasher_door', 'Открытая дверца посудомойки: рампа у пола (тяжёлая дверь)', 'ramp', [(650, 1550, 122), (650, 3000, 160)], 'floor', 'int', ref(GM, 'Door(EKitchenDoorKind::Dishwasher, FVector(650.f, 3000.f, 120.f));'), 'длина дверцы 1476', 'derived')
    return C

def build_spawns(dog, cat):
    S = []
    def add(sid, ru, kind, p, src, note='', radius=None):
        d = {'id': sid, 'ru': ru, 'kind': kind, 'p': P(*p), 'area': area_of(p[0], p[1]), 'src': src, 'note': note}
        if radius: d['radius'] = radius
        S.append(d)
    nest = ref(B, 'return Team == 0 ? FVector(-7900.f, -2500.f, 0.f) : FVector(6300.f, 1250.f, 0.f);')
    rad = ref(BH, 'static float NestRadius(uint8 Team = 0) { return Team == 0 ? 950.f : 560.f; }')
    sp = ref(GM, 'const FVector Loc = Nest + FVector(Out * FMath::FRandRange(180.f, 380.f), FMath::FRandRange(-280.f, 280.f), 60.f);')
    add('nest0', 'Гнездо колонии: сдача еды и безопасная зона', 'nest', (-7900, -2500, 0), nest + ', ' + rad, 'сдача: груз в радиусе 950 и ниже z=500; безопасно: муравей в радиусе 950 и ниже z=400', 950)
    add('spawn0', 'Появление игроков и ботов колонии', 'spawn_ant', (-7620, -2500, 60), sp, 'x -7720..-7520, y -2780..-2220')
    add('nest1', 'Гнездо огненных муравьёв (соперники, боты)', 'nest_rival', (6300, 1250, 0), nest + ', ' + rad, 'их безопасная зона и сдача, радиус 560', 560)
    add('spawn1', 'Появление огненных', 'spawn_rival', (6020, 1250, 60), sp, 'x 5920..6120, y 970..1530; по умолчанию 2 бота')
    add('human', 'Появление Человека (просыпается на 75-й секунде)', 'spawn_human', (3800, -3200, 0), ref(B, 'FVector AKitchenBuilder::HumanSpawn() { return FVector(3800.f, -3200.f, 1720.f); }'))
    add('human_flee', 'Сюда Человек уходит, когда его закусали', 'human_flee', (5600, -4200, 0), ref(B, 'FVector AKitchenBuilder::HumanFleePoint() { return FVector(5600.f, -4200.f, 1720.f); }'))
    add('dog', 'Бадрик: спит на ковре под столом', 'spawn_dog', (600, -600, 0), ref(GM, 'Spawn(ACritter::StaticClass(), FVector(600.f, -600.f, 0.f)'))
    add('cat', 'Кошка: спит на диване', 'spawn_cat', (13500, 400, 950), ref(GM, 'Spawn(ACritter::StaticClass(), FVector(13500.f, 400.f, 950.f)'))
    add('kid', 'Малыш на диване (болтает ногами)', 'npc', (13500, 300, 950), ref(GM, 'Spawn(AHouseholdNpc::StaticClass(), FVector(13500.f, 300.f, 950.f)'))
    add('granny', 'Бабушка в кресле (переставляет тапок)', 'npc', (9250, -5800, 800), ref(GM, 'Spawn(AHouseholdNpc::StaticClass(), FVector(9250.f, -5800.f, 800.f)'))
    add('spider', 'Логово паука: паутина на восточной стене', 'spawn_spider', (6980, 2600, 4300), ref(BH, 'static FVector SpiderLair() { return FVector(6980.f, 2600.f, 4300.f); }'), 'ИИ-паук охотится на всех муравьёв и возвращается сюда')
    add('fly_poop', 'Кормушка мух: собачья кучка', 'fly_food', (-3000, -9500, 0), ref(BH, 'static FVector YardPoop() { return FVector(-3000.f, -9500.f, 0.f); }'))
    add('fly_bin', 'Кормушка мух: уличный бак', 'fly_food', (4200, -13000, 2650), ref(BH, 'static FVector YardBin() { return FVector(4200.f, -13000.f, 0.f); }'))
    add('fly_kbin', 'Кормушка мух: кухонное ведро', 'fly_food', (6000, -1500, 1600), ref(GM, 'Feeder(1, FVector(6000.f, -1500.f, 1600.f));'))
    for k, s in enumerate(dog): add('dog%d' % k, 'Точка Бадрика: ' + s['label'], 'dog_spot', s['p'], s['src'], '%s, %.0f с' % (s['does'], s['seconds']))
    for k, s in enumerate(cat): add('cat%d' % k, 'Точка кошки: ' + s['label'], 'cat_spot', s['p'], s['src'], '%s, %.0f с' % (s['does'], s['seconds']))
    return S

def build_interactables(objs, extras, ob):
    I = []
    def add(iid, ru, kind, p, src, does, **kw):
        d = {'id': iid, 'ru': ru, 'kind': kind, 'p': P(*p), 'area': area_of(p[0], p[1]), 'does': does, 'src': src}
        d.update(kw); I.append(d)
    add('door_fridge', 'Дверь холодильника', 'door', (4700, 3100, 120), ref(GM, 'FridgeDoor = Door(EKitchenDoorKind::Fridge, FVector(4700.f, 3100.f, 120.f));'), 'тяжёлая: держать [E], нужна сила 40 (около трёх фуражиров); заперт внутри = холод', door_class='heavy', human_shuts=True)
    add('door_oven', 'Дверца духовки', 'door', (-750, 3000, 300), ref(GM, 'Door(EKitchenDoorKind::Oven, FVector(-750.f, 3000.f, 300.f));'), 'тяжёлая (40); откидывается вниз; внутри жар 90 (открыта) / 170 (закрыта)', door_class='heavy', human_shuts=True)
    add('door_drawer', 'Верхний ящик', 'door', (-4900, 3000, 1330), ref(GM, 'Door(EKitchenDoorKind::Drawer, FVector(bArt ? -4900.f : -4950.f, 3000.f, 1330.f));'), 'лёгкий (один муравей); при первом открытии внутри появляются 3 сладости и оружие', door_class='light', human_shuts=True)
    add('door_dishwasher', 'Дверца посудомойки', 'door', (650, 3000, 120), ref(GM, 'Door(EKitchenDoorKind::Dishwasher, FVector(650.f, 3000.f, 120.f));'), 'тяжёлая (40); внутри мокро', door_class='heavy', human_shuts=True)
    add('door_microwave', 'Дверца микроволновки', 'door', (1300, 4000, 1740), ref(GM, 'Door(EKitchenDoorKind::Microwave, FVector(1300.f, 4000.f, 1740.f));'), 'средняя (24, около двух)', door_class='medium', human_shuts=True)
    for o in objs:
        if 'mover' in o:
            mv = o['mover']
            add('mover%d' % mv['part'], o['ru'], 'mover', (o['origin'][0], o['origin'][1], o['origin'][2]), mv['src'],
                '%s; класс двери: %s%s' % (mv['open_text'], {'light': 'лёгкая (один)', 'medium': 'средняя (24)', 'heavy': 'тяжёлая (40)'}[mv['door_class']], '; открыта с начала' if 'starts_open' in mv['flags'] else ''),
                door_class=mv['door_class'], human_shuts=mv['human_walks_over_to_shut'], object=o['id'], flags=mv['flags'], exact='art table')
    add('knob', 'Ручка плиты (средняя)', 'gadget', (-920, 2962, 1612), ref(GM, 'Spawn(AKitchenKnob::StaticClass(), bArt ? FVector(-920.f, 2962.f, 1612.f)'), 'включает переднюю правую конфорку: кастрюля закипает за 25 с, остывает за 12')
    o = ob('SM_K_Bin_Pedal')
    add('pedal', 'Педаль ведра', 'gadget', (o['c'][0], o['c'][1], 35) if o else (6000, -2235, 35), ref(GM, 'Spawn(AKitchenPedalBin::StaticClass(), FVector(6000.f, -1500.f, 0.f)'), 'крышка поднимается на 15 с, потом захлопывается: кто внутри — ждёт, пока нажмут снова')
    add('fan', 'Настольный вентилятор', 'gadget', (500, 3400, 1710), ref(GM, 'Spawn(AKitchenFan::StaticClass(), FVector(500.f, 3400.f, 1710.f)'), 'дует 20 с на юг со столешницы: сносит муравьёв и еду легче 12')
    add('tap_hot', 'Вентиль горячей воды', 'noise', (-3750, 4800, 1830), ref(IH, 'static FVector HotValve() { return FVector(-3750.f, 4800.f, 1830.f); }'), 'раковина наполняется за ~15 с кипятком; шум 0.65 каждые 3 с', loud=0.65)
    add('tap_cold', 'Вентиль холодной воды', 'noise', (-3250, 4800, 1830), ref(IH, 'static FVector ColdValve() { return FVector(-3250.f, 4800.f, 1830.f); }'), 'раковина наполняется за ~15 с; шум 0.65 каждые 3 с', loud=0.65)
    add('switch', 'Выключатель света', 'gadget', (-6995, 1900, 2300), ref(GM, 'Spawn(AKitchenSwitch::StaticClass(), FVector(-6995.f, 1900.f, 2300.f)'), 'свет выключен: Человек замечает муравья только ближе 2200 (с грузом 3200)')
    add('toaster', 'Рычаг тостера', 'noise', (-5900, 4200, 2060), ref(IA, 'const FVector Target(-5900.f, 4200.f, bArmed ? 1820.f : 2060.f);'), 'через 1,8 с выстреливает всех, кто сидит в щелях; шум 0.9; перезарядка 6 с; каждый выстрел роняет на столешницу 2 куска еды', loud=0.9)
    add('microwave_btn', 'Кнопка микроволновки', 'noise', (2290, 3985, 1960), ref(RUN, 'SpawnBox(0, FVector(2290.f, 3985.f, 1960.f));'), 'через 5 с дзынь: самый громкий шум (1.0)', loud=1.0)
    o = ob('SM_L_GameConsole')
    add('tv', 'Приставка на тумбе: включает телевизор', 'noise', (o['origin'][0], o['origin'][1], o['origin'][2] + 60) if o else (12500, -8250, 1160), ref(RUN, 'FVector Console(12500.f, -8250.f, 1160.f);'), 'орёт (шум 0.8 каждые 2,5 с), пока Человек не выключит', loud=0.8)
    add('trap', 'Мышеловка под мойкой', 'noise', (-3150, 3450, 140), ref(GM, 'Spawn(AKitchenTrap::StaticClass(), FVector(-3150.f, 3450.f, 140.f)'), 'урон 80 и подброс; шум 0.8; взводится снова через 15 с', loud=0.8)
    for k, pr in enumerate(extras['high_props']):
        add('push%d' % k, pr['ru'] + ' (столкнуть)', 'push', pr['p'], pr['src'], 'толкается рывком (Ctrl); упав на Человека, оглушает на 2,5 с; на огненного — насмерть', mass=pr['mass'])
    for k, pr in enumerate(extras['junk_props']):
        add('junk%d' % k, 'Мелочь для толкания: ' + (pr['label'] or pr['shape']), 'push', pr['p'], pr['src'], 'физический предмет без имени', mass=pr['mass'])
    return I

def build_hazards(dog):
    Z = []
    def add(hid, ru, kind, shape, z, effect, src, exact='code'):
        d = {'id': hid, 'ru': ru, 'kind': kind, 'z': z, 'effect': effect, 'src': src, 'exact': exact}
        d.update(shape)
        c = shape.get('circle') or [(shape['rect'][0] + shape['rect'][2]) / 2, (shape['rect'][1] + shape['rect'][3]) / 2]
        d['area'] = area_of(c[0], c[1]); Z.append(d)
    add('burner', 'Конфорка и стенки кастрюли', 'heat', {'circle': [-350, 3500, 460]}, [1735, 2320], 'после включения ручки: до 150 градусов', ref(KG, 'if (FVector::Dist2D(P, B) < 460.f && P.Z > 1735.f && P.Z < 2320.f)'))
    add('oven', 'Внутри духовки', 'heat', {'rect': [-1450, 3000, -50, 4700]}, [300, 1500], '170 градусов при закрытой дверце, 90 при открытой', ref(IA, 'Env.Temp(bOpen ? 90.f : 170.f, bOpen ? 9.f : 35.f);'))
    add('fridge', 'Внутри холодильника', 'cold', {'rect': [2780, 3100, 4620, 4670]}, [200, 2500], '-7: замедление, потом замерзание', ref(IA, 'Env.Temp(-7.f, bOpen ? 3.5f : 6.f);'))
    add('freezer', 'Внутри морозилки', 'cold', {'rect': [2780, 3100, 4620, 4670]}, [2560, 3620], '-20', ref(IA, 'Env.Temp(-20.f, 9.f);'))
    add('dishwasher', 'Внутри посудомойки', 'wet', {'rect': [60, 3040, 1240, 4700]}, [160, 1600], 'мокро (скорость x0.7), тепло', ref(IA, 'if (FBox(FVector(60.f, 3040.f, 160.f), FVector(1240.f, 4700.f, 230.f)).IsInside(P))'))
    add('sink', 'Раковина: вода', 'water', {'rect': [-4100, 3300, -2900, 4700]}, [1150, 1620], 'при открытом кране наполняется за ~15 с: мокро, глубже 60 — тонешь (7 урона/с); горячая — 62 градуса', ref(IH, 'static FBox Basin() { return FBox(FVector(-4100.f, 3300.f, BasinFloor), FVector(-2900.f, 4700.f, BasinTop)); }'))
    add('stream', 'Струя из крана', 'water', {'circle': [-3500, 4400, 75]}, [1150, 2060], 'сбивает с ног, 6 урона/с', ref(IH, 'static FVector Spout() { return FVector(-3500.f, 4400.f, 2060.f); }'))
    add('toaster_slots', 'Щели тостера', 'heat', {'rect': [-6500, 4000, -6000, 4400]}, [1780, 2400], 'пока взведён: 85 градусов, затем выстрел вверх', ref(IA, 'return FBox(FVector(-6500.f, 4000.f, KitchenArt::Enabled() ? 1780.f : 2100.f), FVector(-6000.f, 4400.f, 2400.f));'))
    add('microwave', 'Работающая закрытая микроволновка', 'heat', {'rect': [1300, 4000, 2200, 4880]}, [1740, 2330], '230 градусов и 25 урона/с тому, кто заперт внутри', ref(KE, 'Env.Hurt(25.f, TEXT("Ты в работающей микроволновке!"));'))
    add('wire', 'Оголённый провод за холодильником', 'shock', {'circle': [3520, 4860, 280]}, [0, 300], 'искрит каждые 3,5-6,5 с: 35 урона и отброс', ref(IA, 'const FVector End(3520.f, 4860.f, 16.f);'))
    add('puddle', 'Лужа под и за холодильником', 'shock', {'rect': [2900, 3700, 4500, 4950]}, [-50, 70], 'мокро; при искре 40 урона всем в луже, цепью по мокрым муравьям', ref(IH, 'static FBox Puddle() { return FBox(FVector(2900.f, 3700.f, -50.f), FVector(4500.f, 4950.f, 70.f)); }'))
    add('trap', 'Мышеловка', 'trap', {'rect': [-3245, 3260, -3055, 3640]}, [130, 230], '80 урона и подброс; шум 0.8; 15 с до взвода', ref(IA, 'const FBox Trigger(FVector(-190.f, -95.f, -10.f), FVector(190.f, 95.f, 90.f));'), 'derived')
    for n, (g, cm, src) in enumerate(parse_table(B, 'static const FBox Sticky[] =', 'for (const FBox& B : Sticky)', r'FBox\(' + FV + r',\s*' + FV + r'\)')):
        f = [float(v) for v in g]
        add('sticky%d' % n, 'Липко: ' + cm, 'sticky', {'rect': [f[0], f[1], f[3], f[4]]}, [f[2], f[5]], 'скорость x0.35, нельзя прыгать', src)
    add('radiator', 'Батарея', 'heat', {'rect': [7700, 1150, 9600, 1500]}, [250, 1500], '70 градусов', ref(B, 'if (FBox(FVector(7700.f, 1150.f, 250.f), FVector(9600.f, 1500.f, 1500.f)).IsInside(P))'))
    add('bowl_water', 'Вода в миске', 'wet', {'circle': [-2500, -3900, 380]}, [110, 230], 'мокро', ref(B, 'if (FVector::Dist2D(P, FVector(-2500.f, -3900.f, 0.f)) < 380.f && P.Z > 110.f && P.Z < 230.f)'))
    add('bucket_water', 'Вода в ведре со шваброй', 'wet', {'circle': [6200, 3900, 400]}, [600, 720], 'мокро', ref(B, 'if (FVector::Dist2D(P, FVector(6200.f, 3900.f, 0.f)) < 400.f && P.Z > 600.f && P.Z < 720.f)'))
    add('fan_wind', 'Ветер вентилятора', 'wind', {'rect': [-200, 0, 1200, 3250]}, [1150, 2130], 'когда включён (20 с): сдувает на юг; полоса качается на ±35 градусов; задевает северный край обеденного стола', ref(KG, 'const FBox Zone(FVector(-700.f, -3400.f, -560.f), FVector(700.f, -150.f, 420.f));'), 'derived')
    add('bin_lid', 'Педальное ведро: крышка', 'trap', {'circle': [6000, -1500, 610]}, [30, 1540], 'крышка закрывается через 15 с: изнутри не выйти, пока кто-то не нажмёт педаль', ref(KG, 'CloseAt = Now() + 15.f;'))
    add('kid', 'Ноги малыша перед диваном', 'npc', {'rect': [12940, -920, 14060, -280]}, [0, 320], 'болтает ногами 4-7 с каждые 12-24 с: 140 урона/с; за секунду предупреждает', ref(CR, 'const FVector Z = GetActorTransform().TransformPosition(FVector(0.f, -900.f, -950.f));'), 'derived')
    add('granny_l', 'Тапок бабушки (левое место)', 'npc', {'circle': [10460, -5520, 260]}, [0, 300], 'раз в 25-45 с переставляет ногу: 300 урона', ref(CR, 'const FVector Foot = GetActorTransform().TransformPosition(FVector(1210.f, Pose == 0 ? 280.f : -280.f, -870.f));'), 'derived')
    add('granny_r', 'Тапок бабушки (правое место)', 'npc', {'circle': [10460, -6080, 260]}, [0, 300], 'раз в 25-45 с переставляет ногу: 300 урона', ref(CR, 'const FVector Foot = GetActorTransform().TransformPosition(FVector(1210.f, Pose == 0 ? 280.f : -280.f, -870.f));'), 'derived')
    for k, s in enumerate(dog):
        if s['does'] == 'sniff_or_eat':
            add('dog_eat%d' % k, 'Бадрик ест еду рядом: ' + s['label'], 'pet', {'circle': [s['p'][0], s['p'][1], 1150]}, [0, 600], 'пока стоит здесь (%.0f с), раз в 1,6 с съедает еду в 450 uu от пасти (пасть в 700 перед ним): оценка зоны 1150' % s['seconds'],
                ref(CR, 'const FVector Mouth = GetActorTransform().TransformPosition(FVector(700.f, 0.f, 150.f));'), 'derived')
    add('spider_web', 'Паутина паука', 'pet', {'circle': [6980, 2600, 770]}, [3530, 5070], 'логово паука на стене (вертикальный круг); паук охотится на всех муравьёв', ref(B, 'const FVector C = SpiderLair() + FVector(10.f, 0.f, 0.f);'))
    add('fire_nest', 'Гнездо огненных', 'rival', {'circle': [6300, 1250, 560]}, [0, 400], 'двое ботов-соперников выходят отсюда, воруют еду, дерутся', ref(BH, 'static float NestRadius(uint8 Team = 0) { return Team == 0 ? 950.f : 560.f; }'))
    return Z

def build_events(ev):
    loc = {1: ('circle', [-6995, 1900, 300], 'у выключателя; действует на весь дом', ref(RUN, 'case AntEvent::Blackout:')),
           2: ('circle', [13500, 400, 800], 'кошка спрыгивает со своего места и охотится на полу: на муравьёв ниже z=350 и южнее y=3000; прыгает с 800', ref(CR, 'A->GetActorLocation().Z > 350.f || A->GetActorLocation().Y > 3000.f')),
           3: ('rect', [-6000, -4300, 6200, 2550], 'появляется в (5300,-3600), ездит по полу кухни (радиус 330, высота 170, 300 uu/с): давит муравьёв перед собой, ест еду весом до 4, сверху на нём можно ехать', ref(RUN, 'Where = FVector(5300.f, -3600.f, ARobotVacuum::Height * 0.5f + 2.f);')),
           4: ('circle', [-3500, 2200, 1700], 'лужа растёт до радиуса 1700 за 7 с, глубина 14: мокро и медленно, еда уплывает от мойки', ref(RUN, 'Where = FVector(-3500.f, 2200.f, 1.f);')),
           5: ('rect', [4300, -4300, 5900, -2700], 'коробка 1600 x 1600 у двери в гостиную, 6 кусков по 45 (вес 9), исчезают через 60 с', ref(RUN, 'Where = FVector(5100.f, -3500.f, 0.f);')),
           6: ('circle', [4700, 3100, 500], 'дверь холодильника открыта 20 с', ref(RUN, 'FridgeDoor->SetOpen(true, Len);'))}
    out = []
    for e in ev:
        if e['id'] not in loc:      # events added after this table was written (7: Badrik's zoomies): the whole house
            loc[e['id']] = ('rect', [-7000, -9000, 20200, 5000], 'по всему дому', 'AntRun.cpp')
        kind, shape, note, src = loc[e['id']]
        d = dict(e); d[kind] = shape; d['where_ru'] = note; d['where_src'] = src
        d['area'] = 'living' if e['id'] == 2 else 'kitchen'
        out.append(d)
    when = {'ru': 'одно событие за смену, в случайный момент между 90-й и ~310-й секундой (75 + 15..235)', 'src': ref(RUN, 'Run.EventAt = Quiet + Rng.FRandRange(15.f, FMath::Max(20.f, Length - Quiet - 170.f));')}
    return out, when

SECRETS = [  # (ru, point, what)
    ('Коридор за нижними шкафами', (-3000, 4875, 0), '4 точки лута (одна tier 3 в западном тупике); Человек туда не смотрит'),
    ('Комната мыши в стене', (3900, 5300, 0), '2 точки tier 3; вход через нору за холодильником'),
    ('Яма под шатающейся плиткой', (5000, 2000, -100), 'заначка "Брошенный груз" (90, вес 16) и 2 точки лута'),
    ('Спичечный коробок у плиты', (-1850, 2600, 10), 'лоток-укрытие, точка tier 2'),
    ('Коробка хлопьев-тоннель', (-5300, 900, 20), '2 точки внутри, одна на крыше'),
    ('Под газетой', (1200, -4300, 0), 'шалаш, точка tier 1'),
    ('В ботинке у двери', (6550, -4150, 320), 'точка tier 2'),
    ('В носке', (9000, -2000, 0), 'точка tier 2'),
    ('Под диваном', (13600, 700, 0), '3 точки (две tier 3), просвет 260'),
    ('Тумба ТВ изнутри', (13700, -8400, 120), 'точки tier 3 и 2 + две за сдвижными дверцами'),
    ('Воздуховоды', (-3100, 1500, 5420), '3 точки (две tier 3 в потолочных участках)'),
    ('Верх верхних шкафов', (-5400, 4650, 4400), '2 точки tier 3; катапульта тостера и выход вентиляции 1'),
    ('Морозилка', (3600, 4000, 2560), '2 точки tier 3 + одна за дверью; -20; лаз сзади из холодильника'),
    ('Ящик с заначкой', (-4900, 3400, 1385), '3 сладости и оружие при первом открытии верхнего ящика'),
    ('Полка с мёдом над гнездом', (-6780, -650, 2680), 'точки tier 3 и 2, банки, мёд (липко)'),
    ('Желоб на крыше', (-7350, 0, 8960), '2 точки tier 3; подъём по водосточной трубе 8900'),
    ('Под машиной и на люке', (-13500, 0, -230), '2 точки tier 2'),
    ('Уличный бак', (4200, -13000, 2790), 'точка tier 3 на мешках'),
    ('Ветка дерева', (-7100, -12500, 5460), 'точка tier 3'),
]

def build_boxes():
    """Box-built things that still stand in the modelled kitchen (not inside an `if (!bArt)` block)."""
    X = []
    def add(ru, bb, z, solid, src, note='', cat='prop'):
        X.append({'id': 'box%d' % len(X), 'ru': ru, 'cat': cat, 'role': 'code box', 'bb': bb, 'c': [(bb[0] + bb[2]) / 2, (bb[1] + bb[3]) / 2], 's': [bb[2] - bb[0], bb[3] - bb[1]], 'yaw': 0.0,
                  'z': z, 'solid': solid, 'climbable': solid, 'area': area_of((bb[0] + bb[2]) / 2, (bb[1] + bb[3]) / 2), 'src': src, 'note': note})
    add('Шатающаяся плитка (один край поднят)', [4500, 1500, 5500, 2500], [100, 215], True, ref(H, 'Part(EAntShape::Cube, FVector(5000.f, 2000.f, 110.f), FVector(1000.f, 1000.f, 20.f), FRotator(0.f, 0.f, 12.f), Hex(150, 60, 55), true);'), 'наклон 12 градусов; под ней яма')
    add('Опрокинутый мешок мусора', [4700, -800, 5700, 0], [0, 550], True, ref(B, 'Ball(FVector(5200.f, -400.f, 250.f), FVector(500.f, 400.f, 300.f), Black);'))
    add('Холмик гнезда огненных', [6200, 650, 7000, 1850], [0, 360], True, ref(B, 'AddMound(1, Ball(FVector(R.X + 420.f, R.Y, -60.f), FVector(520.f, 600.f, 420.f), Red));'), 'растёт по мере запасов')
    add('Кратер гнезда колонии (комья земли)', [-8820, -3420, -6980, -1580], [0, 140], True, ref(H, 'AddMound(0, Ball(FVector(Nest.X + FMath::Cos(A) * R, Nest.Y + FMath::Sin(A) * R, -60.f)'), 'кольцо холмиков радиусом ~620, вход в центре')
    add('Паутина паука на восточной стене', [6960, 1830, 7000, 3370], [3530, 5070], False, ref(B, 'const FVector C = SpiderLair() + FVector(10.f, 0.f, 0.f);'), 'без коллизии', 'deco')
    add('Лампа над обеденным столом', [25, -975, 975, -25], [3500, 3800], False, ref(B, 'const FVector P(500.f, -500.f, 3500.f);'), 'без коллизии', 'deco')
    add('Труба в коридоре за шкафами', [-2040, 4840, -1960, 4920], [0, 1650], True, ref(B, 'Cyl(FVector(-2000.f, 4880.f, 0.f), 40.f, 1650.f, Hex(200, 200, 195));'))
    add('Коробка хлопьев на столешнице', [-2700, 4150, -1800, 4650], [1710, 2030], True, ref(B, 'UStaticMeshComponent* Shell = Box(FVector(-2700.f, 4150.f, 1710.f), FVector(-1800.f, 4650.f, 2030.f), Hex(230, 70, 40));'), 'не смоделирована, осталась коробкой')
    add('Спичечный коробок-кровать мыши', [3550, 5250, 3800, 5500], [0, 90], True, ref(B, 'Box(FVector(3550.f, 5250.f, 0.f), FVector(3800.f, 5500.f, 90.f), Hex(200, 50, 40));'))
    add('Рассыпанная мука', [-6520, 1780, -5480, 2820], [0, 14], False, ref(H, 'Cyl(FVector(-6000.f, 2300.f, 0.f), 520.f, 14.f, White, false);'), 'без коллизии', 'struct')
    add('Мешок в педальном ведре', [5560, -1930, 6440, -1070], [30, 890], True, ref(H, 'Ball(Bin + FVector(0.f, 0.f, 460.f), FVector(440.f, 430.f, 430.f), Hex(30, 70, 40));'))
    add('Вода в ведре со шваброй', [5800, 3500, 6600, 4300], [600, 640], True, ref(B, 'Cyl(FVector(6200.f, 3900.f, 600.f), 400.f, 40.f, Hex(150, 200, 255));'))
    add('Северный забор двора, западная часть', [-12000, -5260, -10800, -5140], [0, 2600], True, ref(H, 'for (float X = -12000.f; X <= -9400.f; X += 1100.f)'), 'остался коробками', 'struct')
    add('Северный забор двора, восточная часть', [-10000, -5260, -9400, -5140], [0, 2600], True, ref(H, 'for (float X = -12000.f; X <= -9400.f; X += 1100.f)'), 'между частями разрыв x -10800..-10000', 'struct')
    add('Треснувшая розетка', [-3250, -5000, -2950, -4984], [480, 720], True, ref(H, 'Box(FVector(-3250.f, -5000.f, 660.f), FVector(-2950.f, -4984.f, 720.f), White);'), '', 'deco')
    return X

def build_interiors(ob):
    L = [{'id': 'under_sink', 'ru': 'под мойкой', 'box': [-4200, 3000, 0, -2800, 4750, 1100], 'entry': 'правая дверца открыта с начала; дыра в задней стенке; труба-телепорт', 'src': ref(HA, '|| FBox(FVector(-4200.f, 3000.f, 0.f), FVector(-2800.f, 4750.f, 1100.f)).IsInside(AL))')},
         {'id': 'pedal_bin', 'ru': 'в педальном ведре', 'box': [5390, -2110, 30, 6610, -890, 1540], 'entry': 'только через крышку (педаль, 15 с)', 'src': (ob('SM_K_Bin') or {}).get('src', '?')}]
    for mesh, ru, z1, entry in [('SM_K_CerealBox', 'в коробке хлопьев', 410, 'открытый торец с востока'), ('SM_K_Shoe', 'в ботинке', 388, 'сверху у пятки'), ('SM_L_Sock', 'в носке', 180, 'открытая манжета'),
                                ('SM_K_Matchbox', 'в спичечном коробке', 168, 'лоток выдвинут с начала'), ('SM_K_Newspaper', 'под газетой', 352, 'шалаш открыт с двух торцов'),
                                ('SM_Y_Bin', 'в уличном баке', 2621, 'крышка открыта с начала')]:
        o = ob(mesh)
        if o: L.append({'id': mesh[5:].lower(), 'ru': ru, 'box': [o['bb'][0], o['bb'][1], 0, o['bb'][2], o['bb'][3], z1], 'entry': entry, 'src': o['src']})
    return L

def build_surfaces(ob):
    """Raised surfaces an ant can stand on (rectangles), with the designed ways up. Derived from the art boxes and the code."""
    S = []
    def add(sid, ru, tier, rects, z, aids, src, note=''):
        S.append({'id': sid, 'ru': ru, 'tier': tier, 'rects': rects, 'z': z, 'area': area_of((rects[0][0] + rects[0][2]) / 2, (rects[0][1] + rects[0][3]) / 2), 'aids': aids, 'src': src, 'note': note})
    def bb(mesh, idx=0):
        o = ob(mesh, idx); return (o['bb'], o['src']) if o else ([0, 0, 0, 0], '?')
    wa, s1 = bb('SM_K_WorktopA'); wb, s2 = bb('SM_K_WorktopB'); sk, s3 = bb('SM_K_Sink'); rg, s4 = bb('SM_K_Range')
    add('worktop_west', 'Столешница запад (тостер, доска, ящики)', 'work', [[wa[0], wa[1], sk[0], wa[3]]], 1710, ['фасады шкафов (1710 вверх)'], s1)
    add('sink_rim', 'Борта мойки', 'work', [[sk[0], wa[1], sk[2], sk[1]], [sk[0], sk[3], sk[2], wa[3]]], 1710, ['drain', 'pipe'], s3)
    add('sink_bottom', 'Дно мойки', 'work', [[-4116, 3284, -2884, 4716]], 1150, ['drain', 'pipe'], s3, 'вода')
    add('worktop_mid', 'Столешница между мойкой и плитой', 'work', [[sk[2], wa[1], wa[2], wa[3]]], 1710, ['фасады шкафов (1710 вверх)'], s1)
    add('stove_top', 'Плита', 'work', [[rg[0], rg[1], rg[2], rg[3]]], 1740, ['towel', 'spatula', 'oven_door'], s4)
    add('worktop_east', 'Столешница восток (вентилятор, чайник)', 'work', [[wb[0], wb[1], wb[2], wb[3]]], 1710, ['teabag', 'dishwasher_door'], s2)
    for sid, ru, mesh, idx, tier, z, aids in [
            ('toaster_top', 'Верх тостера', 'SM_K_Toaster', 0, 'top', 2150, []), ('microwave_top', 'Верх микроволновки', 'SM_K_Microwave', 0, 'top', 2350, []),
            ('table', 'Обеденный стол', 'SM_K_Table', 0, 'work', 1400, ['spoon', 'table_leg0', 'table_leg1', 'table_leg2', 'table_leg3']),
            ('chair_s', 'Стул южный (сиденье)', 'SM_K_Chair', 0, 'work', 880, ['ножки стула']), ('chair_n', 'Стул северный (сиденье)', 'SM_K_Chair', 1, 'work', 880, ['ножки стула']),
            ('sill_in', 'Подоконник', 'SM_K_WindowSill', 0, 'top', 2200, ['curtain_w', 'curtain_e', 'стена под окном (2200 вверх)']),
            ('sill_out', 'Внешний подоконник', 'SM_K_WindowSill', 1, 'top', 2200, ['window']),
            ('wall_shelf', 'Настенная полка над гнездом', 'SM_K_WallShelf', 0, 'top', 2680, ['honey', 'западная стена (2680 вверх)']),
            ('fridge_top', 'Верх холодильника', 'SM_K_Fridge', 0, 'top', 3700, ['duct2', 'стенки холодильника (3700 вверх)']),
            ('cereal_top', 'Крыша коробки хлопьев', 'SM_K_CerealBox', 0, 'work', 428, []), ('bin_lid', 'Крышка педального ведра', 'SM_K_Bin_Lid', 0, 'work', 1644, []),
            ('coffee_table', 'Журнальный стол', 'SM_L_CoffeeTable', 0, 'work', 980, ['ножки стола']), ('tv_stand', 'Верх тумбы ТВ', 'SM_L_TVStand', 0, 'work', 1100, []),
            ('radiator', 'Верх батареи', 'SM_L_Radiator', 0, 'work', 1400, []), ('dog_bed', 'Лежанка собаки', 'SM_L_DogBed', 0, 'work', 323, []),
            ('yard_bin_top', 'Мешки в уличном баке', 'SM_Y_Bin', 0, 'top', 2790, ['yard_bin']), ('gutter', 'Желоб на крыше', 'SM_S_Gutter', 0, 'top', 8960, ['drainpipe']),
            ('car_top', 'Машина сверху', 'SM_S_Car', 0, 'top', 2620, ['колёса'])]:
        b, s = bb(mesh, idx)
        add(sid, ru, tier, [b], z, aids, s)
    add('wallcab_w', 'Верх верхних шкафов (запад)', 'top', [[-7000, 4280, -2000, 5000]], 4400, ['toaster', 'duct1', 'spice', 'западная стена (4400 вверх)'], bb('SM_K_WallCab1250')[1])
    add('wallcab_e', 'Верх верхнего шкафа над микроволновкой', 'top', [[1000, 4280, 2500, 5000]], 4400, [], bb('SM_K_WallCab1500')[1])
    add('counter_cereal', 'Коробка хлопьев на столешнице (верх)', 'work', [[-2700, 4150, -1800, 4650]], 2030, [], ref(B, 'UStaticMeshComponent* Shell = Box(FVector(-2700.f, 4150.f, 1710.f), FVector(-1800.f, 4650.f, 2030.f), Hex(230, 70, 40));'))
    add('sofa_seat', 'Диван: сиденье', 'work', [[10500, -260, 16500, 1000]], 950, ['charger', 'стенки дивана (950 вверх)'], bb('SM_L_Sofa_SeatCushion')[1])
    add('sofa_back', 'Диван: верх спинки и подлокотники', 'top', [[10200, 1000, 16800, 1400]], 2100, ['charger'], bb('SM_L_Sofa')[1])
    add('armchair', 'Кресло: сиденье', 'work', [[8550, -6650, 9950, -4950]], 800, ['yarn'], bb('SM_L_Armchair')[1], 'размер сиденья из коробочной версии')
    bs, sbs = bb('SM_L_Bookshelf')
    for z in (960, 1860):
        add('bookshelf_%d' % z, 'Книжный шкаф: полка %d' % z, 'work', [bs], z, ['bookshelf'], sbs)
    for z in (2760, 3660, 4660):
        add('bookshelf_%d' % z, 'Книжный шкаф: ' + ('верх' if z == 4660 else 'полка %d' % z), 'top', [bs], z, ['bookshelf'], sbs)
    return S

SUSPICIOUS = [  # (kind, ru, evidence)
    ('loot inside solid', 'Яма под плиткой: обе точки лута (surface z=-400) и заначка "Брошенный груз" (5000,2000,-350) стоят внутри коробки пола ямы (она занимает z -400..-100, её верх -100). Еда появляется внутри тела: её вытолкнет или она провалится под мир.',
     'KitchenHouse.cpp: Box(FVector(4700,1700,-400), FVector(5300,2300,-100)); ARTPROBE 165/166: сверху Cube на -100, под точкой пусто; SpawnStash(FVector(5000,2000,-350))'),
    ('loot inside solid', 'Комната мыши: точка tier 3 (3700,5300,0) стоит внутри коробки-кровати (3550..3800 x 5250..5500 x 0..90).', 'KitchenBuilder.cpp LootSpots + matchbox bed box; ARTPROBE 106: сверху Cube на 90'),
    ('loot inside solid', 'Столешница B: точка (600,3300,1710) под основанием настольного вентилятора (он стоит в (500,3400,1710)).', 'ARTPROBE 45: над точкой Cylinder на 1750'),
    ('loot falls', 'Посудомойка: точки нижней корзины (300,3600,420) и (900,4200,420) — луч находит только дно машины на 160: еда проваливается сквозь проволочную корзину в воду на дне.', 'ARTPROBE 49/50: from above SM_K_Dishwasher 160'),
    ('npc overlap', 'Кошка спит в (13500,400,950), малыш сидит в (13500,300,950): кошка внутри малыша. Точка кошки "кресло" (9250,-5800,800) совпадает с бабушкой.', 'Critters.cpp Spots + AntKitchenGameMode.cpp Spawn(AHouseholdNpc...)'),
    ('npc overlap', 'Бадрик спит в (600,-600,0) — на теннисном мяче (310..690 x -590..-210).', 'Critters.cpp Spots; KitchenArtData.inl SM_K_TennisBall'),
    ('open edge', 'У двора нет забора с востока (x=21000), у улицы — с севера (y=5600), запада (x=-16500) и юга (y=-5200 западнее x=-12000). Поля за ними без коллизии: муравей падает и умирает на z<-800.',
     'KitchenBuilder.cpp: Box(FVector(-40000,-60000,-400), ..., false); таблица арта: SM_Y_FencePanel только юг и запад двора и вдоль тротуара; AntCharacter.cpp: Z < -800 -> Die'),
    ('ai', 'ИИ Человека берёт в цель муравья во дворе и на улице (до 8000 uu), а Route() ведёт его к щелям, в которые он не пролезает: он встаёт вплотную к стене у (-6700,-2500) — прямо у входа в гнездо — или у (-4500,-4800). Выведено из кода, в игре не проверено.',
     'HumanAIController.cpp PickTarget + KitchenBuilder.cpp Route/CrackPath/WindowPath'),
    ('ai', 'Ботинок (6330..6770 x -4650..-3650, высота 388) стоит прямо на путевой точке двери (6500,-4000), через которую Route() ведёт Человека в гостиную; между ботинком и стеной 230 uu при ширине Человека 680, шаг у него 250. Пролезает ли он — зависит от формы модели; в игре не проверено.',
     'KitchenBuilder.cpp Route(): FVector(bFromKitchen ? 6500.f : 7700.f, D.Y, Z); KitchenArtData.inl SM_K_Shoe; HumanCharacter.cpp MaxStepHeight = 250'),
    ('balance', 'Тостер — бесконечная еда: каждый выстрел (раз в ~8 с) кладёт на столешницу 2 куска (tier 0 и tier 1), без проверки, что место занято. Шум после проверки Человеком глушится на 40 с, еда — нет.', 'KitchenInteractables.cpp AKitchenToaster::Pop: GM->SpawnFood x2'),
    ('logic', '"Жизнь кухни" (холодильник открылся, кран включился, дверца шкафа открыта) срабатывает по таймеру и в тихую фазу, когда Человек ещё "в телефоне", и независимо от того, где он стоит.', 'AntKitchenGameMode.cpp KitchenLifeEvent, таймер 38 с с 25-й секунды'),
    ('docs', 'FEATURES.md называет кухню "14 x 10 м", гостиную "13 x 11 м". При заявленном масштабе 1 м = 1900 uu кухня 14000 x 10000 uu — это 7,4 x 5,3 м, гостиная 6,8 x 5,5 м.', 'docs/FEATURES.md раздел 9; KitchenBuilder.h'),
    ('ai', 'Коробка блуждания Человека по кухне (-6000..6200, -4300..2550) не доходит до столешниц: полоса y 2550..2950 перед шкафами, западные 1000 uu и южные 700 uu — туда он приходит только за целью или на шум. Робот-пылесос ездит в той же коробке.', 'KitchenBuilder.cpp PlayArea()'),
    ('ai', 'Человек никогда не выбирает целью муравья севернее y=4750 на любой высоте: вентили крана, полочки специй, рейлинг, задняя полоса столешницы и верха шкафов — слепая зона.', 'HumanAIController.cpp: if (AL.Y > 4750.f ...'),
]
UNRESOLVED = [
    ('Мухи: 2 штуки появляются в случайной точке пола на высоте 2500', 'AntKitchenGameMode.cpp SpawnFly: RandomFloorPoint() + (0,0,2500)'),
    ('Оружие: 10 штук в случайных точках лута (±70 uu), потом дозаполнение до 6', 'AntKitchenGameMode.cpp SpawnInitialLoot / RefillLoot'),
    ('Точка приземления катапульты тостера: зависит от физики (оценка на карте)', 'KitchenInteractables.cpp AKitchenToaster::Pop'),
    ('Дыра в задней стенке тумбы ТВ: в манифесте есть ("gnawed hole at floor level"), координат нет', 'SourceArt/Blender/living/living_manifest.json SM_L_TVStand'),
    ('Ножки стола и стульев в модели: взяты координаты коробочной версии', 'KitchenBuilder.cpp Build() / Chair()'),
    ('Размер настольного вентилятора: взята только точка установки', 'KitchenGadgets.cpp AKitchenFan::BeginPlay'),
    ('Просветы под посудомойкой, книжным шкафом, ведром: в манифестах не указаны, на карте считаются сплошными', 'kitchen_manifest.json / living_manifest.json'),
    ('Внутренняя геометрия моделей (полки, дверные карманы, перегородки): коллизия по треугольникам, на карте только габариты', 'docs/ART_INTEGRATION.md'),
    ('Маршруты ботов, паука и мух: фиксированных точек нет', 'AntBotController.cpp, CreatureCharacters.cpp'),
    ('Трава (пучки): рассыпаны случайно, без коллизии, на карте нет', 'KitchenBuilder.cpp ScatterTufts'),
    ('Точки, куда малыш роняет крошки: случайно в полосе перед диваном (x 13000..14000, y около -600)', 'Critters.cpp AHouseholdNpc::Tick'),
]

# ---------------------------------------------------------------------------------------------
# Checks -> "suspicious"
# ---------------------------------------------------------------------------------------------
BOUNDS = {'kitchen': [-7250, -5250, 7250, 5650], 'living': [6950, -9250, 20450, 1750], 'yard': [-12100, -16100, 21100, -4950], 'street': [-16550, -5450, -6950, 6050]}

def run_checks(objs, loot, hazards, connectors, probe):
    sus = []
    for o in objs:
        b = BOUNDS[o['area']]
        if o['bb'][0] < b[0] or o['bb'][1] < b[1] or o['bb'][2] > b[2] or o['bb'][3] > b[3]:
            sus.append({'kind': 'outside its area', 'ru': '%s (%s) выходит за границы зоны "%s": габарит %s' % (o['ru'], o.get('mesh', o['id']), o['area'], o['bb']), 'evidence': o['src'], 'object': o['id']})
    solids = [o for o in objs if (o['cat'] in ('prop', 'food', 'conn') or (o['cat'] == 'furn' and o['z'][1] <= 400)) and not o.get('interior') and o.get('solid') and o.get('mesh') != 'SM_Y_Tree']
    for s in loot:
        if s['tier'] in ('int', 'vent'): continue
        x, y, z = s['p']; zc = z + 35
        for o in solids:
            if o['bb'][0] + 60 < x < o['bb'][2] - 60 and o['bb'][1] + 60 < y < o['bb'][3] - 60 and o['z'][0] + 3 < zc < o['z'][1] - 80:
                pr = s.get('probe')
                if pr and o['z'][1] < zc + 365 and pr['above'][0] != '-' and pr['above'][1] <= z + 40:
                    continue   # the game's own ray from 400 above went through where the box says the object is: the model is not there
                if pr and pr['ceil'][0] != '-' and pr['ceil'][1] - z >= 60 and pr['below'][0] != '-' and z - pr['below'][1] <= 60:
                    continue   # free space above the spot and a surface right under it
                sus.append({'kind': 'loot inside a box', 'ru': 'Точка лута #%d (%.0f, %.0f, %.0f) попадает в габарит "%s" (z %.0f..%.0f). Габарит — не точная форма: проверить в игре.' % (s['i'], x, y, z, o['ru'], o['z'][0], o['z'][1]),
                            'evidence': s['src'] + ' + ' + o['src'], 'loot': s['i'], 'object': o['id']})
                break
    eat = [h for h in hazards if h['id'].startswith('dog_eat')]
    for s in loot:
        if s['p'][2] > 400: continue
        for h in eat:
            if math.hypot(s['p'][0] - h['circle'][0], s['p'][1] - h['circle'][1]) < h['circle'][2]:
                s['dog_eats'] = True
    n = sum(1 for s in loot if s.get('dog_eats'))
    if n: sus.append({'kind': 'loot the dog eats', 'ru': '%d точек лута лежат в зонах, где Бадрик съедает еду (миска, мусорка, ботинок, мука): %s' % (n, ', '.join('#%d' % s['i'] for s in loot if s.get('dog_eats'))), 'evidence': 'Critters.cpp ServerThink (eat) + LootSpots'})
    dead_ends = [c for c in connectors if 'наверху ничего нет' in c['ru']]
    if dead_ends: sus.append({'kind': 'connector to nowhere', 'ru': 'Пути наверх, которые никуда не ведут (наверху нет ни еды, ни перехода): ' + '; '.join(c['ru'] for c in dead_ends), 'evidence': 'сверка connectors с loot'})
    if probe:
        for s in loot:
            pr = s.get('probe')
            if not pr: continue
            z = s['p'][2]
            if pr['below'][0] == '-' and s['tier'] != 'vent':
                sus.append({'kind': 'probe: nothing under the spot', 'ru': 'Точка лута #%d %s: в логе игры под точкой нет поверхности в пределах 300 uu.' % (s['i'], s['p']), 'evidence': 'Saved/art/run_last.txt ARTPROBE %d' % s['i'], 'loot': s['i']})
            elif pr['below'][0] != '-' and z - pr['below'][1] > 60:
                sus.append({'kind': 'probe: the spot is above its surface', 'ru': 'Точка лута #%d %s задумана на z=%.0f, а поверхность под ней (%s) на z=%d: еда падает на %.0f.' % (s['i'], s['p'], z, pr['below'][0], pr['below'][1], z - pr['below'][1]),
                            'evidence': 'Saved/art/run_last.txt ARTPROBE %d' % s['i'], 'loot': s['i']})
    return sus

def main():
    t0 = time.time()
    objs, inst, mov, missing = build_objects()
    by = {}
    for o in objs: by[(o['mesh'], o['index'])] = o
    ob = lambda mesh, idx=0: by.get((mesh, idx))
    areas, walls, openings, spaces = build_areas()
    food = parse_food()
    containers = parse_containers(objs)
    interiors = build_interiors(ob)
    spots, moved = parse_loot_spots()
    probe, probe_time = read_probe()
    pm = {'same': 0, 'moved_since_log': []}
    if probe:
        for s in spots:
            pr = probe.get(s['i'])
            if pr and abs(pr['p'][0] - s['p'][0]) < 1.5 and abs(pr['p'][1] - s['p'][1]) < 1.5 and abs(pr['p'][2] - 35 - s['p'][2]) < 1.5:
                s['probe'] = {'above': pr['above'], 'below': pr['below'], 'ceil': pr['ceil']}; pm['same'] += 1
            elif pr:
                pm['moved_since_log'].append({'i': s['i'], 'now': s['p'], 'in_log': [pr['p'][0], pr['p'][1], pr['p'][2] - 35]})
    classify_loot(spots, objs, containers, spaces, interiors, food)
    extras = parse_loot_extras(food)
    for key in ('heavy', 'trophies', 'behind_doors'):
        for s in extras[key]: s.setdefault('tier_loot', 3)
        classify_loot(extras[key], objs, containers, spaces, interiors, food)
    for s in extras['heavy']: s['ev'] = s['first_item'].get('value', 0)
    for s in extras['trophies']: s['ev'] = s['value_whole_x2']
    dog, cat = parse_pets()
    events, event_when = build_events(parse_events())
    connectors = build_connectors(ob)
    hazards = build_hazards(dog)
    boxes = build_boxes()
    all_objs = objs + boxes
    for o in boxes: o.setdefault('mesh', None); o.setdefault('index', 0)
    interact = build_interactables(objs, extras, ob)
    base = (-4900.0, 1900.0, 1385.0)
    special = [
        {'ru': 'Брошенный груз (3 шт.) в яме под плиткой', 'p': P(5000, 2000, -350), 'value': 90, 'weight': 16, 'once': True, 'src': ref(GM, 'SpawnStash(FVector(5000.f, 2000.f, -350.f), 90, 16, 3);')},
        {'ru': 'Заначка в верхнем ящике: 3 сладости (tier 2, 3, 2) и оружие, при первом открытии', 'pts': [P(base[0] - 320, base[1] + 500, base[2]), P(base[0] + 250, base[1] + 900, base[2]), P(base[0] - 60, base[1] + 1350, base[2]), P(base[0] + 380, base[1] + 1250, base[2])],
         'p': P(-4900, 2900, 1385), 'value': round(2 * food['tiers'][2]['expected_value'] + food['tiers'][3]['expected_value']), 'once': True, 'src': ref(IA, 'const FVector Base = GetActorLocation() + FVector(0.f, -1100.f, 55.f);')},
        {'ru': 'Тостер: при каждом выстреле 2 куска (tier 0 и 1) на столешницу', 'pts': [P(-5700, 3820, 1745), P(-5740, 4560, 1745)], 'p': P(-5720, 4190, 1745), 'value': round(food['tiers'][0]['mean_value'] + food['tiers'][1]['mean_value']),
         'repeat_s': 7.8, 'src': ref(IA, 'GM->SpawnFood(FVector(-5700.f, 3820.f, 1745.f), 0);')},
        {'ru': 'Малыш роняет крошку (tier 1, в 30 % случаев tier 2) перед диваном, в 60 % циклов', 'p': P(13500, -600, 30), 'value': round(food['tiers'][1]['mean_value']), 'repeat_s': '16-31', 'src': ref(CR, 'GM->SpawnFood(At, FMath::FRand() < 0.3f ? 2 : 1);')},
        {'ru': 'Событие "пицца": 6 кусков по 45 (вес 9), 60 с', 'pts': [P(5100 + math.cos(i * math.pi / 3 + 0.3) * 380, -3500 + math.sin(i * math.pi / 3 + 0.3) * 380, 90) for i in range(6)], 'p': P(5100, -3500, 90), 'value': 270, 'event': 5,
         'src': ref(RUN, 'const FVector At = Where + FVector(FMath::Cos(A) * 380.f, FMath::Sin(A) * 380.f, APizzaBox::Thick + 30.f);')},
    ]
    rules = {
        'initial_ru': 'в начале смены каждая точка получает еду своего уровня с вероятностью 0,8; в 10 % случаев уровень на один выше', 'initial_src': ref(GM, 'if (Rng.FRand() < 0.8f)'),
        'refill_ru': 'каждые 10 с: если еды меньше 60 % от числа точек, до 4 случайных свободных точек получают еду', 'refill_src': ref(GM, 'const int32 Want = FMath::RoundToInt(SpotLocations.Num() * 0.6f);'),
        'heavy_ru': 'тяжёлые призы: в начале на всех 6 точках; потом, если тяжёлых (вес >= 20) меньше трёх, случайная точка получает случайный приз', 'heavy_src': ref(GM, 'if (Heavy < 3)'),
        'trophies_ru': 'трофеев одновременно 3, два из них внутри контейнеров; новый через 40 с после сдачи или поломки; донесённый целым стоит x2', 'trophies_src': ref(GM, 'if (Count < 3 && GetWorld()->GetTimeSeconds() - TrophyLostAt > 40.f)'),
        'behind_doors_ru': 'еда за дверями кладётся один раз в начале смены и не дозаполняется', 'behind_doors_src': ref(RUN, 'void AAntKitchenGameMode::SpawnContainerLoot()'),
        'break_ru': 'удар быстрее 1250 uu/с (падение примерно с 800 uu = 0,4 м и выше) разбивает еду на 2-6 кусков: обычная еда в сумме +10 %, трофей теряет x2', 'break_src': ref(KL, 'const int32 N = FMath::Clamp(Weight / 5, 2, 6);'),
        'reset_ru': 'в начале каждой смены вся еда удаляется и раскладывается заново', 'reset_src': ref(GM, 'void AAntKitchenGameMode::ResetMatch()'),
        'delivery_ru': 'сдача: груз в радиусе 950 от (-7900,-2500) и ниже z=500', 'delivery_src': ref(KL, 'FVector::Dist2D(L, AKitchenBuilder::NestLocation(CarrierTeam)) < AKitchenBuilder::NestRadius(CarrierTeam) && L.Z < 500.f'),
    }
    sus = [{'kind': k, 'ru': ru, 'evidence': ev} for (k, ru, ev) in SUSPICIOUS] + run_checks(all_objs, spots, hazards, connectors, probe)
    war = [{'ru': g[0], 'p': P(float(g[1]), float(g[2]), float(g[3])), 'radius': float(g[4]), 'src': src} for (g, cm, src) in
           parse_table(GM, 'static const FZone Zones[] =', 'static const FZone Stations[] =', r'\{ TEXT\("([^"]+)"\), ' + FV + r', ' + NUM + r' \}')]
    tiers = [
        {'id': 'floor', 'ru': 'Пол', 'z': '0..300'}, {'id': 'under', 'ru': 'Под мебелью', 'z': 'пол под мебелью с просветом: стол 1320, стулья 800, диван 260, журнальный стол 300, холодильник 120, кресло 114, тумба ТВ 38, плита 28, машина 600'},
        {'id': 'work', 'ru': 'Столешницы и столы', 'z': '300..2100: столешница 1710, плита 1740, стол 1400, стулья 880, журнальный стол 980, диван 950, тумба ТВ 1100'},
        {'id': 'top', 'ru': 'Верха и полки', 'z': 'от 2100: подоконник 2200, полка 2680, верх холодильника 3700, верх шкафов 4400, книжный шкаф до 4660, желоб 8960'},
        {'id': 'int', 'ru': 'Внутри', 'z': '16 контейнеров за дверями + под мойкой, ведро, коробка хлопьев, ботинок, носок, коробок, газета'},
        {'id': 'vent', 'ru': 'Вентиляция и за стенами', 'z': 'два воздуховода (потолочные участки на 5420), коридор за шкафами, комната мыши, яма под плиткой'}]
    data = {
        'meta': {'title': 'Ant Kitchen: level data', 'generated': time.strftime('%Y-%m-%d %H:%M'), 'units': 'Unreal units (uu); 1 m = 1900 uu; X west(-) -> east(+), Y south(-) -> north(+), floor Z = 0',
                 'tool': 'Tools/level/extract_level.py', 'mode': 'the modelled kitchen (KitchenArt::Enabled), heist rules (default)',
                 'exactness': {'exact (constants and tables in the code)': 'art placements and mover table (KitchenArtData.inl), mesh bounding boxes (Blender manifests), room shell, loot spots incl. the two seeded scatters, heavy / trophy / behind-door spots, containers, gadget positions, pet spots, hazard boxes, AI numbers',
                               'derived (computed here from those constants)': 'world boxes of rotated meshes, open poses of doors, tiers, expected loot values, attack reach, zones of the kid / grandma / dog, catapult landing, ramps of open doors, surfaces list',
                               'approximate': 'objects are drawn as bounding boxes (collision in the game is per triangle); table / chair legs and the armchair seat come from the box-built version'},
                 'probe_log': {'file': 'Saved/art/run_last.txt', 'time': probe_time, 'spots_matching': pm['same'], 'spots_changed_in_code_since_the_log': pm['moved_since_log']} if probe else None},
        'constants': build_constants(food), 'areas': areas, 'walls': walls, 'openings': openings, 'spaces': spaces, 'tiers': tiers,
        'objects': all_objs, 'containers': containers, 'interiors': interiors, 'surfaces': build_surfaces(ob), 'connectors': connectors, 'spawns': build_spawns(dog, cat),
        'loot': {'spots': spots, 'heavy': extras['heavy'], 'trophies': extras['trophies'], 'behind_doors': extras['behind_doors'], 'special': special, 'food': food, 'rules': rules, 'moved_table': [{'from': a, 'to': b, 'why': w} for (a, b, w) in moved]},
        'interactables': interact, 'hazards': hazards, 'human': build_human(), 'pets': {'dog': {'ru': 'Бадрик', 'spots': dog, 'speed_uu_s': 650, 'paw_radius': 110, 'length': 950, 'height': 520, 'note': 'ходит по прямой между точками, сквозь мебель (без коллизии); лапы давят на ходу', 'src': ref(CR, 'const float Spd = (Kind == 0 ? 650.f : 820.f) * (Fleeing > 0.f ? 1.7f : 1.f);')},
                                                                                           'cat': {'ru': 'Кошка', 'spots': cat, 'speed_uu_s': 820, 'paw_radius': 80, 'pounce_radius': 190, 'pounce_range': 900, 'note': 'между уровнями прыгает; проснувшись, прыгает на муравья ближе 900 на своём уровне (±400)', 'src': ref(CR, 'HurtAntsNear(GetWorld(), this, PounceTo, 190.f, 220.f, TEXT("придавлен кошкой"), 60);')}},
        'events': events, 'event_timing': event_when, 'secrets': [{'ru': a, 'p': P(*b), 'what': c, 'area': area_of(b[0], b[1]), 'src': 'docs/FEATURES.md (раздел 9, "Секреты") + KitchenBuilder.cpp LootSpots'} for (a, b, c) in SECRETS],
        'war_mode_only': {'note': 'только с -AntWar, в хайсте нет', 'territories': war},
        'unresolved': [{'ru': a, 'src': b} for (a, b) in UNRESOLVED] + [{'ru': 'В таблице арта есть меш без записи в манифесте: ' + n, 'src': 'KitchenArtData.inl'} for n in missing],
        'suspicious': sus,
    }
    data['stale_needles'] = STALE
    data['unresolved'] += [{'ru': 'Фрагмент кода не найден (транскрипция устарела): ' + s['needle'], 'src': s['file']} for s in STALE]
    by_tier = {}
    for s in spots: by_tier[s['tier']] = by_tier.get(s['tier'], 0) + 1
    data['counts'] = {'art_instances': len(objs), 'code_boxes': len(boxes), 'movers': len(mov), 'containers': len(containers), 'connectors': len(connectors), 'surfaces': len(data['surfaces']), 'spawns': len(data['spawns']),
                      'loot_spots': len(spots), 'loot_spots_by_tier': by_tier, 'heavy_spots': len(extras['heavy']), 'trophy_spots': len(extras['trophies']), 'behind_door_spots': len(extras['behind_doors']),
                      'interactables': len(interact), 'hazards': len(hazards), 'events': len(events), 'secrets': len(SECRETS), 'unresolved': len(data['unresolved']), 'suspicious': len(sus), 'stale_needles': len(STALE)}
    os.makedirs(OUT, exist_ok=True)
    p = os.path.join(OUT, 'level_data.json')
    with open(p, 'w', encoding='utf-8', newline='\n') as fo:
        json.dump(data, fo, ensure_ascii=False, indent=1)
    print('wrote %s (%.0f KB) in %.1f s' % (p, os.path.getsize(p) / 1024.0, time.time() - t0))
    print(json.dumps(data['counts'], ensure_ascii=True))
    if probe: print('probe log %s: %d spots identical, %d changed in the code since' % (probe_time, pm['same'], len(pm['moved_since_log'])))
    for s in STALE: print('STALE NEEDLE %s: %s' % (s['file'], s['needle'].encode('ascii', 'backslashreplace').decode()))
    return data

if __name__ == '__main__':
    main()
