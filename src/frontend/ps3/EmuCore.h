// Thin wrapper around melonDS::NDS used by both the PS3 frontend and the
// headless host runner, so the same boot/run code is exercised on x86.
#ifndef PS3_FRONTEND_EMUCORE_H
#define PS3_FRONTEND_EMUCORE_H

#include <memory>
#include <string>

#include "FrontendContext.h"
#include "types.h"

namespace melonDS { class NDS; }

namespace melonDS::PS3Frontend
{

// Bit positions in the "pressed keys" mask passed to RunFrame
// (1 = pressed). They match the DS key register layout.
enum KeyBit : u32
{
    Key_A      = 1u << 0,
    Key_B      = 1u << 1,
    Key_Select = 1u << 2,
    Key_Start  = 1u << 3,
    Key_Right  = 1u << 4,
    Key_Left   = 1u << 5,
    Key_Up     = 1u << 6,
    Key_Down   = 1u << 7,
    Key_R      = 1u << 8,
    Key_L      = 1u << 9,
    Key_X      = 1u << 10,
    Key_Y      = 1u << 11,
};

constexpr int ScreenWidth = 256;
constexpr int ScreenHeight = 192;

class EmuCore
{
public:
    EmuCore();
    ~EmuCore();

    // Boot a cartridge. The save file is "<rom path without extension>.sav".
    bool LoadROM(const std::string& romPath, std::string& error);

    // Boot the console with no cartridge inserted (used for smoke tests).
    bool BootWithoutCart(std::string& error);

    // Runs one DS frame. Returns false once the console has stopped.
    bool RunFrame(u32 pressedKeys, bool touching, int touchX, int touchY);

    // 256x192, 32-bit BGRA (0xAARRGGBB as a host integer). Null before the
    // first frame, or if the renderer has no RAM framebuffers.
    const u32* TopScreen() const { return Top; }
    const u32* BottomScreen() const { return Bottom; }

    Context& Ctx() { return Ctx_; }

private:
    bool CreateConsole(std::string& error);

    Context Ctx_;
    std::unique_ptr<NDS> Console;
    const u32* Top = nullptr;
    const u32* Bottom = nullptr;
};

}

#endif // PS3_FRONTEND_EMUCORE_H
