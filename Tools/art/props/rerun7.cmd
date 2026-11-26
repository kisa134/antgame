@echo off
rem after rerun6: the remote (d-pad ring) and the bowl (leftovers) again, then the import of batches 5-8 (no glass/break MI step: those come from v14)
set W=C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_props
:wait
findstr /C:"B8DONE" "%W%\Saved\art_props\hero_props.log" > nul 2>&1
if errorlevel 1 ( timeout /t 20 /nobreak > nul & goto wait )
copy /Y "%W%\Saved\art_props\hero_props.log" "%W%\Saved\art_props\hero_props_b8.log" > nul
call "%W%\Saved\art_props\hero_props.cmd" SM_L_Remote+SM_K_Bowl+SM_K_Pot+SM_K_Pot_Lid+SM_K_Ladle+SM_K_Spatula+SM_K_Whisk+SM_K_SugarBowl+SM_K_MilkCarton
echo B9DONE >> "%W%\Saved\art_props\hero_props.log"
start "" /b powershell -ExecutionPolicy Bypass -File "%W%\Saved\art_props\unstick.ps1" 3600
set ANT_ART_SETS=hero_props
set ANT_ART_REPORT=hero_props_report.json
set ANT_HERO_NAMES=SM_L_Remote+SM_K_Bowl+SM_K_PetBowl+SM_K_Pot+SM_K_Pot_Lid+SM_K_Kettle+SM_K_Shaker+SM_K_JarSugar+SM_K_Ladle+SM_K_Spatula+SM_K_Whisk+SM_K_RollingPin+SM_K_CuttingBoard+SM_K_SugarBowl+SM_K_IceTray+SM_K_EggTray+SM_K_TennisBall+SM_L_ToyArch+SM_L_ToyCar+SM_L_ToyTrain+SM_L_ToyMouse+SM_L_SodaCan+SM_K_MilkCarton+SM_V3_CerealBox+SM_L_GameConsole+SM_L_Controller
call "%W%\Saved\art_props\ue_py.cmd" "%W%\Tools\import_blender_art.py" import
echo IMPORTDONE >> "%W%\Saved\art_props\hero_props.log"



