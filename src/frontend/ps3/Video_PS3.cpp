// PS3 video output through the RSX framebuffer.
//
// UNVERIFIED: written without access to the PS3 toolchain, so it has never
// been compiled. It assumes the "rsxutil" helper from the PSL1GHT samples
// (rsxutil.c / rsxutil.h, copy them next to this file) with this interface:
//
//     gcmContextData* context;
//     int currentBuffer;
//     rsxBuffer buffers[MAX_BUFFERS];   // .ptr (u32*), .width, .height, .id
//     void init_screen(void* host_addr, u32 size);
//     void waitflip();
//     void flip(gcmContextData* ctx, s32 buffer_id);
//
// If your copy differs, only this file needs to change.
//
// The core's pixels are 0xAARRGGBB integers. On the big-endian PPU that is the
// byte order A,R,G,B in memory, which is what the RSX A8R8G8B8 framebuffer
// expects, so pixels are copied as-is.

#include <malloc.h>
#include <string.h>

extern "C" {
#include "rsxutil.h"
}

#include "EmuCore.h"
#include "Log.h"
#include "Video.h"

namespace melonDS::PS3Frontend::Video
{

namespace
{
constexpr u32 HostBufferSize = 32 * 1024 * 1024; // RSX IO mapping (command buffer is 1 MB of it); size used by the PSL1GHT samples
void* HostBuffer = nullptr;

void BlitScreen(const rsxBuffer& buf, const u32* src, int x0, int y0)
{
    if (!src) return;

    for (int y = 0; y < ScreenHeight; y++)
    {
        int dy = y0 + y;
        if (dy < 0 || dy >= (int)buf.height) continue;
        memcpy(buf.ptr + (size_t)dy * buf.width + x0, src + (size_t)y * ScreenWidth,
               ScreenWidth * sizeof(u32));
    }
}
}

bool Init()
{
    HostBuffer = memalign(1024 * 1024, HostBufferSize);
    if (!HostBuffer)
    {
        Log("Video: memalign of %u bytes failed", (unsigned)HostBufferSize);
        return false;
    }

    int rc = init_screen(HostBuffer, HostBufferSize);
    if (rc != 0)
    {
        // -1 rsxInit, -2 videoGetState/not enabled, -3 videoGetResolution,
        // -4 videoConfigure, -5 videoGetState, -6 rsxMemalign,
        // -7 rsxAddressToOffset, -8 gcmSetDisplayBuffer
        Log("Video: init_screen failed with code %d", rc);
        return false;
    }

    Log("Video: ready, %ux%u framebuffers", (unsigned)buffers[0].width, (unsigned)buffers[0].height);
    return true;
}

void Present(const u32* top, const u32* bottom)
{
    waitflip();

    rsxBuffer& buf = buffers[currentBuffer];
    memset(buf.ptr, 0, (size_t)buf.width * buf.height * sizeof(u32));

    // Both screens stacked and centered at 1x. TODO: scaling.
    int x0 = ((int)buf.width - ScreenWidth) / 2;
    int y0 = ((int)buf.height - ScreenHeight * 2) / 2;
    BlitScreen(buf, top, x0, y0);
    BlitScreen(buf, bottom, x0, y0 + ScreenHeight);

    flip(context, buf.id);
    currentBuffer = !currentBuffer;
}

void Shutdown()
{
    // The screen is torn down when the process exits.
    free(HostBuffer);
    HostBuffer = nullptr;
}

}
