#pragma once

#include "defines.h"
#include "esp_err.h"


#ifdef __cplusplus
extern "C" {
#endif

// Thresholds (example values, adjust as needed)
#define THRESHOLD_HIGH    1.5f
#define THRESHOLD_MID     0.5f
#define THRESHOLD_LOW    -0.5f

// void heater_interaface_task(void * pvParameters);

void heater_interface_init(void);
void set_heater_mode(DEVICE_MODE mode);
void set_heater_temp(float temp);
esp_err_t set_heater_state(HEATER_STATE state);
HEATER_STATE get_heater_state(void);
HEATER_STATE control_heater(float setpoint, float current_temp);
void operate_heater(void);


#ifdef __cplusplus
}
#endif