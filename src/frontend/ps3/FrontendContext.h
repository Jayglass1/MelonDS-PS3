// Shared state for the PS3 frontend. A pointer to this is passed to the core
// as the "userdata" argument, and comes back in the Platform:: callbacks.
#ifndef PS3_FRONTEND_CONTEXT_H
#define PS3_FRONTEND_CONTEXT_H

#include <atomic>
#include <string>

namespace melonDS::PS3Frontend
{

struct Context
{
    // Where the cartridge save is written back to. Empty = don't write.
    std::string NDSSavePath;
    std::string GBASavePath;

    // Set by Platform::SignalStop when the emulated console stops.
    std::atomic<bool> StopRequested {false};
    std::atomic<int> StopReason {-1};
};

// Directory used by Platform::GetLocalFilePath (include the trailing slash).
void SetDataDir(const std::string& dir);

}

#endif // PS3_FRONTEND_CONTEXT_H
