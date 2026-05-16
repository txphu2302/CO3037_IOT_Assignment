#include <task_handler.h>
#include "global.h"
#include "task_check_info.h"
#include "task_webserver.h"
#include "coreiot.h"

void handleWebSocketMessage(SharedContext *ctx, const String &message)
{
    if (!ctx) {
        return;
    }
    Serial.println(message);
    // Buffer 512 để tránh tràn khi SSID/Pass dài
    StaticJsonDocument<512> doc;

    DeserializationError error = deserializeJson(doc, message);
    if (error)
    {
        Serial.println("❌ Lỗi parse JSON!");
        return;
    }
    JsonObject value = doc["value"];
    if (doc["page"] == "device")
    {
        if (!value.containsKey("gpio") || !value.containsKey("status"))
        {
            Serial.println("⚠️ JSON thiếu thông tin gpio hoặc status");
            return;
        }

        int gpio = value["gpio"];
        String status = value["status"].as<String>();

        Serial.printf("⚙️ Điều khiển GPIO %d → %s\n", gpio, status.c_str());
        pinMode(gpio, OUTPUT);
        if (status.equalsIgnoreCase("ON"))
        {
            digitalWrite(gpio, HIGH);
            Serial.printf("🔆 GPIO %d ON\n", gpio);
        }
        else if (status.equalsIgnoreCase("OFF"))
        {
            digitalWrite(gpio, LOW);
            Serial.printf("💤 GPIO %d OFF\n", gpio);
        }
    }
    else if (doc["page"] == "pump")
    {
        String action = doc["action"] | "";

        if (action == "set_mode")
        {
            String mode = doc["value"]["mode"].as<String>();
            if (mode == "MANUAL")
            {
                xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
                ctx->pumpMode = 1;
                ctx->pumpManualOverride = true;
                xSemaphoreGive(ctx->mutexContext);
            }
            else
            {
                xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
                ctx->pumpMode = 0;
                ctx->pumpManualOverride = false;
                xSemaphoreGive(ctx->mutexContext);
            }

            // Sync to other web clients
            Webserver_sendata(ctx, "{\"pump_mode\":\"" + mode + "\"}");

            // Sync to CoreIOT attributes (best-effort)
            coreiot_publish_attribute(ctx, "modeState", mode == "MANUAL");
        }
        else if (action == "toggle_manual")
        {
            xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
            ctx->pumpMode = 1;
            ctx->pumpManualOverride = true;
            xSemaphoreGive(ctx->mutexContext);
            if (doc["value"].containsKey("state"))
            {
                xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
                ctx->pumpManualState = doc["value"]["state"].as<bool>();
                xSemaphoreGive(ctx->mutexContext);
            }
            else
            {
                xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
                ctx->pumpManualState = !ctx->pumpManualState;
                xSemaphoreGive(ctx->mutexContext);
            }
        }
        else if (action == "set_auto")
        {
            if (doc["value"].containsKey("threshold"))
            {
                int threshold = doc["value"]["threshold"].as<int>();
                if (threshold < 0) threshold = 0;
                if (threshold > 100) threshold = 100;
                xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
                ctx->pumpAutoThreshold = threshold;
                xSemaphoreGive(ctx->mutexContext);
            }
            if (doc["value"].containsKey("hysteresis"))
            {
                int hysteresis = doc["value"]["hysteresis"].as<int>();
                if (hysteresis < 1) hysteresis = 1;
                if (hysteresis > 30) hysteresis = 30;
                xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
                ctx->pumpAutoHysteresis = hysteresis;
                xSemaphoreGive(ctx->mutexContext);
            }
            // Arm AUTO only when user explicitly saves AUTO config.
            xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
            ctx->pumpAutoArmed = true;
            xSemaphoreGive(ctx->mutexContext);
        }
        else if (action == "set_schedule")
        {
            if (doc["value"].containsKey("enabled"))
            {
                xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
                ctx->pumpScheduleEnabled = doc["value"]["enabled"].as<bool>();
                xSemaphoreGive(ctx->mutexContext);
            }
            if (doc["value"].containsKey("duration"))
            {
                int duration = doc["value"]["duration"].as<int>();
                if (duration < 1) duration = 1;
                if (duration > 3600) duration = 3600;
                xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
                ctx->pumpScheduleDurationSec = duration;
                xSemaphoreGive(ctx->mutexContext);
            }

            if (doc["value"].containsKey("time"))
            {
                String hhmm = doc["value"]["time"].as<String>();
                int sep = hhmm.indexOf(':');
                if (sep > 0)
                {
                    int h = hhmm.substring(0, sep).toInt();
                    int m = hhmm.substring(sep + 1).toInt();
                    xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
                    if (h >= 0 && h <= 23) ctx->pumpScheduleHour = h;
                    if (m >= 0 && m <= 59) ctx->pumpScheduleMinute = m;
                    xSemaphoreGive(ctx->mutexContext);
                }
            }
        }
    }
    else if (doc["page"] == "setting")
    {
        String WIFI_SSID_NEW      = doc["value"]["ssid"].as<String>();
        String WIFI_PASS_NEW      = doc["value"]["password"].as<String>();
        String CORE_IOT_TOKEN_NEW  = doc["value"]["token"].as<String>();
        String CORE_IOT_SERVER_NEW = doc["value"]["server"].as<String>();
        String CORE_IOT_PORT_NEW   = doc["value"]["port"].as<String>();

        // Nếu form gửi từ STA Mode (chỉ điền MQTT), giữ nguyên WiFi cũ
        if (WIFI_SSID_NEW == "STA_MODE_KEEP") {
            xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
            WIFI_SSID_NEW = ctx->wifiSsid;
            WIFI_PASS_NEW = ctx->wifiPass;
            xSemaphoreGive(ctx->mutexContext);
        }

        // Nếu form gửi từ AP Mode (chỉ điền WiFi), giữ nguyên MQTT cũ
        if (CORE_IOT_TOKEN_NEW == "") {
            xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
            CORE_IOT_TOKEN_NEW  = ctx->coreIotToken;
            CORE_IOT_SERVER_NEW = ctx->coreIotServer;
            CORE_IOT_PORT_NEW   = ctx->coreIotPort;
            xSemaphoreGive(ctx->mutexContext);
        }

        Serial.println("📥 Nhận cấu hình từ WebSocket:");
        Serial.println("SSID: "   + WIFI_SSID_NEW);
        Serial.println("PASS: "   + WIFI_PASS_NEW);
        Serial.println("TOKEN: "  + CORE_IOT_TOKEN_NEW);
        Serial.println("SERVER: " + CORE_IOT_SERVER_NEW);
        Serial.println("PORT: "   + CORE_IOT_PORT_NEW);

        // Phản hồi lại client trước khi restart
        String msg = "{\"status\":\"ok\",\"page\":\"setting_saved\"}";
        Webserver_sendata(ctx, msg);

        // Lưu cấu hình → ESP sẽ restart bên trong hàm này
        Save_info_File(ctx, WIFI_SSID_NEW, WIFI_PASS_NEW, CORE_IOT_TOKEN_NEW, CORE_IOT_SERVER_NEW, CORE_IOT_PORT_NEW);
    }
}
