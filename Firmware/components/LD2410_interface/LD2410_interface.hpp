#pragma once

#include "MyLD2410_esp.hpp"
#include "defines.h"
#include "app.h"


class LD2410Interface{
    public:
        bool LD2410_init();
        void setDetectionDuration(uint8_t durationVal);
        void setDetectionRange(bool resolution , uint8_t number_of_gates);
        void setGateSensitivities(uint8_t gateNumber , uint8_t movingThres , uint8_t stationaryThres);








};


