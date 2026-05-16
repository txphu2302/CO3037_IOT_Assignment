#include "soil_sensor.h"
#include "global.h"  
#include "task_webserver.h"
#define SOIL_PIN 1

void task_soil_sensor(void *pvParameters) {
    Wire.begin(11, 12);
    uint8_t warmupSamples = 0;
    while(1)
    {
        int soil_moisture = analogRead(SOIL_PIN);
        float new_soil = map(soil_moisture, 0, 4095, 0, 100);
        
        if (warmupSamples < 3) {
            warmupSamples++;
            if (warmupSamples >= 3) {
                if (pvParameters != NULL) {
                    SharedContext* ctx = (SharedContext*)pvParameters;
                    xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
                    ctx->soilReady = true;
                    xSemaphoreGive(ctx->mutexContext);
                }
            }
        }

        if (pvParameters != NULL) {
            SharedContext* ctx = (SharedContext*)pvParameters;
            if (xSemaphoreTake(ctx->mutexContext, portMAX_DELAY) == pdTRUE) {
                ctx->soilMoisture = new_soil;
                xSemaphoreGive(ctx->mutexContext);
            }
        }

        // Gửi qua WebSocket
        String jsonStr = "{\"soil_moisture\":" + String(new_soil, 2) + "}";
        if (pvParameters != NULL) {
            Webserver_sendata((SharedContext*)pvParameters, jsonStr);
        }

        Serial.print("Soil Moisture: ");
        Serial.println(new_soil);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
