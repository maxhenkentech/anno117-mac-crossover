#!/bin/bash
#
# Anno 117 launcher for CrossOver.
# Kills any leftover game / Ubisoft launcher processes first, then starts the
# game through Steam. Use this instead of Steam's Play button to avoid the
# "black screen on reopen" bug (a stale Ubisoft launcher from a previous run
# makes the next launch come up to a black screen).
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
STEAM_APPID=3274580

echo "Clearing any leftover Anno / Ubisoft processes..."
pkill -9 -f "Anno117.exe"            2>/dev/null || true
pkill -9 -f "upc.exe"                2>/dev/null || true
pkill -9 -f "UplayWebCore"           2>/dev/null || true
pkill -9 -f "UbisoftGameLauncher"    2>/dev/null || true
pkill -9 -f "UplayService"           2>/dev/null || true
sleep 2

# Make sure Steam (inside the bottle) is running; start it if not.
if ! pgrep -qf "steamwebhelper"; then
    echo "Starting Steam..."
    "$CX" --bottle "$BOTTLE" --no-wait -- 'C:\Program Files (x86)\Steam\steam.exe' >/dev/null 2>&1
    for _ in $(seq 1 25); do pgrep -qf steamwebhelper && break; sleep 3; done
    sleep 5
fi

echo "Launching Anno 117..."
"$CX" --bottle "$BOTTLE" --no-wait -- 'C:\Program Files (x86)\Steam\steam.exe' "steam://rungameid/$STEAM_APPID" >/dev/null 2>&1
echo "Launched. The 'outdated graphics driver' popup is normal — click to continue."
