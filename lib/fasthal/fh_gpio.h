#ifndef FH_GPIO_H
#define FH_GPIO_H

#include <main_ex.h>
#include <support.h>
#include <stm32f411xe.h>

#include "fh_common.h"

typedef enum GpioMode : u8
{
    GPIO_MODE_INPUT = 0,
    GPIO_MODE_OUTPUT,
    GPIO_MODE_ALTERNATE,
    GPIO_MODE_ANALOG_IO
} GpioMode;

typedef enum GpioOtype : u8
{
    GPIO_OTYPE_PUSH_PULL = 0,
    GPIO_OTYPE_OPEN_DRAIN
} GpioOtype;

typedef enum GpioSpeed : u8
{
    GPIO_SPEED_LOW,
    GPIO_SPEED_MEDIUM,
    GPIO_SPEED_FAST,
    GPIO_SPEED_HIGH,
} GpioSpeed;

typedef enum GpioPull : u8
{
    GPIO_PULL_NONE,
    GPIO_PULL_UP,
    GPIO_PULL_DOWN,
    GPIO_PULL_RESERVED
} GpioPull;

const auto MEM_GPIOA_START = (u8*)0x4002'0000;
const auto MEM_GPIOB_START = (u8*)0x4002'0400;

enum : u16
{
    GPIO_PIN_00 = 0b0000'0000'0000'0001,
    GPIO_PIN_01 = 0b0000'0000'0000'0010,
    GPIO_PIN_02 = 0b0000'0000'0000'0100,
    GPIO_PIN_03 = 0b0000'0000'0000'1000,
    GPIO_PIN_04 = 0b0000'0000'0001'0000,
    GPIO_PIN_05 = 0b0000'0000'0010'0000,
    GPIO_PIN_06 = 0b0000'0000'0100'0000,
    GPIO_PIN_07 = 0b0000'0000'1000'0000,
    GPIO_PIN_08 = 0b0000'0001'0000'0000,
    GPIO_PIN_09 = 0b0000'0010'0000'0000,
    GPIO_PIN_10 = 0b0000'0100'0000'0000,
    GPIO_PIN_11 = 0b0000'1000'0000'0000,
    GPIO_PIN_12 = 0b0001'0000'0000'0000,
    GPIO_PIN_13 = 0b0010'0000'0000'0000,
    GPIO_PIN_14 = 0b0100'0000'0000'0000,
    GPIO_PIN_15 = 0b1000'0000'0000'0000,
};

#define reg(bank, reg) ((GPIO_TypeDef*)bank)->reg

void fhGpioPinSetup(u8* pBank, u16 pPinsMask, GpioMode pMode, GpioOtype pOtype, GpioSpeed pSpeed, GpioPull pPull);

inline void fhGpioInitSystem(u32 pBankBit)
{
    RCC->AHB1ENR |= 1 << pBankBit;
}

inline u8 fhGpioPinRead(u8* pBank, u16 pPin)
{
    return (reg(pBank, IDR) & pPin) == pPin;
}

inline void fhGpioPinWrite(u8* pBank, u16 pPin, u8 pState)
{
    reg(pBank, BSRR) = pState ? pPin : pPin << 16;
}

inline void fhGpioPinToggle(u8* pBank, u16 pPin)
{
    u32 odr = reg(pBank, ODR);
    reg(pBank, BSRR) = (odr & pPin) << 16 | ~odr & pPin;
}

// pPin must not be used as a mask.
inline void fhGpioAlternateFunction(u8* pBank, u16 pPin, u8 pFunc)
{
    u8 pinIdx = fhGetFirstSetBitIdx(pPin);

    if (pinIdx < 8)
    {
        setBits(reg(pBank, AFR[0]), 0xF << (pinIdx * 4), pFunc << (pinIdx * 4));
        return;
    }

    setBits(reg(pBank, AFR[1]), 0xF << ((pinIdx - 8) * 4), pFunc << ((pinIdx - 8) * 4));
}

void fhGpioPinsResetA();
void fhGpioPinsResetB();

#define fhGpioInitSystemA()                            fhGpioInitSystem(RCC_AHB1ENR_GPIOAEN_Pos)
#define fhGpioPinSetupA(pin, mode, otype, speed, pull) fhGpioPinSetup(MEM_GPIOA_START, pin, mode, otype, speed, pull)
#define fhGpioPinReadA(pin)                            fhGpioPinRead(MEM_GPIOA_START, pin)
#define fhGpioPinWriteA(pin, state)                    fhGpioPinWrite(MEM_GPIOA_START, pin, state)
#define fhGpioPinToggleA(pin)                          fhGpioPinToggle(MEM_GPIOA_START, pin)
#define fhGpioAlternateFunctionA(pin, func)            fhGpioAlternateFunction(MEM_GPIOA_START, pin, func)

#define fhGpioInitSystemB()                            fhGpioInitSystem(RCC_AHB1ENR_GPIOBEN_Pos)
#define fhGpioPinSetupB(pin, mode, otype, speed, pull) fhGpioPinSetup(MEM_GPIOB_START, pin, mode, otype, speed, pull)
#define fhGpioPinReadB(pin)                            fhGpioPinRead(MEM_GPIOB_START, pin)
#define fhGpioPinWriteB(pin, state)                    fhGpioPinWrite(MEM_GPIOB_START, pin, state)
#define fhGpioPinToggleB(pin)                          fhGpioPinToggle(MEM_GPIOB_START, pin)
#define fhGpioAlternateFunctionB(pin, func)            fhGpioAlternateFunction(MEM_GPIOB_START, pin, func)

#define fhGpioInitSystemH()                            fhGpioInitSystem(RCC_AHB1ENR_GPIOHEN_Pos)

#endif //FH_GPIO_H
