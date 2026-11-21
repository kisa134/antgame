# -*- coding: utf-8 -*-
# Ant Kitchen, level v3: the art and design pass over level v2, as data. No bpy, no file IO.
# Imported by
#   Tools/level/level_v3_lib.py          (applies BASE and a seed to docs/level/level_v2.json -> one level state)
#   Tools/level/build_level_v3.py        (docs/level/level_v3.json + SourceArt/Blender/level_v3/level_v3_manifest.json)
#   Tools/level/check_level_v3.py        (validates every district variant and >= 200 seeds)
#   Tools/level/build_level_map_v3.py    (docs/level/level_map_v3.html)
#   Tools/blender/level_v3_pieces.py     (the new meshes are built to the boxes in PIECES)
# Units and frames are those of v2 (Tools/level/level_v2_layout.py): Unreal units, 1 m = 1900 uu, X west(-) -> east(+), Y south(-) -> north(+),
# floor Z = 0. Yaw as in the game: yaw 180 faces south, 0 north, 90 west, -90 east.
# A piece box is (x0, x1, y0, y1, z0, z1) in the piece's own Blender frame (front = -Y, back = +Y, origin = the placement point).
# v2 stays the source of truth for everything this file does not name. Nothing here is in the game until the programmer integrates it
# (docs/level/LEVEL_V3_INTEGRATION.md); every mechanic that the code does not have yet is marked needs_code.
UU = 1900.0
VERSION = 'v3.0 (2026-11-21)'

# ---------------------------------------------------------------------------------------------------------------------------------
# 1. The story. One family, one evening. Everything placed in the flat answers "whose is it and why is it here".
# ---------------------------------------------------------------------------------------------------------------------------------
STORY = {
    'ru': 'Пятница, вечер накануне дня рождения малыша. Папа (Человек) с работы, переоделся в майку и пижамные штаны, сидит в телефоне; '
          'бабушка (его мама) живёт с ними, днём испекла торт и спрятала его от внука, вечером вяжет внуку шарф; малыш носится по гостиной, '
          'где папа уже повесил гирлянду и шарики; Бадрик выпрашивает еду, кошка спит где повыше. Мамы этим вечером дома нет (ночная смена: '
          'её кружка, её фото на стене). Квартира в начале рейда спокойная и прибранная: посуда в сушилке, на столе скатерть, игрушки в ящике. '
          'Каждый беспорядок, который игроки увидят к концу рейда, сделали они.',
    'who': [
        {'id': 'human', 'ru': 'Папа (Человек)', 'look': 'майка, полосатые пижамные штаны, розовые тапки, усы (human_build.py)', 'owns': ['телефон на диване', 'газета', 'кружка с надписью', 'кресло-место на диване слева'],
         'evening': 'телефон на диване -> холодильник -> плита (греет суп) -> ужин за столом -> посуда -> мусор в коридор -> диван, телевизор -> дремлет'},
        {'id': 'grandma', 'ru': 'Бабушка', 'look': 'седой пучок, очки, сливовая кофта, зелёные тапки (npc_folk.py)', 'owns': ['кресло, столик с чаем', 'вязание и клубок', 'очки', 'сумка-тележка', 'цветы на подоконниках', 'стремянка'],
         'evening': 'вяжет в кресле -> поливает цветы -> ставит чайник -> дремлет в кресле'},
        {'id': 'kid', 'ru': 'Малыш (4 года, завтра день рождения)', 'look': 'большая голова, вихор, жёлтая футболка, красные кеды', 'owns': ['ящик с игрушками', 'железная дорога', 'рисунки на холодильнике', 'детский стульчик'],
         'evening': 'играет на ковре -> перекус на стульчике -> мультики -> строит шалаш'},
        {'id': 'dog', 'ru': 'Бадрик (французский бульдог)', 'owns': ['лежанка у батареи', 'миски и мешок корма', 'теннисный мяч'], 'evening': 'миска -> клянчит под столом во время ужина -> лежанка'},
        {'id': 'cat', 'ru': 'Кошка Муся', 'owns': ['когтеточка', 'кошачья дверца', 'верх холодильника и книжного шкафа'], 'evening': 'спит где выше всех -> подоконник -> во двор через дверцу'},
    ],
}

# ---------------------------------------------------------------------------------------------------------------------------------
# 2. Look: colour and light per room and per time of day. The renders (Tools/blender/level_v3_preview.py) use exactly these numbers.
#    rgb are linear 0..1, energies are Blender watts for area/point lights and W/m2 for the sun.
# ---------------------------------------------------------------------------------------------------------------------------------
ROOM_LOOK = {
    'kitchen': {'ru': 'Кухня: шалфейные фасады, тёплое дерево, кремовая плитка, белёная стена; акцент — красный холодильник (один красный предмет на комнату)',
                'wall': (0.90, 0.84, 0.72), 'floor_a': (0.86, 0.80, 0.70), 'floor_b': (0.62, 0.50, 0.40), 'floor_tile_uu': 570, 'ceiling': (0.94, 0.93, 0.90),
                'accent': 'красный холодильник', 'palette_rule': 'не больше одного насыщенного цвета на 3 м стены; игрушечные красный/синий/жёлтый только у вещей малыша'},
    'living':  {'ru': 'Гостиная: приглушённо-зелёные обои, дубовый паркет, тёмно-бирюзовый диван, тёплый свет торшера; акцент — праздничная гирлянда и шарики',
                'wall': (0.62, 0.70, 0.60), 'floor_a': (0.55, 0.38, 0.22), 'floor_b': (0.48, 0.32, 0.18), 'ceiling': (0.93, 0.92, 0.88),
                'accent': 'гирлянда + шарики (праздник)', 'palette_rule': 'обои и пол спокойные, цвет даёт свет и праздник'},
    'yard':    {'ru': 'Двор: пыльная трава, серый бетон отмостки, белый забор; вечером — длинные тени и розовое небо', 'grass': (0.28, 0.44, 0.16)},
}
# material looks v3 asks for (KitchenArt.cpp slot looks / new slots; the renders already use them): one saturated accent per room
PALETTE_V3 = {
    'slots': {'Carpet': (0.56, 0.33, 0.30), 'CarpetTrim': (0.80, 0.72, 0.58), 'SofaFabric': (0.12, 0.36, 0.38)},
    'piece_slots': {'SM_K_Rug/Red': (0.60, 0.36, 0.26), 'SM_K_Rug/Paper': (0.88, 0.82, 0.70)},
    'why': 'красный ковёр кухни и красно-белый ковёр гостиной — главный источник «игрушечной» насыщенности сверху: терракота и пыльная роза; '
           'кухонному ковру нужен свой слот (сейчас он делит Red со всеми красными вещами)',
}
TIME_OF_DAY = {
    'day':     {'ru': 'День (14:00)', 'sun': {'energy': 4.0, 'elev': 52, 'azim': -20, 'rgb': (1.0, 0.96, 0.90)}, 'sky': {'rgb': (0.78, 0.88, 1.0), 'strength': 0.40},
                'lamps_on': [], 'lamp_w': 0, 'tv': False, 'note': 'солнце с юга, окна кухни и гостиной дают пятна на полу; лампы выключены',
                'gameplay': 'Человек видит далеко; тени у южных стен; под мебелью темно'},
    'evening': {'ru': 'Вечер (19:30)', 'sun': {'energy': 2.4, 'elev': 9, 'azim': -62, 'rgb': (1.0, 0.62, 0.36)}, 'sky': {'rgb': (0.55, 0.46, 0.62), 'strength': 0.30},
                'lamps_on': ['pendant', 'hood', 'floor_lamp', 'garland'], 'lamp_w': 70, 'tv': False, 'note': 'низкое оранжевое солнце через окна, тёплые лампы над столом и у бабушки',
                'gameplay': 'длинные тени от ножек мебели — полосы укрытия по свету (нужен код, чтобы ИИ это учитывал)'},
    'night':   {'ru': 'Ночь (23:00)', 'sun': {'energy': 0.0, 'elev': -10, 'azim': 0, 'rgb': (0.5, 0.6, 1.0)}, 'sky': {'rgb': (0.05, 0.07, 0.14), 'strength': 0.25},
                'lamps_on': ['hood', 'tv', 'street'], 'lamp_w': 40, 'tv': True, 'note': 'горит вытяжка, мерцает телевизор, с улицы фонарь; большая часть пола в темноте',
                'gameplay': 'как «выбило пробки» по видимости (тёмный радиус 2200 у ИИ Человека уже есть в коде), но Человек дремлет на диване'},
}
LIGHTS = {   # where the practical lights stand (game units): used by the renders and as data for the programmer
    'pendant':    {'p': (500, -500, 3500), 'rgb': (1.0, 0.78, 0.52), 'kind': 'point', 'ru': 'лампа над обеденным столом'},
    'hood':       {'p': (-750, 4500, 3250), 'rgb': (1.0, 0.86, 0.66), 'kind': 'area', 'ru': 'подсветка вытяжки'},
    'floor_lamp': {'p': (9900, -7450, 3900), 'rgb': (1.0, 0.72, 0.45), 'kind': 'point', 'ru': 'торшер бабушки'},
    'garland':    {'p': (13500, -4300, 4300), 'rgb': (1.0, 0.70, 0.40), 'kind': 'point', 'ru': 'тёплая подсветка гирлянды'},
    'tv':         {'p': (13150, -8300, 2200), 'rgb': (0.45, 0.62, 1.0), 'kind': 'area', 'ru': 'экран телевизора'},
    'street':     {'p': (-10500, 0, 6500), 'rgb': (1.0, 0.70, 0.40), 'kind': 'point', 'ru': 'уличный фонарь'},
    'fridge':     {'p': (3700, 3900, 1500), 'rgb': (0.85, 0.92, 1.0), 'kind': 'point', 'ru': 'свет в открытом холодильнике'},
}
DENSITY = [
    'Три плотности: ОЧАГ (вокруг дела: мойка-столешница, плита, стол, диван-ТВ, кресло бабушки, ковёр малыша) — 5–9 предметов на 2 м², все про одно дело; '
    'ДОРОГА (где ходит Человек: кольцо вокруг стола, проход кухня–гостиная, полоса перед диваном) — пусто, кроме одной вещи-истории на 3 м; '
    'КРОМКА (вдоль стен и под мебелью) — укрытия муравья: мешки, коробки, обувь, провода, батарея.',
    'Мелочь на полу — только со смыслом: упало со стола над ней, выпало из сумки, оставил малыш на пути из ящика игрушек. Ничего не лежит «посреди ничего».',
    'Повтор: не больше трёх одинаковых предметов в поле зрения Человека; одинаковое — только в ряду, где оно и в жизни одинаковое (книги, тарелки в сушилке, банки специй).',
    'Масштаб: всё, что человек держит в руке, — реального размера (кружка 12 см = 230 uu, тарелка 26 см = 500 uu, банка 8 см = 150 uu). В v2 кружки, тарелки и банки были в 1,7–2,6 раза больше.',
    'Высоты: у каждой стены есть что-то на высоте глаз Человека (картина, полка, часы, рисунок) и что-то под потолком (карниз, лампа, шарики) — иначе комната читается коробкой.',
    'Масштаб муравья: читаются фактуры и кромки (шов плитки, торец столешницы, шуруп как люк, нитка как канат). Детали меньше 15 uu (длина муравья) моделируем только у того, к чему муравей подходит вплотную: шурупы, петли, кольца, крышки.',
]

# ---------------------------------------------------------------------------------------------------------------------------------
# 3. New meshes (Tools/blender/level_v3_pieces.py builds each to its box). mat = material / break class (BREAK), cloth = simulation-ready
#    sheet (single layer, quads, pin row named), part_of = the object it belongs to (fasteners are separate pieces with their own pivot).
# ---------------------------------------------------------------------------------------------------------------------------------
BREAK = {
    'glass':      {'ru': 'стекло', 'breaks_from_uu': 450, 'result': 'бьётся на осколки (SM_V3_GlassShards): осколки режут муравья (урон 10/с, «порезан»), содержимое вытекает', 'noise': 0.9},
    'ceramic':    {'ru': 'керамика', 'breaks_from_uu': 700, 'result': 'бьётся на 3–5 крупных черепков (укрытие и ступени), содержимое высыпается', 'noise': 0.8},
    'terracotta': {'ru': 'терракота', 'breaks_from_uu': 700, 'result': 'горшок бьётся, земля горкой (SM_V3_SoilHeap): пандус и укрытие', 'noise': 0.7},
    'pet':        {'ru': 'пластиковая бутылка', 'breaks_from_uu': None, 'result': 'не бьётся; откручена крышка и бутылка лежит — жидкость вытекает 20 с, лужа растёт', 'noise': 0.4},
    'plastic':    {'ru': 'пластик', 'breaks_from_uu': None, 'result': 'не бьётся, отскакивает', 'noise': 0.4},
    'cardboard':  {'ru': 'картон', 'breaks_from_uu': None, 'result': 'не бьётся; муравьи прогрызают дыру (8 с), клапаны открываются, от удара мнётся', 'noise': 0.2},
    'paper':      {'ru': 'бумага', 'breaks_from_uu': None, 'result': 'рвётся по линии укусов; падая, планирует', 'noise': 0.05},
    'cloth':      {'ru': 'ткань', 'breaks_from_uu': None, 'result': 'тянется (симуляция), рвётся по линии укусов, падает кучей — новая гора-укрытие', 'noise': 0.1},
    'latex':      {'ru': 'латекс (шарик)', 'breaks_from_uu': None, 'result': 'лопается от кислоты или укуса: громкость 1,0 — Человек, бабушка и Бадрик идут смотреть', 'noise': 1.0},
    'wood':       {'ru': 'дерево', 'breaks_from_uu': None, 'result': 'не бьётся; держится на крепеже', 'noise': 0.6},
    'metal':      {'ru': 'металл', 'breaks_from_uu': None, 'result': 'не бьётся; громко падает', 'noise': 0.9},
    'yarn':       {'ru': 'пряжа', 'breaks_from_uu': None, 'result': 'распускается: нитку можно утащить (длина растёт), вязание укорачивается', 'noise': 0.0},
    'food':       {'ru': 'еда', 'breaks_from_uu': 800, 'result': 'как сейчас в коде: бьётся на куски', 'noise': 0.2},
}
FASTENER = {
    'screw':  {'ru': 'шуруп', 'undo': 'двое муравьёв встают на шляпку и вращают её как ворот (держать [E]): сила 14 — 8 с, 28 — 4 с; полуоткрученный («шатается») — вдвое быстрее',
               'redo': 'Человек закручивает отвёрткой (рутина «чинит», 12 с) — только если заметил', 'piece': 'SM_V3_Screw'},
    'hinge':  {'ru': 'петля (3 шурупа)', 'undo': 'выкрутить два из трёх шурупов петли: дверца висит на одной петле и приоткрыта навсегда; обе петли — дверца падает', 'piece': 'SM_V3_Hinge'},
    'nail':   {'ru': 'гвоздь', 'undo': 'нельзя выдернуть; можно перегрызть шнурок рамки (6 с) — рамка падает', 'piece': 'SM_V3_Nail'},
    'magnet': {'ru': 'магнит', 'undo': 'столкнуть рывком (Ctrl) двоим одновременно — магнит и то, что он держал, падают', 'piece': 'SM_K_FridgeMagnet'},
    'tape':   {'ru': 'скотч', 'undo': 'отклеить уголок (держать [E], 3 с) — лист повисает на втором куске; оба — падает и планирует', 'piece': 'SM_V3_TapeStrip'},
    'ring':   {'ru': 'кольцо-зажим шторы', 'undo': 'разжать зажим (держать [E] вдвоём, 4 с): штора провисает; 3 кольца из 6 — рвётся под своим весом', 'piece': 'SM_V3_CurtainRing'},
    'knot':   {'ru': 'узел (нитка, лента)', 'undo': 'перегрызть (сила любая, 5 с)', 'piece': None},
    'cap':    {'ru': 'винтовая крышка', 'undo': 'трое вращают крышку (держать [E], 6 с); крышка падает', 'piece': 'SM_V3_ColaCap'},
    'needle': {'ru': 'спица', 'undo': 'вытащить спицу (тянуть вдвоём, 4 с): петли спускаются, шарф распускается', 'piece': 'SM_V3_KnitNeedle'},
    'friction': {'ru': 'стоит сам', 'undo': 'толкнуть рывком (Ctrl) — уже есть в коде для кружек, банок, тарелки', 'piece': None},
}
def _p(ru, box, cat, mat, **kw):
    d = {'ru': ru, 'box': box, 'cat': cat, 'mat': mat}
    d.update(kw)
    return d
