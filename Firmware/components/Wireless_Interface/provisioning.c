#include "Comm_Interface.h"
#include "ble_service_tables.h"
#include "defines.h"
#include "app.h"
#include "cJSON.h"

#define TAG "PROVISION_MANAGER"

uint16_t temp_char_handle = 0;
httpd_handle_t server = NULL;

// SoftAP
void start_softap_provisioning(void)
{
    set_led_effect(LED_EFFECT_BREATHING, BLUE ,LED_SPEED_FAST );
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
    set_led_effect(LED_EFFECT_BLINK, BLUE ,LED_SPEED_SLOW );
    start_webserver();

    set_device_status(1, PAIRING);
}

void stop_softap_provisioning(void)
{
    stop_webserver();
    
    // Validate saved credentials before attempting connection
    if (strlen(device_data.ssid) == 0 || strlen(device_data.password) == 0) {
        ESP_LOGE(DEVICE_TAG, "Invalid saved WiFi credentials - SSID or password is empty");
        return;
    }
    
    ESP_LOGI(DEVICE_TAG, "Connecting to saved WiFi: %s\tPassword: %s", device_data.ssid, device_data.password);
    wifi_init_sta(device_data.ssid, device_data.password);
    set_led_effect(LED_EFFECT_STEADY, GREEN ,NO_SPEED );
    set_device_status(0, PAIRING);
}

httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
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

