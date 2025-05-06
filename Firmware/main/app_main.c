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

// bool gpio_isr_handler_flag = false;
// bool uart_cmd_handler_flag = false;
// static prepare_type_env_t a_prepare_write_env;


// void start_softap_provisioning(void);
/// Declare the static function
// static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
// static void gatts_uart_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
// void example_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);
// void example_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);


// const uint8_t uart_tx_uuid[16] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xC3, 0x0E,
//                                   0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x7E};

// const uint8_t uart_rx_uuid[16] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xC3, 0x0E,
//                                   0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x7E};

// const uint8_t uart_service_uuid[16] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xC3, 0x0E,
//                                        0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x7E};


// static uint8_t char1_str[] = {0x11, 0x22, 0x33};
// static esp_gatt_char_prop_t a_property = 0;

// static esp_attr_value_t gatts_demo_char1_val =
//     {
//         .attr_max_len = GATTS_CHAR_VAL_LEN_MAX,
//         .attr_len = sizeof(char1_str),
//         .attr_value = char1_str,
// };

// static uint8_t adv_config_done = 0;
// #define adv_config_flag (1 << 0)
// #define scan_rsp_config_flag (1 << 1)

#ifdef CONFIG_SET_RAW_ADV_DATA
static uint8_t raw_adv_data[] = {
    0x02, 0x01, 0x06,       // Length 2, Data Type 1 (Flags), Data 1 (LE General Discoverable Mode, BR/EDR Not Supported)
    0x02, 0x0a, 0xeb,       // Length 2, Data Type 10 (TX power leve), Data 2 (-21)
    0x03, 0x03, 0xab, 0xcd, // Length 3, Data Type 3 (Complete 16-bit Service UUIDs), Data 3 (UUID)
};
static uint8_t raw_scan_rsp_data[] = { // Length 15, Data Type 9 (Complete Local Name), Data 1 (ESP_GATTS_DEMO)
    0x0f, 0x09, 0x45, 0x53, 0x50, 0x5f, 0x47, 0x41, 0x54, 0x54, 0x53, 0x5f, 0x44,
    0x45, 0x4d, 0x4f};
#else

// static uint8_t adv_service_uuid128[16] = {
//     /* LSB <--------------------------------------------------------------------------------> MSB */
//     // first uuid, 16bit, [12],[13] is the value
//     0xfb,
//     0x34,
//     0x9b,
//     0x5f,
//     0x80,
//     0x00,
//     0x00,
//     0x80,
//     0x00,
//     0x10,
//     0x00,
//     0x00,
//     0xC3,
//     0x0E,
//     0x00,
//     0x00,
//     // // second uuid, 32bit, [12], [13], [14], [15] is the value
//     // 0xfb,
//     // 0x34,
//     // 0x9b,
//     // 0x5f,
//     // 0x80,
//     // 0x00,
//     // 0x00,
//     // 0x80,
//     // 0x00,
//     // 0x10,
//     // 0x00,
//     // 0x00,
//     // 0xFF,
//     // 0x00,
//     // 0x00,
//     // 0x00,
// };

// // The length of adv data must be less than 31 bytes
// // static uint8_t test_manufacturer[TEST_MANUFACTURER_DATA_LEN] =  {0x12, 0x23, 0x45, 0x56};
// // adv data
// static esp_ble_adv_data_t adv_data = {
//     .set_scan_rsp = false,
//     .include_name = true,
//     .include_txpower = false,
//     .min_interval = 0x0006, // slave connection min interval, Time = min_interval * 1.25 msec
//     .max_interval = 0x0010, // slave connection max interval, Time = max_interval * 1.25 msec
//     .appearance = 0x00,
//     .manufacturer_len = 0,       // TEST_MANUFACTURER_DATA_LEN,
//     .p_manufacturer_data = NULL, //&test_manufacturer[0],
//     .service_data_len = 0,
//     .p_service_data = NULL,
//     .service_uuid_len = sizeof(adv_service_uuid128),
//     .p_service_uuid = adv_service_uuid128,
//     .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
// };
// scan response data
// static esp_ble_adv_data_t scan_rsp_data = {
//     .set_scan_rsp = true,
//     .include_name = true,
//     .include_txpower = true,
//     //.min_interval = 0x0006,
//     //.max_interval = 0x0010,
//     .appearance = 0x00,
//     .manufacturer_len = 0,       // TEST_MANUFACTURER_DATA_LEN,
//     .p_manufacturer_data = NULL, //&test_manufacturer[0],
//     .service_data_len = 0,
//     .p_service_data = NULL,
//     .service_uuid_len = sizeof(adv_service_uuid128),
//     .p_service_uuid = adv_service_uuid128,
//     .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
// };

#endif /* CONFIG_SET_RAW_ADV_DATA */

// static esp_ble_adv_params_t adv_params = {
//     .adv_int_min = 0x20,
//     .adv_int_max = 0x40,
//     .adv_type = ADV_TYPE_IND,
//     .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
//     //.peer_addr            =
//     //.peer_addr_type       =
//     .channel_map = ADV_CHNL_ALL,
//     .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
// };

// struct gatts_profile_inst
// {
//     esp_gatts_cb_t gatts_cb;
//     uint16_t gatts_if;
//     uint16_t app_id;
//     uint16_t conn_id;

