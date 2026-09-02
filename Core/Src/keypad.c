#include"keypad.h"

char key_map[4][4] =
{
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};
char KeyPad_GetKey()	// hàm lấy dữ liệu keypad
{
    GPIO_TypeDef *Col_Port[] = {C1_PORT, C2_PORT, C3_PORT, C4_PORT};
    uint16_t Col_Pin[] = {C1_PIN, C2_PIN, C3_PIN, C4_PIN};
    GPIO_TypeDef *Row_Port[] = {A_PORT, B_PORT, C_PORT, D_PORT};
    uint16_t Row_Pin[] = {A_PIN, B_PIN, C_PIN, D_PIN};
    // Quét từng cột (đặt cột đó low, các cột còn lại high)
    for (int col = 0; col < 4; col++)
    {
        // đặt tất cả các cột high
        for (int i = 0; i < 4; i++)
        {
            HAL_GPIO_WritePin(Col_Port[i], Col_Pin[i], GPIO_PIN_SET);
        }
        // đặt cột hiện tại low
        HAL_GPIO_WritePin(Col_Port[col], Col_Pin[col], GPIO_PIN_RESET);
        HAL_Delay(8);
        if (HAL_GPIO_ReadPin(Row_Port[0], Row_Pin[0]) == GPIO_PIN_RESET) return key_map[0][col];
        if (HAL_GPIO_ReadPin(Row_Port[1], Row_Pin[1]) == GPIO_PIN_RESET) return key_map[1][col];
        if (HAL_GPIO_ReadPin(Row_Port[2], Row_Pin[2]) == GPIO_PIN_RESET) return key_map[2][col];
        if (HAL_GPIO_ReadPin(Row_Port[3], Row_Pin[3]) == GPIO_PIN_RESET) return key_map[3][col];
    }
    return 0;
}
