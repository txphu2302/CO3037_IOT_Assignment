#include "task_wifi.h"

void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(String(SSID_AP), String(PASS_AP));
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

static bool startSTA(SharedContext *ctx) {
  if (!ctx || ctx->wifiSsid.isEmpty()) {
    return false;
  }

  // AP đã được khởi động từ check_info_File → chỉ cần chuyển sang AP+STA.
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(String(SSID_AP), String(PASS_AP));

  // Xóa credentials cũ trong Flash để tránh tự kết mạng cũ.
  WiFi.disconnect(true, true);
  delay(100);

  Serial.println("Connecting WiFi SSID: " + ctx->wifiSsid);

  if (ctx->wifiPass.isEmpty()) {
    WiFi.begin(ctx->wifiSsid.c_str());
  } else {
    WiFi.begin(ctx->wifiSsid.c_str(), ctx->wifiPass.c_str());
  }

  // Timeout 15 giây — tránh hang mãi nếu credentials sai hoặc mạng yếu.
  const unsigned long connectStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - connectStart > 15000UL) {
      Serial.println("STA connect timeout (15s), staying on AP mode.");
      WiFi.disconnect(false, false);
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());

  if (ctx->semInternetConnected) {
    xSemaphoreGive(ctx->semInternetConnected);
  }
  return true;
}

bool Wifi_reconnect(SharedContext *ctx) {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  return startSTA(ctx);
}
