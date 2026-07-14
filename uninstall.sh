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
BOTTLE_DIR="$HOME/Library/Application Support/CrossOver/Bottles/$BOTTLE"

# Same game-dir detection as install.sh (Steam or Ubisoft Connect layout).
if [ -n "${ANNO_GAME_DIR:-}" ]; then
    GAME_DIR="$ANNO_GAME_DIR"
else
    GAME_DIR=""
    for cand in \
        "$BOTTLE_DIR/drive_c/Program Files (x86)/Steam/steamapps/common/Anno 117 - Pax Romana/Bin/Win64" \
        "$BOTTLE_DIR/drive_c/Program Files (x86)/Ubisoft/Ubisoft Game Launcher/games/Anno 117 - Pax Romana/Bin/Win64" \
        "$BOTTLE_DIR/drive_c/Program Files/Ubisoft/Ubisoft Game Launcher/games/Anno 117 - Pax Romana/Bin/Win64"; do
        if [ -f "$cand/amd_ags_x64.dll" ] || [ -f "$cand/Anno117.exe" ]; then GAME_DIR="$cand"; break; fi
    done
    if [ -z "$GAME_DIR" ]; then
        found="$(find "$BOTTLE_DIR/drive_c" -iname "Anno117.exe" -type f 2>/dev/null | head -n1)"
        [ -n "$found" ] && GAME_DIR="$(dirname "$found")"
    fi
fi

if [ -z "$GAME_DIR" ]; then
    echo "Could not locate the game in bottle '$BOTTLE'; nothing to restore." >&2
    GAME_DIR="/nonexistent"
fi

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
