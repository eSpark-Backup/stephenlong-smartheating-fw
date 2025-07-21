#include "ThingsBoard_interface.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "Storage_interface.h"
#include "status_led.h"
#include "MLX90614_interface.h"
#include "LD2410_interface.hpp"
#include "esp_timer.h"
#include "Heater_interface.h"
#include "Comm_Interface.h"
#include "driver/gpio.h"

extern "C" {
#include "ble.h"
}

static bool check_memory_available(size_t required_bytes)
{
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap < required_bytes) {
        ESP_LOGW(HTTPS_TAG, "Low memory: %d bytes free, %d bytes required", free_heap, required_bytes);
        return false;
    }
    return true;
}


// Use shared telemetry interval constants from defines.h

extern const char ca_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const char ca_cert_pem_end[]   asm("_binary_ca_cert_pem_end");

esp_err_t thingsBoard_init(void)
{
    // Create the task for the HTTPS ThingsBoard Task
    if(xTaskCreate(thingsBoard_task, "ThingsBoard_Task", 1024*6, NULL, 1, NULL) != pdPASS)
    {
        ESP_LOGE(HTTPS_TAG, "Failed to create ThingsBoard task");
        // set_device_status(1, ERROR);
        set_led_effect(LED_EFFECT_BLINK, RED ,LED_SPEED_SLOW);
        // set_led_status_flag();
        return ESP_FAIL;
    }
    ESP_LOGD(HTTPS_TAG, "ThingsBoard task created successfully");

    return ESP_OK;
}

void thingsBoard_task(void *pvParameters)
{
    bool isDeviceProvisioned = false;
    char tokenKey[CREDENTIAL_VALUE];  // Buffer for token
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X%02X%02X%02X%02X%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if(get_device_thingsboard_credentials(tokenKey) == ESP_OK) isDeviceProvisioned = true;

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
                save_device_thingsboard_credentials(tokenKey);
                ESP_LOGD(HTTPS_TAG, "Provisioning successful. Token: %s", tokenKey);

                // Send device attributes after successful provisioning
                send_device_attributes(tokenKey);
                ESP_LOGD(HTTPS_TAG, "Device attributes sent after provisioning");
            } else {
                ESP_LOGD(HTTPS_TAG, "Provisioning failed. Retrying...");
                vTaskDelay(pdMS_TO_TICKS(3000));
                continue;
            }
        }

        bool presenceDetected = LD2410_getPresenceDetected();
        int telemetryTime;

        if (isDeviceProvisioned)
        {
            // Update presence detection time and get smart interval
            telemetryTime = calculate_telemetry_interval_seconds(presenceDetected);

            // Send telemetry regardless of presence state (but with adaptive timing)
            send_telemetry(tokenKey);

            ESP_LOGD(HTTPS_TAG, "Telemetry sent, next interval: %d seconds (presence: %s)",
                    telemetryTime, presenceDetected ? "detected" : "none");
        }
        else
        {
            ESP_LOGD(HTTPS_TAG, "Device not provisioned, skipping telemetry.");
            // Use normal interval when not provisioned
            telemetryTime = NORMAL_TELEMETRY_TIME;
        }

        vTaskDelay(pdMS_TO_TICKS(telemetryTime*1000));
    }

    vTaskDelete(NULL); // Clean exit (never reached)
}