//     uint16_t service_handle;
//     esp_gatt_srvc_id_t service_id;
//     uint16_t char_handle;
//     esp_bt_uuid_t char_uuid;
//     uint16_t char1_handle;
//     esp_bt_uuid_t char1_uuid;


//     esp_gatt_perm_t perm;
//     esp_gatt_char_prop_t property;
//     uint16_t descr_handle;
//     esp_bt_uuid_t descr_uuid;
// };

// /* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
// static struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
//     [PROFILE_PROV_APP_ID] = {
//         .gatts_cb = gatts_profile_a_event_handler,
//         .gatts_if = ESP_GATT_IF_NONE,
//     },
//     [PROFILE_UART_APP_ID] = {
//         .gatts_cb = gatts_uart_event_handler,
//         .gatts_if = ESP_GATT_IF_NONE, }
// };

// // ISR handler
// static void IRAM_ATTR gpio_isr_handler(void* arg) {
//     gpio_isr_handler_flag=true;

// }

static int64_t press_start_time = 0;
static void IRAM_ATTR button_isr_handler(void *arg) {
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
        } else if (press_duration >= BUTTON_PAIRMODE_THRESHOLD * 1000000) {
            ESP_LOGI(GPIO_TAG ISR_TAG, "Press duration = %lld ms\nStarting pairmode", press_duration / 1000);
        } else {
            ESP_LOGI(GPIO_TAG ISR_TAG, "Press duration = %lld ms\nPowering off", press_duration / 1000);
        }
    }
}


// void send_ble_uart_notification(esp_err_t status, uint8_t *data, uint16_t len)
// {
//     uint8_t resp[1 + ((len == 0) ? 1: len)];
//     ESP_LOGD(GATTS_TAG, "send_ble_uart_notification: status %d", status);
//     ESP_LOGD(GATTS_TAG, "send_ble_uart_notification: len %d", len);
    
//     resp[0] = blecmd;
//     if(data != NULL)
//     {
//         ESP_LOGD(GATTS_TAG, "send_ble_uart_notification: data %s", data);
//         memcpy(&resp[1], data, len);
//         ESP_LOGD(GATTS_TAG, "send_ble_uart_notification: resp");
//         ESP_LOG_BUFFER_HEXDUMP(GATTS_TAG, resp, len + 1, ESP_LOG_INFO);
//     }
//     else
//     {
//         resp[1] = (status == ESP_OK) ? BLE_RESPONSE_SUCCESS : BLE_RESPONSE_FAIL;
//         ESP_LOGD(GATTS_TAG, "send_ble_uart_notification: resp %02X %02X", resp[0], resp[1]);
//     }
    
//     esp_err_t err = esp_ble_gatts_send_indicate(gl_profile_tab[PROFILE_UART_APP_ID].gatts_if,
//                                                 gl_profile_tab[PROFILE_UART_APP_ID].conn_id,
//                                                 gl_profile_tab[PROFILE_UART_APP_ID].char1_handle,
//                                                 sizeof(resp), resp, false);
//     if (err != ESP_OK)
//     {
//         ESP_LOGE(GATTS_TAG, "Failed to send BLE notification, error code = %s", esp_err_to_name(err));
//     }
// }

// static esp_err_t save_wifi_credentials(const char *ssid, const char *password)
// {
//     nvs_handle_t nvs;
//     esp_err_t err = nvs_open(NVS_NAMESPACE_WIFIDATA, NVS_READWRITE, &nvs);
//     if (err != ESP_OK)
//     {
//         ESP_LOGE(WIFI_TAG, "Error opening NVS: %s", esp_err_to_name(err));
//         return err;
//     }

//     nvs_set_str(nvs, "ssid", ssid);
//     nvs_set_str(nvs, "password", password);
//     nvs_commit(nvs);
//     nvs_close(nvs);
//     ESP_LOGI(WIFI_TAG, "WiFi credentials saved");
//     return ESP_OK;
// }

// static esp_err_t load_wifi_credentials(char *ssid, char *password)
// {
//     nvs_handle_t nvs;
//     esp_err_t err = nvs_open(NVS_NAMESPACE_WIFIDATA, NVS_READONLY, &nvs);
//     if (err != ESP_OK)
//     {
//         ESP_LOGE(WIFI_TAG, "No saved credentials found");
//         return err;
//     }

//     size_t ssid_len = WIFI_SSID_MAX_LEN, pass_len = WIFI_PASS_MAX_LEN;
//     nvs_get_str(nvs, "ssid", ssid, &ssid_len);
//     nvs_get_str(nvs, "password", password, &pass_len);
//     nvs_close(nvs);
//     return ESP_OK;
// }

// static void event_handler(void *arg, esp_event_base_t event_base,
//                           int32_t event_id, void *event_data)
// {
//     static int s_retry_num = 0;

//     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
//     {
//         esp_wifi_connect();
//     }
//     else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
//     {
//         if (s_retry_num > AP_RECONN_ATTEMPTS)
//         {
//             ESP_LOGI(WIFI_TAG, "Starting SoftAP for provisioning");
//             start_softap_provisioning();
//         }
//         else{
//             esp_wifi_connect();
//             s_retry_num++;
//             ESP_LOGI(WIFI_TAG, "connect to the AP fail");
//             ESP_LOGI(WIFI_TAG, "retry to connect to the AP");

//         }
//     }
//     else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
//     {
//         ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
//         ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
//         s_retry_num = 0;
//     }
// }

