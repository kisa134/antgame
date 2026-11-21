# Runs a level_v3 Blender script headless, detached, at below-normal priority (the game may be running on this PC).
#   powershell -File Tools\level\level_v3_blender.ps1 <script.py in Tools\blender> <log name> [args after --]
# Writes SourceArt\Blender\level_v3\<log>.log / .err and prints the process id.
param([string]$Script, [string]$Log, [Parameter(ValueFromRemainingArguments=$true)][string[]]$Rest)
$root = Split-Path (Split-Path $PSScriptRoot)
$out = Join-Path $root 'SourceArt\Blender\level_v3'
New-Item -ItemType Directory -Force $out | Out-Null
$blender = 'C:\Program Files\Blender Foundation\Blender 5.3\blender.exe'
$a = @('--background', '--factory-startup', '--python', ('"' + (Join-Path $root ('Tools\blender\' + $Script)) + '"'), '--', ('"' + $out + '"'))
if ($Rest) { $a += $Rest }
$p = Start-Process -FilePath $blender -ArgumentList $a -RedirectStandardOutput (Join-Path $out ($Log + '.log')) -RedirectStandardError (Join-Path $out ($Log + '.err')) -PassThru -WindowStyle Hidden
try { $p.PriorityClass = 'BelowNormal' } catch {}
Write-Output $p.Id
