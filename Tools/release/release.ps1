<#
 Publish a playtest build as a GitHub prerelease with everything the Ant Kitchen Launcher needs:
   AntKitchen-<tag-suffix>.zip   game (top folder Windows\, Saved and *.pdb excluded)
   <zip>.sha256                  "<hex>  <filename>"
   version.json                  tag, date, size, sha256, notes (+ parts, see below)
   AntKitchenLauncher.exe + launcher-version.txt   (the launcher itself, for self-update / landing page)
 GitHub refuses release assets of 2 GiB and more. A zip over -MaxAsset bytes (default 1.9 GB) is therefore uploaded
 in pieces AntKitchen-<tag>.zip.001, .002 ... instead of the zip itself; version.json then lists them in "parts"
 (name, size, sha256 of each) next to the size and sha256 of the whole zip. Launcher 1.0.3+ downloads the pieces,
 checks them and joins them; a zip under the limit is published as one file, as before.
 Usage:
   .\release.ps1 -Tag playtest-2026-11-24-v14.1 -Dist C:\path\DistV14 -Notes C:\path\relnotes.md [-Title "Playtest v14.1: ..."]
   -SkipZip   reuse the zip already in -Out      -PackOnly   build zip / parts / version.json in -Out and stop (no upload)
 The zip name is AntKitchen-<Tag minus 'playtest-' prefix>.zip, e.g. AntKitchen-playtest-2026-11-24-v14.1.zip (the launcher matches AntKitchen-playtest-*.zip).
 If the release already exists, assets are uploaded with --clobber (nothing is deleted or recreated).
#>
param(
  [Parameter(Mandatory)][string]$Tag,
  [Parameter(Mandatory)][string]$Dist,
  [Parameter(Mandatory)][string]$Notes,
  [string]$Title = '',
  [string]$Repo = 'kisa134/antgame',
  [string]$Target = '',
  [string]$Launcher = (Join-Path $PSScriptRoot '..\launcher\dist'),
  [string]$Out = (Join-Path $env:TEMP 'ak_release'),
  [long]$MaxAsset = 1900000000,
  [switch]$SkipZip,
  [switch]$PackOnly
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force $Out | Out-Null
$zipName = "AntKitchen-$Tag.zip"
if ($Tag -notlike 'playtest-*') { $zipName = "AntKitchen-playtest-$Tag.zip" }
$zip = Join-Path $Out $zipName
if (-not $SkipZip -or -not (Test-Path $zip)) {
  Remove-Item $zip -ErrorAction SilentlyContinue
  tar -a -c -f $zip --exclude=Saved '--exclude=*.pdb' -C $Dist Windows
  if ($LASTEXITCODE -ne 0) { throw 'tar failed' }
}
$size = (Get-Item $zip).Length
$sha = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
$shaFile = "$zip.sha256"
[IO.File]::WriteAllText($shaFile, "$sha  $zipName`n", (New-Object Text.UTF8Encoding($false)))
# split a zip that is over the per-asset limit
Get-ChildItem $Out -File | Where-Object { $_.Name -match ('^' + [regex]::Escape($zipName) + '\.\d{3}$') } | Remove-Item
$parts = @()
if ($size -gt $MaxAsset) {
  $buf = New-Object byte[] (16MB)
  $in = [IO.File]::OpenRead($zip)
  try {
    $i = 0
    while ($in.Position -lt $size) {
      $i++
      $pn = '{0}.{1:d3}' -f $zipName, $i
      [long]$plen = [Math]::Min($MaxAsset, $size - $in.Position)
      [long]$left = $plen
      $h = [Security.Cryptography.SHA256]::Create()
      $o = [IO.File]::Create((Join-Path $Out $pn))
      try {
        while ($left -gt 0) {
          $n = $in.Read($buf, 0, [int][Math]::Min([long]$buf.Length, $left))
          if ($n -le 0) { throw "short read in $pn" }
          $o.Write($buf, 0, $n)
          [void]$h.TransformBlock($buf, 0, $n, $null, 0)
          $left -= $n
        }
      } finally { $o.Close() }
      [void]$h.TransformFinalBlock($buf, 0, 0)
      $parts += [ordered]@{ name = $pn; size = $plen; sha256 = (-join ($h.Hash | ForEach-Object { $_.ToString('x2') })) }
    }
  } finally { $in.Close() }
}
$notesText = if (Test-Path $Notes) { [IO.File]::ReadAllText($Notes) } else { '' }
$vj = [ordered]@{ tag = $Tag; date = (Get-Date).ToUniversalTime().ToString('o'); size = $size; sha256 = $sha; asset = $zipName; notes = $notesText }
if ($parts.Count) { $vj.parts = $parts }
$vjFile = Join-Path $Out 'version.json'
[IO.File]::WriteAllText($vjFile, ($vj | ConvertTo-Json -Depth 4), (New-Object Text.UTF8Encoding($false)))
$files = @()
if ($parts.Count) { foreach ($p in $parts) { $files += (Join-Path $Out $p.name) } } else { $files += $zip }
$files += @($shaFile, $vjFile)
foreach ($n in 'AntKitchenLauncher.exe', 'launcher-version.txt') {
  $p = Join-Path $Launcher $n
  if (Test-Path $p) { $files += (Resolve-Path $p).Path } else { Write-Warning "launcher asset missing: $p (run Tools\launcher\build.ps1)" }
}
if ($PackOnly) {
  "PACK OK: $zipName $size bytes, sha256 $sha, parts $($parts.Count)"
  $files | ForEach-Object { '  {0}  {1}' -f (Get-Item $_).Length, $_ }
  exit 0
}
# gh writes to stderr (e.g. 'release not found'): with Stop that would end the script, exit codes are checked instead
$ErrorActionPreference = 'Continue'
gh release view $Tag -R $Repo 2>&1 | Out-Null
if ($LASTEXITCODE -eq 0) {
  gh release upload $Tag @files -R $Repo --clobber
} else {
  if (-not $Title) { $Title = $Tag }
  $args2 = @('release', 'create', $Tag) + $files + @('-R', $Repo, '--prerelease', '--title', $Title, '-F', $Notes)
  if ($Target) { $args2 += @('--target', $Target) }
  gh @args2
}
if ($LASTEXITCODE -ne 0) { throw 'gh failed' }
# keep the stable launcher link (release 'launcher-latest') in sync with the newest exe
$le = Join-Path $Launcher 'AntKitchenLauncher.exe'; $lv = Join-Path $Launcher 'launcher-version.txt'
if ((Test-Path $le) -and (Test-Path $lv)) { gh release upload launcher-latest $le $lv -R $Repo --clobber }
"RELEASE OK: https://github.com/$Repo/releases/tag/$Tag  ($([int]($size/1MB)) MB, sha256 $sha, parts $($parts.Count))"