PIECES = {
    # fasteners: separate small pieces, origin = their pivot
    'SM_V3_Screw':        _p('Шуруп с крестовой шляпкой (шляпка 22 uu = 12 мм: муравей встаёт на неё целиком)', (-11, 11, -6, 60, -11, 11), 'fastener', 'metal', pivot='центр шляпки, ось +Y локально = в стену'),
    'SM_V3_ShelfBracket': _p('Уголок-кронштейн полки, 2 отверстия под шурупы в стену', (-16, 16, -380, 0, 0, 240), 'fastener', 'metal', pivot='верх стенки уголка у стены'),
    'SM_V3_Hinge':        _p('Мебельная петля (чашка + плечо + планка), 3 отверстия', (-30, 30, -80, 0, -50, 50), 'fastener', 'metal', pivot='ось петли'),
    'SM_V3_RodBracket':   _p('Кронштейн карниза: пластина на 2 шурупа + вынос 260', (-40, 40, -280, 0, -90, 90), 'fastener', 'metal', pivot='центр пластины на стене'),
    'SM_V3_CurtainRing':  _p('Кольцо карниза с зажимом-крокодилом', (-36, 36, -36, 36, -75, 36), 'fastener', 'metal', pivot='ось карниза'),
    'SM_V3_Nail':         _p('Гвоздь для картины', (-9, 9, -40, 60, -9, 9), 'fastener', 'metal', pivot='точка входа в стену'),
    'SM_V3_TapeStrip':    _p('Кусочек скотча', (-60, 60, -3, 0, -20, 20), 'fastener', 'paper', pivot='центр'),
    'SM_V3_KnitNeedle':   _p('Спица (вязание бабушки)', (-330, 330, -10, 10, -10, 10), 'fastener', 'metal', pivot='середина'),
    'SM_V3_ColaCap':      _p('Винтовая крышка бутылки', (-28, 28, -28, 28, 0, 38), 'fastener', 'plastic', pivot='низ'),
    # objects
    'SM_V3_WallShelf':    _p('Полка-доска 1160 x 237 мм на двух уголках', (-1100, 1100, -450, 0, 0, 40), 'furn', 'wood'),
    'SM_V3_JamJar':       _p('Стеклянная банка варенья (8 см) с этикеткой', (-80, 80, -80, 80, 0, 200), 'prop', 'glass'),
    'SM_V3_JamJar_Lid':   _p('Крышка банки (клетчатая ткань под резинкой)', (-86, 86, -86, 86, 0, 32), 'part', 'cloth'),
    'SM_V3_PickleJar':    _p('Банка огурцов 1 л', (-110, 110, -110, 110, 0, 330), 'prop', 'glass'),
    'SM_V3_GlassShards':  _p('Осколки банки (состояние «после»)', (-320, 320, -260, 260, 0, 40), 'prop', 'glass', state='after'),
    'SM_V3_JamSplat':     _p('Клякса варенья (состояние «после»)', (-380, 380, -260, 260, 0, 6), 'struct', 'food', state='after'),
    'SM_V3_ColaBottle':   _p('Бутылка колы 2 л, ПЭТ', (-100, 100, -100, 100, 0, 610), 'prop', 'pet'),
    'SM_V3_ColaPuddle':   _p('Лужа колы (липко; состояние «после»)', (-950, 950, -620, 620, 0, 5), 'struct', 'food', state='after'),
    'SM_V3_CurtainRod':   _p('Карниз: труба 36 мм с наконечниками', (-2180, 2180, -30, 30, -30, 30), 'conn', 'metal'),
    'SM_V3_CurtainPanel': _p('Штора: полотно в складку, сетка 24 x 90 квадов, верхний ряд — точки крепления к кольцам', (-300, 300, -90, 90, 0, 5230), 'conn', 'cloth',
                             cloth={'grid': [24, 90], 'pin': 'top row (vertex group "pin")', 'mass_g_m2': 220, 'states': ['висит', 'сорвана с колец (куча)', 'порвана']}),
    'SM_V3_CurtainTorn':  _p('Сорванная порванная штора на полу (состояние «после»)', (-1000, 1000, -800, 800, 0, 320), 'prop', 'cloth', state='after'),
    'SM_V3_Tablecloth':   _p('Скатерть в клетку, свес 450', (-2270, 2270, -1370, 1370, 0, 456), 'conn', 'cloth',
                             cloth={'grid': [64, 40], 'pin': 'нет: лежит на столе, держится трением и весом посуды', 'mass_g_m2': 180, 'states': ['лежит', 'стянута за край (посуда едет)', 'на полу']}),
    'SM_V3_TableclothLong': _p('Длинная скатерть до пола (шатёр под столом)', (-2270, 2270, -1370, 1370, 0, 1366), 'conn', 'cloth',
                             cloth={'grid': [64, 56], 'pin': 'нет', 'mass_g_m2': 180, 'states': ['лежит', 'стянута', 'на полу']}),
    'SM_V3_TableclothPulled': _p('Скатерть, стянутая со стола (состояние «после»)', (-2400, 2400, -1700, 1700, 0, 1410), 'conn', 'cloth', state='after'),
    'SM_V3_Teapot':       _p('Заварочный чайник (фарфор)', (-300, 260, -180, 180, 0, 360), 'prop', 'ceramic'),
    'SM_V3_Teapot_Lid':   _p('Крышка заварочного чайника', (-95, 95, -95, 95, 0, 60), 'part', 'ceramic'),
    'SM_V3_TeaCup':       _p('Чашка на блюдце', (-140, 140, -140, 140, 0, 130), 'prop', 'ceramic'),
    'SM_V3_StepStool':    _p('Табурет-стремянка бабушки: ступени 450 и 900', (-350, 350, -450, 450, 0, 900), 'furn', 'wood'),
    'SM_V3_CerealBox':    _p('Коробка хлопьев с открытым клапаном', (-190, 190, -70, 70, 0, 600), 'prop', 'cardboard'),
    'SM_V3_CerealSpill':  _p('Рассыпанные хлопья (состояние «после»)', (-750, 750, -480, 480, 0, 30), 'struct', 'food', state='after'),
    'SM_V3_PictureFrame': _p('Рамка с фотографией на шнурке', (-450, 450, -45, 0, -700, 0), 'deco', 'glass'),
    'SM_V3_KidDrawing':   _p('Детский рисунок А4 (лист бумаги)', (-200, 200, -4, 0, -285, 285), 'deco', 'paper'),
    'SM_V3_Balloon':      _p('Гелиевый шарик 25 см', (-240, 240, -240, 240, 0, 640), 'prop', 'latex'),
    'SM_V3_BalloonString': _p('Ленточка шарика (длина 4600, масштаб по Z)', (-6, 6, -6, 6, 0, 4600), 'conn', 'yarn'),
    'SM_V3_BalloonWeight': _p('Грузик шариков', (-70, 70, -55, 55, 0, 95), 'prop', 'plastic'),
    'SM_V3_GiftBox':      _p('Подарочная коробка', (-450, 450, -350, 350, 0, 520), 'prop', 'cardboard'),
    'SM_V3_GiftBox_Lid':  _p('Крышка подарка с бантом', (-470, 470, -370, 370, 0, 150), 'part', 'cardboard'),
    'SM_V3_ToyBox':       _p('Деревянный ящик для игрушек', (-700, 700, -450, 450, 0, 780), 'furn', 'wood'),
    'SM_V3_ToyBox_Lid':   _p('Крышка ящика на двух петлях (ось — задняя кромка)', (-720, 720, -920, 0, 0, 60), 'part', 'wood', motion={'type': 'hinge', 'axis': 'X', 'angle_deg': 100}),
    'SM_V3_KnitScarf':    _p('Начатый шарф на спицах (пряжа)', (-380, 380, -230, 230, 0, 70), 'prop', 'yarn'),
    'SM_V3_Glasses':      _p('Очки бабушки', (-140, 140, -80, 80, 0, 55), 'prop', 'glass'),
    'SM_V3_Phone':        _p('Телефон папы', (-80, 80, -155, 155, 0, 18), 'prop', 'plastic'),
    'SM_V3_Cornice':      _p('Потолочный карниз-галтель 2 м (масштаб по X)', (-1000, 1000, -130, 0, -130, 0), 'struct', 'wood'),
    'SM_V3_Outlet':       _p('Розетка с рамкой на одном винте', (-85, 85, -30, 0, -85, 85), 'deco', 'plastic'),
    'SM_V3_PendantLamp':  _p('Подвесная лампа над столом со шнуром до потолка', (-480, 480, -480, 480, 0, 1900), 'deco', 'metal'),
    'SM_V3_DoorMat':      _p('Коврик у двери в коридор', (-650, 650, -400, 400, 0, 18), 'struct', 'cloth'),
    'SM_V3_ShoppingTrolley': _p('Сумка-тележка бабушки (с картошкой и луком)', (-340, 340, -330, 330, 0, 1800), 'furn', 'cloth'),
    'SM_V3_NewspaperStack': _p('Стопка газет, перевязанная бечёвкой', (-420, 420, -300, 300, 0, 520), 'prop', 'paper'),
    'SM_V3_BlanketFort':  _p('Шалаш малыша: плед с дивана на журнальный стол', (-1800, 1800, -1300, 1300, 0, 1050), 'conn', 'cloth',
                             cloth={'grid': [48, 36], 'pin': 'края на диване и столе прижаты подушками', 'mass_g_m2': 400, 'states': ['шалаш', 'стянут']}),
    'SM_V3_TrashBag':     _p('Мешок с мусором (завязан)', (-380, 380, -330, 330, 0, 620), 'prop', 'plastic'),
    'SM_V3_LaundryPole':  _p('Столб бельевой верёвки', (-60, 60, -60, 60, 0, 2900), 'furn', 'metal'),
    'SM_V3_LaundryLine':  _p('Бельевая верёвка 6 м (масштаб по X)', (0, 6000, -5, 5, -5, 5), 'conn', 'yarn'),
    'SM_V3_SoilHeap':     _p('Разбитый горшок: черепки и земля (состояние «после»)', (-400, 400, -350, 350, 0, 120), 'prop', 'terracotta', state='after'),
    'SM_V3_DirtyDishStack': _p('Гора грязной посуды в раковине', (-500, 500, -500, 500, 0, 450), 'prop', 'ceramic'),
    'SM_V3_DogFoodBag':   _p('Мешок корма Бадрика (открыт, завёрнут)', (-300, 300, -200, 200, 0, 900), 'prop', 'paper'),
    'SM_V3_Backpack':     _p('Рабочий рюкзак папы, брошен у стола', (-330, 330, -230, 230, 0, 820), 'prop', 'cloth'),
    'SM_V3_VeggieCrate':  _p('Ящик с картошкой и луком (деревянные рейки)', (-450, 450, -300, 300, 0, 450), 'furn', 'wood'),
    'SM_V3_FloorCushion': _p('Большая напольная подушка (малыш смотрит мультики)', (-550, 550, -550, 550, 0, 300), 'prop', 'cloth'),
}

# ---------------------------------------------------------------------------------------------------------------------------------
# 4. BASE: what v3 changes for every seed (before the district variants). ids are v2 object ids (docs/level/level_v2.json).
# ---------------------------------------------------------------------------------------------------------------------------------
REMOVE = {
    'art157': 'декоративный целый торт на обеденном столе: второй торт путал цель (настоящий спрятан в одном из мест торта); на столе — ужин',
    'new76': 'кубик посреди кухни: игрушки на кухне делали из неё полосу препятствий -> в ящик для игрушек (содержимое)',
    'new77': 'кубик посреди кухни -> в ящик для игрушек',
    'new79': 'кубик-арка у холодильника -> в ящик для игрушек',
    'new80': 'кубик-арка у мойки -> в ящик для игрушек',
    'new85': 'грузовик посреди кухни -> в ящик для игрушек',
    'copy14': 'детский носок посреди кухни: «упал ниоткуда»',
    'new12': 'пустая бутылка посреди кухни: «бутылки по полу» — свалка, а не кухня; две остались у ведра (на сдачу)',
    'new15': 'пустая бутылка у стола -> убрана',
    'new38': 'рассыпанный сахар с начала рейда -> теперь беспорядок по сиду (SCATTER) и результат цепочек',
    'new39': 'лужа варенья с начала рейда -> беспорядок по сиду и результат цепочки «полка»',
    'new74': 'одинокий кубик у дивана -> в ящик', 'new75': 'одинокий кубик у кресла -> в ящик',
    'new81': 'арка у лежанки -> в ящик', 'new82': 'арка у растения -> в ящик', 'new83': 'арка у дороги -> в ящик',
    'new84': 'грузовик у кресла -> в ящик',
    'art298': 'пять одинаковых комков пыли в ряд под диваном -> осталось три', 'art300': 'то же',
    'new28': 'шесть одинаковых магнитов -> четыре + два рисунка на скотче', 'new32': 'то же',
    'art96': 'монолитная настенная полка -> полка v3: доска на двух уголках и четырёх шурупах (откручивается)',
    'art137': 'банка варенья на полке -> стеклянная банка v3 (бьётся)',
    'art308': 'плинтус восточной стены гостиной шёл сквозь новую дверь в коридор -> два отрезка',
    'art93': 'штора-коробка -> штора v3 (ткань в складку на кольцах)', 'art94': 'то же', 'art95': 'карниз -> карниз v3 на двух кронштейнах (по 2 шурупа)',
    'copy3': 'штора гостиной -> штора v3', 'copy4': 'то же', 'copy5': 'карниз гостиной -> карниз v3',
}
# moves: id -> (x, y) of the object's centre, yaw (None = keep), z0 (None = keep), why
MOVE = {
    'art163': ((1300, -4000), None, None, 'газета-шалаш отодвинута от стены: под окном теперь батарея кухни'),
    'art79':  ((1200, 1290), 180, None, 'северный стул задвинут под стол: в начале рейда порядок; и проход между стулом и шкафами (был 736 < 780 капсулы Человека) стал 1136 — приоткрытая дверца шкафа больше не отрезает мойку и плиту'),
    'new4':   ((19250, -1380), 90, None, 'гостевые тапки — парой на коврике у двери в коридор, а не посреди кухни'),
    'new5':   ((19250, -620), 90, None, 'вторая гостевая тапка'),
    'new6':   ((8600, -700), 0, None, 'тапки малыша — парой у обувницы (на их месте теперь ящик для игрушек)'),
    'new7':   ((9250, -700), 0, None, 'вторая тапка малыша'),
}
# per-instance scale of v2 props that were 1.7-2.6 times real size (visual; the pushable physics props of the code are separate actors)
SCALE = {
    'art140': 0.55, 'art141': 0.55, 'copy11': 0.55,                 # mugs 31 cm -> 17 cm (with handle)
    'art142': 0.65, 'art143': 0.65, 'copy8': 0.65, 'copy9': 0.65, 'copy10': 0.65,   # plates 47 cm -> 31 cm
    'art135': 0.6, 'art136': 0.6, 'copy12': 0.6,                    # jam jars 19 cm -> 12 cm
    'art138': 0.7, 'art139': 0.6, 'art150': 0.75, 'art152': 0.8,    # sugar jar, sugar bowl, soup pot, kettle
    'new24': 0.75, 'art181': 0.75, 'art185': 0.6,                   # fruit bowl, snack bowl, soda can
    'art109': 0.75,   # the floor cereal box (the «tunnel»): 72 cm -> 54 cm; 0.6 opened a 4.17 s hole in the cover by the broom (check_level_v3). Still 1.8x real: v3.1 — a delivery box mesh
}
def _a(id, piece, p, yaw=180, ru=None, area=None, mount='floor', district=None, scale=None, hide=None, cat=None, copy=False, **kw):
    d = {'id': id, 'piece': piece, 'p': list(p), 'yaw': yaw, 'ru': ru, 'area': area, 'mount': mount, 'district': district, 'copy': copy}
    if scale is not None: d['scale'] = scale if isinstance(scale, (list, tuple)) else [scale, scale, scale]
    if hide: d['hide'] = hide
    if cat: d['cat'] = cat
    d.update(kw)
    return d
