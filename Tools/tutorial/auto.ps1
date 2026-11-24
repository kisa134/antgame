param([string]$Name = "auto1", [string]$Extra = "", [int]$Timeout = 1800, [switch]$Packaged, [string]$Dist = "DistTutorial", [string]$Res = "1280x720")
$w = "C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_tutorial"
$shots = if ($Packaged) { "$w\$Dist\Windows\AntKitchen\Saved\Screenshots" } else { "$w\Saved\Screenshots" }
if (Test-Path $shots) { Remove-Item "$shots\*" -Recurse -Force -ErrorAction SilentlyContinue }
$x = "-AntNoMenu -AntTutorial -AntTutAuto -AntTutStay -AntTutQuit -AntSeed=7 -AntNoSpider " + $Extra
$a = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "$w\_tut\run.ps1", "-Name", $Name, "-Timeout", "$Timeout", "-Extra", "`"$x`"", "-Res", $Res, "-NoQuit")
if ($Packaged) { $a += @("-Packaged", "-Dist", $Dist) }
& powershell @a
"DONE $Name" | Out-File -Encoding utf8 "$w\_tut\$Name.done"
