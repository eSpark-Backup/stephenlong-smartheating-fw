
#include <string.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <esp_system.h>
#include "esp_mac.h"
#include "driver/gpio.h"

#include "defines.h"
#include "app.h"
#include "MLX90614_interface.h"
#include "Heater_interface.h"
#include "status_led.h"
#include "Comm_Interface.h"
#include "LD2410_interface.hpp"
#include "nvs.h"
#include "esp_timer.h"
#include "ThingsBoard_interface.h"

#define TAG     DEVICE_TAG

TaskHandle_t xButtonTaskHandle = NULL;

// Forward declarations for energy functions
static float calculate_current_power_consumption(void);
static uint8_t calculate_heating_efficiency(void);
static void energy_timer_callback(void *arg);
static void wifi_monitoring_task(void *pvParameters);

// Energy tracking global variables
static energy_tracking_t energy_data = {
    .energy_consumed_kwh = 0.0f,
    .heating_time_seconds = 0,
    .last_update_time = 0,
    .is_initialized = false
};
static esp_timer_handle_t energy_timer_handle;

ButtonFlags buttonflag={
        false, // BLEPairFlag
        false, // bypassModeFlag
        false, // factoryResetFlag
        false, // softAPFlag
        false  // factoryResetConfirmFlag
};
static int64_t press_start_time = 0;
static int64_t press_duration;
static bool button_currently_pressed = false;
static bool long_press_led_shown = false;

// Double-click detection variables
static int64_t last_press_time = 0;
static bool first_click_was_ble = false;
static bool waiting_for_double_click = false;

// Factory reset confirmation variables
static bool factory_reset_confirmation_mode = false;
static int64_t factory_reset_confirmation_start = 0;

// BLE pairing timeout variables
static int64_t ble_pairing_start_time = 0;
static int64_t soft_AP_start_time = 0;
static bool bleAdvertMode= false;
static bool softAPOngoing= false;

// LED feedback state tracking
static bool button_led_feedback_active = false;

// // Centralized LED Status Flags
// static led_status_flags_t led_flags = {
//     .error_critical = false,
//     .error_ir_sensor = false,
//     .error_heating = false,
//     .button_ble_feedback = false,
//     .button_factory_reset_feedback = false,
//     .button_bypass_feedback = false,
//     .button_softap_feedback = false,
//     .pairing_mode_active = false,
//     .firmware_update = false,
//     .factory_reset_confirmation = false,
//     .bypass_mode_active = false,
//     .normal_operation = false,
//     .wifi_connected = false,
//     .wifi_warning = false
// };
// static SemaphoreHandle_t led_flags_mutex = NULL;

// LED Manager Task Handle
// static TaskHandle_t led_manager_task_handle = NULL;

// Energy tracking NVS functions


static esp_err_t energy_data_load_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_ENERGYDATA, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(DEVICE_TAG, "Energy NVS namespace not found, initializing with defaults");
        energy_data.energy_consumed_kwh = 0.0f;
        energy_data.heating_time_seconds = 0;
        energy_data.last_update_time = esp_timer_get_time();
        energy_data.is_initialized = true;
        return ESP_OK;
    }

    size_t required_size = sizeof(float);
    err = nvs_get_blob(nvs_handle, NVS_KEY_ENERGY_CONSUMED, &energy_data.energy_consumed_kwh, &required_size);
    if (err != ESP_OK) {
        energy_data.energy_consumed_kwh = 0.0f;
    }

    required_size = sizeof(uint32_t);
    err = nvs_get_blob(nvs_handle, NVS_KEY_HEATING_TIME, &energy_data.heating_time_seconds, &required_size);
    if (err != ESP_OK) {
        energy_data.heating_time_seconds = 0;
    }

    required_size = sizeof(int64_t);
    err = nvs_get_blob(nvs_handle, NVS_KEY_LAST_UPDATE_TIME, &energy_data.last_update_time, &required_size);
    if (err != ESP_OK) {
        energy_data.last_update_time = esp_timer_get_time();
    }

    energy_data.is_initialized = true;
    nvs_close(nvs_handle);

    ESP_LOGI(DEVICE_TAG, "Energy data loaded: %.3f kWh, %lu seconds heating time",
             energy_data.energy_consumed_kwh, energy_data.heating_time_seconds);
    return ESP_OK;
}

