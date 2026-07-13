#!/bin/bash
#
# Anno 117: Pax Romana — CrossOver fix installer
# Copies the swapchain-fix shim into the game and enables the required
# Wine DLL overrides. Safe to re-run (e.g. after a Steam update).
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# --- locate CrossOver (prefer Preview, which ships the newer D3DMetal) ---
CX=""
for app in "/Applications/CrossOver Preview.app" "/Applications/CrossOver.app"; do
    if [ -x "$app/Contents/SharedSupport/CrossOver/bin/wine" ]; then
        CX="$app/Contents/SharedSupport/CrossOver/bin/wine"; break
    fi
done
if [ -z "$CX" ]; then
    echo "ERROR: CrossOver not found in /Applications. Install CrossOver first." >&2
    exit 1
fi

# --- locate the bottle and the game ---
BOTTLE="${ANNO_BOTTLE:-Steam}"
BOTTLE_DIR="$HOME/Library/Application Support/CrossOver/Bottles/$BOTTLE"
GAME_DIR="$BOTTLE_DIR/drive_c/Program Files (x86)/Steam/steamapps/common/Anno 117 - Pax Romana/Bin/Win64"

if [ ! -d "$GAME_DIR" ]; then
    echo "ERROR: Could not find the game at:" >&2
    echo "  $GAME_DIR" >&2
    echo "If your Steam bottle has a different name, run:" >&2
    echo "  ANNO_BOTTLE=YourBottleName ./install.sh" >&2
    exit 1
fi

echo "CrossOver : $CX"
echo "Bottle    : $BOTTLE"
echo "Game dir  : $GAME_DIR"
echo

# --- back up the real amd_ags_x64.dll -> amd_ags_orig.dll (once) ---
if [ ! -f "$GAME_DIR/amd_ags_orig.dll" ]; then
    cp "$GAME_DIR/amd_ags_x64.dll" "$GAME_DIR/amd_ags_orig.dll"
    echo "Backed up original AMD DLL -> amd_ags_orig.dll"
else
    # If our shim is already installed, don't clobber the real backup.
    echo "amd_ags_orig.dll already present (keeping existing backup)"
fi

# --- install the shim ---
cp "$SCRIPT_DIR/amd_ags_x64.dll" "$GAME_DIR/amd_ags_x64.dll"
echo "Installed shim -> amd_ags_x64.dll"

# --- enable the DLL overrides for Anno117.exe ---
KEY='HKCU\Software\Wine\AppDefaults\Anno117.exe\DllOverrides'
"$CX" --bottle "$BOTTLE" --wait-children -- reg add "$KEY" /v amd_ags_x64  /t REG_SZ /d 'native,builtin' /f >/dev/null 2>&1
"$CX" --bottle "$BOTTLE" --wait-children -- reg add "$KEY" /v amd_ags_orig /t REG_SZ /d 'native'          /f >/dev/null 2>&1
echo "Enabled DLL overrides (amd_ags_x64=native,builtin  amd_ags_orig=native)"

echo
echo "Done. Launch Anno 117 from Steam (inside this bottle) as usual."
echo "Tip: use ./launch-anno117.sh to avoid the black-screen-on-reopen bug."
