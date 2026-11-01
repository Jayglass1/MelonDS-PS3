// Force-included (-include) for the PS3 build only.
//
// The PS3 toolchain's libstdc++ (GCC 7.2 / newlib) is built without gthreads
// and without C99 string conversions, so std::mutex, std::recursive_mutex and
// std::to_string are missing. The core only needs them in two harmless ways:
//  - teakra declares mutex members but never locks them (the locks are
//    commented out), and the PS3 build is single-threaded anyway;
//  - the DSP disassembler builds strings with to_string.
#ifndef PS3_COMPAT_H
#define PS3_COMPAT_H

#ifdef __cplusplus

#include <cstdio>
#include <mutex>
#include <string>

#ifndef _GLIBCXX_HAS_GTHREADS
namespace std
{
// Single-threaded stand-ins: nothing else can contend for them.
class mutex
{
public:
    void lock() {}
    void unlock() {}
    bool try_lock() { return true; }
};

class recursive_mutex
{
public:
    void lock() {}
    void unlock() {}
    bool try_lock() { return true; }
};
}
#endif

#ifndef _GLIBCXX_USE_C99
namespace std
{
// newlib only declares these in the global namespace
using ::snprintf;
using ::vsnprintf;

inline string to_string(int v)                { char b[32]; snprintf(b, sizeof b, "%d", v);   return b; }
inline string to_string(unsigned v)           { char b[32]; snprintf(b, sizeof b, "%u", v);   return b; }
inline string to_string(long v)               { char b[32]; snprintf(b, sizeof b, "%ld", v);  return b; }
inline string to_string(unsigned long v)      { char b[32]; snprintf(b, sizeof b, "%lu", v);  return b; }
inline string to_string(long long v)          { char b[32]; snprintf(b, sizeof b, "%lld", v); return b; }
inline string to_string(unsigned long long v) { char b[32]; snprintf(b, sizeof b, "%llu", v); return b; }
}
#endif

#endif // __cplusplus
#endif // PS3_COMPAT_H
