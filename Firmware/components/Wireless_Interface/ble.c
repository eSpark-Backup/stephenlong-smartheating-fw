#include "ble.h"
#include "ble_service_tables.h"
#include <string.h>
#include "esp_log.h"

// Local definitions to avoid circular includes
#define PROFILE_NUM                 2
#define PROFILE_PROV_APP_ID         0
#define PROFILE_UART_APP_ID         1
#define GATTS_TAG                   "GATTS"
#define COMPANY_ID                  0x0EC3  // Herschel Infrared Ltd.

// Type definitions
typedef struct {
    uint8_t *prepare_buf;
    int prepare_len;
} prepare_type_env_t;

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
};

// Forward declarations
void parse_wifi_json_credentials(uint8_t *data, int len);
void write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);
uint8_t parse_command(uint8_t *data, int len, const char *property);

// Client state storage (exported for use by Comm_Interface.c)
stream_client_state_t client_states[MAX_BLE_CLIENTS];
static int active_client_count = 0;

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

// BLE advertising configuration
esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

uint8_t adv_config_done = 0;
prepare_type_env_t a_prepare_write_env;

// Company and service identifiers
#define COMPANY_ID_TO_BYTES(id) ((uint8_t)((id) & 0xFF)), ((uint8_t)(((id) >> 8) & 0xFF))
const uint8_t company_id[2] = {COMPANY_ID_TO_BYTES(COMPANY_ID)};

// Service UUIDs
const uint8_t prov_service_uuid[16] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E};
const uint8_t wifi_rx_uuid[16] =      {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E};
const uint8_t wifi_tx_uuid[16] =      {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E};
const uint8_t herschel_service_uuid[16] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xC3, 0x0E, 0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x7E};
const uint8_t control_point_uuid[16] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xC3, 0x0E, 0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x7E};
const uint8_t response_uuid[16] =     {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xC3, 0x0E, 0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x7E};
const uint8_t temperature_data_uuid[16] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xC3, 0x0E, 0x93, 0xF3, 0xA3, 0xB5, 0x04, 0x00, 0x40, 0x7E};
const uint8_t presence_data_uuid[16] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xC3, 0x0E, 0x93, 0xF3, 0xA3, 0xB5, 0x05, 0x00, 0x40, 0x7E};

// Advertising data
esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 2,
    .p_manufacturer_data = (uint8_t*)company_id,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0,
    .p_service_uuid = NULL,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// Forward declarations
static void gatts_wifi_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void gatts_herschel_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_PROV_APP_ID] = {
        .gatts_cb = gatts_wifi_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
        .app_id = PROFILE_PROV_APP_ID,
    },
    [PROFILE_UART_APP_ID] = {
        .gatts_cb = gatts_herschel_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
        .app_id = PROFILE_UART_APP_ID,
    },
};

//
// Client State Management Functions
//
void initialize_client_state(uint16_t conn_id, esp_gatt_if_t gatts_if) {
    for (int i = 0; i < MAX_BLE_CLIENTS; i++) {
        if (!client_states[i].is_active) {
            client_states[i].conn_id = conn_id;
            client_states[i].gatts_if = gatts_if;
            client_states[i].is_active = true;
            active_client_count++;
            ESP_LOGI(GATTS_TAG, "Initialized client state slot %d for conn_id=%d (total clients: %d)",
                     i, conn_id, active_client_count);
            return;
        }
    }
    ESP_LOGW(GATTS_TAG, "No available client slots for conn_id=%d", conn_id);
}

void cleanup_client_state(uint16_t conn_id) {
    for (int i = 0; i < MAX_BLE_CLIENTS; i++) {
        if (client_states[i].is_active && client_states[i].conn_id == conn_id) {
            client_states[i].is_active = false;
            active_client_count--;
            ESP_LOGI(GATTS_TAG, "Cleaned up client state slot %d for conn_id=%d (total clients: %d)",
                     i, conn_id, active_client_count);
            return;
        }
    }
    ESP_LOGW(GATTS_TAG, "Attempted to cleanup unknown client conn_id=%d", conn_id);
}

