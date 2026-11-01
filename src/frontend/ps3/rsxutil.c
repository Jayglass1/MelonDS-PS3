#include "rsxutil.h"

#include <rsx/gcm_sys.h>
#include <string.h>
#include <sys/process.h>
#include <sysutil/video.h>
#include <unistd.h>

#define CB_SIZE 0x100000

gcmContextData* context = NULL;
int currentBuffer = 0;
rsxBuffer buffers[MAX_BUFFERS];

static uint32_t Width = 0;
static uint32_t Height = 0;
static int FirstFlip = 1;

int init_screen(void* host_addr, uint32_t host_size)
{
    if (rsxInit(&context, CB_SIZE, host_size, host_addr) != 0)
        return -1;

    videoState state;
    if (videoGetState(VIDEO_PRIMARY, 0, &state) != 0 || state.state != VIDEO_STATE_ENABLED)
        return -2;

    videoResolution res;
    if (videoGetResolution(state.displayMode.resolution, &res) != 0)
        return -3;

    videoConfiguration vconfig;
    memset(&vconfig, 0, sizeof(vconfig));
    vconfig.resolution = state.displayMode.resolution;
    vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
    vconfig.pitch = res.width * sizeof(uint32_t);
    vconfig.aspect = state.displayMode.aspect;

    if (videoConfigure(VIDEO_PRIMARY, &vconfig, NULL, 0) != 0)
        return -4;
    if (videoGetState(VIDEO_PRIMARY, 0, &state) != 0)
        return -5;

    Width = res.width;
    Height = res.height;

    gcmSetFlipMode(GCM_FLIP_VSYNC);

    for (int i = 0; i < MAX_BUFFERS; i++)
    {
        buffers[i].id = i;
        buffers[i].width = Width;
        buffers[i].height = Height;
        buffers[i].ptr = (uint32_t*)rsxMemalign(64, Width * Height * sizeof(uint32_t));
        if (!buffers[i].ptr)
            return -6;

        if (rsxAddressToOffset(buffers[i].ptr, &buffers[i].offset) != 0)
            return -7;
        if (gcmSetDisplayBuffer(i, buffers[i].offset, Width * sizeof(uint32_t), Width, Height) != 0)
            return -8;

        memset(buffers[i].ptr, 0, Width * Height * sizeof(uint32_t));
    }

    gcmResetFlipStatus();
    flip(context, MAX_BUFFERS - 1);
    return 0;
}

void waitflip(void)
{
    if (FirstFlip)
        return;

    while (gcmGetFlipStatus() != 0)
        usleep(200);
    gcmResetFlipStatus();
}

void flip(gcmContextData* ctx, int buffer_id)
{
    if (gcmSetFlip(ctx, (uint8_t)buffer_id) == 0)
    {
        rsxFlushBuffer(ctx);
        gcmSetWaitFlip(ctx);
    }
    FirstFlip = 0;
}
