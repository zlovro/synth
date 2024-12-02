#include "fh_gpio.h"

#include <stm32f411xe.h>
#include <stm32f4xx.h>
#include <support.h>

#include "fh_common.h"

void fhGpioPinSetup(u8* pBank, u16 pPinsMask, GpioMode pMode, GpioOtype pOtype, GpioSpeed pSpeed, GpioPull pPull)
{
    for (auto bitPos = 0; bitPos < 16; ++bitPos)
    {
        if (!(pPinsMask >> bitPos & 1))
        {
            continue;
        }

        auto offset = bitPos << 1;

        setBits(reg(pBank, MODER), 0b11 << offset, pMode << offset);
        setBits(reg(pBank, OTYPER), 0b1 << bitPos, pOtype << bitPos);
        setBits(reg(pBank, OSPEEDR), 0b11 << offset, pSpeed << offset);
        setBits(reg(pBank, PUPDR), 0b11 << offset, pPull << offset);
    }
}

void fhGpioPinsResetA()
{
    reg(MEM_GPIOA_START, MODER)   = 0xA800'0000;
    reg(MEM_GPIOA_START, OTYPER)  = 0;
    reg(MEM_GPIOA_START, OSPEEDR) = 0x0C00'0000;
    reg(MEM_GPIOA_START, PUPDR)   = 0x6400'0000;
    reg(MEM_GPIOA_START, IDR)     = 0;
    reg(MEM_GPIOA_START, ODR)     = 0;
    reg(MEM_GPIOA_START, BSRR)    = 0;
    reg(MEM_GPIOA_START, LCKR)    = 0;
    reg(MEM_GPIOA_START, AFR[0])  = 0;
    reg(MEM_GPIOA_START, AFR[1])  = 0;
}

void fhGpioPinsResetB()
{
    reg(MEM_GPIOA_START, MODER)   = 0x0280;
    reg(MEM_GPIOA_START, OTYPER)  = 0;
    reg(MEM_GPIOA_START, OSPEEDR) = 0x00C0;
    reg(MEM_GPIOA_START, PUPDR)   = 0x0100;
    reg(MEM_GPIOA_START, IDR)     = 0;
    reg(MEM_GPIOA_START, ODR)     = 0;
    reg(MEM_GPIOA_START, BSRR)    = 0;
    reg(MEM_GPIOA_START, LCKR)    = 0;
    reg(MEM_GPIOA_START, AFR[0])  = 0;
    reg(MEM_GPIOA_START, AFR[1])  = 0;
}
