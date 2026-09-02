/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LCD_E_Pin GPIO_PIN_13
#define LCD_E_GPIO_Port GPIOC
#define TRANSCEIVER_A_Pin GPIO_PIN_14
#define TRANSCEIVER_A_GPIO_Port GPIOC
#define TRANSCEIVER_B_Pin GPIO_PIN_15
#define TRANSCEIVER_B_GPIO_Port GPIOC
#define ROW_1_Pin GPIO_PIN_0
#define ROW_1_GPIO_Port GPIOA
#define ROW_2_Pin GPIO_PIN_1
#define ROW_2_GPIO_Port GPIOA
#define ROW_3_Pin GPIO_PIN_4
#define ROW_3_GPIO_Port GPIOA
#define ROW_4_Pin GPIO_PIN_5
#define ROW_4_GPIO_Port GPIOA
#define COL_1_Pin GPIO_PIN_6
#define COL_1_GPIO_Port GPIOA
#define COL_2_Pin GPIO_PIN_7
#define COL_2_GPIO_Port GPIOA
#define COL_3_Pin GPIO_PIN_0
#define COL_3_GPIO_Port GPIOB
#define COL_4_Pin GPIO_PIN_1
#define COL_4_GPIO_Port GPIOB
#define AT24C256_SCL_Pin GPIO_PIN_10
#define AT24C256_SCL_GPIO_Port GPIOB
#define AT24C256_SDA_Pin GPIO_PIN_11
#define AT24C256_SDA_GPIO_Port GPIOB
#define SDA_Pin GPIO_PIN_12
#define SDA_GPIO_Port GPIOB
#define SCK_Pin GPIO_PIN_13
#define SCK_GPIO_Port GPIOB
#define MISO_Pin GPIO_PIN_14
#define MISO_GPIO_Port GPIOB
#define MOSI_Pin GPIO_PIN_15
#define MOSI_GPIO_Port GPIOB
#define BUZZER_Pin GPIO_PIN_8
#define BUZZER_GPIO_Port GPIOA
#define LED_OPEN_Pin GPIO_PIN_11
#define LED_OPEN_GPIO_Port GPIOA
#define LED_LOCK_Pin GPIO_PIN_12
#define LED_LOCK_GPIO_Port GPIOA
#define ECHO_Pin GPIO_PIN_15
#define ECHO_GPIO_Port GPIOA
#define SERVO_PWM_Pin GPIO_PIN_3
#define SERVO_PWM_GPIO_Port GPIOB
#define LCD_D4_Pin GPIO_PIN_4
#define LCD_D4_GPIO_Port GPIOB
#define LCD_D5_Pin GPIO_PIN_5
#define LCD_D5_GPIO_Port GPIOB
#define LCD_D6_Pin GPIO_PIN_6
#define LCD_D6_GPIO_Port GPIOB
#define LCD_D7_Pin GPIO_PIN_7
#define LCD_D7_GPIO_Port GPIOB
#define TRIG_Pin GPIO_PIN_8
#define TRIG_GPIO_Port GPIOB
#define LCD_RS_Pin GPIO_PIN_9
#define LCD_RS_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