def _cornice(prefix, runs, area):
    out = []
    for k, (a, b, fixed, side) in enumerate(runs):
        L = b - a; m = (a + b) / 2.0
        if side in ('n', 's'): p, yaw = (m, fixed, 5400), (180 if side == 'n' else 0)
        else: p, yaw = (fixed, m, 5400), (-90 if side == 'w' else 90)
        out.append(_a('%s%d' % (prefix, k), 'SM_V3_Cornice', p, yaw, 'Потолочный карниз', area, 'wall', scale=[L / 2000.0, 1, 1]))
    return out
def _screws(prefix, pts, yaw, area, part_of):
    return [_a('%s%d' % (prefix, k), 'SM_V3_Screw', p, yaw, 'Шуруп', area, 'wall', part_of=part_of) for k, p in enumerate(pts)]
def _rings(prefix, xs, y, z, area, part_of):
    return [_a('%s%d' % (prefix, k), 'SM_V3_CurtainRing', (x, y, z), 0, 'Кольцо шторы', area, 'hang', part_of=part_of) for k, x in enumerate(xs)]

ADD = []
# --- kitchen: the wall shelf over the nest (chain reaction no. 1)
ADD += [_a('v3_shelf', 'SM_V3_WallShelf', (-7000, 0, 2640), -90, 'Полка над гнездом (доска на уголках)', 'kitchen', 'wall', tops=[2680], replaces='art96'),
        _a('v3_shelf_br0', 'SM_V3_ShelfBracket', (-7000, -800, 2400), -90, 'Уголок полки (южный)', 'kitchen', 'wall', part_of='v3_shelf'),
        _a('v3_shelf_br1', 'SM_V3_ShelfBracket', (-7000, 800, 2400), -90, 'Уголок полки (северный)', 'kitchen', 'wall', part_of='v3_shelf')]
ADD += _screws('v3_shelf_sc', [(-6992, -800, 2460), (-6992, -800, 2580), (-6992, 800, 2460), (-6992, 800, 2580)], -90, 'kitchen', 'v3_shelf')
ADD += [_a('v3_jam', 'SM_V3_JamJar', (-6790, -350, 2680), -90, 'Банка варенья (стекло)', 'kitchen', 'on:v3_shelf', replaces='art137'),
        _a('v3_jam_lid', 'SM_V3_JamJar_Lid', (-6790, -350, 2880), -90, 'Крышка банки', 'kitchen', 'on:v3_jam', part_of='v3_jam'),
        _a('v3_pickles', 'SM_V3_PickleJar', (-6800, 700, 2680), -90, 'Банка огурцов (стекло)', 'kitchen', 'on:v3_shelf')]
# --- kitchen window: rod on two brackets, rings, cloth panels (chain reaction no. 2)
ADD += [_a('v3_rod', 'SM_V3_CurtainRod', (0, -4730, 5320), 0, 'Карниз кухни', 'kitchen', 'hang', replaces='art95'),
        _a('v3_rodbr0', 'SM_V3_RodBracket', (-2000, -5000, 5320), 0, 'Кронштейн карниза (запад)', 'kitchen', 'wall', part_of='v3_rod'),
        _a('v3_rodbr1', 'SM_V3_RodBracket', (2000, -5000, 5320), 0, 'Кронштейн карниза (восток)', 'kitchen', 'wall', part_of='v3_rod')]
ADD += _screws('v3_rod_sc', [(-2000, -4990, 5265), (-2000, -4990, 5375), (2000, -4990, 5265), (2000, -4990, 5375)], 0, 'kitchen', 'v3_rod')
ADD += _rings('v3_ring_w', [-2010, -1910, -1810, -1710, -1610, -1510], -4730, 5320, 'kitchen', 'v3_curt_w') + _rings('v3_ring_e', [1510, 1610, 1710, 1810, 1910, 2010], -4730, 5320, 'kitchen', 'v3_curt_e')
ADD += [_a('v3_curt_w', 'SM_V3_CurtainPanel', (-1750, -4730, 40), 0, 'Штора кухни, западная (ткань)', 'kitchen', 'hang', replaces='art93'),
        _a('v3_curt_e', 'SM_V3_CurtainPanel', (1750, -4730, 40), 0, 'Штора кухни, восточная (ткань)', 'kitchen', 'hang', replaces='art94')]
# --- kitchen: radiator under the window (kitchens have one; the dog's warm spot and a cover strip)
ADD += [_a('v3_rad_k', 'SM_L_Radiator', (0, -4900, 300), 0, 'Батарея под окном кухни', 'kitchen', 'wall', copy=True, scale=[0.8, 1, 1],
           hide={'rect': [-690, -5000, 690, -4800], 'z': [0, 300], 'ru': 'за батареей кухни'}, hazard='radiator')]
# --- kitchen: the pendant over the table, a photo, two drawings, outlets, the cola and the cereal box, the dog's food bag
ADD += [_a('v3_pendant', 'SM_V3_PendantLamp', (500, -500, 3500), 0, 'Лампа над столом (вместо кубика box5)', 'kitchen', 'ceiling'),
        _a('v3_photo_k', 'SM_V3_PictureFrame', (7000, -1500, 3600), 90, 'Фото семьи на восточной стене кухни (мама, папа, малыш на море)', 'kitchen', 'wall'),
        _a('v3_photo_k_nail', 'SM_V3_Nail', (7000, -1500, 3600), 90, 'Гвоздь', 'kitchen', 'wall', part_of='v3_photo_k'),
        _a('v3_draw0', 'SM_V3_KidDrawing', (3200, 3100, 2050), 180, 'Рисунок малыша на дверце холодильника («Бадрик»)', 'kitchen', 'wall'),
        _a('v3_draw0_t0', 'SM_V3_TapeStrip', (3060, 3096, 2320), 180, 'Скотч', 'kitchen', 'wall', part_of='v3_draw0'),
        _a('v3_draw0_t1', 'SM_V3_TapeStrip', (3340, 3096, 2320), 180, 'Скотч', 'kitchen', 'wall', part_of='v3_draw0'),
        _a('v3_draw1', 'SM_V3_KidDrawing', (4700, 4400, 1500), -90, 'Рисунок малыша на боку холодильника («Мама»)', 'kitchen', 'wall'),
        _a('v3_draw1_t0', 'SM_V3_TapeStrip', (4704, 4260, 1770), -90, 'Скотч', 'kitchen', 'wall', part_of='v3_draw1'),
        _a('v3_draw1_t1', 'SM_V3_TapeStrip', (4704, 4540, 1770), -90, 'Скотч', 'kitchen', 'wall', part_of='v3_draw1'),
        _a('v3_out_k0', 'SM_V3_Outlet', (-4600, 4970, 2050), 180, 'Розетка над столешницей', 'kitchen', 'wall'),
        _a('v3_out_k1', 'SM_V3_Outlet', (7000, -400, 450), 90, 'Розетка у пола (восток)', 'kitchen', 'wall'),
        _a('v3_out_k2', 'SM_V3_Outlet', (-7000, -4200, 450), -90, 'Розетка у пола (запад)', 'kitchen', 'wall'),
        _a('v3_cola', 'SM_V3_ColaBottle', (2300, 3100, 1710), 180, 'Кола 2 л на краю столешницы', 'kitchen', 'surface'),
        _a('v3_cola_cap', 'SM_V3_ColaCap', (2300, 3100, 2300), 180, 'Крышка колы (откручивается)', 'kitchen', 'on:v3_cola', part_of='v3_cola'),
        _a('v3_cereal', 'SM_V3_CerealBox', (200, 4550, 1710), 180, 'Хлопья малыша на столешнице', 'kitchen', 'surface'),
        _a('v3_dogfood', 'SM_V3_DogFoodBag', (-4900, -4750, 0), 0, 'Мешок корма Бадрика у мисок', 'kitchen', 'floor',
           hide={'rect': [-5200, -4950, -4600, -4550], 'z': [0, 900], 'ru': 'в мешке корма'}),
        _a('v3_backpack', 'SM_V3_Backpack', (-2900, -2000, 0), 25, 'Рюкзак папы у стола (пришёл с работы)', 'kitchen', 'floor',
           hide={'rect': [-3250, -2300, -2550, -1700], 'z': [0, 820], 'ru': 'под клапаном рюкзака'}),
        _a('v3_crate', 'SM_V3_VeggieCrate', (4700, -3900, 0), 0, 'Ящик с картошкой у двери', 'kitchen', 'floor',
           hide={'rect': [4250, -4200, 5150, -3600], 'z': [0, 450], 'ru': 'между рейками ящика с картошкой'}),
        _a('v3_rug_k', 'SM_V3_DoorMat', (-300, 2450, 0), 180, 'Коврик-дорожка у мойки и посудомойки', 'kitchen', 'floor', scale=[2.2, 1, 1],
           hide={'rect': [-1730, 2050, 1130, 2850], 'z': [0, 18], 'ru': 'под ковриком у мойки'})]
ADD += _cornice('v3_cor_k', [(-7000, -1650, 5000, 'n'), (150, 7000, 5000, 'n'), (-7000, 7000, -5000, 's'), (-5000, 5000, -7000, 'w'), (-5000, 5000, 7000, 'e')], 'kitchen')
# --- living room
ADD += [_a('v3_toybox', 'SM_V3_ToyBox', (9080, 975, 0), 180, 'Ящик для игрушек малыша', 'living', 'floor', hide={'rect': [8378, 525, 9782, 1425], 'z': [0, 780], 'ru': 'в ящике для игрушек'}),
        _a('v3_toybox_lid', 'SM_V3_ToyBox_Lid', (9080, 1425, 780), 180, 'Крышка ящика (петли)', 'living', 'on:v3_toybox', part_of='v3_toybox'),
        _a('v3_toybox_h0', 'SM_V3_Hinge', (8680, 1425, 780), 180, 'Петля крышки', 'living', 'on:v3_toybox', part_of='v3_toybox'),
        _a('v3_toybox_h1', 'SM_V3_Hinge', (9480, 1425, 780), 180, 'Петля крышки', 'living', 'on:v3_toybox', part_of='v3_toybox'),
        _a('v3_photo_l0', 'SM_V3_PictureFrame', (11600, 1500, 3700), 180, 'Фото над диваном: свадьба', 'living', 'wall'),
        _a('v3_photo_l1', 'SM_V3_PictureFrame', (13500, 1500, 3900), 180, 'Фото над диваном: малыш в роддоме (большое)', 'living', 'wall', scale=[1.3, 1, 1.3]),
        _a('v3_photo_l2', 'SM_V3_PictureFrame', (15400, 1500, 3700), 180, 'Фото над диваном: Бадрик щенком', 'living', 'wall'),
        _a('v3_photo_l3', 'SM_V3_PictureFrame', (7200, -5900, 3600), -90, 'Фото над креслом: дедушка', 'living', 'wall', scale=[0.8, 1, 0.8]),
        _a('v3_nail_l0', 'SM_V3_Nail', (11600, 1500, 3700), 180, 'Гвоздь', 'living', 'wall', part_of='v3_photo_l0'),
        _a('v3_nail_l1', 'SM_V3_Nail', (13500, 1500, 3900), 180, 'Гвоздь', 'living', 'wall', part_of='v3_photo_l1'),
        _a('v3_nail_l2', 'SM_V3_Nail', (15400, 1500, 3700), 180, 'Гвоздь', 'living', 'wall', part_of='v3_photo_l2'),
        _a('v3_nail_l3', 'SM_V3_Nail', (7200, -5900, 3600), -90, 'Гвоздь', 'living', 'wall', part_of='v3_photo_l3'),
        # the door to the rest of the flat: closed, a set piece (frame + leaf copied from the kitchen door) and the exit of the routines
        _a('v3_hall_frame', 'SM_L_DoorFrame', (20300, -1000, 0), 90, 'Дверь в коридор: коробка (дальше — остальная квартира)', 'living', 'wall', copy=True),
        _a('v3_hall_leaf', 'SM_L_Door_Leaf', (20148.6, -1792.5, 22.8), 90, 'Дверь в коридор (закрыта; через неё Человек выносит мусор)', 'living', 'wall', copy=True, origin_is_pivot=True, part_of='v3_hall_frame'),
        _a('v3_hall_mat', 'SM_V3_DoorMat', (19500, -1000, 0), 90, 'Коврик у двери в коридор', 'living', 'floor', hide={'rect': [19100, -1650, 19900, -350], 'z': [0, 18], 'ru': 'под ковриком у двери'}),
        _a('v3_skirt_e0', 'SM_L_Skirting', (20185, -5425, 0), 90, 'Плинтус восточной стены (до двери)', 'living', 'wall', copy=True, scale=[7150 / 10500.0, 1, 1], replaces='art308'),
        _a('v3_skirt_e1', 'SM_L_Skirting', (20185, 675, 0), 90, 'Плинтус восточной стены (после двери)', 'living', 'wall', copy=True, scale=[1650 / 10500.0, 1, 1], replaces='art308'),
        _a('v3_cush0', 'SM_V3_FloorCushion', (12500, -6400, 0), 20, 'Напольная подушка у телевизора', 'living', 'floor', hide={'rect': [11950, -6950, 13050, -5850], 'z': [0, 300], 'ru': 'под подушкой'}),
        _a('v3_cush1', 'SM_V3_FloorCushion', (15400, -7140, 0), -15, 'Вторая подушка', 'living', 'floor', hide={'rect': [14850, -7690, 15950, -6590], 'z': [0, 300], 'ru': 'под подушкой'}),
        # birthday: balloons on the floor weight by the sofa, a present on top of the bookcase
        _a('v3_bw', 'SM_V3_BalloonWeight', (17700, -650, 0), 0, 'Грузик шариков', 'living', 'floor'),
        _a('v3_bal0', 'SM_V3_Balloon', (17520, -430, 4700), 0, 'Шарик (красный)', 'living', 'string', tint='Red'),
        _a('v3_bal1', 'SM_V3_Balloon', (17880, -720, 4730), 0, 'Шарик (жёлтый)', 'living', 'string', tint='Honey'),
        _a('v3_bal2', 'SM_V3_Balloon', (17640, -930, 4680), 0, 'Шарик (бирюзовый)', 'living', 'string', tint='Teal'),
        _a('v3_str0', 'SM_V3_BalloonString', (17700, -650, 95), 0, 'Ленточка', 'living', 'string', aim=(17520, -430, 4700), part_of='v3_bal0'),
        _a('v3_str1', 'SM_V3_BalloonString', (17700, -650, 95), 0, 'Ленточка', 'living', 'string', aim=(17880, -720, 4730), part_of='v3_bal1'),
        _a('v3_str2', 'SM_V3_BalloonString', (17700, -650, 95), 0, 'Ленточка', 'living', 'string', aim=(17640, -930, 4680), part_of='v3_bal2'),
        _a('v3_gift0', 'SM_V3_GiftBox', (19700, -3200, 4660), 90, 'Подарок, спрятанный на книжном шкафу', 'living', 'surface', hide={'rect': [19350, -3650, 20050, -2750], 'z': [4660, 5180], 'ru': 'в подарке на шкафу'}),
        _a('v3_gift0_lid', 'SM_V3_GiftBox_Lid', (19700, -3200, 5180), 90, 'Крышка подарка с бантом', 'living', 'on:v3_gift0', part_of='v3_gift0'),
        _a('v3_glasses', 'SM_V3_Glasses', (10800, -8200, 1100), 20, 'Очки бабушки на тумбе ТВ', 'living', 'surface'),
        _a('v3_scarf', 'SM_V3_KnitScarf', (10250, -4450, 760), 0, 'Начатый шарф внуку', 'living', 'on:art303'),
        _a('v3_needle0', 'SM_V3_KnitNeedle', (10250, -4400, 800), 30, 'Спица', 'living', 'on:v3_scarf', part_of='v3_scarf'),
        _a('v3_needle1', 'SM_V3_KnitNeedle', (10200, -4500, 800), -25, 'Спица', 'living', 'on:v3_scarf', part_of='v3_scarf'),
        _a('v3_phone', 'SM_V3_Phone', (11300, 200, 950), 10, 'Телефон папы на диване', 'living', 'on:art174'),
        _a('v3_out_l0', 'SM_V3_Outlet', (7200, -1900, 450), -90, 'Розетка', 'living', 'wall'),
        _a('v3_out_l1', 'SM_V3_Outlet', (20200, -7500, 450), 90, 'Розетка', 'living', 'wall'),
        _a('v3_out_l2', 'SM_V3_Outlet', (12000, 1500, 450), 180, 'Розетка за диваном', 'living', 'wall'),
        # living-room window: the same curtain kit as the kitchen
        _a('v3_rod_l', 'SM_V3_CurtainRod', (18285, -8730, 5320), 0, 'Карниз гостиной', 'living', 'hang', scale=[0.85, 1, 1], replaces='copy5'),
        _a('v3_rodbr_l0', 'SM_V3_RodBracket', (16550, -9000, 5320), 0, 'Кронштейн карниза', 'living', 'wall', part_of='v3_rod_l'),
        _a('v3_rodbr_l1', 'SM_V3_RodBracket', (20020, -9000, 5320), 0, 'Кронштейн карниза', 'living', 'wall', part_of='v3_rod_l'),
        _a('v3_curt_lw', 'SM_V3_CurtainPanel', (16700, -8730, 40), 0, 'Штора гостиной, западная', 'living', 'hang', replaces='copy3'),
        _a('v3_curt_le', 'SM_V3_CurtainPanel', (19870, -8730, 40), 0, 'Штора гостиной, восточная', 'living', 'hang', replaces='copy4')]
