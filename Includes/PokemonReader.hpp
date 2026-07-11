// ============================================================================
//  PokemonReader.hpp  -  Read + decrypt a Gen 6 party Pokemon from RAM.
//
//  Ports the PK6 decryption (LCG + block unshuffle) used by
//  Gen6CTRPluginFramework / PKHeX, then exposes the few fields the hunt needs.
// ============================================================================
#pragma once

#include <3ds.h>

namespace ShinyHunt
{
    struct PkmData
    {
        bool valid;      // decrypt + checksum passed
        u16  species;    // national dex number
        u16  tid;
        u16  sid;
        u32  pid;
        u32  encKey;     // encryption constant (for debugging)
        u32  shinyValue; // TID ^ SID ^ (PID>>16) ^ (PID & 0xFFFF)
        bool shiny;      // shinyValue < Cfg::kShinyThreshold
    };

    namespace PokemonReader
    {
        // Read the encrypted PK6 at 'address', decrypt it, and fill 'out'.
        // Returns false (and out.valid == false) if the memory could not be
        // read or the block does not decrypt to a sane Pokemon.
        bool Read(u32 address, PkmData &out);

        // Convenience: read party slot 1 (Cfg::kPartySlot1Addr).
        bool ReadPartySlot1(PkmData &out);

        // Compute shininess from raw ids (exposed for tests / diagnostics).
        u32  ComputeShinyValue(u16 tid, u16 sid, u32 pid);
    }
}
