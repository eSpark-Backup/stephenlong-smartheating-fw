#include "status_led.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"

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

void status_led_task(void * pvParameters)
{
    // DEVICE_STATUS status = OFF_STATE;
    while(1)
    {
        // static int led_state = 0;
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state1, sizeof(led_strip_pixels_state1), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
        vTaskDelay(pdMS_TO_TICKS(CHASE_SPEED_MS));
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels_state2, sizeof(led_strip_pixels_state2), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
        vTaskDelay(pdMS_TO_TICKS(CHASE_SPEED_MS));

        // led_state++;

        // if(led_state%5 == 0) {
        //     status++;
        //     set_led_status( (status%DEVICE_STATUS_COUNT) );
        // }
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
    set_led_status(OFF_STATE);

    // Create the task for the IR sensor
    if(xTaskCreate(status_led_task, "Status_Led_Task", 512, NULL, 5, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create status led task");
    }
    ESP_LOGD(TAG, "status led task created successfully");
}

void set_led_status(DEVICE_STATUS status)
{
    switch (status) {
        case NORMAL_OP:
            SET_COLOR(state1, 0, 255, 0); // Green
            SET_COLOR(state2, 0, 255, 0); // Green
            break;
        case PAIRING:
            SET_COLOR(state1, 0, 0, 255); // Blue
            SET_COLOR(state2, 0, 0, 0); // off
            break;
        case ERROR:
            SET_COLOR(state1, 255, 0, 0); // Red
            SET_COLOR(state2, 255, 0, 0); // Red
            break;
        case FIRMWARE_UPDATE:
            SET_COLOR(state1, 255, 255, 0); // Yellow
            SET_COLOR(state2, 0, 0, 0); // off
            break;
        default:
            SET_COLOR(state1, 0, 0, 0); // off
            SET_COLOR(state2, 0, 0, 0); // off
    }
}