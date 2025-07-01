#include "LD2410_interface.hpp"


MyLD2410 sensor;
unsigned long nextPrint = 0, printEvery = 1000;  // print every second
LD2410_Data presenceSens;

void printValue(const uint8_t &val) {
    printf(" ' ' %d" , val);
}

void printData(){
    ESP_LOGD(TAG , "Presence Status- %s" , sensor.statusString());
    
    if (sensor.movingTargetDetected()) {
        ESP_LOGD(TAG , "MOVING = Target Signal %d  at %ld cm" , sensor.movingTargetSignal(),  sensor.movingTargetDistance());
        if (sensor.inEnhancedMode()) {

            if (esp_log_level_get(TAG) >= ESP_LOG_DEBUG)
            {
                printf("[PRESENCE SENSOR] signals->[" );
                sensor.getMovingSignals().forEach(printValue);
                printf(" ] thresholds:[" );
                sensor.getMovingThresholds().forEach(printValue);
                printf("]\n" );
            }
         }
    }
}

void setLD2410_Data(SensResponse presenceDetected, unsigned long distance)
{
    presenceSens.isPresenceTrue = presenceDetected;
    presenceSens.presence = distance;
}

LD2410_Data getLD2410_Data()
{
    return presenceSens;
}

void LD2410Task(void *pvParameters){
    while(1){

        if ((esp_timer_get_time() / 1000) > nextPrint)
        {
            SensResponse presence = FAIL;
            unsigned long distance = 0;
            if (sensor.check() == MyLD2410::Response::DATA) {
                if (sensor.presenceDetected()) {
                    presence = (SensResponse)sensor.presenceDetected();
                    distance = sensor.detectedDistance();
                }
                printData();
            }
            else if (sensor.check() == MyLD2410::Response::FAIL) {
                presence = (SensResponse)FAIL;
                distance =  sensor.detectedDistance();
            }
                ESP_LOGD(TAG , "PRESENCE DETECTED %d !!  Distance: %ld cm ", presence, sensor.detectedDistance());
                setLD2410_Data(presence, distance);

            nextPrint = (esp_timer_get_time() / 1000) + printEvery;
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
    if(!sensor.begin()){
         ESP_LOGE( TAG," LD2410 failed to initialize  ");
        
        return false;
    }    
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


bool LD2410_init(){
    
    presenceSens.isPresenceTrue = FAIL;
    presenceSens.presence = 0;

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

    if(xTaskCreate(LD2410Task, "LD2410Task", 2048, NULL, 1, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create IR sensor task");
        set_device_status(1, ERROR);      
        set_led_effect(LED_EFFECT_BLINK, RED ,LED_SPEED_SLOW );
        return false;
    }
    else
        ESP_LOGE(TAG, "Success! Created IR sensor task");
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




