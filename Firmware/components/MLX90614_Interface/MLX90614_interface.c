#include "MLX90614_interface.h"
#include "MLX90614_SMBus_Driver.h"
#include "MLX90614_API.h"
#include "driver/temperature_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <time.h>
#include <math.h>
#include "Comm_Interface.h"
#include "ble.h"
#include "app.h"
#include "defines.h"
#include "Heater_interface.h"

#define TAG "IR_SENSOR"

#define NEGATIVE_FILTER_CHECK(var)                  \
        do {                                        \
            if ((var) >= 0 && !isnan(var)) {       \
                CircularBuffer_Push(&dataBuffer##var, (var)); \
            }                                       \
        } while (0)

#define CHECK_ERROR(expression)                             \
        error = expression;                                 \
        do {                                                \
            if (error != ESP_OK) {                          \
                ESP_LOGE(TAG, "Error: %d", error);          \
                set_device_status(1, ERROR_IR_SENSOR);      \
                set_led_status_flag("error_ir_sensor", true); \
                break;                                      \
            }                                               \
            else { set_device_status(0, ERROR_IR_SENSOR);   \
                 set_led_status_flag("error_ir_sensor", false); \
                }                                                         \
        } while (0);                                        \
        if(error != ESP_OK) continue;

DEVICE_STATUS* _device_status = NULL; // Pointer to device status
circular_buffer_t dataBufferTa, dataBufferTo;            // circular buffer for Tambient and Tobject data
temperature_sensor_handle_t temp_sensor = NULL;

void printMLX90614DetailedMetrics(float Ta, float To, uint16_t ir1, uint16_t ir2, float Toperative, uint8_t restartCount) {
    // Check if there was a temperature shock in this cycle
    static float prev_operative_for_basic_log = 0.0f;
    static bool first_basic_log = true;
    bool shock_detected = false;

    if (!first_basic_log) {
        float temp_change = fabsf(Toperative - prev_operative_for_basic_log);
        if (temp_change > 2.0f) {
            shock_detected = true;
        }
    }
    prev_operative_for_basic_log = Toperative;
    first_basic_log = false;
    static uint32_t detailedCounter = 0;
    detailedCounter++;

    // Print detailed metrics every 60 seconds (reduced frequency)
    if (detailedCounter >= 60) {
        ESP_LOGI(TAG, "=== MLX90614 Detailed Metrics ===");

        // Raw sensor readings
        ESP_LOGI(TAG, "Raw Ambient Temp: %.2f°C", Ta);
        ESP_LOGI(TAG, "Raw Object Temp: %.2f°C", To);
        ESP_LOGI(TAG, "IR Data Channel 1: %d", ir1);
        ESP_LOGI(TAG, "IR Data Channel 2: %d", ir2);
        ESP_LOGI(TAG, "IR Data 1 Converted: %d", MLX90614_ConvertIRdata(ir1));
        ESP_LOGI(TAG, "IR Data 2 Converted: %d", MLX90614_ConvertIRdata(ir2));

        // Processed temperatures
        float filtered_Ta = 0, filtered_To = 0;
        CircularBuffer_Retrieve(&dataBufferTa, &filtered_Ta);
        CircularBuffer_Retrieve(&dataBufferTo, &filtered_To);
        ESP_LOGI(TAG, "Filtered Ambient: %.2f°C", filtered_Ta);
        ESP_LOGI(TAG, "Filtered Object: %.2f°C", filtered_To);
        ESP_LOGI(TAG, "Operative Temp: %.2f°C", Toperative);

        // Buffer states with detailed circular buffer info
        ESP_LOGI(TAG, "Ambient Buffer: count=%d/%d, head=%d, tail=%d",
                 dataBufferTa.count, BUFFER_SIZE, dataBufferTa.head, dataBufferTa.tail);
        ESP_LOGI(TAG, "Object Buffer: count=%d/%d, head=%d, tail=%d",
                 dataBufferTo.count, BUFFER_SIZE, dataBufferTo.head, dataBufferTo.tail);

        // Buffer content analysis (for debugging filtering issues)
        if (dataBufferTa.count > 0) {
            float min_ta = 999, max_ta = -999, sum_ta = 0;
            for (int i = 0; i < dataBufferTa.count; i++) {
                float val = dataBufferTa.buffer[i];
                if (val < min_ta) min_ta = val;
                if (val > max_ta) max_ta = val;
                sum_ta += val;
            }
            ESP_LOGI(TAG, "Ambient Buffer Stats: min=%.2f°C, max=%.2f°C, avg=%.2f°C",
                     min_ta, max_ta, sum_ta / dataBufferTa.count);
        }

        if (dataBufferTo.count > 0) {
            float min_to = 999, max_to = -999, sum_to = 0;
            for (int i = 0; i < dataBufferTo.count; i++) {
                float val = dataBufferTo.buffer[i];
                if (val < min_to) min_to = val;
                if (val > max_to) max_to = val;
                sum_to += val;
            }
            ESP_LOGI(TAG, "Object Buffer Stats: min=%.2f°C, max=%.2f°C, avg=%.2f°C",
                     min_to, max_to, sum_to / dataBufferTo.count);
        }

        // Sensor health
        ESP_LOGI(TAG, "Restart Count: %d/%d", restartCount, SENSOR_BOARD_RESTART_COUNT);
        ESP_LOGI(TAG, "Temp Ranges: %.1f to %.1f°C", MIN_TEMP_VAL, MAX_TEMP_VAL);

        // Internal ESP32 temperature
        float internal_temp = get_internal_temp();
        ESP_LOGI(TAG, "ESP32 Internal: %.1f°C", internal_temp);

        ESP_LOGI(TAG, "================================");
        detailedCounter = 0;
    }

    // Log basic operative temperature every 30 seconds for monitoring (or immediately if shock detected)
    static uint32_t basic_log_counter = 0;
    basic_log_counter++;
    if (basic_log_counter >= 30 || shock_detected) {
        ESP_LOGI(TAG, "Operative: %.2f°C, Ambient: %.2f°C, Object: %.2f°C%s",
                 Toperative, Ta, To, shock_detected ? " [SHOCK]" : "");
        basic_log_counter = 0;
    }
}

void ir_sensor_task(void * pvParameters)
{
    uint8_t restartCount =0;

    // Initial delay to ensure sensor is ready
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "IR sensor task starting...");

    // BLE notification timing variables
    static int64_t last_ble_notification_time = 0;

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Keep 1s for sensor reading and monitoring
        float Ta = NAN;
        float To = NAN;
        uint16_t ir1 = 0;
        uint16_t ir2 = 0;
        int error = ESP_OK;


        // Track I2C communication health
        static uint32_t successful_reads = 0;
        static uint32_t failed_reads = 0;

        esp_err_t ta_result = MLX90614_GetTa(MLX90614_I2C_ADDR1, &Ta);
        esp_err_t to_result = MLX90614_GetTo(MLX90614_I2C_ADDR1, &To);
        esp_err_t ir1_result = MLX90614_GetIRdata1(MLX90614_I2C_ADDR1, &ir1);
        esp_err_t ir2_result = MLX90614_GetIRdata2(MLX90614_I2C_ADDR1, &ir2);

        // Log I2C communication health every 120 seconds (reduced frequency)
        static uint32_t health_counter = 0;
        health_counter++;
        if (health_counter >= 120) {
            float success_rate = (float)successful_reads / (successful_reads + failed_reads) * 100.0;
            ESP_LOGI(TAG, "I2C Health: Success=%lu, Failed=%lu, Rate=%.1f%%",
                     successful_reads, failed_reads, success_rate);
            health_counter = 0;
        }

        // Check each read result individually for better debugging
        if (ta_result != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read Ta: error=%d", ta_result);
            failed_reads++;
            error = ta_result;
        }
        if (to_result != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read To: error=%d", to_result);
            failed_reads++;
            error = (error == ESP_OK) ? to_result : error;
        }
        if (ir1_result != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read IR1: error=%d", ir1_result);
            failed_reads++;
            error = (error == ESP_OK) ? ir1_result : error;
        }
        if (ir2_result != ESP_OK) {
            ESP_LOGW(TAG, "Failed to read IR2: error=%d", ir2_result);
            failed_reads++;
            error = (error == ESP_OK) ? ir2_result : error;
        }

        if (ta_result == ESP_OK && to_result == ESP_OK && ir1_result == ESP_OK && ir2_result == ESP_OK) {
            successful_reads++;
        }
        
        // Additional validation: check for NaN values even if read was successful
        if (!isnan(Ta) && !isnan(To)) {
            // Values are valid numbers, continue processing
        } else {
            ESP_LOGW(TAG, "NaN detected in sensor readings: Ta=%s, To=%s", 
                     isnan(Ta) ? "NaN" : "OK", isnan(To) ? "NaN" : "OK");
            continue; // Skip this iteration if we have NaN values
        }

        // Use existing error handling macro
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "Error: %d", error);
            set_device_status(1, ERROR_IR_SENSOR);
            set_led_status_flag("error_ir_sensor", true);
        } else {
            set_device_status(0, ERROR_IR_SENSOR);
            set_led_status_flag("error_ir_sensor", false);
        }
        if(error != ESP_OK) continue;

        // Log raw data only periodically to reduce spam
        static uint32_t data_log_counter = 0;
        data_log_counter++;
        if (data_log_counter >= 30) { // Log every 30 seconds instead of every second
            ESP_LOGI(TAG, "Raw Data: Ta=%.2f°C, To=%.2f°C, IR1=%d, IR2=%d", Ta, To, ir1, ir2);
            data_log_counter = 0;
        }

        NEGATIVE_FILTER_CHECK(To);
        NEGATIVE_FILTER_CHECK(Ta);

        // Calculate and log operative temperature
        float Toperative = get_operative_temp();

        // Smart logging: Track temperature changes for shock detection
        static float last_operative_temp = 0.0f;
        static bool first_reading = true;

        if (!first_reading && !isnan(Toperative) && !isnan(last_operative_temp)) {
            float temp_change = fabsf(Toperative - last_operative_temp);
            if (temp_change > 2.0f) {  // >2°C change in 1 second
                ESP_LOGW(TAG, "TEMPERATURE SHOCK: %.2f°C change! Was: %.2f°C, Now: %.2f°C",
                         temp_change, last_operative_temp, Toperative);
            }
        }
        
        if (!isnan(Toperative)) {
            last_operative_temp = Toperative;
        }
        first_reading = false;

        // Print detailed metrics periodically
        printMLX90614DetailedMetrics(Ta, To, ir1, ir2, Toperative, restartCount);

        // Send BLE temperature streaming notification with adaptive timing
        // Only send if we have valid temperature readings and enough time has passed
        int64_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
        
        // Determine BLE streaming interval based on presence and activity (matches telemetry pattern)
        bool presence_detected = get_presence_detected();
        static int64_t last_activity_time = 0;
        static int ble_interval = BLE_STREAMING_NORMAL_TIME;
        
        // Check if any client connected within the first minute - if so, use fast interval
        if (has_recently_connected_ble_clients()) {
            ble_interval = BLE_FIRST_MINUTE_TIME;
        } else if (presence_detected) {
            last_activity_time = current_time;
            ble_interval = BLE_STREAMING_PRESENCE_TIME;
        } else if (current_time - last_activity_time < NO_ACTIVITY_TIME_MS) {
            ble_interval = BLE_STREAMING_NORMAL_TIME;
        } else {
            ble_interval = BLE_STREAMING_IDLE_TIME;
        }
        
        int64_t ble_interval_ms = ble_interval * 1000;

        if (current_time - last_ble_notification_time >= ble_interval_ms) {
            // Only send BLE notifications if there are active BLE clients connected
            if (has_active_ble_clients()) {
                send_ble_temperature_notification(Ta, To);
                ESP_LOGD(TAG, "BLE temp notification sent (interval: %ds, presence: %s, recent_connection: %s)",
                        ble_interval, presence_detected ? "yes" : "no",
                        has_recently_connected_ble_clients() ? "yes" : "no");
            } else {
                ESP_LOGD(TAG, "BLE temp notification skipped - no active clients");
            }
            last_ble_notification_time = current_time;
        }

        // Enhanced error detection with detailed logging
        bool ta_out_of_range = (Ta > MAX_TEMP_VAL) || (Ta < MIN_TEMP_VAL);
        bool to_out_of_range = (To > MAX_TEMP_VAL) || (To < MIN_TEMP_VAL);
        bool values_invalid = ta_out_of_range || to_out_of_range;

        if(values_invalid && restartCount < SENSOR_BOARD_RESTART_COUNT)
        {
            ESP_LOGW(TAG "_Task", "Invalid temperature values detected!");
            ESP_LOGW(TAG "_Task", "Ta=%.2f°C %s, To=%.2f°C %s",
                     Ta, ta_out_of_range ? "(OUT OF RANGE)" : "(OK)",
                     To, to_out_of_range ? "(OUT OF RANGE)" : "(OK)");
            ESP_LOGW(TAG "_Task", "Valid range: %.1f to %.1f°C", MIN_TEMP_VAL, MAX_TEMP_VAL);
            ESP_LOGW(TAG "_Task", "Restarting sensor board (attempt %d/%d)", restartCount + 1, SENSOR_BOARD_RESTART_COUNT);

            // Power cycle the sensor board
            ESP_LOGI(TAG "_Task", "Powering off sensor board...");
            gpio_set_level(GND_SW , false);
            vTaskDelay(pdMS_TO_TICKS(500));

            ESP_LOGI(TAG "_Task", "Powering on sensor board...");
            gpio_set_level(GND_SW , true);
            vTaskDelay(pdMS_TO_TICKS(800));

            restartCount++;

            // Re-initialize everything
            ESP_LOGI(TAG "_Task", "Reinitializing circular buffers...");
            CircularBuffer_Init(&dataBufferTa);
            CircularBuffer_Init(&dataBufferTo);

            ESP_LOGI(TAG "_Task", "Reinitializing I2C bus...");
            MLX90614_SMBusInit(MLX90614_SDA_IO, MLX90614_SCL_IO, MLX90614_FREQ, MLX90614_I2C_NUM);

            ESP_LOGI(TAG "_Task", "Reinitializing internal temperature sensor...");
            internal_temp_sensor_init();

            ESP_LOGI(TAG "_Task", "Sensor board restart complete");
        }
        else if(restartCount >= SENSOR_BOARD_RESTART_COUNT)
        {
            set_led_effect(LED_EFFECT_BLINK, YELLOW ,LED_SPEED_SLOW );
            ESP_LOGE(TAG "_Task", "SENSOR FAILURE! Maximum restart attempts reached (%d/%d)",
                     restartCount, SENSOR_BOARD_RESTART_COUNT);
            ESP_LOGE(TAG "_Task", "Last readings: Ta=%.2f°C, To=%.2f°C", Ta, To);
            ESP_LOGE(TAG "_Task", "Sensor board may have hardware issues or be disconnected");
        }
    }
    vTaskDelete( NULL );
}

