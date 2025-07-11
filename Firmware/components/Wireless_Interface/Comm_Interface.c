#include "Comm_Interface.h"
#include "ble_service_tables.h"
#include <string.h>
#include <time.h>

// Forward declarations for BLE streaming structures (defined in ble.c)

// BLE streaming structures defined in ble.c - no need for duplicates

#define TAG "COMM_INTERFACE"
#define COMM_INTERFACE_BLETAG "COMM_INTERFACE"

#define CHECK_PROPERTY(property)                \
    if(!property) {                           \
        blecmd = NO_COMMAND;                    \
        break;                                  \
    }

BLE_COMMANDS blecmd;
char* bleParameters[2] = {NULL, NULL};
char *numberOfGates= NULL;

GateParameters* gateParams = NULL;
static int bond_dev_num = 0;
static esp_ble_bond_dev_t *bond_dev_list = NULL;

bool uart_cmd_handler_flag = false;
    
wifi_ap_record_t ap_info[20]; // max 20 APs
ScanWifi filteredNetworks[20];
const char* wifiSSIDJSON;
uint16_t ap_count = 0;
uint16_t number = 20;
uint16_t scanStartTime;
uint16_t scanFinishedTime;
uint16_t scanProgesstime;
uint16_t totalScanTime;
uint8_t filteredNetworksCount = 0;

TaskHandle_t xWifiScanTaskHandle = NULL;
TaskHandle_t xWifiScanDurationHandle= NULL;

bool wifiScanAction = false;
bool scanStart=false;
bool scanfinsihed=false;
bool scanInProgress=false;


char *cmdStringFetch[]  = {
    "NoCommand",
    "powerControl",
    "factoryReset",
    "readInternalTemp",
    "readIRTemperatures",
    "setTargetTemp",
    "getTargetTemp",
    "setOperatingMode",
    "getOperatingMode",
    "readRelayState",
    "setRelayState",
    "controlSensorBoard",
    "readPresenceData",
    "readWiFiStatus",
    "readSystemDiagnostics",
    "readEnergyMetrics",
    "controlFrontLED",
    "readDeviceInfo",
    "scanWifiSSIDs"
};

void wifi_scan_task(void *pvParameters)
{
    if(scan_wifi_ssids() != ESP_OK)
    {
        ESP_LOGE(TAG " WIFI SCAN", "WIFI SCAN FAILED");
    }
    else{
        ESP_LOGI(TAG " WIFI SCAN", "WIFI SCAN PASSED");
    }
    vTaskDelete(NULL);
}

void scanDuration_task (void *pvParameters)
{
    while(1)
    {
        if(scanStart)
        {
            scanStartTime = US_TO_SEC(esp_timer_get_time());
            scanStart=false;
            scanInProgress=true;
        }
        else if(scanInProgress)
        {
            scanProgesstime = US_TO_SEC(esp_timer_get_time()) - scanStartTime;
            ESP_LOGI(TAG " WIFI SCAN", "Scan in Progress ! Time: %d", scanProgesstime);
        }
        else if(scanfinsihed)
        {   
            scanfinsihed = false;
            scanFinishedTime = US_TO_SEC(esp_timer_get_time());
            totalScanTime= scanFinishedTime -scanStartTime;
            ESP_LOGI(TAG " WIFI SCAN", "Total Wifi Scan time: %d seconds", totalScanTime);
            vTaskDelete(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Delay for 1 second
    }
    vTaskDelete(NULL);
}


void ble_command_task(void * pvParameters)
{
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(500));

        if(uart_cmd_handler_flag)
        {
            ESP_LOGI(TAG, "EXECUTING COMMAND: 0x%02X", blecmd);
            execute_command();
            uart_cmd_handler_flag = false; // Reset flag after execution
            ESP_LOGI(TAG, "COMMAND EXECUTION COMPLETE: 0x%02X", blecmd);
        }
    }
    vTaskDelete( NULL );
}

