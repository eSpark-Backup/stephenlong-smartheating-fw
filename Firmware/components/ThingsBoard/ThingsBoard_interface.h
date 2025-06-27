#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "LD2410_interface.hpp"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t thingsBoard_init(void);
void thingsBoard_task(void *pvParameters);
esp_err_t provision_device(char *access_token_out, size_t token_size, char *mac_add);
void send_telemetry(const char *access_token, LD2410_Data presData);
esp_err_t _http_event_handler(esp_http_client_event_t *evt);
int64_t get_current_milliseconds();

#ifdef __cplusplus
}
#endif