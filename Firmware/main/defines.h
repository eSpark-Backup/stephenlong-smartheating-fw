#pragma once

#include <stdint.h>
#include <stddef.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <hal/i2c_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COMPANY_NAME                "HERSCHEL"
#define COMPANY_ID                  0x0EC3

#define IR_SENSOR_I2C_INTERFACE     I2C_NUM_0
#define IR_SENSOR_I2C_ADDR1         0x5A         // Default sensor slave address
#define IR_SENSOR_SDA_IO            21
#define IR_SENSOR_SCL_IO            22

#define MOTION_SENSOR_IO            GPIO_NUM_23
#define HEATER_HIGH_IO              19
#define HEATER_LOW_IO               18

#define STATUS_LED_IO               8

#define BUTTON_PAIRMODE_THRESHOLD   5
#define BUTTON_FACTORY_RESET        30            // seconds
#define BUTTON_DEBOUNCE_MS          50
// #define BUTTON_GPIO                 4               // <-- Your GPIO pin
#define BUTTON_GPIO                 GPIO_NUM_4               // <-- Your GPIO pin
#define GPIO_PIN_MASK(x)            (1ULL<<x)

#define AP_RECONN_ATTEMPTS          5

#define SSID_WIFI                   COMPANY_NAME "_000001"
#define PASS_WIFI                   "12345678"
#define WIFI_SSID_MAX_LEN           32
#define WIFI_PASS_MAX_LEN           64

#define DEVICE_TAG                  "APP"    
#define GPIO_TAG                    "GPIO"
#define WIFI_TAG                    "_SoftAP"
#define GATTS_TAG                   "_BLE"
#define ISR_TAG                     "_ISR"

#define BLE_RESPONSE_SUCCESS        0xA1
#define BLE_RESPONSE_FAIL           0xA2

#define PROFILE_NUM                 2
#define PROFILE_PROV_APP_ID         0
#define PROFILE_UART_APP_ID         1

#define BLE_DEVICE_NAME             COMPANY_NAME "_000001"
#define GATTS_SERVICE_UUID          COMPANY_ID // Wifi Prov
#define GATTS_NUM_HANDLE_A          4
#define GATTS_NUM_HANDLE_B          8
#define GATTS_CHAR_VAL_LEN_MAX      0x40
#define PREPARE_BUF_MAX_SIZE        1024
#define CONFIG_SET_RAW_ADV_DATA     0

#define NVS_NAMESPACE_DEVICEDATA   "device_data"
#define NVS_NAMESPACE_WIFIDATA     "wifi_storage"
#define NVS_KEY_SSID               "ssid"
#define NVS_KEY_PASSWORD           "password"
#define NVS_KEY_MODE               "mode"
#define NVS_KEY_TARGET_TEMP        "target_temp"

typedef enum {
    NORMAL_OP = 0x01,
    PAIRING = 0x02,
    FIRMWARE_UPDATE = 0x04,
    ERROR = 0x10,
    ERROR_IR_SENSOR = 0x20,
    ERROR_PROVISIONING = 0x40,
    ERROR_HEATER = 0x80,
    OFF_STATE = 0xFFFF,
    // DEVICE_STATUS_COUNT
}DEVICE_STATUS;

typedef enum
{
    HEATER_OFF = 0,
    HEATER_LOW,
    HEATER_MID,
    HEATER_HIGH,
}HEATER_STATE;

typedef enum
{
    MANUAL_MODE = 1,
    AUTO_MODE,
    ECO_MODE,
    DEVICE_MODE_COUNT
}DEVICE_MODE;

typedef enum {
    NO_COMMAND = 0,
    REBOOT,
    FACTORY_RESET,
    WIFI_UPDATE,
    READ_INTERNAL_TEMP,
    READ_TEMP,
    SET_TARGET_TEMP,
    READ_RELAY_STATE,
    SET_RELAYS,
    CHANGE_DEVICE_MODE,
    READ_PRESENCE,
    MAX_BLE_CMDS
}BLE_COMMANDS;

typedef enum {
    ENUM_DEVICE_STATUS,
    ENUM_HEATER_STATE,
    ENUM_DEVICE_MODE,
    ENUM_BLE_COMMANDS
} ENUM_TYPE;

typedef struct
{
    uint8_t *prepare_buf;
    int prepare_len;
} prepare_type_env_t;

typedef struct
{
    DEVICE_MODE mode;
    float target_temp;
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PASS_MAX_LEN];
} device_data_t;

const char* get_enum_name(ENUM_TYPE type, int value);
void set_device_status(uint8_t op, int bitMask);
void execute_command(void);


#ifdef __cplusplus
}
#endif