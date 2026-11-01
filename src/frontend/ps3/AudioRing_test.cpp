// Host unit test for AudioRing.h:  g++ -std=c++17 -I../.. AudioRing_test.cpp
#include <cmath>
#include <cstdio>

#include "AudioRing.h"

using namespace melonDS;
using namespace melonDS::PS3Frontend;

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main()
{
    {   // basic push/pop, scaling, channel order
        AudioRing r(8);
        s16 in[6] = {32767, -32768, 16384, -16384, 0, 8192}; // 3 frames
        CHECK(r.Push(in, 3) == 0);
        CHECK(r.Frames() == 3);

        float out[6];
        CHECK(r.PopFloat(out, 3) == 3);
        CHECK(std::fabs(out[0] - 32767.0f / 32768.0f) < 1e-6f);
        CHECK(out[1] == -1.0f);
        CHECK(out[2] == 0.5f && out[3] == -0.5f);
        CHECK(out[4] == 0.0f && out[5] == 0.25f);
        CHECK(r.Frames() == 0);
    }
    {   // underrun pads with silence and reports only real frames
        AudioRing r(8);
        s16 in[4] = {1000, 2000, 3000, 4000};
        r.Push(in, 2);
        float out[10];
        for (float& f : out) f = 9.0f;
        CHECK(r.PopFloat(out, 5) == 2);
        CHECK(out[0] > 0 && out[3] > 0);
        for (int i = 4; i < 10; i++) CHECK(out[i] == 0.0f);
    }
    {   // wrap-around keeps order
        AudioRing r(4);
        s16 a[6] = {1, 1, 2, 2, 3, 3};
        s16 b[6] = {4, 4, 5, 5, 6, 6};
        float out[8];
        r.Push(a, 3);
        CHECK(r.PopFloat(out, 2) == 2);          // takes 1,2; leaves 3
        r.Push(b, 3);                            // 3,4,5,6 -> wraps, exactly full
        CHECK(r.Frames() == 4);
        CHECK(r.PopFloat(out, 4) == 4);
        for (int i = 0; i < 4; i++)
            CHECK(std::lround(out[i * 2] * 32768.0f) == 3 + i);
    }
    {   // overflow drops the oldest, keeps the newest
        AudioRing r(4);
        s16 a[8] = {1, 1, 2, 2, 3, 3, 4, 4};
        s16 b[4] = {5, 5, 6, 6};
        float out[8];
        r.Push(a, 4);
        CHECK(r.Push(b, 2) == 2);                // 1 and 2 dropped
        CHECK(r.Frames() == 4);
        CHECK(r.PopFloat(out, 4) == 4);
        for (int i = 0; i < 4; i++)
            CHECK(std::lround(out[i * 2] * 32768.0f) == 3 + i);
    }
    {   // a push larger than the whole ring keeps only the tail
        AudioRing r(3);
        s16 a[10] = {1, 1, 2, 2, 3, 3, 4, 4, 5, 5};
        float out[6];
        r.Push(a, 5);
        CHECK(r.Frames() == 3);
        CHECK(r.PopFloat(out, 3) == 3);
        CHECK(std::lround(out[0] * 32768.0f) == 3);
        CHECK(std::lround(out[4] * 32768.0f) == 5);
    }
    {   // volume
        AudioRing r(2);
        s16 a[2] = {16384, -16384};
        float out[2];
        r.Push(a, 1);
        r.PopFloat(out, 1, 0.5f);
        CHECK(out[0] == 0.25f && out[1] == -0.25f);
    }
    {   // block scheduling policy
        CHECK(ShouldWriteBlock(5, 256, 256));    // full block ready: write
        CHECK(!ShouldWriteBlock(5, 100, 256));   // not enough, plenty queued: wait
        CHECK(!ShouldWriteBlock(2, 0, 256));     // 2 queued: still wait
        CHECK(ShouldWriteBlock(1, 100, 256));    // nearly dry: pad with silence
        CHECK(ShouldWriteBlock(0, 0, 256));
    }

    // long stream: pushing 801 frames/frame and popping 256-frame blocks must
    // lose nothing while the ring keeps up
    {
        AudioRing r(4096);
        s16 chunk[801 * 2];
        for (int i = 0; i < 801 * 2; i++) chunk[i] = (s16)(i & 0x7FFF);
        float blk[256 * 2];
        long pushed = 0, popped = 0;
        for (int f = 0; f < 600; f++)
        {
            CHECK(r.Push(chunk, 801) == 0);
            pushed += 801;
            while (ShouldWriteBlock(5, r.Frames(), 256) && r.Frames() >= 256)
                popped += r.PopFloat(blk, 256);
        }
        CHECK(pushed - popped == r.Frames());
    }

    if (fails) { printf("%d check(s) failed\n", fails); return 1; }
    printf("all AudioRing tests passed\n");
    return 0;
}
