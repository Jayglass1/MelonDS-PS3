// Single-threaded synchronization backend. This is what the PS3 build uses
// for its first milestone, and it also builds on the host so the logic can be
// tested there (cmake -DPS3_FRONTEND_SINGLE_THREADED=ON).
//
// The core only starts a thread for the optional threaded software 3D
// renderer, which is off by default, so no thread is ever needed. Semaphores
// and mutexes are still created at startup, so they have to behave sensibly:
// they are plain counters here.
//
// Time comes from gettimeofday and usleep, which newlib provides.

#include <sys/time.h>
#include <unistd.h>

#include "Platform.h"

namespace melonDS::Platform
{

struct Thread {};

Thread* Thread_Create(std::function<void()>)
{
    Log(LogLevel::Error, "Threads are not available in the single-threaded build "
                         "(disable threaded 3D rendering).\n");
    return nullptr;
}

void Thread_Free(Thread*) {}
void Thread_Wait(Thread*) {}

struct Semaphore
{
    int Count = 0;
};

Semaphore* Semaphore_Create() { return new Semaphore; }
void Semaphore_Free(Semaphore* sema) { delete sema; }
void Semaphore_Reset(Semaphore* sema) { sema->Count = 0; }

void Semaphore_Wait(Semaphore* sema)
{
    if (sema->Count > 0)
    {
        sema->Count--;
        return;
    }

    // Nothing else could ever post to it, so waiting would hang forever.
    Log(LogLevel::Error, "Semaphore_Wait would deadlock in the single-threaded build\n");
}

bool Semaphore_TryWait(Semaphore* sema, int)
{
    if (sema->Count <= 0)
        return false;

    sema->Count--;
    return true;
}

void Semaphore_Post(Semaphore* sema, int count)
{
    sema->Count += count;
}

struct Mutex
{
    bool Locked = false;
};

Mutex* Mutex_Create() { return new Mutex; }
void Mutex_Free(Mutex* mutex) { delete mutex; }
void Mutex_Lock(Mutex* mutex) { mutex->Locked = true; }
void Mutex_Unlock(Mutex* mutex) { mutex->Locked = false; }

bool Mutex_TryLock(Mutex* mutex)
{
    if (mutex->Locked)
        return false;

    mutex->Locked = true;
    return true;
}

void Sleep(u64 usecs)
{
    usleep((useconds_t)usecs);
}

u64 GetUSCount()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (u64)tv.tv_sec * 1000000ull + (u64)tv.tv_usec;
}

u64 GetMSCount()
{
    return GetUSCount() / 1000;
}

}
