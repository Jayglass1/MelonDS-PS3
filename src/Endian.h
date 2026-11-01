/*
    Copyright 2016-2026 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#ifndef ENDIAN_H
#define ENDIAN_H

// Byte-order support for hosts that are not little-endian (e.g. PS3/PowerPC).
//
// The DS is little-endian, and the core reads and writes emulated memory
// through typed pointers, e.g.  *(u32*)&RAM[addr].  To keep those accesses
// correct on a big-endian host, such casts use the le_* types below instead:
//
//     *(le_u32*)&RAM[addr]
//
// On little-endian hosts le_u32 is just u32, so there is no change at all.
// On big-endian hosts le_u32 is a trivially-copyable wrapper with the same
// size and alignment as u32 that byte-swaps on every load and store.
//
// Define MELONDS_FORCE_ENDIAN_WRAPPERS on a little-endian host to build with
// the wrapper classes without swapping. That is how to check on x86 that every
// converted site type-checks against the wrapper.
//
// Define MELONDS_FORCE_ENDIAN_SWAP as well to make the helpers and wrappers
// swap on a little-endian host. The stored Raw values then match what a real
// big-endian host would hold, which lets tests check the swap arithmetic on
// x86. Emulation itself is not meaningful in that mode.

#include <stdint.h>
#include <string.h>

#include "types.h"

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define MELONDS_BIG_ENDIAN 1
#else
#define MELONDS_BIG_ENDIAN 0
#endif

#if defined(__GNUC__) || defined(__clang__)
#define MELONDS_BSWAP16(x) __builtin_bswap16(x)
#define MELONDS_BSWAP32(x) __builtin_bswap32(x)
#define MELONDS_BSWAP64(x) __builtin_bswap64(x)
#elif defined(_MSC_VER)
#include <stdlib.h>
#define MELONDS_BSWAP16(x) _byteswap_ushort(x)
#define MELONDS_BSWAP32(x) _byteswap_ulong(x)
#define MELONDS_BSWAP64(x) _byteswap_uint64(x)
#else
#define MELONDS_BSWAP16(x) ((uint16_t)((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF)))
#define MELONDS_BSWAP32(x) ((uint32_t)(((uint32_t)MELONDS_BSWAP16((uint16_t)(x)) << 16) | MELONDS_BSWAP16((uint16_t)((x) >> 16))))
#define MELONDS_BSWAP64(x) ((uint64_t)(((uint64_t)MELONDS_BSWAP32((uint32_t)(x)) << 32) | MELONDS_BSWAP32((uint32_t)((x) >> 32))))
#endif

namespace melonDS
{

// True when the host needs to swap bytes to read little-endian data.
#if MELONDS_BIG_ENDIAN || defined(MELONDS_FORCE_ENDIAN_SWAP)
constexpr bool HostIsBigEndian = true;
#else
constexpr bool HostIsBigEndian = false;
#endif

namespace Endian
{

inline u16 Swap16(u16 v) { return (u16)MELONDS_BSWAP16(v); }
inline u32 Swap32(u32 v) { return (u32)MELONDS_BSWAP32(v); }
inline u64 Swap64(u64 v) { return (u64)MELONDS_BSWAP64(v); }

// Convert between host order and little-endian order.
// (The operation is its own inverse.)
inline u16 LE16(u16 v) { return HostIsBigEndian ? Swap16(v) : v; }
inline u32 LE32(u32 v) { return HostIsBigEndian ? Swap32(v) : v; }
inline u64 LE64(u64 v) { return HostIsBigEndian ? Swap64(v) : v; }

// Alignment-safe loads and stores of little-endian values in byte buffers.
// Use these in new code instead of casting pointers.
inline u16 Load16(const void* p) { u16 v; memcpy(&v, p, sizeof(v)); return LE16(v); }
inline u32 Load32(const void* p) { u32 v; memcpy(&v, p, sizeof(v)); return LE32(v); }
inline u64 Load64(const void* p) { u64 v; memcpy(&v, p, sizeof(v)); return LE64(v); }

inline void Store16(void* p, u16 v) { v = LE16(v); memcpy(p, &v, sizeof(v)); }
inline void Store32(void* p, u32 v) { v = LE32(v); memcpy(p, &v, sizeof(v)); }
inline void Store64(void* p, u64 v) { v = LE64(v); memcpy(p, &v, sizeof(v)); }

}

#if MELONDS_BIG_ENDIAN || defined(MELONDS_FORCE_ENDIAN_WRAPPERS)

// A value of type T that is stored in little-endian order in memory.
// No constructors, so it stays trivial and works inside unions and
// packed structs. Same size and alignment as T.
template<typename T, typename U>
struct LEValue
{
    U Raw; // little-endian representation

    static inline U ToStored(U v);

    inline operator T() const { return (T)ToStored(Raw); }
    inline LEValue& operator=(T v) { Raw = ToStored((U)v); return *this; }

    inline LEValue& operator+=(T v)  { return *this = (T)(*this + v); }
    inline LEValue& operator-=(T v)  { return *this = (T)(*this - v); }
    inline LEValue& operator*=(T v)  { return *this = (T)(*this * v); }
    inline LEValue& operator/=(T v)  { return *this = (T)(*this / v); }
    inline LEValue& operator%=(T v)  { return *this = (T)(*this % v); }
    inline LEValue& operator&=(T v)  { return *this = (T)(*this & v); }
    inline LEValue& operator|=(T v)  { return *this = (T)(*this | v); }
    inline LEValue& operator^=(T v)  { return *this = (T)(*this ^ v); }
    inline LEValue& operator<<=(int n) { return *this = (T)(*this << n); }
    inline LEValue& operator>>=(int n) { return *this = (T)(*this >> n); }

    inline LEValue& operator++()    { return *this = (T)(*this + 1); }
    inline LEValue& operator--()    { return *this = (T)(*this - 1); }
    inline T operator++(int)        { T old = *this; *this = (T)(old + 1); return old; }
    inline T operator--(int)        { T old = *this; *this = (T)(old - 1); return old; }
};

template<> inline u16 LEValue<u16, u16>::ToStored(u16 v) { return Endian::LE16(v); }
template<> inline u32 LEValue<u32, u32>::ToStored(u32 v) { return Endian::LE32(v); }
template<> inline u64 LEValue<u64, u64>::ToStored(u64 v) { return Endian::LE64(v); }
template<> inline u16 LEValue<s16, u16>::ToStored(u16 v) { return Endian::LE16(v); }
template<> inline u32 LEValue<s32, u32>::ToStored(u32 v) { return Endian::LE32(v); }
template<> inline u64 LEValue<s64, u64>::ToStored(u64 v) { return Endian::LE64(v); }

typedef LEValue<u16, u16> le_u16;
typedef LEValue<u32, u32> le_u32;
typedef LEValue<u64, u64> le_u64;
typedef LEValue<s16, u16> le_s16;
typedef LEValue<s32, u32> le_s32;
typedef LEValue<s64, u64> le_s64;

static_assert(sizeof(le_u16) == sizeof(u16) && alignof(le_u16) == alignof(u16), "le_u16 layout");
static_assert(sizeof(le_u32) == sizeof(u32) && alignof(le_u32) == alignof(u32), "le_u32 layout");
static_assert(sizeof(le_u64) == sizeof(u64) && alignof(le_u64) == alignof(u64), "le_u64 layout");

#else

typedef u16 le_u16;
typedef u32 le_u32;
typedef u64 le_u64;
typedef s16 le_s16;
typedef s32 le_s32;
typedef s64 le_s64;

#endif

// Maps a plain integer type to its little-endian memory type, for templated
// memory accessors:  *(le_t<T>*)&buffer[addr]
// Types that have no byte order (u8, s8) map to themselves.
template<typename T> struct LEOf { typedef T type; };
template<> struct LEOf<u16> { typedef le_u16 type; };
template<> struct LEOf<u32> { typedef le_u32 type; };
template<> struct LEOf<u64> { typedef le_u64 type; };
template<> struct LEOf<s16> { typedef le_s16 type; };
template<> struct LEOf<s32> { typedef le_s32 type; };
template<> struct LEOf<s64> { typedef le_s64 type; };

template<typename T> using le_t = typename LEOf<T>::type;

}

#endif // ENDIAN_H
