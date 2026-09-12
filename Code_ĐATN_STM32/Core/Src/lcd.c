#include "lcd.h"

void HAL_Delay_us(uint32_t us)
{
    volatile uint32_t counter = 0;
    for(counter = 0; counter < us * 10; counter++)
    {
        __asm("NOP");
    }
}
void LCD_Write_4Bit(uint8_t nibble)	// Hàm gửi 4 bit dữ liệu/lệnh
{
    // Gán 4 bit dữ liệu vào các chân D4, D5, D6, D7
    HAL_GPIO_WritePin(LCD_D4_PORT, LCD_D4_PIN, (nibble & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D5_PORT, LCD_D5_PIN, (nibble & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D6_PORT, LCD_D6_PIN, (nibble & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D7_PORT, LCD_D7_PIN, (nibble & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_E_PORT, LCD_E_PIN, GPIO_PIN_SET);
    HAL_Delay_us(10);
    HAL_GPIO_WritePin(LCD_E_PORT, LCD_E_PIN, GPIO_PIN_RESET);
    HAL_Delay_us(50);
}
void LCD_Send(uint8_t byte, uint8_t mode)	// Hàm gửi lệnh
{
    // cấu hình RS (command/data) và RW (write)
    HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, (mode == LCD_DATA) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    // gửi 4 bit cao, sau đó gửi 4 bit thấp
    LCD_Write_4Bit(byte >> 4);
    LCD_Write_4Bit(byte);
    if (byte == 0x01 || byte == 0x02)	// Lệnh xóa màn hình hoặc lệnh return home
    {
        HAL_Delay(2);
    }
    else
    {
        HAL_Delay_us(100);
    }
}
void LCD_Init()
{
    HAL_Delay(50);
    // khởi tạo chế độ 4 bit
    HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_RESET);
    // gửi 0x03 ba lần (khởi tạo 8 bit mode)
    LCD_Write_4Bit(0x03);
    HAL_Delay(5);
    LCD_Write_4Bit(0x03);
    HAL_Delay(1);
    LCD_Write_4Bit(0x03);
    HAL_Delay(1);
    // chuyển sang 4 bit mode (gửi 0x02)
    LCD_Write_4Bit(0x02);
    HAL_Delay(1);
    // function set: 4 bit mode, 2 lines, 5x7 (0x28)
    LCD_Send(0x28, LCD_COMMAND);
    // display control: display on, cursor off, blink off (0x0C)
    LCD_Send(0x0C, LCD_COMMAND);
    // clear display (0x01)
    LCD_Send(0x01, LCD_COMMAND);
    // thiết lập chế độ nhập: tăng con tr�?, không dịch chuyển (0x06)
    LCD_Send(0x06, LCD_COMMAND);
}
void LCD_SetCursor(uint8_t row, uint8_t col)
{
    uint8_t address;
    if (row == 0) {
        address = 0x80 + col;
    } else {
        address = 0xC0 + col;
    }
    LCD_Send(address, LCD_COMMAND);
}
void LCD_Puts(char *str)
{
    while (*str)
    {
        LCD_Send(*str++, LCD_DATA);
    }
}
