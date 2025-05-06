#include <string.h>
// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>
// #include <esp_log.h>
// #include "app_prov.h"
#include <esp_wifi.h>
// #include <lwip/err.h>
// #include <lwip/sys.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <esp_system.h>
// #include <esp_http_server.h>

#include "driver/gpio.h"

// #include "esp_bt.h"
// #include "cJSON.h"

// #include "esp_gap_ble_api.h"
// #include "esp_gatts_api.h"
// #include "esp_bt_defs.h"
// #include "esp_bt_main.h"
// #include "esp_bt_device.h"
// #include "esp_gatt_common_api.h"

#include "defines.h"
#include "app.h"
#include "MLX90614_interface.h"
#include "Heater_interface.h"
#include "Status_led.h"
#include "Comm_Interface.h"



static int64_t press_start_time = 0;
static void IRAM_ATTR button_isr_handler(void *arg)
{
    static int64_t last_isr_time = 0;
    int64_t now = 0;// = esp_timer_get_time(); // microseconds

    // Debounce
    if (now - last_isr_time < BUTTON_DEBOUNCE_MS * 1000) return;
    last_isr_time = now;

    if (gpio_get_level(BUTTON_GPIO) == 0) {
        // Button pressed (falling edge)
        press_start_time = now;
    } else {
        // Button released (rising edge)
        int64_t press_duration = now - press_start_time;

        if (press_duration >= BUTTON_FACTORY_RESET * 1000000) {
            ESP_LOGI(GPIO_TAG ISR_TAG, "Press duration = %lld ms\nPerforming factory reset", press_duration / 1000);
            reset_device_and_erase_nvs();
        } else if (press_duration >= BUTTON_PAIRMODE_THRESHOLD * 1000000) {
            ESP_LOGI(GPIO_TAG ISR_TAG, "Press duration = %lld ms\nStarting pairmode", press_duration / 1000);
            start_softap_provisioning();
        } else {
            ESP_LOGI(GPIO_TAG ISR_TAG, "Press duration = %lld ms\nPowering off", press_duration / 1000);
            esp_restart();
        }
    }
}

void execute_command(void)
{
    ESP_LOGD(DEVICE_TAG, "Executing command.");
    switch (blecmd)
    {
    case WIFI_UPDATE:
        ESP_LOGI(GATTS_TAG, "Updating WiFi credentials...");
        send_ble_uart_notification(save_wifi_credentials(bleParameters[0], bleParameters[1]), NULL, 0);
        ESP_LOGW(GATTS_TAG, "Rebooting device...");
        esp_restart();
        break;
    case REBOOT:
        ESP_LOGW(GATTS_TAG, "Rebooting device...");
        send_ble_uart_notification(ESP_OK, NULL, 0);
        esp_restart();
        break;
    case FACTORY_RESET:
        ESP_LOGW(GATTS_TAG, "Performing application reset...");
        send_ble_uart_notification(reset_device_and_erase_nvs(), NULL, 0);
        esp_restart();
        break;
    case READ_INTERNAL_TEMP:
        ESP_LOGI(GATTS_TAG, "Reading internal temperature...");
        float tempInternal = get_ambient_temp();
        send_ble_uart_notification(ESP_FAIL, (uint8_t*)&tempInternal, sizeof(tempInternal));
        break;
    case READ_TEMP:
        ESP_LOGI(GATTS_TAG, "Reading external temperature...");
        float tempObject = get_object_temp();
        send_ble_uart_notification(ESP_OK, (uint8_t*)&tempObject, sizeof(tempObject));
        break;
    case READ_RELAY_STATE:
        ESP_LOGI(GATTS_TAG, "Reading relay state...");
        HEATER_STATE relayState = get_heater_state();
        send_ble_uart_notification(ESP_OK, (uint8_t*)&relayState, sizeof(relayState));
        break;
    case SET_TARGET_TEMP:
        ESP_LOGI(GATTS_TAG, "Setting target temperature to %d.%d", *(uint8_t*)bleParameters[0], *(uint8_t*)bleParameters[1]);
        float targetTemp = (float)(*(uint8_t*)bleParameters[0]) + ((float)(*(uint8_t*)bleParameters[1]) / 100.0f);
        set_heater_temp(targetTemp);
        send_ble_uart_notification(ESP_OK, NULL, 0);
        break;
    case SET_RELAYS:
        ESP_LOGI(GATTS_TAG, "Setting relay state to %d", *(uint8_t*)bleParameters[0]);
        esp_err_t err = set_heater_state((HEATER_STATE)*(uint8_t*)bleParameters[0]);
        send_ble_uart_notification(err, NULL, 0);
        break;
    case CHANGE_DEVICE_MODE:
        ESP_LOGI(GATTS_TAG, "Setting device mode to %d", *(uint8_t*)bleParameters[0]);
        set_heater_mode((DEVICE_MODE)*(uint8_t*)bleParameters[0]);
        send_ble_uart_notification(ESP_OK, NULL, 0);
        break;
    
    default:
        ESP_LOGE(GATTS_TAG, "Invalid command received: %X", blecmd);
        break;
    }
    free(bleParameters[0]);
    free(bleParameters[1]);
    bleParameters[0] = NULL;
    bleParameters[1] = NULL;
    blecmd = NO_COMMAND;
    uart_cmd_handler_flag = false;
}


