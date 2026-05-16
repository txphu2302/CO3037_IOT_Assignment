#include "coreiot.h"
#include "risk_label.h"
#include "task_webserver.h"

static String statusTextFromState(int state)
{
  if (state == 3)
    return "Critical";
  if (state == 2)
    return "Warning";
  return "Normal";
}

void coreiot_publish_attribute(SharedContext *ctx, const String &key, bool value)
{
  if (!ctx || !ctx->coreiotMqtt)
  {
    return;
  }

  if (ctx->mutexMqtt)
  {
    xSemaphoreTake(ctx->mutexMqtt, portMAX_DELAY);
  }

  if (ctx->coreiotMqtt->connected())
  {
    String payload = "{\"" + key + "\":" + (value ? "true" : "false") + "}";
    ctx->coreiotMqtt->publish("v1/devices/me/attributes", payload.c_str());
  }

  if (ctx->mutexMqtt)
  {
    xSemaphoreGive(ctx->mutexMqtt);
  }
}

static bool mqttReconnect(SharedContext *ctx)
{
  if (!ctx || !ctx->coreiotMqtt)
  {
    return false;
  }

  String token;
  xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
  token = ctx->coreIotToken;
  xSemaphoreGive(ctx->mutexContext);

  if (token.isEmpty())
  {
    return false;
  }

  String clientId = "ESP32Client-";
  clientId += String(random(0xffff), HEX);

  return ctx->coreiotMqtt->connect(clientId.c_str(), token.c_str(), nullptr);
}

