param([string]$Name = "run", [string]$Do = "", [string]$Extra = "", [int]$Timeout = 600, [string]$Res = "1280x720", [switch]$Packaged, [string]$Dist = "DistTutorial", [switch]$NoQuit)
# Off-screen game run from the editor binary (uncooked) or the package (-Packaged). Log -> _tut\<Name>.log
$w = "C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_tutorial"
$rx, $ry = $Res.Split("x")
$log = "$w\_tut\$Name.log"
if (Test-Path $log) { Remove-Item $log -Force }
$args = @()
if ($Packaged) { $exe = "$w\$Dist\Windows\AntKitchen\Binaries\Win64\AntKitchen.exe" } else { $exe = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"; $args += "`"$w\AntKitchen.uproject`""; $args += "-game" }
$args += @("-RenderOffScreen", "-nosound", "-unattended", "-NoSplash", "-ResX=$rx", "-ResY=$ry", "-WINDOWED", "-log", "-abslog=`"$log`"")
if (-not $NoQuit) { $args += "-AntM4Quit" }
if ($Do -ne "") { $args += "-AntDo=`"$Do`"" }
if ($Extra -ne "") { $args += $Extra.Split(" ") }
$p = Start-Process -FilePath $exe -ArgumentList $args -PassThru -WindowStyle Hidden
try { $p.PriorityClass = "BelowNormal" } catch {}
"PID $($p.Id)" | Out-File -Encoding utf8 "$w\_tut\$Name.pid"
if (-not $p.WaitForExit($Timeout * 1000)) { Stop-Process -Id $p.Id -Force; "TIMEOUT" | Out-File -Append -Encoding utf8 "$w\_tut\$Name.pid" }
"EXIT" | Out-File -Append -Encoding utf8 "$w\_tut\$Name.pid"