bool has_active_ble_clients(void) {
    return active_client_count > 0;
}

void update_cccd_state(uint16_t conn_id, uint16_t handle, uint16_t value) {
    const char* char_name = "Unknown";
    if (handle == wifi_handle_table[IDX_WIFI_TX_CFG]) {
        char_name = "WiFi Status";
    } else if (handle == herschel_handle_table[IDX_CONTROL_POINT_CFG]) {
        char_name = "Control Point";
    } else if (handle == herschel_handle_table[IDX_RESPONSE_CFG]) {
        char_name = "Response";
    } else if (handle == herschel_handle_table[IDX_TEMP_DATA_CFG]) {
        char_name = "Temperature Data";
    } else if (handle == herschel_handle_table[IDX_PRESENCE_DATA_CFG]) {
        char_name = "Presence Data";
    }

    ESP_LOGI(GATTS_TAG, "CCCD write: %s notifications %s (conn_id=%d, handle=%d)",
             char_name, (value & 0x01) ? "enabled" : "disabled", conn_id, handle);
}

//
// WiFi Provisioning Service Event Handler
//
static void gatts_wifi_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(GATTS_TAG, "WiFi service registered, app_id %04x", param->reg.app_id);
        gl_profile_tab[PROFILE_PROV_APP_ID].gatts_if = gatts_if;

        esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(wifi_gatt_db, gatts_if, WIFI_IDX_NB, 0);
        if (create_attr_ret) {
            ESP_LOGE(GATTS_TAG, "Create WiFi attr table failed, error code = %x", create_attr_ret);
        }
        break;

    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status != ESP_GATT_OK) {
            ESP_LOGE(GATTS_TAG, "Create WiFi attribute table failed, error code=0x%x", param->add_attr_tab.status);
            break;
        }
        if (param->add_attr_tab.num_handle != WIFI_IDX_NB) {
            ESP_LOGE(GATTS_TAG, "Create WiFi attribute table abnormally, num_handle (%d) != WIFI_IDX_NB(%d)",
                     param->add_attr_tab.num_handle, WIFI_IDX_NB);
            break;
        }

        memcpy(wifi_handle_table, param->add_attr_tab.handles, sizeof(wifi_handle_table));
        ESP_LOGI(GATTS_TAG, "WiFi attr table created, starting service (handle %d)", wifi_handle_table[IDX_WIFI_SVC]);
        esp_ble_gatts_start_service(wifi_handle_table[IDX_WIFI_SVC]);
        break;

    case ESP_GATTS_WRITE_EVT:
        if (!param->write.is_prep) {
            if (param->write.len == 2 && param->write.handle == wifi_handle_table[IDX_WIFI_TX_CFG]) {
                uint16_t cccd_value = param->write.value[1] << 8 | param->write.value[0];
                update_cccd_state(param->write.conn_id, param->write.handle, cccd_value);
            } else if (param->write.handle == wifi_handle_table[IDX_WIFI_RX_VAL]) {
                ESP_LOGI(GATTS_TAG, "WiFi credentials received, processing...");
                parse_wifi_json_credentials(param->write.value, param->write.len);
            }
        }
        write_event_env(gatts_if, &a_prepare_write_env, param);
        break;

    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "WiFi service connected, conn_id %d", param->connect.conn_id);
        gl_profile_tab[PROFILE_PROV_APP_ID].conn_id = param->connect.conn_id;
        initialize_client_state(param->connect.conn_id, gatts_if);
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "WiFi service disconnected, conn_id %d", param->disconnect.conn_id);
        cleanup_client_state(param->disconnect.conn_id);
        break;

    default:
        break;
    }
}

