/*
 *  File: spi_io.cpp.example
 *  Author: Nelson Lombardo
 *  Year: 2015
 *  e-mail: nelson.lombardo@gmail.com
 *  License at the end of file.
 */

#include "stm32f4xx.h"
// #include "stm32f4xx_hal_spi.h"
#include "spi_io.h"

#include <main_ex.h>

#ifdef __cplusplus
extern "C" {
#endif

// SPI_HandleTypeDef gSpi;

void spiInitInternal(bool pFast = false)
{
    printf("Initializing %s SPI\n", pFast ? "fast" : "slow");

    // __SPI1_CLK_ENABLE();
    //
    // GPIO_InitTypeDef spiGpio;
    //
    // spiGpio.Pin       = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    // spiGpio.Mode      = GPIO_MODE_AF_PP;
    // spiGpio.Pull      = GPIO_PULLUP;
    // spiGpio.Speed     = GPIO_SPEED_HIGH;
    // spiGpio.Alternate = GPIO_AF5_SPI1;
    //
    // HAL_GPIO_Init(GPIOA, &spiGpio);
    //
    // spiGpio.Pin  = GPIO_PIN_4;
    // spiGpio.Mode = GPIO_MODE_OUTPUT_PP;
    // HAL_GPIO_Init(GPIOA, &spiGpio);
    //
    // gSpi = {};
    //
    // gSpi.Instance               = SPI1;
    // gSpi.Init.BaudRatePrescaler = pFast ? SPI_BAUDRATEPRESCALER_2 : SPI_BAUDRATEPRESCALER_256;
    // gSpi.Init.Direction         = SPI_DIRECTION_2LINES;
    // gSpi.Init.CLKPhase          = SPI_PHASE_1EDGE;
    // gSpi.Init.CLKPolarity       = SPI_POLARITY_LOW;
    // gSpi.Init.DataSize          = SPI_DATASIZE_8BIT;
    // gSpi.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    // gSpi.Init.TIMode            = SPI_TIMODE_DISABLE;
    // gSpi.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    // gSpi.Init.CRCPolynomial     = 7;
    // gSpi.Init.NSS               = SPI_NSS_SOFT;
    //
    // gSpi.Init.Mode = SPI_MODE_MASTER;
    //
    // HAL_SPI_Init(&gSpi);
    //
    // GPIO_InitTypeDef gpio = {GPIO_PIN_0, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_HIGH, 0};
    // HAL_GPIO_Init(GPIOA, &gpio);
}

/******************************************************************************
 Module Public Functions - Low level SPI control functions
******************************************************************************/

void SPI_Init(void)
{
    spiInitInternal();
}

BYTE SPI_RW(BYTE d)
{
    BYTE out;
    // HAL_SPI_TransmitReceive(&gSpi, &d, &out, 1, 500);
    return out;
}

void SPI_Release(void)
{
    for (WORD idx = 512; idx && SPI_RW(0xFF) != 0xFF; idx--);
}

void SPI_CS_Low(void)
{
    // HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
}

void SPI_CS_High(void)
{
    // HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
}

void SPI_Freq_High(void)
{
    // HAL_SPI_Abort(&gSpi);
    spiInitInternal(true);
}

extern void SPI_Freq_Low(void)
{
    // HAL_SPI_Abort(&gSpi);
    spiInitInternal(false);
}

uint32_t gTimerEnd;
bool     gTimerStatus = false;

void SPI_Timer_On(WORD ms)
{
    // gTimerEnd    = HAL_GetTick() + ms;
    gTimerStatus = true;
}

BOOL SPI_Timer_Status(void)
{
    if (!gTimerStatus)
    {
        return false;
    }

    // gTimerStatus = HAL_GetTick() < gTimerEnd;
    return gTimerStatus;
}

void SPI_Timer_Off(void)
{
    // gTimerStatus = false;
}

#ifdef SPI_DEBUG_OSC
 void SPI_Debug_Init(void)
{
    SIM_SCGC5 |= SIM_SCGC5_PORTA_MASK; // Port A enable
    PORTA_PCR12 = PORT_PCR_MUX(1) | PORT_PCR_PE_MASK  | PORT_PCR_PS_MASK;
    GPIOA_PDDR |= (1 << 12); // Pin is configured as general-purpose output, for the GPIO function.
    GPIOA_PDOR &= ~(1 << 12); // Off
}
 void SPI_Debug_Mark(void)
{
    GPIOA_PDOR |= (1 << 12); // On
    GPIOA_PDOR &= ~(1 << 12); // Off
}
# endif

#ifdef __cplusplus
}
#endif

/*
The MIT License (MIT)

Copyright (c) 2015 Nelson Lombardo

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
