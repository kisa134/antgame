# PlaytestKit: collect local playtest data into one zip (zero-backend mode).
# Usage:  collect_playtest.ps1 [-Game <folder with the game exe or Saved\>] [-Out <zip path>] [-NoShots] [-Since <yyyy-MM-dd>]
# Looks for Saved\Playtest under -Game, next to this script, and in %LOCALAPPDATA%\<*>\Saved\Playtest (packaged builds).
param([string]$Game = "", [string]$Out = "", [switch]$NoShots, [string]$Since = "")
$ErrorActionPreference = "Stop"
$cands = @()
if ($Game) { $cands += (Join-Path $Game "Saved\Playtest"); $cands += (Join-Path $Game "*\Saved\Playtest") }
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$cands += (Join-Path $here "Saved\Playtest"); $cands += (Join-Path $here "..\Saved\Playtest"); $cands += (Join-Path $here "*\Saved\Playtest"); $cands += (Join-Path $here "*\*\Saved\Playtest")
$cands += (Join-Path $env:LOCALAPPDATA "*\Saved\Playtest")
$roots = @()
foreach ($c in $cands) { $roots += @(Get-Item -Path $c -ErrorAction SilentlyContinue) }
$roots = $roots | Sort-Object FullName -Unique
if (-not $roots) { Write-Host "No Saved\Playtest folder found. Pass -Game <path to the game folder>."; exit 1 }
$tmp = Join-Path $env:TEMP ("ptk_collect_" + [guid]::NewGuid().ToString("N").Substring(0, 8))
New-Item -ItemType Directory -Path $tmp | Out-Null
$n = 0; $mb = 0
foreach ($r in $roots) {
  Write-Host "found $($r.FullName)"
  foreach ($s in Get-ChildItem $r.FullName -Directory) {
    if (-not (Test-Path (Join-Path $s.FullName "events.jsonl"))) { continue }
    if ($Since -and $s.LastWriteTime -lt [datetime]$Since) { continue }
    $dst = Join-Path $tmp $s.Name
    New-Item -ItemType Directory -Path $dst | Out-Null
    Get-ChildItem $s.FullName -File | Copy-Item -Destination $dst
    if (-not $NoShots -and (Test-Path (Join-Path $s.FullName "shots"))) { Copy-Item (Join-Path $s.FullName "shots") $dst -Recurse }
    $n++
  }
}
if ($n -eq 0) { Write-Host "No sessions found."; Remove-Item $tmp -Recurse -Force; exit 1 }
if (-not $Out) { $Out = Join-Path ([Environment]::GetFolderPath("Desktop")) ("playtest_" + (Get-Date -Format "yyyyMMdd_HHmm") + ".zip") }
if (Test-Path $Out) { Remove-Item $Out -Force }
Compress-Archive -Path (Join-Path $tmp "*") -DestinationPath $Out -CompressionLevel Optimal
Remove-Item $tmp -Recurse -Force
$mb = [math]::Round((Get-Item $Out).Length / 1MB, 1)
Write-Host "OK: $n sessions -> $Out ($mb MB). The zip contains only anonymous gameplay events and screenshots you took with F7."
