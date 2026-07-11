// ============================================================================
//  Sound.cpp
//
//  Set SHINYHUNT_ENABLE_SOUND to 1 once you have confirmed your installed
//  libctrpf exposes a Sound API and you have converted the jingle to the
//  format it expects (docs/AUDIO.md). Then fix the two marked lines below to
//  match that header's exact signatures. Everything else in the plugin works
//  regardless of this flag.
// ============================================================================
#ifndef SHINYHUNT_ENABLE_SOUND
#define SHINYHUNT_ENABLE_SOUND 0
#endif

#include "Sound.hpp"
#include "Config.hpp"

#include <CTRPluginFramework.hpp>

#if SHINYHUNT_ENABLE_SOUND
// Included at file scope (NOT inside a namespace) on purpose.
// <<CALIBRATE: this path/name must match your libctrpf.>>
#include <CTRPluginFramework/System/Sound.hpp>
#endif

namespace ShinyHunt
{
    namespace
    {
        bool s_loaded = false;

#if SHINYHUNT_ENABLE_SOUND
        // Fully qualified to avoid clashing with our own ShinyHunt::Sound.
        ::CTRPluginFramework::Sound *s_clip = nullptr;
#endif

        bool DoInit(void)
        {
#if SHINYHUNT_ENABLE_SOUND
            ::CTRPluginFramework::Sound::Initialize();                 // <<CALIBRATE 1>>
            s_clip   = ::CTRPluginFramework::Sound::Load(Cfg::kSoundPath); // <<CALIBRATE 2>>
            s_loaded = (s_clip != nullptr);
#else
            s_loaded = false;
#endif
            return s_loaded;
        }

        bool DoPlay(void)
        {
#if SHINYHUNT_ENABLE_SOUND
            if (!s_loaded || s_clip == nullptr)
                return false;
            s_clip->Play();
            return true;
#else
            return false;
#endif
        }
    }

    bool Sound::Init(void)       { return DoInit(); }
    bool Sound::PlayJingle(void) { return DoPlay(); }
    bool Sound::Available(void)  { return s_loaded; }
}