void comm_init(void)
{
    /* Initialize networking stack */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Initialize Wi-Fi including netif with default config */
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));



    start_ble();



    if(xTaskCreate(ble_command_task, "ble_command_task", 4096, NULL, 1, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create ble_command_task  task");
        set_led_effect(LED_EFFECT_BLINK, RED ,LED_SPEED_SLOW );
        set_device_status(1, ERROR);
        return;
    }
    ESP_LOGD(TAG, "ble_command_task created successfully");
}

bool startBLEAdvert()
{   
    if(esp_ble_gap_start_advertising(&adv_params) != ESP_OK)
    {
        return false;
    }
    else{ return true;}
}

void display_bonded_devices()
{
    // startAdvertising();
    // First get count of bonded devices
    bond_dev_num = esp_ble_get_bond_device_num();
    if (bond_dev_num > 0) {
        bond_dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * bond_dev_num);
        esp_ble_get_bond_device_list(&bond_dev_num, bond_dev_list);
         for (int i = 0; i < bond_dev_num; i++) {
            ESP_LOGI(TAG, "Bonded device: %s", bond_dev_list[i].bd_addr);
        }
        free(bond_dev_list);
    }
}

void remove_bond_devices()
{
    int dev_num = esp_ble_get_bond_device_num();
    esp_ble_bond_dev_t *bonded_devices = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, bonded_devices);

    for (int i = 0; i < dev_num; i++) {
        esp_ble_remove_bond_device(bonded_devices[i].bd_addr);
    }
    free(bonded_devices);

}

void start_ble(void)
{
    esp_err_t ret;
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_ONLY;   // "Just Works" encryption without bonding
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    uint8_t key_size = BLE_PAIR_ENCRYPTION_KEY; // max encryption key size
    uint8_t key_mask = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

    ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
    {
        ESP_LOGE(TAG GATTS_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
    {
        ESP_LOGE(TAG GATTS_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_init();
    if (ret)
    {
        ESP_LOGE(TAG GATTS_TAG, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_enable();
    if (ret)
    {
        ESP_LOGE(TAG GATTS_TAG, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "gap register error, error code = %x", ret);
        return;
    }
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret)
    {
        ESP_LOGE(TAG GATTS_TAG, "gatts register error, error code = %x", ret);
        return;
    }
    ret = esp_ble_gatts_app_register(PROFILE_PROV_APP_ID);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "gatts app register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gatts_app_register(PROFILE_UART_APP_ID);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "gatts app register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gap_set_device_name(BLE_DEVICE_NAME);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "set device name failed, error code = %x", ret);
    }

#if CONFIG_SET_RAW_ADV_DATA
    esp_err_t raw_adv_ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
    if (raw_adv_ret)
    {
        ESP_LOGE(GATTS_TAG, "config raw adv data failed, error code = %x ", raw_adv_ret);
    }
    adv_config_done |= adv_config_flag;
    esp_err_t raw_scan_ret = esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
    if (raw_scan_ret)
    {
        ESP_LOGE(GATTS_TAG, "config raw scan rsp data failed, error code = %x", raw_scan_ret);
    }
    adv_config_done |= scan_rsp_config_flag;
#else
    // config adv data
    ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "config adv data failed, error code = %x", ret);
    }
    adv_config_done |= adv_config_flag;
    // config scan response data
    ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
    if (ret)
    {
        ESP_LOGE(GATTS_TAG, "config scan response data failed, error code = %x", ret);
    }
    adv_config_done |= scan_rsp_config_flag;
#endif

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(517);
    if (local_mtu_ret)
    {
        ESP_LOGE(GATTS_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }

    display_bonded_devices();
    esp_err_t ble_sec_parm_ret;

    // Set authentication requirement (e.g. Just Works, MITM, etc.)
    ble_sec_parm_ret = esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(auth_req));
    if (ble_sec_parm_ret != ESP_OK)
    {
        ESP_LOGE(GATTS_TAG, "Failed to set AUTHEN_REQ_MODE: 0x%X", ble_sec_parm_ret);
    }

    // Set IO capabilities (e.g. NONE, DISPLAY_ONLY, etc.)
    ble_sec_parm_ret = esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(iocap));
    if (ble_sec_parm_ret != ESP_OK)
    {
        ESP_LOGE(GATTS_TAG, "Failed to set IOCAP_MODE: 0x%X", ble_sec_parm_ret);
    }

    // Set maximum encryption key size
    ble_sec_parm_ret = esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(key_size));
    if (ble_sec_parm_ret != ESP_OK)
    {
        ESP_LOGE(GATTS_TAG, "Failed to set MAX_KEY_SIZE: 0x%X", ble_sec_parm_ret);
    }

    // Set initiator key mask (what your device sends)
    ble_sec_parm_ret = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &key_mask, sizeof(key_mask));
    if (ble_sec_parm_ret != ESP_OK)
    {
        ESP_LOGE(GATTS_TAG, "Failed to set INIT_KEY mask: 0x%X", ble_sec_parm_ret);
    }

    // Set responder key mask (what your device accepts)
    ble_sec_parm_ret = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &key_mask, sizeof(key_mask));
    if (ble_sec_parm_ret != ESP_OK)
    {
        ESP_LOGE(GATTS_TAG, "Failed to set RSP_KEY mask: 0x%X", ble_sec_parm_ret);
    }
}

