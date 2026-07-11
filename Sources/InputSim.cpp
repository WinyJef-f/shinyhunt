// ============================================================================
//  InputSim.cpp
// ============================================================================
#include "InputSim.hpp"
#include "Config.hpp"

#include <CTRPluginFramework.hpp>

namespace ShinyHunt
{
    using namespace CTRPluginFramework;

    void InputSim::HoldThisFrame(u32 keys)
    {
        if (keys != 0)
            Controller::InjectKey(keys);
    }

    // ------------------------------------------------------------------------
    //  Starter script  <<CALIBRATE the whole table on hardware>>
    //
    //  Assumes you saved standing directly in front of the starter bag (the
    //  standard soft-reset setup). Tune counts with the DIAGNOSTIC menu:
    //   1. Watch that A-mashing reliably opens the bag.
    //   2. Watch the cursor land on Mudkip before the confirm press.
    //  The FSM ALSO mashes A while waiting for the party to populate, so this
    //  table only has to: clear pre-bag dialog -> move to Mudkip -> confirm.
    //
    //  ORAS bag layout is Treecko / Torchic / Mudkip (left..right); Mudkip is
    //  the rightmost, hence the two DRIGHT presses. Adjust if your cursor
    //  starts elsewhere.
    // ------------------------------------------------------------------------
    namespace
    {
        const InputStep kStarterScript[] =
        {
            // keys,                 hold,                    gap
            { KEY_A, Cfg::kKeyHoldFrames, Cfg::kKeyGapFrames }, // clear dialog
            { KEY_A, Cfg::kKeyHoldFrames, Cfg::kKeyGapFrames }, // clear dialog
            { KEY_A, Cfg::kKeyHoldFrames, Cfg::kKeyGapFrames }, // open the bag
            { KEY_DRIGHT, Cfg::kKeyHoldFrames, Cfg::kKeyGapFrames }, // -> Torchic
            { KEY_DRIGHT, Cfg::kKeyHoldFrames, Cfg::kKeyGapFrames }, // -> Mudkip
            { KEY_A, Cfg::kKeyHoldFrames, Cfg::kKeyGapFrames }, // select Mudkip
            { KEY_A, Cfg::kKeyHoldFrames, Cfg::kKeyGapFrames }, // "choose this one?" yes
        };
    }

    const InputStep *InputSim::StarterScript(void)
    {
        return kStarterScript;
    }

    u32 InputSim::StarterScriptLength(void)
    {
        return sizeof(kStarterScript) / sizeof(kStarterScript[0]);
    }
}
