// PS3 audio output through the libaudio port API.
//
// UNVERIFIED: compiles against the PSL1GHT headers but has never been run.
// Assumptions that need checking on real hardware or RPCS3:
//  - audioPortConfig::readIndex is the address of a 64-bit counter holding the
//    block the hardware is currently reading (as in the PSL1GHT audio
//    sample);
//  - the port runs and advances without a notify event queue.
//
// Scheduling lives in AudioRing.h (unit-tested on the host). This file only
// moves float blocks into the port.

#include <audio/audio.h>
#include <stdint.h>
#include <string.h>

#include "Audio.h"
#include "AudioRing.h"

namespace melonDS::PS3Frontend::Audio
{

namespace
{
constexpr int Channels = 2;
constexpr int BlockFrames = AUDIO_BLOCK_SAMPLES; // 256
constexpr int RingFrames = 4096;                 // ~85 ms at 48 kHz

bool Open = false;
u32 Port = 0;
audioPortConfig Config;
u32 WriteBlock = 0;
AudioRing* Ring = nullptr;

float* BlockPtr(u32 index)
{
    return (float*)((uintptr_t)Config.audioDataStart +
                    (size_t)index * Channels * BlockFrames * sizeof(float));
}

u32 CurrentReadBlock()
{
    return (u32)(*(volatile u64*)(uintptr_t)Config.readIndex);
}
}

bool Init()
{
    if (audioInit() != 0)
        return false;

    audioPortParam param;
    memset(&param, 0, sizeof(param));
    param.numChannels = AUDIO_PORT_2CH;
    param.numBlocks = AUDIO_BLOCK_8;
    param.attrib = 0;
    param.level = 1.0f;

    if (audioPortOpen(&param, &Port) != 0)
    {
        audioQuit();
        return false;
    }
    if (audioGetPortConfig(Port, &Config) != 0)
    {
        audioPortClose(Port);
        audioQuit();
        return false;
    }

    // Start from silence everywhere.
    memset((void*)(uintptr_t)Config.audioDataStart, 0,
           (size_t)Config.numBlocks * Channels * BlockFrames * sizeof(float));

    Ring = new AudioRing(RingFrames);

    if (audioPortStart(Port) != 0)
    {
        delete Ring;
        Ring = nullptr;
        audioPortClose(Port);
        audioQuit();
        return false;
    }

    // Stay a few blocks ahead of the hardware's read position.
    WriteBlock = (CurrentReadBlock() + 3) % (u32)Config.numBlocks;
    Open = true;
    return true;
}

void Submit(const s16* interleaved, int frames)
{
    if (Open && Ring)
        Ring->Push(interleaved, frames);
}

void Service()
{
    if (!Open)
        return;

    const u32 numBlocks = (u32)Config.numBlocks;
    const u32 cur = CurrentReadBlock() % numBlocks;

    // Every block from WriteBlock up to (not including) the one being read
    // has already been played, so it is free to overwrite.
    while (WriteBlock != cur)
    {
        int queued = (int)((WriteBlock + numBlocks - cur) % numBlocks);
        if (!ShouldWriteBlock(queued, Ring->Frames(), BlockFrames))
            break;

        Ring->PopFloat(BlockPtr(WriteBlock), BlockFrames);
        WriteBlock = (WriteBlock + 1) % numBlocks;
    }
}

void Shutdown()
{
    if (!Open)
        return;

    Open = false;
    audioPortStop(Port);
    audioPortClose(Port);
    audioQuit();
    delete Ring;
    Ring = nullptr;
}

}