uint8_t parse_command(uint8_t *data, int len, const char *property)
{
    // Handle JSON WiFi provisioning
    if (strcmp(property, "wifi") == 0)
    {
        ESP_LOGI(TAG, "Processing WiFi JSON provisioning data");
        parse_wifi_json_credentials(data, len);
        return NO_COMMAND; // No binary command to execute
    }

    char packet[len + 1]; // Create a buffer with space for null terminator
    memcpy(packet, data, len);
    packet[len] = '\0'; // Ensure null termination

    ESP_LOGI(TAG, "%s", packet);

    // Extract command
    blecmd = (BLE_COMMANDS)(uint8_t)packet[0];

    // Determine whether to allow command based on property
    bool allow_all = (strcmp(property, "uart") == 0) || (strcmp(property, "ble") == 0);
    // bool cmd_wifi = (strcmp(property, "wifi") == 0);
    // bool cmd_uart = (strcmp(property, "uart") == 0);

    // Extract property value
    switch (blecmd){

        // Commands that don't require parameters
        case POWER_CONTROL:
        case FACTORY_RESET:
        case READ_INTERNAL_TEMP:
        case GET_TARGET_TEMP:
        case GET_OPERATING_MODE:
        case READ_RELAY_STATE:
        case READ_PRESENCE_DATA:
        case READ_WIFI_STATUS:
        case READ_SYSTEM_DIAGNOSTICS:
        case READ_ENERGY_METRICS:
        case READ_DEVICE_INFO:
            if (!allow_all || len > 1) {
                blecmd = NO_COMMAND;
            }
            break;

        case READ_IR_TEMPERATURES:
            if (!allow_all) {
                blecmd = NO_COMMAND;
                break;
            }
            // Optional parameter for temperature type (ambient/object)
            char* temp_type = (char*)(malloc(1));
            if (!temp_type) {
                ESP_LOGE(TAG, "Memory allocation failed for temperature type parameter.!\n");
                return 0;
            }

            if(len > 1)
            {
                memcpy(temp_type, &packet[1], 1);
            }
            else
            {
                *temp_type = 0x00; // Default to both temperatures
            }

            bleParameters[0] = temp_type;
            break;

        case SET_TARGET_TEMP:
            if (!allow_all) {
                blecmd = NO_COMMAND;
                break;
            }
            if (len < 5) {  // Need command + 4 bytes for float
                ESP_LOGE(TAG, "SET_TARGET_TEMP: insufficient data length %d", len);
                blecmd = NO_COMMAND;
                break;
            }
            char* temp_bytes = (char*)(malloc(4));
            if (!temp_bytes) {
                ESP_LOGE(TAG, "Memory allocation failed for temperature parameters!");
                blecmd = NO_COMMAND;
                break;
            }
            memcpy(temp_bytes, &packet[1], 4);
            bleParameters[0] = temp_bytes;
            break;

        case SET_RELAY_STATE:
            if (!allow_all) {
                blecmd = NO_COMMAND;
                break;
            }
            if (len < 2) {  // Need command + parameter
                ESP_LOGE(TAG, "SET_RELAY_STATE: insufficient data length %d", len);
                blecmd = NO_COMMAND;
                break;
            }
            char* relay_state = (char*)(malloc(1));
            if (!relay_state) {
                ESP_LOGE(TAG, "Memory allocation failed for relay state parameter!");
                blecmd = NO_COMMAND;
                break;
            }
            memcpy(relay_state, &packet[1], 1);
            bleParameters[0] = relay_state;
            break;

        case SET_OPERATING_MODE:
            if (!allow_all) {
                blecmd = NO_COMMAND;
                break;
            }
            if (len < 2) {  // Need command + parameter
                ESP_LOGE(TAG, "SET_OPERATING_MODE: insufficient data length %d", len);
                blecmd = NO_COMMAND;
                break;
            }
            char* mode = (char*)(malloc(1));
            if (!mode) {
                ESP_LOGE(TAG, "Memory allocation failed for operating mode parameter!");
                blecmd = NO_COMMAND;
                break;
            }
            memcpy(mode, &packet[1], 1);
            bleParameters[0] = mode;
            ESP_LOGI(TAG, "SET_OPERATING_MODE command parsed successfully, mode=%d", *mode);
            break;

        case CONTROL_SENSOR_BOARD:
            if (!allow_all) {
                blecmd = NO_COMMAND;
                break;
            }
            if (len < 2) {  // Need command + parameter
                ESP_LOGE(TAG, "CONTROL_SENSOR_BOARD: insufficient data length %d", len);
                blecmd = NO_COMMAND;
                break;
            }
            char* sensor_control = (char*)(malloc(1));
            if (!sensor_control) {
                ESP_LOGE(TAG, "Memory allocation failed for sensor board control parameter!");
                blecmd = NO_COMMAND;
                break;
            }
            memcpy(sensor_control, &packet[1], 1);
            bleParameters[0] = sensor_control;
            ESP_LOGI(TAG, "CONTROL_SENSOR_BOARD command received");
            break;

        case CONTROL_FRONT_LED:
            if (!allow_all) {
                blecmd = NO_COMMAND;
                break;
            }
            if (len < 2) {  // Need command + parameter
                ESP_LOGE(TAG, "CONTROL_FRONT_LED: insufficient data length %d", len);
                blecmd = NO_COMMAND;
                break;
            }
            char* led_control = (char*)(malloc(1));
            if (!led_control) {
                ESP_LOGE(TAG, "Memory allocation failed for LED control parameter!");
                blecmd = NO_COMMAND;
                break;
            }
            memcpy(led_control, &packet[1], 1);
            bleParameters[0] = led_control;
            ESP_LOGI(TAG, "CONTROL_FRONT_LED command received");
            break;

        case WIFI_SCAN:
            if (!allow_all) {
                blecmd = NO_COMMAND;
                break;
            }
            wifiScanAction=true;
            ESP_LOGI(TAG, "WIFI_SCAN command received");
                // Create the task for the IR sensor
            if(xTaskCreate(wifi_scan_task, "wifi_scan_task", 4096, NULL, 1, &xWifiScanTaskHandle) != pdPASS)
            {
                ESP_LOGE(TAG, "Failed to create wifi_scan_task  task");
                break;
            }


        default:
            blecmd = NO_COMMAND;
            break;
    }



    if((blecmd < 1) || (blecmd >= MAX_BLE_CMDS))
    {
        uart_cmd_handler_flag = false;
    }
    else
    {
        uart_cmd_handler_flag = true;
    }

    ESP_LOGD(TAG, "Command received: %s",
            ((blecmd >= NO_COMMAND) && (blecmd < MAX_BLE_CMDS)) ? cmdStringFetch[blecmd] : cmdStringFetch[0]);

    return blecmd;
}

