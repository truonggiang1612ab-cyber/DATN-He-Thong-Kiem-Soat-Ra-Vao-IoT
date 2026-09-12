#include "keypad.h"

#define KEYPAD_VALIDATE_COUNT 4U
#define KEYPAD_SETTLE_MS 1U
#define KEYPAD_VALIDATE_DELAY_MS 3U
#define KEYPAD_RELEASE_DELAY_MS 10U

char key_map[4][4] =
{
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};
static GPIO_TypeDef *Col_Port[4] =
{
    C1_PORT,
    C2_PORT,
    C3_PORT,
    C4_PORT
};
static uint16_t Col_Pin[4] =
{
    C1_PIN,
    C2_PIN,
    C3_PIN,
    C4_PIN
};
static GPIO_TypeDef *Row_Port[4] =
{
    A_PORT,
    B_PORT,
    C_PORT,
    D_PORT
};
static uint16_t Row_Pin[4] =
{
    A_PIN,
    B_PIN,
    C_PIN,
    D_PIN
};
static void KeyPad_AllColHigh(void)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        HAL_GPIO_WritePin(Col_Port[i], Col_Pin[i], GPIO_PIN_SET);
    }
}
static uint8_t KeyPad_AllRowHigh(void)
{
    for (uint8_t row = 0; row < 4; row++)
    {
        if (HAL_GPIO_ReadPin(Row_Port[row], Row_Pin[row]) == GPIO_PIN_RESET)
        {
            return 0;
        }
    }
    return 1;
}
static uint8_t KeyPad_ScanOnce(uint8_t *found_row, uint8_t *found_col)
{
    uint8_t count = 0;
    for (uint8_t col = 0; col < 4; col++)
    {
        KeyPad_AllColHigh();
        HAL_GPIO_WritePin(Col_Port[col], Col_Pin[col], GPIO_PIN_RESET);
        HAL_Delay(KEYPAD_SETTLE_MS);
        for (uint8_t row = 0; row < 4; row++)
        {
            if (HAL_GPIO_ReadPin(Row_Port[row], Row_Pin[row]) == GPIO_PIN_RESET)
            {
                count++;
                if (count == 1)
                {
                    *found_row = row;
                    *found_col = col;
                }
                if (count > 1)
                {
                    KeyPad_AllColHigh();
                    return 0;
                }
            }
        }
    }
    KeyPad_AllColHigh();
    if (count == 1)
    {
        return 1;
    }
    return 0;
}
static uint8_t KeyPad_ValidatePress(uint8_t row, uint8_t col)
{
    for (uint8_t check = 0; check < KEYPAD_VALIDATE_COUNT; check++)
    {
        KeyPad_AllColHigh();
        HAL_Delay(KEYPAD_SETTLE_MS);
        if (KeyPad_AllRowHigh() == 0)
        {
            return 0;
        }
        HAL_GPIO_WritePin(Col_Port[col], Col_Pin[col], GPIO_PIN_RESET);
        HAL_Delay(KEYPAD_SETTLE_MS);
        if (HAL_GPIO_ReadPin(Row_Port[row], Row_Pin[row]) != GPIO_PIN_RESET)
        {
            KeyPad_AllColHigh();
            return 0;
        }
        for (uint8_t other_row = 0; other_row < 4; other_row++)
        {
            if (other_row == row)
            {
                continue;
            }
            if (HAL_GPIO_ReadPin(Row_Port[other_row], Row_Pin[other_row]) == GPIO_PIN_RESET)
            {
                KeyPad_AllColHigh();
                return 0;
            }
        }
        KeyPad_AllColHigh();
        HAL_Delay(KEYPAD_VALIDATE_DELAY_MS);
    }
    return 1;
}
char KeyPad_GetKey(void)
{
    static uint8_t key_active = 0;
    static uint8_t saved_row = 0;
    static uint8_t saved_col = 0;
    uint8_t row = 0;
    uint8_t col = 0;
    if (key_active == 1)
    {
        KeyPad_AllColHigh();
        HAL_GPIO_WritePin(Col_Port[saved_col], Col_Pin[saved_col], GPIO_PIN_RESET);
        HAL_Delay(KEYPAD_SETTLE_MS);
        if (HAL_GPIO_ReadPin(Row_Port[saved_row], Row_Pin[saved_row]) == GPIO_PIN_SET)
        {
            HAL_Delay(KEYPAD_RELEASE_DELAY_MS);
            if (HAL_GPIO_ReadPin(Row_Port[saved_row], Row_Pin[saved_row]) == GPIO_PIN_SET)
            {
                key_active = 0;
            }
        }
        KeyPad_AllColHigh();
        return 0;
    }
    if (KeyPad_ScanOnce(&row, &col) == 0)
    {
        return 0;
    }
    if (KeyPad_ValidatePress(row, col) == 0)
    {
        return 0;
    }
    saved_row = row;
    saved_col = col;
    key_active = 1;
    return key_map[row][col];
}
