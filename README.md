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

## Is this for me?

You need **all** of these:

- An **Apple Silicon** Mac (M1/M2/M3/M4/M5…), macOS 15 or newer.
- **CrossOver** installed (CrossOver 26 or the free **CrossOver Preview**). The
  Preview build is recommended — it ships the newest Apple graphics layer
  (D3DMetal 4.0 beta 1) that this fix relies on.
- A **CrossOver bottle** (named **Steam** by default here) with **Steam**
  installed, and **Anno 117: Pax Romana** installed in it through Steam.
- The game must already **download and install** in the bottle (it does — it
  just won't *run* without this fix).

If your bottle isn't named `Steam`, see **"Different bottle name"** near the end.

> All commands below are run in **Terminal** (Applications → Utilities), from
> **inside this folder** — the one you downloaded/cloned this into. `cd` there
> first, e.g. `cd ~/Downloads/anno117-crossover-fix`.

---

## What was actually broken (short version)

Anno 117 only knows how to create its display surface one specific way
("composition swapchain"). Apple's translation layer doesn't support that exact
call, so the game dies the instant it tries to show a picture. This fix slips a
tiny helper into the game that quietly rewrites that one unsupported call into
an equivalent one the Mac layer *does* support. Nothing else about the game is
modified.

(Technical details are at the bottom.)

---

## Install

1. Open **Terminal** and `cd` into this folder.
2. Run:

   ```
   chmod +x *.sh && ./install.sh
   ```

3. You should see `Done.` at the end. That's it.

The installer:
- copies the fix (`amd_ags_x64.dll`) into the game,
- keeps a safe backup of the original file (`amd_ags_orig.dll`),
- turns on the two settings CrossOver needs to load the fix.

It's safe to run again anytime (see **"After a Steam update"**).

---

## Play

You can launch the game **either way**:

**Option A — normal Steam launch.** Just press **Play** in Steam as usual.
⚠️ If you've played once already this session, **clear the leftover Ubisoft
launcher first** (see the black‑screen note below), or the game may open to a
black screen.

**Option B — the helper script (optional, but easier).** It clears those
leftovers for you automatically, so you never hit the black screen:

```
./launch-anno117.sh
```

Both do the same thing — Option B just saves you the manual cleanup step.

### The "graphics driver" popup is normal ✅

Right after the Ubisoft launcher you'll see a warning popup about your **graphics
driver being outdated / unsupported**.

**This is expected and harmless.** D3DMetal reports a "compatibility" GPU that
the game doesn't recognize as current. Just click the button to continue — the
game loads normally. You'll see it every launch.

---

## ⚠️ Black screen when you re‑open the game? (common — easy fix)

If you quit the game and start it again and get a **black screen** (or it just
won't start), it's almost always a **leftover Ubisoft launcher** from the
previous session still running in the background. Steam/Ubisoft then thinks the
game is "already running" and the new copy comes up black.

**Fix — clear the leftovers,** then launch again. Either use `./launch-anno117.sh`
(which does this automatically), or run this once in Terminal:

```
pkill -9 -f "Anno117.exe"; pkill -9 -f "upc.exe"; pkill -9 -f "UplayWebCore"; pkill -9 -f "UbisoftGameLauncher"
```

If you launch with Steam's **Play** button, do this cleanup **before every
re‑launch** in the same session.

---

## Fullscreen

The installer/config already sets **borderless fullscreen at your display's
native resolution**. If you ever want to change it:

- **In game:** Options → Graphics → *Display Mode*. Choose **Borderless**
  (recommended on Mac). Avoid exclusive **Fullscreen** — with this fix,
  borderless behaves best; exclusive fullscreen can misbehave under CrossOver.
- **By hand:** edit
  `…/Anno 117 - Pax Romana/config/engine.ini` inside the bottle, in the
  `"Window"` section:
  - `"FullscreenType": 1`  → 0 = windowed, 1 = borderless (recommended), 2 = exclusive
  - `"ScreenXSize" / "ScreenYSize"` → your display resolution (e.g. 3456 × 2234)
  - `"NoWindowFrame": true`, `"MaximizedWindow": true`

---

## After a Steam update (important)

A Steam **game update**, or **Verify integrity of game files**, will overwrite
the fix with Ubisoft's original file and the game will crash again. Just
**re‑run the installer** from this folder:

```
./install.sh
```

---

## Uninstall / revert

```
./uninstall.sh
```

This restores the original game file and removes the two CrossOver settings.

---

## Known minor issues

- **A handful of "Path not found" messages** appear in the game log for a few
  assets (some names contain `test`, `dev`, `failure`, `pressversion`,
  `empty`, or `dlc01`). These are the game's own references to
  developer/test/DLC assets that aren't part of the shipped data — they happen
  on Windows too and are **not caused by this fix**. The game loads and plays
  fine; at most a stray prop/flag may show a placeholder. Nothing to fix on our
  side.
- **The "outdated graphics driver" popup** appears every launch (harmless — see
  above).
- **Performance** under CrossOver/D3DMetal will be lower than a native Windows
  PC with the same GPU; tune graphics settings to taste.

---

## Different bottle name

If your Steam bottle isn't called `Steam`, pass its name to every script:

```
ANNO_BOTTLE="YourBottleName" ./install.sh
ANNO_BOTTLE="YourBottleName" ./launch-anno117.sh
ANNO_BOTTLE="YourBottleName" ./uninstall.sh
```

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
