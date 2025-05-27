#pragma once

#include "MyLD2410_esp.hpp"
#include "defines.h"
#include "app.h"
#include "freertos/freertos.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "Comm_Interface.h"


#define TAG "PRESENCE_SENSOR"



typedef struct { 
    uint8_t resolutionMeters; 
    uint8_t activeGates;
    uint16_t range;
}RangeParams;

typedef struct {
    uint8_t values[9];
    uint8_t N;
} Thresholds;

typedef struct {
    Thresholds moving;
    Thresholds stationary;
}SensitivityThresholds;

bool LD2410_init();
void setDetectionDuration(uint8_t durationVal);
uint8_t getDetectionDuration();
void setDetectionRange(bool resolution , uint8_t number_of_gates);
RangeParams getDetectionRange();
void setGateSensitivities(uint8_t gateNumber , uint8_t movingThres , uint8_t stationaryThres);
SensitivityThresholds getGateSensitivities();








