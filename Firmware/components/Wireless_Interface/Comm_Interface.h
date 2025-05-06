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

extern uint8_t adv_config_done;

extern prepare_type_env_t a_prepare_write_env;

void ble_command_task(void * pvParameters);

void comm_init(void);
void start_ble(void);

// BLE
extern const uint8_t prov_service_uuid[16];
extern const uint8_t uart_service_uuid[16];
extern const uint8_t uart_tx_uuid[16];
extern const uint8_t uart_rx_uuid[16];
extern const uint8_t prov_chr_uuid[16];
extern esp_ble_adv_data_t adv_data;
extern esp_ble_adv_data_t scan_rsp_data;

extern uint16_t temp_char_handle;

extern uint8_t adv_config_done;

extern BLE_COMMANDS blecmd;
extern bool uart_cmd_handler_flag;
extern char* bleParameters[2];

struct gatts_profile_inst
{
    esp_gatts_cb_t gatts_cb;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t gatts_if;

    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_count;
    uint16_t char_handle[2];
    // esp_bt_uuid_t char_uuid[2];

    // uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    // uint16_t char1_handle;
    esp_bt_uuid_t char1_uuid;


    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

extern struct gatts_profile_inst gl_profile_tab[PROFILE_NUM];

void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void gatts_prov_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void gatts_uart_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

void write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);
void exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);
// void uart_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);

uint8_t parse_command(uint8_t *data, int len , const char *property);
void send_ble_uart_notification(esp_err_t status, uint8_t *data, uint16_t len);
// void parse_ble_write_data(uint8_t *data, int len);
// char * parse_JSON(uint8_t *data, int len , const char *property);


// WiFi
void wifi_init_sta(const char *ssid, const char *password);
esp_err_t load_wifi_credentials(char *ssid, char *password);
esp_err_t save_wifi_credentials(const char *ssid, const char *password);

void start_softap_provisioning(void);
httpd_handle_t start_webserver(void);
esp_err_t post_handler(httpd_req_t *req);
esp_err_t get_handler(httpd_req_t *req);
void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
