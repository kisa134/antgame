# Ant Kitchen level v2, part 3: the yard and the street. Same conventions as level_v2_p1.py.
import math
from level_v2_common import *

def ant_hill():
    UL([(0, 0), (700, 0), (650, 80), (500, 220), (310, 350), (180, 420), (110, 405), (70, 340), (0, 310)], "Soil", 0, 0, 0, 18)
    for i, (a, r, s) in enumerate(((20, 610, 60), (95, 560, 45), (160, 640, 70), (230, 590, 50), (290, 620, 40), (340, 540, 55))):
        US(s, r * math.cos(math.radians(a)), r * math.sin(math.radians(a)), s * 0.6, "Stone", (1, 0.8, 0.7), 8, 6)
    for a in (40, 170, 290):
        c, s = math.cos(math.radians(a)), math.sin(math.radians(a))
        UC(46, 14, c * 420, s * 420, 262, "Plinth", n=10, rot=(-38 * s, 38 * c, 0))
    piece("SM_Y_AntHill", origin=(0, 0, 0), at=at("SM_Y_AntHill"), note="the red clan's mound: crater on top, three tunnel mouths on the slope; their safe zone has radius 560")

def flower_bed():
    W, D = 1200.0, 300.0
    for k in range(8):
        UB(-W + k * 300 + 4, -W + (k + 1) * 300 - 4, -D, -D + 100, 0, 220 + (k % 2) * 30, "Terracotta", 12, 1)
        UB(-W + k * 300 + 4, -W + (k + 1) * 300 - 4, D - 100, D, 0, 250 - (k % 2) * 30, "Terracotta", 12, 1)
    for s in (-1, 1):
        UB(min(s * W, s * (W - 100)), max(s * W, s * (W - 100)), -D + 104, D - 104, 0, 235, "Terracotta", 12, 1)
    UR(2 * W - 200, 2 * D - 200, 240, 0, 0, 120, "Soil", 40, 2)
    for i, (x, y, h, m) in enumerate(((-900, 20, 420, "CakePink"), (-560, -40, 520, "Honey"), (-200, 30, 380, "Red"), (180, -30, 560, "CakePink"), (540, 40, 440, "Honey"), (880, -10, 500, "Red"))):
        UT([(x, y, 230), (x + 12, y + 6, 230 + h * 0.5), (x - 8, y, 230 + h)], 10, "Leaf", 6)
        US(60, x - 8, y, 240 + h, m, (1, 1, 0.6), 8, 5)
        US(22, x - 8, y, 262 + h, "Cheese", nu=6, nv=4)
        for sgn in (-1, 1):
            US(70, x + sgn * 60, y + 20, 240 + h * 0.4, "LeafLight", (1, 0.45, 0.16), 6, 4, rot=(0, sgn * 30, 0))
    piece("SM_Y_FlowerBed", origin=(0, 0, 0), at=at("SM_Y_FlowerBed"), note="brick-edged bed, soil top z=240; cover between the stems")

def sprinkler():
    UC(130, 30, 0, 0, 15, "PlasticBlue", n=16)
    UC(40, 150, 0, 0, 105, "PlasticBlue", n=10)
    UC(22, 280, 0, 0, 205, "Brass", "X", 8)
    for sx in (-1, 1):
        UC(30, 60, sx * 140, 0, 230, "Brass", n=8, r2=12)
    UC(34, 90, 0, 120, 60, "Brass", "Y", 8)
    piece("SM_Y_Sprinkler", origin=(0, 0, 0), at=at("SM_Y_Sprinkler"))

def hose_and_tap():
    P = V.PLACE["SM_Y_GardenHose"]
    pts = [tuple(v) for v in L.spline([L.G(*p) for p in P["pts"]], 6)]
    L.tube(pts, 34 * K, "Leaf", 8)
    for p in (P["pts"][0], P["pts"][-1]):
        GS(46, p, "Brass", nu=8, nv=6)
    piece("SM_Y_GardenHose", note="garden hose from the wall tap to the sprinkler: 68 uu thick, a dry line across the lawn")
    UC(30, 170, 0, -85, 0, "Brass", "Y", 10)
    US(52, 0, -150, 0, "Brass")
    UT([(0, -150, 0), (0, -215, -20), (0, -240, -90)], 24, "Brass", 8)
    UC(12, 80, 0, -150, 70, "Brass", n=6)
    UC(70, 14, 0, -150, 112, "Red", n=12)
    UC(60, 12, 0, -6, 0, "Brass", "Y", 12)
    piece("SM_Y_OutdoorTap", origin=(0, 0, 0), at=at("SM_Y_OutdoorTap"), note="origin on the wall face, on the pipe's axis")

def watering_can():
    UL([(170, 14), (175, 420), (160, 420), (158, 14)], "BinGreen", 0, 60, 0, 16, closed=True)
    UC(170, 14, 0, 60, 7, "BinGreen", n=16)
    UT([(0, -100, 120), (0, -330, 360), (0, -400, 430)], [34, 26, 24], "BinGreen", 8)
    UC(70, 40, 0, -420, 450, "BinGreen", n=10, r2=30, rot=(42, 0, 0))
    UT([(0, 200, 380), (0, 330, 520), (0, 120, 600), (0, -40, 430)], 16, "BinGreen", 8)
    piece("SM_Y_WateringCan", origin=(0, 0, 0), at=at("SM_Y_WateringCan"), note="open top: a pool or a hiding place inside; the spout is the front")

def lawn_robot():
    UR(700, 900, 200, 0, 0, 170, "CarPaint", 70, 3)
    UR(520, 560, 60, 0, 40, 290, "Plinth", 24, 2)
    UC(46, 30, 0, -230, 285, "Red", n=12)
    for sx in (-1, 1):
        UC(120, 70, sx * 330, 220, 120, "Plinth", "X", 14)
    US(60, 0, -330, 60, "Plinth", nu=8, nv=6)
    UB(-340, 340, -456, -440, 90, 160, "Plinth", 4, 1)
    piece("SM_Y_LawnRobot", origin=(0, 0, 0), at=at("SM_Y_LawnRobot"), note="mower robot; in the game it should drive like the robot vacuum of event 3 (needs code)")

def coffee_cup():
    UL([(140, 12), (165, 500), (156, 500), (132, 12)], "Paper", 0, 250, 165, 16, "Y", closed=True)
    UC(140, 12, 0, 244, 165, "Paper", "Y", 16)
    UL([(152, 150), (162, 380), (150, 380), (140, 150)], "Cardboard", 0, 250, 165, 16, "Y", closed=True)
    UC(172, 22, 330, -420, 11, "Plinth", n=16)
    UP(blob(40, -520, 300, 220, 14, 0.2, 0.9), 0, 5, "Walnut")
    piece("SM_S_CoffeeCup", origin=(0, 0, 0), at=at("SM_S_CoffeeCup"), note="dropped paper cup on its side with a coffee puddle (sticky zone) and its lid")

def build():
    for f in (ant_hill, flower_bed, sprinkler, hose_and_tap, watering_can, lawn_robot, coffee_cup):
        f()