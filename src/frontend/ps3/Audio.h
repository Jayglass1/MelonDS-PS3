// Audio output interface for the PS3 frontend.
#ifndef PS3_FRONTEND_AUDIO_H
#define PS3_FRONTEND_AUDIO_H

#include "types.h"

namespace melonDS::PS3Frontend::Audio
{

// Opens a 48 kHz stereo output port. Returns false on failure (the emulator
// can still run silently).
bool Init();

// Queues interleaved signed 16-bit stereo frames from the core.
void Submit(const s16* interleaved, int frames);

// Moves queued audio into the port's buffer. Call this at least once per
// emulated frame, and more often if possible.
void Service();

void Shutdown();

}

#endif // PS3_FRONTEND_AUDIO_H
