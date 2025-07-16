#include "status_led.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"

#define ERROR_MASK (ERROR | ERROR_IR_SENSOR | ERROR_PROVISIONING)

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM      STATUS_LED_IO

#define LED_NUMBERS         1
#define CHASE_SPEED_MS      1000

#define TAG "LED_STRIP"

#define SET_COLOR(state, r, g, b) { \
    for (int j = 0; j < LED_NUMBERS; j += 3) { \
        led_strip_pixels_##state[j * 3 + 0] = (g); \
        led_strip_pixels_##state[j * 3 + 1] = (r); \
        led_strip_pixels_##state[j * 3 + 2] = (b); \
    } \
}

static uint8_t led_strip_pixels_state1[LED_NUMBERS * 3];
static uint8_t led_strip_pixels_state2[LED_NUMBERS * 3];
rmt_channel_handle_t led_chan = NULL;
rmt_encoder_handle_t led_encoder = NULL;
rmt_transmit_config_t tx_config = {
    .loop_count = 0, // no transfer loop
};


COLORS colorSel = NONE;
EFFECT_SPEED effecSpeed = NO_SPEED;
led_effect_t current_effect = LED_EFFECT_NONE;
static RGB_CODE rgbcode ={0,0,0};
static int status_led_state = 0;
static uint16_t brightness = 0;
static bool increasing = true;


void set_led_status(void)
{
    // int status = status_led_state;
    if (status_led_state & ERROR_MASK) {
        // Any error state: show red
        SET_COLOR(state1, 255, 0, 0); // Red
        SET_COLOR(state2, 255, 0, 0); // Red
    }
    else if (status_led_state & FIRMWARE_UPDATE) {
        SET_COLOR(state1, 255, 255, 0); // Yellow
        SET_COLOR(state2, 0, 0, 0);     // Off
    }
    else if (status_led_state & PAIRING) {
        SET_COLOR(state1, 0, 0, 255); // Blue
        SET_COLOR(state2, 0, 0, 0);   // Off
    }
    else if (status_led_state == NORMAL_OP) {
        SET_COLOR(state1, 0, 255, 0); // Green
        SET_COLOR(state2, 0, 255, 0); // Green
    }
    else {
        // OFF or unknown
        SET_COLOR(state1, 0, 0, 0); // Off
        SET_COLOR(state2, 0, 0, 0); // Off
    }
}

/* Function to set the device status and update the LED status accordingly
 *
 * @param bitMask: The bitmask representing the status to be set
 *        op: The operation to be performed (0 for clear, 1 for set)
 */
void set_device_status(uint8_t op, int bitMask)
{
    if(op)
    {
        status_led_state |= (DEVICE_STATUS)bitMask; // Set the bits specified in bitMask
    }
    else
    {
        status_led_state &= ~(DEVICE_STATUS)bitMask; // Clear the bits specified in bitMask
    }

    // set_led_status();

    // ESP_LOGI(DEVICE_TAG, "Device status set to: %s", get_enum_name(ENUM_DEVICE_STATUS, device_status));
}


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

void set_led_effect(led_effect_t effect , COLORS colorSelection , EFFECT_SPEED effectSPEED)
{
    ESP_LOGI(TAG, "set_led_effect called: effect=%d, color=%d, speed=%d", effect, colorSelection, effectSPEED);
    current_effect = effect;
    effecSpeed = effectSPEED;
    colorSel = colorSelection;
    ESP_LOGI(TAG, "LED effect updated successfully");
}

led_effect_t get_current_led_effect(void)
{
    return current_effect;
}


RGB_CODE getRGBCode(COLORS colorSel)
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


