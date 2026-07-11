// ============================================================================
//  InputSim.hpp  -  Software button injection via CTRPF Controller::InjectKey.
//
//  Controller::InjectKey ORs the key mask into every entry of the HID
//  shared-memory ring for the CURRENT frame only. To HOLD a button you must
//  re-inject it every frame; to RELEASE it you simply stop injecting. All of
//  that framing is done by the FSM in ShinyHunter.cpp; this module only
//  provides the per-frame primitive and the scripted starter sequence.
//
//  Because these are software inputs (not physical buttons), the whole loop
//  keeps working with the lid closed, as long as sleep is suppressed.
// ============================================================================
#pragma once

#include <3ds.h>

namespace ShinyHunt
{
    // One scripted input: hold 'keys' for 'holdFrames', then release (inject
    // nothing) for 'gapFrames' so the game registers a clean press+release.
    struct InputStep
    {
        u32 keys;       // libctru KEY_* mask (0 = pure wait)
        u32 holdFrames;
        u32 gapFrames;
    };

    namespace InputSim
    {
        // Inject 'keys' for this single frame (call again next frame to hold).
        void HoldThisFrame(u32 keys);

        // The tunable sequence that takes a freshly-loaded save from the point
        // you saved at (recommended: standing right in front of the starter
        // bag) up to the moment Mudkip is generated into party slot 1.
        const InputStep *StarterScript(void);
        u32              StarterScriptLength(void);
    }
}
