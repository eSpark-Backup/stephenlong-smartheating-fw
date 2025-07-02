#pragma once
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Client state management
typedef struct {
    uint16_t conn_id;
    esp_gatt_if_t gatts_if;
    bool is_active;
} stream_client_state_t;

#define MAX_BLE_CLIENTS 2
extern stream_client_state_t client_states[MAX_BLE_CLIENTS];

// Client state management functions
bool has_active_ble_clients(void);

extern esp_ble_adv_params_t adv_params;

#ifdef __cplusplus
}
#endif