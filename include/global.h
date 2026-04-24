#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

struct SharedContext {
    float temperature;
    float humidity;
    float soilMoisture;
    SemaphoreHandle_t mutexContext;
    SemaphoreHandle_t semLEDUpdate;
    int ledState; // 1: Normal, 2: Warning, 3: Critical
    SemaphoreHandle_t semNeoUpdate;
    int neoState;
    SemaphoreHandle_t semLCDUpdate;
    int lcdState;
};

extern float glob_temperature;
extern float glob_humidity;
extern float glob_soil_moisture;

extern String WIFI_SSID;
extern String WIFI_PASS;
extern String CORE_IOT_TOKEN;
extern String CORE_IOT_SERVER;
extern String CORE_IOT_PORT;

extern boolean isWifiConnected;
extern SemaphoreHandle_t xBinarySemaphoreInternet;
extern bool led_ap_manual_override;
extern bool led_ap_manual_state;
extern bool neo_ap_manual_override;
extern bool neo_ap_manual_state;
extern uint8_t neo_ap_color_r;
extern uint8_t neo_ap_color_g;
extern uint8_t neo_ap_color_b;
extern bool pump_ap_manual_override;
extern bool pump_ap_manual_state;
#endif