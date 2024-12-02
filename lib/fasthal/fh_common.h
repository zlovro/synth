//
// Created by Made on 19/10/2024.
//

#ifndef FH_COMMON_H
#define FH_COMMON_H

#define HSE_VALUE 25'000'000

#include <stm32f411xe.h>
#include <stm32f4xx_it.h>
#include <support.h>

#define setBits(x, msk, v) x = ((x) & ~((u32)(msk))) | (u32)(v)
#define clearBits(x, msk) x = ((x) & ~(u32)(msk))

void  fhInit();
void  fhDelayMs(int pMs);

extern u32 gFhTotalAllocated;

void* fhAlloc(u32 pSz);
void  fhFree(void* pHandle);

inline u32 fhGetFreeMemory()
{
    return 131072 - gFhTotalAllocated;
}

inline u32 fhGetUsedMemory()
{
    return gFhTotalAllocated;
}

inline volatile u32 fhGetMillis()
{
    return gSysTick;
}

inline u8 fhGetFirstSetBitIdx(u32 pX)
{
    for (u8 bit = 0; bit < 32; bit++)
    {
        if (pX & 1 << bit) return bit;
    }
    return 32;
}

#endif //FH_COMMON_H
