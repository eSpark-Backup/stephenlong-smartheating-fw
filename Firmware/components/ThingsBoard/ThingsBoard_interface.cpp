#include "ThingsBoard_interface.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "app.h"
#include "status_led.h"
#include "MLX90614_interface.h"
#include "esp_timer.h"


#define NORMAL_TEL_TIME 30
#define PRESENCE_TEL_TIME 10
#define IDLE_TEL_TIME 60
#define NO_ACTIVITY_TIME 10*1000*60
#define MAX_IDLE_INTERVAL NO_ACTIVITY_TIME

extern const char ca_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const char ca_cert_pem_end[]   asm("_binary_ca_cert_pem_end");

esp_err_t thingsBoard_init(void)
{
    // Create the task for the HTTPS ThingsBoard Task
    if(xTaskCreate(thingsBoard_task, "ThingsBoard_Task", 1024*6, NULL, 1, NULL) != pdPASS)
    {
        ESP_LOGE(HTTPS_TAG, "Failed to create ThingsBoard task");
        set_led_effect(LED_EFFECT_BLINK, RED ,LED_SPEED_SLOW);
        set_device_status(1, ERROR);
        return ESP_FAIL;
    }
    ESP_LOGD(HTTPS_TAG, "ThingsBoard task created successfully");

    return ESP_OK;
}

void thingsBoard_task(void *pvParameters)
{
    int telemetryTime = NORMAL_TEL_TIME;
    unsigned long presenceDetectedTime = 0;
    bool isDeviceProvisioned = false;
    char tokenKey[CREDENTIAL_VALUE];  // Buffer for token
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X%02X%02X%02X%02X%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if(get_device_credentials(tokenKey) == ESP_OK) isDeviceProvisioned = true;

    while (true)
    {
        // Ensure Wi-Fi is connected
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
            ESP_LOGD(HTTPS_TAG, "Wi-Fi not connected. Waiting...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Provision the device only once
        if (!isDeviceProvisioned) {
            if (provision_device(tokenKey, sizeof(tokenKey), mac_str) == ESP_OK) {
                isDeviceProvisioned = true;
                save_device_credentials(tokenKey);
                ESP_LOGD(HTTPS_TAG, "Provisioning successful. Token: %s", tokenKey);
            } else {
                ESP_LOGD(HTTPS_TAG, "Provisioning failed. Retrying...");
                vTaskDelay(pdMS_TO_TICKS(3000));
                continue;
            }
        }

        LD2410_Data pd = getLD2410_Data();
        if (isDeviceProvisioned) 
        {
            if (pd.isPresenceTrue == DETECTED)
            {
                send_telemetry(tokenKey, pd); // Send telemetry when presence detected
                telemetryTime = PRESENCE_TEL_TIME; // e.g. 10s
                presenceDetectedTime = get_current_milliseconds(); // update last detected time
            }
            else
            {
                if (get_current_milliseconds() - presenceDetectedTime >= NO_ACTIVITY_TIME)
                {
                    telemetryTime += IDLE_TEL_TIME; // e.g. +60s
                    if (telemetryTime > MAX_IDLE_INTERVAL)
                        telemetryTime = MAX_IDLE_INTERVAL;
                }
                else
                {
                    telemetryTime = NORMAL_TEL_TIME; // e.g. 30s when no presence < 10min
                }
                ESP_LOGD(HTTPS_TAG, "cur time: %lld = last detection: %ld\n",get_current_milliseconds(), presenceDetectedTime);
                send_telemetry(tokenKey, pd); // Still send telemetry periodically in no-presence state
            }
        }
        else
        {
            ESP_LOGD(HTTPS_TAG, "Device not provisioned, skipping telemetry.");
        }

        vTaskDelay(pdMS_TO_TICKS(telemetryTime*1000));
    }

    vTaskDelete(NULL); // Clean exit (never reached)
}

esp_err_t provision_device(char *access_token_out, size_t token_size, char *mac_add)
{
    char buffer[1024] = {0};
    int total_read = 0;
    int read_len;
    
    esp_http_client_config_t config = {
        .url = "https://iot.hi-infrastructure.net/api/v1/provision",
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
        .user_data = buffer,
    };
    config.cert_pem = ca_cert_pem_start;
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    esp_http_client_handle_t client = esp_http_client_init(&config);

    char post_data[512];

    snprintf(post_data, sizeof(post_data),
        "{"
            "\"deviceName\":\"%s\","
            "\"provisionDeviceKey\":\"euwcit0szc9i3qagy4is\","
            "\"provisionDeviceSecret\":\"d5bvd3u7s31z20wluf02\""
        "}",
    mac_add);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        ESP_LOGD(HTTPS_TAG, "HTTP status code: %d - Content length: %d\nFull provisioning response:%s", status_code, content_length, buffer);

        cJSON *json = cJSON_Parse(buffer);
        if (json) {
            const cJSON *status = cJSON_GetObjectItem(json, "status");
            if (status && strcmp(status->valuestring, "SUCCESS") == 0) {
                const cJSON *val = cJSON_GetObjectItem(json, "credentialsValue");
                if (val && val->valuestring && strlen(val->valuestring) > 0) {
                    strncpy(access_token_out, val->valuestring, token_size);
                    cJSON_Delete(json);
                    esp_http_client_cleanup(client);
                    return ESP_OK;
                } else {
                    ESP_LOGD(HTTPS_TAG, "credentialsValue missing despite SUCCESS.");
                }
            } else {
                const cJSON *errorMsg = cJSON_GetObjectItem(json, "errorMsg");
                ESP_LOGD(HTTPS_TAG, "Provisioning failed: %s", errorMsg ? errorMsg->valuestring : "Unknown reason");
            }
            cJSON_Delete(json);
        } else {
            ESP_LOGD(HTTPS_TAG, "Failed to parse JSON.");
        }
    } else {
        ESP_LOGD(HTTPS_TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return ESP_FAIL;
}

void send_telemetry(const char *access_token, LD2410_Data presData)
{
    char url[128] = {0};
    snprintf(url, sizeof(url), "https://iot.hi-infrastructure.net/api/v1/%s/telemetry", access_token);
    
    char json_data[256] = {0};
    MLX_Temp irSensTemp {.amb_temp = -1, .obj_temp = -1};
    irSensTemp = getTempValues();
    
    snprintf(json_data, sizeof(json_data), "{\"Ambient_Temp\": %.2f, \"Object_Temp\": %.2f, \"Presence\": %d, \"Presence_Distance\": %" PRIu32"}", irSensTemp.amb_temp, irSensTemp.obj_temp, presData.isPresenceTrue, presData.presence);
    ESP_LOGD(HTTPS_TAG, "json data: %s\n", json_data);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
    };
    config.cert_pem = ca_cert_pem_start;
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_data, strlen(json_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGD(HTTPS_TAG, "HTTP POST Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGD(HTTPS_TAG, "Error sending telemetry: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(HTTPS_TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(HTTPS_TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(HTTPS_TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(HTTPS_TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // ✅ Append data to user buffer
                char *buf = (char *)evt->user_data;
                strncat(buf, (const char *)evt->data, evt->data_len);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            
            break;
        case HTTP_EVENT_DISCONNECTED:
            
            break;
        case HTTP_EVENT_REDIRECT:
            break;
    }
    return ESP_OK;
}

int64_t get_current_milliseconds()
{
    return esp_timer_get_time() / 1000; // Convert microseconds to milliseconds
}