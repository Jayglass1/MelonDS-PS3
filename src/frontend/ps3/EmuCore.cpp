#include "EmuCore.h"

#include <optional>
#include <utility>

#include "Args.h"
#include "NDS.h"
#include "NDSCart.h"
#include "Platform.h"

namespace melonDS::PS3Frontend
{

using namespace melonDS::Platform;

namespace
{

// Reads a whole file into memory. Returns false if it can't be opened.
bool ReadWholeFile(const std::string& path, std::unique_ptr<u8[]>& data, u32& len)
{
    FileHandle* f = OpenFile(path, FileMode::Read);
    if (!f) return false;

    u64 size = FileLength(f);
    if (size == 0 || size > 0x20000000ull) // 512 MB is the largest DS ROM
    {
        CloseFile(f);
        return false;
    }

    data = std::make_unique<u8[]>((size_t)size);
    FileRewind(f);
    u64 got = FileRead(data.get(), size, 1, f);
    CloseFile(f);
    if (got != 1) return false;

    len = (u32)size;
    return true;
}

std::string WithoutExtension(const std::string& path)
{
    size_t dot = path.find_last_of('.');
    size_t slash = path.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return path;
    return path.substr(0, dot);
}

std::string BaseName(const std::string& path)
{
    size_t slash = path.find_last_of("/\\");
    return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

}

EmuCore::EmuCore() = default;
EmuCore::~EmuCore() = default;

bool EmuCore::CreateConsole(std::string& error)
{
    NDSArgs args; // FreeBIOS + generated firmware + software renderer
    args.JIT = std::nullopt; // no PowerPC JIT exists
    Console = std::make_unique<NDS>(std::move(args), &Ctx_);
    if (!Console)
    {
        error = "Could not create the console.";
        return false;
    }
    return true;
}

bool EmuCore::LoadROM(const std::string& romPath, std::string& error)
{
    std::unique_ptr<u8[]> rom;
    u32 romLen = 0;
    if (!ReadWholeFile(romPath, rom, romLen))
    {
        error = "Could not read ROM: " + romPath;
        return false;
    }

    Ctx_.NDSSavePath = WithoutExtension(romPath) + ".sav";

    std::unique_ptr<u8[]> save;
    u32 saveLen = 0;
    if (FileExists(Ctx_.NDSSavePath))
    {
        if (!ReadWholeFile(Ctx_.NDSSavePath, save, saveLen))
        {
            save.reset();
            saveLen = 0;
        }
    }

    NDSCart::NDSCartArgs cartArgs;
    cartArgs.SRAM = std::move(save);
    cartArgs.SRAMLength = saveLen;

    auto cart = NDSCart::ParseROM(std::move(rom), romLen, &Ctx_, std::move(cartArgs));
    if (!cart)
    {
        error = "Not a valid DS ROM: " + romPath;
        return false;
    }

    if (!CreateConsole(error))
        return false;

    Console->SetNDSCart(std::move(cart));
    Console->Reset();
    Console->SetupDirectBoot(BaseName(romPath));
    Console->Start();
    return true;
}

bool EmuCore::BootWithoutCart(std::string& error)
{
    if (!CreateConsole(error))
        return false;

    Console->Reset();
    Console->Start();
    return true;
}

int EmuCore::ReadAudio(s16* out, int maxFrames)
{
    if (!Console || maxFrames <= 0) return 0;

    int avail = Console->SPU.GetOutputSize();
    if (avail > maxFrames) avail = maxFrames;
    return (avail > 0) ? Console->SPU.ReadOutput(out, avail) : 0;
}

void EmuCore::SetFrameSkip(int n)
{
    if (Console)
        Console->GPU.FrameSkip = n < 0 ? 0 : n;
}

bool EmuCore::RunFrame(u32 pressedKeys, bool touching, int touchX, int touchY)
{
    if (!Console || Ctx_.StopRequested)
        return false;

    // The key register is active-low: a pressed key is a 0 bit.
    Console->SetKeyMask((~pressedKeys) & 0xFFF);

    if (touching)
    {
        if (touchX < 0) touchX = 0;
        if (touchX > ScreenWidth - 1) touchX = ScreenWidth - 1;
        if (touchY < 0) touchY = 0;
        if (touchY > ScreenHeight - 1) touchY = ScreenHeight - 1;
        Console->TouchScreen((u16)touchX, (u16)touchY);
    }
    else
    {
        Console->ReleaseScreen();
    }

    Console->RunFrame();

    void* top = nullptr;
    void* bottom = nullptr;
    if (Console->GPU.GetFramebuffers(&top, &bottom))
    {
        Top = static_cast<const u32*>(top);
        Bottom = static_cast<const u32*>(bottom);
    }
    else
    {
        Top = Bottom = nullptr;
    }

    return !Ctx_.StopRequested;
}

}
