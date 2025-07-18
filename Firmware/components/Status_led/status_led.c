#include "status_led.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"
#include "esp_timer.h"

// #define ERROR_MASK (ERROR | ERROR_IR_SENSOR | ERROR_PROVISIONING)

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM      STATUS_LED_IO

#define LED_NUMBERS         1
#define CHASE_SPEED_MS      1000

#define TAG                 DEVICE_TAG LED_TAG

#define SET_COLOR(state, r, g, b) { \
    for (int j = 0; j < LED_NUMBERS; j += 3) { \
        led_strip_pixels_##state[j * 3 + 0] = (g); \
        led_strip_pixels_##state[j * 3 + 1] = (r); \
        led_strip_pixels_##state[j * 3 + 2] = (b); \
    } \
}

///*
#define SET_LED_FLAG(field)                                          \
    do {                                                             \
        if (strcmp(flag_name, #field) == 0) {                        \
            if (led_flags.field != value) {                          \
                led_flags.field = value;                             \
                ESP_LOGD(TAG, "%s LED status flag: %s",              \
                         (value ? "Setting" : "Resetting"), #field); \
            }                                                        \
            xSemaphoreGive(led_flags_mutex);                         \
            return;                                                  \
        }                                                            \
    } while (0)
//*/

static uint8_t led_strip_pixels_state1[LED_NUMBERS * 3];
static uint8_t led_strip_pixels_state2[LED_NUMBERS * 3];
rmt_channel_handle_t led_chan = NULL;
rmt_encoder_handle_t led_encoder = NULL;
rmt_transmit_config_t tx_config = {
    .loop_count = 0, // no transfer loop
};


LED_STATE_T current_led_state = {LED_EFFECT_NONE, NONE, NO_SPEED, false};

static RGB_CODE rgbcode ={0,0,0};
static int status_led_state = 0;
static uint16_t brightness = 0;
static bool increasing = true;

static led_status_flags_t led_flags = {0};       // initialize all flags to false
static SemaphoreHandle_t led_flags_mutex = NULL;
static TaskHandle_t led_manager_task_handle = NULL;


void status_led_task(void * pvParameters)
{
    // const TickType_t xDelay = pdMS_TO_TICKS(150); // 150ms update rate for responsive feedback

    // // Track current LED state to avoid redundant calls
    // static LED_STATE_T current_led_state = {LED_EFFECT_NONE, NONE, NO_SPEED, false};
    
    uint64_t time_stamp = esp_timer_get_time();
    while(1)
    {
        uint64_t now = esp_timer_get_time();
        if ((led_flags_mutex != NULL) && ((now - time_stamp) > MS_TO_US(150)))
        {
            time_stamp = now;

            if(xSemaphoreTake(led_flags_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
            {
                set_led_status(&current_led_state);
    
                xSemaphoreGive(led_flags_mutex);
            }
        }

        if (current_led_state.effect == LED_EFFECT_NONE)
        {
            // Default behavior: show status LED normally
            ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state1, sizeof(led_strip_pixels_state1), &tx_config));
            ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, pdMS_TO_TICKS(100)));
            vTaskDelay(pdMS_TO_TICKS(CHASE_SPEED_MS));
            ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state2, sizeof(led_strip_pixels_state2), &tx_config));
            ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, pdMS_TO_TICKS(100)));
            vTaskDelay(pdMS_TO_TICKS(CHASE_SPEED_MS));
        }
        else
        {
            led_effect_handler();
        }

    }

    vTaskDelete( NULL );
}

