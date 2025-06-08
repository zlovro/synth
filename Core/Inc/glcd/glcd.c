//
// Created by Made on 18/05/2025.
//

#include "glcd.h"

#include <cmsis_os2.h>
#include <main.h>
#include <stdio.h>
#include <stdlib.h>
#include <stm32h7xx_hal_spi.h>
#include <string.h>
#include <stdarg.h>
#include <ssys/ssys.h>

SPI_HandleTypeDef* gGlcdSpi = NULL;
u8                 gGlcdFrameBufBack[0x400];
u8                 gGlcdFrameBufFront[0x400];
GPIO_TypeDef*      gGlcdCsPort = NULL;
sfsGlyph*          gGlyphs     = NULL;
SD_HandleTypeDef*  gGlcdSd     = NULL;

bool gGlcdInitialized = false;

u16 gGlcdCsPin   = 0;
u8  gGlcdCursorX = 0;
u8  gGlcdCursorY = 0;
u8  gGlcdOriginX = 0;
u8  gGlcdOriginY = 0;


void glcdSpiTx(u8 pDat)
{
    HAL_SPI_Transmit(gGlcdSpi, &pDat, 1, 1000);
}

void glcdSpiTxBulk(u8* pDat, u8 pCount)
{
    glcdCsHigh();
    HAL_SPI_Transmit(gGlcdSpi, pDat, pCount, 1000);
    glcdCsLow();
}

void glcdCmd(u8 pRs, u8 pRw, u8 pData)
{
    GLCD_TIM->CNT = 0;

    glcdCsHigh();
    glcdSpiTx(0b11111000 | pRw << 2 | pRs << 1);
    glcdSpiTx(pData & 0b11110000);
    glcdSpiTx((pData & 0b1111) << 4);
    glcdCsLow();

    delayUsGlcdThread(72 - (s32)GLCD_TIM->CNT);
}

void glcdGdramSetAddr(u8 pX, u8 pY)
{
    glcdCmd(0, 0, 0b10000000 | pY);
    glcdCmd(0, 0, 0b10000000 | pX);
}

void glcdSeek(u8 pX, u8 pY)
{
    gGlcdCursorX = pX;
    gGlcdCursorY = pY;
}

void glcdSetOrigin(u8 pX, u8 pY)
{
    gGlcdOriginX = pX;
    gGlcdOriginY = pY;
}

void glcdDrawPixel(u8 pX, u8 pY, bool pSet)
{
    u8 idx                 = pY * 16 + pX / 8;
    u8 row                 = gGlcdFrameBufBack[idx];
    u8 rem                 = pX & 7;
    row                    = (row & ~(0x80 >> rem)) | (pSet << (7 - rem));
    gGlcdFrameBufBack[idx] = row;
}

void glcdTest()
{
    glcdGdramSetAddr(0, 0);
    glcdPrintf("testiramo printf.\ngSfsHeader = %x\ninstruments: %d\nmagic: %x\n", gSfsHeader, gSfsHeader->instrumentCount, gSfsHeader->magic);
}

void glcdClsSoft()
{
    memset(gGlcdFrameBufBack, 0, 0x400);
}

void glcdInit(SD_HandleTypeDef* pSd, SPI_HandleTypeDef* pSpi, GPIO_TypeDef* pCsPort, u16 pPin)
{
    HAL_Delay(50);

    gGlcdCsPort = pCsPort;
    gGlcdCsPin  = pPin;
    gGlcdSpi    = pSpi;
    gGlcdSd     = pSd;

    memset(gGlcdFrameBufFront, 0, 0x400);
    memset(gGlcdFrameBufBack, 0, 0x400);

    glcdClsSoft();

    gGlyphs                  = malloc(SFS_FONT_SIZE_ALIGNED);
    HAL_StatusTypeDef halRet = HAL_SD_ReadBlocks(gGlcdSd, (u8*)gGlyphs, gSfsHeader->fontDataBlockStart, SFS_FONT_SIZE_BLOCKS, 1000);

    if (halRet != HAL_OK)
    {
        return;
    }

    glcdCmd(0, 0, 0b00110000);
    delayUsGlcdThread(200);

    glcdCmd(0, 0, 0b00110000);
    delayUsGlcdThread(80);

    glcdCmd(0, 0, 1);
    osDelay(20);

    glcdCmd(0, 0, 6);
    delayUsGlcdThread(200);

    glcdCmd(0, 0, 12);
    delayUsGlcdThread(200);

    glcdCmd(0, 0, 0x34);
    delayUsGlcdThread(200);

    glcdCmd(0, 0, 0x36);
    delayUsGlcdThread(200);

    gGlcdInitialized = true;
}

