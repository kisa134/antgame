param([string]$Name = "coop", [int]$Beat = 0, [int]$Timeout = 420, [switch]$Packaged)
# Two players on this machine over 127.0.0.1:7807 (no Steam): the host runs the training (scripted), the client joins and takes
# pictures of its own HUD (-AntLocalDo). Logs: _tut\<Name>_h.log, _tut\<Name>_c.log
$w = (Resolve-Path "$PSScriptRoot\..\..").Path
if ($Packaged) { $exe = "$w\DistTut2\Windows\AntKitchen\Binaries\Win64\AntKitchen.exe"; $proj = "" }
else { $exe = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"; $proj = "`"$w\AntKitchen.uproject`" -game" }
$base = "$proj -RenderOffScreen -nosound -unattended -NoSplash -ResX=960 -ResY=540 -WINDOWED -log -NOSTEAM -AntNoMenu"
$hl = "$w\_tut\${Name}_h.log"; $cl = "$w\_tut\${Name}_c.log"
Remove-Item $hl, $cl -ErrorAction SilentlyContinue
$h = Start-Process -FilePath $exe -ArgumentList "$base -abslog=`"$hl`" -AntTutorial=$Beat -AntTutAuto -AntTutStay -AntSeed=7 -AntNoSpider -AntListenPort=7807 -port=7807" -PassThru -WindowStyle Hidden
try { $h.PriorityClass = "BelowNormal" } catch {}
Start-Sleep 35
$c = Start-Process -FilePath $exe -ArgumentList "$base -abslog=`"$cl`" -AntJoin=127.0.0.1:7807 -AntLocalDo=`"30:shot ${Name}_c030;60:shot ${Name}_c060;90:shot ${Name}_c090;120:shot ${Name}_c120;150:shot ${Name}_c150;180:shot ${Name}_c180`"" -PassThru -WindowStyle Hidden
try { $c.PriorityClass = "BelowNormal" } catch {}
if (-not $h.WaitForExit($Timeout * 1000)) { Stop-Process -Id $h.Id -Force }
if (-not $c.HasExited) { Stop-Process -Id $c.Id -Force }
"DONE" | Out-File -Encoding utf8 "$w\_tut\$Name.done"

