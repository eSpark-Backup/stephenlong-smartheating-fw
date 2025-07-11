#include "ble_service_tables.h"
#include "esp_gatt_common_api.h"



// External UUID declarations (from ble.c)
extern const uint8_t prov_service_uuid[16];
extern const uint8_t wifi_rx_uuid[16];
extern const uint8_t wifi_tx_uuid[16];
extern const uint8_t herschel_service_uuid[16];
extern const uint8_t control_point_uuid[16];
extern const uint8_t response_uuid[16];
extern const uint8_t temperature_data_uuid[16];
extern const uint8_t presence_data_uuid[16];

// Handle tables - populated automatically by ESP-IDF
uint16_t wifi_handle_table[WIFI_IDX_NB];
uint16_t herschel_handle_table[HERSCHEL_IDX_NB];

// Standard GATT UUIDs
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

// Characteristic properties
static const uint8_t char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_notify_read = ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_indicate_write = ESP_GATT_CHAR_PROP_BIT_INDICATE | ESP_GATT_CHAR_PROP_BIT_WRITE;

// Default CCCD values (notifications disabled)
static const uint8_t cccd_default[2] = {0x00, 0x00};

// Default characteristic values
static const uint8_t char_value_default[4] = {0x00, 0x00, 0x00, 0x00};

//
// WiFi Provisioning Service Table
//
const esp_gatts_attr_db_t wifi_gatt_db[WIFI_IDX_NB] = {
    // WiFi Provisioning Service Declaration
    [IDX_WIFI_SVC] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
         sizeof(prov_service_uuid), sizeof(prov_service_uuid), (uint8_t *)prov_service_uuid}
    },

    // WiFi RX Characteristic (Credential Input) - Write
    [IDX_WIFI_RX_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
         CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write}
    },
    [IDX_WIFI_RX_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_128, (uint8_t *)wifi_rx_uuid, ESP_GATT_PERM_WRITE,
         CHAR_VAL_LEN_MAX, sizeof(char_value_default), (uint8_t *)char_value_default}
    },

    // WiFi TX Characteristic (Status Output) - Notify + Read
    [IDX_WIFI_TX_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
         CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_notify_read}
    },
    [IDX_WIFI_TX_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_128, (uint8_t *)wifi_tx_uuid, ESP_GATT_PERM_READ,
         CHAR_VAL_LEN_MAX, sizeof(char_value_default), (uint8_t *)char_value_default}
    },
    [IDX_WIFI_TX_CFG] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), sizeof(cccd_default), (uint8_t *)cccd_default}
    },
};

//
// Herschel Control Service Table
//
const esp_gatts_attr_db_t herschel_gatt_db[HERSCHEL_IDX_NB] = {
    // Herschel Control Service Declaration
    [IDX_HERSCHEL_SVC] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
         sizeof(herschel_service_uuid), sizeof(herschel_service_uuid), (uint8_t *)herschel_service_uuid}
    },

    // Control Point Characteristic - Write + Indicate
    [IDX_CONTROL_POINT_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
         CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_indicate_write}
    },
    [IDX_CONTROL_POINT_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_128, (uint8_t *)control_point_uuid, ESP_GATT_PERM_WRITE | ESP_GATT_PERM_READ,
         CHAR_VAL_LEN_MAX, sizeof(char_value_default), (uint8_t *)char_value_default}
    },
    [IDX_CONTROL_POINT_CFG] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), sizeof(cccd_default), (uint8_t *)cccd_default}
    },

    // Response Characteristic - Notify + Read
    [IDX_RESPONSE_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
         CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_notify_read}
    },
    [IDX_RESPONSE_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_128, (uint8_t *)response_uuid, ESP_GATT_PERM_READ,
         CHAR_VAL_LEN_MAX, sizeof(char_value_default), (uint8_t *)char_value_default}
    },
    [IDX_RESPONSE_CFG] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), sizeof(cccd_default), (uint8_t *)cccd_default}
    },

    // Temperature Data Characteristic - Notify + Read (Auto-streaming)
    [IDX_TEMP_DATA_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
         CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_notify_read}
    },
    [IDX_TEMP_DATA_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_128, (uint8_t *)temperature_data_uuid, ESP_GATT_PERM_READ,
         CHAR_VAL_LEN_MAX, sizeof(char_value_default), (uint8_t *)char_value_default}
    },
    [IDX_TEMP_DATA_CFG] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), sizeof(cccd_default), (uint8_t *)cccd_default}
    },

    // Presence Data Characteristic - Notify + Read (Auto-streaming)
    [IDX_PRESENCE_DATA_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
         CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_notify_read}
    },
    [IDX_PRESENCE_DATA_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_128, (uint8_t *)presence_data_uuid, ESP_GATT_PERM_READ,
         CHAR_VAL_LEN_MAX, sizeof(char_value_default), (uint8_t *)char_value_default}
    },
    [IDX_PRESENCE_DATA_CFG] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), sizeof(cccd_default), (uint8_t *)cccd_default}
    },
};