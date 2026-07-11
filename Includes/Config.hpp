// ============================================================================
//  Config.hpp  -  All tunables in ONE place.
//
//  Everything you are likely to change while calibrating on real hardware
//  lives here. The rest of the code reads these constants; you should rarely
//  need to touch the .cpp files.
//
//  Values marked  <<CALIBRATE>>  MUST be verified/tuned on the console.
//  Values marked  <<VERIFIED>>   were cross-checked against multiple public
//                                sources (Gen6CTRPluginFramework, PKMN-NTR,
//                                Project Pokemon RAM research).
// ============================================================================
#pragma once

#include <3ds.h>

namespace ShinyHunt
{
    namespace Cfg
    {
        // --------------------------------------------------------------------
        //  MODE
        // --------------------------------------------------------------------
        // The hunt never starts itself. It's off by default whenever the game
        // boots/reboots -- start it explicitly from the L+R menu once you're
        // actually ready to hunt, e.g. standing in front of the starter bag.
        // This also means a crash recovery / game restart never silently
        // resumes an unattended loop you didn't intend to leave running.
        static constexpr bool kStartHuntOnBoot = false;

        // --------------------------------------------------------------------
        //  TARGET POKEMON
        // --------------------------------------------------------------------
        // National Dex number of the starter we are hunting.
        //   Mudkip = 258, Treecko = 252, Torchic = 255.
        // The loop only accepts a freshly generated party slot 1 whose species
        // matches this; a script desync that picks the wrong starter is treated
        // as a failed attempt and soft-resets, so it can never corrupt a hunt.
        static constexpr u16 kTargetSpecies = 258; // Mudkip <<VERIFIED>>

        // --------------------------------------------------------------------
        //  SHININESS
        // --------------------------------------------------------------------
        // shiny_val = TID ^ SID ^ (PID>>16) ^ (PID & 0xFFFF)
        // shiny if shiny_val < kShinyThreshold.
        //
        // IMPORTANT: Gen 6 (ORAS) uses a threshold of 16 in-game. Every Mudkip
        // with shiny_val < 16 genuinely sparkles in ORAS. The project brief
        // quoted "< 8" (that is the Gen 3 Emerald rule). Using 8 here would keep
        // resetting away real ORAS shinies whose value happens to be 8..15.
        // Default is therefore 16 (game-accurate). Set it to 8 only if you
        // deliberately want the stricter subset.
        static constexpr u32 kShinyThreshold = 16; // <<VERIFIED for Gen 6>>

        // --------------------------------------------------------------------
        //  MEMORY ADDRESSES  (static; the 3DS does not ASLR these titles)
        // --------------------------------------------------------------------
        // Address of PARTY slot 1's encrypted PK6 block for Alpha Sapphire.
        // Source: PKMN-NTR "sango" (ORAS) partyOff. Cross-checked against the
        // ORAS box base used by Gen6CTRPluginFramework / Project Pokemon RAM
        // research.                                                <<VERIFIED>>
        // Confirmed correct on hardware (open-question #3): read a real
        // Mudkip, TID/SID matched the trainer card, checksum valid.
        //   Party slot N (0-based) = kPartySlot1Addr + N * kPartyEntryStride.
        static constexpr u32 kPartySlot1Addr    = 0x8CFB26C; // ORAS party slot 1
        static constexpr u32 kPartyEntryStride  = 0x104;     // 260 bytes per party entry

        // Size of the encrypted stored block we copy + decrypt (PK6 = 232).
        static constexpr u32 kPk6StoredSize     = 232; // 0xE8

        // --------------------------------------------------------------------
        //  PK6 FIELD OFFSETS  (into the DECRYPTED 232-byte buffer)   <<VERIFIED>>
        // --------------------------------------------------------------------
        static constexpr u32 kOffEncryptionKey  = 0x00; // u32
        static constexpr u32 kOffChecksum       = 0x06; // u16
        static constexpr u32 kOffSpecies        = 0x08; // u16 (national dex in Gen 6)
        static constexpr u32 kOffTID            = 0x0C; // u16
        static constexpr u32 kOffSID            = 0x0E; // u16
        static constexpr u32 kOffPID            = 0x18; // u32

