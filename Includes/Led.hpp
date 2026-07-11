// ============================================================================
//  Led.hpp  -  Notification LED control via ptm:sysm SetInfoLedPattern.
//
//  Ported from PabloMK7/ctr-led-brary. The MCU drives the notification LED
//  independently of the SoC, so a pattern set here keeps glowing even with the
//  lid closed / screens off -- exactly what we want for "find it by the LED".
//
//  OPEN QUESTION #2: ptm:sysm is a privileged service. Whether a plugin
//  injected into the game process is allowed to obtain its handle must be
//  confirmed on hardware. SetSolid()/Off() return a Result so the diagnostic
//  menu can show whether the call actually succeeded. If it does NOT, see
//  docs/OPEN_QUESTIONS.md for the Rosalina-proxy fallback.
// ============================================================================
#pragma once

#include <3ds.h>

namespace ShinyHunt
{
    namespace Led
    {
        // Set a solid, non-animated colour and hold it. Safe to call repeatedly
        // (we re-assert it while the shiny state is held).
        Result SetSolid(u8 r, u8 g, u8 b);

        // Turn the notification LED off (zeroed pattern).
        Result Off(void);

        // True if the last ptm:sysm handle acquisition succeeded.
        bool   ServiceAvailable(void);
    }
}
