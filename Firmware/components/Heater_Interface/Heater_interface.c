#include "Heater_interface.h"
#include "freertos/freertos.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "driver/gpio.h"

#define TAG "HEATER_INTERFACE"

float tempSetPoint = 0.0;

void heater_interaface_task(void * pvParameters)
{
    while(1)
    {
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
    }

    vTaskDelete(NULL);
}

void heater_interaface_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = GPIO_OUTPUT_PIN_SEL(HEATER_HIGH_IO) | GPIO_OUTPUT_PIN_SEL(HEATER_LOW_IO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    set_heater_state(HEATER_OFF); // Initialize heater state to OFF

    // Create the task for the heater interface
    if(xTaskCreate(heater_interaface_task, "Heater_Interface_Task", 2048, NULL, 5, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create heater interface task");
    }
    ESP_LOGD(TAG, "Heater interface task created successfully");
}

void set_heater_state(HEATER_STATE state)
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
            break;
    }
}

HEATER_STATE get_heater_state(void)
{
    int heater_low = gpio_get_level(HEATER_LOW_IO);
    int heater_high = gpio_get_level(HEATER_HIGH_IO);

    uint8_t heater_bits = (heater_high << 1) | heater_low;

    switch (heater_bits)
    {
        case 0x0: return HEATER_OFF;  // 00
        case 0x1: return HEATER_LOW;  // 01
        case 0x2: return HEATER_MID;  // 10
        case 0x3: return HEATER_HIGH; // 11
        default:  return HEATER_OFF;  // Should not happen
    }
}

void set_heater_target_temp(float target_temp)
{
    tempSetPoint = target_temp;
    ESP_LOGD(TAG, "Target temperature set to: %.2f", tempSetPoint);
}

float get_heater_target_temp(void)
{
    return tempSetPoint;
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