#include "app.h"
#include "Heater_interface.h"
#include "MLX90614_interface.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "driver/gpio.h"

#define TAG "HEATER_INTERFACE"

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
                break;
            }
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