void button_init(void)
{
    gpio_config_t btn_cfg = {
        .pin_bit_mask = GPIO_PIN_MASK(BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,     // Or PULLDOWN based on your wiring
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE        // You can use RISING/FALLING if needed
    };
    gpio_config(&btn_cfg);
    // Install GPIO ISR service
    gpio_install_isr_service(0);  // Use ESP_INTR_FLAG_DEFAULT or 0
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, (void*)(uintptr_t)BUTTON_GPIO);
    
    ESP_LOGI(GPIO_TAG, "ISR set up on GPIO %d", BUTTON_GPIO);
}

void app_main(void)
{
    status_led_init();
    device_data_init();
    button_init();

    ir_sensor_init();
    heater_interface_init();

    // set_heater_temp(27.5);
    // set_heater_mode(AUTO_MODE);
    
    comm_init();

    if (strlen(device_data.ssid) != 0 || strlen(device_data.password) != 0)
    {
        ESP_LOGI(WIFI_TAG, "Connecting to saved WiFi: %s\tPassword: %s", device_data.ssid, device_data.password);
        wifi_init_sta(device_data.ssid, device_data.password);
        // start_ble_provisioning();
    }
    else
    {
        ESP_LOGI(WIFI_TAG, "Starting SoftAP for provisioning");
        start_softap_provisioning();
        ESP_LOGI(WIFI_TAG, "Starting BLE for provisioning");
        // start_ble_provisioning();
    }
}

const char* get_enum_name(ENUM_TYPE type, int value)
{
    switch (type) {
        case ENUM_DEVICE_STATUS:
            switch ((DEVICE_STATUS)value) {
                case OFF_STATE:         return "OFF_STATE";
                case NORMAL_OP:         return "NORMAL_OP";
                case PAIRING:           return "PAIRING";
                case ERROR:             return "ERROR";
                case FIRMWARE_UPDATE:   return "FIRMWARE_UPDATE";
                default:                return "UNKNOWN_DEVICE_STATUS";
            }

        case ENUM_HEATER_STATE:
            switch ((HEATER_STATE)value) {
                case HEATER_OFF: return "HEATER_OFF";
                case HEATER_LOW: return "HEATER_LOW";
                case HEATER_MID: return "HEATER_MID";
                case HEATER_HIGH: return "HEATER_HIGH";
                default: return "UNKNOWN_HEATER_STATE";
            }

        case ENUM_DEVICE_MODE:
            switch ((DEVICE_MODE)value) {
                case MANUAL_MODE: return "MANUAL_MODE";
                case AUTO_MODE:   return "AUTO_MODE";
                case ECO_MODE:    return "ECO_MODE";
                case DEVICE_MODE_COUNT: return "DEVICE_MODE_COUNT";
                default: return "UNKNOWN_DEVICE_MODE";
            }

        case ENUM_BLE_COMMANDS:
            switch ((BLE_COMMANDS)value) {
                case NO_COMMAND:         return "NO_COMMAND";
                case REBOOT:             return "REBOOT";
                case FACTORY_RESET:      return "FACTORY_RESET";
                case WIFI_UPDATE:        return "WIFI_UPDATE";
                case READ_INTERNAL_TEMP: return "READ_INTERNAL_TEMP";
                case READ_TEMP:          return "READ_TEMP";
                case SET_TARGET_TEMP:    return "SET_TARGET_TEMP";
                case READ_RELAY_STATE:   return "READ_RELAY_STATE";
                case SET_RELAYS:         return "SET_RELAYS";
                case MAX_BLE_CMDS:       return "MAX_BLE_CMDS";
                default: return "UNKNOWN_BLE_COMMAND";
            }

        default:
            return "UNKNOWN_ENUM_TYPE";
    }
}

