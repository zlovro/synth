#include "fh_common.h"

#include <system_stm32f4xx.h>
#include "stm32f4xx.h"
#include "stm32f4xx_it.h"
#include <malloc.h>

#include <core_cm4.h>

void systickInit()
{
    SysTick_Config(SystemCoreClock / 1'000);
}

void fhMspInit()
{
    setBits(FLASH->ACR, FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY, FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_7WS);
}

void fhHseInit()
{
    setBits(RCC->CR, RCC_CR_HSEON, RCC_CR_HSEON);
    halt(RCC->CR & RCC_CR_HSERDY == 0);
}

void fhPllDisable()
{
    setBits(RCC->CR, RCC_CR_PLLON, 0);
    halt(RCC->CR & RCC_CR_PLLRDY);
}

void fhPllEnable()
{
    setBits(RCC->CR, RCC_CR_PLLON, 1);
    halt(RCC->CR & RCC_CR_PLLRDY == 0);
}

void fhPllInit()
{
    fhPllDisable();

    auto m = 25;
    auto n = 192;
    auto p = 2;
    auto q = 4;

    auto pllReg = RCC_PLLCFGR_PLLSRC_HSE;
    pllReg |= m << RCC_PLLCFGR_PLLM_Pos;
    pllReg |= n << RCC_PLLCFGR_PLLN_Pos;
    pllReg |= (p >> 1) - 1 << RCC_PLLCFGR_PLLP_Pos;
    pllReg |= q <<  RCC_PLLCFGR_PLLQ_Pos;

    setBits(RCC->PLLCFGR, RCC_PLLCFGR_PLLSRC | RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN | RCC_PLLCFGR_PLLP | RCC_PLLCFGR_PLLQ, pllReg);

    fhPllEnable();
}

void fhInitPower()
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    setBits(PWR->CR, PWR_CR_VOS, 0b11 << PWR_CR_VOS_Pos);
}

void fhUpdateCoreClock()
{
    u32 pllm = RCC->PLLCFGR & RCC_PLLCFGR_PLLM;
    u32 pllvco = (uint64_t)HSE_VALUE * (uint64_t)((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos) / (uint64_t)pllm;
    u32 pllp = (((RCC->PLLCFGR & RCC_PLLCFGR_PLLP) >> RCC_PLLCFGR_PLLP_Pos) + 1U) * 2U;
    u32 sysCfkFreq = pllvco / pllp;
    SystemCoreClock = sysCfkFreq >> AHBPrescTable[(RCC->CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos];
}

void fhCpuClockInit()
{
    fhPllEnable();

    setBits(RCC->CFGR, RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 | RCC_CFGR_HPRE, RCC_CFGR_PPRE2_DIV1 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_HPRE_DIV1);

    setBits(RCC->CFGR, RCC_CFGR_SW, RCC_CFGR_SW_PLL);
    halt((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

    fhUpdateCoreClock();
}

void fhInit()
{
    fhInitPower();
    fhMspInit();
    fhHseInit();
    fhPllInit();
    fhCpuClockInit();

    systickInit();
}

void fhDelayMs(int pMs)
{
    auto t0 = fhGetMillis();
    halt(fhGetMillis() < t0 + pMs);
}

u32 gFhTotalAllocated = 0;

void* fhAlloc(u32 pSz)
{
    gFhTotalAllocated += pSz;
    return malloc(pSz);
}

void fhFree(void* pHandle)
{
    gFhTotalAllocated -= malloc_usable_size(pHandle);
    free(pHandle);
}