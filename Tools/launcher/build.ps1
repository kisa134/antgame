# Build AntKitchenLauncher.exe (PyInstaller onefile). Usage: powershell -File build.ps1
param([string]$OutDir = 'dist')
$ErrorActionPreference = 'Stop'
$L = $PSScriptRoot
Set-Location $L
python -m pip install --user --quiet pywebview pyinstaller requests pillow
$ver = (Select-String -Path "$L\src\launcher.py" -Pattern '^VERSION = "([^"]+)"').Matches[0].Groups[1].Value
python -m PyInstaller --noconfirm --clean --onefile --windowed --name AntKitchenLauncher --icon "$L\assets\icon.ico" `
  --add-data "$L\src\web;web" --hidden-import clr --collect-all webview --distpath "$L\$OutDir" --workpath "$L\build" --specpath "$L\build" "$L\src\launcher.py"
if ($LASTEXITCODE -ne 0) { throw 'PyInstaller failed' }
Set-Content -Path "$L\$OutDir\launcher-version.txt" -Value $ver -Encoding ASCII -NoNewline
Write-Host ("OK: {0} ({1:N1} MB), version {2}" -f "$L\$OutDir\AntKitchenLauncher.exe", ((Get-Item "$L\$OutDir\AntKitchenLauncher.exe").Length / 1MB), $ver)