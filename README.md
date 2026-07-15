# Anno 117: Pax Romana — Mac / CrossOver fix

Make **Anno 117: Pax Romana** launch and run on an Apple‑Silicon Mac using
**CrossOver**. Out of the box the game shows the Ubisoft launcher, flashes a
black screen, then crashes back to Steam. This fix gets it to the main menu and
into campaigns.

> Not affiliated with, authorized, or endorsed by Ubisoft, Blue Byte, Apple, or
> CodeWeavers. "Anno" is a trademark of its respective owner, used here only to
> describe what the fix is for. Requires a legitimately purchased copy of the
> game. This is an **interoperability fix**: it does not modify, bypass, or
> disable any DRM or copy protection. Provided as‑is, no warranty. See
> [LICENSE](LICENSE).

---

## Screenshots

<img src="screenshots/Anno.png" alt="Anno 117 on Mac CrossOver" />

<img src="screenshots/Anno2.png" alt="Anno 117 gameplay on Mac CrossOver" />

**Performance:** Getting ~30 FPS on high settings and ~50 FPS on medium settings on an **M5 Max**. High settings with DLSS set to Quality, running on **Game Porting Toolkit 4.0b1**.

---

## Quick Start

### What you need before starting

- An **Apple Silicon Mac** (M1, M2, M3, M4, M5, or newer) running macOS 15 or newer
- **Anno 117: Pax Romana** purchased and installed, either through **Steam** or through **Ubisoft Connect** running directly in CrossOver
- **CrossOver** installed. This fix is developed and tested against **[CrossOver Preview](https://www.codeweavers.com/crossover/preview)**, which ships **Game Porting Toolkit 4 (GPTK4)** integrated — Apple's latest D3DMetal translation layer, and the one this shim's swapchain path is verified against. The free CrossOver Preview is the recommended version; older CrossOver releases without GPTK4 may not run the game at all.

If you don't have CrossOver yet, download it first and set up a bottle with Anno 117 installed (CrossOver will walk you through this).

> **Steam or Ubisoft Connect — same fix.** Steam doesn't run the game itself; it just launches Ubisoft Connect, which launches the game. The fix patches the game the same way either way. The Quick Start below uses the Steam copy; if you run Ubisoft Connect directly, see **"Ubisoft Connect version"** just after it.

### Step 1: Download this fix

Download the latest release from [here](https://github.com/maxhenkentech/anno117-mac-crossover/releases/latest).

This will download a `.zip` file to your **Downloads** folder. Double-click it to extract it — you'll get a folder called `anno117-mac-crossover`.

### Step 2: Install the fix

1. Open **Terminal** (press `⌘ Space`, type "Terminal", press Enter)
2. Copy and paste the following command, then press Enter:

   ```
   cd ~/Downloads/anno117-mac-crossover && chmod +x *.sh && ./install.sh
   ```

3. The installer is **interactive**. It shows a numbered list of your CrossOver bottles (and marks the ones where it can see Anno 117, and whether it's the **Steam** or **Ubisoft Connect** copy). Type the number of the bottle the game is in, press Enter, check the folder it found, and type `y` to confirm.

4. You should see `Done.` at the end. That's it — the fix is installed!

> **Note:** If you saved the folder somewhere other than Downloads, replace `~/Downloads/` with the path where you extracted it. For example, if it's on your Desktop: `cd ~/Desktop/anno117-mac-crossover && chmod +x *.sh && ./install.sh`

### Step 3: Play the game

The easiest way to launch is with the included helper script, which prevents common issues:

```
./launch-anno117.sh
```

You can also just press **Play** in Steam normally. (But if you've played once already today, use the script above — see the troubleshooting section below.)

**First launch:** After the Ubisoft launcher appears, you'll see a popup about your **graphics driver being outdated**. This is **normal and harmless** — just click through it and the game will load.

---

## Ubisoft Connect version (running Ubisoft Connect directly in CrossOver)

> **Note:** the fix in this repo was developed and tested against the **Steam** copy. The steps here are the same fix applied to a direct Ubisoft Connect install — provided as best-effort guidance and **not yet tested** by the author (I don't own the game on Ubisoft Connect). If you try it, a PR confirming or correcting these steps is very welcome.

The fix is **identical** — it patches `Anno117.exe`, the same binary regardless of store. Only two things change: **where the game folder is** and **how you launch it**.

**Install.** Ubisoft Connect users should prefer the dedicated installer:

```
./install-ubisoft.sh
```

It applies the **same** swapchain-fix shim as `install.sh`, but is tailored to a direct Ubisoft Connect install:

- The bottle defaults to **`Ubisoft Connect`** (no menu if that's your bottle name).
- It sets the Wine DLL overrides for **both** `Anno117.exe` **and** `Anno117_plus.exe`. The Ubisoft build ships the `_plus` variant alongside the base exe, and **both** statically import `amd_ags_x64.dll`. The generic `install.sh` only overrides `Anno117.exe` — so on a Ubisoft Connect install it can leave the DLLs copied but the game still crashing on the `_plus` path.
- It does **not** swallow `reg` errors: a silently-failed override write looks exactly like "the fix doesn't work", which was the original Ubisoft bug.

It takes the same non-interactive overrides as `install.sh` (`ANNO_BOTTLE`, `ANNO_GAME_DIR`, `ANNO_YES`) and is safe to re-run after a game update or "Verify files".

**Launch.** Start the game from **Ubisoft Connect's own Play button** (or its CrossOver launcher entry) — not through Steam. The helper script `./launch-anno117.sh` is **Steam-specific** and does not apply. The black-screen-on-reopen note in Troubleshooting still applies; if a relaunch comes up black, clear the leftover Ubisoft processes first (command below).

Placing the files by hand? See **"Manual install"** below — the DLL location is the same as Steam, only the parent path differs.

---

## Troubleshooting

### Black screen when reopening the game? (common)

If you quit the game and try to start it again and get a **black screen**, a leftover Ubisoft process is still running in the background.

**The fix:** Run the launch script instead of clicking Play in Steam:

```
./launch-anno117.sh
```

This automatically cleans up leftover processes before launching. Make sure you're in the right folder first:

```
cd ~/Downloads/anno117-mac-crossover && ./launch-anno117.sh
```

### Game stopped working after a game update?

If the game was updated (or you ran Steam's "Verify integrity of game files" / Ubisoft Connect's "Verify files"), the fix was overwritten. Just reinstall it and pick the same bottle:

```
cd ~/Downloads/anno117-mac-crossover && ./install.sh
 ```

### Want to remove the fix completely?

```
cd ~/Downloads/anno117-mac-crossover && ./uninstall.sh
```

This restores the original game file and removes the CrossOver settings.

---

## Fullscreen

The installer sets **borderless fullscreen** at your display's native resolution by default.

- **In game:** Options → Graphics → *Display Mode*. Choose **Borderless** (recommended). Avoid exclusive **Fullscreen** — it can misbehave under CrossOver.
- **By hand:** Edit `engine.ini`, in the `"Window"` section. The file lives in the **Documents** folder inside the bottle (created on first launch), **not** next to the game binary — Anno stores its config here for both the Steam and Ubisoft Connect copies:
  - `…/Bottles/<YourBottle>/drive_c/users/crossover/Documents/Anno 117 - Pax Romana/config/engine.ini`
  - `"FullscreenType": 1` → 0 = windowed, 1 = borderless (recommended), 2 = exclusive
  - `"ScreenXSize"` / `"ScreenYSize"` → your display's native resolution (e.g. 3456 × 2234 for a 16" MacBook Pro, 3024 × 1964 for a 14")
  - `"NoWindowFrame": true`, `"MaximizedWindow": true`

---

## Different bottle name?

`./install.sh` shows a menu of your bottles, so you normally don't need to name one. But you can skip the prompts (handy for re-runs or automation):

```
ANNO_BOTTLE="YourBottleName" ANNO_YES=1 ./install.sh
ANNO_BOTTLE="YourBottleName" ./launch-anno117.sh          # Steam only
ANNO_BOTTLE="YourBottleName" ./uninstall.sh
```

- `ANNO_BOTTLE="Name"` — use this bottle, skip the menu.
- `ANNO_GAME_DIR="/path/.../Bin/Win64"` — use this exact game folder, skip detection (for unusual custom installs).
- `ANNO_YES=1` — skip the confirmation prompt.

---

## Manual install (if you'd rather not run the script)

The script just automates these four steps. They're the **same for Steam and Ubisoft Connect** — only the game folder's location differs.

**1. Find the game's `Bin\Win64` folder** (the one containing `Anno117.exe`) inside your CrossOver bottle. `…/Bottles/` below is `~/Library/Application Support/CrossOver/Bottles/`.

- **Steam:** `…/Bottles/<YourBottle>/drive_c/Program Files (x86)/Steam/steamapps/common/Anno 117 - Pax Romana/Bin/Win64/`
- **Ubisoft Connect (default):** `…/Bottles/<YourBottle>/drive_c/Program Files (x86)/Ubisoft/Ubisoft Game Launcher/games/Anno 117 - Pax Romana/Bin/Win64/`
- **Custom install folder:** wherever you installed it, ending in `…/Anno 117 - Pax Romana/Bin/Win64/`.

**2. Back up the original AMD DLL.** In that folder, rename the existing `amd_ags_x64.dll` to `amd_ags_orig.dll`.

**3. Copy in the fix.** Copy this repo's `amd_ags_x64.dll` into that same folder. It should now contain, side by side:

```
Anno117.exe        (the game)
amd_ags_x64.dll    ← the fix (from this repo)
amd_ags_orig.dll   ← the original, renamed (backup — the fix forwards to it)
```

**4. Turn on the two Wine DLL overrides** for `Anno117.exe` in that bottle. Easiest is the same `reg` command the installer uses — run it in Terminal (replace `Steam` with your bottle name):

```
WINE="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine"   # or "CrossOver Preview.app"
KEY='HKCU\Software\Wine\AppDefaults\Anno117.exe\DllOverrides'
"$WINE" --bottle "Steam" --wait-children -- reg add "$KEY" /v amd_ags_x64  /t REG_SZ /d 'native,builtin' /f
"$WINE" --bottle "Steam" --wait-children -- reg add "$KEY" /v amd_ags_orig /t REG_SZ /d 'native'          /f
```

Prefer a GUI? In CrossOver, open the bottle → **Wine Configuration → Libraries**, add `amd_ags_x64` set to *native then builtin* and `amd_ags_orig` set to *native*.

---

## What was actually broken (short version)

Anno 117 only knows how to create its display surface one specific way
("composition swapchain"). Apple's translation layer doesn't support that exact
call, so the game dies the instant it tries to show a picture. This fix slips a
tiny helper into the game that quietly rewrites that one unsupported call into
an equivalent one the Mac layer *does* support. Nothing else about the game is
modified.

---

## Known minor issues

- **A handful of "Path not found" messages** appear in the game log for a few
  assets (some names contain `test`, `dev`, `failure`, `pressversion`,
  `empty`, or `dlc01`). These are the game's own references to
  developer/test/DLC assets that aren't part of the shipped data — they happen
  on Windows too and are **not caused by this fix**. The game loads and plays
  fine; at most a stray prop/flag may show a placeholder. Nothing to fix on our
  side.
- **The "outdated graphics driver" popup** appears every launch (harmless — click through it).
- **Performance** under CrossOver/D3DMetal will be lower than a native Windows
  PC with the same GPU; tune graphics settings to taste.

---

## What's in this folder

| File | What it is |
|------|------------|
| `amd_ags_x64.dll` | The fix (a proxy DLL that patches the one unsupported call). |
| `install.sh` | Installs the fix + enables the CrossOver settings. Works for both Steam and Ubisoft Connect. |
| `install-ubisoft.sh` | Same fix, tailored to a direct Ubisoft Connect install (also overrides `Anno117_plus.exe`; defaults to the `Ubisoft Connect` bottle). |
| `uninstall.sh` | Restores the original and removes the settings. |
| `launch-anno117.sh` | Clears leftover processes and launches the game (avoids the black‑screen bug). |
| `src/ags_shim.c`, `src/ags_shim.def` | Source code of the fix, and how to rebuild it. |

### Rebuild the fix from source (optional, for the curious)

Requires the MinGW cross‑compiler (`brew install mingw-w64`):

```
cd src
x86_64-w64-mingw32-gcc -O2 -shared -o ../amd_ags_x64.dll ags_shim.c ags_shim.def -lgdi32 -luser32
```

---

## How it works (technical)

- Anno 117 is **D3D12‑only** and creates its swapchain **exclusively** via
  `IDXGIFactory2::CreateSwapChainForComposition`.
- Apple **D3DMetal** (the only D3D12→Metal path in CrossOver; the DXMT/"dxvk"
  backend exposes no D3D12 adapter for this game) returns **`E_NOTIMPL`
  (0x80004001)** for that call → instant crash.
- The game statically imports `amd_ags_x64.dll` (AMD GPU Services), a plain
  native DLL. We replace it with a **proxy** that:
  1. forwards all 36 real AGS functions to a renamed copy (`amd_ags_orig.dll`), and
  2. from `DllMain`, spawns a thread that creates a throwaway DXGI factory to
     obtain D3DMetal's real shared factory vtable, then patches **slot 24**
     (`CreateSwapChainForComposition`) to a thunk that instead calls **slot 15**
     (`CreateSwapChainForHwnd`) bound to the game's own top‑level window — a
     path D3DMetal fully supports.
- Wine DLL overrides make CrossOver load our native proxy:
  `amd_ags_x64 = native,builtin`, `amd_ags_orig = native`.
- The vtable layout and DXGI interfaces used come from Microsoft's public
  DirectX documentation.

**Do not** disable `dcomp.dll` (DirectComposition) in the bottle — D3DMetal's
adapter enumeration needs it; disabling it causes a "Failed to retrieve the DXGI
adapter" (-1213) startup failure.