// static void wifi_init_sta(const char *ssid, const char *password)
// {
//     esp_event_handler_instance_t instance_any_id;
//     esp_event_handler_instance_t instance_got_ip;

//     ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

//     wifi_config_t wifi_config = {0};
//     strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
//     strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());
// }


// void decode_plus_to_space(char *str)
// {
//     while (*str)
//     {
//         if (*str == '+')
//         {
//             *str = ' ';
//         }
//         str++;
//     }
// }

// esp_err_t post_handler(httpd_req_t *req)
// {
//     char buf[100];
//     int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
//     if (ret <= 0)
//     {
//         httpd_resp_send_500(req);
//         return ESP_FAIL;
//     }
//     buf[ret] = '\0';

//     char ssid[WIFI_SSID_MAX_LEN] = {0}, password[WIFI_PASS_MAX_LEN] = {0};
//     httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid));
//     httpd_query_key_value(buf, "password", password, sizeof(password));

//     if (strlen(ssid) > 0 && strlen(password) > 0)
//     {
//         decode_plus_to_space(ssid);
//         decode_plus_to_space(password);
//         save_wifi_credentials(ssid, password);
//         ESP_LOGI(WIFI_TAG, "Received SSID: %s, Password: %s", ssid, password);
//         httpd_resp_send(req, "WiFi credentials received. Rebooting...", HTTPD_RESP_USE_STRLEN);
//         vTaskDelay(pdMS_TO_TICKS(2000));
//         esp_restart();
//     }
//     else
//     {
//         httpd_resp_send(req, "Invalid input. Try again.", HTTPD_RESP_USE_STRLEN);
//     }
//     return ESP_OK;
// }

// // /* Start HTTP Web Server */
// static httpd_handle_t start_webserver(void)
// {
//     httpd_config_t config = HTTPD_DEFAULT_CONFIG();
//     httpd_handle_t server = NULL;
//     if (httpd_start(&server, &config) == ESP_OK)
//     {
//         httpd_uri_t uri_get = {.uri = "/", .method = HTTP_GET, .handler = get_handler};
//         httpd_uri_t uri_post = {.uri = "/set", .method = HTTP_POST, .handler = post_handler};
//         httpd_register_uri_handler(server, &uri_get);
//         httpd_register_uri_handler(server, &uri_post);
//     }
//     else
//     {
//         ESP_LOGI(WIFI_TAG, "ERRORRRRRR HEREEEEEEEEE");
//     }
//     return server;
// }

// // /* Start Wi-Fi in SoftAP Mode */
//  void start_softap_provisioning(void)
// {
//     gpio_isr_handler_flag=false;
//     wifi_config_t wifi_config = {
//         .ap = {
//             .max_connection = 4,
//             .authmode = WIFI_AUTH_WPA2_PSK}};
//     strncpy((char *)wifi_config.ap.ssid, SSID_WIFI, sizeof(wifi_config.ap.ssid));
//     strcpy((char *)wifi_config.ap.password, PASS_WIFI);

//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
//     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());

//     ESP_LOGI(WIFI_TAG, "SoftAP mode started. Go to http://192.168.4.1/");
//     start_webserver();
// }

// char * parse_JSON(uint8_t *data, int len , const char *property){

     
//     char json_str[len + 1]; // Create a buffer with space for null terminator
//     memcpy(json_str, data, len);
//     json_str[len] = '\0'; // Ensure null termination

//     ESP_LOGI(GATTS_TAG, "Received JSON: %s", json_str);    
//     // Parse JSON
//     cJSON *json = cJSON_Parse(json_str);
//     if (json == NULL)
//     {
//         ESP_LOGE(GATTS_TAG, "Failed to parse JSON");
//         return 0;
//     }

//     cJSON *property_JSON = cJSON_GetObjectItem(json, property);

    
//     if (property_JSON && cJSON_IsString(property_JSON) && (property_JSON->valuestring != NULL))
//     {
//         ESP_LOGI(GATTS_TAG, "%s : %s", property , property_JSON->valuestring);
//         return(property_JSON->valuestring);
//     }
//     else
//     {
//         ESP_LOGE(GATTS_TAG, "Failed to extract Property value");
//         return 0;
//     }
// }

// uint8_t parse_command(uint8_t *data, int len , const char *property)
// {
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

// void parse_ble_write_data(uint8_t *data, int len)
// {
  
//     char json_str[len + 1]; // Create a buffer with space for null terminator
//     memcpy(json_str, data, len);
//     json_str[len] = '\0'; // Ensure null termination

//     ESP_LOGI(GATTS_TAG, "Received JSON: %s", json_str);

//     // Parse JSON
//     cJSON *json = cJSON_Parse(json_str);
//     if (json == NULL)
//     {
//         ESP_LOGE(GATTS_TAG, "Failed to parse JSON");
//         return;
//     }

//     // Extract ssid
//     cJSON *ssid_item = cJSON_GetObjectItem(json, "ssid");
//     cJSON *password_item = cJSON_GetObjectItem(json, "password");

//     if (ssid_item && cJSON_IsString(ssid_item) && (ssid_item->valuestring != NULL))
//     {
//         ESP_LOGI(GATTS_TAG, "SSID: %s", ssid_item->valuestring);
//     }
//     else
//     {
//         ESP_LOGE(GATTS_TAG, "Failed to extract SSID");
//     }

