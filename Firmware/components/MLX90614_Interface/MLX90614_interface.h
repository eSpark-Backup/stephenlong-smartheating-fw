#pragma once

#include "defines.h"
#include <stdbool.h>
#include "esp_err.h"

#define MLX90614_I2C_NUM    IR_SENSOR_I2C_INTERFACE
#define MLX90614_I2C_ADDR1  IR_SENSOR_I2C_ADDR1
#define MLX90614_I2C_ADDR2  IR_SENSOR_I2C_ADDR2
#define MLX90614_SDA_IO     IR_SENSOR_SDA_IO
#define MLX90614_SCL_IO     IR_SENSOR_SCL_IO
#define MLX90614_FREQ       100000

#define BUFFER_SIZE 5

typedef struct {
    float buffer[BUFFER_SIZE]; // You can change `float` to whatever sensor data type you use
    int head;    // Index for the next write
    int tail;    // Index for the next read
    int count;   // Number of elements currently in the buffer
} circular_buffer_t;

// void ir_sensor_task(void * pvParameters);

esp_err_t ir_sensor_init(void);
void internal_temp_sensor_init(void);
float get_operative_temp(void);
float get_ambient_temp(void);
float get_object_temp(void);
float get_internal_temp(void);

void CircularBuffer_Init(circular_buffer_t* cb);
bool CircularBuffer_IsFull(circular_buffer_t* cb);
bool CircularBuffer_IsEmpty(circular_buffer_t* cb);
bool CircularBuffer_Push(circular_buffer_t* cb, float data);
bool CircularBuffer_Pop(circular_buffer_t* cb, float* data);
float CircularBuffer_Peek(circular_buffer_t* cb, int index);
float CircularBuffer_Retrieve(circular_buffer_t* cb, float* data);
void CircularBuffer_Print(circular_buffer_t* cb);
