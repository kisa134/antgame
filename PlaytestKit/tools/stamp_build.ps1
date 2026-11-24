# PlaytestKit: stamp the current git sha + a build label into Config\DefaultPlaytestKit.ini (run before packaging a playtest build).
# Usage: stamp_build.ps1 [-ProjectDir <dir>] [-Version 0.9.1]
param([string]$ProjectDir = "", [string]$Version = "")
$ErrorActionPreference = "Stop"
if (-not $ProjectDir) { $ProjectDir = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)) }
$ini = Join-Path $ProjectDir "Config\DefaultPlaytestKit.ini"
if (-not (Test-Path $ini)) { throw "no $ini" }
$sha = (git -C $ProjectDir rev-parse --short HEAD).Trim()
if (-not $Version) { $Version = (Get-Date -Format "yyyy.MM.dd") }
$t = [IO.File]::ReadAllText($ini)
function SetKey($text, $k, $v) { if ($text -match "(?m)^$k=.*$") { return [regex]::Replace($text, "(?m)^$k=.*$", "$k=$v") } else { return $text.TrimEnd() + "`r`n$k=$v`r`n" } }
$t = SetKey $t "BuildVersion" $Version
$t = SetKey $t "GitSha" $sha
[IO.File]::WriteAllText($ini, $t, (New-Object Text.UTF8Encoding($false)))
Write-Host "stamped BuildVersion=$Version GitSha=$sha in $ini"
