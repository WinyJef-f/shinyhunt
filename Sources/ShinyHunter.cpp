// ============================================================================
//  ShinyHunter.cpp
// ============================================================================
#include "ShinyHunter.hpp"
#include "Config.hpp"
#include "PokemonReader.hpp"
#include "InputSim.hpp"
#include "Led.hpp"
#include "Sound.hpp"
#include "SleepControl.hpp"

#include <CTRPluginFramework.hpp>
#include <3ds.h>
#include <cstdio>

namespace ShinyHunt
{
    using namespace CTRPluginFramework;

    namespace
    {
        enum class State
        {
            Idle,           // not hunting
            SoftReset,      // hold L+R+Start
            PostResetWait,  // wait for title + save load
            RunScript,      // step through the starter script
            WaitParty,      // poll party slot 1 (also mashes A)
            ShinyHold       // terminal: LED + jingle, hold forever
        };

        State   s_state       = State::Idle;
        u32     s_frames      = 0;   // frames spent in current state / sub-phase
        u32     s_scriptIndex = 0;
        u32     s_scriptSub   = 0;   // frames spent in current script step
        bool    s_scriptGap   = false; // false = holding, true = release gap
        u32     s_attempts    = 0;
        u32     s_jinglePlays = 0;
        u32     s_jingleTimer = 0;
        PkmData s_lastRead     = {};

        void Enter(State next)
        {
            s_state  = next;
            s_frames = 0;
        }

        // Small helpers for formatting hex without depending on Utils::Format.
        std::string Hex32(u32 v)
        {
            char b[16];
            snprintf(b, sizeof(b), "0x%08lX", (unsigned long)v);
            return std::string(b);
        }
        std::string Hex16(u16 v)
        {
            char b[8];
            snprintf(b, sizeof(b), "0x%04X", (unsigned)v);
            return std::string(b);
        }

        std::string Describe(const PkmData &p)
        {
            std::string s;
            s += "species: " + std::to_string(p.species) + "\n";
            s += "PID: " + Hex32(p.pid) + "\n";
            s += "TID: " + std::to_string(p.tid) + "  SID: " + std::to_string(p.sid) + "\n";
            s += "EC:  " + Hex32(p.encKey) + "\n";
            s += "shinyVal: " + std::to_string(p.shinyValue) +
                 " (thr " + std::to_string(Cfg::kShinyThreshold) + ")\n";
            s += std::string("valid(decrypt+checksum): ") + (p.valid ? "YES" : "no") + "\n";
            s += std::string("SHINY: ") + (p.shiny ? "*** YES ***" : "no");
            return s;
        }

        void EnterShinyHold(void)
        {
            s_jinglePlays = 0;
            s_jingleTimer = 0;
            Led::SetSolid(Cfg::kLedR, Cfg::kLedG, Cfg::kLedB);
            OSD::Notify("SHINY FOUND -- LED locked yellow");
            Enter(State::ShinyHold);
        }

        void Evaluate(const PkmData &p)
        {
            s_attempts++;
            if (p.shiny)
            {
                EnterShinyHold();
                return;
            }
            // Not shiny: notify occasionally and reset.
            if ((s_attempts % 10) == 0)
                OSD::Notify("Attempt " + std::to_string(s_attempts) + ": not shiny");
            Enter(State::SoftReset);
        }
    }

