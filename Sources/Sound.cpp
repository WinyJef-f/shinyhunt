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

        CwavSound *s_clip   = nullptr;
        bool       s_loaded = false;

        bool DoInit(void)
        {
            if (s_loaded)
                return true;

            // Construct the clip once. Even on failure we keep the object so
            // we don't re-read the file (and re-allocate) every frame; a
            // failed load just leaves s_loaded false.
            if (s_clip == nullptr)
                s_clip = new CwavSound(std::string(Cfg::kSoundPath),
                                       Cfg::kJingleMaxSimultPlays);

            s_loaded = (s_clip != nullptr &&
                        s_clip->GetLoadStatus() == CwavStatus::SUCCESS);
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
