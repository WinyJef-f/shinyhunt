# Open questions — resolve these FIRST (one short hardware session)

The brief calls out three unknowns to settle before trusting an overnight run.
The plugin ships with **Diag** menu entries so each one is a 2-minute test.

Boot the game with the plugin installed, press **L+R** to open the Rosalina
plugin menu, and you will see the Shiny Hunter entries. Set
`Cfg::kStartHuntOnBoot = false` in `Includes/Config.hpp` (and rebuild) while
you are testing so the loop does not start on its own.

---

## Q1 — Does the console stay awake with the lid closed?

Retail titles sleep on lid-close. Luma only auto-suppresses that while
InputRedirection / the debugger is active, and we use neither. The plugin calls
`aptSetSleepAllowed(false)` at start and **re-asserts it every frame**
(`SleepControl::Reassert()` in the FSM), which is how music players stay alive
lid-closed.

**Test:**
1. Start the hunt (or just leave the plugin loaded — keep-awake is asserted
   whenever the FSM runs; for a pure sleep test, start the hunt).
2. Close the lid for ~60 seconds, then open it.
3. Watch the on-screen attempt counter / OSD notifications.

- **Pass:** the attempt count advanced while the lid was shut → nothing else to do.
- **Fail (it slept):** the count is frozen and the game resumes only on lid-open.
  Then: confirm `SleepControl::KeepAwake()` runs (it is called from
  `Hunter::Start`). If it still sleeps, the game is overriding APT harder than
  expected; the fallback is to also block the shell-close notification — see the
  note at the bottom of `SleepControl.cpp` and open an issue with what you see.

> Even when awake, the **backlight turns off** with the lid closed — that is
> normal and fine. The game logic and our injected inputs keep running.

---

## Q2 — Can we set the LED from inside the game process?

`ptm:sysm` is privileged. A plugin lives inside ORAS's process, whose service
ACL may or may not include `ptm:sysm`. `Led::SetSolid()` acquires the handle
with `srvGetServiceHandle("ptm:sysm")` and returns the `Result`.

**Test:** run **Diag: test LED (solid yellow)**. The dialog reports:
- `ptm:sysm reachable: YES/NO`
- `SetInfoLedPattern result: 0x……`

- **Pass:** reachable = YES, result `0x00000000`, LED glows solid yellow. Done —
  the shiny alert will work.
- **Fail:** reachable = NO (handle denied). Fallbacks, in order of effort:
  1. Some setups allow the handle anyway via Luma's service manager; confirm your
     Luma is current.
  2. **Proxy through Rosalina** (the brief's suggested fallback): Rosalina is a
     sysmodule with full access. Route the 100-byte pattern to it instead of
     calling `ptm:sysm` directly. This is the one piece most likely to need real
     trial-and-error; keep the LED call isolated in `Led.cpp` so only that file
     changes.
  3. Worst case, the hunt still *works* — it just cannot signal via LED; you would
     rely on the on-screen "SHINY!" state and (if enabled) the jingle.

---

## Q3 — Do the party offsets hold for THIS cartridge/region?

The party slot-1 address `0x8CFB26C` and the PK6 decryption are cross-checked
against multiple sources, but verify before trusting it overnight.

**Test (with a known Pokémon in your party — ideally one you know is NOT shiny):**
1. Load a save that has Pokémon in the party.
2. Run **Diag: read party + box now**.
3. Compare the reported **party slot 1** species / TID / SID against what you see
   in-game (your TID/SID are on the Trainer Card).

- **Pass:** species matches your slot-1 Pokémon, TID/SID match your trainer card,
  `valid(decrypt+checksum): YES`, and `SHINY: no` for a Pokémon that is not shiny
  by eye. The offsets are correct.
- **Fail:** `valid: no` or nonsense species/IDs. The static address differs for
  your revision. Fix `Cfg::kPartySlot1Addr` in `Includes/Config.hpp`:
  - The dialog also reads **box 1 slot 1** at `0x8C9E134` as a cross-check. If the
    box reads correctly but the party does not, only the party address is off.
  - See `docs/HARDWARE_CALIBRATION.md` for how to relocate it.

> Do this with a **known non-shiny** Pokémon so a `SHINY: no` result proves the
> math is right, not just that it read *something*.
