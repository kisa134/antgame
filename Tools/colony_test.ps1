# Off-screen test of the colony screen (docs/COLONY.md): fresh colony, chamber card, the post-raid show, after 1 / 5 / 15 raids on paper,
# the Queen's moods, zoom. Screenshots -> Saved/Screenshots/colony, log -> Saved/ColonyLogs/test_<name>.log.
# Usage: powershell -File Tools/colony_test.ps1 [-Exe <game exe or editor>] [-Name run] [-Script "<t:cmd;...>"]
param([string]$Exe = "", [string]$Name = "main", [string]$Script = "", [int]$Wait = 150)
$w = Split-Path -Parent $PSScriptRoot
if ($Exe -eq "") { $Exe = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" }
if ($Script -eq "") {
  $s = @(
    "10:colony reset", "12:colony view", "16:colony shot c00_fresh",
    "17:colony open stores", "19:colony shot c01_card_stores", "20:colony open 100", "21:colony shot c02_card_dig", "22:colony close",
    "23:colony sim 1", "24:colony showat 1", "25:colony shot s1_title", "26:colony showat 4.5", "27:colony shot s2_haul",
    "28:colony showat 8.5", "29:colony shot s3_queen", "30:colony showat 11", "31:colony shot s4_brood", "32:colony showat 13.5", "33:colony shot s5_trophy",
    "34:colony showat 16", "35:colony shot s6_build", "36:colony showat 19", "37:colony shot s7_chronicle", "38:colony showat -1", "40:colony shot r01_after1",
    "41:colony sim 4", "42:colony showat -1", "44:colony shot r05_after5", "45:colony info",
    "46:colony sim 10", "47:colony showat -1", "49:colony shot r15_after15", "50:colony info",
    "51:colony open trophies", "53:colony shot c03_card_trophies", "54:colony open memorial", "56:colony shot c04_card_memorial", "57:colony close",
    "58:colony zoom 2.2 0.5 0.33", "60:colony shot z01_throne", "61:colony mood 10", "63:colony shot q0_furious", "64:colony mood 40", "66:colony shot q1_grumpy",
    "67:colony mood 70", "69:colony shot q2_happy", "70:colony mood 95", "72:colony shot q3_delighted", "73:colony sleep", "75:colony shot q4_sleep",
    "76:colony zoom 2.0 0.3 0.4", "78:colony shot z02_nursery", "79:colony zoom 1 0.5 0.5", "80:colony sheet", "82:colony shot c05_sheet", "83:colony view",
    "84:colony v1save", "85:colony reload", "86:colony info", "88:colony shot m01_migrated", "92:colony quit")
  $Script = ($s -join ";")
}
New-Item -ItemType Directory -Force "$w\Saved\ColonyLogs" | Out-Null
$log = "$w\Saved\ColonyLogs\test_$Name.log"
$args = @()
if ($Exe -like "*UnrealEditor*") { $args += "`"$w\AntKitchen.uproject`""; $args += "-game" }
$args += @("-RenderOffScreen", "-nosound", "-windowed", "-ResX=1920", "-ResY=1080", "-AntFastVote", "-AntNoMenu", "-AntDo=`"$Script`"", "-log", "-abslog=`"$log`"")
$p = Start-Process -PassThru -FilePath $Exe -ArgumentList $args
"started $($p.Id) -> $log"
