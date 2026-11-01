// Audio buffering logic for the PS3 frontend, free of any PS3 calls so it can
// be unit-tested on the host (see AudioRing_test.cpp).
//
// The core produces interleaved signed 16-bit stereo frames once per emulated
// frame. The PS3 audio port wants float blocks of 256 frames, consumed at a
// fixed rate. This ring sits between them.
#ifndef PS3_FRONTEND_AUDIORING_H
#define PS3_FRONTEND_AUDIORING_H

#include <vector>

#include "types.h"

namespace melonDS::PS3Frontend
{

class AudioRing
{
public:
    // capacityFrames must be > 0. When full, the oldest audio is dropped.
    explicit AudioRing(int capacityFrames)
        : Data((size_t)capacityFrames * 2), Capacity(capacityFrames) {}

    int Frames() const { return Count; }

    // Appends interleaved s16 stereo frames. Returns how many old frames were
    // dropped to make room.
    int Push(const s16* interleaved, int frames)
    {
        if (frames <= 0) return 0;

        // Only the newest Capacity frames can survive.
        if (frames > Capacity)
        {
            interleaved += (size_t)(frames - Capacity) * 2;
            frames = Capacity;
        }

        int dropped = 0;
        if (Count + frames > Capacity)
        {
            dropped = Count + frames - Capacity;
            Read = (Read + dropped) % Capacity;
            Count -= dropped;
        }

        for (int i = 0; i < frames; i++)
        {
            int pos = (Read + Count) % Capacity;
            Data[(size_t)pos * 2 + 0] = interleaved[i * 2 + 0];
            Data[(size_t)pos * 2 + 1] = interleaved[i * 2 + 1];
            Count++;
        }
        return dropped;
    }

    // Writes `frames` interleaved float stereo frames (range -1..1) to `out`.
    // If the ring runs dry the rest is silence. Returns the frames of real
    // audio that were written.
    int PopFloat(float* out, int frames, float volume = 1.0f)
    {
        const float scale = volume / 32768.0f;
        int real = (frames < Count) ? frames : Count;

        for (int i = 0; i < real; i++)
        {
            out[i * 2 + 0] = (float)Data[(size_t)Read * 2 + 0] * scale;
            out[i * 2 + 1] = (float)Data[(size_t)Read * 2 + 1] * scale;
            Read = (Read + 1) % Capacity;
        }
        Count -= real;

        for (int i = real * 2; i < frames * 2; i++)
            out[i] = 0.0f;

        return real;
    }

    void Clear() { Read = 0; Count = 0; }

private:
    std::vector<s16> Data;
    int Capacity;
    int Read = 0;
    int Count = 0;
};

// Decides whether to fill the next audio-port block now.
//   queuedBlocks: blocks already written ahead of the one being played
//   ringFrames:   audio waiting in the ring
//   blockFrames:  frames per block (256 on the PS3)
// A block is written when there is a full block of audio, or when the port is
// about to run dry and silence has to be written instead (leaving a stale block
// would replay old audio, because the port loops over its buffer).
inline bool ShouldWriteBlock(int queuedBlocks, int ringFrames, int blockFrames)
{
    if (ringFrames >= blockFrames) return true;
    return queuedBlocks < 2;
}

}

#endif // PS3_FRONTEND_AUDIORING_H
