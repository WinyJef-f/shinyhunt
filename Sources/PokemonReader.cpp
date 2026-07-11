// ============================================================================
//  PokemonReader.cpp
//
//  Gen 6 (PK6) storage decryption, ported from the algorithm used by
//  Gen6CTRPluginFramework and PKHeX:
//    * key  = first u32 of the block (the "encryption constant")
//    * PRNG = LCG  seed = 0x41C64E6D * seed + 0x00006073,  advanced once per
//             2 bytes over [0x08, 0xE8), XORing the high 16 bits of the state
//    * blocks = four 56-byte blocks starting at 0x08 are shuffled; the shuffle
//               index is ((key >> 13) & 0x1F) % 24 and un-shuffled with the
//               standard 4x24 permutation table.
// ============================================================================
#include "PokemonReader.hpp"
#include "Config.hpp"

#include <CTRPluginFramework.hpp>
#include <cstring>

namespace ShinyHunt
{
    using namespace CTRPluginFramework;

    namespace
    {
        constexpr u32 kBlockSize = 56; // 0x38, four of these from 0x08..0xE8

        // blockPosition[logicalBlock][shuffleIndex] = physical slot it sits in.
        // (Same table as Gen6CTRPluginFramework / PKHeX.)
        const u8 kBlockPosition[4][24] = {
            {0, 0, 0, 0, 0, 0, 1, 1, 2, 3, 2, 3, 1, 1, 2, 3, 2, 3, 1, 1, 2, 3, 2, 3},
            {1, 1, 2, 3, 2, 3, 0, 0, 0, 0, 0, 0, 2, 3, 1, 1, 3, 2, 2, 3, 1, 1, 3, 2},
            {2, 3, 1, 1, 3, 2, 2, 3, 1, 1, 3, 2, 0, 0, 0, 0, 0, 0, 3, 2, 3, 2, 1, 1},
            {3, 2, 3, 2, 1, 1, 3, 2, 3, 2, 1, 1, 3, 2, 3, 2, 1, 1, 0, 0, 0, 0, 0, 0}
        };

        // Undo the LCG stream cipher over [0x08, 0xE8) in place.
        void Decrypt(u8 *data)
        {
            u32 seed = *reinterpret_cast<u32 *>(data); // encryption constant
            for (u32 i = 8; i < Cfg::kPk6StoredSize; i += 2)
            {
                seed = (0x41C64E6D * seed) + 0x00006073;
                data[i]     ^= static_cast<u8>(seed >> 16);
                data[i + 1] ^= static_cast<u8>(seed >> 24);
            }
        }

        // Un-shuffle the four 56-byte blocks back into logical order.
        // 'src' is the decrypted-but-shuffled buffer; writes ordered result to 'dst'.
        void Unshuffle(const u8 *src, u8 *dst, u32 shuffle)
        {
            memcpy(dst, src, 8); // header (EC, sanity, checksum) is not shuffled
            for (u32 logical = 0; logical < 4; ++logical)
            {
                u32 physical = kBlockPosition[logical][shuffle];
                memcpy(dst + 8 + logical * kBlockSize,
                       src + 8 + physical * kBlockSize,
                       kBlockSize);
            }
        }

        // PK6 16-bit checksum over the ordered payload [0x08, 0xE8).
        u16 Checksum(const u8 *ordered)
        {
            u16 chk = 0;
            for (u32 i = 8; i < Cfg::kPk6StoredSize; i += 2)
                chk += *reinterpret_cast<const u16 *>(ordered + i);
            return chk;
        }
    }

    u32 PokemonReader::ComputeShinyValue(u16 tid, u16 sid, u32 pid)
    {
        return static_cast<u32>(tid) ^ static_cast<u32>(sid) ^
               ((pid >> 16) & 0xFFFF) ^ (pid & 0xFFFF);
    }

    bool PokemonReader::Read(u32 address, PkmData &out)
    {
        memset(&out, 0, sizeof(out));

        u8 raw[Cfg::kPk6StoredSize];
        // Process::CopyMemory copies from the game's address space into ours.
        if (!Process::CopyMemory(raw, reinterpret_cast<void *>(address), Cfg::kPk6StoredSize))
            return false;

        u32 encKey  = *reinterpret_cast<u32 *>(raw);
        u32 shuffle = ((encKey >> 13) & 0x1F) % 24;

        Decrypt(raw);

        u8 ordered[Cfg::kPk6StoredSize];
        Unshuffle(raw, ordered, shuffle);

        u16 storedChecksum = *reinterpret_cast<u16 *>(ordered + Cfg::kOffChecksum);
        u16 species        = *reinterpret_cast<u16 *>(ordered + Cfg::kOffSpecies);

        out.encKey  = encKey;
        out.species = species;
        out.tid     = *reinterpret_cast<u16 *>(ordered + Cfg::kOffTID);
        out.sid     = *reinterpret_cast<u16 *>(ordered + Cfg::kOffSID);
        out.pid     = *reinterpret_cast<u32 *>(ordered + Cfg::kOffPID);

        // "Valid" == checksum matches AND species is in the real dex range.
        // A freshly generated / empty slot fails one of these, which is exactly
        // the gate the hunt loop uses to know slot 1 is populated for real.
        bool checksumOk = (Checksum(ordered) == storedChecksum);
        bool speciesOk  = (species >= 1 && species <= 721 && out.pid != 0);
        out.valid = checksumOk && speciesOk;

        out.shinyValue = ComputeShinyValue(out.tid, out.sid, out.pid);
        out.shiny      = (out.shinyValue < Cfg::kShinyThreshold);

        return out.valid;
    }

    bool PokemonReader::ReadPartySlot1(PkmData &out)
    {
        return Read(Cfg::kPartySlot1Addr, out);
    }
}
