//
// Created by Made on 20/10/2024.
//

#include "fh_uart.h"
#include "main_ex.h"
#include "fh_gpio.h"

void fhUartChkErr(u8* pBase)
{
    u32 data = fhUsartReg(pBase, SR);

    u8 framingError = data >> USART_SR_FE_Pos & 1;
    u8 parityError  = data >> USART_SR_PE_Pos & 1;
    u8 overrunError = data >> USART_SR_ORE_Pos & 1;

    if (framingError || parityError || overrunError)
    {
        // error(ERR_CAT_SPI, framingError << 2 | parityError << 1 | overrunError, true);
    }
}

void fhUartReset(u8* pBase)
{
    fhUsartReg(pBase, SR)   = 0x00C0'0000;
    fhUsartReg(pBase, DR)   = 0;
    fhUsartReg(pBase, BRR)  = 0;
    fhUsartReg(pBase, CR1)  = 0;
    fhUsartReg(pBase, CR2)  = 0;
    fhUsartReg(pBase, CR3)  = 0;
    fhUsartReg(pBase, GTPR) = 0;
}

void fhUartCommonInit(u8* pBase, u32 pClk, u32 pBaud)
{
    clearBits(fhUsartReg(pBase, CR1), USART_CR1_UE);

    fhUsartReg(pBase, BRR) = UART_BRR_SAMPLING16(pClk, pBaud);

    clearBits(fhUsartReg(pBase, CR2), USART_CR2_LINEN | USART_CR2_CLKEN);
    clearBits(fhUsartReg(pBase, CR2), USART_CR2_LINEN | USART_CR2_CLKEN);
    clearBits(fhUsartReg(pBase, CR3), USART_CR3_SCEN | USART_CR3_HDSEL | USART_CR3_IREN);

    setBits(fhUsartReg(pBase, CR1), USART_CR1_RE_Msk | USART_CR1_TE_Msk | USART_CR1_UE_Msk, USART_CR1_RE_Msk | USART_CR1_TE_Msk | USART_CR1_UE_Msk);
}

void fhUartInit1(u32 pBaud)
{
    fhGpioInitSystemA();
    fhGpioInitSystemH();

    RCC->APB2ENR |= RCC_APB2ENR_USART1EN_Msk;

    fhGpioPinSetupA(GPIO_PIN_09 | GPIO_PIN_10, GPIO_MODE_ALTERNATE, GPIO_OTYPE_PUSH_PULL, GPIO_SPEED_HIGH, GPIO_PULL_NONE);
    fhGpioAlternateFunctionA(GPIO_PIN_09, 7);
    fhGpioAlternateFunctionA(GPIO_PIN_10, 7);

    sz_t clk = SystemCoreClock >> APBPrescTable[(RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos];

    fhUartCommonInit(MEM_USART1_START, clk, pBaud);
}

void fhUartWrite(u8* pBank, u8 pVal)
{
    halt(fhUsartReg(pBank, SR) & USART_SR_TXE == 0);
    fhUsartReg(pBank, DR) = pVal;
}

u8 fhUartRead(u8* pBank)
{
    halt(!(fhUsartReg(pBank, SR) & USART_SR_RXNE_Msk));
    return fhUsartReg(pBank, DR);
}

void fhUartWriteBulk(u8* pBank, u8* pBuf, u32 pSize)
{
    for (sz_t i = 0; i < pSize; ++i)
    {
        fhUartWrite(pBank, pBuf[i]);
    }

    halt(fhUsartReg(pBank, SR) & USART_SR_TC == 0);
}

u8 fhUartRw(u8* pBank, u8 pVal)
{
    halt(!(fhUsartReg(pBank, SR) & USART_SR_RXNE_Msk));
    u32 ret = fhUsartReg(pBank, DR);

    fhUsartReg(pBank, DR) = pVal;
    halt(!(fhUsartReg(pBank, SR) & USART_SR_TXE_Msk));

    return ret;
}

void fhUartRwBulk(u8* pBank, u8* pInBuf, u8* pOutBuf, u32 pBufSize)
{
    for (int i = 0; i < pBufSize; ++i)
    {
        pOutBuf[i] = fhUartRw(pBank, pInBuf[i]);
    }
}

int __io_putchar(int pChar)
{
    fhUartWrite(MEM_USART6_START, pChar);
    return 0;
}