//     if (password_item && cJSON_IsString(password_item) && (password_item->valuestring != NULL))
//     {
//         ESP_LOGI(GATTS_TAG, "Password: %s", password_item->valuestring);
//     }
//     else
//     {
//         ESP_LOGE(GATTS_TAG, "Failed to extract Password");
//     }
//     if (ssid_item && password_item)
//     {
//         save_wifi_credentials(ssid_item->valuestring, password_item->valuestring);
//         esp_restart();
//     }
//     else
//     {
//         ESP_LOGE(GATTS_TAG, "Creds not received correctly!");
//     }

//     cJSON_Delete(json); // Free memory
// }

// static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
// {
//     switch (event)
//     {
// #ifdef CONFIG_SET_RAW_ADV_DATA
//     case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
//         adv_config_done &= (~adv_config_flag);
//         if (adv_config_done == 0)
//         {
//             esp_ble_gap_start_advertising(&adv_params);
//         }
//         break;
//     case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
//         adv_config_done &= (~scan_rsp_config_flag);
//         if (adv_config_done == 0)
//         {
//             esp_ble_gap_start_advertising(&adv_params);
//         }
//         break;
// #else
//     case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
//         adv_config_done &= (~adv_config_flag);
//         if (adv_config_done == 0)
//         {
//             esp_ble_gap_start_advertising(&adv_params);
//         }
//         break;
//     case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
//         adv_config_done &= (~scan_rsp_config_flag);
//         if (adv_config_done == 0)
//         {
//             esp_ble_gap_start_advertising(&adv_params);
//         }
//         break;
// #endif
//     case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
//         // advertising start complete event to indicate advertising start successfully or failed
//         if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
//         {
//             ESP_LOGE(GATTS_TAG, "Advertising start failed\n");
//         }
//         break;
//     case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
//         if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
//         {
//             ESP_LOGE(GATTS_TAG, "Advertising stop failed\n");
//         }
//         else
//         {
//             ESP_LOGI(GATTS_TAG, "Stop adv successfully\n");
//         }
//         break;
//     case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
//         ESP_LOGI(GATTS_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
//                  param->update_conn_params.status,
//                  param->update_conn_params.min_int,
//                  param->update_conn_params.max_int,
//                  param->update_conn_params.conn_int,
//                  param->update_conn_params.latency,
//                  param->update_conn_params.timeout);
//         break;
//     default:
//         break;
//     }
// }

// void example_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
// {
//     esp_gatt_status_t status = ESP_GATT_OK;
//     if (param->write.need_rsp)
//     {
//         if (param->write.is_prep)
//         {
//             if (prepare_write_env->prepare_buf == NULL)
//             {
//                 prepare_write_env->prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE * sizeof(uint8_t));
//                 prepare_write_env->prepare_len = 0;
//                 if (prepare_write_env->prepare_buf == NULL)
//                 {
//                     ESP_LOGE(GATTS_TAG, "Gatt_server prep no mem\n");
//                     status = ESP_GATT_NO_RESOURCES;
//                 }
//             }
//             else
//             {
//                 if (param->write.offset > PREPARE_BUF_MAX_SIZE)
//                 {
//                     status = ESP_GATT_INVALID_OFFSET;
//                 }
//                 else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE)
//                 {
//                     status = ESP_GATT_INVALID_ATTR_LEN;
//                 }
//             }

//             esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
//             gatt_rsp->attr_value.len = param->write.len;
//             gatt_rsp->attr_value.handle = param->write.handle;
//             gatt_rsp->attr_value.offset = param->write.offset;
//             gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
//             memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
//             esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
//             if (response_err != ESP_OK)
//             {
//                 ESP_LOGE(GATTS_TAG, "Send response error\n");
//             }
//             free(gatt_rsp);
//             if (status != ESP_GATT_OK)
//             {
//                 return;
//             }
//             memcpy(prepare_write_env->prepare_buf + param->write.offset,
//                    param->write.value,
//                    param->write.len);
//             prepare_write_env->prepare_len += param->write.len;

//         }
//         else
//         {
//             esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
//         }
//     }
// }

// void uart_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
// {
//     if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC)
//     {
//         // parse_ble_write_data(prepare_write_env->prepare_buf, prepare_write_env->prepare_len);
//         char *cmd_value = parse_JSON( param->write.value, param->write.len , "cmd");

//     }
//     else
//     {
//         ESP_LOGI(GATTS_TAG, "ESP_GATT_PREP_WRITE_CANCEL");
//     }
//     if (prepare_write_env->prepare_buf)
//     {
//         free(prepare_write_env->prepare_buf);
//         prepare_write_env->prepare_buf = NULL;
//     }
//     prepare_write_env->prepare_len = 0;
// }

// void example_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
// {
//     if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC)
//     {
//         parse_ble_write_data(prepare_write_env->prepare_buf, prepare_write_env->prepare_len);
//     }
//     else
//     {
//         ESP_LOGI(GATTS_TAG, "ESP_GATT_PREP_WRITE_CANCEL");
//     }
//     if (prepare_write_env->prepare_buf)
//     {
//         free(prepare_write_env->prepare_buf);
//         prepare_write_env->prepare_buf = NULL;
//     }
//     prepare_write_env->prepare_len = 0;
// }

