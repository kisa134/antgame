param([string]$List, [string]$Dir)
$ProgressPreference = 'SilentlyContinue'
New-Item -ItemType Directory -Force $Dir | Out-Null
foreach ($line in Get-Content $List) {
  if ($line.Trim() -eq "") { continue }
  $name, $url = $line.Split(" ", 2)
  $out = Join-Path $Dir $name
  if (Test-Path $out) { continue }
  try { Invoke-WebRequest $url -OutFile $out -UseBasicParsing; "OK $name $((Get-Item $out).Length)" } catch { "FAIL $name $($_.Exception.Message)" }
}
