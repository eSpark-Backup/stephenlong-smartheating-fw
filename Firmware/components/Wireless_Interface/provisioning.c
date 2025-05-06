#include "Comm_Interface.h"

#include "cJSON.h"

#define TAG "PROVISION_MANAGER"

uint16_t temp_char_handle = 0;

// SoftAP
void start_softap_provisioning(void)
{
    // gpio_isr_handler_flag = false;
    wifi_config_t wifi_config = {
        .ap = {
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK}};
    strncpy((char *)wifi_config.ap.ssid, SSID_WIFI, sizeof(wifi_config.ap.ssid));
    strcpy((char *)wifi_config.ap.password, PASS_WIFI);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG WIFI_TAG, "SoftAP mode started. Go to http://192.168.4.1/");
    start_webserver();

    set_device_status(1, PAIRING);
}

httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t uri_get = {.uri = "/", .method = HTTP_GET, .handler = get_handler};
        httpd_uri_t uri_post = {.uri = "/set", .method = HTTP_POST, .handler = post_handler};
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_post);
    }
    else
    {
        ESP_LOGI(TAG WIFI_TAG, "Error setting up http web server");
    }
    return server;
}

void decode_plus_to_space(char *str)
{
    while (*str)
    {
        if (*str == '+')
        {
            *str = ' ';
        }
        str++;
    }
}

esp_err_t post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char ssid[WIFI_SSID_MAX_LEN] = {0}, password[WIFI_PASS_MAX_LEN] = {0};
    httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid));
    httpd_query_key_value(buf, "password", password, sizeof(password));

    if (strlen(ssid) > 0 && strlen(password) > 0)
    {
        decode_plus_to_space(ssid);
        decode_plus_to_space(password);
        save_wifi_credentials(ssid, password);
        ESP_LOGI(TAG WIFI_TAG, "Received SSID: %s, Password: %s", ssid, password);
        httpd_resp_send(req, "WiFi credentials received. Rebooting...", HTTPD_RESP_USE_STRLEN);
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }
    else
    {
        httpd_resp_send(req, "Invalid input. Try again.", HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}

esp_err_t get_handler(httpd_req_t *req)
{
    const char *html = "<html>\
        <head><title>ESP32 WiFi Provisioning</title></head>\
        <body>\
        <h2>Enter WiFi Credentials</h2>\
        <form action='/set' method='POST'>\
            SSID: <input type='text' name='ssid'><br>\
            Password: <input type='password' name='password'><br>\
            <input type='submit' value='Submit'>\
        </form>\
        </body></html>";
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    static int s_retry_num = 0;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
        // set_device_status(1, PAIRING);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num > AP_RECONN_ATTEMPTS)
        {
            ESP_LOGI(TAG WIFI_TAG, "Starting SoftAP for provisioning");
            start_softap_provisioning();
        }
        else{
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG WIFI_TAG, "connect to the AP fail");
            ESP_LOGI(TAG WIFI_TAG, "retry to connect to the AP");

        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;

        set_device_status(0, PAIRING);
    }
}

