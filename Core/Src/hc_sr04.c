/*
 * hc_sr04.c
 *
 *  Created on: Jun 27, 2026
 *      Author: TRUONGGIANG
 */
#include "hc_sr04.h"

static void HCSR04_Delay_us(uint16_t us)
{
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    while (__HAL_TIM_GET_COUNTER(&htim3) < us);
}

void HCSR04_Init(void)
{
    HAL_TIM_Base_Start(&htim3);
    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);
}

uint16_t HCSR04_ReadDistance(void)
{
    uint32_t timeout = 0;
    uint32_t echo_time = 0;

    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);
    HCSR04_Delay_us(2);

    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_SET);
    HCSR04_Delay_us(10);
    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);

    timeout = 30000;
    while (HAL_GPIO_ReadPin(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN) == GPIO_PIN_RESET)
    {
        if (timeout-- == 0) return 999;
        HCSR04_Delay_us(1);
    }

    __HAL_TIM_SET_COUNTER(&htim3, 0);

    timeout = 30000;
    while (HAL_GPIO_ReadPin(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN) == GPIO_PIN_SET)
    {
        if (timeout-- == 0) return 999;
    }

    echo_time = __HAL_TIM_GET_COUNTER(&htim3);

    return echo_time / 58;
}

uint8_t HCSR04_HasObstacle(void)
{
    uint16_t distance = HCSR04_ReadDistance();

    if (distance >= 2 && distance <= OBSTACLE_DISTANCE_CM)
        return 1;

    return 0;
}
