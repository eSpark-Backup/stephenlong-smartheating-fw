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

typedef struct {
    led_effect_t effect;
    COLORS color;
    EFFECT_SPEED speed;
    bool initialized;
}LED_STATE_T;

// Creatign flag bitfield
typedef struct
{
    // Critical errors (highest priority - RED)
    uint16_t error_critical                 :1;                              // LSB?
    uint16_t error_ir_sensor                :1;
    uint16_t error_heating                  :1;
    uint16_t error_nvs                      :1;
    uint16_t erase_nvs                      :1;

    // Button feedback (high priority - various colors)
    uint16_t button_ble_feedback            :1;
    uint16_t button_factory_reset_feedback  :1;
    uint16_t button_bypass_feedback         :1;
    uint16_t button_softap_feedback         :1;

    // System operations (medium priority - PURPLE/BLUE)
    uint16_t pairing_mode_active            :1;
    uint16_t firmware_update                :1;
    uint16_t factory_reset_confirmation     :1;
    uint16_t bypass_mode_active             :1;

    // Normal operation (lowest priority - GREEN/YELLOW)
    uint16_t normal_operation               :1;
    uint16_t wifi_connected                 :1;
    uint16_t wifi_warning                   :1;
} led_status_flags_t;

led_status_flags_t* get_led_status_flags(void);
int effect_delay(EFFECT_SPEED effecSpeed);
RGB_CODE get_rgb_code(COLORS colorSel);
void set_led_status(LED_STATE_T* state);
void set_led_effect(led_effect_t effect , COLORS colorSelection , EFFECT_SPEED effectSPEED);
led_effect_t get_current_led_effect(void);

void led_effect_handler(void);

void status_led_init(void);


#ifdef __cplusplus
}
#endif