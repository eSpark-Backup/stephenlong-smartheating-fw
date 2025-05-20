#pragma once

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif


extern device_data_t device_data;




esp_err_t device_data_init(void);
esp_err_t device_data_set(uint8_t op);
esp_err_t reset_device_and_erase_nvs();

void set_target_temp(float target_temp);
float get_target_temp(void);
void set_device_mode(DEVICE_MODE mode);
DEVICE_MODE get_device_mode(void);




#ifdef __cplusplus
}
#endif