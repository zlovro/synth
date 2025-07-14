/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <types.h>

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */


/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */


typedef pstruct {
    u16 pin;
    #ifndef SYNTHWIN
    GPIO_TypeDef *bank;
    #else
    void *bank;
    #endif
} gpioPin;

typedef struct {
    u8      id;
    gpioPin enable;
} mux;

typedef struct {
    gpioPin pinSig;
    gpioPin pinsSelect[4];
} muxMasterCfg;


// typedef struct
// {
//     u8 muxId;
//     u8 muxChan;
// } muxedPin;
//
//     typedef struct
//     {
//         muxedPin row, col;
//     } muxedPinRowCol;

typedef struct {
    u8 row, col;
} rowColCoord;

#define gpioSet(gpio, state) ((gpio)->bank->BSRR = (gpio)->pin << (state ? 0 : 16))
#define gpioModeInput(gpio) (gpio)->bank->MODER &= ~((3 << (2 * (gpio)->pin))); (gpio)->bank->PUPDR &= ~(2 << (2 * (gpio)->pin))
#define gpioModeOutput(gpio) ((gpio)->bank->MODER |= ((1 << (2 * (gpio)->pin))))
#define gpioGet(gpio) ((gpio)->bank->IDR & (gpio)->pin)

bool muxRead(mux *pMux, u8 pChan);

void muxWrite(mux *pMux, u8 pChan, bool pVal);

void serialPrintf(const char *format, ...);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define dac1ch1 audio
#define dac1ch2 triangle
#define usage4 us delay timer
#define usage3 poll timer
#define usage6 48khz dac
#define usage5 us timer
#define usage7 10khz timer
#define usage17 rotary encoder
#define LCD_CS_Pin GPIO_PIN_3
#define LCD_CS_GPIO_Port GPIOE
#define LCD_RESET_Pin GPIO_PIN_5
#define LCD_RESET_GPIO_Port GPIOE
#define LED0_Pin GPIO_PIN_1
#define LED0_GPIO_Port GPIOA
#define AUDIO_OUT_Pin GPIO_PIN_4
#define AUDIO_OUT_GPIO_Port GPIOA
#define PITCHBEND_IN_Pin GPIO_PIN_6
#define PITCHBEND_IN_GPIO_Port GPIOA
#define BTN_RUN_Pin GPIO_PIN_5
#define BTN_RUN_GPIO_Port GPIOC
#define MUX_EN_OG_KEY_ROW_Pin GPIO_PIN_8
#define MUX_EN_OG_KEY_ROW_GPIO_Port GPIOD
#define MUX_EN_OG_KEY_COL_Pin GPIO_PIN_9
#define MUX_EN_OG_KEY_COL_GPIO_Port GPIOD
#define MUX_EN_OG_KBD_ROW_Pin GPIO_PIN_10
#define MUX_EN_OG_KBD_ROW_GPIO_Port GPIOD
#define MUX_EN_OG_KBD_COL_Pin GPIO_PIN_11
#define MUX_EN_OG_KBD_COL_GPIO_Port GPIOD
#define MUX_EN_KBDV_ROW_Pin GPIO_PIN_12
#define MUX_EN_KBDV_ROW_GPIO_Port GPIOD
#define MUX_EN_KBDV_COL_Pin GPIO_PIN_13
#define MUX_EN_KBDV_COL_GPIO_Port GPIOD
#define MUX_EN_KEY_ROW_Pin GPIO_PIN_14
#define MUX_EN_KEY_ROW_GPIO_Port GPIOD
#define MUX_EN_KEY_COL_Pin GPIO_PIN_15
#define MUX_EN_KEY_COL_GPIO_Port GPIOD
#define MUX_SIG_Pin GPIO_PIN_3
#define MUX_SIG_GPIO_Port GPIOD
#define MUX_S0_Pin GPIO_PIN_4
#define MUX_S0_GPIO_Port GPIOD
#define MUX_S1_Pin GPIO_PIN_5
#define MUX_S1_GPIO_Port GPIOD
#define MUX_S2_Pin GPIO_PIN_6
#define MUX_S2_GPIO_Port GPIOD
#define MUX_S3_Pin GPIO_PIN_7
#define MUX_S3_GPIO_Port GPIOD
#define ROT_SW_Pin GPIO_PIN_3
#define ROT_SW_GPIO_Port GPIOB
#define ROT_B_Pin GPIO_PIN_4
#define ROT_B_GPIO_Port GPIOB
#define ROT_A_Pin GPIO_PIN_6
#define ROT_A_GPIO_Port GPIOB
#define BTN_DBG_Pin GPIO_PIN_7
#define BTN_DBG_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

#define TIM_KHZ_10 TIM7
#define TIM_DAC TIM6
#define TIM_US TIM5
#define TIM_DELAY_US TIM4

extern const mux gMuxOgKeyRow;
extern const mux gMuxOgKeyCol;
extern const mux gMuxOgKbdRow;
extern const mux gMuxOgKbdCol;
extern const mux gMuxKbdvRow;
extern const mux gMuxKbdvCol;
extern const mux gMuxKeyRow;
extern const mux gMuxKeyCol;

extern const mux gMuxList[];

extern const muxMasterCfg gMuxMasterCfg;

extern const rowColCoord gOgKeyMap[];
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
