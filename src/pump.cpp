#include "pump.h"
#include "global.h"
#include "task_webserver.h"
#include "coreiot.h"

#include <WiFi.h>
#include <time.h>

#define PUMP_PIN 6

void task_pump(void *pvParameters)
{
  SharedContext *ctx = static_cast<SharedContext *>(pvParameters);
  if (!ctx)
  {
    vTaskDelete(nullptr);
    return;
  }

  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);

  bool ntpConfigured = false;
  int lastScheduleYDay = -1;
  unsigned long scheduleEndMs = 0;
  unsigned long lastHeartbeat = 0;
  bool lastOutputState = false;
  const unsigned long autoStartupDelayMs = 6000;

  auto broadcastPumpState = [&]()
  {
    float soil = 0.0f;
    bool pumpActual = false;
    int pumpMode = 0;
    bool pumpAutoArmed = false;
    String pumpController;
    int pumpThreshold = 0;
    int pumpHysteresis = 0;
    bool scheduleEnabled = false;
    int scheduleHour = 0;
    int scheduleMinute = 0;
    int scheduleDurationSec = 0;

    xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
    soil = ctx->soilMoisture;
    pumpActual = ctx->pumpActualState;
    pumpMode = ctx->pumpMode;
    pumpAutoArmed = ctx->pumpAutoArmed;
    pumpController = ctx->pumpController;
    pumpThreshold = ctx->pumpAutoThreshold;
    pumpHysteresis = ctx->pumpAutoHysteresis;
    scheduleEnabled = ctx->pumpScheduleEnabled;
    scheduleHour = ctx->pumpScheduleHour;
    scheduleMinute = ctx->pumpScheduleMinute;
    scheduleDurationSec = ctx->pumpScheduleDurationSec;
    xSemaphoreGive(ctx->mutexContext);

    String json = "{\"pump_state\":\"" + String(pumpActual ? "ON" : "OFF") +
                  "\",\"pump_mode\":\"" + String(pumpMode == 0 ? "AUTO" : "MANUAL") +
                  "\",\"pump_auto_armed\":" + String(pumpAutoArmed ? "true" : "false") +
                  ",\"pump_controller\":\"" + pumpController +
                  "\",\"soil_moisture\":" + String(soil, 2) +
                  ",\"pump_threshold\":" + String(pumpThreshold) +
                  ",\"pump_hysteresis\":" + String(pumpHysteresis) +
                  ",\"pump_schedule_enabled\":" + String(scheduleEnabled ? "true" : "false") +
                  ",\"pump_schedule_time\":\"" +
                  String((scheduleHour < 10 ? "0" : "")) + String(scheduleHour) + ":" +
                  String((scheduleMinute < 10 ? "0" : "")) + String(scheduleMinute) +
                  "\",\"pump_schedule_duration\":" + String(scheduleDurationSec) + "}";
    Webserver_sendata(ctx, json);
  };

  // Initialize context-side runtime values
  xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
  ctx->pumpActualState = false;
  ctx->pumpController = "AUTO";
  xSemaphoreGive(ctx->mutexContext);

  while (1)
  {
    if (!ntpConfigured && WiFi.status() == WL_CONNECTED)
    {
      configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
      ntpConfigured = true;
    }

    // Snapshot control inputs from context
    bool scheduleRunning = false;
    bool scheduleEnabled = false;
    int scheduleHour = 0;
    int scheduleMinute = 0;
    int scheduleDurationSec = 0;
    int pumpMode = 0;
    bool pumpManualOverride = false;
    bool pumpManualState = false;
    bool pumpAutoArmed = false;
    float soil = 0.0f;
    bool soilReady = false;
    int threshold = 0;
    int hysteresis = 0;
    bool pumpActual = false;

    xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
    scheduleRunning = ctx->pumpScheduleRunning;
    scheduleEnabled = ctx->pumpScheduleEnabled;
    scheduleHour = ctx->pumpScheduleHour;
    scheduleMinute = ctx->pumpScheduleMinute;
    scheduleDurationSec = ctx->pumpScheduleDurationSec;
    pumpMode = ctx->pumpMode;
    pumpManualOverride = ctx->pumpManualOverride;
    pumpManualState = ctx->pumpManualState;
    pumpAutoArmed = ctx->pumpAutoArmed;
    soil = ctx->soilMoisture;
    soilReady = ctx->soilReady;
    threshold = ctx->pumpAutoThreshold;
    hysteresis = ctx->pumpAutoHysteresis;
    pumpActual = ctx->pumpActualState;
    xSemaphoreGive(ctx->mutexContext);

    if (scheduleRunning && millis() >= scheduleEndMs)
    {
      scheduleRunning = false;
    }

    if (scheduleEnabled && ntpConfigured && !scheduleRunning)
    {
      time_t now = time(nullptr);
      if (now > 1700000000)
      {
        struct tm t;
        localtime_r(&now, &t);
        if (t.tm_hour == scheduleHour && t.tm_min == scheduleMinute && t.tm_yday != lastScheduleYDay)
        {
          scheduleRunning = true;
          scheduleEndMs = millis() + (unsigned long)scheduleDurationSec * 1000UL;
          lastScheduleYDay = t.tm_yday;
        }
      }
    }

    bool targetPumpState = false;
    String controller = "AUTO";

    if (scheduleRunning)
    {
      targetPumpState = true;
      controller = "SCHEDULE";
    }
    else if (pumpMode == 1 || pumpManualOverride)
    {
      targetPumpState = pumpManualState;
      controller = "MANUAL";
    }
    else
    {
      if (!pumpAutoArmed)
      {
        targetPumpState = false;
        controller = "AUTO_DISARMED";
      }
      else if (!soilReady || millis() < autoStartupDelayMs)
      {
        targetPumpState = false;
        controller = "AUTO_WAIT";
      }
      else
      {
        // AUTO with hysteresis to avoid rapid switching near threshold.
        if (soil < threshold)
        {
          targetPumpState = true;
        }
        else if (soil > (threshold + hysteresis))
        {
          targetPumpState = false;
        }
        else
        {
          targetPumpState = pumpActual;
        }
        controller = "AUTO";
      }
    }

    digitalWrite(PUMP_PIN, targetPumpState ? HIGH : LOW);

    xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
    ctx->pumpScheduleRunning = scheduleRunning;
    ctx->pumpActualState = targetPumpState;
    ctx->pumpController = controller;
    xSemaphoreGive(ctx->mutexContext);

    if (targetPumpState != lastOutputState || (millis() - lastHeartbeat) > 5000)
    {
      lastOutputState = targetPumpState;
      lastHeartbeat = millis();

      // Đồng bộ lên WebSocket (browser)
      broadcastPumpState();

      // Đồng bộ lên CoreIOT attributes (giống cơ chế LED)
      coreiot_publish_attribute(ctx, "pumpState", targetPumpState);
      coreiot_publish_attribute(ctx, "modeState", pumpMode == 1);
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