void coreiot_task(void *pvParameters)
{
  SharedContext *ctx = static_cast<SharedContext *>(pvParameters);
  if (!ctx || !ctx->coreiotMqtt)
  {
    vTaskDelete(nullptr);
    return;
  }

  // Wait for STA WiFi
  if (ctx->semInternetConnected)
  {
    xSemaphoreTake(ctx->semInternetConnected, portMAX_DELAY);
  }

  String server;
  uint16_t port = 1883;
  xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
  server = ctx->coreIotServer;
  port = (uint16_t)ctx->coreIotPort.toInt();
  xSemaphoreGive(ctx->mutexContext);
  if (port == 0)
  {
    port = 1883;
  }

  ctx->coreiotMqtt->setServer(server.c_str(), port);

  // Callback uses std::function on ESP32, so we can capture ctx safely.
  ctx->coreiotMqtt->setCallback([ctx](char *topic, uint8_t *payload, unsigned int length)
                                {
    String topicStr(topic);
    String message;
    message.reserve(length + 1);
    for (unsigned int i = 0; i < length; ++i) {
      message += (char)payload[i];
    }

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, message) != DeserializationError::Ok) {
      return;
    }

    const char* method = doc["method"] | "";
    Serial.printf("CoreIoT RPC received: method=%s\n", method);
    if (strcmp(method, "getValueLED") == 0) {
      const String requestId = topicStr.substring(topicStr.lastIndexOf('/') + 1);
      const String responseTopic = "v1/devices/me/rpc/response/" + requestId;

      bool ledOn = false;
      xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
      ledOn = ctx->ledManualState;
      xSemaphoreGive(ctx->mutexContext);

      if (ctx->coreiotMqtt) ctx->coreiotMqtt->publish(responseTopic.c_str(), ledOn ? "true" : "false");
      return;
    }

    if (strcmp(method, "setValueLED") == 0) {
      bool params = false;
      if (doc["params"].is<bool>()) {
        params = doc["params"].as<bool>();
      } else {
        params = (doc["params"].as<String>() == "true" || doc["params"].as<String>() == "1" || doc["params"].as<String>() == "ON");
      }

      const String requestId = topicStr.substring(topicStr.lastIndexOf('/') + 1);
      const String responseTopic = "v1/devices/me/rpc/response/" + requestId;

      Serial.printf("setValueLED: params=%s\n", params ? "true" : "false");

      xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
      ctx->ledManualOverride = true;
      ctx->ledManualState = params;
      xSemaphoreGive(ctx->mutexContext);

      if (ctx->coreiotMqtt) ctx->coreiotMqtt->publish(responseTopic.c_str(), params ? "true" : "false");

      Serial.println("setValueLED: Calling Webserver_sendata");
      Webserver_sendata(ctx, "{\"led\":\"" + String(params ? "ON" : "OFF") + "\"}");
      return;
    }

    if (strcmp(method, "getValuePump") == 0) {
      const String requestId = topicStr.substring(topicStr.lastIndexOf('/') + 1);
      const String responseTopic = "v1/devices/me/rpc/response/" + requestId;

      bool pumpOn = false;
      xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
      pumpOn = ctx->pumpManualState;
      xSemaphoreGive(ctx->mutexContext);

      if (ctx->coreiotMqtt) ctx->coreiotMqtt->publish(responseTopic.c_str(), pumpOn ? "true" : "false");
      return;
    }

    if (strcmp(method, "setValuePump") == 0) {
      bool params = false;
      if (doc["params"].is<bool>()) {
        params = doc["params"].as<bool>();
      } else {
        params = (doc["params"].as<String>() == "true" || doc["params"].as<String>() == "1" || doc["params"].as<String>() == "ON");
      }

      const String requestId = topicStr.substring(topicStr.lastIndexOf('/') + 1);
      const String responseTopic = "v1/devices/me/rpc/response/" + requestId;

      Serial.printf("setValuePump: params=%s\n", params ? "true" : "false");
      Serial.println("setValuePump: Entered setValuePump");
      Serial.println("setValuePump: params=" + String(params));

      xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
      ctx->pumpManualOverride = true;
      ctx->pumpManualState = params;
      xSemaphoreGive(ctx->mutexContext);

      Serial.println("setValuePump: Published pumpManualState");

      if (ctx->coreiotMqtt) ctx->coreiotMqtt->publish(responseTopic.c_str(), params ? "true" : "false");

      Serial.println("setValuePump: Published responseTopic");

      Webserver_sendata(ctx, "{\"pump_state\":\"" + String(params ? "ON" : "OFF") + "\"}");
      Serial.println("setValuePump: Called Webserver_sendata");
      return;
    }

    if (strcmp(method, "getValueMode") == 0) {
      const String requestId = topicStr.substring(topicStr.lastIndexOf('/') + 1);
      const String responseTopic = "v1/devices/me/rpc/response/" + requestId;

      int modeInt = 0;
      String modeStr = "AUTO";
      xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
      modeInt = ctx->pumpMode;
      modeStr = (modeInt == 1) ? "MANUAL" : "AUTO";
      xSemaphoreGive(ctx->mutexContext);

      if (ctx->coreiotMqtt) ctx->coreiotMqtt->publish(responseTopic.c_str(), modeStr.c_str());
      return;
    }

    if (strcmp(method, "setValueMode") == 0) {
      const char* params = doc["params"] | "AUTO";
      const String requestId = topicStr.substring(topicStr.lastIndexOf('/') + 1);
      const String responseTopic = "v1/devices/me/rpc/response/" + requestId;

      String modeStr = String(params);
      Serial.printf("setValueMode: params=%s\n", modeStr.c_str());

      if (modeStr != "AUTO" && modeStr != "MANUAL") {
        modeStr = "AUTO";
      }

      xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
      if (modeStr == "MANUAL") {
        ctx->pumpMode = 1;
        ctx->pumpController = "MANUAL";
        ctx->pumpManualOverride = true;
      } else {
        ctx->pumpMode = 0;
        ctx->pumpController = "AUTO";
        ctx->pumpManualOverride = false;
      }
      xSemaphoreGive(ctx->mutexContext);

      if (ctx->coreiotMqtt) ctx->coreiotMqtt->publish(responseTopic.c_str(), modeStr.c_str());

      Serial.println("setValueMode: Calling Webserver_sendata");
      Webserver_sendata(ctx, "{\"pump_mode\":\"" + modeStr + "\", \"pump_controller\":\"" + (modeStr == "MANUAL" ? "MANUAL" : "AUTO") + "\"}");
      return;
    } });

  unsigned long lastPublish = 0;

  while (1)
  {
    if (!ctx->coreiotMqtt->connected())
    {
      if (!mqttReconnect(ctx))
      {
        vTaskDelay(pdMS_TO_TICKS(1000));
        continue;
      }

      // Subscribe after connect (best-effort)
      if (ctx->mutexMqtt)
      {
        xSemaphoreTake(ctx->mutexMqtt, portMAX_DELAY);
      }
      ctx->coreiotMqtt->subscribe("v1/devices/me/rpc/request/+");
      if (ctx->mutexMqtt)
      {
        xSemaphoreGive(ctx->mutexMqtt);
      }
    }

    if (ctx->mutexMqtt)
    {
      xSemaphoreTake(ctx->mutexMqtt, portMAX_DELAY);
    }
    ctx->coreiotMqtt->loop();
    if (ctx->mutexMqtt)
    {
      xSemaphoreGive(ctx->mutexMqtt);
    }

    if (millis() - lastPublish >= 10000)
    {
      lastPublish = millis();

      float t = 0.0f;
      float h = 0.0f;
      float soil = 0.0f;
      int lcd = 1;
      bool pumpState = false;

      xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
      t = ctx->temperature;
      h = ctx->humidity;
      soil = ctx->soilMoisture;
      lcd = ctx->lcdState;
      pumpState = ctx->pumpManualState;
      xSemaphoreGive(ctx->mutexContext);

      const String statusStr = statusTextFromState(lcd);

      // Coordinates are fixed demo values (from frontend); keep them local to avoid globals.
      constexpr float deviceLatitude = 10.880018f;
      constexpr float deviceLongitude = 106.806336f;

      const String payload = "{\"temperature\":" + String(t) +
                             ",\"humidity\":" + String(h) +
                             ",\"soil_moisture\":" + String(soil) +
                             ",\"system_status\":\"" + statusStr + "\"" +
                             ",\"pump_state\":\"" + String(pumpState ? "ON" : "OFF") + "\"" +
                             ",\"lat\":" + String(deviceLatitude, 6) +
                             ",\"long\":" + String(deviceLongitude, 6) + "}";

      if (ctx->mutexMqtt)
      {
        xSemaphoreTake(ctx->mutexMqtt, portMAX_DELAY);
      }
      ctx->coreiotMqtt->publish("v1/devices/me/telemetry", payload.c_str());
      if (ctx->mutexMqtt)
      {
        xSemaphoreGive(ctx->mutexMqtt);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

