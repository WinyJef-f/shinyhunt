# shinyhunt — Autonomous Shiny Starter Bot (Pokémon Alpha Sapphire, 3DS)

A self-contained Luma3DS **3GX plugin** that runs inside Pokémon Alpha Sapphire
(ORAS) on a modded New 3DS. While active it soft-resets the game, picks the
Mudkip starter, reads party slot 1 straight from process memory, checks
shininess, and repeats **fully unattended, with the lid open** (see
`docs/OPEN_QUESTIONS.md` Q1 for why lid-closed operation isn't currently
possible). On a shiny hit it locks the notification **LED solid yellow** and
plays a shiny jingle, then holds that state so you can find the console by
the LED alone.

No PC, no InputRedirection, no network at runtime. A Mac is used only to build
and deploy.

---

## Status — read this first

This plugin has now been run on hardware (New 3DS, ORAS) across multiple test
sessions. Party read/decrypt and the LED are confirmed working, and sound is
implemented. **There is no way to keep the console awake with the lid
closed** — it's a hardware Hall sensor, not a software setting (see
`docs/OPEN_QUESTIONS.md` Q1). To avoid the crashes that came from a hunt
being active through a sleep/HOME/app-swap transition, the hunt now **stops
itself the instant any of those begins**; restart it from the menu
afterwards. **Run with the lid open.**

| Area | Confidence | Notes |
|------|-----------|-------|
| Gen 6 PK6 read + decrypt + shiny math | **Confirmed on hardware** | `PokemonReader.cpp`; party slot 1 read a real Mudkip, checksum valid |
| ORAS party slot-1 address `0x8CFB26C` | **Confirmed on hardware** (Q3) | matched species + trainer TID/SID |
| Input injection model (`InjectKey`, per-frame) | **High** — from CTRPF `Controller.cpp` | timing needs tuning |
| LED via `ptm:sysm` | **Confirmed on hardware** (Q2) | reachable, `SetInfoLedPattern` returned success, LED visually solid yellow |
| Lid-closed sleep | **Not possible** (Q1) | hardware Hall sensor, no software override exists; run with the lid open |
| Sleep / HOME / app-swap while hunting | **Mitigated** (Q1) | it crashed (heap corruption in newlib `_free_r`, 4 hardware dumps); the hunt now auto-stops the moment any transition begins so the FSM is inert through it |
| Shiny jingle playback | **Implemented** | plays the converted `emerald_0066` clip; copy one file to the SD — see `docs/AUDIO.md` |
| Starter input *timing/sequence* | **Needs calibration** | `InputSim.cpp` script — the main untested piece |

The **LED is the primary, always-on indicator**; sound is a bonus.

> **Shiny threshold:** ORAS treats `shinyVal < 16` as shiny. The brief quoted
> `< 8` (the Gen 3 rule); using 8 would reset away real ORAS shinies with a
> value of 8–15. The default is therefore **16** (`Cfg::kShinyThreshold`).
> Change it in `Includes/Config.hpp` if you truly want the stricter subset.

---

## Do this, in order

1. **Build** → `docs/BUILD_MAC.md` (macOS) or `docs/BUILD_WINDOWS.md` (Windows)
2. **Read your cartridge's Title ID** (FBI → Titles) and install to
   `sd:/luma/plugins/<TITLEID>/` → `docs/BUILD_MAC.md`
3. **One-time pre-flight:** Rosalina → set **forced volume to max** and save.
4. **Resolve the open questions on hardware** using the built-in
   **Diag** menu entries → `docs/OPEN_QUESTIONS.md`
5. **Calibrate** the party offset check + starter input timing →
   `docs/HARDWARE_CALIBRATION.md`
6. **Sound** (optional, one file copy) → `docs/AUDIO.md`
7. Save **standing right in front of the starter bag**. The hunt is stopped
   by default on every boot — start it from the L+R menu when you're ready.
   Keep the lid **open**; see `docs/OPEN_QUESTIONS.md` Q1 for why lid-closed
   operation isn't possible. Entering sleep / the HOME menu automatically
   stops the hunt (restart it afterwards).

---

## How it works

`main.cpp` registers a per-frame callback (`Hunter::OnFrame`) that runs a small
state machine even while the game plays normally and the menu is closed:

```
SoftReset  -> hold L+R+Start for a few frames
PostReset  -> wait for the title screen + save load
RunScript  -> injected inputs: clear dialog, open bag, move to Mudkip, confirm
WaitParty  -> poll party slot 1 (0x8CFB26C); mash A through "received!" dialog
              accept only a VALID, freshly-decrypted Mudkip
Evaluate   -> shinyVal = TID ^ SID ^ (PID>>16) ^ (PID & 0xFFFF)
                 < threshold ? -> ShinyHold : -> SoftReset
ShinyHold  -> LED solid yellow (re-asserted), jingle x5, hold forever
```

Inputs are **software-injected** (not physical buttons), so the loop runs
unattended without anyone touching the console. `main.cpp` also registers
`Hunter::OnProcessEvent` via `Process::SetProcessEventCallback`: on
`SLEEP_ENTER` / `HOME_ENTER` / `SWAP_ENTER` it **stops the hunt** so the FSM
is completely inert (no input injection, no memory reads) through the
transition — doing anything during those windows crashed on hardware (see
`docs/OPEN_QUESTIONS.md` Q1). Restart the hunt from the menu afterwards.

## Layout

```
Makefile, 3gx.ld, ShinyHunter.plgInfo   build system (from the CTRPF template)
Includes/Config.hpp                      ALL tunables (addresses, timings, threshold)
Includes|Sources/PokemonReader.*         party read + Gen6 decrypt + shiny
Includes|Sources/InputSim.*              InjectKey wrapper + starter input script
Includes|Sources/Led.*                   ptm:sysm SetInfoLedPattern (solid yellow)
Includes|Sources/Sound.*                 CTRPF Sound wrapper (BCWAV jingle, loaded from SD)
Includes|Sources/ShinyHunter.*           the FSM + diagnostics + stop-on-sleep/HOME/swap
Sources/main.cpp                         plugin entry, menu, callback registration
assets/emerald_0066.wav                  source jingle (RIFF WAV)
assets/make_jingle.py                    WAV -> BCWAV converter (pure python, no deps)
assets/shiny_jingle.bcwav                the built jingle the plugin loads (copy to SD)
docs/BUILD_MAC.md | BUILD_WINDOWS.md      native toolchain setup + build + deploy
docs/OPEN_QUESTIONS.md                    the three hardware tests to run first
docs/HARDWARE_CALIBRATION.md              offset verify + input-timing tuning
docs/AUDIO.md                             jingle format + conversion
```

## Credits / references

- CTRPluginFramework (ThePixellizerOSS / Nanquitas) — the plugin runtime.
- Gen6CTRPluginFramework (biometrix76) — PK6 decryption + ORAS box base.
- PKMN-NTR (drgoku282 / fa-dx) — ORAS party/box RAM offsets.
- ctr-led-brary (PabloMK7) — `ptm:sysm SetInfoLedPattern` LED control.
- Project Pokémon RAM research — ORAS address cross-checks.
