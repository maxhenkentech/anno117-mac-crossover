#!/bin/bash
#
# Anno 117: Pax Romana — restore display settings in engine.ini.
#
# Anno overwrites config/engine.ini on every launch with the resolution it
# thinks your display is, which on a HiDPI Mac is the scaled (logical)
# resolution (e.g. 1728x1117) rather than the native panel resolution
# (e.g. 3456x2234). Running this before each launch resets the Window
# section to borderless at your native resolution. Idempotent — safe to
# run any number of times.
#
# Usage:
#   ./restore-resolution.sh
#
# Non-interactive overrides:
#   ANNO_BOTTLE="Name"      bottle name (default: "Ubisoft Connect")
#   ANNO_WIDTH=3456         native panel width  (default: 3456, 16" MBP)
#   ANNO_HEIGHT=2234        native panel height (default: 2234, 16" MBP)
#   ANNO_USER="crossover    Windows username inside the bottle (default: crossover)
#
set -euo pipefail

BOTTLE="${ANNO_BOTTLE:-Ubisoft Connect}"
WIDTH="${ANNO_WIDTH:-3456}"
HEIGHT="${ANNO_HEIGHT:-2234}"
USER="${ANNO_USER:-crossover}"

INI="$HOME/Library/Application Support/CrossOver/Bottles/$BOTTLE/drive_c/users/$USER/Documents/Anno 117 - Pax Romana/config/engine.ini"

if [ ! -f "$INI" ]; then
    echo "ERROR: engine.ini not found at:" >&2
    echo "  $INI" >&2
    echo "Launch the game once so it creates the file, then re-run this script." >&2
    exit 1
fi

cp "$INI" "$INI.bak"
sed -E -i '' \
    -e "s/\"ScreenXSize\": *[0-9]+/\"ScreenXSize\": $WIDTH/" \
    -e "s/\"ScreenYSize\": *[0-9]+/\"ScreenYSize\": $HEIGHT/" \
    -e "s/\"FullscreenType\": *[0-9]+/\"FullscreenType\": 1/" \
    -e "s/\"NoWindowFrame\": *(true|false)/\"NoWindowFrame\": true/" \
    -e "s/\"MaximizedWindow\": *(true|false)/\"MaximizedWindow\": true/" \
    "$INI"

echo "Restored display settings in engine.ini:"
echo "  Resolution : ${WIDTH}x${HEIGHT} (borderless, maximized, no frame)"
echo "  Backup     : ${INI}.bak"
echo "Now launch the game (e.g. Ubisoft Connect's Play button)."