httpd_handle_t stop_webserver(void)
{
    if (httpd_stop(&server) == ESP_OK)
    {
        ESP_LOGW(TAG WIFI_TAG, "HTTP Server stopped");
    }
    else
    {
        ESP_LOGE(TAG WIFI_TAG, "Failed to Stop HTTP server");
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
            send_wifi_status_json("failed", NULL, "Connection failed after retries");
            start_softap_provisioning();
        }
        else{
            set_led_effect(LED_EFFECT_BLINK, YELLOW ,LED_SPEED_SLOW );
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

        set_led_effect(LED_EFFECT_BLINK, GREEN ,LED_SPEED_SLOW );
        set_led_effect(LED_EFFECT_STEADY, GREEN ,NO_SPEED );
        set_device_status(0, PAIRING);

        // Exit BLE pairing mode and restore normal operation
        set_led_status_flag("pairing_mode_active", false);
        set_led_status_flag("normal_operation", true);

        // For development: Keep BLE advertising active even after WiFi connects
        ESP_LOGI(TAG WIFI_TAG, "Restarting BLE advertising for development access");
        startBLEAdvert();

        // Send JSON connected status
        send_wifi_status_json("connected", NULL, NULL);
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
            if ( param->write.handle == herschel_handle_table[IDX_CONTROL_POINT_VAL])
            {
                ESP_LOGD(GATTS_TAG, "calling 'uart'");
                parse_command( param->write.value, param->write.len , "uart");
            }
            else if ( param->write.handle == wifi_handle_table[IDX_WIFI_RX_VAL])
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
        if ( temp_char_handle == herschel_handle_table[IDX_CONTROL_POINT_VAL])
        {
            ESP_LOGD(GATTS_TAG, "calling 'ble'");
            parse_command( prepare_write_env->prepare_buf, prepare_write_env->prepare_len , "ble");
        }
        else if ( temp_char_handle == wifi_handle_table[IDX_WIFI_RX_VAL])
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

void parse_wifi_json_credentials(uint8_t *data, int len)
{
    char json_str[len + 1]; // Create a buffer with space for null terminator
    memcpy(json_str, data, len);
    json_str[len] = '\0'; // Ensure null termination

    ESP_LOGI(GATTS_TAG, "Received WiFi JSON: %s", json_str);

    // Parse JSON
    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL)
    {
        ESP_LOGE(GATTS_TAG, "Failed to parse WiFi credentials JSON");
        send_wifi_status_json("failed", NULL, "Invalid JSON format");
        return;
    }

    // Extract ssid and password
    cJSON *ssid_item = cJSON_GetObjectItem(json, "ssid");
    cJSON *password_item = cJSON_GetObjectItem(json, "pass");

    // Validate SSID
    if (!ssid_item || !cJSON_IsString(ssid_item) || ssid_item->valuestring == NULL)
    {
        ESP_LOGE(GATTS_TAG, "Missing or invalid SSID in JSON");
        send_wifi_status_json("failed", NULL, "Missing SSID");
        cJSON_Delete(json);
        return;
    }

    // Validate password
    if (!password_item || !cJSON_IsString(password_item) || password_item->valuestring == NULL)
    {
        ESP_LOGE(GATTS_TAG, "Missing or invalid password in JSON");
        send_wifi_status_json("failed", NULL, "Missing password");
        cJSON_Delete(json);
        return;
    }

    // Check SSID and password length limits
    if (strlen(ssid_item->valuestring) > WIFI_SSID_MAX_LEN - 1)
    {
        ESP_LOGE(GATTS_TAG, "SSID too long: %d characters (max %d)", strlen(ssid_item->valuestring), WIFI_SSID_MAX_LEN - 1);
        send_wifi_status_json("failed", NULL, "SSID too long");
        cJSON_Delete(json);
        return;
    }

    if (strlen(password_item->valuestring) > WIFI_PASS_MAX_LEN - 1)
    {
        ESP_LOGE(GATTS_TAG, "Password too long: %d characters (max %d)", strlen(password_item->valuestring), WIFI_PASS_MAX_LEN - 1);
        send_wifi_status_json("failed", NULL, "Password too long");
        cJSON_Delete(json);
        return;
    }

    ESP_LOGI(GATTS_TAG, "WiFi Credentials - SSID: %s", ssid_item->valuestring);

    // Save credentials and attempt connection
    save_wifi_credentials(ssid_item->valuestring, password_item->valuestring);

    // Send connecting status
    send_wifi_status_json("connecting", ssid_item->valuestring, NULL);

    // Double-check credentials before calling wifi_init_sta
    if (ssid_item->valuestring == NULL || password_item->valuestring == NULL) {
        ESP_LOGE(GATTS_TAG, "SSID or password valuestring is NULL after validation");
        send_wifi_status_json("failed", NULL, "Internal error");
        cJSON_Delete(json);
        return;
    }

    // Copy strings to local variables before freeing JSON memory
    char ssid_copy[WIFI_SSID_MAX_LEN];
    char password_copy[WIFI_PASS_MAX_LEN];
    strncpy(ssid_copy, ssid_item->valuestring, sizeof(ssid_copy) - 1);
    strncpy(password_copy, password_item->valuestring, sizeof(password_copy) - 1);
    ssid_copy[sizeof(ssid_copy) - 1] = '\0';  // Ensure null termination
    password_copy[sizeof(password_copy) - 1] = '\0';  // Ensure null termination

    cJSON_Delete(json); // Free memory

    // Initialize WiFi with copied credentials
    wifi_init_sta(ssid_copy, password_copy);
}

void send_wifi_status_json(const char *status, const char *ssid, const char *error)
{
    cJSON *json = cJSON_CreateObject();
    if (json == NULL)
    {
        ESP_LOGE(GATTS_TAG, "Failed to create WiFi status JSON object");
        return;
    }

    // Add status field
    cJSON *status_item = cJSON_CreateString(status);
    if (status_item == NULL || !cJSON_AddItemToObject(json, "status", status_item))
    {
        ESP_LOGE(GATTS_TAG, "Failed to add status to WiFi JSON");
        cJSON_Delete(json);
        return;
    }

    // Add optional fields based on status
    if (strcmp(status, "connected") == 0)
    {
        // Get IP address for connected status
        esp_netif_ip_info_t ip_info;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
        {
            char ip_str[16];
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
            cJSON *ip_item = cJSON_CreateString(ip_str);
            if (ip_item)
            {
                cJSON_AddItemToObject(json, "ip", ip_item);
            }
        }
    }
    else if (strcmp(status, "connecting") == 0 && ssid != NULL)
    {
        // Add SSID for connecting status
        cJSON *ssid_item = cJSON_CreateString(ssid);
        if (ssid_item)
        {
            cJSON_AddItemToObject(json, "ssid", ssid_item);
        }
    }
    else if (strcmp(status, "failed") == 0 && error != NULL)
    {
        // Add error description for failed status
        cJSON *error_item = cJSON_CreateString(error);
        if (error_item)
        {
            cJSON_AddItemToObject(json, "error", error_item);
        }
    }

    // Convert JSON to string
    char *json_string = cJSON_Print(json);
    if (json_string == NULL)
    {
        ESP_LOGE(GATTS_TAG, "Failed to convert WiFi status to JSON string");
        cJSON_Delete(json);
        return;
    }

    ESP_LOGI(GATTS_TAG, "Sending WiFi status JSON: %s", json_string);

    // Send via BLE notification on WiFi provisioning service
    esp_err_t err = esp_ble_gatts_send_indicate(gl_profile_tab[PROFILE_PROV_APP_ID].gatts_if,
                                                gl_profile_tab[PROFILE_PROV_APP_ID].conn_id,
                                                wifi_handle_table[IDX_WIFI_TX_VAL],
                                                strlen(json_string), (uint8_t*)json_string, false);
    if (err != ESP_OK)
    {
        ESP_LOGE(GATTS_TAG, "Failed to send WiFi status JSON notification: %s", esp_err_to_name(err));
    }

    // Clean up
    free(json_string);
    cJSON_Delete(json);
}



