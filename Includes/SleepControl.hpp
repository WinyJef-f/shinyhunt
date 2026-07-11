// ============================================================================
//  SleepControl.hpp  -  Keep the console awake with the lid closed.
//
//  OPEN QUESTION #1: normal retail titles sleep on lid-close. Luma only
//  suppresses that automatically while InputRedirection / the debugger is
//  active, neither of which we use. libctru's aptSetSleepAllowed(false) tells
//  APT to refuse sleep (this is how music players keep running lid-closed).
//
//  The game may re-enable sleep every frame, so Reassert() is meant to be
//  called from the per-frame loop. Confirm on hardware that this actually
//  holds through a lid-close (docs/OPEN_QUESTIONS.md).
// ============================================================================
#pragma once

namespace ShinyHunt
{
    namespace SleepControl
    {
        void KeepAwake(void);  // call once at startup
        void Reassert(void);   // call every frame (cheap)
    }
}
