# Builds the game (Development, Win64) from this worktree and writes the log next to it. Run detached: Start-Process powershell -ArgumentList '-NoProfile','-File',<this> -WindowStyle Hidden
param([string]$Root = (Resolve-Path "$PSScriptRoot\..\..").Path, [string]$Log = "$env:TEMP\ptk_build.log")
$uproject = Join-Path $Root 'AntKitchen.uproject'
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' AntKitchen Win64 Development "-Project=$uproject" -WaitMutex *> $Log
"EXIT=$LASTEXITCODE" | Add-Content $Log
