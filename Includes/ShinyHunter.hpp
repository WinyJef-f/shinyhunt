// ============================================================================
//  ShinyHunter.hpp  -  The autonomous soft-reset FSM + hardware diagnostics.
//
//  OnFrame() is a per-frame state machine (registered with the plugin menu's
//  frame callback). It drives: soft reset -> advance to bag -> pick Mudkip ->
//  read party slot 1 from memory -> shiny check -> repeat, or hold a solid
//  yellow LED + jingle on a hit.
// ============================================================================
#pragma once

#include <string>
#include <CTRPluginFramework/System/Process.hpp>

namespace ShinyHunt
{
    namespace Hunter
    {
        // Called once per frame by the framework. Safe to call before Start()
        // (it no-ops in the Idle state).
        void OnFrame(void);

        // Registered via Process::SetProcessEventCallback (see main.cpp).
        // Pauses the FSM for SLEEP/HOME/SWAP _ENTER (no input injection or
        // memory reads while the game's own state/memory layout is mid-
        // transition) and resumes it on the matching _EXIT, exactly where it
        // left off. This is the framework-supported replacement for the
        // removed SleepControl hack -- see docs/OPEN_QUESTIONS.md Q1.
        void OnProcessEvent(CTRPluginFramework::Process::Event event);

        void Start(void);      // begin / restart the autonomous hunt
        void Stop(void);       // halt the loop (go Idle)
        bool IsRunning(void);

        // Human-readable status for the menu ("Idle", "Attempt 137: waiting", ...).
        std::string StatusLine(void);

        // ---- Diagnostics (bound to on-screen menu entries) ------------------
        namespace Diag
        {
            // Read party slot 1 AND box slot 1 right now, decrypt, and show
            // PID/TID/SID/species/shiny + checksum validity. Use this to answer
            // open-question #3 (do the offsets hold for this cartridge?).
            void ReadPokemon(void);

            // Force the solid yellow LED and report the ptm:sysm Result +
            // whether the service was reachable (open-question #2).
            void TestLed(void);

            // Try to play the jingle once and report whether sound is available.
            void TestSound(void);
        }
    }
}
