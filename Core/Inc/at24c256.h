#ifndef AT24C256_H_
#define AT24C256_H_

#include "stm32f1xx_hal.h"
#include "RC522.h"

#define CONFIG_MAGIC               0xB6B6C1C3U
#define MAX_ACCESS_LOG             10U
#define MAX_USERS                  150U
#define FINGER_EMERGENCY_OFFSET    150U
#define USER_PASSWORD_LENGTH       6U
#define ACTIVATION_CODE_LENGTH     6U
#define USER_NAME_MAX_LENGTH       12U

#define AT24_I2C_ADDRESS           (0x50 << 1)
#define AT24_PAGE_SIZE             64U
#define AT24_TOTAL_SIZE            32768U
#define AT24_TIMEOUT               1000U

extern I2C_HandleTypeDef hi2c2;

typedef uint8_t UserStatus_t;
enum
{
    USER_STATUS_EMPTY,
    USER_STATUS_PENDING,
    USER_STATUS_ACTIVE
};

typedef struct
{
    char name[USER_NAME_MAX_LENGTH + 1U];
    char action[8];
    char time[9];
    char date[11];
    uint8_t floor;
    uint16_t room;
} __attribute__((packed)) AccessLog_t;

typedef struct
{
    RFID_UID_t normal_card;
    RFID_UID_t emergency_card;
    uint8_t normal_card_exists;
    uint8_t emergency_card_exists;
} __attribute__((packed)) UserCardData_t;

typedef struct
{
    char name[USER_NAME_MAX_LENGTH + 1U];
    char user_password[USER_PASSWORD_LENGTH + 1U];
    char duress_password[USER_PASSWORD_LENGTH + 1U];
    char activation_code[ACTIVATION_CODE_LENGTH + 1U];
    uint16_t finger_id;
    uint16_t duress_finger_id;
    uint16_t version;
    uint16_t room;
    uint8_t floor;
    UserStatus_t status;
    uint8_t access_state;
} __attribute__((packed)) UserProfileData_t;

typedef struct
{
    UserProfileData_t users[MAX_USERS];
    uint8_t existing_user_count;
    AccessLog_t access_logs[MAX_ACCESS_LOG];
    uint8_t access_log_count;
    uint8_t access_log_index;
    uint32_t magic;
    char admin_password[8];
    UserCardData_t user_cards[MAX_USERS];
} __attribute__((packed)) ConfigData_t;

typedef char ConfigData_Fits_AT24C256[(sizeof(ConfigData_t) <= AT24_TOTAL_SIZE) ? 1 : -1];

extern ConfigData_t myConfig;

uint8_t SaveDataToAT24C256(uint8_t *data,uint16_t size);
uint8_t LoadDataFromAT24C256(uint8_t *data,uint16_t size);
uint8_t Storage_SaveUserProfile(uint8_t index,const UserProfileData_t *user);
uint8_t Storage_SaveUserCard(uint8_t index,const UserCardData_t *card);
uint8_t Storage_SaveUserCount(uint8_t existing_user_count);
uint8_t Storage_SaveUserAndCard(uint8_t index,const UserProfileData_t *user,const UserCardData_t *card);
uint8_t Storage_SaveAccessLog(uint8_t log_index,const AccessLog_t *log);
uint8_t Storage_SaveAccessLogMeta(uint8_t log_count,uint8_t next_log_index);
uint8_t Storage_SaveAccessEvent(uint8_t user_index,const UserProfileData_t *user,uint8_t log_index,const AccessLog_t *log,uint8_t log_count,uint8_t next_log_index);
uint8_t Storage_SaveAdminPassword(const char *password);
uint8_t Storage_SaveAllUserCards(const UserCardData_t *cards);

#endif
