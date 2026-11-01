// Video output interface for the PS3 frontend.
#ifndef PS3_FRONTEND_VIDEO_H
#define PS3_FRONTEND_VIDEO_H

#include "types.h"

namespace melonDS::PS3Frontend::Video
{

// Sets up the display. Returns false on failure.
bool Init();

// Draws both screens (256x192, 0xAARRGGBB; either may be null, meaning
// black), flips, and waits for the vertical sync. That wait is what paces the
// emulator to ~60 frames per second.
void Present(const u32* top, const u32* bottom);

void Shutdown();

}

#endif // PS3_FRONTEND_VIDEO_H