void led_idle()
{
    RGB_CODE code_rgb= get_rgb_code(current_led_state.color);

    SET_COLOR(state1, code_rgb.red, code_rgb.green, code_rgb.blue);
    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state1, sizeof(led_strip_pixels_state1), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, pdMS_TO_TICKS(100)));
    vTaskDelay(pdMS_TO_TICKS(effect_delay(30)));
}
void led_blink_effect()
{
    RGB_CODE code_rgb= get_rgb_code(current_led_state.color);

    SET_COLOR(state1, code_rgb.red, code_rgb.green, code_rgb.blue);
    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state1, sizeof(led_strip_pixels_state1), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, pdMS_TO_TICKS(100)));
    vTaskDelay(pdMS_TO_TICKS(effect_delay(current_led_state.speed)));

    SET_COLOR(state2, 0, 0, 0);
    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state2, sizeof(led_strip_pixels_state2), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, pdMS_TO_TICKS(100)));
    vTaskDelay(pdMS_TO_TICKS(effect_delay(current_led_state.speed)));
}

void led_breath_effect()
{
    RGB_CODE code_rgb= get_rgb_code(current_led_state.color);
    if (increasing)
    {
        brightness += 1;
        if (brightness >= 255){
            increasing = false;}
        SET_COLOR(state1, (code_rgb.red * brightness) / 255, (code_rgb.green * brightness) / 255, (code_rgb.blue * brightness) / 255);
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state1, sizeof(led_strip_pixels_state1), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, pdMS_TO_TICKS(100)));
    }
    else
    {
        brightness -= 1;
        if (brightness <= 8){
            increasing = true;}
        SET_COLOR(state2, (code_rgb.red * brightness) / 255, (code_rgb.green * brightness) / 255, (code_rgb.blue * brightness) / 255);
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state2, sizeof(led_strip_pixels_state2), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, pdMS_TO_TICKS(100)));
    }
    vTaskDelay(pdMS_TO_TICKS((effect_delay(current_led_state.speed))/100));

}

void led_double_blink_effect()
{
    RGB_CODE code_rgb= get_rgb_code(current_led_state.color);
    for (int i = 0; i < 2; i++)
    {
        SET_COLOR(state1, code_rgb.red, code_rgb.green, code_rgb.blue);
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state1, sizeof(led_strip_pixels_state1), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, pdMS_TO_TICKS(100)));
        vTaskDelay(pdMS_TO_TICKS(100));

        SET_COLOR(state2, 0, 0, 0);
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state2, sizeof(led_strip_pixels_state2), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, pdMS_TO_TICKS(100)));
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(effect_delay(current_led_state.speed)));

}

void led_tripple_flash_effect()
{
    RGB_CODE code_rgb= get_rgb_code(current_led_state.color);
    for (int i = 0; i < 3; i++)
    {
        SET_COLOR(state1, code_rgb.red, code_rgb.green, code_rgb.blue);
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state1, sizeof(led_strip_pixels_state1), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, pdMS_TO_TICKS(100)));
        vTaskDelay(pdMS_TO_TICKS(70));

        SET_COLOR(state2, 0, 0, 0);
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state2, sizeof(led_strip_pixels_state2), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, pdMS_TO_TICKS(100)));
        vTaskDelay(pdMS_TO_TICKS(70));
    }
    vTaskDelay(pdMS_TO_TICKS(effect_delay(current_led_state.speed)));

}

