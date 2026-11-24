<#
 Publish a playtest build as a GitHub prerelease with everything the Ant Kitchen Launcher needs:
   AntKitchen-<tag-suffix>.zip   game (top folder Windows\, Saved excluded)
   <zip>.sha256                  "<hex>  <filename>"
   version.json                  tag, date, size, sha256, notes
   AntKitchenLauncher.exe + launcher-version.txt   (the launcher itself, for self-update / landing page)
 Usage:
   .\release.ps1 -Tag playtest-2026-11-24-v14.1 -Dist C:\path\DistV14 -Notes C:\path\relnotes.md [-Title "Playtest v14.1: ..."]
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
  [switch]$SkipZip
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force $Out | Out-Null
$zipName = "AntKitchen-$Tag.zip"
if ($Tag -notlike 'playtest-*') { $zipName = "AntKitchen-playtest-$Tag.zip" }
$zip = Join-Path $Out $zipName
if (-not $SkipZip -or -not (Test-Path $zip)) {
  Remove-Item $zip -ErrorAction SilentlyContinue
  tar -a -c -f $zip --exclude=Saved -C $Dist Windows
  if ($LASTEXITCODE -ne 0) { throw 'tar failed' }
}
$size = (Get-Item $zip).Length
$sha = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
$shaFile = "$zip.sha256"
[IO.File]::WriteAllText($shaFile, "$sha  $zipName`n", (New-Object Text.UTF8Encoding($false)))
$notesText = if (Test-Path $Notes) { [IO.File]::ReadAllText($Notes) } else { '' }
$vj = [ordered]@{ tag = $Tag; date = (Get-Date).ToUniversalTime().ToString('o'); size = $size; sha256 = $sha; asset = $zipName; notes = $notesText }
$vjFile = Join-Path $Out 'version.json'
[IO.File]::WriteAllText($vjFile, ($vj | ConvertTo-Json -Depth 4), (New-Object Text.UTF8Encoding($false)))
$files = @($zip, $shaFile, $vjFile)
foreach ($n in 'AntKitchenLauncher.exe', 'launcher-version.txt') {
  $p = Join-Path $Launcher $n
  if (Test-Path $p) { $files += (Resolve-Path $p).Path } else { Write-Warning "launcher asset missing: $p (run Tools\launcher\build.ps1)" }
}
gh release view $Tag -R $Repo 2>$null | Out-Null
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
"RELEASE OK: https://github.com/$Repo/releases/tag/$Tag  ($([int]($size/1MB)) MB, sha256 $sha)"