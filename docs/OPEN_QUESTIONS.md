# Open questions — resolve these FIRST (one short hardware session)

The brief calls out three unknowns to settle before trusting an overnight run.
The plugin ships with **Diag** menu entries so each one is a 2-minute test.

Boot the game with the plugin installed, press **L+R** to open the Rosalina
plugin menu, and you will see the Shiny Hunter entries. Set
`Cfg::kStartHuntOnBoot = false` in `Includes/Config.hpp` (and rebuild) while
you are testing so the loop does not start on its own.

---

## Q1 — Does the console stay awake with the lid closed?

**RESOLVED — the plugin no longer tries to suppress sleep at all. It uses
`Process::SetProcessEventCallback` to pause/resume around the transition
instead.** This required two iterations; both are recorded below because the
second one only makes sense in light of what the first one got wrong.

### Attempt 1 (removed): blind `APT:U ReplySleepQuery` — actively harmful

The first attempt reimplemented `APT:U`'s `ReplySleepQuery` via raw IPC
(`srvGetServiceHandle("APT:U")`, own command buffer — the same low-level
pattern `Led.cpp` uses for `ptm:sysm`), called **unconditionally, every
~2 seconds, from the per-frame callback, on the same thread the host game
uses for its own APT session** — even while `Idle`.

Libctru's high-level `aptSetSleepAllowed()` couldn't even be linked in the
first place (`undefined reference to '__apt_appid'` — it depends on a global
only populated by the normal homebrew `crt0` → `aptInit()` path, which an
injected 3GX plugin never takes), which is what motivated writing a
replacement by hand instead of just calling the libctru function.

**Hardware result: this was actively harmful, not just ineffective.**
1. The console didn't sleep on lid-close, but on lid-open **the screen stayed
   black**, recoverable only by a **hard reboot**.
2. **Random crashes during boot / save-load** — exactly the moments the game
   itself is mid-sequence on its own legitimate `APT:U` calls.

`ReplySleepQuery` is meant to be sent *in response to* an actual pending
sleep-query notification, not fired speculatively. Doing so unsolicited, on
the game's own thread, desynced APT's state machine.

### Attempt 2 (shipped): pause/resume via `Process::SetProcessEventCallback`

Reading CTRPluginFramework's own source (`Library/source/pluginInit.cpp` in
the [upstream repo](https://gitlab.com/thepixellizeross/ctrpluginframework))
showed the framework already owns sleep/HOME/swap handling correctly, on its
**own dedicated thread** (`KeepThreadMain`), through Luma's plugin-loader
(`plgldr`) event protocol (`PLG_SLEEP_ENTRY/EXIT`, `PLG_HOME_ENTER/EXIT`,
`PLG_ABOUT_TO_SWAP`) — not raw `APT:U` calls at all. It exposes this to
plugin code through a public, documented API:

```cpp
// Library/include/CTRPluginFramework/System/Process.hpp
enum class Event { EXIT, SLEEP_ENTER, SLEEP_EXIT, HOME_ENTER, HOME_EXIT, SWAP_ENTER, SWAP_EXIT };
static void SetProcessEventCallback(ProcessEventCallback callback);
```

`Hunter::OnProcessEvent` (`Sources/ShinyHunter.cpp`, registered in
`main.cpp`) uses this to set a `s_paused` flag on `*_ENTER` and clear it on
`*_EXIT`. `Hunter::OnFrame` checks it first and returns immediately when
paused — no input injection, no party-memory reads — freezing the FSM
exactly where it was and resuming on the same frame-counter logic once the
transition completes. This also directly explains a crash the user
reproduced by switching from Alpha Sapphire to `ftpd` via the HOME menu: the
FSM was still injecting buttons and reading process memory *during* the
swap, while the game's own memory layout was being torn down and rebuilt
(`ProcessImpl::UpdateMemRegions()` inside the same `plgldr` event loop).

**Consequence — there is no more attempt to keep the console awake.**
The console sleeps normally on lid-close; the hunt pauses and resumes
automatically around it. For a run that must keep progressing with the lid
closed, disable **Sleep Mode** in the 3DS System Settings (Other Settings) —
a standard, OS-level, fully supported setting — rather than having the
plugin fight sleep from inside an injected process, which is what caused
attempt 1's hard-reboot failures. `Process::SetProcessEventCallback` will
still correctly pause/resume around any HOME-menu entry or app-swap either
way.

**Hardware verification still needed:**
1. Start the hunt, open the HOME menu (or switch to another app, e.g.
   `ftpd`), wait a few seconds, then return to Alpha Sapphire.
2. Check the on-screen status line shows `Paused (sleep/HOME/swap in
   progress)` while away, and resumes normal `Attempt N: ...` status on
   return, with **no crash**.
3. With Sleep Mode left on (default) and the hunt running, close the lid,
   wait, then reopen it — same expectation: pause, then clean resume.

### Crash investigation notes (for anyone debugging further)

Three Luma3DS exception dumps from hardware (before attempt 2 above) were
decoded with the [official parser](https://github.com/LumaTeam/luma3ds_exception_dump_parser)
and cross-referenced against `shinyhunt.elf`'s symbol table
(`arm-none-eabi-addr2line`/`nm`, unstripped build). All three: **data abort
on write**, `PC` inside newlib's `_free_r` (+0x88), `LR` inside
`__libc_lock_acquire_recursive` (+0x18) — i.e. heap corruption, detected
while `free()` was walking/relinking the free list. Both `PC` and `LR`
resolved to addresses inside the plugin's own mapped range (`0x07000100+`
per `3gx.ld`), confirming the corruption is within the plugin's own
statically-linked code/heap, not the game. The faulting instruction
(`STR r5, [r1, #0xc]`) was writing through a register that had been loaded
from a stale/garbage value resembling raw `.text` bytes rather than a valid
heap pointer — consistent with a corrupted free-list node, though the
original corrupting write was not identified (that would need a live
debugger session or bisection, not just post-mortem register dumps).
Pausing all memory/input access during transitions (attempt 2) removes the
highest-risk window for this, but if crashes persist **outside** a
sleep/HOME/swap transition after this fix, that would point to a second,
separate cause and is worth new dumps + a fresh look.

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

**Note on the box slot after that fix:** `SHINY: no` is now correct, but the
diagnostic may still print a non-zero `species`/`PID` for an empty box slot.
That's expected, not a bug: an "empty" box slot on real hardware isn't
necessarily zeroed memory — it can hold stale bytes from whatever was last
stored/withdrawn there, with emptiness tracked by a separate flag elsewhere
that this diagnostic doesn't read. `valid: no` (checksum fails) is the
correct signal that the slot isn't a real, currently-held Pokémon; the raw
species/PID printed alongside it are not meaningful in that case. This is
only a cross-check display — the box address is never used by the actual
hunt loop, only `kPartySlot1Addr` is.
