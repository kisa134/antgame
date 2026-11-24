param([string]$Name = "demo1", [int]$Timeout = 900)
# The demo end to end, off screen: the training's graduation (beat 15) -> straight into the first real raid -> 3 short raids
# (-AntRaidLen=45, the host starts each one by itself: -AntDemoAuto) -> the end screen (a picture) -> quit.
$w = (Resolve-Path "$PSScriptRoot\..\..").Path
& powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\pack_demo.ps1" | Out-Null
$exe = "$w\DistDemo\Windows\AntKitchen\Binaries\Win64\AntKitchen.exe"
$log = "$w\_tut\$Name.log"
Remove-Item $log -ErrorAction SilentlyContinue
$a = "-RenderOffScreen -nosound -unattended -NoSplash -ResX=1280 -ResY=720 -WINDOWED -log -abslog=`"$log`" -AntNoMenu -AntTutorial=15 -AntTutAuto -AntDemoAuto -AntRaidLen=45 -AntNoSpider"
$p = Start-Process -FilePath $exe -ArgumentList $a -PassThru -WindowStyle Hidden
try { $p.PriorityClass = "BelowNormal" } catch {}
if (-not $p.WaitForExit($Timeout * 1000)) { Stop-Process -Id $p.Id -Force; "TIMEOUT" | Out-File -Encoding utf8 "$w\_tut\$Name.done" } else { "EXIT" | Out-File -Encoding utf8 "$w\_tut\$Name.done" }