void send_ble_uart_notification(esp_err_t status, uint8_t *data, uint16_t len)
{
    uint8_t resp[1 + ((len == 0) ? 1: len)];
    ESP_LOGI(TAG GATTS_TAG, "Sending BLE response for command 0x%02X, status=%d, len=%d", blecmd, status, len);
    ESP_LOGD(TAG GATTS_TAG, "send_ble_uart_notification: len %d", len);

    resp[0] = blecmd;
    if(data != NULL)
    {
        ESP_LOGD(TAG GATTS_TAG, "send_ble_uart_notification: data %s", data);
        memcpy(&resp[1], data, len);
        ESP_LOGD(TAG GATTS_TAG, "send_ble_uart_notification: resp");
        ESP_LOG_BUFFER_HEXDUMP(TAG GATTS_TAG, resp, len + 1, ESP_LOG_INFO);
    }
    else
    {
        resp[1] = (status == ESP_OK) ? BLE_RESPONSE_SUCCESS : BLE_RESPONSE_FAIL;
        ESP_LOGD(TAG GATTS_TAG, "send_ble_uart_notification: resp %02X %02X", resp[0], resp[1]);
    }

    // Send to all active clients using the multi-client approach from ble.c
    int notifications_sent = 0;

    ESP_LOGD(TAG GATTS_TAG, "Sending BLE response: status=%d, len=%d, using handle=%d", 
             status, len, herschel_handle_table[IDX_RESPONSE_VAL]);

    for (int i = 0; i < MAX_BLE_CLIENTS; i++) {
        if (client_states[i].is_active) {
            esp_err_t err = esp_ble_gatts_send_indicate(
                client_states[i].gatts_if,
                client_states[i].conn_id,
                herschel_handle_table[IDX_RESPONSE_VAL],
                sizeof(resp), resp, false);
            if (err == ESP_OK) {
                notifications_sent++;
                ESP_LOGD(TAG GATTS_TAG, "BLE response sent to client %d successfully", i);
            } else {
                ESP_LOGE(TAG GATTS_TAG, "Failed to send BLE notification to client %d, error: %s", i, esp_err_to_name(err));
            }
        }
    }

    if (notifications_sent == 0) {
        ESP_LOGW(TAG GATTS_TAG, "No active BLE clients to send notification to");
    } else {
        ESP_LOGI(TAG GATTS_TAG, "BLE response sent to %d clients", notifications_sent);
    }
}

