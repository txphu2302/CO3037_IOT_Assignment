#include "global.h"
float glob_temperature = 0;
float glob_humidity = 0;
bool glob_soil_ready = false;

String WIFI_SSID;
String WIFI_PASS;
String CORE_IOT_TOKEN;
String CORE_IOT_SERVER;
String CORE_IOT_PORT;

String ssid = "ESP32-YOUR NETWORK HERE!!!";
String password = "12345678";
String wifi_ssid = "abcde";
String wifi_password = "123456789";
boolean isWifiConnected = false;
SemaphoreHandle_t xBinarySemaphoreInternet = xSemaphoreCreateBinary();
bool led_ap_manual_override = false;
bool led_ap_manual_state = false;
bool neo_ap_manual_override = false;
bool neo_ap_manual_state = false;
uint8_t neo_ap_color_r = 255;
uint8_t neo_ap_color_g = 0;
uint8_t neo_ap_color_b = 0;
bool pump_ap_manual_override = false;
bool pump_ap_manual_state = false;
int pump_mode = 0;
bool pump_auto_armed = false;
int pump_auto_threshold = 40;
int pump_auto_hysteresis = 5;
bool pump_schedule_enabled = false;
int pump_schedule_hour = 6;
int pump_schedule_minute = 0;
int pump_schedule_duration_sec = 15;
bool pump_schedule_running = false;
bool pump_actual_state = false;
String pump_controller = "AUTO";