@echo off
set W=C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_props
:wait
findstr /C:"B10DONE" "%W%\Saved\art_props\hero_props.log" > nul 2>&1
if errorlevel 1 ( timeout /t 20 /nobreak > nul & goto wait )
copy /Y "%W%\Saved\art_props\hero_props.log" "%W%\Saved\art_props\hero_props_b10.log" > nul
call "%W%\Saved\art_props\hero_props.cmd" SM_K_FruitBowl+SM_K_KnifeBlock+SM_K_DishRack+SM_L_BiscuitPlate+SM_L_SnackBowl+SM_V3_Glasses+SM_L_Coin+SM_V3_KnitNeedle+SM_L_Sock+SM_L_YarnBall+SM_K_CarTrack+SM_L_TrainTrack+SM_V3_Balloon+SM_V3_BalloonWeight+SM_V3_GiftBox+SM_V3_GiftBox_Lid+SM_V3_PickleJar+SM_K_Calendar+SM_K_FridgeNote+SM_V3_KidDrawing+SM_L_Magazines
echo B11DONE >> "%W%\Saved\art_props\hero_props.log"
start "" /b powershell -ExecutionPolicy Bypass -File "%W%\Saved\art_props\unstick.ps1" 1800
set ANT_ART_SETS=hero_props
set ANT_ART_REPORT=hero_props_report.json
set ANT_HERO_NAMES=SM_K_CookieTin+SM_K_TeaBag+SM_V3_DogFoodBag+SM_K_FlourSack+SM_V3_JamJar+SM_V3_JamJar_Lid+SM_V3_PickleJar+SM_V3_ColaBottle+SM_V3_ColaCap+SM_K_GroceryBag+SM_K_CardboardBox+SM_K_ButterTub+SM_K_IceCreamTub+SM_K_Calendar+SM_K_FridgeNote+SM_V3_KidDrawing+SM_K_Newspaper+SM_L_Magazines+SM_K_Towel+SM_K_PaperTowel+SM_K_FlyPaper+SM_K_Dustpan+SM_L_Charger+SM_L_PowerStrip+SM_K_FruitBowl+SM_K_KnifeBlock+SM_K_DishRack+SM_L_BiscuitPlate+SM_L_SnackBowl+SM_V3_Glasses+SM_L_Coin+SM_V3_KnitNeedle+SM_L_Sock+SM_L_YarnBall+SM_K_CarTrack+SM_L_TrainTrack+SM_V3_Balloon+SM_V3_BalloonWeight+SM_V3_GiftBox+SM_V3_GiftBox_Lid
call "%W%\Saved\art_props\ue_py.cmd" "%W%\Tools\import_blender_art.py" import3
echo IMPORT3DONE >> "%W%\Saved\art_props\hero_props.log"

