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

### Attempt 2: pause/resume via `Process::SetProcessEventCallback` (superseded)

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

Attempt 2 used this to **pause** the FSM on `*_ENTER` and resume on `*_EXIT`.

**Attempt 2 hardware result: not enough.** Boot/save-load transitions stopped
crashing, but switching from Alpha Sapphire to `ftpd` via the HOME menu
**still crashed**, producing a 4th exception dump — **the exact same fault**:
a data abort on write at `PC = _free_r+0x88`, byte-for-byte the same offset
into the same function as the first three dumps (only `LR` differed:
`__libc_cond_broadcast+0x8` vs. `__libc_lock_acquire_recursive+0x18`, a
different caller reaching the same corrupted heap state).

### Attempt 3 (shipped): stop the hunt on any transition

Since even a *paused* FSM didn't prevent the HOME/swap crash, attempt 3 stops
being clever about resuming and just **fully stops the hunt** the instant any
`SLEEP_ENTER` / `HOME_ENTER` / `SWAP_ENTER` fires. `Hunter::OnProcessEvent`
sets the state straight to `Idle` (no allocation, no OSD, nothing that could
itself race the framework's mid-transition heap work), leaving the FSM
completely inert. You restart the hunt from the menu when you're back. This
is the simplest possible contract — "any interruption stops the hunt" — and
removes every bit of our own activity from the dangerous window.

**Honest status:** whether this fully eliminates the HOME/swap crash is
**not yet confirmed on hardware.** The 4th dump showed the fault can occur
with our per-frame code already inert, which points at
CTRPluginFramework's own swap-handling sequence (its `KeepThread`
unmapping/remapping a "hook memory" page adjacent to the newlib heap's upper
boundary — `__ctru_heap + __ctru_heap_size`, see `allocateHeaps.cpp`) rather
than at us. If that's the true cause, stopping the hunt reduces but may not
100% remove the risk. The strong correlation the user observed ("only crashes
when hunting is on") still suggests our activity is a contributing trigger,
so going fully inert is the best mitigation available from the plugin side.
The failure mode is at least **recoverable** now (a normal Luma exception
screen that writes a dump, not attempt 1's hard-reboot black screen).

**Current recommendation:** run with the lid open and avoid the HOME menu
while a hunt is active. Entering sleep or HOME now stops the hunt
automatically, so the main risk is the instant of the transition itself.

**Hardware verification still needed:**
1. With a hunt running, close the lid (or open HOME / swap to `ftpd`) and
   confirm the hunt is stopped on return and — the open question — whether
   the crash is now gone or merely less frequent. New dumps welcome if it
   still happens.
2. Repeat the boot/save-load cycle several more times to confirm that case
   stays crash-free.
2. If anyone wants to dig further into the HOME/swap crash: a live GDB
   session via Luma's debugger, or trying a newer `libctrpf` build (the
   [upstream repo](https://gitlab.com/thepixellizeross/ctrpluginframework)
   has commits after the `0.8.0.r1444` revision currently pulled by CI,
   including hook/GSP-interrupt bugfixes — none confirmed to match this
   specific issue, but worth trying), would be the next steps.

### Crash investigation notes (for anyone debugging further)

**Five** Luma3DS exception dumps from hardware were decoded with the
[official parser](https://github.com/LumaTeam/luma3ds_exception_dump_parser)
and cross-referenced against `shinyhunt.elf`'s symbol table
(`arm-none-eabi-addr2line`/`nm`, unstripped build). **Every single one** is
the same fault: a **data abort on write**, `PC` inside newlib's `_free_r` at
the identical offset, at one specific free-list unlink instruction
(`STR r5, [r1, #0xc]`, writing through a "next chunk" pointer that has been
overwritten with garbage that decodes as ARM instruction bytes, e.g.
`0xe92d4010` = `push {r4, lr}`). This is a textbook **heap-metadata
corruption** signature: something earlier wrote past the end of a heap
allocation and clobbered an adjacent chunk header; `free()` only trips over
it later. `PC`/`LR` always land in the plugin's own mapped range
(`0x07000100+` per `3gx.ld`) — this is the *shared* newlib/heap that both our
code and the whole CTRPluginFramework runtime are statically linked against,
so any `free()` anywhere in the injected code hits it.

The triggers vary — sleep entry, HOME-menu app-swap (dumps 0–3), and now
**entering the first rival battle on Route 103 while the hunt was running**
(dump 6) — but the corrupted state and crash site are identical. That points
to a single class of corrupting write that happens during the **game's
scene/memory transitions** (battle intro, sleep, HOME), when the framework
is doing the most concurrent memory work (region remaps, OSD, hook
bookkeeping) on its own threads alongside the plugin. It is **not** the
"read party memory / inject buttons" logic itself — those touch stack
buffers and HID shared memory, not the heap.

**Mitigation applied from the plugin side:** the per-frame hunt loop is now
**allocation-free** — the periodic `OSD::Notify(...)` calls (which built
`std::string`s on that shared heap every so often mid-hunt) were removed, so
the FSM contributes zero heap churn while running. Whether that meaningfully
reduces the crashes is unverified; if the corrupting write lives in the
framework/hook machinery rather than in our allocations, it may not fully
help. But it removes the one heap-touching thing we were doing during the
loop, which is the only lever available without a live on-hardware debugger.

**Note on real-hunt exposure:** an actual automated soft-reset hunt never
walks to Route 103 or enters a battle — it cycles title → save-load → bag →
party read → reset. The battle/HOME/manual-play crashes happen when the
console is driven *by hand* with the hunt active. The transitions a real
hunt does hit are the title-screen load and save load each cycle; those need
their own repeated-run confirmation (see the boot/save-load item above).

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
