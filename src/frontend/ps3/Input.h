// Controller input interface for the PS3 frontend.
#ifndef PS3_FRONTEND_INPUT_H
#define PS3_FRONTEND_INPUT_H

#include "types.h"

namespace melonDS::PS3Frontend::Input
{

struct State
{
    u32 Keys = 0;           // EmuCore.h KeyBit mask, 1 = pressed
    bool Touching = false;  // stylus down
    int TouchX = 0;         // bottom-screen coordinates, 0..255 / 0..191
    int TouchY = 0;
    bool Quit = false;      // exit requested (PS button / Start+Select)
};

bool Init();
State Poll();
void Shutdown();

}

#endif // PS3_FRONTEND_INPUT_H
