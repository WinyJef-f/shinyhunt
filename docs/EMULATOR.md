# Using a 3DS emulator on the Mac — what it can and can't do

## The hard limit

**A 3DS emulator cannot run this plugin.** `.3gx` plugins are loaded by
**Luma3DS's plugin loader**, a custom-firmware feature. Emulators (Azahar /
Lime3DS — the maintained successors to the now-discontinued Citra) high-level-
emulate the 3DS OS: there is **no Luma3DS, no Rosalina, no plugin loader**, so a
`.3gx` has nowhere to load. The plugin's core actions also depend on
CFW/hardware behavior an emulator doesn't reproduce:

- `Controller::InjectKey` writing HID shared memory,
- `ptm:sysm` LED control,
- `aptSetSleepAllowed` sleep suppression,
- reading the live party block at a fixed RAM address.

So there is no "run it in the emulator, then move to hardware" path. The plugin
only ever runs on the real, CFW'd console.

## What the emulator IS good for (all on the Mac)

Use Azahar (Intel + Apple Silicon) with your Alpha Sapphire dump:

1. **Input choreography — the highest-value use.** Play manually to work out the
   exact sequence and rough timing to go: soft reset → clear intro dialog → open
   the starter bag → move the cursor to Mudkip → confirm. Then encode that into
   `kStarterScript` (`Sources/InputSim.cpp`) and the timing constants in
   `Includes/Config.hpp`. This is the part that most needs discovery before you
   ever touch hardware.

2. **Validate the decryption against real game data.** Get a Mudkip, save
   in-game, then open the emulator's save file in **PKHeX** and compare its
   PID/TID/SID to what this plugin's decryption computes on the same bytes. (The
   decrypt math is already host-verified to roundtrip; this checks it against
   actual ORAS data.)

3. **Confirm the starter flow**: soft-reset behavior, where Mudkip lands (party
   slot 1), and its species id (258).

## What still needs the real 3DS

The three open questions plus input injection — none are observable in an
emulator:

- Does `Controller::InjectKey` actually drive the game (open question: injection).
- `ptm:sysm` LED reachable in-process (open question #2).
- Sleep suppression with the lid closed (open question #1).
- The live party RAM address `0x8CFB26C` reading correctly (open question #3).

## Recommended all-Mac dev loop

1. Build natively (`docs/BUILD_MAC.md`) so `make` produces a `.3gx` — proves the
   toolchain and that the code compiles.
2. Use the emulator to nail the input choreography and to sanity-check data via
   save + PKHeX.
3. (Optional) run the logic-only host harness — a Mac executable that mocks
   `Process::CopyMemory`/`Controller` and drives the real FSM + decryption
   against real party bytes. Ask if you want this added under `tests/`.
4. One focused hardware session to close the open questions using the on-screen
   **Diag** menu (`docs/OPEN_QUESTIONS.md`), then the supervised → overnight runs.
