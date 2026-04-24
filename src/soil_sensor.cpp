#include "soil_sensor.h"
#include "global.h"  
#include "task_webserver.h"
#define SOIL_PIN 1

float glob_soil_moisture = 0;

void task_soil_sensor(void *pvParameters) {
    Wire.begin(11, 12);
    while(1)
    {
        int soil_moisture = analogRead(SOIL_PIN);
        float new_soil = map(soil_moisture, 0, 4095, 0, 100);
        
        glob_soil_moisture = new_soil;

        if (pvParameters != NULL) {
            SharedContext* ctx = (SharedContext*)pvParameters;
            if (xSemaphoreTake(ctx->mutexContext, portMAX_DELAY) == pdTRUE) {
                ctx->soilMoisture = new_soil;
                xSemaphoreGive(ctx->mutexContext);
            }
        }

        // Gửi qua WebSocket
        String jsonStr = "{\"soil_moisture\":" + String(new_soil, 2) + "}";
        Webserver_sendata(jsonStr);

        Serial.print("Soil Moisture: ");
        Serial.println(new_soil);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}