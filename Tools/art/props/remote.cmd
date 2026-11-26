@echo off
rem the remote again (its keys: the screw merge fixed), baked, beside rerun2 (own log)
set W=C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_props
echo START %DATE% %TIME% > "%W%\Saved\art_props\remote.log"
"C:\Program Files\Blender Foundation\Blender 5.3\blender.exe" --background --factory-startup --python "%W%\Tools\blender\hero_props.py" -- "%W%\SourceArt\Blender\hero_props" "%W%\Saved\art_review\props" SM_L_Remote >> "%W%\Saved\art_props\remote.log" 2>&1
echo EXIT %ERRORLEVEL% %DATE% %TIME% >> "%W%\Saved\art_props\remote.log"
