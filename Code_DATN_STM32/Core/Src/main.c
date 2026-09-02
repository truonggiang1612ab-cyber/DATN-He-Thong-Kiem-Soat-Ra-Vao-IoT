/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "lcd.h"
#include "keypad.h"
#include "servo.h"
#include "hc_sr04.h"
#include "as608.h"
#include "at24c256.h"
#include "rc522.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum
{
	STATE_DOOR_OPERATION,
	STATE_HOME_SCREEN,
	STATE_LOG_VIEW,
	STATE_ADMIN_AUTHENTICATION,
	STATE_ADMIN_NEW_PASSWORD_INPUT,
	STATE_ADMIN_DELETE_ALL_CARDS,
	STATE_ADMIN_DELETE_ALL_FINGERPRINTS,
	STATE_ADMIN_MENU,
	STATE_USER_AUTH_METHOD_MENU,
	STATE_AUTH_METHOD_DELETE_MENU,
	STATE_AUTH_METHOD_DELETE_CONFIRM,
	STATE_CREDENTIAL_MANAGEMENT_USER_ID_INPUT,
	STATE_CREDENTIAL_MANAGEMENT_USER_CONFIRMATION,
	STATE_ACTIVATION_CODE_INPUT,
	STATE_CURRENT_USER_PASSWORD_INPUT,
	STATE_NEW_NORMAL_PASSWORD_INPUT,
	STATE_NEW_EMERGENCY_PASSWORD_INPUT,
	STATE_NORMAL_FINGERPRINT_MANAGEMENT,
	STATE_EMERGENCY_FINGERPRINT_MANAGEMENT,
	STATE_NORMAL_CARD_MANAGEMENT,
	STATE_EMERGENCY_CARD_MANAGEMENT,
	STATE_CARD_CHANGES_SAVE_CONFIRM
} SystemState_t;

typedef enum
{
    DOOR_PHASE_OPENING,
    DOOR_PHASE_HOLDING_OPEN,
    DOOR_PHASE_CLOSING
} DoorPhase_t;

typedef enum
{
	CREDENTIAL_MANAGEMENT_FLOW_NONE,
	CREDENTIAL_MANAGEMENT_FLOW_CARD,
	CREDENTIAL_MANAGEMENT_FLOW_FINGERPRINT,
	CREDENTIAL_MANAGEMENT_FLOW_PASSWORD
} CredentialManagementFlow_t;

typedef enum
{
	ACCESS_AUTH_TARGET_NONE,
	ACCESS_AUTH_TARGET_DOOR_ACCESS,
	ACCESS_AUTH_TARGET_ADMIN_MENU
} AccessAuthTarget_t;

typedef enum
{
	PASSAGE_WAIT_FIRST_SENSOR,
	PASSAGE_A_BLOCKED_FIRST,
	PASSAGE_WAIT_SENSOR_B_AFTER_A,
	PASSAGE_B_BLOCKED_AFTER_A,
	PASSAGE_B_BLOCKED_FIRST,
	PASSAGE_WAIT_SENSOR_A_AFTER_B,
	PASSAGE_A_BLOCKED_AFTER_B,
	PASSAGE_WAIT_BOTH_CLEAR
} PassageState_t;

typedef enum
{
	PASSAGE_DIRECTION_NONE,
	PASSAGE_DIRECTION_ENTERING,
	PASSAGE_DIRECTION_EXITING
} PassageDirection_t;

typedef enum
{
	LCD_ALIGNMENT_LEFT,
	LCD_ALIGNMENT_CENTER
} LcdAlignment_t;

typedef enum
{
	LCD_BEEP_NONE,
	LCD_BEEP_ONCE,
	LCD_BEEP_ERROR
} LcdBeepPattern_t;

typedef enum
{
	ACCESS_INFO_DISPLAY_INACTIVE,
	ACCESS_INFO_DISPLAY_USER_INFO,
	ACCESS_INFO_DISPLAY_TIME_DATE
} AccessInfoDisplayPhase_t;

typedef enum
{
	ACCESS_DIRECTION_EXITING,
	ACCESS_DIRECTION_ENTERING
} AccessDirection_t;

typedef enum
{
	AUTH_DELETE_NONE,
	AUTH_DELETE_NORMAL,
	AUTH_DELETE_EMERGENCY,
	AUTH_DELETE_BOTH
} AuthDeleteOption_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define ACCESS_DISPLAY_DURATION_MS 2000U
#define UI_IDLE_TIMEOUT_MS 30000U
#define FINGER_PLACE_TIMEOUT_MS 10000U
#define FINGER_REMOVE_TIMEOUT_MS 10000U
#define ADMIN_USER_ID 1U
#define ADMIN_USER_INDEX (ADMIN_USER_ID - 1U)
#define AUTH_METHOD_CARD_MASK (1U << 0)
#define AUTH_METHOD_FINGER_MASK (1U << 1)
#define AUTH_METHOD_PASSWORD_MASK (1U << 2)
#define ALL_AUTH_METHODS_MASK (AUTH_METHOD_CARD_MASK | AUTH_METHOD_FINGER_MASK | AUTH_METHOD_PASSWORD_MASK)
#define DOOR_REQUIRED_AUTH_METHOD_COUNT 2U
#define DOOR_OPEN_HOLD_MS 5000U
#define USER_RECORDS   		 (myConfig.users)
#define ACCESS_LOG_RECORDS   (myConfig.access_logs)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
 I2C_HandleTypeDef hi2c2;

SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

// Trạng thái hệ thống
SystemState_t current_system_state = STATE_HOME_SCREEN;
DoorPhase_t current_door_phase = DOOR_PHASE_HOLDING_OPEN;
CredentialManagementFlow_t current_credential_management_flow = CREDENTIAL_MANAGEMENT_FLOW_NONE;
AccessAuthTarget_t access_auth_target = ACCESS_AUTH_TARGET_NONE;
AuthDeleteOption_t selected_auth_delete_option = AUTH_DELETE_NONE;
PassageState_t current_passage_state = PASSAGE_WAIT_FIRST_SENSOR;
PassageDirection_t passage_direction = PASSAGE_DIRECTION_NONE;
AccessInfoDisplayPhase_t access_info_display_phase = ACCESS_INFO_DISPLAY_INACTIVE;

// Input
uint8_t input_length = 0;
char input_buffer[10] = {0};

//Quản lý quá trình thêm phương thức xác thực
uint8_t credential_management_user_index = 0;
uint8_t is_normal_fingerprint_registered = 0;
uint16_t expected_fingerprint_template_id = 0;
char pending_normal_password[USER_PASSWORD_LENGTH + 1U] = {0};
UserCardData_t user_card_backup = {0};
uint8_t is_card_edit_active = 0;

// Quản lý phiên xác thực đa yếu tố mở cửa và menu admin
uint8_t is_access_auth_active = 0;
uint8_t is_access_auth_linked_to_user = 0;
uint8_t access_auth_user_index = 0;
uint8_t verified_auth_method_mask = 0;
uint8_t is_emergency_auth_method_used = 0;

// State
uint32_t last_ui_activity_tick = 0;

// Door state
uint8_t is_door_obstacle_detected = 0U;
uint32_t door_hold_start_tick = 0;

// Passage state
uint8_t is_passage_tracking_active = 0U;
uint8_t tracked_passage_user_index = 0U;
uint8_t is_passage_direction_result_ready = 0U;
char detected_passage_time[9] = "00:00:00";
char detected_passage_date[11] = "00/00/0000";

// ESP32 UART
uint8_t esp_received_byte;
char esp_receive_buffer[128];
uint8_t esp_receive_buffer_index = 0;
volatile uint8_t is_esp_door_open_requested = 0;
volatile uint8_t is_esp_system_reset_requested = 0;
volatile uint8_t is_esp_access_log_requested = 0;
volatile uint8_t is_esp_user_command_ready = 0;
char esp_user_command[128] = {0};

// Time date
char current_time[9] = "00:00:00";
char current_date[11] = "00/00/0000";

// RFID
MFRC522_Name rfid_reader;
RFID_UID_t scanned_card_uid;
uint8_t is_rfid_scan_handled = 0;

// Fingerprint sensor
AS608_t finger_sensor;
uint8_t is_finger_sensor_ready = 0;
uint32_t last_finger_reconnect_attempt_tick = 0;
uint32_t last_finger_poll_tick = 0;
uint8_t is_finger_scan_handled = 0;

// Ký tự tùy chỉnh LCD
uint8_t lcd_lock_icon[8] = { 0x0E, 0x11, 0x11, 0x1F, 0x1B, 0x1B, 0x1F, 0x00 };
uint8_t lcd_unlock_icon[8] = { 0x0E, 0x11, 0x10, 0x1F, 0x1B, 0x1B, 0x1F, 0x00 };

// User And Log
uint8_t existing_user_count = 0;
uint8_t current_displayed_log_index = 0;
uint8_t access_log_count = 0;
uint8_t next_log_write_index = 0;
uint32_t access_info_display_start_tick = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C2_Init(void);
/* USER CODE BEGIN PFP */

// Giao tiếp ESP32
void ESP_SendLine(char *prefix, char *text);
void ESP_SendLCD(char *line1, char *line2);
void ESP_SendLCD2(char *line2);

// LCD, còi và giao diện
void Flash_Buzzer(uint8_t count, uint16_t on_ms, uint16_t off_ms);
void LCD_ShowCenteredInput(char *text);
void LCD_ShowPasswordMask6(void);
void LCD_CreateChar(uint8_t custom_char_slot, uint8_t custom_char_bitmap[]);
void LCD_LoadCustomChars(void);
void LCD_ShowMessage(char *line1, char *line2, LcdAlignment_t alignment, LcdBeepPattern_t beep_pattern, uint16_t delay_ms);
void HomeScreen(void);
void UI_Timeout_Process(void);
void Admin_ShowSystemMenu(void);
void Admin_ShowPasswordInput(void);

// Dieu khien cua tu dong
void Door_Open_Auto(void);
void Door_ShowOpenStatus(void);
void Door_AutoControl_Process(void);

// Quản lý dữ liệu ngư�?i dùng
uint8_t UserDB_UserExists(uint8_t user_index);
uint8_t UserDB_CanAuthenticate(uint8_t user_index);
uint8_t UserDB_CanManage(uint8_t user_index);
uint8_t UserDB_IsSixDigitText(const char *text);
uint8_t UserDB_PasswordExists(const char *password, uint8_t excluded_user_index);
void UserDB_Recount(void);
void UserDB_SendAck(const char *command_id, const char *ack_result);
int16_t FindUserByAnyPassword(const char *password, uint8_t *matched_emergency_password);
int16_t FindUserByFingerId(uint16_t fingerprint_template_id, uint8_t *matched_emergency_fingerprint);

// Nhật ký ra vào
void Access_ShowUserInfo(uint8_t user_index, char *access_action_text);
void Access_ShowTimeDate(void);
void AccessLog_Save(uint8_t user_index, AccessDirection_t access_direction);
void Passage_Process(void);
void AccessLog_Show(uint8_t log_index);
void ESP_SendAccessLogs(void);
void AccessDisplay_Process(void);

// Hỗ trợ quy trình xác thực
void Auth_ResetInput(void);
void Auth_BeginCardEditSession(void);
void Auth_CancelCardEditSession(void);
uint8_t Auth_InputDigit(char key, uint8_t max_length, uint8_t mask_input);

// Xác thực đa yếu tố mở cửa và menu admin
void AccessAuth_Reset(void);
uint8_t AccessAuth_CountMethods(uint8_t verified_method_mask);
void AccessAuth_Start(AccessAuthTarget_t new_auth_target);
void AccessAuth_ShowProgress(void);
uint8_t AccessAuth_LinkUser(uint8_t user_index);
void AccessAuth_RecordVerifiedMethod(uint8_t user_index, uint8_t auth_method_mask_to_add, uint8_t is_verified_method_emergency);
void AccessAuth_ProcessPassword(const char *password);
void AccessAuth_ProcessCard(RFID_UID_t *card);
void AccessAuth_Complete(void);

// Màn hình quy trình xác thực
void Auth_ShowUserIdInput(void);
void Auth_ShowConfirmUser(void);
void Auth_ShowActivationCode(void);
void Auth_ShowCurrentUserPasswordInput(void);
void Auth_ShowNewNormalPasswordInput(void);
void Auth_ShowNewEmergencyPasswordInput(void);
void Auth_ShowNormalFinger(void);
void Auth_ShowEmergencyFinger(void);
void Auth_ShowNormalCard(void);
void Auth_ShowEmergencyCard(void);
void Auth_ShowUserManageMenu(void);
void Auth_ShowUserDeleteMenu(void);
void Auth_ShowUserDeleteConfirm(AuthDeleteOption_t delete_option);
void Auth_ShowCardSaveConfirm(void);

// Dieu huong quy trinh xac thuc
void Auth_StartFingerFlow(void);
void Auth_StartCardFlow(void);
void Auth_StartPasswordFlow(void);
void Auth_ValidateTemplateIdAndEnrollFingerprint(uint8_t is_emergency_fingerprint);
void Auth_EnrollCard(RFID_UID_t *card_uid, uint8_t is_emergency_card);

// Xử lý thẻ RFID
uint8_t Auth_HasDeletableCard(void);
uint8_t Auth_CardAlreadyExists(RFID_UID_t *card_uid);
int16_t Auth_FindCardOwner(RFID_UID_t *card_uid, uint8_t include_emergency_card, uint8_t *matched_emergency_card);
void RFID_Process(void);
void Auth_ScanEnrollCard(uint8_t is_emergency_card);

// Xử lý vân tay
void Finger_HandleConnectionError(uint8_t sensor_status);
uint8_t Finger_CheckTemplate(uint16_t fingerprint_template_id, uint8_t *template_exists);
uint8_t Finger_DeleteTemplateIfExists(uint16_t fingerprint_template_id, uint8_t *was_template_deleted);
uint8_t Finger_DeleteSelectedTemplates(uint16_t normal_template_id, uint16_t emergency_template_id, AuthDeleteOption_t delete_option, uint8_t *was_any_template_deleted);
uint8_t Auth_WaitFingerPlaced(uint32_t timeout_ms);
uint8_t Auth_WaitFingerRemoved(uint32_t timeout_ms);
uint8_t Auth_EnrollFinger(uint16_t fingerprint_template_id);
void Finger_Process(void);

