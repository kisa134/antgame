#!/usr/bin/env bash
# Ant Kitchen - one-command native macOS build (Apple Silicon).
#
#   ./Tools/mac/build_mac.sh                 # checks + build + cook + package  (the normal case)
#   ./Tools/mac/build_mac.sh --check         # only check the machine, build nothing
#   ./Tools/mac/build_mac.sh --editor        # only compile the editor modules (for opening the .uproject)
#   ./Tools/mac/build_mac.sh --clean         # delete Binaries/Intermediate/Saved-Staged first (fixes odd errors)
#
# Env overrides:  UE_ROOT=/path/to/UE_5.8   OUT_DIR=/path/for/the/app   CONFIG=Development|Shipping   ARCH=arm64
# Step-by-step guide for humans (Russian): docs/MAC_BUILD.md
set -u
set -o pipefail

say()  { printf '\n\033[1;36m[AntKitchen/mac] %s\033[0m\n' "$*"; }
ok()   { printf '\033[1;32m  OK   %s\033[0m\n' "$*"; }
warn() { printf '\033[1;33m  WARN %s\033[0m\n' "$*"; }
die()  { printf '\033[1;31m  FAIL %s\033[0m\n' "$*" >&2; exit 1; }

MODE=build
for a in "$@"; do
  case "$a" in
    --check) MODE=check ;;
    --editor) MODE=editor ;;
    --clean) CLEAN=1 ;;
    -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
    *) die "unknown option: $a (try --help)" ;;
  esac
done

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_DIR="$(cd "$HERE/../.." && pwd)"
PROJ="$PROJ_DIR/AntKitchen.uproject"
CONFIG="${CONFIG:-Development}"
ARCH="${ARCH:-arm64}"
OUT_DIR="${OUT_DIR:-$PROJ_DIR/Build/Mac}"
WANT_XCODE="26.1"          # Epic recommends Xcode 26.1.1 for UE 5.8; 26.4 is NOT supported

# ---------------------------------------------------------------- 1. the machine
say "1/6  Checking the Mac"
[ "$(uname -s)" = "Darwin" ] || die "This script is for macOS only (you are on $(uname -s))."
[ -f "$PROJ" ] || die "AntKitchen.uproject not found at $PROJ (run the script from the cloned repo)."
ok "project: $PROJ"

HW_ARCH="$(uname -m)"
if [ "$HW_ARCH" != "arm64" ]; then
  warn "This Mac is $HW_ARCH (Intel). Intel Macs are not supported by this guide; trying ARCH=x86_64."
  ARCH="x86_64"
fi
ok "CPU: $HW_ARCH, building for: $ARCH"

MACOS_VER="$(sw_vers -productVersion)"
MACOS_MAJOR="${MACOS_VER%%.*}"
ok "macOS $MACOS_VER"
[ "$MACOS_MAJOR" -ge 14 ] || die "macOS 14 (Sonoma) or newer is required; you have $MACOS_VER."

FREE_GB="$(df -g "$PROJ_DIR" | awk 'NR==2{print $4}')"
ok "free disk space: ${FREE_GB} GB"
[ "${FREE_GB:-0}" -ge 40 ] || warn "Less than 40 GB free: cook + package of this project needs about 30-40 GB of scratch space."

command -v git >/dev/null || die "git not found. Run: xcode-select --install"
if ! git lfs version >/dev/null 2>&1; then
  die "git-lfs is missing. Install it:  brew install git-lfs && git lfs install   (or download from git-lfs.com)"
fi
ok "git + git-lfs present"

# LFS pointers instead of real files = the #1 reason for a broken build. A pointer file is ~130 bytes of text.
SAMPLE="$(find "$PROJ_DIR/Content" -name '*.uasset' -size -300c 2>/dev/null | head -n 1)"
if [ -n "$SAMPLE" ] && head -c 40 "$SAMPLE" | grep -q 'git-lfs'; then
  warn "Content still contains Git LFS pointers (e.g. ${SAMPLE#"$PROJ_DIR"/}). Downloading real files now: git lfs pull"
  [ "$MODE" = check ] || (cd "$PROJ_DIR" && git lfs pull) || die "git lfs pull failed (network? LFS quota?). Fix and run again."
fi

