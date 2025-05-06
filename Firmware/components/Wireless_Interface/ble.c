#include "defines.h"
#include "Comm_Interface.h"

#define COMPANY_ID_TO_BYTES(id)  ((uint8_t)((id) & 0xFF)), ((uint8_t)(((id) >> 8) & 0xFF))
#define COMPANY_ID_LEN           2

uint8_t adv_config_done = 0;
static esp_gatt_char_prop_t a_property = 0;
prepare_type_env_t a_prepare_write_env;

const uint8_t company_id[2] = {COMPANY_ID_TO_BYTES(COMPANY_ID)};
const uint8_t prov_service_uuid[16] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xC3, 0x0E, 0x00, 0x10, 0xA3, 0xB5, 0x7E, 0x00, 0x40, 0x7E};
const uint8_t uart_service_uuid[16] = {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xC3, 0x0E, 0x00, 0x20, 0xA3, 0xB5, 0x7E, 0x00, 0x40, 0x7E};
const uint8_t uart_tx_uuid[16] =      {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xC3, 0x0E, 0x01, 0x20, 0xA3, 0xB5, 0x7E, 0x00, 0x40, 0x7E};
const uint8_t uart_rx_uuid[16] =      {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xC3, 0x0E, 0x02, 0x20, 0xA3, 0xB5, 0x7E, 0x00, 0x40, 0x7E};
const uint8_t prov_chr_uuid[16] =     {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xC3, 0x0E, 0x01, 0x10, 0xA3, 0xB5, 0x7E, 0x00, 0x40, 0x7E};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

#if CONFIG_SET_RAW_ADV_DATA
static uint8_t raw_adv_data[] = {
    0x02, 0x01, 0x06,       // Length 2, Data Type 1 (Flags), Data 1 (LE General Discoverable Mode, BR/EDR Not Supported)
    0x02, 0x0a, 0xeb,       // Length 2, Data Type 10 (TX power leve), Data 2 (-21)
    0x03, 0x03, 0xab, 0xcd, // Length 3, Data Type 3 (Complete 16-bit Service UUIDs), Data 3 (UUID)
};
static uint8_t raw_scan_rsp_data[] = { // Length 15, Data Type 9 (Complete Local Name), Data 1 (ESP_GATTS_DEMO)
    0x0f, 0x09, 0x45, 0x53, 0x50, 0x5f, 0x47, 0x41, 0x54, 0x54, 0x53, 0x5f, 0x44,
    0x45, 0x4d, 0x4f};
#else
// The length of adv data must be less than 31 bytes
esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006, // slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, // slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x00,
    .manufacturer_len = COMPANY_ID_LEN,
    .p_manufacturer_data = company_id,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0,  //sizeof(prov_service_uuid),      NO NEED TO ADVERTISE SERVICE UUIDs
    .p_service_uuid = NULL, //prov_service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = false,
    .include_txpower = true,
    //.min_interval = 0x0006,
    //.max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,       // TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = NULL, //&test_manufacturer[0],
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0,  //sizeof(uart_service_uuid),      NO NEED TO ADVERTISE SERVICE UUIDs
    .p_service_uuid = NULL, //uart_service_uuid,
    .flag = 0,
};
#endif /* CONFIG_SET_RAW_ADV_DATA */

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_PROV_APP_ID] = {
        .gatts_cb = gatts_prov_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
        .char_count = 0,
    },
    [PROFILE_UART_APP_ID] = {
        .gatts_cb = gatts_uart_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
        .char_count = 0,
    }
};

static uint8_t char1_str[] = {0x11, 0x22, 0x33};
static esp_attr_value_t gatts_char_val =
{
    .attr_max_len = GATTS_CHAR_VAL_LEN_MAX,
    .attr_len = sizeof(char1_str),
    .attr_value = char1_str,
};


