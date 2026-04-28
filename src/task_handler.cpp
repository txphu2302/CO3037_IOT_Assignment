#include <task_handler.h>
#include "global.h"

void handleWebSocketMessage(String message)
{
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
                pump_mode = 1;
                pump_ap_manual_override = true;
            }
            else
            {
                pump_mode = 0;
                pump_ap_manual_override = false;
            }
        }
        else if (action == "toggle_manual")
        {
            pump_mode = 1;
            pump_ap_manual_override = true;
            if (doc["value"].containsKey("state"))
            {
                pump_ap_manual_state = doc["value"]["state"].as<bool>();
            }
            else
            {
                pump_ap_manual_state = !pump_ap_manual_state;
            }
        }
        else if (action == "set_auto")
        {
            if (doc["value"].containsKey("threshold"))
            {
                int threshold = doc["value"]["threshold"].as<int>();
                if (threshold < 0) threshold = 0;
                if (threshold > 100) threshold = 100;
                pump_auto_threshold = threshold;
            }
            if (doc["value"].containsKey("hysteresis"))
            {
                int hysteresis = doc["value"]["hysteresis"].as<int>();
                if (hysteresis < 1) hysteresis = 1;
                if (hysteresis > 30) hysteresis = 30;
                pump_auto_hysteresis = hysteresis;
            }
            // Arm AUTO only when user explicitly saves AUTO config.
            pump_auto_armed = true;
        }
        else if (action == "set_schedule")
        {
            if (doc["value"].containsKey("enabled"))
            {
                pump_schedule_enabled = doc["value"]["enabled"].as<bool>();
            }
            if (doc["value"].containsKey("duration"))
            {
                int duration = doc["value"]["duration"].as<int>();
                if (duration < 1) duration = 1;
                if (duration > 3600) duration = 3600;
                pump_schedule_duration_sec = duration;
            }

            if (doc["value"].containsKey("time"))
            {
                String hhmm = doc["value"]["time"].as<String>();
                int sep = hhmm.indexOf(':');
                if (sep > 0)
                {
                    int h = hhmm.substring(0, sep).toInt();
                    int m = hhmm.substring(sep + 1).toInt();
                    if (h >= 0 && h <= 23) pump_schedule_hour = h;
                    if (m >= 0 && m <= 59) pump_schedule_minute = m;
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
            WIFI_SSID_NEW = WIFI_SSID;
            WIFI_PASS_NEW = WIFI_PASS;
        }

        // Nếu form gửi từ AP Mode (chỉ điền WiFi), giữ nguyên MQTT cũ
        if (CORE_IOT_TOKEN_NEW == "") {
            CORE_IOT_TOKEN_NEW  = CORE_IOT_TOKEN;
            CORE_IOT_SERVER_NEW = CORE_IOT_SERVER;
            CORE_IOT_PORT_NEW   = CORE_IOT_PORT;
        }

        Serial.println("📥 Nhận cấu hình từ WebSocket:");
        Serial.println("SSID: "   + WIFI_SSID_NEW);
        Serial.println("PASS: "   + WIFI_PASS_NEW);
        Serial.println("TOKEN: "  + CORE_IOT_TOKEN_NEW);
        Serial.println("SERVER: " + CORE_IOT_SERVER_NEW);
        Serial.println("PORT: "   + CORE_IOT_PORT_NEW);

        // Phản hồi lại client trước khi restart
        String msg = "{\"status\":\"ok\",\"page\":\"setting_saved\"}";
        ws.textAll(msg);

        // Lưu cấu hình → ESP sẽ restart bên trong hàm này
        Save_info_File(WIFI_SSID_NEW, WIFI_PASS_NEW, CORE_IOT_TOKEN_NEW, CORE_IOT_SERVER_NEW, CORE_IOT_PORT_NEW);
    }
}
