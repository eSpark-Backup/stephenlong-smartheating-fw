#include "MLX90614_interface.h"
#include "MLX90614_SMBus_Driver.h"
#include "MLX90614_API.h"
#include "driver/temperature_sensor.h"
#include "freertos/freertos.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TAG "IR_SENSOR"

#define NEGATIVE_FILTER_CHECK(var)                  \
        do {                                        \
            if ((var) >= 0) {                       \
                CircularBuffer_Push(&dataBuffer##var, (var)); \
            }                                       \
        } while (0)

#define CHECK_ERROR(expression)                             \
        error = expression;                                 \
        do {                                                \
            if (error != ESP_OK) {                          \
                ESP_LOGE(TAG, "Error: %d", error);          \
                setTempValues(-1, -1);                      \
                set_device_status(1, ERROR_IR_SENSOR);      \
                set_led_effect(LED_EFFECT_BLINK, RED ,LED_SPEED_SLOW ); \
                break;                                      \
            }                                               \
            else { set_device_status(0, ERROR_IR_SENSOR);   \
                 set_led_effect(LED_EFFECT_STEADY, GREEN ,NO_SPEED );\
                }                                                         \
        } while (0);                                        \
        if(error != ESP_OK) continue;

DEVICE_STATUS* _device_status = NULL; // Pointer to device status
circular_buffer_t dataBufferTa, dataBufferTo;            // circular buffer for Tambient and Tobject data
temperature_sensor_handle_t temp_sensor = NULL;
MLX_Temp irTemp;

void ir_sensor_task(void * pvParameters)
{
    uint8_t restartCount =0;
    irTemp.amb_temp = -1;
    irTemp.obj_temp = -1;

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        float Ta = -1;
        float To = -1;
        uint16_t ir1 = 0;
        uint16_t ir2 = 0;
        int error = ESP_OK;

        
        CHECK_ERROR(MLX90614_GetTa(MLX90614_I2C_ADDR1, &Ta));
        CHECK_ERROR(MLX90614_GetTo(MLX90614_I2C_ADDR1, &To));
        CHECK_ERROR(MLX90614_GetIRdata1(MLX90614_I2C_ADDR1, &ir1));
        CHECK_ERROR(MLX90614_GetIRdata2(MLX90614_I2C_ADDR1, &ir2));
        
        ESP_LOGD(TAG "_Task", "Ta: %.2f, To: %.2f, IR1: %d, IR2: %d", Ta, To, ir1, ir2);
        setTempValues(Ta, To);
        
        NEGATIVE_FILTER_CHECK(To);
        NEGATIVE_FILTER_CHECK(Ta);

        
        if(((Ta > MAX_TEMP_VAL) || (Ta < MIN_TEMP_VAL) || (To > MAX_TEMP_VAL) || (To < MIN_TEMP_VAL)) 
                && restartCount <SENSOR_BOARD_RESTART_COUNT)
        {
            ESP_LOGW(TAG "_Task", "Invalid Values Detected !");
            ESP_LOGW(TAG "_Task","Restarting Sensor Board");
            gpio_set_level(GND_SW , false);
            vTaskDelay(pdMS_TO_TICKS(500));
            gpio_set_level(GND_SW , true);
            vTaskDelay(pdMS_TO_TICKS(800));
            restartCount++;
              // Initialize the circular buffers
            CircularBuffer_Init(&dataBufferTa);
            CircularBuffer_Init(&dataBufferTo);    
            MLX90614_SMBusInit(MLX90614_SDA_IO, MLX90614_SCL_IO, MLX90614_FREQ, MLX90614_I2C_NUM);
            internal_temp_sensor_init();
            /* GND switch High*/
        }
        else if(restartCount >= SENSOR_BOARD_RESTART_COUNT)
        {
            set_led_effect(LED_EFFECT_BLINK, YELLOW ,LED_SPEED_SLOW );
            ESP_LOGE(TAG "_Task","Sensor FAILURE!");
        }
    }
    vTaskDelete( NULL );
}

void setTempValues(float a, float b)
{
    irTemp.amb_temp = a;
    irTemp.obj_temp = b;
}

MLX_Temp getTempValues()
{
    return irTemp;
}

esp_err_t ir_sensor_init(void)
{
    // Initialize the circular buffers
    CircularBuffer_Init(&dataBufferTa);
    CircularBuffer_Init(&dataBufferTo);

    MLX90614_SMBusInit(MLX90614_SDA_IO, MLX90614_SCL_IO, MLX90614_FREQ, MLX90614_I2C_NUM);
    internal_temp_sensor_init();

    // Create the task for the IR sensor
    if(xTaskCreate(ir_sensor_task, "IR_Sensor_Task", 2048, NULL, 1, NULL) != pdPASS)
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
    float Ta = 0;
    float To = 0;
    float Toperative = 0;

    CircularBuffer_Retrieve(&dataBufferTa, &Ta);
    CircularBuffer_Retrieve(&dataBufferTo, &To);
    Toperative = (0.5 * Ta) + (0.5 * To);

    ESP_LOGD(TAG, "Ta: %.2f, To: %.2f", Ta, To);
    ESP_LOGD(TAG, "Toperative: %.2f", Toperative);

    return Toperative;
}

float get_ambient_temp(void)
{
    float Ta = 0;

    CircularBuffer_Retrieve(&dataBufferTa, &Ta);

    ESP_LOGD(TAG, "Ta: %.2f", Ta);

    return Ta;
}

float get_object_temp(void)
{
    float To = 0;

    CircularBuffer_Retrieve(&dataBufferTa, &To);

    ESP_LOGD(TAG, "Ta: %.2f", To);

    return To;
}

float get_internal_temp(void)
{
    float internal_temp = 0;
    temperature_sensor_get_celsius(temp_sensor, &internal_temp);
    ESP_LOGD(DEVICE_TAG, "Internal temperature: %.2f", internal_temp);
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
        // Buffer empty or invalid index; return 0 or handle error
        ESP_LOGE("CircularBuffer", "Peek error: buffer empty or invalid index");
        return -1; // or some error value
        // return 0; // or some error value
    }

    return cb->buffer[index];
}

// Retrieve latest element without removing it
float CircularBuffer_Retrieve(circular_buffer_t* cb, float *data)
{
    if (CircularBuffer_IsEmpty(cb))
    {
        // Buffer empty or invalid index; return 0 or handle error
        ESP_LOGE("CircularBuffer", "Retrieve error: buffer empty");
        return -1; // or some error value
        // return 0; // or some error value
    }

    if(cb->count < 3)
    {
        ESP_LOGD("CircularBuffer", "Retrieve: Using latest value from buffer");
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
