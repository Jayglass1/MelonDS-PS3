// Minimal RSX display setup for the PS3 frontend: double-buffered
// 32-bit framebuffers with vsync flipping. Written against the PSL1GHT
// rsx/gcm and sysutil/video headers (the toolchain ships no helper for this).
#ifndef PS3_RSXUTIL_H
#define PS3_RSXUTIL_H

#include <rsx/rsx.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_BUFFERS 2

typedef struct
{
    int id;
    uint32_t* ptr;      // framebuffer memory (A8R8G8B8)
    uint32_t offset;    // RSX offset of ptr
    uint32_t width;
    uint32_t height;
} rsxBuffer;

extern gcmContextData* context;
extern int currentBuffer;
extern rsxBuffer buffers[MAX_BUFFERS];

// host_addr must be a 1 MB-aligned block of host_size bytes (command buffer
// and IO mapping for the RSX). Returns 0 on success.
int init_screen(void* host_addr, uint32_t host_size);

// Blocks until the previously queued flip has happened.
void waitflip(void);

// Queues a flip to the given buffer.
void flip(gcmContextData* ctx, int buffer_id);

#ifdef __cplusplus
}
#endif

#endif
