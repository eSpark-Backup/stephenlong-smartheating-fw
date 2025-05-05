#include "Comm_Interface.h"
#include "provisioning.h"
#include <string.h>

#define TAG "COMM_INTERFACE"

BLE_COMMANDS blecmd;
char* bleParameters[2] = {NULL, NULL};

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

const uint8_t uart_tx_uuid[16] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xC3, 0x0E,
    0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x7E};

const uint8_t uart_rx_uuid[16] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xC3, 0x0E,
    0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x7E};

const uint8_t uart_service_uuid[16] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xC3, 0x0E,
         0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x7E};

         /* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gatts_cb = gatts_profile_a_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
    },
    [PROFILE_UART_APP_ID] = {
        .gatts_cb = gatts_uart_event_handler,
        .gatts_if = ESP_GATT_IF_NONE, }
};


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

uint8_t parse_command(uint8_t *data, int len , const char *property)
{
    char json_str[len + 1]; // Create a buffer with space for null terminator
    memcpy(json_str, data, len);
    json_str[len] = '\0'; // Ensure null termination

    ESP_LOGI(GATTS_TAG, "%s", json_str);

    // Extract command
    blecmd = (BLE_COMMANDS)(uint8_t)json_str[0];

    // Extract property value
    switch (blecmd)
    {
    case REBOOT:
    case FACTORY_RESET:
    case READ_INTERNAL_TEMP:
    case READ_TEMP:
    case READ_RELAY_STATE:
        // No parameters needed for these commands and if any, command is discarded.
        blecmd = (len > 1) ? NO_COMMAND : blecmd;
        break;

    case WIFI_UPDATE:
        uint8_t ssid_len = (uint8_t)(json_str[1]);
        char* ssid = (char*)(malloc(ssid_len + 1));
        if (!ssid) {
            ESP_LOGE(GATTS_TAG, "Memory allocation failed for SSID!\n");
            return 0;
        }
        memcpy(ssid, &json_str[2], ssid_len);
        ssid[ssid_len] = '\0';

        uint8_t pwd_len = (uint8_t)(json_str[2 + ssid_len]);
        char* password = (char*)(malloc(pwd_len + 1));
        if (!password) {
            ESP_LOGE(GATTS_TAG, "Memory allocation failed for Password!\n");
            free(ssid);
            return 0;
        }
        memcpy(password, &json_str[3 + ssid_len], pwd_len);
        password[pwd_len] = '\0';

        if ((ssid_len > WIFI_SSID_MAX_LEN) || (pwd_len > WIFI_PASS_MAX_LEN)) {
            ESP_LOGE(GATTS_TAG, "WiFi credential parameters out of range.!\n");
            // Clean up
            free(ssid);
            free(password);
            return 0;
        }

        bleParameters[0] = ssid;
        bleParameters[1] = password;
        break;
    case SET_TARGET_TEMP:
        char* temp_h = (char*)(malloc(1));
        char* temp_l = (char*)(malloc(1));
        if (!temp_h || !temp_l) {
            ESP_LOGE(GATTS_TAG, "Memory allocation failed for temprature parameters.!\n");
            return 0;
        }
        memcpy(temp_h, &json_str[1], 1);
        memcpy(temp_l, &json_str[2], 1);

        bleParameters[0] = temp_h;
        bleParameters[1] = temp_l;
        break;
    case SET_RELAYS:
        char* relay_state = (char*)(malloc(1));
        if (!relay_state) {
            ESP_LOGE(GATTS_TAG, "Memory allocation failed for relay state parameter.!\n");
            return 0;
        }
        memcpy(relay_state, &json_str[1], 1);
        bleParameters[0] = relay_state;
        break;
    
    default:
        break;
    }

    ESP_LOGD(GATTS_TAG, "Command received: %s",
            ((blecmd >= NO_COMMAND) && (blecmd < MAX_BLE_CMDS)) ? cmdStringFetch[blecmd] : cmdStringFetch[0]);

    return blecmd;
}

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
                                                gl_profile_tab[PROFILE_UART_APP_ID].char1_handle,
                                                sizeof(resp), resp, false);
    if (err != ESP_OK)
    {
        ESP_LOGE(GATTS_TAG, "Failed to send BLE notification, error code = %s", esp_err_to_name(err));
    }
}


