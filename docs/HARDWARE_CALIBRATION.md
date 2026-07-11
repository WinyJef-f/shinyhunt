# Hardware calibration

Two things must be tuned on the console: the **party offset** (once, if Q3
fails) and the **starter input timing** (the fiddly part). Everything else is
already correct or self-checking.

Set `Cfg::kStartHuntOnBoot = false` while calibrating so the loop only runs when
you toggle it from the menu.

---

## A. Confirm / fix the party offset

Covered by **Q3** in `docs/OPEN_QUESTIONS.md`. If **Diag: read party + box now**
shows a valid, correct slot-1 Pokémon, you are done — skip to section B.

If it is wrong:
- The box cross-check (`0x8C9E134`) reading correctly while the party reads
  garbage means only `Cfg::kPartySlot1Addr` is off for your revision.
- The value shipped (`0x8CFB26C`) is the PKMN-NTR "sango" party offset. If your
  cart differs, the delta is usually small and constant. Options to find it:
  - Use Rosalina's memory viewer / a memory-editing tool (NTR, or the CTRPF
    template's memory searcher) to locate your slot-1 Pokémon's encrypted block
    near `0x8CFB000`, and set the constant to that address.
  - It is the start of the party region; slot N (0-based) is
    `kPartySlot1Addr + N * 0x104`.

Rebuild after editing `Config.hpp`.

---

## B. Tune the starter input script

This is the part that genuinely needs iteration; real-hardware timing drifts and
fixed frame counts will occasionally desync. Two safety nets are already built
in so a desync only costs one cycle, never the hunt:

1. `WaitParty` **only accepts a valid, freshly-decrypted Mudkip** (`species ==
   kTargetSpecies`). Wrong starter / empty slot → it times out and soft-resets.
2. It also mashes A while waiting, so post-selection dialog clears itself.

So you only need the script to reliably **open the bag and land the cursor on
Mudkip**. Everything in `Sources/InputSim.cpp` (`kStarterScript`) plus the
timing constants in `Config.hpp` are the knobs:

```
kResetHoldFrames   how long L+R+Start is held for the soft reset
kPostResetWait     wait for the title screen + save to load (raise if it desyncs
                   early — a cold cartridge load can be slow)
kKeyHoldFrames     frames each scripted press is held
kKeyGapFrames      release gap between presses (raise if presses get "eaten")
kStarterScript[]   the actual sequence: A,A,A (open bag), Right,Right (to Mudkip),
                   A (select), A (confirm)
```

### Recommended setup (makes the script short and robust)

Save **standing directly in front of the starter bag**, right before the
selection prompt. Then the script only has to clear a couple of dialog boxes and
pick Mudkip — far more reliable than driving the whole intro.

### Tuning loop

1. Start from the shipped values.
2. Toggle the hunt from the menu and **watch one full cycle with the lid open**.
3. If the bag never opens → increase `kPostResetWait` and/or the leading A count.
4. If the cursor lands on the wrong starter → adjust the `KEY_DRIGHT` count (or
   change direction) so it ends on Mudkip. ORAS bag order is
   Treecko / Torchic / Mudkip (left→right).
5. If presses are dropped → raise `kKeyHoldFrames` and `kKeyGapFrames`.
6. When it reliably generates a Mudkip and the attempt counter advances on its
   own, you are ready. Watch ~10 cycles unattended before an overnight run.

### If you want to replace timed waits with a memory flag

The brief prefers state-polling over fixed delays. The one state the plugin
already polls reliably is **party population** (the `WaitParty` gate). If you
later reverse-engineer a "bag is open" flag, wire it in place of the fixed
`kPostResetWait` using the same `Process::CopyMemory`/`Read` pattern as
`PokemonReader` — the FSM is structured to make that a localized change.

---

## C. Shiny threshold sanity

Default `Cfg::kShinyThreshold = 16` (ORAS-correct). During calibration the Diag
readout prints `shinyVal` and `thr` so you can confirm the math against a known
Pokémon. Only lower it to 8 if you deliberately want the stricter subset and
accept that the loop will reset away in-game shinies with value 8–15.