// WiFi
void wifi_init_sta(const char *ssid, const char *password)
{
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {0};
    
    // Check for null pointers to prevent crashes
    if (ssid == NULL || password == NULL) {
        ESP_LOGE(TAG WIFI_TAG, "wifi_init_sta: ssid or password is NULL");
        return;
    }
    
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';  // Ensure null termination
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';  // Ensure null termination

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

esp_err_t load_wifi_credentials(char *ssid, char *password)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE_WIFIDATA, NVS_READONLY, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG WIFI_TAG, "No saved credentials found");
        return err;
    }

    size_t ssid_len = WIFI_SSID_MAX_LEN, pass_len = WIFI_PASS_MAX_LEN;
    nvs_get_str(nvs, "ssid", ssid, &ssid_len);
    nvs_get_str(nvs, "password", password, &pass_len);
    nvs_close(nvs);
    return ESP_OK;
}

esp_err_t save_wifi_credentials(const char *ssid, const char *password)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE_WIFIDATA, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG WIFI_TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return err;
    }

    nvs_set_str(nvs, "ssid", ssid);
    nvs_set_str(nvs, "password", password);
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG WIFI_TAG, "WiFi credentials saved");
    return ESP_OK;
}


esp_err_t send_ble_wifi_SSID_JSON_notification(const char* wifiSSIDJSON )
{
    if(xWifiScanTaskHandle != NULL)
    {
        ESP_LOGW(TAG  " WIFI SCAN", "Sending Wifi SSID JSON on BLE Indicate");
        if(send_wifi_ssid_JSON_notification(wifiSSIDJSON) == ESP_OK)
        {
            return ESP_OK;
        }
        else{
            return ESP_FAIL;
        }
    }
    return ESP_FAIL;
}

// BLE Temperature Data Streaming (12 bytes) - uses CCCD management
void send_ble_temperature_notification(float ambient_temp, float object_temp)
{
    // Use the advanced streaming function with current timestamp
    send_temperature_stream_notification((uint32_t)time(NULL), ambient_temp, object_temp);
}

