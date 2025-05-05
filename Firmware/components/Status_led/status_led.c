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

int status_led_state = 0;

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
    // set_device_status(0, OFF_STATE);
    set_device_status(1, NORMAL_OP);

    // Create the task for the IR sensor
    if(xTaskCreate(status_led_task, "Status_Led_Task", 512, NULL, 5, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create status led task");
    }
    ESP_LOGD(TAG, "status led task created successfully");
}

void set_led_status(void)
{
    // int status = status_led_state;
    if (status_led_state & ERROR_MASK) {
        // Any error state: show red
        SET_COLOR(state1, 255, 0, 0); // Red
        SET_COLOR(state2, 255, 0, 0); // Red
    }
    else if (status_led_state == FIRMWARE_UPDATE) {
        SET_COLOR(state1, 255, 255, 0); // Yellow
        SET_COLOR(state2, 0, 0, 0);     // Off
    }
    else if (status_led_state == PAIRING) {
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

    set_led_status();

    // ESP_LOGI(DEVICE_TAG, "Device status set to: %s", get_enum_name(ENUM_DEVICE_STATUS, device_status));
}