#include "temp_humi_monitor.h"
DHT20 dht20;
LiquidCrystal_I2C lcd(33,16,2);


void temp_humi_monitor(void *pvParameters){

    Wire.begin(11, 12);
    Serial.begin(115200);
    dht20.begin();

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
            
            xSemaphoreGive(ctx->mutexContext);
        }

        // Print the results
        
        Serial.print("Humidity: ");
        Serial.print(humidity);
        Serial.print("%  Temperature: ");
        Serial.print(temperature);
        Serial.println("°C");
        
        vTaskDelay(5000);
    }
    
}