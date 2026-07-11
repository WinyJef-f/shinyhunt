# shinyhunt — Autonomous Shiny Starter Bot (Pokémon Alpha Sapphire, 3DS)

A self-contained Luma3DS **3GX plugin** that runs inside Pokémon Alpha Sapphire
(ORAS) on a modded New 3DS. While active it soft-resets the game, picks the
Mudkip starter, reads party slot 1 straight from process memory, checks
shininess, and repeats **fully unattended** — indefinitely, with the lid closed.
On a shiny hit it locks the notification **LED solid yellow** and plays a shiny
jingle, then holds that state so you can find the console by the LED alone.

No PC, no InputRedirection, no network at runtime. A Mac is used only to build
and deploy.

---

## Status — read this first

This plugin was written against the documented APIs of CTRPluginFramework,
Gen6CTRPluginFramework, PKMN-NTR and ctr-led-brary. **It has not been run on
hardware from here** (that requires your console). The design deliberately
front-loads the three open questions from the brief and isolates every
hardware-dependent value so you can close them quickly.

| Area | Confidence | Notes |
|------|-----------|-------|
| Gen 6 PK6 read + decrypt + shiny math | **High** — cross-checked vs PKHeX/Gen6CTRPF | `PokemonReader.cpp` |
| ORAS party slot-1 address `0x8CFB26C` | **High** — from PKMN-NTR + Project Pokémon RAM research | verify on your cart (Q3) |
| Input injection model (`InjectKey`, per-frame) | **High** — from CTRPF `Controller.cpp` | timing needs tuning |
| LED via `ptm:sysm` | **Medium** — call is correct; **in-process access is Q2** | has diagnostic + fallback |
| Keep-awake with lid closed | **Medium** — `aptSetSleepAllowed(false)`; **that is Q1** | verify with a lid test |
| Starter input *timing/sequence* | **Needs calibration** | `InputSim.cpp` script |
| Shiny jingle playback | **Off by default** | enable after confirming the Sound API — see `docs/AUDIO.md` |

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
6. **(Optional) enable sound** → `docs/AUDIO.md`
7. Save **standing right in front of the starter bag**, start the hunt, close
   the menu, close the lid.

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

Because the inputs are **software-injected** (not physical buttons) and sleep is
suppressed, the whole loop keeps running with the lid closed.

## Layout

```
Makefile, 3gx.ld, ShinyHunter.plgInfo   build system (from the CTRPF template)
Includes/Config.hpp                      ALL tunables (addresses, timings, threshold)
Includes|Sources/PokemonReader.*         party read + Gen6 decrypt + shiny
Includes|Sources/InputSim.*              InjectKey wrapper + starter input script
Includes|Sources/Led.*                   ptm:sysm SetInfoLedPattern (solid yellow)
Includes|Sources/Sound.*                 CTRPF Sound wrapper (guarded, off by default)
Includes|Sources/SleepControl.*          aptSetSleepAllowed keep-awake
Includes|Sources/ShinyHunter.*           the FSM + on-screen diagnostics
Sources/main.cpp                         plugin entry, menu, callback registration
assets/emerald_0066.wav                  your shiny jingle (needs conversion, see AUDIO.md)
docs/BUILD_MAC.md | BUILD_WINDOWS.md      native toolchain setup + build + deploy
docs/OPEN_QUESTIONS.md                    the three hardware tests to run first
docs/HARDWARE_CALIBRATION.md              offset verify + input-timing tuning
docs/EMULATOR.md                          what a 3DS emulator can/can't do for this
docs/AUDIO.md                             jingle format + conversion
```

> **Can I develop this entirely in a 3DS emulator?** No — emulators don't run
> Luma3DS's plugin loader, so the `.3gx` can't load there. An emulator is still
> useful on the Mac for working out the input choreography and validating the
> decryption against real game data; see `docs/EMULATOR.md`.

## Credits / references

- CTRPluginFramework (ThePixellizerOSS / Nanquitas) — the plugin runtime.
- Gen6CTRPluginFramework (biometrix76) — PK6 decryption + ORAS box base.
- PKMN-NTR (drgoku282 / fa-dx) — ORAS party/box RAM offsets.
- ctr-led-brary (PabloMK7) — `ptm:sysm SetInfoLedPattern` LED control.
- Project Pokémon RAM research — ORAS address cross-checks.
