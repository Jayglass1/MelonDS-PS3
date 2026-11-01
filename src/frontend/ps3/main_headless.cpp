// Headless host runner for the PS3 frontend code path.
//
//   melonds-ps3-headless [rom.nds] [frames] [output-prefix]
//
// Boots the ROM (or the bare console if none is given), runs the requested
// number of frames, prints a checksum of the final screens, and writes them as
// <prefix>_top.ppm / <prefix>_bottom.ppm. The checksum makes it easy to
// compare runs, e.g. between the threaded and single-threaded sync backends.

#include <cstdio>
#include <cstdlib>
#include <string>

#include "EmuCore.h"

using namespace melonDS;
using namespace melonDS::PS3Frontend;

static u64 Checksum(const u32* fb)
{
    u64 h = 1469598103934665603ull; // FNV-1a over the pixel values
    for (int i = 0; i < ScreenWidth * ScreenHeight; i++)
    {
        h ^= fb[i];
        h *= 1099511628211ull;
    }
    return h;
}

static bool WritePPM(const std::string& path, const u32* fb)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;

    fprintf(f, "P6\n%d %d\n255\n", ScreenWidth, ScreenHeight);
    for (int i = 0; i < ScreenWidth * ScreenHeight; i++)
    {
        u32 p = fb[i]; // 0xAARRGGBB
        unsigned char rgb[3] = {(unsigned char)(p >> 16), (unsigned char)(p >> 8), (unsigned char)p};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return true;
}

int main(int argc, char** argv)
{
    std::string rom = (argc > 1) ? argv[1] : "";
    int frames = (argc > 2) ? atoi(argv[2]) : 120;
    std::string prefix = (argc > 3) ? argv[3] : "frame";
    if (frames < 1) frames = 1;

    EmuCore core;
    std::string error;
    bool ok = rom.empty() ? core.BootWithoutCart(error) : core.LoadROM(rom, error);
    if (!ok)
    {
        fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }

    int ran = 0;
    for (int i = 0; i < frames; i++)
    {
        if (!core.RunFrame(0, false, 0, 0))
            break;
        ran++;
    }

    printf("ran %d frame(s)%s\n", ran, core.Ctx().StopRequested ? " (console stopped)" : "");

    if (!core.TopScreen() || !core.BottomScreen())
    {
        fprintf(stderr, "error: no framebuffer available\n");
        return 1;
    }

    printf("top    checksum %016llx\n", (unsigned long long)Checksum(core.TopScreen()));
    printf("bottom checksum %016llx\n", (unsigned long long)Checksum(core.BottomScreen()));

    if (!WritePPM(prefix + "_top.ppm", core.TopScreen()) ||
        !WritePPM(prefix + "_bottom.ppm", core.BottomScreen()))
    {
        fprintf(stderr, "error: could not write output images\n");
        return 1;
    }
    return 0;
}
