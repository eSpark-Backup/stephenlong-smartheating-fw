#pragma once

#include "defines.h"
#include "esp_gatts_api.h"
#include <nvs_flash.h>


// SoftAP

// BLE
extern const uint8_t uart_tx_uuid[16];
extern const uint8_t uart_rx_uuid[16];
extern const uint8_t uart_service_uuid[16];

extern BLE_COMMANDS blecmd;

struct gatts_profile_inst
{
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;

    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    uint16_t char1_handle;
    esp_bt_uuid_t char1_uuid;


    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

extern struct gatts_profile_inst gl_profile_tab[PROFILE_NUM];

void wifi_init_sta(const char *ssid, const char *password);
esp_err_t load_wifi_credentials(char *ssid, char *password);
esp_err_t save_wifi_credentials(const char *ssid, const char *password);
uint8_t parse_command(uint8_t *data, int len , const char *property);
void send_ble_uart_notification(esp_err_t status, uint8_t *data, uint16_t len);
