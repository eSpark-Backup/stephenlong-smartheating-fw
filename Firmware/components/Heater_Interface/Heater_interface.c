#include "Storage_interface.h"
#include "Heater_interface.h"
#include "MLX90614_interface.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "driver/gpio.h"

#define TAG "HEATER_INTERFACE"

esp_timer_handle_t energy_timer_handle;
TaskHandle_t xHeaterTaskHandle = NULL;
float tempSetPoint = 0.0;

void heater_interaface_task(void * pvParameters)
{
    while(1)
    {
        operate_heater();
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
    }
    vTaskDelete(xHeaterTaskHandle);
}

void energy_timer_callback(void *arg)
{
    if (!is_energy_data_initialized()) {
        return;
    }

    int64_t current_time = esp_timer_get_time();
    int64_t time_diff_us = current_time - get_update_time();
    float time_diff_hours = (float)time_diff_us / (1000000.0f * 3600.0f); // Convert to hours

    float current_power = calculate_current_power_consumption();

    // Update energy consumption (integrate power over time)
    set_energy_consumed(get_energy_consumed() + ((current_power * time_diff_hours) / 1000.0f));

    // Update heating time if any heating is active
    if (current_power > 0) {
        set_heating_time(get_heating_time() + ((uint32_t)US_TO_SEC(time_diff_us)));// += (uint32_t)(time_diff_us / 1000000); // Convert to seconds
    }

    set_update_time(current_time);

    // Save to NVS every 60 seconds to avoid excessive flash writes
    static int save_counter = 0;
    save_counter++;
    if (save_counter >= 60) {
        // energy_data_save_to_nvs();
        energy_data_set(255);
        save_counter = 0;
        ESP_LOGD(TAG, "Energy data saved: %.3f kWh, %lu seconds, %.1f W",
                 get_energy_consumed(), get_heating_time(), current_power);
    }
}

void heater_interface_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = GPIO_PIN_MASK(HEATER_HIGH_IO) | GPIO_PIN_MASK(HEATER_LOW_IO),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    set_heater_mode(get_device_mode());

    // Create energy tracking timer
    esp_timer_create_args_t energy_timer_args = {
        .callback = energy_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "energy_timer",
        .skip_unhandled_events = false
    };
    
    esp_err_t timer_err = esp_timer_create(&energy_timer_args, &energy_timer_handle);
    if (timer_err == ESP_OK) {
        esp_timer_start_periodic(energy_timer_handle, ENERGY_UPDATE_INTERVAL);
        ESP_LOGI(TAG, "Energy tracking timer started successfully");
    } else {
        ESP_LOGE(TAG, "Failed to create energy tracking timer: %s", esp_err_to_name(timer_err));
    }
}

void set_heater_mode(DEVICE_MODE mode)
{
    if(get_device_mode() != mode)
    {
        set_device_mode(mode);
    }

    switch(mode)
    {
        case MANUAL_MODE:
            if(xHeaterTaskHandle != NULL)
            {
                vTaskDelete(xHeaterTaskHandle);
                xHeaterTaskHandle = NULL;
                ESP_LOGD(TAG, "Heater mode switched to MANUAL_MODE");
            }
            break;
        case AUTO_MODE:
            // Create the task for the heater interface
            if(xTaskCreate(heater_interaface_task, "Heater_Interface_Task", 2048, NULL, 5, &xHeaterTaskHandle) != pdPASS)
            {
                ESP_LOGE(TAG, "Failed to create heater interface task");
                set_led_status_flag("error_heating", true);
                break;
            }
            set_led_status_flag("error_heating", false);
            ESP_LOGD(TAG, "Heater interface task created successfully");
            ESP_LOGD(TAG, "Heater mode switched to AUTO_MODE");
            break;
        case BYPASS_MODE:
            // Stop automatic control task
            if(xHeaterTaskHandle != NULL)
            {
                vTaskDelete(xHeaterTaskHandle);
                xHeaterTaskHandle = NULL;
                ESP_LOGD(TAG, "Heater automatic task stopped for BYPASS_MODE");
            }
            // Turn on both heating elements for full power
            set_heater_state(HEATER_HIGH);
            ESP_LOGD(TAG, "Heater mode switched to BYPASS_MODE - full power enabled");
            break;
        default:
            ESP_LOGE(TAG, "Mode not supported");
            break;
    }
}

void set_heater_temp(float temp)
{
    set_target_temp(temp);
    if(get_device_mode() != AUTO_MODE)
    {
        operate_heater();
    }
}

