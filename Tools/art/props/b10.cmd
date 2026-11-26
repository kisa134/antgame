@echo off
set W=C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_props
:wait
findstr /C:"B9DONE" "%W%\Saved\art_props\hero_props.log" > nul 2>&1
if errorlevel 1 ( timeout /t 20 /nobreak > nul & goto wait )
copy /Y "%W%\Saved\art_props\hero_props.log" "%W%\Saved\art_props\hero_props_b9.log" > nul
call "%W%\Saved\art_props\hero_props.cmd" SM_K_Calendar+SM_K_FridgeNote+SM_V3_KidDrawing+SM_K_Newspaper+SM_L_Magazines+SM_K_Towel+SM_K_PaperTowel+SM_K_FlyPaper+SM_K_Dustpan+SM_L_Charger+SM_L_PowerStrip
echo B10DONE >> "%W%\Saved\art_props\hero_props.log"

