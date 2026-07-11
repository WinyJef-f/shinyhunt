# Building & deploying on macOS (native — no Docker)

Everything here runs natively on macOS (Intel or Apple Silicon). The 3DS
compiler (`devkitARM`) and `libctrpf` install cleanly via devkitPro pacman; the
only piece you build yourself is `3gxtool`, and the instructions below use the
**maintained** source with the exact steps that avoid the common failures.

## 0. Prerequisites

```sh
xcode-select --install        # git + make + a C/C++ toolchain
# Install Homebrew if you don't have it: https://brew.sh
brew install cmake            # needed to build 3gxtool
```

Then install **devkitPro pacman** with the official macOS graphical installer
(`devkitpro-pacman` .pkg from devkitpro.org). After it finishes:

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=/opt/devkitpro/devkitARM
# (the installer normally adds these to your shell profile; open a new terminal
#  or `source ~/.zshrc` so they're set)
echo $DEVKITARM               # must print a path, or the plugin Makefile errors
```

## 1. Install the 3DS toolchain

```sh
sudo dkp-pacman -S 3ds-dev
```

This gives you `devkitARM`, `libctru`, and `$DEVKITARM/3ds_rules`.
(Note: this does **not** include `3gxtool` — that's step 3.)

## 2. Install CTRPluginFramework (libctrpf)

`libctrpf` lives in ThePixellizerOSS's own pacman repo, which you add once.

> The Mac/Linux `dkp-pacman` config is at
> **`/opt/devkitpro/pacman/etc/pacman.conf`** — NOT `/opt/devkitpro/pacman.conf`.
> Writing to the wrong path is why `-Sy` "succeeds" but `-S libctrpf` then says
> *target not found*. If unsure, locate it: `find /opt/devkitpro -name pacman.conf`.

Add the repo (guarded so it's safe to run twice), then install:

```sh
if ! grep -Fxq "[thepixellizeross-lib]" /opt/devkitpro/pacman/etc/pacman.conf; then \
  printf '\n[thepixellizeross-lib]\nServer = https://thepixellizeross.gitlab.io/packages/any\nSigLevel = Optional\n' \
  | sudo tee -a /opt/devkitpro/pacman/etc/pacman.conf > /dev/null; fi

sudo dkp-pacman -Sy            # you should now see a "thepixellizeross-lib" line
sudo dkp-pacman -S libctrpf
```

Sanity check the headers/lib landed:

```sh
ls $DEVKITPRO/libctrpf/include $DEVKITPRO/libctrpf/lib
```

The plugin `Makefile` looks for `CTRPFLIB ?= $(DEVKITPRO)/libctrpf`. If yours is
elsewhere, pass it: `make CTRPFLIB=/path/to/libctrpf`.

## 3. Build `3gxtool` (the .3gx packer)

Use the **maintained ThePixellizerOSS source with CMake** — do NOT use the old
`Nanquitas/3gxtool` Makefile repo (that one ships a Windows-only prebuilt
`libyaml-cpp.a` and Windows-only linker flags, which is what produced the
`types.hpp not found` / `file format not recognized` errors).

```sh
git clone --recursive https://gitlab.com/thepixellizeross/3gxtool.git
cd 3gxtool
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Why this works where the earlier attempts didn't:
- `--recursive` pulls the `dynalo` + `yaml-cpp` submodules (fixes the missing
  headers).
- CMake **builds yaml-cpp from source for your Mac** (fixes the "file format not
  recognized" from the vendored Windows `.a`).
- No `-static` flags (those are Windows/Linux-only and break Apple's linker).

Find the built binary and put it on your PATH:

```sh
BIN=$(find build -type f -name '3gxtool*' -perm +111 | head -n1); echo "$BIN"
sudo cp "$BIN" /opt/devkitpro/tools/bin/3gxtool
sudo chmod +x /opt/devkitpro/tools/bin/3gxtool
which 3gxtool && 3gxtool --help    # confirm it runs
```

## 4. Build this plugin

```sh
cd /path/to/shinyhunt
make            # produces shinyhunt.3gx (+ shinyhunt.elf) in the repo root
# make clean   # remove build artifacts
# make re      # clean + rebuild
```

> **One line to watch:** `main.cpp` registers the per-frame loop with
> `menu->Callback(ShinyHunt::Hunter::OnFrame)`. If your libctrpf version renames
> that method and the build stops there, that's the only line to adjust — it just
> needs to register a `void(*)(void)` called every frame.

## 5. Find the Title ID and install

The install folder is the game's **Title ID**, which varies by region/revision —
read it from the console, do **not** assume:

- On the 3DS: **FBI → Titles →** Pokémon Alpha Sapphire → note the Title ID.

Copy the plugin to the SD card:

```
sd:/luma/plugins/<TITLEID>/shinyhunt.3gx
```

Create the `<TITLEID>` folder if needed, then enable the loader once:
**Rosalina (L+Down+Select) → Plugin Loader → Enabled**. Current Luma3DS has the
3GX loader built in (no "loader edition" fork). If yours is older than ~v10.3,
update Luma first.

## 6. Pre-flight (one time)

- Rosalina → **forced volume override → max**, and **save** it so it persists
  across reboots.
- Optional sound: see `docs/AUDIO.md` (convert the jingle, copy it to the SD path
  in `Config.hpp`, rebuild with sound enabled).

## 7. First boot

Launch the game, press **L+R** to open the plugin menu, and run the **Diag**
entries (`docs/OPEN_QUESTIONS.md`) before letting the hunt run unattended.

---

## Troubleshooting (the exact errors seen so far)

| Symptom | Cause | Fix |
|---|---|---|
| `dkp-pacman -S libctrpf` → *target not found* | repo written to the wrong `pacman.conf` | use `/opt/devkitpro/pacman/etc/pacman.conf` (step 2), re-run `-Sy` |
| `3gxtool ... types.hpp: No such file` | cloned the old repo / no submodules | use the GitLab repo with `git clone --recursive` + CMake (step 3) |
| `libyaml-cpp.a: file format not recognized` | vendored Windows static lib | CMake build from source (step 3) compiles yaml-cpp for macOS |
| `make` can't find `3gxtool` | not on PATH | copy the built binary into `/opt/devkitpro/tools/bin/` (step 3) |
| plugin `make` errors at `menu->Callback` | libctrpf API name differs | see the "one line to watch" note above |

If `3gxtool` **builds but crashes/segfaults when packing** (a bug reported for one
older *fork* on macOS — not expected from the CMake build above): paste me the
exact command + output and I'll get you a working binary. The heavy part (the
devkitARM compile) is always native and fine; only this final packing step could
ever need attention.
