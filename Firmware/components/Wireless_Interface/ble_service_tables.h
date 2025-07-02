#pragma once

#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"

// WiFi Provisioning Service Table Enums
enum {
    IDX_WIFI_SVC,

    IDX_WIFI_RX_CHAR,
    IDX_WIFI_RX_VAL,

    IDX_WIFI_TX_CHAR,
    IDX_WIFI_TX_VAL,
    IDX_WIFI_TX_CFG,        // CCCD for WiFi status notifications

    WIFI_IDX_NB,
};

// Herschel Control Service Table Enums
enum {
    IDX_HERSCHEL_SVC,

    IDX_CONTROL_POINT_CHAR,
    IDX_CONTROL_POINT_VAL,
    IDX_CONTROL_POINT_CFG,  // CCCD for command response indications

    IDX_RESPONSE_CHAR,
    IDX_RESPONSE_VAL,
    IDX_RESPONSE_CFG,       // CCCD for response notifications

    IDX_TEMP_DATA_CHAR,
    IDX_TEMP_DATA_VAL,
    IDX_TEMP_DATA_CFG,      // CCCD for temperature notifications

    IDX_PRESENCE_DATA_CHAR,
    IDX_PRESENCE_DATA_VAL,
    IDX_PRESENCE_DATA_CFG,  // CCCD for presence notifications

    HERSCHEL_IDX_NB,
};

// Handle tables - populated by ESP_GATTS_CREAT_ATTR_TAB_EVT
extern uint16_t wifi_handle_table[WIFI_IDX_NB];
extern uint16_t herschel_handle_table[HERSCHEL_IDX_NB];

// Service table declarations
extern const esp_gatts_attr_db_t wifi_gatt_db[WIFI_IDX_NB];
extern const esp_gatts_attr_db_t herschel_gatt_db[HERSCHEL_IDX_NB];