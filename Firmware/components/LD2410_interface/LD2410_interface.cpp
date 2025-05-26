#include "LD2410_interface.hpp"

MyLD2410 sensor;


bool LD2410Interface::LD2410_init(){
    
    int ret =sensor.uartInit(LD2410_UART_PORT , LD2410_UART_TX , LD2410_UART_RX, LD2410_BAUD);
    if(ret!= ESP_OK){    
        printf(" uart initialization failed with code: %d\n\r", ret);
        return false;
    }
    printf(" uart initialization success: %d\n\r", ret);

    if(!sensor.begin()){
        printf("[LD2410_init] LD2410 failed to initialize\n\r");
        
        return false;
    }
    printf("[LD2410_init] LD2410 initialized\n\r");

    if(!sensor.configMode()){
        printf("[LD2410_init] LD2410 failed to enter config mode\n\r");
        
        return false;
    }
    printf("[LD2410_init] LD2410 entering config mode\n\r");

    if(!sensor.requestBToff()){
        printf("[LD2410_init] LD2410 BLE turned off FALIED\n\r");
        
        return false;
    }
    printf("[LD2410_init] LD2410 BLE turned off successfull\n\r");

    if(!sensor.requestReboot()){
        printf("[LD2410_init] LD2410 rebooting failedn\r");
        
        return false;
    }
    printf("[LD2410_init] LD2410 rebooting\n\r");

    vTaskDelay(pdMS_TO_TICKS(2000)); // Delay for 1 second
    // printf("%s\n", (sensor.getFirmware()).c_str());

     return true;
}

void LD2410Interface::setDetectionDuration(uint8_t durationVal){

    set_detection_duration(durationVal);

    if(!sensor.setNoOneWindow(durationVal)){
        printf("[LD2410_init] LD2410 setNoOneWindow failedn\r");
        
        return ;
    }
    printf("[LD2410_init] LD2410 setNoOneWindow success\n\r");


    uint8_t duration = sensor.getNoOneWindow();
    printf("[LD2410_init] LD2410 noone window value:%x \n\r",duration );

}

void LD2410Interface::setDetectionRange(bool resolution , uint8_t number_of_gates){
   
    uint8_t gateSplit = number_of_gates/2;
    if(gateSplit ==0){
        gateSplit=1;
    }
     if(!(sensor.setResolution(resolution) & sensor.setMaxGate(gateSplit,gateSplit,get_detection_duration()))){
        printf("[LD2410_init] LD2410 setDetectionRange failedn\n\r");     
        return ;
    }
    printf("[LD2410_init] LD2410 setDetectionRange Success\n\r");
    
    uint8_t resolutionMeters = sensor.getResolution();
    uint8_t activeGates = (sensor.getMaxGate())*2;
    uint16_t range = sensor.getConfiguredRange();

    // printf("[LD2410_init] LD2410 noone window value:%x \n\r",duration );
    printf("[LD2410_init] Resolution: %u cm, Active Gates: %u, Range: %u cm\n", resolutionMeters, activeGates, range);
    
}
void LD2410Interface::setGateSensitivities(uint8_t gateNumber , uint8_t movingThres , uint8_t stationaryThres){
    
   
    if(!(sensor.setGateParameters(gateNumber,movingThres,stationaryThres))){
        printf("[LD2410_init] LD2410 setGateParameters failedn\n\r");     
        return ;
    }
    printf("[LD2410_init] LD2410 setGateParameters Success\n\r");
    
    MyLD2410::ValuesArray movingThresValues;
    movingThresValues = sensor.getMovingThresholds();
    for(uint8_t i=0 ; i<movingThresValues.N ; i++){
        printf("[LD2410_init] LD2410  moving Gate : %d  , moving Threshold Values :%d \n\r",i ,movingThresValues.values[i] );

    }
    MyLD2410::ValuesArray stationaryThresValues;
    stationaryThresValues = sensor.getStationaryThresholds();
    for(uint8_t i=0 ; i<stationaryThresValues.N ; i++){
        printf("[LD2410_init] LD2410  stationary Gate : %d  , stationary Threshold Values :%d \n\r",i ,stationaryThresValues.values[i] );

    }
    
}