# ---------------------------------------------------------------- 2. Xcode
say "2/6  Checking Xcode"
command -v xcodebuild >/dev/null || die "Xcode is not installed. Install Xcode $WANT_XCODE from the App Store / developer.apple.com, open it once, accept the licence."
XVER="$(xcodebuild -version 2>/dev/null | awk 'NR==1{print $2}')"
XPATH="$(xcode-select -p 2>/dev/null)"
ok "Xcode $XVER at $XPATH"
case "$XPATH" in
  *Xcode*.app/*) : ;;
  *) die "xcode-select points to '$XPATH' (command line tools only). Run: sudo xcode-select -s /Applications/Xcode.app/Contents/Developer" ;;
esac
case "$XVER" in
  26.1*|26.0*|26.2*|26.3*) ok "Xcode version is in the supported range" ;;
  26.4*|26.5*|26.6*|26.7*|26.8*|26.9*) warn "Xcode $XVER is newer than what Epic supports for UE 5.8 (recommended: 26.1.1). If the build fails with compiler errors, install Xcode 26.1.1 from developer.apple.com/download/all and run: sudo xcode-select -s /path/to/Xcode-26.1.1.app/Contents/Developer" ;;
  *) warn "Xcode $XVER is not the recommended 26.1.1 - it may still work, but if you hit compiler errors install 26.1.1." ;;
esac
if ! xcodebuild -license check >/dev/null 2>&1; then
  die "The Xcode licence is not accepted. Run once:  sudo xcodebuild -license accept"
fi
# The Metal shader compiler is a separate download in new Xcode versions.
if ! xcrun -sdk macosx metal --version >/dev/null 2>&1; then
  warn "Metal shader toolchain not found. Run:  xcodebuild -downloadComponent MetalToolchain   (then run this script again)"
  [ "$MODE" = check ] || die "Metal toolchain is required to cook shaders."
else
  ok "Metal toolchain present"
fi

# ---------------------------------------------------------------- 3. engine
say "3/6  Looking for Unreal Engine 5.8"
UE_ROOT_CAND=("${UE_ROOT:-}" "/Users/Shared/Epic Games/UE_5.8" "$HOME/Epic Games/UE_5.8" "/Applications/Epic Games/UE_5.8")
UE=""
for c in "${UE_ROOT_CAND[@]}"; do
  [ -n "$c" ] && [ -x "$c/Engine/Build/BatchFiles/RunUAT.sh" ] && UE="$c" && break
done
[ -n "$UE" ] || die "UE 5.8 not found. Install it in the Epic Games Launcher (Library -> + -> 5.8), or pass UE_ROOT=\"/path/to/UE_5.8\"."
ok "engine: $UE"
UAT="$UE/Engine/Build/BatchFiles/RunUAT.sh"
UBT="$UE/Engine/Build/BatchFiles/Mac/Build.sh"
[ -d "$UE/Engine/Binaries/Mac" ] || warn "Engine/Binaries/Mac not found: the Mac platform may not be installed. Launcher -> UE 5.8 -> Options -> Target Platforms -> tick 'Mac'."

[ "$MODE" = check ] && { say "Check finished: this Mac looks ready."; exit 0; }

if [ "${CLEAN:-0}" = 1 ]; then
  say "Cleaning old build output"
  rm -rf "$PROJ_DIR/Binaries/Mac" "$PROJ_DIR/Intermediate/Build/Mac" "$PROJ_DIR/Saved/StagedBuilds" "$OUT_DIR"
  ok "cleaned"
fi

# ---------------------------------------------------------------- 4. (optional) project files / editor
say "4/6  Project files"
GEN="$UE/Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh"
if [ -x "$GEN" ]; then
  "$GEN" -project="$PROJ" -game >/tmp/antkitchen_genproj.log 2>&1 && ok "Xcode project generated (optional)" || warn "GenerateProjectFiles failed (not needed for packaging); log: /tmp/antkitchen_genproj.log"
else
  ok "no GenerateProjectFiles.sh in this engine install - not needed, packaging does not use it"
fi

if [ "$MODE" = editor ]; then
  say "Compiling editor modules (so the .uproject opens without the 'rebuild?' dialog)"
  "$UBT" AntKitchenEditor Mac Development -project="$PROJ" -architecture="$ARCH" -WaitMutex || die "Editor build failed - see the log above (search for 'error:')."
  ok "editor modules built. Now double-click AntKitchen.uproject"
  exit 0
fi

# ---------------------------------------------------------------- 5. build + cook + stage + package
say "5/6  Build + cook + package ($CONFIG, $ARCH). First run: 40-90 minutes (shaders). Next runs are much faster."
mkdir -p "$OUT_DIR"
START=$(date +%s)
"$UAT" BuildCookRun \
  -project="$PROJ" \
  -noP4 -utf8output \
  -platform=Mac -targetplatform=Mac \
  -specifiedarchitecture="$ARCH" \
  -clientconfig="$CONFIG" \
  -build -cook -allmaps -stage -pak -compressed \
  -archive -archivedirectory="$OUT_DIR" \
  -nocompileeditor 2>&1 | tee /tmp/antkitchen_mac_build.log
RC=${PIPESTATUS[0]}
MIN=$(( ( $(date +%s) - START ) / 60 ))
if [ "$RC" -ne 0 ]; then
  printf '\033[1;31m'
  echo "  FAIL: packaging stopped after ${MIN} min (exit code $RC). Last error lines:"
  grep -E 'error|Error:|ERROR|Failed to|failed' /tmp/antkitchen_mac_build.log | tail -n 15
  printf '\033[0m'
  echo "  Full log: /tmp/antkitchen_mac_build.log   | read docs/MAC_BUILD.md, section 'Если что-то сломалось'"
  exit "$RC"
fi
ok "packaged in ${MIN} min"

# ---------------------------------------------------------------- 6. result
say "6/6  Preparing the app"
APP="$(find "$OUT_DIR" -maxdepth 3 -name '*.app' -print -quit)"
[ -n "$APP" ] || die "Packaging reported success but no .app was found under $OUT_DIR"
ok "app: $APP"
# Local use needs no Apple certificate. Remove the download quarantine flag and sign ad-hoc so Gatekeeper does not block the run.
xattr -dr com.apple.quarantine "$APP" 2>/dev/null && ok "quarantine flag removed"
if codesign --force --deep --sign - "$APP" >/dev/null 2>&1; then ok "ad-hoc signed"; else warn "ad-hoc signing failed (usually harmless for a locally built app)"; fi

cat <<EOF

=====================================================================
 DONE.  Start Steam first and log in, then run:

   open "$APP"

 or, to see the game log in the Terminal:

   "$APP/Contents/MacOS/"* -log

 Co-op: host or join through Steam exactly like on Windows.
 The Windows friends must run the SAME game version (same git commit)!
 Your version:  $(cd "$PROJ_DIR" && git rev-parse --short HEAD 2>/dev/null || echo unknown)
=====================================================================
EOF
