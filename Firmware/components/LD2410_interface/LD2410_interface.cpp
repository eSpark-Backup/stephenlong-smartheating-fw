#include "LD2410_interface.hpp"
#include <time.h>
#include "app.h"
#include "ble.h"

// Forward declaration
void groundSwitchCheck();

MyLD2410 sensor;
unsigned long nextPrint = 0, printEvery = 1000;  // print every second (for debug output)
unsigned long nextBLENotification = 0;  // separate timing for BLE notifications


void printValue(const uint8_t &val) {
    printf(" ' ' %d" , val);
}

void printData(){
    // Enhanced logging - immediate log when presence changes, periodic otherwise
    static bool last_presence_state = false;
    static uint32_t log_counter = 0;
    static unsigned long last_detection_time = 0;
    bool current_presence = sensor.presenceDetected();

    log_counter++;

    // Always log immediately when presence state changes
    bool presence_changed = (current_presence != last_presence_state);

    if (presence_changed || log_counter >= 30) {
        if (presence_changed) {
            unsigned long now = esp_timer_get_time() / 1000; // Convert to milliseconds
            if (current_presence) {
                ESP_LOGW(TAG, "PRESENCE DETECTED! Distance: %ld cm", sensor.detectedDistance());
                last_detection_time = now;
            } else {
                unsigned long detection_duration = now - last_detection_time;
                ESP_LOGW(TAG, "PRESENCE LOST (was detected for %lu ms)", detection_duration);
            }
        } else {
            ESP_LOGI(TAG, "Presence: %s", current_presence ? "DETECTED" : "None");
        }

        // Log detailed information when present or when state changes
        if (current_presence || presence_changed) {
            if (sensor.movingTargetDetected()) {
                ESP_LOGI(TAG, "Moving: %ld cm (energy: %d)",
                         sensor.movingTargetDistance(), sensor.movingTargetSignal());
            }
            if (sensor.stationaryTargetDetected()) {
                ESP_LOGI(TAG, "Stationary: %ld cm (energy: %d)",
                         sensor.stationaryTargetDistance(), sensor.stationaryTargetSignal());
            }
        }

        last_presence_state = current_presence;
        if (log_counter >= 30) log_counter = 0;
    }
}

void printDetailedMetrics() {
    // Log all available metrics similar to ESPHome sensors
    ESP_LOGI(TAG, "=== LD2410 Detailed Metrics ===");

    // Binary sensor equivalents (like ESPHome binary_sensor)
    ESP_LOGI(TAG, "Has Target: %s", sensor.presenceDetected() ? "TRUE" : "FALSE");
    ESP_LOGI(TAG, "Has Moving Target: %s", sensor.movingTargetDetected() ? "TRUE" : "FALSE");
    ESP_LOGI(TAG, "Has Still Target: %s", sensor.stationaryTargetDetected() ? "TRUE" : "FALSE");
    ESP_LOGI(TAG, "OUT Pin Status: %s", gpio_get_level(MOTION_SENSOR_IO) ? "HIGH" : "LOW");

    // Sensor data equivalents (like ESPHome sensor)
    ESP_LOGI(TAG, "Overall Status: %s (%d)", sensor.statusString(), sensor.getStatus());
    ESP_LOGI(TAG, "Detection Distance: %ld cm", sensor.detectedDistance());
    ESP_LOGI(TAG, "Moving Distance: %ld cm", sensor.movingTargetDistance());
    ESP_LOGI(TAG, "Moving Energy: %d", sensor.movingTargetSignal());
    ESP_LOGI(TAG, "Still Distance: %ld cm", sensor.stationaryTargetDistance());
    ESP_LOGI(TAG, "Still Energy: %d", sensor.stationaryTargetSignal());

    // Device info
    ESP_LOGI(TAG, "Enhanced Mode: %s", sensor.inEnhancedMode() ? "Yes" : "No");
    ESP_LOGI(TAG, "Firmware: %s", sensor.getFirmware().c_str());
    ESP_LOGI(TAG, "===============================");
}

