# Open questions — resolve these FIRST (one short hardware session)

The brief calls out three unknowns to settle before trusting an overnight run.
The plugin ships with **Diag** menu entries so each one is a 2-minute test.

Boot the game with the plugin installed, press **L+R** to open the Rosalina
plugin menu, and you will see the Shiny Hunter entries. Set
`Cfg::kStartHuntOnBoot = false` in `Includes/Config.hpp` (and rebuild) while
you are testing so the loop does not start on its own.

---

## Q1 — Does the console stay awake with the lid closed?

**Still open.** Short answer: **no** — there is no way, from this plugin or
from System Settings, to keep the console out of sleep mode with the lid
closed. Sleep-on-lid-close on the 3DS is triggered by a **physical Hall-
effect sensor** (a magnet near the hinge, detected by the right speaker
area), not a software setting — an earlier version of this doc incorrectly
suggested a "Sleep Mode" toggle in System Settings; **no such setting
exists.** The only software lever at all is APT-level sleep-query
suppression, which is exactly what attempt 1 below tried and found unsafe.
Practically: **run with the lid open** for now.

Two iterations are recorded below because the second one only makes sense in
light of what the first one got wrong, and because the second one is only a
**partial** fix — see "Attempt 2 hardware result."

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
exactly where it was and resuming once the transition completes.

**Attempt 2 hardware result: partial fix.** Boot/save-load transitions no
longer crashed in the next test session (tentative — the user's own words:
"unless I was able to go from the intro to the title to the save load to the
game all in a fluke"; needs more repetitions to call it solid). But
switching from Alpha Sapphire to `ftpd` via the HOME menu **still crashed**,
producing a 4th exception dump. Decoding it (same method as below) showed
**the exact same fault**: a data abort on write at `PC = _free_r+0x88` — the
identical faulting instruction as the first three dumps, byte-for-byte the
same offset into the same function. Only `LR` differs: `__libc_cond_broadcast
+0x8` this time, vs. `__libc_lock_acquire_recursive+0x18` before — a
different caller reaching the same corrupted state.

This is important: it means pausing `OnFrame` **did not** stop this specific
crash, even though our FSM is provably inert (no input injection, no memory
reads) throughout the swap by the time it happens. That rules out our own
per-frame code as the direct trigger for the HOME/swap case and points at
something in **CTRPluginFramework's own swap-handling sequence** (running on
its internal `KeepThread`, unmapping/remapping a "hook memory" page located
immediately adjacent to the newlib heap's upper boundary —
`__ctru_heap + __ctru_heap_size`, see `allocateHeaps.cpp` — while some other
thread, e.g. the framework's own OSD/menu rendering, may still be touching
the heap). That's a plausible mechanism, not a confirmed one; pinning it down
further would need a live debugger session on hardware, not just post-mortem
register dumps.

**What did improve:** the failure mode itself. Attempt 1's unsafe IPC hack
produced an *unrecoverable* black screen requiring a hard reboot. This crash
now surfaces as a normal, *recoverable* Luma3DS exception screen (dumps get
written, the game/plugin can be closed normally) — worse than no crash, but
much better than attempt 1.

**Practical mitigation shipped alongside this:** a **SELECT hotkey**
(`Hunter::Toggle`, checked every frame regardless of pause state) instantly
stops or starts the hunt without navigating the L+R menu, so you're not
racing a menu to avoid a crash before doing anything risky (switching apps,
etc.). It doesn't fix the underlying HOME/swap crash — the evidence above
suggests it may not even help, since the crash reproduced with the FSM
already paused/inert — but it's a fast, always-available escape hatch and a
good habit regardless.

**Current recommendation:** avoid returning to the HOME menu / switching
apps while the plugin is loaded and a hunt might be active, until this is
root-caused further. If you do need to swap apps, expect a small chance of a
recoverable crash (Luma exception screen, not a hang) rather than
data loss or hardware risk.

**Hardware verification still needed:**
1. Repeat the boot/save-load cycle several more times to confirm attempt 2
   actually fixed that case and it wasn't a fluke.
2. If anyone wants to dig further into the HOME/swap crash: a live GDB
   session via Luma's debugger, or trying a newer `libctrpf` build (the
   [upstream repo](https://gitlab.com/thepixellizeross/ctrpluginframework)
   has commits after the `0.8.0.r1444` revision currently pulled by CI,
   including hook/GSP-interrupt bugfixes — none confirmed to match this
   specific issue, but worth trying), would be the next steps.

### Crash investigation notes (for anyone debugging further)

Four Luma3DS exception dumps from hardware (three before attempt 2, one
after) were decoded with the [official parser](https://github.com/LumaTeam/luma3ds_exception_dump_parser)
and cross-referenced against `shinyhunt.elf`'s symbol table
(`arm-none-eabi-addr2line`/`nm`, unstripped build). All four: **data abort
on write**, `PC` inside newlib's `_free_r` at the identical `+0x88` offset
every time — i.e. heap corruption, detected while `free()` was
walking/relinking the free list at one specific unlink instruction
(`STR r5, [r1, #0xc]`, writing through a register loaded from a
stale/garbage value that resembles raw `.text` bytes rather than a valid
heap pointer). `PC` and `LR` always resolved to addresses inside the
plugin's own mapped range (`0x07000100+` per `3gx.ld`), confirming the
corruption is within the plugin's own statically-linked code/heap, not the
game. `LR` — the caller that invoked the fatal `free()` — differs between
occurrences (`__libc_lock_acquire_recursive+0x18`,
`__libc_cond_broadcast+0x8`), meaning multiple call paths reach the same
corrupted heap state. The consistency of the exact crash offset across
otherwise-different triggers suggests a single specific corrupting write
happening earlier and being discovered later, rather than a new corruption
each time — but identifying that original write needs live debugging, not
static analysis of post-mortem dumps.

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

**RESOLVED on hardware.** Run **Diag: read party now** with a known
Pokémon in your party (ideally one you know is NOT shiny) and compare the
reported species / TID / SID against what you see in-game (your TID/SID are
on the Trainer Card). On this cartridge it read species 258 (Mudkip),
TID/SID matching the trainer card, `valid: YES`, `SHINY: no` — the offset is
correct.

If it ever reads `valid: no` or nonsense species/IDs on a different
cartridge/revision, the static address differs for that revision; fix
`Cfg::kPartySlot1Addr` in `Includes/Config.hpp` and see
`docs/HARDWARE_CALIBRATION.md` for how to relocate it.

> Do this with a **known non-shiny** Pokémon so a `SHINY: no` result proves the
> math is right, not just that it read *something*.

**Bug found and fixed along the way:** the diagnostic used to also read box
1 slot 1 as a cross-check, and displayed `SHINY: *** YES ***` for that
(empty) slot despite `valid: no`. An empty slot decrypts to species 0 / PID
0, and `shinyValue = 0 ^ 0 ^ 0 ^ 0 = 0`, which is `< 16` — so the shiny flag
was computed even though the slot held no real Pokémon. This never affected
the actual hunt loop (`WaitParty` in `ShinyHunter.cpp` already gates on
`p.valid && p.species == kTargetSpecies` before evaluating shininess), but
it was confusing in the diagnostic — and with Q3 fully resolved, the box
cross-check no longer served a purpose. It's been removed entirely;
`Diag::ReadPokemon()` now only reads and displays party slot 1.
