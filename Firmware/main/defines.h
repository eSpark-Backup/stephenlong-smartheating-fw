#pragma once

#include <stdint.h>
#include <stddef.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <hal/i2c_types.h>

#define DEVICE_TEST

#ifdef __cplusplus
extern "C" {
#endif

#define COMPANY_NAME                "HERSCHEL"
#define COMPANY_ID                  0x0EC3

// Device Attributes for ThingsBoard - HC-1200-VP Standard Configuration
#define DEVICE_MODEL                "HC-1200-VP"
#define FIRMWARE_VERSION            "1.1.2"
#define HARDWARE_VERSION            "ESP32-C6"
#define RATED_VOLTAGE               "240V AC"
#define INSTALLATION_TYPE           "wall-mount"
#define LOW_ELEMENT_NAME            "Edge Element"
#define HIGH_ELEMENT_NAME           "Center Element"
#define LOW_ELEMENT_ID              "heating_edge"
#define HIGH_ELEMENT_ID             "heating_center"

#define IR_SENSOR_I2C_INTERFACE     I2C_NUM_0
#define IR_SENSOR_I2C_ADDR1         0x5A         // Default sensor slave address
#define IR_SENSOR_SDA_IO            GPIO_NUM_21
#define IR_SENSOR_SCL_IO            GPIO_NUM_22
#define MAX_TEMP_VAL                125.00f
#define MIN_TEMP_VAL                -40.00f
#define SENSOR_BOARD_RESTART_COUNT  10

// UI temperature range constants
#define UI_MIN_TEMP_CELSIUS         0.0f    // 0°C minimum for UI
#define UI_MAX_TEMP_CELSIUS         35.0f   // 35°C maximum for UI

#define MOTION_SENSOR_IO            GPIO_NUM_23
#define HEATER_HIGH_IO              GPIO_NUM_19
#define HEATER_LOW_IO               GPIO_NUM_18

#ifdef DEVICE_TEST
#define STATUS_LED_IO               GPIO_NUM_8
#else
#define STATUS_LED_IO               GPIO_NUM_11
#endif

// Minutes
#define MIN_TO_MS(x)                ((x) * 60 * 1000ULL)
#define MIN_TO_US(x)                ((x) * 60 * 1000000ULL)

// Seconds
#define SEC_TO_MS(x)                ((x) * 1000ULL)
#define SEC_TO_US(x)                ((x) * 1000000ULL)

// Milliseconds
#define MS_TO_US(x)                 ((x) * 1000ULL)
#define MS_TO_SEC(x)                ((x) / 1000ULL)
#define MS_TO_MIN(x)                ((x) / (60 * 1000ULL))

// Microseconds
#define US_TO_MS(x)                 ((x) / 1000ULL)
#define US_TO_SEC(x)                ((x) / 1000000ULL)
#define US_TO_MIN(x)                ((x) / (60 * 1000000ULL))


// Single press duration thresholds (in seconds)
#define BUTTON_BLE_PAIRING_MIN_DURATION     SEC_TO_US(5)    // 5-10s = BLE pairing mode
#define BUTTON_BLE_PAIRING_MAX_DURATION     SEC_TO_US(10)
#define BUTTON_FACTORY_RESET_MIN_DURATION   SEC_TO_US(10)   // 10-30s = Factory reset confirmation
#define BUTTON_FACTORY_RESET_MAX_DURATION   SEC_TO_US(30)
#define BUTTON_BYPASS_MODE_MIN_DURATION     SEC_TO_US(30)   // 30s+ = Bypass mode (technician only)

// Double-click detection (in milliseconds)
#define BUTTON_DOUBLE_CLICK_MAX_INTERVAL    MS_TO_US(500)   // 500ms max between clicks
#define BUTTON_DEBOUNCE_MS                  MS_TO_US(50)    // 50ms Debounce time

// Factory reset confirmation
#define FACTORY_RESET_CONFIRMATION_TIMEOUT  SEC_TO_US(5) // 5 seconds to confirm

// BLE pairing timeout (in milliseconds)
#define BLE_PAIRING_TIMEOUT         MIN_TO_US(1) //1 mins
#define SOFT_AP_TIMEOUT             MIN_TO_US(5) //5 mins

#define BUTTON_GPIO                 GPIO_NUM_2   // <-- Your GPIO pin
#define GND_SW                      GPIO_NUM_10  // Sensor board ground switch
#define GPIO_PIN_MASK(x)            (1ULL<<x)

#define AP_RECONN_ATTEMPTS          5

#define SSID_WIFI                   COMPANY_NAME "_000001"
#define PASS_WIFI                   "12345678"
#define WIFI_SSID_MAX_LEN           32
#define WIFI_PASS_MAX_LEN           64
#define CREDENTIAL_VALUE            128

#ifdef DEVICE_TEST
#define TEST_AP_SSID                "eSpark Wifi"
#define TEST_AP_PASS                "47356880"
#endif

#define DEVICE_TAG                  "APP"
#define STORAGE_TAG                 "_Storage"
#define GPIO_TAG                    "_GPIO"
#define WIFI_TAG                    "_SoftAP"
#define GATTS_TAG                   "_BLE"
#define ISR_TAG                     "_ISR"
#define HTTPS_TAG                   "_ThingsBoard"
#define LED_TAG                     "_LED_Strip"

#ifdef DEVICE_TEST
#define LD2410_UART_TX              GPIO_NUM_7
#define LD2410_UART_RX              GPIO_NUM_6
#else
#define LD2410_UART_TX              GPIO_NUM_17
#define LD2410_UART_RX              GPIO_NUM_16
#endif
#define LD2410_BAUD                 256000
#define LD2410_UART_PORT            UART_NUM_1


#define BLE_RESPONSE_SUCCESS        0xA1
#define BLE_RESPONSE_FAIL           0xA2

#define PROFILE_NUM                 2
#define PROFILE_PROV_APP_ID         0
#define PROFILE_UART_APP_ID         1

#define BLE_DEVICE_NAME             COMPANY_NAME "_000001"
#define GATTS_SERVICE_UUID          COMPANY_ID // Wifi Prov
#define GATTS_NUM_HANDLE_A          4
#define GATTS_NUM_HANDLE_B          16
#define GATTS_CHAR_VAL_LEN_MAX      0x40
#define PREPARE_BUF_MAX_SIZE        1024
#define CONFIG_SET_RAW_ADV_DATA     0
#define BLE_PAIR_ENCRYPTION_KEY     16


// These should not be longer than 16 characters?
#define NVS_NAMESPACE_DEVICEDATA        "device_data"
#define NVS_KEY_MODE                    "mode"
#define NVS_KEY_TARGET_TEMP             "target_temp"
#define NVS_KEY_DETECTION_DURATION      "detection_dur"
#define NVS_NAMESPACE_WIFIDATA          "wifi_storage"
#define NVS_KEY_SSID                    "ssid"
#define NVS_KEY_PASSWORD                "password"
#define NVS_NAMESPACE_DEVICE_HTTPS_DATA "https_dev_data"
#define NVS_KEY_ACCESS_TOKEN            "access_token"
#define NVS_NAMESPACE_ENERGYDATA        "energy_data"
#define NVS_KEY_ENERGY_CONSUMED         "energy_kwh"
#define NVS_KEY_HEATING_TIME            "heating_time"
#define NVS_KEY_LAST_UPDATE_TIME        "last_update"

// Energy measurement constants - HC-1200-VP Configuration
#define HEATING_LOW_WATTS              500.0f         // Watts for edge element (lower power)
#define HEATING_HIGH_WATTS             700.0f         // Watts for center element (higher power)
#define ENERGY_UPDATE_INTERVAL         SEC_TO_US(1)   // 1 second intervals for energy calculation

// Telemetry interval constants (shared between BLE and ThingsBoard)
#define NORMAL_TELEMETRY_TIME          30               // 30 seconds normal operation
#define PRESENCE_TELEMETRY_TIME        10               // 10 seconds when presence detected
#define IDLE_TELEMETRY_TIME            60               // 60 seconds increment when idle
#define BLE_STREAMING_PRESENCE_TIME    5                // 5 seconds when presence detected
#define BLE_STREAMING_NORMAL_TIME      30               // 30 seconds normal operation  
#define BLE_STREAMING_IDLE_TIME        (5*60)           // 5 minutes when idle
#define BLE_FIRST_MINUTE_TIME          5                // 5 seconds for first minute after BLE connection
#define BLE_FIRST_MINUTE_DURATION      SEC_TO_MS(60)    // 60 seconds in milliseconds
#define NO_ACTIVITY_TIME_MS            MIN_TO_MS(10)    // 10 minutes in milliseconds
#define MAX_IDLE_INTERVAL              (10*60)          // 10 minutes max interval


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
    BYPASS_MODE,    // Full power heating, no automatic control logic
    DEVICE_MODE_COUNT
}DEVICE_MODE;

