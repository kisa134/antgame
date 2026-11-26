@echo off
set W=C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_props
:wait
findstr /C:"IMPORT3DONE" "%W%\Saved\art_props\hero_props.log" > nul 2>&1
if errorlevel 1 ( timeout /t 20 /nobreak > nul & goto wait )
copy /Y "%W%\Saved\art_props\hero_props.log" "%W%\Saved\art_props\hero_props_b11.log" > nul
call "%W%\Saved\art_props\hero_props.cmd" SM_K_KnifeBlock
start "" /b powershell -ExecutionPolicy Bypass -File "%W%\Saved\art_props\unstick.ps1" 900
set ANT_ART_SETS=hero_props
set ANT_ART_REPORT=hero_props_report.json
set ANT_HERO_NAMES=SM_K_KnifeBlock
call "%W%\Saved\art_props\ue_py.cmd" "%W%\Tools\import_blender_art.py" import4
echo IMPORT4DONE >> "%W%\Saved\art_props\hero_props.log"