// BLE
void write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
{
    esp_gatt_status_t status = ESP_GATT_OK;
    if (param->write.need_rsp)
    {
        if (param->write.is_prep)
        {
            temp_char_handle = param->write.handle;
            if (prepare_write_env->prepare_buf == NULL)
            {
                prepare_write_env->prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE * sizeof(uint8_t));
                prepare_write_env->prepare_len = 0;
                if (prepare_write_env->prepare_buf == NULL)
                {
                    ESP_LOGE(GATTS_TAG, "Gatt_server prep no mem\n");
                    status = ESP_GATT_NO_RESOURCES;
                }
            }
            else
            {
                if (param->write.offset > PREPARE_BUF_MAX_SIZE)
                {
                    status = ESP_GATT_INVALID_OFFSET;
                }
                else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE)
                {
                    status = ESP_GATT_INVALID_ATTR_LEN;
                }
            }

            esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
            gatt_rsp->attr_value.len = param->write.len;
            gatt_rsp->attr_value.handle = param->write.handle;
            gatt_rsp->attr_value.offset = param->write.offset;
            gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
            memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
            esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
            if (response_err != ESP_OK)
            {
                ESP_LOGE(GATTS_TAG, "Send response error\n");
            }
            free(gatt_rsp);
            if (status != ESP_GATT_OK)
            {
                return;
            }
            memcpy(prepare_write_env->prepare_buf + param->write.offset,
                   param->write.value,
                   param->write.len);
            prepare_write_env->prepare_len += param->write.len;

        }
        else
        {
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
        }

        if(!param->write.is_prep)
        {
            // ESP_LOGI(GATTS_TAG, "param->write.conn_id %d", param->write.conn_id);
            // ESP_LOGI(GATTS_TAG, "param->write.handle %d", param->write.handle);
            // ESP_LOGI(GATTS_TAG, "gl_profile_tab[PROFILE_UART_APP_ID].descr_handle %d", gl_profile_tab[PROFILE_UART_APP_ID].descr_handle);
            // ESP_LOGI(GATTS_TAG, "gl_profile_tab[PROFILE_PROV_APP_ID].descr_handle %d", gl_profile_tab[PROFILE_PROV_APP_ID].descr_handle);
            // ESP_LOGI(GATTS_TAG, "gl_profile_tab[PROFILE_UART_APP_ID].char_handle[0] %d", gl_profile_tab[PROFILE_UART_APP_ID].char_handle[0] );
            // ESP_LOGI(GATTS_TAG, "gl_profile_tab[PROFILE_UART_APP_ID].char_handle[1] %d", gl_profile_tab[PROFILE_UART_APP_ID].char_handle[1] );
            // ESP_LOGI(GATTS_TAG, "gl_profile_tab[PROFILE_PROV_APP_ID].char_handle[0] %d", gl_profile_tab[PROFILE_PROV_APP_ID].char_handle[0] );
            if ( param->write.handle == gl_profile_tab[PROFILE_UART_APP_ID].char_handle[0])
            {
                ESP_LOGD(GATTS_TAG, "calling 'cmd'");
                parse_command( param->write.value, param->write.len , "cmd");
            }
            else if ( param->write.handle == gl_profile_tab[PROFILE_PROV_APP_ID].char_handle[0])
            {
                ESP_LOGD(GATTS_TAG, "calling 'wifi'");
                parse_command( param->write.value, param->write.len , "wifi");
            }
        }
    }
}

void exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
{
    if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC)
    {
        // ESP_LOGI(GATTS_TAG, "param->write.conn_id %d", param->write.conn_id);
        // ESP_LOGI(GATTS_TAG, "param->write.handle %d", param->write.handle);
        // ESP_LOGI(GATTS_TAG, "gl_profile_tab[PROFILE_UART_APP_ID].descr_handle %d", gl_profile_tab[PROFILE_UART_APP_ID].descr_handle);
        // ESP_LOGI(GATTS_TAG, "gl_profile_tab[PROFILE_PROV_APP_ID].descr_handle %d", gl_profile_tab[PROFILE_PROV_APP_ID].descr_handle);
        // ESP_LOGI(GATTS_TAG, "gl_profile_tab[PROFILE_UART_APP_ID].char_handle[0] %d", gl_profile_tab[PROFILE_UART_APP_ID].char_handle[0] );
        // ESP_LOGI(GATTS_TAG, "gl_profile_tab[PROFILE_UART_APP_ID].char_handle[1] %d", gl_profile_tab[PROFILE_UART_APP_ID].char_handle[1] );
        // ESP_LOGI(GATTS_TAG, "gl_profile_tab[PROFILE_PROV_APP_ID].char_handle[0] %d", gl_profile_tab[PROFILE_PROV_APP_ID].char_handle[0] );
        if ( temp_char_handle == gl_profile_tab[PROFILE_UART_APP_ID].char_handle[0])
        // if (param->write.conn_id == gl_profile_tab[PROFILE_UART_APP_ID].conn_id)
        {
            ESP_LOGD(GATTS_TAG, "calling 'cmd'");
            parse_command( prepare_write_env->prepare_buf, prepare_write_env->prepare_len , "cmd");
        }
        else if ( temp_char_handle == gl_profile_tab[PROFILE_PROV_APP_ID].char_handle[0])
        {
            ESP_LOGD(GATTS_TAG, "calling 'wifi'");
            parse_command( prepare_write_env->prepare_buf, prepare_write_env->prepare_len , "wifi");
        }
    }
    else
    {
        ESP_LOGI(GATTS_TAG, "ESP_GATT_PREP_WRITE_CANCEL");
    }
    if (prepare_write_env->prepare_buf)
    {
        free(prepare_write_env->prepare_buf);
        prepare_write_env->prepare_buf = NULL;
    }
    prepare_write_env->prepare_len = 0;
}

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

// char * parse_JSON(uint8_t *data, int len , const char *property)
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