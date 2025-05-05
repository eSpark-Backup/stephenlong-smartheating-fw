#pragma once

#include "defines.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gatt_common_api.h"
#include "esp_bt.h"
#include <nvs_flash.h>
#include <esp_http_server.h>
#include <esp_wifi.h>
#include <lwip/err.h>
#include <lwip/sys.h>

#define adv_config_flag (1 << 0)
#define scan_rsp_config_flag (1 << 1)

extern bool uart_cmd_handler_flag;
extern prepare_type_env_t a_prepare_write_env;

// SoftAP
void start_softap_provisioning(void);
httpd_handle_t start_webserver(void);
esp_err_t post_handler(httpd_req_t *req);
esp_err_t get_handler(httpd_req_t *req);
void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

// BLE
void start_ble_provisioning(void);
void uart_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);
void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void gatts_uart_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

void write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);
void exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);
void parse_ble_write_data(uint8_t *data, int len);
