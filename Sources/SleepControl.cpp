// ============================================================================
//  SleepControl.cpp
//
//  libctru's high-level aptSetSleepAllowed() cannot be linked from a 3GX
//  plugin: it calls envGetAptAppId(), which reads a symbol (__apt_appid) that
//  devkitARM's normal homebrew startup (crt0 -> system init -> aptInit())
//  populates -- a path a plugin injected into an already-running game never
//  takes (confirmed via a real link failure: "undefined reference to
//  __apt_appid"). The plugin's statically-linked libctru is a "shadow" copy
//  that was never through that bootstrap.
//
//  This reimplements the same underlying APT:U IPC calls directly -- same
//  pattern as Led.cpp (own srvGetServiceHandle, own cmdbuf, no dependency on
//  libctru's internal/uninitialized statics):
//    - GetAppletManInfo (0x00050040) to read the CURRENTLY ACTIVE app's id
//      live from the service, instead of a stale/unset global.
//    - ReplySleepQuery (0x003E0080) with APTREPLY_REJECT to refuse an
//      incoming sleep query.
//
//  OPEN QUESTION #1 CAVEAT: ReplySleepQuery is normally sent in response to
//  an actual pending sleep-query notification the app receives; whether an
//  unsolicited call here has any effect (vs. only mattering right as the lid
//  closes) is NOT verified and is exactly what the hardware lid-close test in
//  docs/OPEN_QUESTIONS.md must confirm. Reassert() calls it periodically as a
//  best-effort attempt.
// ============================================================================
#include "SleepControl.hpp"

#include <3ds.h>

namespace ShinyHunt
{
    namespace
    {
        Handle s_aptHandle = 0;

        bool EnsureHandle(void)
        {
            if (s_aptHandle != 0)
                return true;
            return R_SUCCEEDED(srvGetServiceHandle(&s_aptHandle, "APT:U"));
        }

        // GetAppletManInfo(APTPOS_NONE) -> current (active) app's NS_APPID.
        bool GetActiveAppId(u32 &outAppId)
        {
            if (!EnsureHandle())
                return false;

            u32 *cmd = getThreadCommandBuffer();
            cmd[0] = 0x00050040; // GetAppletManInfo: 1 normal param
            cmd[1] = 0;          // APTPOS_NONE

            if (R_FAILED(svcSendSyncRequest(s_aptHandle)))
                return false;
            if (R_FAILED(static_cast<Result>(cmd[1])))
                return false;

            outAppId = cmd[5]; // "Current AppID" per APT:GetAppletManInfo
            return true;
        }

        // ReplySleepQuery(appId, APTREPLY_REJECT).
        void RejectSleepQuery(void)
        {
            u32 appId;
            if (!GetActiveAppId(appId))
                return;

            u32 *cmd = getThreadCommandBuffer();
            cmd[0] = 0x003E0080; // ReplySleepQuery: 2 normal params
            cmd[1] = appId;
            cmd[2] = 0;          // APTREPLY_REJECT

            svcSendSyncRequest(s_aptHandle);
        }
    }

    void SleepControl::KeepAwake(void)
    {
        RejectSleepQuery();
    }

    void SleepControl::Reassert(void)
    {
        // A fresh IPC round-trip every single frame is wasteful for what is a
        // best-effort, infrequently-relevant call; once every couple of
        // seconds is plenty even at 60 fps.
        static u32 counter = 0;
        if ((++counter % 120) == 0)
            RejectSleepQuery();
    }
}
