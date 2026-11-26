@echo off
rem hero_props.cmd <names joined by +|all> [nobake]
set W=C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_props
echo START %DATE% %TIME% > "%W%\Saved\art_props\hero_props.log"
python "%W%\Tools\art\make_labels.py" >> "%W%\Saved\art_props\hero_props.log" 2>&1
"C:\Program Files\Blender Foundation\Blender 5.3\blender.exe" --background --factory-startup --python "%W%\Tools\blender\hero_props.py" -- "%W%\SourceArt\Blender\hero_props" "%W%\Saved\art_review\props" %1 %2 >> "%W%\Saved\art_props\hero_props.log" 2>&1
echo EXIT %ERRORLEVEL% %DATE% %TIME% >> "%W%\Saved\art_props\hero_props.log"
