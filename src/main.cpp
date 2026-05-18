#include "global.h"
#include "serial_log.h"

#include "led_blinky.h"
#include "neo_blinky.h"
#include "temp_humi_monitor.h"
// #include "mainserver.h"
#include "tinyml.h"
#include "coreiot.h"
#include "pump.h"
#include "soil_sensor.h"


// include task
#include "task_check_info.h"
#include "task_toogle_boot.h"
#include "task_webserver.h"
#include "task_wifi.h"

static void task_mainloop(void *pvParameters) {
  SharedContext *ctx = static_cast<SharedContext *>(pvParameters);
  if (!ctx) {
    vTaskDelete(nullptr);
    return;
  }

  while (1) {
    if (check_info_File(ctx, true)) {
      if (!Wifi_reconnect(ctx)) {
        Webserver_stop(ctx);
      }
    }
    Webserver_reconnect(ctx);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup() {
  Serial.begin(115200);
  // LittleFS.begin(true); LittleFS.remove("/info.dat"); ESP.restart();

  SharedContext *ctx = new SharedContext();
  ctx->temperature = 0;
  ctx->humidity = 0;
  ctx->soilMoisture = 0;
  ctx->soilReady = false;
  ctx->ledState = 1;
  ctx->neoState = 1;
  ctx->lcdState = 1;
  ctx->mutexContext = xSemaphoreCreateMutex();
  ctx->mutexSerial = nullptr;
  ctx->semLEDUpdate = xSemaphoreCreateBinary();
  ctx->semNeoUpdate = xSemaphoreCreateBinary();
  ctx->semLCDUpdate = xSemaphoreCreateBinary();
  ctx->semInternetConnected = xSemaphoreCreateBinary();

  ctx->wifiSsid = "";
  ctx->wifiPass = "";
  ctx->coreIotToken = "";
  ctx->coreIotServer = "";
  ctx->coreIotPort = "";

  ctx->ledManualOverride = false;
  ctx->ledManualState = false;
  ctx->neoManualOverride = false;
  ctx->neoManualState = false;
  ctx->neoManualR = 255;
  ctx->neoManualG = 0;
  ctx->neoManualB = 0;
  ctx->pumpManualOverride = false;
  ctx->pumpManualState = false;

  ctx->pumpMode = 0;
  ctx->pumpAutoArmed = false;
  ctx->pumpAutoThreshold = 40;
  ctx->pumpAutoHysteresis = 5;
  ctx->pumpScheduleEnabled = false;
  ctx->pumpScheduleHour = 6;
  ctx->pumpScheduleMinute = 0;
  ctx->pumpScheduleDurationSec = 15;
  ctx->pumpScheduleRunning = false;
  ctx->pumpActualState = false;
  ctx->pumpController = "AUTO";

  ctx->webServer = new AsyncWebServer(80);
  ctx->webSocket = new AsyncWebSocket("/ws");
  ctx->webServerRunning = false;

  ctx->coreiotWifi = new WiFiClient();
  ctx->coreiotMqtt = new PubSubClient(*ctx->coreiotWifi);
  ctx->coreiotReady = false;
  ctx->mutexMqtt = xSemaphoreCreateMutex();

  ctx->pendingLedAttributeUpdate = false;
  ctx->pendingLedAttributeValue = false;

  serialLogInit(ctx);

  // Load saved configuration and start AP if needed (Task 4 + Task 6)
  check_info_File(ctx, false);

  xTaskCreate(led_blinky, "Task LED Blink", 2048, (void *)ctx, 2, NULL);
  xTaskCreate(neo_blinky, "Task NEO Blink", 2048, (void *)ctx, 2, NULL);
  xTaskCreate(temp_humi_monitor, "Task TEMP HUMI Monitor", 2048, (void *)ctx, 2, NULL);
  xTaskCreate(task_soil_sensor, "Task Soil Sensor", 2048, (void *)ctx, 2, NULL);
  xTaskCreate(task_pump, "Task Pump", 4096, (void *)ctx, 2, NULL);
  // xTaskCreate(main_server_task, "Task Main Server" ,8192  ,NULL  ,2 , NULL);
  xTaskCreate(coreiot_task, "CoreIOT Task", 6144, (void *)ctx, 2, NULL);
  xTaskCreate(tiny_ml_task, "Tiny ML Task", 8192, (void *)ctx, 2, NULL);
  xTaskCreate(task_mainloop, "Task MainLoop", 4096, (void *)ctx, 1, NULL);
  // xTaskCreate(Task_Toogle_BOOT, "Task_Toogle_BOOT", 4096, NULL, 2, NULL);
}

void loop() {
  // Main logic is handled by FreeRTOS tasks (see task_mainloop).
  vTaskDelay(pdMS_TO_TICKS(1000));
}
