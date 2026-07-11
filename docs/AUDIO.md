# Shiny jingle audio

**Sound now works out of the box.** The plugin ships with a ready-made jingle
and loads it automatically — you just copy one file to the SD card. The
notification **LED is still the primary indicator**; the jingle is a bonus.

## Just do this

1. Get **`shiny_jingle.bcwav`**. It comes in the CI build artifact alongside
   `shinyhunt.3gx` (it is also committed at `assets/shiny_jingle.bcwav` in the
   repo).
2. Copy it to exactly:
   ```
   sd:/luma/plugins/shiny_jingle.bcwav
   ```
   That's a single fixed path — **no per-title folder**, it does not go in the
   `<TITLEID>` subfolder with the `.3gx`.
3. Boot the game and run **Diag: test shiny jingle**. It reports `jingle
   loaded: YES` / `play started: YES` and you should hear it.

That's it. On a shiny hit the plugin plays the jingle **5×** with **1 s**
between plays (`Cfg::kJinglePlays`, `Cfg::kJingleGapFrames`).

> Pre-flight: set Rosalina's **forced volume to max** and save, so the jingle
> is audible regardless of the physical volume slider.

## If it doesn't load

`Diag: test shiny jingle` showing `jingle loaded: NO` almost always means the
file isn't at the path above (wrong folder, wrong name, or not copied). Fix the
path and retry. A missing/!unloadable jingle never crashes the plugin — it just
runs silently, and the LED still locks solid yellow on a shiny.

## How it works (for the curious / for changing the sound)

CTRPluginFramework's SD sound player is backed by **CSND + libcwav**, which
plays Nintendo's **BCWAV** (`.bcwav`) container. The plugin loads the clip with
`CTRPluginFramework::Sound(path)` — the same File → heap → `cwavLoad` path the
framework uses for its own menu sounds, which is the one that works from inside
a 3GX plugin (the audio buffer lands on the plugin heap where CSND's virtual→
physical address conversion succeeds; an embedded in-binary buffer is *not*
reliable for CSND DMA, which is why we load from a file).

The shipped jingle is a short PCM16-mono ascending chime, generated
deterministically by **`assets/make_jingle.py`** (pure Python standard library,
no external tools). Its BCWAV container layout is built to match libcwav's
parser exactly (validated field-by-field). To change the sound:

- **Tweak the built-in chime:** edit the `synth()` notes in
  `assets/make_jingle.py`, run `python3 assets/make_jingle.py`, and commit the
  regenerated `assets/shiny_jingle.bcwav`.
- **Use your own audio:** convert any WAV to BCWAV with
  [VGAudio](https://github.com/Thealexbarney/VGAudio)
  (`VGAudioCli input.wav shiny_jingle.bcwav`), keeping it **PCM16** (mono or
  stereo both load; the plugin plays it in mono). Drop the result at the SD
  path above, or replace `assets/shiny_jingle.bcwav` in the repo.

Playback tunables live in `Includes/Config.hpp`: `kJinglePlays`,
`kJingleGapFrames`, `kJingleMaxSimultPlays`, `kSoundPath`.