// static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
// {
//     switch (event)
//     {
//     case ESP_GATTS_REG_EVT:
//         // ESP_LOGI(GATTS_TAG, "REGISTER_APP_EVT, status %d, app_id %d\n", param->reg.status, param->reg.app_id);
//         gl_profile_tab[PROFILE_PROV_APP_ID].service_id.is_primary = true;
//         gl_profile_tab[PROFILE_PROV_APP_ID].service_id.id.inst_id = 0x00;
//         gl_profile_tab[PROFILE_PROV_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
//         gl_profile_tab[PROFILE_PROV_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID;

//         esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(SSID_WIFI);
//         if (set_dev_name_ret)
//         {
//             ESP_LOGE(GATTS_TAG, "set device name failed, error code = %x", set_dev_name_ret);
//         }
// #ifdef CONFIG_SET_RAW_ADV_DATA
//         esp_err_t raw_adv_ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
//         if (raw_adv_ret)
//         {
//             ESP_LOGE(GATTS_TAG, "config raw adv data failed, error code = %x ", raw_adv_ret);
//         }
//         adv_config_done |= adv_config_flag;
//         esp_err_t raw_scan_ret = esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
//         if (raw_scan_ret)
//         {
//             ESP_LOGE(GATTS_TAG, "config raw scan rsp data failed, error code = %x", raw_scan_ret);
//         }
//         adv_config_done |= scan_rsp_config_flag;
// #else
//         // config adv data
//         esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
//         if (ret)
//         {
//             ESP_LOGE(GATTS_TAG, "config adv data failed, error code = %x", ret);
//         }
//         adv_config_done |= adv_config_flag;
//         // config scan response data
//         ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
//         if (ret)
//         {
//             ESP_LOGE(GATTS_TAG, "config scan response data failed, error code = %x", ret);
//         }
//         adv_config_done |= scan_rsp_config_flag;

// #endif
//         esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_PROV_APP_ID].service_id, GATTS_NUM_HANDLE_A);
//         break;

//     case ESP_GATTS_WRITE_EVT:
//     {
//         // ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, conn_id %d, trans_id %d, handle %d", param->write.conn_id, param->write.trans_id, param->write.handle);
//         if (!param->write.is_prep)
//         {
//             // esp_log_buffer_hex(GATTS_TAG, param->write.value, param->write.len);
//             if (gl_profile_tab[PROFILE_PROV_APP_ID].descr_handle == param->write.handle && param->write.len == 2)
//             {
//                 uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
//                 if (descr_value == 0x0001)
//                 {
//                     if (a_property & ESP_GATT_CHAR_PROP_BIT_NOTIFY)
//                     {
//                         ESP_LOGI(GATTS_TAG, "notify enable");
//                         uint8_t notify_data[15];
//                         for (int i = 0; i < sizeof(notify_data); ++i)
//                         {
//                             notify_data[i] = i % 0xff;
//                         }
//                         // the size of notify_data[] need less than MTU size
//                         esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gl_profile_tab[PROFILE_PROV_APP_ID].char_handle,
//                                                     sizeof(notify_data), notify_data, false);
//                     }
//                 }
//                 else if (descr_value == 0x0002)
//                 {
//                     if (a_property & ESP_GATT_CHAR_PROP_BIT_INDICATE)
//                     {
//                         ESP_LOGI(GATTS_TAG, "indicate enable");
//                         uint8_t indicate_data[15];
//                         for (int i = 0; i < sizeof(indicate_data); ++i)
//                         {
//                             indicate_data[i] = i % 0xff;
//                         }
//                         // the size of indicate_data[] need less than MTU size
//                         esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gl_profile_tab[PROFILE_PROV_APP_ID].char_handle,
//                                                     sizeof(indicate_data), indicate_data, true);
//                     }
//                 }
//                 else if (descr_value == 0x0000)
//                 {
//                     ESP_LOGI(GATTS_TAG, "notify/indicate disable ");
//                 }
//                 else
//                 {
//                     ESP_LOGE(GATTS_TAG, "unknown descr value");
//                     // esp_log_buffer_hex(GATTS_TAG, param->write.value, param->write.len);
//                 }
//             }
//         }
//         example_write_event_env(gatts_if, &a_prepare_write_env, param);
//         break;
//     }
//     case ESP_GATTS_EXEC_WRITE_EVT:
//         ESP_LOGI(GATTS_TAG, "ESP_GATTS_EXEC_WRITE_EVT");
//         esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
//         example_exec_write_event_env(&a_prepare_write_env, param);
//         break;

//     case ESP_GATTS_CREATE_EVT:
//         // ESP_LOGI(GATTS_TAG, "CREATE_SERVICE_EVT, status %d,  service_handle %d\n", param->create.status, param->create.service_handle);
//         gl_profile_tab[PROFILE_PROV_APP_ID].service_handle = param->create.service_handle;
//         gl_profile_tab[PROFILE_PROV_APP_ID].char_uuid.len = ESP_UUID_LEN_16;
//         gl_profile_tab[PROFILE_PROV_APP_ID].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID;

