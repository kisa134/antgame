$w = (Resolve-Path "$PSScriptRoot\..\..").Path
# DistDemo = the packaged DistTut2 (same binaries and paks) + the demo flag; its own Saved folder comes with the copy being separate.
if (Test-Path "$w\DistDemo") { Remove-Item "$w\DistDemo" -Recurse -Force }
robocopy "$w\DistTut2" "$w\DistDemo" /E /XD Saved /NFL /NDL /NJH /NJS | Out-Null
"Ant Kitchen Steam demo (docs/DEMO.md)" | Out-File -Encoding utf8 "$w\DistDemo\Windows\AntKitchen\AntDemo.flag"
Get-ChildItem "$w\DistDemo\Windows" | select Name
"DEMO PACKED"