//
// Herschel Control Service Event Handler
//
static void gatts_herschel_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(GATTS_TAG, "Herschel service registered, app_id %04x", param->reg.app_id);
        gl_profile_tab[PROFILE_UART_APP_ID].gatts_if = gatts_if;

        esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(herschel_gatt_db, gatts_if, HERSCHEL_IDX_NB, 1);
        if (create_attr_ret) {
            ESP_LOGE(GATTS_TAG, "Create Herschel attr table failed, error code = %x", create_attr_ret);
        }
        break;

    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status != ESP_GATT_OK) {
            ESP_LOGE(GATTS_TAG, "Create Herschel attribute table failed, error code=0x%x", param->add_attr_tab.status);
            break;
        }
        if (param->add_attr_tab.num_handle != HERSCHEL_IDX_NB) {
            ESP_LOGE(GATTS_TAG, "Create Herschel attribute table abnormally, num_handle (%d) != HERSCHEL_IDX_NB(%d)",
                     param->add_attr_tab.num_handle, HERSCHEL_IDX_NB);
            break;
        }

        memcpy(herschel_handle_table, param->add_attr_tab.handles, sizeof(herschel_handle_table));

        // Log handle mapping for debugging
        ESP_LOGI(GATTS_TAG, "Herschel service handles:");
        ESP_LOGI(GATTS_TAG, "  Control Point: %d (CCCD: %d)", herschel_handle_table[IDX_CONTROL_POINT_VAL], herschel_handle_table[IDX_CONTROL_POINT_CFG]);
        ESP_LOGI(GATTS_TAG, "  Response: %d (CCCD: %d)", herschel_handle_table[IDX_RESPONSE_VAL], herschel_handle_table[IDX_RESPONSE_CFG]);
        ESP_LOGI(GATTS_TAG, "  Temperature: %d (CCCD: %d)", herschel_handle_table[IDX_TEMP_DATA_VAL], herschel_handle_table[IDX_TEMP_DATA_CFG]);
        ESP_LOGI(GATTS_TAG, "  Presence: %d (CCCD: %d)", herschel_handle_table[IDX_PRESENCE_DATA_VAL], herschel_handle_table[IDX_PRESENCE_DATA_CFG]);

        esp_ble_gatts_start_service(herschel_handle_table[IDX_HERSCHEL_SVC]);
        break;

    case ESP_GATTS_WRITE_EVT:
        if (!param->write.is_prep) {
            if (param->write.len == 2) {
                uint16_t cccd_value = param->write.value[1] << 8 | param->write.value[0];

                if (param->write.handle == herschel_handle_table[IDX_CONTROL_POINT_CFG] ||
                    param->write.handle == herschel_handle_table[IDX_RESPONSE_CFG] ||
                    param->write.handle == herschel_handle_table[IDX_TEMP_DATA_CFG] ||
                    param->write.handle == herschel_handle_table[IDX_PRESENCE_DATA_CFG]) {
                    update_cccd_state(param->write.conn_id, param->write.handle, cccd_value);
                } else {
                    ESP_LOGW(GATTS_TAG, "CCCD write to unknown handle=%d", param->write.handle);
                }
            } else if (param->write.handle == herschel_handle_table[IDX_CONTROL_POINT_VAL]) {
                ESP_LOGI(GATTS_TAG, "BLE command received, processing...");
                parse_command(param->write.value, param->write.len, "ble");
            }
        }
        write_event_env(gatts_if, &a_prepare_write_env, param);
        break;

    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "Herschel service connected, conn_id %d", param->connect.conn_id);
        gl_profile_tab[PROFILE_UART_APP_ID].conn_id = param->connect.conn_id;

        // Check if client already exists (multi-service connection)
        for (int i = 0; i < MAX_BLE_CLIENTS; i++) {
            if (client_states[i].is_active && client_states[i].conn_id == param->connect.conn_id) {
                ESP_LOGW(GATTS_TAG, "Client conn_id=%d already exists, reinitializing", param->connect.conn_id);
                cleanup_client_state(param->connect.conn_id);
                break;
            }
        }

        initialize_client_state(param->connect.conn_id, gatts_if);

        // Set connection parameters
        esp_ble_conn_update_params_t conn_params = {
            .latency = 0,
            .max_int = 0x20,    // 40ms
            .min_int = 0x10,    // 20ms
            .timeout = 400,     // 4000ms
        };
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        esp_ble_gap_update_conn_params(&conn_params);
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "Herschel service disconnected, conn_id %d", param->disconnect.conn_id);
        cleanup_client_state(param->disconnect.conn_id);
        break;

    default:
        break;
    }
}