    // ------------------------------------------------------------------------
    void Hunter::OnFrame(void)
    {
        // Keep the system awake whenever the plugin is loaded and active.
        SleepControl::Reassert();

        switch (s_state)
        {
        case State::Idle:
            return;

        case State::SoftReset:
            InputSim::HoldThisFrame(KEY_L | KEY_R | KEY_START);
            if (++s_frames >= Cfg::kResetHoldFrames)
                Enter(State::PostResetWait);
            break;

        case State::PostResetWait:
            // Inject nothing (buttons released) while the title/save loads.
            if (++s_frames >= Cfg::kPostResetWait)
            {
                s_scriptIndex = 0;
                s_scriptSub   = 0;
                s_scriptGap   = false;
                Enter(State::RunScript);
            }
            break;

        case State::RunScript:
        {
            const InputStep *script = InputSim::StarterScript();
            const u32 len = InputSim::StarterScriptLength();

            if (s_scriptIndex >= len)
            {
                Enter(State::WaitParty);
                break;
            }

            const InputStep &step = script[s_scriptIndex];
            if (!s_scriptGap)
            {
                InputSim::HoldThisFrame(step.keys);
                if (++s_scriptSub >= step.holdFrames)
                {
                    s_scriptSub = 0;
                    s_scriptGap = true;
                }
            }
            else
            {
                // release gap (inject nothing)
                if (++s_scriptSub >= step.gapFrames)
                {
                    s_scriptSub = 0;
                    s_scriptGap = false;
                    s_scriptIndex++;
                }
            }
            break;
        }

        case State::WaitParty:
        {
            // Gently mash A to push through "You received Mudkip!" style dialog
            // while we poll. Duty cycle: hold for kKeyHoldFrames, release for
            // kKeyGapFrames.
            const u32 period = Cfg::kKeyHoldFrames + Cfg::kKeyGapFrames;
            if ((s_frames % period) < Cfg::kKeyHoldFrames)
                InputSim::HoldThisFrame(KEY_A);

            PkmData p;
            if (PokemonReader::ReadPartySlot1(p) && p.valid && p.species == Cfg::kTargetSpecies)
            {
                s_lastRead = p;
                Evaluate(p);
                break;
            }

            if (++s_frames >= Cfg::kPartyWaitTimeout)
            {
                // Timed out: either a script desync or the wrong starter got
                // picked. Treat as a failed attempt and reset -- this can never
                // "accept" a wrong-species Pokemon, so a desync only wastes a
                // cycle, it does not corrupt the hunt.
                if (Cfg::kMaxAttempts != 0 && s_attempts >= Cfg::kMaxAttempts)
                {
                    OSD::Notify("Reached max attempts; stopping");
                    Enter(State::Idle);
                    break;
                }
                OSD::Notify("Party wait timed out; resetting");
                Enter(State::SoftReset);
            }
            break;
        }

        case State::ShinyHold:
            // Re-assert the LED so nothing steals it, and play the jingle
            // kJinglePlays times with kJingleGapFrames between plays.
            Led::SetSolid(Cfg::kLedR, Cfg::kLedG, Cfg::kLedB);

            if (s_jinglePlays < Cfg::kJinglePlays)
            {
                if (s_jingleTimer == 0)
                {
                    Sound::PlayJingle();
                    s_jinglePlays++;
                    s_jingleTimer = Cfg::kJingleGapFrames;
                }
                else
                {
                    s_jingleTimer--;
                }
            }
            // Stay here forever.
            break;
        }
    }

    // ------------------------------------------------------------------------
    void Hunter::Start(void)
    {
        s_attempts = 0;
        Sound::Init(); // load the clip once (no-op if sound disabled)
        SleepControl::KeepAwake();
        Enter(State::SoftReset);
        OSD::Notify("Shiny hunt started");
    }

    void Hunter::Stop(void)
    {
        Enter(State::Idle);
        OSD::Notify("Shiny hunt stopped");
    }

    bool Hunter::IsRunning(void)
    {
        return s_state != State::Idle;
    }

    std::string Hunter::StatusLine(void)
    {
        switch (s_state)
        {
        case State::Idle:          return "Idle";
        case State::SoftReset:     return "Soft-resetting (attempt " + std::to_string(s_attempts + 1) + ")";
        case State::PostResetWait: return "Waiting for title/save";
        case State::RunScript:     return "Running starter script (step " + std::to_string(s_scriptIndex) + ")";
        case State::WaitParty:     return "Attempt " + std::to_string(s_attempts + 1) + ": waiting for party";
        case State::ShinyHold:     return "SHINY! holding LED";
        }
        return "?";
    }

    // ------------------------------------------------------------------------
    //  Diagnostics
    // ------------------------------------------------------------------------
    void Hunter::Diag::ReadPokemon(void)
    {
        PkmData party, box;
        PokemonReader::Read(Cfg::kPartySlot1Addr, party);
        PokemonReader::Read(Cfg::kBoxSlot1Addr, box);

        std::string body;
        body += "=== PARTY slot 1 (" + Hex32(Cfg::kPartySlot1Addr) + ") ===\n";
        body += Describe(party) + "\n\n";
        body += "=== BOX 1 slot 1 (" + Hex32(Cfg::kBoxSlot1Addr) + ") ===\n";
        body += Describe(box);

        MessageBox("Shiny Hunter - memory check", body)();
    }

    void Hunter::Diag::TestLed(void)
    {
        Result r = Led::SetSolid(Cfg::kLedR, Cfg::kLedG, Cfg::kLedB);
        std::string body;
        body += std::string("ptm:sysm reachable: ") + (Led::ServiceAvailable() ? "YES" : "NO") + "\n";
        body += "SetInfoLedPattern result: " + Hex32((u32)r) + "\n";
        body += (R_SUCCEEDED(r)
                     ? "LED should now be SOLID YELLOW."
                     : "Call failed -- see docs/OPEN_QUESTIONS.md (#2, Rosalina proxy).");
        MessageBox("Shiny Hunter - LED test", body)();
    }

    void Hunter::Diag::TestSound(void)
    {
        Sound::Init();
        bool played = Sound::PlayJingle();
        std::string body;
        body += std::string("sound compiled in + loaded: ") + (Sound::Available() ? "YES" : "NO") + "\n";
        body += std::string("play started: ") + (played ? "YES" : "NO") + "\n";
        if (!Sound::Available())
            body += "Enable SHINYHUNT_ENABLE_SOUND and set the clip path/format (docs/AUDIO.md).";
        MessageBox("Shiny Hunter - sound test", body)();
    }
}
