// ============================================================================
//  Led.cpp
// ============================================================================
#include "Led.hpp"

#include <cstring>

namespace ShinyHunt
{
    namespace
    {
        // 100-byte (0x64) RGBLedPattern, matching ctr-led-brary / MCURTC.
        struct RGBLedPattern
        {
            u8 delay;      // time per sample
            u8 smooth;     // interpolation between samples
            u8 loop_delay; // delay before the pattern loops
            u8 unknown1;
            u8 r[32];
            u8 g[32];
            u8 b[32];
        };
        static_assert(sizeof(RGBLedPattern) == 0x64, "RGBLedPattern must be 100 bytes");

        bool   s_serviceOk = false;

        // Acquire ptm:sysm, send the SetInfoLedPattern (0x8010640) request.
        Result SendPattern(const RGBLedPattern &pat)
        {
            Handle handle = 0;
            Result res = srvGetServiceHandle(&handle, "ptm:sysm");
            if (R_FAILED(res))
            {
                s_serviceOk = false;
                return res; // OPEN QUESTION #2 lands here if we are not allowed.
            }
            s_serviceOk = true;

            u32 *cmd = getThreadCommandBuffer();
            cmd[0] = 0x8010640; // SetInfoLedPattern: 25 normal words (0x64 bytes)
            memcpy(&cmd[1], &pat, sizeof(pat));

            res = svcSendSyncRequest(handle);
            if (R_SUCCEEDED(res))
                res = static_cast<Result>(cmd[1]); // IPC return value

            svcCloseHandle(handle);
            return res;
        }

        void FillSolid(RGBLedPattern &pat, u8 r, u8 g, u8 b)
        {
            memset(&pat, 0, sizeof(pat));
            pat.delay      = 0x20; // any value; all samples equal -> constant colour
            pat.smooth     = 0x00;
            pat.loop_delay = 0x00;
            for (int i = 0; i < 32; ++i)
            {
                pat.r[i] = r;
                pat.g[i] = g;
                pat.b[i] = b;
            }
        }
    }

    Result Led::SetSolid(u8 r, u8 g, u8 b)
    {
        RGBLedPattern pat;
        FillSolid(pat, r, g, b);
        return SendPattern(pat);
    }

    Result Led::Off(void)
    {
        RGBLedPattern pat;
        memset(&pat, 0, sizeof(pat));
        return SendPattern(pat);
    }

    bool Led::ServiceAvailable(void)
    {
        return s_serviceOk;
    }
}
