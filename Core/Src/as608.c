#include "as608.h"

static uint8_t AS608_SendCommand(AS608_t *dev, uint8_t *cmd, uint16_t cmd_len, uint8_t *resp, uint16_t resp_len, uint32_t timeout)
{
	uint8_t packet[64];
	uint16_t index = 0;
	uint16_t length = cmd_len + 2;
	uint16_t checksum = 0;
	packet[index++] = 0xEF;
	packet[index++] = 0x01;
	packet[index++] = (dev->address >> 24) & 0xFF;
	packet[index++] = (dev->address >> 16) & 0xFF;
	packet[index++] = (dev->address >> 8) & 0xFF;
	packet[index++] = dev->address & 0xFF;
	packet[index++] = 0x01;
	packet[index++] = (length >> 8) & 0xFF;
	packet[index++] = length & 0xFF;
	checksum = 0x01 + packet[7] + packet[8];
	for (uint16_t i = 0; i < cmd_len; i++)
	{
		packet[index++] = cmd[i];
		checksum += cmd[i];
	}
	packet[index++] = (checksum >> 8) & 0xFF;
	packet[index++] = checksum & 0xFF;
	if (HAL_UART_Transmit(dev->huart, packet, index, timeout) != HAL_OK) return AS608_TIMEOUT;
	if (HAL_UART_Receive(dev->huart, resp, resp_len, timeout) != HAL_OK) return AS608_TIMEOUT;
	if (resp[0] != 0xEF || resp[1] != 0x01) return AS608_PACKET_ERROR;
	return resp[9];
}
uint8_t AS608_CheckPassword(AS608_t *dev)
{
    uint8_t cmd[5];
    uint8_t resp[12];
    cmd[0] = 0x13;
    cmd[1] = (dev->password >> 24) & 0xFF;
    cmd[2] = (dev->password >> 16) & 0xFF;
    cmd[3] = (dev->password >> 8) & 0xFF;
    cmd[4] = dev->password & 0xFF;
    return AS608_SendCommand(dev, cmd, 5, resp, 12, 500);
}
uint8_t AS608_GetImage(AS608_t *dev)
{
    uint8_t cmd[1] = {0x01};
    uint8_t resp[12];
    return AS608_SendCommand(dev, cmd, 1, resp, 12, 1000);
}
uint8_t AS608_Image2Tz(AS608_t *dev, uint8_t buffer_id)
{
    uint8_t cmd[2] = {0x02, buffer_id};
    uint8_t resp[12];
    return AS608_SendCommand(dev, cmd, 2, resp, 12, 1000);
}
uint8_t AS608_CreateModel(AS608_t *dev)
{
    uint8_t cmd[1] = {0x05};
    uint8_t resp[12];
    return AS608_SendCommand(dev, cmd, 1, resp, 12, 1000);
}

uint8_t AS608_StoreModel(AS608_t *dev, uint16_t id)
{
    uint8_t cmd[4];
    cmd[0] = 0x06;
    cmd[1] = 0x01;
    cmd[2] = (id >> 8) & 0xFF;
    cmd[3] = id & 0xFF;
    uint8_t resp[12];
    return AS608_SendCommand(dev, cmd, 4, resp, 12, 1000);
}
uint8_t AS608_Search(AS608_t *dev, uint8_t buffer_id, uint16_t *finger_id, uint16_t *score)
{
    uint8_t cmd[6];
    uint8_t resp[16];
    cmd[0] = 0x04;
    cmd[1] = buffer_id;
    cmd[2] = 0x00;
    cmd[3] = 0x00;   // start = 0
    cmd[4] = 0x01;
    cmd[5] = 0x2C;   //  0 -> 299
    uint8_t result = AS608_SendCommand(dev, cmd, 6, resp, 16, 1000);
    if(result == AS608_OK)
    {
        *finger_id = ((uint16_t)resp[10] << 8) | resp[11];
        *score     = ((uint16_t)resp[12] << 8) | resp[13];
    }
    return result;
}
uint8_t AS608_Delete(AS608_t *dev, uint16_t id)
{
    uint8_t cmd[5];
    uint8_t resp[12];
    cmd[0] = 0x0C;
    cmd[1] = (id >> 8) & 0xFF;
    cmd[2] = id & 0xFF;
    cmd[3] = 0x00;
    cmd[4] = 0x01;
    return AS608_SendCommand(dev, cmd, 5, resp, 12, 1000);
}
uint8_t AS608_LoadModel(AS608_t *dev, uint16_t id)
{
    uint8_t cmd[4];
    uint8_t resp[12];
    cmd[0] = 0x07;
    cmd[1] = 0x01;
    cmd[2] = (id >> 8) & 0xFF;
    cmd[3] = id & 0xFF;
    return AS608_SendCommand(dev, cmd, 4, resp, 12, 1000);
}
uint8_t AS608_TemplateExists(AS608_t *dev, uint16_t id, uint8_t *exists)
{
	uint8_t result;
	if (exists == NULL) return AS608_PACKET_ERROR;
	*exists = 0U;
	result = AS608_LoadModel(dev, id);
	if (result == AS608_OK)
	{
		*exists = 1U;
		return AS608_OK;
	}
	if (result == AS608_LOAD_FAIL) return AS608_OK;
	return result;
}
