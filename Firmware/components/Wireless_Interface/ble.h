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
    int64_t connection_time_us;  // Connection timestamp in microseconds
} stream_client_state_t;

// Streaming data structures
typedef struct __attribute__((packed)) {
    uint32_t timestamp;
    uint8_t has_target;
    float move_dist;
    float still_dist;
    uint8_t move_energy;
    uint8_t still_energy;
} presence_stream_packet_t;

typedef struct __attribute__((packed)) {
    uint32_t timestamp;
    float ambient_temp;
    float object_temp;
} temp_stream_packet_t;

#define MAX_BLE_CLIENTS 2
extern stream_client_state_t client_states[MAX_BLE_CLIENTS];

// Client state management functions
bool has_active_ble_clients(void);
bool has_recently_connected_ble_clients(void);

extern esp_ble_adv_params_t adv_params;

#ifdef __cplusplus
}
#endif