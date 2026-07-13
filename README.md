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
- **Anno 117: Pax Romana** purchased and installed through Steam
- **CrossOver** installed (you can get the free **CrossOver Preview** from [CodeWeavers](https://www.codeweavers.com/crossover/preview) — this is the recommended version)

If you don't have CrossOver yet, download it first and set up a Steam bottle with Anno 117 installed (CrossOver will walk you through this).

### Step 1: Download this fix

Download the latest release from [here](https://github.com/maxhenkentech/anno117-mac-crossover/releases/latest).

This will download a `.zip` file to your **Downloads** folder. Double-click it to extract it — you'll get a folder called `anno117-mac-crossover`.

### Step 2: Install the fix

1. Open **Terminal** (press `⌘ Space`, type "Terminal", press Enter)
2. Copy and paste the following command, then press Enter:

   ```
   cd ~/Downloads/anno117-mac-crossover && chmod +x *.sh && ./install.sh
   ```

3. You should see `Done.` at the end. That's it — the fix is installed!

> **Note:** If you saved the folder somewhere other than Downloads, replace `~/Downloads/` with the path where you extracted it. For example, if it's on your Desktop: `cd ~/Desktop/anno117-mac-crossover && chmod +x *.sh && ./install.sh`

### Step 3: Play the game

The easiest way to launch is with the included helper script, which prevents common issues:

```
./launch-anno117.sh
```

You can also just press **Play** in Steam normally. (But if you've played once already today, use the script above — see the troubleshooting section below.)

**First launch:** After the Ubisoft launcher appears, you'll see a popup about your **graphics driver being outdated**. This is **normal and harmless** — just click through it and the game will load.

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

### Game stopped working after a Steam update?

If Steam updated the game (or you clicked "Verify integrity of game files"), the fix was overwritten. Just reinstall it:

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
- **By hand:** Edit `…/Anno 117 - Pax Romana/config/engine.ini` inside the bottle, in the `"Window"` section:
  - `"FullscreenType": 1` → 0 = windowed, 1 = borderless (recommended), 2 = exclusive
  - `"ScreenXSize"` / `"ScreenYSize"` → your display resolution (e.g. 3456 × 2234)
  - `"NoWindowFrame": true`, `"MaximizedWindow": true`

---

## Different bottle name?

If your CrossOver bottle isn't called `Steam`, pass its name to any command:

```
ANNO_BOTTLE="YourBottleName" ./install.sh
ANNO_BOTTLE="YourBottleName" ./launch-anno117.sh
ANNO_BOTTLE="YourBottleName" ./uninstall.sh
```

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
| `install.sh` | Installs the fix + enables the CrossOver settings. |
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
