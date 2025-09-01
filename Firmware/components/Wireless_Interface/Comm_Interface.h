#pragma once

#include "defines.h"
#include "ble.h"
#include "status_led.h"
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
#include <esp_netif.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

#define adv_config_flag (1 << 0)
#define scan_rsp_config_flag (1 << 1)

enum {
    NOTIFY_WIFI,
    NOTIFY_CONTROL,
    NOTIFY_RESPONSE,
    NOTIFY_TEMPERATURE,
    NOTIFY_PRESENCE,
    NOTIFY_MAX
};

extern uint8_t adv_config_done;

extern prepare_type_env_t a_prepare_write_env;

void ble_command_task(void * pvParameters);

void comm_init(void);
void display_bonded_devices();
void remove_bond_devices();
bool startBLEAdvert();
void start_ble(void);

// BLE - New DRAFT specification UUIDs
extern const uint8_t prov_service_uuid[16];        // Wi-Fi Provisioning Service
extern const uint8_t wifi_rx_uuid[16];             // Wi-Fi RX (Credential Input)
extern const uint8_t wifi_tx_uuid[16];             // Wi-Fi TX (Status Output)
extern const uint8_t herschel_service_uuid[16];    // Herschel Control Service
extern const uint8_t control_point_uuid[16];       // Control Point
extern const uint8_t response_uuid[16];            // Response
extern const uint8_t temperature_data_uuid[16];    // Temperature Data
extern const uint8_t presence_data_uuid[16];       // Presence Data

// Legacy compatibility aliases
extern const uint8_t uart_service_uuid[16];
extern const uint8_t uart_tx_uuid[16];
extern const uint8_t uart_rx_uuid[16];
extern const uint8_t prov_chr_uuid[16];
extern esp_ble_adv_data_t adv_data;
extern esp_ble_adv_data_t scan_rsp_data;

extern uint8_t adv_config_done;

// Clean BLE service table implementation - no legacy variables needed

typedef struct{
    uint8_t gate;
    uint8_t movingThreshold;
    uint8_t stationaryThreshold;
}GateParameters;

extern GateParameters* gateParams;  // Pointer to array
extern char *numberOfGates;

// BLE command variables (defined in Comm_Interface.c)
extern BLE_COMMANDS blecmd;
extern char* bleParameters[2];
extern bool uart_cmd_handler_flag;

struct gatts_profile_inst
{
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
};

extern struct gatts_profile_inst gl_profile_tab[PROFILE_NUM];

typedef struct {
    char ssid[33];
    int8_t rssi;
    const char* protectionMode;
    uint8_t networkChannnel;
}ScanWifi;

extern TaskHandle_t xWifiScanTaskHandle;
extern const char* wifiSSIDJSON; 
extern bool scanInProgress;
extern bool scanFinished;


void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void gatts_prov_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void gatts_uart_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

void write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);
void exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);
// void uart_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);

uint8_t parse_command(uint8_t *data, int len , const char *property);
esp_err_t send_ble_notification(esp_err_t status, uint8_t *data, uint16_t len, uint8_t notify_enum);
void send_ble_temperature_notification(float ambient_temp, float object_temp);
void send_ble_presence_notification(uint8_t has_target, float move_dist, float still_dist, uint8_t move_energy, uint8_t still_energy);


// Client State Management Functions (auto-streaming enabled)
void update_cccd_state(uint16_t conn_id, uint16_t handle, uint16_t value);
void initialize_client_state(uint16_t conn_id, esp_gatt_if_t gatts_if);
void cleanup_client_state(uint16_t conn_id);
void log_client_states(void);

void parse_wifi_json_credentials(uint8_t *data, int len);
void send_wifi_status_json(const char *status, const char *ssid, const char *error);


// WiFi
void wifi_init_sta(const char *ssid, const char *password);
esp_err_t get_wifi_status(char *ssid_out, uint8_t *connected_out, int8_t *rssi_out, uint32_t *ip_address_out, uint8_t *internet_available_out);
esp_err_t scan_wifi_ssids();
void wifi_scan_task(void *pvParameters);


void start_softap_provisioning(void);
void stop_softap_provisioning(void);
httpd_handle_t start_webserver(void);
httpd_handle_t stop_webserver(void);
esp_err_t post_handler(httpd_req_t *req);
esp_err_t get_handler(httpd_req_t *req);
esp_err_t scan_results_handler(httpd_req_t *req);
void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);



#ifdef __cplusplus
}
#endif