// BLE Presence Data Streaming (18 bytes) - uses CCCD management
void send_ble_presence_notification(uint8_t has_target, float move_dist, float still_dist, uint8_t move_energy, uint8_t still_energy)
{
    // Use the advanced streaming function with current timestamp
    send_presence_stream_notification((uint32_t)time(NULL), has_target, move_dist, still_dist, move_energy, still_energy);
}

// WiFi Status Reading Function
esp_err_t get_wifi_status(char *ssid_out, uint8_t *connected_out, int8_t *rssi_out, uint32_t *ip_address_out, uint8_t *internet_available_out)
{
    // Initialize all outputs to default/safe values
    if (ssid_out) strcpy(ssid_out, "");
    if (connected_out) *connected_out = 0;
    if (rssi_out) *rssi_out = 0;
    if (ip_address_out) *ip_address_out = 0;
    if (internet_available_out) *internet_available_out = 0;

    // Check if WiFi is started and in STA mode
    wifi_mode_t mode;
    esp_err_t err = esp_wifi_get_mode(&mode);
    if (err != ESP_OK || (mode != WIFI_MODE_STA && mode != WIFI_MODE_APSTA)) {
        ESP_LOGD(TAG, "WiFi not in STA mode or not started");
        return ESP_OK; // Return OK but with default values (not connected)
    }

    // Get WiFi connection status
    wifi_ap_record_t ap_info;
    err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err == ESP_OK) {
        // WiFi is connected to an AP
        if (connected_out) *connected_out = 1;

        // Copy SSID (ensure null termination)
        if (ssid_out) {
            strncpy(ssid_out, (char*)ap_info.ssid, 31);
            ssid_out[31] = '\0';
        }

        // Get RSSI
        if (rssi_out) *rssi_out = ap_info.rssi;

        ESP_LOGD(TAG, "WiFi connected to: %s, RSSI: %d", ap_info.ssid, ap_info.rssi);
    } else {
        ESP_LOGD(TAG, "WiFi not connected to AP: %s", esp_err_to_name(err));
        return ESP_OK; // Return OK but with default values (not connected)
    }

    // Get IP address information
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip_info;
        err = esp_netif_get_ip_info(netif, &ip_info);
        if (err == ESP_OK && ip_info.ip.addr != 0) {
            if (ip_address_out) *ip_address_out = ip_info.ip.addr;

            // Basic internet connectivity check: if we have an IP and a gateway, assume internet is available
            // This is a simple heuristic - for more thorough checking, we'd need to actually ping a server
            if (internet_available_out) {
                *internet_available_out = (ip_info.gw.addr != 0) ? 1 : 0;
            }

            ESP_LOGD(TAG, "IP: " IPSTR ", Gateway: " IPSTR,
                     IP2STR(&ip_info.ip), IP2STR(&ip_info.gw));
        } else {
            ESP_LOGD(TAG, "Failed to get IP info: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGD(TAG, "Failed to get network interface handle");
    }

    return ESP_OK;
}

// Streaming functions are now implemented in ble.c with CCCD management



const char* get_SSID_auth_mode(uint8_t authmode)
{
    switch (authmode)
    {
        case WIFI_AUTH_OPEN:
            return "WIFI_AUTH_OPEN";
        case WIFI_AUTH_WEP:
            return "WIFI_AUTH_WEP";
        case WIFI_AUTH_WPA_PSK:
            return "WIFI_AUTH_WPA_PSK";
        case WIFI_AUTH_WPA2_PSK:
            return "WIFI_AUTH_WPA2_PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:
            return "WIFI_AUTH_WPA_WPA2_PSK";
        case WIFI_AUTH_ENTERPRISE:
            return "WIFI_AUTH_ENTERPRISE";
        case WIFI_AUTH_WPA3_PSK:
            return "WIFI_AUTH_WPA3_PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK:
            return "WIFI_AUTH_WPA2_WPA3_PSK";
        case WIFI_AUTH_WAPI_PSK:
            return "WIFI_AUTH_WAPI_PSK";
        case WIFI_AUTH_OWE:
            return "WIFI_AUTH_OWE";
        case WIFI_AUTH_WPA3_ENT_192:
            return "WIFI_AUTH_WPA3_ENT_192";
        case WIFI_AUTH_WPA3_EXT_PSK:
            return "WIFI_AUTH_WPA3_EXT_PSK";
        case WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE:
            return "WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE";
        case WIFI_AUTH_DPP:
            return "WIFI_AUTH_DPP";
        case WIFI_AUTH_WPA3_ENTERPRISE:
            return "WIFI_AUTH_WPA3_ENTERPRISE";
        case WIFI_AUTH_WPA2_WPA3_ENTERPRISE:
            return "WIFI_AUTH_WPA2_WPA3_ENTERPRISE";
        case WIFI_AUTH_MAX:
            return "WIFI_AUTH_MAX";
        default:
            return "UNKNOWN_AUTH_MODE";
    }
}


