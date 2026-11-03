// Optional frame profiler for the PS3 port. Compiled in only when
// MELONDS_PS3_PROFILE is defined (the PS3 Makefile does that); otherwise every
// macro is empty. Reads the PowerPC timebase (79.8 MHz) with no system call.
#ifndef MELONDS_PROF_H
#define MELONDS_PROF_H

#ifdef MELONDS_PS3_PROFILE
namespace melonds_prof
{
enum { ARM9, ARM7, GEOM, SYS, DRAW2D, RENDER3D, FINISH3D, COUNT };
inline unsigned long long Ticks[COUNT];
inline unsigned long long Now()
{
    unsigned long long tb;
    __asm__ volatile("mftb %0" : "=r"(tb));
    return tb;
}
}
#define PROF_BEGIN(name) unsigned long long prof_t_##name = melonds_prof::Now()
#define PROF_END(name) (melonds_prof::Ticks[melonds_prof::name] += melonds_prof::Now() - prof_t_##name)
#else
#define PROF_BEGIN(name)
#define PROF_END(name)
#endif

#endif