// Lệnh từ ESP32, Firebase và Blynk
void UserDB_ProcessCommand(void);
void ESP_Command_Process(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Gửi một dòng dữ liệu sang ESP32 theo định dạng prefix + nội dung + ký tự xuống dòng
void ESP_SendLine(char *prefix, char *text)
{
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%s%s\n", prefix, text);
	HAL_UART_Transmit(&huart1, (uint8_t*) buffer, strlen(buffer), 100);
}
// Gửi đồng th�?i hai dòng LCD sang ESP32 để đồng bộ giao diện Blynk
void ESP_SendLCD(char *line1, char *line2)
{
	ESP_SendLine("LCD1:", line1);
	ESP_SendLine("LCD2:", line2);
}
// Chỉ gửi nội dung dòng LCD thứ hai sang ESP32
void ESP_SendLCD2(char *line2)
{
	ESP_SendLine("LCD2:", line2);
}
// Nhay coi theo so lan va thoi gian yeu cau
void Flash_Buzzer(uint8_t count, uint16_t on_ms, uint16_t off_ms)
{
	for (uint8_t i = 0U; i < count; i++)
	{
		HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
		HAL_Delay(on_ms);
		HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
		HAL_Delay(off_ms);
	}
	HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
}
// Căn giữa nội dung và hiển thị trực tiếp các số  đã nhập
void LCD_ShowCenteredInput(char *text)
{
	char input_frame[7] = "______";
	uint8_t text_length;
	uint8_t start_column;
	if (text == NULL) text = "";
	text_length = strlen(text);
	if (text_length > 6) text_length = 6;
	start_column = (6 - text_length) / 2;
	memcpy(&input_frame[start_column], text, text_length);
	LCD_SetCursor(1, 0);
	LCD_Puts("                ");
	LCD_SetCursor(1, 5);
	LCD_Puts(input_frame);
	ESP_SendLCD2(input_frame);
}
// Hiển thị mật khẩu đã nhập bằng dấu sao và phần còn lại bằng dấu gạch dưới
void LCD_ShowPasswordMask6(void)
{
	char password_mask[USER_PASSWORD_LENGTH + 1U];
	uint8_t masked_character_count = input_length;
	if (masked_character_count > USER_PASSWORD_LENGTH) masked_character_count = USER_PASSWORD_LENGTH;
	for (uint8_t i = 0; i < USER_PASSWORD_LENGTH; i++)
	{
		if (i < masked_character_count) password_mask[i] = '*';
		else password_mask[i] = '_';
	}
	password_mask[USER_PASSWORD_LENGTH] = '\0';
	LCD_SetCursor(1, 0);
	LCD_Puts("                ");
	LCD_SetCursor(1, 5);
	LCD_Puts(password_mask);
	ESP_SendLCD2(password_mask);
}
// Ghi một mẫu ký tự tùy chỉnh vào bộ nhớ CGRAM của LCD
void LCD_CreateChar(uint8_t custom_char_slot, uint8_t custom_char_bitmap[])
{
	custom_char_slot &= 0x07;
	LCD_Send(0x40 + (custom_char_slot * 8), LCD_COMMAND);
	for (uint8_t i = 0; i < 8; i++)
	{
		LCD_Send(custom_char_bitmap[i], LCD_DATA);
	}
}
// Nap bieu tuong khoa va mo khoa vao LCD
void LCD_LoadCustomChars(void)
{
	LCD_CreateChar(0U, lcd_lock_icon);
	LCD_CreateChar(1U, lcd_unlock_icon);
}
// Hien thi hai dong LCD theo kieu can le duoc yeu cau
void LCD_ShowMessage(char *line1, char *line2, LcdAlignment_t alignment, LcdBeepPattern_t beep_pattern, uint16_t delay_ms)
{
	size_t line1_length;
	size_t line2_length;
	uint8_t line1_start_column = 0U;
	uint8_t line2_start_column = 0U;
	if (line1 == NULL) line1 = "";
	if (line2 == NULL) line2 = "";
	if (alignment == LCD_ALIGNMENT_CENTER)
	{
		line1_length = strlen(line1);
		line2_length = strlen(line2);
		if (line1_length < 16U) line1_start_column = (uint8_t) ((16U - line1_length) / 2U);
		if (line2_length < 16U) line2_start_column = (uint8_t) ((16U - line2_length) / 2U);
	}
	LCD_Send(0x01, LCD_COMMAND);
	LCD_SetCursor(0U, line1_start_column);
	LCD_Puts(line1);
	LCD_SetCursor(1U, line2_start_column);
	LCD_Puts(line2);
	ESP_SendLCD(line1, line2);
	if (beep_pattern == LCD_BEEP_ONCE) Flash_Buzzer(1U, 50U, 1U);
	else if (beep_pattern == LCD_BEEP_ERROR) Flash_Buzzer(3U, 50U, 50U);
	if (delay_ms > 0U) HAL_Delay(delay_ms);
	last_ui_activity_tick = HAL_GetTick();
}
// Xóa trạng thái thao tác cũ và đưa hệ thống v�? màn hình cửa khóa
void HomeScreen(void)
{
	Auth_CancelCardEditSession();
	AccessAuth_Reset();
	is_passage_tracking_active = 0U;
	access_info_display_phase = ACCESS_INFO_DISPLAY_INACTIVE;
	current_credential_management_flow = CREDENTIAL_MANAGEMENT_FLOW_NONE;
	memset(pending_normal_password, 0, sizeof(pending_normal_password));
	current_system_state = STATE_HOME_SCREEN;
	last_ui_activity_tick = HAL_GetTick();
	LCD_Send(0x01, LCD_COMMAND);
	LCD_SetCursor(0, 12);
	LCD_Send(0, LCD_DATA);
	LCD_SetCursor(0, 3);
	LCD_Puts("CUA KHOA");
	ESP_SendLCD("CUA KHOA", "");
}
// Hien thi menu quan tri
void Admin_ShowSystemMenu(void)
{
	current_system_state = STATE_ADMIN_MENU;
	Auth_ResetInput();
	LCD_ShowMessage("1:L/S 2:DOI PASS", "3:ALLTHE 4:ALLVT", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
}
// Hien thi man hinh nhap pass admin moi
void Admin_ShowPasswordInput(void)
{
	current_system_state = STATE_ADMIN_NEW_PASSWORD_INPUT;
	Auth_ResetInput();
	LCD_ShowMessage("DOI PASS ADMIN", "", LCD_ALIGNMENT_CENTER, LCD_BEEP_NONE, 0);
	LCD_ShowCenteredInput("");
}
// Tự hủy thao tác và trở ve màn hình chính khi nguoi dùng không thao tác quá lâu
void UI_Timeout_Process(void)
{
	if (current_system_state == STATE_HOME_SCREEN && !is_access_auth_active && input_length == 0) return;
	if (current_system_state == STATE_DOOR_OPERATION) return;
	if (HAL_GetTick() - last_ui_activity_tick < UI_IDLE_TIMEOUT_MS) return;
	LCD_ShowMessage("DA QUA THOI GIAN", "THAO TAC", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 1500);
	HomeScreen();
}
// Tat hai den va bat dau qua trinh mo cua cham
void Door_Open_Auto(void)
{
	HAL_GPIO_WritePin(OPEN_PORT, OPEN_PIN, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LOCK_PORT, LOCK_PIN, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
	access_info_display_phase = ACCESS_INFO_DISPLAY_INACTIVE;
	access_info_display_start_tick = 0U;
	Servo_SetAngle(90U);
	current_door_phase = DOOR_PHASE_OPENING;
	is_door_obstacle_detected = 0U;
	door_hold_start_tick = 0U;
}
// Hien thi trang thai cua da mo va xoa noi dung dong hai
void Door_ShowOpenStatus(void)
{
	LCD_Send(0x01, LCD_COMMAND);
	LCD_SetCursor(0, 2);
	LCD_Puts("CUA DA MO");
	LCD_SetCursor(0, 12);
	LCD_Send(1, LCD_DATA);
	ESP_SendLCD("CUA DA MO", "");
}
void Door_AutoControl_Process(void)
{
	if (current_system_state != STATE_DOOR_OPERATION) return;
	if (current_door_phase == DOOR_PHASE_CLOSING && !is_passage_tracking_active && access_info_display_phase != ACCESS_INFO_DISPLAY_INACTIVE) return;
	switch (current_door_phase)
	{
	case DOOR_PHASE_OPENING:
		if (!Servo_IsSettledAtAngle(90U)) break;
		HAL_GPIO_WritePin(OPEN_PORT, OPEN_PIN, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LOCK_PORT, LOCK_PIN, GPIO_PIN_RESET);
		Flash_Buzzer(1U, 50U, 1U);
		ESP_SendLine("DOOR:", "OPEN");
		access_info_display_phase = ACCESS_INFO_DISPLAY_INACTIVE;
		access_info_display_start_tick = 0U;
		current_door_phase = DOOR_PHASE_HOLDING_OPEN;
		door_hold_start_tick = HAL_GetTick();
		if (HCSR04_HasObstacle())
		{
			is_door_obstacle_detected = 1U;
			LCD_ShowMessage("PHAT HIEN VAT", "CAN:CUA LUON MO", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
		}
		else
		{
			is_door_obstacle_detected = 0U;
			Door_ShowOpenStatus();
		}
		break;
	case DOOR_PHASE_HOLDING_OPEN:
		if (HCSR04_HasObstacle())
		{
			if (!is_door_obstacle_detected)
			{
				is_door_obstacle_detected = 1U;
				LCD_ShowMessage("PHAT HIEN VAT", "CAN:CUA LUON MO", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
			}
			break;
		}
		if (is_door_obstacle_detected)
		{
			is_door_obstacle_detected = 0U;
			door_hold_start_tick = HAL_GetTick();
			LCD_ShowMessage("KHONG CO VAT CAN", "CUA VAN DANG MO", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
			break;
		}
		if (HAL_GetTick() - door_hold_start_tick < DOOR_OPEN_HOLD_MS) break;
		HAL_GPIO_WritePin(OPEN_PORT, OPEN_PIN, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LOCK_PORT, LOCK_PIN, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
		access_info_display_phase = ACCESS_INFO_DISPLAY_INACTIVE;
		access_info_display_start_tick = 0U;
		LCD_ShowMessage("CUA DANG DONG", "CHU Y AN TOAN", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
		current_door_phase = DOOR_PHASE_CLOSING;
		Servo_SetAngle(0U);
		break;
	case DOOR_PHASE_CLOSING:
		if (HCSR04_HasObstacle())
		{
			Servo_SetAngle(90U);
			current_door_phase = DOOR_PHASE_OPENING;
			is_door_obstacle_detected = 1U;
			LCD_ShowMessage("PHAT HIEN VAT", "CAN:MO LAI CUA", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
			break;
		}
		if (!Servo_IsSettledAtAngle(0U)) break;
		HAL_GPIO_WritePin(OPEN_PORT, OPEN_PIN, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LOCK_PORT, LOCK_PIN, GPIO_PIN_SET);
		Flash_Buzzer(1U, 50U, 1U);
		ESP_SendLine("DOOR:", "CLOSE");
		if (is_passage_tracking_active)
		{
			is_passage_tracking_active = 0U;
			if (current_passage_state == PASSAGE_WAIT_FIRST_SENSOR && is_passage_direction_result_ready && passage_direction == PASSAGE_DIRECTION_ENTERING)
			{
				AccessLog_Save(tracked_passage_user_index, ACCESS_DIRECTION_ENTERING);
				break;
			}
			if (current_passage_state == PASSAGE_WAIT_FIRST_SENSOR && is_passage_direction_result_ready && passage_direction == PASSAGE_DIRECTION_EXITING)
			{
				AccessLog_Save(tracked_passage_user_index, ACCESS_DIRECTION_EXITING);
				break;
			}
		}
		HomeScreen();
		break;
	default:
		HomeScreen();
		break;
	}
}
// Kiểm tra vị trí user có nằm trong giới hạn và đang chứa dữ liệu hay không
uint8_t UserDB_UserExists(uint8_t user_index)
{
	return user_index < MAX_USERS && USER_RECORDS[user_index].status != USER_STATUS_EMPTY;
}
// Kiểm tra user có trạng thái ACTIVE và được phép xác thực mở cửa hay không
uint8_t UserDB_CanAuthenticate(uint8_t user_index)
{
	return user_index < MAX_USERS && USER_RECORDS[user_index].status == USER_STATUS_ACTIVE;
}
// Kiểm tra user có trạng thái PENDING hoặc ACTIVE để được phép quản lý
uint8_t UserDB_CanManage(uint8_t user_index)
{
	return user_index < MAX_USERS && (USER_RECORDS[user_index].status == USER_STATUS_PENDING || USER_RECORDS[user_index].status == USER_STATUS_ACTIVE);
}
// Kiểm tra chuỗi có đúng sáu ký tự số hay không
uint8_t UserDB_IsSixDigitText(const char *text)
{
	if (text == NULL || strlen(text) != USER_PASSWORD_LENGTH) return 0;
	for (uint8_t i = 0; i < USER_PASSWORD_LENGTH; i++)
	{
		if (text[i] < '0' || text[i] > '9') return 0;
	}
	return 1;
}
// Kiểm tra mật khẩu thường hoặc khẩn cấp đã tồn tại trong cơ sơ dữ liệu người dùng chưa
uint8_t UserDB_PasswordExists(const char *password, uint8_t excluded_user_index)
{
	if (!UserDB_IsSixDigitText(password)) return 1;
	for (uint8_t i = 0; i < MAX_USERS; i++)
	{
		if (i == excluded_user_index || !UserDB_UserExists(i)) continue;
		if (strcmp(USER_RECORDS[i].user_password, password) == 0) return 1;
		if (strcmp(USER_RECORDS[i].duress_password, password) == 0) return 1;
	}
	return 0;
}
// Tim user theo mat khau thuong hoac mat khau khan cap trong mot lan quet
int16_t FindUserByAnyPassword(const char *password, uint8_t *matched_emergency_password)
{
	if (matched_emergency_password != NULL) *matched_emergency_password = 0U;
	if (!UserDB_IsSixDigitText(password)) return -1;
	for (uint16_t i = 0U; i < MAX_USERS; i++)
	{
		if (!UserDB_CanAuthenticate((uint8_t) i)) continue;
		if (strcmp(USER_RECORDS[i].user_password, password) == 0) return (int16_t) i;
		if (strcmp(USER_RECORDS[i].duress_password, password) == 0)
		{
			if (matched_emergency_password != NULL) *matched_emergency_password = 1U;
			return (int16_t) i;
		}
	}
	return -1;
}
// Tinh truc tiep user tu ID van tay co dinh
int16_t FindUserByFingerId(uint16_t fingerprint_template_id, uint8_t *matched_emergency_fingerprint)
{
	uint16_t user_index;
	uint8_t is_emergency_finger;
	if (matched_emergency_fingerprint != NULL) *matched_emergency_fingerprint = 0U;
	if (fingerprint_template_id < MAX_USERS)
	{
		user_index = fingerprint_template_id;
		is_emergency_finger = 0U;
	}
	else if (fingerprint_template_id >= FINGER_EMERGENCY_OFFSET && fingerprint_template_id < FINGER_EMERGENCY_OFFSET + MAX_USERS)
	{
		user_index = fingerprint_template_id - FINGER_EMERGENCY_OFFSET;
		is_emergency_finger = 1U;
	}
	else return -1;
	if (!UserDB_CanAuthenticate((uint8_t) user_index)) return -1;
	if (!is_emergency_finger && USER_RECORDS[user_index].finger_id != fingerprint_template_id) return -1;
	if (is_emergency_finger && USER_RECORDS[user_index].duress_finger_id != fingerprint_template_id) return -1;
	if (matched_emergency_fingerprint != NULL) *matched_emergency_fingerprint = is_emergency_finger;
	return (int16_t) user_index;
}
// �?ếm lại tổng số user khác trạng thái EMPTY trong cơ sở dữ liệu
void UserDB_Recount(void)
{
	existing_user_count = 0;
	for (uint8_t i = 0; i < MAX_USERS; i++)
	{
		if (USER_RECORDS[i].status != USER_STATUS_EMPTY) existing_user_count++;
	}
}
// Gửi ACK kết quả xử lý kèm command_id tương ứng đến ESP32
void UserDB_SendAck(const char *command_id, const char *ack_result)
{
	char ack_message[64];
	snprintf(ack_message, sizeof(ack_message), "USER_ACK|%s|%s", command_id, ack_result);
	ESP_SendLine("", ack_message);
}
// Hiển thị ngay tên, trạng thái vào ra, tầng và phòng của user vừa xác thực
void Access_ShowUserInfo(uint8_t user_index, char *access_action_text)
{
	char line1[17];
	char line2[17];
	snprintf(line1, sizeof(line1), "%.*s %s", (int) USER_NAME_MAX_LENGTH, USER_RECORDS[user_index].name, access_action_text);
	snprintf(line2, sizeof(line2), "TANG %u P.%u", USER_RECORDS[user_index].floor, USER_RECORDS[user_index].room);
	LCD_ShowMessage(line1, line2, LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
}
// Hiển thị giờ và ngày của lần truy cập sau màn hình thông tin user
void Access_ShowTimeDate(void)
{
	char line1[17];
	char line2[17];
	snprintf(line1, sizeof(line1), "TIME:%s", detected_passage_time);
	snprintf(line2, sizeof(line2), "DATE:%s", detected_passage_date);
	LCD_ShowMessage(line1, line2, LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
}
// Cap nhat trang thai theo huong di, luu log va khoi dong hien thi non-blocking
void AccessLog_Save(uint8_t user_index, AccessDirection_t access_direction)
{
	char *access_action_text;
	char floor_text[8];
	char room_text[8];
	uint8_t access_log_write_index = next_log_write_index;
	USER_RECORDS[user_index].access_state = access_direction == ACCESS_DIRECTION_ENTERING ? ACCESS_DIRECTION_ENTERING : ACCESS_DIRECTION_EXITING;
	access_action_text = access_direction == ACCESS_DIRECTION_ENTERING ? "VAO" : "RA";
	Access_ShowUserInfo(user_index, access_action_text);
	access_info_display_phase = ACCESS_INFO_DISPLAY_USER_INFO;
	access_info_display_start_tick = HAL_GetTick();
	strcpy(ACCESS_LOG_RECORDS[access_log_write_index].name, USER_RECORDS[user_index].name);
	strcpy(ACCESS_LOG_RECORDS[access_log_write_index].action, access_action_text);
	strcpy(ACCESS_LOG_RECORDS[access_log_write_index].time, detected_passage_time);
	strcpy(ACCESS_LOG_RECORDS[access_log_write_index].date, detected_passage_date);
	ACCESS_LOG_RECORDS[access_log_write_index].floor = USER_RECORDS[user_index].floor;
	ACCESS_LOG_RECORDS[access_log_write_index].room = USER_RECORDS[user_index].room;
	if (access_log_count < MAX_ACCESS_LOG) access_log_count++;
	next_log_write_index++;
	if (next_log_write_index >= MAX_ACCESS_LOG) next_log_write_index = 0U;
	if (!Storage_SaveAccessEvent(user_index, &USER_RECORDS[user_index], access_log_write_index, &ACCESS_LOG_RECORDS[access_log_write_index], access_log_count, next_log_write_index))
	{
		myConfig.access_log_count = access_log_count;
		myConfig.access_log_index = next_log_write_index;
		if (!SaveDataToAT24C256((uint8_t*) &myConfig, sizeof(myConfig))) ESP_SendLine("STORAGE:", "ACCESS_WRITE_ERROR");
		else ESP_SendLine("STORAGE:", "ACCESS_RECOVERED");
	}
	ESP_SendLine("LOG:", USER_RECORDS[user_index].name);
	ESP_SendLine("ACTION:", access_action_text);
	snprintf(floor_text, sizeof(floor_text), "%u", USER_RECORDS[user_index].floor);
	snprintf(room_text, sizeof(room_text), "%u", USER_RECORDS[user_index].room);
	ESP_SendLine("FLOOR:", floor_text);
	ESP_SendLine("ROOM:", room_text);
	ESP_SendLine("TIME:", detected_passage_time);
	ESP_SendLine("DATE:", detected_passage_date);
	ESP_SendLine("CLOUD:", "PUSH");
}
// Theo doi toan bo phien di qua cua bang thu tu chan va nha cam bien A, B
void Passage_Process(void)
{
	uint8_t is_sensor_a_blocked;
	uint8_t is_sensor_b_blocked;
	if (!is_passage_tracking_active || current_system_state != STATE_DOOR_OPERATION) return;
	is_sensor_a_blocked = HAL_GPIO_ReadPin(TRANSCEIVER_A_GPIO_Port, TRANSCEIVER_A_Pin) == GPIO_PIN_RESET;
	is_sensor_b_blocked = HAL_GPIO_ReadPin(TRANSCEIVER_B_GPIO_Port, TRANSCEIVER_B_Pin) == GPIO_PIN_RESET;
	switch (current_passage_state)
	{
	case PASSAGE_WAIT_FIRST_SENSOR:
		if (is_sensor_a_blocked && is_sensor_b_blocked)
		{
			is_passage_direction_result_ready = 0U;
			current_passage_state = PASSAGE_WAIT_BOTH_CLEAR;
		}
		else if (is_sensor_a_blocked)
		{
			is_passage_direction_result_ready = 0U;
			current_passage_state = PASSAGE_A_BLOCKED_FIRST;
		}
		else if (is_sensor_b_blocked)
		{
			is_passage_direction_result_ready = 0U;
			current_passage_state = PASSAGE_B_BLOCKED_FIRST;
		}
		break;
	case PASSAGE_A_BLOCKED_FIRST:
		if (is_sensor_a_blocked && is_sensor_b_blocked) current_passage_state = PASSAGE_WAIT_BOTH_CLEAR;
		else if (!is_sensor_a_blocked && is_sensor_b_blocked)
		{
			current_passage_state = PASSAGE_B_BLOCKED_AFTER_A;
		}
		else if (!is_sensor_a_blocked)
		{
			current_passage_state = PASSAGE_WAIT_SENSOR_B_AFTER_A;
		}
		break;
	case PASSAGE_WAIT_SENSOR_B_AFTER_A:
		if (is_sensor_a_blocked && is_sensor_b_blocked) current_passage_state = PASSAGE_WAIT_BOTH_CLEAR;
		else if (is_sensor_a_blocked) current_passage_state = PASSAGE_A_BLOCKED_FIRST;
		else if (is_sensor_b_blocked)
		{
			current_passage_state = PASSAGE_B_BLOCKED_AFTER_A;
		}
		break;
	case PASSAGE_B_BLOCKED_AFTER_A:
		if (is_sensor_a_blocked) current_passage_state = PASSAGE_WAIT_BOTH_CLEAR;
		else if (!is_sensor_b_blocked)
		{
			if (passage_direction == PASSAGE_DIRECTION_NONE)
			{
				passage_direction = PASSAGE_DIRECTION_ENTERING;
				strcpy(detected_passage_time, current_time);
				strcpy(detected_passage_date, current_date);
			}
			else if (passage_direction == PASSAGE_DIRECTION_EXITING)
			{
				passage_direction = PASSAGE_DIRECTION_NONE;
				strcpy(detected_passage_time, "00:00:00");
				strcpy(detected_passage_date, "00/00/0000");
				LCD_ShowMessage("KHONG LUU LOG", "USER DA DI VAO", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
			}
			is_passage_direction_result_ready = 1U;
			current_passage_state = PASSAGE_WAIT_FIRST_SENSOR;
		}
		break;
	case PASSAGE_B_BLOCKED_FIRST:
		if (is_sensor_a_blocked && is_sensor_b_blocked) current_passage_state = PASSAGE_WAIT_BOTH_CLEAR;
		else if (is_sensor_a_blocked && !is_sensor_b_blocked)
		{
			current_passage_state = PASSAGE_A_BLOCKED_AFTER_B;
		}
		else if (!is_sensor_b_blocked)
		{
			current_passage_state = PASSAGE_WAIT_SENSOR_A_AFTER_B;
		}
		break;
	case PASSAGE_WAIT_SENSOR_A_AFTER_B:
		if (is_sensor_a_blocked && is_sensor_b_blocked) current_passage_state = PASSAGE_WAIT_BOTH_CLEAR;
		else if (is_sensor_b_blocked) current_passage_state = PASSAGE_B_BLOCKED_FIRST;
		else if (is_sensor_a_blocked)
		{
			current_passage_state = PASSAGE_A_BLOCKED_AFTER_B;
		}
		break;
	case PASSAGE_A_BLOCKED_AFTER_B:
		if (is_sensor_b_blocked) current_passage_state = PASSAGE_WAIT_BOTH_CLEAR;
		else if (!is_sensor_a_blocked)
		{
			if (passage_direction == PASSAGE_DIRECTION_NONE)
			{
				passage_direction = PASSAGE_DIRECTION_EXITING;
				strcpy(detected_passage_time, current_time);
				strcpy(detected_passage_date, current_date);
			}
			else if (passage_direction == PASSAGE_DIRECTION_ENTERING)
			{
				passage_direction = PASSAGE_DIRECTION_NONE;
				strcpy(detected_passage_time, "00:00:00");
				strcpy(detected_passage_date, "00/00/0000");
				LCD_ShowMessage("KHONG LUU LOG", "USER DA DI RA", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
			}
			is_passage_direction_result_ready = 1U;
			current_passage_state = PASSAGE_WAIT_FIRST_SENSOR;
		}
		break;
	case PASSAGE_WAIT_BOTH_CLEAR:
		if (!is_sensor_a_blocked && !is_sensor_b_blocked)
		{
			current_passage_state = PASSAGE_WAIT_FIRST_SENSOR;
		}
		break;
	default:
		current_passage_state = PASSAGE_WAIT_BOTH_CLEAR;
		break;
	}
}
// Hien thi log that, sau do gio ngay va tro ve man hinh khoa
void AccessDisplay_Process(void)
{
	if (access_info_display_phase == ACCESS_INFO_DISPLAY_INACTIVE) return;
	if (current_system_state != STATE_DOOR_OPERATION)
	{
		access_info_display_phase = ACCESS_INFO_DISPLAY_INACTIVE;
		access_info_display_start_tick = 0U;
		return;
	}
	if (HAL_GetTick() - access_info_display_start_tick < ACCESS_DISPLAY_DURATION_MS) return;
	if (access_info_display_phase == ACCESS_INFO_DISPLAY_USER_INFO)
	{
		Access_ShowTimeDate();
		access_info_display_phase = ACCESS_INFO_DISPLAY_TIME_DATE;
		access_info_display_start_tick = HAL_GetTick();
		return;
	}
	HomeScreen();
}
// Hiển thị một bản ghi nhật ký đã lưu trên LCD và gửi thông tin sang ESP32
void AccessLog_Show(uint8_t log_index)
{
	char line1[17];
	char line2[17];
	snprintf(line1, sizeof(line1), "%.*s %s", (int) USER_NAME_MAX_LENGTH, ACCESS_LOG_RECORDS[log_index].name, ACCESS_LOG_RECORDS[log_index].action);
	snprintf(line2, sizeof(line2), "TANG %u P.%u", ACCESS_LOG_RECORDS[log_index].floor, ACCESS_LOG_RECORDS[log_index].room);
	LCD_ShowMessage(line1, line2, LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 2000);
	snprintf(line1, sizeof(line1), "TIME:%s", ACCESS_LOG_RECORDS[log_index].time);
	snprintf(line2, sizeof(line2), "DATE:%s", ACCESS_LOG_RECORDS[log_index].date);
	LCD_ShowMessage(line1, line2, LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 2000);
	ESP_SendLine("LOG:", ACCESS_LOG_RECORDS[log_index].name);
	ESP_SendLine("ACTION:", ACCESS_LOG_RECORDS[log_index].action);
	ESP_SendLine("TIME:", ACCESS_LOG_RECORDS[log_index].time);
	ESP_SendLine("DATE:", ACCESS_LOG_RECORDS[log_index].date);
}
// Gửi toàn bộ nhật ký đang lưu sang ESP32 giữa LOG START và LOG END
void ESP_SendAccessLogs(void)
{
	char access_log_tx_buffer[96];
	ESP_SendLine("", "LOG_START");
	if (access_log_count == 0)
	{
		ESP_SendLine("LOGVIEW:", "CHUA CO NHAT KY");
		ESP_SendLine("", "LOG_END");
		return;
	}
	uint8_t log_index;
	for (uint8_t i = 0; i < access_log_count; i++)
	{
		if (next_log_write_index == 0) log_index = access_log_count - 1 - i;
		else
		{
			if (next_log_write_index > i) log_index = next_log_write_index - 1 - i;
			else log_index = MAX_ACCESS_LOG + next_log_write_index - 1 - i;
		}
		snprintf(access_log_tx_buffer, sizeof(access_log_tx_buffer), "%02u|%s|%s|%u|%u|%s|%s", i + 1,
		ACCESS_LOG_RECORDS[log_index].name,
		ACCESS_LOG_RECORDS[log_index].action,
		ACCESS_LOG_RECORDS[log_index].floor,
		ACCESS_LOG_RECORDS[log_index].room,
		ACCESS_LOG_RECORDS[log_index].time,
		ACCESS_LOG_RECORDS[log_index].date);
		ESP_SendLine("LOGVIEW:", access_log_tx_buffer);
	}
	ESP_SendLine("", "LOG_END");
}
// Xóa bộ đệm và số lượng ký tự của lần nhập hiện tại
void Auth_ResetInput(void)
{
	input_length = 0;
	memset(input_buffer, 0, sizeof(input_buffer));
}
// Them mot chu so vao bo dem va cap nhat kieu hien thi
uint8_t Auth_InputDigit(char key, uint8_t max_length, uint8_t mask_input)
{
	if (key < '0' || key > '9') return 0U;
	if (input_length >= max_length) return 0U;
	input_buffer[input_length++] = key;
	input_buffer[input_length] = '\0';
	if (mask_input) LCD_ShowPasswordMask6();
	else LCD_ShowCenteredInput(input_buffer);
	return 1U;
}
// Sao lưu dữ liệu thẻ hiện tại trước khi bắt đầu chỉnh sửa
void Auth_BeginCardEditSession(void)
{
	user_card_backup = myConfig.user_cards[credential_management_user_index];
	is_card_edit_active = 1;
}
// Khôi phục dữ liệu thẻ đã sao lưu khi quá trình chỉnh sửa bị hủy
void Auth_CancelCardEditSession(void)
{
	if (!is_card_edit_active) return;
	myConfig.user_cards[credential_management_user_index] = user_card_backup;
	is_card_edit_active = 0;
}
// Xóa toàn bộ dữ liệu của phiên xác thực đa yếu tố hiện tại
void AccessAuth_Reset(void)
{
	access_auth_target = ACCESS_AUTH_TARGET_NONE;
	is_access_auth_active = 0;
	is_access_auth_linked_to_user = 0;
	access_auth_user_index = 0;
	verified_auth_method_mask = 0;
	is_emergency_auth_method_used = 0;
	Auth_ResetInput();
}
// �?ếm số nhóm yếu tố khác nhau đã được xác thực trong mặt nạ hiện tại
uint8_t AccessAuth_CountMethods(uint8_t verified_method_mask)
{
	uint8_t verified_method_count = 0;
	if (verified_method_mask & AUTH_METHOD_CARD_MASK) verified_method_count++;
	if (verified_method_mask & AUTH_METHOD_FINGER_MASK) verified_method_count++;
	if (verified_method_mask & AUTH_METHOD_PASSWORD_MASK) verified_method_count++;
	return verified_method_count;
}
// Khởi tạo một phiên xác thực mới cho mở cửa hoặc truy cập menu admin
void AccessAuth_Start(AccessAuthTarget_t new_auth_target)
{
	AccessAuth_Reset();
	access_auth_target = new_auth_target;
	is_access_auth_active = 1;
	if (new_auth_target == ACCESS_AUTH_TARGET_ADMIN_MENU)
	{
		current_system_state = STATE_ADMIN_AUTHENTICATION;
		LCD_ShowMessage("CHE DO ADMIN", "PASS/THE/VT", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
	}
}
// Hiển thị tiến độ và yếu tố còn có thể sử dụng trong phiên xác thực
void AccessAuth_ShowProgress(void)
{
	char line1[17];
	char line2[17];
	uint8_t verified_method_count;
	if (!is_access_auth_active) return;
	verified_method_count = AccessAuth_CountMethods(verified_auth_method_mask);
	if (access_auth_target == ACCESS_AUTH_TARGET_ADMIN_MENU)
	{
		snprintf(line1, sizeof(line1), "ADMIN %u/3", verified_method_count);
		if (verified_auth_method_mask == AUTH_METHOD_PASSWORD_MASK) strcpy(line2, "QUET THE VA VT");
		else if (verified_auth_method_mask == AUTH_METHOD_CARD_MASK) strcpy(line2, "PASS VA VT");
		else if (verified_auth_method_mask == AUTH_METHOD_FINGER_MASK) strcpy(line2, "PASS VA THE");
		else if (verified_auth_method_mask == (AUTH_METHOD_PASSWORD_MASK | AUTH_METHOD_CARD_MASK)) strcpy(line2, "MOI QUET VT");
		else if (verified_auth_method_mask == (AUTH_METHOD_PASSWORD_MASK | AUTH_METHOD_FINGER_MASK)) strcpy(line2, "MOI QUET THE");
		else if (verified_auth_method_mask == (AUTH_METHOD_CARD_MASK | AUTH_METHOD_FINGER_MASK)) strcpy(line2, "NHAP PASS ADMIN");
		else strcpy(line2, "PASS/THE/VT");
		LCD_ShowMessage(line1, line2, LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
		return;
	}
	snprintf(line1, sizeof(line1), "%.*s 1/2", (int) USER_NAME_MAX_LENGTH, USER_RECORDS[access_auth_user_index].name);
	if (verified_auth_method_mask & AUTH_METHOD_CARD_MASK) strcpy(line2, "PASS HOAC VT");
	else if (verified_auth_method_mask & AUTH_METHOD_FINGER_MASK) strcpy(line2, "PASS HOAC THE");
	else strcpy(line2, "QUET THE HOAC VT");
	LCD_ShowMessage(line1, line2, LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
}
// Gắn phiên xác thực với đúng user và chỉ cho user admin truy cập chức năng quản trị
uint8_t AccessAuth_LinkUser(uint8_t user_index)
{
	if (!UserDB_CanAuthenticate(user_index)) return 0;
	if (access_auth_target == ACCESS_AUTH_TARGET_ADMIN_MENU && user_index != ADMIN_USER_INDEX)
	{
		LCD_ShowMessage("KHONG CO QUYEN", "TRUY CAP ADMIN", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		HomeScreen();
		return 0;
	}
	if (!is_access_auth_linked_to_user)
	{
		access_auth_user_index = user_index;
		is_access_auth_linked_to_user = 1;
		return 1;
	}
	if (access_auth_user_index == user_index) return 1;
	LCD_ShowMessage("KHONG CUNG USER", "XAC THUC LAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
	HomeScreen();
	return 0;
}
// Ghi nhận một yếu tố hợp lệ, kiểm tra cùng user và hoàn tất khi đã đủ yêu cầu
void AccessAuth_RecordVerifiedMethod(uint8_t user_index, uint8_t auth_method_mask_to_add, uint8_t is_verified_method_emergency)
{
	if (!is_access_auth_active) AccessAuth_Start(ACCESS_AUTH_TARGET_DOOR_ACCESS);
	if (!AccessAuth_LinkUser(user_index)) return;
	if (access_auth_target == ACCESS_AUTH_TARGET_DOOR_ACCESS && is_verified_method_emergency) is_emergency_auth_method_used = 1;
	if (verified_auth_method_mask & auth_method_mask_to_add)
	{
		Auth_ResetInput();
		LCD_ShowMessage("LOI:TRUNG PHUONG", "THUC XAC THUC", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		AccessAuth_ShowProgress();
		return;
	}
	verified_auth_method_mask |= auth_method_mask_to_add;
	Auth_ResetInput();
	if (access_auth_target == ACCESS_AUTH_TARGET_DOOR_ACCESS && AccessAuth_CountMethods(verified_auth_method_mask) >= DOOR_REQUIRED_AUTH_METHOD_COUNT)
	{
		AccessAuth_Complete();
		return;
	}
	if (access_auth_target == ACCESS_AUTH_TARGET_ADMIN_MENU && verified_auth_method_mask == ALL_AUTH_METHODS_MASK)
	{
		AccessAuth_Complete();
		return;
	}
	AccessAuth_ShowProgress();
}
// Kiem tra mat khau theo dung loai phien roi ghi nhan yeu to kien thuc
void AccessAuth_ProcessPassword(const char *password)
{
	int16_t user_index;
	uint8_t is_emergency_method = 0U;
	if (password == NULL) return;
	if (access_auth_target == ACCESS_AUTH_TARGET_ADMIN_MENU)
	{
		if (strcmp(password, myConfig.admin_password) != 0)
		{
			LCD_ShowMessage("LOI:NHAP SAI", "MAT KHAU ADMIN", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
			HomeScreen();
			return;
		}
		if (verified_auth_method_mask & AUTH_METHOD_PASSWORD_MASK)
		{
			Auth_ResetInput();
			LCD_ShowMessage("LOI:TRUNG PHUONG", "THUC XAC THUC", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
			AccessAuth_ShowProgress();
			return;
		}
		verified_auth_method_mask |= AUTH_METHOD_PASSWORD_MASK;
		Auth_ResetInput();
		if (verified_auth_method_mask == ALL_AUTH_METHODS_MASK) AccessAuth_Complete();
		else AccessAuth_ShowProgress();
		return;
	}
	user_index = FindUserByAnyPassword(password, &is_emergency_method);
	if (user_index >= 0)
	{
		AccessAuth_RecordVerifiedMethod((uint8_t) user_index, AUTH_METHOD_PASSWORD_MASK, is_emergency_method);
		return;
	}
	LCD_ShowMessage("LOI:NHAP SAI", "MAT KHAU", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
	HomeScreen();
}
// Kiem tra the theo dung loai phien roi ghi nhan yeu to so huu
void AccessAuth_ProcessCard(RFID_UID_t *card)
{
	int16_t user_index;
	uint8_t is_emergency_method = 0U;
	if (card == NULL) return;
	if (access_auth_target == ACCESS_AUTH_TARGET_ADMIN_MENU)
	{
		user_index = Auth_FindCardOwner(card, 0U, &is_emergency_method);
		if (user_index >= 0)
		{
			AccessAuth_RecordVerifiedMethod((uint8_t) user_index, AUTH_METHOD_CARD_MASK, 0U);
			return;
		}
		LCD_ShowMessage("THE ADMIN KHONG", "HOP LE", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		HomeScreen();
		return;
	}
	user_index = Auth_FindCardOwner(card, 1U, &is_emergency_method);
	if (user_index >= 0)
	{
		AccessAuth_RecordVerifiedMethod((uint8_t) user_index, AUTH_METHOD_CARD_MASK, is_emergency_method);
		return;
	}
	LCD_ShowMessage("LOI:THE KHONG", "HOP LE", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
	HomeScreen();
}
// Cấp quy�?n mở cửa hoặc truy cập menu admin sau khi đủ số yếu tố yêu cầu
void AccessAuth_Complete(void)
{
	uint8_t user_index;
	uint8_t was_emergency_auth_method_used;
	AccessAuthTarget_t completed_auth_target;
	if (!is_access_auth_active || !is_access_auth_linked_to_user) return;
	completed_auth_target = access_auth_target;
	user_index = access_auth_user_index;
	was_emergency_auth_method_used = is_emergency_auth_method_used;
	AccessAuth_Reset();
	if (completed_auth_target == ACCESS_AUTH_TARGET_ADMIN_MENU)
	{
		if (user_index != ADMIN_USER_INDEX)
		{
			LCD_ShowMessage("KHONG CO QUYEN", "TRUY CAP ADMIN", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
			HomeScreen();
			return;
		}
		Admin_ShowSystemMenu();
		return;
	}
	is_passage_tracking_active = 1U;
	tracked_passage_user_index = user_index;
	current_passage_state = PASSAGE_WAIT_FIRST_SENSOR;
	passage_direction = PASSAGE_DIRECTION_NONE;
	is_passage_direction_result_ready = 0U;
	strcpy(detected_passage_time, "00:00:00");
	strcpy(detected_passage_date, "00/00/0000");
	Door_Open_Auto();
	current_system_state = STATE_DOOR_OPERATION;
	LCD_ShowMessage("XAC THUC OK", "CUA DANG MO", LCD_ALIGNMENT_CENTER, LCD_BEEP_NONE, 0);
	if (was_emergency_auth_method_used) ESP_SendLine("EVENT:", "EMERGENCY");
}
// Chuyển sang bước nhập USER_ID và xóa dữ liệu nhập trước đó
void Auth_ShowUserIdInput(void)
{
	current_system_state = STATE_CREDENTIAL_MANAGEMENT_USER_ID_INPUT;
	Auth_ResetInput();
	LCD_ShowMessage("NHAP USER ID", "", LCD_ALIGNMENT_CENTER, LCD_BEEP_NONE, 0);
	LCD_ShowCenteredInput("");
}
// Hiển thị tên user kèm dấu h�?i để ngư�?i dùng xác nhận đúng tài khoản
void Auth_ShowConfirmUser(void)
{
	char line2[17];
	current_system_state = STATE_CREDENTIAL_MANAGEMENT_USER_CONFIRMATION;
	snprintf(line2, sizeof(line2), "%s?", USER_RECORDS[credential_management_user_index].name);
	LCD_ShowMessage("XAC NHAN TEN", line2, LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
}
// Chuyển sang bước nhập mã xác minh được cấp từ hệ thống quản lý
void Auth_ShowActivationCode(void)
{
	current_system_state = STATE_ACTIVATION_CODE_INPUT;
	Auth_ResetInput();
	LCD_ShowMessage("NHAP MA XAC MINH", "", LCD_ALIGNMENT_CENTER, LCD_BEEP_NONE, 0);
	LCD_ShowCenteredInput("");
}
// Yêu cầu user nhập mật khẩu hiện tại trước khi thay đổi mật khẩu
void Auth_ShowCurrentUserPasswordInput(void)
{
	current_system_state = STATE_CURRENT_USER_PASSWORD_INPUT;
	Auth_ResetInput();
	LCD_ShowMessage("NHAP PASS USER", "", LCD_ALIGNMENT_CENTER, LCD_BEEP_NONE, 0);
	LCD_ShowCenteredInput("");
}
// Hien thi buoc tao hoac doi mat khau thuong
void Auth_ShowNewNormalPasswordInput(void)
{
	current_system_state = STATE_NEW_NORMAL_PASSWORD_INPUT;
	Auth_ResetInput();
	if (USER_RECORDS[credential_management_user_index].status == USER_STATUS_ACTIVE)
	{
		LCD_ShowMessage("PASS THUONG MOI", "", LCD_ALIGNMENT_CENTER, LCD_BEEP_NONE, 0);
	}
	else LCD_ShowMessage("TAO PASS THUONG", "", LCD_ALIGNMENT_CENTER, LCD_BEEP_NONE, 0);
	LCD_ShowCenteredInput("");
}
// Hien thi buoc tao hoac doi mat khau khan cap
void Auth_ShowNewEmergencyPasswordInput(void)
{
	current_system_state = STATE_NEW_EMERGENCY_PASSWORD_INPUT;
	Auth_ResetInput();
	if (USER_RECORDS[credential_management_user_index].status == USER_STATUS_ACTIVE)
	{
		LCD_ShowMessage("PASS KHANCAP MOI", "", LCD_ALIGNMENT_CENTER, LCD_BEEP_NONE, 0);
	}
	else LCD_ShowMessage("TAO PASS KHANCAP", "", LCD_ALIGNMENT_CENTER, LCD_BEEP_NONE, 0);
	LCD_ShowCenteredInput("");
}
// Hien thi va kiem tra van tay thuong cua user
void Auth_ShowNormalFinger(void)
{
	char line2[17];
	uint8_t template_exists;
	current_system_state = STATE_NORMAL_FINGERPRINT_MANAGEMENT;
	Auth_ResetInput();
	expected_fingerprint_template_id = USER_RECORDS[credential_management_user_index].finger_id;
	if (!Finger_CheckTemplate(expected_fingerprint_template_id, &template_exists))
	{
		LCD_ShowMessage("LOI KET NOI", "CAM BIEN VAN TAY", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		Auth_ShowUserManageMenu();
		return;
	}
	is_normal_fingerprint_registered = template_exists;
	if (template_exists)
	{
		LCD_ShowMessage("DA CO VT THUONG", "*:BACK    #:NEXT", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
		return;
	}
	snprintf(line2, sizeof(line2), "NHAP ID:%u", expected_fingerprint_template_id);
	LCD_ShowMessage("VAN TAY THUONG", line2, LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
}
// Hien thi va kiem tra van tay khan cap cua user
void Auth_ShowEmergencyFinger(void)
{
	char line2[17];
	uint8_t template_exists;
	current_system_state = STATE_EMERGENCY_FINGERPRINT_MANAGEMENT;
	Auth_ResetInput();
	expected_fingerprint_template_id = USER_RECORDS[credential_management_user_index].duress_finger_id;
	if (!Finger_CheckTemplate(expected_fingerprint_template_id, &template_exists))
	{
		LCD_ShowMessage("LOI KET NOI", "CAM BIEN VAN TAY", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		Auth_ShowUserManageMenu();
		return;
	}
	if (template_exists)
	{
		LCD_ShowMessage("CO VT KHAN CAP", "*:BACK     #:END", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
		return;
	}
	snprintf(line2, sizeof(line2), "NHAP ID:%u", expected_fingerprint_template_id);
	LCD_ShowMessage("VAN TAY KHAN CAP", line2, LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
}
// Hiển thị bước thêm hoặc b�? qua thẻ RFID thư�?ng
void Auth_ShowNormalCard(void)
{
	current_system_state = STATE_NORMAL_CARD_MANAGEMENT;
	Auth_ResetInput();
	if (myConfig.user_cards[credential_management_user_index].normal_card_exists == 1)
	{
		LCD_ShowMessage("DA CO THE THUONG", "*:BACK    #:NEXT", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
		return;
	}
	LCD_ShowMessage("THEM THE THUONG", "QUET THE", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
}
// Hiển thị bước thêm hoặc b�? qua thẻ RFID khẩn cấp
void Auth_ShowEmergencyCard(void)
{
	current_system_state = STATE_EMERGENCY_CARD_MANAGEMENT;
	Auth_ResetInput();
	if (myConfig.user_cards[credential_management_user_index].emergency_card_exists == 1)
	{
		LCD_ShowMessage("CO THE KHAN CAP", "*:BACK     #:END", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
		return;
	}
	LCD_ShowMessage("THEM THE KHAN", "CAP,QUET THE", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
}
// Hien thi menu them hoac xoa phuong thuc dang duoc quan ly
void Auth_ShowUserManageMenu(void)
{
	Auth_ResetInput();
	current_system_state = STATE_USER_AUTH_METHOD_MENU;
	if (current_credential_management_flow == CREDENTIAL_MANAGEMENT_FLOW_CARD)
	{
		LCD_ShowMessage("1:THEM THE", "2:XOA THE", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
		return;
	}
	if (current_credential_management_flow == CREDENTIAL_MANAGEMENT_FLOW_FINGERPRINT)
	{
		LCD_ShowMessage("1:THEM VAN TAY", "2:XOA VAN TAY", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
		return;
	}
	HomeScreen();
}
// Hien thi lua chon xoa phuong thuc thuong, khan cap hoac ca hai
void Auth_ShowUserDeleteMenu(void)
{
	current_system_state = STATE_AUTH_METHOD_DELETE_MENU;
	Auth_ResetInput();
	selected_auth_delete_option = AUTH_DELETE_NONE;
	LCD_ShowMessage("1:THUONG 2:KHAN", "CAP 3:CA HAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
}
// Hien thi xac nhan xoa theo phuong thuc dang quan ly
void Auth_ShowUserDeleteConfirm(AuthDeleteOption_t delete_option)
{
	selected_auth_delete_option = delete_option;
	current_system_state = STATE_AUTH_METHOD_DELETE_CONFIRM;
	Auth_ResetInput();
	if (current_credential_management_flow == CREDENTIAL_MANAGEMENT_FLOW_CARD)
	{
		if (delete_option == AUTH_DELETE_NORMAL)
		{
			LCD_ShowMessage("XOA THE THUONG?", "*:NO       #:YES", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
		}
		else if (delete_option == AUTH_DELETE_EMERGENCY)
		{
			LCD_ShowMessage("XOA THE KHANCAP?", "*:NO       #:YES", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
		}
		else LCD_ShowMessage("XOA CA HAI THE?", "*:NO       #:YES", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
		return;
	}
	if (current_credential_management_flow == CREDENTIAL_MANAGEMENT_FLOW_FINGERPRINT)
	{
		if (delete_option == AUTH_DELETE_NORMAL)
		{
			LCD_ShowMessage("XOA VT THUONG?", "*:NO       #:YES", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
		}
		else if (delete_option == AUTH_DELETE_EMERGENCY)
		{
			LCD_ShowMessage("XOA VT KHAN CAP?", "*:NO       #:YES", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
		}
		else LCD_ShowMessage("XOA CA HAI VT?", "*:NO       #:YES", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
		return;
	}
	HomeScreen();
}
// Hien thi xac nhan ket thuc va luu thay doi
void Auth_ShowCardSaveConfirm(void)
{
	current_system_state = STATE_CARD_CHANGES_SAVE_CONFIRM;
	Auth_ResetInput();
	LCD_ShowMessage("KET THUC QTRINH", "*:BACK     #:YES", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
}
// Khởi tạo quy trình quản lý vân tay và bắt đầu từ bước nhập USER_ID
void Auth_StartFingerFlow(void)
{
	if (!is_finger_sensor_ready)
	{
		LCD_ShowMessage("LOI KET NOI", "CAM BIEN VAN TAY", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		HomeScreen();
		return;
	}
	current_credential_management_flow = CREDENTIAL_MANAGEMENT_FLOW_FINGERPRINT;
	is_normal_fingerprint_registered = 0;
	credential_management_user_index = 0;
	Auth_ShowUserIdInput();
}
// Khởi tạo quy trình quản lý thẻ RFID và bắt đầu từ bước nhập USER_ID
void Auth_StartCardFlow(void)
{
	current_credential_management_flow = CREDENTIAL_MANAGEMENT_FLOW_CARD;
	credential_management_user_index = 0;
	Auth_ShowUserIdInput();
}
// Khởi tạo quy trình đổi mật khẩu user và bắt đầu từ bước nhập USER_ID
void Auth_StartPasswordFlow(void)
{
	current_credential_management_flow = CREDENTIAL_MANAGEMENT_FLOW_PASSWORD;
	credential_management_user_index = 0;
	memset(pending_normal_password, 0, sizeof(pending_normal_password));
	Auth_ShowUserIdInput();
}
// Kiem tra va dang ky ID van tay dang nhap
void Auth_ValidateTemplateIdAndEnrollFingerprint(uint8_t is_emergency_fingerprint)
{
	char expected_template_id_text[17];
	uint16_t entered_template_id;
	if (input_length == 0U)
	{
		LCD_ShowMessage("LOI:CHUA NHAP", "ID VAN TAY", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		if (is_emergency_fingerprint) Auth_ShowEmergencyFinger();
		else Auth_ShowNormalFinger();
		return;
	}
	entered_template_id = (uint16_t) atoi(input_buffer);
	if (entered_template_id != expected_fingerprint_template_id)
	{
		snprintf(expected_template_id_text, sizeof(expected_template_id_text), "ID DUNG:%u", expected_fingerprint_template_id);
		LCD_ShowMessage("NHAP SAI ID", expected_template_id_text, LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		if (is_emergency_fingerprint) Auth_ShowEmergencyFinger();
		else Auth_ShowNormalFinger();
		return;
	}
	if (!Auth_EnrollFinger(entered_template_id))
	{
		if (!is_finger_sensor_ready)
		{
			Auth_ShowUserManageMenu();
			return;
		}
		if (is_emergency_fingerprint) Auth_ShowEmergencyFinger();
		else Auth_ShowNormalFinger();
		return;
	}
	if (is_emergency_fingerprint)
	{
		Auth_ShowUserManageMenu();
		return;
	}
	is_normal_fingerprint_registered = 1U;
	Auth_ShowEmergencyFinger();
}
// Luu the vua quet vao dung vi tri thuong hoac khan cap
void Auth_EnrollCard(RFID_UID_t *card_uid, uint8_t is_emergency_card)
{
	if (card_uid == NULL) return;
	if (Auth_CardAlreadyExists(card_uid))
	{
		LCD_ShowMessage("LOI:THE NAY", "DA TON TAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		if (is_emergency_card) Auth_ShowEmergencyCard();
		else Auth_ShowNormalCard();
		return;
	}
	if (is_emergency_card)
	{
		memcpy(&myConfig.user_cards[credential_management_user_index].emergency_card, card_uid, sizeof(RFID_UID_t));
		myConfig.user_cards[credential_management_user_index].emergency_card_exists = 1U;
		LCD_ShowMessage("THEM THE KHAN", "THANH CONG", LCD_ALIGNMENT_LEFT, LCD_BEEP_ONCE, 1500);
		Auth_ShowCardSaveConfirm();
		return;
	}
	memcpy(&myConfig.user_cards[credential_management_user_index].normal_card, card_uid, sizeof(RFID_UID_t));
	myConfig.user_cards[credential_management_user_index].normal_card_exists = 1U;
	LCD_ShowMessage("THEM THE THUONG", "THANH CONG", LCD_ALIGNMENT_LEFT, LCD_BEEP_ONCE, 1500);
	Auth_ShowEmergencyCard();
}
// Tim chu so huu the thuong hoac khan cap trong mot lan quet
int16_t Auth_FindCardOwner(RFID_UID_t *card_uid, uint8_t include_emergency_card, uint8_t *matched_emergency_card)
{
	if (matched_emergency_card != NULL) *matched_emergency_card = 0U;
	if (card_uid == NULL) return -1;
	for (uint16_t i = 0U; i < MAX_USERS; i++)
	{
		if (!UserDB_CanAuthenticate((uint8_t) i)) continue;
		if (include_emergency_card && myConfig.user_cards[i].emergency_card_exists == 1U && MFRC522_CompareUID(card_uid, &myConfig.user_cards[i].emergency_card) == MI_OK)
		{
			if (matched_emergency_card != NULL) *matched_emergency_card = 1U;
			return (int16_t) i;
		}
		if (myConfig.user_cards[i].normal_card_exists == 1U && MFRC522_CompareUID(card_uid, &myConfig.user_cards[i].normal_card) == MI_OK) return (int16_t) i;
	}
	return -1;
}
// Kiem tra con the nao co the xoa ngoai the thuong cua admin
uint8_t Auth_HasDeletableCard(void)
{
	for (uint8_t i = 0U; i < MAX_USERS; i++)
	{
		if (myConfig.user_cards[i].emergency_card_exists == 1U) return 1U;
		if (i != ADMIN_USER_INDEX && myConfig.user_cards[i].normal_card_exists == 1U) return 1U;
	}
	return 0U;
}
// Kiểm tra thẻ vừa quét đã được gán cho bất kỳ user nào hay chưa
uint8_t Auth_CardAlreadyExists(RFID_UID_t *card)
{
	for (uint8_t i = 0; i < MAX_USERS; i++)
	{
		if (!UserDB_UserExists(i)) continue;
		if (myConfig.user_cards[i].normal_card_exists == 1 && MFRC522_CompareUID(card, &myConfig.user_cards[i].normal_card) == MI_OK) return 1;
		if (myConfig.user_cards[i].emergency_card_exists == 1 && MFRC522_CompareUID(card, &myConfig.user_cards[i].emergency_card) == MI_OK) return 1;
	}
	return 0;
}
// Quet the truy cap va chi ghi nhan mot lan cho moi lan dua the
void RFID_Process(void)
{
	if (MFRC522_CheckUID(&rfid_reader, &scanned_card_uid) == MI_OK)
	{
		if (!is_rfid_scan_handled)
		{
			is_rfid_scan_handled = 1U;
			AccessAuth_ProcessCard(&scanned_card_uid);
		}
		return;
	}
	is_rfid_scan_handled = 0U;
}
// Quet the dang ky va luu vao dung loai thuong hoac khan cap
void Auth_ScanEnrollCard(uint8_t is_emergency_card)
{
	if (MFRC522_CheckUID(&rfid_reader, &scanned_card_uid) == MI_OK) Auth_EnrollCard(&scanned_card_uid, is_emergency_card);
}
// Danh dau cam bien mat ket noi khi lenh UART bi timeout hoac sai goi tin
void Finger_HandleConnectionError(uint8_t sensor_status)
{
	if (sensor_status != AS608_TIMEOUT && sensor_status != AS608_PACKET_ERROR) return;
	is_finger_sensor_ready = 0U;
	last_finger_reconnect_attempt_tick = HAL_GetTick();
}
// Kiem tra template va xu ly trang thai ket noi tai mot vi tri duy nhat
uint8_t Finger_CheckTemplate(uint16_t template_id, uint8_t *template_exists)
{
	uint8_t sensor_result;
	if (template_exists == NULL) return 0U;
	*template_exists = 0U;
	if (!is_finger_sensor_ready) return 0U;
	sensor_result = AS608_TemplateExists(&finger_sensor, template_id, template_exists);
	if (sensor_result != AS608_OK) Finger_HandleConnectionError(sensor_result);
	return sensor_result == AS608_OK;
}
// Xoa template neu ton tai va tra ve co thuc su xoa du lieu hay khong
uint8_t Finger_DeleteTemplateIfExists(uint16_t fingerprint_template_id, uint8_t *was_template_deleted)
{
	uint8_t template_exists = 0U;
	uint8_t sensor_status;
	if (was_template_deleted != NULL) *was_template_deleted = 0U;
	if (!Finger_CheckTemplate(fingerprint_template_id, &template_exists)) return 0U;
	if (!template_exists) return 1U;
	sensor_status = AS608_Delete(&finger_sensor, fingerprint_template_id);
	if (sensor_status != AS608_OK)
	{
		Finger_HandleConnectionError(sensor_status);
		return 0U;
	}
	if (was_template_deleted != NULL) *was_template_deleted = 1U;
	return 1U;
}
// Xoa van tay thuong, khan cap hoac ca hai theo lua chon hien tai
uint8_t Finger_DeleteSelectedTemplates(uint16_t normal_template_id, uint16_t emergency_template_id, AuthDeleteOption_t delete_option, uint8_t *was_any_template_deleted)
{
	uint8_t was_template_deleted = 0U;
	if (was_any_template_deleted != NULL) *was_any_template_deleted = 0U;
	if (delete_option == AUTH_DELETE_NORMAL || delete_option == AUTH_DELETE_BOTH)
	{
		if (!Finger_DeleteTemplateIfExists(normal_template_id, &was_template_deleted)) return 0U;
		if (was_template_deleted && was_any_template_deleted != NULL) *was_any_template_deleted = 1U;
	}
	if (delete_option == AUTH_DELETE_EMERGENCY || delete_option == AUTH_DELETE_BOTH)
	{
		if (!Finger_DeleteTemplateIfExists(emergency_template_id, &was_template_deleted)) return 0U;
		if (was_template_deleted && was_any_template_deleted != NULL) *was_any_template_deleted = 1U;
	}
	return 1U;
}
// Ch�? ngư�?i dùng đặt ngón tay lên cảm biến trong th�?i gian giới hạn
uint8_t Auth_WaitFingerPlaced(uint32_t timeout_ms)
{
	uint32_t wait_start_tick = HAL_GetTick();
	while (HAL_GetTick() - wait_start_tick < timeout_ms)
	{
		uint8_t sensor_status = AS608_GetImage(&finger_sensor);
		if (sensor_status == AS608_OK) return AS608_OK;
		if (sensor_status != AS608_NO_FINGER) return sensor_status;
		HAL_Delay(50);
	}
	return AS608_TIMEOUT;
}
// Ch�? ngư�?i dùng nhấc ngón tay kh�?i cảm biến trong th�?i gian giới hạn
uint8_t Auth_WaitFingerRemoved(uint32_t timeout_ms)
{
	uint32_t wait_start_tick = HAL_GetTick();
	while (HAL_GetTick() - wait_start_tick < timeout_ms)
	{
		uint8_t sensor_status = AS608_GetImage(&finger_sensor);
		if (sensor_status == AS608_NO_FINGER) return AS608_OK;
		if (sensor_status != AS608_OK) return sensor_status;
		HAL_Delay(50);
	}
	return AS608_TIMEOUT;
}
// Thu hai mẫu vân tay, tạo mẫu hoàn chỉnh và lưu vào ID được chỉ định
uint8_t Auth_EnrollFinger(uint16_t template_id)
{
	uint8_t sensor_result;
	uint8_t template_exists = 0U;
	if (!Finger_CheckTemplate(template_id, &template_exists))
	{
		LCD_ShowMessage("LOI KET NOI", "CAM BIEN VAN TAY", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		return 0;
	}
	if (template_exists)
	{
		LCD_ShowMessage("ID VAN TAY NAY", "DA TON TAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		return 0;
	}
	LCD_ShowMessage("DAT NGON TAY DE", "DANG KY VAN TAY", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
	sensor_result = Auth_WaitFingerPlaced(FINGER_PLACE_TIMEOUT_MS);
	if (sensor_result != AS608_OK)
	{
		if (sensor_result == AS608_TIMEOUT) LCD_ShowMessage("QUA THOI GIAN", "DAT VAN TAY", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		else LCD_ShowMessage("LAY ID VAN TAY", "THAT BAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		return 0;
	}
	sensor_result = AS608_Image2Tz(&finger_sensor, 1);
	if (sensor_result != AS608_OK)
	{
		Finger_HandleConnectionError(sensor_result);
		LCD_ShowMessage("TAO MAU VAN TAY", "THAT BAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		return 0;
	}
	uint16_t matched_template_id = 0;
	uint16_t match_score = 0;
	sensor_result = AS608_Search(&finger_sensor, 1, &matched_template_id, &match_score);
	if (sensor_result == AS608_OK)
	{
		LCD_ShowMessage("LOI:HIEN TAI", "DA CO VAN TAY", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		return 0;
	}
	if (sensor_result != AS608_NOT_FOUND)
	{
		Finger_HandleConnectionError(sensor_result);
		LCD_ShowMessage("LOI TIM VAN TAY", "KIEM TRA LAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		return 0;
	}
	LCD_ShowMessage("DA NHAN VAN TAY", "HAY THA TAY RA", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
	sensor_result = Auth_WaitFingerRemoved(FINGER_REMOVE_TIMEOUT_MS);
	if (sensor_result != AS608_OK)
	{
		if (sensor_result == AS608_TIMEOUT) LCD_ShowMessage("QUA THOI GIAN", "THA VAN TAY", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		else LCD_ShowMessage("LOI CAM BIEN", "KHI THA VAN TAY", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		return 0;
	}
	LCD_ShowMessage("DAT LAI NGON TAY", "DE XAC THUC", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
	sensor_result = Auth_WaitFingerPlaced(FINGER_PLACE_TIMEOUT_MS);
	if (sensor_result != AS608_OK)
	{
		if (sensor_result == AS608_TIMEOUT) LCD_ShowMessage("QUA THOI GIAN", "DAT LAI VAN TAY", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		else LCD_ShowMessage("LAY ID VAN TAY", "THAT BAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		return 0;
	}
	sensor_result = AS608_Image2Tz(&finger_sensor, 2);
	if (sensor_result != AS608_OK)
	{
		Finger_HandleConnectionError(sensor_result);
		LCD_ShowMessage("TAO MAU VAN TAY", "THAT BAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		return 0;
	}
	sensor_result = AS608_CreateModel(&finger_sensor);
	if (sensor_result != AS608_OK)
	{
		Finger_HandleConnectionError(sensor_result);
		LCD_ShowMessage("VAN TAY KHONG", "TRUNG KHOP", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		return 0;
	}
	sensor_result = AS608_StoreModel(&finger_sensor, template_id);
	if (sensor_result != AS608_OK)
	{
		Finger_HandleConnectionError(sensor_result);
		LCD_ShowMessage("LUU VAN TAY", "THAT BAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		return 0;
	}
	LCD_ShowMessage("LUU VAN TAY", "THANH CONG", LCD_ALIGNMENT_LEFT, LCD_BEEP_ONCE, 1500);
	return 1;
}
// Quét vân tay và ghi nhận yếu tố sinh trắc h�?c cho phiên mở cửa hoặc menu admin
void Finger_Process(void)
{
	int16_t user_index;
	uint16_t matched_fingerprint_template_id;
	uint16_t fingerprint_match_score;
	uint8_t fingerprint_sensor_status;
	uint8_t is_emergency_method = 0U;
	if (!is_finger_sensor_ready) return;
	if (current_system_state != STATE_HOME_SCREEN && current_system_state != STATE_ADMIN_AUTHENTICATION) return;
	if (input_length > 0) return;
	if (HAL_GetTick() - last_finger_poll_tick < 300U) return;
	last_finger_poll_tick = HAL_GetTick();
	uint8_t fingerprint_image_status = AS608_GetImage(&finger_sensor);
	if (fingerprint_image_status == AS608_NO_FINGER)
	{
		is_finger_scan_handled = 0;
		return;
	}
	if (fingerprint_image_status != AS608_OK)
	{
		Finger_HandleConnectionError(fingerprint_image_status);
		return;
	}
	if (is_finger_scan_handled) return;
	is_finger_scan_handled = 1;
	fingerprint_sensor_status = AS608_Image2Tz(&finger_sensor, 1);
	if (fingerprint_sensor_status != AS608_OK)
	{
		Finger_HandleConnectionError(fingerprint_sensor_status);
		return;
	}
	fingerprint_sensor_status = AS608_Search(&finger_sensor, 1, &matched_fingerprint_template_id, &fingerprint_match_score);
	if (fingerprint_sensor_status == AS608_OK)
	{
		user_index = FindUserByFingerId(matched_fingerprint_template_id, &is_emergency_method);
		if (user_index >= 0)
		{
			if (access_auth_target == ACCESS_AUTH_TARGET_ADMIN_MENU && is_emergency_method)
			{
				LCD_ShowMessage("LOI:VAN TAY", "KHONG DUOC CAP", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
				HomeScreen();
				return;
			}
			AccessAuth_RecordVerifiedMethod((uint8_t) user_index, AUTH_METHOD_FINGER_MASK, is_emergency_method);
			return;
		}
		LCD_ShowMessage("LOI:VAN TAY", "KHONG DUOC CAP", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		HomeScreen();
		return;
	}
	if (fingerprint_sensor_status == AS608_NOT_FOUND)
	{
		LCD_ShowMessage("LOI:VAN TAY", "KHONG HOP LE", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
		HomeScreen();
		return;
	}
	Finger_HandleConnectionError(fingerprint_sensor_status);
}
// Phan tich USER_ADD hoac USER_DELETE, cap nhat phan cung, EEPROM va gui ACK
void UserDB_ProcessCommand(void)
{
	if (!is_esp_user_command_ready) return;
	if (current_system_state != STATE_HOME_SCREEN || is_access_auth_active || input_length > 0U) return;
	char command_parse_buffer[128];
	strncpy(command_parse_buffer, esp_user_command, sizeof(command_parse_buffer) - 1U);
	command_parse_buffer[sizeof(command_parse_buffer) - 1U] = '\0';
	is_esp_user_command_ready = 0U;
	memset(esp_user_command, 0, sizeof(esp_user_command));
	char *command_action = strtok(command_parse_buffer, "|");
	char *command_id = strtok(NULL, "|");
	char *user_id_text = strtok(NULL, "|");
	char *version_text = strtok(NULL, "|");
	char *user_name = strtok(NULL, "|");
	char *floor_text = strtok(NULL, "|");
	char *room_text = strtok(NULL, "|");
	char *activation_code = strtok(NULL, "|");
	if (command_action == NULL || command_id == NULL || user_id_text == NULL || version_text == NULL)
	{
		UserDB_SendAck(command_id != NULL ? command_id : "0", "BAD_FORMAT");
		return;
	}
	uint16_t user_id = (uint16_t) atoi(user_id_text);
	uint16_t received_version = (uint16_t) atoi(version_text);
	if (user_id < 1U || user_id > MAX_USERS)
	{
		UserDB_SendAck(command_id, "INVALID_USER");
		return;
	}
	uint8_t user_index = (uint8_t) (user_id - 1U);
	if (received_version <= USER_RECORDS[user_index].version)
	{
		UserDB_SendAck(command_id, "STALE");
		return;
	}
	if (strcmp(command_action, "USER_ADD") == 0)
	{
		if (user_name == NULL || strlen(user_name) == 0U || strlen(user_name) > USER_NAME_MAX_LENGTH)
		{
			UserDB_SendAck(command_id, "INVALID_NAME");
			return;
		}
		if (floor_text == NULL || room_text == NULL)
		{
			UserDB_SendAck(command_id, "MISSING_LOCATION");
			return;
		}
		if (!UserDB_IsSixDigitText(activation_code))
		{
			UserDB_SendAck(command_id, "INVALID_ACTIVATION_CODE");
			return;
		}
		uint16_t floor_value = (uint16_t) atoi(floor_text);
		uint16_t room_value = (uint16_t) atoi(room_text);
		if (floor_value < 1U || floor_value > 99U)
		{
			UserDB_SendAck(command_id, "INVALID_FLOOR");
			return;
		}
		if (room_value < 1U || room_value > 9999U)
		{
			UserDB_SendAck(command_id, "INVALID_ROOM");
			return;
		}
		if (USER_RECORDS[user_index].status != USER_STATUS_EMPTY)
		{
			UserDB_SendAck(command_id, "USER_EXISTS");
			return;
		}
		UserProfileData_t previous_user_data = USER_RECORDS[user_index];
		UserCardData_t previous_card_data = myConfig.user_cards[user_index];
		uint8_t previous_existing_user_count = existing_user_count;
		memset(&USER_RECORDS[user_index], 0, sizeof(USER_RECORDS[user_index]));
		memset(&myConfig.user_cards[user_index], 0, sizeof(UserCardData_t));
		strncpy(USER_RECORDS[user_index].name, user_name, sizeof(USER_RECORDS[user_index].name) - 1U);
		USER_RECORDS[user_index].name[sizeof(USER_RECORDS[user_index].name) - 1U] = '\0';
		USER_RECORDS[user_index].finger_id = user_id - 1U;
		USER_RECORDS[user_index].duress_finger_id = user_id - 1U + FINGER_EMERGENCY_OFFSET;
		USER_RECORDS[user_index].version = received_version;
		USER_RECORDS[user_index].status = USER_STATUS_PENDING;
		USER_RECORDS[user_index].access_state = ACCESS_DIRECTION_EXITING;
		USER_RECORDS[user_index].floor = (uint8_t) floor_value;
		USER_RECORDS[user_index].room = room_value;
		strcpy(USER_RECORDS[user_index].activation_code, activation_code);
		UserDB_Recount();
		uint8_t save_succeeded = Storage_SaveUserAndCard(user_index, &USER_RECORDS[user_index], &myConfig.user_cards[user_index]);
		if (save_succeeded) save_succeeded = Storage_SaveUserCount(existing_user_count);
		if (save_succeeded)
		{
			UserDB_SendAck(command_id, "OK");
			return;
		}
		USER_RECORDS[user_index] = previous_user_data;
		myConfig.user_cards[user_index] = previous_card_data;
		existing_user_count = previous_existing_user_count;
		uint8_t rollback_succeeded = Storage_SaveUserAndCard(user_index, &USER_RECORDS[user_index], &myConfig.user_cards[user_index]);
		if (rollback_succeeded) rollback_succeeded = Storage_SaveUserCount(existing_user_count);
		if (rollback_succeeded) UserDB_SendAck(command_id, "STORAGE ERROR");
		else UserDB_SendAck(command_id, "ROLLBACK ERROR");
		return;
	}
	if (strcmp(command_action, "USER_DELETE") == 0)
	{
	    if (user_index == ADMIN_USER_INDEX)
	    {
	        UserDB_SendAck(command_id, "ADMIN_PROTECTED");
	        return;
	    }
	    uint16_t normal_id = user_id - 1U;
	    uint16_t emergency_id = user_id - 1U + FINGER_EMERGENCY_OFFSET;
	    if (!is_finger_sensor_ready)
	    {
	        UserDB_SendAck(command_id, "AS608_OFFLINE");
	        return;
	    }
	    UserProfileData_t previous_user_data = USER_RECORDS[user_index];
	    UserCardData_t previous_card_data = myConfig.user_cards[user_index];
	    uint8_t previous_existing_user_count = existing_user_count;
	    memset(&USER_RECORDS[user_index], 0, sizeof(USER_RECORDS[user_index]));
	    USER_RECORDS[user_index].version = received_version;
	    USER_RECORDS[user_index].status = USER_STATUS_EMPTY;
	    memset(&myConfig.user_cards[user_index], 0, sizeof(UserCardData_t));
	    UserDB_Recount();
	    uint8_t save_succeeded = Storage_SaveUserAndCard(user_index, &USER_RECORDS[user_index], &myConfig.user_cards[user_index]);
	    if (save_succeeded) save_succeeded = Storage_SaveUserCount(existing_user_count);
	    if (!save_succeeded)
	    {
	        USER_RECORDS[user_index] = previous_user_data;
	        myConfig.user_cards[user_index] = previous_card_data;
	        existing_user_count = previous_existing_user_count;

	        uint8_t rollback_succeeded = Storage_SaveUserAndCard(user_index, &USER_RECORDS[user_index], &myConfig.user_cards[user_index]);
	        if (rollback_succeeded) rollback_succeeded = Storage_SaveUserCount(existing_user_count);

	        if (rollback_succeeded) UserDB_SendAck(command_id, "STORAGE ERROR");
	        else UserDB_SendAck(command_id, "ROLLBACK ERROR");
	        return;
	    }
	    uint8_t cleanup_succeeded = Finger_DeleteSelectedTemplates(normal_id, emergency_id, AUTH_DELETE_BOTH, NULL);
	    if (!cleanup_succeeded)
	    {
	        ESP_SendLine("AS608:", "DELETE_CLEANUP_ERROR");
	        UserDB_SendAck(command_id, "FINGER_CLEANUP_ERROR");
	        return;
	    }
	    UserDB_SendAck(command_id, "OK");
	    return;
	}
	UserDB_SendAck(command_id, "UNKNOWN_COMMAND");
}
// Xử lý yêu cầu mở cửa, reset và xem nhật ký được gửi từ Blynk qua ESP32
void ESP_Command_Process(void)
{
	if (is_esp_door_open_requested)
	{
		is_esp_door_open_requested = 0;
		if (current_system_state != STATE_HOME_SCREEN || is_access_auth_active || input_length > 0U)
		{
			ESP_SendLine("CANH BAO:CO ", "NGUOI THAO TAC");
		}
		else
		{
			Door_Open_Auto();
			current_system_state = STATE_DOOR_OPERATION;
			LCD_ShowMessage("DANG MO CUA", "TU XA QUA BLYNK", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
		}
	}
	if (is_esp_system_reset_requested)
	{
		is_esp_system_reset_requested = 0;
		LCD_ShowMessage("DANG RESET TU XA", "QUA BLYNK", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 1500);
		ESP_SendLCD("CUA KHOA", "");
		NVIC_SystemReset();
	}
	if (is_esp_access_log_requested)
	{
		is_esp_access_log_requested = 0;
		ESP_SendAccessLogs();
	}
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI2_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */
	HAL_UART_Receive_IT(&huart1, &esp_received_byte, 1);
	Servo_Init();
	LCD_Init();
	LCD_LoadCustomChars();
	LCD_ShowMessage("HE THONG", "DANG KHOI DONG", LCD_ALIGNMENT_CENTER, LCD_BEEP_NONE, 0);
	HAL_Delay(500);
	MFRC522_Init(&rfid_reader, &hspi2, GPIOB, GPIO_PIN_12);
	HCSR04_Init();
	finger_sensor.huart = &huart2;
	finger_sensor.address = 0xFFFFFFFFU;
	finger_sensor.password = 0x00000000U;
	__HAL_UART_FLUSH_DRREGISTER(&huart2);
	is_finger_sensor_ready = (AS608_CheckPassword(&finger_sensor) == AS608_OK);
	last_finger_reconnect_attempt_tick = HAL_GetTick();
	uint8_t was_config_loaded = LoadDataFromAT24C256((uint8_t*) &myConfig, sizeof(myConfig));
	if (!was_config_loaded || myConfig.magic != CONFIG_MAGIC)
	{
		memset(&myConfig, 0, sizeof(myConfig));
		myConfig.magic = CONFIG_MAGIC;
		strcpy(myConfig.admin_password, "888888");
		myConfig.existing_user_count = 0;
		myConfig.access_log_count = 0;
		myConfig.access_log_index = 0;
		if (!SaveDataToAT24C256((uint8_t*) &myConfig, sizeof(ConfigData_t)))
		{
			Error_Handler();
		}
	}
	access_log_count = myConfig.access_log_count;
	next_log_write_index = myConfig.access_log_index;
	if (access_log_count > MAX_ACCESS_LOG)
		access_log_count = 0;
	if (next_log_write_index >= MAX_ACCESS_LOG)
		next_log_write_index = 0;
	for (uint8_t i = 0; i < MAX_USERS; i++)
	{
		if (USER_RECORDS[i].status > USER_STATUS_ACTIVE)
		{
			memset(&USER_RECORDS[i], 0, sizeof(USER_RECORDS[i]));
		}
		if (USER_RECORDS[i].status != USER_STATUS_EMPTY)
		{
			USER_RECORDS[i].name[sizeof(USER_RECORDS[i].name) - 1] = '\0';
			USER_RECORDS[i].user_password[USER_PASSWORD_LENGTH] = '\0';
			USER_RECORDS[i].duress_password[USER_PASSWORD_LENGTH] = '\0';
			USER_RECORDS[i].activation_code[ACTIVATION_CODE_LENGTH] = '\0';
			USER_RECORDS[i].finger_id = i;
			USER_RECORDS[i].duress_finger_id = i + FINGER_EMERGENCY_OFFSET;
			if (USER_RECORDS[i].access_state > ACCESS_DIRECTION_ENTERING) USER_RECORDS[i].access_state = ACCESS_DIRECTION_EXITING;
		}
	}
	UserDB_Recount();
	HomeScreen();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1)
	{
		if (!is_finger_sensor_ready && (current_system_state == STATE_HOME_SCREEN || current_system_state == STATE_ADMIN_AUTHENTICATION)
				&& input_length == 0 && HAL_GetTick() - last_finger_reconnect_attempt_tick >= 5000U)
		{
			last_finger_reconnect_attempt_tick = HAL_GetTick();
			__HAL_UART_FLUSH_DRREGISTER(&huart2);
			if (AS608_CheckPassword(&finger_sensor) == AS608_OK) is_finger_sensor_ready = 1;
		}
		Finger_Process();
		ESP_Command_Process();
		UserDB_ProcessCommand();
		Passage_Process();
		AccessDisplay_Process();
		Door_AutoControl_Process();
		char key = KeyPad_GetKey();
		if (key != 0) last_ui_activity_tick = HAL_GetTick();
		UI_Timeout_Process();
		if (current_system_state != STATE_DOOR_OPERATION && key != 0) HAL_Delay(180);
		switch (current_system_state)
		{
		case STATE_DOOR_OPERATION:
			break;
		case STATE_HOME_SCREEN:
			if (key != 0)
			{
				if (key == '#' && input_length == 0)
				{
					if (is_access_auth_active)
					{
						AccessAuth_ShowProgress();
						break;
					}
					LCD_ShowMessage("LOI:CHUA NHAP", "MAT KHAU", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					HomeScreen();
					break;
				}
				if (Auth_InputDigit(key, USER_PASSWORD_LENGTH, 1U)) break;
				else if (key == '#')
				{
					if (input_length != USER_PASSWORD_LENGTH)
					{
						LCD_ShowMessage("PASS PHAI DU", "6 CHU SO", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
						HomeScreen();
						break;
					}
					AccessAuth_ProcessPassword(input_buffer);
					break;
				}
				else if (key == 'A' && !is_access_auth_active)
				{
					Auth_StartPasswordFlow();
					break;
				}
				else if (key == 'B' && !is_access_auth_active)
				{
					Auth_StartCardFlow();
					break;
				}
				else if (key == 'C' && !is_access_auth_active)
				{
					Auth_StartFingerFlow();
					break;
				}
				else if (key == 'D' && !is_access_auth_active)
				{
					AccessAuth_Start(ACCESS_AUTH_TARGET_ADMIN_MENU);
					break;
				}
				else if (key == '*')
				{
					HomeScreen();
					break;
				}
			}
			RFID_Process();
			break;
		case STATE_LOG_VIEW:
			if (key == '#')
			{
				if (access_log_count > 0)
				{
					if (current_displayed_log_index == 0) current_displayed_log_index = access_log_count - 1;
					else current_displayed_log_index--;
					AccessLog_Show(current_displayed_log_index);
				}
				break;
			}
			else if (key == '*')
			{
				Admin_ShowSystemMenu();
				break;
			}
			break;
		case STATE_ADMIN_AUTHENTICATION:
			if (Auth_InputDigit(key, USER_PASSWORD_LENGTH, 1U)) break;
			else if (key == '#')
			{
				if (input_length != USER_PASSWORD_LENGTH)
				{
					LCD_ShowMessage("PASS ADMIN PHAI", "DU 6 CHU SO", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					HomeScreen();
					break;
				}
				AccessAuth_ProcessPassword(input_buffer);
				break;
			}
			else if (key == '*')
			{
				HomeScreen();
				break;
			}
			RFID_Process();
			break;
		case STATE_ADMIN_NEW_PASSWORD_INPUT:
			if (Auth_InputDigit(key, USER_PASSWORD_LENGTH, 1U)) break;
			else if (key == '#')
			{
				if (input_length != USER_PASSWORD_LENGTH)
				{
					LCD_ShowMessage("NHAP SAI MAT", "KHAU ADMIN", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Admin_ShowPasswordInput();
					break;
				}
				if (UserDB_PasswordExists(input_buffer, MAX_USERS))
				{
					LCD_ShowMessage("PASS ADMIN TRUNG", "PASS USER", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Admin_ShowPasswordInput();
					break;
				}
				char admin_password_backup[sizeof(myConfig.admin_password)];
				memcpy(admin_password_backup, myConfig.admin_password, sizeof(admin_password_backup));
				admin_password_backup[sizeof(admin_password_backup) - 1U] = '\0';
				uint8_t save_succeeded = Storage_SaveAdminPassword(input_buffer);
				if (save_succeeded) LCD_ShowMessage("DOI PASS ADMIN", "THANH CONG", LCD_ALIGNMENT_LEFT, LCD_BEEP_ONCE, 1500);
				else
				{
					memcpy(myConfig.admin_password, admin_password_backup, sizeof(myConfig.admin_password));
					if (!Storage_SaveAdminPassword(admin_password_backup)) ESP_SendLine("STORAGE:", "ADMIN_ROLLBACK_ERROR");
					LCD_ShowMessage("LUU DU LIEU", "THAT BAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
				}
				Admin_ShowSystemMenu();
				break;
			}
			else if (key == '*')
			{
				Admin_ShowSystemMenu();
				break;
			}
			break;
		case STATE_ADMIN_DELETE_ALL_CARDS:
		{
			static UserCardData_t all_cards_backup[MAX_USERS];
			if (key == '*')
			{
				Admin_ShowSystemMenu();
				break;
			}
			if (key == '#')
			{
				if (!Auth_HasDeletableCard())
				{
					LCD_ShowMessage("KHONG CO THE", "USER DE XOA", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Admin_ShowSystemMenu();
					break;
				}
				memcpy(all_cards_backup, myConfig.user_cards, sizeof(myConfig.user_cards));
				for (uint8_t i = 0U; i < MAX_USERS; i++)
				{
					if (i == ADMIN_USER_INDEX)
					{
						memset(&myConfig.user_cards[i].emergency_card, 0, sizeof(RFID_UID_t));
						myConfig.user_cards[i].emergency_card_exists = 0U;
						continue;
					}
					memset(&myConfig.user_cards[i], 0, sizeof(UserCardData_t));
				}
				if (Storage_SaveAllUserCards(myConfig.user_cards))
				{
					LCD_ShowMessage("XOA THE USER", "THANH CONG", LCD_ALIGNMENT_LEFT, LCD_BEEP_ONCE, 1500);
				}
				else
				{
					memcpy(myConfig.user_cards, all_cards_backup, sizeof(myConfig.user_cards));
					if (!Storage_SaveAllUserCards(all_cards_backup)) ESP_SendLine("STORAGE:", "ALL_CARD_ROLLBACK_ERROR");
					LCD_ShowMessage("LUU DU LIEU", "THAT BAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
				}
				Admin_ShowSystemMenu();
				break;
			}
			break;
		}
		case STATE_ADMIN_DELETE_ALL_FINGERPRINTS:
		{
			uint8_t all_fingerprint_deletions_succeeded = 1U;
			uint8_t was_template_deleted;
			uint16_t deleted_template_count = 0U;
			uint16_t admin_template_id = USER_RECORDS[ADMIN_USER_INDEX].finger_id;
			uint16_t fingerprint_template_id_limit = FINGER_EMERGENCY_OFFSET + MAX_USERS;
			if (key == '*')
			{
				Admin_ShowSystemMenu();
				break;
			}
			if (key == '#')
			{
				if (!is_finger_sensor_ready)
				{
					LCD_ShowMessage("LOI KET NOI", "CAM BIEN VAN TAY", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Admin_ShowSystemMenu();
					break;
				}
				for (uint16_t template_id = 0U; template_id < fingerprint_template_id_limit; template_id++)
				{
					if (template_id == admin_template_id) continue;
					if (!Finger_DeleteTemplateIfExists(template_id, &was_template_deleted))
					{
						all_fingerprint_deletions_succeeded = 0U;
						break;
					}
					deleted_template_count += was_template_deleted;
				}
				if (!all_fingerprint_deletions_succeeded) LCD_ShowMessage("XOA VT CHUA HET", "KIEM TRA LAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
				else if (deleted_template_count == 0U) LCD_ShowMessage("KHONG CO VAN TAY", "USER DE XOA", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
				else LCD_ShowMessage("XOA VAN TAY USER", "THANH CONG", LCD_ALIGNMENT_LEFT, LCD_BEEP_ONCE, 1500);
				Admin_ShowSystemMenu();
				break;
			}
			break;
		}
		case STATE_CREDENTIAL_MANAGEMENT_USER_ID_INPUT:
			if (Auth_InputDigit(key, 3U, 0U)) break;
			if (key == '#')
			{
				if (input_length == 0)
				{
					LCD_ShowMessage("LOI:CHUA NHAP", "USER ID", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Auth_ShowUserIdInput();
					break;
				}
				uint16_t entered_user_id = atoi(input_buffer);
				if (entered_user_id < 1 || entered_user_id > MAX_USERS)
				{
					LCD_ShowMessage("USER ID SAI", "NHAP TU 1-150", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Auth_ShowUserIdInput();
					break;
				}
				uint8_t entered_user_index = (uint8_t) (entered_user_id - 1);
				if (current_credential_management_flow == CREDENTIAL_MANAGEMENT_FLOW_NONE)
				{
					LCD_ShowMessage("LOI QUY TRINH", "THAO TAC LAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					HomeScreen();
					break;
				}
				if (!UserDB_CanManage(entered_user_index))
				{
					LCD_ShowMessage("USER KHONG TON", "TAI HOAC BI XOA", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Auth_ShowUserIdInput();
					break;
				}
				credential_management_user_index = entered_user_index;
				Auth_ShowConfirmUser();
				break;
			}
			if (key == '*')
			{
				HomeScreen();
				break;
			}
			break;
		case STATE_CREDENTIAL_MANAGEMENT_USER_CONFIRMATION:
			if (key == '#')
			{
				if (USER_RECORDS[credential_management_user_index].status == USER_STATUS_PENDING)
				{
					if (current_credential_management_flow == CREDENTIAL_MANAGEMENT_FLOW_PASSWORD)
					{
						Auth_ShowActivationCode();
						break;
					}
					LCD_ShowMessage("CHUA ACTIVE,TAO", "PASS DE ACTIVE", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					HomeScreen();
					break;
				}
				if (USER_RECORDS[credential_management_user_index].status == USER_STATUS_ACTIVE)
				{
					Auth_ShowCurrentUserPasswordInput();
					break;
				}
				LCD_ShowMessage("USER KHONG TON", "TAI HOAC BI XOA", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
				HomeScreen();
				break;
			}
			if (key == '*')
			{
				Auth_ShowUserIdInput();
				break;
			}
			break;
		case STATE_ACTIVATION_CODE_INPUT:
			if (Auth_InputDigit(key, ACTIVATION_CODE_LENGTH, 1U)) break;
			if (key == '#')
			{
				if (input_length != ACTIVATION_CODE_LENGTH)
				{
					LCD_ShowMessage("MA XAC MINH PHAI", "DU 6 CHU SO", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Auth_ShowActivationCode();
					break;
				}
				if (strcmp(input_buffer,
				USER_RECORDS[credential_management_user_index].activation_code) != 0)
				{
					LCD_ShowMessage("NHAP SAI MA XAC", "MINH,HAY THU LAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Auth_ShowActivationCode();
					break;
				}
				Auth_ShowNewNormalPasswordInput();
				break;
			}
			if (key == '*')
			{
				Auth_ShowConfirmUser();
				break;
			}
			break;
		case STATE_CURRENT_USER_PASSWORD_INPUT:
			if (Auth_InputDigit(key, USER_PASSWORD_LENGTH, 1U)) break;
			if (key == '#')
			{
				if (input_length != USER_PASSWORD_LENGTH || strcmp(input_buffer,USER_RECORDS[credential_management_user_index].user_password) != 0)
				{
					LCD_ShowMessage("LOI:PASS USER", "KHONG HOP LE", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Auth_ShowCurrentUserPasswordInput();
					break;
				}
				if (current_credential_management_flow == CREDENTIAL_MANAGEMENT_FLOW_PASSWORD)
				{
					Auth_ShowNewNormalPasswordInput();
					break;
				}
				Auth_ShowUserManageMenu();
				break;
			}
			if (key == '*')
			{
				Auth_ShowConfirmUser();
				break;
			}
			break;
		case STATE_USER_AUTH_METHOD_MENU:
			if (key == '1')
			{
				if (current_credential_management_flow == CREDENTIAL_MANAGEMENT_FLOW_CARD)
				{
					Auth_BeginCardEditSession();
					Auth_ShowNormalCard();
				}
				else if (current_credential_management_flow == CREDENTIAL_MANAGEMENT_FLOW_FINGERPRINT) Auth_ShowNormalFinger();
				else HomeScreen();
				break;
			}
			if (key == '2')
			{
				Auth_ShowUserDeleteMenu();
				break;
			}
			if (key == '*')
			{
				HomeScreen();
				break;
			}
			break;
		case STATE_AUTH_METHOD_DELETE_MENU:
		{
			uint8_t normal_credential_exists = 0U;
			uint8_t emergency_credential_exists = 0U;
			if (key == '*')
			{
				Auth_ShowUserManageMenu();
				break;
			}
			if (key < '1' || key > '3') break;
			if (current_credential_management_flow == CREDENTIAL_MANAGEMENT_FLOW_CARD)
			{
				normal_credential_exists = myConfig.user_cards[credential_management_user_index].normal_card_exists;
				emergency_credential_exists = myConfig.user_cards[credential_management_user_index].emergency_card_exists;
			}
			else if (current_credential_management_flow == CREDENTIAL_MANAGEMENT_FLOW_FINGERPRINT)
			{
				if ((key == '1' || key == '3') && !Finger_CheckTemplate(USER_RECORDS[credential_management_user_index].finger_id, &normal_credential_exists))
				{
					LCD_ShowMessage("LOI KET NOI", "CAM BIEN VAN TAY", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Auth_ShowUserManageMenu();
					break;
				}
				if ((key == '2' || key == '3') && !Finger_CheckTemplate(USER_RECORDS[credential_management_user_index].duress_finger_id, &emergency_credential_exists))
				{
					LCD_ShowMessage("LOI KET NOI", "CAM BIEN VAN TAY", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Auth_ShowUserManageMenu();
					break;
				}
			}
			else
			{
				HomeScreen();
				break;
			}
			if (key == '1' && !normal_credential_exists)
			{
				if (current_credential_management_flow == CREDENTIAL_MANAGEMENT_FLOW_CARD)
				{
					LCD_ShowMessage("CHUA CO THE", "THUONG DE XOA", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
				}
				else LCD_ShowMessage("CHUA CO VAN TAY", "THUONG DE XOA", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);

				Auth_ShowUserDeleteMenu();
				break;
			}
			if (key == '2' && !emergency_credential_exists)
			{
				if (current_credential_management_flow == CREDENTIAL_MANAGEMENT_FLOW_CARD)
				{
					LCD_ShowMessage("CHUA CO THE", "KHAN CAP DE XOA", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
				}
				else LCD_ShowMessage("CHUA CO VAN TAY", "KHAN CAP DE XOA", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);

				Auth_ShowUserDeleteMenu();
				break;
			}
			if (key == '3' && !normal_credential_exists && !emergency_credential_exists)
			{
				if (current_credential_management_flow == CREDENTIAL_MANAGEMENT_FLOW_CARD)
				{
					LCD_ShowMessage("HIEN TAI CHUA CO", "THE NAO DE XOA", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
				}
				else LCD_ShowMessage("HIEN TAI CHUA CO", "VAN TAY DE XOA", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);

				Auth_ShowUserDeleteMenu();
				break;
			}
			Auth_ShowUserDeleteConfirm((AuthDeleteOption_t) (key - '0'));
			break;
		}
		case STATE_AUTH_METHOD_DELETE_CONFIRM:
			if (key == '*')
			{
				Auth_ShowUserDeleteMenu();
				break;
			}
			if (key == '#')
			{
				if (current_credential_management_flow == CREDENTIAL_MANAGEMENT_FLOW_CARD)
				{
					UserCardData_t previous_card_data = myConfig.user_cards[credential_management_user_index];
					if (selected_auth_delete_option == AUTH_DELETE_NORMAL || selected_auth_delete_option == AUTH_DELETE_BOTH)
					{
						memset(&myConfig.user_cards[credential_management_user_index].normal_card, 0, sizeof(RFID_UID_t));
						myConfig.user_cards[credential_management_user_index].normal_card_exists = 0U;
					}
					if (selected_auth_delete_option == AUTH_DELETE_EMERGENCY || selected_auth_delete_option == AUTH_DELETE_BOTH)
					{
						memset(&myConfig.user_cards[credential_management_user_index].emergency_card, 0, sizeof(RFID_UID_t));
						myConfig.user_cards[credential_management_user_index].emergency_card_exists = 0U;
					}
					if (Storage_SaveUserCard(credential_management_user_index, &myConfig.user_cards[credential_management_user_index]))
					{
						LCD_ShowMessage("DA XOA THE", "THANH CONG", LCD_ALIGNMENT_LEFT, LCD_BEEP_ONCE, 1500);
					}
					else
					{
						myConfig.user_cards[credential_management_user_index] = previous_card_data;
						if (!Storage_SaveUserCard(credential_management_user_index, &previous_card_data))
						{
							ESP_SendLine("STORAGE:", "CARD_DELETE_ROLLBACK_ERROR");
						}
						LCD_ShowMessage("LUU DU LIEU", "THAT BAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					}
				}
				else if (current_credential_management_flow == CREDENTIAL_MANAGEMENT_FLOW_FINGERPRINT)
				{
					uint8_t was_any_template_deleted = 0U;
					uint16_t normal_template_id = USER_RECORDS[credential_management_user_index].finger_id;
					uint16_t emergency_template_id = USER_RECORDS[credential_management_user_index].duress_finger_id;
					if (!Finger_DeleteSelectedTemplates(normal_template_id, emergency_template_id, selected_auth_delete_option, &was_any_template_deleted))
					{
						LCD_ShowMessage("XOA VAN TAY", "THAT BAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					}
					else if (!was_any_template_deleted)
					{
						LCD_ShowMessage("VAN TAY CAN XOA", "KHONG TON TAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					}
					else
					{
						LCD_ShowMessage("XOA VAN TAY", "THANH CONG", LCD_ALIGNMENT_LEFT, LCD_BEEP_ONCE, 1500);
					}
				}
				else
				{
					HomeScreen();
					break;
				}
				Auth_ShowUserManageMenu();
				break;
			}
			break;
		case STATE_NEW_NORMAL_PASSWORD_INPUT:
			if (Auth_InputDigit(key, USER_PASSWORD_LENGTH, 1U)) break;
			if (key == '#')
			{
				if (input_length != USER_PASSWORD_LENGTH)
				{
					LCD_ShowMessage("PASS PHAI DU", "6 CHU SO", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Auth_ShowNewNormalPasswordInput();
					break;
				}
				if (strcmp(input_buffer,
				USER_RECORDS[credential_management_user_index].activation_code) == 0)
				{
					LCD_ShowMessage("KHONG DUOC TRUNG", "MA XAC MINH", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Auth_ShowNewNormalPasswordInput();
					break;
				}
				if (strcmp(input_buffer, myConfig.admin_password) == 0)
				{
					LCD_ShowMessage("PASS KHONG DUOC", "TRUNG PASS ADMIN", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Auth_ShowNewNormalPasswordInput();
					break;
				}
				if (UserDB_PasswordExists(input_buffer, credential_management_user_index))
				{
					LCD_ShowMessage("PASS DA TON TAI", "DOI PASS KHAC", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Auth_ShowNewNormalPasswordInput();
					break;
				}
				strcpy(pending_normal_password, input_buffer);
				Auth_ShowNewEmergencyPasswordInput();
				break;
			}
			if (key == '*')
			{
				if (USER_RECORDS[credential_management_user_index].status == USER_STATUS_PENDING) Auth_ShowActivationCode();
				else if (USER_RECORDS[credential_management_user_index].status == USER_STATUS_ACTIVE) Auth_ShowCurrentUserPasswordInput();
				else HomeScreen();
				break;
			}
			break;
		case STATE_NEW_EMERGENCY_PASSWORD_INPUT:
			if (Auth_InputDigit(key, USER_PASSWORD_LENGTH, 1U)) break;
			if (key == '#')
			{
				if (input_length != USER_PASSWORD_LENGTH)
				{
					LCD_ShowMessage("PASS PHAI DU", "6 CHU SO", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Auth_ShowNewEmergencyPasswordInput();
					break;
				}
				if (strcmp(input_buffer, pending_normal_password) == 0 || strcmp(input_buffer,USER_RECORDS[credential_management_user_index].activation_code) == 0)
				{
					LCD_ShowMessage("KHONG LUU TRUNG", "VOI PASS THUONG", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Auth_ShowNewEmergencyPasswordInput();
					break;
				}
				if (strcmp(input_buffer, myConfig.admin_password) == 0)
				{
					LCD_ShowMessage("PASS KHONG DUOC", "TRUNG PASS ADMIN", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Auth_ShowNewEmergencyPasswordInput();
					break;
				}
				if (UserDB_PasswordExists(input_buffer, credential_management_user_index))
				{
					LCD_ShowMessage("PASS DA TON TAI", "DOI PASS KHAC", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Auth_ShowNewEmergencyPasswordInput();
					break;
				}
				uint8_t was_user_pending = USER_RECORDS[credential_management_user_index].status == USER_STATUS_PENDING;
				UserProfileData_t previous_user_data = USER_RECORDS[credential_management_user_index];
				strcpy(USER_RECORDS[credential_management_user_index].user_password, pending_normal_password);
				strcpy(USER_RECORDS[credential_management_user_index].duress_password, input_buffer);
				memset(USER_RECORDS[credential_management_user_index].activation_code, 0, sizeof(USER_RECORDS[credential_management_user_index].activation_code));
				USER_RECORDS[credential_management_user_index].status = USER_STATUS_ACTIVE;
				if (!Storage_SaveUserProfile(credential_management_user_index, &USER_RECORDS[credential_management_user_index]))
				{
					USER_RECORDS[credential_management_user_index] = previous_user_data;
					if (!Storage_SaveUserProfile(credential_management_user_index, &previous_user_data)) ESP_SendLine("STORAGE:", "PASSWORD_ROLLBACK_ERROR");
					LCD_ShowMessage("QUA TRINH LUU", "PASS THAT BAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
					Auth_ShowNewEmergencyPasswordInput();
					break;
				}
				if (was_user_pending)
				{
					char user_event[48];
					snprintf(user_event, sizeof(user_event), "USER_EVENT|%u|ACTIVE|%u", credential_management_user_index + 1U, USER_RECORDS[credential_management_user_index].version);
					ESP_SendLine("", user_event);
				}
				memset(pending_normal_password, 0, sizeof(pending_normal_password));
				if (was_user_pending) LCD_ShowMessage("DA LUU PASS", "THANH CONG", LCD_ALIGNMENT_LEFT, LCD_BEEP_ONCE, 1500);
				else LCD_ShowMessage("DA DOI PASS", "THANH CONG", LCD_ALIGNMENT_LEFT, LCD_BEEP_ONCE, 1500);
				HomeScreen();
				break;
			}
			if (key == '*')
			{
				Auth_ShowNewNormalPasswordInput();
				break;
			}
			break;
		case STATE_NORMAL_FINGERPRINT_MANAGEMENT:
			if (key == '*')
			{
				Auth_ShowUserManageMenu();
				break;
			}
			if (is_normal_fingerprint_registered)
			{
				if (key == '#') Auth_ShowEmergencyFinger();
				break;
			}
			if (Auth_InputDigit(key, 3U, 0U)) break;
			if (key == '#') Auth_ValidateTemplateIdAndEnrollFingerprint(0U);
			break;
		case STATE_EMERGENCY_FINGERPRINT_MANAGEMENT:
			if (Auth_InputDigit(key, 3U, 0U)) break;
			if (key == '#')
			{
				if (input_length > 0U) Auth_ValidateTemplateIdAndEnrollFingerprint(1U);
				else if (is_normal_fingerprint_registered)
				{
					LCD_ShowMessage("QUA TRINH THEM", "DA HOAN TAT", LCD_ALIGNMENT_LEFT, LCD_BEEP_ONCE, 1500);
					Auth_ShowUserManageMenu();
				}
				break;
			}
			if (key == '*')
			{
				Auth_ShowNormalFinger();
				break;
			}
			break;
		case STATE_NORMAL_CARD_MANAGEMENT:
			if (key == '*')
			{
				Auth_CancelCardEditSession();
				Auth_ShowUserManageMenu();
				break;
			}
			if (myConfig.user_cards[credential_management_user_index].normal_card_exists == 1U)
			{
				if (key == '#') Auth_ShowEmergencyCard();
				break;
			}
			Auth_ScanEnrollCard(0U);
			break;
		case STATE_EMERGENCY_CARD_MANAGEMENT:
			if (key == '#')
			{
				Auth_ShowCardSaveConfirm();
				break;
			}
			if (key == '*')
			{
				Auth_ShowNormalCard();
				break;
			}
			if (myConfig.user_cards[credential_management_user_index].emergency_card_exists == 1U) break;
			Auth_ScanEnrollCard(1U);
			break;
		case STATE_CARD_CHANGES_SAVE_CONFIRM:
			if (current_credential_management_flow != CREDENTIAL_MANAGEMENT_FLOW_CARD)
			{
				HomeScreen();
				break;
			}
			if (key == '*')
			{
				Auth_ShowEmergencyCard();
				break;
			}
			if (key == '#')
			{
				uint8_t save_succeeded = Storage_SaveUserAndCard(credential_management_user_index, &USER_RECORDS[credential_management_user_index], &myConfig.user_cards[credential_management_user_index]);
				if (save_succeeded)
				{
					is_card_edit_active = 0U;
					LCD_ShowMessage("QUA TRINH LUU", "HOAN TAT", LCD_ALIGNMENT_LEFT, LCD_BEEP_ONCE, 1500);
				}
				else
				{
					Auth_CancelCardEditSession();
					if (!Storage_SaveUserCard(credential_management_user_index, &myConfig.user_cards[credential_management_user_index]))
					{
						ESP_SendLine("STORAGE:", "CARD ROLLBACK ERROR");
					}
					LCD_ShowMessage("LUU DU LIEU", "THAT BAI", LCD_ALIGNMENT_LEFT, LCD_BEEP_ERROR, 1500);
				}
				Auth_ShowUserManageMenu();
				break;
			}
			break;
		case STATE_ADMIN_MENU:
			if (key == '1')
			{
				if (access_log_count == 0)
				{
					LCD_ShowMessage("HIEN TAI CHUA CO", "NHAT KY RA VAO", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 1500);
					Admin_ShowSystemMenu();
				}
				else
				{
					current_system_state = STATE_LOG_VIEW;
					if (next_log_write_index == 0) current_displayed_log_index = access_log_count - 1;
					else current_displayed_log_index = next_log_write_index - 1;
					AccessLog_Show(current_displayed_log_index);
				}
				break;
			}
			else if (key == '2')
			{
				Admin_ShowPasswordInput();
				break;
			}
			else if (key == '3')
			{
				current_system_state = STATE_ADMIN_DELETE_ALL_CARDS;
				Auth_ResetInput();
				LCD_ShowMessage("XOA THE USER?", "*:NO       #:YES", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
				break;
			}
			else if (key == '4')
			{
				current_system_state = STATE_ADMIN_DELETE_ALL_FINGERPRINTS;
				Auth_ResetInput();
				LCD_ShowMessage("XOA VT USER?", "*:NO       #:YES", LCD_ALIGNMENT_LEFT, LCD_BEEP_NONE, 0);
				break;
			}
			else if (key == '*')
			{
				HomeScreen();
				break;
			}
			break;
		}
	}
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */
  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */
  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */
  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 63;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 19999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 2000;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */
  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */
  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 63;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */
  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */
  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */
  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 57600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_E_GPIO_Port, LCD_E_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, COL_1_Pin|COL_2_Pin|BUZZER_Pin|LED_OPEN_Pin
                          |LED_LOCK_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, COL_3_Pin|COL_4_Pin|SDA_Pin|LCD_D4_Pin
                          |LCD_D5_Pin|LCD_D6_Pin|LCD_D7_Pin|TRIG_Pin
                          |LCD_RS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LCD_E_Pin */
  GPIO_InitStruct.Pin = LCD_E_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_E_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : TRANSCEIVER_A_Pin TRANSCEIVER_B_Pin */
  GPIO_InitStruct.Pin = TRANSCEIVER_A_Pin|TRANSCEIVER_B_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : ROW_1_Pin ROW_2_Pin ROW_3_Pin ROW_4_Pin
                           ECHO_Pin */
  GPIO_InitStruct.Pin = ROW_1_Pin|ROW_2_Pin|ROW_3_Pin|ROW_4_Pin
                          |ECHO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : COL_1_Pin COL_2_Pin BUZZER_Pin LED_OPEN_Pin
                           LED_LOCK_Pin */
  GPIO_InitStruct.Pin = COL_1_Pin|COL_2_Pin|BUZZER_Pin|LED_OPEN_Pin
                          |LED_LOCK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : COL_3_Pin COL_4_Pin SDA_Pin LCD_D4_Pin
                           LCD_D5_Pin LCD_D6_Pin LCD_D7_Pin TRIG_Pin
                           LCD_RS_Pin */
  GPIO_InitStruct.Pin = COL_3_Pin|COL_4_Pin|SDA_Pin|LCD_D4_Pin
                          |LCD_D5_Pin|LCD_D6_Pin|LCD_D7_Pin|TRIG_Pin
                          |LCD_RS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
// Nhận từng byte UART1 từ ESP32, ghép thành dòng và phân loại lệnh hoàn chỉnh
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART1)
	{
		if (esp_received_byte == '\n')
		{
			esp_receive_buffer[esp_receive_buffer_index] = '\0';
			if (strncmp(esp_receive_buffer, "TIME:", 5) == 0)
			{
				strncpy(current_time, esp_receive_buffer + 5,sizeof(current_time) - 1);
				current_time[sizeof(current_time) - 1] = '\0';
			}
			else if (strncmp(esp_receive_buffer, "DATE:", 5) == 0)
			{
				strncpy(current_date, esp_receive_buffer + 5,sizeof(current_date) - 1);
				current_date[sizeof(current_date) - 1] = '\0';
			}
			else if (strcmp(esp_receive_buffer, "OPEN") == 0) is_esp_door_open_requested = 1;
			else if (strcmp(esp_receive_buffer, "RESET") == 0) is_esp_system_reset_requested = 1;
			else if (strcmp(esp_receive_buffer, "GET_LOG") == 0) is_esp_access_log_requested = 1;
			else if (strncmp(esp_receive_buffer, "USER_", 5) == 0)
			{
			    if (!is_esp_user_command_ready)
			    {
			        strncpy(esp_user_command, esp_receive_buffer, sizeof(esp_user_command) - 1U);
			        esp_user_command[sizeof(esp_user_command) - 1U] = '\0';
			        is_esp_user_command_ready = 1U;
			    }
			}
			esp_receive_buffer_index = 0;
			memset(esp_receive_buffer, 0, sizeof(esp_receive_buffer));
		}
		else if (esp_received_byte != '\r')
		{
			if (esp_receive_buffer_index < sizeof(esp_receive_buffer) - 1) esp_receive_buffer[esp_receive_buffer_index++] = esp_received_byte;
			else esp_receive_buffer_index = 0;
		}
		HAL_UART_Receive_IT(&huart1, &esp_received_byte, 1);
	}
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
