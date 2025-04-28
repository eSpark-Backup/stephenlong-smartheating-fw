#pragma once

#include "../../main/defines.h"

// Thresholds (example values, adjust as needed)
#define THRESHOLD_HIGH    1.5f
#define THRESHOLD_MID     0.5f
#define THRESHOLD_LOW    -0.5f

void heater_interaface_task(void * pvParameters);

void heater_interaface_init(void);
void set_heater_state(HEATER_STATE state);
HEATER_STATE get_heater_state(void);
void set_heater_target_temp(float target_temp);
HEATER_STATE control_heater(float setpoint, float current_temp);