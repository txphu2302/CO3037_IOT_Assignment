#include "task_wifi.h"

void startAP()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP(String(SSID_AP), String(PASS_AP));
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

static bool startSTA(SharedContext *ctx)
{
  if (!ctx || ctx->wifiSsid.isEmpty())
  {
    return false;
  }

  // Keep AP alive while connecting STA so client devices do not get dropped abruptly.
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(String(SSID_AP), String(PASS_AP));

  // Clear cached credentials to avoid connecting to an old network.
  WiFi.disconnect(true, true);
  delay(100);

  Serial.println("Connecting WiFi SSID: " + ctx->wifiSsid);

  if (ctx->wifiPass.isEmpty())
  {
    WiFi.begin(ctx->wifiSsid.c_str());
  }
  else
  {
    WiFi.begin(ctx->wifiSsid.c_str(), ctx->wifiPass.c_str());
  }

  while (WiFi.status() != WL_CONNECTED)
  {
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());

  if (ctx->semInternetConnected)
  {
    xSemaphoreGive(ctx->semInternetConnected);
  }
  return true;
}

bool Wifi_reconnect(SharedContext *ctx)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return true;
  }
  return startSTA(ctx);
}

