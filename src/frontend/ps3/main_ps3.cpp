// melonDS PS3 frontend entry point.
//
// UNVERIFIED: compiles and links with the PS3 toolchain but has never been run
// (see README.md). The emulation side (EmuCore, Platform_PS3, Sync_single) is
// also built and run on the host by the headless runner; the PS3-specific
// code is Video_PS3.cpp, Input_PS3.cpp, Audio_PS3.cpp, rsxutil.* and this file.

#include <stdio.h>
#include <string>

#include "Audio.h"
#include "EmuCore.h"
#include "FrontendContext.h"
#include "Input.h"
#include "Video.h"

using namespace melonDS;
using namespace melonDS::PS3Frontend;

static const char* DefaultDataDir = "/dev_hdd0/game/MELONDS00/USRDIR/";

// There is no text rendering yet, so errors go to a file next to the ROM.
static void ReportError(const std::string& dir, const std::string& message)
{
    FILE* f = fopen((dir + "error.txt").c_str(), "w");
    if (!f) return;
    fprintf(f, "%s\n", message.c_str());
    fclose(f);
}

int main(int argc, char** argv)
{
    std::string dataDir = DefaultDataDir;
    SetDataDir(dataDir);

    // First argument overrides the ROM path, otherwise <data dir>/rom.nds.
    std::string romPath = (argc > 1) ? argv[1] : dataDir + "rom.nds";

    if (!Video::Init())
        return 1;
    // Audio is optional: if it fails the emulator runs silently.
    const bool haveAudio = Audio::Init();

    if (!Input::Init())
    {
        ReportError(dataDir, "Could not initialize controller input.");
        return 1;
    }

    EmuCore core;
    std::string error;
    if (!core.LoadROM(romPath, error))
    {
        ReportError(dataDir, error);
        // Keep the process alive on a black screen until the user quits.
        while (!Input::Poll().Quit)
            Video::Present(nullptr, nullptr);

        Input::Shutdown();
        Video::Shutdown();
        return 1;
    }

    // Single-threaded loop: emulate one frame, queue its audio, draw it, and
    // let Present's vsync wait hold us at 60 Hz.
    static s16 audioBuf[4096 * 2];
    while (true)
    {
        Input::State in = Input::Poll();
        if (in.Quit)
            break;

        if (!core.RunFrame(in.Keys, in.Touching, in.TouchX, in.TouchY))
            break;

        if (haveAudio)
        {
            int n = core.ReadAudio(audioBuf, 4096);
            if (n > 0) Audio::Submit(audioBuf, n);
            Audio::Service();
        }

        Video::Present(core.TopScreen(), core.BottomScreen());
    }

    Audio::Shutdown();
    Input::Shutdown();
    Video::Shutdown();
    return 0;
}
