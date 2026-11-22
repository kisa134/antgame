param([string]$Ids, [string]$Dir, [string]$Res = "2k")
$ProgressPreference = 'SilentlyContinue'
foreach ($id in $Ids.Split(",")) {
  $d = Join-Path $Dir $id
  New-Item -ItemType Directory -Force $d | Out-Null
  try { $f = Invoke-RestMethod "https://api.polyhaven.com/files/$id" } catch { "FAIL files $id"; continue }
  foreach ($map in @("Diffuse", "nor_gl", "Rough", "AO", "arm", "Displacement")) {
    $node = $f.$map
    if (-not $node) { continue }
    $r = $node.$Res
    if (-not $r) { continue }
    $j = $r.jpg
    if (-not $j) { $j = $r.png }
    if (-not $j) { continue }
    $ext = [System.IO.Path]::GetExtension($j.url)
    $out = Join-Path $d "$map$ext"
    if (Test-Path $out) { continue }
    try { Invoke-WebRequest $j.url -OutFile $out -UseBasicParsing } catch { "FAIL $id $map" }
  }
  "OK $id " + ((Get-ChildItem $d).Name -join " ")
}
