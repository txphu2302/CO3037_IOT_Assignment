#include "temp_humi_monitor.h"
#include "task_webserver.h"
DHT20 dht20;
LiquidCrystal_I2C lcd(0x27,16,2);

static const char* statusText(int state) {
    if (state == 3) return "Critical";
    if (state == 2) return "Warning";
    return "Normal";
}

void temp_humi_monitor(void *pvParameters){

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
            Serial.println("Failed to read from DHT sensor!");
            temperature = humidity =  -1;
            //return;
        }

        //Update global variables for temperature and humidity
        glob_temperature = temperature;
        glob_humidity = humidity;

        if (pvParameters != NULL) {
            SharedContext* ctx = (SharedContext*)pvParameters;
            xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
            ctx->temperature = temperature;
            ctx->humidity = humidity;
            
            // Task 1 Logic
            int newLedState = 1; // Normal
            if (temperature >= 30.0) newLedState = 3; // Critical
            else if (temperature >= 25.0) newLedState = 2; // Warning
            
            if (newLedState != ctx->ledState) {
                ctx->ledState = newLedState;
                xSemaphoreGive(ctx->semLEDUpdate);
            }

            // Task 2 Logic
            int newNeoState = 1; // Normal
            if (humidity >= 70.0) newNeoState = 3; // Critical
            else if (humidity >= 50.0) newNeoState = 2; // Warning
            
            if (newNeoState != ctx->neoState) {
                ctx->neoState = newNeoState;
                xSemaphoreGive(ctx->semNeoUpdate);
            }

            // LCD status logic: worst case between temperature and humidity
            int newLcdState = (newLedState > newNeoState) ? newLedState : newNeoState;
            if (newLcdState != ctx->lcdState) {
                ctx->lcdState = newLcdState;
                xSemaphoreGive(ctx->semLCDUpdate);
            }
            
            xSemaphoreGive(ctx->mutexContext);
        }

        // Print the results
        Serial.print("Humidity: ");
        Serial.print(humidity);
        Serial.print("%  Temperature: ");
        Serial.print(temperature);
        Serial.println("°C");

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
        Webserver_sendata(jsonStr);

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
