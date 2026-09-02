#ifndef INC_KEYPAD_H_
#define INC_KEYPAD_H_

#include"main.h"

// keypad
#define A_PORT 	GPIOA
#define A_PIN  	GPIO_PIN_0
#define B_PORT 	GPIOA
#define B_PIN  	GPIO_PIN_1
#define C_PORT 	GPIOA
#define C_PIN  	GPIO_PIN_4
#define D_PORT 	GPIOA
#define D_PIN  	GPIO_PIN_5
#define C1_PORT GPIOA
#define C1_PIN  GPIO_PIN_6
#define C2_PORT GPIOA
#define C2_PIN  GPIO_PIN_7
#define C3_PORT GPIOB
#define C3_PIN  GPIO_PIN_0
#define C4_PORT GPIOB
#define C4_PIN  GPIO_PIN_1

char KeyPad_GetKey();

#endif /* INC_KEYPAD_H_ */
