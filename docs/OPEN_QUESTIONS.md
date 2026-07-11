# Open questions — resolve these FIRST (one short hardware session)

The brief calls out three unknowns to settle before trusting an overnight run.
The plugin ships with **Diag** menu entries so each one is a 2-minute test.

Boot the game with the plugin installed, press **L+R** to open the Rosalina
plugin menu, and you will see the Shiny Hunter entries. Set
`Cfg::kStartHuntOnBoot = false` in `Includes/Config.hpp` (and rebuild) while
you are testing so the loop does not start on its own.

---

## Q1 — Does the console stay awake with the lid closed?

**RESOLVED (negatively) on hardware — `SleepControl` is now disabled by
default (`Cfg::kSleepControlEnabled = false`). Do not re-enable it as-is.**

Retail titles sleep on lid-close. Luma only auto-suppresses that while
InputRedirection / the debugger is active, and we use neither.

**Finding from CI (not hardware, but relevant):** libctru's high-level
`aptSetSleepAllowed()` cannot even be *linked* from a 3GX plugin — it calls
`envGetAptAppId()`, which reads a symbol (`__apt_appid`) that's only populated
by devkitARM's normal homebrew startup path (`crt0` → system init →
`aptInit()`). A plugin injected into an already-running game never takes that
path; its statically-linked libctru is a "shadow" copy that was never
bootstrapped. Confirmed by a real link error:
`undefined reference to '__apt_appid'`.

`SleepControl.cpp` reimplemented the same underlying `APT:U` service calls
directly — same pattern as `Led.cpp` (own `srvGetServiceHandle`, own IPC
command buffer): it queries the **live, currently-active app ID** via
`GetAppletManInfo`, then calls `ReplySleepQuery` with `APTREPLY_REJECT`,
periodically (every ~2s, unconditionally, from the per-frame callback — even
while `Idle`).

**Hardware result: this is actively harmful, not just ineffective.**
On real hardware this produced two reproducible failures:
1. The console did not sleep on lid-close (the reject "worked"), but on
   **lid-open the screen stayed black**, with no recovery except a **hard
   reboot**.
2. **Random crashes during boot / save-load** — i.e. exactly the moments the
   game itself is mid-sequence on its own legitimate `APT:U` IPC calls.

Root cause: `ReplySleepQuery` is meant to be sent *in response to* an actual
pending sleep-query notification the app receives through the normal
`APT:U` protocol sequence (`GetLock` → `ReceiveParameter` → ... → reply).
Firing it **unsolicited**, on the same thread the host game uses for its own
APT session, desyncs that state machine — either by corrupting the wake
handshake (black screen after lid-open) or by colliding with the game's own
in-flight APT transaction during a boot/scene transition (random crash). A
hard-reboot failure mode is unacceptable for an unattended run, so this is
now **off by default**. Leaving it off means the console sleeps normally on
lid-close (safe) and the hunt simply **pauses until the lid is reopened** —
not full "hands-off with the lid closed," but no more hangs/crashes.

**A real fix (not yet implemented)** would need to be notification-driven
instead of blind: register for the actual `APTSIGNAL_SLEEP` event (the
equivalent of libctru's internal `aptHook`/`ReceiveParameter` handling, done
manually via raw IPC the same way `GetActiveAppId` is), and only call
`ReplySleepQuery` in direct response to that specific pending query — never
speculatively. This is a nontrivial rewrite; do not flip
`Cfg::kSleepControlEnabled` back on without it.

**If you want to experiment further anyway:** set `Cfg::kSleepControlEnabled
= true`, rebuild, and expect the two failures above. Useful only for
debugging a proper notification-driven replacement.

> The **backlight turns off** with the lid closed even when asleep — that is
> normal. With `kSleepControlEnabled = false`, the game (and our FSM) simply
> pause during sleep and resume correctly on lid-open.

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

**RESOLVED on hardware:** party slot 1 read species 258 (Mudkip), matching
TID/SID against the trainer card, `valid: YES`, `SHINY: no`. The offset is
correct for this cartridge.

**Bug found and fixed:** the box 1 slot 1 cross-check showed `valid: no` (as
expected — box was empty) but also `SHINY: *** YES ***`, which is wrong. An
empty slot decrypts to species 0 / PID 0, and `shinyValue = 0 ^ 0 ^ 0 ^ 0 =
0`, which is `< 16` — so the shiny flag was computed even though the slot
holds no real Pokémon. This never affected the actual hunt loop (`WaitParty`
in `ShinyHunter.cpp` already gates on `p.valid && p.species ==
kTargetSpecies` before evaluating shininess), but it was misleading in the
diagnostic. Fixed in `PokemonReader.cpp` by gating `out.shiny` on
`out.valid`.
