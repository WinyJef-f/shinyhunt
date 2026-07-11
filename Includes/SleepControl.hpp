// ============================================================================
//  SleepControl.hpp  -  Keep the console awake with the lid closed.
//
//  CONFIRMED HARMFUL ON HARDWARE, DISABLED BY DEFAULT (Cfg::kSleepControlEnabled).
//  Calling ReplySleepQuery unsolicited (not in response to an actual pending
//  sleep-query notification), on the game's own thread, desyncs APT's state
//  machine: the screen goes black on lid-open and requires a hard reboot, and
//  it also causes random crashes during boot/save-load. See
//  docs/OPEN_QUESTIONS.md Q1 for what a real fix requires. Do not call these
//  functions (or flip the config flag) without a notification-driven rewrite.
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
