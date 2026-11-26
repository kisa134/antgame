# Ant Kitchen Launcher

Windows desktop launcher for the Ant Kitchen playtest (Python 3.11 + pywebview/WebView2 UI, PyInstaller one-file exe).
It detects the platform, finds the newest `AntKitchen-playtest-*.zip` among the GitHub Releases of `kisa134/antgame`
(prereleases included, newest by `created_at`), downloads it with pause/resume (HTTP Range + `.part`), verifies size and
SHA-256 (`.sha256` asset or the GitHub asset digest), extracts to `<root>\builds\<tag>\`, migrates `Windows\AntKitchen\Saved`
to the new build, keeps one previous build for rollback, installs the VC++ prerequisites once, checks Steam and launches the game.
Tabs: Play, News (release notes), How to play, Co-op, Links, Bug report (collects `Saved\Logs|Crashes|Playtest` to the Desktop), Settings.
The launcher updates itself from the release asset `AntKitchenLauncher.exe` (+ `launcher-version.txt`).

## Build
    powershell -File Tools\launcher\build.ps1      # -> Tools\launcher\dist\AntKitchenLauncher.exe + launcher-version.txt
Bump `VERSION` in `src/launcher.py` for every launcher release.

## Release
    powershell -File Tools\release\release.ps1 -Tag playtest-YYYY-MM-DD-vNN -Dist <DistDir> -Notes <notes.md>
uploads zip, `.sha256`, `version.json`, `AntKitchenLauncher.exe`, `launcher-version.txt` (creates the prerelease or `--clobber`-uploads to an existing one).

## Dev / test
- `src/web/index.html?mock=installed|update|download|notinstalled|mac|nosteam|selfupd` renders the UI in any browser with fake state.
- Env: `AK_HOME` (config dir), `AK_ROOT` (install root), `AK_API` (releases API URL), `AK_AUTO=<log>` (headless end-to-end self test:
  install, Saved migration, headless launch, kill), `AK_SHOT=<png>` + `AK_SHOT_TABS=play,news` (PrintWindow screenshots of the real window).
- Platform logic is portable (macOS paths and `ps`/`open` fallbacks); on a Mac the launcher shows the CrossOver page. No Mac binary is built here.
- Config: `%APPDATA%\AntKitchenLauncher\config.json`. Default root: `%LOCALAPPDATA%\AntKitchen`.