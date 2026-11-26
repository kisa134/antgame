@echo off
rem after rerun7's import: the pad again (layout), then its import
set W=C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_props
:wait
findstr /C:"IMPORTDONE" "%W%\Saved\art_props\hero_props.log" > nul 2>&1
if errorlevel 1 ( timeout /t 20 /nobreak > nul & goto wait )
copy /Y "%W%\Saved\art_props\hero_props.log" "%W%\Saved\art_props\hero_props_b9.log" > nul
call "%W%\Saved\art_props\hero_props.cmd" SM_L_Controller
start "" /b powershell -ExecutionPolicy Bypass -File "%W%\Saved\art_props\unstick.ps1" 1800
set ANT_ART_SETS=hero_props
set ANT_ART_REPORT=hero_props_report.json
set ANT_HERO_NAMES=SM_L_Controller
call "%W%\Saved\art_props\ue_py.cmd" "%W%\Tools\import_blender_art.py" import2
echo IMPORT2DONE >> "%W%\Saved\art_props\hero_props.log"
