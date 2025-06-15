//
// Created by Made on 18/05/2025.
//

#ifndef GLCD_H
#define GLCD_H

#include <types.h>
#include <stm32h7xx_hal.h>
#include <sfs.h>

#define GLCD_TIM TIM3
#define delayUsGlcdThread(u) delayUs(GLCD_TIM, u)

extern SD_HandleTypeDef*  gGlcdSd;
extern SPI_HandleTypeDef* gGlcdSpi;
extern GPIO_TypeDef*      gGlcdCsPort;
extern u16                gGlcdCsPin;
extern u8                 gGlcdFrameBufBack[0x400];
extern u8                 gGlcdFrameBufFront[0x400];
extern sfsGlyph*          gGlyphs;
extern u8                 gGlcdCursorX, gGlcdCursorY;
extern u8                 gGlcdOriginX, gGlcdOriginY;
extern bool gGlcdInitialized;

#define glcdCsLow() HAL_GPIO_WritePin(gGlcdCsPort, gGlcdCsPin, GPIO_PIN_RESET)
#define glcdCsHigh() HAL_GPIO_WritePin(gGlcdCsPort, gGlcdCsPin, GPIO_PIN_SET)

void glcdSpiTx(u8 pDat);
void glcdSpiTxBulk(u8* pDat, u8 pCount);
void glcdInit(SD_HandleTypeDef* pSd, SPI_HandleTypeDef* pSpi, GPIO_TypeDef* pCsPort, u16 pPin);
void glcdCmd(u8 pRs, u8 pRw, u8 pData);
void glcdGdramSetAddr(u8 pX, u8 pY);
void glcdSeek(u8 pX, u8 pY);
void glcdSetOrigin(u8 pX, u8 pY);

void glcdDrawPixel(u8 pX, u8 pY, bool pSet);

void glcdDrawChar(char pChar);
void glcdDrawStringLen(char* pString, u8 pLen);
void glcdDrawString(char* pString);
void glcdDrawStringLenCenteredInRect(char* pString, u8 pLen, u8 pX0, u8 pY0, u8 pWidth, u8 pHeight, bool pVertically, bool pHorizontally);
void glcdDrawStringCenteredInRect(char* pString, u8 pX0, u8 pY0, u8 pWidth, u8 pHeight, bool pVertically, bool pHorizontally);
void glcdPrintf(char* pFormat, ...);

void glcdDrawLineHorizontal(u8 pX0, u8 pY0, u8 pLength);
void glcdDrawLineVertical(u8 pX0, u8 pY0, u8 pLength);
void glcdDrawRectangle(u8 pX0, u8 pY0, u8 pWidth, u8 pHeight, u8 pThickness);

void glcdCopyBackBufferToFront();
void glcdRenderThread();
void glcdTest();
void glcdClsSoft();
void glcdDeinit();


#endif //GLCD_H
