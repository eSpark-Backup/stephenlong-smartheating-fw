#include "LD2410_interface.hpp"


MyLD2410 sensor;

void LD2410Task(void *pvParameters){
    while(1){



        vTaskDelay(pdMS_TO_TICKS(100));
    }

}

bool LD2410_init(){
    
    int ret =sensor.uartInit(LD2410_UART_PORT , LD2410_UART_TX , LD2410_UART_RX, LD2410_BAUD);
    if(ret!= ESP_OK){    
         ESP_LOGE( TAG," uart initialization failed with code: %d  ", ret);
        return false;
    }
     ESP_LOGD( TAG," uart initialization success: %d  ", ret);

    if(!sensor.begin()){
         ESP_LOGE( TAG," LD2410 failed to initialize  ");
        
        return false;
    }
     ESP_LOGD( TAG," LD2410 initialized  ");

    if(!sensor.configMode()){
         ESP_LOGE( TAG," LD2410 failed to enter config mode  ");
        
        return false;
    }
     ESP_LOGD( TAG," LD2410 entering config mode  ");

    if(!sensor.requestBToff()){
         ESP_LOGE( TAG," LD2410 BLE turned off FALIED  ");
        
        return false;
    }
     ESP_LOGD( TAG," LD2410 BLE turned off successfull  ");

    if(!sensor.requestReboot()){
         ESP_LOGE( TAG," LD2410 rebooting failedn ");
        
        return false;
    }
     ESP_LOGD( TAG," LD2410 rebooting  ");

    vTaskDelay(pdMS_TO_TICKS(2000)); // Delay for 1 second
    //  ESP_LOGD( TAG,"%s ", (sensor.getFirmware()).c_str());

    if(xTaskCreate(LD2410Task, "LD2410Task", 2048, NULL, 1, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create IR sensor task");
        set_device_status(1, ERROR);
        return false;
    }
    getDetectionRange();
    getDetectionDuration();
    getGateSensitivities();


    return true;
}

void setDetectionDuration(uint8_t durationVal){

    set_detection_duration(durationVal);

    if(!sensor.setNoOneWindow(durationVal)){
         ESP_LOGE( TAG," LD2410 setNoOneWindow failedn ");
        
        return ;
    }
     ESP_LOGD( TAG," LD2410 setNoOneWindow success  ");

    getDetectionDuration();

}

uint8_t getDetectionDuration(){

    uint8_t duration = sensor.getNoOneWindow();
    ESP_LOGW( TAG," LD2410 noone window value:%x   ",duration );
    return(duration);

}

void setDetectionRange(bool resolution , uint8_t number_of_gates){
   
    uint8_t gateSplit = number_of_gates/2;
    if(gateSplit ==0){
        gateSplit=1;
    }
     if(!(sensor.setResolution(resolution) & sensor.setMaxGate(gateSplit,gateSplit,get_detection_duration()))){
         ESP_LOGE( TAG," LD2410 setDetectionRange failed  ");     
        return ;
    }
     ESP_LOGD( TAG," LD2410 setDetectionRange Success  ");
    
    getDetectionRange();
}

RangeParams getDetectionRange(){
    RangeParams rangeParameters;
    rangeParameters.resolutionMeters = sensor.getResolution();
    rangeParameters.activeGates = sensor.getMaxGate();
    rangeParameters.range = sensor.getConfiguredRange();

    //  ESP_LOGD( TAG,"  LD2410 noone window value:%x   ",duration );
    ESP_LOGW( TAG," LD2410 Resolution: %u cm, Active Gates: %u, Range: %u cm ", rangeParameters.resolutionMeters, rangeParameters.activeGates, rangeParameters.range);
    return(rangeParameters);
}

void setGateSensitivities(uint8_t gateNumber , uint8_t movingThres , uint8_t stationaryThres){
    
   
    if(!(sensor.setGateParameters(gateNumber,movingThres,stationaryThres))){
         ESP_LOGE( TAG," LD2410 setGateParameters failedn  ");     
        return ;
    }
     ESP_LOGD( TAG," LD2410 setGateParameters Success  ");


    getGateSensitivities();

}

SensitivityThresholds getGateSensitivities(){

    SensitivityThresholds sensThresh;

    MyLD2410::ValuesArray movingThresValues;
    movingThresValues = sensor.getMovingThresholds();
    for(uint8_t i=0 ; i<movingThresValues.N ; i++){
        sensThresh.moving.N= i ;
        sensThresh.moving.values[i]=movingThresValues.values[i];
        ESP_LOGW( TAG," LD2410  moving Gate : %d  , moving Threshold Values :%d   ",sensThresh.moving.N ,sensThresh.moving.values[i] );

    }
    MyLD2410::ValuesArray stationaryThresValues;
    stationaryThresValues = sensor.getStationaryThresholds();
    for(uint8_t i=0 ; i<stationaryThresValues.N ; i++){
        sensThresh.stationary.N= i ;
        sensThresh.stationary.values[i] = movingThresValues.values[i];
        ESP_LOGW( TAG," LD2410  stationary Gate : %d  , stationary Threshold Values :%d   ",sensThresh.stationary.N ,sensThresh.stationary.values[i]);

    }

    return(sensThresh);
}




