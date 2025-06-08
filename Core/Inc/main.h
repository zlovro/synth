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

void delayUs(TIM_TypeDef* pTim, volatile s32 pUs);

typedef pstruct
{
    u16           pin;
    GPIO_TypeDef* bank;
} gpioPin;

typedef struct
{
    u8      id;
    gpioPin enable;
} mux;

typedef struct
{
    gpioPin pinSig;
    gpioPin pinsSelect[4];
} muxMasterCfg;

#define gpioSet(gpio, state) HAL_GPIO_WritePin((gpio)->bank, (gpio)->pin, state)
#define gpioGet(gpio) HAL_GPIO_ReadPin((gpio)->bank, (gpio)->pin)

bool muxRead(mux* pMux, u8 pChan);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED0_Pin GPIO_PIN_1
#define LED0_GPIO_Port GPIOA
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
#define SPI6_CS_Pin GPIO_PIN_15
#define SPI6_CS_GPIO_Port GPIOA
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
#define BTN_RUN_Pin GPIO_PIN_0
#define BTN_RUN_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

extern mux gMuxOgKeyRow;
extern mux gMuxOgKeyCol;
extern mux gMuxOgKbdRow;
extern mux gMuxOgKbdCol;
extern mux gMuxKbdvRow;
extern mux gMuxKbdvCol;
extern mux gMuxKeyRow;
extern mux gMuxKeyCol;

    extern mux gMuxList[];

    extern muxMasterCfg gMuxMasterCfg;

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
