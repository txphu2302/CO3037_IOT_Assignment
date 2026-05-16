#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <Arduino.h>

// Forward declarations to avoid heavy includes in global state header.
class AsyncWebServer;
class AsyncWebSocket;
class PubSubClient;
class WiFiClient;

struct SharedContext {
  float temperature;
  float humidity;
  float soilMoisture;
  bool soilReady;

  SemaphoreHandle_t mutexContext;
  SemaphoreHandle_t mutexSerial;
  SemaphoreHandle_t semLEDUpdate;
  int ledState; // 1: Normal, 2: Warning, 3: Critical
  SemaphoreHandle_t semNeoUpdate;
  int neoState;
  SemaphoreHandle_t semLCDUpdate;
  int lcdState;

  // Network / CoreIOT configuration loaded from LittleFS
  String wifiSsid;
  String wifiPass;
  String coreIotToken;
  String coreIotServer;
  String coreIotPort;

  // STA connected semaphore (given once after WiFi STA connected)
  SemaphoreHandle_t semInternetConnected;

  // Manual overrides (AP/STA UI)
  bool ledManualOverride;
  bool ledManualState;
  bool neoManualOverride;
  bool neoManualState;
  uint8_t neoManualR;
  uint8_t neoManualG;
  uint8_t neoManualB;
  bool pumpManualOverride;
  bool pumpManualState;

  // Pump advanced control (STA page)
  int pumpMode;               // 0 = AUTO, 1 = MANUAL
  bool pumpAutoArmed;         // AUTO runs only after user saves AUTO config
  int pumpAutoThreshold;
  int pumpAutoHysteresis;
  bool pumpScheduleEnabled;
  int pumpScheduleHour;
  int pumpScheduleMinute;
  int pumpScheduleDurationSec;
  bool pumpScheduleRunning;
  bool pumpActualState;
  String pumpController;      // AUTO | MANUAL | SCHEDULE | AUTO_WAIT | AUTO_DISARMED

  // Web server (AP + STA pages)
  AsyncWebServer *webServer;
  AsyncWebSocket *webSocket;
  bool webServerRunning;

  // CoreIOT MQTT client (shared publish helper)
  WiFiClient *coreiotWifi;
  PubSubClient *coreiotMqtt;
  bool coreiotReady;
  SemaphoreHandle_t mutexMqtt;
};

#endif
