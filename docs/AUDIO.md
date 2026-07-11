# Shiny jingle audio

The notification **LED is the primary indicator**; the jingle is a bonus and is
**compiled out by default** so the plugin always builds cleanly. Enable it only
after the steps below.

Your file `assets/emerald_0066.wav` is **RIFF PCM, 16-bit, stereo, 44100 Hz,
~0.77 s** — a perfectly good source. What it needs to become depends on what
your installed `libctrpf` expects.

## What format does CTRPF want?

CTRPF's SD sound player is backed by **CSND + libcwav**, and libcwav plays
Nintendo's **BCWAV** (`.bcwav`) container — not raw RIFF WAV. So in almost all
cases you convert `emerald_0066.wav` → `shiny_jingle.bcwav`.

**Confirm against your actual install** by opening the header:
`$DEVKITPRO/libctrpf/include/CTRPluginFramework/System/Sound.hpp` (path may vary).
Look at the `Load`/`Play` signatures and any comment about the expected file
type. Match the two `<<CALIBRATE>>` lines in `Sources/Sound.cpp` to it.

## Convert on the Mac

Recommended: **VGAudio** (cross-platform, makes clean BCWAV from PCM):

```sh
# VGAudioCli is a .NET tool; install the .NET runtime, then:
VGAudioCli assets/emerald_0066.wav shiny_jingle.bcwav
```

If your libctrpf build instead accepts plain PCM/WAV, you can normalize with
ffmpeg (keep 16-bit; downmix/downsample only if the header says to):

```sh
ffmpeg -i assets/emerald_0066.wav -acodec pcm_s16le shiny_jingle.wav
# mono / lower rate variant, only if needed:
# ffmpeg -i assets/emerald_0066.wav -ac 1 -ar 32000 -acodec pcm_s16le shiny_jingle.wav
```

## Deploy + enable

1. Copy the converted file to the SD path that matches `Cfg::kSoundPath`
   (default `sd:/luma/plugins/shiny_jingle.bcwav`). Change the constant if you
   put it elsewhere or used a different extension.
2. In `Sources/Sound.cpp`, flip the top guard on:
   ```cpp
   #define SHINYHUNT_ENABLE_SOUND 1
   ```
3. Make the two `<<CALIBRATE>>` lines match your `Sound.hpp` (e.g. the class name
   might be `Sound`, `SoundEngine`, or the load call might be `Play(path)`
   directly). The wrapper only needs: initialize once, load the clip, play it.
4. `make re`, redeploy the `.3gx`.
5. Verify with **Diag: test shiny jingle** — it reports whether the clip loaded
   and playback started.

The plugin plays the jingle **5×** with **1 s** between plays on a shiny hit
(`Cfg::kJinglePlays`, `Cfg::kJingleGapFrames`). With the Rosalina forced-volume
override set to max (pre-flight), it is audible regardless of the physical
slider.

> If sound cannot be made to work from inside the process, nothing else breaks —
> the LED still locks solid yellow on a shiny, which is the indicator you said
> matters most for a lid-closed overnight run.
