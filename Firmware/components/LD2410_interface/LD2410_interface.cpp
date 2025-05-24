#include "LD2410_interface.hpp"

MyLD2410 sensor;


bool LD2410Interface::LD2410_init(){
    
    int ret =sensor.uartInit();
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

    if(!sensor.setNoOneWindow(durationVal)){
        printf("[LD2410_init] LD2410 setNoOneWindow failedn\r");
        
        return ;
    }
    printf("[LD2410_init] LD2410 setNoOneWindow success\n\r");


    uint8_t duration = sensor.getNoOneWindow();
    printf("[LD2410_init] LD2410 noone window value:%x \n\r",duration );



}



