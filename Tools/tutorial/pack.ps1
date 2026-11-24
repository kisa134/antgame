param([switch]$SkipCook, [string]$Out = "DistTutorial2")
$w = "C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_tutorial"
$log = "$w\_tut\pack_$Out.log"
# Build both targets ourselves (another worktree's editor keeps a Live Coding session open: UAT's own build would refuse).
$bat = "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat"
& $bat AntKitchenEditor Win64 Development "-Project=$w\AntKitchen.uproject" -WaitMutex -NoHotReloadFromIDE 2>&1 | Out-File -Encoding utf8 $log
& $bat AntKitchen Win64 Development "-Project=$w\AntKitchen.uproject" -WaitMutex -NoHotReloadFromIDE 2>&1 | Out-File -Append -Encoding utf8 $log
$uat = "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat"
$a = @("BuildCookRun", "-project=$w\AntKitchen.uproject", "-noP4", "-platform=Win64", "-clientconfig=Development", "-stage", "-pak", "-prereqs", "-archive", "-archivedirectory=$w\$Out", "-unattended", "-utf8output", "-WaitForUATMutex")
if ($SkipCook) { $a += "-skipcook" } else { $a += "-cook" }
& $uat @a 2>&1 | Out-File -Append -Encoding utf8 $log
"EXIT $LASTEXITCODE" | Out-File -Append -Encoding utf8 $log


