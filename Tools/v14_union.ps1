param([string[]]$Files, [switch]$OursOnly)
# v14 integrator: resolve git conflict blocks by keeping both sides (ours then theirs), or ours only. Keeps BOM / line endings.
foreach ($f in $Files) {
	$p = (Resolve-Path $f).Path
	$bytes = [IO.File]::ReadAllBytes($p)
	$bom = ($bytes.Length -ge 3 -and $bytes[0] -eq 239 -and $bytes[1] -eq 187 -and $bytes[2] -eq 191)
	$t = [IO.File]::ReadAllText($p, [Text.Encoding]::UTF8)
	$nl = if ($t.Contains("`r`n")) { "`r`n" } else { "`n" }
	$lines = $t -split "`r?`n"
	$out = New-Object System.Collections.Generic.List[string]
	$state = 0
	foreach ($x in $lines) {
		if ($x -match '^<<<<<<< ') { $state = 1; continue }
		if ($state -ge 1 -and $x -match '^\|\|\|\|\|\|\| ') { $state = 3; continue }
		if ($state -ge 1 -and $x -eq '=======') { $state = 2; continue }
		if ($state -ge 1 -and $x -match '^>>>>>>> ') { $state = 0; continue }
		if ($state -eq 3) { continue }
		if ($state -eq 2 -and $OursOnly) { continue }
		$out.Add($x)
	}
	[IO.File]::WriteAllText($p, ($out -join $nl), (New-Object Text.UTF8Encoding($bom)))
	"$f resolved (bom=$bom)"
}
