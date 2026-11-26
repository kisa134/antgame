@echo off
rem both targets built by us with -WaitMutex (the PC is shared: UAT's own build has no wait and fails on a busy mutex), then cook/stage/pak
set W=C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_props
echo START %DATE% %TIME% > "%W%\Saved\art_props\package.log"
if not exist "%W%\Content\Movies\Life" mkdir "%W%\Content\Movies\Life"
copy /Y "C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_v14\Content\Movies\Life\*.mp4" "%W%\Content\Movies\Life\" >> "%W%\Saved\art_props\package.log" 2>&1
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AntKitchenEditor Win64 Development "-Project=%W%\AntKitchen.uproject" -WaitMutex -NoHotReloadFromIDE >> "%W%\Saved\art_props\package.log" 2>&1
echo EDITOR_BUILD %ERRORLEVEL% %TIME% >> "%W%\Saved\art_props\package.log"
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AntKitchen Win64 Development "-Project=%W%\AntKitchen.uproject" -WaitMutex -NoHotReloadFromIDE >> "%W%\Saved\art_props\package.log" 2>&1
echo GAME_BUILD %ERRORLEVEL% %TIME% >> "%W%\Saved\art_props\package.log"
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=%W%\AntKitchen.uproject" -noP4 -platform=Win64 -clientconfig=Development -skipbuild -cook -iterate -stage -pak -prereqs -archive "-archivedirectory=%W%\DistProps" -unattended -utf8output -WaitForUATMutex >> "%W%\Saved\art_props\package.log" 2>&1
echo EXIT %ERRORLEVEL% %DATE% %TIME% >> "%W%\Saved\art_props\package.log"