esp_err_t set_heater_state(HEATER_STATE state)
{
    switch(state)
    {
        case HEATER_OFF:
            gpio_set_level(HEATER_HIGH_IO, 0);
            gpio_set_level(HEATER_LOW_IO, 0);
            ESP_LOGD(TAG, "Heater OFF");
            break;
        case HEATER_LOW:
            gpio_set_level(HEATER_HIGH_IO, 0);
            gpio_set_level(HEATER_LOW_IO, 1);
            ESP_LOGD(TAG, "Heater LOW");
            break;
        case HEATER_MID:
            gpio_set_level(HEATER_HIGH_IO, 1);
            gpio_set_level(HEATER_LOW_IO, 0);
            ESP_LOGD(TAG, "Heater MID");
            break;
        case HEATER_HIGH:
            gpio_set_level(HEATER_HIGH_IO, 1);
            gpio_set_level(HEATER_LOW_IO, 1);
            ESP_LOGD(TAG, "Heater HIGH");
            break;
        default:
            ESP_LOGE(TAG, "Invalid heater state");
            return ESP_FAIL; // Invalid state
            break;
    }
    return ESP_OK;
}

HEATER_STATE get_heater_state(void)
{
    int heater_low = gpio_get_level(HEATER_LOW_IO);
    int heater_high = gpio_get_level(HEATER_HIGH_IO);

    uint8_t heater_bits = (heater_high << 1) | heater_low;
    // ESP_LOGD(TAG, "Heater state bits: %d", heater_bits); // Removed to reduce spam

    switch (heater_bits)
    {
        case 0x0: return HEATER_OFF;  // 00
        case 0x1: return HEATER_LOW;  // 01
        case 0x2: return HEATER_MID;  // 10
        case 0x3: return HEATER_HIGH; // 11
        default:  return HEATER_OFF;  // Should not happen
    }
}

HEATER_STATE control_heater(float setpoint, float current_temp)
{
    float diff = setpoint - current_temp;

    if (diff > THRESHOLD_HIGH) {
        return HEATER_HIGH;
    } else if (diff > THRESHOLD_MID && diff <= THRESHOLD_HIGH) {
        return HEATER_MID;
    } else if (diff > THRESHOLD_LOW && diff <= THRESHOLD_MID) {
        return HEATER_LOW;
    } else { // diff <= -0.5
        return HEATER_OFF;
    }
}

void operate_heater(void)
{
    float target = get_target_temp();
    if (target <= 0) {
        // Auto mode with no valid target - turn off heater but stay in auto mode
        if(get_heater_state() != HEATER_OFF) {
            set_heater_state(HEATER_OFF);
            ESP_LOGD(TAG, "Heater OFF - no valid target temperature");
        }
        return;
    }

    // Normal auto mode operation
    float Toperative = get_operative_temp();
    HEATER_STATE state = control_heater(target, Toperative);
    if(state != get_heater_state())
    {
        set_heater_state(state);
        ESP_LOGD(TAG, "Heater state changed");
    }
}

float calculate_current_power_consumption(void)
{
    HEATER_STATE current_state = get_heater_state();
    float power = 0.0f;

    switch (current_state) {
        case HEATER_OFF:
            power = 0.0f;
            break;
        case HEATER_LOW:
            power = HEATING_LOW_WATTS;
            break;
        case HEATER_MID:
            power = HEATING_HIGH_WATTS;
            break;
        case HEATER_HIGH:
            power = HEATING_LOW_WATTS + HEATING_HIGH_WATTS;
            break;
        default:
            power = 0.0f;
            break;
    }

    return power;
}

uint8_t calculate_heating_efficiency(void)
{
    // Calculate efficiency based on heating duty cycle over the last period
    // This is a simplified efficiency calculation
    float target_temp = get_target_temp();
    float current_temp = get_operative_temp();

    if (target_temp <= 0) {
        return 0; // No target set
    }

    float temp_diff = target_temp - current_temp;

    // Simple efficiency calculation: closer to target = higher efficiency
    if (temp_diff <= 0.5f && temp_diff >= -0.5f) {
        return 95; // Very close to target
    } else if (temp_diff <= 1.0f && temp_diff >= -1.0f) {
        return 85; // Close to target
    } else if (temp_diff <= 2.0f && temp_diff >= -2.0f) {
        return 70; // Moderately close
    } else {
        return 50; // Far from target
    }
}
