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
#include <lv2/systime.h>

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

// Switch file: <data dir>/frameskip containing a number N renders 1 of every
// N+1 frames (e.g. 1 = half the frames, 2 = a third). Absent/0 = off.
static int ReadFrameSkip(const std::string& dir)
{
    FILE* f = fopen((dir + "frameskip").c_str(), "r");
    if (!f) return 0;
    int n = 0;
    if (fscanf(f, "%d", &n) != 1) n = 1; // empty file = skip 1
    fclose(f);
    return n < 0 ? 0 : (n > 5 ? 5 : n);
}

static long long NowUs()
{
    return (long long)sysGetSystemTime(); // microseconds
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
    const int frameSkip = ReadFrameSkip(dataDir);
    core.SetFrameSkip(frameSkip);
    Log("frameskip: %d (switch file %sframeskip)", frameSkip, dataDir.c_str());
    long long tEmu = 0, tAudio = 0, tPresent = 0, tWindow = NowUs();
    while (true)
    {
        Input::State in = Input::Poll();
        if (in.Quit)
        {
            Log("exit: quit requested after %u frames", frame);
            break;
        }

        if (frame < 3) Log("frame %u: RunFrame", frame);
        long long t0 = NowUs();
        if (!core.RunFrame(in.Keys, in.Touching, in.TouchX, in.TouchY))
        {
            Log("exit: console stopped after %u frames", frame);
            break;
        }

        long long t1 = NowUs();
        if (haveAudio)
        {
            if (frame < 3) Log("frame %u: audio", frame);
            int n = core.ReadAudio(audioBuf, 4096);
            if (n > 0) Audio::Submit(audioBuf, n);
            Audio::Service();
        }

        long long t2 = NowUs();
        if (frame < 3) Log("frame %u: present", frame);
        Video::Present(core.TopScreen(), core.BottomScreen());
        long long t3 = NowUs();
        tEmu += t1 - t0; tAudio += t2 - t1; tPresent += t3 - t2;
        if ((frame + 1) % 120 == 0)
        {
            long long wall = t3 - tWindow;
            Log("perf: %.1f fps | per frame us: emu %lld audio %lld present %lld | skip %d",
                120.0 * 1e6 / (double)wall, tEmu / 120, tAudio / 120, tPresent / 120, frameSkip);
            tEmu = tAudio = tPresent = 0;
            tWindow = t3;
        }
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