ADD += _screws('v3_rod_l_sc', [(16550, -8990, 5265), (16550, -8990, 5375), (20020, -8990, 5265), (20020, -8990, 5375)], 0, 'living', 'v3_rod_l')
ADD += _rings('v3_ring_lw', [16450, 16550, 16650, 16750, 16850, 16950], -8730, 5320, 'living', 'v3_curt_lw') + _rings('v3_ring_le', [19620, 19720, 19820, 19920, 20020, 20120], -8730, 5320, 'living', 'v3_curt_le')
ADD += _cornice('v3_cor_l', [(7200, 20200, 1500, 'n'), (7200, 20200, -9000, 's'), (-9000, 1500, 7200, 'w'), (-9000, 1500, 20200, 'e')], 'living')
# toy box contents: what used to lie about the flat
TOYBOX_CONTENTS = ['SM_L_ToyBlock x2 (из кухни)', 'SM_L_ToyArch x5 (кухня, гостиная)', 'SM_L_ToyCar x3', 'SM_L_ToyBlock x2 (гостиная)', 'носок малыша', 'еда ур. 1–2: печенье, мармеладка (малыш прячет сладкое в игрушки)']

# ---------------------------------------------------------------------------------------------------------------------------------
# 5. Interactive and destructible objects: how each is held, what ants do to it, what happens, what it can start.
#    obj = object id in the level (v2 id or v3 id); fasteners = ids of the fastener pieces (or kind + count when they are not modelled);
#    needs_code: every one of these needs the "fasteners and states" system (see NEEDS_CODE) unless it says 'есть'.
# ---------------------------------------------------------------------------------------------------------------------------------
INTERACTIVE = [
    {'id': 'shelf', 'ru': 'Полка над гнездом', 'obj': 'v3_shelf', 'mat': 'wood', 'fasteners': ['v3_shelf_sc0', 'v3_shelf_sc1', 'v3_shelf_sc2', 'v3_shelf_sc3'], 'kind': 'screw',
     'undo': 'открутить оба шурупа одного уголка (2 x 8 с вдвоём)', 'result': 'полка повисает на втором уголке и наклоняется на 35°: всё с неё съезжает к открученному краю и падает с 2680',
     'chain': ['jam_jar', 'pickles', 'honey_spill'], 'district': 'plintus'},
    {'id': 'jam_jar', 'ru': 'Банка варенья (стекло)', 'obj': 'v3_jam', 'mat': 'glass', 'fasteners': ['friction'], 'undo': 'столкнуть рывком с полки или уронить вместе с полкой',
     'result': 'бьётся: осколки (режут) и клякса варенья 760 x 520 (липко, есть в коде как sticky); варенье — еда ур. 2 на 6 муравьиных порций', 'chain': ['human_slip', 'dog_lick'], 'district': 'plintus'},
    {'id': 'pickles', 'ru': 'Банка огурцов', 'obj': 'v3_pickles', 'mat': 'glass', 'fasteners': ['friction'], 'undo': 'толкнуть рывком вдвоём (тяжёлая)', 'result': 'бьётся: рассол — мокрая лужа (скользко), огурцы — тяжёлая еда ур. 2', 'chain': ['human_slip'], 'district': 'plintus'},
    {'id': 'curtain_k', 'ru': 'Шторы кухни', 'obj': 'v3_curt_w', 'mat': 'cloth', 'fasteners': ['v3_ring_w0', 'v3_ring_w1', 'v3_ring_w2', 'v3_ring_w3', 'v3_ring_w4', 'v3_ring_w5'], 'kind': 'ring',
     'undo': 'разжать кольца (по 4 с вдвоём) или прогрызть полосу (12 с, двое)', 'result': '3 кольца из 6 — штора рвётся и падает кучей к окну: новое укрытие 1,5 x 1 м у плинтуса; шум 0,3',
     'chain': ['curtain_rod'], 'district': 'plintus', 'cloth': True},
    {'id': 'curtain_rod', 'ru': 'Карниз кухни', 'obj': 'v3_rod', 'mat': 'metal', 'fasteners': ['v3_rod_sc0', 'v3_rod_sc1', 'v3_rod_sc2', 'v3_rod_sc3'], 'kind': 'screw',
     'undo': 'открутить два шурупа одного кронштейна (под потолком: путь — по шторе)', 'result': 'карниз падает одним концом на подоконник: обе шторы съезжают, грохот 0,9 — Человек идёт смотреть; висящий карниз — пандус подоконник -> потолок',
     'chain': ['plant_k'], 'district': 'plintus'},
    {'id': 'plant_k', 'ru': 'Цветок на подоконнике кухни', 'obj': 'art92', 'mat': 'terracotta', 'fasteners': ['friction'], 'undo': 'столкнуть рывком (или карниз сбивает)',
     'result': 'горшок бьётся о пол (2200): черепки и горка земли — укрытие и ступень; бабушка идёт убирать (рутина «убирает», 30 с у окна)', 'chain': ['grandma_cleans'], 'district': 'plintus'},
    {'id': 'tablecloth', 'ru': 'Скатерть', 'obj': 'v3_cloth', 'mat': 'cloth', 'fasteners': ['friction'], 'undo': 'взяться за край вчетвером и тянуть (держать [E]: сила 40, как тяжёлая дверь) — скатерть сползает к тянущим',
     'result': 'за 3 с скатерть съезжает на 1 м: всё, что на ней, едет и падает с того края (чашки бьются, чайник бьётся, фрукты катятся, торт-трофей падает с 1400 — бьётся, если он там)',
     'chain': ['teapot', 'cups', 'fruit_roll', 'dog_eats_floor'], 'district': 'plato', 'cloth': True},
    {'id': 'teapot', 'ru': 'Заварочный чайник', 'obj': 'v3_teapot', 'mat': 'ceramic', 'fasteners': ['friction'], 'undo': 'столкнуть рывком', 'result': 'бьётся: чай — мокрая лужа, черепки; громко (0,8)', 'chain': ['human_slip'], 'district': 'plato'},
    {'id': 'cola', 'ru': 'Кола 2 л', 'obj': 'v3_cola', 'mat': 'pet', 'fasteners': ['v3_cola_cap'], 'kind': 'cap',
     'undo': 'открутить крышку втроём (6 с), потом толкнуть бутылку рывком вдвоём', 'result': 'бутылка падает со столешницы и 20 с льёт: липкая лужа до 1,9 x 1,2 м у посудомойки (sticky есть в коде), сладкая еда ур. 1 в луже',
     'chain': ['human_slip', 'dog_lick'], 'district': 'lednik'},
    {'id': 'cereal', 'ru': 'Коробка хлопьев', 'obj': 'v3_cereal', 'mat': 'cardboard', 'fasteners': ['friction'], 'undo': 'толкнуть рывком вдвоём к краю', 'result': 'падает, хлопья рассыпаются веером 1,5 x 1 м: 30 крошек ур. 0–1 (много мелочи сразу)', 'chain': ['dog_eats_floor'], 'district': 'lednik'},
    {'id': 'upper_doors', 'ru': 'Дверцы верхних шкафов', 'obj': 'art27', 'mat': 'wood', 'fasteners': ['hinge x2 на дверцу (по 3 шурупа)'], 'kind': 'hinge',
     'undo': 'выкрутить верхнюю петлю (2 шурупа из 3)', 'result': 'дверца висит криво и не закрывается: Человек её не захлопнет; обе петли — дверца падает (грохот 0,9) и лежит на столешнице пандусом',
     'chain': ['human_fixes'], 'district': 'kladovye'},
    {'id': 'fridge_art', 'ru': 'Рисунки и магниты на холодильнике', 'obj': 'v3_draw0', 'mat': 'paper', 'fasteners': ['v3_draw0_t0', 'v3_draw0_t1', 'new29', 'new30'], 'kind': 'tape',
     'undo': 'отклеить скотч / столкнуть магнит', 'result': 'рисунок планирует на пол: лист 400 x 570 — укрытие на полу; малыш расстраивается и идёт поднимать (рутина)', 'chain': ['kid_comes'], 'district': 'lednik'},
    {'id': 'photo_l', 'ru': 'Фото над диваном', 'obj': 'v3_photo_l1', 'mat': 'glass', 'fasteners': ['v3_nail_l1'], 'kind': 'nail',
     'undo': 'перегрызть шнурок (6 с)', 'result': 'рамка падает на спинку дивана и стекло бьётся: осколки на сиденье (режут), грохот 0,8', 'chain': ['human_comes'], 'district': 'play'},
    {'id': 'balloons', 'ru': 'Шарики', 'obj': 'v3_bal0', 'mat': 'latex', 'fasteners': ['v3_str0', 'v3_str1', 'v3_str2'], 'kind': 'knot',
     'undo': 'перегрызть ленточку у грузика (5 с) — шарик уходит под потолок с муравьём на ленте; кислотой — лопается', 'result': 'ленточка — канат с пола до потолка; хлопок 1,0 — все идут на звук (отвлечение на другом конце дома)',
     'chain': ['human_comes', 'dog_barks'], 'district': 'play'},
    {'id': 'toybox', 'ru': 'Ящик для игрушек', 'obj': 'v3_toybox', 'mat': 'wood', 'fasteners': ['v3_toybox_h0', 'v3_toybox_h1'], 'kind': 'hinge',
     'undo': 'поднять крышку втроём (сила 24) или выкрутить петли', 'result': 'внутри: игрушки и сладости малыша; опрокинуть ящик нельзя, но открытый ящик малыш вываливает сам (рутина «играет»): игрушки по полу — укрытия',
     'chain': ['kid_dumps'], 'district': 'play'},
    {'id': 'gift', 'ru': 'Подарок на шкафу', 'obj': 'v3_gift0', 'mat': 'cardboard', 'fasteners': ['лента (knot)'], 'kind': 'knot',
     'undo': 'перегрызть ленту (5 с), сдвинуть крышку вдвоём', 'result': 'внутри конфеты (еда ур. 3, 4 шт.); крышка с бантом падает на пол — шум 0,3', 'chain': [], 'district': 'stenka'},
    {'id': 'knitting', 'ru': 'Вязание бабушки', 'obj': 'v3_scarf', 'mat': 'yarn', 'fasteners': ['v3_needle0', 'v3_needle1'], 'kind': 'needle',
     'undo': 'вытащить спицу вдвоём (4 с)', 'result': 'шарф распускается: нитка от клубка через всю комнату — путь (как нитки v2) и ловушка для бабушки: она ищет и сматывает (рутина 40 с, не охотится)',
     'chain': ['grandma_busy'], 'district': 'babushka'},
    {'id': 'glasses', 'ru': 'Очки бабушки', 'obj': 'v3_glasses', 'mat': 'glass', 'fasteners': ['friction'], 'undo': 'утащить вчетвером (вес 18) или столкнуть с тумбы',
     'result': 'без очков бабушка видит вдвое ближе (2600 -> 1300) и ищет их (рутина); разбитые — до конца рейда', 'chain': ['grandma_busy'], 'district': 'stenka'},
    {'id': 'outlets', 'ru': 'Розетки', 'obj': 'v3_out_k1', 'mat': 'plastic', 'fasteners': ['винт рамки x1'], 'kind': 'screw',
     'undo': 'открутить винт рамки (8 с вдвоём)', 'result': 'рамка отходит: лаз в стену к проводам (короткий ход в вентиляцию; ток в мокрой стене)', 'chain': [], 'district': 'svalka'},
    {'id': 'trolley', 'ru': 'Сумка-тележка', 'obj': 'v3_trolley', 'mat': 'cloth', 'fasteners': ['молния (держать [E] вдвоём, 4 с)'], 'kind': 'friction',
     'undo': 'открыть молнию', 'result': 'внутри картошка и лук (тяжёлая еда ур. 1–2); толкнуть тележку нельзя', 'chain': [], 'district': 'plintus'},
    {'id': 'toaster', 'ru': 'Тостер', 'obj': 'art73', 'mat': 'metal', 'fasteners': ['есть в коде (катапульта)'], 'kind': 'friction', 'undo': 'есть', 'result': 'есть: шум и тост', 'chain': [], 'district': 'kladovye'},
]
CHAINS = [
    {'id': 'C1', 'ru': 'Полка -> банки -> лужа -> Человек поскальзывается -> роняет еду',
     'steps': ['двое откручивают южный уголок полки над гнездом (16 с, рядом с домом — безопасно)', 'полка наклоняется: банка варенья и огурцы съезжают и падают с 2680',
               'стекло бьётся у западной стены: клякса варенья (липко) и рассол (скользко), осколки', 'грохот 0,9: Человек идёт к полке — по пути наступает в рассол и падает (нужен код: «скользкая лужа» делает другой агент прямо сейчас)',
               'падая, роняет то, что нёс (газета, дихлофос, отнятая еда) и 3 с лежит: окно, чтобы пронести торт мимо'], 'needs_code': ['fasteners', 'break', 'slip']},
    {'id': 'C2', 'ru': 'Скатерть -> посуда -> Бадрик под столом',
     'steps': ['отряд берётся за свес скатерти (сила 40) и тянет', 'чайник и чашки едут и падают — бьются (шум 0,8)', 'фрукты катятся по полу (яблоко — тяжёлая еда)',
               'Бадрик спит под столом: грохот — лает (есть в коде) и идёт есть упавшее (есть в коде) — зовёт Человека к столу, пока вы у холодильника'], 'needs_code': ['cloth_pull', 'break']},
    {'id': 'C3', 'ru': 'Карниз -> штора -> цветок -> бабушка убирает',
     'steps': ['по шторе наверх, откручиваем кронштейн карниза (под потолком)', 'карниз падает концом на подоконник, шторы сползают кучей', 'цветок сбит с подоконника: горшок бьётся',
               'бабушка идёт убирать землю (30 с у окна кухни) — гостиная без бабушки, кошачья дверца свободна'], 'needs_code': ['fasteners', 'break', 'routine_react']},
    {'id': 'C4', 'ru': 'Кола -> липкий пол -> Человек вязнет',
     'steps': ['трое откручивают крышку колы на столешнице', 'рывком бутылка падает и льёт 20 с', 'лужа 1,9 x 1,2 м перед посудомойкой: Человек в ней шагает медленнее и громче (нужен код), муравей вязнет (есть: sticky)',
               'лужа отрезает Человеку короткий путь к холодильнику — торт из ящика для овощей выносят под его носом'], 'needs_code': ['fasteners', 'liquid_spill']},
    {'id': 'C5', 'ru': 'Шарик -> хлопок -> весь дом идёт в гостиную',
     'steps': ['перегрызть ленточку — шарик с муравьём уходит под потолок', 'кислотой по шарику — хлопок 1,0', 'Человек, бабушка, Бадрик идут к дивану (шум — есть в коде)', 'кухня пустая 15–20 с'], 'needs_code': ['knot', 'pop_noise']},
    {'id': 'C6', 'ru': 'Спица -> пряжа через комнату -> бабушка сматывает',
     'steps': ['двое вытаскивают спицу', 'нитку тащат через гостиную к двери (путь и метка)', 'бабушка встаёт сматывать (40 с) и не охотится; если нитку протащили через проход — Человек цепляется (нужен код)'], 'needs_code': ['yarn', 'routine_react']},
]
CONTENTS = {   # what a container holds; the seed draws from the pool (tier = loot tier of v2)
    'Fridge': {'pool': [('сыр', 2), ('колбаса', 2), ('варенье', 2), ('йогурт', 1), ('виноград', 1)], 'n': (3, 5)},
    'Freezer': {'pool': [('мороженое', 3), ('пельмени', 2), ('лёд', 0)], 'n': (2, 3)},
    'Oven': {'pool': [('пирог', 3), ('курица', 3), ('пусто', 0)], 'n': (1, 1), 'by_variant': {'plita': {'B': 'пирог'}}},
    'Microwave': {'pool': [('пицца', 2), ('попкорн', 1)], 'n': (1, 2)},
    'Dishwasher': {'pool': [('объедки', 1), ('ложка мёда', 2)], 'n': (1, 2)},
    'TopDrawer': {'pool': [('конфеты', 2), ('сахар-рафинад', 1)], 'n': (1, 2)},
    'CabA': {'pool': [('печенье', 1), ('макароны', 0), ('сахар', 1)], 'n': (2, 3)},
    'BreadBox': {'pool': [('булка', 1), ('сухарики', 1)], 'n': (1, 2)},
    'TVStand': {'pool': [('конфеты бабушки', 2), ('печенье', 1)], 'n': (1, 2)},
    'v3_toybox': {'pool': [('печенье', 1), ('мармеладка', 2), ('леденец', 2)], 'n': (1, 3), 'note': 'малыш прячет сладкое в игрушки'},
    'v3_gift0': {'pool': [('конфеты', 3)], 'n': (2, 4)},
    'v3_trolley': {'pool': [('картошка', 1), ('лук', 1), ('морковь', 1)], 'n': (2, 3), 'heavy': True},
    'v3_dogfood': {'pool': [('корм Бадрика', 0)], 'n': (3, 6), 'note': 'Бадрик охраняет: ест всё, что рядом (есть в коде)'},
    'v3_cereal': {'pool': [('хлопья', 0)], 'n': (20, 30), 'spill': True},
}
CLOTH = [
    {'id': 'tablecloth', 'ru': 'Скатерть', 'piece': 'SM_V3_Tablecloth / SM_V3_TableclothLong', 'size_m': [2.39, 1.44], 'pins': 'нет',
     'ants': ['лезут по свесу как по стене (есть)', 'тянут за край вчетвером (нужен код)', 'прогрызают дыру (нужен код)'], 'humans': ['Человек поправляет (рутина)', 'малыш стягивает (нужен код)']},
    {'id': 'curtains', 'ru': 'Шторы кухни и гостиной (4 шт.)', 'piece': 'SM_V3_CurtainPanel', 'size_m': [0.32, 2.75], 'pins': 'верхний ряд — 6 колец',
     'ants': ['лезут (есть, как v2)', 'разжимают кольца (нужен код)', 'рвут (нужен код)'], 'humans': ['Человек вешает обратно (рутина «чинит», 20 с)']},
    {'id': 'towel', 'ru': 'Полотенце на ручке духовки (v2 SM_K_Towel)', 'piece': 'SM_K_Towel', 'size_m': [0.32, 0.76], 'pins': 'перекинуто через ручку',
     'ants': ['лезут (есть)', 'стягивают вдвоём — падает кучей у плиты (нужен код)'], 'humans': ['вытирает руки (рутина)']},
    {'id': 'blanket', 'ru': 'Плед на диване (v2 SM_L_Blanket) / шалаш малыша', 'piece': 'SM_L_Blanket / SM_V3_BlanketFort', 'size_m': [1.9, 1.4], 'pins': 'придавлен подушками',
     'ants': ['пандус (есть)', 'стянуть — шалаш падает и накрывает тех, кто под ним (нужен код)'], 'humans': ['малыш строит шалаш (вариант Игровой B)']},
    {'id': 'knitting', 'ru': 'Вязание (шарф на спицах + клубок)', 'piece': 'SM_V3_KnitScarf + SM_L_YarnBall', 'size_m': [0.4, 0.24], 'pins': 'спицы',
     'ants': ['вытащить спицу — распустить (нужен код)', 'тащить нитку (нитки как путь — есть с v2)'], 'humans': ['бабушка вяжет (есть), сматывает (нужен код)']},
    {'id': 'laundry', 'ru': 'Простыни на верёвке во дворе A (вариант двора B)', 'piece': 'SM_V3_CurtainPanel x3 (масштаб)', 'size_m': [0.7, 0.9], 'pins': 'прищепки',
     'ants': ['лезут', 'снимают прищепки — простыня падает и накрывает газон (нужен код)'], 'humans': []},
    {'id': 'doormat', 'ru': 'Коврик у двери в коридор', 'piece': 'SM_V3_DoorMat', 'size_m': [0.68, 0.42], 'pins': 'лежит',
     'ants': ['под ковриком щель 18 uu — укрытие (данные укрытий, кода не надо)'], 'humans': []},
]

