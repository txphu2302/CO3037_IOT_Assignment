#include "task_check_info.h"
#include "task_wifi.h"

void Load_info_File(SharedContext *ctx)
{
  if (!ctx)
  {
    return;
  }

  File file = LittleFS.open("/info.dat", "r");
  if (!file)
  {
    return;
  }

  DynamicJsonDocument doc(4096);
  const DeserializationError error = deserializeJson(doc, file);
  if (!error)
  {
    xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
    ctx->wifiSsid = doc["WIFI_SSID"] | "";
    ctx->wifiPass = doc["WIFI_PASS"] | "";
    ctx->coreIotToken = doc["CORE_IOT_TOKEN"] | "";
    ctx->coreIotServer = doc["CORE_IOT_SERVER"] | "";
    ctx->coreIotPort = doc["CORE_IOT_PORT"] | "";
    xSemaphoreGive(ctx->mutexContext);
  }

  file.close();
}

void Delete_info_File()
{
  if (LittleFS.exists("/info.dat"))
  {
    LittleFS.remove("/info.dat");
  }
  ESP.restart();
}

void Save_info_File(SharedContext *ctx, String wifiSsid, String wifiPass, String token, String server, String port)
{
  // Persist configuration; device restarts after saving.
  (void)ctx;

  DynamicJsonDocument doc(4096);
  doc["WIFI_SSID"] = wifiSsid;
  doc["WIFI_PASS"] = wifiPass;
  doc["CORE_IOT_TOKEN"] = token;
  doc["CORE_IOT_SERVER"] = server;
  doc["CORE_IOT_PORT"] = port;

  File configFile = LittleFS.open("/info.dat", "w");
  if (configFile)
  {
    serializeJson(doc, configFile);
    configFile.close();
  }

  ESP.restart();
}

bool check_info_File(SharedContext *ctx, bool check)
{
  if (!ctx)
  {
    return false;
  }

  if (!check)
  {
    if (!LittleFS.begin(true))
    {
      Serial.println("LittleFS init failed");
      return false;
    }
    Load_info_File(ctx);
  }

  xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
  const bool missingWifi = ctx->wifiSsid.isEmpty() && ctx->wifiPass.isEmpty();
  xSemaphoreGive(ctx->mutexContext);

  if (!check)
  {
    // Luôn phát AP ngay khi boot, dù có hay không có credentials.
    // User có thể vào 192.168.4.1 để cấu hình bất kỳ lúc nào.
    startAP();
  }

  if (missingWifi)
  {
    // Không có credentials → ở lại AP mode, không thử STA.
    return false;
  }

  // Có credentials → báo caller tiếp tục kết STA (song song với AP).
  return true;
}

