#!/bin/bash
#
# Anno 117: Pax Romana — CrossOver fix uninstaller
# Restores the original AMD DLL and removes the Wine DLL overrides.
#
set -euo pipefail

CX=""
for app in "/Applications/CrossOver Preview.app" "/Applications/CrossOver.app"; do
    if [ -x "$app/Contents/SharedSupport/CrossOver/bin/wine" ]; then
        CX="$app/Contents/SharedSupport/CrossOver/bin/wine"; break
    fi
done
[ -z "$CX" ] && { echo "ERROR: CrossOver not found." >&2; exit 1; }

BOTTLE="${ANNO_BOTTLE:-Steam}"
GAME_DIR="$HOME/Library/Application Support/CrossOver/Bottles/$BOTTLE/drive_c/Program Files (x86)/Steam/steamapps/common/Anno 117 - Pax Romana/Bin/Win64"

if [ -f "$GAME_DIR/amd_ags_orig.dll" ]; then
    cp "$GAME_DIR/amd_ags_orig.dll" "$GAME_DIR/amd_ags_x64.dll"
    rm -f "$GAME_DIR/amd_ags_orig.dll"
    echo "Restored original amd_ags_x64.dll"
else
    echo "No backup found; leaving amd_ags_x64.dll as-is."
fi

KEY='HKCU\Software\Wine\AppDefaults\Anno117.exe\DllOverrides'
"$CX" --bottle "$BOTTLE" --wait-children -- reg delete "$KEY" /v amd_ags_x64  /f >/dev/null 2>&1 || true
"$CX" --bottle "$BOTTLE" --wait-children -- reg delete "$KEY" /v amd_ags_orig /f >/dev/null 2>&1 || true
echo "Removed DLL overrides. Fix uninstalled."
