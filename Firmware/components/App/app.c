#include "app.h"
#include "Comm_Interface.h"
#include <nvs_flash.h>
#include <esp_log.h>
#include "esp_timer.h"

#define TAG     DEVICE_TAG

device_data_t device_data = {0};

esp_err_t device_data_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE_DEVICEDATA, NVS_READONLY, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
        if(err == ESP_ERR_NVS_NOT_FOUND)
        {
            err = nvs_open(NVS_NAMESPACE_DEVICEDATA, NVS_READWRITE, &nvs);
            device_data.mode = MANUAL_MODE;
            device_data.target_temp = 0;
            device_data.duration_val = 0;
            nvs_set_i8(nvs, NVS_KEY_MODE, device_data.mode);
            nvs_set_blob(nvs, NVS_KEY_TARGET_TEMP, &device_data.target_temp, sizeof(device_data.target_temp));
            nvs_set_u8(nvs, NVS_KEY_DETECTION_DURATION, device_data.duration_val);
            ESP_LOGW(TAG, "Device mode not set. Setting to %s.", get_enum_name(ENUM_DEVICE_MODE, device_data.mode));
            ESP_LOGW(TAG, "Target temp not set. Setting to 0.");
            ESP_LOGW(TAG, "Presence detection duration val not set 0.");
            nvs_commit(nvs);
        }
        // return err;
    }
    else
    {
        err = nvs_get_i8(nvs, NVS_KEY_MODE, (int8_t*)&device_data.mode);
        if (err != ESP_OK)
        {
            device_data.mode = MANUAL_MODE;
            ESP_LOGE(TAG, "Device mode not set. Setting to %s.", get_enum_name(ENUM_DEVICE_MODE, device_data.mode));
            nvs_set_i8(nvs, NVS_KEY_MODE, device_data.mode);
            nvs_commit(nvs);
        }
        size_t target_temp_len = sizeof(device_data.target_temp);
        err = nvs_get_blob(nvs, NVS_KEY_TARGET_TEMP, &device_data.target_temp, &target_temp_len);
        if (err != ESP_OK)
        {
            device_data.target_temp = 0;
            ESP_LOGE(TAG, "Target temp not set. Setting to 0.");
            nvs_set_blob(nvs, NVS_KEY_TARGET_TEMP, &device_data.target_temp, sizeof(device_data.target_temp));
            nvs_commit(nvs);
        }
        err = nvs_get_u8(nvs, NVS_KEY_DETECTION_DURATION, &device_data.duration_val);
        if (err != ESP_OK)
        {
            device_data.duration_val = 0;
            ESP_LOGE(TAG, "Presence detection duration val not set 0.");
            nvs_set_u8(nvs, NVS_KEY_DETECTION_DURATION, device_data.duration_val);
            nvs_commit(nvs);
        }
    }

    ESP_LOGW(TAG, "Device mode: %s", get_enum_name(ENUM_DEVICE_MODE, device_data.mode));
    ESP_LOGW(TAG, "Target temp: %0.2f", device_data.target_temp);
    ESP_LOGW(TAG, "Presence detection duration val: %x", device_data.duration_val);

    nvs_close(nvs);

    err = load_wifi_credentials(device_data.ssid, device_data.password);
    if(err == ESP_OK)
    {
        ESP_LOGW(TAG, "WiFi credentials loaded.");
        ESP_LOGW(TAG, "SSID: %s", device_data.ssid);
        ESP_LOGW(TAG, "Password: %s", device_data.password);
    }

    ESP_LOGI(TAG, "Device data initialized.");

    return err;
}

/* Function for storing device values in nvs
 * @param op: operation to perform
              0: store device mode
              1: store target temperature
 */
