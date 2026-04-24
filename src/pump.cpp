#include "pump.h"
#include "global.h"  
#define PUMP_PIN 6 

void task_pump(void *pvParameters) {
  pinMode(PUMP_PIN, OUTPUT);
  while (1) {
    if (pump_ap_manual_override) {
        digitalWrite(PUMP_PIN, pump_ap_manual_state ? HIGH : LOW);
    } else {
        digitalWrite(PUMP_PIN, LOW); // Mặc định tắt nếu không có lệnh
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}