esp_err_t provision_device(char *access_token_out, size_t token_size, char *mac_add)
{
    char buffer[1024] = {0};

    esp_http_client_config_t config = {
        .url = "https://iot.hi-infrastructure.net/api/v1/provision",
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
        .user_data = buffer,
    };
    config.cert_pem = ca_cert_pem_start;
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.timeout_ms = 15000;  // 15 second timeout for provisioning
    config.buffer_size = 1024;  // Smaller buffer for provisioning response
    config.buffer_size_tx = 512;  // Small TX buffer for provisioning request
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

void send_telemetry(const char *access_token)
{
    char url[128] = {0};
    snprintf(url, sizeof(url), "https://iot.hi-infrastructure.net/api/v1/%s/telemetry", access_token);

    char *json_data = build_telemetry_json();
    if (!json_data) {
        ESP_LOGE(HTTPS_TAG, "Failed to build telemetry JSON");
        return;
    }

    ESP_LOGD(HTTPS_TAG, "Telemetry JSON: %s", json_data);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
    };
    config.cert_pem = ca_cert_pem_start;
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.timeout_ms = 10000;  // 10 second timeout
    config.buffer_size = 2048;  // Optimize for ESP32-C6 memory
    config.buffer_size_tx = 1024;  // Smaller TX buffer for memory efficiency

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_data, strlen(json_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code >= 200 && status_code < 300) {
            ESP_LOGD(HTTPS_TAG, "Telemetry sent successfully, status: %d", status_code);
        } else {
            ESP_LOGW(HTTPS_TAG, "Telemetry HTTP error: %d", status_code);
        }
    } else {
        ESP_LOGE(HTTPS_TAG, "Error sending telemetry: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(json_data);
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    // HTTP event handling
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

char* build_telemetry_json(void)
{
    // Check if we have enough memory for JSON operations (estimate ~2KB needed)
    if (!check_memory_available(2048)) {
        ESP_LOGE(HTTPS_TAG, "Insufficient memory for telemetry JSON");
        return NULL;
    }

    cJSON *telemetry = cJSON_CreateObject();
    if (!telemetry) {
        ESP_LOGE(HTTPS_TAG, "Failed to create telemetry JSON object - out of memory");
        return NULL;
    }

    // Temperature Readings (per schema)
    cJSON *tempReadings = cJSON_CreateObject();
    cJSON_AddNumberToObject(tempReadings, "ambient", get_ambient_temp());
    cJSON_AddNumberToObject(tempReadings, "object", get_object_temp());
    cJSON_AddNumberToObject(tempReadings, "operative", get_operative_temp());
    cJSON_AddNumberToObject(tempReadings, "internal", get_internal_temp());
    cJSON_AddItemToObject(telemetry, "temperatureReadings", tempReadings);

    // Presence Detection (per schema)
    cJSON *presenceDetection = cJSON_CreateObject();
    cJSON_AddBoolToObject(presenceDetection, "detected", LD2410_getPresenceDetected());

    cJSON *distances = cJSON_CreateObject();
    cJSON_AddNumberToObject(distances, "moving", LD2410_getMovingDistance());
    cJSON_AddNumberToObject(distances, "stationary", LD2410_getStationaryDistance());
    cJSON_AddItemToObject(presenceDetection, "distances", distances);

    cJSON *signals = cJSON_CreateObject();
    cJSON_AddNumberToObject(signals, "moving", LD2410_getMovingEnergy());
    cJSON_AddNumberToObject(signals, "stationary", LD2410_getStationaryEnergy());
    cJSON_AddItemToObject(presenceDetection, "signals", signals);
    cJSON_AddItemToObject(telemetry, "presenceDetection", presenceDetection);

    // Element States (relay states per schema)
    cJSON *elementStates = cJSON_CreateArray();
    cJSON_AddItemToArray(elementStates, cJSON_CreateBool(gpio_get_level((gpio_num_t)HEATER_LOW_IO)));
    cJSON_AddItemToArray(elementStates, cJSON_CreateBool(gpio_get_level((gpio_num_t)HEATER_HIGH_IO)));
    cJSON_AddItemToObject(telemetry, "elementStates", elementStates);

    // Power Metrics (per schema) - using heater state
    cJSON *powerMetrics = cJSON_CreateObject();
    HEATER_STATE state = get_heater_state();
    float power = 0.0f;
    switch (state) {
        case HEATER_OFF: power = 0.0f; break;
        case HEATER_LOW: power = HEATING_LOW_WATTS; break;
        case HEATER_MID: power = HEATING_HIGH_WATTS; break;
        case HEATER_HIGH: power = HEATING_LOW_WATTS + HEATING_HIGH_WATTS; break;
        default: power = 0.0f; break;
    }
    cJSON_AddNumberToObject(powerMetrics, "currentUsage", power);
    cJSON_AddItemToObject(telemetry, "powerMetrics", powerMetrics);

    // Network Status (per schema)
    cJSON *networkStatus = cJSON_CreateObject();

    cJSON *wifi = cJSON_CreateObject();
    char ssid[33] = {0};
    uint8_t connected = 0;
    int8_t rssi = 0;
    uint32_t ip_address = 0;
    uint8_t internet_available = 0;

    get_wifi_status(ssid, &connected, &rssi, &ip_address, &internet_available);
    cJSON_AddBoolToObject(wifi, "connected", connected);
    cJSON_AddNumberToObject(wifi, "rssi", rssi);
    cJSON_AddStringToObject(wifi, "ssid", ssid);

    char ip_str[16];
    esp_ip4_addr_t ip_addr = { .addr = ip_address };
    snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_addr));
    cJSON_AddStringToObject(wifi, "ipAddress", ip_str);
    cJSON_AddItemToObject(networkStatus, "wifi", wifi);

    cJSON *bluetooth = cJSON_CreateObject();
    cJSON_AddBoolToObject(bluetooth, "enabled", true);  // BLE always enabled in this firmware
    cJSON_AddBoolToObject(bluetooth, "advertising", adv_config_done == 0);  // Actual advertising status
    cJSON_AddBoolToObject(bluetooth, "connected", has_active_ble_clients());  // Actual BLE connection status
    cJSON_AddItemToObject(networkStatus, "bluetooth", bluetooth);
    cJSON_AddItemToObject(telemetry, "networkStatus", networkStatus);

    // System Health (per schema)
    cJSON *systemHealth = cJSON_CreateObject();
    cJSON_AddNumberToObject(systemHealth, "uptime", esp_timer_get_time() / 1000000);  // seconds

    cJSON *memory = cJSON_CreateObject();
    uint32_t free_heap = esp_get_free_heap_size();
    cJSON_AddNumberToObject(memory, "free", free_heap / 1024);  // KB
    // Calculate usage percentage (ESP32-C6 has 512KB HP SRAM + 16KB LP SRAM)
    uint32_t total_ram = 512 * 1024;  // 512KB HP SRAM
    cJSON_AddNumberToObject(memory, "usage", (int)(((total_ram - free_heap) * 100) / total_ram));
    cJSON_AddItemToObject(systemHealth, "memory", memory);

    cJSON *sensors = cJSON_CreateObject();
    // Check actual sensor status by validating sensor data
    float ambient_temp = get_ambient_temp();
    float object_temp = get_object_temp();
    bool mlx90614_ok = (ambient_temp > MIN_TEMP_VAL && ambient_temp < MAX_TEMP_VAL &&
                        object_temp > MIN_TEMP_VAL && object_temp < MAX_TEMP_VAL);

    // LD2410 status check - sensor is ok if it returns valid distance data
    uint16_t moving_dist = LD2410_getMovingDistance();
    uint16_t stationary_dist = LD2410_getStationaryDistance();
    bool ld2410_ok = (moving_dist < 9999 && stationary_dist < 9999);  // 9999 typically means no target

    cJSON_AddBoolToObject(sensors, "mlx90614Connected", mlx90614_ok);
    cJSON_AddBoolToObject(sensors, "ld2410Connected", ld2410_ok);
    cJSON_AddItemToObject(systemHealth, "sensors", sensors);
    cJSON_AddItemToObject(telemetry, "systemHealth", systemHealth);

    char *json_string = cJSON_Print(telemetry);
    cJSON_Delete(telemetry);

    if (!json_string) {
        ESP_LOGE(HTTPS_TAG, "Failed to serialize telemetry JSON - out of memory");
        return NULL;
    }

    ESP_LOGD(HTTPS_TAG, "Telemetry JSON size: %d bytes", strlen(json_string));
    return json_string;
}

