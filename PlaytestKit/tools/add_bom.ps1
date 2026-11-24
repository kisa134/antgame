# Adds a UTF-8 BOM to every C++ / header / Python / md file that contains non-ASCII text (MSVC needs it to read Cyrillic literals as UTF-8).
param([string]$Root = (Resolve-Path "$PSScriptRoot\..\..").Path)
$utf8bom = New-Object System.Text.UTF8Encoding($true)
$dirs = @("$Root\Plugins\PlaytestKit\Source", "$Root\Source\AntKitchen\AntPlaytest.cpp", "$Root\Source\AntKitchen\AntPlaytest.h")
$files = foreach ($d in $dirs) { if (Test-Path $d -PathType Container) { Get-ChildItem $d -Recurse -Include *.cpp, *.h } else { Get-Item $d } }
foreach ($f in $files) {
  $b = [IO.File]::ReadAllBytes($f.FullName)
  $has = ($b.Length -ge 3 -and $b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF)
  $non = $false; foreach ($x in $b) { if ($x -gt 127) { $non = $true; break } }
  if ($non -and -not $has) { $t = [IO.File]::ReadAllText($f.FullName); [IO.File]::WriteAllText($f.FullName, $t, $utf8bom); "BOM added: $($f.Name)" }
}
