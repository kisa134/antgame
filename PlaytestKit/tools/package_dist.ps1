# Package a Development build to <project>\DistPTK (cook + stage + pak). Log: <project>\pack_ptk.log
# Usage: package_dist.ps1 [-SkipCook] [-Out DistPTK]   (re-run with -SkipCook if the cook crashes on exit after success)
param([switch]$SkipCook, [string]$Out = "DistPTK")
$w = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
$u = "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat"
$a = @("BuildCookRun", "-project=`"$w\AntKitchen.uproject`"", "-noP4", "-platform=Win64", "-clientconfig=Development", "-build")
if ($SkipCook) { $a += "-skipcook" } else { $a += "-cook" }
$a += @("-stage", "-pak", "-prereqs", "-archive", "-archivedirectory=`"$w\$Out`"", "-unattended", "-utf8output", "-WaitForUATMutex")
& $u @a *> "$w\pack_ptk.log"
"EXIT=$LASTEXITCODE" | Add-Content "$w\pack_ptk.log"