void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
#if CONFIG_SET_RAW_ADV_DATA
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        adv_config_done &= (~adv_config_flag);
        if (adv_config_done == 0)
        {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
        adv_config_done &= (~scan_rsp_config_flag);
        if (adv_config_done == 0)
        {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
#else
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~adv_config_flag);
        if (adv_config_done == 0)
        {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~scan_rsp_config_flag);
        if (adv_config_done == 0)
        {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
#endif
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        // advertising start complete event to indicate advertising start successfully or failed
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(GATTS_TAG, "Advertising start failed\n");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(GATTS_TAG, "Advertising stop failed\n");
        }
        else
        {
            ESP_LOGI(GATTS_TAG, "Stop adv successfully\n");
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(GATTS_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                 param->update_conn_params.status,
                 param->update_conn_params.min_int,
                 param->update_conn_params.max_int,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.app_id == PROFILE_PROV_APP_ID) {
            gl_profile_tab[PROFILE_PROV_APP_ID].gatts_if = gatts_if;
        } else if (param->reg.app_id == PROFILE_UART_APP_ID) {
            gl_profile_tab[PROFILE_UART_APP_ID].gatts_if = gatts_if;
        }
    }
    else if(event == ESP_GATTS_DISCONNECT_EVT)
    {
        ESP_LOGI(GATTS_TAG, "ESP_GATTS_DISCONNECT_EVT, disconnect reason 0x%x", param->disconnect.reason);
        esp_ble_gap_start_advertising(&adv_params);
    }

    /* If the gatts_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do
    {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++)
        {
            if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                gatts_if == gl_profile_tab[idx].gatts_if)
            {
                if (gl_profile_tab[idx].gatts_cb)
                {
                    gl_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

void gatts_prov_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GATTS_REG_EVT:
        ESP_LOGD(GATTS_TAG, "ESP_GATTS_REG_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
        gl_profile_tab[PROFILE_PROV_APP_ID].service_id.is_primary = true;
        gl_profile_tab[PROFILE_PROV_APP_ID].service_id.id.inst_id = 0x00;
        gl_profile_tab[PROFILE_PROV_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_128;
        // gl_profile_tab[PROFILE_PROV_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID;
        memcpy(gl_profile_tab[PROFILE_PROV_APP_ID].service_id.id.uuid.uuid.uuid128,
            prov_service_uuid, ESP_UUID_LEN_128);

        esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_PROV_APP_ID].service_id, GATTS_NUM_HANDLE_A);
        break;

    case ESP_GATTS_CREATE_EVT:
        ESP_LOGD(GATTS_TAG, "ESP_GATTS_CREATE_EVT, status %d,  service_handle %d", param->create.status, param->create.service_handle);
        gl_profile_tab[PROFILE_PROV_APP_ID].service_handle = param->create.service_handle;
        gl_profile_tab[PROFILE_PROV_APP_ID].char_uuid.len = ESP_UUID_LEN_128;
        // gl_profile_tab[PROFILE_PROV_APP_ID].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID;
        memcpy(gl_profile_tab[PROFILE_PROV_APP_ID].char_uuid.uuid.uuid128,
            prov_chr_uuid, ESP_UUID_LEN_128);

        esp_ble_gatts_start_service(gl_profile_tab[PROFILE_PROV_APP_ID].service_handle);
        a_property = ESP_GATT_CHAR_PROP_BIT_WRITE;
        esp_err_t add_char_ret = esp_ble_gatts_add_char(gl_profile_tab[PROFILE_PROV_APP_ID].service_handle, &gl_profile_tab[PROFILE_PROV_APP_ID].char_uuid,
                                                        ESP_GATT_PERM_WRITE,
                                                        a_property,
                                                        &gatts_char_val, NULL);
        if (add_char_ret)
        {
            ESP_LOGE(GATTS_TAG, "add char failed, error code =%x", add_char_ret);
        }
        break;

    case ESP_GATTS_ADD_CHAR_EVT:
        gl_profile_tab[PROFILE_PROV_APP_ID].char_handle[gl_profile_tab[PROFILE_PROV_APP_ID].char_count++] = param->add_char.attr_handle;

        ESP_LOGD(GATTS_TAG, "ESP_GATTS_ADD_CHAR_EVT PROV PROFILE, attr_handle = %d", param->add_char.attr_handle);
        break;

    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        gl_profile_tab[PROFILE_PROV_APP_ID].descr_handle = param->add_char_descr.attr_handle;
        ESP_LOGD(GATTS_TAG, "ESP_GATTS_ADD_CHAR_DESCR_EVT, status %d, attr_handle %d, service_handle %d\n",
                 param->add_char_descr.status, param->add_char_descr.attr_handle, param->add_char_descr.service_handle);
        break;

    case ESP_GATTS_WRITE_EVT:
    {
        // ESP_LOGI(GATTS_TAG, "PROV_WRITE_EVT");
        if(param->write.len > 0)
        {
            ESP_LOGI(GATTS_TAG, "Received data:");
            ESP_LOG_BUFFER_HEX(GATTS_TAG, param->write.value, param->write.len);
        }
        if (!param->write.is_prep)
        {
            if (gl_profile_tab[PROFILE_PROV_APP_ID].descr_handle == param->write.handle && param->write.len == 2)
            {
                uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
                if (descr_value == 0x0001)
                {
                    if (a_property & ESP_GATT_CHAR_PROP_BIT_NOTIFY)
                    {
                        ESP_LOGI(GATTS_TAG, "notify enable");
                        uint8_t notify_data[15];
                        for (int i = 0; i < sizeof(notify_data); ++i)
                        {
                            notify_data[i] = i % 0xff;
                        }
                        // the size of notify_data[] need less than MTU size
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gl_profile_tab[PROFILE_PROV_APP_ID].char_handle[gl_profile_tab[PROFILE_PROV_APP_ID].char_count],
                                                    sizeof(notify_data), notify_data, false);
                    }
                }
                else if (descr_value == 0x0002)
                {
                    if (a_property & ESP_GATT_CHAR_PROP_BIT_INDICATE)
                    {
                        ESP_LOGI(GATTS_TAG, "indicate enable");
                        uint8_t indicate_data[15];
                        for (int i = 0; i < sizeof(indicate_data); ++i)
                        {
                            indicate_data[i] = i % 0xff;
                        }
                        // the size of indicate_data[] need less than MTU size
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gl_profile_tab[PROFILE_PROV_APP_ID].char_handle[gl_profile_tab[PROFILE_PROV_APP_ID].char_count],
                                                    sizeof(indicate_data), indicate_data, true);
                    }
                }
                else if (descr_value == 0x0000)
                {
                    ESP_LOGI(GATTS_TAG, "notify/indicate disable ");
                }
                else
                {
                    ESP_LOGE(GATTS_TAG, "unknown descr value");
                }
            }
        }

        write_event_env(gatts_if, &a_prepare_write_env, param);
        break;
    }

    case ESP_GATTS_EXEC_WRITE_EVT:
        ESP_LOGD(GATTS_TAG, "ESP_GATTS_EXEC_WRITE_EVT");
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        exec_write_event_env(&a_prepare_write_env, param);
        break;

    case ESP_GATTS_START_EVT:
        ESP_LOGD(GATTS_TAG, "ESP_GATTS_START_EVT, status %d, service_handle %d",
                 param->start.status, param->start.service_handle);
        break;

    case ESP_GATTS_CONNECT_EVT:
    {
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
        conn_params.latency = 0;
        conn_params.max_int = 0x20; // max_int = 0x20*1.25ms = 40ms
        conn_params.min_int = 0x10; // min_int = 0x10*1.25ms = 20ms
        conn_params.timeout = 400;  // timeout = 400*10ms = 4000ms
        ESP_LOGD(GATTS_TAG, "ESP_GATTS_CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:",
                 param->connect.conn_id,
                 param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                 param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
        gl_profile_tab[PROFILE_PROV_APP_ID].conn_id = param->connect.conn_id;
        // start sent the update connection parameters to the peer device.
        esp_ble_gap_update_conn_params(&conn_params);
        break;
    }

    default:
        break;
    }
}

void gatts_uart_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GATTS_REG_EVT:
        ESP_LOGD(GATTS_TAG, "ESP_GATTS_REG_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
        gl_profile_tab[PROFILE_UART_APP_ID].service_id.is_primary = true;
        gl_profile_tab[PROFILE_UART_APP_ID].service_id.id.inst_id = 0x00;
        gl_profile_tab[PROFILE_UART_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_128;
        memcpy(gl_profile_tab[PROFILE_UART_APP_ID].service_id.id.uuid.uuid.uuid128,
               uart_service_uuid, ESP_UUID_LEN_128);

        esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_UART_APP_ID].service_id,
                                     GATTS_NUM_HANDLE_B);
        break;

    case ESP_GATTS_CREATE_EVT:
        ESP_LOGD(GATTS_TAG, "ESP_GATTS_CREATE_EVT, status %d, service_handle %d",
                 param->create.status, param->create.service_handle);

        gl_profile_tab[PROFILE_UART_APP_ID].service_handle = param->create.service_handle;

        // Tx Characteristic
        gl_profile_tab[PROFILE_UART_APP_ID].char_uuid.len = ESP_UUID_LEN_128;
        memcpy(gl_profile_tab[PROFILE_UART_APP_ID].char_uuid.uuid.uuid128, uart_tx_uuid, ESP_UUID_LEN_128);

        // Rx Characteristic
        gl_profile_tab[PROFILE_UART_APP_ID].char1_uuid.len = ESP_UUID_LEN_128;
        memcpy(gl_profile_tab[PROFILE_UART_APP_ID].char1_uuid.uuid.uuid128, uart_rx_uuid, ESP_UUID_LEN_128);

        esp_ble_gatts_start_service(gl_profile_tab[PROFILE_UART_APP_ID].service_handle);

        //Tx
        esp_err_t add_tx_char_ret = esp_ble_gatts_add_char(gl_profile_tab[PROFILE_UART_APP_ID].service_handle,
            &gl_profile_tab[PROFILE_UART_APP_ID].char_uuid,
            ESP_GATT_PERM_WRITE | ESP_GATT_PERM_READ,
            ESP_GATT_CHAR_PROP_BIT_WRITE,
            // NULL, 
            &gatts_char_val,
            NULL);

        //Rx
        esp_err_t add_rx_char_ret = esp_ble_gatts_add_char(gl_profile_tab[PROFILE_UART_APP_ID].service_handle,
                &gl_profile_tab[PROFILE_UART_APP_ID].char1_uuid,
                ESP_GATT_PERM_WRITE | ESP_GATT_PERM_READ,
                ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                // NULL, 
                &gatts_char_val,
                NULL);

        if (add_tx_char_ret || add_rx_char_ret) { 
            ESP_LOGE(GATTS_TAG, "UART PROFILE add char failed, error code = %x", add_tx_char_ret|add_rx_char_ret );
        }
        break;

    case ESP_GATTS_ADD_CHAR_EVT:
        // // gl_profile_tab[PROFILE_UART_APP_ID].char_handle = param->add_char.attr_handle; //tx
        gl_profile_tab[PROFILE_UART_APP_ID].char_handle[gl_profile_tab[PROFILE_UART_APP_ID].char_count++] = param->add_char.attr_handle; //rx

        ESP_LOGD(GATTS_TAG, "ESP_GATTS_ADD_CHAR_EVT UART PROFILE, attr_handle = %d", param->add_char.attr_handle);
        
        if(gl_profile_tab[PROFILE_UART_APP_ID].char_count == 1)
        {
            esp_bt_uuid_t descr_uuid = {
                .len = ESP_UUID_LEN_16,
                .uuid = { .uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG },
            };
        
            esp_attr_value_t cccd_val = {
                .attr_max_len = 2,
                .attr_len = 2,
                .attr_value = (uint8_t[]){0x00, 0x00},
            };
        
            esp_ble_gatts_add_char_descr( gl_profile_tab[PROFILE_UART_APP_ID].service_handle,
                                          &descr_uuid,
                                          ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                          &cccd_val,
                                          NULL);
        }
        break;

    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        gl_profile_tab[PROFILE_UART_APP_ID].descr_handle = param->add_char_descr.attr_handle;
        ESP_LOGD(GATTS_TAG, "ESP_GATTS_ADD_CHAR_DESCR_EVT, handle = %d", param->add_char_descr.attr_handle);
        
        break;

    case ESP_GATTS_START_EVT:
        ESP_LOGD(GATTS_TAG, "ESP_GATTS_START_EVT, status %d, service_handle %d\n",
                 param->start.status, param->start.service_handle);
        break;

    case ESP_GATTS_WRITE_EVT:
        if(param->write.len > 0)
        {
            ESP_LOGI(GATTS_TAG, "Received data:");
            ESP_LOG_BUFFER_HEX(GATTS_TAG, param->write.value, param->write.len);
        }
        if (!param->write.is_prep)
        {
            if (gl_profile_tab[PROFILE_PROV_APP_ID].descr_handle == param->write.handle && param->write.len == 2)
            {
                uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
                if (descr_value == 0x0001)
                {
                    if (a_property & ESP_GATT_CHAR_PROP_BIT_NOTIFY)
                    {
                        ESP_LOGI(GATTS_TAG, "notify enable");
                        uint8_t notify_data[15];
                        for (int i = 0; i < sizeof(notify_data); ++i)
                        {
                            notify_data[i] = i % 0xff;
                        }
                        // the size of notify_data[] need less than MTU size
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gl_profile_tab[PROFILE_PROV_APP_ID].char_handle[0],
                                                    sizeof(notify_data), notify_data, false);
                    }
                }
                else if (descr_value == 0x0002)
                {
                    if (a_property & ESP_GATT_CHAR_PROP_BIT_INDICATE)
                    {
                        ESP_LOGI(GATTS_TAG, "indicate enable");
                        uint8_t indicate_data[15];
                        for (int i = 0; i < sizeof(indicate_data); ++i)
                        {
                            indicate_data[i] = i % 0xff;
                        }
                        // the size of indicate_data[] need less than MTU size
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gl_profile_tab[PROFILE_PROV_APP_ID].char_handle[0],
                                                    sizeof(indicate_data), indicate_data, true);
                    }
                }
                else if (descr_value == 0x0000)
                {
                    ESP_LOGI(GATTS_TAG, "notify/indicate disable ");
                }
                else
                {
                    ESP_LOGE(GATTS_TAG, "unknown descr value");
                }
            }
        }
        write_event_env(gatts_if, &a_prepare_write_env, param);

        break;

    case ESP_GATTS_EXEC_WRITE_EVT:
        ESP_LOGD(GATTS_TAG, "ESP_GATTS_EXEC_WRITE_EVT");
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        exec_write_event_env(&a_prepare_write_env, param);
        break;

    case ESP_GATTS_CONNECT_EVT:
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
        conn_params.latency = 0;
        conn_params.max_int = 0x20; // max_int = 0x20*1.25ms = 40ms
        conn_params.min_int = 0x10; // min_int = 0x10*1.25ms = 20ms
        conn_params.timeout = 400;  // timeout = 400*10ms = 4000ms
        ESP_LOGD(GATTS_TAG, "ESP_GATTS_CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:",
                 param->connect.conn_id,
                 param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                 param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
        gl_profile_tab[PROFILE_UART_APP_ID].conn_id = param->connect.conn_id;
        // start sent the update connection parameters to the peer device.
        esp_ble_gap_update_conn_params(&conn_params);
        gl_profile_tab[PROFILE_UART_APP_ID].conn_id = param->connect.conn_id;
        break;

    default:
        break;
    }
}