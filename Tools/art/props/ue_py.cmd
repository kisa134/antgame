@echo off
rem ue_py.cmd <script> <logname>  (env vars pass through)
set W=C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_props
copy /Y "%~1" "%TEMP%\props_%~nx1" > nul
echo START %DATE% %TIME% > "%W%\Saved\art_props\%2.log"
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%W%\AntKitchen.uproject" -run=pythonscript -script="%TEMP%\props_%~nx1" -unattended -nosplash -nullrhi -stdout -FullStdOutLogOutput < nul >> "%W%\Saved\art_props\%2.log" 2>&1
echo EXIT %ERRORLEVEL% %DATE% %TIME% >> "%W%\Saved\art_props\%2.log"
