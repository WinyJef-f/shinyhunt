#!/usr/bin/env python3
"""
make_jingle.py -- build assets/shiny_jingle.bcwav from a source WAV
(default: assets/emerald_0066.wav) as a PCM16, mono, non-looped BCWAV that
libcwav (the audio library CTRPluginFramework uses) can load and play through
CSND on a 3GX plugin.

The BCWAV container layout below is validated field-by-field against libcwav's
own parser (PabloMK7/libcwav, source/cwav.c + include/internal/cwav_defs.h):
  * header magic 'CWAV', endian 0xFEFF, version 0x02010000, blockCount 2
  * INFO block magic 'INFO', encoding PCM16 (=1), one CHANNEL_INFO ref
  * DATA block magic 'DATA', raw int16 LE samples right after its 8-byte header
PCM16 is accepted by both the DSP and CSND environments, so it is the safe
choice for a plugin (which must use CSND).

The source WAV is read directly (no external tools / packages -- pure standard
library), downmixed to mono if stereo, and wrapped in the BCWAV container.
Re-run this after changing the source or the SOURCE_WAV below, then commit the
regenerated assets/shiny_jingle.bcwav (that file is what the plugin loads).
"""

import struct
import os

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE_WAV = os.path.join(HERE, "emerald_0066.wav")
OUTPUT     = os.path.join(HERE, "shiny_jingle.bcwav")

# Reference types / magics from libcwav's cwav_defs.h.
SAMPLE_DATA  = 0x1F00
INFO_BLOCK   = 0x7000
DATA_BLOCK   = 0x7001
CHANNEL_INFO = 0x7100
ENC_PCM16    = 1

MAGIC_CWAV = 0x56415743  # 'CWAV'
MAGIC_INFO = 0x4F464E49  # 'INFO'
MAGIC_DATA = 0x41544144  # 'DATA'


def read_wav_pcm16_mono(path):
    """Minimal RIFF/WAVE PCM reader -> (mono int16 bytes, sample_rate, count)."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("not a RIFF/WAVE file: %s" % path)

    fmt = None
    pcm = None
    pos = 12
    while pos + 8 <= len(data):
        cid = data[pos:pos + 4]
        csz = struct.unpack_from("<I", data, pos + 4)[0]
        body = data[pos + 8: pos + 8 + csz]
        if cid == b"fmt ":
            audio_format, num_ch, rate, _byterate, _align, bits = \
                struct.unpack_from("<HHIIHH", body, 0)
            fmt = (audio_format, num_ch, rate, bits)
        elif cid == b"data":
            pcm = body
        pos += 8 + csz + (csz & 1)  # chunks are word-aligned

    if fmt is None or pcm is None:
        raise ValueError("missing fmt/data chunk in %s" % path)
    audio_format, num_ch, rate, bits = fmt
    if audio_format != 1 or bits != 16:
        raise ValueError("only 16-bit PCM WAV supported (got fmt=%d bits=%d)"
                         % (audio_format, bits))

    total = len(pcm) // 2
    samples = struct.unpack("<%dh" % total, pcm[:total * 2])

    if num_ch == 1:
        mono = samples
    else:
        # Downmix by averaging the interleaved channels.
        frames = total // num_ch
        mono = []
        for i in range(frames):
            acc = 0
            base = i * num_ch
            for c in range(num_ch):
                acc += samples[base + c]
            mono.append(int(acc / num_ch))

    out = bytearray()
    for s in mono:
        s = max(-32768, min(32767, s))
        out += struct.pack("<h", s)
    # Pad sample data to a 0x20 boundary (harmless, keeps blocks tidy).
    while len(out) % 0x20:
        out += b"\x00\x00"
    return bytes(out), rate, len(mono)


def ref(reftype, offset):
    # cwavReference_t: u16 refType, u16 padding, u32 offset
    return struct.pack("<HHI", reftype, 0, offset)


def build(pcm, sample_rate, num_samples):
    # ---- INFO block (starts at file offset 0x40) --------------------------
    # Offsets relative to INFO start:
    #   0x00 header magic+size / 0x08 encoding,isLooped,pad / 0x0C sampleRate
    #   0x10 loopStart / 0x14 LoopEnd(=sample count) / 0x18 reserved
    #   0x1C channelInfoRefs.count  <-- channel-info offsets are relative here
    #   0x20 references[0] CHANNEL_INFO ref / 0x28 channelInfo[0] / 0x3C -> pad
    info = bytearray()
    info += struct.pack("<II", MAGIC_INFO, 0)          # magic + size (patched)
    info += struct.pack("<BBH", ENC_PCM16, 0, 0)       # encoding, isLooped, pad
    info += struct.pack("<I", sample_rate)             # sampleRate
    info += struct.pack("<I", 0)                        # loopStart
    info += struct.pack("<I", num_samples)             # LoopEnd = sample count
    info += struct.pack("<I", 0)                        # reserved
    info += struct.pack("<I", 1)                        # channel count (ref pt)
    info += ref(CHANNEL_INFO, 0x0C)                    # ref -> channelInfo @0x28
    info += ref(SAMPLE_DATA, 0)                        # samples: 0 from DATA payload
    info += ref(0, 0)                                  # adpcm ref (unused, PCM)
    info += struct.pack("<I", 0)                        # reserved
    while len(info) % 0x20:
        info += b"\x00"
    struct.pack_into("<I", info, 4, len(info))         # patch INFO size

    # ---- DATA block -------------------------------------------------------
    data = bytearray()
    data += struct.pack("<II", MAGIC_DATA, 0)          # magic + size (patched)
    data += pcm                                         # cwavData->data == here
    while len(data) % 0x20:
        data += b"\x00"
    struct.pack_into("<I", data, 4, len(data))         # patch DATA size

    # ---- Header (0x40 bytes) ---------------------------------------------
    info_off = 0x40
    data_off = info_off + len(info)
    file_size = data_off + len(data)

    hdr = bytearray()
    hdr += struct.pack("<I", MAGIC_CWAV)
    hdr += struct.pack("<HH", 0xFEFF, 0x40)           # endian, headerSize
    hdr += struct.pack("<I", 0x02010000)             # version
    hdr += struct.pack("<I", file_size)
    hdr += struct.pack("<HH", 2, 0)                   # blockCount, reserved
    hdr += ref(INFO_BLOCK, info_off) + struct.pack("<I", len(info))
    hdr += ref(DATA_BLOCK, data_off) + struct.pack("<I", len(data))
    while len(hdr) % 0x40:
        hdr += b"\x00"

    return bytes(hdr) + bytes(info) + bytes(data)


def main():
    pcm, rate, count = read_wav_pcm16_mono(SOURCE_WAV)
    blob = build(pcm, rate, count)
    with open(OUTPUT, "wb") as f:
        f.write(blob)
    print("wrote {} ({} bytes) from {} @ {} Hz, {} samples"
          .format(OUTPUT, len(blob), os.path.basename(SOURCE_WAV), rate, count))


if __name__ == "__main__":
    main()
