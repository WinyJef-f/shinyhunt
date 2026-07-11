# Building & deploying on macOS

## 1. Install devkitPro / devkitARM

Use the official pacman installer (macOS supported):

```sh
# Install the pacman-based devkitPro setup, then:
sudo dkp-pacman -S 3ds-dev
```

This gives you `devkitARM`, `libctru`, `3ds_rules`, and `3gxtool`. Make sure the
standard environment variables are exported (the installer adds these to your
shell profile — open a new terminal or `source` it):

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=/opt/devkitpro/devkitARM
```

Confirm:

```sh
echo $DEVKITARM        # must be set or the Makefile errors out
which 3gxtool          # used to produce the .3gx
```

## 2. Install CTRPluginFramework (libctrpf)

Add the ThePixellizerOSS package repo and install `libctrpf` per its README, so
the framework headers/libs land at `$DEVKITPRO/libctrpf`:

```sh
# Follow the CTRPluginFramework README to add the repo, then:
sudo dkp-pacman -S libctrpf
ls $DEVKITPRO/libctrpf/include $DEVKITPRO/libctrpf/lib   # sanity check
```

The `Makefile` here looks for `CTRPFLIB ?= $(DEVKITPRO)/libctrpf`. If yours is
elsewhere, override it: `make CTRPFLIB=/path/to/libctrpf`.

## 3. Build this plugin

```sh
cd shinyhunt
make            # produces shinyhunt.3gx (+ shinyhunt.elf) in the repo root
make clean      # remove build artifacts
make re         # clean + build
```

The output `.3gx` is named after the folder (`shinyhunt.3gx`). Rename freely.

> **The one line to watch:** `main.cpp` registers the per-frame loop with
> `menu->Callback(ShinyHunt::Hunter::OnFrame)`. If your libctrpf version names
> that method differently and the build fails there, that is the only place to
> adjust — it just needs to register a `void(*)(void)` that runs every frame.

## 4. Find the Title ID and install

The install folder is the game's **Title ID**, which varies by region/revision —
read it from the console, do **not** assume:

- On the 3DS: **FBI → Titles →** select Pokémon Alpha Sapphire → note the Title ID.

Copy the plugin to the SD card at:

```
sd:/luma/plugins/<TITLEID>/shinyhunt.3gx
```

(Create the `<TITLEID>` folder if needed.) Enable the loader once:
**Rosalina (L+Down+Select) → Plugin Loader → Enabled**. Confirm your Luma3DS is
recent enough that the 3GX loader is built in (it is, on current Luma — no
"loader edition" fork needed). If not, update Luma first.

## 5. Pre-flight (one time)

- Rosalina → **forced volume override → max**, and **save** it so it persists
  across reboots (covers the physical slider for the shiny jingle).
- If you want sound, see `docs/AUDIO.md` (convert the jingle, copy it to the SD
  path in `Config.hpp`, rebuild with sound enabled).

## 6. First boot

Launch the game. Press **L+R** to open the plugin menu. Run the **Diag** entries
(`docs/OPEN_QUESTIONS.md`) before letting the hunt run unattended.