void glcdDrawChar(char pChar)
{
    sfsGlyph* glyph = gGlyphs + pChar;

    u8 drawingStartEnd = glyph->drawingStartEnd;

    u8 drawingStart = (drawingStartEnd >> 0) & 0b111;
    u8 drawingEnd   = (drawingStartEnd >> 3) & 0b111;

    int glyphWidth = 1 + drawingEnd - drawingStart;

    if (gGlcdCursorX + glyphWidth > 128)
    {
        gGlcdCursorX = gGlcdOriginX;
        gGlcdCursorY += SYS_FONT_HEIGHT + SYS_FONT_SPACING_Y;
    }

    for (int localX = drawingStart, i = 0; i < glyphWidth && gGlcdCursorX < 128; gGlcdCursorX++, localX++, i++)
    {
        u8 column = glyph->cols[localX];

        u8 rightShift = gGlcdCursorX % 8;
        u8 mask       = ~(0x80 >> rightShift);

        for (int y = gGlcdCursorY, j = 7; j >= 0 && y < 64; j--, y++)
        {
            u8  rowIdx = gGlcdCursorX / 8;
            u16 pxIdx  = y * 16 + rowIdx;

            u8 row = gGlcdFrameBufBack[pxIdx];
            row &= mask;
            u8 bit = column << j & 0x80;
            row |= bit >> rightShift;

            gGlcdFrameBufBack[pxIdx] = row;
        }
    }
}

void glcdDrawStringLen(char* pString, u8 pLen)
{
    for (int i = 0; i < pLen; ++i)
    {
        char c = *pString++;
        if (c == '\n')
        {
            gGlcdCursorY += SYS_FONT_HEIGHT + SYS_FONT_SPACING_Y;
            gGlcdCursorX = gGlcdOriginX;
            continue;
        }

        glcdDrawChar(c);
        gGlcdCursorX += SYS_FONT_SPACING_X;
    }
}

void glcdDrawString(char* pString)
{
    glcdDrawStringLen(pString, strlen(pString));
}

void glcdDrawStringLenCenteredInRect(char* pString, u8 pLen, u8 pX0, u8 pY0, u8 pWidth, u8 pHeight, bool pVertically, bool pHorizontally)
{
    // glcdDrawRectangle(pX0, pY0, pWidth, pHeight, 1);

    if (!pVertically && !pHorizontally)
    {
        glcdSetOrigin(pX0, pY0);
        gGlcdCursorY = pY0;
        glcdDrawStringLen(pString, pLen);

        return;
    }

    u8 lineLengths[64 / SYS_FONT_HEIGHT];
    u8 pixelLenCurrentLine = 0;
    u8 pixelHeight         = 0;
    u8 line                = 0;

    for (int i = 0; i < pLen; ++i)
    {
        char c = pString[i];

        if (c != '\n')
        {
            sfsGlyph* glyph = gGlyphs + c;

            u8 drawingStartEnd = glyph->drawingStartEnd;

            u8 drawingStart = (drawingStartEnd >> 0) & 0b111;
            u8 drawingEnd   = (drawingStartEnd >> 3) & 0b111;

            int glyphWidth = SYS_FONT_SPACING_X + 1 + drawingEnd - drawingStart;
            pixelLenCurrentLine += glyphWidth;
        }

        if (c == '\n' || i == pLen - 1)
        {
            lineLengths[line++] = pixelLenCurrentLine;
            pixelLenCurrentLine = 0;
            pixelHeight += SYS_FONT_HEIGHT + SYS_FONT_SPACING_Y;
        }
    }

    f32 marginY = pVertically ? (pHeight - pixelHeight) / 2.0F : 0;
    f32 cursorY = marginY >= 0 ? pY0 + marginY : pY0;

    gGlcdCursorY = cursorY;

    line        = 0;
    u8 lastLine = 0xFF;

    for (int i = 0; i < pLen; ++i)
    {
        if (line != lastLine)
        {
            f32 marginX = pHorizontally ? (pWidth - lineLengths[line]) / 2.0F : 0;
            f32 cursorX = marginX >= 0 ? pX0 + marginX : pX0;

            // glcdDrawRectangle(pX0, gGlcdCursorY, marginX, SYS_FONT_HEIGHT + SYS_FONT_SPACING_Y, 1);

            gGlcdOriginX = cursorX;
            gGlcdCursorX = cursorX;
        }
        lastLine = line;

        // glcdDrawRectangle(gGlcdOriginX, gGlcdCursorY, max(lineLengths[line], 1), SYS_FONT_HEIGHT, 1);

        char c = *pString++;
        if (c == '\n')
        {
            line++;
            gGlcdCursorY += SYS_FONT_HEIGHT + SYS_FONT_SPACING_Y;
            continue;
        }

        glcdDrawChar(c);
        gGlcdCursorX += SYS_FONT_SPACING_X;
    }

    gGlcdCursorX = pX0;
}

