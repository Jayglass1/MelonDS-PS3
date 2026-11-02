// PS3 controller input.
//
// UNVERIFIED: written without access to the PS3 toolchain, so it has never
// been compiled. It uses the PSL1GHT pad API as used in the SDK samples:
// ioPadInit / ioPadGetInfo / ioPadGetData with padInfo and padData, plus
// sysUtilRegisterCallback for the PS button.
//
// Mapping:  Circle=A  Cross=B  Triangle=X  Square=Y  L1=L  R1=R
//           D-pad, Start, Select as on the DS
//           Right stick moves the touch cursor, R3 (press stick) taps it
//           Start+Select+L1+R1 together quits

#include <io/pad.h>
#include <sysutil/sysutil.h>

#include "EmuCore.h"
#include "Input.h"
#include "Log.h"

namespace melonDS::PS3Frontend::Input
{

namespace
{
bool QuitRequested = false;

// Cursor position on the bottom screen, kept between polls.
int CursorX = ScreenWidth / 2;
int CursorY = ScreenHeight / 2;

void SysutilCallback(u64 status, u64, void*)
{
    if (status == SYSUTIL_EXIT_GAME)
        QuitRequested = true;
}

int Clamp(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}
}

bool Init()
{
    sysUtilRegisterCallback(0, SysutilCallback, nullptr);
    int rc = ioPadInit(7);
    if (rc != 0)
        Log("Input: ioPadInit failed with %d", rc);
    return rc == 0;
}

State Poll()
{
    State state;

    sysUtilCheckCallback();

    padInfo info;
    ioPadGetInfo(&info);

    if (info.status[0])
    {
        padData pad;
        ioPadGetData(0, &pad);

        if (pad.BTN_CIRCLE)   state.Keys |= Key_A;
        if (pad.BTN_CROSS)    state.Keys |= Key_B;
        if (pad.BTN_TRIANGLE) state.Keys |= Key_X;
        if (pad.BTN_SQUARE)   state.Keys |= Key_Y;
        if (pad.BTN_L1)       state.Keys |= Key_L;
        if (pad.BTN_R1)       state.Keys |= Key_R;
        if (pad.BTN_START)    state.Keys |= Key_Start;
        if (pad.BTN_SELECT)   state.Keys |= Key_Select;
        if (pad.BTN_UP)       state.Keys |= Key_Up;
        if (pad.BTN_DOWN)     state.Keys |= Key_Down;
        if (pad.BTN_LEFT)     state.Keys |= Key_Left;
        if (pad.BTN_RIGHT)    state.Keys |= Key_Right;

        // Analog values are 0..255 with ~128 at rest.
        int dx = (int)pad.ANA_R_H - 128;
        int dy = (int)pad.ANA_R_V - 128;
        if (dx > -20 && dx < 20) dx = 0;
        if (dy > -20 && dy < 20) dy = 0;
        CursorX = Clamp(CursorX + dx / 16, 0, ScreenWidth - 1);
        CursorY = Clamp(CursorY + dy / 16, 0, ScreenHeight - 1);

        state.Touching = pad.BTN_R3 != 0;
        state.TouchX = CursorX;
        state.TouchY = CursorY;

        const u32 quitCombo = Key_Start | Key_Select | Key_L | Key_R;
        if ((state.Keys & quitCombo) == quitCombo)
            QuitRequested = true;
    }

    state.Quit = QuitRequested;
    return state;
}

void Shutdown()
{
    ioPadEnd();
}

}