// LED Manager Functions
void set_led_status_flag(const char* flag_name, bool value)
{
    if (led_flags_mutex == NULL) return;

    if (xSemaphoreTake(led_flags_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        SET_LED_FLAG(erase_nvs);
        SET_LED_FLAG(error_critical);
        SET_LED_FLAG(error_ir_sensor);
        SET_LED_FLAG(error_heating);
        SET_LED_FLAG(error_nvs);
        SET_LED_FLAG(button_ble_feedback);
        SET_LED_FLAG(button_factory_reset_feedback);
        SET_LED_FLAG(button_bypass_feedback);
        SET_LED_FLAG(button_softap_feedback);
        SET_LED_FLAG(pairing_mode_active);
        SET_LED_FLAG(firmware_update);
        SET_LED_FLAG(factory_reset_confirmation);
        SET_LED_FLAG(bypass_mode_active);
        SET_LED_FLAG(normal_operation);
        SET_LED_FLAG(wifi_connected);
        SET_LED_FLAG(wifi_warning);

        ESP_LOGE(TAG, "Unknown LED status flag: %s", flag_name);
        xSemaphoreGive(led_flags_mutex);
    }
}

led_status_flags_t* get_led_status_flags(void)
{
    return &led_flags;
}

// /* Function to set the device status and update the LED status accordingly
//  *
//  * @param bitMask: The bitmask representing the status to be set
//  *        op: The operation to be performed (0 for clear, 1 for set)
//  */
// void set_device_status(uint8_t op, int bitMask)
// {
//     if(op)
//     {
//         status_led_state |= (DEVICE_STATUS)bitMask; // Set the bits specified in bitMask
//     }
//     else
//     {
//         status_led_state &= ~(DEVICE_STATUS)bitMask; // Clear the bits specified in bitMask
//     }

//     // set_led_status();

//     // ESP_LOGI(TAG, "Device status set to: %s", get_enum_name(ENUM_DEVICE_STATUS, device_status));
// }


int effect_delay(EFFECT_SPEED effecSpeed)
{
    switch (effecSpeed)
    {
    case LED_SPEED_SLOW:
        return 1000;
    case LED_SPEED_MEDIUM:
        return 500;
    case LED_SPEED_FAST:
        return 200;
    default:
        return 1500;
    }
}

RGB_CODE get_rgb_code(COLORS colorSel)
{
    switch (colorSel)
    {
        case RED:
            rgbcode.red= 255;
            rgbcode.green= 0;
            rgbcode.blue= 0;
            return(rgbcode);
            break;
        case BLUE:
            rgbcode.red= 0;
            rgbcode.green= 0;
            rgbcode.blue= 255;
            return(rgbcode);
            break;
        case BLUE_DARK:
            rgbcode.red= 75;   // Dark blue/indigo for SoftAP
            rgbcode.green= 0;
            rgbcode.blue= 130;
            return(rgbcode);
            break;
        case GREEN:
            rgbcode.red= 0;
            rgbcode.green= 255;
            rgbcode.blue= 0;
            return(rgbcode);
            break;
        case PURPLE:
            rgbcode.red= 128;
            rgbcode.green= 0;
            rgbcode.blue= 128;
            return(rgbcode);
            break;
        case YELLOW:
            rgbcode.red= 255;
            rgbcode.green= 255;
            rgbcode.blue= 0;
            return(rgbcode);
            break;

        default:
            rgbcode.red= 255;
            rgbcode.green= 255;
            rgbcode.blue= 255;
            return(rgbcode);
            break;
    }
}

void set_led_status(LED_STATE_T* state)
{
    led_effect_t desired_effect;
    COLORS desired_color;
    EFFECT_SPEED desired_speed;
    
    // Priority-based LED control (highest priority first)
    
    if (led_flags.erase_nvs) {
        desired_effect = LED_EFFECT_BLINK;
        desired_color = PURPLE;
        desired_speed = LED_SPEED_FAST;
    }

    // 1. Critical errors (RED)
    else if (led_flags.error_critical || led_flags.error_ir_sensor || led_flags.error_heating || led_flags.error_nvs) {
        desired_effect = LED_EFFECT_BLINK;
        desired_color = RED;
        desired_speed = LED_SPEED_FAST;
    }

    // 2. Button feedback (various colors, high priority)
    else if (led_flags.button_ble_feedback) {
        desired_effect = LED_EFFECT_BREATHING;
        desired_color = BLUE;
        desired_speed = NO_SPEED;
    }
    else if (led_flags.button_softap_feedback) {
        desired_effect = LED_EFFECT_BREATHING;
        desired_color = BLUE_DARK;
        desired_speed = NO_SPEED;
    }
    else if (led_flags.button_factory_reset_feedback) {
        desired_effect = LED_EFFECT_BREATHING;
        desired_color = PURPLE;
        desired_speed = NO_SPEED;
    }
    else if (led_flags.button_bypass_feedback) {
        desired_effect = LED_EFFECT_STEADY;
        desired_color = YELLOW;
        desired_speed = NO_SPEED;
    }

    // 3. System operations (medium priority)
    else if (led_flags.factory_reset_confirmation) {
        desired_effect = LED_EFFECT_BREATHING;
        desired_color = PURPLE;
        desired_speed = LED_SPEED_SLOW;
    }
    else if (led_flags.firmware_update) {
        desired_effect = LED_EFFECT_BREATHING;
        desired_color = PURPLE;
        desired_speed = LED_SPEED_MEDIUM;
    }
    else if (led_flags.pairing_mode_active) {
        desired_effect = LED_EFFECT_BREATHING;
        desired_color = BLUE;
        desired_speed = LED_SPEED_SLOW;
    }
    else if (led_flags.bypass_mode_active) {
        desired_effect = LED_EFFECT_STEADY;
        desired_color = YELLOW;
        desired_speed = NO_SPEED;
    }

    // 4. Warnings (medium-low priority)
    else if (led_flags.wifi_warning) {
        desired_effect = LED_EFFECT_BLINK;
        desired_color = YELLOW;
        desired_speed = LED_SPEED_SLOW;
    }

    // 5. Normal operation (lowest priority - only if no other states)
    else if (led_flags.normal_operation) {
        desired_effect = LED_EFFECT_STEADY;
        desired_color = GREEN;
        desired_speed = NO_SPEED;
    }

    // 6. No status - LED off
    else {
        desired_effect = LED_EFFECT_NONE;
        desired_color = NONE;
        desired_speed = NO_SPEED;
    }

    // Only update LED when state actually changes
    if (!state->initialized ||
        desired_effect != state->effect ||
        desired_color != state->color ||
        desired_speed != state->speed) {

        set_led_effect(desired_effect, desired_color, desired_speed);

        // // Update tracked state
        // state->effect = desired_effect;
        // state->color = desired_color;
        // state->speed = desired_speed;
        state->initialized = true;
    }
}

void set_led_effect(led_effect_t effect , COLORS colorSelection , EFFECT_SPEED effectSPEED)
{
    ESP_LOGD(TAG, "set_led_effect: effect=%d, color=%d, speed=%d", effect, colorSelection, effectSPEED);
    current_led_state.effect = effect;
    current_led_state.speed = effectSPEED;
    current_led_state.color = colorSelection;
    ESP_LOGD(TAG, "LED effect updated successfully");
}

led_effect_t get_current_led_effect(void)
{
    return current_led_state.effect;
}

void led_effect_handler()
{
    switch (current_led_state.effect)
    {
        case LED_EFFECT_BLINK:
            led_blink_effect();
        break;

        case LED_EFFECT_BREATHING:
            led_breath_effect();
        break;

        case LED_EFFECT_DOUBLE_BLINK:
            led_double_blink_effect();
        break;

        case LED_EFFECT_TRIPLE_FLASH:
            led_tripple_flash_effect();
        break;

        case LED_EFFECT_STEADY:
            led_idle();
        break;

        default:
            ESP_LOGE(TAG, "Unknown LED effect: %d", current_led_state.effect);
            break;
    }
}

void status_led_init(void)
{
    ESP_LOGD(TAG, "Create RMT TX channel");
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
        .gpio_num = RMT_LED_STRIP_GPIO_NUM,
        .mem_block_symbols = 64, // increase the block size can make the LED less flickering
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    ESP_LOGD(TAG, "Installing led strip encoder");

    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    ESP_LOGD(TAG, "Enabling RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(led_chan));

    // Create mutex for thread-safe flag access
    led_flags_mutex = xSemaphoreCreateMutex();
    if (led_flags_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create LED flags mutex");
        return;
    }
    
    // Initialize with normal operation
    set_led_status_flag("normal_operation", true);
    // set_led_status_flag("erase_nvs", true);

    // Create status LED task to execute LED effects (controlled by centralized LED manager)
    if(xTaskCreate(status_led_task, "Status_Led_Task", 4096, NULL, 5, &led_manager_task_handle) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create status led task");
    }

    ESP_LOGD(TAG, "Status LED hardware initialized with centralized LED manager control");
}
