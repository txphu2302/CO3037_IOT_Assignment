#include "task_webserver.h"
#include "coreiot.h"
#include <WiFi.h>


AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool webserver_isrunning = false;

void Webserver_sendata(String data) {
  if (ws.count() > 0) {
    ws.textAll(data); // Gửi đến tất cả client đang kết nối
    Serial.println("📤 Đã gửi dữ liệu qua WebSocket: " + data);
  } else {
    Serial.println("⚠️ Không có client WebSocket nào đang kết nối!");
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(),
                  client->remoteIP().toString().c_str());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;

    if (info->opcode == WS_TEXT) {
      String message;
      message += String((char *)data).substring(0, len);
      // parseJson(message, true);
      handleWebSocketMessage(message);
    }
  }
}

void connnectWSV() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
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
    request->send(LittleFS, "/AP.html", "text/html");
  });
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/script.js", "application/javascript");
  });
  server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/styles.css", "text/css");
  });
  server.on("/chart.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/chart.js", "application/javascript");
  });

  server.on("/toggle-led", HTTP_GET, [](AsyncWebServerRequest *request) {
    led_ap_manual_override = true;
    led_ap_manual_state = !led_ap_manual_state;
    request->send(200, "text/plain", led_ap_manual_state ? "ON" : "OFF");

    // Đồng bộ cho các tab Web khác
    String wsMsg =
        "{\"led\":\"" + String(led_ap_manual_state ? "ON" : "OFF") + "\"}";
    Webserver_sendata(wsMsg);

    // Đồng bộ lên CoreIOT
    coreiot_publish_attribute("ledState", led_ap_manual_state);
  });

  server.on("/toggle-neo", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("color")) {
      String colorHex = request->getParam("color")->value();
      if (colorHex.startsWith("#")) {
        long number = strtol(&colorHex[1], NULL, 16);
        neo_ap_color_r = number >> 16;
        neo_ap_color_g = number >> 8 & 0xFF;
        neo_ap_color_b = number & 0xFF;
      }
      // Nếu có gửi màu thì luôn luôn BẬT
      neo_ap_manual_state = true;
    } else {
      // Nếu không gửi màu thì TẮT/BẬT tuần tự
      neo_ap_manual_state = !neo_ap_manual_state;
    }
    neo_ap_manual_override = true;
    request->send(200, "text/plain", neo_ap_manual_state ? "ON" : "OFF");
  });

  server.on("/toggle-pump", HTTP_GET, [](AsyncWebServerRequest *request) {
    pump_mode = 1; // MANUAL
    pump_ap_manual_override = true;
    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      pump_ap_manual_state = state.equalsIgnoreCase("ON");
    } else {
      pump_ap_manual_state = !pump_ap_manual_state;
    }
    request->send(200, "text/plain", pump_ap_manual_state ? "ON" : "OFF");

    String wsMsg = "{\"pump_state\":\"" + String(pump_ap_manual_state ? "ON" : "OFF") +
                   "\",\"pump_mode\":\"MANUAL\",\"pump_controller\":\"MANUAL\"}";
    Webserver_sendata(wsMsg);
  });

  // Kick off async scan (non-blocking, returns immediately)
  server.on("/scan/start", HTTP_GET, [](AsyncWebServerRequest *request) {
    WiFi.scanNetworks(true); // true = async
    request->send(200, "application/json", "{\"status\":\"scanning\"}");
  });

  // Poll for scan results
  server.on("/scan/result", HTTP_GET, [](AsyncWebServerRequest *request) {
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
      if (i)
        json += ",";
      String enc =
          (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "false" : "true";
      json += "{\"ssid\":\"" + WiFi.SSID(i) +
              "\",\"rssi\":" + String(WiFi.RSSI(i)) + ",\"secure\":" + enc +
              "}";
    }
    json += "]}";
    WiFi.scanDelete(); // Giải phóng bộ nhớ
    request->send(200, "application/json", json);
  });

  server.begin();
  ElegantOTA.begin(&server);
  webserver_isrunning = true;
}

void Webserver_stop() {
  ws.closeAll();
  server.end();
  webserver_isrunning = false;
}

void Webserver_reconnect() {
  if (!webserver_isrunning) {
    connnectWSV();
  }
  ElegantOTA.loop();
}

