// ============================================================================
//  Sound.hpp  -  Play the shiny jingle from the SD card.
//
//  CTRPF ships an SD-card sound player (System/Sound.hpp, backed by CSND +
//  libcwav) in recent ThePixellizerOSS / PabloMK7 builds. The EXACT class/
//  method names vary between libctrpf versions and could not be pinned from
//  docs, so the real call is isolated in Sound.cpp behind SHINYHUNT_ENABLE_SOUND
//  and defaults OFF, guaranteeing the plugin always builds.
//
//  The notification LED is the primary, always-working shiny indicator. Sound
//  is a bonus: enable it after confirming your libctrpf's Sound API and the
//  audio format it expects (docs/AUDIO.md).
// ============================================================================
#pragma once

namespace ShinyHunt
{
    namespace Sound
    {
        // Load the jingle once (no-op if sound is disabled at compile time).
        // Returns true if a real, playable sound was loaded.
        bool Init(void);

        // Fire-and-forget one play of the jingle. Returns true if it actually
        // started playback (false when sound is disabled / failed to load).
        bool PlayJingle(void);

        // Whether real sound support is compiled in AND a clip loaded.
        bool Available(void);
    }
}
