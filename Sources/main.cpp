// ============================================================================
//  main.cpp  -  Plugin entry points.
//
//  Autonomous Shiny Starter Hunter for Pokemon Alpha Sapphire (ORAS).
//  See README.md and docs/ for setup, calibration and the open-question tests.
// ============================================================================
#include <3ds.h>
#include <CTRPluginFramework.hpp>

#include "Config.hpp"
#include "ShinyHunter.hpp"

namespace CTRPluginFramework
{
    // Called before main, before the game starts. We do no code patching.
    void    PatchProcess(FwkSettings &settings)
    {
        (void)settings;
    }

    // Called when the game process exits. Nothing to undo; if a shiny was
    // found we intentionally leave the LED as-is (the console may be mid-close).
    void    OnProcessExit(void)
    {
    }

    static void InitMenu(PluginMenu &menu)
    {
        // Start / stop the autonomous hunt. Also bound to the SELECT hotkey
        // (see Hunter::OnFrame) so it can be stopped instantly without
        // navigating this menu.
        menu += new MenuEntry("Start / Stop hunt", nullptr, [](MenuEntry *)
        {
            ShinyHunt::Hunter::Toggle();
        }, "Toggle the automatic soft-reset shiny hunt. Also: SELECT.");

        // ---- Diagnostics (resolve the open questions on hardware) -----------
        menu += new MenuEntry("Diag: read party now", nullptr, [](MenuEntry *)
        {
            ShinyHunt::Hunter::Diag::ReadPokemon();
        }, "Decrypt party slot 1; show PID/TID/SID/shiny.\n"
           "Use to verify the offset on this cartridge (open question #3).");

        menu += new MenuEntry("Diag: test LED (solid yellow)", nullptr, [](MenuEntry *)
        {
            ShinyHunt::Hunter::Diag::TestLed();
        }, "Set the notification LED and report the ptm:sysm result\n"
           "(open question #2).");

        menu += new MenuEntry("Diag: test shiny jingle", nullptr, [](MenuEntry *)
        {
            ShinyHunt::Hunter::Diag::TestSound();
        }, "Play the jingle once and report whether sound is available.");
    }

    int     main(void)
    {
        PluginMenu *menu = new PluginMenu("Shiny Starter Hunter", 0, 1, 0,
            "Autonomous shiny Mudkip hunt for Alpha Sapphire.\n"
            "L+R: open this menu. SELECT: instantly start/stop the hunt.\n"
            "Hunt is stopped by default on every boot -- start it explicitly.");

        // Run our FSM once per frame in the background, even while the menu is
        // closed and the game is playing normally.
        //   NOTE: if your libctrpf names this differently, this is the only
        //   line to adjust (see docs/BUILD_MAC.md). It must register a
        //   void(*)(void) called every frame.
        menu->Callback(ShinyHunt::Hunter::OnFrame);

        // Sync the menu/callback with the game's frame event.
        menu->SynchronizeWithFrame(true);

        // Pause the FSM during sleep/HOME/swap transitions instead of trying
        // to prevent them (see docs/OPEN_QUESTIONS.md Q1 and
        // ShinyHunter.cpp's Hunter::OnProcessEvent).
        Process::SetProcessEventCallback(ShinyHunt::Hunter::OnProcessEvent);

        InitMenu(*menu);

        if (ShinyHunt::Cfg::kStartHuntOnBoot)
            ShinyHunt::Hunter::Start();

        menu->Run();

        delete menu;
        return (0);
    }
}
