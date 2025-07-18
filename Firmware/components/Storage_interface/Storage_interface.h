#pragma once

#include "defines.h"
#include "status_led.h"

#ifdef __cplusplus
extern "C" {
#endif


extern device_data_t device_data;

//---------------------------------
esp_err_t device_data_init(void);
esp_err_t device_data_set(uint8_t op);
esp_err_t reset_device_and_erase_nvs();
esp_err_t get_device_thingsboard_credentials(char *token);
esp_err_t save_device_thingsboard_credentials(const char *token);

void set_target_temp(float target_temp);
float get_target_temp(void);
void set_detection_duration(uint8_t duration_val);
float get_detection_duration(void);

void set_device_mode(DEVICE_MODE mode);
DEVICE_MODE get_device_mode(void);

//---------------------------------
esp_err_t energy_data_init(void);
esp_err_t energy_data_set(uint8_t op);

void set_energy_consumed(float energy_consumed);
float get_energy_consumed(void);
void set_heating_time(uint32_t heating_time);
uint32_t get_heating_time(void);
void set_update_time(int64_t update_time);
int64_t get_update_time(void);
bool is_energy_data_initialized(void);

//---------------------------------
// Shared telemetry interval management
int calculate_telemetry_interval_seconds(bool presence_detected);
void update_presence_detection_time(bool presence_detected);
bool get_presence_detected(void);  // C-compatible wrapper for LD2410_getPresenceDetected()

#ifdef __cplusplus
}
#endif