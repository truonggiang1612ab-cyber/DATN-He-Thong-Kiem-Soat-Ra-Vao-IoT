/*
 * servo.h
 *
 *  Created on: May 31, 2026
 *      Author: TRUONGGIANG
 */
#ifndef INC_SERVO_H_
#define INC_SERVO_H_

#include"main.h"

extern TIM_HandleTypeDef htim2;

#define SERVO TIM_CHANNEL_2
#define MAX_ANGLE 180U
#define MIN_PULSE 600U
#define MAX_PULSE 2600U
#define PULSE_RANGE (MAX_PULSE - MIN_PULSE)

#define SERVO_STEP_PULSE 6U
#define SERVO_UPDATE_PERIOD_MS 20U
#define SERVO_SETTLE_TIME_MS 200U

#define OPEN_PORT GPIOA
#define OPEN_PIN GPIO_PIN_11
#define LOCK_PORT GPIOA
#define LOCK_PIN GPIO_PIN_12

#define BUZZER_PORT GPIOA
#define BUZZER_PIN GPIO_PIN_8

void Servo_SetAngle(uint8_t angle);
void Servo_Tick1ms(void);
uint8_t Servo_IsSettledAtAngle(uint8_t angle);
void Servo_Init(void);

#endif /* INC_SERVO_H_ */
