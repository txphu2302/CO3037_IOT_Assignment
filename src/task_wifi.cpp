#include "task_wifi.h"

void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(String(SSID_AP), String(PASS_AP));
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void startSTA() {
  if (WIFI_SSID.isEmpty()) {
    vTaskDelete(NULL);
  }

  // Keep AP alive while connecting STA so client devices do not get dropped
  // abruptly and auto-switch to another remembered Wi-Fi network.
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(String(SSID_AP), String(PASS_AP));

  // Xóa credentials cache trong NVS để tránh kết nối nhầm mạng cũ
  WiFi.disconnect(true, true);
  delay(100);

  Serial.println("📡 Đang kết nối tới: " + WIFI_SSID);

  if (WIFI_PASS.isEmpty()) {
    WiFi.begin(WIFI_SSID.c_str());
  } else {
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
  }


  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  Serial.println("");
  Serial.print("✅ Đã kết nối WiFi! IP Address: ");
  Serial.println(WiFi.localIP());

  // Give a semaphore here
  xSemaphoreGive(xBinarySemaphoreInternet);
}

bool Wifi_reconnect() {
  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    return true;
  }
  startSTA();
  return false;
}
