#include "Storage_interface.h"
#include "Comm_Interface.h"
#include <nvs_flash.h>
#include <esp_log.h>
#include "esp_timer.h"
#include "driver/gpio.h"

#define TAG     DEVICE_TAG STORAGE_TAG

device_data_t device_data = {0};
energy_tracking_t energy_data = {
    .energy_consumed_kwh = 0.0f,
    .heating_time_seconds = 0,
    .last_update_time = 0,
    .is_initialized = false
};

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
            ESP_LOGW(TAG, "Presence detection duration not set. Setting to 0.");
            nvs_commit(nvs);
            nvs_close(nvs);
        }
        // return err;
    }
    else
    {
        err = nvs_open(NVS_NAMESPACE_DEVICEDATA, NVS_READWRITE, &nvs);

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
        ESP_LOGW(TAG, "Presence detection duration val: %x %d", device_data.duration_val,err);
        if (err != ESP_OK)
        {
            device_data.duration_val = 100;
            ESP_LOGE(TAG, "Presence detection duration not set. Setting to 0.");
            nvs_set_u8(nvs, NVS_KEY_DETECTION_DURATION, device_data.duration_val);
            nvs_commit(nvs);
        }
        nvs_close(nvs);
    }

    ESP_LOGW(TAG, "Device mode: %s", get_enum_name(ENUM_DEVICE_MODE, device_data.mode));
    ESP_LOGW(TAG, "Target temp: %0.2f", device_data.target_temp);
    ESP_LOGW(TAG, "Presence detection duration val: %x", device_data.duration_val);

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
              2: store presence detection duration value
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

    nvs_commit(nvs);
    nvs_close(nvs);

    return err;
}

esp_err_t reset_device_and_erase_nvs()
{
    set_led_status_flag("erase_nvs", true);
    esp_err_t err;

    // Erase all NVS partitions
    err = nvs_flash_erase();
    if (err != ESP_OK) {
        ESP_LOGE(TAG "_RESET", "Failed to erase NVS: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG "_RESET", "NVS erased, restarting device...");
    vTaskDelay(pdMS_TO_TICKS(10000)); // Allow time for LED to indicate erase
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
    ESP_LOGD(TAG, "Device mode set to: %s", get_enum_name(ENUM_DEVICE_MODE, device_data.mode));

    device_data_set(0);
}

DEVICE_MODE get_device_mode(void)
{
    return device_data.mode;
}

esp_err_t get_device_thingsboard_credentials(char *token)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE_DEVICE_HTTPS_DATA, NVS_READONLY, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG HTTPS_TAG, "No saved credentials found");
#ifdef DEVICE_TEST
        char test_token[CREDENTIAL_VALUE] = "test_token_1234567890";
        save_device_thingsboard_credentials(test_token);
#endif
        return err;
    }

    size_t length = CREDENTIAL_VALUE;
    nvs_get_str(nvs, NVS_KEY_ACCESS_TOKEN, token, &length);
    nvs_close(nvs);

    ESP_LOGW(TAG, "HTTPS Provisioning Token loaded: %s", token);
    return ESP_OK;
}

esp_err_t save_device_thingsboard_credentials(const char *token)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE_DEVICE_HTTPS_DATA, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG HTTPS_TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return err;
    }

    nvs_set_str(nvs, NVS_KEY_ACCESS_TOKEN, token);
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG HTTPS_TAG, "HTTPS Provisioning Token saved");
    return ESP_OK;
}

esp_err_t energy_data_init(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE_ENERGYDATA, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGI(DEVICE_TAG, "Energy NVS namespace not found, initializing with defaults");
        energy_data.energy_consumed_kwh = 0.0f;
        energy_data.heating_time_seconds = 0;
        energy_data.last_update_time = esp_timer_get_time();
        energy_data.is_initialized = true;
        energy_data_set(255); // Store all defaults
        return ESP_OK;
    }

    size_t required_size = sizeof(float);
    err = nvs_get_blob(nvs, NVS_KEY_ENERGY_CONSUMED, &energy_data.energy_consumed_kwh, &required_size);
    if (err != ESP_OK) {
        energy_data.energy_consumed_kwh = 0.0f;
    }

    required_size = sizeof(uint32_t);
    err = nvs_get_blob(nvs, NVS_KEY_HEATING_TIME, &energy_data.heating_time_seconds, &required_size);
    if (err != ESP_OK) {
        energy_data.heating_time_seconds = 0;
    }

    required_size = sizeof(int64_t);
    err = nvs_get_blob(nvs, NVS_KEY_LAST_UPDATE_TIME, &energy_data.last_update_time, &required_size);
    if (err != ESP_OK) {
        energy_data.last_update_time = esp_timer_get_time();
    }

    energy_data.is_initialized = true;
    nvs_close(nvs);

    ESP_LOGI(DEVICE_TAG, "Energy data loaded: %.3f kWh, %lu seconds heating time",
             energy_data.energy_consumed_kwh, energy_data.heating_time_seconds);
    return ESP_OK;
}

