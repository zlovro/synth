//
// Created by Made on 06/07/2025.
//

#include "sser.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <tgmath.h>

#include "main.h"
#include "stm32h7xx_hal.h"
#include "usbd_cdc_if.h"
// #include "usbd_hid.h"
// #include "usbd_cdc_if.h"

u8*              gSserHidAudioBuf = NULL;
UART_HandleTypeDef *gSserUartDev;
USBD_HandleTypeDef *gSserUsbDev;
bool                gSserBusy = false;
u8                  gSserBuf[1024];
u32                 gSserHidAudioQueueSize = 0;


char gSserPrintfBuf[512];

void sserInitUart(UART_HandleTypeDef* pDev) {
    gSserUartDev = pDev;
}

void sserInitUsb(USBD_HandleTypeDef* pUsbDev) {
    gSserUsbDev = pUsbDev;
}

void sserPrintf(const char *pFormat, ...) {
    if (gSserUartDev->gState != HAL_UART_STATE_READY)
    {
        return;
    }

    va_list args;
    va_start(args, format);

    int len = vsprintf(gSserPrintfBuf, pFormat, args);

    va_end(args);

    HAL_UART_Transmit_DMA(gSserUartDev, (u8*)gSserPrintfBuf, len);
}

// pLen must be a multiple of HID audio size
void sserSendAudio(u8 *pDataIn, u16 pLen) {
    if (gSserBusy)
    {
        return;
    }

    gSserBusy = true;

    // gSserHidAudioBuf       = pDataIn;
    // gSserHidAudioQueueSize = pLen / SSER_HID_AUDIO_SIZE;
    //
    // gSserHidReportBuf[0] = SSER_HID_REPORT_ID_AUDIO;
    // memcpy(gSserHidReportBuf + 1, pDataIn, SSER_HID_AUDIO_SIZE);


    *(u16*)gSserBuf = 0xA0A0;
    memcpy(gSserBuf + 2, pDataIn, 0x200);
    CDC_Transmit_FS(gSserBuf, pLen + 2);
    // USBD_HID_SendReport(gSserUsbDev, gSserHidReportBuf, SSER_HID_AUDIO_REPORT_SIZE);
}
