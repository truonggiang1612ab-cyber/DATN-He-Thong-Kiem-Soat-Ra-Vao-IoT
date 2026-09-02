
#include "servo.h"

static volatile uint16_t servo_current_pulse = MIN_PULSE;
static volatile uint16_t servo_target_pulse = MIN_PULSE;
static volatile uint16_t servo_settle_elapsed_ms = SERVO_SETTLE_TIME_MS;
static volatile uint8_t servo_target_angle = 0U;
static volatile uint8_t servo_update_elapsed_ms = 0U;

// Chuyen goc servo thanh do rong xung PWM
static uint16_t Servo_AngleToPulse(uint8_t angle)
{
	if (angle > MAX_ANGLE) angle = MAX_ANGLE;
	return (uint16_t)(MIN_PULSE + ((uint32_t)angle * PULSE_RANGE) / MAX_ANGLE);
}

// Dat vi tri dich de servo tu di chuyen cham den goc yeu cau
void Servo_SetAngle(uint8_t angle)
{
	uint16_t target_pulse;
	if (angle > MAX_ANGLE) angle = MAX_ANGLE;
	target_pulse = Servo_AngleToPulse(angle);
	if (servo_target_angle == angle && servo_target_pulse == target_pulse) return;
	servo_target_pulse = target_pulse;
	servo_target_angle = angle;
	servo_settle_elapsed_ms = 0U;
}
// Cap nhat servo tung buoc nho sau moi 20 ms
void Servo_Tick1ms(void)
{
	uint16_t remaining_pulse;
	servo_update_elapsed_ms++;
	if (servo_update_elapsed_ms < SERVO_UPDATE_PERIOD_MS) return;
	servo_update_elapsed_ms = 0U;
	if (servo_current_pulse < servo_target_pulse)
	{
		remaining_pulse = servo_target_pulse - servo_current_pulse;
		if (remaining_pulse > SERVO_STEP_PULSE) servo_current_pulse += SERVO_STEP_PULSE;
		else servo_current_pulse = servo_target_pulse;
		servo_settle_elapsed_ms = 0U;
	}
	else if (servo_current_pulse > servo_target_pulse)
	{
		remaining_pulse = servo_current_pulse - servo_target_pulse;
		if (remaining_pulse > SERVO_STEP_PULSE) servo_current_pulse -= SERVO_STEP_PULSE;
		else servo_current_pulse = servo_target_pulse;
		servo_settle_elapsed_ms = 0U;
	}
	else if (servo_settle_elapsed_ms < SERVO_SETTLE_TIME_MS)
	{
		servo_settle_elapsed_ms += SERVO_UPDATE_PERIOD_MS;
		if (servo_settle_elapsed_ms > SERVO_SETTLE_TIME_MS) servo_settle_elapsed_ms = SERVO_SETTLE_TIME_MS;
	}
	__HAL_TIM_SET_COMPARE(&htim2, SERVO, servo_current_pulse);
}
// Kiem tra servo da den dung goc va on dinh co khi hay chua
uint8_t Servo_IsSettledAtAngle(uint8_t angle)
{
	if (angle > MAX_ANGLE) angle = MAX_ANGLE;
	if (servo_target_angle != angle) return 0U;
	if (servo_current_pulse != servo_target_pulse) return 0U;
	return servo_settle_elapsed_ms >= SERVO_SETTLE_TIME_MS;
}
// Khoi tao PWM va dat servo ngay tai vi tri cua dong
void Servo_Init(void)
{
	HAL_TIM_PWM_Start(&htim2, SERVO);
	servo_current_pulse = MIN_PULSE;
	servo_target_pulse = MIN_PULSE;
	servo_target_angle = 0U;
	servo_update_elapsed_ms = 0U;
	servo_settle_elapsed_ms = SERVO_SETTLE_TIME_MS;
	__HAL_TIM_SET_COMPARE(&htim2, SERVO, servo_current_pulse);
	HAL_GPIO_WritePin(OPEN_PORT, OPEN_PIN, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LOCK_PORT, LOCK_PIN, GPIO_PIN_SET);
}
