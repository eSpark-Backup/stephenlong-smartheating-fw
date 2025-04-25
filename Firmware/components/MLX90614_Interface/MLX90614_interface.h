#pragma once

#include "../../main/defines.h"

#define MLX90614_I2C_NUM    IR_SENSOR_I2C_INTERFACE
#define MLX90614_I2C_ADDR   IR_SENSOR_I2C_ADDR
#define MLX90614_SDA_IO     IR_SENSOR_SDA_IO
#define MLX90614_SCL_IO     IR_SENSOR_SCL_IO
#define MLX90614_FREQ       100000


void ir_sensor_task(void * pvParameters);

void ir_sensor_init(void);