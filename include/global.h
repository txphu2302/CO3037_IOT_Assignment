#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <Arduino.h>


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
extern bool glob_soil_ready;

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

// Pump advanced control (STA page)
extern int pump_mode; // 0 = AUTO, 1 = MANUAL
extern bool pump_auto_armed; // AUTO runs only after user saves AUTO config
extern int pump_auto_threshold;
extern int pump_auto_hysteresis;
extern bool pump_schedule_enabled;
extern int pump_schedule_hour;
extern int pump_schedule_minute;
extern int pump_schedule_duration_sec;
extern bool pump_schedule_running;
extern bool pump_actual_state;
extern String pump_controller; // AUTO | MANUAL | SCHEDULE
#endif