static esp_err_t energy_data_save_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_ENERGYDATA, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(DEVICE_TAG, "Failed to open energy NVS for writing: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(nvs_handle, NVS_KEY_ENERGY_CONSUMED, &energy_data.energy_consumed_kwh, sizeof(float));
    if (err != ESP_OK) {
        ESP_LOGE(DEVICE_TAG, "Failed to save energy consumed: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_set_blob(nvs_handle, NVS_KEY_HEATING_TIME, &energy_data.heating_time_seconds, sizeof(uint32_t));
    if (err != ESP_OK) {
        ESP_LOGE(DEVICE_TAG, "Failed to save heating time: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_set_blob(nvs_handle, NVS_KEY_LAST_UPDATE_TIME, &energy_data.last_update_time, sizeof(int64_t));
    if (err != ESP_OK) {
        ESP_LOGE(DEVICE_TAG, "Failed to save last update time: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(DEVICE_TAG, "Failed to commit energy NVS: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    return err;
}

// Function to reset energy data (useful for testing/debugging)
__attribute__((unused)) static esp_err_t energy_data_reset(void)
{
    energy_data.energy_consumed_kwh = 0.0f;
    energy_data.heating_time_seconds = 0;
    energy_data.last_update_time = esp_timer_get_time();

    ESP_LOGI(DEVICE_TAG, "Energy data reset");
    return energy_data_save_to_nvs();
}

// Function to get current energy statistics (useful for debugging)
__attribute__((unused)) static void energy_data_print_stats(void)
{
    if (!energy_data.is_initialized) {
        ESP_LOGW(DEVICE_TAG, "Energy system not initialized");
        return;
    }

    float current_power = calculate_current_power_consumption();
    uint8_t efficiency = calculate_heating_efficiency();
    HEATER_STATE current_state = get_heater_state();

    ESP_LOGI(DEVICE_TAG, "=== Energy Statistics ===");
    ESP_LOGI(DEVICE_TAG, "Current Power: %.1f W", current_power);
    ESP_LOGI(DEVICE_TAG, "Heater State: %s", get_enum_name(ENUM_HEATER_STATE, current_state));
    ESP_LOGI(DEVICE_TAG, "Total Energy: %.3f kWh", energy_data.energy_consumed_kwh);
    ESP_LOGI(DEVICE_TAG, "Heating Time: %lu seconds (%.1f hours)",
             energy_data.heating_time_seconds,
             (float)energy_data.heating_time_seconds / 3600.0f);
    ESP_LOGI(DEVICE_TAG, "Efficiency: %d%%", efficiency);
    ESP_LOGI(DEVICE_TAG, "========================");
}

// Energy calculation functions
static float calculate_current_power_consumption(void)
{
    HEATER_STATE current_state = get_heater_state();
    float power = 0.0f;

    switch (current_state) {
        case HEATER_OFF:
            power = 0.0f;
            break;
        case HEATER_LOW:
            power = HEATING_LOW_WATTS;
            break;
        case HEATER_MID:
            power = HEATING_HIGH_WATTS;
            break;
        case HEATER_HIGH:
            power = HEATING_LOW_WATTS + HEATING_HIGH_WATTS;
            break;
        default:
            power = 0.0f;
            break;
    }

    return power;
}

static uint8_t calculate_heating_efficiency(void)
{
    // Calculate efficiency based on heating duty cycle over the last period
    // This is a simplified efficiency calculation
    float target_temp = get_target_temp();
    float current_temp = get_operative_temp();

    if (target_temp <= 0) {
        return 0; // No target set
    }

    float temp_diff = target_temp - current_temp;

    // Simple efficiency calculation: closer to target = higher efficiency
    if (temp_diff <= 0.5f && temp_diff >= -0.5f) {
        return 95; // Very close to target
    } else if (temp_diff <= 1.0f && temp_diff >= -1.0f) {
        return 85; // Close to target
    } else if (temp_diff <= 2.0f && temp_diff >= -2.0f) {
        return 70; // Moderately close
    } else {
        return 50; // Far from target
    }
}

static void energy_timer_callback(void *arg)
{
    if (!energy_data.is_initialized) {
        return;
    }

    int64_t current_time = esp_timer_get_time();
    int64_t time_diff_us = current_time - energy_data.last_update_time;
    float time_diff_hours = (float)time_diff_us / (1000000.0f * 3600.0f); // Convert to hours

    float current_power = calculate_current_power_consumption();

    // Update energy consumption (integrate power over time)
    energy_data.energy_consumed_kwh += (current_power * time_diff_hours) / 1000.0f; // Convert watts to kW

    // Update heating time if any heating is active
    if (current_power > 0) {
        energy_data.heating_time_seconds += (uint32_t)(time_diff_us / 1000000); // Convert to seconds
    }

    energy_data.last_update_time = current_time;

    // Save to NVS every 60 seconds to avoid excessive flash writes
    static int save_counter = 0;
    save_counter++;
    if (save_counter >= 60) {
        energy_data_save_to_nvs();
        save_counter = 0;
        ESP_LOGD(DEVICE_TAG, "Energy data saved: %.3f kWh, %lu seconds, %.1f W",
                 energy_data.energy_consumed_kwh, energy_data.heating_time_seconds, current_power);
    }
}

static void IRAM_ATTR button_isr_handler(void *arg)
{
    static int64_t last_isr_time = 0;
    int64_t now = esp_timer_get_time(); // microseconds

    // Debounce
    if ((now - last_isr_time) < BUTTON_DEBOUNCE_MS)
    {
        return;
    }
    last_isr_time = now;

    int button_level = gpio_get_level(BUTTON_GPIO);

    if (button_level == 1)
    {
        // Button pressed (rising edge) - pulldown means HIGH when pressed
        press_start_time = now;
        button_currently_pressed = true;
        long_press_led_shown = false;
        button_led_feedback_active = true;

        // Check if this might be a double-click
        if (waiting_for_double_click && ((now - last_press_time) < BUTTON_DOUBLE_CLICK_MAX_INTERVAL))
        {
            // This is a double-click!
            if (first_click_was_ble) {
                buttonflag.softAPFlag = true;
                waiting_for_double_click = false;
                first_click_was_ble = false;
                button_led_feedback_active = false;
                return;
            }
        }

        // Check if we're in factory reset confirmation mode
        if (factory_reset_confirmation_mode)
        {
            if ((now - factory_reset_confirmation_start) < FACTORY_RESET_CONFIRMATION_TIMEOUT)
            {
                // Second press within confirmation window - execute factory reset
                buttonflag.factoryResetConfirmFlag = true;
                factory_reset_confirmation_mode = false;
                button_led_feedback_active = false;
                return;
            }
            else
            {
                // Timeout expired, cancel factory reset
                factory_reset_confirmation_mode = false;
            }
        }

        // Reset double-click detection if too much time has passed
        if ((now - last_press_time) >= BUTTON_DOUBLE_CLICK_MAX_INTERVAL)
        {
            waiting_for_double_click = false;
            first_click_was_ble = false;
        }

        // Note: Cannot use ESP_LOGI in ISR - flag will be processed in button task
    }
    else
    {
        // Button released (falling edge) - pulldown means LOW when released
        button_currently_pressed = false;
        press_duration = now - press_start_time;
        button_led_feedback_active = false;

        // Clear all button feedback flags when button is released
        set_led_status_flag("button_ble_feedback", false);
        set_led_status_flag("button_factory_reset_feedback", false);
        set_led_status_flag("button_bypass_feedback", false);
        set_led_status_flag("button_softap_feedback", false);

        // Restore normal operation unless entering a persistent mode
        if (!buttonflag.BLEPairFlag && !buttonflag.factoryResetFlag && !buttonflag.bypassModeFlag) {
            set_led_status_flag("normal_operation", true);
        }

        // Determine action based on press duration
        if (press_duration >= BUTTON_BYPASS_MODE_MIN_DURATION)
        {
            // 30+ seconds: Bypass mode (technician only)
            buttonflag.bypassModeFlag = true;
        }
        else if (press_duration >= BUTTON_FACTORY_RESET_MIN_DURATION &&
                 press_duration < BUTTON_FACTORY_RESET_MAX_DURATION)
        {
            // 10-30 seconds: Factory reset confirmation mode
            buttonflag.factoryResetFlag = true;
            factory_reset_confirmation_mode = true;
            factory_reset_confirmation_start = now;
        }
        else if (press_duration >= BUTTON_BLE_PAIRING_MIN_DURATION &&
                 press_duration < BUTTON_BLE_PAIRING_MAX_DURATION)
        {
            // 5-10 seconds: BLE pairing mode
            buttonflag.BLEPairFlag = true;

            // Set up for potential double-click detection
            last_press_time = now;
            first_click_was_ble = true;
            waiting_for_double_click = true;
        }
        // 0-5 seconds: No action (prevents accidental triggers)
    }
}

bool is_button_led_feedback_active(void)
{
    return button_led_feedback_active;
}


void execute_command(void)
{
    // Reduced logging: only log command execution for important commands
    if (blecmd != READ_IR_TEMPERATURES && blecmd != READ_PRESENCE_DATA) {
        ESP_LOGD(DEVICE_TAG, "Executing command: %s", get_enum_name(ENUM_BLE_COMMANDS, blecmd));
    }
    switch (blecmd)
    {
    case POWER_CONTROL:
        ESP_LOGW(DEVICE_TAG, "Power control/reboot command...");
        send_ble_uart_notification(ESP_OK, NULL, 0);
        esp_restart();
        break;
    case FACTORY_RESET:
        ESP_LOGW(DEVICE_TAG, "Performing application reset...");
        send_ble_uart_notification(reset_device_and_erase_nvs(), NULL, 0);
        esp_restart();
        break;
    case READ_INTERNAL_TEMP:{
        ESP_LOGI(DEVICE_TAG, "Reading internal temperature...");
        float tempInternal = get_internal_temp();
        send_ble_uart_notification(ESP_OK, (uint8_t*)&tempInternal, sizeof(tempInternal));
        break;
    }
    case READ_IR_TEMPERATURES:{
        ESP_LOGI(DEVICE_TAG, "Reading IR temperatures...");
        // Enhanced format: return both ambient and object temperatures
        struct {
            float ambient;
            float object;
            float operative;
        } temp_data;

        temp_data.ambient = get_ambient_temp();
        temp_data.object = get_object_temp();
        temp_data.operative = get_operative_temp(); // Use adaptive calculation consistent with heating control

        send_ble_uart_notification(ESP_OK, (uint8_t*)&temp_data, sizeof(temp_data));
        break;
    }
    case SET_TARGET_TEMP:{
        float targetTemp = *(float*)bleParameters[0];
        
        // Calculate relative trigger values
        float autoModeTrigger = UI_MIN_TEMP_CELSIUS - 2.0f;    // min-2
        float manualOffTrigger = UI_MIN_TEMP_CELSIUS - 1.0f;   // min-1  
        float manualHighTrigger = UI_MAX_TEMP_CELSIUS + 1.0f;  // max+1
        
        // Check for special trigger values
        if (targetTemp == autoModeTrigger) {
            // Special value min-2: Set auto mode (restore trigger)
            ESP_LOGI(DEVICE_TAG, "Restore trigger received (%.1f)", targetTemp);
            set_device_mode(AUTO_MODE);
            send_ble_uart_notification(ESP_OK, NULL, 0);
        }
        else if (targetTemp == manualOffTrigger) {
            // Special value min-1: Set manual mode + heater off (off trigger)
            ESP_LOGI(DEVICE_TAG, "Off trigger received (%.1f)", targetTemp);
            set_device_mode(MANUAL_MODE);
            set_heater_state(HEATER_OFF);
            send_ble_uart_notification(ESP_OK, NULL, 0);
        }
        else if (targetTemp == manualHighTrigger) {
            // Special value max+1: Set manual mode + heater high (boost trigger)
            ESP_LOGI(DEVICE_TAG, "Boost trigger received (%.1f)", targetTemp);
            set_device_mode(MANUAL_MODE);
            set_heater_state(HEATER_HIGH);
            send_ble_uart_notification(ESP_OK, NULL, 0);
        }
        else if (targetTemp >= UI_MIN_TEMP_CELSIUS && targetTemp <= UI_MAX_TEMP_CELSIUS) {
            // Normal temperature range: Store temperature normally
            ESP_LOGI(DEVICE_TAG, "Setting target temperature to %.1f", targetTemp);
            set_heater_temp(targetTemp);
            send_ble_uart_notification(ESP_OK, NULL, 0);
        }
        else {
            // Invalid temperature range: Send error response
            ESP_LOGE(DEVICE_TAG, "Invalid temperature %.1f (valid range: %.0f-%.0f°C, triggers: %.0f/%.0f/%.0f)", 
                     targetTemp, UI_MIN_TEMP_CELSIUS, UI_MAX_TEMP_CELSIUS, 
                     autoModeTrigger, manualOffTrigger, manualHighTrigger);
            send_ble_uart_notification(ESP_FAIL, NULL, 0);
        }
        break;
    }
    case GET_TARGET_TEMP:{
        ESP_LOGI(DEVICE_TAG, "Getting target temperature...");
        float responseTemp;
        
        DEVICE_MODE currentMode = get_device_mode();
        if (currentMode == MANUAL_MODE) {
            // In manual mode, return appropriate trigger value based on heater state
            HEATER_STATE heaterState = get_heater_state();
            if (heaterState == HEATER_OFF) {
                // Manual off state - return off trigger
                responseTemp = UI_MIN_TEMP_CELSIUS - 1.0f;
                ESP_LOGI(DEVICE_TAG, "Manual mode OFF - returning off trigger (%.1f)", responseTemp);
            }
            else if (heaterState == HEATER_HIGH) {
                // Manual high state - return boost trigger  
                responseTemp = UI_MAX_TEMP_CELSIUS + 1.0f;
                ESP_LOGI(DEVICE_TAG, "Manual mode HIGH - returning boost trigger (%.1f)", responseTemp);
            }
            else {
                // Manual mode but not off/high state - return stored temp
                responseTemp = get_target_temp();
                ESP_LOGI(DEVICE_TAG, "Manual mode other state - returning stored temp (%.1f)", responseTemp);
            }
        }
        else {
            // Auto mode or other modes - return actual stored target temperature
            responseTemp = get_target_temp();
            ESP_LOGI(DEVICE_TAG, "Auto mode - returning stored temp (%.1f)", responseTemp);
        }
        
        send_ble_uart_notification(ESP_OK, (uint8_t*)&responseTemp, sizeof(responseTemp));
        break;
    }
    case SET_OPERATING_MODE:{
        ESP_LOGI(DEVICE_TAG, "Setting operating mode to %d", *(uint8_t*)bleParameters[0]);
        set_heater_mode((DEVICE_MODE)*(uint8_t*)bleParameters[0]);
        send_ble_uart_notification(ESP_OK, NULL, 0);
        break;
    }
    case GET_OPERATING_MODE:{
        ESP_LOGI(DEVICE_TAG, "Getting operating mode...");
        DEVICE_MODE mode = get_device_mode();
        send_ble_uart_notification(ESP_OK, (uint8_t*)&mode, sizeof(mode));
        break;
    }
    case READ_RELAY_STATE:{
        ESP_LOGI(DEVICE_TAG, "Reading relay state and power...");
        // Enhanced format: include power information
        struct {
            HEATER_STATE state;
            uint8_t low_relay;
            uint8_t high_relay;
            float power_consumption; // Placeholder for future power measurement
        } relay_data;

        relay_data.state = get_heater_state();
        relay_data.low_relay = gpio_get_level((gpio_num_t)HEATER_LOW_IO);
        relay_data.high_relay = gpio_get_level((gpio_num_t)HEATER_HIGH_IO);
        relay_data.power_consumption = 0.0f; // Placeholder

        send_ble_uart_notification(ESP_OK, (uint8_t*)&relay_data, sizeof(relay_data));
        break;
    }
    case SET_RELAY_STATE:{
        ESP_LOGI(DEVICE_TAG, "Setting relay state to %d", *(uint8_t*)bleParameters[0]);
        esp_err_t err = set_heater_state((HEATER_STATE)*(uint8_t*)bleParameters[0]);
        send_ble_uart_notification(err, NULL, 0);
        break;
    }
    case CONTROL_SENSOR_BOARD:{
        ESP_LOGI(DEVICE_TAG, "Controlling sensor board power: %d", *(uint8_t*)bleParameters[0]);
        uint8_t enable = *(uint8_t*)bleParameters[0];
        gpio_set_level((gpio_num_t)GND_SW, enable ? 1 : 0);
        send_ble_uart_notification(ESP_OK, NULL, 0);
        break;
    }
    case READ_PRESENCE_DATA:{
        ESP_LOGI(DEVICE_TAG, "Reading enhanced presence data...");
        // Enhanced format: include detailed LD2410 presence information
        struct {
            uint8_t presence_detected;      // LD2410 software presence detection
            uint8_t binary_sensor;          // GPIO hardware pin status
            uint16_t moving_distance;       // Moving target distance in cm
            uint16_t stationary_distance;   // Stationary target distance in cm
            uint8_t moving_energy;          // Moving target signal energy (0-100)
            uint8_t stationary_energy;      // Stationary target signal energy (0-100)
        } presence_data = {
            .presence_detected = 0,
            .binary_sensor = 0,
            .moving_distance = 0,
            .stationary_distance = 0,
            .moving_energy = 0,
            .stationary_energy = 0
        };

        // Get actual LD2410 sensor data
        presence_data.presence_detected = LD2410_getPresenceDetected() ? 1 : 0;
        presence_data.binary_sensor = gpio_get_level(MOTION_SENSOR_IO);  // Hardware pin status
        presence_data.moving_distance = LD2410_getMovingDistance();
        presence_data.stationary_distance = LD2410_getStationaryDistance();
        presence_data.moving_energy = LD2410_getMovingEnergy();
        presence_data.stationary_energy = LD2410_getStationaryEnergy();

        ESP_LOGI(DEVICE_TAG, "Presence: %d, Moving: %d cm (%d%%), Stationary: %d cm (%d%%)",
                 presence_data.presence_detected, presence_data.moving_distance, presence_data.moving_energy,
                 presence_data.stationary_distance, presence_data.stationary_energy);

        send_ble_uart_notification(ESP_OK, (uint8_t*)&presence_data, sizeof(presence_data));
        break;
    }
    case READ_WIFI_STATUS:{
        ESP_LOGI(DEVICE_TAG, "Reading Wi-Fi status...");
        // Wi-Fi status structure
        struct {
            uint8_t wifi_connected;
            uint8_t internet_available;
            int8_t rssi;
            char ssid[32];
            uint32_t ip_address;
        } wifi_status = {
            .wifi_connected = 0,
            .internet_available = 0,
            .rssi = 0,
            .ssid = {0},
            .ip_address = 0
        };

        // Get actual WiFi status using the new function
        esp_err_t status_err = get_wifi_status(
            wifi_status.ssid,
            &wifi_status.wifi_connected,
            &wifi_status.rssi,
            &wifi_status.ip_address,
            &wifi_status.internet_available
        );

        if (status_err != ESP_OK) {
            ESP_LOGW(DEVICE_TAG, "Failed to get WiFi status: %s", esp_err_to_name(status_err));
            strcpy(wifi_status.ssid, "Error");
        }

        ESP_LOGI(DEVICE_TAG, "WiFi Status - Connected: %d, SSID: %s, RSSI: %d, IP: " IPSTR ", Internet: %d",
                 wifi_status.wifi_connected, wifi_status.ssid, wifi_status.rssi,
                 IP2STR((esp_ip4_addr_t*)&wifi_status.ip_address), wifi_status.internet_available);

        send_ble_uart_notification(ESP_OK, (uint8_t*)&wifi_status, sizeof(wifi_status));
        break;
    }
    case READ_SYSTEM_DIAGNOSTICS:{
        ESP_LOGI(DEVICE_TAG, "Reading system diagnostics...");
        // Placeholder for system diagnostics
        struct {
            uint32_t uptime_seconds;
            uint32_t free_heap;
            uint32_t min_free_heap;
            uint8_t cpu_usage;
            uint8_t i2c_errors;
            uint8_t uart_errors;
        } diagnostics = {
            .uptime_seconds = 0,
            .free_heap = 0,
            .min_free_heap = 0,
            .cpu_usage = 0,
            .i2c_errors = 0,
            .uart_errors = 0
        };

        diagnostics.uptime_seconds = esp_timer_get_time() / 1000000;
        diagnostics.free_heap = esp_get_free_heap_size();
        diagnostics.min_free_heap = esp_get_minimum_free_heap_size();
        diagnostics.cpu_usage = 0; // Placeholder

        send_ble_uart_notification(ESP_OK, (uint8_t*)&diagnostics, sizeof(diagnostics));
        break;
    }
    case READ_ENERGY_METRICS:{
        ESP_LOGI(DEVICE_TAG, "Reading energy metrics...");

        // Return actual energy metrics from the tracking system
        struct {
            float power_consumption_w;
            float energy_consumed_kwh;
            uint32_t heating_time_seconds;
            uint8_t efficiency_percent;
        } ble_energy_data = {
            .power_consumption_w = calculate_current_power_consumption(),
            .energy_consumed_kwh = energy_data.energy_consumed_kwh,
            .heating_time_seconds = energy_data.heating_time_seconds,
            .efficiency_percent = calculate_heating_efficiency()
        };

        ESP_LOGI(DEVICE_TAG, "Energy metrics: %.1fW, %.3fkWh, %lus, %d%%",
                 ble_energy_data.power_consumption_w,
                 ble_energy_data.energy_consumed_kwh,
                 ble_energy_data.heating_time_seconds,
                 ble_energy_data.efficiency_percent);

        send_ble_uart_notification(ESP_OK, (uint8_t*)&ble_energy_data, sizeof(ble_energy_data));
        break;
    }
    case CONTROL_FRONT_LED:{
        ESP_LOGI(DEVICE_TAG, "Controlling front LED: %d", *(uint8_t*)bleParameters[0]);
        uint8_t led_mode = *(uint8_t*)bleParameters[0];

        // Implement LED control based on led_mode
        // Don't override LED if button feedback is active
        if (!button_led_feedback_active) {
            switch (led_mode) {
                case 0: // Turn OFF
                    ESP_LOGI(DEVICE_TAG, "LED: OFF");
                    set_led_effect(LED_EFFECT_NONE, NONE, NO_SPEED);
                    break;

                case 1: // Green steady (normal operation)
                    ESP_LOGI(DEVICE_TAG, "LED: Green steady");
                    set_led_effect(LED_EFFECT_STEADY, GREEN, NO_SPEED);
                    break;

            case 2: // Red steady (error state)
                ESP_LOGI(DEVICE_TAG, "LED: Red steady");
                set_led_effect(LED_EFFECT_STEADY, RED, NO_SPEED);
                break;

            case 3: // Yellow steady (warning state)
                ESP_LOGI(DEVICE_TAG, "LED: Yellow steady");
                set_led_effect(LED_EFFECT_STEADY, YELLOW, NO_SPEED);
                break;

            case 4: // Blue steady (info/pairing)
                ESP_LOGI(DEVICE_TAG, "LED: Blue steady");
                set_led_effect(LED_EFFECT_STEADY, BLUE, NO_SPEED);
                break;

            case 5: // Red fast blink (critical error)
                ESP_LOGI(DEVICE_TAG, "LED: Red fast blink");
                set_led_effect(LED_EFFECT_BLINK, RED, LED_SPEED_FAST);
                break;

            case 6: // Yellow slow blink (warning)
                ESP_LOGI(DEVICE_TAG, "LED: Yellow slow blink");
                set_led_effect(LED_EFFECT_BLINK, YELLOW, LED_SPEED_SLOW);
                break;

            case 7: // Blue breathing (BLE pairing mode)
                ESP_LOGI(DEVICE_TAG, "LED: Blue breathing");
                set_led_effect(LED_EFFECT_BREATHING, BLUE, LED_SPEED_MEDIUM);
                break;

            case 8: // Green triple flash (success indication)
                ESP_LOGI(DEVICE_TAG, "LED: Green triple flash");
                set_led_effect(LED_EFFECT_TRIPLE_FLASH, GREEN, LED_SPEED_FAST);
                break;

                case 255: // Return to automatic status control
                    ESP_LOGI(DEVICE_TAG, "LED: Return to automatic status control");
                    set_led_effect(LED_EFFECT_NONE, NONE, NO_SPEED);
                    break;

                default: // Invalid mode
                    ESP_LOGW(DEVICE_TAG, "Invalid LED mode: %d", led_mode);
                    send_ble_uart_notification(ESP_ERR_INVALID_ARG, NULL, 0);
                    return;
            }
        } else {
            ESP_LOGI(DEVICE_TAG, "LED control blocked - button feedback active");
        }

        // Acknowledge successful command
        send_ble_uart_notification(ESP_OK, NULL, 0);
        break;
    }
    case READ_DEVICE_INFO:{
        ESP_LOGI(DEVICE_TAG, "Reading device information...");
        struct {
            char firmware_version[16];
            char hardware_version[16];
            uint8_t mac_address[6];
            uint32_t serial_number;
            char device_name[32];
        } device_info = {
            .firmware_version = {0},
            .hardware_version = {0},
            .mac_address = {0},
            .serial_number = {0},
            .device_name = {0}
        };

        strcpy(device_info.firmware_version, FIRMWARE_VERSION);
        strcpy(device_info.hardware_version, HARDWARE_VERSION);
        strcpy(device_info.device_name, BLE_DEVICE_NAME);

        // Get MAC address
        esp_read_mac(device_info.mac_address, ESP_MAC_WIFI_STA);
        device_info.serial_number = 0x000001; // Placeholder

        send_ble_uart_notification(ESP_OK, (uint8_t*)&device_info, sizeof(device_info));
        break;
    }

    default:
        ESP_LOGE(DEVICE_TAG, "Invalid command received: %X", blecmd);
        send_ble_uart_notification(ESP_FAIL, NULL, 0);
        break;
    }
    free(bleParameters[0]);
    free(bleParameters[1]);
    bleParameters[0] = NULL;
    bleParameters[1] = NULL;
    blecmd = NO_COMMAND;
    uart_cmd_handler_flag = false;
}

void button_task(void * pvParameters)
{
    while(1)
    {
        // Handle immediate LED feedback during button press
        if (button_currently_pressed && button_led_feedback_active)
        {
            int64_t current_press_duration = esp_timer_get_time() - press_start_time;
            // int64_t duration_seconds = current_press_duration / 1000000;

            if ((current_press_duration >= BUTTON_BYPASS_MODE_MIN_DURATION) && !long_press_led_shown)
            {
                // 30+ seconds: Yellow breathing for bypass mode (technician only)
                ESP_LOGI(GPIO_TAG, "Button: Bypass mode ready (30s+) - technician only");
                set_led_status_flag("normal_operation", false);  // Clear normal operation during feedback
                set_led_status_flag("button_bypass_feedback", true);
                long_press_led_shown = true;
            }
            else if ((current_press_duration >= BUTTON_FACTORY_RESET_MIN_DURATION) && (current_press_duration < BUTTON_BYPASS_MODE_MIN_DURATION) && !long_press_led_shown)
            {
                // 10-30 seconds: Purple breathing for factory reset
                ESP_LOGI(GPIO_TAG, "Button: Factory reset confirmation ready (10-30s)");
                set_led_status_flag("normal_operation", false);  // Clear normal operation during feedback
                set_led_status_flag("button_factory_reset_feedback", true);
                long_press_led_shown = true;
            }
            else if ((current_press_duration >= BUTTON_BLE_PAIRING_MIN_DURATION) && (current_press_duration < BUTTON_FACTORY_RESET_MIN_DURATION) && !long_press_led_shown)
            {
                // 5-10 seconds: Blue breathing for BLE pairing
                ESP_LOGI(GPIO_TAG, "Button: BLE pairing ready (5-10s) - setting blue breathing LED");
                set_led_status_flag("normal_operation", false);  // Clear normal operation during feedback
                set_led_status_flag("button_ble_feedback", true);
                ESP_LOGI(GPIO_TAG, "Button: LED flag set to blue breathing");
                long_press_led_shown = true;
            }
            // 0-5 seconds: No LED feedback (prevents accidental triggers)
        }

  
        // Handle factory reset confirmation
        if(buttonflag.factoryResetConfirmFlag)
        {
            buttonflag.factoryResetConfirmFlag = false;
            ESP_LOGI(GPIO_TAG, "Button: Factory reset confirmed - executing");
            set_led_effect(LED_EFFECT_BLINK, PURPLE, LED_SPEED_FAST);
            if(reset_device_and_erase_nvs() != ESP_OK)
            {
                ESP_LOGE(GPIO_TAG, "Factory Reset Failed");
                set_led_effect(LED_EFFECT_BLINK, RED, LED_SPEED_SLOW);
                vTaskDelay(pdMS_TO_TICKS(3000));
            }
            esp_restart();
        }
    
        // Handle factory reset confirmation timeout entry
        if(buttonflag.factoryResetFlag)
        {
            buttonflag.factoryResetFlag = false;
            ESP_LOGI(GPIO_TAG, "Button: Factory reset confirmation mode (8s+) - press again within 5s to confirm");
            set_led_effect(LED_EFFECT_BREATHING, PURPLE, LED_SPEED_SLOW);
            // ISR handler will manage the confirmation timeout
        }
    
        // Handle SoftAP pairing (double-click from BLE mode)
        if(buttonflag.softAPFlag && !buttonflag.BLEPairFlag)
        {
            buttonflag.softAPFlag = false;
            ESP_LOGI(GPIO_TAG, "Button: SoftAP pairing (double-click from BLE mode)");
            set_led_status_flag("button_softap_feedback", true);
            start_softap_provisioning();
            soft_AP_start_time = esp_timer_get_time();
            softAPOngoing=true;
        }
    
        // Handle BLE pairing
        if(buttonflag.BLEPairFlag && !buttonflag.softAPFlag)
        {
            buttonflag.BLEPairFlag = false;
        
            // Check if we're in bypass mode - if so, this is first click of potential double-click
            DEVICE_MODE current_mode = get_device_mode();
            if (current_mode == BYPASS_MODE)
            {
                if (waiting_for_double_click && first_click_was_ble)
                {
                    // This is the second press - exit bypass mode
                    ESP_LOGI(GPIO_TAG, "Button: Exiting bypass mode (double-click detected)");
                    set_heater_mode(AUTO_MODE); // Return to automatic mode
                    set_led_effect(LED_EFFECT_STEADY, GREEN, NO_SPEED);
                    waiting_for_double_click = false;
                    first_click_was_ble = false;
                }
                else
                {
                    // This is the first press in bypass mode - just log, wait for potential second press
                    ESP_LOGI(GPIO_TAG, "Button: First press in bypass mode - press again quickly to exit");
                    // The double-click detection is already set up in ISR
                }
            }
            else
            {
                // Normal BLE pairing mode when not in bypass
                ESP_LOGI(GPIO_TAG, "Button: BLE pairing mode (5-10s) - disabling WiFi");
            
                // Disconnect WiFi before entering BLE pairing mode
                esp_wifi_disconnect();
                esp_wifi_stop();
            
                // Start BLE pairing timeout time
                set_led_status_flag("pairing_mode_active", true);
                remove_bond_devices();
                if(startBLEAdvert())
                {
                    ble_pairing_start_time = esp_timer_get_time();
                    bleAdvertMode=true;
                }
            }
        }
        // Handle bypass mode entry
        if(buttonflag.bypassModeFlag)
        {
            buttonflag.bypassModeFlag = false;
            ESP_LOGI(GPIO_TAG, "Button: Entering bypass mode (30s+) - technician only - full power heating enabled");
            set_heater_mode(BYPASS_MODE);
            set_led_effect(LED_EFFECT_STEADY, YELLOW, NO_SPEED);
        }
    
        // Handle factory reset confirmation timeout
        if (factory_reset_confirmation_mode)
        {
            int64_t elapsed = esp_timer_get_time() - factory_reset_confirmation_start;
            if (elapsed >= FACTORY_RESET_CONFIRMATION_TIMEOUT)
            {
                ESP_LOGI(GPIO_TAG, "Factory reset confirmation timeout - cancelling");
                factory_reset_confirmation_mode = false;
                set_led_effect(LED_EFFECT_STEADY, GREEN, NO_SPEED); // Return to normal
            }
        
        }

        if(bleAdvertMode || softAPOngoing)   //BLE pair mode Timeout check
        {
            if((esp_timer_get_time() - ble_pairing_start_time) >= BLE_PAIRING_TIMEOUT && bleAdvertMode) //60 sec timer
            {
                ESP_LOGW(GPIO_TAG, "BLE pair mode Timeout!");
                if(esp_ble_gap_stop_advertising() != ESP_OK)
                {
                    ESP_LOGE(GPIO_TAG, "Failed to Exit BLE pair mode");
                } else{  ESP_LOGW(GPIO_TAG, "Exited BLE pair mode Successfully"); }
                esp_wifi_start();
                esp_wifi_connect();
                set_led_status_flag("pairing_mode_active", false);
                set_heater_mode(get_device_mode());
                bleAdvertMode=false;
            }
            else if((esp_timer_get_time() - soft_AP_start_time) >= SOFT_AP_TIMEOUT && softAPOngoing)
            {
                ESP_LOGW(GPIO_TAG, "Soft AP mode Timeout!");
                stop_softap_provisioning();
                set_led_status_flag("button_softap_feedback", false);
                set_heater_mode(get_device_mode());
                softAPOngoing=false;
            }
        }


        vTaskDelay(pdMS_TO_TICKS(100)); // Check every 100ms for responsive LED feedback
    }
    vTaskDelete(xButtonTaskHandle);
}


void button_init(void)
{
    // // Configure GND_SW (GPIO10) as output for sensor board power control
    // gpio_config_t gnd_sw_cfg = {
    //     .pin_bit_mask = GPIO_PIN_MASK(GND_SW),
    //     .mode = GPIO_MODE_OUTPUT,
    //     .pull_up_en = GPIO_PULLUP_DISABLE,
    //     .pull_down_en = GPIO_PULLDOWN_DISABLE,
    //     .intr_type = GPIO_INTR_DISABLE
    // };
    // gpio_config(&gnd_sw_cfg);

    // // Turn on sensor board power (MOSFET control)
    // gpio_set_level(GND_SW , true);
    // ESP_LOGI(GPIO_TAG, "Sensor board power enabled (GPIO10=HIGH)");

    // // Allow time for sensor board to power up and stabilize
    // vTaskDelay(pdMS_TO_TICKS(100));  // 100ms delay for sensor stabilization

    // Configure button GPIO - match ESPHome config (pulldown enabled)
    gpio_config_t btn_cfg = {
        .pin_bit_mask = GPIO_PIN_MASK(BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,  // Match ESPHome pulldown: true
        .intr_type = GPIO_INTR_ANYEDGE
    };

    esp_err_t ret = gpio_config(&btn_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG GPIO_TAG, "Failed to configure button GPIO: %s", esp_err_to_name(ret));
        return;
    }

    // Read initial button state
    int initial_level = gpio_get_level(BUTTON_GPIO);
    ESP_LOGI(TAG GPIO_TAG, "Button GPIO%d configured, initial level: %d", BUTTON_GPIO, initial_level);

    // Install GPIO ISR service
    ret = gpio_install_isr_service(0);  // Use ESP_INTR_FLAG_DEFAULT or 0
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {  // ESP_ERR_INVALID_STATE means already installed
        ESP_LOGE(TAG GPIO_TAG, "Failed to install ISR service: %s", esp_err_to_name(ret));
        return;
    }

    ret = gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, (void*)(uintptr_t)BUTTON_GPIO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG GPIO_TAG, "Failed to add ISR handler: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG GPIO_TAG, "Button ISR successfully set up on GPIO%d", BUTTON_GPIO);

    if(xTaskCreate(button_task, "Button_Task", 8192, NULL, 1, &xButtonTaskHandle) != pdPASS)
    {
        ESP_LOGE(TAG GPIO_TAG, "Failed to create Button Task");
        return;
    }

    ESP_LOGI(TAG GPIO_TAG, "Button task created successfully");
}

const char* get_enum_name(ENUM_TYPE type, int value)
{
    switch (type) {
        case ENUM_DEVICE_STATUS:
            switch ((DEVICE_STATUS)value) {
                case OFF_STATE:         return "OFF_STATE";
                case NORMAL_OP:         return "NORMAL_OP";
                case PAIRING:           return "PAIRING";
                case ERROR:             return "ERROR";
                case FIRMWARE_UPDATE:   return "FIRMWARE_UPDATE";
                default:                return "UNKNOWN_DEVICE_STATUS";
            }

        case ENUM_HEATER_STATE:
            switch ((HEATER_STATE)value) {
                case HEATER_OFF: return "HEATER_OFF";
                case HEATER_LOW: return "HEATER_LOW";
                case HEATER_MID: return "HEATER_MID";
                case HEATER_HIGH: return "HEATER_HIGH";
                default: return "UNKNOWN_HEATER_STATE";
            }

        case ENUM_DEVICE_MODE:
            switch ((DEVICE_MODE)value) {
                case MANUAL_MODE: return "MANUAL_MODE";
                case AUTO_MODE:   return "AUTO_MODE";
                case BYPASS_MODE: return "BYPASS_MODE";
                case DEVICE_MODE_COUNT: return "DEVICE_MODE_COUNT";
                default: return "UNKNOWN_DEVICE_MODE";
            }

        case ENUM_BLE_COMMANDS:
            switch ((BLE_COMMANDS)value) {
                case NO_COMMAND:               return "NO_COMMAND";
                case POWER_CONTROL:            return "POWER_CONTROL";
                case FACTORY_RESET:            return "FACTORY_RESET";
                case READ_INTERNAL_TEMP:       return "READ_INTERNAL_TEMP";
                case READ_IR_TEMPERATURES:     return "READ_IR_TEMPERATURES";
                case SET_TARGET_TEMP:          return "SET_TARGET_TEMP";
                case GET_TARGET_TEMP:          return "GET_TARGET_TEMP";
                case SET_OPERATING_MODE:       return "SET_OPERATING_MODE";
                case GET_OPERATING_MODE:       return "GET_OPERATING_MODE";
                case READ_RELAY_STATE:         return "READ_RELAY_STATE";
                case SET_RELAY_STATE:          return "SET_RELAY_STATE";
                case CONTROL_SENSOR_BOARD:     return "CONTROL_SENSOR_BOARD";
                case READ_PRESENCE_DATA:       return "READ_PRESENCE_DATA";
                case READ_WIFI_STATUS:         return "READ_WIFI_STATUS";
                case READ_SYSTEM_DIAGNOSTICS:  return "READ_SYSTEM_DIAGNOSTICS";
                case READ_ENERGY_METRICS:      return "READ_ENERGY_METRICS";
                case CONTROL_FRONT_LED:        return "CONTROL_FRONT_LED";
                case READ_DEVICE_INFO:         return "READ_DEVICE_INFO";
                case MAX_BLE_CMDS:             return "MAX_BLE_CMDS";
                default: return "UNKNOWN_BLE_COMMAND";
            }

        default:
            return "UNKNOWN_ENUM_TYPE";
    }
}

// WiFi monitoring task - sends periodic WiFi status notifications
static void wifi_monitoring_task(void *pvParameters)
{
    ESP_LOGI(DEVICE_TAG, "WiFi monitoring task started - will send status with adaptive intervals");

    static int64_t last_wifi_notification_time = 0;
    static int64_t last_activity_time = 0;

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Check every second

        int64_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
        
        // Determine WiFi status streaming interval based on presence and activity (matches BLE pattern)
        bool presence_detected = get_presence_detected();
        static int wifi_interval = BLE_STREAMING_NORMAL_TIME;
        
        // Check if any client connected within the first minute - if so, use fast interval
        if (has_recently_connected_ble_clients()) {
            wifi_interval = BLE_FIRST_MINUTE_TIME;
        } else if (presence_detected) {
            last_activity_time = current_time;
            wifi_interval = BLE_STREAMING_PRESENCE_TIME;
        } else if (current_time - last_activity_time < NO_ACTIVITY_TIME_MS) {
            wifi_interval = BLE_STREAMING_NORMAL_TIME;
        } else {
            wifi_interval = BLE_STREAMING_IDLE_TIME;
        }
        
        int64_t wifi_interval_ms = wifi_interval * 1000;

        if (current_time - last_wifi_notification_time >= wifi_interval_ms) {
            // Get current WiFi status
            char ssid[33] = {0};
            uint8_t connected = 0;
            int8_t rssi = 0;
            uint32_t ip_address = 0;
            uint8_t internet_available = 0;

            esp_err_t status_err = get_wifi_status(ssid, &connected, &rssi, &ip_address, &internet_available);

            if (status_err == ESP_OK) {
                // Create JSON status message
                char json_status[256];  // Increased buffer size to prevent truncation
                if (connected) {
                    // Convert IP address to string - ESP-IDF returns in network byte order
                    // Use esp_ip4_addr_t structure to handle byte order correctly
                    esp_ip4_addr_t ip_addr = { .addr = ip_address };

                    snprintf(json_status, sizeof(json_status),
                        "{\"status\":\"connected\",\"ssid\":\"%s\",\"rssi\":%d,\"ip\":\"" IPSTR "\",\"internet\":%s}",
                        ssid, rssi, IP2STR(&ip_addr),
                        internet_available ? "true" : "false");
                } else {
                    snprintf(json_status, sizeof(json_status),
                        "{\"status\":\"disconnected\",\"ssid\":\"\",\"rssi\":0,\"ip\":\"\",\"internet\":false}");
                }

                // Send WiFi status notification
                send_wifi_status_notification(json_status);
                last_wifi_notification_time = current_time;

                ESP_LOGD(DEVICE_TAG, "WiFi status notification sent (interval: %ds, presence: %s, recent_connection: %s): %s", 
                        wifi_interval, presence_detected ? "yes" : "no",
                        has_recently_connected_ble_clients() ? "yes" : "no", json_status);
            } else {
                // Send error status
                send_wifi_status_notification("{\"status\":\"error\",\"error\":\"Failed to read WiFi status\"}");
                last_wifi_notification_time = current_time;
                ESP_LOGW(DEVICE_TAG, "Failed to get WiFi status: %s", esp_err_to_name(status_err));
            }
        }
    }

    vTaskDelete(NULL);
}

extern "C" void app_main(void)
{
    // Configure log levels to reduce verbose output
    esp_log_level_set("wifi", ESP_LOG_INFO);        // Hide debug WiFi logs like "mms: 0->0"
    esp_log_level_set("HTTP_CLIENT", ESP_LOG_WARN); // Reduce HTTP client verbosity
    esp_log_level_set("esp-tls", ESP_LOG_WARN);     // Reduce TLS verbosity
    esp_log_level_set("event", ESP_LOG_WARN);       // Hide "no handlers registered" messages

    status_led_init();
    device_data_init();
    button_init();

    // Initialize sensors after power is applied to sensor board
    sensor_board_init();
    // LD2410_init();
    ir_sensor_init();
    // heater_interface_init();
    
    // // Initialize energy tracking system
    // ESP_LOGI(DEVICE_TAG, "Initializing energy tracking system...");
    // energy_data_load_from_nvs();
    
    // // Create energy tracking timer
    // esp_timer_create_args_t energy_timer_args = {
    //     .callback = energy_timer_callback,
    //     .arg = NULL,
    //     .dispatch_method = ESP_TIMER_TASK,
    //     .name = "energy_timer",
    //     .skip_unhandled_events = false
    // };
    
    // esp_err_t timer_err = esp_timer_create(&energy_timer_args, &energy_timer_handle);
    // if (timer_err == ESP_OK) {
    //     esp_timer_start_periodic(energy_timer_handle, ENERGY_UPDATE_INTERVAL);
    //     ESP_LOGI(DEVICE_TAG, "Energy tracking timer started successfully");
    // } else {
    //     ESP_LOGE(DEVICE_TAG, "Failed to create energy tracking timer: %s", esp_err_to_name(timer_err));
    // }
    
    // // set_heater_temp(27.5);
    // // set_heater_mode(AUTO_MODE);
    
    // comm_init();
   
    
    // // Create WiFi monitoring task for BLE auto-streaming
    // if(xTaskCreate(wifi_monitoring_task, "WiFi_Monitor_Task", 4096, NULL, 1, NULL) != pdPASS)
    // {
    //     ESP_LOGE(DEVICE_TAG, "Failed to create WiFi monitoring task");
    // }
    // else
    // {
    //     ESP_LOGI(DEVICE_TAG, "WiFi monitoring task created successfully");
    // }
    
    // if (strlen(device_data.ssid) != 0 || strlen(device_data.password) != 0)
    // {
    //     ESP_LOGI(DEVICE_TAG, "Connecting to saved WiFi: %s\tPassword: %s", device_data.ssid, device_data.password);
    //     wifi_init_sta(device_data.ssid, device_data.password);
    //     vTaskDelay(pdMS_TO_TICKS(500)); 
    //     thingsBoard_init();
    // }
    // else
    // {
    //     ESP_LOGI(DEVICE_TAG, "Starting SoftAP for provisioning");
    //     start_softap_provisioning();
    //     ESP_LOGI(DEVICE_TAG, "Starting BLE for provisioning");
    // }
}
