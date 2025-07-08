//
// Created by Made on 06/07/2025.
//

#ifndef SSER_H
#define SSER_H

#include <types.h>
#include "stm32h7xx_hal.h"
#include "usbd_def.h"
// #include "usbd_def.h"

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

extern u8* gSserHidAudioBuf;
extern u32 gSserHidAudioQueueSize;
extern UART_HandleTypeDef* gSserUartDev;
extern USBD_HandleTypeDef* gSserUsbDev;
extern bool gSserBusy;

#define SSER_TX_BUF_DATA (gSserTxBuf + 5)
#define SSER_HID_AUDIO_SIZE 64
#define SSER_HID_AUDIO_REPORT_SIZE (SSER_HID_AUDIO_SIZE + 1)
#define SSER_HID_MAX_REPORT_SIZE SSER_HID_AUDIO_REPORT_SIZE

extern u8 gSserHidReportBuf[SSER_HID_MAX_REPORT_SIZE];

typedef enum : u16 {
    SSER_HID_USAGE_AUDIO,
} sserHidUsage;

typedef enum : u8 {
    SSER_HID_REPORT_ID_AUDIO,
} sserHidReportId;

void sserInitUart(UART_HandleTypeDef* pDev);
void sserInitUsb(USBD_HandleTypeDef* pUsbDev);
void sserPrintf(const char *pFormat, ...);
void sserSendAudio(u8 *pDataIn, u16 pLen);

#define sserTxWrite(data, type) *(type*)(gSserTxBuf + gSserTxOffset) = (data); gSserTxOffset += sizeof(type)
#define sserRead(data, type) *(type*)(data + gSserTxOffset); gSserTxOffset += sizeof(type)

#endif //SSER_H