//
// Notification Functions
//
void send_temperature_stream_notification(uint32_t timestamp, float ambient_temp, float object_temp) {
    if (timestamp == 0) return;

    temp_stream_packet_t temp_packet = {
        .timestamp = timestamp,
        .ambient_temp = ambient_temp,
        .object_temp = object_temp
    };

    int notifications_sent = 0;
    for (int i = 0; i < MAX_BLE_CLIENTS; i++) {
        if (client_states[i].is_active) {
            esp_err_t ret = esp_ble_gatts_send_indicate(
                client_states[i].gatts_if,
                client_states[i].conn_id,
                herschel_handle_table[IDX_TEMP_DATA_VAL],
                sizeof(temp_stream_packet_t),
                (uint8_t*)&temp_packet,
                false
            );
            if (ret == ESP_OK) notifications_sent++;
        }
    }

    static uint32_t log_counter = 0;
    if (++log_counter >= 30) {
        ESP_LOGI(GATTS_TAG, "Temperature streaming (%d clients)", notifications_sent);
        log_counter = 0;
    }
}

void send_presence_stream_notification(uint32_t timestamp, uint8_t has_target, float move_dist, float still_dist, uint8_t move_energy, uint8_t still_energy) {
    if (timestamp == 0) return;

    presence_stream_packet_t presence_packet = {
        .timestamp = timestamp,
        .has_target = has_target,
        .move_dist = move_dist,
        .still_dist = still_dist,
        .move_energy = move_energy,
        .still_energy = still_energy
    };

    int notifications_sent = 0;
    for (int i = 0; i < MAX_BLE_CLIENTS; i++) {
        if (client_states[i].is_active) {
            esp_err_t ret = esp_ble_gatts_send_indicate(
                client_states[i].gatts_if,
                client_states[i].conn_id,
                herschel_handle_table[IDX_PRESENCE_DATA_VAL],
                sizeof(presence_stream_packet_t),
                (uint8_t*)&presence_packet,
                false
            );
            if (ret == ESP_OK) notifications_sent++;
        }
    }

    static uint32_t log_counter = 0;
    if (++log_counter >= 30) {
        ESP_LOGI(GATTS_TAG, "Presence streaming (%d clients)", notifications_sent);
        log_counter = 0;
    }
}

void send_wifi_status_notification(const char *json_status) {
    if (!json_status) return;

    size_t json_len = strlen(json_status);
    if (json_len == 0 || json_len > 512) return;

    int notifications_sent = 0;
    for (int i = 0; i < MAX_BLE_CLIENTS; i++) {
        if (client_states[i].is_active) {
            esp_err_t ret = esp_ble_gatts_send_indicate(
                client_states[i].gatts_if,
                client_states[i].conn_id,
                wifi_handle_table[IDX_WIFI_TX_VAL],
                json_len,
                (uint8_t*)json_status,
                false
            );
            if (ret == ESP_OK) notifications_sent++;
        }
    }

    ESP_LOGD(GATTS_TAG, "WiFi status sent to %d clients", notifications_sent);
}

// Simple notification functions removed - implemented in Comm_Interface.c

//
// Global Event Handlers
//
void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~1);  // adv_config_flag
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~2);  // scan_rsp_config_flag
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(GATTS_TAG, "Advertising start failed");
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(GATTS_TAG, "Connection params updated: status=%d, conn_int=%d, latency=%d, timeout=%d",
                  param->update_conn_params.status, param->update_conn_params.conn_int,
                  param->update_conn_params.latency, param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gatts_if = gatts_if;
        } else {
            ESP_LOGE(GATTS_TAG, "Register app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
            return;
        }
    }

    // Route events to appropriate profile handlers
    for (int idx = 0; idx < PROFILE_NUM; idx++) {
        if (gatts_if == ESP_GATT_IF_NONE || gatts_if == gl_profile_tab[idx].gatts_if) {
            if (gl_profile_tab[idx].gatts_cb) {
                gl_profile_tab[idx].gatts_cb(event, gatts_if, param);
            }
        }
    }
}