# ---------------------------------------------------------------------------------------------------------------------------------
# 6. Inside people. Segments are the real ones of the segmented bodies (human_manifest.json: SM_H_*; the grandma and the kid use the same
#    19 segments, HumanGranData.inl / HumanKidData.inl). Riding a segment exists in the code; going INSIDE is new (needs_code: 'inside').
# ---------------------------------------------------------------------------------------------------------------------------------
INSIDE_PEOPLE = [
    {'id': 'ear', 'ru': 'Ухо', 'who': ['human', 'grandma'], 'segment': 'SM_H_Head', 'local_uu': [0, 95, 1480], 'when': 'сидит, стоит на месте или спит (голова не трясётся)',
     'enter': 'с плеча по шее (SM_H_Neck -> SM_H_Head), 2 с у уха', 'effect': 'пока муравей в ухе: Человек не слышит шум (не идёт на шумелку, ТВ, тостер), трясёт головой каждые 3 с; укус — 2 с оглушения «по голове» (есть анимация)',
     'counter': 'стряхивание [F] не помогает; хлопает себя по уху ладонью (шлепок по своей голове: 150 урона тому, кто в ухе, раз в 6 с)', 'risk': 'высокий'},
    {'id': 'nose', 'ru': 'Нос', 'who': ['human', 'grandma'], 'segment': 'SM_H_Head', 'local_uu': [0, 120, 1400], 'when': 'только во сне (рутина «дремлет» на диване / в кресле)',
     'enter': 'по лицу, 3 с', 'effect': 'чихает через 2 с: муравья выстреливает на 2000 uu вперёд (катапульта, как тостер), Человек просыпается в ярости +1; бабушка просыпается и ищет очки',
     'counter': 'нет; это способ разбудить специально (отвлечь от дивана нельзя — он встаёт прямо здесь)', 'risk': 'средний'},
    {'id': 'moustache', 'ru': 'Усы папы', 'who': ['human'], 'segment': 'SM_H_Head', 'local_uu': [0, 115, 1370], 'when': 'всегда (как езда на сегменте)',
     'enter': 'с подбородка', 'effect': 'укрытие от его же атак: газета и шлепок по лицу не применяются; щекотка — через 4 с чихает (как нос)', 'counter': 'стряхивание [F]', 'risk': 'средний'},
    {'id': 'sleeve', 'ru': 'Рукав / под ремешком часов', 'who': ['human'], 'segment': 'SM_H_Forearm_R', 'local_uu': [0, 0, -120], 'when': 'всегда',
     'enter': 'с кисти внутрь рукава майки нельзя (майка без рукавов) — под ремешок часов на правом запястье', 'effect': 'укус под ремешком: Человек роняет то, что в правой руке (газета, дихлофос, схваченная еда) и 2 с трясёт рукой',
     'counter': 'стряхивание [F] сбрасывает через 1 с', 'risk': 'низкий'},
    {'id': 'cuff', 'ru': 'Штанина пижамы', 'who': ['human'], 'segment': 'SM_H_Shin_L', 'local_uu': [0, 0, 120], 'when': 'всегда; вход с тапка или пола',
     'enter': 'с пола на тапок, с тапка под штанину (1 с)', 'effect': 'внутри штанины муравья не видно и топот его не достаёт; укус — Человек скачет на одной ноге 2 с (есть анимация «по ноге») и не топает 5 с',
     'counter': 'трясёт ногой: каждые 4 с шанс 50 % вытряхнуть', 'risk': 'низкий'},
    {'id': 'pocket', 'ru': 'Карман', 'who': ['human', 'kid', 'grandma'], 'segment': 'SM_H_Thigh_R / SM_H_Belly (кофта бабушки)', 'local_uu': [80, 60, -150], 'when': 'всегда',
     'enter': 'с пояса', 'effect': 'в кармане малыша — конфета (еда ур. 2), у бабушки — леденцы (ур. 1, 3 шт.) и носовой платок; у папы — ключи (шум, если уронить). Укрытие: в кармане не достают ни газета, ни ладонь',
     'counter': 'малыш лезет в карман за конфетой каждые 40 с — берёт и муравья (как «хватает»)', 'risk': 'низкий'},
    {'id': 'hair', 'ru': 'Пучок бабушки / вихор малыша', 'who': ['grandma', 'kid'], 'segment': 'SM_H_Head', 'local_uu': [0, -40, 1560], 'when': 'всегда',
     'enter': 'по спинке кресла или с плеча', 'effect': 'бабушка не видит муравья на себе; на голове у неё — лучшая обзорная точка гостиной (метки для команды дальше на 50 %)',
     'counter': 'бабушка поправляет пучок раз в 30 с — муравей падает на колени', 'risk': 'низкий'},
    {'id': 'slipper', 'ru': 'Внутри тапка', 'who': ['human', 'grandma'], 'segment': 'SM_H_Foot_L/R (тапок снят — отдельный предмет у дивана / кресла)', 'local_uu': [0, 60, 40],
     'when': 'когда он дремлет (тапки снимает и ставит у дивана) / бабушка в кресле',
     'enter': 'в снятый тапок', 'effect': 'укрытие; Человек надевает тапок — укус в ступню: он скидывает тапок пинком (тапок летит 1500 uu с муравьём) и 3 с прыгает',
     'counter': 'вытряхивает тапок перед тем, как надеть (30 %)', 'risk': 'средний'},
    {'id': 'collar', 'ru': 'Ворот кофты бабушки', 'who': ['grandma'], 'segment': 'SM_H_Neck', 'local_uu': [0, -60, 80], 'when': 'всегда',
     'enter': 'с плеча', 'effect': 'укус в шею: бабушка роняет вязание (спицы на пол — распускается, см. C6) и 3 с ищет', 'counter': 'хлопает себя по шее (как ухо)', 'risk': 'средний'},
]