        // --------------------------------------------------------------------
        //  INPUT TIMING  (in frames; the plugin runs its FSM once per frame)
        // --------------------------------------------------------------------
        // Because Controller::InjectKey ORs the key into the HID shared-memory
        // ring, a held button must be re-injected every frame; "release" simply
        // means we stop injecting. These counts are how many frames each phase
        // holds/waits. Starting points that work for many setups, but the exact
        // numbers drift per console/game state.                   <<CALIBRATE>>
        static constexpr u32 kFramesPerSecond   = 60;

        static constexpr u32 kResetHoldFrames   = 20;  // hold L+R+Start this long
        static constexpr u32 kPostResetWait     = 240; // wait for title + save load (~4s)

        static constexpr u32 kKeyHoldFrames     = 8;   // hold each scripted press
        static constexpr u32 kKeyGapFrames      = 10;  // release gap between presses

        // How long (frames) to keep polling party slot 1 for a valid target
        // after finishing the input script before assuming a desync and
        // resetting. ~10 s.
        static constexpr u32 kPartyWaitTimeout  = 600;

        // Safety: absolute cap on iterations (0 = unlimited). Overnight runs
        // should leave this 0; useful to bound a supervised test.
        static constexpr u32 kMaxAttempts       = 0;

        // --------------------------------------------------------------------
        //  SLEEP / HOME / SWAP  (lid-close, HOME menu, app-swap)
        // --------------------------------------------------------------------
        // There is no "keep-awake" setting here, and there is no way to add
        // one: sleep-on-lid-close is a hardware Hall sensor, not a software
        // toggle. An earlier version tried to suppress it with a blind APT:U
        // ReplySleepQuery IPC call -- confirmed harmful on hardware (black
        // screen requiring a hard reboot). Instead, Hunter::OnProcessEvent
        // (registered in main.cpp via Process::SetProcessEventCallback) simply
        // STOPS the hunt the instant sleep / HOME / app-swap begins, so the
        // FSM is inert through the transition. Restart from the menu after.
        // Run with the lid OPEN. See docs/OPEN_QUESTIONS.md Q1.

        // --------------------------------------------------------------------
        //  LED (solid yellow on shiny)
        // --------------------------------------------------------------------
        // Yellow = red + green, no blue.
        static constexpr u8 kLedR = 0xFF;
        static constexpr u8 kLedG = 0xFF;
        static constexpr u8 kLedB = 0x00;

        // --------------------------------------------------------------------
        //  SOUND  (shiny jingle)  -- see Sound.cpp and docs/AUDIO.md
        // --------------------------------------------------------------------
        // Number of times to play the jingle and the gap between plays.
        static constexpr u32 kJinglePlays       = 5;
        static constexpr u32 kJingleGapFrames   = 60; // 1 second between plays

        // How many overlapping plays the clip allocates channels for. Our
        // jingle is ~0.5 s and plays are 1 s apart, so they never overlap, but
        // a small margin is harmless.
        static constexpr int kJingleMaxSimultPlays = 4;

        // Absolute SD path the plugin loads the jingle from at runtime.
        // Copy assets/shiny_jingle.bcwav (built by assets/make_jingle.py and
        // shipped in the CI artifact) to exactly this location:
        //   sd:/luma/plugins/shiny_jingle.bcwav
        // It is a single fixed path -- no per-title folder needed. If the file
        // is missing the plugin simply runs without sound (the LED is the
        // primary indicator); it never crashes over a missing jingle.
        static constexpr const char *kSoundPath =
            "/luma/plugins/shiny_jingle.bcwav";
    }
}
