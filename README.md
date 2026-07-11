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

This plugin has now been run on hardware (New 3DS, ORAS). Party read/decrypt
and the LED are confirmed working. **Lid-close keep-awake is confirmed
harmful and is disabled by default** — see the table below and
`docs/OPEN_QUESTIONS.md` Q1.

| Area | Confidence | Notes |
|------|-----------|-------|
| Gen 6 PK6 read + decrypt + shiny math | **Confirmed on hardware** | `PokemonReader.cpp`; party slot 1 read a real Mudkip, checksum valid |
| ORAS party slot-1 address `0x8CFB26C` | **Confirmed on hardware** (Q3) | matched species + trainer TID/SID |
| Input injection model (`InjectKey`, per-frame) | **High** — from CTRPF `Controller.cpp` | timing needs tuning |
| LED via `ptm:sysm` | **Confirmed on hardware** (Q2) | reachable, `SetInfoLedPattern` returned success, LED visually solid yellow |
| Keep-awake with lid closed | **Confirmed harmful — disabled by default** (Q1) | caused black-screen hangs (hard reboot required) + random crashes; console now sleeps normally on lid-close, hunt pauses until lid-open — see `docs/OPEN_QUESTIONS.md` |
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
   the menu. Leave the lid **open** (or propped) for a true unattended run
   until Q1 (lid-close keep-awake) has a working fix — closing the lid
   currently just pauses the hunt safely until you reopen it.

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
unattended without anyone touching the console. Lid-close keep-awake is
currently **disabled by default** (see Status table above) — the console
sleeps normally when the lid closes, and the loop resumes automatically on
lid-open rather than running through a closed lid.

## Layout

```
Makefile, 3gx.ld, ShinyHunter.plgInfo   build system (from the CTRPF template)
Includes/Config.hpp                      ALL tunables (addresses, timings, threshold)
Includes|Sources/PokemonReader.*         party read + Gen6 decrypt + shiny
Includes|Sources/InputSim.*              InjectKey wrapper + starter input script
Includes|Sources/Led.*                   ptm:sysm SetInfoLedPattern (solid yellow)
Includes|Sources/Sound.*                 CTRPF Sound wrapper (guarded, off by default)
Includes|Sources/SleepControl.*          raw APT:U keep-awake (disabled by default, see docs/OPEN_QUESTIONS.md)
Includes|Sources/ShinyHunter.*           the FSM + on-screen diagnostics
Sources/main.cpp                         plugin entry, menu, callback registration
assets/emerald_0066.wav                  your shiny jingle (needs conversion, see AUDIO.md)
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
