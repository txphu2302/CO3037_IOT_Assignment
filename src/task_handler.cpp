#include <task_handler.h>

void handleWebSocketMessage(String message)
{
    Serial.println(message);
    StaticJsonDocument<256> doc;

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
    else if (doc["page"] == "setting")
    {
        String WIFI_SSID_NEW = doc["value"]["ssid"].as<String>();
        String WIFI_PASS_NEW = doc["value"]["password"].as<String>();
        String CORE_IOT_TOKEN_NEW = doc["value"]["token"].as<String>();
        String CORE_IOT_SERVER_NEW = doc["value"]["server"].as<String>();
        String CORE_IOT_PORT_NEW = doc["value"]["port"].as<String>();

        // Nếu form gửi từ STA Mode (chỉ điền MQTT), giữ nguyên WiFi cũ
        if (WIFI_SSID_NEW == "STA_MODE_KEEP") {
            WIFI_SSID_NEW = WIFI_SSID;
            WIFI_PASS_NEW = WIFI_PASS;
        }

        // Nếu form gửi từ AP Mode (chỉ điền WiFi), giữ nguyên MQTT cũ
        if (CORE_IOT_TOKEN_NEW == "") {
            CORE_IOT_TOKEN_NEW = CORE_IOT_TOKEN;
            CORE_IOT_SERVER_NEW = CORE_IOT_SERVER;
            CORE_IOT_PORT_NEW = CORE_IOT_PORT;
        }

        Serial.println("📥 Nhận cấu hình từ WebSocket:");
        Serial.println("SSID: " + WIFI_SSID_NEW);
        Serial.println("PASS: " + WIFI_PASS_NEW);
        Serial.println("TOKEN: " + CORE_IOT_TOKEN_NEW);
        Serial.println("SERVER: " + CORE_IOT_SERVER_NEW);
        Serial.println("PORT: " + CORE_IOT_PORT_NEW);

        // Phản hồi lại client (tùy chọn) trước khi restart
        String msg = "{\"status\":\"ok\",\"page\":\"setting_saved\"}";
        ws.textAll(msg);

        // 👉 Gọi hàm lưu cấu hình
        Save_info_File(WIFI_SSID_NEW, WIFI_PASS_NEW, CORE_IOT_TOKEN_NEW, CORE_IOT_SERVER_NEW, CORE_IOT_PORT_NEW);
    }
}