# ---------------------------------------------------------------------------------------------------------------------------------
# 7. Routines: the places of the daily life. p = where the person stands (checked: reachable by the capsule radius 340 / step 250 from the
#    Human's spawn, same nav as the grandma and the kid); uses = objects; when = time of day. The order is the evening loop.
# ---------------------------------------------------------------------------------------------------------------------------------
ROUTINES = {
    'human': [
        {'id': 'h_phone', 'ru': 'сидит на диване в телефоне (фаза 0 рейда)', 'p': (11300, -900), 'face': 90, 'seconds': 150, 'uses': ['v3_phone', 'art171'], 'when': ['day', 'evening', 'night']},
        {'id': 'h_fridge', 'ru': 'открывает холодильник, стоит, смотрит', 'p': (3300, 2000), 'alt': [(2200, 2300), (5000, 2100)], 'face': 0, 'seconds': 8, 'uses': ['door_fridge'], 'when': ['day', 'evening', 'night']},
        {'id': 'h_stove', 'ru': 'греет суп у плиты, мешает половником', 'p': (-750, 2200), 'face': 0, 'seconds': 40, 'uses': ['art150', 'knob', 'art118'], 'when': ['evening']},
        {'id': 'h_table', 'ru': 'ужинает за столом (южный стул)', 'p': (0, -3600), 'face': 0, 'seconds': 60, 'uses': ['art78', 'v3_teapot'], 'when': ['evening']},
        {'id': 'h_sink', 'ru': 'моет посуду (кран — шум, есть в коде «жизнь кухни»)', 'p': (-3500, 2250), 'face': 0, 'seconds': 45, 'uses': ['tap_hot', 'art22'], 'when': ['day', 'evening']},
        {'id': 'h_bin', 'ru': 'достаёт мешок из ведра, завязывает', 'p': (6000, -3000), 'face': 90, 'seconds': 10, 'uses': ['art81', 'pedal'], 'when': ['evening', 'night']},
        {'id': 'h_trash_out', 'ru': 'выносит мусор: уходит в дверь коридора на 45 с (дома его нет!)', 'p': (19000, -1000), 'face': -90, 'seconds': 45, 'uses': ['v3_hall_leaf'], 'when': ['evening']},
        {'id': 'h_tv', 'ru': 'смотрит телевизор с дивана', 'p': (13500, -1100), 'alt': [(11300, -1000)], 'face': 180, 'seconds': 120, 'uses': ['art190', 'tv'], 'when': ['evening', 'night']},
        {'id': 'h_nap', 'ru': 'дремлет на диване, тапки сняты (нос, тапок — входы «внутрь людей»)', 'p': (12300, -1100), 'alt': [(16500, -1400)], 'face': 180, 'seconds': 90, 'uses': ['art171', 'new6'], 'when': ['night']},
        {'id': 'h_window', 'ru': 'проветривает: стоит у окна кухни', 'p': (0, -4100), 'face': 180, 'seconds': 15, 'uses': ['v3_curt_w'], 'when': ['day']},
        {'id': 'h_fix', 'ru': 'чинит то, что сломали муравьи (отвёртка, совок) — реакция, а не расписание', 'p': None, 'seconds': 12, 'uses': [], 'when': ['day', 'evening', 'night']},
    ],
    'grandma': [
        {'id': 'g_knit', 'ru': 'вяжет в кресле (есть в коде)', 'p': (10250, -5800), 'face': 90, 'seconds': 120, 'uses': ['art288', 'v3_scarf', 'art296'], 'when': ['day', 'evening']},
        {'id': 'g_water_l', 'ru': 'поливает цветы на подоконнике гостиной', 'p': (17000, -8000), 'face': 180, 'seconds': 20, 'uses': ['copy6', 'copy7'], 'when': ['day']},
        {'id': 'g_water_k', 'ru': 'поливает цветок на подоконнике кухни', 'p': (-1000, -4100), 'face': 180, 'seconds': 15, 'uses': ['art92'], 'when': ['day', 'evening']},
        {'id': 'g_tea', 'ru': 'ставит чайник, ждёт у столешницы', 'p': (2200, 2300), 'face': 0, 'seconds': 40, 'uses': ['art152'], 'when': ['evening']},
        {'id': 'g_hide_food', 'ru': 'прячет еду в тумбу ТВ (есть в коде)', 'p': (12600, -7200), 'face': 180, 'seconds': 8, 'uses': ['TVStand'], 'when': ['day', 'evening', 'night']},
        {'id': 'g_doze', 'ru': 'дремлет в кресле (есть в коде; нос и пучок — входы)', 'p': (10250, -5800), 'face': 90, 'seconds': 60, 'uses': ['art288'], 'when': ['evening', 'night']},
    ],
    'kid': [
        {'id': 'k_play', 'ru': 'играет на ковре у железной дороги (есть в коде: сидит на полу и играет)', 'p': (14800, -5600), 'face': 0, 'seconds': 60, 'uses': ['new89', 'new87'], 'when': ['day', 'evening']},
        {'id': 'k_toybox', 'ru': 'роется в ящике с игрушками, вываливает их на пол', 'p': (9100, -200), 'face': 0, 'seconds': 25, 'uses': ['v3_toybox'], 'when': ['day', 'evening']},
        {'id': 'k_snack', 'ru': 'перекус на детском стульчике (крошки — есть в коде)', 'p': (3300, -1800), 'face': 0, 'seconds': 40, 'uses': ['new0'], 'when': ['day', 'evening']},
        {'id': 'k_tv', 'ru': 'мультики перед телевизором', 'p': (13000, -6100), 'face': 180, 'seconds': 60, 'uses': ['art190'], 'when': ['evening']},
        {'id': 'k_draw', 'ru': 'рисует за журнальным столом', 'p': (13750, -4750), 'face': 0, 'seconds': 40, 'uses': ['art180'], 'when': ['day']},
        {'id': 'k_balloons', 'ru': 'дёргает шарики за ленточки', 'p': (17000, -1300), 'face': 0, 'seconds': 15, 'uses': ['v3_bal0'], 'when': ['day', 'evening']},
    ],
    'dog': [
        {'id': 'd_bowl', 'ru': 'миска (есть)', 'p': (-3000, -3800), 'seconds': 14, 'when': ['day', 'evening', 'night']},
        {'id': 'd_beg', 'ru': 'клянчит под столом, пока Человек ужинает', 'p': (600, -2300), 'seconds': 40, 'when': ['evening']},
        {'id': 'd_bed', 'ru': 'лежанка у батареи (есть)', 'p': (18150, -7900), 'seconds': 45, 'when': ['day', 'evening', 'night']},
        {'id': 'd_door', 'ru': 'ждёт у двери в коридор, пока Человек выносит мусор', 'p': (19000, -1800), 'seconds': 45, 'when': ['evening']},
        {'id': 'd_rad_k', 'ru': 'греется у батареи кухни', 'p': (0, -4300), 'seconds': 30, 'when': ['day', 'night']},
    ],
    'cat': [
        {'id': 'c_fridge', 'ru': 'спит на холодильнике (новое высокое место)', 'p': (3700, 3900, 3700), 'seconds': 60, 'when': ['day', 'night']},
        {'id': 'c_shelf', 'ru': 'спит на книжном шкафу рядом с подарком', 'p': (19700, -5000, 4660), 'seconds': 60, 'when': ['evening', 'night']},
        {'id': 'c_sill', 'ru': 'подоконник кухни (есть)', 'p': (0, -4900, 2200), 'seconds': 25, 'when': ['day', 'evening']},
        {'id': 'c_sofa', 'ru': 'диван (есть)', 'p': (15500, 400, 950), 'seconds': 60, 'when': ['day', 'evening', 'night']},
        {'id': 'c_post', 'ru': 'когтеточка (есть)', 'p': (8000, -8300, 1311), 'seconds': 45, 'when': ['day', 'evening']},
        {'id': 'c_flap', 'ru': 'уходит во двор через дверцу и возвращается', 'p': (7400, -7350, 0), 'seconds': 10, 'when': ['evening', 'night']},
    ],
}

# ---------------------------------------------------------------------------------------------------------------------------------
# 8. District variants. Rooms, walls, the nest and the big landmarks (fridge, stove, sink, table, sofa, TV, bookcase, armchair, bin,
#    the cabinets) never move. Each district has 3-4 authored states; a seed picks one per district.
#    ops: ('add', piece-dict) ('remove', id) ('restore', v2 id removed by BASE) ('move', id, (x, y), yaw, z0) ('mover', id, 'open'|'closed'|'ajar:<deg>')
#         ('edge', a, b, 'closed'|'slow:<k>'|'cover', note) ('conn_off', connector id) ('hide', {rect, z, ru}) ('hide_off', hide id)
#         ('hazard', {id, ru, kind, circle|rect}) ('hazard_off', id) ('loot_z', [loot ids], z: lift loot onto what now covers it) ('cake', spot id, weight) ('dog_spot', label, (x, y)) ('cat_spot', label, (x, y, z))
# ---------------------------------------------------------------------------------------------------------------------------------
def V(id, ru, plays, ops, weight=1.0, when=None):
    return {'id': id, 'ru': ru, 'plays': plays, 'ops': ops, 'weight': weight, 'when': when}
_STOOL = lambda cx, district: _a('v3_stool', 'SM_V3_StepStool', (cx, -400, 0), 90, 'Табурет-стремянка бабушки у торца стола', 'kitchen', 'floor', district,
                                hide={'rect': [cx - 450, -750, cx + 450, -50], 'z': [0, 440], 'ru': 'под табуретом-стремянкой'})
