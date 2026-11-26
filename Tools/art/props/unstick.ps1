# kills only the ValidatePlatforms (AutoSDKInfo) cmd that MY props commandlets spawn (it waits on the shared build lock)
param([int]$secs = 65)
$end = (Get-Date).AddSeconds($secs)
while ((Get-Date) -lt $end) {
    Get-CimInstance Win32_Process -Filter "name='cmd.exe'" | ? { $_.CommandLine -match 'AutoSDKInfo' } | % {
        $par = Get-CimInstance Win32_Process -Filter "ProcessId=$($_.ParentProcessId)"
        if ($par -and $par.CommandLine -match 'AntKitchen_props' -and $par.CommandLine -match 'props_') {
            "kill $($_.ProcessId) (parent $($par.ProcessId))"; Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
        }
    }
    Start-Sleep 5
}
$d = "C:\Users\HYPERPC\Documents\Unreal Projects\AntKitchen_props\Saved\art_props"
foreach ($f in "glass.log", "import.log") { if (Test-Path "$d\$f") { "$f : " + ((Get-Content "$d\$f" -Tail 1) -replace '^(.{0,150}).*', '$1') } }