void glcdDrawStringCenteredInRect(char* pString, u8 pX0, u8 pY0, u8 pWidth, u8 pHeight, bool pVertically, bool pHorizontally)
{
    glcdDrawStringLenCenteredInRect(pString, strlen(pString), pX0, pY0, pWidth, pHeight, pVertically, pHorizontally);
}

void glcdPrintf(char* pFormat, ...)
{
    va_list args;
    va_start(args, pFormat);

    char buf[256];
    vsprintf(buf, pFormat, args);

    glcdDrawString(buf);

    va_end(args);
}

void glcdCopyBackBufferToFront()
{
    memcpy(gGlcdFrameBufFront, gGlcdFrameBufBack, 0x400);
}

void glcdRenderThread()
{
    if (!gGlcdInitialized)
    {
        return;
    }

    for (int i = 0; i < 32; ++i)
    {
        glcdGdramSetAddr(0, i);

        glcdCsHigh();
        glcdSpiTx(0xFA);

        for (int j = 0; j < 16; ++j)
        {
            u8 row = gGlcdFrameBufFront[i * 16 + j];
            glcdSpiTx(row & 0xF0);
            glcdSpiTx(row << 4);
        }

        for (int j = 0; j < 16; ++j)
        {
            u8 row = gGlcdFrameBufFront[(32 + i) * 16 + j];
            glcdSpiTx(row & 0xF0);
            glcdSpiTx(row << 4);
        }

        glcdCsLow();
    }
}

void glcdDrawLineHorizontal(u8 pX0, u8 pY0, u8 pLength)
{
    u8 length = pLength;
    if (pX0 + length > 128)
    {
        length -= pX0;
    }

    u8 modX = pX0 & 7;

    u16 yIdx        = pY0 * 16;
    int rowIdxStart = pX0 / 8;
    if (modX)
    {
        u8 leftShift = 8 - modX;
        gGlcdFrameBufBack[yIdx + rowIdxStart] |= (1 << leftShift) - 1;
        if (leftShift > length)
        {
            gGlcdFrameBufBack[yIdx + rowIdxStart] &= ~((1 << (length - modX)) - 1);
            return;
        }

        if (leftShift == length)
        {
            return;
        }

        length -= leftShift;
        rowIdxStart++;
    }

    int rowIdx, i;
    for (i = 0, rowIdx = rowIdxStart; i < length / 8; rowIdx++, i++)
    {
        gGlcdFrameBufBack[yIdx + rowIdx] = 0xFF;
    }

    u8 modLen = length & 7;
    if (modLen)
    {
        gGlcdFrameBufBack[yIdx + rowIdx] |= ((1 << modLen) - 1) << (8 - modLen);
    }
}

void glcdDrawLineVertical(u8 pX0, u8 pY0, u8 pLength)
{
    u8 length = pLength;
    if (pY0 + length > 64)
    {
        length -= pY0;
    }

    u8 shift = pX0 & 7;
    u8 xIdx  = pX0 / 8;

    for (int y = pY0, i = 0; i < length; y++, i++)
    {
        gGlcdFrameBufBack[(y * 16 + xIdx)] |= 0x80 >> shift;
    }
}

void glcdDrawRectangle(u8 pX0, u8 pY0, u8 pWidth, u8 pHeight, u8 pThickness)
{
    // upper line
    for (int i = 0; i < pThickness; ++i)
    {
        glcdDrawLineHorizontal(pX0, pY0 + i, pWidth);
    }

    // left line
    for (int i = 0; i < pThickness; ++i)
    {
        glcdDrawLineVertical(pX0 + i, pY0 + pThickness, pHeight - 2 * pThickness);
    }

    // lower line
    for (int i = 0; i < pThickness; ++i)
    {
        glcdDrawLineHorizontal(pX0, pY0 + pHeight - i - 1, pWidth);
    }

    // right line
    for (int i = 0; i < pThickness; ++i)
    {
        glcdDrawLineVertical(pX0 + pWidth - i - 1, pY0 + pThickness, pHeight - 2 * pThickness);
    }
}

void glcdDeinit()
{
    free(gGlyphs);
}