char* build_device_attributes_json(void)
{
    // Check if we have enough memory for JSON operations (estimate ~1KB needed)
    if (!check_memory_available(1024)) {
        ESP_LOGE(HTTPS_TAG, "Insufficient memory for device attributes JSON");
        return NULL;
    }

    cJSON *attributes = cJSON_CreateObject();
    if (!attributes) {
        ESP_LOGE(HTTPS_TAG, "Failed to create attributes JSON object - out of memory");
        return NULL;
    }

    // Device Identity (per schema)
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char deviceId[18];
    snprintf(deviceId, sizeof(deviceId), "%02X%02X%02X%02X%02X%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    cJSON_AddStringToObject(attributes, "deviceId", deviceId);
    cJSON_AddStringToObject(attributes, "deviceName", BLE_DEVICE_NAME);
    cJSON_AddStringToObject(attributes, "deviceModel", DEVICE_MODEL);
    cJSON_AddStringToObject(attributes, "firmwareVersion", FIRMWARE_VERSION);
    cJSON_AddStringToObject(attributes, "serialNumber", deviceId);

    // Hardware Capabilities (per schema)
    cJSON *hardwareCapabilities = cJSON_CreateObject();

    cJSON *elements = cJSON_CreateArray();

    cJSON *element0 = cJSON_CreateObject();
    cJSON_AddStringToObject(element0, "id", LOW_ELEMENT_ID);
    cJSON_AddStringToObject(element0, "name", LOW_ELEMENT_NAME);
    cJSON_AddNumberToObject(element0, "wattage", HEATING_LOW_WATTS);
    cJSON_AddItemToArray(elements, element0);

    cJSON *element1 = cJSON_CreateObject();
    cJSON_AddStringToObject(element1, "id", HIGH_ELEMENT_ID);
    cJSON_AddStringToObject(element1, "name", HIGH_ELEMENT_NAME);
    cJSON_AddNumberToObject(element1, "wattage", HEATING_HIGH_WATTS);
    cJSON_AddItemToArray(elements, element1);

    cJSON_AddItemToObject(hardwareCapabilities, "elements", elements);
    cJSON_AddBoolToObject(hardwareCapabilities, "hasPresenceSensor", true);
    cJSON_AddBoolToObject(hardwareCapabilities, "hasTemperatureSensor", true);
    cJSON_AddBoolToObject(hardwareCapabilities, "hasBluetooth", true);
    cJSON_AddStringToObject(hardwareCapabilities, "ratedVoltage", RATED_VOLTAGE);
    cJSON_AddStringToObject(hardwareCapabilities, "installationType", INSTALLATION_TYPE);
    cJSON_AddItemToObject(attributes, "hardwareCapabilities", hardwareCapabilities);

    char *json_string = cJSON_Print(attributes);
    cJSON_Delete(attributes);

    if (!json_string) {
        ESP_LOGE(HTTPS_TAG, "Failed to serialize attributes JSON - out of memory");
        return NULL;
    }

    ESP_LOGD(HTTPS_TAG, "Device attributes JSON size: %d bytes", strlen(json_string));
    return json_string;
}

void send_device_attributes(const char *access_token)
{
    char url[128] = {0};
    snprintf(url, sizeof(url), "https://iot.hi-infrastructure.net/api/v1/%s/attributes", access_token);

    char *json_data = build_device_attributes_json();
    if (!json_data) {
        ESP_LOGE(HTTPS_TAG, "Failed to build device attributes JSON");
        return;
    }

    ESP_LOGD(HTTPS_TAG, "Device Attributes JSON: %s", json_data);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
    };
    config.cert_pem = ca_cert_pem_start;
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.timeout_ms = 10000;  // 10 second timeout
    config.buffer_size = 2048;  // Optimize for ESP32-C6 memory
    config.buffer_size_tx = 1024;  // Smaller TX buffer for memory efficiency

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_data, strlen(json_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code >= 200 && status_code < 300) {
            ESP_LOGD(HTTPS_TAG, "Device attributes sent successfully, status: %d", status_code);
        } else {
            ESP_LOGW(HTTPS_TAG, "Device attributes HTTP error: %d", status_code);
        }
    } else {
        ESP_LOGE(HTTPS_TAG, "Error sending device attributes: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(json_data);
}

int64_t get_current_milliseconds()
{
    return esp_timer_get_time() / 1000; // Convert microseconds to milliseconds
}