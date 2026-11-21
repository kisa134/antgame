# Level v3: all preview renders, one Blender at a time, below-normal priority (the game may be running on this PC).
#   powershell -File Tools\level\level_v3_render_all.ps1      (detached: Start-Process powershell -ArgumentList '-File', this)
# Output: SourceArt\Blender\level_v3\preview_<state>_<shot>_<time>.jpg, logs _r_<n>.log
$root = Split-Path (Split-Path $PSScriptRoot)
$jobs = @(
  @('showcase', 'day', 'top', 'iso_kitchen', 'iso_living', 'ant_shelf', 'ant_screws', 'ant_fridge', 'ant_window', 'ant_worktop'),
  @('showcase', 'evening', 'iso_kitchen', 'iso_living', 'ant_table', 'ant_sofa', 'ant_toybox', 'ant_gifts', 'ant_knitting', 'ant_halldoor'),
  @('chaos', 'evening', 'iso_kitchen', 'ant_chaos')
)
$n = 0
foreach ($j in $jobs) {
  $n++
  $id = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'level_v3_blender.ps1') level_v3_preview.py ('_r_' + $n) @j
  Wait-Process -Id $id -Timeout 900 -ErrorAction SilentlyContinue
}
'RENDER_ALL_DONE' | Out-File (Join-Path $root 'SourceArt\Blender\level_v3\_render_all.done')
