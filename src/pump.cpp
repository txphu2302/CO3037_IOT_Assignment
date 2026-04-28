#include "pump.h"
#include "global.h"
#include "task_webserver.h"
#include <time.h>
#include <WiFi.h>
#define PUMP_PIN 6

static bool ntpConfigured = false;
static int lastScheduleYDay = -1;

static void broadcastPumpState() {
  String json = "{\"pump_state\":\"" + String(pump_actual_state ? "ON" : "OFF") +
                "\",\"pump_mode\":\"" + String(pump_mode == 0 ? "AUTO" : "MANUAL") +
                "\",\"pump_auto_armed\":" + String(pump_auto_armed ? "true" : "false") +
                "\",\"pump_controller\":\"" + pump_controller +
                "\",\"soil_moisture\":" + String(glob_soil_moisture, 2) +
                ",\"pump_threshold\":" + String(pump_auto_threshold) +
                ",\"pump_hysteresis\":" + String(pump_auto_hysteresis) +
                ",\"pump_schedule_enabled\":" + String(pump_schedule_enabled ? "true" : "false") +
                ",\"pump_schedule_time\":\"" +
                String((pump_schedule_hour < 10 ? "0" : "")) + String(pump_schedule_hour) + ":" +
                String((pump_schedule_minute < 10 ? "0" : "")) + String(pump_schedule_minute) +
                "\",\"pump_schedule_duration\":" + String(pump_schedule_duration_sec) + "}";
  Webserver_sendata(json);
}

void task_pump(void *pvParameters) {
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  pump_actual_state = false;
  unsigned long scheduleEndMs = 0;
  unsigned long lastHeartbeat = 0;
  bool lastOutputState = false;
  const unsigned long autoStartupDelayMs = 6000;

  while (1) {
    if (!ntpConfigured && WiFi.status() == WL_CONNECTED) {
      configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
      ntpConfigured = true;
    }

    if (pump_schedule_running && millis() >= scheduleEndMs) {
      pump_schedule_running = false;
    }

    if (pump_schedule_enabled && ntpConfigured && !pump_schedule_running) {
      time_t now = time(nullptr);
      if (now > 1700000000) {
        struct tm t;
        localtime_r(&now, &t);
        if (t.tm_hour == pump_schedule_hour && t.tm_min == pump_schedule_minute &&
            t.tm_yday != lastScheduleYDay) {
          pump_schedule_running = true;
          scheduleEndMs = millis() + (unsigned long)pump_schedule_duration_sec * 1000UL;
          lastScheduleYDay = t.tm_yday;
        }
      }
    }

    bool targetPumpState = false;
    if (pump_schedule_running) {
      targetPumpState = true;
      pump_controller = "SCHEDULE";
    } else if (pump_mode == 1 || pump_ap_manual_override) {
      targetPumpState = pump_ap_manual_state;
      pump_controller = "MANUAL";
    } else {
      if (!pump_auto_armed) {
        targetPumpState = false;
        pump_controller = "AUTO_DISARMED";
      } else if (!glob_soil_ready || millis() < autoStartupDelayMs) {
        targetPumpState = false;
        pump_controller = "AUTO_WAIT";
      } else {
      // AUTO with hysteresis to avoid rapid switching near threshold.
        if (glob_soil_moisture < pump_auto_threshold) {
          targetPumpState = true;
        } else if (glob_soil_moisture > (pump_auto_threshold + pump_auto_hysteresis)) {
          targetPumpState = false;
        } else {
          targetPumpState = pump_actual_state;
        }
        pump_controller = "AUTO";
      }
    }

    digitalWrite(PUMP_PIN, targetPumpState ? HIGH : LOW);
    pump_actual_state = targetPumpState;

    if (pump_actual_state != lastOutputState || (millis() - lastHeartbeat) > 5000) {
      lastOutputState = pump_actual_state;
      lastHeartbeat = millis();
      broadcastPumpState();
    }

    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}