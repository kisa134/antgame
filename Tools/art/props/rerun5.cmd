@echo off
rem after rerun2: batches 5-7 (dishes and pans, kitchen tools, toys), baked
set W=C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_props
:wait
findstr /C:"ALLDONE" "%W%\Saved\art_props\hero_props.log" > nul 2>&1
if errorlevel 1 ( timeout /t 20 /nobreak > nul & goto wait )
copy /Y "%W%\Saved\art_props\hero_props.log" "%W%\Saved\art_props\hero_props_rerun2.log" > nul
call "%W%\Saved\art_props\hero_props.cmd" SM_K_Bowl+SM_K_PetBowl+SM_K_Pot+SM_K_Pot_Lid+SM_K_Kettle+SM_K_Shaker+SM_K_JarSugar+SM_K_Ladle+SM_K_Spatula+SM_K_Whisk+SM_K_RollingPin+SM_K_CuttingBoard+SM_K_SugarBowl+SM_K_IceTray+SM_K_EggTray+SM_K_TennisBall+SM_L_ToyArch+SM_L_ToyCar+SM_L_ToyTrain+SM_L_ToyMouse
echo B567DONE >> "%W%\Saved\art_props\hero_props.log"
