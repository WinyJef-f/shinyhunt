// ============================================================================
//  Sound.hpp  -  Play the shiny jingle from the SD card.
//
//  Thin wrapper over CTRPluginFramework's Sound class (backed by CSND +
//  libcwav). The clip is a PCM16 mono BCWAV loaded from a fixed SD path
//  (Cfg::kSoundPath); see assets/make_jingle.py for how it is generated and
//  docs/AUDIO.md for how to replace it.
//
//  Sound is ENABLED by default now that the real API is wired up. If the
//  BCWAV file is missing at that path, loading simply fails and PlayJingle()
//  is a no-op -- the plugin never crashes over a missing jingle. The
//  notification LED remains the primary shiny indicator; sound is a bonus.
// ============================================================================
#pragma once

namespace ShinyHunt
{
    namespace Sound
    {
        // Load the jingle once from Cfg::kSoundPath. Returns true if a real,
        // playable clip loaded (CWAV load status == SUCCESS). Safe to call
        // repeatedly; only the first call does the work.
        bool Init(void);

        // Fire-and-forget one play of the jingle. Returns true if playback
        // actually started (false if the clip never loaded).
        bool PlayJingle(void);

        // Whether a real, playable clip is currently loaded.
        bool Available(void);
    }
}