//         esp_ble_gatts_start_service(gl_profile_tab[PROFILE_PROV_APP_ID].service_handle);
//         a_property = ESP_GATT_CHAR_PROP_BIT_WRITE;
//         esp_err_t add_char_ret = esp_ble_gatts_add_char(gl_profile_tab[PROFILE_PROV_APP_ID].service_handle, &gl_profile_tab[PROFILE_PROV_APP_ID].char_uuid,
//                                                         ESP_GATT_PERM_WRITE,
//                                                         a_property,
//                                                         &gatts_demo_char1_val, NULL);
//         if (add_char_ret)
//         {
//             ESP_LOGE(GATTS_TAG, "add char failed, error code =%x", add_char_ret);
//         }
//         break;
//     case ESP_GATTS_ADD_CHAR_DESCR_EVT:
//         gl_profile_tab[PROFILE_PROV_APP_ID].descr_handle = param->add_char_descr.attr_handle;
//         // ESP_LOGI(GATTS_TAG, "ADD_DESCR_EVT, status %d, attr_handle %d, service_handle %d\n",
//                 //  param->add_char_descr.status, param->add_char_descr.attr_handle, param->add_char_descr.service_handle);
//         break;
//     case ESP_GATTS_START_EVT:
//         // ESP_LOGI(GATTS_TAG, "SERVICE_START_EVT, status %d, service_handle %d\n",
//                 //  param->start.status, param->start.service_handle);
//         break;
//     case ESP_GATTS_CONNECT_EVT:
//     {
//         esp_ble_conn_update_params_t conn_params = {0};
//         memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
//         /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
//         conn_params.latency = 0;
//         conn_params.max_int = 0x20; // max_int = 0x20*1.25ms = 40ms
//         conn_params.min_int = 0x10; // min_int = 0x10*1.25ms = 20ms
//         conn_params.timeout = 400;  // timeout = 400*10ms = 4000ms
//         // ESP_LOGI(GATTS_TAG, "ESP_GATTS_CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:",
//                 //  param->connect.conn_id,
//                 //  param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
//                 //  param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
//         gl_profile_tab[PROFILE_PROV_APP_ID].conn_id = param->connect.conn_id;
//         // start sent the update connection parameters to the peer device.
//         esp_ble_gap_update_conn_params(&conn_params);
//         break;
//     }
//     case ESP_GATTS_DISCONNECT_EVT:
//         ESP_LOGI(GATTS_TAG, "ESP_GATTS_DISCONNECT_EVT, disconnect reason 0x%x", param->disconnect.reason);
//         esp_ble_gap_start_advertising(&adv_params);
//         break;
//     default:
//         break;
//     }
// }

// static void gatts_uart_event_handler(esp_gatts_cb_event_t event,
//                                      esp_gatt_if_t gatts_if,
//                                      esp_ble_gatts_cb_param_t *param)
// {
//     switch (event)
//     {
//     case ESP_GATTS_REG_EVT:
//         ESP_LOGI(GATTS_TAG, "REGISTER_APP_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
//         gl_profile_tab[PROFILE_UART_APP_ID].service_id.is_primary = true;
//         gl_profile_tab[PROFILE_UART_APP_ID].service_id.id.inst_id = 0x00;
//         gl_profile_tab[PROFILE_UART_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_128;
//         memcpy(gl_profile_tab[PROFILE_UART_APP_ID].service_id.id.uuid.uuid.uuid128,
//                uart_service_uuid, ESP_UUID_LEN_128);

//         esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_UART_APP_ID].service_id,
//                                      GATTS_NUM_HANDLE_B);
//         break;

//     case ESP_GATTS_CREATE_EVT:
//         ESP_LOGI(GATTS_TAG, "CREATE_SERVICE_EVT, status %d, service_handle %d",
//                  param->create.status, param->create.service_handle);

//         gl_profile_tab[PROFILE_UART_APP_ID].service_handle = param->create.service_handle;

//         // Tx Characteristic
//         gl_profile_tab[PROFILE_UART_APP_ID].char_uuid.len = ESP_UUID_LEN_128;
//         memcpy(gl_profile_tab[PROFILE_UART_APP_ID].char_uuid.uuid.uuid128, uart_tx_uuid, ESP_UUID_LEN_128);

//         // Rx Characteristic
//         gl_profile_tab[PROFILE_UART_APP_ID].char1_uuid.len = ESP_UUID_LEN_128;
//         memcpy(gl_profile_tab[PROFILE_UART_APP_ID].char1_uuid.uuid.uuid128, uart_rx_uuid, ESP_UUID_LEN_128);

//         esp_ble_gatts_start_service(gl_profile_tab[PROFILE_UART_APP_ID].service_handle);

//         //Tx
//         esp_err_t add_tx_char_ret = esp_ble_gatts_add_char(gl_profile_tab[PROFILE_UART_APP_ID].service_handle,
//             &gl_profile_tab[PROFILE_UART_APP_ID].char_uuid,
//             ESP_GATT_PERM_WRITE | ESP_GATT_PERM_READ,
//             // ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
//             ESP_GATT_CHAR_PROP_BIT_WRITE  ,
//             // NULL, 
//             &gatts_demo_char1_val,
//             NULL);

//         //Rx
//         esp_err_t add_rx_char_ret = esp_ble_gatts_add_char(gl_profile_tab[PROFILE_UART_APP_ID].service_handle,
//                 &gl_profile_tab[PROFILE_UART_APP_ID].char1_uuid,
//                 ESP_GATT_PERM_WRITE | ESP_GATT_PERM_READ,
//                 ESP_GATT_CHAR_PROP_BIT_NOTIFY,
//                 // NULL, 
//                 &gatts_demo_char1_val,
//                 NULL);

