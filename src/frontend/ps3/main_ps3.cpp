// melonDS PS3 frontend entry point.
//
// UNVERIFIED: compiles and links with the PS3 toolchain but has never been run
// (see README.md). The emulation side (EmuCore, Platform_PS3, Sync_single) is
// also built and run on the host by the headless runner; the PS3-specific
// code is Video_PS3.cpp, Input_PS3.cpp, Audio_PS3.cpp, rsxutil.* and this file.

#include <exception>
#include <stdio.h>
#include <string>

#include <sys/process.h>

#include "Audio.h"
#include "EmuCore.h"
#include "FrontendContext.h"
#include "Input.h"
#include "Log.h"
#include "Video.h"

using namespace melonDS;
using namespace melonDS::PS3Frontend;

// Explicit priority and the largest stack the loader supports (1 MB).
SYS_PROCESS_PARAM(1001, SYS_PROCESS_SPAWN_STACK_SIZE_1M);

static const char* DefaultDataDir = "/dev_hdd0/game/MELONDS00/USRDIR/";

// There is no text rendering yet, so errors go to a file next to the ROM.
static void ReportError(const std::string& dir, const std::string& message)
{
    FILE* f = fopen((dir + "error.txt").c_str(), "w");
    if (!f) return;
    fprintf(f, "%s\n", message.c_str());
    fclose(f);
}

static int Run(int argc, char** argv)
{
    std::string dataDir = DefaultDataDir;
    SetDataDir(dataDir);

    // First argument overrides the ROM path, otherwise <data dir>/rom.nds.
    std::string romPath = (argc > 1) ? argv[1] : dataDir + "rom.nds";
    Log("main: rom path %s", romPath.c_str());

    Log("stage: video init");
    if (!Video::Init())
    {
        Log("FATAL: video init failed");
        return 1;
    }

    // Audio is optional: if it fails the emulator runs silently.
    Log("stage: audio init");
    const bool haveAudio = Audio::Init();
    Log("audio: %s", haveAudio ? "ok" : "unavailable, running silent");

    Log("stage: input init");
    if (!Input::Init())
    {
        ReportError(dataDir, "Could not initialize controller input.");
        Log("FATAL: input init failed");
        return 1;
    }

    Log("stage: load ROM");
    EmuCore core;
    std::string error;
    if (!core.LoadROM(romPath, error))
    {
        ReportError(dataDir, error);
        Log("ROM load failed: %s (showing black screen; Start+Select+L1+R1 quits)", error.c_str());
        // Keep the process alive on a black screen until the user quits.
        while (!Input::Poll().Quit)
            Video::Present(nullptr, nullptr);

        Input::Shutdown();
        Video::Shutdown();
        return 1;
    }
    Log("stage: ROM loaded, entering main loop");

    // Single-threaded loop: emulate one frame, queue its audio, draw it, and
    // let Present's vsync wait hold us at 60 Hz.
    static s16 audioBuf[4096 * 2];
    unsigned frame = 0;
    while (true)
    {
        Input::State in = Input::Poll();
        if (in.Quit)
        {
            Log("exit: quit requested after %u frames", frame);
            break;
        }

        if (frame < 3) Log("frame %u: RunFrame", frame);
        if (!core.RunFrame(in.Keys, in.Touching, in.TouchX, in.TouchY))
        {
            Log("exit: console stopped after %u frames", frame);
            break;
        }

        if (haveAudio)
        {
            if (frame < 3) Log("frame %u: audio", frame);
            int n = core.ReadAudio(audioBuf, 4096);
            if (n > 0) Audio::Submit(audioBuf, n);
            Audio::Service();
        }

        if (frame < 3) Log("frame %u: present", frame);
        Video::Present(core.TopScreen(), core.BottomScreen());
        if (frame < 3) Log("frame %u: done", frame);
        else if (frame % 600 == 0) Log("frame %u", frame);
        frame++;
    }

    Audio::Shutdown();
    Input::Shutdown();
    Video::Shutdown();
    return 0;
}

int main(int argc, char** argv)
{
    LogReset();
    Log("main: entered");
    try
    {
        int rc = Run(argc, argv);
        Log("main: returning %d", rc);
        return rc;
    }
    catch (const std::exception& e)
    {
        Log("FATAL: uncaught exception: %s", e.what());
    }
    catch (...)
    {
        Log("FATAL: uncaught unknown exception");
    }
    return 1;
}
