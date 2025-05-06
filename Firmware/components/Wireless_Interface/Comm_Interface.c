#include "Comm_Interface.h"
#include <string.h>

#define TAG "COMM_INTERFACE"

BLE_COMMANDS blecmd;
char* bleParameters[2] = {NULL, NULL};
bool uart_cmd_handler_flag = false;

char *cmdStringFetch[]  = {
    "NoCommand",
    "poweroff/reboot",
    "FactoryReset",
    "wifiUpdate",
    "readInternalTemp",
    "readTemp",
    "setTargetTemp",
    "readRelayState",
    "setRelays",
};

void ble_command_task(void * pvParameters)
{
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        
        if(uart_cmd_handler_flag)
        {
            execute_command();
        }
    }
    vTaskDelete( NULL );
}

void comm_init(void)
{
    /* Initialize networking stack */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Initialize Wi-Fi including netif with default config */
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    start_ble();

    // Create the task for the IR sensor
    if(xTaskCreate(ble_command_task, "ble_command_task", 2048, NULL, 1, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create IR sensor task");
        set_device_status(1, ERROR);
        return;
    }
    ESP_LOGD(TAG, "ble_command_task created successfully");
}

void start_ble(void)
{
    esp_err_t ret;
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
    {
        ESP_LOGE(TAG GATTS_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
    {
        ESP_LOGE(TAG GATTS_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_init();
    if (ret)
    {
        ESP_LOGE(TAG GATTS_TAG, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_enable();
    if (ret)
    {
        ESP_LOGE(TAG GATTS_TAG, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "gap register error, error code = %x", ret);
        return;
    }
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret)
    {
        ESP_LOGE(TAG GATTS_TAG, "gatts register error, error code = %x", ret);
        return;
    }
    ret = esp_ble_gatts_app_register(PROFILE_PROV_APP_ID);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "gatts app register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gatts_app_register(PROFILE_UART_APP_ID);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "gatts app register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gap_set_device_name(BLE_DEVICE_NAME);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "set device name failed, error code = %x", ret);
    }

#if CONFIG_SET_RAW_ADV_DATA
    esp_err_t raw_adv_ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
    if (raw_adv_ret)
    {
        ESP_LOGE(GATTS_TAG, "config raw adv data failed, error code = %x ", raw_adv_ret);
    }
    adv_config_done |= adv_config_flag;
    esp_err_t raw_scan_ret = esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
    if (raw_scan_ret)
    {
        ESP_LOGE(GATTS_TAG, "config raw scan rsp data failed, error code = %x", raw_scan_ret);
    }
    adv_config_done |= scan_rsp_config_flag;
#else
    // config adv data
    ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "config adv data failed, error code = %x", ret);
    }
    adv_config_done |= adv_config_flag;
    // config scan response data
    ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "config scan response data failed, error code = %x", ret);
    }
    adv_config_done |= scan_rsp_config_flag;
#endif

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(517);
    if (local_mtu_ret)
    {
        ESP_LOGE(GATTS_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }
}

uint8_t parse_command(uint8_t *data, int len, const char *property)
{
    char packet[len + 1]; // Create a buffer with space for null terminator
    memcpy(packet, data, len);
    packet[len] = '\0'; // Ensure null termination

    ESP_LOGI(GATTS_TAG, "%s", packet);

    // Extract command
    blecmd = (BLE_COMMANDS)(uint8_t)packet[0];

    // Determine whether to allow command based on property
    bool allow_all = (strcmp(property, "cmd") == 0);
    bool allow_wifi = (strcmp(property, "wifi") == 0);

    // Extract property value
    switch (blecmd)
    {
    case REBOOT:
    case FACTORY_RESET:
    case READ_INTERNAL_TEMP:
    case READ_TEMP:
    case READ_RELAY_STATE:
        if (!allow_all || len > 1) {
            blecmd = NO_COMMAND;
        }
        break;

    case WIFI_UPDATE:
        if (!allow_wifi && !allow_all) {
            blecmd = NO_COMMAND;
            break;
        }

        uint8_t ssid_len = (uint8_t)(packet[1]);
        char* ssid = (char*)(malloc(ssid_len + 1));
        if (!ssid) {
            ESP_LOGE(GATTS_TAG, "Memory allocation failed for SSID!\n");
            return 0;
        }
        memcpy(ssid, &packet[2], ssid_len);
        ssid[ssid_len] = '\0';

        uint8_t pwd_len = (uint8_t)(packet[2 + ssid_len]);
        char* password = (char*)(malloc(pwd_len + 1));
        if (!password) {
            ESP_LOGE(GATTS_TAG, "Memory allocation failed for Password!\n");
            free(ssid);
            return 0;
        }
        memcpy(password, &packet[3 + ssid_len], pwd_len);
        password[pwd_len] = '\0';

        if ((ssid_len > WIFI_SSID_MAX_LEN) || (pwd_len > WIFI_PASS_MAX_LEN)) {
            ESP_LOGE(GATTS_TAG, "WiFi credential parameters out of range.!\n");
            free(ssid);
            free(password);
            return 0;
        }

        bleParameters[0] = ssid;
        bleParameters[1] = password;
        break;

    case SET_TARGET_TEMP:
        if (!allow_all) {
            blecmd = NO_COMMAND;
            break;
        }

        char* temp_h = (char*)(malloc(1));
        char* temp_l = (char*)(malloc(1));
        if (!temp_h || !temp_l) {
            ESP_LOGE(GATTS_TAG, "Memory allocation failed for temperature parameters.!\n");
            return 0;
        }
        memcpy(temp_h, &packet[1], 1);
        memcpy(temp_l, &packet[2], 1);

        bleParameters[0] = temp_h;
        bleParameters[1] = temp_l;
        break;

    case SET_RELAYS:
        if (!allow_all) {
            blecmd = NO_COMMAND;
            break;
        }

        char* relay_state = (char*)(malloc(1));
        if (!relay_state) {
            ESP_LOGE(GATTS_TAG, "Memory allocation failed for relay state parameter.!\n");
            return 0;
        }
        memcpy(relay_state, &packet[1], 1);
        bleParameters[0] = relay_state;
        break;

    default:
        blecmd = NO_COMMAND;
        break;
    }

    if((blecmd < 1) || (blecmd >= MAX_BLE_CMDS))
    {
        uart_cmd_handler_flag = false;
    }
    else
    {
        uart_cmd_handler_flag = true;
    }

    ESP_LOGD(GATTS_TAG, "Command received: %s",
            ((blecmd >= NO_COMMAND) && (blecmd < MAX_BLE_CMDS)) ? cmdStringFetch[blecmd] : cmdStringFetch[0]);

    return blecmd;
}

// uint8_t parse_command(uint8_t *data, int len , const char *property)
// {
//     ESP_LOGI(GATTS_TAG, "Received data with len: %d", len);
//     char json_str[len + 1]; // Create a buffer with space for null terminator
//     memcpy(json_str, data, len);
//     json_str[len] = '\0'; // Ensure null termination

//     ESP_LOGI(GATTS_TAG, "%s", json_str);

//     // Extract command
//     blecmd = (BLE_COMMANDS)(uint8_t)json_str[0];

//     // Extract property value
//     switch (blecmd)
//     {
//     case REBOOT:
//     case FACTORY_RESET:
//     case READ_INTERNAL_TEMP:
//     case READ_TEMP:
//     case READ_RELAY_STATE:
//         // No parameters needed for these commands and if any, command is discarded.
//         blecmd = (len > 1) ? NO_COMMAND : blecmd;
//         break;

//     case WIFI_UPDATE:
//         uint8_t ssid_len = (uint8_t)(json_str[1]);
//         char* ssid = (char*)(malloc(ssid_len + 1));
//         if (!ssid) {
//             ESP_LOGE(GATTS_TAG, "Memory allocation failed for SSID!\n");
//             return 0;
//         }
//         memcpy(ssid, &json_str[2], ssid_len);
//         ssid[ssid_len] = '\0';

//         uint8_t pwd_len = (uint8_t)(json_str[2 + ssid_len]);
//         char* password = (char*)(malloc(pwd_len + 1));
//         if (!password) {
//             ESP_LOGE(GATTS_TAG, "Memory allocation failed for Password!\n");
//             free(ssid);
//             return 0;
//         }
//         memcpy(password, &json_str[3 + ssid_len], pwd_len);
//         password[pwd_len] = '\0';

//         if ((ssid_len > WIFI_SSID_MAX_LEN) || (pwd_len > WIFI_PASS_MAX_LEN)) {
//             ESP_LOGE(GATTS_TAG, "WiFi credential parameters out of range.!\n");
//             // Clean up
//             free(ssid);
//             free(password);
//             return 0;
//         }

//         bleParameters[0] = ssid;
//         bleParameters[1] = password;
//         break;
//     case SET_TARGET_TEMP:
//         char* temp_h = (char*)(malloc(1));
//         char* temp_l = (char*)(malloc(1));
//         if (!temp_h || !temp_l) {
//             ESP_LOGE(GATTS_TAG, "Memory allocation failed for temprature parameters.!\n");
//             return 0;
//         }
//         memcpy(temp_h, &json_str[1], 1);
//         memcpy(temp_l, &json_str[2], 1);

//         bleParameters[0] = temp_h;
//         bleParameters[1] = temp_l;
//         break;
//     case SET_RELAYS:
//         char* relay_state = (char*)(malloc(1));
//         if (!relay_state) {
//             ESP_LOGE(GATTS_TAG, "Memory allocation failed for relay state parameter.!\n");
//             return 0;
//         }
//         memcpy(relay_state, &json_str[1], 1);
//         bleParameters[0] = relay_state;
//         break;
    
//     default:
//         break;
//     }

//     ESP_LOGD(GATTS_TAG, "Command received: %s",
//             ((blecmd >= NO_COMMAND) && (blecmd < MAX_BLE_CMDS)) ? cmdStringFetch[blecmd] : cmdStringFetch[0]);

//     return blecmd;
// }

void send_ble_uart_notification(esp_err_t status, uint8_t *data, uint16_t len)
{
    uint8_t resp[1 + ((len == 0) ? 1: len)];
    ESP_LOGD(GATTS_TAG, "send_ble_uart_notification: status %d", status);
    ESP_LOGD(GATTS_TAG, "send_ble_uart_notification: len %d", len);
    
    resp[0] = blecmd;
    if(data != NULL)
    {
        ESP_LOGD(GATTS_TAG, "send_ble_uart_notification: data %s", data);
        memcpy(&resp[1], data, len);
        ESP_LOGD(GATTS_TAG, "send_ble_uart_notification: resp");
        ESP_LOG_BUFFER_HEXDUMP(GATTS_TAG, resp, len + 1, ESP_LOG_INFO);
    }
    else
    {
        resp[1] = (status == ESP_OK) ? BLE_RESPONSE_SUCCESS : BLE_RESPONSE_FAIL;
        ESP_LOGD(GATTS_TAG, "send_ble_uart_notification: resp %02X %02X", resp[0], resp[1]);
    }
    
    esp_err_t err = esp_ble_gatts_send_indicate(gl_profile_tab[PROFILE_UART_APP_ID].gatts_if,
                                                gl_profile_tab[PROFILE_UART_APP_ID].conn_id,
                                                gl_profile_tab[PROFILE_UART_APP_ID].char_handle[1],
                                                sizeof(resp), resp, false);
    if (err != ESP_OK)
    {
        ESP_LOGE(GATTS_TAG, "Failed to send BLE notification, error code = %s", esp_err_to_name(err));
    }
}

// WiFi
void wifi_init_sta(const char *ssid, const char *password)
{
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

esp_err_t load_wifi_credentials(char *ssid, char *password)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE_WIFIDATA, NVS_READONLY, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(WIFI_TAG, "No saved credentials found");
        return err;
    }

    size_t ssid_len = WIFI_SSID_MAX_LEN, pass_len = WIFI_PASS_MAX_LEN;
    nvs_get_str(nvs, "ssid", ssid, &ssid_len);
    nvs_get_str(nvs, "password", password, &pass_len);
    nvs_close(nvs);
    return ESP_OK;
}

esp_err_t save_wifi_credentials(const char *ssid, const char *password)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE_WIFIDATA, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(WIFI_TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return err;
    }

    nvs_set_str(nvs, "ssid", ssid);
    nvs_set_str(nvs, "password", password);
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(WIFI_TAG, "WiFi credentials saved");
    return ESP_OK;
}
