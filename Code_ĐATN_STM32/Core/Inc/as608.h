#ifndef __AS608_H
#define __AS608_H

#include "main.h"
#include <string.h>
#include <stdio.h>

#define AS608_OK              0x00
#define AS608_PACKET_ERROR    0x01
#define AS608_NO_FINGER       0x02
#define AS608_IMAGE_FAIL      0x03
#define AS608_FEATURE_FAIL    0x06
#define AS608_NO_MATCH        0x08
#define AS608_NOT_FOUND       0x09
#define AS608_ENROLL_FAIL     0x0A
#define AS608_BAD_LOCATION    0x0B
#define AS608_LOAD_FAIL       0x0C
#define AS608_DELETE_FAIL     0x10
#define AS608_TIMEOUT         0xFF


typedef struct
{
    UART_HandleTypeDef *huart;
    uint32_t address;
    uint32_t password;
} AS608_t;

uint8_t AS608_CheckPassword(AS608_t *dev);
uint8_t AS608_GetImage(AS608_t *dev);
uint8_t AS608_Image2Tz(AS608_t *dev, uint8_t buffer_id);
uint8_t AS608_CreateModel(AS608_t *dev);
uint8_t AS608_StoreModel(AS608_t *dev, uint16_t id);
uint8_t AS608_Search(AS608_t *dev, uint8_t buffer_id, uint16_t *finger_id, uint16_t *score);
uint8_t AS608_Delete(AS608_t *dev, uint16_t id);
uint8_t AS608_LoadModel(AS608_t *dev, uint16_t id);
uint8_t AS608_TemplateExists(AS608_t *dev, uint16_t id, uint8_t *exists);

#endif
