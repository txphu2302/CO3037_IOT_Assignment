#include "global.h"
float glob_temperature = 0;
float glob_humidity = 0;

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