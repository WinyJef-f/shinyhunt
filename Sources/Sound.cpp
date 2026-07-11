// ============================================================================
//  Sound.cpp
//
//  Real implementation over CTRPluginFramework::Sound (Sound/Sound.hpp,
//  pulled in by <CTRPluginFramework.hpp>). Constructs the clip from a BCWAV
//  file on the SD card and plays it. This is the SAME load path the framework
//  uses for its own menu sounds (File -> operator new -> cwavLoad), which is
//  the one proven to work from inside a 3GX plugin -- the buffer ends up on
//  the plugin's heap where CSND's VA->PA conversion works. (An embedded
//  in-.data buffer is NOT reliable for CSND DMA, so we load from a file.)
//
//  We try two locations so the file works wherever you dropped it:
//    1. Cfg::kSoundPath            -> sd:/luma/plugins/shiny_jingle.bcwav
//    2. "shiny_jingle.bcwav"       -> the plugin's own folder (CWD is set to
//                                     the .3gx's directory by the framework),
//                                     i.e. next to the .3gx in the <TITLEID>
//                                     subfolder.
//
//  NOTE: our own namespace has a `Sound` namespace, which would collide with
//  CTRPluginFramework::Sound, so every reference to the framework class is
//  fully qualified as ::CTRPluginFramework::Sound.
// ============================================================================
#include "Sound.hpp"
#include "Config.hpp"

#include <CTRPluginFramework.hpp>

namespace ShinyHunt
{
    namespace
    {
        using CwavSound  = ::CTRPluginFramework::Sound;
        using CwavStatus = ::CTRPluginFramework::Sound::CWAVStatus;

        CwavSound  *s_clip     = nullptr;
        bool        s_loaded   = false;

        bool TryLoad(const std::string &path)
        {
            CwavSound *clip = new CwavSound(path, Cfg::kJingleMaxSimultPlays);
            if (clip != nullptr && clip->GetLoadStatus() == CwavStatus::SUCCESS)
            {
                s_clip = clip;
                return true;
            }
            delete clip; // failed load: free it and try the next candidate
            return false;
        }

        // Init() is only ever called from Hunter::Start() (once per hunt) and
        // from the sound diagnostic (once per menu open) -- never per frame --
        // so it is fine to re-attempt the load if a previous try failed (e.g.
        // you just copied the file and re-ran the diagnostic).
        bool DoInit(void)
        {
            if (s_loaded)
                return true;

            const char *candidates[] = {
                Cfg::kSoundPath,          // absolute: sd:/luma/plugins/...
                "shiny_jingle.bcwav",     // relative: plugin's own folder
            };
            for (const char *p : candidates)
            {
                if (TryLoad(std::string(p)))
                {
                    s_loaded = true;
                    break;
                }
            }
            return s_loaded;
        }

        bool DoPlay(void)
        {
            if (!s_loaded || s_clip == nullptr)
                return false;
            return s_clip->Play() == CwavStatus::SUCCESS;
        }
    }

    bool Sound::Init(void)       { return DoInit(); }
    bool Sound::PlayJingle(void) { return DoPlay(); }
    bool Sound::Available(void)  { return s_loaded; }
}