void led_idle()
{
    RGB_CODE code_rgb= getRGBCode(colorSel);

    SET_COLOR(state1, code_rgb.red, code_rgb.green, code_rgb.blue);
    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state1, sizeof(led_strip_pixels_state1), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
    vTaskDelay(pdMS_TO_TICKS(effect_delay(30)));
}
void led_blink_effect()
{
    RGB_CODE code_rgb= getRGBCode(colorSel);

    SET_COLOR(state1, code_rgb.red, code_rgb.green, code_rgb.blue);
    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state1, sizeof(led_strip_pixels_state1), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
    vTaskDelay(pdMS_TO_TICKS(effect_delay(effecSpeed)));

    SET_COLOR(state2, 0, 0, 0);
    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state2, sizeof(led_strip_pixels_state2), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
    vTaskDelay(pdMS_TO_TICKS(effect_delay(effecSpeed)));
}

void led_breath_effect()
{
    RGB_CODE code_rgb= getRGBCode(colorSel);
    if (increasing)
    {
        brightness += 1;
        if (brightness >= 255){
            increasing = false;}
        SET_COLOR(state1, (code_rgb.red * brightness) / 255, (code_rgb.green * brightness) / 255, (code_rgb.blue * brightness) / 255);
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state1, sizeof(led_strip_pixels_state1), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
    }
    else
    {
        brightness -= 1;
        if (brightness <= 8){
            increasing = true;}
        SET_COLOR(state2, (code_rgb.red * brightness) / 255, (code_rgb.green * brightness) / 255, (code_rgb.blue * brightness) / 255);
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state2, sizeof(led_strip_pixels_state2), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
    }
    vTaskDelay(pdMS_TO_TICKS((effect_delay(effecSpeed))/100));

}

void led_double_blink_effect()
{
    RGB_CODE code_rgb= getRGBCode(colorSel);
    for (int i = 0; i < 2; i++)
    {
        SET_COLOR(state1, code_rgb.red, code_rgb.green, code_rgb.blue);
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state1, sizeof(led_strip_pixels_state1), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
        vTaskDelay(pdMS_TO_TICKS(100));

        SET_COLOR(state2, 0, 0, 0);
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state2, sizeof(led_strip_pixels_state2), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(effect_delay(effecSpeed)));

}

void led_tripple_flash_effect()
{
    RGB_CODE code_rgb= getRGBCode(colorSel);
    for (int i = 0; i < 3; i++)
    {
        SET_COLOR(state1, code_rgb.red, code_rgb.green, code_rgb.blue);
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state1, sizeof(led_strip_pixels_state1), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
        vTaskDelay(pdMS_TO_TICKS(70));

        SET_COLOR(state2, 0, 0, 0);
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state2, sizeof(led_strip_pixels_state2), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
        vTaskDelay(pdMS_TO_TICKS(70));
    }
    vTaskDelay(pdMS_TO_TICKS(effect_delay(effecSpeed)));

}

void led_effect_handler()
{
        switch (current_effect)
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
                break;
            }

}

void status_led_task(void * pvParameters)
{
    // DEVICE_STATUS status = OFF_STATE;
    while(1)
    {
        if (current_effect == LED_EFFECT_NONE)
        {
            // Default behavior: show status LED normally
            ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state1, sizeof(led_strip_pixels_state1), &tx_config));
            ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
            vTaskDelay(pdMS_TO_TICKS(CHASE_SPEED_MS));
            ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state2, sizeof(led_strip_pixels_state2), &tx_config));
            ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
            vTaskDelay(pdMS_TO_TICKS(CHASE_SPEED_MS));
        }
        else
        {
            led_effect_handler();
        }

    }

    vTaskDelete( NULL );
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

    ESP_LOGD(TAG, "Install led strip encoder");

    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    ESP_LOGD(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(led_chan));

    // Build RGB pixels
    set_device_status(1, NORMAL_OP);
    // Note: LED control now handled by centralized LED manager
    // set_led_effect(LED_EFFECT_STEADY, GREEN , NO_SPEED);

    // Create status LED task to execute LED effects (controlled by centralized LED manager)
    if(xTaskCreate(status_led_task, "Status_Led_Task", 2048, NULL, 5, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create status led task");
    }
    ESP_LOGD(TAG, "Status LED hardware initialized with centralized LED manager control");
}