/* Function for storing energy values in nvs
 * @param op: operation to perform
              0: store energy consumed
              1: store heating time
              2: store last update time
              255: store all parameters
 */
esp_err_t energy_data_set(uint8_t op)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE_ENERGYDATA, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        set_led_status_flag("error_nvs", true);
        return err;
    }
    if(op == 0)
    {
        err = nvs_set_blob(nvs, NVS_KEY_ENERGY_CONSUMED, &energy_data.energy_consumed_kwh, sizeof(float));
    }
    else if(op == 1)
    {
        err |= nvs_set_blob(nvs, NVS_KEY_HEATING_TIME, &energy_data.heating_time_seconds, sizeof(uint32_t));
    }
    else if(op == 2)
    {
        err |= nvs_set_blob(nvs, NVS_KEY_LAST_UPDATE_TIME, &energy_data.last_update_time, sizeof(int64_t));
    }
    else if(op == 255)
    {
        err |= nvs_set_blob(nvs, NVS_KEY_ENERGY_CONSUMED, &energy_data.energy_consumed_kwh, sizeof(float));
        err |= nvs_set_blob(nvs, NVS_KEY_HEATING_TIME, &energy_data.heating_time_seconds, sizeof(uint32_t));
        err |= nvs_set_blob(nvs, NVS_KEY_LAST_UPDATE_TIME, &energy_data.last_update_time, sizeof(int64_t));
    }

    nvs_commit(nvs);
    nvs_close(nvs);

    return err;
}

void set_energy_consumed(float energy_consumed)
{
    energy_data.energy_consumed_kwh = energy_consumed;
    ESP_LOGD(TAG, "Energy consumed set to: %.2f", energy_data.energy_consumed_kwh);

    // energy_data_set(0);
}

float get_energy_consumed(void)
{
    return energy_data.energy_consumed_kwh;
}

void set_heating_time(uint32_t heating_time)
{
    energy_data.heating_time_seconds = heating_time;
    ESP_LOGD(TAG, "Heating time set to: %lu", energy_data.heating_time_seconds);

    // energy_data_set(1);
}

uint32_t get_heating_time(void)
{
    return energy_data.heating_time_seconds;
}

void set_update_time(int64_t update_time)
{
    energy_data.last_update_time = update_time;
    ESP_LOGD(TAG, "Last update time set to: %lli", energy_data.last_update_time);

    // energy_data_set(2);
}

int64_t get_update_time(void)
{
    return energy_data.last_update_time;
}

bool is_energy_data_initialized(void)
{
    return energy_data.is_initialized;
}

// Shared telemetry interval management
static int current_telemetry_interval = NORMAL_TELEMETRY_TIME;
static int64_t last_presence_detected_time = 0;

int calculate_telemetry_interval_seconds(bool presence_detected)
{
    int64_t current_time = US_TO_MS(esp_timer_get_time());

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
        last_presence_detected_time = US_TO_MS(esp_timer_get_time());
    }
}

esp_err_t load_wifi_credentials(char *ssid, char *password)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE_WIFIDATA, NVS_READONLY, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG WIFI_TAG, "No saved credentials found");
#ifdef DEVICE_TEST
        save_wifi_credentials(TEST_AP_SSID, TEST_AP_PASS);
#endif
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
        ESP_LOGE(TAG WIFI_TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return err;
    }

    nvs_set_str(nvs, "ssid", ssid);
    nvs_set_str(nvs, "password", password);
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG WIFI_TAG, "WiFi credentials saved");
    return ESP_OK;
}
