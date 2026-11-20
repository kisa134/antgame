# Level v2: rebuild everything that is data (no game, no Unreal). Usage: powershell -File Tools\level\run_level_v2.ps1 [-NoBlender]
#   1. Blender: the new meshes -> SourceArt\Blender\level_v2 (FBX + level_v2_new_pieces.json)
#   2. build_level_v2.py -> docs\level\level_v2.json + level_v2_manifest.json
#   3. audit_level.py on v2 (with and without the hide table) -> level_audit_v2.json, level_audit_v2_nohide.json
#   4. check_level_v2.py -> level_v2_check.json
param([switch]$NoBlender)
$root = Split-Path (Split-Path $PSScriptRoot)
Set-Location $root
if (-not $NoBlender) {
    $id = & powershell -NoProfile -ExecutionPolicy Bypass -File Tools\blender\level_v2_run.ps1 level_v2_pieces.py _build
    Wait-Process -Id $id -Timeout 120 -ErrorAction SilentlyContinue
    Get-Content SourceArt\Blender\level_v2\_build.log | Select-String 'TOTAL|OPEN_EDGE|MISSING|Error|Traceback' | ForEach-Object { $_.Line }
    Get-Content SourceArt\Blender\level_v2\_build.err -ErrorAction SilentlyContinue | Select-Object -Last 5
}
python Tools\level\build_level_v2.py | Select-Object -First 2
python Tools\level\audit_level.py --data level_v2.json | Select-Object -First 1
python Tools\level\audit_level.py --data level_v2.json --no-hide | Select-Object -First 1
python Tools\level\check_level_v2.py