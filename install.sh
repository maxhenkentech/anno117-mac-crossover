#!/bin/bash
#
# Anno 117: Pax Romana — CrossOver fix installer
# Copies the swapchain-fix shim into the game and enables the required
# Wine DLL overrides. Safe to re-run (e.g. after a game update).
#
# Works for BOTH stores: Steam and Ubisoft Connect running directly in
# CrossOver. Steam doesn't run the game itself — it just launches Ubisoft
# Connect, which launches the game — so the patch is identical; only the game
# folder's location differs. The DLLs always go in the same game-relative spot:
# <game>/Bin/Win64/ next to Anno117.exe.
#
# By default the installer is interactive: it lists your CrossOver bottles, you
# pick the one the game is in, it locates the game (Steam or Ubisoft layout),
# shows you what it found, and asks you to confirm before touching anything.
#
# Non-interactive overrides (for automation / re-runs):
#   ANNO_BOTTLE="Name"      skip the menu, use this bottle
#   ANNO_GAME_DIR="/path"   skip detection, use this Bin/Win64 folder
#   ANNO_YES=1              skip the confirmation prompt
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BOTTLES_ROOT="$HOME/Library/Application Support/CrossOver/Bottles"

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

if [ ! -d "$BOTTLES_ROOT" ]; then
    echo "ERROR: No CrossOver bottles found at:" >&2
    echo "  $BOTTLES_ROOT" >&2
    exit 1
fi

# --- detect the game inside a bottle -----------------------------------------
# Sets globals GAME_DIR and STORE. Returns 0 if found, 1 otherwise.
# $2 = "quick" to skip the (slower) bottle-wide search fallback.
detect_game() {
    local bdir="$1" mode="${2:-full}"
    GAME_DIR=""; STORE=""
    local steam="$bdir/drive_c/Program Files (x86)/Steam/steamapps/common/Anno 117 - Pax Romana/Bin/Win64"
    local ubi1="$bdir/drive_c/Program Files (x86)/Ubisoft/Ubisoft Game Launcher/games/Anno 117 - Pax Romana/Bin/Win64"
    local ubi2="$bdir/drive_c/Program Files/Ubisoft/Ubisoft Game Launcher/games/Anno 117 - Pax Romana/Bin/Win64"
    if [ -f "$steam/Anno117.exe" ]; then GAME_DIR="$steam"; STORE="Steam"; return 0; fi
    if [ -f "$ubi1/Anno117.exe" ]; then GAME_DIR="$ubi1"; STORE="Ubisoft Connect"; return 0; fi
    if [ -f "$ubi2/Anno117.exe" ]; then GAME_DIR="$ubi2"; STORE="Ubisoft Connect"; return 0; fi
    [ "$mode" = "quick" ] && return 1
    # Custom install folder: search the bottle for Anno117.exe.
    local found
    found="$(find "$bdir/drive_c" -iname "Anno117.exe" -type f 2>/dev/null | head -n1)"
    if [ -n "$found" ]; then
        GAME_DIR="$(dirname "$found")"
        case "$found" in
            *steamapps*)  STORE="Steam" ;;
            *Ubisoft*)    STORE="Ubisoft Connect" ;;
            *)            STORE="unknown store" ;;
        esac
        return 0
    fi
    return 1
}

# --- choose the bottle -------------------------------------------------------
if [ -n "${ANNO_BOTTLE:-}" ]; then
    BOTTLE="$ANNO_BOTTLE"
    if [ ! -d "$BOTTLES_ROOT/$BOTTLE" ]; then
        echo "ERROR: Bottle '$BOTTLE' not found at $BOTTLES_ROOT/$BOTTLE" >&2
        exit 1
    fi
else
    # Interactive menu.
    if [ ! -t 0 ]; then
        echo "ERROR: no terminal to prompt on. Set ANNO_BOTTLE=\"Name\" and re-run." >&2
        exit 1
    fi
    bottles=()
    while IFS= read -r d; do bottles+=("$(basename "$d")"); done \
        < <(find "$BOTTLES_ROOT" -mindepth 1 -maxdepth 1 -type d | sort)
    if [ ${#bottles[@]} -eq 0 ]; then
        echo "ERROR: No bottles found in $BOTTLES_ROOT" >&2
        exit 1
    fi
    echo "Which CrossOver bottle is Anno 117 installed in?"
    echo
    i=1
    for b in "${bottles[@]}"; do
        hint=""
        if detect_game "$BOTTLES_ROOT/$b" quick; then hint="  → Anno 117 found ($STORE)"; fi
        printf "  %2d) %s%s\n" "$i" "$b" "$hint"
        i=$((i+1))
    done
    echo
    printf "Enter a number (1-%d): " "${#bottles[@]}"
    read -r choice || { echo "Aborted."; exit 1; }
    if ! [[ "$choice" =~ ^[0-9]+$ ]] || [ "$choice" -lt 1 ] || [ "$choice" -gt "${#bottles[@]}" ]; then
        echo "ERROR: '$choice' is not a valid choice." >&2
        exit 1
    fi
    BOTTLE="${bottles[$((choice-1))]}"
fi

BOTTLE_DIR="$BOTTLES_ROOT/$BOTTLE"

# --- locate the game in the chosen bottle ------------------------------------
if [ -n "${ANNO_GAME_DIR:-}" ]; then
    GAME_DIR="$ANNO_GAME_DIR"
    STORE="(manually specified)"
    if [ ! -f "$GAME_DIR/Anno117.exe" ]; then
        echo "ERROR: ANNO_GAME_DIR does not contain Anno117.exe:" >&2
        echo "  $GAME_DIR" >&2
        exit 1
    fi
else
    echo
    echo "Looking for Anno 117 in bottle '$BOTTLE'..."
    if ! detect_game "$BOTTLE_DIR"; then
        echo "ERROR: Could not find Anno117.exe in bottle '$BOTTLE'." >&2
        echo "Make sure the game is fully installed in this bottle, then re-run." >&2
        echo "Or point straight at the game's Bin/Win64 folder:" >&2
        echo "  ANNO_GAME_DIR=\"/full/path/to/Anno 117 - Pax Romana/Bin/Win64\" ./install.sh" >&2
        exit 1
    fi
fi

# --- confirm before touching anything ----------------------------------------
echo
echo "Found the game:"
echo "  Store    : $STORE"
echo "  Bottle   : $BOTTLE"
echo "  Game dir : $GAME_DIR"
echo
if [ "${ANNO_YES:-0}" != "1" ]; then
    if [ ! -t 0 ]; then
        echo "ERROR: no terminal to confirm on. Set ANNO_YES=1 to proceed non-interactively." >&2
        exit 1
    fi
    printf "Install the fix here? [y/N] "
    read -r ans || { echo "Aborted."; exit 1; }
    case "$ans" in
        [yY]|[yY][eE][sS]) ;;
        *) echo "Aborted — nothing was changed."; exit 0 ;;
    esac
fi

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
echo "Done."
if [ "$STORE" = "Steam" ]; then
    echo "Launch Anno 117 from Steam (inside this bottle) as usual."
    echo "Tip: use ./launch-anno117.sh to avoid the black-screen-on-reopen bug."
else
    echo "Launch Anno 117 from Ubisoft Connect's Play button (inside this bottle)."
    echo "If a relaunch comes up black, clear leftover Ubisoft processes first —"
    echo "see the black-screen note in the README."
fi
