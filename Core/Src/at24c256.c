#include "AT24C256.h"
#include <stddef.h>
#include <string.h>

#define EEPROM_START_ADDR 0x0000U

static uint8_t AT24_WriteBuffer(uint16_t address,const uint8_t *data,uint16_t size)
{
    if(data == NULL || size == 0U) return 0;
    if((uint32_t)address + size > AT24_TOTAL_SIZE) return 0;

    while(size > 0U)
    {
        uint16_t page_offset = address % AT24_PAGE_SIZE;
        uint16_t page_space = AT24_PAGE_SIZE - page_offset;
        uint16_t write_size = size < page_space ? size : page_space;

        if(HAL_I2C_Mem_Write(&hi2c2,AT24_I2C_ADDRESS,address,I2C_MEMADD_SIZE_16BIT,(uint8_t*)data,write_size,AT24_TIMEOUT) != HAL_OK) return 0;
        if(HAL_I2C_IsDeviceReady(&hi2c2,AT24_I2C_ADDRESS,20,10) != HAL_OK) return 0;

        address += write_size;
        data += write_size;
        size -= write_size;
    }

    return 1;
}

static uint8_t AT24_ReadBuffer(uint16_t address,uint8_t *data,uint16_t size)
{
    if(data == NULL || size == 0U) return 0;
    if((uint32_t)address + size > AT24_TOTAL_SIZE) return 0;

    while(size > 0U)
    {
        uint16_t read_size = size > AT24_PAGE_SIZE ? AT24_PAGE_SIZE : size;

        if(HAL_I2C_Mem_Read(&hi2c2,AT24_I2C_ADDRESS,address,I2C_MEMADD_SIZE_16BIT,data,read_size,AT24_TIMEOUT) != HAL_OK) return 0;

        address += read_size;
        data += read_size;
        size -= read_size;
    }

    return 1;
}

uint8_t SaveDataToAT24C256(uint8_t *data,uint16_t size)
{
    return AT24_WriteBuffer(EEPROM_START_ADDR,data,size);
}

uint8_t LoadDataFromAT24C256(uint8_t *data,uint16_t size)
{
    return AT24_ReadBuffer(EEPROM_START_ADDR,data,size);
}

uint8_t Storage_SaveUserProfile(uint8_t index,const UserProfileData_t *user)
{
    uint16_t address;

    if(index >= MAX_USERS || user == NULL) return 0;

    myConfig.users[index] = *user;
    address = (uint16_t)(offsetof(ConfigData_t,users) + index * sizeof(UserProfileData_t));

    return AT24_WriteBuffer(address,(const uint8_t*)&myConfig.users[index],sizeof(UserProfileData_t));
}

uint8_t Storage_SaveUserCard(uint8_t index,const UserCardData_t *card)
{
    uint16_t address;

    if(index >= MAX_USERS || card == NULL) return 0;

    myConfig.user_cards[index] = *card;
    address = (uint16_t)(offsetof(ConfigData_t,user_cards) + index * sizeof(UserCardData_t));

    return AT24_WriteBuffer(address,(const uint8_t*)&myConfig.user_cards[index],sizeof(UserCardData_t));
}

uint8_t Storage_SaveUserCount(uint8_t existing_user_count)
{
    uint16_t address = (uint16_t)offsetof(ConfigData_t,existing_user_count);

    myConfig.existing_user_count = existing_user_count;

    return AT24_WriteBuffer(address,&myConfig.existing_user_count,sizeof(myConfig.existing_user_count));
}

uint8_t Storage_SaveUserAndCard(uint8_t index,const UserProfileData_t *user,const UserCardData_t *card)
{
    if(!Storage_SaveUserProfile(index,user)) return 0;
    if(!Storage_SaveUserCard(index,card)) return 0;

    return 1;
}

uint8_t Storage_SaveAccessLog(uint8_t log_index,const AccessLog_t *log)
{
    uint16_t address;

    if(log_index >= MAX_ACCESS_LOG || log == NULL) return 0;

    myConfig.access_logs[log_index] = *log;
    address = (uint16_t)(offsetof(ConfigData_t,access_logs) + log_index * sizeof(AccessLog_t));

    return AT24_WriteBuffer(address,(const uint8_t*)&myConfig.access_logs[log_index],sizeof(AccessLog_t));
}

uint8_t Storage_SaveAccessLogMeta(uint8_t log_count,uint8_t next_log_index)
{
    uint16_t address = (uint16_t)offsetof(ConfigData_t,access_log_count);
    uint8_t meta[2];

    if(log_count > MAX_ACCESS_LOG) return 0;
    if(next_log_index >= MAX_ACCESS_LOG) return 0;

    myConfig.access_log_count = log_count;
    myConfig.access_log_index = next_log_index;

    meta[0] = log_count;
    meta[1] = next_log_index;

    return AT24_WriteBuffer(address,meta,sizeof(meta));
}

uint8_t Storage_SaveAccessEvent(uint8_t user_index,const UserProfileData_t *user,uint8_t log_index,const AccessLog_t *log,uint8_t log_count,uint8_t next_log_index)
{
    if(!Storage_SaveUserProfile(user_index,user)) return 0;
    if(!Storage_SaveAccessLog(log_index,log)) return 0;
    if(!Storage_SaveAccessLogMeta(log_count,next_log_index)) return 0;

    return 1;
}

uint8_t Storage_SaveAdminPassword(const char *password)
{
    uint16_t address = (uint16_t)offsetof(ConfigData_t,admin_password);

    if(password == NULL) return 0;

    memset(myConfig.admin_password,0,sizeof(myConfig.admin_password));
    strncpy(myConfig.admin_password,password,sizeof(myConfig.admin_password) - 1);

    return AT24_WriteBuffer(address,(const uint8_t*)myConfig.admin_password,sizeof(myConfig.admin_password));
}

uint8_t Storage_SaveAllUserCards(const UserCardData_t *cards)
{
    uint16_t address = (uint16_t)offsetof(ConfigData_t,user_cards);

    if(cards == NULL) return 0;

    memcpy(myConfig.user_cards,cards,sizeof(myConfig.user_cards));

    return AT24_WriteBuffer(address,(const uint8_t*)myConfig.user_cards,sizeof(myConfig.user_cards));
}

ConfigData_t myConfig;
