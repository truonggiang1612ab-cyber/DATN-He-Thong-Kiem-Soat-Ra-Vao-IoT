#ifndef LCD_H
#define LCD_H

#include "main.h"

#define LCD_RS_PORT GPIOB
#define LCD_RS_PIN  GPIO_PIN_9
#define LCD_E_PORT  GPIOC
#define LCD_E_PIN   GPIO_PIN_13
#define LCD_D4_PORT GPIOB
#define LCD_D4_PIN  GPIO_PIN_4
#define LCD_D5_PORT GPIOB
#define LCD_D5_PIN  GPIO_PIN_5
#define LCD_D6_PORT GPIOB
#define LCD_D6_PIN  GPIO_PIN_6
#define LCD_D7_PORT GPIOB
#define LCD_D7_PIN  GPIO_PIN_7
#define LCD_COMMAND 0
#define LCD_DATA 1
// Khai báo hàm
void HAL_Delay_us(uint32_t us);
void LCD_Write_4Bit(uint8_t nibble);
void LCD_Init(void);
void LCD_Send(uint8_t byte, uint8_t mode);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_Puts(char *str);

#endif
