@echo off
rem fix14: the sink fills from the tap and slops over; the kitchen chairs. %1 = shot prefix, %2 = extra flag in quotes ("-AntHeroArt=0", "-AntHeroPalette=new"), %3 = the packaged folder (default DistFix)
set D=%~3
if "%D%"=="" set D=DistFix
set W=%~dp0..\..
set P=%1
echo START %DATE% %TIME% > "%W%\Saved\play_%P%.log"
"%W%\%D%\Windows\AntKitchen.exe" -windowed -ResX=1600 -ResY=900 -AntNoMenu -AntFastVote -NOSTEAM -nosound -log -AntFluidLog %~2 -AntDo="12:level 4;20:raid ready;21:raid go;25:fluid loot 10 -3500 3900 1250;30:fluid tap on 5;31:fluid debug on;60:fluid debug on;100:fluid debug on;122:rx quit" -AntFilmDo="26:key F8;27:key F5;28:key F2;29:key F6;30:at -3500 2700 2450 90 -38 60;34:shot %P%_sink_0s;49:at -3500 2700 2450 90 -38 60;50:shot %P%_sink_20s;74:at -3500 1300 2700 90 -32 70;75:shot %P%_sink_45s;94:at -3500 900 1100 90 -12 75;95:shot %P%_sink_65s;98:at -3500 1300 2700 90 -32 70;99:shot %P%_sink_wide;100:at 1300 -4400 1300 128 -16 60;104:shot %P%_chair1;105:at 5600 -2300 2600 141 -21 60;109:shot %P%_highchair;110:at 2700 -200 1200 136 -13 60;114:shot %P%_chair2;115:at 4800 -4200 2000 132 -16 80;119:shot %P%_chairs_wide" >> "%W%\Saved\play_%P%.log" 2>&1
echo EXIT %ERRORLEVEL% %DATE% %TIME% >> "%W%\Saved\play_%P%.log"