//         if (add_tx_char_ret || add_rx_char_ret) { 
//             ESP_LOGE(GATTS_TAG, "UART PROFILE add char failed, error code = %x", add_tx_char_ret|add_rx_char_ret );
//         }
//         break;

//     case ESP_GATTS_ADD_CHAR_EVT:

 
//         // // gl_profile_tab[PROFILE_UART_APP_ID].char_handle = param->add_char.attr_handle; //tx
//         gl_profile_tab[PROFILE_UART_APP_ID].char1_handle = param->add_char.attr_handle; //rx

//         ESP_LOGI(GATTS_TAG, "UART PROFILE ADD_CHAR_EVT, attr_handle = %d", param->add_char.attr_handle);
        
//         esp_bt_uuid_t descr_uuid = {
//             .len = ESP_UUID_LEN_16,
//             .uuid = { .uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG },
//         };
    
//         esp_attr_value_t cccd_val = {
//             .attr_max_len = 2,
//             .attr_len = 2,
//             .attr_value = (uint8_t[]){0x00, 0x00},
//         };
    
//         esp_ble_gatts_add_char_descr( gl_profile_tab[PROFILE_UART_APP_ID].service_handle,
//                                       &descr_uuid,
//                                       ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
//                                       &cccd_val,
//                                       NULL);

//         break;

//     case ESP_GATTS_ADD_CHAR_DESCR_EVT:

//         gl_profile_tab[PROFILE_UART_APP_ID].descr_handle = param->add_char_descr.attr_handle;
//         ESP_LOGI(GATTS_TAG, "ADD_DESCR_EVT, handle = %d", param->add_char_descr.attr_handle);
        
//         break;

//     case ESP_GATTS_START_EVT:
//         // ESP_LOGI(GATTS_TAG, "SERVICE_START_EVT, status %d, service_handle %d\n",
//                 //  param->start.status, param->start.service_handle);
//         break;

//     case ESP_GATTS_WRITE_EVT:

//         ESP_LOGI(GATTS_TAG, "UART WRITE_EVT");
//         if (!param->write.is_prep) {
//             if(param->write.len > 0)
//             {
//                 ESP_LOGI(GATTS_TAG, "Received data:");
//                 esp_log_buffer_hex(GATTS_TAG, param->write.value, param->write.len);
//                 parse_command( param->write.value, param->write.len , "cmd");
//                 if((blecmd < 1) || (blecmd >= MAX_BLE_CMDS)){
//                     uart_cmd_handler_flag = false;
//                 }
//                 else
//                 {
//                     uart_cmd_handler_flag = true;
//                 }
    
//                 // ESP_LOGI(GATTS_TAG, "cmd : %s", property , property_JSON->valuestring);
    
    
//                 // esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id,
//                 //                             gl_profile_tab[PROFILE_UART_APP_ID].char1_handle,
//                 //                             param->write.len, param->write.value, false);
//             }

//         }
//         example_write_event_env(gatts_if, &a_prepare_write_env, param);

//         break;

//     case ESP_GATTS_EXEC_WRITE_EVT:

//         ESP_LOGI(GATTS_TAG, "ESP_GATTS_EXEC_WRITE_EVT");
//          esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
//         //  example_exec_write_event_env(&a_prepare_write_env, param);
//          uart_exec_write_event_env(&a_prepare_write_env,param);
        
//         break;

//     case ESP_GATTS_CONNECT_EVT:
//         esp_ble_conn_update_params_t conn_params = {0};
//         memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
//         /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
//         conn_params.latency = 0;
//         conn_params.max_int = 0x20; // max_int = 0x20*1.25ms = 40ms
//         conn_params.min_int = 0x10; // min_int = 0x10*1.25ms = 20ms
//         conn_params.timeout = 400;  // timeout = 400*10ms = 4000ms
//         ESP_LOGI(GATTS_TAG, "ESP_GATTS_CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:",
//                  param->connect.conn_id,
//                  param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
//                  param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
//         gl_profile_tab[PROFILE_UART_APP_ID].conn_id = param->connect.conn_id;
//         // start sent the update connection parameters to the peer device.
//         esp_ble_gap_update_conn_params(&conn_params);
//         gl_profile_tab[PROFILE_UART_APP_ID].conn_id = param->connect.conn_id;
//         break;

//     case ESP_GATTS_DISCONNECT_EVT:
//         ESP_LOGI(GATTS_TAG, "DEVICE DISCONNECTED");
//         esp_ble_gap_start_advertising(&adv_params);
//         break;

//     default:
//         break;
//     }
// }

// static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
// {
//     /* If event is register event, store the gatts_if for each profile */
//     if (event == ESP_GATTS_REG_EVT) {
//         if (param->reg.app_id == PROFILE_PROV_APP_ID) {
//             gl_profile_tab[PROFILE_PROV_APP_ID].gatts_if = gatts_if;
//         } else if (param->reg.app_id == PROFILE_UART_APP_ID) {
//             gl_profile_tab[PROFILE_UART_APP_ID].gatts_if = gatts_if;
//         }
//     }

//     /* If the gatts_if equal to profile A, call profile A cb handler,
//      * so here call each profile's callback */
//     do
//     {
//         int idx;
//         for (idx = 0; idx < PROFILE_NUM; idx++)
//         {
//             if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
//                 gatts_if == gl_profile_tab[idx].gatts_if)
//             {
//                 if (gl_profile_tab[idx].gatts_cb)
//                 {
//                     gl_profile_tab[idx].gatts_cb(event, gatts_if, param);
//                 }
//             }
//         }
//     } while (0);
// }