typedef enum {
    NO_COMMAND = 0,
    POWER_CONTROL = 1,           // 0x01 - Power Control (was REBOOT)
    FACTORY_RESET = 2,           // 0x02 - Factory Reset
    READ_INTERNAL_TEMP = 3,      // 0x03 - Read Internal Temp (was 0x04)
    READ_IR_TEMPERATURES = 4,    // 0x04 - Read IR Temperatures (was READ_TEMP, enhanced format)
    SET_TARGET_TEMP = 5,         // 0x05 - Set Target Temperature (was 0x06)
    GET_TARGET_TEMP = 6,         // 0x06 - Get Target Temperature (NEW)
    SET_OPERATING_MODE = 7,      // 0x07 - Set Operating Mode (was CHANGE_DEVICE_MODE)
    GET_OPERATING_MODE = 8,      // 0x08 - Get Operating Mode (NEW)
    READ_RELAY_STATE = 9,        // 0x09 - Read Relay States and Power (was READ_RELAY_STATE, enhanced)
    SET_RELAY_STATE = 10,        // 0x0A - Set Relay States (was SET_RELAYS)
    CONTROL_SENSOR_BOARD = 11,   // 0x0B - Control Sensor Board (NEW)
    READ_PRESENCE_DATA = 12,     // 0x0C - Read Presence Data (was READ_PRESENCE, enhanced)
    READ_WIFI_STATUS = 13,       // 0x0D - Read Wi-Fi Status (NEW)
    READ_SYSTEM_DIAGNOSTICS = 14,// 0x0E - Read System Diagnostics (NEW)
    READ_ENERGY_METRICS = 15,    // 0x0F - Read Energy Metrics (NEW)
    CONTROL_FRONT_LED = 16,      // 0x10 - Control Front LED (NEW)
    READ_DEVICE_INFO = 17,       // 0x11 - Read Device Information (NEW)
    WIFI_SCAN = 18 ,             // 0x12 - Scans Wifi SSIDS and sends JSON on indicate
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
    uint8_t duration_val;
    char tokenKey[CREDENTIAL_VALUE];
} device_data_t;

typedef struct
{
    bool BLEPairFlag;           // < 3s press
    bool bypassModeFlag;        // 3-8s press
    bool factoryResetFlag;      // 8s+ press
    bool softAPFlag;            // double-click during BLE mode
    bool factoryResetConfirmFlag; // second press during confirmation
}ButtonFlags;

typedef struct
{
    float energy_consumed_kwh;      // Total energy consumed in kWh
    uint32_t heating_time_seconds;  // Total time with heating active (seconds)
    int64_t last_update_time;       // Last update timestamp (microseconds)
    bool is_initialized;            // Flag to track if energy system is initialized
} energy_tracking_t;


const char* get_enum_name(ENUM_TYPE type, int value);
// void set_device_status(uint8_t op, int bitMask);
// void set_led_effect(led_effect_t effect , COLORS colorSelection , EFFECT_SPEED effectSPEED);
// led_effect_t get_current_led_effect(void);
void execute_command(void);
bool is_button_led_feedback_active(void);

void set_led_status_flag(const char* flag_name, bool value);
// led_status_flags_t* get_led_status_flags(void);

void sensor_board_init(void);
void set_sensor_board_power(bool enable);

#ifdef __cplusplus
}
#endif
