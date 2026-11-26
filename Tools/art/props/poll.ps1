param([int]$secs = 70)
Start-Sleep $secs
$l = "C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_props\Saved\art_props\hero_props.log"
$d = @(Select-String $l -Pattern "HERO_PROPS .* done" | % { $_.Line.Split(' ')[1] })
$m = @(Select-String $l -Pattern "ALLDONE|B567DONE|B8DONE|Traceback|EXIT [1-9]" | % { $_.Line })
"done $($d.Count): $($d[-1]) | $($m -join ' ; ')"
