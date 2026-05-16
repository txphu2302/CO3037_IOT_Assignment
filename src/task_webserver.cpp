#include "task_webserver.h"
#include "coreiot.h"

#include <WiFi.h>

static void connectWSV(SharedContext *ctx)
{
  if (!ctx || !ctx->webServer || !ctx->webSocket)
  {
    return;
  }

  ctx->webSocket->onEvent([ctx](AsyncWebSocket *server, AsyncWebSocketClient *client,
                                AwsEventType type, void *arg, uint8_t *data, size_t len)
                          {
    (void)server;
    if (type == WS_EVT_CONNECT) {
      Serial.printf("WebSocket client #%u connected from %s\n",
                    client->id(), client->remoteIP().toString().c_str());
      return;
    }
    if (type == WS_EVT_DISCONNECT) {
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      return;
    }
    if (type != WS_EVT_DATA) {
      return;
    }

    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->opcode != WS_TEXT) {
      return;
    }

    String message;
    message += String((char *)data).substring(0, len);
    handleWebSocketMessage(ctx, message); });

  ctx->webServer->addHandler(ctx->webSocket);

  ctx->webServer->on("/", HTTP_GET, [ctx](AsyncWebServerRequest *request)
                     {
    const IPAddress localIp = request->client()->localIP();

    // Serve page by interface that received the request:
    // - AP client hitting 192.168.4.1 => AP page
    // - STA/LAN client hitting station IP => STA page
    if (localIp == WiFi.softAPIP()) {
      request->send(LittleFS, "/AP.html", "text/html");
      return;
    }

    if ((WiFi.status() == WL_CONNECTED) && (localIp == WiFi.localIP())) {
      request->send(LittleFS, "/STA.html", "text/html");
      return;
    }

    // Fallback: keep AP setup reachable even in ambiguous states.
    request->send(LittleFS, "/AP.html", "text/html"); });

  ctx->webServer->on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
                     { request->send(LittleFS, "/script.js", "application/javascript"); });
  ctx->webServer->on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request)
                     { request->send(LittleFS, "/styles.css", "text/css"); });
  ctx->webServer->on("/chart.js", HTTP_GET, [](AsyncWebServerRequest *request)
                     { request->send(LittleFS, "/chart.js", "application/javascript"); });

  // Status endpoint for UI (STA/AP): WiFi + MQTT + WebSocket clients
  ctx->webServer->on("/api/status", HTTP_GET, [ctx](AsyncWebServerRequest *request)
                     {
    String mode = "UNKNOWN";
    const wifi_mode_t m = WiFi.getMode();
    if (m == WIFI_AP) mode = "AP";
    else if (m == WIFI_STA) mode = "STA";
    else if (m == WIFI_AP_STA) mode = "AP+STA";

    const bool wifiConnected = (WiFi.status() == WL_CONNECTED);
    const int rssi = wifiConnected ? WiFi.RSSI() : 0;
    const String ip = wifiConnected ? WiFi.localIP().toString() : String("");
    const String apIp = WiFi.softAPIP().toString();

    bool mqttConnected = false;
    if (ctx->coreiotMqtt) {
      if (ctx->mutexMqtt) xSemaphoreTake(ctx->mutexMqtt, portMAX_DELAY);
      mqttConnected = ctx->coreiotMqtt->connected();
      if (ctx->mutexMqtt) xSemaphoreGive(ctx->mutexMqtt);
    }

    const int wsClients = (ctx->webSocket) ? (int)ctx->webSocket->count() : 0;

    String json = "{";
    json += "\"mode\":\"" + mode + "\"";
    json += ",\"wifi_connected\":" + String(wifiConnected ? "true" : "false");
    json += ",\"wifi_rssi\":" + String(rssi);
    json += ",\"ip\":\"" + ip + "\"";
    json += ",\"ap_ip\":\"" + apIp + "\"";
    json += ",\"mqtt_connected\":" + String(mqttConnected ? "true" : "false");
    json += ",\"ws_clients\":" + String(wsClients);
    json += "}";
    request->send(200, "application/json", json); });

  ctx->webServer->on("/toggle-led", HTTP_GET, [ctx](AsyncWebServerRequest *request)
                     {
    xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
    ctx->ledManualOverride = true;
    ctx->ledManualState = !ctx->ledManualState;
    const bool nowOn = ctx->ledManualState;
    xSemaphoreGive(ctx->mutexContext);

    request->send(200, "text/plain", nowOn ? "ON" : "OFF");

    // Sync to other web clients
    Webserver_sendata(ctx, "{\"led\":\"" + String(nowOn ? "ON" : "OFF") + "\"}");

    // Sync to CoreIOT attributes (best-effort)
    coreiot_publish_attribute(ctx, "ledState", nowOn); });

  ctx->webServer->on("/toggle-neo", HTTP_GET, [ctx](AsyncWebServerRequest *request)
                     {
    xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
    if (request->hasParam("color")) {
      String colorHex = request->getParam("color")->value();
      if (colorHex.startsWith("#")) {
        long number = strtol(&colorHex[1], NULL, 16);
        ctx->neoManualR = number >> 16;
        ctx->neoManualG = number >> 8 & 0xFF;
        ctx->neoManualB = number & 0xFF;
      }
      ctx->neoManualState = true;
    } else {
      ctx->neoManualState = !ctx->neoManualState;
    }
    ctx->neoManualOverride = true;
    const bool nowOn = ctx->neoManualState;
    xSemaphoreGive(ctx->mutexContext);

    request->send(200, "text/plain", nowOn ? "ON" : "OFF"); });

  ctx->webServer->on("/toggle-pump", HTTP_GET, [ctx](AsyncWebServerRequest *request)
                     {
    bool target = false;
    xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
    ctx->pumpMode = 1; // MANUAL
    ctx->pumpManualOverride = true;
    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      ctx->pumpManualState = state.equalsIgnoreCase("ON");
    } else {
      ctx->pumpManualState = !ctx->pumpManualState;
    }
    target = ctx->pumpManualState;
    xSemaphoreGive(ctx->mutexContext);

    request->send(200, "text/plain", target ? "ON" : "OFF");

    // Sync to other web clients
    Webserver_sendata(ctx, "{\"pump_state\":\"" + String(target ? "ON" : "OFF") + "\"}");

    // Sync to CoreIOT attributes (best-effort)
    coreiot_publish_attribute(ctx, "pumpState", target); });

  // Kick off async scan (non-blocking, returns immediately)
  ctx->webServer->on("/scan/start", HTTP_GET, [](AsyncWebServerRequest *request)
                     {
    WiFi.scanNetworks(true); // async
    request->send(200, "application/json", "{\"status\":\"scanning\"}"); });

  // Poll for scan results
  ctx->webServer->on("/scan/result", HTTP_GET, [](AsyncWebServerRequest *request)
                     {
    int16_t n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) {
      request->send(200, "application/json", "{\"status\":\"scanning\"}");
      return;
    }
    if (n == WIFI_SCAN_FAILED || n < 0) {
      WiFi.scanDelete();
      request->send(200, "application/json", "{\"status\":\"error\"}");
      return;
    }
    String json = "{\"status\":\"ready\",\"networks\":[";
    for (int i = 0; i < n; ++i) {
      if (i) json += ",";
      String enc = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "false" : "true";
      json += "{\"ssid\":\"" + WiFi.SSID(i) +
              "\",\"rssi\":" + String(WiFi.RSSI(i)) +
              ",\"secure\":" + enc + "}";
    }
    json += "]}";
    WiFi.scanDelete();
    request->send(200, "application/json", json); });

  ctx->webServer->begin();
  ElegantOTA.begin(ctx->webServer);
  ctx->webServerRunning = true;
}

void Webserver_sendata(SharedContext *ctx, const String &data)
{
  Serial.printf("Webserver_sendata: data=%s\n", data.c_str());
  if (!ctx || !ctx->webSocket)
  {
    Serial.println("Webserver_sendata: ctx or webSocket is null");
    return;
  }

  int clientCount = ctx->webSocket->count();
  Serial.printf("Webserver_sendata: clientCount=%d\n", clientCount);

  if (clientCount > 0)
  {
    ctx->webSocket->textAll(data);
    Serial.println("Webserver_sendata: Sent to clients");
  }
  else
  {
    Serial.println("Webserver_sendata: No clients connected");
  }
}

void Webserver_stop(SharedContext *ctx)
{
  if (!ctx || !ctx->webServer || !ctx->webSocket)
  {
    return;
  }

  ctx->webSocket->closeAll();
  ctx->webServer->end();
  ctx->webServerRunning = false;
}

void Webserver_reconnect(SharedContext *ctx)
{
  if (!ctx)
  {
    return;
  }

  if (!ctx->webServerRunning)
  {
    connectWSV(ctx);
  }

  ElegantOTA.loop();
}
