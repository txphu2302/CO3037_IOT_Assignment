#include "temp_humi_monitor.h"

#include "task_webserver.h"
#include "risk_label.h"
#include "serial_log.h"

static const char* statusText(int state) {
    if (state == 3) return "Critical";
    if (state == 2) return "Warning";
    return "Normal";
}

void temp_humi_monitor(void *pvParameters){

    DHT20 dht20;
    LiquidCrystal_I2C lcd(0x27,16,2);
    SharedContext* ctx = (SharedContext*)pvParameters;

    Wire.begin(11, 12);
    Serial.begin(115200);
    dht20.begin();

    lcd.begin();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("IOT ASSIGNMENT");
    delay(5000);
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("Status: Normal");

    while (1){
        /* code */
        
        dht20.read();
        // Reading temperature in Celsius
        float temperature = dht20.getTemperature();
        // Reading humidity
        float humidity = dht20.getHumidity();

        

        // Check if any reads failed and exit early
        if (isnan(temperature) || isnan(humidity)) {
            serialLogLock(ctx);
            Serial.println("Failed to read from DHT sensor!");
            serialLogUnlock(ctx);
            temperature = humidity =  -1;
            //return;
        }

        if (pvParameters != NULL) {
            SharedContext* ctx = (SharedContext*)pvParameters;
            xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
            ctx->temperature = temperature;
            ctx->humidity = humidity;
            
            // Task 1 Logic (temperature bands)
            const int newLedState = risk_led_state_from_temperature(temperature);
            if (newLedState != ctx->ledState) {
                ctx->ledState = newLedState;
                xSemaphoreGive(ctx->semLEDUpdate);
            }

            // Task 2 Logic (humidity bands)
            const int newNeoState = risk_neo_state_from_humidity(humidity);
            if (newNeoState != ctx->neoState) {
                ctx->neoState = newNeoState;
                xSemaphoreGive(ctx->semNeoUpdate);
            }

            // LCD status logic: worst case between temperature and humidity
            const int newLcdState = risk_final_label(temperature, humidity);
            if (newLcdState != ctx->lcdState) {
                ctx->lcdState = newLcdState;
                xSemaphoreGive(ctx->semLCDUpdate);
            }
            
            xSemaphoreGive(ctx->mutexContext);
        }

        // Print the results (whole line under mutex — avoids interleave with TinyML Serial)
        serialLogLock(ctx);
        Serial.print("Humidity: ");
        Serial.print(humidity);
        Serial.print("%  Temperature: ");
        Serial.print(temperature);
        Serial.println("°C");
        serialLogUnlock(ctx);

        String statusStr = "Unknown";
        if (pvParameters != NULL) {
            SharedContext* ctx = (SharedContext*)pvParameters;
            xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
            statusStr = statusText(ctx->lcdState);
            xSemaphoreGive(ctx->mutexContext);
        }

        // Gửi dữ liệu qua WebSocket cho frontend cập nhật
        String jsonStr = "{\"temperature\":" + String(temperature, 2) + 
                         ",\"humidity\":" + String(humidity, 2) + 
                         ",\"system_status\":\"" + statusStr + "\"}";
        if (pvParameters != NULL) {
            Webserver_sendata((SharedContext*)pvParameters, jsonStr);
        }

        lcd.setCursor(0,0);
        lcd.print("T:");
        lcd.print(temperature, 1);
        lcd.print((char)223);
        lcd.print("C H:");
        lcd.print(humidity, 1);
        lcd.print("% "); 

        // Print the status of the system on LCD
        if (pvParameters != NULL) {
            SharedContext* ctx = (SharedContext*)pvParameters;
            if (xSemaphoreTake(ctx->semLCDUpdate, 0) == pdTRUE) {
                lcd.setCursor(0,1);
                lcd.print("Status: ");
                lcd.print("        ");
                lcd.setCursor(8,1);
                lcd.print(statusText(ctx->lcdState));
            }
        }
        
        vTaskDelay(5000);
    }
    
}
