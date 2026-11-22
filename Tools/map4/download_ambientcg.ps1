param([string]$Ids, [string]$Dir)
$ProgressPreference = 'SilentlyContinue'
foreach ($id in $Ids.Split(",")) {
  $d = Join-Path $Dir ("acg_" + $id)
  New-Item -ItemType Directory -Force $d | Out-Null
  $zip = Join-Path $d "src.zip"
  try { Invoke-WebRequest "https://ambientcg.com/get?file=${id}_2K-JPG.zip" -OutFile $zip -UseBasicParsing; Expand-Archive $zip -DestinationPath $d -Force; Remove-Item $zip; "OK $id " + ((Get-ChildItem $d).Name -join " ") } catch { "FAIL $id $($_.Exception.Message)" }
}
