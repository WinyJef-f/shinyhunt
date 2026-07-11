# Building & deploying on Windows

Windows is CTRPluginFramework's primary platform, so this is the most trodden
path. Everything happens inside the **devkitPro MSYS2 shell**.

Confidence notes are inline: steps marked **[verified]** are quoted from the
CTRPF project's own README; the one step I could not fetch a fixed URL for
(the prebuilt `3gxtool.exe`) is marked **[grab latest]** with a from-source
fallback so you are never stuck.

## 0. Install devkitPro

Run the official **Windows graphical installer** (`devkitProUpdater-*.exe` from
`github.com/devkitPro/installer/releases`). Accept the 3DS development component.
It installs MSYS2 at `C:\devkitPro\msys2`, plus devkitARM and pacman, and sets
`DEVKITPRO` / `DEVKITARM` for you. **[verified]**

Open the devkitPro MSYS2 shell for every step below:
- Start-menu shortcut **"MSYS2"** (installed by devkitPro), or
- run `C:\devkitPro\msys2\msys2_shell.bat`. **[verified]**

Inside that shell, `pacman` is already the devkitPro pacman (not `dkp-pacman`),
and `$DEVKITARM` is already set — check with `echo $DEVKITARM`.

## 1. Install the 3DS toolchain

```sh
pacman -S 3ds-dev
```

Gives devkitARM, libctru, and `3ds_rules`. **[verified]**

## 2. Install CTRPluginFramework (libctrpf)

Add ThePixellizerOSS's package repos to `/etc/pacman.conf`. This is the **exact
command from CTRPF's README** (adds both the arch-independent lib repo and the
Windows repo): **[verified]**

```sh
if ! grep -Fxq "[thepixellizeross-lib]" /etc/pacman.conf; then echo -e "\n[thepixellizeross-lib]\nServer = https://thepixellizeross.gitlab.io/packages/any\nSigLevel = Optional" | tee -a /etc/pacman.conf > /dev/null; fi; if ! grep -Fxq "[thepixellizeross-win]" /etc/pacman.conf; then echo -e "\n[thepixellizeross-win]\nServer = https://thepixellizeross.gitlab.io/packages/x86_64/win\nSigLevel = Optional" | tee -a /etc/pacman.conf > /dev/null; fi
```

> On Windows MSYS2, the config is `/etc/pacman.conf` (i.e.
> `C:\devkitPro\msys2\etc\pacman.conf`). This is different from macOS/Linux
> dkp-pacman, which uses `/opt/devkitpro/pacman/etc/pacman.conf`.

Then update and install: **[verified]**

```sh
pacman -Sy          # should now list "thepixellizeross-lib" / "-win"
pacman -S libctrpf
ls $DEVKITPRO/libctrpf/include $DEVKITPRO/libctrpf/lib   # sanity check
```

## 3. Get `3gxtool` (the .3gx packer)

`3gxtool` is a Windows host program you place on your PATH. Two ways:

### Option A — prebuilt binary (fastest) **[grab latest]**

1. Go to the **ThePixellizerOSS/3gxtool** releases page on GitLab
   (`gitlab.com/thepixellizeross/3gxtool/-/releases`).
2. Download the latest release's Windows `3gxtool.exe`.
3. Put it at `C:\devkitPro\tools\bin\3gxtool.exe` (that folder is already on the
   MSYS2 PATH).
4. Verify in the MSYS2 shell: `3gxtool --help`

> Use a **recent** release (v1.2 or newer). Do **not** use the ancient 2018
> `Nanquitas/3gxtool` build — it predates the YAML `.plgInfo` format this repo
> uses and will fail to parse `ShinyHunter.plgInfo`.

### Option B — build from source (if no prebuilt you trust)

In the MSYS2 shell:

```sh
pacman -S git cmake gcc make        # if not already present
git clone --recursive https://gitlab.com/thepixellizeross/3gxtool.git
cd 3gxtool
cmake -B build -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
cp build/3gxtool.exe /c/devkitPro/tools/bin/3gxtool.exe
3gxtool --help
```

`--recursive` pulls the `dynalo` + `yaml-cpp` submodules; CMake builds yaml-cpp
from source, so there is no prebuilt-library mismatch.

## 4. Build this plugin

In the MSYS2 shell, navigate to the repo (Windows drives are `/c/...`) and run
make:

```sh
cd /c/Users/<you>/path/to/shinyhunt
make                # produces shinyhunt.3gx (+ shinyhunt.elf)
```

> **One line to watch:** `main.cpp` uses
> `menu->Callback(ShinyHunt::Hunter::OnFrame)`. If your libctrpf renames that
> and the build stops there, that's the only line to adjust — it just needs to
> register a `void(*)(void)` called every frame.

## 5. Find the Title ID and install

Read the game's **Title ID** from the console (FBI → Titles → Alpha Sapphire) —
don't assume it. Copy the plugin to the SD card:

```
sd:/luma/plugins/<TITLEID>/shinyhunt.3gx
```

Enable the loader once: **Rosalina (L+Down+Select) → Plugin Loader → Enabled**
(built into current Luma3DS; update Luma if older than ~v10.3).

## 6. Pre-flight + first boot

- Rosalina → **forced volume override → max**, and **save**.
- Optional sound: `docs/AUDIO.md`.
- Launch the game, press **L+R** for the plugin menu, and run the **Diag**
  entries (`docs/OPEN_QUESTIONS.md`) before an unattended run.

---

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `pacman -S libctrpf` → *target not found* | repos not added / not synced | re-run the step-2 command, then `pacman -Sy` |
| `3gxtool` not found by `make` | not on PATH | put `3gxtool.exe` in `C:\devkitPro\tools\bin\` |
| `3gxtool` fails to parse `ShinyHunter.plgInfo` | old 2018 binary | use a v1.2+ build (step 3) |
| `make` errors at `menu->Callback` | libctrpf API name differs | see the "one line to watch" note |
| `cmake` picks MSVC / wrong generator | default generator | pass `-G "MSYS Makefiles"` as shown |