char* create_wifi_scan_json(ScanWifi *networks, int count) {
    // Root object
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "action", "scan_result");

    // Array of networks
    cJSON *networks_array = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "networks", networks_array);

    for (int i = 0; i < count; i++) {
        cJSON *net = cJSON_CreateObject();

        cJSON_AddStringToObject(net, "ssid", networks[i].ssid);
        cJSON_AddNumberToObject(net, "rssi", networks[i].rssi);
        cJSON_AddStringToObject(net, "auth", networks[i].protectionMode);
        cJSON_AddNumberToObject(net, "channel", networks[i].networkChannnel);
        
        cJSON_AddItemToArray(networks_array, net);
    }

    // Convert to JSON string
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);  // Free cJSON structure

    return json_str;  // You must free this with `free()` after use
}

esp_err_t scan_wifi_ssids()
{
    ESP_LOGW(TAG " WIFI SCAN", "Scanning Wifi Networks");
    
    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true
    };

    if(xTaskCreate(scanDuration_task, "scanDuration_task", 2048, NULL, 2, &xWifiScanDurationHandle) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create heater interface task");
    }

    scanStart=true;
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true)); // Blocking scan
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    
    ESP_LOGI(TAG " WIFI SCAN", "Found %d networks", ap_count);
    
    for (uint8_t i = 0; i < ap_count; ++i) {
        bool found = false;
        for (uint8_t j = 0; j < filteredNetworksCount; ++j) {
            if (!strcmp((char *)ap_info[i].ssid, filteredNetworks[j].ssid)) {
                found = true;
                if (ap_info[i].rssi > filteredNetworks[j].rssi) {
                    filteredNetworks[j].rssi = ap_info[i].rssi; // update if stronger
                }
                break;
            }
        }
        if (!found && strlen((char *)ap_info[i].ssid) > 0) {
            strncpy(filteredNetworks[filteredNetworksCount].ssid, (char *)ap_info[i].ssid, 32);
            filteredNetworks[filteredNetworksCount].ssid[32] = '\0';
            filteredNetworks[filteredNetworksCount].rssi = ap_info[i].rssi;
            uint8_t protectionType = ap_info[i].authmode;
            filteredNetworks[filteredNetworksCount].protectionMode= get_SSID_auth_mode(protectionType);
            filteredNetworks[filteredNetworksCount].networkChannnel= ap_info[i].primary;
            filteredNetworksCount++;
        }
    }
    
    ESP_LOGI(TAG " WIFI SCAN", "Filtered count: %d", filteredNetworksCount);
    wifiSSIDJSON = create_wifi_scan_json(filteredNetworks ,filteredNetworksCount);
    ESP_LOGI(TAG " WIFI SCAN", "SSID JSON: %s",wifiSSIDJSON);
    scanInProgress=false;
    scanfinsihed=true;
    
    if(wifiScanAction) // if scan triggered from BLE
    {
        if(send_ble_wifi_SSID_JSON_notification(wifiSSIDJSON) == ESP_OK)
        {   
            ESP_LOGI(TAG " WIFI SCAN", "Successfully Sent Network Scan Notification");
            wifiScanAction=false;
            return ESP_OK;
        }
        else{   
            ESP_LOGE(TAG " WIFI SCAN", "Failed to Send Network Scan Notification");
            wifiScanAction=false;
            return ESP_FAIL; 
        }
    }
    return ESP_OK;
}