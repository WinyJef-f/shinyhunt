// ============================================================================
//  SleepControl.cpp
// ============================================================================
#include "SleepControl.hpp"

#include <3ds.h>

namespace ShinyHunt
{
    void SleepControl::KeepAwake(void)
    {
        // Refuse auto-sleep and shell-close sleep.
        aptSetSleepAllowed(false);
    }

    void SleepControl::Reassert(void)
    {
        // Cheap to call each frame; guarantees our policy wins even if the game
        // flips it back. If profiling ever shows this is hot, gate it to every
        // N frames -- but it is a single APT flag write, so it is fine.
        aptSetSleepAllowed(false);
    }
}