esp_err_t device_data_set(uint8_t op)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE_DEVICEDATA, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        set_led_status_flag("error_nvs", true);
        return err;
    }
    if(op == 0)
    {
        err = nvs_set_i8(nvs, NVS_KEY_MODE, device_data.mode);
    }
    else if(op == 1)
    {
        err |= nvs_set_blob(nvs, NVS_KEY_TARGET_TEMP, &device_data.target_temp, sizeof(device_data.target_temp));
    }
    else if(op == 2)
    {
        err |= nvs_set_u8(nvs, NVS_KEY_DETECTION_DURATION, device_data.duration_val);
    }
    // else if(op == 2)
    // {
    //     err |= nvs_set_str(nvs, NVS_KEY_SSID, device_data.ssid);
    //     err |= nvs_set_str(nvs, NVS_KEY_PASSWORD, device_data.password);
    // }

    nvs_commit(nvs);
    nvs_close(nvs);

    return err;
}

esp_err_t reset_device_and_erase_nvs()
{
    // set_led_effect(LED_EFFECT_BLINK, PURPLE, LED_SPEED_FAST);
    set_led_status_flag("erase_nvs", true);
    esp_err_t err;

    // Erase all NVS partitions
    err = nvs_flash_erase();
    if (err != ESP_OK) {
        ESP_LOGE("RESET", "Failed to erase NVS: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI("RESET", "NVS erased, restarting device...");
    return err;
}

void set_target_temp(float target_temp)
{
    device_data.target_temp = target_temp;
    ESP_LOGD(TAG, "Target temperature set to: %.2f", device_data.target_temp);

    device_data_set(1);
}

float get_target_temp(void)
{
    return device_data.target_temp;
}

void set_detection_duration(uint8_t duration_val)
{
    // device_data.target_temp = target_temp;
    device_data.duration_val = duration_val;
    ESP_LOGD(TAG, "Presence Detection Duration time set to: %x", device_data.duration_val);


    device_data_set(2);
}

float get_detection_duration(void)
{
    return device_data.duration_val;
}

void set_device_mode(DEVICE_MODE mode)
{
    device_data.mode = mode;
    // ESP_LOGD(DEVICE_TAG, "Device mode set to: %s", get_enum_name(ENUM_DEVICE_MODE, device_data.mode));

    device_data_set(0);
}

DEVICE_MODE get_device_mode(void)
{
    return device_data.mode;
}

esp_err_t get_device_credentials(char *token)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE_DEVICE_HTTPS_DATA, NVS_READONLY, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG HTTPS_TAG, "No saved credentials found");
        return err;
    }

    size_t length = CREDENTIAL_VALUE;
    nvs_get_str(nvs, "token", token, &length);
    nvs_close(nvs);
    return ESP_OK;
}

esp_err_t save_device_credentials(const char *token)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE_DEVICE_HTTPS_DATA, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG HTTPS_TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return err;
    }

    nvs_set_str(nvs, "token", token);
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG HTTPS_TAG, "HTTPS Provisioning Token saved");
    return ESP_OK;
}

// Shared telemetry interval management
static int current_telemetry_interval = NORMAL_TELEMETRY_TIME;
static int64_t last_presence_detected_time = 0;

int calculate_telemetry_interval_seconds(bool presence_detected)
{
    int64_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds

    if (presence_detected) {
        current_telemetry_interval = PRESENCE_TELEMETRY_TIME; // 10 seconds
        last_presence_detected_time = current_time;
    } else {
        // Check how long since last presence detection
        if (current_time - last_presence_detected_time >= NO_ACTIVITY_TIME_MS) {
            // Gradually increase interval when idle
            current_telemetry_interval += IDLE_TELEMETRY_TIME; // +60s
            if (current_telemetry_interval > MAX_IDLE_INTERVAL) {
                current_telemetry_interval = MAX_IDLE_INTERVAL;
            }
        } else {
            // Return to normal interval when no presence but < 10min since last detection
            current_telemetry_interval = NORMAL_TELEMETRY_TIME; // 30 seconds
        }
    }

    return current_telemetry_interval;
}

void update_presence_detection_time(bool presence_detected)
{
    if (presence_detected) {
        last_presence_detected_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    }
}
