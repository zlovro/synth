//
// Created by Made on 06/07/2025.
//

#ifndef SSER_H
#define SSER_H

#include <types.h>
#include "stm32h7xx_hal.h"

constexpr u16 SSER_MSG_START = 0xA0A0;
constexpr u16 SSER_MSG_END   = 0xE0E0;

typedef enum : u8 {
    SSER_MSG_ID_CONSOLE,
    SSER_MSG_ID_AUDIO
} sserMsgId;

typedef struct {
    sserMsgId id;
    u16       len;
} sserMsg;

#define SSER_TX_BUF_SIZE 2048
#define SSER_MAX_DATA_SIZE (SSER_TX_BUF_SIZE - 2 - 1 - 2 - 2)

extern u8                  gSserTxBuf[SSER_TX_BUF_SIZE];
extern u16                 gSserTxOffset;
extern sserMsg             gSserMsg;
extern UART_HandleTypeDef *gSserUart;


#define SSER_TX_BUF_DATA (gSserTxBuf + sizeof(u16) + sizeof(u8) + sizeof(u16))

void sserInit(UART_HandleTypeDef *pDev);
void sserSendMsg();
void sserParseMsg(u8 *pDataIn);
void sserPrintf(const char *pFormat, ...);
void sserSendAudio(u8 *pDataIn, u16 pLen);

#define sserTxWrite(data, type) *(type*)(gSserTxBuf + gSserTxOffset) = (data); gSserTxOffset += sizeof(type)
#define sserRead(data, type) *(type*)(data + gSserTxOffset); gSserTxOffset += sizeof(type)

#endif //SSER_H