VARIANTS = {
    'palisad': [
        V('A', 'Тихий вечер', 'как v2: лужайка, тротуар, машина; от гнезда к северному лазу — 17 с по открытому газону', []),
        V('B', 'Мешки у калитки', 'два мешка мусора у фасада: путь к северному лазу в укрытии, красная засада у угла дома теряет обзор',
          [('add', _a('v3_trash0', 'SM_V3_TrashBag', (-8500, 4100, 0), 20, 'Мешок мусора у калитки', 'street', 'floor', 'palisad', hide={'rect': [-8880, 3770, -8120, 4430], 'z': [0, 620], 'ru': 'за мешком у калитки'})),
           ('add', _a('v3_trash1', 'SM_V3_TrashBag', (-7700, 3100, 0), -30, 'Мешок мусора', 'street', 'floor', 'palisad')),
           ('edge', 'nest', 'nhole', 'cover', 'вдоль мешков')]),
        V('C', 'После дождя', 'лужи вдоль фасада: путь к северному лазу мокрый и медленный, все идут через щель гнезда — кухня под нагрузкой',
          [('add', _a('v3_rain0', 'SM_K_Puddle', (-7800, 800, 0), 30, 'Лужа после дождя', 'street', 'floor', 'palisad', copy=True, scale=[3.2, 3.2, 1])),
           ('add', _a('v3_rain1', 'SM_K_Puddle', (-8200, 2600, 0), -60, 'Лужа после дождя', 'street', 'floor', 'palisad', copy=True, scale=[2.6, 2.6, 1])),
           ('hazard', {'id': 'rain', 'ru': 'Мокро: лужи у фасада', 'kind': 'wet', 'rect': [-8700, 200, -7200, 3200]}),
           ('edge', 'nest', 'nhole', 'slow:1.4', 'мокро')]),
    ],
    'reddvor': [
        V('A', 'Полив', 'v2: поливалка 9 с из 26, сухая дорожка по шлангу', []),
        V('B', 'Бельё сушится', 'поливалка выключена; три простыни на верёвке через двор A: тень и укрытие от красных разведчиков, по столбу — на верёвку',
          [('add', _a('v3_pole0', 'SM_V3_LaundryPole', (-6000, -7600, 0), 0, 'Столб бельевой верёвки', 'yard', 'floor', 'reddvor')),
           ('add', _a('v3_pole1', 'SM_V3_LaundryPole', (1000, -7600, 0), 0, 'Столб бельевой верёвки', 'yard', 'floor', 'reddvor')),
           ('add', _a('v3_line', 'SM_V3_LaundryLine', (-6000, -7600, 2850), 0, 'Бельевая верёвка', 'yard', 'hang', 'reddvor', scale=[7000 / 6000.0, 1, 1])),
           ('add', _a('v3_sheet0', 'SM_V3_CurtainPanel', (-4700, -7600, 1180), 0, 'Простыня', 'yard', 'hang', 'reddvor', scale=[2.2, 1, 0.32], tint='Enamel')),
           ('add', _a('v3_sheet1', 'SM_V3_CurtainPanel', (-2500, -7600, 1180), 0, 'Пододеяльник', 'yard', 'hang', 'reddvor', scale=[2.2, 1, 0.32], tint='Teal')),
           ('add', _a('v3_sheet2', 'SM_V3_CurtainPanel', (-300, -7600, 1180), 0, 'Простыня', 'yard', 'hang', 'reddvor', scale=[2.2, 1, 0.32], tint='Enamel')),
           ('hazard_off', 'sprinkler'), ('edge', 'corner', 'gryadka', 'cover', 'под бельём, сухо')]),
        V('C', 'Газон стригут', 'робот-газонокосилка выехал на середину двора A, шланг смотан: сухой дорожки нет',
          [('move', 'new105', (-4000, -7000), 30, None), ('remove', 'new102'), ('conn_off', 'hose'), ('edge', 'dvor_e', 'gryadka', 'slow:1.2', 'без сухой дорожки')]),
    ],
    'plintus': [
        V('A', 'Чисто', 'газета, мешок корма, батарея: каждые 3 м есть куда нырнуть', []),
        V('B', 'Бабушка с рынка', 'сумка-тележка у западной стены: тяжёлая еда (картошка) прямо у гнезда, за ней укрытие; бабушка через 2 мин идёт её разбирать',
          [('add', _a('v3_trolley', 'SM_V3_ShoppingTrolley', (-6550, -3500, 0), -90, 'Сумка-тележка бабушки', 'kitchen', 'floor', 'plintus',
                      hide={'rect': [-7000, -3840, -6220, -3160], 'z': [0, 600], 'ru': 'за сумкой-тележкой'}))]),
        V('C', 'Газеты на сдачу', 'стопка газет у стены между щелью и метлой: ступень на 520 и укрытие под бечёвкой',
          [('add', _a('v3_papers', 'SM_V3_NewspaperStack', (-6450, -1700, 0), -90, 'Стопка газет', 'kitchen', 'floor', 'plintus',
                      hide={'rect': [-6750, -2120, -6150, -1280], 'z': [0, 520], 'ru': 'между газетами'}))]),
        V('D', 'Миски у двери', 'миски и мешок корма переставлены к двери в гостиную: Бадрик пасётся у порога, западная половина кухни спокойнее',
          [('move', 'art110', (3300, -4100), None, None), ('move', 'art111', (2350, -4050), None, None), ('move', 'art112', (3950, -4150), None, None),
           ('move', 'art113', (2350, -4050), None, None), ('move', 'art166', (3250, -4550), None, None), ('move', 'v3_dogfood', (4900, -4750), None, None),
           ('dog_spot', 'миска', (3450, -4050))]),
    ],
    'kladovye': [
        V('A', 'Всё закрыто', 'как v2: ящики задвинуты (лесенку муравьи выдвигают сами — ящики лёгкие), дверцы закрыты — еда за дверцами', []),
        V('B', 'Ящики лесенкой', 'малыш выдвинул ящики: лесенка со столешницы на пол готова с начала рейда — тяжёлое вниз без метлы',
          [('mover', 'door_drawer', 'open'), ('mover', 'mover2', 'open'), ('mover', 'mover3', 'open')]),
        V('C', 'Шкаф открыт', 'бабушка искала муку: шкаф у тостера и хлебница открыты',
          [('mover', 'mover0', 'open'), ('mover', 'mover1', 'open'), ('mover', 'breadbox_lid', 'open')]),
    ],
    'moika': [
        V('A', 'Посуда помыта', 'v2: тарелки в сушилке, дверца под мойкой открыта', []),
        V('B', 'Гора посуды', 'раковина полна посуды: слив закрыт (без телепорта), Человек раньше придёт мыть посуду',
          [('add', _a('v3_dishes', 'SM_V3_DirtyDishStack', (-3500, 4000, 1150), 180, 'Гора посуды в раковине', 'kitchen', 'surface', 'moika')), ('conn_off', 'drain'), ('conn_off', 'pipe')]),
        V('C', 'Дверца закрыта', 'дверца под мойкой закрыта: внутрь только через дыру из коридора и по трубе',
          [('mover', 'mover8', 'closed')]),
    ],
    'plita': [
        V('A', 'Суп на плите', 'конфорка под кастрюлей горит с начала: путь по плите — только по краю', [('hazard', {'id': 'burner_on', 'ru': 'Конфорка включена с начала', 'kind': 'heat', 'circle': [-350, 3500, 460]})]),
        V('B', 'Пирог в духовке', 'дверца духовки приоткрыта (полка на 300), внутри пирог — трофей ур. 3, но жарко',
          [('mover', 'door_oven', 'open')]),
        V('C', 'Полотенце сохнет', 'полотенце снято с духовки и сушится на батарее под окном: на плиту только с рейлинга и полочек специй, зато у окна новая тряпка-укрытие',
          [('move', 'art156', (0, -4880), 0, 0, 'on:v3_rad_k'), ('conn_off', 'towel')]),
    ],
    'lednik': [
        V('A', 'Рисунки', 'магниты и рисунки — путь наверх по боку холодильника', []),
        V('B', 'Посудомойка открыта', 'посудомойку разгружают: дверца лежит рампой у пола (вход внутрь без троих), корзины на месте — выдвинутые корзины отрезали бы Человеку мойку и плиту, когда на столе трасса (Плато B)',
          [('mover', 'door_dishwasher', 'open')]),
        V('C', 'Холодильник приоткрыт', 'папа не закрыл холодильник: внутрь без троих, но Человек закроет его, заметив (и запрёт того, кто внутри)',
          [('mover', 'door_fridge', 'ajar:35'), ('cake', 'M3', 1.6)]),
    ],
    'plato': [
        V('A', 'Ужин накрыт', 'скатерть, чайник, чашки; трасса убрана, у торца — табурет-стремянка (ступени 450/900 — пандус для тяжёлого)',
          [('remove', 'new1'), ('conn_off', 'car_track'), ('add', _STOOL(-2150, 'plato')),
           ('add', _a('v3_cloth', 'SM_V3_Tablecloth', (500, -500, 950), 180, 'Скатерть', 'kitchen', 'on:art77', 'plato', cloth=True)),
           ('add', _a('v3_teapot', 'SM_V3_Teapot', (1300, -700, 1406), 160, 'Заварочный чайник', 'kitchen', 'on:art77', 'plato')),
           ('add', _a('v3_teapot_lid', 'SM_V3_Teapot_Lid', (1280, -700, 1716), 160, 'Крышка чайника', 'kitchen', 'on:v3_teapot', 'plato', part_of='v3_teapot')),
           ('add', _a('v3_cup0', 'SM_V3_TeaCup', (300, -1500, 1406), 0, 'Чашка папы', 'kitchen', 'on:art77', 'plato')),
           ('add', _a('v3_cup1', 'SM_V3_TeaCup', (2150, -1300, 1406), 40, 'Чашка бабушки', 'kitchen', 'on:art77', 'plato'))], when=['evening']),
        V('B', 'Малыш завтракал', 'без скатерти; трасса для машинок от стола к полу (v2), грузовик и арка малыша у трассы, хлопья на столе, крошки у стульчика',
          [('add', _a('v3_cereal_t', 'SM_V3_CerealBox', (2100, -1250, 1400), 30, 'Хлопья на столе', 'kitchen', 'on:art77', 'plato')), ('restore', 'new85'), ('restore', 'new80')], when=['day']),
        V('C', 'Стулья придвинуты', 'оба стула вплотную к столу: сиденье 880 -> стол 1400 (ступень 520 — тяжёлое тащат); малыш катал машинки: трасса, грузовик и арка на кухне',
          [('move', 'art78', (0, -2290), 0, None), ('move', 'art79', (1200, 1290), 180, None), ('restore', 'new85'), ('restore', 'new80'),
           ('add', _a('v3_cloth', 'SM_V3_Tablecloth', (500, -500, 950), 180, 'Скатерть', 'kitchen', 'on:art77', 'plato', cloth=True))]),
        V('D', 'Длинная скатерть', 'праздничная скатерть до пола: под столом шатёр (Человек не видит), но там спит Бадрик; табурет снаружи у торца',
          [('remove', 'new1'), ('conn_off', 'car_track'), ('add', _STOOL(-2215, 'plato')),
           ('add', _a('v3_cloth_long', 'SM_V3_TableclothLong', (500, -500, 40), 180, 'Длинная скатерть', 'kitchen', 'on:art77', 'plato', cloth=True)),
           ('hide', {'id': 'tent', 'rect': [-1700, -1800, 2700, 800], 'z': [0, 1320], 'ru': 'под длинной скатертью'})], when=['evening']),
    ],
    'svalka': [
        V('A', 'Полное ведро', 'v2: бутылки и банки у ведра, пакет, коробка-форт', []),
        V('B', 'Бутылки сдали', 'бутылки и банки унесли: у ведра голый пол, укрытия — только коробка, пакет и новый мешок с бутылками у стены',
          [('remove', 'new13'), ('remove', 'new14'), ('remove', 'new16'), ('remove', 'new17'),
           ('add', _a('v3_recycle', 'SM_V3_TrashBag', (4900, -1500, 0), 90, 'Мешок с бутылками на сдачу у ведра', 'kitchen', 'floor', 'svalka',
                      hide={'rect': [4570, -1880, 5230, -1120], 'z': [0, 620], 'ru': 'за мешком с бутылками'})),
           ('add', _a('v3_recycle2', 'SM_V3_TrashBag', (4950, 1000, 0), 90, 'Мешок с банками у коробки', 'kitchen', 'floor', 'svalka',
                      hide={'rect': [4620, 620, 5280, 1380], 'z': [0, 620], 'ru': 'за мешком с банками'}))]),
        V('C', 'Коробку сложили', 'картонную коробку сложили и прислонили к стене: форта нет, мусорный лаз на виду',
          [('remove', 'new11'), ('conn_off', 'box_hole'), ('conn_off', 'box_flaps'), ('loot_z', ['L220', 'L221', 'L226', 'H2'], 200),
           ('add', _a('v3_box_flat', 'SM_K_CardboardBox', (6250, 1150, 0), 90, 'Сложенная коробка', 'kitchen', 'floor', 'svalka', copy=True, scale=[1, 1, 0.12],
                      hide={'rect': [5560, 190, 6940, 2116], 'z': [0, 190], 'ru': 'между листами сложенной коробки'})),
           ('edge', 'svalka', 'vent2', 'open', 'за коробкой больше не спрятаться')]),
    ],
    'zastenye': [
        V('A', 'Сухо', 'v2', []),
        V('B', 'Протечка', 'течёт сифон: в коридоре за шкафами лужа от мойки до западного лаза — мокро и медленно',
          [('hazard', {'id': 'leak', 'ru': 'Мокро: протечка за шкафами', 'kind': 'wet', 'rect': [-6800, 4750, -2600, 5000]}), ('edge', 'corr_w', 'corr_m', 'slow:1.5', 'мокро')]),
        V('C', 'Завал', 'за плиту упал противень и стоит ребром: коридор перегорожен посередине',
          [('add', _a('v3_jam_tray', 'SM_K_Range_Tray', (-750, 4875, 0), 90, 'Упавший противень (ребром)', 'kitchen', 'floor', 'zastenye', copy=True, tilt=(90, 0), solid=True)),
           ('edge', 'corr_m', 'corr_e', 'closed', 'противень')]),
    ],
    'porog': [
        V('A', 'Дверь настежь', 'v2: дверь сложена к стене, щель под ней — укрытие', []),
        V('B', 'Дверь не до конца', 'дверь открыта на 100°, а не сложена к стене: щели под ней нет, зато створка — ширма между порогом и обувницей',
          [('mover', 'mover27', 'ajar:100'), ('hide_off', 'door_nook'), ('hide', {'id': 'door_leaf100', 'rect': [7250, -3300, 8830, -2900], 'z': [0, 30], 'ru': 'под створкой двери (щель 23)'})]),
        V('C', 'Обувь у порога', 'папины ботинки и сапоги бабушки у двери: цепочка укрытий через порог',
          [('add', _a('v3_shoe0', 'SM_K_Shoe', (7650, -4550, 0), 10, 'Ботинок папы', 'living', 'floor', 'porog', copy=True, hide={'rect': [7347, -5080, 7953, -4020], 'z': [0, 388], 'ru': 'в ботинке у порога'})),
           ('add', _a('v3_shoe1', 'SM_K_Shoe', (8350, -4050, 0), -15, 'Второй ботинок', 'living', 'floor', 'porog', copy=True)),
           ('add', _a('v3_shoe2', 'SM_K_Shoe', (6400, -4400, 0), 20, 'Ботинок на кухне у двери', 'kitchen', 'floor', 'porog', copy=True, hide={'rect': [6110, -4945, 6690, -3855], 'z': [0, 388], 'ru': 'в ботинке у двери'}))]),
    ],
    'babushka': [
        V('A', 'Вяжет', 'бабушка в кресле, вязание на корзине, клубок на полу', []),
        V('B', 'Поливала цветы', 'стремянка у окна гостиной, лейка на полу: бабушки нет в кресле (рутина у окна)',
          [('add', _a('v3_stool_l', 'SM_V3_StepStool', (16600, -7300, 0), 180, 'Стремянка у окна гостиной', 'living', 'floor', 'babushka',
                      hide={'rect': [16250, -7750, 16950, -6850], 'z': [0, 440], 'ru': 'под стремянкой'})),
           ('add', _a('v3_can_l', 'SM_Y_WateringCan', (17000, -6400, 0), -40, 'Лейка бабушки', 'living', 'floor', 'babushka', copy=True))], when=['day']),
        V('C', 'Дверца подпёрта', 'кошачья дверца подпёрта тапком: выход во двор без троих — и вход для красных',
          [('add', _a('v3_flap_slipper', 'SM_K_Slipper', (7330, -7350, 0), 90, 'Тапок подпирает кошачью дверцу', 'living', 'on:new95', 'babushka', copy=True, part_of='new96')),
           ('mover', 'cat_flap', 'open'), ('edge', 'flap', 'dvor_e', 'open', 'дверца подпёрта')]),
    ],
    'play': [
        V('A', 'Железная дорога', 'v2: кольцо дороги на ковре, паровозик', []),
        V('B', 'Шалаш', 'малыш построил шалаш из пледа между диваном и журнальным столом: огромное укрытие, но малыш возвращается в него',
          [('add', _a('v3_fort', 'SM_V3_BlanketFort', (13750, -1450, 0), 180, 'Шалаш из пледа', 'living', 'on:art180', 'play', cloth=True)),
           ('hide', {'id': 'fort_tent', 'rect': [12100, -2700, 15400, -260], 'z': [0, 900], 'ru': 'в шалаше'}), ('edge', 'divan', 'kover', 'cover', 'через шалаш')]),
        V('C', 'Прибрано', 'дорога и паровозик в ящике, кубиковая лесенка у стола разобрана: ковёр голый, торт с журнального стола — только по книжке',
          [('remove', 'new89'), ('remove', 'new87'), ('remove', 'new50'), ('remove', 'new51'), ('remove', 'new52'), ('remove', 'new53'), ('remove', 'new54'), ('remove', 'new55'),
           ('remove', 'new56'), ('remove', 'new57'), ('remove', 'new58'), ('remove', 'new59'), ('conn_off', 'block_stairs_ct'), ('cake', 'M1', 0.6)]),
        V('D', 'Праздник', 'ещё два подарка на журнальном столе и шарики на стульях: торт вероятнее на журнальном столе',
          [('add', _a('v3_gift1', 'SM_V3_GiftBox', (12760, -3450, 980), 90, 'Подарок на журнальном столе', 'living', 'on:art180', 'play', scale=0.6)),
           ('add', _a('v3_gift1_lid', 'SM_V3_GiftBox_Lid', (12760, -3450, 1292), 90, 'Крышка', 'living', 'on:v3_gift1', 'play', scale=0.6, part_of='v3_gift1')),
           ('cake', 'M1', 1.6)], when=['evening']),
    ],
    'stenka': [
        V('A', 'Телевизор выключен', 'v2', []),
        V('B', 'Мультики', 'телевизор включён с начала (шум): малыш сидит перед ним, Человек приходит выключать', [('hazard', {'id': 'tv_on', 'ru': 'Телевизор включён', 'kind': 'noise', 'circle': [13150, -8000, 1500]})], when=['evening', 'night']),
        V('C', 'Подарки за шторой', 'подарки спрятаны под окном за шторой: укрытие у батареи, конфеты, торт может быть в большом подарке',
          [('add', _a('v3_gift2', 'SM_V3_GiftBox', (19700, -8400, 0), 0, 'Большой подарок под окном, за восточной шторой', 'living', 'floor', 'stenka', hide={'rect': [19250, -8750, 20150, -8050], 'z': [0, 520], 'ru': 'за подарками'})),
           ('add', _a('v3_gift2_lid', 'SM_V3_GiftBox_Lid', (19700, -8400, 520), 0, 'Крышка', 'living', 'on:v3_gift2', 'stenka', part_of='v3_gift2')),
           ('add', _a('v3_gift3', 'SM_V3_GiftBox', (19700, -8400, 670), 25, 'Подарок поменьше', 'living', 'on:v3_gift2_lid', 'stenka', scale=0.55)),
           ('cake', 'M4', 1.0), ('edge', 'tv', 'okno', 'cover', 'вдоль подарков')]),
    ],
}
# the cake places: v2 M1-M3 + v3 M4 (only when stenka C is picked); node = graph node it is carried from
CAKE_SPOTS = {'M1': {'node': 'kover', 'w': 0.4, 'ru': 'на журнальном столе'}, 'M2': {'node': 'tv', 'w': 0.4, 'ru': 'в тумбе ТВ'},
              'M3': {'node': 'holgap', 'w': 0.2, 'ru': 'в ящике для овощей холодильника'},
              'M4': {'node': 'okno', 'w': 0.0, 'ru': 'в большом подарке под окном гостиной', 'p': (19700, -8400, 60), 'new': True}}

# ---------------------------------------------------------------------------------------------------------------------------------
# 8b. «После боя»: the kitchen at the end of a raid where every chain went off (for the contrast render; data for the art of the
#     broken states). ops as in VARIANTS; ('rot', id, axis, deg, pivot) turns an object about a game axis through a game point.
# ---------------------------------------------------------------------------------------------------------------------------------
CHAOS = [
    ('rot', 'v3_shelf', 'x', -32, (-6800, 800, 2660)), ('remove', 'v3_shelf_sc0'), ('remove', 'v3_shelf_sc1'),
    ('rot', 'v3_shelf_br0', 'x', -32, (-6800, 800, 2660)),
    ('add', _a('x_sc0', 'SM_V3_Screw', (-6500, -700, 11), 40, 'Выкрученный шуруп на полу', 'kitchen', 'floor', tilt=(0, 90))),
    ('add', _a('x_sc1', 'SM_V3_Screw', (-6300, -950, 11), -70, 'Выкрученный шуруп на полу', 'kitchen', 'floor', tilt=(0, 90))),
    ('remove', 'v3_jam'), ('remove', 'v3_jam_lid'),
    ('add', _a('x_shards', 'SM_V3_GlassShards', (-6450, -650, 0), 20, 'Осколки банки варенья', 'kitchen', 'floor')),
    ('add', _a('x_jam', 'SM_V3_JamSplat', (-6400, -700, 0), 10, 'Варенье на полу', 'kitchen', 'floor')),
    ('add', _a('x_lid', 'SM_V3_JamJar_Lid', (-6050, -300, 0), 0, 'Крышка банки', 'kitchen', 'floor', tilt=(70, 0))),
    ('remove', 'v3_pickles'), ('add', _a('x_pickles', 'SM_V3_PickleJar', (-6450, 300, 110), 70, 'Банка огурцов на боку', 'kitchen', 'floor', tilt=(90, 0))),
    ('remove', 'v3_curt_w'), ('add', _a('x_curtain', 'SM_V3_CurtainTorn', (-1900, -4150, 0), 15, 'Сорванная штора', 'kitchen', 'floor')),
    ('rot', 'v3_rod', 'y', -40, (2000, -4730, 5320)), ('rot', 'v3_rodbr0', 'y', -40, (2000, -4730, 5320)),
    ('rot', 'v3_curt_e', 'z', 0, (0, 0, 0)),
    ('remove', 'art92'), ('add', _a('x_soil', 'SM_V3_SoilHeap', (-900, -4450, 0), 30, 'Разбитый цветок', 'kitchen', 'floor')),
    ('remove', 'v3_cola'), ('remove', 'v3_cola_cap'),
    ('add', _a('x_cola', 'SM_V3_ColaBottle', (2100, 2650, 100), -60, 'Бутылка колы на полу', 'kitchen', 'floor', tilt=(90, 0))),
    ('add', _a('x_cap', 'SM_V3_ColaCap', (2700, 2300, 0), 0, 'Крышка колы', 'kitchen', 'floor')),
    ('add', _a('x_puddle', 'SM_V3_ColaPuddle', (1700, 2150, 0), 8, 'Лужа колы', 'kitchen', 'floor')),
    ('remove', 'v3_cereal'), ('add', _a('x_cereal', 'SM_V3_CerealBox', (350, 2700, 70), 100, 'Коробка хлопьев упала', 'kitchen', 'floor', tilt=(90, 0))),
    ('add', _a('x_spill', 'SM_V3_CerealSpill', (-300, 2300, 0), 160, 'Хлопья по полу', 'kitchen', 'floor')),
    ('remove', 'v3_cloth'), ('add', _a('x_cloth', 'SM_V3_TableclothPulled', (500, -500, 0), 180, 'Скатерть стянута', 'kitchen', 'floor')),
    ('remove', 'v3_teapot'), ('remove', 'v3_teapot_lid'), ('remove', 'v3_cup0'),
    ('add', _a('x_teapot', 'SM_V3_GlassShards', (900, -2650, 0), 60, 'Черепки чайника', 'kitchen', 'floor', scale=1.3)),
    ('add', _a('x_cup', 'SM_V3_TeaCup', (300, -2500, 70), 30, 'Чашка на боку', 'kitchen', 'floor', tilt=(80, 0))),
    ('rot', 'art78', 'x', 90, (0, -2750, 0)), ('move', 'new9', (-400, -2900), None, None), ('move', 'new10', (1500, -3100), None, None),
    ('add', _a('x_draw', 'SM_V3_KidDrawing', (3600, 2500, 4), 20, 'Рисунок упал с холодильника', 'kitchen', 'floor', tilt=(90, 0))),
    ('remove', 'v3_draw0'), ('remove', 'v3_draw0_t0'),
    ('move', 'art134', (-6350, -1150), 40, 0), ('move', 'art133', (-6600, -300), None, 0),
    ('add', _a('x_bag', 'SM_V3_TrashBag', (4800, -1600, 0), 70, 'Мешок из ведра на боку', 'kitchen', 'floor', tilt=(70, 0))),
    ('add', _a('x_spill2', 'SM_V3_CerealSpill', (-700, -2700, 0), -30, 'Хлопья у стола', 'kitchen', 'floor')),
    ('add', _a('x_shards2', 'SM_V3_GlassShards', (300, -3900, 0), 0, 'Осколки чашки', 'kitchen', 'floor')),
    ('mover', 'mover10', 'ajar:70'), ('mover', 'mover0', 'open'), ('mover', 'mover8', 'open'),
]

