#include "LD2410_interface.hpp"

MyLD2410 sensor;


bool LD2410Interface::LD2410_init(){
    
    if(sensor.uartInit() != ESP_OK){    
        
        return false;
    }
    if(!sensor.begin()){
        printf("[LD2410_init] LD2410 failed to initialize\n\r");
        
        return false;
    }
    printf("[LD2410_init] LD2410 initialized\n\r");


    return false;
}


