// Threads, semaphores, mutexes and clocks built on the C++ standard library.
// Used by the host headless runner. The PS3 build uses Sync_single.cpp.

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "Platform.h"

namespace melonDS::Platform
{

struct Thread
{
    std::thread T;
};

Thread* Thread_Create(std::function<void()> func)
{
    Thread* t = new Thread;
    t->T = std::thread(std::move(func));
    return t;
}

void Thread_Free(Thread* thread)
{
    if (!thread) return;
    if (thread->T.joinable()) thread->T.join();
    delete thread;
}

void Thread_Wait(Thread* thread)
{
    if (thread && thread->T.joinable()) thread->T.join();
}

struct Semaphore
{
    std::mutex M;
    std::condition_variable CV;
    int Count = 0;
};

Semaphore* Semaphore_Create()
{
    return new Semaphore;
}

void Semaphore_Free(Semaphore* sema)
{
    delete sema;
}

void Semaphore_Reset(Semaphore* sema)
{
    std::lock_guard<std::mutex> lock(sema->M);
    sema->Count = 0;
}

void Semaphore_Wait(Semaphore* sema)
{
    std::unique_lock<std::mutex> lock(sema->M);
    sema->CV.wait(lock, [sema] { return sema->Count > 0; });
    sema->Count--;
}

bool Semaphore_TryWait(Semaphore* sema, int timeout_ms)
{
    std::unique_lock<std::mutex> lock(sema->M);
    if (timeout_ms > 0)
    {
        if (!sema->CV.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                               [sema] { return sema->Count > 0; }))
            return false;
    }
    else if (sema->Count <= 0)
    {
        return false;
    }

    sema->Count--;
    return true;
}

void Semaphore_Post(Semaphore* sema, int count)
{
    {
        std::lock_guard<std::mutex> lock(sema->M);
        sema->Count += count;
    }
    sema->CV.notify_all();
}

struct Mutex
{
    std::mutex M;
};

Mutex* Mutex_Create()
{
    return new Mutex;
}

void Mutex_Free(Mutex* mutex)
{
    delete mutex;
}

void Mutex_Lock(Mutex* mutex)
{
    mutex->M.lock();
}

void Mutex_Unlock(Mutex* mutex)
{
    mutex->M.unlock();
}

bool Mutex_TryLock(Mutex* mutex)
{
    return mutex->M.try_lock();
}

void Sleep(u64 usecs)
{
    std::this_thread::sleep_for(std::chrono::microseconds(usecs));
}

static const std::chrono::steady_clock::time_point StartTime = std::chrono::steady_clock::now();

u64 GetMSCount()
{
    return GetUSCount() / 1000;
}

u64 GetUSCount()
{
    auto d = std::chrono::steady_clock::now() - StartTime;
    return (u64)std::chrono::duration_cast<std::chrono::microseconds>(d).count();
}

}