esp_err_t ir_sensor_init(void)
{
    // Initialize the circular buffers
    CircularBuffer_Init(&dataBufferTa);
    CircularBuffer_Init(&dataBufferTo);

    // Additional delay to ensure sensor board is fully powered and stable
    vTaskDelay(pdMS_TO_TICKS(250));  // 250ms delay for MLX90614 power-up time

    MLX90614_SMBusInit(MLX90614_SDA_IO, MLX90614_SCL_IO, MLX90614_FREQ, MLX90614_I2C_NUM);
    internal_temp_sensor_init();

    // Create the task for the IR sensor
    if(xTaskCreate(ir_sensor_task, "IR_Sensor_Task", 8192, NULL, 1, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create IR sensor task");
        set_led_effect(LED_EFFECT_BLINK, RED ,LED_SPEED_SLOW );
        set_device_status(1, ERROR);
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "IR sensor task created successfully");

    return ESP_OK;
}

void internal_temp_sensor_init(void)
{
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    temperature_sensor_install(&temp_sensor_config, &temp_sensor);

    temperature_sensor_enable(temp_sensor);
}

float get_operative_temp(void)
{
    float Ta = NAN;
    float To = NAN;
    float Toperative = NAN;

    CircularBuffer_Retrieve(&dataBufferTa, &Ta);
    CircularBuffer_Retrieve(&dataBufferTo, &To);
    
    // Get current heater state to determine calculation method
    HEATER_STATE heater_state = get_heater_state();
    
    // Adaptive operative temperature calculation based on heating element status:
    //
    // When heating elements are ON, use object temperature only (Toperative = To):
    // - Heating elements generate localized heat that affects ambient sensor readings
    // - MLX90614 ambient sensor measures air temperature near the sensor, which becomes
    //   artificially elevated due to heating element proximity and convection
    // - Object temperature (infrared) remains accurate as it measures radiant heat from
    //   the actual target surface, unaffected by local air temperature changes
    // - Using only object temperature prevents control oscillations caused by 
    //   heating-induced ambient temperature spikes
    //
    // When heating elements are OFF, use traditional average (Toperative = (Ta + To)/2):
    // - Both ambient and object sensors provide accurate readings without heating interference
    // - Traditional operative temperature calculation balances air and radiant temperatures
    // - Provides more comprehensive thermal comfort assessment when system is at equilibrium
    
    if (heater_state != HEATER_OFF) {
        // Heating elements are active - use object temperature only to avoid heating interference
        Toperative = To;
        ESP_LOGD(TAG, "Heating ON - Using To: %.2f°C (State: %s)", To, get_enum_name(ENUM_HEATER_STATE, heater_state));
    } else {
        // Heating elements are off - use traditional calculation for balanced thermal assessment
        Toperative = (0.5 * Ta) + (0.5 * To);
        ESP_LOGD(TAG, "Heating OFF - Using (Ta+To)/2: %.2f°C", Toperative);
    }

    return Toperative;
}

float get_ambient_temp(void)
{
    float Ta = NAN;

    CircularBuffer_Retrieve(&dataBufferTa, &Ta);

    // ESP_LOGD(TAG, "Ta: %.2f", Ta); // Removed to reduce spam

    return Ta;
}

float get_object_temp(void)
{
    float To = NAN;

    CircularBuffer_Retrieve(&dataBufferTo, &To);

    // ESP_LOGD(TAG, "To: %.2f", To); // Removed to reduce spam

    return To;
}

float get_internal_temp(void)
{
    float internal_temp = 0;
    temperature_sensor_get_celsius(temp_sensor, &internal_temp);
    // ESP_LOGD(DEVICE_TAG, "Internal temperature: %.2f", internal_temp); // Removed to reduce spam
    return internal_temp;
}

// Initialize the buffer
void CircularBuffer_Init(circular_buffer_t* cb)
{
    cb->head = 0;
    cb->tail = 0;
    cb->count = 0;
}

// Check if buffer is full
bool CircularBuffer_IsFull(circular_buffer_t* cb)
{
    return cb->count == BUFFER_SIZE;
}

// Check if buffer is empty
bool CircularBuffer_IsEmpty(circular_buffer_t* cb)
{
    return cb->count == 0;
}

// Add an element to the buffer
bool CircularBuffer_Push(circular_buffer_t* cb, float data)
{
    if (cb == NULL)     // Invalid buffer pointer check
    {
        ESP_LOGE("CircularBuffer", "Push error: buffer pointer is NULL");
        return -1;
    }
    cb->buffer[cb->head] = data;
    cb->head = (cb->head + 1) % BUFFER_SIZE;
    cb->count = (cb->count < BUFFER_SIZE) ? cb->count + 1 : BUFFER_SIZE; // Increment count, wrap around if full
    return 0;
}

// Remove and get the oldest element from the buffer
bool CircularBuffer_Pop(circular_buffer_t* cb, float* data)
{
    if (CircularBuffer_IsEmpty(cb))
    {
        return -1;
    }
    *data = cb->buffer[cb->tail];
    cb->tail = (cb->tail + 1) % BUFFER_SIZE;
    cb->count = (cb->count > 0) ? cb->count - 1 : 0;
    return 0;
}

// Peek at the element without removing it
float CircularBuffer_Peek(circular_buffer_t* cb, int index)
{
    if (CircularBuffer_IsEmpty(cb) || index < 0 || index >= BUFFER_SIZE)
    {
        // Buffer empty or invalid index; return NaN or handle error
        ESP_LOGE("CircularBuffer", "Peek error: buffer empty or invalid index");
        return NAN; // Invalid value
        // return 0; // or some error value
    }

    return cb->buffer[index];
}

// Retrieve latest element without removing it
float CircularBuffer_Retrieve(circular_buffer_t* cb, float *data)
{
    if (CircularBuffer_IsEmpty(cb))
    {
        // Buffer empty or invalid index; return NaN or handle error
        ESP_LOGE("CircularBuffer", "Retrieve error: buffer empty");
        return NAN; // Invalid value
        // return 0; // or some error value
    }

    if(cb->count < 3)
    {
        // ESP_LOGD("CircularBuffer", "Retrieve: Using latest value from buffer"); // Removed to reduce spam
        *data = cb->buffer[cb->tail];
    }
    else
    {
        float temp = 0;
        for(int i = 0; i < cb->count; i++)
        {
            temp += cb->buffer[i];
        }
        *data = temp/cb->count;
    }
    return 0;
}

// Print buffer content (for debugging)
void CircularBuffer_Print(circular_buffer_t* cb)
{
    printf("Buffer contents: ");
    for (int i = 0; i < cb->count; i++)
    {
        int index = (cb->tail + i) % BUFFER_SIZE;
        printf("%.2f ", cb->buffer[index]);
    }
    printf("\n");
}
