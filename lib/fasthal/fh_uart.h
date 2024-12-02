//
// Created by Made on 20/10/2024.
//

#ifndef FH_UART_H
#define FH_UART_H

#include <support.h>
#include "fh_common.h"

const auto MEM_USART1_START = (u8*)USART1;
const auto MEM_USART2_START = (u8*)USART2;
const auto MEM_USART6_START = (u8*)USART6;

#define fhUsartReg(base, reg) ((USART_TypeDef*)base)->reg
// #define fhUsartRegTptr(base, reg, t) ((t*)(base + REG_USART_##reg))
// #define fhUsartRegT(base, reg, t) *fhUsartRegT(base, reg, t)

void fhUartChkErr(u8* pBase);
void fhUartReset(u8* pBank);
void fhUartSetBaud(u8* pBase, u32 pBaud);
void fhUartInit1(u32 pBaud);
void fhUartInit2(u32 pBaud);
void fhUartInit6(u32 pBaud);
void fhUartWrite(u8* pBank, u8 pVal);
void fhUartWriteBulk(u8* pBank, u8* pBuf, u32 pSize);
u8   fhUartRead(u8* pBank);
u8   fhUartRw(u8* pBank, u8 pVal);
void fhUartRwBulk(u8* pBank, u8* pInBuf, u8* pOutBuf, u32 pBufSize);

#define fhUartRw2(x) fhUartRw(MEM_USART2_START, x)
#define fhUartRwBulk2(in, out, sz) fhUartRwBulk(MEM_USART2_START, in, out, sz)
#define fhUartWriteBulk1(in, sz) fhUartWriteBulk(MEM_USART1_START, in, sz)
#define fhUartWriteBulk2(in, sz) fhUartWriteBulk(MEM_USART2_START, in, sz)
#define fhUartWriteBulk6(in, sz) fhUartWriteBulk(MEM_USART6_START, in, sz)

#define UART_DIV_SAMPLING16(_PCLK_, _BAUD_)            ((uint32_t)((((uint64_t)(_PCLK_))*25U)/(4U*((uint64_t)(_BAUD_)))))
#define UART_DIVMANT_SAMPLING16(_PCLK_, _BAUD_)        (UART_DIV_SAMPLING16((_PCLK_), (_BAUD_))/100U)
#define UART_DIVFRAQ_SAMPLING16(_PCLK_, _BAUD_)        ((((UART_DIV_SAMPLING16((_PCLK_), (_BAUD_)) - (UART_DIVMANT_SAMPLING16((_PCLK_), (_BAUD_)) * 100U)) * 16U) + 50U) / 100U)
/* UART BRR = mantissa + overflow + fraction
            = (UART DIVMANT << 4) + (UART DIVFRAQ & 0xF0) + (UART DIVFRAQ & 0x0FU) */
#define UART_BRR_SAMPLING16(_PCLK_, _BAUD_)            ((UART_DIVMANT_SAMPLING16((_PCLK_), (_BAUD_)) << 4U) + \
(UART_DIVFRAQ_SAMPLING16((_PCLK_), (_BAUD_)) & 0xF0U) + \
(UART_DIVFRAQ_SAMPLING16((_PCLK_), (_BAUD_)) & 0x0FU))

inline void _write(int pHandle, char* pData, int pSize)
{
    fhUartWriteBulk2((u8*) pData, pSize);
}

#endif //FH_UART_H
