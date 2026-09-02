/*
 * hc_sr04.h
 *
 *  Created on: Jun 27, 2026
 *      Author: TRUONGGIANG
 */
#ifndef INC_HCSR04_H_
#define INC_HCSR04_H_

#include "main.h"

extern TIM_HandleTypeDef htim3;

#define HCSR04_TRIG_PORT GPIOB
#define HCSR04_TRIG_PIN  GPIO_PIN_8

#define HCSR04_ECHO_PORT GPIOA
#define HCSR04_ECHO_PIN  GPIO_PIN_15

#define OBSTACLE_DISTANCE_CM 5

void HCSR04_Init(void);
uint16_t HCSR04_ReadDistance(void);
uint8_t HCSR04_HasObstacle(void);

#endif
