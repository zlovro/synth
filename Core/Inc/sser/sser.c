//
// Created by Made on 06/07/2025.
//

#include "sser.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

u8                  gSserTxBuf[SSER_TX_BUF_SIZE] = {};
u16                 gSserTxOffset                = 0;
sserMsg             gSserMsg                     = {};
UART_HandleTypeDef *gSserUart                     = NULL;

void sserInit(UART_HandleTypeDef *pDev) {
    gSserUart = pDev;
}

void sserSendMsg() {
    gSserTxOffset = 0;

    sserTxWrite(SSER_MSG_START, u16);
    sserTxWrite(gSserMsg.id, u8);
    sserTxWrite(gSserMsg.len, u16);

    gSserTxOffset += gSserMsg.len;

    sserTxWrite(SSER_MSG_END, u16);

    HAL_UART_Transmit_DMA(gSserUart, gSserTxBuf, gSserTxOffset);
}

void sserParseMsg(u8 *pDataIn) {
    gSserTxOffset = 0;

    u16 _ = sserRead(pDataIn, u16);

    gSserMsg.id  = sserRead(pDataIn, u8);
    gSserMsg.len = sserRead(pDataIn, u16);

    for (int i = 0; i < gSserMsg.len; ++i)
    {
        SSER_TX_BUF_DATA[i] = sserRead(pDataIn, u8);
    }

    _ = sserRead(pDataIn, u16);
    UNUSED(_);
}

void sserPrintf(const char *pFormat, ...) {
    va_list args;
    va_start(args, format);

    int len = vsprintf((str) SSER_TX_BUF_DATA, pFormat, args);

    va_end(args);

    gSserMsg.id  = SSER_MSG_ID_CONSOLE;
    gSserMsg.len = len;
    sserSendMsg();
}

void sserSendAudio(u8 *pDataIn, u16 pLen) {
    gSserMsg.id  = SSER_MSG_ID_AUDIO;
    gSserMsg.len = pLen;

    memcpy(SSER_TX_BUF_DATA, pDataIn, gSserMsg.len);

    sserSendMsg();
}
