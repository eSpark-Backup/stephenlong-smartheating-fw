#pragma once

#include "defines.h"


#ifdef __cplusplus
extern "C" {
#endif



typedef enum {
    LED_EFFECT_NONE = 0,
    LED_EFFECT_BLINK,
    LED_EFFECT_DOUBLE_BLINK,
    LED_EFFECT_TRIPLE_FLASH,
    LED_EFFECT_BREATHING,
    LED_EFFECT_STEADY,
} led_effect_t;

typedef enum
{
    RED = 0,
    BLUE,           // Standard blue (for BLE pairing)
    BLUE_DARK,      // Dark blue/indigo (for SoftAP pairing)
    GREEN,
    PURPLE,
    YELLOW,
    NONE,
}COLORS;

typedef enum
{
    LED_SPEED_SLOW = 0,
    LED_SPEED_MEDIUM,
    LED_SPEED_FAST,
    NO_SPEED,
}EFFECT_SPEED;

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
}RGB_CODE;

void status_led_init(void);
void set_led_status(void);

int effect_delay(EFFECT_SPEED effecSpeed);
void set_led_effect(led_effect_t effect , COLORS colorSelection , EFFECT_SPEED effectSPEED);
led_effect_t get_current_led_effect(void);



#ifdef __cplusplus
}
#endif