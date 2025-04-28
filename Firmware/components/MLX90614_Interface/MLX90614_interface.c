#include "MLX90614_interface.h"
#include "MLX90614_API.h"
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

circular_buffer_t dataBufferTa;            // circular buffer for Tambient data
circular_buffer_t dataBufferTo;            // circular buffer for Tobject data

void ir_sensor_task(void * pvParameters)
{
    while(1)
    {
        float Ta = 0;
        float To = 0;
        uint16_t ir1 = 0;
        uint16_t ir2 = 0;
        
        MLX90614_GetTa(MLX90614_I2C_ADDR, &Ta);
        MLX90614_GetTo(MLX90614_I2C_ADDR, &To);
        MLX90614_GetIRdata1(MLX90614_I2C_ADDR, &ir1);
        MLX90614_GetIRdata2(MLX90614_I2C_ADDR, &ir2);
        
        ESP_LOGD(TAG, "Ta: %.2f, To: %.2f, IR1: %d, IR2: %d", Ta, To, ir1, ir2);

        NEGATIVE_FILTER_CHECK(Ta);
        NEGATIVE_FILTER_CHECK(To);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete( NULL );
}

void ir_sensor_init(void)
{
    // Initialize the circular buffers
    CircularBuffer_Init(&dataBufferTa);
    CircularBuffer_Init(&dataBufferTo);

    // Create the task for the IR sensor
    if(xTaskCreate(ir_sensor_task, "IR_Sensor_Task", 2048, NULL, 5, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create IR sensor task");
    }
    ESP_LOGD(TAG, "IR sensor task created successfully");
}

float get_operative_temp(void)
{
    float Ta = 0;
    float To = 0;
    float Toperative = 0;

    CircularBuffer_Retrieve(&dataBufferTa, &Ta);
    CircularBuffer_Retrieve(&dataBufferTo, &To);
    Toperative = (0.7 * To) + (0.3 * Ta);

    ESP_LOGD(TAG, "Ta: %.2f, To: %.2f", Ta, To);
    ESP_LOGD(TAG, "Toperative: %.2f", Toperative);

    return Toperative;
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
    // if (CircularBuffer_IsFull(cb))
    if (cb == NULL)     // Invalid buffer pointer check
    {
        ESP_LOGE("CircularBuffer", "Push error: buffer pointer is NULL");
        return -1;
    }
    cb->buffer[cb->head] = data;
    cb->head = (cb->head + 1) % BUFFER_SIZE;
    cb->count++;
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
    cb->count--;
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