# ---------------------------------------------------------------------------------------------------------------------------------
# 9. Seeds: what is fixed, what a seed draws, and the rules a seed must pass (Tools/level/check_level_v3.py).
# ---------------------------------------------------------------------------------------------------------------------------------
SEED = {
    'fixed': ['стены, окна, двери-проёмы, щели и лазы', 'гнездо и южный вход', 'большие ориентиры: холодильник, плита, мойка, шкафы, стол, диван, ТВ и тумба, книжный шкаф, кресло, ведро, батареи, кровать Бадрика',
              'красный холм, дерево, бак', 'граф маршрутов (узлы), районы и их имена'],
    'time': {'day': 0.4, 'evening': 0.45, 'night': 0.15},
    'time_by_threat': {4: {'evening': 0.75, 'night': 0.25}},       # День рождения — вечер; каждый четвёртый (по кубику) — ночь, 23:00, гости ушли
    # v3.1 (owner's decision, 2026-11-21): who is home and the time of day follow the threat tier instead of the dice.
    # The draws are still made (the variant dice after them stay where they were), then replaced by the tier's.
    'by_threat': {1: {'time': 'day', 'home': {'grandma': True, 'kid': False, 'dog': True, 'cat': True},
                      'ru': 'Будний день: день, малыш в садике, дома бабушка, Бадрик и кошка'},
                  2: {'time': 'day', 'home': {'grandma': True, 'kid': True, 'dog': True, 'cat': True},
                      'ru': 'Выходной: день, дома все'},
                  3: {'time': 'evening', 'home': {'grandma': False, 'kid': True, 'dog': True, 'cat': True},
                      'ru': 'Большая уборка: вечер, бабушка уехала к подруге, папа убирает один'},
                  4: {'time': None, 'home': {'grandma': True, 'kid': True, 'dog': True, 'cat': True},
                      'ru': 'День рождения: вечер (каждый четвёртый рейд — ночь после праздника), дома все'}},
    'home': {'grandma': 0.75, 'kid': 0.85, 'dog': 0.9, 'cat': 0.85},
    'home_rules': ['Человек дома всегда', 'дома минимум двое из четырёх (бабушка, малыш, Бадрик, кошка)', 'на 4-й угрозе (День рождения) малыш дома всегда',
                   'бабушки нет -> район «Бабушкин угол» не берёт вариант B (стремянка у окна — её рутина)', 'малыша нет -> «Игровая» берёт C (прибрано) с весом x3, шалаша (B) не бывает'],
    'doors_random': {'pool': ['mover4', 'mover5', 'mover6', 'mover7', 'mover9', 'mover10', 'mover11', 'mover12', 'mover13', 'mover14', 'mover15', 'mover16', 'mover17', 'mover18', 'mover19', 'mover25', 'mover26'],
                     'n': (0, 3), 'state': 'ajar:25', 'ru': '0–3 случайных дверцы шкафов приоткрыты (не холодильник, не духовка: они только по вариантам)'},
    'loose': {'pool': ['v3_shelf_sc0', 'v3_shelf_sc1', 'v3_shelf_sc2', 'v3_shelf_sc3', 'v3_rod_sc0', 'v3_rod_sc1', 'v3_rod_sc2', 'v3_rod_sc3', 'v3_rod_l_sc0', 'v3_rod_l_sc1',
                       'v3_rod_l_sc2', 'v3_rod_l_sc3', 'v3_cola_cap', 'v3_draw0_t0', 'v3_draw1_t1', 'v3_nail_l1', 'v3_needle0'], 'n': (1, 3),
              'ru': '1–3 крепежа уже «шатаются» (откручиваются вдвое быстрее): на шурупе заметна щель и пыль'},
    'scatter': [   # zones of small mess: rect, z, what, count; items keep 300 uu apart, never in a door sweep or on a routine spot
        {'id': 'crumbs_table', 'ru': 'крошки под столом', 'rect': [-1600, -1700, 2600, 700], 'z': 0, 'items': ['крошка хлеба', 'крошка печенья', 'горошина'], 'n': (2, 5)},
        {'id': 'crumbs_chair', 'ru': 'крошки у детского стульчика', 'rect': [2700, -1600, 4100, 300], 'z': 0, 'items': ['хлопья', 'изюм'], 'n': (1, 4)},
        {'id': 'toys', 'ru': 'игрушки по пути от ящика', 'rect': [9600, -3000, 12400, -400], 'z': 0, 'items': ['SM_L_ToyBlock', 'SM_L_ToyCar'], 'n': (0, 3), 'needs': 'kid'},
        {'id': 'yarn', 'ru': 'обрывки пряжи у кресла', 'rect': [8600, -4600, 10400, -3600], 'z': 0, 'items': ['клубочек'], 'n': (0, 1), 'needs': 'grandma'},
        {'id': 'dog_mess', 'ru': 'корм вокруг миски', 'rect': [-4500, -4500, -1800, -3200], 'z': 15, 'items': ['корм'], 'n': (2, 6), 'needs': 'dog'},
        {'id': 'sofa_crumbs', 'ru': 'крошки чипсов у дивана', 'rect': [10400, -1200, 16600, -350], 'z': 0, 'items': ['чипсина', 'попкорн'], 'n': (1, 4)},
        {'id': 'mess', 'ru': 'беспорядок v2 по сиду: рассыпанный сахар, лужа варенья', 'rect': None, 'items': ['new38', 'new39'], 'n': (0, 1), 'threat_max': 2},
    ],
    'rules': ['каждый район: ровно один вариант', 'варианты с when берутся только в своё время суток', 'варианты зависят от того, кто дома (home_rules)',
              'торт: одно место из включённых (M1–M3 всегда, M4 — если «Стенка C»), вес места меняют варианты (op cake)'],
}
# loot points that v3 had to move (v2 loot spot index -> new point, why). Points that were IN or UNDER a removed object (bottles, socks,
# toys) stay where they are: they were floor points, now they are simply on the floor.
LOOT_FIX = {64: ((1740, -360, 1405), 'стояла на верхушке декоративного торта (1906): торт убран, точка — на столе')}
# v3 moves two of the Human's patrol stops (LevelV2Data patrol points are data): they stood where the variants put furniture
PATROL_FIX = {2: ((-3700, 1300), 'точка у кладовых: выдвинутые ящики (Кладовые B) закрывали её; на 1000 восточнее и 600 южнее'),
              10: ((11500, -2100), 'точка перед диваном: в шалаше (Игровая B); на 1500 западнее')}
# sloped pieces whose low end the Human steps over (FHumanNav: a cell is free where his capsule passes from the step height up):
# id -> (axis, +1 if the high end is at the max of that axis). Only the part higher than the 250 step blocks him.
HUMAN_STEP_RAMPS = {'new1': ('x', 1)}
VALIDATE = {
    'cake_routes': 2,                 # edge-disjoint haul routes from the cake's node to the nest
    'cake_route_max_s': [200.0, 330.0],   # carried at 0.7 of walk speed (288 uu/s): the best route <= 200 s, the second <= 330 s (raid 660 s)
    'cover_max_s': 4.0,               # every floor cell of the kitchen and the living room: sprint to cover
    'cover_tolerance_s': 0.0,         # strict: 4.0 s
    'human_capsule': [340, 250],      # radius, step height
    'routine_reach_uu': 450,          # a routine spot counts as reachable when he can stand within this distance of it
    'scatter_spacing_uu': 300,
}

# ---------------------------------------------------------------------------------------------------------------------------------
# 10. compose(seed): the draw. Deterministic and portable: mulberry32, the same in Python and in the map's JavaScript.
# ---------------------------------------------------------------------------------------------------------------------------------
class Rng:
    def __init__(self, seed):
        self.a = (int(seed) * 2654435761 + 0x9E3779B9) & 0xFFFFFFFF
    def next(self):
        self.a = (self.a + 0x6D2B79F5) & 0xFFFFFFFF
        t = self.a
        t = ((t ^ (t >> 15)) * (t | 1)) & 0xFFFFFFFF
        t ^= (t + (((t ^ (t >> 7)) * (t | 61)) & 0xFFFFFFFF)) & 0xFFFFFFFF
        return ((t ^ (t >> 14)) & 0xFFFFFFFF) / 4294967296.0
    def pick(self, items, weights):
        tot = sum(weights)
        if tot <= 0: return items[0]
        r = self.next() * tot
        for it, w in zip(items, weights):
            r -= w
            if r < 0: return it
        return items[-1]
    def int(self, a, b):
        return a + int(self.next() * (b - a + 1))

DISTRICT_ORDER = ['palisad', 'reddvor', 'plintus', 'kladovye', 'moika', 'plita', 'lednik', 'plato', 'svalka', 'zastenye', 'porog', 'babushka', 'play', 'stenka']

def compose(seed, threat=1, force=None):
    """The draw for one raid: time of day, who is home, one variant per district, doors, loose fasteners, cake spot, scatter.
    force: {district: variant id} to pin variants (the per-variant checks use it). Pure: geometry is applied by level_v3_lib.apply()."""
    r = Rng(seed)
    tw = SEED['time_by_threat'].get(threat, SEED['time'])
    times = list(tw.keys())
    time = r.pick(times, [tw[k] for k in times])
    home = {'human': True}
    for k in ('grandma', 'kid', 'dog', 'cat'):
        home[k] = r.next() < SEED['home'][k]
    if threat >= 4: home['kid'] = True
    if sum(1 for k in ('grandma', 'kid', 'dog', 'cat') if home[k]) < 2:
        home['dog'] = True; home['kid'] = True
    bt = SEED.get('by_threat', {}).get(threat)
    if bt:   # v3.1: the tier decides (the draws above are kept so the dice below stay in step)
        time = bt['time'] or time; home = dict({'human': True}, **bt['home'])   # (time None: the dice of time_by_threat decide - night only at the top tier)
    variants = {}
    for d in DISTRICT_ORDER:
        vs = VARIANTS[d]
        ws = []
        for v in vs:
            w = v['weight']
            if v.get('when') and time not in v['when']: w = 0.0
            if d == 'babushka' and v['id'] == 'B' and not home['grandma']: w = 0.0
            if d == 'play' and not home['kid']: w = (w * 3.0 if v['id'] == 'C' else (0.0 if v['id'] == 'B' else w))
            ws.append(w)
        variants[d] = r.pick([v['id'] for v in vs], ws)
        if force and d in force: variants[d] = force[d]
    pool = SEED['doors_random']['pool']
    doors = {}
    for _ in range(r.int(*SEED['doors_random']['n'])):
        doors[pool[int(r.next() * len(pool))]] = SEED['doors_random']['state']
    lp = SEED['loose']['pool']
    loose = sorted(set(lp[int(r.next() * len(lp))] for _ in range(r.int(*SEED['loose']['n']))))
    spots = {k: v['w'] for k, v in CAKE_SPOTS.items()}
    for d in DISTRICT_ORDER:
        v = [x for x in VARIANTS[d] if x['id'] == variants[d]][0]
        for op in v['ops']:
            if op[0] == 'cake':
                spots[op[1]] = (spots[op[1]] or 0.25) * op[2] if spots[op[1]] else op[2] * 0.25
    ks = sorted(spots)
    cake = r.pick(ks, [spots[k] for k in ks])
    scatter = []
    for z in SEED['scatter']:
        if z.get('needs') and not home.get(z['needs']): continue
        if z.get('threat_max') and threat > z['threat_max']: continue
        n = r.int(*z['n'])
        for _ in range(n):
            item = z['items'][int(r.next() * len(z['items']))]
            if z['rect'] is None:
                scatter.append({'zone': z['id'], 'item': item, 'v2_object': item}); continue
            x = z['rect'][0] + r.next() * (z['rect'][2] - z['rect'][0]); y = z['rect'][1] + r.next() * (z['rect'][3] - z['rect'][1])
            scatter.append({'zone': z['id'], 'item': item, 'p': [round(x), round(y), z.get('z', 0)], 'yaw': round(r.next() * 360 - 180)})
    return {'seed': seed, 'threat': threat, 'time': time, 'home': home, 'variants': variants, 'doors': doors, 'loose': loose, 'cake': cake, 'cake_weights': spots, 'scatter': scatter}

# ---------------------------------------------------------------------------------------------------------------------------------
# 11. What the seed checker caught while v3 was being laid out, and what was changed (check_level_v3.py prints it with the results).
# ---------------------------------------------------------------------------------------------------------------------------------
FIX_LOG = [
    ('cover', 'Убрали игрушки, носок, бутылки и тапки с пола кухни — укрытие хуже 4 с на 868 клетках кухни и 670 гостиной (худшее 6,4 с)',
     'вернули укрытия, но со смыслом: рюкзак папы у стола, ящик с картошкой у двери, коврик у мойки (щель под ним), мешок корма у мисок, батарея под окном; в гостиной — две напольные подушки у ТВ; грузовик у дороги оставили'),
    ('nav', 'Ужин и дрёма Человека стояли внутри стула и в пледе', 'точки рутины перенесены туда, где стоит его капсула (у стула — на 750 южнее; дрёма — западная часть дивана, запасная — восточная)'),
    ('overlap', 'Мешок корма в плинтусе, дверь в коридор сквозь плинтус, ящик для игрушек в диване на 2 uu', 'мешок на 40 от стены; плинтус восточной стены разрезан на два отрезка; ящик на 20 западнее'),
    ('nav', 'Кладовые B (ящики выдвинуты) закрывали точку патруля №2 у кладовых', 'точка патруля №2 перенесена на (-3700, 1300) — данные LevelV2Data, не код'),
    ('nav', 'Плато B (трасса) + приоткрытая дверца шкафа: Человек не мог дойти до мойки и плиты (полоса между северным стулом и шкафами 736 < 780 капсулы)',
     'северный стул задвинут под стол во всей v3 (порядок в начале рейда); трассу Человек перешагивает там, где она ниже ступени 250 (как FHumanNav)'),
    ('nav', 'Плато B + Ледник B (корзины посудомойки выдвинуты) отрезали мойку и плиту', 'в Леднике B только дверца посудомойки лежит рампой, корзины внутри'),
    ('overlap', 'Подарки за шторой стояли на кубиковой лесенке у тумбы; ботинки у порога — в кресле; лейка — на подушке; мешки у калитки — друг в друге', 'переставлены (подарки — под восточную штору, ботинки — к двери, лейка — к лежанке)'),
    ('float', 'Полотенце на спинке стула висело в воздухе, когда Плато C отодвигало стул', 'полотенце в Плите C сушится на батарее кухни (её никто не двигает)'),
    ('nav', 'Дверь гостиной на 60° (Порог B) закрывала проход: щель 793 при капсуле 780', 'Порог B: дверь на 100°; щель под створкой — новое укрытие на месте старого'),
    ('cover', 'Свалка B (бутылки сдали): 4,75 с у ведра', 'два мешка на сдачу — у ведра и у коробки'),
    ('cover', 'Плато C без трассы: 4,21 с у западного торца стола', 'в Плато C трасса малыша остаётся'),
    ('cover', 'Плато B и C (трасса на месте, табурета нет): 4,1 с у северо-западного угла стола', 'где малыш играл на кухне (B, C), его грузовик и арка возвращаются к трассе — игрушки на кухне только там, где есть история'),
    ('nav', 'Длинная скатерть (Плато D) по габариту закрывала малышу стульчик', 'ткань не препятствие для капсулы (её проходят насквозь, стол под ней уже препятствие); шалаш — препятствие'),
]
