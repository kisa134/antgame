# Verification run: a solo raid session with markers/surveys/selftest driven by -PTKScript (console commands at second N).
# Usage: test_raid_session.ps1 [-Name s2] [-Packaged -Dist DistPTK] [-Timeout 240] [-Off]   (-Off = same run with -PTKOff, for the overhead comparison)
param([string]$Name = "s2", [switch]$Packaged, [string]$Dist = "DistPTK", [int]$Timeout = 240, [switch]$Off, [int]$Len = 60)
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$w = Split-Path -Parent (Split-Path -Parent $here)
$scr = '12:ptk.marker fun first marker;15:ptk.marker bug second marker;18:ptk.marker confusing third one;22:ptk.ask pace 1 5 Is the pace ok?;30:ptk.selftest 20000;' +
       ($Len + 12) + ':ptk.survey test;' + ($Len + 14) + ':ptk.autosurvey;' + ($Len + 18) + ':ptk.flush;' + ($Len + 22) + ':quit'
$x = '-AntNoMenu -AntRaidNow -AntSeed=7 -AntNoSpider -AntRaidLen=' + $Len + ' -AntDo="20:crush;' + ($Len - 5) + ':raid end leave" '
if ($Off) { $x += '-PTKOff ' } else { $x += '-PTKConsent=yes -PTKScript="' + $scr + '" ' }
if ($Packaged) { & "$here\run_session.ps1" -Name $Name -Timeout $Timeout -Extra $x -Packaged -Dist $Dist } else { & "$here\run_session.ps1" -Name $Name -Timeout $Timeout -Extra $x }