// static void start_ble_provisioning(void)
// {
//     esp_err_t ret;
//     ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

//     esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
//     ret = esp_bt_controller_init(&bt_cfg);
//     if (ret)
//     {
//         ESP_LOGE(GATTS_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
//         return;
//     }

//     ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
//     if (ret)
//     {
//         ESP_LOGE(GATTS_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
//         return;
//     }
//     ret = esp_bluedroid_init();
//     if (ret)
//     {
//         ESP_LOGE(GATTS_TAG, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
//         return;
//     }
//     ret = esp_bluedroid_enable();
//     if (ret)
//     {
//         ESP_LOGE(GATTS_TAG, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
//         return;
//     }

//     ret = esp_ble_gatts_register_callback(gatts_event_handler);
//     if (ret)
//     {
//         ESP_LOGE(GATTS_TAG, "gatts register error, error code = %x", ret);
//         return;
//     }
//     ret = esp_ble_gap_register_callback(gap_event_handler);
//     if (ret)
//     {
//         ESP_LOGE(GATTS_TAG, "gap register error, error code = %x", ret);
//         return;
//     }
//     ret = esp_ble_gatts_app_register(PROFILE_PROV_APP_ID);
//     if (ret)
//     {
//         ESP_LOGE(GATTS_TAG, "gatts app register error, error code = %x", ret);
//         return;
//     }

//     ret = esp_ble_gatts_app_register(PROFILE_UART_APP_ID);
//     if (ret)
//     {
//         ESP_LOGE(GATTS_TAG, "gatts app register error, error code = %x", ret);
//         return;
//     }

//     esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
//     if (local_mtu_ret)
//     {
//         ESP_LOGE(GATTS_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
//     }
// }


// void execute_command(void)
// {
//     switch (blecmd)
//     {
//     case WIFI_UPDATE:
//         ESP_LOGI(GATTS_TAG, "Updating WiFi credentials...");
//         send_ble_uart_notification(save_wifi_credentials(bleParameters[0], bleParameters[1]), NULL, 0);
//         ESP_LOGW(GATTS_TAG, "Rebooting device...");
//         esp_restart();
//         break;
//     case REBOOT:
//         ESP_LOGW(GATTS_TAG, "Rebooting device...");
//         send_ble_uart_notification(ESP_OK, NULL, 0);
//         esp_restart();
//         break;
//     case FACTORY_RESET:
//         ESP_LOGW(GATTS_TAG, "Performing application reset...");
//         send_ble_uart_notification(reset_device_and_erase_nvs(), NULL, 0);
//         esp_restart();
//         break;
//     case READ_INTERNAL_TEMP:
//         ESP_LOGI(GATTS_TAG, "Reading internal temperature...");
//         float tempInternal = get_ambient_temp();
//         send_ble_uart_notification(ESP_FAIL, (uint8_t*)&tempInternal, sizeof(tempInternal));
//         break;
//     case READ_TEMP:
//         ESP_LOGI(GATTS_TAG, "Reading external temperature...");
//         float tempObject = get_object_temp();
//         send_ble_uart_notification(ESP_OK, (uint8_t*)&tempObject, sizeof(tempObject));
//         break;
//     case READ_RELAY_STATE:
//         ESP_LOGI(GATTS_TAG, "Reading relay state...");
//         HEATER_STATE relayState = get_heater_state();
//         send_ble_uart_notification(ESP_OK, (uint8_t*)&relayState, sizeof(relayState));
//         break;
//     case SET_TARGET_TEMP:
//         ESP_LOGI(GATTS_TAG, "Setting target temperature to %d.%d", *(uint8_t*)bleParameters[0], *(uint8_t*)bleParameters[1]);
//         float targetTemp = (float)(*(uint8_t*)bleParameters[0]) + ((float)(*(uint8_t*)bleParameters[1]) / 100.0f);
//         set_heater_target_temp(targetTemp);
//         send_ble_uart_notification(ESP_OK, NULL, 0);
//         break;
//     case SET_RELAYS:
//         ESP_LOGI(GATTS_TAG, "Setting relay state to %d", *(uint8_t*)bleParameters[0]);
//         esp_err_t err = set_heater_state((HEATER_STATE)*(uint8_t*)bleParameters[0]);
//         send_ble_uart_notification(err, NULL, 0);
//         break;
    
//     default:
//         ESP_LOGE(GATTS_TAG, "Invalid command received: %X", blecmd);
//         break;
//     }
//     free(bleParameters[0]);
//     free(bleParameters[1]);
//     bleParameters[0] = NULL;
//     bleParameters[1] = NULL;
//     blecmd = NO_COMMAND;
//     uart_cmd_handler_flag = false;
// }


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
    // status_led_init();
    device_data_init();
    // button_init();

    // ir_sensor_init();
    // heater_interface_init();

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

    // while(1)
    // {
    //     // if(gpio_isr_handler_flag == true){
    //     //     start_softap_provisioning();
    //     // }
    //     if(uart_cmd_handler_flag)
    //     {
    //         execute_command();
    //     }
    //     vTaskDelay(pdMS_TO_TICKS(500));  // Delay for 1 second
    // }
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