void LD2410Task(void *pvParameters){
    uint32_t detailedMetricsCounter = 0;

    while(1){
        // Ensure sensor board power remains on
        groundSwitchCheck();

        if (sensor.check() == MyLD2410::Response::DATA) {
            unsigned long currentTime = esp_timer_get_time() / 1000; // milliseconds

            // Handle debug output (keep original 1-second timing)
            if (currentTime > nextPrint) {
                nextPrint = currentTime + printEvery;
                printData();

                // Print detailed metrics every 60 seconds (reduced frequency)
                detailedMetricsCounter++;
                if (detailedMetricsCounter >= 60) {
                    printDetailedMetrics();
                    detailedMetricsCounter = 0;
                }
            }

            // Handle BLE notifications with adaptive timing
            bool presence_detected = sensor.presenceDetected();
            
            // Determine BLE streaming interval based on presence and activity (matches telemetry pattern)
            static unsigned long last_activity_time = 0;
            static int ble_interval = BLE_STREAMING_NORMAL_TIME;
            
            // Check if any client connected within the first minute - if so, use fast interval
            if (has_recently_connected_ble_clients()) {
                ble_interval = BLE_FIRST_MINUTE_TIME;
            } else if (presence_detected) {
                last_activity_time = currentTime;
                ble_interval = BLE_STREAMING_PRESENCE_TIME;
            } else if (currentTime - last_activity_time < NO_ACTIVITY_TIME_MS) {
                ble_interval = BLE_STREAMING_NORMAL_TIME;
            } else {
                ble_interval = BLE_STREAMING_IDLE_TIME;
            }
            
            unsigned long ble_interval_ms = ble_interval * 1000;

            if (currentTime >= nextBLENotification) {
                // Only send BLE notifications if there are active BLE clients connected
                if (has_active_ble_clients()) {
                    // Send BLE presence streaming notification with actual LD2410 sensor data
                    uint8_t has_target = presence_detected ? 1 : 0;
                    float move_dist = (float)sensor.movingTargetDistance();    // Convert unsigned long to float
                    float still_dist = (float)sensor.stationaryTargetDistance(); // Convert unsigned long to float
                    uint8_t move_energy = sensor.movingTargetSignal();         // Already uint8_t (byte)
                    uint8_t still_energy = sensor.stationaryTargetSignal();    // Already uint8_t (byte)

                    // Send streaming notification with current timestamp
                    send_ble_presence_notification(has_target, move_dist, still_dist, move_energy, still_energy);

                    ESP_LOGD(TAG, "BLE presence notification sent (interval: %ds, presence: %s, recent_connection: %s)",
                            ble_interval, presence_detected ? "yes" : "no", 
                            has_recently_connected_ble_clients() ? "yes" : "no");
                } else {
                    ESP_LOGD(TAG, "BLE presence notification skipped - no active clients");
                }

                nextBLENotification = currentTime + ble_interval_ms;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}
void groundSwitchCheck()
{
    if(!gpio_get_level(GND_SW))
    {
        gpio_set_level(GND_SW , true);
    }
}

bool sensorSetup()
{
    ESP_LOGI(TAG, "Attempting to begin LD2410 sensor communication...");
    if(!sensor.begin()){
         ESP_LOGE( TAG," LD2410 failed to initialize - sensor.begin() returned false");
         ESP_LOGE( TAG," Check sensor power, UART wiring, and baud rate");

        return false;
    }
    ESP_LOGI(TAG, "LD2410 sensor.begin() successful");
    if(!sensor.configMode()){
         ESP_LOGE( TAG," LD2410 failed to enter config mode  ");

        return false;
    }
    if(!sensor.requestBToff()){
         ESP_LOGE( TAG," LD2410 BLE turned off FALIED  ");

        return false;
    }

    if(!sensor.requestReboot()){
         ESP_LOGE( TAG," LD2410 rebooting failedn ");

        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(2000)) ; // Delay for 1 second

    if(!sensor.enhancedMode()){
         ESP_LOGE( TAG," LD2410 failed to enter enhanced mode  ");

         return false;
    }

    return true;

}

// LD2410 sensor data access functions for BLE commands
bool LD2410_getPresenceDetected() {
    return sensor.presenceDetected();
}

uint16_t LD2410_getMovingDistance() {
    return (uint16_t)sensor.movingTargetDistance();
}

uint16_t LD2410_getStationaryDistance() {
    return (uint16_t)sensor.stationaryTargetDistance();
}

uint8_t LD2410_getMovingEnergy() {
    return sensor.movingTargetSignal();
}

uint8_t LD2410_getStationaryEnergy() {
    return sensor.stationaryTargetSignal();
}


bool LD2410_init()
{
    // Allow time for LD2410 sensor to power up after MOSFET enable
    ESP_LOGI(TAG, "Waiting for LD2410 sensor to power up...");
    vTaskDelay(pdMS_TO_TICKS(1000));  // 1000ms delay for sensor power-up

    int ret =sensor.uartInit(LD2410_UART_PORT , LD2410_UART_TX , LD2410_UART_RX, LD2410_BAUD);
    if(ret!= ESP_OK){
         ESP_LOGE( TAG," uart initialization failed with code: %d  ", ret);
         return false;
    }
    ESP_LOGD( TAG," uart initialization success: %d  ", ret);

    if(!sensorSetup())
    {
        ESP_LOGE( TAG," LD2410 Setup Failed");
        return false;
    }
    ESP_LOGD( TAG," LD2410 Setup Successfull ");


    // if(!sensor.requestReset()){
    //      ESP_LOGE( TAG," LD2410 Factory Reset Failed  ");

    //     return false;
    // }
    //  ESP_LOGD( TAG," Factory Reset Success  ");

    //  ESP_LOGD( TAG,"%s ", (sensor.getFirmware()).c_str());

    if(xTaskCreate(LD2410Task, "LD2410Task", 8192, NULL, 1, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create IR sensor task");
        set_led_status_flag("error_ir_sensor", true);
        return false;
    }
    getDetectionRange();
    getDetectionDuration();
    getGateSensitivities();


    return true;
}

void setDetectionDuration(uint8_t durationVal)
{
    groundSwitchCheck();
    if(!sensorSetup())
    {
        ESP_LOGE( TAG," LD2410 Setup Failed");
    }

    set_detection_duration(durationVal);

    if(!sensor.setNoOneWindow(durationVal)){
         ESP_LOGE( TAG," LD2410 setNoOneWindow failedn ");

        return ;
    }
     ESP_LOGD( TAG," LD2410 setNoOneWindow success  ");

    getDetectionDuration();

}

uint8_t getDetectionDuration()
{
    groundSwitchCheck();
    if(!sensorSetup())
    {
        ESP_LOGE( TAG," LD2410 Setup Failed");
    }

    uint8_t duration = sensor.getNoOneWindow();
    ESP_LOGW( TAG," LD2410 noone window value:%x   ",duration );
    return(duration);

}

void setDetectionRange(bool resolution , uint8_t number_of_gates)
{
    groundSwitchCheck();
    if(!sensorSetup())
    {
        ESP_LOGE( TAG," LD2410 Setup Failed");
    }

    // uint8_t gateSplit = number_of_gates/2;
    // if(gateSplit ==0){
    //     gateSplit=1;
    // }
     if(!(sensor.setResolution(resolution) & sensor.setMaxGate(number_of_gates,number_of_gates,sensor.getNoOneWindow()))){
    //  if(!(sensor.setResolution(resolution) & sensor.setMaxGate(number_of_gates,number_of_gates,get_detection_duration()))){
    //  if(!(sensor.setResolution(resolution) || sensor.setMaxMovingGate(number_of_gates) || sensor.setMaxStationaryGate(number_of_gates))){ //activates moving and stationary detection on x no of gates
    //  if(!(sensor.setResolution(resolution) || sensor.setMaxMovingGate(number_of_gates) )){ //activates moving and stationary detection on x no of gates
         ESP_LOGE( TAG," LD2410 setDetectionRange failed  ");
        return ;
    }
    ESP_LOGD( TAG," LD2410 setDetectionRange Success  ");

    getDetectionRange();
}

RangeParams getDetectionRange()
{
    groundSwitchCheck();
    if(!sensorSetup())
    {
        ESP_LOGE( TAG," LD2410 Setup Failed");
    }
    RangeParams rangeParameters;
    rangeParameters.resolutionMeters = sensor.getResolution();
    rangeParameters.activeGates = sensor.getMaxGate();
    rangeParameters.range = sensor.getConfiguredRange();

    //  ESP_LOGD( TAG,"  LD2410 noone window value:%x   ",duration );
    ESP_LOGW( TAG," LD2410 Resolution: %u cm, Active Gates: %u, Range: %u cm ", rangeParameters.resolutionMeters, rangeParameters.activeGates, rangeParameters.range);
    return(rangeParameters);
}

void setGateSensitivities(uint8_t gateNumber , uint8_t movingThres , uint8_t stationaryThres)
{
    groundSwitchCheck();
    if(!sensorSetup())
    {
        ESP_LOGE( TAG," LD2410 Setup Failed");
    }

    if(!(sensor.setGateParameters(gateNumber,movingThres,stationaryThres))){
         ESP_LOGE( TAG," LD2410 setGateParameters failedn  ");
        return ;
    }
     ESP_LOGD( TAG," LD2410 setGateParameters Success  ");


    getGateSensitivities();

}

SensitivityThresholds getGateSensitivities()
{
    groundSwitchCheck();
    if(!sensorSetup())
    {
        ESP_LOGE( TAG," LD2410 Setup Failed");
    }
    SensitivityThresholds sensThresh;

    MyLD2410::ValuesArray movingThresValues;
    movingThresValues = sensor.getMovingThresholds();
    for(uint8_t i=0 ; i<movingThresValues.N ; i++){
        sensThresh.moving.values[i]=movingThresValues.values[i];
        ESP_LOGW( TAG," LD2410   Gate : %d  , moving Threshold Value :%d   ",sensThresh.moving.N ,sensThresh.moving.values[i] );

    }
    MyLD2410::ValuesArray stationaryThresValues;
    stationaryThresValues = sensor.getStationaryThresholds();
    for(uint8_t i=0 ; i<stationaryThresValues.N ; i++){
        sensThresh.stationary.values[i] = stationaryThresValues.values[i];
        ESP_LOGW( TAG," LD2410   Gate : %d  , stationary Threshold Value :%d   ",sensThresh.stationary.N ,sensThresh.stationary.values[i]);

    }

    return(sensThresh);
}




