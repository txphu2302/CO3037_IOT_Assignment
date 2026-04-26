#include "coreiot.h"
#include "task_webserver.h"

// ----------- CONFIGURE THESE! -----------
const char* coreIOT_Server = "10.235.76.226";  
const char* coreIOT_Token = "g7drm1amhd3dchr379xu";   // Device Access Token
const int   mqttPort = 1883;
// ----------------------------------------

WiFiClient espClient;
PubSubClient client(espClient);

void coreiot_publish_attribute(String key, bool value) {
    if (client.connected()) {
        String payload = "{\"" + key + "\":" + (value ? "true" : "false") + "}";
        client.publish("v1/devices/me/attributes", payload.c_str());
        Serial.println("📤 Đã đồng bộ lên CoreIOT: " + payload);
    }
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect (username=token, password=empty)
    //if (client.connect("ESP32Client", coreIOT_Token, NULL)) {
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), CORE_IOT_TOKEN.c_str(), NULL)) {
        
      Serial.println("connected to CoreIOT Server!");
      client.subscribe("v1/devices/me/rpc/request/+");
      Serial.println("Subscribed to v1/devices/me/rpc/request/+");

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("] ");

  // Allocate a temporary buffer for the message
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  Serial.print("Payload: ");
  Serial.println(message);

  // Parse JSON
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  const char* method = doc["method"];
  
  if (strcmp(method, "getValueLED") == 0) {
      // CoreIOT hỏi trạng thái hiện tại của LED
      String topicStr = String(topic);
      String requestId = topicStr.substring(topicStr.lastIndexOf('/') + 1);
      String responseTopic = "v1/devices/me/rpc/response/" + requestId;
      
      String responsePayload = led_ap_manual_state ? "true" : "false";
      client.publish(responseTopic.c_str(), responsePayload.c_str());
  } 
  else if (strcmp(method, "setValueLED") == 0) {
      // CoreIOT ra lệnh bật/tắt LED
      bool params = doc["params"];
      
      // Cập nhật trạng thái cho hệ thống
      led_ap_manual_override = true;
      led_ap_manual_state = params;

      // Phản hồi xác nhận lại cho CoreIOT
      String topicStr = String(topic);
      String requestId = topicStr.substring(topicStr.lastIndexOf('/') + 1);
      String responseTopic = "v1/devices/me/rpc/response/" + requestId;
      client.publish(responseTopic.c_str(), params ? "true" : "false");

      Serial.println(params ? "Device turned ON from CoreIOT." : "Device turned OFF from CoreIOT.");
      
      // Đồng bộ xuống tất cả các màn hình WebServer
      String wsMsg = "{\"led\":\"" + String(params ? "ON" : "OFF") + "\"}";
      Webserver_sendata(wsMsg);
  } else {
    Serial.print("Unknown method: ");
    Serial.println(method);
  }
}


void setup_coreiot(){

  //Serial.print("Connecting to WiFi...");
  //WiFi.begin(wifi_ssid, wifi_password);
  //while (WiFi.status() != WL_CONNECTED) {
  
  // while (isWifiConnected == false) {
  //   delay(500);
  //   Serial.print(".");
  // }

  while(1){
    if (xSemaphoreTake(xBinarySemaphoreInternet, portMAX_DELAY)) {
      break;
    }
    delay(500);
    Serial.print(".");
  }


  Serial.println(" Connected!");

  client.setServer(CORE_IOT_SERVER.c_str(), CORE_IOT_PORT.toInt());
  client.setCallback(callback);

}

void coreiot_task(void *pvParameters){

    setup_coreiot();

    unsigned long lastPublish = 0;
    while(1){

        if (!client.connected()) {
            reconnect();
        }
        client.loop();

        if (millis() - lastPublish >= 10000) {
            lastPublish = millis();
            
            // Tính toán System Status giống như Local WebServer
            String statusStr = "Normal";
            if (glob_temperature >= 30.0 || glob_humidity >= 70.0) {
                statusStr = "Critical";
            } else if (glob_temperature >= 25.0 || glob_humidity >= 50.0) {
                statusStr = "Warning";
            }

            // Mở rộng Payload
            String payload = "{\"temperature\":" + String(glob_temperature) +  
                             ",\"humidity\":" + String(glob_humidity) + 
                             ",\"soil_moisture\":" + String(glob_soil_moisture) + 
                             ",\"system_status\":\"" + statusStr + "\"}";
            
            client.publish("v1/devices/me/telemetry", payload.c_str());
            
            Serial.println("Published payload: " + payload);
        }
        
        vTaskDelay(10 / portTICK_PERIOD_MS);  // Yield to FreeRTOS (chạy cực mượt, không bị block)
    }
}