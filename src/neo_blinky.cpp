#include "neo_blinky.h"
#include "global.h"

void neo_blinky(void *pvParameters){

    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    strip.clear();
    strip.show();

    SharedContext* ctx = (SharedContext*)pvParameters;
    
    // Initial color setup
    if (ctx != NULL) {
        xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
        int state = ctx->neoState;
        xSemaphoreGive(ctx->mutexContext);
        
        if (state == 1) strip.setPixelColor(0, strip.Color(0, 255, 0)); // Green
        else if (state == 2) strip.setPixelColor(0, strip.Color(255, 255, 0)); // Yellow
        else if (state == 3) strip.setPixelColor(0, strip.Color(255, 0, 0)); // Red
        strip.show();
    }

    while(1) {                          
        if (ctx != NULL) {
            // Apply Manual Override
            if (neo_ap_manual_override) {
                if (neo_ap_manual_state) {
                    strip.setPixelColor(0, strip.Color(neo_ap_color_r, neo_ap_color_g, neo_ap_color_b));
                } else {
                    strip.setPixelColor(0, strip.Color(0, 0, 0));
                }
                strip.show();
            }

            // Wait indefinitely for a semaphore signal indicating state change
            if (xSemaphoreTake(ctx->semNeoUpdate, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (!neo_ap_manual_override) {
                    xSemaphoreTake(ctx->mutexContext, portMAX_DELAY);
                    int state = ctx->neoState;
                    xSemaphoreGive(ctx->mutexContext);
                    
                    if (state == 1) { // Normal
                       strip.setPixelColor(0, strip.Color(0, 255, 0)); // Green
                    } else if (state == 2) { // Warning
                       strip.setPixelColor(0, strip.Color(255, 255, 0)); // Yellow
                    } else if (state == 3) { // Critical
                       strip.setPixelColor(0, strip.Color(255, 0, 0)); // Red
                    }
                    strip.show();
                }
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}