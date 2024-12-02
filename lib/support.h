#pragma once

#include <stddef.h>
#include <stdint.h>

typedef size_t sz_t;

typedef unsigned u;

typedef int16_t  s16;
typedef uint16_t u16;

typedef int8_t  s8;
typedef uint8_t u8;

#ifndef __cplusplus
typedef uint8_t bool;

#define false (bool)0
#define true (bool)1

#endif

typedef int32_t  s32;
typedef uint32_t u32;

#ifdef __GNUC__
#define pack __attribute__((packed))

#ifndef __cplusplus
#define static_assert(cond, msg) _Static_assert(cond, msg)
#endif

inline void spin (volatile u32 pCount)
{
    while (pCount--) asm("nop");
}
#define halt(cond) while (cond) { spin(1); }

#endif

#define assert_sz(structure, sz) static_assert(sizeof(structure) == sz, "Size of " #structure " is not " #sz)