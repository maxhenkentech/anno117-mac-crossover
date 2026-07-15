#!/bin/bash
#
# Anno 117: Pax Romana — CrossOver fix installer (Ubisoft Connect edition)
#
# Same swapchain-fix shim as install.sh, but tailored to a direct Ubisoft
# Connect install running in CrossOver:
#   * bottle defaults to "Ubisoft Connect"
#   * detects the game in the Ubisoft Game Launcher layout (x86 / x64 / custom)
#   * sets the Wine DLL overrides for BOTH Anno117.exe and Anno117_plus.exe
#     (the Ubisoft build ships the _plus variant; both import amd_ags_x64.dll,
#     so both need the overrides — the generic installer only covers the base
#     exe, which is why the Ubisoft copy shipped the shim but still crashed)
#   * does NOT suppress `reg` errors: a silently-failed override write leaves
#     the DLLs copied but the fix inactive, which looks exactly like "doesn't
#     work" (the original Ubisoft bug)
#
# Safe to re-run (e.g. after a game update or "Verify files" in Ubisoft Connect).
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

# --- detect the game inside a bottle (Ubisoft layout) ------------------------
# Sets globals GAME_DIR. Returns 0 if found, 1 otherwise.
# $2 = "quick" to skip the (slower) bottle-wide search fallback.
detect_game() {
    local bdir="$1" mode="${2:-full}"
    GAME_DIR=""
    local ubi1="$bdir/drive_c/Program Files (x86)/Ubisoft/Ubisoft Game Launcher/games/Anno 117 - Pax Romana/Bin/Win64"
    local ubi2="$bdir/drive_c/Program Files/Ubisoft/Ubisoft Game Launcher/games/Anno 117 - Pax Romana/Bin/Win64"
    if [ -f "$ubi1/Anno117.exe" ]; then GAME_DIR="$ubi1"; return 0; fi
    if [ -f "$ubi2/Anno117.exe" ]; then GAME_DIR="$ubi2"; return 0; fi
    [ "$mode" = "quick" ] && return 1
    # Custom install folder: search the bottle for Anno117.exe.
    local found
    found="$(find "$bdir/drive_c" -iname "Anno117.exe" -type f 2>/dev/null | head -n1)"
    if [ -n "$found" ]; then
        GAME_DIR="$(dirname "$found")"
        return 0
    fi
    return 1
}

# --- choose the bottle (default: "Ubisoft Connect") --------------------------
if [ -n "${ANNO_BOTTLE:-}" ]; then
    BOTTLE="$ANNO_BOTTLE"
    if [ ! -d "$BOTTLES_ROOT/$BOTTLE" ]; then
        echo "ERROR: Bottle '$BOTTLE' not found at $BOTTLES_ROOT/$BOTTLE" >&2
        exit 1
    fi
else
    BOTTLE="Ubisoft Connect"
    if [ ! -d "$BOTTLES_ROOT/$BOTTLE" ]; then
        # Fall back to an interactive menu if the default Ubisoft bottle is absent.
        if [ ! -t 0 ]; then
            echo "ERROR: default bottle '$BOTTLE' not found and no terminal to prompt on." >&2
            echo "Set ANNO_BOTTLE=\"Name\" and re-run." >&2
            exit 1
        fi
        echo "Default bottle '$BOTTLE' not found."
        bottles=()
        while IFS= read -r d; do bottles+=("$(basename "$d")"); done \
            < <(find "$BOTTLES_ROOT" -mindepth 1 -maxdepth 1 -type d | sort)
        if [ ${#bottles[@]} -eq 0 ]; then
            echo "ERROR: No bottles found in $BOTTLES_ROOT" >&2
            exit 1
        fi
        echo "Which CrossOver bottle is Anno 117 (Ubisoft Connect) installed in?"
        echo
        i=1
        for b in "${bottles[@]}"; do
            hint=""
            if detect_game "$BOTTLES_ROOT/$b" quick; then hint="  → Anno 117 found"; fi
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
fi

BOTTLE_DIR="$BOTTLES_ROOT/$BOTTLE"

# --- locate the game in the chosen bottle ------------------------------------
if [ -n "${ANNO_GAME_DIR:-}" ]; then
    GAME_DIR="$ANNO_GAME_DIR"
    if [ ! -f "$GAME_DIR/Anno117.exe" ]; then
        echo "ERROR: ANNO_GAME_DIR does not contain Anno117.exe:" >&2
        echo "  $GAME_DIR" >&2
        exit 1
    fi
else
    echo
    echo "Looking for Anno 117 (Ubisoft Connect) in bottle '$BOTTLE'..."
    if ! detect_game "$BOTTLE_DIR"; then
        echo "ERROR: Could not find Anno117.exe in bottle '$BOTTLE'." >&2
        echo "Make sure the game is fully installed via Ubisoft Connect, then re-run." >&2
        echo "Or point straight at the game's Bin/Win64 folder:" >&2
        echo "  ANNO_GAME_DIR=\"/full/path/to/Anno 117 - Pax Romana/Bin/Win64\" ./install-ubisoft.sh" >&2
        exit 1
    fi
fi

# --- confirm before touching anything ----------------------------------------
echo
echo "Found the game:"
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

# --- enable the DLL overrides for the game exe(s) ----------------------------
# The Ubisoft build ships Anno117_plus.exe alongside Anno117.exe; both
# statically import amd_ags_x64.dll, so both need the overrides. Do NOT swallow
# reg output — a silent failure here is exactly what left the Ubisoft copy with
# copied DLLs but a crashing game.
set_overrides() {
    local exe="$1"
    local key="HKCU\\Software\\Wine\\AppDefaults\\${exe}\\DllOverrides"
    "$CX" --bottle "$BOTTLE" --wait-children -- reg add "$key" /v amd_ags_x64 /t REG_SZ /d 'native,builtin' /f \
        || { echo "ERROR: failed to set amd_ags_x64 override for $exe." >&2; exit 1; }
    "$CX" --bottle "$BOTTLE" --wait-children -- reg add "$key" /v amd_ags_orig /t REG_SZ /d 'native' /f \
        || { echo "ERROR: failed to set amd_ags_orig override for $exe." >&2; exit 1; }
}

set_overrides "Anno117.exe"
[ -f "$GAME_DIR/Anno117_plus.exe" ] && set_overrides "Anno117_plus.exe"
echo "Enabled DLL overrides (amd_ags_x64=native,builtin  amd_ags_orig=native)"

echo
echo "Done."
echo "Launch Anno 117 from Ubisoft Connect's Play button (inside this bottle)."
echo "If a relaunch comes up black, clear leftover Ubisoft processes first —"
echo "see the black-screen note in the README."
