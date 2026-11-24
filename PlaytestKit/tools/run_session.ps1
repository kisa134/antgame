# Off-screen test session of the game (editor -game or packaged exe), with a hard timeout. Stops ONLY the process it started.
# Usage: run_session.ps1 -Name s1 -Extra "-AntTutorial -PTKScript=...;" [-Timeout 300] [-Packaged -Dist DistPTK] [-Port 0]
param([string]$Name = "s1", [string]$Extra = "", [int]$Timeout = 300, [switch]$Packaged, [string]$Dist = "DistPTK", [string]$Res = "1280x720", [switch]$Attended)
$w = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
$resx, $resy = $Res.Split("x")
$log = "$w\Saved\ptk_run_$Name.log"
if ($Packaged) { $exe = "$w\$Dist\Windows\AntKitchen\Binaries\Win64\AntKitchen.exe"; $a = @() }
else { $exe = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"; $a = @("`"$w\AntKitchen.uproject`"", "-game") }
$a += @("-RenderOffScreen", "-nosound", "-nosplash", "-ResX=$resx", "-ResY=$resy", "-windowed", "-AbsLog=`"$log`"", "-stdout", "-FullStdOutLogOutput")
if (-not $Attended) { $a += "-unattended" }; $a += $Extra
$p = Start-Process -FilePath $exe -ArgumentList ($a -join " ") -PassThru -WindowStyle Hidden
$t0 = Get-Date
while (-not $p.HasExited -and ((Get-Date) - $t0).TotalSeconds -lt $Timeout) { Start-Sleep -Seconds 2 }
if (-not $p.HasExited) { "TIMEOUT $Name after $Timeout s: stopping pid $($p.Id)"; Stop-Process -Id $p.Id -Force }
else { "EXITED $Name code $($p.ExitCode) after $([int]((Get-Date) - $t0).TotalSeconds) s" }
# the engine leaves its CrashReportClient behind (it locks the plugin DLL): stop only the one that monitors the process we started
Start-Sleep -Seconds 2
Get-CimInstance Win32_Process -Filter "Name='CrashReportClientEditor.exe' OR Name='CrashReportClient.exe'" | Where-Object { $_.CommandLine -match "-MONITOR=$($p.Id)(\D|$)" } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
